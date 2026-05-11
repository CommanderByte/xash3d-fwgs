#ifndef XASH_ENGINE_SERVER_CLIENT_POLICY_HPP
#define XASH_ENGINE_SERVER_CLIENT_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

struct ClientFlagSnapshot
{
	bool resendUserinfo;
	bool resendMovevars;
	bool skipNetMessage;
	bool sendNetMessage;
	bool predictMovement;
	bool localWeapons;
	bool lagCompensation;
	bool fakeClient;
	bool hltvProxy;
	bool sendResources;
	bool forceUnmodified;
};

struct UserinfoPenaltyInput
{
	bool penaltyEnabled;
	bool fakeClient;
	bool singlePlayer;
	double realtime;
	double nextChangeTime;
	double penalty;
	double basePenalty;
	double penaltyMultiplier;
	int changeAttempts;
	int maxAttempts;
};

struct UserinfoPenaltyPlan
{
	bool allowUpdate;
	bool reportIgnoredUpdate;
	bool reportPenaltyChanged;
	double nextChangeTime;
	double penalty;
	int changeAttempts;
};

struct ClientUserinfoFlagPlan
{
	bool predictMovement;
	bool lagCompensation;
	bool localWeapons;
};

ClientFlagSnapshot BuildClientFlagSnapshot(unsigned int flags);
bool ClientIsFakeClient(unsigned int flags);
bool ClientIsHltvProxy(unsigned int flags);
bool ClientUsesLocalWeapons(unsigned int flags);
bool ClientPredictsMovement(unsigned int flags);
bool ClientUsesLagCompensation(unsigned int flags);
bool ClientShouldAppearInHumanQueries(unsigned int flags);

UserinfoPenaltyPlan BuildUserinfoPenaltyPlan(
	const UserinfoPenaltyInput &input);

double ResolveRequestedClientRate(
	int requestedRate,
	double defaultRate,
	double minimumRate,
	double maximumRate);

double ResolveRequestedUpdateInterval(
	int requestedUpdateRate,
	int defaultUpdateRate);

double ApplyUpdateIntervalLimits(
	double interval,
	double maximumUpdateRate,
	double minimumUpdateRate);

double ApplyLegacyServerRateLimits(
	double rate,
	double maximumRate,
	double minimumRate);

ClientUserinfoFlagPlan BuildClientUserinfoFlagPlan(
	int noPrediction,
	int lagCompensation,
	int localWeapons);

}
}
}

#endif
