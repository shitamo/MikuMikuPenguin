#include "postfx.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

using namespace std;

//Texture units used by the passes.  Unit 4 holds the self-shadow map for
//the whole run and is left alone; the model shader rebinds 0-3 per draw.
enum { UNIT_A=0, UNIT_B=1, UNIT_C=2, UNIT_D=3, UNIT_E=5 };

PostEffects::PostEffects()
	: initialised(false), broken(false), active(false),
	  width(0), height(0), samples(0), vao(0),
	  sceneFBO(0), sceneColorRB(0), sceneGlowRB(0), sceneDepthRB(0),
	  resolveFBO(0), colorTex(0), glowTex(0), depthTex(0),
	  halfW(0), halfH(0), ldrTex(0), ldrFBO(0), shaderDir(), glowAllocated(false)
{
	aoTex[0]=aoTex[1]=aoFBO[0]=aoFBO[1]=0;
	dofTex[0]=dofTex[1]=dofFBO[0]=dofFBO[1]=0;
	for(int i=0; i<BLOOM_LEVELS; ++i) { bloomTex[i]=bloomFBO[i]=0; bloomW[i]=bloomH[i]=0; }
}

PostEffects::~PostEffects()
{
	//the GL context is gone by now (Viewer calls release() before)
}

void PostEffects::init(const string &dir)
{
	shaderDir=dir;
	//the window's sample count: the offscreen scene is antialiased the same way
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	GLint s=0, maxSamples=0;
	glGetIntegerv(GL_SAMPLES, &s);
	glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
	samples=std::max(0, std::min((int)s, (int)maxSamples));
}

void PostEffects::release()
{
	destroyTargets();
	Program *programs[]={&pSSAO, &pSSAOBlur, &pDofPre, &pDofBlur, &pBloomPre, &pBloomDown, &pBloomUp, &pComposite, &pFXAA};
	for(Program *p : programs) { if(p->id) glDeleteProgram(p->id); p->id=0; }
	if(vao) glDeleteVertexArrays(1, &vao);
	vao=0;
	initialised=false;
}

void PostEffects::fail(const string &message)
{
	if(!broken) cerr<<"WARNING: screen effects disabled: "<<message<<endl;
	broken=true;
	errorText=message;
	destroyTargets();
}

GLint PostEffects::Program::loc(const char *name) const
{
	return glGetUniformLocation(id, name);
}

static bool readFile(const string &path, string &out)
{
	ifstream f(path.c_str(), ios::binary);
	if(!f) return false;
	stringstream ss;
	ss<<f.rdbuf();
	out=ss.str();
	return true;
}

static GLuint compileStage(GLenum type, const string &path, string &error)
{
	string src;
	if(!readFile(path, src)) { error="cannot read "+path; return 0; }
	GLuint sh=glCreateShader(type);
	const GLchar *p=src.c_str();
	glShaderSource(sh, 1, &p, NULL);
	glCompileShader(sh);
	GLint ok=GL_FALSE;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if(ok!=GL_TRUE)
	{
		GLint len=0;
		glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
		vector<char> log(std::max(len, 1));
		glGetShaderInfoLog(sh, (GLsizei)log.size(), NULL, log.data());
		error=path+": "+log.data();
		glDeleteShader(sh);
		return 0;
	}
	return sh;
}

bool PostEffects::loadProgram(Program &p, const string &dir, const char *frag)
{
	string error;
	GLuint vs=compileStage(GL_VERTEX_SHADER, dir+"/postfx/fullscreen.vert", error);
	GLuint fs=vs ? compileStage(GL_FRAGMENT_SHADER, dir+"/postfx/"+frag, error) : 0;
	if(!vs || !fs)
	{
		if(vs) glDeleteShader(vs);
		fail(error);
		return false;
	}
	p.id=glCreateProgram();
	glAttachShader(p.id, vs);
	glAttachShader(p.id, fs);
	glBindFragDataLocation(p.id, 0, "color");
	glLinkProgram(p.id);
	glDeleteShader(vs);
	glDeleteShader(fs);
	GLint ok=GL_FALSE;
	glGetProgramiv(p.id, GL_LINK_STATUS, &ok);
	if(ok!=GL_TRUE)
	{
		char log[1024]="";
		glGetProgramInfoLog(p.id, sizeof(log), NULL, log);
		fail(string(frag)+": "+log);
		return false;
	}
	return true;
}

//***Render targets
static GLuint makeTexture(GLenum internalFormat, int w, int h, GLenum format, GLenum type, GLenum filter)
{
	GLuint t=0;
	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_2D, t);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return t;
}

static GLuint makeTarget(GLuint tex)
{
	GLuint f=0;
	glGenFramebuffers(1, &f);
	glBindFramebuffer(GL_FRAMEBUFFER, f);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	return f;
}

static GLuint makeRenderbuffer(GLenum format, int samples, int w, int h)
{
	GLuint rb=0;
	glGenRenderbuffers(1, &rb);
	glBindRenderbuffer(GL_RENDERBUFFER, rb);
	if(samples>0) glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, w, h);
	else glRenderbufferStorage(GL_RENDERBUFFER, format, w, h);
	return rb;
}

static bool complete()
{
	return glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
}

bool PostEffects::createTargets(int w, int h, bool withGlow)
{
	destroyTargets();
	width=w; height=h;
	glowAllocated=withGlow;
	glActiveTexture(GL_TEXTURE0);

	//resolved scene that the passes read: floating point colour, emitted
	//light (only with glow) and depth/stencil (the ground shadow uses the stencil)
	colorTex=makeTexture(GL_RGBA16F, w, h, GL_RGBA, GL_FLOAT, GL_LINEAR);
	if(withGlow) glowTex=makeTexture(GL_R11F_G11F_B10F, w, h, GL_RGB, GL_FLOAT, GL_LINEAR);
	depthTex=makeTexture(GL_DEPTH24_STENCIL8, w, h, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_NEAREST);
	glGenFramebuffers(1, &resolveFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
	if(withGlow) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, glowTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
	if(!complete()) { fail("floating point framebuffer not supported"); return false; }
	
	//with a multisampled window the scene is drawn multisampled as well and
	//resolved into the textures; otherwise straight into the textures
	if(samples>0)
	{
		sceneColorRB=makeRenderbuffer(GL_RGBA16F, samples, w, h);
		if(withGlow) sceneGlowRB=makeRenderbuffer(GL_R11F_G11F_B10F, samples, w, h);
		sceneDepthRB=makeRenderbuffer(GL_DEPTH24_STENCIL8, samples, w, h);
		glGenFramebuffers(1, &sceneFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, sceneColorRB);
		if(withGlow) glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, sceneGlowRB);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthRB);
		if(!complete())
		{
			cerr<<"Note: multisampled offscreen framebuffer not supported; effects run without MSAA"<<endl;
			glDeleteFramebuffers(1, &sceneFBO); sceneFBO=0;
			GLuint rbs[3]={sceneColorRB, sceneGlowRB, sceneDepthRB};
			glDeleteRenderbuffers(3, rbs);
			sceneColorRB=sceneGlowRB=sceneDepthRB=0;
			samples=0;
		}
	}
	
	//heavy passes run at half size
	halfW=std::max(1, w/2); halfH=std::max(1, h/2);
	for(int i=0; i<2; ++i)
	{
		aoTex[i]=makeTexture(GL_R8, halfW, halfH, GL_RED, GL_UNSIGNED_BYTE, GL_LINEAR);
		aoFBO[i]=makeTarget(aoTex[i]);
		dofTex[i]=makeTexture(GL_RGBA16F, halfW, halfH, GL_RGBA, GL_FLOAT, GL_LINEAR);
		dofFBO[i]=makeTarget(dofTex[i]);
	}
	int bw=halfW, bh=halfH;
	for(int i=0; i<BLOOM_LEVELS; ++i)
	{
		bloomW[i]=bw; bloomH[i]=bh;
		bloomTex[i]=makeTexture(GL_R11F_G11F_B10F, bw, bh, GL_RGB, GL_FLOAT, GL_LINEAR);
		bloomFBO[i]=makeTarget(bloomTex[i]);
		bw=std::max(1, bw/2); bh=std::max(1, bh/2);
	}
	ldrTex=makeTexture(GL_RGBA8, w, h, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
	ldrFBO=makeTarget(ldrTex);
	if(!complete()) { fail("framebuffer not supported"); return false; }
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}

void PostEffects::destroyTargets()
{
	auto delFB=[](GLuint &f){ if(f) glDeleteFramebuffers(1, &f); f=0; };
	auto delTex=[](GLuint &t){ if(t) glDeleteTextures(1, &t); t=0; };
	auto delRB=[](GLuint &r){ if(r) glDeleteRenderbuffers(1, &r); r=0; };
	delFB(sceneFBO); delRB(sceneColorRB); delRB(sceneGlowRB); delRB(sceneDepthRB);
	delFB(resolveFBO); delTex(colorTex); delTex(glowTex); delTex(depthTex);
	for(int i=0; i<2; ++i) { delFB(aoFBO[i]); delTex(aoTex[i]); delFB(dofFBO[i]); delTex(dofTex[i]); }
	for(int i=0; i<BLOOM_LEVELS; ++i) { delFB(bloomFBO[i]); delTex(bloomTex[i]); }
	delFB(ldrFBO); delTex(ldrTex);
	width=height=0;
	active=false;
}

//***Frame
int PostEffects::glowMode() const
{
	if(!active || !settings.glow || !glowAllocated) return 0;
	return 1 | (settings.glowBright ? 2 : 0);
}

bool PostEffects::beginScene(int w, int h, const glm::vec4 &clearColor)
{
	active=false;
	if(broken || w<=0 || h<=0) return false;
	if(!settings.any())
	{
		if(width>0) destroyTargets(); //all effects turned off: give the memory back
		return false;
	}
	if(!initialised)
	{
		initialised=true;
		glGenVertexArrays(1, &vao);
		const char *files[]={"ssao.frag", "ssao_blur.frag", "dof_prefilter.frag", "dof_blur.frag",
		                     "bloom_prefilter.frag", "bloom_down.frag", "bloom_up.frag", "composite.frag", "fxaa.frag"};
		Program *programs[]={&pSSAO, &pSSAOBlur, &pDofPre, &pDofBlur, &pBloomPre, &pBloomDown, &pBloomUp, &pComposite, &pFXAA};
		for(int i=0; i<9; ++i) if(!loadProgram(*programs[i], shaderDir, files[i])) return false;
	}
	//(re)create the buffers for a new window size; the glow buffer only
	//exists while glow is on
	if(w!=width || h!=height || glowAllocated!=settings.glow)
	{
		if(!createTargets(w, h, settings.glow)) return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO ? sceneFBO : resolveFBO);
	const GLenum buffers[2]={GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
	glDrawBuffers(glowAllocated ? 2 : 1, buffers);
	glViewport(0, 0, w, h);
	glDepthMask(GL_TRUE);
	glStencilMask(0xFF);
	const GLfloat zero[4]={0.0f, 0.0f, 0.0f, 0.0f};
	glClearBufferfv(GL_COLOR, 0, &clearColor[0]);
	if(glowAllocated) glClearBufferfv(GL_COLOR, 1, zero);
	glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
	active=true;
	return true;
}

void PostEffects::sceneColorOnly()
{
	if(!active) return;
	glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer());
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
}

void PostEffects::drawQuad()
{
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

void PostEffects::bindTexture(int unit, GLuint tex)
{
	glActiveTexture(GL_TEXTURE0+unit);
	glBindTexture(GL_TEXTURE_2D, tex);
}

void PostEffects::target(GLuint fbo, int w, int h)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, w, h);
}

void PostEffects::endScene(const FrameInfo &frame, GLuint output)
{
	if(!active) return;
	const EffectSettings &s=settings;
	glm::mat4 invProj=glm::inverse(frame.projection);

	//***resolve the multisampled scene into textures
	if(sceneFBO)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO);
		for(int i=0; i<(glowAllocated ? 2 : 1); ++i)
		{
			glReadBuffer(GL_COLOR_ATTACHMENT0+i);
			glDrawBuffer(GL_COLOR_ATTACHMENT0+i);
			glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
	}

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDepthMask(GL_FALSE);
	glBindVertexArray(vao);

	//***SSAO (half size) and its blur
	if(s.ssao)
	{
		target(aoFBO[0], halfW, halfH);
		glUseProgram(pSSAO.id);
		bindTexture(UNIT_A, depthTex);
		glUniform1i(pSSAO.loc("depthTex"), UNIT_A);
		glUniformMatrix4fv(pSSAO.loc("projection"), 1, GL_FALSE, &frame.projection[0][0]);
		glUniformMatrix4fv(pSSAO.loc("invProjection"), 1, GL_FALSE, &invProj[0][0]);
		glUniform2f(pSSAO.loc("depthTexel"), 1.0f/width, 1.0f/height);
		glUniform1f(pSSAO.loc("radius"), std::max(0.01f, s.ssaoRadius));
		glUniform1f(pSSAO.loc("strength"), s.ssaoStrength);
		drawQuad();

		target(aoFBO[1], halfW, halfH);
		glUseProgram(pSSAOBlur.id);
		bindTexture(UNIT_A, aoTex[0]);
		bindTexture(UNIT_B, depthTex);
		glUniform1i(pSSAOBlur.loc("aoTex"), UNIT_A);
		glUniform1i(pSSAOBlur.loc("depthTex"), UNIT_B);
		glUniformMatrix4fv(pSSAOBlur.loc("invProjection"), 1, GL_FALSE, &invProj[0][0]);
		glUniform2f(pSSAOBlur.loc("aoTexel"), 1.0f/halfW, 1.0f/halfH);
		glUniform1f(pSSAOBlur.loc("radius"), std::max(0.01f, s.ssaoRadius));
		drawQuad();
	}

	//***Depth of field (half size)
	float focus=s.dofAutoFocus ? frame.focusDistance : s.dofFocus;
	focus=std::max(focus, 0.01f);
	float maxBlurFull=std::max(0.0f, s.dofMaxBlur)*height/40.0f; //1 = 1/40 of the picture height
	if(s.dof)
	{
		target(dofFBO[0], halfW, halfH);
		glUseProgram(pDofPre.id);
		bindTexture(UNIT_A, colorTex);
		bindTexture(UNIT_B, depthTex);
		bindTexture(UNIT_C, aoTex[1]);
		glUniform1i(pDofPre.loc("colorTex"), UNIT_A);
		glUniform1i(pDofPre.loc("depthTex"), UNIT_B);
		glUniform1i(pDofPre.loc("aoTex"), UNIT_C);
		glUniform1i(pDofPre.loc("useAO"), s.ssao ? 1 : 0);
		glUniformMatrix4fv(pDofPre.loc("invProjection"), 1, GL_FALSE, &invProj[0][0]);
		drawQuad();

		target(dofFBO[1], halfW, halfH);
		glUseProgram(pDofBlur.id);
		bindTexture(UNIT_A, dofTex[0]);
		glUniform1i(pDofBlur.loc("dofTex"), UNIT_A);
		glUniform2f(pDofBlur.loc("texel"), 1.0f/halfW, 1.0f/halfH);
		glUniform1f(pDofBlur.loc("focus"), focus);
		glUniform1f(pDofBlur.loc("aperture"), s.dofStrength);
		glUniform1f(pDofBlur.loc("maxBlur"), maxBlurFull*0.5f);
		drawQuad();
	}

	//***Bloom and glow: bright parts, 5 smaller levels, added back up
	bool useBloom=s.bloom || (s.glow && glowAllocated);
	float bloomScale=1.0f;
	if(useBloom)
	{
		target(bloomFBO[0], bloomW[0], bloomH[0]);
		glUseProgram(pBloomPre.id);
		bindTexture(UNIT_A, colorTex);
		bindTexture(UNIT_B, glowTex);
		glUniform1i(pBloomPre.loc("colorTex"), UNIT_A);
		glUniform1i(pBloomPre.loc("glowTex"), UNIT_B);
		glUniform1i(pBloomPre.loc("useBloom"), s.bloom ? 1 : 0);
		glUniform1f(pBloomPre.loc("threshold"), std::min(std::max(s.bloomThreshold, 0.0f), 0.999f));
		glUniform1f(pBloomPre.loc("bloomIntensity"), s.bloomIntensity);
		glUniform1i(pBloomPre.loc("useGlow"), (s.glow && glowAllocated) ? 1 : 0);
		glUniform1f(pBloomPre.loc("glowIntensity"), s.glowIntensity);
		drawQuad();

		glUseProgram(pBloomDown.id);
		glUniform1i(pBloomDown.loc("src"), UNIT_A);
		for(int i=1; i<BLOOM_LEVELS; ++i)
		{
			target(bloomFBO[i], bloomW[i], bloomH[i]);
			bindTexture(UNIT_A, bloomTex[i-1]);
			glUniform2f(pBloomDown.loc("srcTexel"), 1.0f/bloomW[i-1], 1.0f/bloomH[i-1]);
			drawQuad();
		}

		//each level gets the larger blurs added with the weight `spread`, up
		//to level 1 (quarter size: level 0 is hardly blurred and would only
		//brighten the surfaces); the sum is normalised so that the spread does
		//not change the brightness
		float spread=std::min(std::max(s.bloomRadius, 0.0f), 1.0f);
		glUseProgram(pBloomUp.id);
		glUniform1i(pBloomUp.loc("src"), UNIT_A);
		glUniform1f(pBloomUp.loc("spread"), spread);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		for(int i=BLOOM_LEVELS-2; i>=1; --i)
		{
			target(bloomFBO[i], bloomW[i], bloomH[i]);
			bindTexture(UNIT_A, bloomTex[i+1]);
			glUniform2f(pBloomUp.loc("srcTexel"), 1.0f/bloomW[i+1], 1.0f/bloomH[i+1]);
			drawQuad();
		}
		glDisable(GL_BLEND);
		float sum=0.0f, w=1.0f;
		for(int i=1; i<BLOOM_LEVELS; ++i) { sum+=w; w*=spread; }
		bloomScale=1.0f/sum;
	}

	//***Composite (to the window, or to an 8-bit buffer for FXAA)
	target(s.fxaa ? ldrFBO : output, width, height);
	glUseProgram(pComposite.id);
	bindTexture(UNIT_A, colorTex);
	bindTexture(UNIT_B, depthTex);
	bindTexture(UNIT_C, aoTex[1]);
	bindTexture(UNIT_D, dofTex[1]);
	bindTexture(UNIT_E, bloomTex[1]);
	glUniform1i(pComposite.loc("colorTex"), UNIT_A);
	glUniform1i(pComposite.loc("depthTex"), UNIT_B);
	glUniform1i(pComposite.loc("aoTex"), UNIT_C);
	glUniform1i(pComposite.loc("dofTex"), UNIT_D);
	glUniform1i(pComposite.loc("bloomTex"), UNIT_E);
	glUniform1i(pComposite.loc("useAO"), s.ssao ? 1 : 0);
	glUniform1i(pComposite.loc("useDof"), s.dof ? 1 : 0);
	glUniformMatrix4fv(pComposite.loc("invProjection"), 1, GL_FALSE, &invProj[0][0]);
	glUniform1f(pComposite.loc("focus"), focus);
	glUniform1f(pComposite.loc("aperture"), s.dofStrength);
	glUniform1f(pComposite.loc("maxBlur"), maxBlurFull);
	glUniform1i(pComposite.loc("useBloom"), useBloom ? 1 : 0);
	glUniform1f(pComposite.loc("bloomScale"), bloomScale);
	glUniform1i(pComposite.loc("useColor"), s.color ? 1 : 0);
	glUniform1f(pComposite.loc("brightness"), s.brightness);
	glUniform1f(pComposite.loc("contrast"), s.contrast);
	glUniform1f(pComposite.loc("saturation"), s.saturation);
	glUniform1f(pComposite.loc("gamma"), std::max(s.gamma, 0.01f));
	glUniform1i(pComposite.loc("lumaInAlpha"), s.fxaa ? 1 : 0);
	drawQuad();

	if(s.fxaa)
	{
		target(output, width, height);
		glUseProgram(pFXAA.id);
		bindTexture(UNIT_A, ldrTex);
		glUniform1i(pFXAA.loc("src"), UNIT_A);
		glUniform2f(pFXAA.loc("texel"), 1.0f/width, 1.0f/height);
		drawQuad();
	}

	//***back to the viewer's state
	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindFramebuffer(GL_FRAMEBUFFER, output);
	glViewport(0, 0, width, height);
	active=false;
}
