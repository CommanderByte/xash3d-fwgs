#include "engine/server/server_challenge_policy.hpp"

#include <limits>

namespace xash
{
namespace engine
{
namespace server
{

std::uint32_t BuildServerChallengeTimeWindow(
	double realtimeSeconds,
	int windowSeconds)
{
	if (realtimeSeconds <= 0.0 || windowSeconds <= 0)
		return 0;

	const double window =
		realtimeSeconds / static_cast<double>(windowSeconds);
	const double maxWindow =
		static_cast<double>(std::numeric_limits<std::uint32_t>::max());

	if (window >= maxWindow)
		return std::numeric_limits<std::uint32_t>::max();

	return static_cast<std::uint32_t>(window);
}

std::uint32_t BuildServerPreviousChallengeTimeWindow(
	std::uint32_t currentWindow)
{
	return currentWindow - 1u;
}

ServerChallengeWindowPair BuildServerChallengeAcceptedWindows(
	double realtimeSeconds,
	int windowSeconds)
{
	ServerChallengeWindowPair windows = {};
	windows.current = BuildServerChallengeTimeWindow(
		realtimeSeconds,
		windowSeconds);
	windows.previous = BuildServerPreviousChallengeTimeWindow(windows.current);
	return windows;
}

bool ServerChallengeAcceptsWindow(
	std::uint32_t issuedWindow,
	const ServerChallengeWindowPair &acceptedWindows)
{
	return issuedWindow == acceptedWindows.current ||
		issuedWindow == acceptedWindows.previous;
}

}
}
}
