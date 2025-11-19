#include <config.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include <freetds/windows.h>
#include <freetds/macros.h>
#include <freetds/sysdep_private.h>

#ifdef DLL_EXPORT

HINSTANCE hinstFreeTDS;

#if defined(_MSC_VER) && defined(_DEBUG)
HANDLE h_crt_file = _CRTDBG_FILE_STDOUT;
#endif

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);

BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved TDS_UNUSED)
{
	const char* crt_filename = NULL;

	hinstFreeTDS = hinstDLL;
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
#if defined(_MSC_VER) && defined(_DEBUG)
		crt_filename = getenv("TDS_DEBUG_CRT_FILENAME");
		if (crt_filename)
			h_crt_file = CreateFile(crt_filename, GENERIC_WRITE,
				FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL, NULL);

		_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_WARN, h_crt_file);
		_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ERROR, h_crt_file);
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ASSERT, h_crt_file);
		_CrtSetDbgFlag(_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF | _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
#endif

		if (tds_socket_init() != 0)
			return FALSE;

		DisableThreadLibraryCalls(hinstDLL);
		break;

	case DLL_PROCESS_DETACH:
		tds_socket_done();
#if defined(_MSC_VER) && defined(_DEBUG)
		_CrtDumpMemoryLeaks();
		if (h_crt_file != _CRTDBG_FILE_STDOUT)
		{
			CloseHandle(h_crt_file);
			h_crt_file = _CRTDBG_FILE_STDOUT;
		}
#endif
		break;
	}
	return TRUE;
}

#endif

