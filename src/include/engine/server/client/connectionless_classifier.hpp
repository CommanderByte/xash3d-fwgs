#ifndef XASH_ENGINE_SERVER_CONNECTIONLESS_CLASSIFIER_HPP
#define XASH_ENGINE_SERVER_CONNECTIONLESS_CLASSIFIER_HPP

namespace xash
{
namespace engine
{
namespace server
{

enum class ServerConnectionlessCommand
{
	Ignore,
	SourceQuery,
	NetApiInfo,
	LegacyInfo,
	BandwidthTest,
	ChallengeRequest,
	Connect,
	Ping,
	GoldSrcPing,
	RemoteCommand,
	Acknowledgement,
	MasterChallenge,
	MasterNatConnect,
	GameDllPacket,
};

ServerConnectionlessCommand ClassifyServerConnectionlessCommand(
	const char *fullCommandLine,
	const char *firstToken,
	bool serverInitialized,
	bool fromMasterServer);

}
}
}

#endif
