#include "engine/server/server_event_log.hpp"

#include <cstdarg>
#include <cstdio>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

const char *SafeString(const char *value)
{
	if (!value)
		return "";

	return value;
}

bool Format(char *out, std::size_t capacity, const char *format, ...)
{
	if (!out || capacity == 0)
		return false;

	va_list args;
	va_start(args, format);
	const int written = std::vsnprintf(out, capacity, format, args);
	va_end(args);

	out[capacity - 1] = '\0';
	return written >= 0;
}

}

bool FormatServerLogLine(
	char *out,
	std::size_t capacity,
	const ServerLogTimestamp &timestamp,
	const char *message)
{
	return Format(
		out,
		capacity,
		"%02i/%02i/%04i - %02i:%02i:%02i: %s",
		timestamp.month,
		timestamp.day,
		timestamp.year,
		timestamp.hour,
		timestamp.minute,
		timestamp.second,
		SafeString(message));
}

bool FormatServerCvarMessage(
	char *out,
	std::size_t capacity,
	const char *name,
	const char *value)
{
	return Format(
		out,
		capacity,
		"Server cvar \"%s\" = \"%s\"\n",
		SafeString(name),
		SafeString(value));
}

bool FormatServerCvarsStartMessage(char *out, std::size_t capacity)
{
	return Format(out, capacity, "Server cvars start\n");
}

bool FormatServerCvarsEndMessage(char *out, std::size_t capacity)
{
	return Format(out, capacity, "Server cvars end\n");
}

bool FormatLogFileStartedMessage(
	char *out,
	std::size_t capacity,
	const char *fileName,
	const char *gameFolder,
	int protocolVersion,
	const char *engineVersion,
	int buildNumber)
{
	return Format(
		out,
		capacity,
		"Log file started (file \"%s\") (game \"%s\") (version \"%i/%s/%d\")\n",
		SafeString(fileName),
		SafeString(gameFolder),
		protocolVersion,
		SafeString(engineVersion),
		buildNumber);
}

bool FormatLogFileClosedMessage(char *out, std::size_t capacity)
{
	return Format(out, capacity, "Log file closed\n");
}

}
}
}
