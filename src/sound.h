#ifndef SOUND_H
#define SOUND_H

#include <string>

void initSound(std::string filename);
//! (Re)load the music: stops the current one; an empty name only stops it.
//! Returns false if the file cannot be played.
bool soundLoad(const std::string &filename);
//! Pause / resume the music
void soundSetPaused(bool paused);
//! Move the music to the given position (restarting it if it had ended)
void soundSeek(double seconds, bool paused);


#endif
