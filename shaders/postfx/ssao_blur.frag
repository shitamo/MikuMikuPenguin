#version 150

// 4x4 blur of the occlusion (half size) that does not mix different depths

uniform sampler2D aoTex;
uniform sampler2D depthTex;
uniform mat4 invProjection;
uniform vec2 aoTexel;
uniform float radius;

in vec2 uv;
out vec4 color;

float viewZ(vec2 t)
{
	float z = texture(depthTex, t).r;
	vec4 p = invProjection * vec4(t * 2.0 - 1.0, z * 2.0 - 1.0, 1.0);
	return p.z / p.w;
}

void main()
{
	float zc = viewZ(uv);
	float sum = 0.0, wsum = 0.0;
	for(int y = -2; y < 2; ++y)
		for(int x = -2; x < 2; ++x)
		{
			vec2 t = uv + (vec2(x, y) + 0.5) * aoTexel;
			float w = max(0.0, 1.0 - abs(viewZ(t) - zc) / (radius * 0.5));
			sum += texture(aoTex, t).r * w;
			wsum += w;
		}
	color = vec4(wsum > 0.0 ? sum / wsum : texture(aoTex, uv).r);
}
