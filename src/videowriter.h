#ifndef VIDEOWRITER_H
#define VIDEOWRITER_H

#include <string>
#include <vector>
#include <sys/types.h>

//! Writes rendered frames (RGB, 8 bits per channel, top row first) either as
//! numbered PNG files or into a video file encoded by an ffmpeg process that
//! reads raw frames from a pipe.
class VideoWriter
{
	public:
	struct Options
	{
		std::string path;      //!< "*.png", a directory, or a video file (mp4, webm, mkv, mov, avi, gif...)
		int width=1280, height=720;
		int fps=30;
		std::string musicPath; //!< muxed into a video (empty: no sound)
		double musicStart=0.0; //!< position in the music of the first frame (seconds)
		double duration=0.0;   //!< length of the output (seconds), cuts the music
	};

	VideoWriter();
	~VideoWriter(); //!< aborts an unfinished video (the ffmpeg process is killed)

	bool open(const Options &opt, std::string &error);
	bool writeFrame(const unsigned char *rgb, std::string &error);
	//! Finish the output; for a video waits for ffmpeg.  With abort the
	//! encoder is stopped and the (incomplete) file removed.
	bool close(std::string &error, bool abort=false);
	bool isOpen() const { return opened; }
	int framesWritten() const { return frames; }

	//! true when the path is written as a PNG sequence (no ffmpeg needed)
	static bool isImageSequence(const std::string &path);
	//! ffmpeg found in PATH (cached)
	static bool ffmpegAvailable();
	//! Name of the n-th PNG of a sequence (path "out.png" -> "out_00000.png",
	//! "out_%04d.png" -> printf format, a directory -> "dir/00000.png")
	static std::string sequenceFileName(const std::string &path, int n);

	private:
	Options opt;
	bool opened, sequence;
	int frames;
	pid_t child;
	int pipeFd;
	std::vector<unsigned char> buffer;
	std::vector<std::string> ffmpegArgs() const;
};

#endif
