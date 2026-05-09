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

}
}
}
