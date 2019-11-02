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
		wprintln(stderr, L"Ctrl+C: Realpath has been interrupted !!!\n");
		break;
	case CTRL_BREAK_EVENT:
		wprintln(stderr, L"Break: Realpath has been interrupted !!!\n");
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
			wprintln(stderr, L"Error: Options are mutually exclusive!\n"); \
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
	INITIALIZE_C_RUNTIME();

	//Check command-line arguments
	if ((argc < 2) || (!_wcsicmp(argv[1U], L"/?")) || (!_wcsicmp(argv[1U], L"--help")))
	{
		fwprintf(stderr, L"realpath %s\n", PROGRAM_VERSION);
		wprintln(stderr, L"Convert file name or relative path into fully qualified \"canonical\" path.\n");
		wprintln(stderr, L"Usage:");
		wprintln(stderr, L"   realpath.exe [options] <filename_1> [<filename_2> ... <filename_N>]\n");
		wprintln(stderr, L"Options:");
		wprintln(stderr, L"   --exists     requires the target file system object to exist");
		wprintln(stderr, L"   --file       requires the target path to point to a regular file");
		wprintln(stderr, L"   --directory  requires the target path to point to a directory\n");
		wprintln(stderr, L"Exit status:");
		wprintln(stderr, L"   0 - Path converted successfully");
		wprintln(stderr, L"   1 - Failed with error");
		wprintln(stderr, L"   2 - Interrupted by user\n");
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
		TRY_PARSE_OPTION(1, exists)
		TRY_PARSE_OPTION(2, file)
		TRY_PARSE_OPTION(3, directory)
		fwprintf(stderr, L"Error: Unknown option \"%s\" encountered!\n\n", argv[argOffset]);
		return EXIT_FAILURE;
	}

	//Check remaining file count
	if (argOffset >= argc)
	{
		wprintln(stderr, L"Error: No file name specified. Nothing to do!\n");
		return EXIT_FAILURE;
	}

	//Process all files
	for (; argOffset < argc; ++argOffset)
	{
		//Convert to absoloute paths
		fullPath = getCanonicalPath(argv[argOffset]);
		if (!fullPath)
		{
			fwprintf(stderr, L"Error: Path \"%s\" could not be resolved!\n\n", argv[argOffset]);
			goto cleanup;
		}

		//Check if file exists
		if(check_mode) 
		{
			if ((attribs = getAttributes(fullPath, NULL)) == INVALID_FILE_ATTRIBUTES) \
			{ 
				fwprintf(stderr, L"Error: File \"%s\" not found or access denied!\n\n", fullPath);
				goto cleanup;
			}
			switch(check_mode) {
			case 2:
				if(attribs & FILE_ATTRIBUTE_DIRECTORY)
				{
					fwprintf(stderr, L"Error: Path \"%s\" points to a directory!\n\n", fullPath); \
					goto cleanup;
				}
				break;
			case 3:
				if (!(attribs & FILE_ATTRIBUTE_DIRECTORY))
				{
					fwprintf(stderr, L"Error: Path \"%s\" points to a regular file!\n\n", fullPath); \
					goto cleanup;
				}
				break;
			}
		}

		//Print the full path to stdout
		_putws(fullPath);
	}

	//Completed
	result = EXIT_SUCCESS;

	//Perform final clean-up
cleanup:
	FREE(fullPath);
	return result; /*exit*/
}
