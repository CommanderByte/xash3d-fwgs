#include "engine/server/connectionless_classifier.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

constexpr const char *kA2aPing = "ping";
constexpr const char *kA2aAck = "ack";
constexpr const char *kA2aInfo = "info";
constexpr const char *kA2aNetInfo = "netinfo";
constexpr const char *kA2aGoldSrcPing = "i";
constexpr const char *kA2aGoldSrcAck = "j";
constexpr const char *kA2sGoldSrcInfo = "TSource Engine Query";
constexpr char kA2sGoldSrcRules = 'V';
constexpr char kA2sGoldSrcPlayers = 'U';
constexpr const char *kM2sChallenge = "s";
constexpr const char *kM2sNatConnect = "c";
constexpr const char *kC2sBandwidthTest = "bandwidth";
constexpr const char *kC2sGetChallenge = "getchallenge";
constexpr const char *kC2sConnect = "connect";
constexpr const char *kC2sRcon = "rcon";

const char *SafeString(const char *value)
{
	if (!value)
		return "";

	return value;
}

bool Equals(const char *lhs, const char *rhs)
{
	return std::strcmp(SafeString(lhs), rhs) == 0;
}

bool IsGoldSrcSourceQuery(const char *fullCommandLine, const char *firstToken)
{
	const char *token = SafeString(firstToken);

	return Equals(fullCommandLine, kA2sGoldSrcInfo) ||
		token[0] == kA2sGoldSrcPlayers ||
		token[0] == kA2sGoldSrcRules;
}

}

ServerConnectionlessCommand ClassifyServerConnectionlessCommand(
	const char *fullCommandLine,
	const char *firstToken,
	bool serverInitialized,
	bool fromMasterServer)
{
	if (!serverInitialized)
	{
		if (Equals(firstToken, kC2sRcon))
			return ServerConnectionlessCommand::RemoteCommand;

		return ServerConnectionlessCommand::Ignore;
	}

	if (fromMasterServer)
	{
		if (Equals(firstToken, kM2sChallenge))
			return ServerConnectionlessCommand::MasterChallenge;

		if (Equals(firstToken, kM2sNatConnect))
			return ServerConnectionlessCommand::MasterNatConnect;

		return ServerConnectionlessCommand::Ignore;
	}

	if (IsGoldSrcSourceQuery(fullCommandLine, firstToken))
		return ServerConnectionlessCommand::SourceQuery;

	if (Equals(firstToken, kA2aNetInfo))
		return ServerConnectionlessCommand::NetApiInfo;

	if (Equals(firstToken, kA2aInfo))
		return ServerConnectionlessCommand::LegacyInfo;

	if (Equals(firstToken, kC2sBandwidthTest))
		return ServerConnectionlessCommand::BandwidthTest;

	if (Equals(firstToken, kC2sGetChallenge))
		return ServerConnectionlessCommand::ChallengeRequest;

	if (Equals(firstToken, kC2sConnect))
		return ServerConnectionlessCommand::Connect;

	if (Equals(firstToken, kA2aPing))
		return ServerConnectionlessCommand::Ping;

	if (Equals(firstToken, kA2aGoldSrcPing))
		return ServerConnectionlessCommand::GoldSrcPing;

	if (Equals(firstToken, kC2sRcon))
		return ServerConnectionlessCommand::RemoteCommand;

	if (Equals(firstToken, kA2aAck) || Equals(firstToken, kA2aGoldSrcAck))
		return ServerConnectionlessCommand::Acknowledgement;

	return ServerConnectionlessCommand::GameDllPacket;
}

}
}
}
