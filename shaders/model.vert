#version 150

//Vertex Properties
//layout(location = #) if version is 330 or greater
in vec4 vPosition;
in vec2 vUV;
in vec3 vNormal;

in vec4 vBoneIndices;
in vec4 vBoneWeights;
in float vWeightFormula;

out vec2 UV;
out vec3 normal;

uniform mat4 MVP;
uniform samplerBuffer boneTexture; //skinning matrices, 4 RGBA32F texels per bone

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
	else if(weightFormula==3) //Fake SDEF by using BDEF2 formula
	{
		newVertex  = (boneMatrix(index1) * vPosition)          * vBoneWeights.x;
		newVertex += (boneMatrix(index2) * vPosition)          * (1.0 - vBoneWeights.x);
		
		newNormal  = (boneMatrix(index1) * vec4(vNormal, 0.0)) * vBoneWeights.x;
		newNormal += (boneMatrix(index2) * vec4(vNormal, 0.0)) * (1.0 - vBoneWeights.x);
		
		//newVertex = vPosition;
	}
	else // TODO: SDEF (weightFormula==3), QDEF (weightFormula==4)
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
