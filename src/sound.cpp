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

#else
/* ---------------- SDL2_mixer (2.x) ---------------- */
#include <SDL.h>
#include <SDL_mixer.h>

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
	
	Mix_Music *music = Mix_LoadMUS(filename.c_str());
	if(music == NULL)
	{
		cerr<<"ERROR: failed to load music "<<filename<<": "<<Mix_GetError()<<endl;
		return;
	}
	
	cout<<"done"<<endl;
	Mix_PlayMusic(music, 0);
}
#endif
