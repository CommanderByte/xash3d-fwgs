#include "remote_admin_command_adapter.h"

#include "client_adapter_shared.hpp"
#include "engine/server/client/remote_admin_command.hpp"

using xash::engine::server::adapter::client::FromLegacyBool;
using xash::engine::server::adapter::client::ToLegacyEnum;

static_assert(SV_REMOTE_ADMIN_AUTH_IGNORE_DISABLED ==
	static_cast<int>(xash::engine::server::RemoteAdminAuthAction::IgnoreDisabled),
	"remote admin ignore action changed");
static_assert(SV_REMOTE_ADMIN_AUTH_REJECT_BAD_PASSWORD ==
	static_cast<int>(xash::engine::server::RemoteAdminAuthAction::RejectBadPassword),
	"remote admin reject action changed");
static_assert(SV_REMOTE_ADMIN_AUTH_ACCEPT ==
	static_cast<int>(xash::engine::server::RemoteAdminAuthAction::Accept),
	"remote admin accept action changed");

extern "C" int SV_RemoteAdmin_BuildAuthAction(
	int enabled,
	const char *configured_password,
	const char *supplied_password)
{
	return ToLegacyEnum(
		xash::engine::server::BuildRemoteAdminAuthAction(
			FromLegacyBool(enabled),
			configured_password,
			supplied_password));
}

extern "C" size_t SV_RemoteAdmin_ResetCommand(char *buffer, size_t size)
{
	return xash::engine::server::ResetRemoteAdminCommand(buffer, size);
}

extern "C" size_t SV_RemoteAdmin_AppendCommandArgument(
	char *buffer,
	size_t size,
	size_t used,
	const char *argument)
{
	return xash::engine::server::AppendRemoteAdminCommandArgument(
		buffer,
		size,
		used,
		argument);
}
