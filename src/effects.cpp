#include "effects.h"

#include <cstdlib>
#include <cctype>
#include <cmath>

using namespace std;

namespace
{
	string trim(const string &s)
	{
		size_t b=0, e=s.size();
		while(b<e && isspace((unsigned char)s[b])) ++b;
		while(e>b && isspace((unsigned char)s[e-1])) --e;
		return s.substr(b, e-b);
	}

	string lower(string s)
	{
		for(char &c : s) c=(char)tolower((unsigned char)c);
		return s;
	}

	bool parseFloat(const string &s, float &out)
	{
		string t=trim(s);
		if(t.empty()) return false;
		char *end=NULL;
		double v=strtod(t.c_str(), &end);
		if(!end || *end!='\0' || !std::isfinite(v)) return false;
		out=(float)v;
		return true;
	}
}

bool parseEffectList(const string &list, EffectSettings &s, string &error)
{
	size_t pos=0;
	while(pos<=list.size())
	{
		size_t comma=list.find(',', pos);
		if(comma==string::npos) comma=list.size();
		string item=trim(list.substr(pos, comma-pos));
		pos=comma+1;
		if(item.empty()) continue;

		string name=item, value;
		size_t eq=item.find('=');
		if(eq!=string::npos)
		{
			name=trim(item.substr(0, eq));
			value=item.substr(eq+1);
		}
		name=lower(name);
		float v=0.0f;
		bool hasValue=eq!=string::npos;
		if(hasValue && (!parseFloat(value, v) || v<0.0f))
		{
			error="bad value for effect "+name+": "+trim(value);
			return false;
		}

		if(name=="bloom")
		{
			s.bloom=true;
			if(hasValue) s.bloomIntensity=v;
		}
		else if(name=="glow" || name=="al" || name=="autoluminous")
		{
			s.glow=true;
			if(hasValue) s.glowIntensity=v;
		}
		else if(name=="dof")
		{
			s.dof=true;
			if(hasValue) s.dofStrength=v;
		}
		else if(name=="ssao" || name=="ao")
		{
			s.ssao=true;
			if(hasValue) s.ssaoStrength=v;
		}
		else if(name=="color" || name=="colour")
		{
			if(hasValue) { error="use --color B,C,S,G to set the colour correction"; return false; }
			s.color=true;
		}
		else if(name=="fxaa")
		{
			if(hasValue) { error="fxaa takes no value"; return false; }
			s.fxaa=true;
		}
		else if(name=="all")
		{
			if(hasValue) { error="all takes no value"; return false; }
			s.bloom=s.glow=s.dof=s.ssao=s.color=s.fxaa=true;
		}
		else if(name=="none")
		{
			if(hasValue) { error="none takes no value"; return false; }
			s.bloom=s.glow=s.dof=s.ssao=s.color=s.fxaa=false;
		}
		else
		{
			error="unknown effect: "+name+" (bloom, glow, dof, ssao, color, fxaa, all, none)";
			return false;
		}
	}
	return true;
}

bool parseColorAdjust(const string &values, EffectSettings &s, string &error)
{
	float v[4]={s.brightness, s.contrast, s.saturation, s.gamma};
	size_t pos=0;
	int n=0;
	while(pos<=values.size())
	{
		size_t comma=values.find(',', pos);
		if(comma==string::npos) comma=values.size();
		string item=values.substr(pos, comma-pos);
		pos=comma+1;
		if(n>=4) { error="--color takes at most 4 values (brightness,contrast,saturation,gamma)"; return false; }
		if(!trim(item).empty() && !parseFloat(item, v[n]))
		{
			error="bad --color value: "+trim(item);
			return false;
		}
		++n;
	}
	if(v[1]<0.0f || v[2]<0.0f || v[3]<=0.0f)
	{
		error="--color: contrast and saturation must be >= 0 and gamma > 0";
		return false;
	}
	s.brightness=v[0]; s.contrast=v[1]; s.saturation=v[2]; s.gamma=v[3];
	s.color=true;
	return true;
}
