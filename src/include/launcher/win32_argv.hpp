#ifndef XASH_LAUNCHER_WIN32_ARGV_HPP
#define XASH_LAUNCHER_WIN32_ARGV_HPP

#include "port.h"

#if XASH_WIN32

namespace xash
{
namespace launcher
{

class Win32Argv
{
public:
	Win32Argv();
	~Win32Argv();

	bool capture();

	int argc() const;
	char **argv() const;

private:
	Win32Argv(const Win32Argv &);
	Win32Argv &operator=(const Win32Argv &);

	void release();

	int m_argc;
	char **m_argv;
};

}
}

#endif

#endif
