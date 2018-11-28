/*
 * notifywait for Win32
 * Created by LoRd_MuldeR <mulder2@gmx.de>.
 * 
 * This work is licensed under the CC0 1.0 Universal License.
 * To view a copy of the license, visit:
 * https://creativecommons.org/publicdomain/zero/1.0/legalcode
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <io.h>
#include <fcntl.h>

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

static BOOL __stdcall crtlHandler(DWORD dwCtrlTyp)
{
	switch (dwCtrlTyp)
	{
	case CTRL_C_EVENT:
		fputws(L"Ctrl+C: Notifywait has been interrupted !!!\n", stderr);
		break;
	case CTRL_BREAK_EVENT:
		fputws(L"Break: Notifywait has been interrupted !!!\n", stderr);
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
		NAME = TRUE; \
		continue; \
	}

#define CHECK_IF_MODFIED(IDX) do \
{ \
	unsigned long long _timeStamp; \
	const DWORD _attribs = getAttributes(fullPath[(IDX)], &_timeStamp); \
	if ((_attribs == INVALID_FILE_ATTRIBUTES) || (_attribs & FILE_ATTRIBUTE_DIRECTORY)) \
	{ \
		fwprintf(stderr, L"Error: File \"%s\" does not exist anymore!\n", fullPath[(IDX)]); \
		goto cleanup; \
	} \
	if ((_attribs & FILE_ATTRIBUTE_ARCHIVE) || (_timeStamp != lastModTs[(IDX)])) \
	{ \
		if (!quiet) \
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

/* ======================================================================= */
/* MAIN                                                                    */
/* ======================================================================= */

/*Globals*/
static const wchar_t *fullPath[MAXIMUM_FILES];
static const wchar_t* directoryPath[MAXIMUM_FILES];
static unsigned long long lastModTs[MAXIMUM_FILES];
static fileIndex_list dirToFilesMap[MAXIMUM_FILES];
static HANDLE notifyHandle[MAXIMUM_FILES];

int wmain(int argc, wchar_t *argv[])
{
	BOOL clear = FALSE, reset = FALSE, quiet = FALSE, debug = FALSE;
	int result = EXIT_FAILURE, argOffset = 1, fileCount = 0, fileIdx = 0, dirCount = 0, dirIdx = 0;

	//Initialize
	SetErrorMode(SetErrorMode(0x0003) | 0x0003);
	setlocale(LC_ALL, "C");
	SetConsoleCtrlHandler(crtlHandler, TRUE);
	_setmode(_fileno(stdout), _O_U8TEXT);
	_setmode(_fileno(stderr), _O_U8TEXT);

	//Check command-line arguments
	if (argc < 2)
	{
		fputws(L"notifywait [" TEXT(__DATE__) L"]\n", stderr);
		fputws(L"Wait until a file is changed. File changes are detected via \"archive\" bit.\n\n", stderr);
		fputws(L"Usage:\n", stderr);
		fputws(L"   notifywait.exe [options] <filename_1> [<filename_2> ... <filename_N>]\n\n", stderr);
		fputws(L"Options:\n", stderr);
		fputws(L"   --clear  unset the \"archive\" bit *before* monitoring for file changes\n", stderr);
		fputws(L"   --reset  unset the \"archive\" bit *after* a file change was detected\n", stderr);
		fputws(L"   --quiet  do *not* print the file name that changed to standard output\n", stderr);
		fputws(L"   --debug  turn *on* additional diagnostic output (for testing only!)\n\n", stderr);
		fputws(L"Remarks:\n", stderr);
		fputws(L"   The operating system sets the \"archive\" bit whenever a file is changed.\n", stderr);
		fputws(L"   If, initially, the \"archive\" bit is set, program terminates right away.\n", stderr);
		fputws(L"   If *multiple* files are given, program terminates on *any* file change.\n", stderr);
		goto cleanup;
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
		fwprintf(stderr, L"Error: Unknown option \"%s\" encountered!\n", argv[argOffset]);
		goto cleanup;
	}

	//Check remaining file count
	if (argOffset >= argc)
	{
		fputws(L"Error: No file name(s) specified. Nothing to do!\n", stderr);
		goto cleanup;
	}
	if ((argc - argOffset) > MAXIMUM_FILES)
	{
		fwprintf(stderr, L"Error: Too many file name(s) specified! [limit: %d]\n", MAXIMUM_FILES);
		goto cleanup;
	}

	//Convert all input file(s) to absoloute paths
	for (; argOffset < argc; ++argOffset)
	{
		BOOL duplicate = FALSE;
		const wchar_t *fullPathNext = getCanonicalPath(argv[argOffset]);
		if (!fullPathNext)
		{
			fwprintf(stderr, L"Error: Path \"%s\" could not be resolved!\n", argv[argOffset]);
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
			fwprintf(stderr, L"Error: File \"%s\" not found or access denied!\n", fullPath[fileIdx]);
			goto cleanup;
		}
		if (attribs & FILE_ATTRIBUTE_DIRECTORY)
		{
			fwprintf(stderr, L"Error: Path \"%s\" points to a directory!\n", fullPath[fileIdx]);
			goto cleanup;
		}
		if ((!clear) && (attribs & FILE_ATTRIBUTE_ARCHIVE))
		{
			if (!quiet)
			{
				fwprintf(stdout, L"%s\n", fullPath[fileIdx]); /*file was modified*/
			}
			goto success;
		}
	}

	//Clear the "archive" bit initially
	if (clear)
	{
		for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
		{
			if (!clearAttribute(fullPath[fileIdx], FILE_ATTRIBUTE_ARCHIVE))
			{
				fwprintf(stderr, L"Warning: File \"%s\" could not be cleared!\n", fullPath[fileIdx]);
			}
		}
	}

	//Get directory part of path(s)
	for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		BOOL duplicate = FALSE;
		const wchar_t *const directoryNext = getDirectoryPart(fullPath[fileIdx]);
		if (!directoryNext)
		{
			fwprintf(stderr, L"Error: Directory part of \"%s\" could not be determined!\n", fullPath[fileIdx]);
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
	if (debug)
	{
		for (dirIdx = 0; dirIdx < dirCount; ++dirIdx)
		{
			fwprintf(stderr, L"%02d: %s\n", dirIdx, directoryPath[dirIdx]);
			for (fileIdx = 0; fileIdx < dirToFilesMap[dirIdx].count; ++fileIdx)
			{
				fwprintf(stderr, L"   %02d: %s\n", fileIdx, fullPath[dirToFilesMap[dirIdx].files[fileIdx]]);
			}
		}
	}

	//Install file system watcher
	for (dirIdx = 0; dirIdx < dirCount; ++dirIdx)
	{
		notifyHandle[dirIdx] = FindFirstChangeNotificationW(directoryPath[dirIdx], FALSE, NOTIFY_FLAGS);
		if (notifyHandle[dirIdx] == INVALID_HANDLE_VALUE)
		{
			fputws(L"System Error: Failed to install the file watcher!\n", stderr);
			goto cleanup;
		}
	}

	//Has any file been modified?
	for (fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		CHECK_IF_MODFIED(fileIdx);
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
			if (debug)
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
				fputws(L"Error: Failed to request next notification!\n", stderr);
				goto cleanup;
			}
		}
		else if (status == WAIT_TIMEOUT)
		{
			//Timeout encountered, check all files!
			for (fileIdx = 0; fileIdx < fileCount; ++fileIdx)
			{
				CHECK_IF_MODFIED(fileIdx);
			}
		}
		else
		{
			fputws(L"System Error: Failed to wait for notification!\n", stderr);
			goto cleanup;
		}
	}

	//Completed successfully
success:
	result = EXIT_SUCCESS;
	if (reset)
	{
		Sleep(25); /*some extra delay*/
		for (fileIdx = 0; fileIdx < fileCount; ++fileIdx)
		{
			if (!clearAttribute(fullPath[fileIdx], FILE_ATTRIBUTE_ARCHIVE))
			{
				fwprintf(stderr, L"Warning: File \"%s\" could not be reset!\n", fullPath[fileIdx]);
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
