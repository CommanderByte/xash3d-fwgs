#include "engine/server/client/remote_admin_command.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool Empty(const char *text)
{
	return !text || text[0] == '\0';
}

const char *SafeText(const char *text)
{
	return text ? text : "";
}

std::size_t AppendText(
	char *buffer,
	std::size_t size,
	std::size_t used,
	const char *text)
{
	if (!buffer || size == 0)
		return used;

	if (used >= size)
	{
		buffer[size - 1] = '\0';
		return used;
	}

	const char *safe = SafeText(text);
	const std::size_t available = size - used;
	std::size_t copy = std::strlen(safe);

	if (copy >= available)
		copy = available - 1;

	if (copy != 0)
		std::memcpy(buffer + used, safe, copy);

	used += copy;
	buffer[used] = '\0';
	return used;
}

}

RemoteAdminAuthAction BuildRemoteAdminAuthAction(
	bool enabled,
	const char *configuredPassword,
	const char *suppliedPassword)
{
	if (!enabled || Empty(configuredPassword))
		return RemoteAdminAuthAction::IgnoreDisabled;

	if (Empty(suppliedPassword) ||
		std::strcmp(suppliedPassword, configuredPassword) != 0)
	{
		return RemoteAdminAuthAction::RejectBadPassword;
	}

	return RemoteAdminAuthAction::Accept;
}

std::size_t ResetRemoteAdminCommand(char *buffer, std::size_t size)
{
	if (buffer && size != 0)
		buffer[0] = '\0';

	return 0;
}

std::size_t AppendRemoteAdminCommandArgument(
	char *buffer,
	std::size_t size,
	std::size_t used,
	const char *argument)
{
	used = AppendText(buffer, size, used, "\"");
	used = AppendText(buffer, size, used, argument);
	used = AppendText(buffer, size, used, "\" ");
	return used;
}

}
}
}
