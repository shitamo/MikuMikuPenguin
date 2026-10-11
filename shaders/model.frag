#version 150

// MMD-compatible material shading:
//   color = clamp(diffuse*lightColor + ambient) * texture
//           (*|+) sphere map  * toon(N.L)  + specular
// With self shadow (as MMD's full.fx): the lit colour (without toon) and
// the shadow colour (multiplied by the darkest toon colour) are blended by
//   comp = min(saturate(N.L*3), shadow map visibility)

uniform sampler2D textureSampler;
uniform sampler2D sphereSampler;
uniform sampler2D toonSampler;

uniform vec4 diffuse;
uniform vec3 ambient;
uniform vec3 specular;
uniform float shininess;   //specular power

uniform vec3 lightColor;   //MMD default: 154/255
uniform vec3 lightDirection; //direction the light travels, in view space

uniform bool isEdge;
uniform vec4 edgeColor;

uniform float fSphereMode; //0: none 1: multiply (sph) 2: add (spa) 3: sub-texture (not supported)
uniform bool useToon;

//render passes: 0 normal, 1 self-shadow depth map, 2 ground shadow
uniform int renderMode;
uniform vec4 groundShadowColor;

//self shadow
uniform bool selfShadow;          //this material receives the self shadow
uniform sampler2DShadow shadowMap;
uniform vec2 shadowTexelSize;

//material morph texture coefficients: tex = tex*textureMul + textureAdd
uniform vec4 textureMul;
uniform vec4 textureAdd;
uniform vec4 sphereMul;
uniform vec4 sphereAdd;
uniform vec4 toonMul;
uniform vec4 toonAdd;

in vec2 UV;
in vec3 normal;
in vec3 viewPos;
in vec4 shadowCoord;

out vec4 color;

//fraction of light reaching this point (1 = lit), 3x3 PCF
float shadowVisibility(vec3 N, vec3 L)
{
	vec3 sc = shadowCoord.xyz / shadowCoord.w;
	if(any(lessThan(sc, vec3(0.0))) || any(greaterThan(sc, vec3(1.0)))) return 1.0; //outside the map
	//slope-scaled bias against shadow acne
	float bias = (0.0008 + 0.0025 * (1.0 - clamp(dot(N, L), 0.0, 1.0)));
	float sum = 0.0;
	for(int y = -1; y <= 1; ++y)
		for(int x = -1; x <= 1; ++x)
			sum += texture(shadowMap, vec3(sc.xy + vec2(x, y) * shadowTexelSize, sc.z - bias));
	return sum / 9.0;
}

void main()
{
	if(renderMode == 1) //depth only
	{
		color = vec4(1.0);
		return;
	}
	if(renderMode == 2)
	{
		color = groundShadowColor;
		return;
	}
	
	if(isEdge)
	{
		color = edgeColor;
		return;
	}
	
	vec3 N = normalize(normal);
	vec3 L = normalize(-lightDirection);   //towards the light
	vec3 V = normalize(-viewPos);          //towards the eye
	
	float alpha = diffuse.a;
	
	//base colour
	vec3 c = clamp(diffuse.rgb * lightColor + ambient, 0.0, 1.0);
	
	//texture
	vec4 tex = clamp(texture(textureSampler, UV) * textureMul + textureAdd, 0.0, 1.0);
	c *= tex.rgb;
	alpha *= tex.a;
	if(alpha <= 0.0) discard;
	
	//sphere map (view-space normal -> UV)
	int sphereMode = int(fSphereMode + 0.5);
	if(sphereMode == 1 || sphereMode == 2)
	{
		vec2 sphereUV = vec2(N.x * 0.5 + 0.5, 1.0 - (N.y * 0.5 + 0.5));
		vec3 sph = clamp(texture(sphereSampler, sphereUV) * sphereMul + sphereAdd, 0.0, 1.0).rgb;
		if(sphereMode == 1) c *= sph;
		else                c += sph;
	}
	
	if(selfShadow)
	{
		vec3 shadowC = c;
		float comp = shadowVisibility(N, L);
		if(useToon)
		{
			comp = min(clamp(dot(N, L) * 3.0, 0.0, 1.0), comp);
			shadowC *= clamp(texture(toonSampler, vec2(0.5, 1.0)) * toonMul + toonAdd, 0.0, 1.0).rgb;
		}
		vec3 litC = c;
		if(shininess > 0.0)
		{
			vec3 H = normalize(V + L);
			litC += pow(max(dot(H, N), 0.0), shininess) * specular * lightColor;
		}
		color = vec4(clamp(mix(shadowC, litC, comp), 0.0, 1.0), alpha);
		return;
	}
	
	//toon shading: the toon texture is looked up with the N.L term
	if(useToon)
	{
		float ln = clamp(dot(N, L) + 0.5, 0.0, 1.0);
		c *= clamp(texture(toonSampler, vec2(0.5, 1.0 - ln)) * toonMul + toonAdd, 0.0, 1.0).rgb;
	}
	
	//specular (Blinn-Phong, like MMD)
	if(shininess > 0.0)
	{
		vec3 H = normalize(V + L);
		c += pow(max(dot(H, N), 0.0), shininess) * specular * lightColor;
	}
	
	color = vec4(clamp(c, 0.0, 1.0), alpha);
}
