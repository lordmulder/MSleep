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
#include <io.h>
#include <fcntl.h>

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <Shlwapi.h>

//VC 6.0 workaround
#ifdef ENABLE_VC6_WORKAROUNDS
_CRTIMP extern FILE _iob[];
#undef stdout
#undef stderr
#define stdout (&_iob[1])
#define stderr (&_iob[2])
#endif //ENABLE_VC6_WORKAROUNDS

#define NOTIFY_FLAGS (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE)

/* ======================================================================= */
/* UTILITY FUNCTIONS                                                       */
/* ======================================================================= */

static __inline const DWORD getAttributes(const wchar_t *const filePath)
{
	DWORD loop, attribs;

	for (loop = 0U; loop <= 13U; ++loop)
	{
		if (loop > 0U)
		{
			Sleep(loop);
		}
		if ((attribs = GetFileAttributesW(filePath)) != INVALID_FILE_ATTRIBUTES)
		{
			return attribs;
		}
	}

	return INVALID_FILE_ATTRIBUTES;
}

static __inline const BOOL clearAttribute(const wchar_t *const filePath, const DWORD mask)
{
	DWORD loop, attribs;

	for (loop = 0U; loop < 31U; ++loop)
	{
		if ((attribs = getAttributes(filePath)) == INVALID_FILE_ATTRIBUTES)
		{
			break;
		}
		if (!(attribs & mask))
		{
			return TRUE; /*attrib successfully cleared*/
		}
		else
		{
			SetFileAttributesW(filePath, attribs & (~mask));
			Sleep(1); /*small delay!*/
		}
	}

	return FALSE;
}

static __inline BOOL resizeBuffer(wchar_t **const buffer, size_t *const size, const size_t requiredSize)
{
	if((!(*buffer)) || (*size != requiredSize))
	{
		//Try to allocate new buffer
		wchar_t *const bufferNext = (wchar_t*) realloc(*buffer, sizeof(wchar_t) * requiredSize);
		if (!bufferNext)
		{
			return FALSE; /*allocation failed*/
		}

		*buffer = bufferNext;
		*size = requiredSize;
	}
	
	return TRUE; /*success*/
}

static __inline const wchar_t* getFullPath(const wchar_t *const fileName)
{
	int loop;
	wchar_t *buffer = NULL;
	size_t size = 0UL;

	for(loop = 0; loop < 7; ++loop)
	{
		//Try to get full path name
		const DWORD result = GetFullPathNameW(fileName, size, buffer, NULL);
		if (result < 1U)
		{
			goto failure;
		}

		//Increase buffer size as needed
		if (result > size)
		{
			if (!resizeBuffer(&buffer, &size, result))
			{
				goto failure;
			}
			continue;
		}

		return buffer; /*success*/
	}

failure:
	if (buffer)
	{
		free(buffer);
	}

	return NULL; /*failure*/
}

static __inline const wchar_t* getDirectoryPart(const wchar_t *const fullPath)
{
	wchar_t *const buffer = _wcsdup(fullPath);
	if (buffer)
	{
		PathRemoveFileSpecW(buffer);
		return buffer;
	}

	return NULL;
}

static BOOL __stdcall crtlHandler(DWORD dwCtrlTyp)
{
	switch (dwCtrlTyp)
	{
	case CTRL_C_EVENT:
		fputs("Ctrl+C: Watcher has been interrupted !!!\n\n", stderr);
		break;
	case CTRL_BREAK_EVENT:
		fputs("Break: Watcher has been interrupted !!!\n\n", stderr);
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

#define TRY_PARSE_OPTION(NAME) \
	if(!_wcsicmp(argv[fileNameOffset] + 2U, L#NAME)) \
	{ \
		NAME = TRUE; \
		++fileNameOffset; \
		continue; \
	}

#define CHECK_IF_MODFIED(IDX) do \
{ \
	if ((attribs = getAttributes(fullPath[(IDX)])) == INVALID_FILE_ATTRIBUTES) \
	{ \
		fprintf(stderr, "Error: File \"%S\" not found or access denied!\n\n", fullPath[(IDX)]); \
		goto cleanup; \
	} \
	if (attribs & FILE_ATTRIBUTE_DIRECTORY) \
	{ \
		fprintf(stderr, "Error: Path \"%S\" is a directory!\n\n", fullPath[(IDX)]); \
		goto cleanup; \
	} \
	if (attribs & FILE_ATTRIBUTE_ARCHIVE) \
	{ \
		if(!quiet) \
		{ \
			fputws(fullPath[(IDX)], stdout); \
		} \
		goto success; \
	} \
} \
while(0)

static const wchar_t *fullPath[MAXIMUM_WAIT_OBJECTS];
static const wchar_t* directoryPath[MAXIMUM_WAIT_OBJECTS];
static HANDLE watcher[MAXIMUM_WAIT_OBJECTS];

int wmain(int argc, wchar_t *argv[])
{
	DWORD attribs = 0UL, status = 0UL;
	int result = EXIT_FAILURE, fileNameOffset = 1, fileCount = 0, fileIdx = 0;
	BOOL clear = FALSE, reset = FALSE, quiet = FALSE;

	//Initialize
	SetErrorMode(SetErrorMode(0x0003) | 0x0003);
	setlocale(LC_ALL, "C");
	SetConsoleCtrlHandler(crtlHandler, TRUE);
	_setmode(_fileno(stdout), _O_U8TEXT);

	//Check command-line arguments
	if (argc < 2)
	{
		fputs("file change watcher [" __DATE__ "]\n", stderr);
		fputs("Wait until a file is changed. File changes are detected via \"archive\" bit.\n\n", stderr);
		fputs("Usage:\n", stderr);
		fputs("   watch.exe [options] <filename_1> [<filename_2> ... <filename_N>]\n\n", stderr);
		fputs("Options:\n", stderr);
		fputs("   --clear  unset the \"archive\" bit *before* monitoring for file changes\n", stderr);
		fputs("   --reset  unset the \"archive\" bit *after* a file change was detected\n", stderr);
		fputs("   --quiet  do *not* print the file name that changed to standard output\n\n", stderr);
		fputs("Remarks:\n", stderr);
		fputs("   The operating system sets the \"archive\" bit whenever a file is changed.\n", stderr);
		fputs("   If, initially, the \"archive\" bit is set, program terminates right away.\n", stderr);
		fputs("   If *multiple* files are given, program terminates on *any* file change.\n\n", stderr);
		goto cleanup;
	}

	//Parse command-line arguments
	while ((fileNameOffset < argc) && (!wcsncmp(argv[fileNameOffset], L"--", 2)))
	{
		if(!argv[fileNameOffset][2U])
		{
			break; /*stop argument parsing*/
		}
		TRY_PARSE_OPTION(clear)
		TRY_PARSE_OPTION(reset)
		TRY_PARSE_OPTION(quiet)
		fprintf(stderr, "Error: Unknown option \"%S\" encountered!\n\n", argv[fileNameOffset]);
		goto cleanup;
	}

	//Check file count
	if((fileCount = (argc - fileNameOffset)) <= 0)
	{
		fputs("Error: No file name(s) specified. Nothing to do!\n\n", stderr);
		goto cleanup;
	}
	if(fileCount > MAXIMUM_WAIT_OBJECTS)
	{
		fprintf(stderr, "Error: Too many file name(s) specified! (limit: %d)\n\n", MAXIMUM_WAIT_OBJECTS);
		goto cleanup;
	}

	//Get the full path(s)
	for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		if (!(fullPath[fileIdx] = getFullPath(argv[fileNameOffset + fileIdx])))
		{
			fputs("Error: Failed to get full path name!\n\n", stderr);
			goto cleanup;
		}
	}

	//Has any file already been modified yet?
	if (!clear)
	{
		for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
		{
			CHECK_IF_MODFIED(fileIdx);
		}
	}
	else
	{
		for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
		{
			if (!clearAttribute(fullPath[fileIdx], FILE_ATTRIBUTE_ARCHIVE))
			{
				fprintf(stderr, "Warning: File \"%S\" could not be cleared!\n\n", fullPath[fileIdx]);
			}
		}
	}

	//Get directory path
	for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		if(!(directoryPath[fileIdx] = getDirectoryPart(fullPath[fileIdx])))
		{
			fputs("Error: Failed to get directory part!\n\n", stderr);
			goto cleanup;
		}
	}

	//Install file system watcher
	for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		watcher[fileIdx] = FindFirstChangeNotificationW(directoryPath[fileIdx], FALSE, NOTIFY_FLAGS);
		if (watcher[fileIdx] == INVALID_HANDLE_VALUE)
		{
			fputs("System Error: Failed to install the file watcher!\n\n", stderr);
			goto cleanup;
		}
	}

	//Has any file been modified?
	for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		CHECK_IF_MODFIED(fileIdx);
	}

	//Wait until a file has been modified
	for (;;)
	{
		//Wait for next event
		status = WaitForMultipleObjects(fileCount, watcher, FALSE, 29989U);
		if ((status >= WAIT_OBJECT_0) && (status < (WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS)))
		{
			//Has the file been modified?
			const DWORD notifyIdx = (status - WAIT_OBJECT_0);
			CHECK_IF_MODFIED(notifyIdx);

			//Request the *next* notification
			if (!FindNextChangeNotification(watcher[notifyIdx]))
			{
				fputs("Error: Failed to request next notification!\n\n", stderr);
				goto cleanup;
			}
		}
		else if(status == WAIT_TIMEOUT)
		{
			//Timeout encountered, check all files!
			for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
			{
				CHECK_IF_MODFIED(fileIdx);
			}
		}
		else
		{
			fputs("System Error: Failed to wait for notification!\n\n", stderr);
			goto cleanup;
		}
	}

	//Completed successfully
success:
	result = EXIT_SUCCESS;
	if (reset)
	{
		Sleep(25); /*some extra delay*/
		for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
		{
			if (!clearAttribute(fullPath[fileIdx], FILE_ATTRIBUTE_ARCHIVE))
			{
				fprintf(stderr, "Warning: File \"%S\" could not be reset!\n\n", fullPath[fileIdx]);
			}
		}
	}

	//Perform final clean-up
cleanup:
	for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		if (watcher[fileIdx] && (watcher[fileIdx] != INVALID_HANDLE_VALUE))
		{
			FindCloseChangeNotification(watcher[fileIdx]);
		}
	}
	for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		if (fullPath[fileIdx])
		{
			free((void*)fullPath[fileIdx]);
		}
		if(directoryPath[fileIdx])
		{
			free((void*)directoryPath[fileIdx]);
		}
	}

	return result; /*exit*/
}
