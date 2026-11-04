#version 150

// Bloom: the smaller level, enlarged with a tent filter, is added (blending
// ONE, ONE) to the next larger level with the weight `spread`

uniform sampler2D src;
uniform vec2 srcTexel;
uniform float spread;

in vec2 uv;
out vec4 color;

void main()
{
	vec2 h = srcTexel;
	vec3 c = texture(src, uv).rgb * 4.0;
	c += (texture(src, uv + vec2(-h.x, 0.0)).rgb + texture(src, uv + vec2(h.x, 0.0)).rgb +
	      texture(src, uv + vec2(0.0, -h.y)).rgb + texture(src, uv + vec2(0.0, h.y)).rgb) * 2.0;
	c += texture(src, uv + vec2(-h.x, -h.y)).rgb + texture(src, uv + vec2(h.x, -h.y)).rgb +
	     texture(src, uv + vec2(-h.x,  h.y)).rgb + texture(src, uv + vec2(h.x,  h.y)).rgb;
	color = vec4(c / 16.0 * spread, 1.0);
}
