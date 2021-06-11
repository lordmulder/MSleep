/*
 * notifywait for Win32
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
		wprintln(stderr, L"Ctrl+C: Notifywait has been interrupted !!!\n");
		break;
	case CTRL_BREAK_EVENT:
		wprintln(stderr, L"Break: Notifywait has been interrupted !!!\n");
		break;
	default:
		return FALSE;
	}

	fflush(stderr);
	_exit(2);
	return TRUE;
}

/* ======================================================================= */
/* HELPER MACROS AND TYPES                                                 */
/* ======================================================================= */

#define MAXIMUM_FILES 32 /*maximum number of files*/
#define NOTIFY_FLAGS (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION)

#define TRY_PARSE_OPTION(NAME) \
	if (!_wcsicmp(argv[argOffset] + 2U, L#NAME)) \
	{ \
		opt_##NAME = TRUE; \
		continue; \
	}

#define CHECK_IF_MODFIED(IDX) do \
{ \
	unsigned long long _timeStamp; \
	const DWORD _attribs = getAttributes(fullPath[(IDX)], &_timeStamp); \
	if ((_attribs == INVALID_FILE_ATTRIBUTES) || (_attribs & FILE_ATTRIBUTE_DIRECTORY) || (_attribs & FILE_ATTRIBUTE_ARCHIVE) || (_timeStamp != lastModTs[(IDX)])) \
	{ \
		if (!opt_quiet) \
		{ \
			fwprintf(stdout, L"%s\n", fullPath[(IDX)]); /*file was modified*/ \
		} \
		goto success; \
	} \
} \
while(0)

#define APPEND_TO_MAP(IDX,VALUE) do \
{ \
	dirToFilesMap[(IDX)].files[dirToFilesMap[(IDX)].count++] = (VALUE); \
} \
while(0)

#define CLOSE_NOTIFY_HANDLE(H) do \
{ \
	if ((H) && ((H) != INVALID_HANDLE_VALUE)) \
	{ \
		FindCloseChangeNotification((H)); \
	} \
} \
while(0)

typedef struct
{
	int count;
	int files[MAXIMUM_FILES];
}
fileIndex_list;

#define BOOLIFY(X) (!(!(X)))

/* ======================================================================= */
/* MAIN                                                                    */
/* ======================================================================= */

/*Globals*/
static const wchar_t *fullPath[MAXIMUM_FILES];
static BOOL directory[MAXIMUM_FILES];
static const wchar_t* directoryPath[MAXIMUM_FILES];
static unsigned long long lastModTs[MAXIMUM_FILES];
static fileIndex_list dirToFilesMap[MAXIMUM_FILES];
static HANDLE notifyHandle[MAXIMUM_FILES];

int wmain(int argc, wchar_t *argv[])
{
	BOOL opt_clear = FALSE, opt_reset = FALSE, opt_quiet = FALSE, opt_debug = FALSE;
	int result = EXIT_FAILURE, argOffset = 1, fileCount = 0, fileIdx = 0, dirCount = 0, dirIdx = 0;

	//Initialize
	INITIALIZE_C_RUNTIME();

	//Check command-line arguments
	if ((argc < 2) || (!_wcsicmp(argv[1U], L"/?")) || (!_wcsicmp(argv[1U], L"--help")))
	{
		fwprintf(stderr, L"notifywait %s\n", PROGRAM_VERSION);
		wprintln(stderr, L"Wait until a file is changed. File changes are detected via \"archive\" bit.\n");
		wprintln(stderr, L"Usage:");
		wprintln(stderr, L"   notifywait.exe [options] <name_1> [<name_2> ... <name_N>]\n");
		wprintln(stderr, L"Options:");
		wprintln(stderr, L"   --clear  unset the \"archive\" bit *before* monitoring for file changes");
		wprintln(stderr, L"   --reset  unset the \"archive\" bit *after* a file change was detected");
		wprintln(stderr, L"   --quiet  do *not* print the file name that changed to standard output");
		wprintln(stderr, L"   --debug  turn *on* additional diagnostic output (for testing only!)\n");
		wprintln(stderr, L"Exit status:");
		wprintln(stderr, L"   0 - File change was detected");
		wprintln(stderr, L"   1 - Failed with error");
		wprintln(stderr, L"   2 - Interrupted by user\n");
		wprintln(stderr, L"Remarks:");
		wprintln(stderr, L"   The operating system sets the \"archive\" bit whenever a file is changed.");
		wprintln(stderr, L"   If a file's \"archive\" bit is already set, a change is detected right away.");
		wprintln(stderr, L"   Either clear the \"archive\" bit beforehand, or use the --clear option!");
		wprintln(stderr, L"   If *multiple* files are given, the program detects changes in *any* file.");
		wprintln(stderr, L"   If a directory is given, *any* changes in that directory are detected.\n");
		return EXIT_FAILURE;
	}

	//Parse command-line options
	for (; (argOffset < argc) && (!wcsncmp(argv[argOffset], L"--", 2)); ++argOffset)
	{
		if(!argv[argOffset][2U])
		{
			++argOffset;
			break; /*stop option parsing*/
		}
		TRY_PARSE_OPTION(clear)
		TRY_PARSE_OPTION(reset)
		TRY_PARSE_OPTION(quiet)
		TRY_PARSE_OPTION(debug)
		fwprintf(stderr, L"Error: Unknown option \"%s\" encountered!\n\n", argv[argOffset]);
		return EXIT_FAILURE;
	}

	//Check remaining file count
	if (argOffset >= argc)
	{
		wprintln(stderr, L"Error: No file name(s) specified. Nothing to do!\n");
		return EXIT_FAILURE;
	}
	if ((argc - argOffset) > MAXIMUM_FILES)
	{
		fwprintf(stderr, L"Error: Too many file name(s) specified! [limit: %d]\n\n", MAXIMUM_FILES);
		return EXIT_FAILURE;
	}

	//Convert all input file(s) to absoloute paths
	for (; argOffset < argc; ++argOffset)
	{
		BOOL duplicate = FALSE;
		const wchar_t *fullPathNext = getCanonicalPath(argv[argOffset]);
		if (!fullPathNext)
		{
			fwprintf(stderr, L"Error: Path \"%s\" could not be resolved!\n\n", argv[argOffset]);
			goto cleanup;
		}
		for (fileIdx = 0; fileIdx < fileCount; ++fileIdx)
		{
			if(!wcscmp(fullPathNext, fullPath[fileIdx]))
			{
				duplicate = TRUE;
				break;
			}
		}
		if (!duplicate)
		{
			fullPath[fileCount++] = fullPathNext;
		}
		else
		{
			FREE(fullPathNext); /*skip duplicate file*/
		}
	}

	//Initialize all input file(s)
	for (fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		const DWORD attribs = getAttributes(fullPath[fileIdx], &lastModTs[fileIdx]);
		if (attribs == INVALID_FILE_ATTRIBUTES)
		{
			fwprintf(stderr, L"Error: File \"%s\" not found or access denied!\n\n", fullPath[fileIdx]);
			goto cleanup;
		}
		directory[fileIdx] = BOOLIFY(attribs & FILE_ATTRIBUTE_DIRECTORY);
		if ((!directory[fileIdx]) && (!opt_clear) && (attribs & FILE_ATTRIBUTE_ARCHIVE))
		{
			if (!opt_quiet)
			{
				fwprintf(stdout, L"%s\n", fullPath[fileIdx]); /*file was modified*/
			}
			goto success;
		}
	}

	//Clear the "archive" bit initially
	if (opt_clear)
	{
		for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
		{
			if (!directory[fileIdx])
			{
				if (!clearAttribute(fullPath[fileIdx], FILE_ATTRIBUTE_ARCHIVE))
				{
					fwprintf(stderr, L"Warning: File \"%s\" could not be cleared!\n\n", fullPath[fileIdx]);
				}
			}
		}
	}

	//Get directory part of path(s)
	for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		BOOL duplicate = FALSE;
		const wchar_t *const directoryNext = (!directory[fileIdx]) ? getDirectoryPart(fullPath[fileIdx]) : _wcsdup(fullPath[fileIdx]);
		if (!directoryNext)
		{
			fwprintf(stderr, L"Error: Directory part of \"%s\" could not be determined!\n\n", fullPath[fileIdx]);
			goto cleanup;
		}
		for (dirIdx = 0; dirIdx < dirCount; ++dirIdx)
		{
			if(!wcscmp(directoryNext, directoryPath[dirIdx]))
			{
				duplicate = TRUE;
				APPEND_TO_MAP(dirIdx, fileIdx);
				break;
			}
		}
		if (!duplicate)
		{
			APPEND_TO_MAP(dirCount, fileIdx);
			directoryPath[dirCount++] = directoryNext;
		}
		else
		{
			free((void*)directoryNext); /*skip duplicate directory*/
		}
	}

	//Print directory to file map (DEBUG)
	if (opt_debug)
	{
		for (dirIdx = 0; dirIdx < dirCount; ++dirIdx)
		{
			fwprintf(stderr, L"%02d: %s\n", dirIdx, directoryPath[dirIdx]);
			for (fileIdx = 0; fileIdx < dirToFilesMap[dirIdx].count; ++fileIdx)
			{
				fwprintf(stderr, L"   %02d: %s\n", fileIdx, fullPath[dirToFilesMap[dirIdx].files[fileIdx]]);
			}
		}
		wprintln(stderr, L"");
	}

	//Install file system watcher
	for (dirIdx = 0; dirIdx < dirCount; ++dirIdx)
	{
		notifyHandle[dirIdx] = FindFirstChangeNotificationW(directoryPath[dirIdx], FALSE, NOTIFY_FLAGS);
		if (notifyHandle[dirIdx] == INVALID_HANDLE_VALUE)
		{
			wprintln(stderr, L"System Error: Failed to install the file watcher!\n");
			goto cleanup;
		}
	}

	//Has any file been modified already?
	for (fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		if (!directory[fileIdx])
		{
			CHECK_IF_MODFIED(fileIdx);
		}
	}

	//Wait until a file has been modified
	for (;;)
	{
		//Wait for next event
		const DWORD status = WaitForMultipleObjects(dirCount, notifyHandle, FALSE, 29989U);
		if ((status >= WAIT_OBJECT_0) && (status < (WAIT_OBJECT_0 + dirCount)))
		{
			//Compute directory index
			const DWORD notifyIdx = status - WAIT_OBJECT_0;
			
			//Print DEBUG information
			if (opt_debug)
			{
				fwprintf(stderr, L"Directory #%02u was notified!\n", notifyIdx);
			}
			
			//Has any file been modified?
			for (fileIdx = 0; fileIdx < dirToFilesMap[notifyIdx].count; ++fileIdx)
			{
				CHECK_IF_MODFIED(dirToFilesMap[notifyIdx].files[fileIdx]);
			}
			
			//Request the *next* notification
			if (!FindNextChangeNotification(notifyHandle[notifyIdx]))
			{
				wprintln(stderr, L"Error: Failed to request next notification!\n");
				goto cleanup;
			}
		}
		else if (status == WAIT_TIMEOUT)
		{
			//Timeout encountered, check all files!
			for (fileIdx = 0; fileIdx < fileCount; ++fileIdx)
			{
				if (!directory[fileIdx])
				{
					CHECK_IF_MODFIED(fileIdx);
				}
			}
		}
		else
		{
			wprintln(stderr, L"System Error: Failed to wait for notification!\n");
			goto cleanup;
		}
	}

	//Completed successfully
success:
	result = EXIT_SUCCESS;
	if (opt_reset)
	{
		Sleep(25); /*some extra delay*/
		for (fileIdx = 0; fileIdx < fileCount; ++fileIdx)
		{
			if (!clearAttribute(fullPath[fileIdx], FILE_ATTRIBUTE_ARCHIVE))
			{
				fwprintf(stderr, L"Warning: File \"%s\" could not be reset!\n\n", fullPath[fileIdx]);
			}
		}
	}

	//Perform final clean-up
cleanup:
	for (dirIdx = 0; dirIdx < dirCount; ++dirIdx)
	{
		FREE(directoryPath[dirIdx]);
		CLOSE_NOTIFY_HANDLE(notifyHandle[dirIdx]);
	}
	for (fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		FREE(fullPath[fileIdx]);
	}

	return result; /*exit*/
}
