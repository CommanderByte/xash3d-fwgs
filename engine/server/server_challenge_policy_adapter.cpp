#include "server_challenge_policy_adapter.h"

#include "engine/server/client/server_challenge_policy.hpp"

extern "C" unsigned int SV_ChallengePolicy_TimeWindow(double realtime_seconds)
{
	return xash::engine::server::BuildServerChallengeTimeWindow(
		realtime_seconds);
}

extern "C" unsigned int SV_ChallengePolicy_PreviousTimeWindow(
	unsigned int current_window)
{
	return xash::engine::server::BuildServerPreviousChallengeTimeWindow(
		current_window);
}
