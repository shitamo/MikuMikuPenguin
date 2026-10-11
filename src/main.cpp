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
#include <stdexcept>
#include <vector>


#include <libmmp.h>
/*#include "texthandle.h"
#include "pmx.h"
#include "vmd.h"
#include "pmxvLogger.h"

#include "viewer.h"*/
#include "pmxvLogger.h"
#include "viewer.h"

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
		Viewer viewer(models,music_file,camera_file,light_file);
		
		viewer.setLoop(loop);
		viewer.setGUI(gui);
		viewer.run();
	}
	catch(const std::exception &e)
	{
		cerr<<"ERROR: "<<e.what()<<endl;
		delete pmxvLogger::get();
		return 1;
	}
	
	delete pmxvLogger::get();	
	return 0;
}
