//Command line syntax of the screen effects (--effect, --color)
#include "effects.h"
#include "minunit.h"
#include <cstdio>
#include <cmath>
#include <string>

using namespace std;

int tests_run = 0;

static bool near(float a, float b) { return std::fabs(a-b) < 1e-6f; }

static const char*
test_defaults_off()
{
    EffectSettings s;
    mu_assert(!s.any());
    return 0;
}

static const char*
test_list()
{
    EffectSettings s;
    string err;
    mu_assert(parseEffectList("bloom, dof", s, err));
    mu_assert(s.bloom && s.dof && !s.glow && !s.ssao && !s.color && !s.fxaa);
    mu_assert(s.any());
    return 0;
}

static const char*
test_values()
{
    EffectSettings s;
    string err;
    mu_assert(parseEffectList("Bloom=1.25,al=2,ssao=0.5,dof=3", s, err));
    mu_assert(s.bloom && near(s.bloomIntensity, 1.25f));
    mu_assert(s.glow && near(s.glowIntensity, 2.0f));
    mu_assert(s.ssao && near(s.ssaoStrength, 0.5f));
    mu_assert(s.dof && near(s.dofStrength, 3.0f));
    return 0;
}

static const char*
test_all_none()
{
    EffectSettings s;
    string err;
    mu_assert(parseEffectList("all", s, err));
    mu_assert(s.bloom && s.glow && s.dof && s.ssao && s.color && s.fxaa);
    mu_assert(parseEffectList("none,fxaa", s, err));
    mu_assert(s.fxaa && !s.bloom && !s.glow && !s.dof && !s.ssao && !s.color);
    return 0;
}

static const char*
test_errors()
{
    EffectSettings s;
    string err;
    mu_assert(!parseEffectList("sparkle", s, err) && !err.empty());
    mu_assert(!parseEffectList("bloom=abc", s, err));
    mu_assert(!parseEffectList("bloom=-1", s, err));
    mu_assert(!parseEffectList("fxaa=1", s, err));
    mu_assert(!s.bloom); //nothing set by a failed item
    return 0;
}

static const char*
test_color()
{
    EffectSettings s;
    string err;
    mu_assert(parseColorAdjust("0.1,1.2", s, err));
    mu_assert(s.color && near(s.brightness, 0.1f) && near(s.contrast, 1.2f) && near(s.saturation, 1.0f) && near(s.gamma, 1.0f));
    mu_assert(parseColorAdjust(",,0.5,2", s, err));
    mu_assert(near(s.brightness, 0.1f) && near(s.saturation, 0.5f) && near(s.gamma, 2.0f));
    mu_assert(!parseColorAdjust("0,1,1,0", s, err));
    mu_assert(!parseColorAdjust("0,1,1,1,1", s, err));
    mu_assert(!parseColorAdjust("x", s, err));
    return 0;
}

static const char*
all_tests()
{
    mu_run_test(test_defaults_off);
    mu_run_test(test_list);
    mu_run_test(test_values);
    mu_run_test(test_all_none);
    mu_run_test(test_errors);
    mu_run_test(test_color);
    return 0;
}

int
main()
{
    const char *result = all_tests();
    if (result != 0)
    {
        printf("%s\n", result);
    }
    else
    {
        printf("ALL TESTS PASSED\n");
    }
    printf("Tests run: %d\n", tests_run);
    return result != 0;
}
