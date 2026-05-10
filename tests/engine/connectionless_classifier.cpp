#include <cstdlib>

#include "engine/server/connectionless_classifier.hpp"

using xash::engine::server::ClassifyServerConnectionlessCommand;
using xash::engine::server::ServerConnectionlessCommand;

static bool Expect(
	const char *fullCommandLine,
	const char *firstToken,
	bool serverInitialized,
	bool fromMasterServer,
	ServerConnectionlessCommand expected)
{
	return ClassifyServerConnectionlessCommand(
		fullCommandLine,
		firstToken,
		serverInitialized,
		fromMasterServer) == expected;
}

static bool TestUninitializedServerOnlyAcceptsRcon()
{
	return Expect("rcon secret status", "rcon", false, false, ServerConnectionlessCommand::RemoteCommand) &&
		Expect("connect 49 1", "connect", false, false, ServerConnectionlessCommand::Ignore) &&
		Expect("s", "s", false, true, ServerConnectionlessCommand::Ignore);
}

static bool TestMasterServerCommandsPreemptPublicCommands()
{
	return Expect("s", "s", true, true, ServerConnectionlessCommand::MasterChallenge) &&
		Expect("c", "c", true, true, ServerConnectionlessCommand::MasterNatConnect) &&
		Expect("ping", "ping", true, true, ServerConnectionlessCommand::Ignore);
}

static bool TestGoldSrcSourceQueryQuirks()
{
	return Expect(
		"TSource Engine Query",
		"TSource",
		true,
		false,
		ServerConnectionlessCommand::SourceQuery) &&
		Expect("TSource Engine Query with suffix", "TSource", true, false, ServerConnectionlessCommand::GameDllPacket) &&
		Expect("U", "U", true, false, ServerConnectionlessCommand::SourceQuery) &&
		Expect("V", "V", true, false, ServerConnectionlessCommand::SourceQuery) &&
		Expect("Unrelated", "Unrelated", true, false, ServerConnectionlessCommand::SourceQuery) &&
		Expect("Verbose", "Verbose", true, false, ServerConnectionlessCommand::SourceQuery);
}

static bool TestPublicServerCommands()
{
	return Expect("netinfo 7 1 49", "netinfo", true, false, ServerConnectionlessCommand::NetApiInfo) &&
		Expect("info 49", "info", true, false, ServerConnectionlessCommand::LegacyInfo) &&
		Expect("bandwidth", "bandwidth", true, false, ServerConnectionlessCommand::BandwidthTest) &&
		Expect("getchallenge", "getchallenge", true, false, ServerConnectionlessCommand::ChallengeRequest) &&
		Expect("connect", "connect", true, false, ServerConnectionlessCommand::Connect) &&
		Expect("rcon secret status", "rcon", true, false, ServerConnectionlessCommand::RemoteCommand);
}

static bool TestPingAndAckAliases()
{
	return Expect("ping", "ping", true, false, ServerConnectionlessCommand::Ping) &&
		Expect("i", "i", true, false, ServerConnectionlessCommand::GoldSrcPing) &&
		Expect("ack", "ack", true, false, ServerConnectionlessCommand::Acknowledgement) &&
		Expect("j", "j", true, false, ServerConnectionlessCommand::Acknowledgement);
}

static bool TestUnknownFallsBackToGameDll()
{
	return Expect("custom payload", "custom", true, false, ServerConnectionlessCommand::GameDllPacket) &&
		Expect("", "", true, false, ServerConnectionlessCommand::GameDllPacket) &&
		Expect(nullptr, nullptr, true, false, ServerConnectionlessCommand::GameDllPacket);
}

int main()
{
	if (!TestUninitializedServerOnlyAcceptsRcon() ||
		!TestMasterServerCommandsPreemptPublicCommands() ||
		!TestGoldSrcSourceQueryQuirks() ||
		!TestPublicServerCommands() ||
		!TestPingAndAckAliases() ||
		!TestUnknownFallsBackToGameDll())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
