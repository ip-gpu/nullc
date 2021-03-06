#include "StrAlgo.h"

unsigned int NULLC::GetStringHash(const char *str)
{
	unsigned int hash = 5381;
	int c;
	while((c = *str++) != 0)
		hash = ((hash << 5) + hash) + c;
	return hash;
}

unsigned int NULLC::GetStringHash(const char *str, const char *end)
{
	unsigned int hash = 5381;
	while(str != end)
		hash = ((hash << 5) + hash) + (*str++);
	return hash;
}

unsigned int NULLC::StringHashContinue(unsigned int hash, const char *str)
{
	int c;
	while((c = *str++) != 0)
		hash = ((hash << 5) + hash) + c;
	return hash;
}

unsigned int NULLC::StringHashContinue(unsigned int hash, const char *str, const char *end)
{
	while(str != end)
		hash = ((hash << 5) + hash) + (*str++);
	return hash;
}

char* NULLC::PrintInteger(char* str, int number)
{
	char buf[16];
	char *curr = buf;
	*curr++ = (char)(number % 10 + '0');
	while(number /= 10)
		*curr++ = (char)(number % 10 + '0');
	do 
	{
		--curr;
		*str++ = *curr;
	}while(curr != buf);
	return str;
}

int	NULLC::SafeSprintf(char* dst, size_t size, const char* src, ...)
{
	va_list args;
	va_start(args, src);

	int result = vsnprintf(dst, size, src, args);
	dst[size-1] = '\0';
	return (result == -1 || (size_t)result >= size) ? (int)size : result;
}
