#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sound.h"

#include <iostream>

using namespace std;

#ifdef MMP_SDL3_MIXER
/* ---------------- SDL3_mixer (3.x) ---------------- */
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

static MIX_Mixer *mixer=NULL;
static MIX_Audio *music=NULL;
static MIX_Track *track=NULL;

void initSound(string filename)
{
	if(filename=="") return;
	
	cout<<"initializing sound (SDL3_mixer)...";
	
	//keep Ctrl-C / SIGTERM working (SDL would otherwise turn them into an
	//SDL quit event that the viewer never looks at)
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
	if(!SDL_Init(SDL_INIT_AUDIO) || !MIX_Init())
	{
		cerr<<"Error initializing SDL3_mixer: "<<SDL_GetError()<<endl<<"Continuing without sound."<<endl;
		return;
	}
	mixer=MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
	if(!mixer)
	{
		cerr<<"Error opening audio device: "<<SDL_GetError()<<endl<<"Continuing without sound."<<endl;
		return;
	}
	music=MIX_LoadAudio(mixer, filename.c_str(), false);
	if(!music)
	{
		cerr<<"ERROR: failed to load music "<<filename<<": "<<SDL_GetError()<<endl;
		return;
	}
	track=MIX_CreateTrack(mixer);
	if(!track || !MIX_SetTrackAudio(track, music) || !MIX_PlayTrack(track, 0))
	{
		cerr<<"ERROR: failed to play music: "<<SDL_GetError()<<endl;
		return;
	}
	
	cout<<"done"<<endl;
}

void soundSetPaused(bool paused)
{
	if(!track) return;
	if(paused) MIX_PauseTrack(track);
	else MIX_ResumeTrack(track);
}

void soundSeek(double seconds, bool paused)
{
	if(!track) return;
	Sint64 ms=(Sint64)(seconds*1000.0);
	if(ms<0) ms=0;
	if(MIX_TrackPlaying(track) || MIX_TrackPaused(track))
	{
		MIX_SetTrackPlaybackPosition(track, MIX_TrackMSToFrames(track, ms));
	}
	else //the music had ended: start it again from that position
	{
		SDL_PropertiesID props=SDL_CreateProperties();
		SDL_SetNumberProperty(props, MIX_PROP_PLAY_START_MILLISECOND_NUMBER, ms);
		MIX_PlayTrack(track, props);
		SDL_DestroyProperties(props);
	}
	if(paused) MIX_PauseTrack(track);
}

#else
/* ---------------- SDL2_mixer (2.x) ---------------- */
#include <SDL.h>
#include <SDL_mixer.h>

static Mix_Music *music=NULL;
//SDL2_mixer cannot seek in WAV music, so WAV files are played as a sound
//chunk instead; seeking plays a chunk that starts inside its buffer.
static Mix_Chunk *wavChunk=NULL;
static Mix_Chunk *partChunk=NULL; //view into wavChunk's buffer (does not own it)
static int wavChannel=-1;
static int devFreq=44100, devChannels=2;
static Uint16 devFormat=AUDIO_S16SYS;

void initSound(string filename)
{
	if(filename=="") return;
	
	cout<<"initializing sound (SDL2_mixer)...";
	
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"); //keep Ctrl-C / SIGTERM working
	SDL_Init(SDL_INIT_AUDIO);
	Mix_Init(MIX_INIT_OGG | MIX_INIT_MP3);
	
	if(Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 4096)==-1)
	{
		cerr<<"Error initializing SDL_mixer: "<<Mix_GetError()<<endl<<"Continuing without sound."<<endl;
		return;
	}
	Mix_QuerySpec(&devFreq, &devFormat, &devChannels);
	
	music = Mix_LoadMUS(filename.c_str());
	if(music == NULL)
	{
		cerr<<"ERROR: failed to load music "<<filename<<": "<<Mix_GetError()<<endl;
		return;
	}
	if(Mix_GetMusicType(music)==MUS_WAV)
	{
		wavChunk=Mix_LoadWAV(filename.c_str()); //decoded to the device format
		if(wavChunk)
		{
			Mix_FreeMusic(music);
			music=NULL;
			wavChannel=Mix_PlayChannel(-1, wavChunk, 0);
			cout<<"done"<<endl;
			return;
		}
	}
	
	cout<<"done"<<endl;
	Mix_PlayMusic(music, 0);
}

void soundSetPaused(bool paused)
{
	if(wavChunk)
	{
		if(wavChannel<0) return;
		if(paused) Mix_Pause(wavChannel);
		else Mix_Resume(wavChannel);
		return;
	}
	if(!music) return;
	if(paused) Mix_PauseMusic();
	else Mix_ResumeMusic();
}

void soundSeek(double seconds, bool paused)
{
	if(seconds<0.0) seconds=0.0;
	if(wavChunk)
	{
		if(wavChannel>=0) Mix_HaltChannel(wavChannel);
		wavChannel=-1;
		if(partChunk) { Mix_FreeChunk(partChunk); partChunk=NULL; }
		
		Uint32 frameBytes=(SDL_AUDIO_BITSIZE(devFormat)/8)*devChannels;
		double offFrames=seconds*devFreq;
		if(frameBytes==0 || offFrames>=(double)(wavChunk->alen/frameBytes)) return; //past the end
		Uint32 off=(Uint32)offFrames*frameBytes;
		partChunk=Mix_QuickLoad_RAW(wavChunk->abuf+off, wavChunk->alen-off);
		if(!partChunk) return;
		wavChannel=Mix_PlayChannel(-1, partChunk, 0);
		if(paused && wavChannel>=0) Mix_Pause(wavChannel);
		return;
	}
	
	if(!music) return;
	if(!Mix_PlayingMusic()) Mix_PlayMusic(music, 0); //the music had ended
	Mix_RewindMusic();
	if(seconds>0.0 && Mix_SetMusicPosition(seconds)==-1)
	{
		cerr<<"WARNING: cannot seek in this music format: "<<Mix_GetError()<<endl;
	}
	if(paused) Mix_PauseMusic();
	else Mix_ResumeMusic();
}
#endif
