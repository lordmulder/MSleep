/*
 * FS utility functions
 * Created by LoRd_MuldeR <mulder2@gmx.de>.
 * 
 * This work is licensed under the CC0 1.0 Universal License.
 * To view a copy of the license, visit:
 * https://creativecommons.org/publicdomain/zero/1.0/legalcode
 */

#include <stdlib.h>
#include <wchar.h>

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <Shlwapi.h>

/* ======================================================================= */
/* STRING BUFFER HANDLING                                                  */
/* ======================================================================= */

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

/* ======================================================================= */
/* FILE ATTRIBUTES                                                         */
/* ======================================================================= */

DWORD getAttributes(const wchar_t *const filePath)
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

BOOL clearAttribute(const wchar_t *const filePath, const DWORD mask)
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

/* ======================================================================= */
/* CANONICAL FILE NAME*/
/* ======================================================================= */

typedef DWORD (WINAPI *PGETFINALPATHNAMEBYHANDLEW)(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags);
static volatile LONG getFinalPathNameInit = 0L;
static PGETFINALPATHNAMEBYHANDLEW getFinalPathNamePtr = NULL;

#define _SAFE_WITHOUT_PREFIX(OFF) \
	((!wcschr(buffer + (OFF), L'/')) && (!wcsstr(buffer + (OFF), L".\\")) && (buffer[result - 1U] != L'.'))

static __inline const wchar_t* getFinalPathName(const wchar_t *const fileName)
{
	LONG loop = 0L;
	wchar_t *buffer = NULL;
	size_t size = 0UL;
	HANDLE handle = NULL;

	//Initialize on first call
	while((loop = InterlockedCompareExchange(&getFinalPathNameInit, -1L, 0L)) != 1L)
	{
		if(!loop) /*first thread initializes*/
		{
			getFinalPathNamePtr = (PGETFINALPATHNAMEBYHANDLEW) GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetFinalPathNameByHandleW");
			InterlockedExchange(&getFinalPathNameInit, 1L);
		}
	}

	//Is available?
	if(!getFinalPathNamePtr)
	{
		goto failure; /*GetFinalPathNameByHandleW unavailable!*/
	}

	//Try to open file
	handle = CreateFileW(fileName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		goto failure;
	}

	for(loop = 0L; loop < 3L; ++loop)
	{
		//Try to get full path name
		const DWORD result = getFinalPathNamePtr(handle, buffer, size, VOLUME_NAME_DOS);
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

		//Close file handle now!
		CloseHandle(handle);

		//Remove ‘\\?\’ or ‘\\?\UNC\’ prefix, iff it's safe
		if ((result > 6U) && ((result - 4U) < MAX_PATH) && (!wcsncmp(buffer, L"\\\\?\\", 4U)) && iswalpha(buffer[4U]) && (!wcsncmp(buffer + 5U, L":\\", 2U)))
		{
			if (_SAFE_WITHOUT_PREFIX(6U))
			{
				wmemmove(buffer, buffer + 4U, result - 3U); 
			}
		}
		else if ((result > 8U) && ((result - 6U) < MAX_PATH) && (!wcsncmp(buffer, L"\\\\?\\UNC\\", 8U)))
		{
			if (_SAFE_WITHOUT_PREFIX(8U))
			{
				wmemmove(buffer + 2U, buffer + 8U, result - 7U); 
			}
		}
		
		return buffer; /*success*/
	}

failure:
	if (buffer)
	{
		free(buffer);
	}
	if (handle && (handle != INVALID_HANDLE_VALUE))
	{
		CloseHandle(handle);
	}

	return NULL; /*failure*/
}

const wchar_t* getCanonicalPath(const wchar_t *const fileName)
{
	const wchar_t* canonicalPath = getFinalPathName(fileName);
	if(!canonicalPath)
	{
		canonicalPath = _wfullpath(NULL, fileName, 0UL);
	}

	return canonicalPath;
}

/* ======================================================================= */
/* GET DIRECTORY PART                                                      */
/* ======================================================================= */

const wchar_t* getDirectoryPart(const wchar_t *const fullPath)
{
	wchar_t *const buffer = _wcsdup(fullPath);
	if (buffer)
	{
		if(PathRemoveFileSpecW(buffer))
		{
			return buffer;
		}
		free((void*)buffer); /*clean-up*/
	}

	return NULL;
}
