#include <config.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include <freetds/windows.h>
#include <freetds/macros.h>
#include <freetds/sysdep_private.h>
#include <freetds/utils/path.h>

#ifdef DLL_EXPORT

HINSTANCE hinstFreeTDS;

#if defined(_MSC_VER) && defined(_DEBUG)
static HANDLE crt_file = _CRTDBG_FILE_STDOUT;
#endif

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);

BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved TDS_UNUSED)
{
#if defined(_MSC_VER) && defined(_DEBUG)
	const tds_dir_char *crt_filename;
#endif

	hinstFreeTDS = hinstDLL;
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
#if defined(_MSC_VER) && defined(_DEBUG)
		crt_filename = tds_dir_getenv(TDS_DIR("TDS_DEBUG_CRT_FILENAME"));
		if (crt_filename) {
			HANDLE file =
				CreateFileW(crt_filename, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, CREATE_ALWAYS,
					    FILE_ATTRIBUTE_NORMAL, NULL);

			if (file != INVALID_HANDLE_VALUE)
				crt_file = file;
		}

		_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_WARN, crt_file);
		_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ERROR, crt_file);
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ASSERT, crt_file);
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
		if (crt_file != _CRTDBG_FILE_STDOUT) {
			CloseHandle(crt_file);
			crt_file = _CRTDBG_FILE_STDOUT;
		}
#endif
		break;
	}
	return TRUE;
}

#endif
