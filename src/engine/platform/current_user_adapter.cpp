#include "engine/platform/current_user.hpp"
#include "engine/platform/current_user_adapter.h"

#if XASH_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

extern "C" const char *Xash_GetCurrentUserName(void)
{
#if XASH_WIN32
	static const int kMaxLegacyUserName = 256;
	static const int kMaxUtf8UserName = kMaxLegacyUserName * 4;
	static char userName[kMaxUtf8UserName];
	wchar_t wideUserName[kMaxLegacyUserName];
	DWORD size = kMaxLegacyUserName;

	userName[0] = '\0';

	if (GetUserNameW(wideUserName, &size) && wideUserName[0] != 0)
	{
		WideCharToMultiByte(
			CP_UTF8,
			0,
			wideUserName,
			-1,
			userName,
			static_cast<int>(sizeof(userName)),
			nullptr,
			nullptr);
	}

	return xash::engine::platform::SelectCurrentUserName(userName);
#else
	return xash::engine::platform::DefaultCurrentUserName();
#endif
}
