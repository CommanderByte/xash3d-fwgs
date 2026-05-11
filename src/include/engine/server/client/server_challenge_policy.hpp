#ifndef XASH_ENGINE_SERVER_SERVER_CHALLENGE_POLICY_HPP
#define XASH_ENGINE_SERVER_SERVER_CHALLENGE_POLICY_HPP

#include "engine/server/shared/server_limits.hpp"

#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

struct ServerChallengeWindowPair
{
	std::uint32_t current;
	std::uint32_t previous;
};

std::uint32_t BuildServerChallengeTimeWindow(
	double realtimeSeconds,
	int windowSeconds = kServerChallengeWindowSeconds);
std::uint32_t BuildServerPreviousChallengeTimeWindow(
	std::uint32_t currentWindow);
ServerChallengeWindowPair BuildServerChallengeAcceptedWindows(
	double realtimeSeconds,
	int windowSeconds = kServerChallengeWindowSeconds);
bool ServerChallengeAcceptsWindow(
	std::uint32_t issuedWindow,
	const ServerChallengeWindowPair &acceptedWindows);

}
}
}

#endif
