#version 150

// Final picture: scene * ambient occlusion, depth of field, + bloom/glow,
// colour correction.  With FXAA the luma goes into alpha for the next pass.

uniform sampler2D colorTex;
uniform sampler2D depthTex;
uniform sampler2D aoTex;
uniform sampler2D dofTex;
uniform sampler2D bloomTex;

uniform bool useAO;
uniform bool useDof;
uniform mat4 invProjection;
uniform float focus;
uniform float aperture;
uniform float maxBlur;      // full-size pixels
uniform bool useBloom;
uniform float bloomScale;
uniform bool useColor;
uniform float brightness;
uniform float contrast;
uniform float saturation;
uniform float gamma;
uniform bool lumaInAlpha;

in vec2 uv;
out vec4 color;

void main()
{
	vec3 c = texture(colorTex, uv).rgb;
	float z = texture(depthTex, uv).r;
	// (the background has no occlusion: keeps the half-size AO from bleeding onto it)
	if(useAO && z < 1.0) c *= texture(aoTex, uv).r;
	if(useDof)
	{
		vec4 p = invProjection * vec4(uv * 2.0 - 1.0, z * 2.0 - 1.0, 1.0);
		float d = -p.z / p.w;
		float coc = maxBlur * clamp(aperture * abs(d - focus) / max(d, 1e-3), 0.0, 1.0);
		c = mix(c, texture(dofTex, uv).rgb, smoothstep(0.5, 2.5, coc));
	}
	if(useBloom) c += texture(bloomTex, uv).rgb * bloomScale;
	if(useColor)
	{
		c = clamp(c, 0.0, 1.0);
		c = (c - 0.5) * contrast + 0.5 + brightness;
		float l = dot(c, vec3(0.299, 0.587, 0.114));
		c = mix(vec3(l), c, saturation);
		c = pow(clamp(c, 0.0, 1.0), vec3(1.0 / gamma));
	}
	c = clamp(c, 0.0, 1.0);
	color = vec4(c, lumaInAlpha ? dot(c, vec3(0.299, 0.587, 0.114)) : 1.0);
}
