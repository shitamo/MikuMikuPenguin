#ifndef EFFECTS_H
#define EFFECTS_H

//Screen effects of the viewer (MME-like looks done by the viewer itself,
//without reading .fx files): settings and their command line syntax.
//The OpenGL side is in postfx.h.

#include <string>

struct EffectSettings
{
	//Bloom: bright parts of the picture bleed into their surroundings
	bool bloom=false;
	float bloomIntensity=0.8f;
	float bloomThreshold=0.8f;  //luminance where the bloom starts (0..1)
	float bloomRadius=0.7f;     //spread (weight of the wider blur levels, 0..1)

	//Glow (AutoLuminous-like): materials with specular power >= 110 emit
	//light, and optionally the brightest parts of the models glow
	bool glow=false;
	float glowIntensity=1.5f;
	bool glowBright=true;       //also the brightest parts of the models
	float glowThreshold=0.9f;   //luminance where they start to glow (0..1)

	//Depth of field
	bool dof=false;
	bool dofAutoFocus=true;     //focus on the camera's point of interest
	float dofFocus=45.0f;       //focus distance (MMD units) when not automatic
	float dofStrength=1.0f;     //how fast things blur away from the focus (aperture)
	float dofMaxBlur=1.0f;      //largest blur, relative (1 = 1/50 of the picture height)

	//Screen-space ambient occlusion
	bool ssao=false;
	float ssaoStrength=1.0f;
	float ssaoRadius=0.6f;      //MMD units

	//Colour correction
	bool color=false;
	float brightness=0.0f;      //-1..1, added
	float contrast=1.0f;        //0..2
	float saturation=1.0f;      //0..2
	float gamma=1.0f;           //0.2..3 (>1 brightens the mid tones)

	//Fast approximate anti-aliasing
	bool fxaa=false;

	//any effect on: the scene is drawn into an offscreen buffer first
	bool any() const { return bloom || glow || dof || ssao || color || fxaa; }
};

//"bloom,dof=1.5,ssao": turn on the named effects; "=value" sets the main
//strength of the effect (bloom/glow intensity, DoF strength, SSAO strength).
//Names: bloom, glow (al, autoluminous), dof, ssao, color, fxaa, all, none.
//Returns false (and an error message) for unknown names or bad values.
bool parseEffectList(const std::string &list, EffectSettings &settings, std::string &error);

//"B,C,S,G": brightness, contrast, saturation, gamma (trailing ones may be
//left out); turns colour correction on
bool parseColorAdjust(const std::string &values, EffectSettings &settings, std::string &error);

#endif
