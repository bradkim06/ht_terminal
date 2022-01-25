#ifdef TDD_DEBUG
#include <stdio.h>
#include <stdarg.h>

#define TDD_BUF_SIZE 1000
#endif

void tddPrint(const char *format, ...)
{
#ifdef TDD_DEBUG
	int len = 0;
	char buf[TDD_BUF_SIZE];

	va_list arg;
	va_start(arg, format);
	// Console 출력 시 New line을 위한 1byte 고려.
	len += vsnprintf(buf + len, TDD_BUF_SIZE - 1, format, arg);
	va_end(arg);

	printf("%s", buf);
#endif
}
