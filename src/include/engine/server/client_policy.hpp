#ifndef XASH_ENGINE_SERVER_CLIENT_POLICY_HPP
#define XASH_ENGINE_SERVER_CLIENT_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

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
