//imgdiff: compare two PNG images with a tolerance (for the rendering test).
//
//  imgdiff EXPECTED.png ACTUAL.png [--diff DIFF.png] [--threshold N] [--max-fraction F]
//
//A pixel differs when one of its channels differs by more than N (default
//48 of 255).  The images match when at most F (default 0.005 = 0.5%) of the
//pixels differ, which absorbs small rasterisation differences between Mesa
//versions but not a missing part, a wrong colour or a moved bone.  With
//--diff, the differing pixels are written in red over a faded copy of
//ACTUAL.  Exit status: 0 match, 1 mismatch, 2 error.
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
	const char *expectedPath=NULL, *actualPath=NULL, *diffPath=NULL;
	int threshold=48;
	double maxFraction=0.005;
	for(int i=1; i<argc; ++i)
	{
		std::string a=argv[i];
		if(a=="--diff" && i+1<argc) diffPath=argv[++i];
		else if(a=="--threshold" && i+1<argc) threshold=atoi(argv[++i]);
		else if(a=="--max-fraction" && i+1<argc) maxFraction=atof(argv[++i]);
		else if(!expectedPath) expectedPath=argv[i];
		else if(!actualPath) actualPath=argv[i];
		else { fprintf(stderr, "imgdiff: unexpected argument %s\n", argv[i]); return 2; }
	}
	if(!expectedPath || !actualPath)
	{
		fprintf(stderr, "usage: imgdiff EXPECTED.png ACTUAL.png [--diff DIFF.png] [--threshold N] [--max-fraction F]\n");
		return 2;
	}

	int w1, h1, w2, h2, n;
	unsigned char *a=stbi_load(expectedPath, &w1, &h1, &n, 3);
	if(!a) { fprintf(stderr, "imgdiff: cannot read %s\n", expectedPath); return 2; }
	unsigned char *b=stbi_load(actualPath, &w2, &h2, &n, 3);
	if(!b) { fprintf(stderr, "imgdiff: cannot read %s\n", actualPath); return 2; }
	if(w1!=w2 || h1!=h2)
	{
		fprintf(stderr, "imgdiff: size differs: %dx%d vs %dx%d\n", w1, h1, w2, h2);
		return 1;
	}

	long differing=0;
	int maxDelta=0;
	std::vector<unsigned char> diff((size_t)w1*h1*3);
	for(long p=0; p<(long)w1*h1; ++p)
	{
		int delta=0;
		for(int c=0; c<3; ++c) delta=std::max(delta, abs((int)a[p*3+c]-(int)b[p*3+c]));
		maxDelta=std::max(maxDelta, delta);
		bool differs=delta>threshold;
		if(differs) ++differing;
		for(int c=0; c<3; ++c)
		{
			unsigned char faded=(unsigned char)(255-(255-b[p*3+c])/4);
			diff[p*3+c]=differs ? (c==0 ? 255 : 0) : faded;
		}
	}
	double fraction=(double)differing/((double)w1*h1);
	bool ok=fraction<=maxFraction;
	printf("%s: %ld of %d pixels differ (%.3f%%, limit %.3f%%), largest channel difference %d\n",
	       ok ? "match" : "MISMATCH", differing, w1*h1, fraction*100.0, maxFraction*100.0, maxDelta);
	if(diffPath && !stbi_write_png(diffPath, w1, h1, 3, diff.data(), w1*3))
	{
		fprintf(stderr, "imgdiff: cannot write %s\n", diffPath);
	}
	stbi_image_free(a);
	stbi_image_free(b);
	return ok ? 0 : 1;
}
