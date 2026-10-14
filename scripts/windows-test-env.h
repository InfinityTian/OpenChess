#ifdef _WIN32
#include <stdlib.h>
/* Test-only POSIX environment wrapper; release binaries use native sources. */
static inline int oc_test_setenv(const char *key, const char *value, int overwrite)
{
    if (!overwrite && getenv(key)) return 0;
    return _putenv_s(key, value);
}
#define setenv oc_test_setenv
#endif
