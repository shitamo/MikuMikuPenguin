//Video output (MMD's "AVI output"): the scene is drawn frame by frame into a
//framebuffer object of the requested size, without the GUI, and the frames
//are written as PNG files or piped into ffmpeg.  Time does not follow the
//clock: every output frame advances the animation and the physics by exactly
//1/fps seconds in 1/60 s steps, starting from frame 0 so that the physics
//looks the same as when the motion is played from its beginning.

#include "viewer.h"
#include "videowriter.h"
#include "sound.h"

#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <unistd.h>

using namespace std;

bool Viewer::createRecordTargets(int w, int h, string &error)
{
	destroyRecordTargets();
	GLint maxSize=0, maxSamples=0;
	glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxSize);
	glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
	if(w>maxSize || h>maxSize)
	{
		error="video size too large for this OpenGL driver (at most "+std::to_string(maxSize)+" pixels)";
		return false;
	}
	//4x multisampling like the window, resolved into a plain colour buffer
	//that is read back
	int samples=std::min(4, (int)maxSamples);

	glGenRenderbuffers(1, &recColorRB);
	glBindRenderbuffer(GL_RENDERBUFFER, recColorRB);
	if(samples>0) glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, w, h);
	else glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);
	glGenRenderbuffers(1, &recDepthRB);
	glBindRenderbuffer(GL_RENDERBUFFER, recDepthRB);
	//stencil: the ground shadow darkens each pixel only once
	if(samples>0) glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, w, h);
	else glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
	glGenFramebuffers(1, &recFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, recFBO);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, recColorRB);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, recDepthRB);
	bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;

	glGenRenderbuffers(1, &recResolveRB);
	glBindRenderbuffer(GL_RENDERBUFFER, recResolveRB);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);
	glGenFramebuffers(1, &recResolveFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, recResolveFBO);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, recResolveRB);
	ok=ok && glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if(!ok || glGetError()!=GL_NO_ERROR)
	{
		destroyRecordTargets();
		error="cannot create the offscreen framebuffer ("+std::to_string(w)+"x"+std::to_string(h)+")";
		return false;
	}
	recPixels.assign((size_t)w*h*3, 0);
	return true;
}

void Viewer::destroyRecordTargets()
{
	if(recFBO) glDeleteFramebuffers(1, &recFBO);
	if(recResolveFBO) glDeleteFramebuffers(1, &recResolveFBO);
	GLuint rbs[3]={recColorRB, recDepthRB, recResolveRB};
	for(GLuint rb : rbs) if(rb) glDeleteRenderbuffers(1, &rb);
	recFBO=recResolveFBO=recColorRB=recDepthRB=recResolveRB=0;
	recPixels.clear();
	recPixels.shrink_to_fit();
}

bool Viewer::startRecording(const RecordSettings &s, string &error)
{
	if(recording) { error="a video is already being written"; return false; }
	rec=s;
	if(models.empty()) { error="no model to record"; return false; }
	if(rec.output.empty()) { error="no output file given"; return false; }
	int lastFrame=std::max(endTime/2, 0);
	if(rec.startFrame<0) rec.startFrame=0;
	if(rec.endFrame<0) rec.endFrame=lastFrame;
	if(rec.endFrame<rec.startFrame)
	{
		error="the range ends ("+std::to_string(rec.endFrame)+") before it starts ("+std::to_string(rec.startFrame)+")";
		return false;
	}
	if(rec.fps!=30 && rec.fps!=60) { error="the frame rate must be 30 or 60"; return false; }
	bool video=!VideoWriter::isImageSequence(rec.output);
	if(video)
	{
		//H.264 / VP9 in 4:2:0 need even sizes
		rec.width&=~1;
		rec.height&=~1;
	}
	if(rec.width<16 || rec.height<16 || rec.width>16384 || rec.height>16384)
	{
		error="invalid video size "+std::to_string(rec.width)+"x"+std::to_string(rec.height);
		return false;
	}
	recStep=60/rec.fps;
	//the range includes both ends: (end-start+1)/30 seconds
	recTotal=(rec.endFrame-rec.startFrame+1)*2/recStep;

	if(!createRecordTargets(rec.width, rec.height, error)) return false;

	VideoWriter::Options o;
	o.path=rec.output;
	o.width=rec.width;
	o.height=rec.height;
	o.fps=rec.fps;
	if(video && rec.withMusic) o.musicPath=musicPathShown;
	o.musicStart=rec.startFrame/30.0;
	o.duration=recTotal/(double)rec.fps;
	writer=new VideoWriter();
	if(!writer->open(o, error))
	{
		delete writer;
		writer=NULL;
		destroyRecordTargets();
		return false;
	}

	//stop the music and playback; restored afterwards
	recSavedTime=sceneTime;
	recSavedPaused=paused;
	if(!paused) soundSetPaused(true);
	paused=true;
	recording=true;
	recordCancel=false;
	recordFailed=false;
	recordMessage.clear();
	recFrame=0;
	recStartWall=glfwGetTime();
	//start at frame 0 (physics reset to the pose there) and run up to the
	//first frame of the range
	seekTo(0);
	recordPreroll=endTime>0 && sceneTime<rec.startFrame*2;
	if(endTime<=0 && rec.startFrame>0) seekTo(rec.startFrame*2);
	return true;
}

void Viewer::renderRecordFrame()
{
	int savedW=framebufferWidth, savedH=framebufferHeight;
	bool savedStencil=hasStencil;
	framebufferWidth=rec.width;
	framebufferHeight=rec.height;
	hasStencil=true;
	drawFBO=recFBO;

	glBindFramebuffer(GL_FRAMEBUFFER, recFBO);
	glViewport(0, 0, rec.width, rec.height);
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	updateCameraUniforms(); //projection for the video's aspect ratio
	render();

	//resolve the multisampled image and read it back
	glBindFramebuffer(GL_READ_FRAMEBUFFER, recFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, recResolveFBO);
	glBlitFramebuffer(0, 0, rec.width, rec.height, 0, 0, rec.width, rec.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, recResolveFBO);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, rec.width, rec.height, GL_RGB, GL_UNSIGNED_BYTE, recPixels.data());
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//OpenGL's first row is the bottom one
	size_t stride=(size_t)rec.width*3;
	vector<unsigned char> row(stride);
	for(int y=0; y<rec.height/2; ++y)
	{
		unsigned char *a=&recPixels[(size_t)y*stride], *b=&recPixels[(size_t)(rec.height-1-y)*stride];
		memcpy(row.data(), a, stride);
		memcpy(a, b, stride);
		memcpy(b, row.data(), stride);
	}

	drawFBO=0;
	framebufferWidth=savedW;
	framebufferHeight=savedH;
	hasStencil=savedStencil;
	updateCameraUniforms();
}

bool Viewer::recordNextFrame()
{
	if(!recording) return false;
	if(recordPreroll)
	{
		//a few seconds of motion per call, so that the GUI stays responsive
		for(int n=0; n<240 && sceneTime<rec.startFrame*2; ++n) stepLogic();
		recordPreroll=sceneTime<rec.startFrame*2;
		return true;
	}

	renderRecordFrame();
	string err;
	if(!writer->writeFrame(recPixels.data(), err))
	{
		finishRecording(true, err);
		return false;
	}
	++recFrame;
	if(recFrame>=recTotal)
	{
		finishRecording(false);
		return !recordFailed;
	}
	for(int i=0; i<recStep; ++i) stepLogic();
	return true;
}

void Viewer::finishRecording(bool cancelled, const string &error)
{
	if(!recording) return;
	string err=error;
	string closeErr;
	int written=writer ? writer->framesWritten() : 0;
	bool ok=writer ? writer->close(closeErr, cancelled) : true;
	if(err.empty() && !ok) err=closeErr;
	delete writer;
	writer=NULL;
	destroyRecordTargets();
	recording=false;
	recordPreroll=false;
	recordCancel=false;
	recordFailed=!err.empty();
	recordCancelled=cancelled && !recordFailed;

	double secs=glfwGetTime()-recStartWall;
	char buf[256];
	if(recordFailed)
	{
		recordMessage="Video output failed: "+err;
	}
	else if(cancelled)
	{
		bool seq=VideoWriter::isImageSequence(rec.output);
		snprintf(buf, sizeof(buf), "Video output cancelled after %d of %d frames", written, recTotal);
		recordMessage=string(buf)+(seq ? " (the PNG files written so far are kept)" : "");
	}
	else
	{
		snprintf(buf, sizeof(buf), "Wrote %d frames (%.2f s at %d fps, %dx%d) in %.1f s: ",
		         written, written/(double)rec.fps, rec.fps, rec.width, rec.height, secs);
		recordMessage=string(buf)+rec.output;
	}

	//back to where playback was
	paused=recSavedPaused;
	seekTo(recSavedTime);
}

//The frame being written, letterboxed into the window (GUI recording)
void Viewer::drawRecordPreview()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, framebufferWidth, framebufferHeight);
	glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glClearColor(1, 1, 1, 1);
	if(!recResolveFBO || recordPreroll || recFrame==0) return;
	float scale=std::min(framebufferWidth/(float)rec.width, framebufferHeight/(float)rec.height);
	int w=(int)(rec.width*scale), h=(int)(rec.height*scale);
	int x=(framebufferWidth-w)/2, y=(framebufferHeight-h)/2;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, recResolveFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, rec.width, rec.height, x, y, x+w, y+h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int Viewer::runBatchRecord()
{
	string err;
	if(!startRecording(batchSettings, err))
	{
		cerr<<"ERROR: "<<err<<endl;
		return 1;
	}
	bool video=!VideoWriter::isImageSequence(rec.output);
	char info[128];
	snprintf(info, sizeof(info), "Rendering frames %d to %d: %d frames (%.2f s) at %d fps, %dx%d -> ",
	         rec.startFrame, rec.endFrame, recTotal, recTotal/(double)rec.fps, rec.fps, rec.width, rec.height);
	cout<<info<<rec.output<<(video ? "" : " (PNG)")<<endl;
	if(!musicPathShown.empty())
	{
		if(!video) cout<<"Note: the music is not written into PNG files"<<endl;
		else if(rec.withMusic) cout<<"With music "<<musicPathShown<<" from "<<rec.startFrame/30.0<<" s"<<endl;
	}
	if(recordPreroll) cout<<"Running the motion and physics up to frame "<<rec.startFrame<<"..."<<endl;
	bool tty=isatty(2);
	int lastShown=-1;
	while(recording)
	{
		if(!recordNextFrame()) break;
		glfwPollEvents();
		if(recording && !recordPreroll && recFrame>0 && recFrame!=lastShown && (tty ? true : recFrame%30==0))
		{
			lastShown=recFrame;
			double secs=glfwGetTime()-recStartWall;
			if(tty) fprintf(stderr, "\rframe %d / %d  (%.1f s)", recFrame, recTotal, secs);
			else fprintf(stderr, "frame %d / %d\n", recFrame, recTotal);
		}
	}
	if(tty) fprintf(stderr, "\n");
	if(recordFailed)
	{
		cerr<<"ERROR: "<<recordMessage<<endl;
		return 1;
	}
	cout<<recordMessage<<endl;
	return 0;
}
