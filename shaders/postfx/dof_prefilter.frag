#version 150

// Depth of field, step 1 (half size): colour (with ambient occlusion) and
// the distance from the camera

uniform sampler2D colorTex;
uniform sampler2D depthTex;
uniform sampler2D aoTex;
uniform bool useAO;
uniform mat4 invProjection;

in vec2 uv;
out vec4 color;

void main()
{
	vec3 c = texture(colorTex, uv).rgb; // bilinear: average of 2x2 pixels
	if(useAO) c *= texture(aoTex, uv).r;
	float z = texture(depthTex, uv).r;
	vec4 p = invProjection * vec4(uv * 2.0 - 1.0, z * 2.0 - 1.0, 1.0);
	color = vec4(c, -p.z / p.w);
}
