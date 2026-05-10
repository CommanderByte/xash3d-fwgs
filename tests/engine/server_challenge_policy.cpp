#include <cstdlib>
#include <cstdint>
#include <limits>

#include "engine/server/server_challenge_policy.hpp"

using namespace xash::engine::server;

namespace
{

static bool TestBoundaryWindows()
{
	return BuildServerChallengeTimeWindow(0.0) == 0u &&
		BuildServerChallengeTimeWindow(4.999) == 0u &&
		BuildServerChallengeTimeWindow(5.0) == 1u &&
		BuildServerChallengeTimeWindow(9.999) == 1u &&
		BuildServerChallengeTimeWindow(10.0) == 2u &&
		BuildServerChallengeTimeWindow(14.999) == 2u;
}

static bool TestCustomWindowSize()
{
	return BuildServerChallengeTimeWindow(0.0, 2) == 0u &&
		BuildServerChallengeTimeWindow(1.999, 2) == 0u &&
		BuildServerChallengeTimeWindow(2.0, 2) == 1u &&
		BuildServerChallengeTimeWindow(6.0, 2) == 3u;
}

static bool TestInvalidInputsClampToInitialWindow()
{
	return BuildServerChallengeTimeWindow(-1.0) == 0u &&
		BuildServerChallengeTimeWindow(5.0, 0) == 0u &&
		BuildServerChallengeTimeWindow(5.0, -1) == 0u &&
		BuildServerChallengeTimeWindow(
			static_cast<double>(std::numeric_limits<std::uint32_t>::max()) *
				10.0) == std::numeric_limits<std::uint32_t>::max();
}

static bool TestAcceptedWindowPair()
{
	const ServerChallengeWindowPair early =
		BuildServerChallengeAcceptedWindows(4.999);
	const ServerChallengeWindowPair boundary =
		BuildServerChallengeAcceptedWindows(5.0);

	return early.current == 0u &&
		early.previous == UINT32_MAX &&
		boundary.current == 1u &&
		boundary.previous == 0u;
}

static bool TestRepeatWindowAcceptance()
{
	const ServerChallengeWindowPair windows =
		BuildServerChallengeAcceptedWindows(12.0);

	return windows.current == 2u &&
		windows.previous == 1u &&
		ServerChallengeAcceptsWindow(2u, windows) &&
		ServerChallengeAcceptsWindow(1u, windows) &&
		!ServerChallengeAcceptsWindow(0u, windows) &&
		!ServerChallengeAcceptsWindow(3u, windows);
}

}

int main()
{
	if (!TestBoundaryWindows() ||
		!TestCustomWindowSize() ||
		!TestInvalidInputsClampToInitialWindow() ||
		!TestAcceptedWindowPair() ||
		!TestRepeatWindowAcceptance())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
