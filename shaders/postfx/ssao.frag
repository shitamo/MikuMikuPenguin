#version 150

// Screen-space ambient occlusion (drawn at half size).  View-space positions
// are rebuilt from the depth buffer; points of a small hemisphere around the
// normal that lie behind the visible surface occlude.

uniform sampler2D depthTex;
uniform mat4 projection;
uniform mat4 invProjection;
uniform vec2 depthTexel;   // 1 / size of the depth texture
uniform float radius;      // MMD units
uniform float strength;

in vec2 uv;
out vec4 color;

vec3 viewPos(vec2 t)
{
	float z = texture(depthTex, t).r;
	vec4 p = invProjection * vec4(t * 2.0 - 1.0, z * 2.0 - 1.0, 1.0);
	return p.xyz / p.w;
}

const int SAMPLES = 12;
// hemisphere (z up), denser near the centre
const vec3 kernel[SAMPLES] = vec3[](
	vec3( 0.0560,  0.0180, 0.0700), vec3(-0.0870,  0.0640, 0.0520),
	vec3( 0.0260, -0.1450, 0.0960), vec3( 0.1980,  0.0850, 0.1100),
	vec3(-0.1700, -0.2100, 0.1300), vec3(-0.0800,  0.3100, 0.1900),
	vec3( 0.3400, -0.2000, 0.2300), vec3(-0.4300,  0.0700, 0.2800),
	vec3( 0.1500,  0.4900, 0.3300), vec3( 0.3500,  0.3200, 0.5000),
	vec3(-0.3300, -0.4700, 0.5200), vec3( 0.1200, -0.2400, 0.8600));

void main()
{
	// a pixel centre of the full-size depth buffer (this pass is half size)
	vec2 c0 = (floor(uv / depthTexel) + 0.5) * depthTexel;
	float d = texture(depthTex, c0).r;
	if(d >= 1.0) { color = vec4(1.0); return; } // background
	vec3 P = viewPos(c0);
	
	// normal from the neighbour with the smaller depth step (no halo at silhouettes)
	vec3 pr = viewPos(c0 + vec2(depthTexel.x, 0.0)), pl = viewPos(c0 - vec2(depthTexel.x, 0.0));
	vec3 pu = viewPos(c0 + vec2(0.0, depthTexel.y)), pd = viewPos(c0 - vec2(0.0, depthTexel.y));
	vec3 dx = (abs(pr.z - P.z) < abs(P.z - pl.z)) ? pr - P : P - pl;
	vec3 dy = (abs(pu.z - P.z) < abs(P.z - pd.z)) ? pu - P : P - pd;
	vec3 N = normalize(cross(dx, dy));
	if(dot(N, -P) < 0.0) N = -N;
	
	// kernel rotation from a 4x4 pattern (removed again by the 4x4 blur)
	ivec2 q = ivec2(gl_FragCoord.xy) & 3;
	const float bayer[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
	float a = bayer[q.y * 4 + q.x] / 16.0 * 6.2831853;
	vec3 r = vec3(cos(a), sin(a), 0.0);
	vec3 T = r - N * dot(r, N);
	if(dot(T, T) < 1e-4) T = cross(N, vec3(0.0, 0.0, 1.0)); // normal along the rotation axis
	T = normalize(T);
	vec3 B = cross(N, T);
	
	float occlusion = 0.0;
	float bias = 0.02 * radius;
	for(int i = 0; i < SAMPLES; ++i)
	{
		vec3 k = kernel[i];
		vec3 s = P + (T * k.x + B * k.y + N * k.z) * radius;
		vec4 c = projection * vec4(s, 1.0);
		vec2 st = c.xy / c.w * 0.5 + 0.5;
		if(any(lessThan(st, vec2(0.0))) || any(greaterThan(st, vec2(1.0)))) continue;
		float sz = viewPos(st).z;
		float range = smoothstep(0.0, 1.0, radius / max(abs(P.z - sz), 1e-4));
		occlusion += (sz >= s.z + bias) ? range : 0.0;
	}
	float ao = clamp(1.0 - strength * occlusion / float(SAMPLES), 0.0, 1.0);
	color = vec4(ao);
}
