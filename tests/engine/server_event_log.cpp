#include <cstdlib>
#include <cstring>

#include "engine/server/runtime/server_event_log.hpp"

using xash::engine::server::FormatLogFileClosedMessage;
using xash::engine::server::FormatLogFileStartedMessage;
using xash::engine::server::FormatServerCvarMessage;
using xash::engine::server::FormatServerCvarsEndMessage;
using xash::engine::server::FormatServerCvarsStartMessage;
using xash::engine::server::FormatServerLogLine;
using xash::engine::server::ServerLogTimestamp;

static bool Equals(const char *lhs, const char *rhs)
{
	return std::strcmp(lhs, rhs) == 0;
}

static bool TestTimestampedLine()
{
	char out[128] = {};
	const ServerLogTimestamp timestamp = { 5, 10, 2026, 4, 7, 9 };

	return FormatServerLogLine(out, sizeof(out), timestamp, "Server shutdown\n") &&
		Equals(out, "05/10/2026 - 04:07:09: Server shutdown\n");
}

static bool TestServerCvarMessages()
{
	char out[128] = {};

	if (!FormatServerCvarsStartMessage(out, sizeof(out)) ||
		!Equals(out, "Server cvars start\n"))
	{
		return false;
	}

	if (!FormatServerCvarMessage(out, sizeof(out), "sv_gravity", "800") ||
		!Equals(out, "Server cvar \"sv_gravity\" = \"800\"\n"))
	{
		return false;
	}

	return FormatServerCvarsEndMessage(out, sizeof(out)) &&
		Equals(out, "Server cvars end\n");
}

static bool TestLogFileMessages()
{
	char out[192] = {};

	if (!FormatLogFileStartedMessage(
		out,
		sizeof(out),
		"logs/L0510000.log",
		"valve",
		49,
		"0.21",
		4056))
	{
		return false;
	}

	if (!Equals(out, "Log file started (file \"logs/L0510000.log\") (game \"valve\") (version \"49/0.21/4056\")\n"))
		return false;

	return FormatLogFileClosedMessage(out, sizeof(out)) &&
		Equals(out, "Log file closed\n");
}

static bool TestTruncationStillTerminates()
{
	char out[16] = {};
	const ServerLogTimestamp timestamp = { 12, 31, 2026, 23, 59, 58 };

	if (!FormatServerLogLine(out, sizeof(out), timestamp, "very long message\n"))
		return false;

	return out[sizeof(out) - 1] == '\0' &&
		std::strncmp(out, "12/31/2026 - ", 13) == 0;
}

int main()
{
	if (!TestTimestampedLine() ||
		!TestServerCvarMessages() ||
		!TestLogFileMessages() ||
		!TestTruncationStillTerminates())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
