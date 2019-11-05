/*
 * notifywait for Win32
 * Created by LoRd_MuldeR <mulder2@gmx.de>.
 * 
 * This work is licensed under the CC0 1.0 Universal License.
 * To view a copy of the license, visit:
 * https://creativecommons.org/publicdomain/zero/1.0/legalcode
 */

#include <stdlib.h>

#ifdef ENABLE_CUSTOM_ENTRYPOINT

#define _UNKNOWN_APP 0
#define _CONSOLE_APP 1
#define _GUI_APP     2

typedef struct
{
	int newmode;
}
_startupinfo;

/* CRT imports */
__declspec(dllimport) void __set_app_type(int at);
__declspec(dllimport) void _fpreset(void);
__declspec(dllimport) int __wgetmainargs(int *_Argc, wchar_t ***_Argv, wchar_t ***_Env, int _DoWildCard, _startupinfo * _StartInfo);

/* Kernel32 imports */
__declspec(dllimport) void __stdcall ExitProcess(unsigned int uExitCode);

/* main function */
int wmain(int argc, wchar_t *argv[]);

/* application entry point */
void _startup(void)
{
	int argc = 0, retval = -1;
	wchar_t **argv = NULL, **env = NULL;
	_startupinfo startupinfo;
	
	__set_app_type(_CONSOLE_APP);
	_fpreset();

	startupinfo.newmode = 0;
	if(__wgetmainargs(&argc,&argv, &env, 1, &startupinfo))
	{
		abort(); /*__wgetmainargs failed!*/
	}

	retval = wmain(argc, argv);
	ExitProcess(retval);
}

#endif //ENABLE_CUSTOM_ENTRYPOINT
