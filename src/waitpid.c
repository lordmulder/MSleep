/*
 * waitpid for Win32
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

static BOOL __stdcall crtlHandler(DWORD dwCtrlTyp)
{
	switch (dwCtrlTyp)
	{
	case CTRL_C_EVENT:
		fputws(L"Ctrl+C: Waitpid has been interrupted !!!\n\n", stderr);
		break;
	case CTRL_BREAK_EVENT:
		fputws(L"Break: Waitpid has been interrupted !!!\n\n", stderr);
		break;
	default:
		return FALSE;
	}

	fflush(stderr);
	_exit(3);
	return TRUE;
}

static BOOL enablePrivilege(const wchar_t *const privilegeName)
{
	BOOL success = FALSE;
	HANDLE hToken;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		TOKEN_PRIVILEGES tkp;
		if (LookupPrivilegeValueW(NULL, privilegeName, &tkp.Privileges[0].Luid))
		{
			tkp.PrivilegeCount = 1;
			tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			success = AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
		}
		CloseHandle(hToken);
	}
	return success;
}

/* ======================================================================= */
/* HELPER MACROS AND TYPES                                                 */
/* ======================================================================= */

#define EXIT_TIMEOUT  2 /*exit code when timeout occrus*/

#define SHUTDOWN_FLAGS (SHUTDOWN_FORCE_OTHERS | SHUTDOWN_FORCE_SELF | SHUTDOWN_POWEROFF)
#define SHUTDOWN_REASON (SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHUTDOWN_POWEROFF | SHTDN_REASON_FLAG_PLANNED)

#define TRY_PARSE_OPTION(NAME) \
	if (!_wcsicmp(argv[argOffset] + 2U, L#NAME)) \
	{ \
		opt_##NAME = TRUE; \
		continue; \
	}

/* ======================================================================= */
/* MAIN                                                                    */
/* ======================================================================= */

/*Globals*/
static DWORD pids[MAXIMUM_WAIT_OBJECTS];
static HANDLE procHandles[MAXIMUM_WAIT_OBJECTS];

int _wmain(int argc, wchar_t *argv[])
{
	int result = EXIT_FAILURE, argOffset = 1;
	BOOL opt_shutdown = FALSE, opt_waitone = FALSE, opt_pedantic = FALSE, opt_timeout = FALSE, opt_quiet = FALSE;
	DWORD idx, error, pidCount = 0U, procCount = 0U, timeout = 0U, waitStatus = MAXDWORD;

	//Initialize
	SetErrorMode(SetErrorMode(0x0003) | 0x0003);
	setlocale(LC_ALL, "C");
	SetConsoleCtrlHandler(crtlHandler, TRUE);
	_setmode(_fileno(stdout), _O_U8TEXT);
	_setmode(_fileno(stderr), _O_U8TEXT);

	//Check command-line arguments
	if (argc < 2)
	{
		fputws(L"waitpid [" TEXT(__DATE__) L"]\n", stderr);
		fputws(L"Wait (sleep) until the specified processes all have terminated.\n\n", stderr);
		fputws(L"Usage:\n   waitpid.exe [options] <PID_1> [<PID_2> ... <PID_n>]\n\n", stderr);
		fputws(L"Options:\n", stderr);
		fputws(L"   --waitone   exit as soon as *any* of the specified processes terminates\n", stderr);
		fputws(L"   --shutdown  power off the machine, as soon as the processes have terminated\n", stderr);
		fputws(L"   --timeout   exit as soon as the timeout (default: 30 sec) has expired\n", stderr);
		fputws(L"   --pedantic  abort with error, if a specified process can *not* be opened\n", stderr);
		fputws(L"   --quiet     do *not* print any diagnostic messages; errors are shown anyway\n\n", stderr);
		fputws(L"Environment:\n", stderr);
		fputws(L"   WAITPID_TIMEOUT  timeout in millisonds, only if `--timeout` is specified\n\n", stderr);
		fputws(L"Exit status:\n", stderr);
		fputws(L"   0 - Processes have terminated normally\n", stderr);
		fputws(L"   1 - Failed with error\n", stderr);
		fputws(L"   2 - Aborted because the timeout has expired\n", stderr);
		fputws(L"   3 - Interrupted by user\n\n", stderr);
		return EXIT_FAILURE;
	}

	//Parse command-line options
	for (; (argOffset < argc) && (!wcsncmp(argv[argOffset], L"--", 2)); ++argOffset)
	{
		if (!argv[argOffset][2U])
		{
			++argOffset;
			break; /*stop option parsing*/
		}
		TRY_PARSE_OPTION(waitone)
		TRY_PARSE_OPTION(shutdown)
		TRY_PARSE_OPTION(timeout)
		TRY_PARSE_OPTION(pedantic)
		TRY_PARSE_OPTION(quiet)
		fwprintf(stderr, L"Error: Unknown option \"%s\" encountered!\n\n", argv[argOffset]);
		return EXIT_FAILURE;
	}

	//Read timeout environemnt string
	if (opt_timeout)
	{
		const WCHAR *const envstr = getEnvironmentString(L"WAITPID_TIMEOUT");
		timeout = 30000U;
		if (envstr && envstr[0U])
		{
			DWORD value;
			if (parseULong(envstr, &value) || (value < 1U) || (value == INFINITE))
			{
				fputws(L"Warning: WAITPID_TIMEOUT is invalid. Using default timeout!\n\n", stderr);
			}
			else
			{
				timeout = value; /*ovrride timeout*/
			}
			FREE(envstr);
		}
	}

	//Check remaining argument count
	if (argOffset >= argc)
	{
		fputws(L"Error: No PID(s) specified. Nothing to do!\n\n", stderr);
		return EXIT_FAILURE;
	}
	if ((argc - argOffset) > MAXIMUM_WAIT_OBJECTS)
	{
		fwprintf(stderr, L"Error: Too many PID(s) specified! [limit: %d]\n\n", MAXIMUM_WAIT_OBJECTS);
		return EXIT_FAILURE;
	}

	//Convert all given PID arguments to numeric values
	for (; argOffset < argc; ++argOffset)
	{
		BOOL duplicate = FALSE;
		DWORD currentPid;
		if (parseULong(argv[argOffset], &currentPid))
		{
			fwprintf(stderr, L"Error: Specified PID \"%s\"is invalid!\n\n", argv[argOffset]);
			return EXIT_FAILURE;
		}
		currentPid &= ~0x3L;
		for (idx = 0U; idx < pidCount; ++idx)
		{
			if (pids[idx] == currentPid)
			{
				duplicate = TRUE;
				break;
			}
		}
		if (!duplicate)
		{
			pids[pidCount++] = currentPid;
		}
	}

	//Enable the SE_SHUTDOWN_NAME privilege for this process (if needed)
	if (opt_shutdown)
	{
		const BOOL havePrivilege = enablePrivilege(SE_SHUTDOWN_NAME);
		if ((!havePrivilege) && (!opt_quiet))
		{
			fputws(L"Warning: Unable to to acquire SE_SHUTDOWN_NAME privilege!\nThe planned shutdown is probably going to fail.", stderr);
		}
	}

	//Open all specified processes
	for (idx = 0U; idx < pidCount; ++idx)
	{
		const HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, pids[idx]);
		if (handle)
		{
			procHandles[procCount++] = handle;
		}
		else
		{
			error = GetLastError();
			if (opt_pedantic)
			{
				fwprintf(stderr, L"Error: Failed to open process #%lu, aborting! [error: %lu]\n\n", pids[idx], error);
				goto cleanup;
			}
			else if (!opt_quiet)
			{
				fwprintf(stderr, L"Non-existing process #%lu ignored.\n", pids[idx]);
			}
		}
	}
	if ((!opt_quiet) && (procCount < pidCount))
	{
		fputws(L"\n", stderr);
	}

	//Any existing processes found?
	if (procCount < 1U)
	{
		result = EXIT_SUCCESS;
		if (!opt_quiet)
		{
			fputws(L"No existing processes found. Nothing to do.\n\n", stderr);
		}
		goto success;
	}

	//Print progress message
	if (!opt_quiet)
	{
		fwprintf(stderr, L"Waiting for %lu unique process(es) to terminate...\n", procCount);
	}

	//Wait for processes to terminate
	if (procCount > 1U)
	{
		waitStatus = WaitForMultipleObjects(procCount, procHandles, !opt_waitone, opt_timeout ? timeout : INFINITE);
	}
	else
	{
		waitStatus = WaitForSingleObject(procHandles[0U], opt_timeout ? timeout : INFINITE);
	}

	//Check the resulting wait status
	if (waitStatus < procCount)
	{
		result = EXIT_SUCCESS;
		if (!opt_quiet)
		{
			fputws(L"Terminated.\n\n", stderr);
		}
	}
	else
	{
		if (waitStatus == WAIT_TIMEOUT)
		{
			result = EXIT_TIMEOUT;
			if (!opt_quiet)
			{
				fputws(L"Timeout has expired.\n\n", stderr);
			}
		}
		else
		{
			fwprintf(stderr, L"Failed to wait for processes! [error: %lu]\n\n", GetLastError());
			goto cleanup;
		}
	}

success:
	if (opt_shutdown)
	{
		if (!opt_quiet)
		{
			fputws(L"Shutting down the system now...\n", stderr);
		}
		if ((error = InitiateShutdownW(NULL, L"Computer shutting down on behalf of WAITPID utility by Muldersoft.", 30U, SHUTDOWN_FLAGS, SHUTDOWN_REASON)) == ERROR_SUCCESS)
		{
			if (!opt_quiet)
			{
				fputws(L"Shutdown initiated.\n\n", stderr);
			}
		}
		else
		{
			fwprintf(stderr, L"Failed to initiate system shutdown! [error: %lu]\n\n", error);
			if (opt_pedantic)
			{
				result = EXIT_FAILURE;
			}
		}
	}

cleanup:
	for (idx = 0U; idx < procCount; ++idx)
	{
		CloseHandle(procHandles[idx]);
	}

	return result; /*exit*/
}
