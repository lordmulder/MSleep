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
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>

extern const wchar_t *const PROGRAM_VERSION;

//VC 6.0 workaround
#ifdef ENABLE_VC6_WORKAROUNDS
_CRTIMP extern FILE _iob[];
#undef stdout
#undef stderr
#define stdout (&_iob[1])
#define stderr (&_iob[2])
#endif

int parseULong(const wchar_t *str, ULONG *const out);

unsigned long long getCurrentTime(void);
unsigned long long getStartupTime(void);

DWORD getAttributes(const wchar_t *const filePath, unsigned long long *const timeStamp);
BOOL clearAttribute(const wchar_t *const filePath, const DWORD mask);

const wchar_t* getCanonicalPath(const wchar_t *const fileName);
const wchar_t* getDirectoryPart(const wchar_t *const fullPath);
const wchar_t* getEnvironmentString(const wchar_t *const name);

/* initialize CRT */
#define INITIALIZE_C_RUNTIME() do \
{ \
	SetErrorMode(SetErrorMode(0x0003) | 0x0003); \
	setlocale(LC_ALL, "C"); \
	SetConsoleCtrlHandler(crtlHandler, TRUE); \
	_setmode(_fileno(stdout), _O_U8TEXT); \
	_setmode(_fileno(stderr), _O_U8TEXT); \
} \
while(0)

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
