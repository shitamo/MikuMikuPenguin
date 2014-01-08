		for(int j=0;j<p_IK[i].loopNum;j++){
			int ikt = p_IK[i].loopNum / 2;
			for(int k=0;k<p_IK[i].sBoneNum;k++){
				// calculate IK bone position
				int tv=p_IK[i].dstBone;
				D3DVECTOR dbp;
				dbp.x=p_bone[tv].mat._11 * p_bone[tv].pos.x
					+ p_bone[tv].mat._21 * p_bone[tv].pos.y
					+ p_bone[tv].mat._31 * p_bone[tv].pos.z + p_bone[tv].mat._41;
				dbp.y=p_bone[tv].mat._12 * p_bone[tv].pos.x
					+ p_bone[tv].mat._22 * p_bone[tv].pos.y
					+ p_bone[tv].mat._32 * p_bone[tv].pos.z + p_bone[tv].mat._42;
				dbp.z=p_bone[tv].mat._13 * p_bone[tv].pos.x
					+ p_bone[tv].mat._23 * p_bone[tv].pos.y
					+ p_bone[tv].mat._33 * p_bone[tv].pos.z + p_bone[tv].mat._43;
				// calculate [k]th bone position
				tv=p_IK[i].sBone[k];
				D3DVECTOR kbp;
				kbp.x=p_bone[tv].mat._11 * p_bone[tv].pos.x
					+ p_bone[tv].mat._21 * p_bone[tv].pos.y
					+ p_bone[tv].mat._31 * p_bone[tv].pos.z + p_bone[tv].mat._41;
				kbp.y=p_bone[tv].mat._12 * p_bone[tv].pos.x
					+ p_bone[tv].mat._22 * p_bone[tv].pos.y
					+ p_bone[tv].mat._32 * p_bone[tv].pos.z + p_bone[tv].mat._42;
				kbp.z=p_bone[tv].mat._13 * p_bone[tv].pos.x
					+ p_bone[tv].mat._23 * p_bone[tv].pos.y
					+ p_bone[tv].mat._33 * p_bone[tv].pos.z + p_bone[tv].mat._43;

				// calculate [k]th bone quaternion
				D3DXVECTOR3 v1;
				v1.x= kbp.x - dbp.x;
				v1.y= kbp.y - dbp.y;
				v1.z= kbp.z - dbp.z;
				D3DXVECTOR3 v2;
				v2.x= kbp.x - ikp.x;
				v2.y= kbp.y - ikp.y;
				v2.z= kbp.z - ikp.z;

				D3DXVec3Normalize(&v1,&v1);
				D3DXVec3Normalize(&v2,&v2);
				if((v1.x-v2.x)*(v1.x-v2.x)+(v1.y-v2.y)*(v1.y-v2.y)+(v1.z-v2.z)*(v1.z-v2.z)<0.0000001f) break;

				D3DXVECTOR3 v;
				D3DXVec3Cross(&v,&v1,&v2);
				// 角度制限
				if(pmxv==2){
					if(p_bone[tv].rflag && j<ikt){
						if(p_bone[tv].rdlimit.y==0 && p_bone[tv].rulimit.y==0 && p_bone[tv].rdlimit.z==0 && p_bone[tv].rulimit.z==0){
							float vvx=v.x*p_bone[tv].p_mat._11+v.y*p_bone[tv].p_mat._12+v.z*p_bone[tv].p_mat._13;
							if(vvx>=0.0f) v.x=1.0f;
							else		  v.x=-1.0f;
							v.y=0.0f;
							v.z=0.0f;
						}else if(p_bone[tv].rdlimit.x==0 && p_bone[tv].rulimit.x==0 && p_bone[tv].rdlimit.z==0 && p_bone[tv].rulimit.z==0){
							float vvy=v.x*p_bone[tv].p_mat._21+v.y*p_bone[tv].p_mat._22+v.z*p_bone[tv].p_mat._23;
							if(vvy>=0.0f) v.y=1.0f;
							else		  v.y=-1.0f;
							v.x=0.0f;
							v.z=0.0f;
						}else if(p_bone[tv].rdlimit.x==0 && p_bone[tv].rulimit.x==0 && p_bone[tv].rdlimit.y==0 && p_bone[tv].rulimit.y==0){
							float vvz=v.x*p_bone[tv].p_mat._31+v.y*p_bone[tv].p_mat._32+v.z*p_bone[tv].p_mat._33;
							if(vvz>=0.0f) v.z=1.0f;
							else		  v.z=-1.0f;
							v.x=0.0f;
							v.y=0.0f;
						}else{
							// calculate roll axis
							D3DXVECTOR3 vv;
							vv.x = v.x*p_bone[tv].p_mat._11+v.y*p_bone[tv].p_mat._12+v.z*p_bone[tv].p_mat._13;
							vv.y = v.x*p_bone[tv].p_mat._21+v.y*p_bone[tv].p_mat._22+v.z*p_bone[tv].p_mat._23;
							vv.z = v.x*p_bone[tv].p_mat._31+v.y*p_bone[tv].p_mat._32+v.z*p_bone[tv].p_mat._33;
							D3DXVec3Normalize(&v,&vv);
						}
					}else{
						// calculate roll axis
						D3DXVECTOR3 vv;
						vv.x = v.x*p_bone[tv].p_mat._11+v.y*p_bone[tv].p_mat._12+v.z*p_bone[tv].p_mat._13;
						vv.y = v.x*p_bone[tv].p_mat._21+v.y*p_bone[tv].p_mat._22+v.z*p_bone[tv].p_mat._23;
						vv.z = v.x*p_bone[tv].p_mat._31+v.y*p_bone[tv].p_mat._32+v.z*p_bone[tv].p_mat._33;
						D3DXVec3Normalize(&v,&vv);
					}
				}else{
					if(((strncmp(p_bone[tv].name,"右ひざ",6)==0)|(strncmp(p_bone[tv].name,"左ひざ",6)==0))){
						// if bone=knee roll only x-axis +
						float vvx=v.x*p_bone[tv].p_mat._11+v.y*p_bone[tv].p_mat._12+v.z*p_bone[tv].p_mat._13;
						if(vvx>=0.0f) v.x=1.0f;
						else		  v.x=-1.0f;
						v.y=0.0f;
						v.z=0.0f;
					}else{
						// calculate roll axis
						D3DXVECTOR3 vv;
						vv.x = v.x*p_bone[tv].p_mat._11+v.y*p_bone[tv].p_mat._12+v.z*p_bone[tv].p_mat._13;
						vv.y = v.x*p_bone[tv].p_mat._21+v.y*p_bone[tv].p_mat._22+v.z*p_bone[tv].p_mat._23;
						vv.z = v.x*p_bone[tv].p_mat._31+v.y*p_bone[tv].p_mat._32+v.z*p_bone[tv].p_mat._33;

						D3DXVec3Normalize(&v,&vv);
					}
				}
				// calculate roll angle of [k]th bone(limited by p_IK[i].dlimit*(k+1)*2)
				float nais=v1.x*v2.x+v1.y*v2.y+v1.z*v2.z;
				if(nais>1.0f) nais=1.0f;
				if(nais<-1.0f) nais=-1.0f;
				float IKrot = 0.5f * acosf(nais);
				if(IKrot>p_IK[i].dlimit*(k+1)*2) IKrot=p_IK[i].dlimit*(k+1)*2;
				D3DXQUATERNION qIK;
				float IKsin=sinf(IKrot);
				qIK.x = v.x*IKsin;
				qIK.y = v.y*IKsin;
				qIK.z = v.z*IKsin;
				qIK.w = cosf(IKrot);

				p_bone[tv].IKQuat= p_bone[tv].IKQuat * qIK;
				if(j==0){
					p_bone[tv].IKQuat=p_bone[tv].BQuat * p_bone[tv].IKQuat;
				}
				// 角度制限
				D3DXMatrixRotationQuaternion(&temp,&p_bone[tv].IKQuat);
				if(pmxv==2){
					if(p_bone[tv].rflag){
						// 軸回転角度を算出
						if((p_bone[tv].rdlimit.x>-1.570796f)&(p_bone[tv].rulimit.x<1.570796f)){
							// Z*X*Y順
							// X軸回り
							float fLimit = 1.535889f;		// 88.0f/180.0f*3.14159265f;
							float fSX = -temp._32;			// sin(θx)
							float fX  = (float)asin(fSX);	// X軸回り決定
							float fCX = (float)cos(fX);

							// ジンバルロック回避
							if(fabs(fX) > fLimit){
								fX = (fX<0)?- fLimit: fLimit;
								fCX = (float)cos(fX);
							}

							// Y軸回り
							float fSY = temp._31 / fCX;
							float fCY = temp._33 / fCX;
							float fY = (FLOAT)atan2(fSY, fCY);	// Y軸回り決定

							// Z軸回り
							float fSZ = temp._12 / fCX;
							float fCZ = temp._22 / fCX;
							float fZ = (FLOAT)atan2(fSZ, fCZ);

							// 角度の制限
							if(fX < p_bone[tv].rdlimit.x){
								float tf= 2 * p_bone[tv].rdlimit.x - fX;
								if(tf <= p_bone[tv].rulimit.x && j<ikt) fX = tf;
								else						   fX = p_bone[tv].rdlimit.x;
							}
							if(fX > p_bone[tv].rulimit.x){
								float tf= 2 * p_bone[tv].rulimit.x - fX;
								if(tf >= p_bone[tv].rdlimit.x && j<ikt) fX = tf;
								else						   fX = p_bone[tv].rulimit.x;
							}
							if(fY < p_bone[tv].rdlimit.y){
								float tf= 2 * p_bone[tv].rdlimit.y - fY;
								if(tf <= p_bone[tv].rulimit.y && j<ikt) fY = tf;
								else						   fY = p_bone[tv].rdlimit.y;
							}
							if(fY > p_bone[tv].rulimit.y){
								float tf= 2 * p_bone[tv].rulimit.y - fY;
								if(tf >= p_bone[tv].rdlimit.y && j<ikt) fY = tf;
								else						   fY = p_bone[tv].rulimit.y;
							}
							if(fZ < p_bone[tv].rdlimit.z){
								float tf= 2 * p_bone[tv].rdlimit.z - fZ;
								if(tf <= p_bone[tv].rulimit.z && j<ikt) fZ = tf;
								else						   fZ = p_bone[tv].rdlimit.z;
							}
							if(fZ > p_bone[tv].rulimit.z){
								float tf= 2 * p_bone[tv].rulimit.z - fZ;
								if(tf >= p_bone[tv].rdlimit.z && j<ikt) fZ = tf;
								else						   fZ = p_bone[tv].rulimit.z;
							}

							// 決定した角度でベクトルを回転
							D3DXMATRIX mX,mY,mZ;
							D3DXMatrixRotationX(&mX, fX);
							D3DXMatrixRotationY(&mY, fY);
							D3DXMatrixRotationZ(&mZ, fZ);
							temp = mZ * mX * mY;
						}else if((p_bone[tv].rdlimit.y>-1.570796f)&(p_bone[tv].rulimit.y<1.570796f)){
							// X*Y*Z順
							// Y軸回り
							float fLimit = 1.535889f;		// 88.0f/180.0f*3.14159265f;
							float fSY = -temp._13;			// sin(θy)
							float fY  = (float)asin(fSY);	// Y軸回り決定
							float fCY = (float)cos(fY);

							// ジンバルロック回避
							if(fabs(fY) > fLimit){
								fY = (fY<0)?- fLimit: fLimit;
								fCY = (float)cos(fY);
							}

							// X軸回り
							float fSX = temp._23 / fCY;
							float fCX = temp._33 / fCY;
							float fX  = (FLOAT)atan2(fSX, fCX);	// X軸回り決定

							// Z軸回り
							float fSZ = temp._12 / fCY;
							float fCZ = temp._11 / fCY;
							float fZ  = (FLOAT)atan2(fSZ, fCZ);	// Z軸回り決定

							// 角度の制限
							if(fX < p_bone[tv].rdlimit.x){
								float tf= 2 * p_bone[tv].rdlimit.x - fX;
								if(tf <= p_bone[tv].rulimit.x && j<ikt) fX = tf;
								else						   fX = p_bone[tv].rdlimit.x;
							}
							if(fX > p_bone[tv].rulimit.x){
								float tf= 2 * p_bone[tv].rulimit.x - fX;
								if(tf >= p_bone[tv].rdlimit.x && j<ikt) fX = tf;
								else						   fX = p_bone[tv].rulimit.x;
							}
							if(fY < p_bone[tv].rdlimit.y){
								float tf= 2 * p_bone[tv].rdlimit.y - fY;
								if(tf <= p_bone[tv].rulimit.y && j<ikt) fY = tf;
								else						   fY = p_bone[tv].rdlimit.y;
							}
							if(fY > p_bone[tv].rulimit.y){
								float tf= 2 * p_bone[tv].rulimit.y - fY;
								if(tf >= p_bone[tv].rdlimit.y && j<ikt) fY = tf;
								else						   fY = p_bone[tv].rulimit.y;
							}
							if(fZ < p_bone[tv].rdlimit.z){
								float tf= 2 * p_bone[tv].rdlimit.z - fZ;
								if(tf <= p_bone[tv].rulimit.z && j<ikt) fZ = tf;
								else						   fZ = p_bone[tv].rdlimit.z;
							}
							if(fZ > p_bone[tv].rulimit.z){
								float tf= 2 * p_bone[tv].rulimit.z - fZ;
								if(tf >= p_bone[tv].rdlimit.z && j<ikt) fZ = tf;
								else						   fZ = p_bone[tv].rulimit.z;
							}

							// 決定した角度でベクトルを回転
							D3DXMATRIX mX,mY,mZ;
							D3DXMatrixRotationX(&mX, fX);
							D3DXMatrixRotationY(&mY, fY);
							D3DXMatrixRotationZ(&mZ, fZ);
							temp = mX * mY * mZ;
						}else{
							// Y*Z*X順
							// Z軸回り
							float fLimit = 1.535889f;		// 88.0f/180.0f*3.14159265f;
							float fSZ = -temp._21;			// sin(θy)
							float fZ  = (float)asin(fSZ);	// Y軸回り決定
							float fCZ = (float)cos(fZ);

							// ジンバルロック回避
							if(fabs(fZ) > fLimit){
								fZ = (fZ<0)?- fLimit: fLimit;
								fCZ = (float)cos(fZ);
							}

							// X軸回り
							float fSX = temp._23 / fCZ;
							float fCX = temp._22 / fCZ;
							float fX  = (FLOAT)atan2(fSX, fCX);	// X軸回り決定

							// Y軸回り
							float fSY = temp._31 / fCZ;
							float fCY = temp._11 / fCZ;
							float fY  = (FLOAT)atan2(fSY, fCY);	// Z軸回り決定

							// 角度の制限
							if(fX < p_bone[tv].rdlimit.x){
								float tf= 2 * p_bone[tv].rdlimit.x - fX;
								if(tf <= p_bone[tv].rulimit.x && j<ikt) fX = tf;
								else						   fX = p_bone[tv].rdlimit.x;
							}
							if(fX > p_bone[tv].rulimit.x){
								float tf= 2 * p_bone[tv].rulimit.x - fX;
								if(tf >= p_bone[tv].rdlimit.x && j<ikt) fX = tf;
								else						   fX = p_bone[tv].rulimit.x;
							}
							if(fY < p_bone[tv].rdlimit.y){
								float tf= 2 * p_bone[tv].rdlimit.y - fY;
								if(tf <= p_bone[tv].rulimit.y && j<ikt) fY = tf;
								else						   fY = p_bone[tv].rdlimit.y;
							}
							if(fY > p_bone[tv].rulimit.y){
								float tf= 2 * p_bone[tv].rulimit.y - fY;
								if(tf >= p_bone[tv].rdlimit.y && j<ikt) fY = tf;
								else						   fY = p_bone[tv].rulimit.y;
							}
							if(fZ < p_bone[tv].rdlimit.z){
								float tf= 2 * p_bone[tv].rdlimit.z - fZ;
								if(tf <= p_bone[tv].rulimit.z && j<ikt) fZ = tf;
								else						   fZ = p_bone[tv].rdlimit.z;
							}
							if(fZ > p_bone[tv].rulimit.z){
								float tf= 2 * p_bone[tv].rulimit.z - fZ;
								if(tf >= p_bone[tv].rdlimit.z && j<ikt) fZ = tf;
								else						   fZ = p_bone[tv].rulimit.z;
							}

							// 決定した角度でベクトルを回転
							D3DXMATRIX mX,mY,mZ;
							D3DXMatrixRotationX(&mX, fX);
							D3DXMatrixRotationY(&mY, fY);
							D3DXMatrixRotationZ(&mZ, fZ);
							temp = mY * mZ * mX;
						}
						D3DXQuaternionRotationMatrix(&p_bone[tv].IKQuat,&temp);
					}
				}else{
					if(((strncmp(p_bone[tv].name,"右ひざ",6)==0)|(strncmp(p_bone[tv].name,"左ひざ",6)==0))){
						if(atan2f(temp._32,temp._33)<0.0f){
							temp._23 = -temp._23;
							temp._32 = -temp._32;
							p_bone[tv].IKQuat.x = -p_bone[tv].IKQuat.x;
						}
					}
				}

				// calculate matrix after roll
				p_bone[tv].mat_local=temp;
				D3DXMatrixTranslation(&temp,-p_bone[tv].pos.x,-p_bone[tv].pos.y,-p_bone[tv].pos.z);
				D3DXMatrixMultiply(&p_bone[tv].mat_local,&temp,&p_bone[tv].mat_local);
				D3DXMatrixTranslation(&temp,p_bone[tv].Btrans.x,p_bone[tv].Btrans.y,p_bone[tv].Btrans.z);
				D3DXMatrixMultiply(&p_bone[tv].mat_local,&p_bone[tv].mat_local,&temp);
				D3DXMatrixTranslation(&temp,p_bone[tv].pos.x,p_bone[tv].pos.y,p_bone[tv].pos.z);
				D3DXMatrixMultiply(&p_bone[tv].mat_local,&p_bone[tv].mat_local,&temp);

				for(int l=k;l>=0;l--){
					int tv=p_IK[i].sBone[l];
					D3DXMatrixMultiply(&p_bone[tv].mat,&p_bone[tv].mat_local,&p_bone[p_bone[tv].parent].mat);
					D3DXMATRIX tm = p_bone[p_bone[tv].parent].mat;tm._41=tm._42=tm._43=0.0f;tm._44=1.0f;
					p_bone[tv].p_mat=tm;
				}
				tv=p_IK[i].dstBone;
				D3DXMatrixMultiply(&p_bone[tv].mat,&p_bone[tv].mat_local,&p_bone[p_bone[tv].parent].mat);
				D3DXMATRIX tm = p_bone[p_bone[tv].parent].mat;tm._41=tm._42=tm._43=0.0f;tm._44=1.0f;
				p_bone[tv].p_mat=tm;
			}
		}
	}