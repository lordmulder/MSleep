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
#include <wchar.h>

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

static __inline const wchar_t* getFinalPath(const wchar_t *const fileName, DWORD *const error)
{
	typedef DWORD (__stdcall *PGETFINALPATHNAMEBYHANDLEW)(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags);
	static BOOL initialized = FALSE;
	static PGETFINALPATHNAMEBYHANDLEW getFinalPathNameByHandleW = NULL;

	int loop;
	wchar_t *buffer = NULL;
	size_t size = 0UL;
	HANDLE h = INVALID_HANDLE_VALUE;
	*error = (DWORD)(-1L);

	//Initialize on first call
	if(!initialized)
	{
		const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
		if(kernel32)
		{
			getFinalPathNameByHandleW = (PGETFINALPATHNAMEBYHANDLEW) GetProcAddress(kernel32, "GetFinalPathNameByHandleW");
		}
		initialized = TRUE;
	}

	//Check availability
	if(!getFinalPathNameByHandleW)
	{
		*error = ERROR_PROC_NOT_FOUND; /*unavailable!*/
		goto failure;
	}

	//Try to open file
	h = CreateFileW(fileName, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE)
	{
		*error = GetLastError();
		goto failure;
	}

	for(loop = 0; loop < 3; ++loop)
	{
		//Try to get full path name
		const DWORD result = getFinalPathNameByHandleW(h, buffer, size, VOLUME_NAME_DOS);
		if (result < 1U)
		{
			*error = GetLastError();
			goto failure;
		}

		//Increase buffer size as needed
		if (result > size)
		{
			if (!resizeBuffer(&buffer, &size, result))
			{
				*error = ERROR_OUTOFMEMORY;
				goto failure;
			}
			continue;
		}

		//Close file handle
		CloseHandle(h);

		//Remove "//?/" prefix, iff safe
		if ((result > 6U) && ((result - 4U) < MAX_PATH) && (!wcsncmp(buffer, L"\\\\?\\", 4U)) && iswalpha(buffer[4U]) && (!wcsncmp(buffer + 5U, L":\\", 2U)))
		{
			wmemmove(buffer, buffer + 4U, result - 3U); 
		}

		//Clean-up and return
		*error = ERROR_SUCCESS;
		return buffer;
	}

failure:
	if (buffer)
	{
		free(buffer);
	}
	if (h != INVALID_HANDLE_VALUE)
	{
		CloseHandle(h);
	}

	return NULL; /*failure*/
}

static __inline const wchar_t* getFullPath(const wchar_t *const fileName, DWORD *const error)
{
	int loop;
	wchar_t *buffer = NULL;
	size_t size = 0UL;
	*error = (DWORD)(-1L);

	for(loop = 0; loop < 3; ++loop)
	{
		//Try to get full path name
		const DWORD result = GetFullPathNameW(fileName, size, buffer, NULL);
		if (result < 1U)
		{
			*error = GetLastError();
			goto failure;
		}

		//Increase buffer size as needed
		if (result > size)
		{
			if (!resizeBuffer(&buffer, &size, result))
			{
				*error = ERROR_OUTOFMEMORY;
				goto failure;
			}
			continue;
		}

		*error = ERROR_SUCCESS;
		return buffer; /*success*/
	}

failure:
	if (buffer)
	{
		free(buffer);
	}

	return NULL; /*failure*/
}

static __inline const wchar_t* getNormalizedPath(const wchar_t *const fileName, DWORD *const error)
{
	const wchar_t *normalizedPath = getFinalPath(fileName, error);
	if((!normalizedPath) && (*error == ERROR_PROC_NOT_FOUND))
	{
		normalizedPath = getFullPath(fileName, error);
	}
	return normalizedPath;
}

static void printFilePathError(const DWORD error, const wchar_t *const fileName)
{
	switch(error)
	{
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		fwprintf(stderr, L"Error: File name \"%s\" could not be found!\n", fileName);
		break;
	case ERROR_ACCESS_DENIED:
		fwprintf(stderr, L"Error: Access to file \"%s\" was denied!\n", fileName);
		break;
	case ERROR_NOT_ENOUGH_MEMORY:
	case ERROR_OUTOFMEMORY:
		fputws(L"Error: Out of memory!\n", stderr);
		break;
	case ERROR_INVALID_NAME:
		fwprintf(stderr, L"Error: File name \"%s\" is invalid!\n", fileName);
		break;
	default:
		fwprintf(stderr, L"Error: Path \"%s\" could not be resolved!\n", fileName);
	}
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
		fputws(L"Ctrl+C: Watcher has been interrupted !!!\n", stderr);
		break;
	case CTRL_BREAK_EVENT:
		fputws(L"Break: Watcher has been interrupted !!!\n", stderr);
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

#define TRY_PARSE_OPTION(NAME) \
	if(!_wcsicmp(argv[argOffset] + 2U, L#NAME)) \
	{ \
		NAME = TRUE; \
		++argOffset; \
		continue; \
	}

#define CHECK_IF_MODFIED(IDX) do \
{ \
	if ((attribs = getAttributes(fullPath[(IDX)])) == INVALID_FILE_ATTRIBUTES) \
	{ \
		fwprintf(stderr, L"Error: File \"%s\" not found or access denied!\n", fullPath[(IDX)]); \
		goto cleanup; \
	} \
	if (attribs & FILE_ATTRIBUTE_DIRECTORY) \
	{ \
		fwprintf(stderr, L"Error: Path \"%s\" is a directory!\n", fullPath[(IDX)]); \
		goto cleanup; \
	} \
	if (attribs & FILE_ATTRIBUTE_ARCHIVE) \
	{ \
		if(!quiet) \
		{ \
			fwprintf(stdout, L"%s\n", fullPath[(IDX)]); \
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

typedef struct
{
	int count;
	int files[MAXIMUM_WAIT_OBJECTS];
}
fileIndex_list;

/* ======================================================================= */
/* MAIN                                                                    */
/* ======================================================================= */

/*Globals*/
static const wchar_t *fullPath[MAXIMUM_WAIT_OBJECTS];
static const wchar_t* directoryPath[MAXIMUM_WAIT_OBJECTS];
static fileIndex_list dirToFilesMap[MAXIMUM_WAIT_OBJECTS];
static HANDLE notifyHandle[MAXIMUM_WAIT_OBJECTS];

int wmain(int argc, wchar_t *argv[])
{
	DWORD attribs = 0UL, status = 0UL;
	int result = EXIT_FAILURE, argOffset = 1, fileCount = 0, fileIdx = 0, dirCount = 0, dirIdx = 0;
	BOOL clear = FALSE, reset = FALSE, quiet = FALSE;

	//Initialize
	SetErrorMode(SetErrorMode(0x0003) | 0x0003);
	setlocale(LC_ALL, "C");
	SetConsoleCtrlHandler(crtlHandler, TRUE);
	_setmode(_fileno(stdout), _O_U8TEXT);
	_setmode(_fileno(stderr), _O_U8TEXT);

	//Check command-line arguments
	if (argc < 2)
	{
		fputws(L"file change watcher [" TEXT(__DATE__) L"]\n", stderr);
		fputws(L"Wait until a file is changed. File changes are detected via \"archive\" bit.\n\n", stderr);
		fputws(L"Usage:\n", stderr);
		fputws(L"   watch.exe [options] <filename_1> [<filename_2> ... <filename_N>]\n\n", stderr);
		fputws(L"Options:\n", stderr);
		fputws(L"   --clear  unset the \"archive\" bit *before* monitoring for file changes\n", stderr);
		fputws(L"   --reset  unset the \"archive\" bit *after* a file change was detected\n", stderr);
		fputws(L"   --quiet  do *not* print the file name that changed to standard output\n\n", stderr);
		fputws(L"Remarks:\n", stderr);
		fputws(L"   The operating system sets the \"archive\" bit whenever a file is changed.\n", stderr);
		fputws(L"   If, initially, the \"archive\" bit is set, program terminates right away.\n", stderr);
		fputws(L"   If *multiple* files are given, program terminates on *any* file change.\n", stderr);
		goto cleanup;
	}

	//Parse command-line options
	while ((argOffset < argc) && (!wcsncmp(argv[argOffset], L"--", 2)))
	{
		if(!argv[argOffset][2U])
		{
			++argOffset;
			break; /*stop option parsing*/
		}
		TRY_PARSE_OPTION(clear)
		TRY_PARSE_OPTION(reset)
		TRY_PARSE_OPTION(quiet)
		fwprintf(stderr, L"Error: Unknown option \"%s\" encountered!\n", argv[argOffset]);
		goto cleanup;
	}

	//Check remaining file count
	if (argOffset >= argc)
	{
		fputws(L"Error: No file name(s) specified. Nothing to do!\n", stderr);
		goto cleanup;
	}
	if ((argc - argOffset) > MAXIMUM_WAIT_OBJECTS)
	{
		fwprintf(stderr, L"Error: Too many file name(s) specified! [limit: %d]\n", MAXIMUM_WAIT_OBJECTS);
		goto cleanup;
	}

	//Get normalized path(s) of input files
	while (argOffset < argc)
	{
		BOOL duplicate = FALSE;
		DWORD error = 0L;
		const wchar_t *const fullPathNext = getNormalizedPath(argv[argOffset++], &error);
		if (!fullPathNext)
		{
			printFilePathError(error, argv[argOffset-1]);
			goto cleanup;
		}
		for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
		{
			if(!wcscmp(fullPathNext, fullPath[fileIdx]))
			{
				duplicate = TRUE;
				break;
			}
		}
		if(!duplicate)
		{
			fullPath[fileCount++] = fullPathNext;
		}
		else
		{
			free((void*)fullPathNext); /*skip duplicate file*/
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
				fwprintf(stderr, L"Warning: File \"%s\" could not be cleared!\n", fullPath[fileIdx]);
			}
		}
	}

	//Get directory part of path(s)
	for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		BOOL duplicate = FALSE;
		const wchar_t *const directoryNext = getDirectoryPart(fullPath[fileIdx]);
		if(!directoryNext)
		{
			fwprintf(stderr, L"Error: Directory part of \"%s\" could not be determined!\n", fullPath[fileIdx]);
			goto cleanup;
		}
		for(dirIdx = 0; dirIdx < dirCount; ++dirIdx)
		{
			if(!wcscmp(directoryNext, directoryPath[dirIdx]))
			{
				duplicate = TRUE;
				APPEND_TO_MAP(dirIdx, fileIdx);
				break;
			}
		}
		if(!duplicate)
		{
			APPEND_TO_MAP(dirCount, fileIdx);
			directoryPath[dirCount++] = directoryNext;
		}
		else
		{
			free((void*)directoryNext); /*skip duplicate directory*/
		}
	}

#ifndef NDEBUG
	for(dirIdx = 0; dirIdx < dirCount; ++dirIdx)
	{
		fwprintf(stderr, L"%d: %s\n", dirIdx, directoryPath[dirIdx]);
		for(fileIdx = 0; fileIdx < dirToFilesMap[dirIdx].count; ++fileIdx)
		{
			fwprintf(stderr, L"--> %d: %s\n", fileIdx, fullPath[dirToFilesMap[dirIdx].files[fileIdx]]);
		}
	}
#endif //NDEBUG

	//Install file system watcher
	for(dirIdx = 0; dirIdx < dirCount; ++dirIdx)
	{
		notifyHandle[dirIdx] = FindFirstChangeNotificationW(directoryPath[dirIdx], FALSE, NOTIFY_FLAGS);
		if (notifyHandle[dirIdx] == INVALID_HANDLE_VALUE)
		{
			fputws(L"System Error: Failed to install the file watcher!\n", stderr);
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
		status = WaitForMultipleObjects(dirCount, notifyHandle, FALSE, 29989U);
		if ((status >= WAIT_OBJECT_0) && (status < (WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS)))
		{
			//Has any file been modified?
			const DWORD notifyIdx = status - WAIT_OBJECT_0;
			for(fileIdx = 0; fileIdx < dirToFilesMap[notifyIdx].count; ++fileIdx)
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
		for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
		{
			if (!clearAttribute(fullPath[fileIdx], FILE_ATTRIBUTE_ARCHIVE))
			{
				fwprintf(stderr, L"Warning: File \"%s\" could not be reset!\n", fullPath[fileIdx]);
			}
		}
	}

	//Perform final clean-up
cleanup:
	for(dirIdx = 0; dirIdx < dirCount; ++dirIdx)
	{
		if (notifyHandle[dirIdx] && (notifyHandle[dirIdx] != INVALID_HANDLE_VALUE))
		{
			FindCloseChangeNotification(notifyHandle[dirIdx]);
		}
		if(directoryPath[dirIdx])
		{
			free((void*)directoryPath[dirIdx]);
		}
	}
	for(fileIdx = 0; fileIdx < fileCount; ++fileIdx)
	{
		if (fullPath[fileIdx])
		{
			free((void*)fullPath[fileIdx]);
		}

	}

	return result; /*exit*/
}
