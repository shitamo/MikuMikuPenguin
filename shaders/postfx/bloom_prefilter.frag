#version 150

// Bloom / glow, step 1 (half size): the parts brighter than the threshold
// and the light emitted by the models

uniform sampler2D colorTex;
uniform sampler2D glowTex;
uniform bool useBloom;
uniform float threshold;
uniform float bloomIntensity;
uniform bool useGlow;
uniform float glowIntensity;

in vec2 uv;
out vec4 color;

void main()
{
	vec3 result = vec3(0.0);
	if(useBloom)
	{
		vec3 c = texture(colorTex, uv).rgb;
		float l = dot(c, vec3(0.299, 0.587, 0.114));
		float w = clamp((l - threshold) / max(1.0 - threshold, 1e-3), 0.0, 1.0);
		result += c * (w * w) * bloomIntensity;
	}
	if(useGlow) result += texture(glowTex, uv).rgb * glowIntensity;
	color = vec4(result, 1.0);
}
