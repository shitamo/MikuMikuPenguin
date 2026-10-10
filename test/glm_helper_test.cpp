#include "glm_helper.h"
#include "minunit.h"
#include <cstdio>
#include <iostream>

using namespace std;

int tests_run = 0;

static const char*
test_quat_euler()
{
    const glm::vec3 original(0.1, 0.2, 0.3);
    const glm::quat q = fromEulerAnglesRadians(original);
    const glm::vec3 calculated = toEulerAnglesRadians(q);
    mu_assert(glm::length(original - calculated) < 1e-5);
    return 0;
}

static const char*
test_quat_euler_glm()
{
    const glm::vec3 original(1, 1.5, 1);
    const glm::quat q(original);
    const glm::vec3 calculated = glm::eulerAngles(q); //radians in GLM >= 0.9.6
    mu_assert(glm::length(original - calculated) < 1e-5);
    return 0;
}

static const char*
all_tests()
{
    mu_run_test(test_quat_euler);
    // mu_run_test(test_quat_euler_glm);
    return 0;
}

int
main(int argc, char **argv)
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
