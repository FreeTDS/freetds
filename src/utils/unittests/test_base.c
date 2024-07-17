#include <freetds/utils/test_base.h>

#ifdef _WIN32
#  include <crtdbg.h>
#  include <windows.h>

static LONG CALLBACK seh_handler(EXCEPTION_POINTERS* ep)
{
	/* Always terminate the test. */
	return EXCEPTION_EXECUTE_HANDLER;
}

static void suppress_diag_popup_messages(void)
{
	/* Check environment variable for silent abort app at error */
	const char* value = getenv("DIAG_SILENT_ABORT");
	if (value  &&  (*value == 'Y'  ||  *value == 'y')) {
		/* Windows GPF errors */
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
			     SEM_NOOPENFILEERRORBOX);

		/* Runtime library */
		_set_error_mode(_OUT_TO_STDERR);

		/* Debug library */
		_CrtSetReportFile(_CRT_WARN,   _CRTDBG_FILE_STDERR);
		_CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDERR);
		_CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);

		/* Exceptions(!) */
		SetUnhandledExceptionFilter(seh_handler);
	}
}
#endif

int main(int argc, char ** argv)
{
#ifdef _WIN32
	suppress_diag_popup_messages();
#endif
	return test_main(argc, argv);
}
