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

typedef struct
{
	int newmode;
}
_startupinfo;

int __declspec(dllimport) __cdecl __wgetmainargs
(
	int *_Argc,
	wchar_t ***_Argv,
	wchar_t ***_Env,
	int _DoWildCard,
	_startupinfo * _StartInfo
);

int wmain
(
	int argc,
	wchar_t *argv[]
);

int my_startup()
{
	int argc = 0;
	wchar_t **argv = NULL, **env = NULL;
	_startupinfo startupinfo = { 0 };
	
	if(__wgetmainargs(&argc,&argv, &env, 1, &startupinfo))
	{
		abort(); /*__wgetmainargs has failed!*/
	}

	return wmain(argc, argv);
}

#endif //ENABLE_VC6_WORKAROUNDS
