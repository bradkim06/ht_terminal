#ifndef __ASSERT_H__
#define __ASSERT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h> // va_list, va_start, va_end
#include <string.h>
#include "uart.h"

#ifdef NDEBUG /* required by ANSI standard */
#define ASSERT(__e) ((void)0)
#define ASSERT_PRINT(__e, str) ((void)0)
#else
#define __FILENAME__ (strrchr(__FILE__, '\\') + 1) // windows style
//# define __FILENAME__  (strrchr(__FILE__, '/') + 1)   // unix style

#define ASSERT(__e) ((__e) ? (void)0 : __assert_func(__FILENAME__, __LINE__, __ASSERT_FUNC, #__e))
#define ASSERT_PRINT(__e, str)                                                                     \
	((__e) ? (void)0 : __assert_print_func(__FILENAME__, __LINE__, __ASSERT_FUNC, #__e, str))

#ifndef __ASSERT_FUNC
/* Use g++'s demangled names in C++.  */
#if defined(__cplusplus) && defined(__GNUC__)
#define __ASSERT_FUNC __PRETTY_FUNCTION__

/* C99 requires the use of __func__, gcc also supports it.  */
#elif defined(__GNUC__) || __STDC_VERSION__ >= 199901L
#define __ASSERT_FUNC __func__

/* failed to detect __func__ support.  */
#else
#define __ASSERT_FUNC ((char *)0)
#endif
#endif /* !__ASSERT_FUNC */
#endif /* !NDEBUG */

static inline void __assert_func(const char *file, int line, const char *func, const char *expr)
{
	printf("ASSERT:%s:%d:%s:(%s)\n", file, line, func, expr);
	while (1)
		;
}

static inline void __assert_print_func(const char *file, int line, const char *func,
				       const char *expr, char *str)
{
	printf("ASSERT:%s:%d:%s:(%s):%s\n", file, line, func, expr, str);
	while (1)
		;
}

#ifdef __cplusplus
}
#endif

#endif /* __ASSERT_H__ */