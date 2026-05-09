#ifndef XASH_ENGINE_CONSOLE_PLATFORM_CONSOLE_BACKEND_HPP
#define XASH_ENGINE_CONSOLE_PLATFORM_CONSOLE_BACKEND_HPP

namespace xash
{
namespace engine
{
namespace console
{

typedef unsigned PlatformConsoleCapabilities;

enum class PlatformConsoleCapability : unsigned
{
	Output = 1u << 0,
	Input = 1u << 1,
	Visibility = 1u << 2,
	StatusLine = 1u << 3,
	CommandRegistration = 1u << 4,
};

PlatformConsoleCapabilities PlatformConsoleCapabilityMask(PlatformConsoleCapability capability);
bool PlatformConsoleHasCapability(PlatformConsoleCapabilities capabilities, PlatformConsoleCapability capability);

struct PlatformConsoleConfig
{
	PlatformConsoleConfig();

	bool dedicated;
	bool showAlways;
	int developerLevel;
};

class IPlatformConsoleBackend
{
public:
	virtual ~IPlatformConsoleBackend() = default;

	virtual PlatformConsoleCapabilities capabilities() const = 0;
	virtual void initialize(const PlatformConsoleConfig &config) = 0;
	virtual void shutdown() = 0;
	virtual void print(const char *text) = 0;
	virtual const char *readCommand() = 0;
	virtual void show(bool visible) = 0;
	virtual void disableInput() = 0;
	virtual void setStatus(const char *text) = 0;
	virtual void registerCommands() = 0;
};

class NullPlatformConsoleBackend : public IPlatformConsoleBackend
{
public:
	NullPlatformConsoleBackend();

	PlatformConsoleCapabilities capabilities() const override;
	void initialize(const PlatformConsoleConfig &config) override;
	void shutdown() override;
	void print(const char *text) override;
	const char *readCommand() override;
	void show(bool visible) override;
	void disableInput() override;
	void setStatus(const char *text) override;
	void registerCommands() override;

	bool initialized() const;
	const PlatformConsoleConfig &lastConfig() const;

private:
	bool m_initialized;
	PlatformConsoleConfig m_lastConfig;
};

class IPosixConsoleIo
{
public:
	virtual ~IPosixConsoleIo() = default;

	virtual void writeOutput(const char *text) = 0;
	virtual const char *readCommand() = 0;
};

class PosixPlatformConsoleBackend : public IPlatformConsoleBackend
{
public:
	explicit PosixPlatformConsoleBackend(IPosixConsoleIo &io,
		PlatformConsoleCapabilities capabilities =
			PlatformConsoleCapabilityMask(PlatformConsoleCapability::Output) |
			PlatformConsoleCapabilityMask(PlatformConsoleCapability::Input));

	PlatformConsoleCapabilities capabilities() const override;
	void initialize(const PlatformConsoleConfig &config) override;
	void shutdown() override;
	void print(const char *text) override;
	const char *readCommand() override;
	void show(bool visible) override;
	void disableInput() override;
	void setStatus(const char *text) override;
	void registerCommands() override;

	bool initialized() const;
	bool inputEnabled() const;
	const PlatformConsoleConfig &lastConfig() const;

private:
	IPosixConsoleIo &m_io;
	PlatformConsoleCapabilities m_capabilities;
	bool m_initialized;
	bool m_inputEnabled;
	PlatformConsoleConfig m_lastConfig;
};

}
}
}

#endif
