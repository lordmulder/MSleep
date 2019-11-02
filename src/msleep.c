/*
 * msleep for Win32
 * Created by LoRd_MuldeR <mulder2@gmx.de>.
 * 
 * This work is licensed under the CC0 1.0 Universal License.
 * To view a copy of the license, visit:
 * https://creativecommons.org/publicdomain/zero/1.0/legalcode
 */

#include "common.h"

/* ======================================================================= */
/* UTILITY FUNCTIONS                                                       */
/* ======================================================================= */

static __inline unsigned long computeDelta(const unsigned long long begin, const unsigned long long end)
{
	if(end > begin)
	{
		const unsigned long long delta = (end - begin) / 10000ULL;
		return (delta < ULONG_MAX) ? ((unsigned long)delta) : ULONG_MAX;
	}
	return 0UL;
}

static BOOL __stdcall crtlHandler(DWORD dwCtrlTyp)
{
	switch (dwCtrlTyp)
	{
	case CTRL_C_EVENT:
		fputws(L"Ctrl+C: MSleep has been interrupted !!!\n\n", stderr);
		break;
	case CTRL_BREAK_EVENT:
		fputws(L"Break: MSleep has been interrupted !!!\n\n", stderr);
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
	int error;
	unsigned long timeout, delta;

	//Initialize
	INITIALIZE_C_RUNTIME();

	//Check command-line arguments
	if ((argc < 2) || (!_wcsicmp(argv[1U], L"/?")) || (!_wcsicmp(argv[1U], L"--help")))
	{
		fwprintf(stderr, L"msleep %s\n", PROGRAM_VERSION);
		fputws(L"Wait (sleep) for the specified amount of time, in milliseconds.\n\n", stderr);
		fputws(L"Usage:\n", stderr);
		fputws(L"   msleep.exe <timeout_ms>\n\n", stderr);
		fputws(L"Exit status:\n", stderr);
		fputws(L"   0 - Timeout expired normally\n", stderr);
		fputws(L"   1 - Failed with error\n", stderr);
		fputws(L"   2 - Interrupted by user\n\n", stderr);
		fputws(L"Note: Process creation overhead will be measured and compensated.\n\n", stderr);
		return EXIT_FAILURE;
	}

	//Check argument count
	if (argc > 2)
	{
		fputws(L"Error: Found excess command-line argument!\n", stderr);
		return EXIT_FAILURE;
	}

	//Parse timeout
	if(error = parseULong(argv[1], &timeout))
	{
		switch (error)
		{
		case ERANGE:
			fputws(L"Error: Given timeout value is out of range!\n\n", stderr);
			return EXIT_FAILURE;
		default:
			fputws(L"Error: Given timeout value could not be parsed!\n\n", stderr);
			return EXIT_FAILURE;
		}
	}

	//Sleep remaining time
	delta = computeDelta(getStartupTime(), getCurrentTime());
	if (timeout >= delta)
	{
		Sleep(timeout - delta);
	}

	return EXIT_SUCCESS;
}
