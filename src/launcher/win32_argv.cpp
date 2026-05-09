#include "launcher/win32_argv.hpp"

#if XASH_WIN32

#include <stdlib.h>
#include <string.h>

#include <shellapi.h>

namespace xash
{
namespace launcher
{

Win32Argv::Win32Argv()
	: m_argc(0),
	  m_argv(0)
{
}

Win32Argv::~Win32Argv()
{
	release();
}

bool Win32Argv::capture()
{
	release();

	LPWSTR *wideArgv = CommandLineToArgvW(GetCommandLineW(), &m_argc);
	if (!wideArgv)
	{
		return false;
	}

	m_argv = (char **)malloc((m_argc + 1) * sizeof(char *));
	if (!m_argv)
	{
		LocalFree(wideArgv);
		m_argc = 0;
		return false;
	}

	for (int i = 0; i < m_argc; ++i)
	{
		size_t size = wcslen(wideArgv[i]) + 1;

		m_argv[i] = (char *)malloc(size * sizeof(wchar_t));
		if (!m_argv[i])
		{
			m_argv[i] = 0;
			LocalFree(wideArgv);
			release();
			return false;
		}

		wcstombs(m_argv[i], wideArgv[i], size);
	}

	m_argv[m_argc] = 0;
	LocalFree(wideArgv);
	return true;
}

int Win32Argv::argc() const
{
	return m_argc;
}

char **Win32Argv::argv() const
{
	return m_argv;
}

void Win32Argv::release()
{
	if (m_argv)
	{
		for (int i = 0; i < m_argc; ++i)
		{
			free(m_argv[i]);
		}

		free(m_argv);
	}

	m_argv = 0;
	m_argc = 0;
}

}
}

#endif
