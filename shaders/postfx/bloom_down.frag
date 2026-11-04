#version 150

// Bloom: half the size of the previous level (dual filter downsample)

uniform sampler2D src;
uniform vec2 srcTexel;

in vec2 uv;
out vec4 color;

void main()
{
	vec2 h = srcTexel;
	vec3 c = texture(src, uv).rgb * 4.0;
	c += texture(src, uv + vec2(-h.x, -h.y)).rgb;
	c += texture(src, uv + vec2( h.x, -h.y)).rgb;
	c += texture(src, uv + vec2(-h.x,  h.y)).rgb;
	c += texture(src, uv + vec2( h.x,  h.y)).rgb;
	color = vec4(c / 8.0, 1.0);
}
