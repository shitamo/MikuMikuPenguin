#version 150

//Vertex Properties
//layout(location = #) if version is 330 or greater
in vec4 vPosition;
in vec2 vUV;
in vec3 vNormal;

in vec4 vBoneIndices;
in vec4 vBoneWeights;
in float vWeightFormula;

in vec3 vSdefC;  //SDEF centre
in vec3 vSdefR0; //SDEF pre-processed rotation centre for bone 1
in vec3 vSdefR1; //SDEF pre-processed rotation centre for bone 2

out vec2 UV;
out vec3 normal;

uniform mat4 MVP;
uniform samplerBuffer boneTexture; //skinning matrices, 4 RGBA32F texels per bone

vec4 quatFromMat3(mat3 m)
{
	//m[col][row]
	float tr = m[0][0] + m[1][1] + m[2][2];
	vec4 q;
	if(tr > 0.0)
	{
		float s = sqrt(tr + 1.0) * 2.0;
		q = vec4((m[1][2] - m[2][1]) / s, (m[2][0] - m[0][2]) / s, (m[0][1] - m[1][0]) / s, 0.25 * s);
	}
	else if(m[0][0] > m[1][1] && m[0][0] > m[2][2])
	{
		float s = sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0;
		q = vec4(0.25 * s, (m[1][0] + m[0][1]) / s, (m[2][0] + m[0][2]) / s, (m[1][2] - m[2][1]) / s);
	}
	else if(m[1][1] > m[2][2])
	{
		float s = sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0;
		q = vec4((m[1][0] + m[0][1]) / s, 0.25 * s, (m[2][1] + m[1][2]) / s, (m[2][0] - m[0][2]) / s);
	}
	else
	{
		float s = sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0;
		q = vec4((m[2][0] + m[0][2]) / s, (m[2][1] + m[1][2]) / s, 0.25 * s, (m[0][1] - m[1][0]) / s);
	}
	return normalize(q); //(x, y, z, w)
}

mat3 mat3FromQuat(vec4 q)
{
	float x = q.x, y = q.y, z = q.z, w = q.w;
	return mat3(1.0 - 2.0*(y*y + z*z), 2.0*(x*y + z*w),       2.0*(x*z - y*w),
	            2.0*(x*y - z*w),       1.0 - 2.0*(x*x + z*z), 2.0*(y*z + x*w),
	            2.0*(x*z + y*w),       2.0*(y*z - x*w),       1.0 - 2.0*(x*x + y*y));
}

vec4 quatSlerp(vec4 a, vec4 b, float t)
{
	float d = dot(a, b);
	if(d < 0.0) { b = -b; d = -d; }
	if(d > 0.9995) return normalize(mix(a, b, t));
	float th = acos(clamp(d, -1.0, 1.0));
	return (a * sin((1.0 - t) * th) + b * sin(t * th)) / sin(th);
}

mat4 boneMatrix(int index)
{
	int base = index * 4;
	return mat4(texelFetch(boneTexture, base),
	            texelFetch(boneTexture, base + 1),
	            texelFetch(boneTexture, base + 2),
	            texelFetch(boneTexture, base + 3));
}

uniform bool isEdge;
uniform float edgeSize;

//NOTE! When Model mesh has major defects, set newVertex to vPosition and/or try increasing bone Bone matrix Array size!

void main()
{
	vec4 newVertex=vPosition;
	vec4 newNormal;
	
	int index1, index2, index3, index4, weightFormula=4;

	index1 = int(vBoneIndices.x);
	index2 = int(vBoneIndices.y);
	index3 = int(vBoneIndices.z);
	index4 = int(vBoneIndices.w);

	weightFormula = int(vWeightFormula);
	
	
	newNormal = vec4(vNormal, 0.0);
	
	if(weightFormula==0) //BDEF1
	{
		newVertex  = (boneMatrix(index1) * vPosition);
		newNormal  = (boneMatrix(index1) * vec4(vNormal, 0.0));
		
		//newVertex = vPosition;
	}
	else if(weightFormula==1) //BDEF2
	{
		newVertex  = (boneMatrix(index1) * vPosition)          * vBoneWeights.x;
		newVertex += (boneMatrix(index2) * vPosition)          * (1.0 - vBoneWeights.x);
		
		newNormal  = (boneMatrix(index1) * vec4(vNormal, 0.0)) * vBoneWeights.x;
		newNormal += (boneMatrix(index2) * vec4(vNormal, 0.0)) * (1.0 - vBoneWeights.x);
		
		//newVertex = vPosition;
	}
	else if(weightFormula==2) //BDEF4
	{
		newVertex  = (boneMatrix(index1) * vPosition)          * vBoneWeights.x;
		newNormal  = (boneMatrix(index1) * vec4(vNormal, 0.0)) * vBoneWeights.x;
		
		newVertex += (boneMatrix(index2) * vPosition)          * vBoneWeights.y;
		newNormal += (boneMatrix(index2) * vec4(vNormal, 0.0)) * vBoneWeights.y;
		
		newVertex += (boneMatrix(index3) * vPosition)          * vBoneWeights.z;
		newNormal += (boneMatrix(index3) * vec4(vNormal, 0.0)) * vBoneWeights.z;
		
		newVertex += (boneMatrix(index4) * vPosition)          * vBoneWeights.w;
		newNormal += (boneMatrix(index4) * vec4(vNormal, 0.0)) * vBoneWeights.w;
		
		//newVertex = vPosition;
	}
	else if(weightFormula==3) //SDEF (spherical deform)
	{
		float w0 = vBoneWeights.x;
		float w1 = 1.0 - w0;
		mat4 m0 = boneMatrix(index1);
		mat4 m1 = boneMatrix(index2);
		
		mat3 rot = mat3FromQuat(quatSlerp(quatFromMat3(mat3(m0)), quatFromMat3(mat3(m1)), w1));
		
		vec3 p = rot * (vPosition.xyz - vSdefC)
		       + (m0 * vec4(vSdefR0, 1.0)).xyz * w0
		       + (m1 * vec4(vSdefR1, 1.0)).xyz * w1;
		newVertex = vec4(p, 1.0);
		newNormal = vec4(rot * vNormal, 0.0);
	}
	else // TODO: QDEF (weightFormula==4)
	{
		newVertex=vPosition;
		//newVertex = vec4(0.0,0.0,0.0,0.0);
		newNormal = vec4(vNormal, 0.0);
	}
	
	if(isEdge)
	{
		vec4 pos = MVP * vec4(newVertex);
		vec4 pos2 = MVP * vec4(newVertex + newNormal);
		vec4 norm = normalize(pos2 - pos);
		gl_Position = pos + norm * edgeSize/10.f;
		return;
    }
	
	//newVertex = vPosition;
	//newNormal = vec4(vNormal, 0.0);
	
	//newVertex.w = 1.0;

	gl_Position = MVP * newVertex;
	UV 		    = vUV;
	normal      = normalize(newNormal.xyz);
}
