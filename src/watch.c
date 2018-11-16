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

static BOOL resizeBuffer(wchar_t **const buffer, const size_t size)
{
	wchar_t *const ptr = (wchar_t*)realloc(*buffer, sizeof(wchar_t) * size);
	if (ptr)
	{
		*buffer = ptr;
		return TRUE;
	}

	return FALSE;
}

static const wchar_t* getFullPath(const wchar_t *const fileName)
{
	int loop;
	wchar_t *buffer = NULL;
	DWORD bufferLen;

	//Get required buffer size
	bufferLen = GetFullPathNameW(fileName, 0, NULL, NULL);
	if (bufferLen < 1U)
	{
		goto failure;
	}

	//Allocate initial buffer
	if (!resizeBuffer(&buffer, bufferLen))
	{
		goto failure;
	}

	//Get the full path name
	for(loop = 0; loop < 97; ++loop)
	{
		const DWORD result = GetFullPathNameW(fileName, bufferLen, buffer, NULL);
		if (result < 1U)
		{
			goto failure;
		}

		//Increase buffer size as needed
		if (result >= bufferLen)
		{
			if (!resizeBuffer(&buffer, bufferLen = result))
			{
				goto failure;
			}
			continue;
		}
		return buffer;
	}

failure:
	if (buffer)
	{
		free(buffer);
	}

	return NULL;
}

static const wchar_t* getDirectoryPart(const wchar_t *const fullPath)
{
	wchar_t *const buffer = _wcsdup(fullPath);
	if (buffer)
	{
		PathRemoveFileSpecW(buffer);
		return buffer;
	}

	return NULL;
}

static const BOOL clearAttribute(const wchar_t *const filePath, const DWORD clearAttrib)
{
	int loop;

	for (loop = 0; loop < 97; ++loop)
	{
		const DWORD attribs = GetFileAttributesW(filePath);
		if (attribs == INVALID_FILE_ATTRIBUTES)
		{
			break;
		}
		if (!(attribs & clearAttrib))
		{
			return TRUE; /*attrib successfully cleared*/
		}
		else
		{
			if (!SetFileAttributesW(filePath, attribs & (~clearAttrib)))
			{
				break;
			}
			Sleep(1); /*small delay!*/
		}
	}

	return FALSE;
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

int wmain(int argc, wchar_t *argv[])
{
	int clear = FALSE, result = EXIT_FAILURE;
	const wchar_t *fullPath = NULL, *directoryPath = NULL;
	DWORD attribs = 0UL, status = 0UL;
	HANDLE watcher = NULL;

	//Initialize
	SetErrorMode(SetErrorMode(0x0003) | 0x0003);
	setlocale(LC_ALL, "C");
	SetConsoleCtrlHandler(crtlHandler, TRUE);

	//Check command-line arguments
	if (argc < 2)
	{
		fputs("file change watcher [" __DATE__ "]\n\n", stderr);
		fputs("Wait until the file has changed. File changes are detected via \"archive\" bit.\n", stderr);
		fputs("If initially the \"archive\" bit is already set, program terminates immeditely.\n\n", stderr);
		fputs("Usage:\n   watch.exe [--clear] <file_name>\n\n", stderr);
		fputs("With option \"--clear\", the archive bit is cleared after a change was dected!\n\n", stderr);
		return EXIT_FAILURE;
	}

	//Clear archive bit?
	clear = (argc > 2) && (!_wcsicmp(argv[1U], L"--clear"));

	//Get the full path
	fullPath = getFullPath(argv[clear ? 2U : 1U]);
	if (!fullPath)
	{
		fputs("Error: Failed to get full path name!\n\n", stderr);
		return EXIT_FAILURE;
	}

	//Get initial file attributes
	attribs = GetFileAttributesW(fullPath);
	if (attribs == INVALID_FILE_ATTRIBUTES)
	{
		fprintf(stderr, "Error: File \"%S\" not found!\n\n", fullPath);
		free((void*)fullPath);
		return EXIT_FAILURE;
	}

	//Has file been modified yet?
	if (attribs & FILE_ATTRIBUTE_ARCHIVE)
	{
		if (clear)
		{
			if (!clearAttribute(fullPath, FILE_ATTRIBUTE_ARCHIVE))
			{
				fputs("Warning: Failed to clear archive bit!\n\n", stderr);
			}
		}
		free((void*)fullPath);
		return EXIT_SUCCESS;
	}

	//Get directory path
	directoryPath = getDirectoryPart(fullPath);

	//Install file system watcher
	watcher = FindFirstChangeNotificationW(directoryPath, FALSE, FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
	if (watcher == INVALID_HANDLE_VALUE)
	{
		fputs("Error: Failed to install the file watcher!\n\n", stderr);
		free((void*)fullPath);
		free((void*)directoryPath);
		return EXIT_FAILURE;
	}

	//Wait until file has been modified
	for (;;)
	{
		//Update file attributes
		attribs = GetFileAttributesW(fullPath);
		if (attribs == INVALID_FILE_ATTRIBUTES)
		{
			fputs("Error: File does not seem to exist anymore!\n\n", stderr);
			break;
		}

		//Has file been modified?
		if (attribs & FILE_ATTRIBUTE_ARCHIVE)
		{
			if (clear)
			{
				Sleep(25); /*some extra delay*/
				if (!clearAttribute(fullPath, FILE_ATTRIBUTE_ARCHIVE))
				{
					fputs("Warning: Failed to clear archive bit!\n\n", stderr);
				}
			}
			result = EXIT_SUCCESS;
			break;
		}

		//Wait for next modification
		status = WaitForSingleObject(watcher, INFINITE);
		if (status != WAIT_OBJECT_0)
		{
			fputs("Error: Failed to wait for notification!\n\n", stderr);
			break;
		}

		//Request next notification, iff we were notified
		if (!FindNextChangeNotification(watcher))
		{
			fputs("Error: Failed to request next notification!\n\n", stderr);
			break;
		}
	}

	//Final clean-up
	FindCloseChangeNotification(watcher);
	free((void*)fullPath);
	free((void*)directoryPath);

	return result;
}
