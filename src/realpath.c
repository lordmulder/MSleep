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

#define TRY_PARSE_OPTION(VAL, NAME) \
	if (!_wcsicmp(argv[argOffset] + 2U, L#NAME)) \
	{ \
		if(check_mode && (check_mode != (VAL))) \
		{ \
			fputws(L"Error: Options are mutually exclusive!\n", stderr); \
			goto cleanup; \
		} \
		check_mode = (VAL); \
		continue; \
	}

/* ======================================================================= */
/* MAIN                                                                    */
/* ======================================================================= */

int wmain(int argc, wchar_t *argv[])
{
	int result = EXIT_FAILURE, argOffset = 1;
	const wchar_t *fullPath = NULL;
	DWORD attribs = 0UL, check_mode = 0UL;

	//Initialize
	SetErrorMode(SetErrorMode(0x0003) | 0x0003);
	setlocale(LC_ALL, "C");
	SetConsoleCtrlHandler(crtlHandler, TRUE);
	_setmode(_fileno(stdout), _O_U8TEXT);
	_setmode(_fileno(stderr), _O_U8TEXT);

	//Check command-line arguments
	if (argc < 2)
	{
		fputws(L"realpath [" TEXT(__DATE__) L"]\n", stderr);
		fputws(L"Convert file name or relative path into fully qualified \"canonical\" path.\n\n", stderr);
		fputws(L"Usage:\n", stderr);
		fputws(L"   realpath.exe [options] <filename>\n\n", stderr);
		fputws(L"Options:\n", stderr);
		fputws(L"   --exists     check whether the target file system object exists\n", stderr);
		fputws(L"   --file       check whether the path points to a file\n", stderr);
		fputws(L"   --directory  check whether the path points to a directory\n", stderr);
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
		TRY_PARSE_OPTION(1, exists)
		TRY_PARSE_OPTION(2, file)
		TRY_PARSE_OPTION(3, directory)
		fwprintf(stderr, L"Error: Unknown option \"%s\" encountered!\n", argv[argOffset]);
		goto cleanup;
	}

	//Check remaining file count
	if (argOffset >= argc)
	{
		fputws(L"Error: No file name specified. Nothing to do!\n", stderr);
		goto cleanup;
	}
	if ((argc - argOffset) > 1)
	{
		fputws(L"Error: Found excess command-line argument!\n", stderr);
		goto cleanup;
	}

	//Convert to absoloute paths
	fullPath = getCanonicalPath(argv[argOffset]);
	if (!fullPath)
	{
		fwprintf(stderr, L"Error: Path \"%s\" could not be resolved!\n", argv[argOffset]);
		goto cleanup;
	}

	//Check if file exists
	if(check_mode) 
	{
		if ((attribs = getAttributes(fullPath, NULL)) == INVALID_FILE_ATTRIBUTES) \
		{ 
			fwprintf(stderr, L"Error: File \"%s\" not found or access denied!\n", fullPath);
			goto cleanup;
		}
		switch(check_mode) {
		case 2:
			if(attribs & FILE_ATTRIBUTE_DIRECTORY)
			{
				fwprintf(stderr, L"Error: Path \"%s\" points to a directory!\n", fullPath); \
				goto cleanup;
			}
			break;
		case 3:
			if (!(attribs & FILE_ATTRIBUTE_DIRECTORY))
			{
				fwprintf(stderr, L"Error: Path \"%s\" points to a regular file!\n", fullPath); \
				goto cleanup;
			}
			break;
		}
	}

	//Print full path
	fwprintf(stdout, L"%s\n", fullPath);
	result = EXIT_SUCCESS;

	//Perform final clean-up
cleanup:
	FREE(fullPath);
	return result; /*exit*/
}
