#version 150

// FXAA (after Timothy Lottes' FXAA 2): blur along the edges found from the
// luma differences (the luma is in alpha)

uniform sampler2D src;
uniform vec2 texel;

in vec2 uv;
out vec4 color;

const float REDUCE_MIN = 1.0 / 128.0;
const float REDUCE_MUL = 1.0 / 8.0;
const float SPAN_MAX = 8.0;

void main()
{
	float lNW = texture(src, uv + vec2(-1.0, -1.0) * texel).a;
	float lNE = texture(src, uv + vec2( 1.0, -1.0) * texel).a;
	float lSW = texture(src, uv + vec2(-1.0,  1.0) * texel).a;
	float lSE = texture(src, uv + vec2( 1.0,  1.0) * texel).a;
	vec4 m = texture(src, uv);
	float lM = m.a;
	float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
	float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
	
	vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), (lNW + lSW) - (lNE + lSE));
	float reduce = max((lNW + lNE + lSW + lSE) * 0.25 * REDUCE_MUL, REDUCE_MIN);
	float rcpMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
	dir = clamp(dir * rcpMin, vec2(-SPAN_MAX), vec2(SPAN_MAX)) * texel;
	
	vec3 a = 0.5 * (texture(src, uv + dir * (1.0 / 3.0 - 0.5)).rgb + texture(src, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
	vec3 b = a * 0.5 + 0.25 * (texture(src, uv - dir * 0.5).rgb + texture(src, uv + dir * 0.5).rgb);
	float lB = dot(b, vec3(0.299, 0.587, 0.114));
	color = vec4((lB < lMin || lB > lMax) ? a : b, 1.0);
}
