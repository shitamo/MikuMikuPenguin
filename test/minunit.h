#define mu_assert(test) do { if (!(test)) return __FUNCTION__; } while (0)
#define mu_run_test(test) do { const char *message = test(); tests_run++; \
        if (message) return message; } while (0)
extern int tests_run;
