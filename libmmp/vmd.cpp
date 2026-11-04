#include "vmd.h"
#include <stdexcept>

#include <fstream>
#include <iostream>
#include <bitset>
#include <sstream>

#include "texthandle.h"
#include "binreader.h"

#include <memory>

#include <string.h>
#include <cstdint>
#include <cstdlib>

using namespace std;

namespace ClosedMMDFormat
{

	namespace
	{
		//Number of records of a section.  Files may end before any section
		//(older MMD versions write fewer sections), which means "none"; a
		//count that the rest of the file cannot hold is an error.
		uint32_t sectionCount(BinaryReader &r, size_t recordSize, const char *name)
		{
			if(r.remaining()<4) { r.skip(r.remaining()); return 0; }
			return r.checkCount(r.u32(), recordSize, name);
		}

		string fixedText(BinaryReader &r, size_t n)
		{
			char buf[32]={0};
			r.raw(buf, n);
			return sjisToUTF8(buf, n);
		}
	}

	VMDInfo &readVMD(string filename)
	{
		std::vector<char> data=readWholeFile(filename, "VMD");
		BinaryReader r(data, BinaryReader::Mode::Throw, "VMD file", filename);
		std::unique_ptr<VMDInfo> vInfo(new VMDInfo());
		VMDInfo &vmdInfo = *vInfo;
		
		//***Extract Header Info***
		char headerStr[30] = {0};
		r.raw(headerStr, 30);
		if(memcmp(headerStr, "Vocaloid Motion Data", 20)!=0) r.fail("not a VMD file");
		//version 1 ("Vocaloid Motion Data file") has a 10-byte model name
		bool version1=memcmp(headerStr, "Vocaloid Motion Data file", 25)==0;
		vmdInfo.modelName=fixedText(r, version1 ? 10 : 20);
		
		cout<<std::string(headerStr, strnlen(headerStr, sizeof(headerStr)))<<endl;
		
		//***Extract Bone Info***
		vmdInfo.boneFrames.resize(sectionCount(r, 15+4+12+16+64, "bone frame"));
		cout<<"Loading bone frames...";
		for(VMDBoneFrame &frame : vmdInfo.boneFrames)
		{
			VMDBoneFrame *f = &frame;
			f->name=fixedText(r, 15);
			f->frame=r.u32();
			
			f->translation.x=r.f32();
			f->translation.y=r.f32();
			f->translation.z=r.f32();
			f->translation.z = -f->translation.z;
			
			f->rotation.x=r.f32();
			f->rotation.y=r.f32();
			f->rotation.z=r.f32();
			f->rotation.w=r.f32();
			glm::mat3 rotMatrix=glm::toMat3(f->rotation);
			rotMatrix[2][0] = -rotMatrix[2][0];
			rotMatrix[2][1] = -rotMatrix[2][1];
			rotMatrix[0][2] = -rotMatrix[0][2];
			rotMatrix[1][2] = -rotMatrix[1][2];
			f->rotation=glm::toQuat(rotMatrix);
			
			uint8_t bezier[64];
			r.raw(bezier, 64);
			
			//Cubic bezier variables
			//line 1
			f->bezier.X1.x = bezier[0];
			f->bezier.Y1.x = bezier[1];
			f->bezier.Z1.x = bezier[2];
			f->bezier.R1.x = bezier[3];
			
			f->bezier.X1.y = bezier[4];
			f->bezier.Y1.y = bezier[5];
			f->bezier.Z1.y = bezier[6];
			f->bezier.R1.y = bezier[7];
			
			//line 2
			f->bezier.X2.x = bezier[8];
			f->bezier.Y2.x = bezier[9];
			f->bezier.Z2.x = bezier[10];
			f->bezier.R2.x = bezier[11];
			
			f->bezier.X2.y = bezier[12];
			f->bezier.Y2.y = bezier[13];
			f->bezier.Z2.y = bezier[14];
			f->bezier.R2.y = bezier[15];
		}
		cout<<"done."<<endl;
		
		//***Extract Morph Info***
		vmdInfo.morphFrames.resize(sectionCount(r, 15+4+4, "morph frame"));
		cout<<"Loading morph frames...";
		for(VMDMorphFrame &f : vmdInfo.morphFrames)
		{
			f.name=fixedText(r, 15);
			f.frame=r.u32();
			f.value=r.f32();
		}
		cout<<"done."<<endl;
		
		//***Extract Camera Info***
		vmdInfo.cameraFrames.resize(sectionCount(r, 61, "camera frame"));
		cout<<"Loading camera frames...";
		for(VMDCameraFrame &frame : vmdInfo.cameraFrames)
		{
			VMDCameraFrame *f = &frame;
			//61 bytes: frame, distance, interest point, rotation (radians),
			//24 bytes of interpolation, view angle (degrees), perspective flag
			f->frame=r.u32();
			f->distance=r.f32();
			
			f->position.x=r.f32();
			f->position.y=r.f32();
			f->position.z=r.f32();
			f->position.z = -f->position.z; //Direct3D -> OpenGL
			
			//Euler angles in radians, kept as stored in the file; see
			//VMDCameraController for how they are applied.
			f->rotation.x=r.f32();
			f->rotation.y=r.f32();
			f->rotation.z=r.f32();
			
			r.raw(f->interpolationParameters, 24);
			
			f->viewAngle=r.u32();
			f->perspective = r.flag(); //0: perspective ON, 1: OFF (as stored)
		}
		cout<<"done."<<endl;	
		
		//***Extract Light Info***
		vmdInfo.lightFrames.resize(sectionCount(r, 28, "light frame"));
		cout<<"Loading light frames...";
		for(VMDLightFrame &f : vmdInfo.lightFrames)
		{
			f.frame=r.u32();
			f.color.r=r.f32();
			f.color.g=r.f32();
			f.color.b=r.f32();
			f.position.x=r.f32();
			f.position.y=r.f32();
			f.position.z=r.f32();
			f.position.z = -f.position.z;
		}
		cout<<"done."<<endl;
	
		//***Extract Self Shadow Info***
		vmdInfo.selfShadowFrames.resize(sectionCount(r, 9, "self shadow frame"));
		cout<<"Loading SelfShadow frames...";
		for(VMDSelfShadowFrame &f : vmdInfo.selfShadowFrames)
		{
			f.frame=r.u32();
			f.type=r.u8();
			f.distance=r.f32();
		}
		cout<<"done."<<endl;
		
		//***Extract Show IK Frame Info***
		vmdInfo.showIKFrames.resize(sectionCount(r, 9, "show/IK frame"));
		cout<<"Loading ShowIK frames...";
		for(VMDShowIKFrame &f : vmdInfo.showIKFrames)
		{
			f.frame=r.u32();
			f.show=r.flag();
			f.IKList.resize(r.checkCount(r.u32(), 21, "IK state"));
			for(VMDIKInfo &info : f.IKList)
			{
				info.name=fixedText(r, 20);
				info.isOn=r.flag();
			}
		}
		cout<<"done."<<endl;

		cout<<endl;
		
		return *vInfo.release();
	}
	
	void writeVMD(VMDInfo &vmdInfo, string filename)
	{
		ofstream miku(filename.c_str(), ios::out | ios::binary);
		if (!miku) { { std::ostringstream es_; es_<<"ERROR: cannot write VMD file: "<<filename; cerr<<es_.str()<<endl; throw std::runtime_error(es_.str()); } }
		
		static const char headerStr[30]="Vocaloid Motion Data 0002\0\0\0\0"; //The last '\0' is implied
		string modelName=UTF8ToSJIS(vmdInfo.modelName.c_str()); //TODO: Convert UTF8 string back to SHIFT_JIS!!!

		miku.write(headerStr,30);
		modelName.resize(20, '\0'); miku.write(modelName.data(), 20); //pad/truncate to the fixed field size
		
		
		unsigned boneCount=vmdInfo.boneFrames.size();
		miku.write((char*)&boneCount,4);
		
		for(unsigned i=0; i<vmdInfo.boneFrames.size(); ++i)
		{
			VMDBoneFrame *f = &vmdInfo.boneFrames[i];
			
			string name=UTF8ToSJIS(f->name.c_str());
			
			glm::vec3 translation=f->translation;
			translation.z=-translation.z; //Flip Z-axis back to DirectX (left-handed) coordinate form
			
			glm::quat rotation;
			
			//Flip Quaternion back to DirectX (left-handed) coordinate form
			glm::mat3 rotMatrix=glm::toMat3(f->rotation);
			rotMatrix[2][0] = -rotMatrix[2][0];
			rotMatrix[2][1] = -rotMatrix[2][1];
			rotMatrix[0][2] = -rotMatrix[0][2];
			rotMatrix[1][2] = -rotMatrix[1][2];
			rotation=glm::toQuat(rotMatrix);
			
			uint8_t bezier[64];
			//VMD stores bezier parameters in a really weird/meaningless way. For details:
			//http://harigane.at.webry.info/201103/article_1.html
			bezier[ 0] = f->bezier.X1.x;
			bezier[ 1] = bezier[16] = f->bezier.Y1.x;
			bezier[ 2] = bezier[17] = bezier[32] = f->bezier.Z1.x;
			bezier[ 3] = bezier[18] = bezier[33] = bezier[48] = f->bezier.R1.x;
			
			bezier[ 4] = bezier[19] = bezier[34] = bezier[49] = f->bezier.X1.y;
			bezier[ 5] = bezier[20] = bezier[35] = bezier[50] = f->bezier.Y1.y;
			bezier[ 6] = bezier[21] = bezier[36] = bezier[51] = f->bezier.Z1.y;
			bezier[ 7] = bezier[22] = bezier[37] = bezier[52] = f->bezier.R1.y;
			
			//line 2
			bezier[ 8] = bezier[23] = bezier[38] = bezier[53] = f->bezier.X2.x;
			bezier[ 9] = bezier[24] = bezier[39] = bezier[54] = f->bezier.Y2.x;
			bezier[10] = bezier[25] = bezier[40] = bezier[55] = f->bezier.Z2.x;
			bezier[11] = bezier[26] = bezier[41] = bezier[56] = f->bezier.R2.x;
			
			bezier[12] = bezier[27] = bezier[42] = bezier[57] = f->bezier.X2.y;
			bezier[13] = bezier[28] = bezier[43] = bezier[58] = f->bezier.Y2.y;
			bezier[14] = bezier[29] = bezier[44] = bezier[59] = f->bezier.Z2.y;
			bezier[15] = bezier[30] = bezier[45] = bezier[60] = f->bezier.R2.y;
			
						 bezier[31] = bezier[46] = bezier[61] = 01;
									  bezier[47] = bezier[62] = bezier[63] = 00;
			
			
			name.resize(15, '\0'); miku.write(name.data(), 15); //pad/truncate to the fixed field size
			
			miku.write((char*)&f->frame,4);
			miku.write((char*)&translation.x,4);
			miku.write((char*)&translation.y,4);
			miku.write((char*)&translation.z,4);
			
			//Write each axis individually to ensure correct order
			miku.write((char*)&rotation.x, 4);
			miku.write((char*)&rotation.y, 4);
			miku.write((char*)&rotation.z, 4);
			miku.write((char*)&rotation.w, 4);
			
			miku.write((char*)&bezier,64);
		}
		
		unsigned morphCount=vmdInfo.morphFrames.size();
		miku.write((char*)&morphCount,4);
		
		for(unsigned i=0; i<vmdInfo.morphFrames.size(); ++i)
		{
			VMDMorphFrame *f = &vmdInfo.morphFrames[i];
			
			string name=UTF8ToSJIS(f->name.c_str()).c_str();
			
			name.resize(15, '\0'); miku.write(name.data(), 15); //pad/truncate to the fixed field size
			miku.write((char*)&f->frame, 4);
			miku.write((char*)&f->value, 4);
		}
		
		unsigned cameraCount=vmdInfo.cameraFrames.size();
		miku.write((char*)&cameraCount,4);
		
		for(unsigned i=0; i<vmdInfo.cameraFrames.size(); ++i)
		{
			VMDCameraFrame *f = &vmdInfo.cameraFrames[i];
			
			glm::vec3 position=f->position;
			position.z=-position.z; //Flip Z-axis back to DirectX (left-handed) coordinate form
			
			miku.write((char*)&f->frame,4);
			miku.write((char*)&f->distance,4);
			miku.write((char*)&position.x,4);
			miku.write((char*)&position.y,4);
			miku.write((char*)&position.z,4);
			miku.write((char*)&f->rotation.x,4);
			miku.write((char*)&f->rotation.y,4);
			miku.write((char*)&f->rotation.z,4);
			miku.write(f->interpolationParameters,24);
			miku.write((char*)&f->viewAngle,4);
			uint8_t perspective = f->perspective ? 1 : 0;
			miku.write((char*)&perspective,1);
		}
		
		unsigned lightCount=vmdInfo.lightFrames.size();
		miku.write((char*)&lightCount,4);
		
		for(unsigned i=0; i<vmdInfo.lightFrames.size(); ++i)
		{
			VMDLightFrame *f = &vmdInfo.lightFrames[i];
			
			glm::vec3 position=f->position;
			position.z=-position.z;
			
			miku.write((char*)&f->frame, 4);
			
			miku.write((char*)&f->color.r, 4);
			miku.write((char*)&f->color.g, 4);
			miku.write((char*)&f->color.b, 4);

			miku.write((char*)&position.x, 4);
			miku.write((char*)&position.y, 4);
			miku.write((char*)&position.z, 4);
		}
		
		//***Extract Self Shadow Info***
		unsigned selfShadowCount=vmdInfo.selfShadowFrames.size();
		miku.write((char*)&selfShadowCount,4);

		for(int i=0; i<vmdInfo.selfShadowFrames.size(); ++i)
		{
			VMDSelfShadowFrame *f = &vmdInfo.selfShadowFrames[i];
			
			miku.write((char*)&f->frame, 4);
			miku.write((char*)&f->type, 1);
			miku.write((char*)&f->distance, 4);
		}
		
		//***Extract Show IK Frame Info***
		unsigned showIKCount=vmdInfo.showIKFrames.size();
		miku.write((char*)&showIKCount, 4);
		
		for(int i=0; i < vmdInfo.showIKFrames.size(); ++i)
		{
			VMDShowIKFrame *f = &vmdInfo.showIKFrames[i];
			
			miku.write((char*)&f->frame, 4);
			miku.write((char*)&f->show, 1);
			
			
			unsigned IKCount=f->IKList.size();
			miku.write((char*)&IKCount, 4);
			
			for(int i=0; i<f->IKList.size(); ++i)
			{
				VMDIKInfo *info = &f->IKList[i];
				
				string name=UTF8ToSJIS(info->name.c_str());
				
				name.resize(20, '\0'); miku.write(name.data(), 20); //pad/truncate to the fixed field size
				miku.write((char*)&info->isOn, 1);
			}
		}
		
		miku.close();
	}
}
