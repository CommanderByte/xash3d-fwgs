#include <cstdlib>
#include <cstring>

#include "engine/server/remote_admin_command.hpp"

using namespace xash::engine::server;

namespace
{

bool TestAuthIgnoresDisabledOrEmptyPassword()
{
	return BuildRemoteAdminAuthAction(false, "secret", "secret") ==
			RemoteAdminAuthAction::IgnoreDisabled &&
		BuildRemoteAdminAuthAction(true, "", "secret") ==
			RemoteAdminAuthAction::IgnoreDisabled &&
		BuildRemoteAdminAuthAction(true, nullptr, "secret") ==
			RemoteAdminAuthAction::IgnoreDisabled;
}

bool TestAuthRejectsBadPassword()
{
	return BuildRemoteAdminAuthAction(true, "secret", "wrong") ==
			RemoteAdminAuthAction::RejectBadPassword &&
		BuildRemoteAdminAuthAction(true, "secret", nullptr) ==
			RemoteAdminAuthAction::RejectBadPassword &&
		BuildRemoteAdminAuthAction(true, "secret", "") ==
			RemoteAdminAuthAction::RejectBadPassword;
}

bool TestAuthAcceptsMatchingPassword()
{
	return BuildRemoteAdminAuthAction(true, "secret", "secret") ==
		RemoteAdminAuthAction::Accept;
}

bool TestCommandBuilderQuotesArgumentsWithTrailingSpaces()
{
	char buffer[128];
	std::size_t used = ResetRemoteAdminCommand(buffer, sizeof(buffer));

	used = AppendRemoteAdminCommandArgument(
		buffer,
		sizeof(buffer),
		used,
		"sv_cheats");
	used = AppendRemoteAdminCommandArgument(
		buffer,
		sizeof(buffer),
		used,
		"1");

	return used == std::strlen("\"sv_cheats\" \"1\" ") &&
		std::strcmp(buffer, "\"sv_cheats\" \"1\" ") == 0;
}

bool TestCommandBuilderPreservesEmptyArgumentShape()
{
	char buffer[32];
	std::size_t used = ResetRemoteAdminCommand(buffer, sizeof(buffer));

	used = AppendRemoteAdminCommandArgument(
		buffer,
		sizeof(buffer),
		used,
		"");

	return used == 3 && std::strcmp(buffer, "\"\" ") == 0;
}

bool TestCommandBuilderTruncatesSafely()
{
	char buffer[8];
	std::size_t used = ResetRemoteAdminCommand(buffer, sizeof(buffer));

	used = AppendRemoteAdminCommandArgument(
		buffer,
		sizeof(buffer),
		used,
		"abcdefghi");
	used = AppendRemoteAdminCommandArgument(
		buffer,
		sizeof(buffer),
		used,
		"second");

	return used == 7 &&
		buffer[sizeof(buffer) - 1] == '\0' &&
		std::strcmp(buffer, "\"abcdef") == 0;
}

bool TestCommandBuilderHandlesMissingBuffer()
{
	std::size_t used = ResetRemoteAdminCommand(nullptr, 0);
	used = AppendRemoteAdminCommandArgument(nullptr, 0, used, "ignored");
	return used == 0;
}

}

int main()
{
	if (!TestAuthIgnoresDisabledOrEmptyPassword() ||
		!TestAuthRejectsBadPassword() ||
		!TestAuthAcceptsMatchingPassword() ||
		!TestCommandBuilderQuotesArgumentsWithTrailingSpaces() ||
		!TestCommandBuilderPreservesEmptyArgumentShape() ||
		!TestCommandBuilderTruncatesSafely() ||
		!TestCommandBuilderHandlesMissingBuffer())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
