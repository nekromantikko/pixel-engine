#include "debug.h"
#include <windows.h>

namespace Debug
{
	void Log(const char* msg)
	{
		OutputDebugStringA(msg);
	}
}