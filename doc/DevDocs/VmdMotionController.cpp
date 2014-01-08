#include "StdAfx.h"
#include "VmdMotionController.h"
#include "Bezie.h"

VmdMotionController::VmdMotionController(LPCTSTR filename, vector<Bone>* b,
vector<MmdStruct::PmdIkData>* p) : bones(b), pmdIkData(p), time() {
	const int frame_rate = 60;			// –{ƒvƒƒOƒ‰ƒ€‚ÌƒtƒŒ[ƒ€ƒŒ[ƒ

	const int mmd_frame_rate = 30;		// MMD‚ÌƒtƒŒ[ƒ€ƒŒ[ƒ

	// VMDƒtƒ@ƒCƒ‹‚©‚çVMDƒf[ƒ^‚ğ’Š

    ifstream ifs(filename, ios::binary);
	if (ifs.fail()) throw TEXT("ƒtƒ@ƒCƒ‹‚ª‚ ‚è‚Ü‚¹‚ñ"

	MmdStruct::VmdHeader vmdHeader;
	ifs.read((char*)&vmdHeader, sizeof(MmdStruct::VmdHeader));
	unsigned long numVmdMotion;
	ifs.read((char*)&numVmdMotion, sizeof(numVmdMotion));
	vector<MmdStruct::VmdMotion> vmdMotions(numVmdMotion);
	ifs.read((char*)&vmdMotions[0],
sizeof(MmdStruct::VmdMotion)*numVmdMotion);
	// KeyFrames‚ÉŠi”

	keyFrames.resize(bones->size());
	for (unsigned int i = 0; i < vmdMotions.size(); ++i) {
		KeyFrame keyFrame;
		keyFrame.boneName = vmdMotions[i].boneName;
		keyFrame.frameNo = vmdMotions[i].frameNo;
		keyFrame.frameNo *= frame_rate/mmd_frame_rate;
		keyFrame.position = D3DXVECTOR3(vmdMotions[i].location[0],
vmdMotions[i].location[1], vmdMotions[i].location[2]);
		keyFrame.rotation = D3DXQUATERNION(vmdMotions[i].rotation[0],
vmdMotions[i].rotation[1], vmdMotions[i].rotation[2],
vmdMotions[i].rotation[3]);
		keyFrame.bezie_x = new Bezie(vmdMotions[i].interpolation[0],
vmdMotions[i].interpolation[4], vmdMotions[i].interpolation[8],
vmdMotions[i].interpolation[12]);
		keyFrame.bezie_y = new Bezie(vmdMotions[i].interpolation[1],
vmdMotions[i].interpolation[5], vmdMotions[i].interpolation[9],
vmdMotions[i].interpolation[13]);
		keyFrame.bezie_z = new Bezie(vmdMotions[i].interpolation[2],
vmdMotions[i].interpolation[6], vmdMotions[i].interpolation[10],
vmdMotions[i].interpolation[14]);
		keyFrame.bezie_r = new Bezie(vmdMotions[i].interpolation[3],
vmdMotions[i].interpolation[7], vmdMotions[i].interpolation[11],
vmdMotions[i].interpolation[15]);
		for (unsigned int j = 0; j < bones->size(); ++j) {
			if (keyFrame.boneName == (*bones)[j].name) {	//
ƒ{[ƒ“–¼‚©‚çƒ{[ƒ“”Ô†‚ğ’
·
				keyFrames[j].push_back(keyFrame);
				break;
			}
		}
	}
	for (unsigned int i = 0; i < bones->size(); ++i) {
		keyFrames[i].sort();
		ite_keyFrames.push_back(keyFrames[i].begin());
		boneRot.push_back(D3DXQUATERNION(0, 0, 0, 0));
		bonePos.push_back(D3DXVECTOR3(0, 0, 0));
	}
	UpdateBoneMatrix();
}

VmdMotionController::~VmdMotionController() {
	for (unsigned int i = 0; i < keyFrames.size(); ++i) {
		for (list<KeyFrame>::iterator ite = keyFrames[i].begin(); ite !=
keyFrames[i].end(); ++ite) {
			SAFE_DELETE((*ite).bezie_x);
			SAFE_DELETE((*ite).bezie_y);
			SAFE_DELETE((*ite).bezie_z);
			SAFE_DELETE((*ite).bezie_r);
		}
	}
}

void VmdMotionController::UpdateBoneMatrix() {
	for (unsigned int i = 0; i < bones->size(); i++) {
		// ƒL[ƒtƒŒ[ƒ€•â

		unsigned long t0, t1;
		D3DXQUATERNION q0, q1;
		D3DXVECTOR3 p0, p1;
		if (ite_keyFrames[i] != keyFrames[i].end()) {
			t0 = (*ite_keyFrames[i]).frameNo;
			boneRot[i] = q0 = (*ite_keyFrames[i]).rotation;
			bonePos[i] = p0 = (*ite_keyFrames[i]).position;
			if (++ite_keyFrames[i] != keyFrames[i].end()) {
				KeyFrame k = *ite_keyFrames[i];
				t1 = k.frameNo;
				q1 = k.rotation;
				p1 = k.position;
				float s = (float)(time - t0)/(float)(t1 - t0);
				D3DXQuaternionSlerp(&boneRot[i], &q0, &q1, k.bezie_r->GetY(s));
				bonePos[i].x = p0.x + (p1.x - p0.x)*k.bezie_x->GetY(s);
				bonePos[i].y = p0.y + (p1.y - p0.y)*k.bezie_y->GetY(s);
				bonePos[i].z = p0.z + (p1.z - p0.z)*k.bezie_z->GetY(s);
				if (time != t1) --ite_keyFrames[i];
			}
		}
		// eƒ{[ƒ“À•WŒn‚Ìƒ{[ƒ“s—ñ‚ğ‹
‚é
		D3DXMATRIX rot, trans;
		D3DXMatrixRotationQuaternion(&rot, &boneRot[i]);
		D3DXMatrixTranslation(&trans, bonePos[i].x, bonePos[i].y, bonePos[i].z);
		(*bones)[i].boneMatBL = rot*trans*(*bones)[i].initMatBL;
	}
	for (unsigned int i = 0; i < pmdIkData->size(); ++i)
UpdateIK((*pmdIkData)[i]);
}

void VmdMotionController::UpdateIK(const MmdStruct::PmdIkData& ikData) {
	D3DXVECTOR3 localEffectorPos, localTargetPos;
	for (unsigned int j = 0; j < ikData.iterations; ++j) {
		for (unsigned int i = 0; i < ikData.ik_child_bone_index.size(); ++i) {
			unsigned short attentionIdx = ikData.ik_child_bone_index[i];
			D3DXVECTOR3
effectorPos((*bones)[ikData.ik_target_bone_index].CalBoneMatML().m[3]);	//
ƒGƒtƒFƒNƒ^(ƒ^[ƒQƒbƒgƒ{[ƒ“)‚ÌˆÊ

			D3DXVECTOR3
targetPos((*bones)[ikData.ik_bone_index].CalBoneMatML().m[3]);			//
ƒ^[ƒQƒbƒg(IKƒ{[ƒ“)‚ÌˆÊ

			D3DXMATRIX invCoord;																	//
’–Úƒ{[ƒ“‚Ìƒ{[ƒ“s—ñ‚Ì‹t
—ñ
			D3DXMatrixInverse(&invCoord, 0,
&(*bones)[attentionIdx].CalBoneMatML());
			D3DXVec3TransformCoord(&localEffectorPos, &effectorPos, &invCoord);
// ’–Úƒ{[ƒ“Šî€‚
ÏŠ

			D3DXVec3TransformCoord(&localTargetPos, &targetPos, &invCoord);
// ’–Úƒ{[ƒ“Šî€‚
ÏŠ

			D3DXVECTOR3 localEffectorDir;															//
ƒGƒtƒFƒNƒ^‚Ìƒ[ƒJƒ‹•ûŒüi’–Úƒ{[ƒ“Šî
j
			D3DXVec3Normalize(&localEffectorDir, &localEffectorPos);
			D3DXVECTOR3 localTargetDir;																//
ƒ^[ƒQƒbƒg‚Ìƒ[ƒJƒ‹•ûŒüi’–Úƒ{[ƒ“Šî
j
			D3DXVec3Normalize(&localTargetDir, &localTargetPos);
			if ((*bones)[attentionIdx].name.find("‚Ğ‚´") != string::npos) 

				localEffectorDir = D3DXVECTOR3(0, localEffectorDir.y,
localEffectorDir.z);
				D3DXVec3Normalize(&localEffectorDir, &localEffectorDir);
				localTargetDir = D3DXVECTOR3(0, localTargetDir.y, localTargetDir.z);
				D3DXVec3Normalize(&localTargetDir, &localTargetDir);
			}
			float p = D3DXVec3Dot(&localEffectorDir, &localTargetDir);
			if (p > 1 - 1.0e-8f) continue;	//
ŒvZŒë·‚É‚æ‚è1‚ğ‰z‚¦‚é‚Æacos()‚ª”­U‚·‚é
‚Å’ˆÓ

			float angle = acos(p);
			if (angle > 4*ikData.control_weight) angle =
4.0f*ikData.control_weight;
			D3DXVECTOR3 axis;
			D3DXVec3Cross(&axis, &localEffectorDir, &localTargetDir);
			D3DXMATRIX rotation;
			D3DXMatrixRotationAxis(&rotation, &axis, angle);
			if ((*bones)[attentionIdx].name.find("‚Ğ‚´") != string::npos) 

				D3DXMATRIX inv;
				D3DXMatrixInverse(&inv, 0, &(*bones)[attentionIdx].initMatBL);
				D3DXMATRIX def = rotation*(*bones)[attentionIdx].boneMatBL*inv;
				D3DXVECTOR3 t(0, 0, 1);
				D3DXVec3TransformCoord(&t, &t, &def);
				if (t.y < 0) D3DXMatrixRotationAxis(&rotation, &axis, -angle);
				// •Gƒ{[ƒ“‚ªƒGƒtƒFƒNƒ^(ƒ^[ƒQƒbƒgƒ{[ƒ“)‚æ‚è‹ß‚¢‚Í‰ñ“]—Ê‚ğ
‰Á‚·‚é
				float l =
D3DXVec3Length(&localTargetPos)/D3DXVec3Length(&localEffectorPos);
				if (fabs(angle) <= D3DX_PI/2 && l < 1.0f) {
					static const float a = 0.5f;	// ’Ç‰Á—Ê‚Ì”ä—
W”
					float diff = acosf(l)*a;		// ’Ç‰Á—

					static const float diff_limit = D3DX_PI/6;	// ’Ç‰Á—Ê‚Ì
À
					if (diff > diff_limit) {
						diff = diff_limit;
					}
					if(fabs(angle) > 1.0e-6f) diff *= angle/fabs(angle);	// •„†‡‚í

					angle += diff;
				}
			}
			(*bones)[attentionIdx].boneMatBL =
rotation*(*bones)[attentionIdx].boneMatBL;
		}
		const float errToleranceSq = 0.000001f;
		if (D3DXVec3LengthSq(&(localEffectorPos - localTargetPos)) <
errToleranceSq) {
			return;
		}
	}
}

void VmdMotionController::AdvanceTime() { ++time; }

