#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	WSADATA wsaData;

	switch(fdwReason) {
	case DLL_PROCESS_ATTACH:
		if (WSAStartup( MAKEWORD( 1, 1 ), &wsaData ) != 0)
			return FALSE;
		break;
	}
	return TRUE;
}