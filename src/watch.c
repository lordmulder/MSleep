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
#include <Shlwapi.h>

//VC 6.0 workaround
_CRTIMP extern FILE _iob[];
#ifdef stderr
#undef stderr
#endif
#define stderr (&_iob[2])

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
	if(!_wcsicmp(argv[pos], L"--" L#NAME)) \
	{ \
		NAME = TRUE; \
		continue; \
	}

#define CHECK_IF_MODFIED() do \
{ \
	if ((attribs = getAttributes(fullPath)) == INVALID_FILE_ATTRIBUTES) \
	{ \
		fprintf(stderr, "Error: File \"%S\" not found or access denied!\n\n", fullPath); \
		goto cleanup; \
	} \
	if (attribs & FILE_ATTRIBUTE_ARCHIVE) \
	{ \
		goto success; \
	} \
} \
while(0)

int wmain(int argc, wchar_t *argv[])
{
	const wchar_t *fullPath = NULL, *directoryPath = NULL;
	DWORD attribs = 0UL, status = 0UL;
	int result = EXIT_FAILURE, clear = FALSE, reset = FALSE;
	HANDLE watcher = INVALID_HANDLE_VALUE;

	//Initialize
	SetErrorMode(SetErrorMode(0x0003) | 0x0003);
	setlocale(LC_ALL, "C");
	SetConsoleCtrlHandler(crtlHandler, TRUE);

	//Check command-line arguments
	if (argc < 2)
	{
		fputs("file change watcher [" __DATE__ "]\n\n", stderr);
		fputs("Wait until the file has changed. File changes are detected via \"archive\" bit.\n", stderr);
		fputs("The operating system sets the \"archive\" bit whenever a file is modified.\n", stderr);
		fputs("If, initially, the \"archive\" bit is already set, program terminates promptly.\n\n", stderr);
		fputs("Usage:\n", stderr);
		fputs("   watch.exe [--clear] [--reset] <file_name>\n\n", stderr);
		fputs("Options:\n", stderr);
		fputs("   --clear  unset the \"archive\" bit *before* monitoring for changes\n", stderr);
		fputs("   --reset  unset the \"archive\" bit *after* a change was detected\n\n", stderr);
		goto cleanup;
	}

	//Parse command-line arguments
	if (argc > 2)
	{
		int pos;
		for(pos = 1; pos < argc - 1; ++pos)
		{
			TRY_PARSE_OPTION(clear)
			TRY_PARSE_OPTION(reset)
			fprintf(stderr, "Error: Unknown option \"%S\" encountered!\n\n", argv[pos]);
			goto cleanup;
		}
	}

	//Get the full path
	fullPath = getFullPath(argv[argc-1]);
	if (!fullPath)
	{
		fputs("Error: Failed to get full path name!\n\n", stderr);
		goto cleanup;
	}

	//Has file already been modified yet?
	if (!clear)
	{
		CHECK_IF_MODFIED();
	}
	else
	{
		if (!clearAttribute(fullPath, FILE_ATTRIBUTE_ARCHIVE))
		{
			fputs("Warning: Failed to clear archive bit!\n\n", stderr);
		}
	}

	//Get directory path
	directoryPath = getDirectoryPart(fullPath);

	//Install file system watcher
	watcher = FindFirstChangeNotificationW(directoryPath, FALSE, FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
	if (watcher == INVALID_HANDLE_VALUE)
	{
		fputs("Error: Failed to install the file watcher!\n\n", stderr);
		goto cleanup;
	}

	//Has file been modified in the meantime?
	CHECK_IF_MODFIED();

	//Wait until file has been modified
	for (;;)
	{
		//Wait for next event
		status = WaitForSingleObject(watcher, INFINITE);
		if (status != WAIT_OBJECT_0)
		{
			fputs("Error: Failed to wait for notification!\n\n", stderr);
			goto cleanup;
		}

		//Has file been modified in the meantime?
		CHECK_IF_MODFIED();

		//Request next notification
		if (!FindNextChangeNotification(watcher))
		{
			fputs("Error: Failed to request next notification!\n\n", stderr);
			goto cleanup;
		}
	}

	//Completed successfully
success:
	result = EXIT_SUCCESS;
	if (reset)
	{
		Sleep(25); /*some extra delay*/
		if (!clearAttribute(fullPath, FILE_ATTRIBUTE_ARCHIVE))
		{
			fputs("Warning: Failed to reset archive bit!\n\n", stderr);
		}
	}

	//Perform final clean-up
cleanup:
	if (watcher != INVALID_HANDLE_VALUE)
	{
		FindCloseChangeNotification(watcher);
	}
	if (directoryPath)
	{
		free((void*)directoryPath);
	}
	if (fullPath)
	{
		free((void*)fullPath);
	}

	return result; /*exit*/
}
