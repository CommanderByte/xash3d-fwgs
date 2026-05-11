#ifndef XASH_ENGINE_SERVER_REMOTE_ADMIN_COMMAND_HPP
#define XASH_ENGINE_SERVER_REMOTE_ADMIN_COMMAND_HPP

#include <cstddef>

namespace xash
{
namespace engine
{
namespace server
{

enum class RemoteAdminAuthAction
{
	IgnoreDisabled = 0,
	RejectBadPassword = 1,
	Accept = 2,
};

RemoteAdminAuthAction BuildRemoteAdminAuthAction(
	bool enabled,
	const char *configuredPassword,
	const char *suppliedPassword);

std::size_t ResetRemoteAdminCommand(char *buffer, std::size_t size);

std::size_t AppendRemoteAdminCommandArgument(
	char *buffer,
	std::size_t size,
	std::size_t used,
	const char *argument);

}
}
}

#endif
