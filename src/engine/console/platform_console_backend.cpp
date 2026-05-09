#include "engine/console/platform_console_backend.hpp"

namespace xash
{
namespace engine
{
namespace console
{

PlatformConsoleCapabilities PlatformConsoleCapabilityMask(PlatformConsoleCapability capability)
{
	return static_cast<PlatformConsoleCapabilities>(capability);
}

bool PlatformConsoleHasCapability(PlatformConsoleCapabilities capabilities, PlatformConsoleCapability capability)
{
	return (capabilities & PlatformConsoleCapabilityMask(capability)) != 0;
}

PlatformConsoleConfig::PlatformConsoleConfig()
	: dedicated(false)
	, showAlways(false)
	, developerLevel(0)
{
}

NullPlatformConsoleBackend::NullPlatformConsoleBackend()
	: m_initialized(false)
	, m_lastConfig()
{
}

PlatformConsoleCapabilities NullPlatformConsoleBackend::capabilities() const
{
	return 0;
}

void NullPlatformConsoleBackend::initialize(const PlatformConsoleConfig &config)
{
	m_lastConfig = config;
	m_initialized = true;
}

void NullPlatformConsoleBackend::shutdown()
{
	m_initialized = false;
}

void NullPlatformConsoleBackend::print(const char *text)
{
	(void)text;
}

const char *NullPlatformConsoleBackend::readCommand()
{
	return nullptr;
}

void NullPlatformConsoleBackend::show(bool visible)
{
	(void)visible;
}

void NullPlatformConsoleBackend::disableInput()
{
}

void NullPlatformConsoleBackend::setStatus(const char *text)
{
	(void)text;
}

void NullPlatformConsoleBackend::registerCommands()
{
}

bool NullPlatformConsoleBackend::initialized() const
{
	return m_initialized;
}

const PlatformConsoleConfig &NullPlatformConsoleBackend::lastConfig() const
{
	return m_lastConfig;
}

PosixPlatformConsoleBackend::PosixPlatformConsoleBackend(IPosixConsoleIo &io,
	PlatformConsoleCapabilities capabilities)
	: m_io(io)
	, m_capabilities(capabilities)
	, m_initialized(false)
	, m_inputEnabled(false)
	, m_lastConfig()
{
}

PlatformConsoleCapabilities PosixPlatformConsoleBackend::capabilities() const
{
	return m_capabilities;
}

void PosixPlatformConsoleBackend::initialize(const PlatformConsoleConfig &config)
{
	m_lastConfig = config;
	m_initialized = true;
	m_inputEnabled = true;
}

void PosixPlatformConsoleBackend::shutdown()
{
	m_initialized = false;
	m_inputEnabled = false;
}

void PosixPlatformConsoleBackend::print(const char *text)
{
	if (!m_initialized || !text ||
		!PlatformConsoleHasCapability(m_capabilities, PlatformConsoleCapability::Output))
	{
		return;
	}

	m_io.writeOutput(text);
}

const char *PosixPlatformConsoleBackend::readCommand()
{
	if (!m_initialized || !m_inputEnabled || !m_lastConfig.dedicated ||
		!PlatformConsoleHasCapability(m_capabilities, PlatformConsoleCapability::Input))
	{
		return nullptr;
	}

	return m_io.readCommand();
}

void PosixPlatformConsoleBackend::show(bool visible)
{
	(void)visible;
}

void PosixPlatformConsoleBackend::disableInput()
{
	m_inputEnabled = false;
}

void PosixPlatformConsoleBackend::setStatus(const char *text)
{
	(void)text;
}

void PosixPlatformConsoleBackend::registerCommands()
{
}

bool PosixPlatformConsoleBackend::initialized() const
{
	return m_initialized;
}

bool PosixPlatformConsoleBackend::inputEnabled() const
{
	return m_inputEnabled;
}

const PlatformConsoleConfig &PosixPlatformConsoleBackend::lastConfig() const
{
	return m_lastConfig;
}

}
}
}
