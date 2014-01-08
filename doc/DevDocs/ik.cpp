	// IK process
	for(int i=0;i<m_pIKnum;i++){
		if(!p_IK[i].IKOn) continue;
		// calculate IK bone position
		D3DVECTOR ikp;
		int tv=p_IK[i].IKBone;
		ikp.x = p_bone[tv].mat._11 * p_bone[tv].pos.x
			  + p_bone[tv].mat._21 * p_bone[tv].pos.y
			  + p_bone[tv].mat._31 * p_bone[tv].pos.z + p_bone[tv].mat._41;
		ikp.y = p_bone[tv].mat._12 * p_bone[tv].pos.x
			  + p_bone[tv].mat._22 * p_bone[tv].pos.y
			  + p_bone[tv].mat._32 * p_bone[tv].pos.z + p_bone[tv].mat._42;
		ikp.z = p_bone[tv].mat._13 * p_bone[tv].pos.x
			  + p_bone[tv].mat._23 * p_bone[tv].pos.y
			  + p_bone[tv].mat._33 * p_bone[tv].pos.z + p_bone[tv].mat._43;
		// calculate matrix of last bone
		tv=p_IK[i].dstBone;
		p_bone[tv].IKmat_normal=lpDx3d->QuatConvertToMatrix(p_bone[tv].Quat);
		D3DXMatrixTranslation(&temp,-p_bone[tv].pos.x,-p_bone[tv].pos.y,-p_bone[tv].pos.z);
		D3DXMatrixMultiply(&p_bone[tv].IKmat,&temp,&p_bone[tv].IKmat_normal);
		D3DXMatrixTranslation(&temp,p_bone[tv].trans.x,p_bone[tv].trans.y,p_bone[tv].trans.z);
		D3DXMatrixMultiply(&p_bone[tv].IKmat,&p_bone[tv].IKmat,&temp);
		D3DXMatrixTranslation(&temp,p_bone[tv].pos.x,p_bone[tv].pos.y,p_bone[tv].pos.z);
		D3DXMatrixMultiply(&p_bone[tv].IKmat,&p_bone[tv].IKmat,&temp);

		for(int j=0;j<p_IK[i].loopNum;j++){
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
				// calculate roll axis
				if(((strncmp(p_bone[tv].name,"âEÇ–Ç¥",6)==0)|(strncmp(p_bone[tv].name,"ç∂Ç–Ç¥",6)==0))){
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
				// calculate roll angle of [k]th bone(limited by p_IK[i].dlimit*(k+1)*2)
				float nais=v1.x*v2.x+v1.y*v2.y+v1.z*v2.z;
				if(nais>1.0f) nais=1.0f;
				if(nais<-1.0f) nais=-1.0f;
				float IKrot = 0.5f * acosf(nais);
				if(IKrot>p_IK[i].dlimit*(k+1)*2) IKrot=p_IK[i].dlimit*(k+1)*2;
				QUAT_DATA qIK;
				float IKsin=sinf(IKrot);
				qIK.x = v.x*IKsin;
				qIK.y = v.y*IKsin;
				qIK.z = v.z*IKsin;
				qIK.w = cosf(IKrot);

				p_bone[tv].IKQuat=lpDx3d->QuatMul(qIK,p_bone[tv].IKQuat);
				if(j==0){
					p_bone[tv].IKQuat=lpDx3d->QuatMul(p_bone[tv].IKQuat,p_bone[tv].Quat);
				}
				temp=lpDx3d->QuatConvertToMatrix(p_bone[tv].IKQuat);
				if(((strncmp(p_bone[tv].name,"âEÇ–Ç¥",6)==0)|(strncmp(p_bone[tv].name,"ç∂Ç–Ç¥",6)==0))){
					if(atan2f(temp._32,temp._33)<0.0f){
						temp._23 = -temp._23;
						temp._32 = -temp._32;
						p_bone[tv].IKQuat.x = -p_bone[tv].IKQuat.x;
					}
				}
				p_bone[tv].qsdef = p_bone[tv].IKQuat;

				// calculate matrix after roll
				p_bone[tv].IKmat_normal=temp;
				D3DXMatrixTranslation(&temp,-p_bone[tv].pos.x,-p_bone[tv].pos.y,-p_bone[tv].pos.z);
				D3DXMatrixMultiply(&p_bone[tv].IKmat,&temp,&p_bone[tv].IKmat_normal);
				D3DXMatrixTranslation(&temp,p_bone[tv].trans.x,p_bone[tv].trans.y,p_bone[tv].trans.z);
				D3DXMatrixMultiply(&p_bone[tv].IKmat,&p_bone[tv].IKmat,&temp);
				D3DXMatrixTranslation(&temp,p_bone[tv].pos.x,p_bone[tv].pos.y,p_bone[tv].pos.z);
				D3DXMatrixMultiply(&p_bone[tv].IKmat,&p_bone[tv].IKmat,&temp);

				for(int l=k;l>=0;l--){
					int tv=p_IK[i].sBone[l];
					D3DXMatrixMultiply(&p_bone[tv].mat,&p_bone[tv].IKmat,&p_bone[p_bone[tv].parent].mat);
					D3DXMatrixMultiply(&p_bone[tv].mat_normal,&p_bone[tv].IKmat_normal,&p_bone[p_bone[tv].parent].mat_normal);
					p_bone[tv].p_mat=p_bone[p_bone[tv].parent].mat_normal;
				}
				tv=p_IK[i].dstBone;
				D3DXMatrixMultiply(&p_bone[tv].mat,&p_bone[tv].IKmat,&p_bone[p_bone[tv].parent].mat);
				D3DXMatrixMultiply(&p_bone[tv].mat_normal,&p_bone[tv].IKmat_normal,&p_bone[p_bone[tv].parent].mat_normal);
				p_bone[tv].p_mat=p_bone[p_bone[tv].parent].mat_normal;
			}
		}
	}
