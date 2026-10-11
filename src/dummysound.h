#ifndef DUMMY_SOUND_H
#define DUMMY_SOUND_H

void initSound(std::string filename);
//! Pause / resume the music
void soundSetPaused(bool paused);
//! Move the music to the given position (restarting it if it had ended)
void soundSeek(double seconds, bool paused);


#endif
