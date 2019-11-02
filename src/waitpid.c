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
		wprintln(stderr, L"Ctrl+C: Waitpid has been interrupted !!!\n");
		break;
	case CTRL_BREAK_EVENT:
		wprintln(stderr, L"Break: Waitpid has been interrupted !!!\n");
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

#define EXIT_TIMEOUT 2 /*exit code when timeout occrus*/
#define PID_MASK (~((DWORD)0x3))

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

int wmain(int argc, wchar_t *argv[])
{
	int result = EXIT_FAILURE, argOffset = 1;
	BOOL opt_shutdown = FALSE, opt_waitone = FALSE, opt_pedantic = FALSE, opt_timeout = FALSE, opt_quiet = FALSE;
	DWORD idx, error, pidCount = 0U, procCount = 0U, timeout = 30000U, waitStatus = MAXDWORD;

	//Initialize
	INITIALIZE_C_RUNTIME();

	//Check command-line arguments
	if ((argc < 2) || (!_wcsicmp(argv[1U], L"/?")) || (!_wcsicmp(argv[1U], L"--help")))
	{
		fwprintf(stderr, L"waitpid %s\n", PROGRAM_VERSION);
		wprintln(stderr, L"Wait (sleep) until the specified processes all have terminated.\n");
		wprintln(stderr, L"Usage:");
		wprintln(stderr, L"   waitpid.exe [options] <PID_1> [<PID_2> ... <PID_n>]\n");
		wprintln(stderr, L"Options:");
		wprintln(stderr, L"   --waitone   exit as soon as *any* of the specified processes terminates");
		wprintln(stderr, L"   --shutdown  power off the machine, as soon as the processes have terminated");
		wprintln(stderr, L"   --timeout   exit as soon as the timeout (default: 30 sec) has expired");
		wprintln(stderr, L"   --pedantic  abort with error, if a specified process can *not* be opened");
		wprintln(stderr, L"   --quiet     do *not* print any diagnostic messages; errors are shown anyway\n");
		wprintln(stderr, L"Environment:");
		wprintln(stderr, L"   WAITPID_TIMEOUT  timeout in millisonds, only if `--timeout` is specified\n");
		wprintln(stderr, L"Exit status:");
		wprintln(stderr, L"   0 - Processes have terminated normally");
		wprintln(stderr, L"   1 - Failed with error");
		wprintln(stderr, L"   2 - Aborted because the timeout has expired");
		wprintln(stderr, L"   3 - Interrupted by user\n");
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
		if (envstr)
		{
			DWORD value;
			if (parseULong(envstr, &value) || (value < 1U) || (value == INFINITE))
			{
				wprintln(stderr, L"Warning: WAITPID_TIMEOUT is invalid. Using default timeout!\n");
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
		wprintln(stderr, L"Error: No PID(s) specified. Nothing to do!\n");
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
		for (idx = 0U; idx < pidCount; ++idx)
		{
			if (pids[idx] == (currentPid & PID_MASK))
			{
				if(!opt_quiet)
				{
					fwprintf(stderr, L"Redundant PID #%u ignored.\n\n", currentPid);
				}
				duplicate = TRUE;
				break;
			}
		}
		if (!duplicate)
		{
			pids[pidCount++] = (currentPid & PID_MASK);
		}
	}

	//Enable the SE_SHUTDOWN_NAME privilege for this process (if needed)
	if (opt_shutdown)
	{
		const BOOL havePrivilege = enablePrivilege(SE_SHUTDOWN_NAME);
		if ((!havePrivilege) && (!opt_quiet))
		{
			wprintln(stderr, L"Warning: Unable to to acquire SE_SHUTDOWN_NAME privilege!\nThe planned shutdown is probably going to fail.\n");
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
		wprintln(stderr, L"");
	}

	//Any existing processes found?
	if (procCount < 1U)
	{
		result = EXIT_SUCCESS;
		if (!opt_quiet)
		{
			wprintln(stderr, L"No existing processes found. Nothing to do.\n");
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
			wprintln(stderr, L"Terminated.\n");
		}
	}
	else
	{
		if (waitStatus == WAIT_TIMEOUT)
		{
			result = EXIT_TIMEOUT;
			if (!opt_quiet)
			{
				wprintln(stderr, L"Timeout has expired.\n");
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
			wprintln(stderr, L"Shutting down the system now...");
		}
		if ((error = InitiateShutdownW(NULL, L"Computer shutting down on behalf of WAITPID utility by Muldersoft.", 30U, SHUTDOWN_FLAGS, SHUTDOWN_REASON)) == ERROR_SUCCESS)
		{
			if (!opt_quiet)
			{
				wprintln(stderr, L"Shutdown initiated.\n");
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
