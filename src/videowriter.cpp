//Video output ("AVI output" of MMD): numbered PNG files written with
//stb_image_write, or raw RGB frames piped into ffmpeg, which encodes them
//(and muxes the music) into the format given by the file extension.

#include "videowriter.h"

#include <stb/stb_image_write.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>

extern char **environ;

using namespace std;

namespace
{
	string lowerExtension(const string &path)
	{
		size_t slash=path.find_last_of('/');
		size_t dot=path.find_last_of('.');
		if(dot==string::npos || (slash!=string::npos && dot<slash)) return "";
		string e=path.substr(dot+1);
		for(char &c : e) c=(char)tolower((unsigned char)c);
		return e;
	}

	bool isDir(const string &path)
	{
		struct stat st;
		return stat(path.c_str(), &st)==0 && S_ISDIR(st.st_mode);
	}

	//one printf conversion of the form %d / %05d and no other '%' (except %%)
	bool validFrameFormat(const string &s)
	{
		int conversions=0;
		for(size_t i=0; i<s.size(); ++i)
		{
			if(s[i]!='%') continue;
			if(i+1<s.size() && s[i+1]=='%') { ++i; continue; }
			size_t j=i+1;
			if(j<s.size() && s[j]=='0') ++j;
			while(j<s.size() && s[j]>='0' && s[j]<='9' && j-i<4) ++j;
			if(j>=s.size() || s[j]!='d') return false;
			++conversions;
			i=j;
		}
		return conversions==1;
	}

	string fmtNumber(double v)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "%.6f", v);
		return buf;
	}
}

VideoWriter::VideoWriter(): opened(false), sequence(false), frames(0), child(-1), pipeFd(-1)
{
}

VideoWriter::~VideoWriter()
{
	if(opened)
	{
		string err;
		close(err, true);
	}
}

bool VideoWriter::isImageSequence(const string &path)
{
	return path.empty() || path.back()=='/' || isDir(path) || lowerExtension(path)=="png";
}

bool VideoWriter::ffmpegAvailable()
{
	static int found=-1;
	if(found<0)
	{
		found=0;
		const char *env=getenv("PATH");
		string paths=env ? env : "/usr/local/bin:/usr/bin:/bin";
		size_t pos=0;
		while(pos<=paths.size())
		{
			size_t next=paths.find(':', pos);
			if(next==string::npos) next=paths.size();
			string dir=paths.substr(pos, next-pos);
			if(dir.empty()) dir=".";
			if(access((dir+"/ffmpeg").c_str(), X_OK)==0) { found=1; break; }
			pos=next+1;
		}
	}
	return found==1;
}

string VideoWriter::sequenceFileName(const string &path, int n)
{
	char num[32];
	if(path.empty() || path.back()=='/' || isDir(path))
	{
		snprintf(num, sizeof(num), "%05d.png", n);
		return (path.empty() ? string("") : (path.back()=='/' ? path : path+"/"))+num;
	}
	if(path.find('%')!=string::npos && validFrameFormat(path))
	{
		int len=snprintf(NULL, 0, path.c_str(), n);
		if(len>0)
		{
			vector<char> buf(len+1);
			snprintf(buf.data(), buf.size(), path.c_str(), n);
			return string(buf.data());
		}
	}
	//"out.png" -> "out_00000.png"
	size_t dot=path.find_last_of('.');
	snprintf(num, sizeof(num), "_%05d", n);
	return path.substr(0, dot)+num+path.substr(dot);
}

vector<string> VideoWriter::ffmpegArgs() const
{
	string ext=lowerExtension(opt.path);
	bool audio=!opt.musicPath.empty() && ext!="gif";
	char size[32];
	snprintf(size, sizeof(size), "%dx%d", opt.width, opt.height);
	vector<string> a={"ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
		"-f", "rawvideo", "-pix_fmt", "rgb24", "-video_size", size,
		"-framerate", std::to_string(opt.fps), "-i", "pipe:0"};
	if(audio)
	{
		//input seeking: the music starts at the first rendered frame
		if(opt.musicStart>0.0) { a.push_back("-ss"); a.push_back(fmtNumber(opt.musicStart)); }
		a.push_back("-i"); a.push_back(opt.musicPath);
	}
	a.push_back("-map"); a.push_back("0:v:0");
	if(audio) { a.push_back("-map"); a.push_back("1:a:0?"); }

	vector<string> v, s;
	if(ext=="mp4" || ext=="m4v" || ext=="mov" || ext=="mkv")
	{
		v={"-c:v", "libx264", "-preset", "medium", "-crf", "18", "-pix_fmt", "yuv420p"};
		if(ext!="mkv") { v.push_back("-movflags"); v.push_back("+faststart"); }
		s={"-c:a", "aac", "-b:a", "192k"};
	}
	else if(ext=="webm")
	{
		v={"-c:v", "libvpx-vp9", "-crf", "30", "-b:v", "0", "-row-mt", "1", "-deadline", "good", "-cpu-used", "4", "-pix_fmt", "yuv420p"};
		s={"-c:a", "libopus", "-b:a", "160k"};
	}
	else if(ext=="avi")
	{
		v={"-c:v", "mjpeg", "-q:v", "2", "-pix_fmt", "yuvj420p"};
		s={"-c:a", "pcm_s16le"};
	}
	else if(ext=="gif")
	{
		v={"-filter_complex", "[0:v]split[a][b];[a]palettegen[p];[b][p]paletteuse", "-loop", "0"};
		//-filter_complex output replaces the -map of the video
		a.erase(a.end()-2, a.end());
	}
	else
	{
		v={"-pix_fmt", "yuv420p"}; //let ffmpeg choose the codecs for the container
	}
	a.insert(a.end(), v.begin(), v.end());
	if(audio)
	{
		a.insert(a.end(), s.begin(), s.end());
		//the video decides the length (the music is cut, or ends early)
		a.push_back("-t"); a.push_back(fmtNumber(opt.duration));
	}
	//a name starting with '-' would be taken for an option
	a.push_back(opt.path[0]=='-' ? "./"+opt.path : opt.path);
	return a;
}

bool VideoWriter::open(const Options &o, string &error)
{
	if(opened) close(error, true);
	opt=o;
	frames=0;
	if(opt.width<=0 || opt.height<=0 || opt.fps<=0)
	{
		error="invalid video size or frame rate";
		return false;
	}
	sequence=isImageSequence(opt.path);
	if(sequence)
	{
		if(!opt.path.empty() && opt.path.back()=='/' && !isDir(opt.path) && mkdir(opt.path.c_str(), 0777)!=0)
		{
			error="cannot create directory "+opt.path+": "+strerror(errno);
			return false;
		}
		opened=true;
		return true;
	}

	if(!ffmpegAvailable())
	{
		error="ffmpeg is needed for video files (install it, or write PNG files: --render out.png)";
		return false;
	}
	vector<string> args=ffmpegArgs();
	vector<char*> argv;
	for(string &s : args) argv.push_back(&s[0]);
	argv.push_back(NULL);

	int fds[2];
	if(pipe(fds)!=0)
	{
		error=string("pipe: ")+strerror(errno);
		return false;
	}
	fcntl(fds[1], F_SETFD, FD_CLOEXEC);
	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, fds[0], 0);
	posix_spawn_file_actions_addclose(&actions, fds[0]);
	pid_t pid=-1;
	int rc=posix_spawnp(&pid, "ffmpeg", &actions, NULL, argv.data(), environ);
	posix_spawn_file_actions_destroy(&actions);
	::close(fds[0]);
	if(rc!=0)
	{
		::close(fds[1]);
		error=string("cannot start ffmpeg: ")+strerror(rc);
		return false;
	}
	//a dying ffmpeg must not kill the viewer: write() reports EPIPE instead
	signal(SIGPIPE, SIG_IGN);
	child=pid;
	pipeFd=fds[1];
	opened=true;
	return true;
}

bool VideoWriter::writeFrame(const unsigned char *rgb, string &error)
{
	if(!opened) { error="output not open"; return false; }
	size_t stride=(size_t)opt.width*3;
	if(sequence)
	{
		string name=sequenceFileName(opt.path, frames);
		if(!stbi_write_png(name.c_str(), opt.width, opt.height, 3, rgb, (int)stride))
		{
			error="cannot write "+name;
			return false;
		}
		++frames;
		return true;
	}

	size_t len=stride*opt.height;
	const unsigned char *p=rgb;
	while(len>0)
	{
		ssize_t n=write(pipeFd, p, len);
		if(n<0)
		{
			if(errno==EINTR) continue;
			error=(errno==EPIPE) ? string("ffmpeg stopped (see its messages above)") : string("writing to ffmpeg: ")+strerror(errno);
			return false;
		}
		p+=n;
		len-=(size_t)n;
	}
	++frames;
	return true;
}

bool VideoWriter::close(string &error, bool abort)
{
	if(!opened) return true;
	opened=false;
	if(sequence) return true;

	if(pipeFd>=0) { ::close(pipeFd); pipeFd=-1; }
	if(child<=0) return true;
	if(abort) kill(child, SIGTERM);
	int status=0;
	while(waitpid(child, &status, 0)<0 && errno==EINTR) {}
	child=-1;
	if(abort)
	{
		unlink(opt.path.c_str()); //incomplete file
		return true;
	}
	if(!WIFEXITED(status) || WEXITSTATUS(status)!=0)
	{
		error="ffmpeg failed (see its messages above)";
		return false;
	}
	return true;
}
