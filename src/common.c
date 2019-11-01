/*
 * FS utility functions
 * Created by LoRd_MuldeR <mulder2@gmx.de>.
 * 
 * This work is licensed under the CC0 1.0 Universal License.
 * To view a copy of the license, visit:
 * https://creativecommons.org/publicdomain/zero/1.0/legalcode
 */

#define _CRT_SECURE_NO_WARNINGS
#include "common.h"

#include <stdlib.h>
#include <wchar.h>
#include <Shlwapi.h>

/* ======================================================================= */
/* PARSE UNSIGNED LONG                                                     */
/* ======================================================================= */

int parseULong(const wchar_t *str, ULONG *const out)
{
	unsigned long long value;
	char c;
	while(isspace(*str))
	{
		str++;
	}
	if(swscanf(str, _wcsnicmp(str, L"0x", 2) ? L"%I64u %c" : L"%I64x %c", &value, &c) != 1)
	{
		return EINVAL;
	}
	if(value > ULONG_MAX)
	{
		return ERANGE;
	}
	*out = (unsigned long) value;
	return 0;
}

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
/* TIME FUNCTIONS                                                          */
/* ======================================================================= */

static __inline unsigned long long fileTimeToMSec(const FILETIME *const fileTime)
{
	ULARGE_INTEGER tmp;
	tmp.HighPart = fileTime->dwHighDateTime;
	tmp.LowPart = fileTime->dwLowDateTime;
	return tmp.QuadPart;
}

unsigned long long getCurrentTime(void)
{
	FILETIME now;
	GetSystemTimeAsFileTime(&now);
	return fileTimeToMSec(&now);
}

unsigned long long getStartupTime(void)
{
	FILETIME timeCreation, timeExit, timeKernel, timeUser;
	GetProcessTimes(GetCurrentProcess(), &timeCreation, &timeExit, &timeKernel, &timeUser);
	return fileTimeToMSec(&timeCreation);
}

/* ======================================================================= */
/* FILE ATTRIBUTES                                                         */
/* ======================================================================= */

DWORD getAttributes(const wchar_t *const filePath, unsigned long long *const timeStamp)
{
	DWORD loop;
	WIN32_FILE_ATTRIBUTE_DATA attribs;

	for (loop = 0U; loop <= 7U; ++loop)
	{
		if (loop > 0U)
		{
			Sleep(loop);
		}
		if (GetFileAttributesExW(filePath, GetFileExInfoStandard, &attribs))
		{
			if(timeStamp)
			{
				*timeStamp = fileTimeToMSec(&attribs.ftLastWriteTime);
			}
			return attribs.dwFileAttributes;
		}
	}

	if(timeStamp)
	{
		*timeStamp = 0ULL;
	}

	return INVALID_FILE_ATTRIBUTES;
}

BOOL clearAttribute(const wchar_t *const filePath, const DWORD mask)
{
	DWORD loop, attribs;

	for (loop = 0U; loop < 31U; ++loop)
	{
		if ((attribs = getAttributes(filePath, NULL)) == INVALID_FILE_ATTRIBUTES)
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
/* CANONICAL FILE NAME                                                     */
/* ======================================================================= */

//GetFinalPathNameByHandleW support
typedef DWORD (WINAPI *PGETFINALPATHNAMEBYHANDLEW)(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags);
static volatile LONG getFinalPathNameInit = 0L;
static PGETFINALPATHNAMEBYHANDLEW getFinalPathNamePtr = NULL;

#define _SAFE_WITHOUT_PREFIX(OFF) \
	((!wcspbrk(buffer + (OFF), L"<>\"/|?*")) && (!wcsstr(buffer + (OFF), L".\\")) && (!wcsstr(buffer + (OFF), L":\\")) && (buffer[length - 1U] != L'.') && (buffer[length - 1U] != L':'))

static __inline void cleanFilePath(wchar_t *const buffer)
{
	//Determine length
	size_t length = wcslen(buffer);
	if (!length)
	{
		return; /*empty string*/
	}

	//Remove ‘\\?\’, ‘\\.\’ or ‘\\?\UNC\’ prefix, iff it's safe
	if ((length > 6U) && ((length - 4U) < MAX_PATH) && ((!wcsncmp(buffer, L"\\\\?\\", 4U)) || (!wcsncmp(buffer, L"\\\\.\\", 4U))) && iswalpha(buffer[4U]) && (!wcsncmp(buffer + 5U, L":\\", 2U)))
	{
		if (_SAFE_WITHOUT_PREFIX(6U))
		{
			wmemmove(buffer, buffer + 4U, length - 3U);
			length -= 4U;
		}
	}
	else if ((length > 8U) && ((length - 6U) < MAX_PATH) && ((!wcsncmp(buffer, L"\\\\?\\UNC\\", 8U)) || (!wcsncmp(buffer, L"\\\\.\\UNC\\", 8U))))
	{
		if (_SAFE_WITHOUT_PREFIX(8U))
		{
			wmemmove(buffer + 2U, buffer + 8U, length - 7U); 
			length -= 6U;
		}
	}

	//Normalize drive letter casing
	if ((length >= 3U) && iswalpha(buffer[0]) && (!wcsncmp(buffer + 1U, L":\\", 2U)))
	{
		buffer[0U] = towupper(buffer[0U]);
	}
	else if ((length > 6U) && ((!wcsncmp(buffer, L"\\\\?\\", 4U)) || (!wcsncmp(buffer, L"\\\\.\\", 4U))) && iswalpha(buffer[4U]) && (!wcsncmp(buffer + 5U, L":\\", 2U)))
	{
		buffer[4U] = towupper(buffer[4U]);
	}

	//Remove trailing backslash characters
	while ((length > 1) && (buffer[length - 1U] == L'\\') && (buffer[length - 2U] != L':'))
	{
		buffer[--length] = L'\0';
	}
}

static const wchar_t* getFinalPathName(const wchar_t *const fileName)
{
	LONG loop = 0L;
	wchar_t *buffer = NULL;
	size_t size = 0UL;
	HANDLE handle = NULL;

	//Initialize on first call
	while ((loop = InterlockedCompareExchange(&getFinalPathNameInit, -1L, 0L)) != 1L)
	{
		if(!loop) /*first thread initializes*/
		{
			getFinalPathNamePtr = (PGETFINALPATHNAMEBYHANDLEW) GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetFinalPathNameByHandleW");
			InterlockedExchange(&getFinalPathNameInit, 1L);
		}
	}

	//Is available?
	if (!getFinalPathNamePtr)
	{
		goto failure; /*GetFinalPathNameByHandleW unavailable!*/
	}

	//Try to open file (or directory)
	handle = CreateFileW(fileName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		goto failure;
	}

	for (loop = 0L; loop < 3L; ++loop)
	{
		//Try to get full path name
		const DWORD result = getFinalPathNamePtr(handle, buffer, (DWORD)size, VOLUME_NAME_DOS);
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

		//Clean up
		CLOSE_HANDLE(handle);

		//Clean the path string
		cleanFilePath(buffer);
		return buffer; /*success*/
	}

failure:
	FREE(buffer);
	CLOSE_HANDLE(handle);
	return NULL;
}

static const wchar_t* getFullPathName(const wchar_t *const fileName)
{
	LONG loop = 0L;
	wchar_t *buffer = NULL;
	size_t size = 0UL;

	for (loop = 0L; loop < 3L; ++loop)
	{
		//Try to get full path name
		const DWORD result = GetFullPathNameW(fileName, (DWORD)size, buffer, NULL);
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

		//Clean the path string
		cleanFilePath(buffer);
		return buffer; /*success*/
	}

failure:
	FREE(buffer);
	return NULL;
}

static const wchar_t* getLongPathName(const wchar_t *const fileName)
{
	LONG loop = 0L;
	wchar_t *buffer = NULL;
	size_t size = 0UL;

	for (loop = 0L; loop < 3L; ++loop)
	{
		//Try to get full path name
		const DWORD result = GetLongPathNameW(fileName, buffer, (DWORD)size);
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

		//Clean the path string
		cleanFilePath(buffer);
		return buffer; /*success*/
	}

failure:
	FREE(buffer);
	return NULL;
}

const wchar_t* getCanonicalPath(const wchar_t *const fileName)
{
	//Try the "modern" way first
	const wchar_t* canonicalPath = getFinalPathName(fileName);
	if (canonicalPath)
	{
		return canonicalPath;
	}

	//Fallback method for "legacy" OS
	if (canonicalPath = getFullPathName(fileName))
	{
		const wchar_t* longFullPath = getLongPathName(canonicalPath);
		if (longFullPath)
		{
			free((void*)canonicalPath);
			canonicalPath = longFullPath;
		}
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
		if (PathRemoveFileSpecW(buffer))
		{
			return buffer;
		}
		free((void*)buffer); /*clean-up*/
	}

	return NULL;
}

/* ======================================================================= */
/* ENVIRONMENT STRING                                                      */
/* ======================================================================= */

const wchar_t* getEnvironmentString(const wchar_t *const name)
{
	const WCHAR *const envstr = _wgetenv(name);
	if(envstr && envstr[0U])
	{
		return _wcsdup(envstr);
	}
	return NULL;
}
