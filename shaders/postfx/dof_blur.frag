#version 150

// Depth of field, step 2 (half size): gather blur over a disc whose size is
// the circle of confusion.  Samples on a golden-angle spiral; sharp things
// behind the pixel do not spread over it (after Dennis Gustafsson's
// single-pass depth of field).

uniform sampler2D dofTex;   // rgb colour, a distance
uniform vec2 texel;
uniform float focus;        // focus distance
uniform float aperture;     // strength
uniform float maxBlur;      // largest circle of confusion radius, in pixels of this buffer

in vec2 uv;
out vec4 color;

float blurSize(float d)
{
	return maxBlur * clamp(aperture * abs(d - focus) / max(d, 1e-3), 0.0, 1.0);
}

void main()
{
	vec4 centre = texture(dofTex, uv);
	float centreSize = blurSize(centre.a);
	vec3 c = centre.rgb;
	float total = 1.0;
	// ~32 samples whatever the size
	float step = max(0.5, maxBlur * maxBlur / 64.0);
	float r = step;
	float ang = 0.0;
	for(int i = 0; i < 64 && r < maxBlur; ++i)
	{
		vec2 t = uv + vec2(cos(ang), sin(ang)) * texel * r;
		vec4 s = texture(dofTex, t);
		float size = blurSize(s.a);
		if(s.a > centre.a) size = clamp(size, 0.0, centreSize * 2.0);
		float m = smoothstep(r - 0.5, r + 0.5, size);
		c += mix(c / total, s.rgb, m);
		total += 1.0;
		r += step / r;
		ang += 2.39996323;
	}
	color = vec4(c / total, centre.a);
}
