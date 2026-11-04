#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifndef MMP_VERSION
#define MMP_VERSION "0.2.1+GLFW3"
#endif
//g++ *.cpp src/*.cpp src/*.cc -o pmx -I/usr/include/bullet -lglfw -lepoxy -lGL -lSDL2 -lSDL2_mixer -ggdb -lBulletDynamics -lBulletCollision -lLinearMath -lBulletSoftBody -std=c++11
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <cstdlib>


#include <libmmp.h>
#include "viewer.h"
#include "effects.h"

#include <strings.h>

using namespace std;

static bool hasSuffix(const string &str, const char *suffix)
{
	size_t n=strlen(suffix);
	return str.size()>=n && strcasecmp(str.c_str()+str.size()-n, suffix)==0;
}

void printHelpInfo()
{
	cout<<"This is PMXViewer, a demonstration using the MikuMikuPenguin library."<<endl
	<<"Usage: pmx [options] [model.pmx|model.pmd [motion.vmd] [model2.pmx motion2.vmd ...]]"<<endl
	<<"       pmx --render out.mp4|out.png [--range A:B] [--size WxH] [--fps 30|60] [options] model [motion.vmd] ..."<<endl
	<<"Without files the viewer starts empty: use File in the window or drop files onto it."<<endl
	<<"Several models can be played together; each VMD belongs to the model before it."<<endl
	<<"Options: "<<endl
	<<"  -s\tPlay sound file to go with VMD motion"<<endl
	<<"\tOnly formats compatible with your build of SDL2_mixer are accepted."<<endl
	<<"\tIf libmmp was compiled without SDL2_mixer support, no music will play."<<endl<<endl
	<<"  -c\tCamera motion VMD (camera frames in the motion VMD are used otherwise)"<<endl
	<<"  -l\tLight motion VMD (otherwise light frames from the -c VMD, then the motion VMD)"<<endl
	<<"  --pos X,Y,Z\tPosition (MMD coordinates) of the model given just before"<<endl
	<<"  --loop\tRestart from the beginning at the end of the motion"<<endl
	<<"  --no-gui\tStart with the on-screen controls hidden (Tab shows them)"<<endl
	<<"  --size WxH\tWindow size, or the video size with --render (default 1280x720)"<<endl
	<<"  --snapshot FRAME OUT.png"<<endl
	<<"\t\tPlay to VMD frame FRAME (with physics), save that frame as a PNG"<<endl
	<<"\t\tand exit, without showing the window or the controls (for tests)"<<endl
	<<"  --effect LIST\tScreen effects to turn on, comma separated: bloom, glow"<<endl
	<<"\t\t(AutoLuminous-like), dof, ssao, color, fxaa, all; NAME=VALUE also sets"<<endl
	<<"\t\tthe strength (e.g. --effect bloom=1.2,dof)"<<endl
	<<"  --dof-focus D\tFocus distance of the depth of field (default: the camera's"<<endl
	<<"\t\tpoint of interest)"<<endl
	<<"  --color B,C,S,G\tColour correction: brightness, contrast, saturation, gamma"<<endl
	<<"\t\t(defaults 0,1,1,1)"<<endl
	<<endl
	<<"Video output (like MMD's AVI output; no window is shown, the program quits when done):"<<endl
	<<"  --render FILE\tWrite the motion to FILE frame by frame and exit."<<endl
	<<"\t\tFILE.png (or FILE_%04d.png, or a directory/) writes numbered PNG files;"<<endl
	<<"\t\tFILE.mp4, .webm, .mkv, .mov, .avi, .gif ... are encoded by ffmpeg"<<endl
	<<"\t\t(the -s music is included, starting at the first frame of the range)."<<endl
	<<"  --range A:B\tVMD frames A to B, both included (default: 0 to the last key;"<<endl
	<<"\t\t\"A:\" or \":B\" keep the other end)"<<endl
	<<"  --fps 30|60\tFrame rate of the video (default 30)"<<endl
	<<"  --no-music\tDo not put the -s music into the video"<<endl
	<<"\tAn X display is still needed (e.g. run it under xvfb-run on a server)."<<endl
	<<"\tThe same is available in the window from File > Export video."<<endl<<endl
	<<"  --help\tdisplay this help and exit"<<endl
	<<"  --version\toutput version information and exit"<<endl<<endl
	<<"Controls:"<<endl
	<<"  left drag\trotate camera      wheel\tzoom"<<endl
	<<"  Up/Down\tmove model          Q (hold)\tpause physics"<<endl
	<<"  A (hold)\thide model          S/D/F/G (hold)\tBullet debug drawing"<<endl
	<<"  R\t\treset camera       C\tcamera motion on/off"<<endl
	<<"  L\t\tlight motion on/off"<<endl
	<<"  Z\t\tground shadow on/off   X\tself shadow on/off"<<endl
	<<"  Space\t\tpause/resume       O\tloop on/off"<<endl
	<<"  Left/Right\tseek 1 s (Shift: 10 s)   , .\tone frame back/forward"<<endl
	<<"  Home/End\tbeginning/end of the motion"<<endl
	<<"  Tab\t\tshow/hide the on-screen controls"<<endl
	<<"  Esc\t\tquit"<<endl<<endl
	<<"FOR JAPANESE DOCUMENTATION, SEE:"<<endl
	<<"<file://"<<DATA_PATH<<"/doc/README_JP.html>"<<endl;
}

void printVersionInfo()
{
	cout<<"PMXViewer v"<<MMP_VERSION<<endl
	<<"Copyright (C) 2014 Ben Clapp (sn0w75)"<<endl
     <<"This is free software; see the source for copying conditions.  There is NO"<<endl
     <<"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."<<endl
     <<"Written by Ben Clapp (sn0w75); modified in 2026."<<endl<<endl;
}

int main(int argc, char** argv)
{
	vector<ModelSpec> models;
	vector<string> pendingMotions; //VMDs given before their model
	string music_file="";
	string camera_file="";
	string light_file="";
	bool loop=false;
	bool gui=true;
	ViewerOptions options;
	int snapshotFrame=-1;
	string snapshotPath;
	bool render=false;
	RecordSettings record;
	bool recordOptions=false; //--range/--size/--fps without --render
	EffectSettings effects;
	
	int count=0;
	if(argc > 1)
	{
	  for (count = 1; count < argc; count++)
		{
			if(string(argv[count]) == "--help")
			{
				printHelpInfo();
				return 0;
			}
			else if(string(argv[count]) == "--version")
			{
				printVersionInfo();
				return 0;
			}
			else if(string(argv[count]) == "-s")
			{
				if(count+1<argc) music_file=argv[++count];
			}
			else if(string(argv[count]) == "--no-gui")
			{
				gui=false;
			}
			else if(string(argv[count]) == "--size")
			{
				int w=0, h=0;
				if(count+1<argc && sscanf(argv[count+1], "%dx%d", &w, &h)==2 && w>=16 && h>=16 && w<=16384 && h<=16384)
				{
					++count;
					options.width=w;   //window size
					options.height=h;
					record.width=w;    //video size with --render
					record.height=h;
				}
				else
				{
					cerr<<"--size needs WIDTHxHEIGHT"<<endl;
					return 2;
				}
			}
			else if(string(argv[count]) == "--snapshot")
			{
				char *end=NULL;
				long f=(count+2<argc) ? strtol(argv[count+1], &end, 10) : -1;
				if(count+2>=argc || f<0 || !end || *end!='\0')
				{
					cerr<<"--snapshot needs FRAME OUT.png"<<endl;
					return 2;
				}
				snapshotFrame=(int)f;
				snapshotPath=argv[count+2];
				count+=2;
			}
			else if(string(argv[count]) == "--effect" || string(argv[count]) == "--effects")
			{
				string error;
				if(count+1>=argc) { cerr<<"--effect needs a list (e.g. bloom,dof)"<<endl; return 1; }
				if(!parseEffectList(argv[++count], effects, error)) { cerr<<error<<endl; return 1; }
			}
			else if(string(argv[count]) == "--color")
			{
				string error;
				if(count+1>=argc) { cerr<<"--color needs brightness,contrast,saturation,gamma"<<endl; return 1; }
				if(!parseColorAdjust(argv[++count], effects, error)) { cerr<<error<<endl; return 1; }
			}
			else if(string(argv[count]) == "--dof-focus")
			{
				float d=0.0f;
				if(count+1>=argc || sscanf(argv[count+1], "%f", &d)!=1 || !(d>0.0f))
				{
					cerr<<"--dof-focus needs a distance > 0"<<endl;
					return 1;
				}
				++count;
				effects.dofFocus=d;
				effects.dofAutoFocus=false;
				effects.dof=true;
			}
			else if(string(argv[count]) == "--loop")
			{
				loop=true;
			}
			else if(string(argv[count]) == "-l")
			{
				if(count+1<argc) light_file=argv[++count];
			}
			else if(string(argv[count]) == "--pos")
			{
				//position of the preceding model, in MMD coordinates
				float x=0, y=0, z=0;
				if(count+1<argc && sscanf(argv[count+1], "%f,%f,%f", &x, &y, &z)>=1)
				{
					++count;
					if(models.empty()) cerr<<"--pos must follow a model file"<<endl;
					else models.back().offset=glm::vec3(x, y, -z);
				}
				else
				{
					cerr<<"--pos needs X,Y,Z"<<endl;
				}
			}
			else if(string(argv[count]) == "--render")
			{
				if(count+1>=argc) { cerr<<"--render needs an output file"<<endl; return 2; }
				render=true;
				record.output=argv[++count];
			}
			else if(string(argv[count]) == "--range")
			{
				recordOptions=true;
				string r=count+1<argc ? argv[++count] : "";
				size_t colon=r.find(':');
				int a=0, b=-1;
				char extra;
				bool ok=colon!=string::npos;
				if(ok && colon>0) ok=sscanf(r.substr(0, colon).c_str(), "%d%c", &a, &extra)==1 && a>=0;
				if(ok && colon+1<r.size()) ok=sscanf(r.substr(colon+1).c_str(), "%d%c", &b, &extra)==1 && b>=0;
				if(!ok) { cerr<<"--range needs START:END (VMD frames, e.g. 0:300)"<<endl; return 2; }
				if(b>=0 && b<a) { cerr<<"--range: the end ("<<b<<") is before the start ("<<a<<")"<<endl; return 2; }
				record.startFrame=a;
				record.endFrame=b;
			}
			else if(string(argv[count]) == "--fps")
			{
				recordOptions=true;
				string f=count+1<argc ? argv[++count] : "";
				if(f!="30" && f!="60") { cerr<<"--fps must be 30 or 60"<<endl; return 2; }
				record.fps=atoi(f.c_str());
			}
			else if(string(argv[count]) == "--no-music")
			{
				recordOptions=true;
				record.withMusic=false;
			}
			else if(string(argv[count]) == "-c")
			{
				if(count+1<argc) camera_file=argv[++count];
			}
			else
			{
				string str=string(argv[count]);
				//each VMD goes to the last model that has no motion yet
				//("model1.pmx motion1.vmd model2.pmx motion2.vmd ..."); a VMD
				//given before its model is kept for the next model
				if(hasSuffix(str, ".pmx") || hasSuffix(str, ".pmd"))
				{
					models.push_back(ModelSpec(str));
					if(!pendingMotions.empty())
					{
						models.back().motionPath=pendingMotions.front();
						pendingMotions.erase(pendingMotions.begin());
					}
				}
				else if(hasSuffix(str, ".vmd"))
				{
					if(!models.empty() && models.back().motionPath.empty()) models.back().motionPath=str;
					else pendingMotions.push_back(str);
				}
				else
				{
					cerr<<"Ignoring unknown argument: "<<str<<endl;
				}
			}
		}
	}
	//without arguments the viewer starts empty; files are opened from the
	//GUI or dropped onto the window
	
	if(render && models.empty())
	{
		cerr<<"--render needs a model (pmx [options] --render out.mp4 model.pmx motion.vmd)"<<endl;
		return 2;
	}
	if(recordOptions && !render) cerr<<"Note: --range, --fps and --no-music are only used with --render"<<endl;
	if(models.empty())
	{
		cout<<"No model given: open files from the GUI (File) or drop them onto the window."<<endl;
	}
	for(const string &m : pendingMotions)
	{
		cerr<<"WARNING: no model left for motion "<<m<<"; ignored"<<endl;
	}
	for(const ModelSpec &m : models)
	{
		if(m.motionPath.empty()) cerr<<"Note: "<<m.modelPath<<" has no motion (shown in its rest pose)"<<endl;
	}
	
	try
	{
		if(snapshotFrame>=0)
		{
			options.hidden=true;
			options.multisample=false;
			Viewer viewer(models,"",camera_file,light_file,options);
			viewer.setEffects(effects);
			return viewer.snapshot(snapshotFrame, snapshotPath) ? 0 : 1;
		}
		if(render)
		{
			options.hidden=true;
			options.offscreen=true;
		}
		Viewer viewer(models,music_file,camera_file,light_file,options);
		
		viewer.setLoop(loop);
		viewer.setGUI(gui);
		viewer.setEffects(effects);
		if(render) viewer.setRender(record);
		return viewer.run();
	}
	catch(const std::exception &e)
	{
		cerr<<"ERROR: "<<e.what()<<endl;
		return 1;
	}
	
	return 0;
}
