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

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);

BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved TDS_UNUSED)
{
	hinstFreeTDS = hinstDLL;
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
#if defined(_MSC_VER) && defined(_DEBUG)
		_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
		_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
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
#endif
		break;
	}
	return TRUE;
}

#endif

