#ifndef XASH_LAUNCHER_WIN32_ARGV_HPP
#define XASH_LAUNCHER_WIN32_ARGV_HPP

namespace xash
{
namespace launcher
{
namespace platform
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
}

#endif
