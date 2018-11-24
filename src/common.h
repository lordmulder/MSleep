/*
 * FS utility functions
 * Created by LoRd_MuldeR <mulder2@gmx.de>.
 * 
 * This work is licensed under the CC0 1.0 Universal License.
 * To view a copy of the license, visit:
 * https://creativecommons.org/publicdomain/zero/1.0/legalcode
 */

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

unsigned long long getCurrentTime(void);
unsigned long long getStartupTime(void);

DWORD getAttributes(const wchar_t *const filePath);
BOOL clearAttribute(const wchar_t *const filePath, const DWORD mask);

const wchar_t* getCanonicalPath(const wchar_t *const fileName);
const wchar_t* getDirectoryPart(const wchar_t *const fullPath);

/* free buffer, if not NULL */
#define FREE(PTR) do \
{ \
	if ((PTR)) \
	{ \
		free((void*)(PTR)); \
	} \
} \
while(0);

/* close system handle, if valid */
#define CLOSE_HANDLE(H) do \
{ \
	if ((H) && ((H) != INVALID_HANDLE_VALUE)) \
	{ \
		CloseHandle((H)); \
	} \
} \
while(0)
