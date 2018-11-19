/*
 * msleep for Win32
 * Created by LoRd_MuldeR <mulder2@gmx.de>.
 * 
 * This work is licensed under the CC0 1.0 Universal License.
 * To view a copy of the license, visit:
 * https://creativecommons.org/publicdomain/zero/1.0/legalcode
 */

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

//VC 6.0 workaround
#ifdef ENABLE_VC6_WORKAROUNDS
_CRTIMP extern FILE _iob[];
#undef stdout
#undef stderr
#define stdout (&_iob[1])
#define stderr (&_iob[2])
#endif //ENABLE_VC6_WORKAROUNDS

/* ======================================================================= */
/* UTILITY FUNCTIONS                                                       */
/* ======================================================================= */

static __inline unsigned long parseULong(const wchar_t *str, unsigned long *const out)
{
	unsigned long long value;
	char c;
	while(isspace(*str))
	{
		str++;
	}
	if(swscanf(str, _wcsnicmp(str, L"0x", 2) ? L"%I64u %c" : L"%I64x %c", &value, &c) != 1)
	{
		return EINVAL;
	}
	if(value > ULONG_MAX)
	{
		return ERANGE;
	}
	*out = (unsigned long) value;
	return 0;;
}

static __inline unsigned long long fileTimeToMSec(const FILETIME *const fileTime)
{
	ULARGE_INTEGER tmp;
	tmp.HighPart = fileTime->dwHighDateTime;
	tmp.LowPart = fileTime->dwLowDateTime;
	return tmp.QuadPart;
}

static __inline unsigned long long getStartupTime(void)
{
	FILETIME timeCreation, timeExit, timeKernel, timeUser;
	GetProcessTimes(GetCurrentProcess(), &timeCreation, &timeExit, &timeKernel, &timeUser);
	return fileTimeToMSec(&timeCreation);
}

static __inline unsigned long computeDelta(const unsigned long long begin, const unsigned long long end)
{
	if(end > begin)
	{
		const unsigned long long delta = (end - begin) / 10000ULL;
		return (delta < ULONG_MAX) ? ((unsigned long)delta) : ULONG_MAX;
	}
	return 0UL;
}

static __inline unsigned long long getCurrentTime(void)
{
	FILETIME now;
	GetSystemTimeAsFileTime(&now);
	return fileTimeToMSec(&now);
}

static BOOL __stdcall crtlHandler(DWORD dwCtrlTyp)
{
	switch (dwCtrlTyp)
	{
	case CTRL_C_EVENT:
		fputs("Ctrl+C: Sleep has been interrupted !!!\n\n", stderr);
		break;
	case CTRL_BREAK_EVENT:
		fputs("Break: Sleep has been interrupted !!!\n\n", stderr);
		break;
	default:
		return FALSE;
	}

	fflush(stderr);
	_exit(2);
	return TRUE;
}

/* ======================================================================= */
/* MAIN                                                                    */
/* ======================================================================= */

int wmain(int argc, wchar_t *argv[])
{
	unsigned long timeout, delta;

	//Initialize
	SetErrorMode(SetErrorMode(0x0003) | 0x0003);
	setlocale(LC_ALL, "C");
	SetConsoleCtrlHandler(crtlHandler, TRUE);

	//Check command-line arguments
	if (argc < 2)
	{
		fputs("msleep [" __DATE__ "]\n", stderr);
		fputs("Wait (sleep) for the specified amount of time, in milliseconds.\n\n", stderr);
		fputs("Usage:\n   msleep.exe <timeout_ms>\n\n", stderr);
		fputs("Note: Process creation overhead will be measured and compensated.\n\n", stderr);
		return EXIT_FAILURE;
	}

	//Parse timeout
	switch (parseULong(argv[1], &timeout))
	{
	case EINVAL:
		fputs("Error: Given timeout value could not be parsed!\n\n", stderr);
		return EXIT_FAILURE;
	case ERANGE:
		fputs("Error: Given timeout value is out of range!\n\n", stderr);
		return EXIT_FAILURE;
	}

	//Sleep remaining time
	delta = computeDelta(getStartupTime(), getCurrentTime());
	if (timeout >= delta)
	{
		Sleep(timeout - delta);
	}

	return EXIT_SUCCESS;
}
