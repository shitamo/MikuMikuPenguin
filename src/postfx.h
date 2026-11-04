#ifndef POSTFX_H
#define POSTFX_H

//Post-effect chain: the scene is drawn into a floating point offscreen
//framebuffer (multisampled like the window, with depth and stencil and a
//second colour attachment for emitted light), then processed by full-screen
//passes and written to the window:
//
//  scene (MSAA) --resolve--> colour, glow, depth (full size)
//  SSAO      : depth -> occlusion (half size) -> depth-aware blur
//  DoF       : colour+depth -> half size -> disc gather blur
//  bloom/glow: bright parts + glow -> half size -> 5 smaller levels -> upsampled back
//  composite : colour * AO, DoF blend, + bloom, colour correction -> window (or FXAA)
//  FXAA      : -> window
//
//Everything is created on first use, and nothing is done while all effects
//are off (the scene is then drawn directly into the window as before).

#include "mmp_gl.h"
#include "mmp_glm.h"
#include "effects.h"

#include <string>

class PostEffects
{
	public:
	PostEffects();
	~PostEffects();

	EffectSettings settings;

	//! shader directory ("shaders" or the installed one); call with a current context
	void init(const std::string &shaderDir);
	//! free the GL objects (before the context goes away)
	void release();

	//! the effects cannot run (shader or framebuffer error); see error()
	bool failed() const { return broken; }
	const std::string &error() const { return errorText; }

	//! bind and clear the offscreen framebuffer for this frame; false: draw into the window
	bool beginScene(int width, int height, const glm::vec4 &clearColor);
	//! the framebuffer the scene is drawn into (0 = window)
	GLuint sceneFramebuffer() const { return active ? (sceneFBO ? sceneFBO : resolveFBO) : 0; }
	//! only the colour attachment is written (for shaders without the glow output)
	void sceneColorOnly();

	struct FrameInfo
	{
		glm::mat4 projection;
		float focusDistance; //!< distance from the camera to its point of interest
	};
	//! run the passes and write the result into the window (framebuffer 0)
	void endScene(const FrameInfo &frame);

	//! glow output mode for the model shader (bit 1: emissive materials, bit 2: bright parts)
	int glowMode() const;

	private:
	bool initialised, broken, active;
	std::string errorText;
	int width, height, samples;

	GLuint vao;
	//scene (multisampled; 0 when drawn straight into the textures)
	GLuint sceneFBO, sceneColorRB, sceneGlowRB, sceneDepthRB;
	GLuint resolveFBO, colorTex, glowTex, depthTex;
	//half size buffers
	int halfW, halfH;
	GLuint aoTex[2], aoFBO[2];
	GLuint dofTex[2], dofFBO[2];
	static const int BLOOM_LEVELS=6;
	GLuint bloomTex[BLOOM_LEVELS], bloomFBO[BLOOM_LEVELS];
	int bloomW[BLOOM_LEVELS], bloomH[BLOOM_LEVELS];
	//FXAA input
	GLuint ldrTex, ldrFBO;
	std::string shaderDir;
	bool glowAllocated; //!< the scene has the glow attachment

	struct Program
	{
		GLuint id=0;
		GLint loc(const char *name) const;
	};
	Program pSSAO, pSSAOBlur, pDofPre, pDofBlur, pBloomPre, pBloomDown, pBloomUp, pComposite, pFXAA;

	bool createTargets(int w, int h, bool withGlow);
	void destroyTargets();
	bool loadProgram(Program &p, const std::string &dir, const char *frag);
	void fail(const std::string &message);

	void drawQuad();
	void bindTexture(int unit, GLuint tex);
	void target(GLuint fbo, int w, int h);
};

#endif
