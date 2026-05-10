#include "connectionless_classifier_adapter.h"

#include "engine/server/connectionless_classifier.hpp"

namespace
{

sv_connectionless_command_t ToLegacyCommand(
	xash::engine::server::ServerConnectionlessCommand command)
{
	using xash::engine::server::ServerConnectionlessCommand;

	switch (command)
	{
	case ServerConnectionlessCommand::Ignore:
		return SV_CONNLESS_IGNORE;
	case ServerConnectionlessCommand::SourceQuery:
		return SV_CONNLESS_SOURCE_QUERY;
	case ServerConnectionlessCommand::NetApiInfo:
		return SV_CONNLESS_NETAPI_INFO;
	case ServerConnectionlessCommand::LegacyInfo:
		return SV_CONNLESS_LEGACY_INFO;
	case ServerConnectionlessCommand::BandwidthTest:
		return SV_CONNLESS_BANDWIDTH_TEST;
	case ServerConnectionlessCommand::ChallengeRequest:
		return SV_CONNLESS_CHALLENGE_REQUEST;
	case ServerConnectionlessCommand::Connect:
		return SV_CONNLESS_CONNECT;
	case ServerConnectionlessCommand::Ping:
		return SV_CONNLESS_PING;
	case ServerConnectionlessCommand::GoldSrcPing:
		return SV_CONNLESS_GOLDSRC_PING;
	case ServerConnectionlessCommand::RemoteCommand:
		return SV_CONNLESS_REMOTE_COMMAND;
	case ServerConnectionlessCommand::Acknowledgement:
		return SV_CONNLESS_ACKNOWLEDGEMENT;
	case ServerConnectionlessCommand::MasterChallenge:
		return SV_CONNLESS_MASTER_CHALLENGE;
	case ServerConnectionlessCommand::MasterNatConnect:
		return SV_CONNLESS_MASTER_NAT_CONNECT;
	case ServerConnectionlessCommand::GameDllPacket:
		return SV_CONNLESS_GAME_DLL_PACKET;
	}

	return SV_CONNLESS_GAME_DLL_PACKET;
}

}

extern "C" sv_connectionless_command_t SV_ConnectionlessClassifier_Classify(
	const char *full_command_line,
	const char *first_token,
	int server_initialized,
	int from_master_server)
{
	return ToLegacyCommand(xash::engine::server::ClassifyServerConnectionlessCommand(
		full_command_line,
		first_token,
		server_initialized != 0,
		from_master_server != 0));
}
