#include "connectionless_classifier_adapter.h"

#include "client_adapter_shared.hpp"
#include "engine/server/client/connectionless_classifier.hpp"

using xash::engine::server::ServerConnectionlessCommand;
using xash::engine::server::adapter::client::FromLegacyBool;
using xash::engine::server::adapter::client::ToLegacyEnum;

static_assert(SV_CONNLESS_IGNORE ==
	static_cast<int>(ServerConnectionlessCommand::Ignore),
	"ServerConnectionlessCommand::Ignore value changed");
static_assert(SV_CONNLESS_SOURCE_QUERY ==
	static_cast<int>(ServerConnectionlessCommand::SourceQuery),
	"ServerConnectionlessCommand::SourceQuery value changed");
static_assert(SV_CONNLESS_NETAPI_INFO ==
	static_cast<int>(ServerConnectionlessCommand::NetApiInfo),
	"ServerConnectionlessCommand::NetApiInfo value changed");
static_assert(SV_CONNLESS_LEGACY_INFO ==
	static_cast<int>(ServerConnectionlessCommand::LegacyInfo),
	"ServerConnectionlessCommand::LegacyInfo value changed");
static_assert(SV_CONNLESS_BANDWIDTH_TEST ==
	static_cast<int>(ServerConnectionlessCommand::BandwidthTest),
	"ServerConnectionlessCommand::BandwidthTest value changed");
static_assert(SV_CONNLESS_CHALLENGE_REQUEST ==
	static_cast<int>(ServerConnectionlessCommand::ChallengeRequest),
	"ServerConnectionlessCommand::ChallengeRequest value changed");
static_assert(SV_CONNLESS_CONNECT ==
	static_cast<int>(ServerConnectionlessCommand::Connect),
	"ServerConnectionlessCommand::Connect value changed");
static_assert(SV_CONNLESS_PING ==
	static_cast<int>(ServerConnectionlessCommand::Ping),
	"ServerConnectionlessCommand::Ping value changed");
static_assert(SV_CONNLESS_GOLDSRC_PING ==
	static_cast<int>(ServerConnectionlessCommand::GoldSrcPing),
	"ServerConnectionlessCommand::GoldSrcPing value changed");
static_assert(SV_CONNLESS_REMOTE_COMMAND ==
	static_cast<int>(ServerConnectionlessCommand::RemoteCommand),
	"ServerConnectionlessCommand::RemoteCommand value changed");
static_assert(SV_CONNLESS_ACKNOWLEDGEMENT ==
	static_cast<int>(ServerConnectionlessCommand::Acknowledgement),
	"ServerConnectionlessCommand::Acknowledgement value changed");
static_assert(SV_CONNLESS_MASTER_CHALLENGE ==
	static_cast<int>(ServerConnectionlessCommand::MasterChallenge),
	"ServerConnectionlessCommand::MasterChallenge value changed");
static_assert(SV_CONNLESS_MASTER_NAT_CONNECT ==
	static_cast<int>(ServerConnectionlessCommand::MasterNatConnect),
	"ServerConnectionlessCommand::MasterNatConnect value changed");
static_assert(SV_CONNLESS_GAME_DLL_PACKET ==
	static_cast<int>(ServerConnectionlessCommand::GameDllPacket),
	"ServerConnectionlessCommand::GameDllPacket value changed");

extern "C" sv_connectionless_command_t SV_ConnectionlessClassifier_Classify(
	const char *full_command_line,
	const char *first_token,
	int server_initialized,
	int from_master_server)
{
	return static_cast<sv_connectionless_command_t>(
		ToLegacyEnum(
			xash::engine::server::ClassifyServerConnectionlessCommand(
				full_command_line,
				first_token,
				FromLegacyBool(server_initialized),
				FromLegacyBool(from_master_server))));
}
