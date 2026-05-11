#ifndef XASH_ENGINE_SERVER_EVENT_LOG_HPP
#define XASH_ENGINE_SERVER_EVENT_LOG_HPP

#include <cstddef>

namespace xash
{
namespace engine
{
namespace server
{

struct ServerLogTimestamp
{
	int month;
	int day;
	int year;
	int hour;
	int minute;
	int second;
};

bool FormatServerLogLine(
	char *out,
	std::size_t capacity,
	const ServerLogTimestamp &timestamp,
	const char *message);

bool FormatServerCvarMessage(
	char *out,
	std::size_t capacity,
	const char *name,
	const char *value);

bool FormatServerCvarsStartMessage(char *out, std::size_t capacity);
bool FormatServerCvarsEndMessage(char *out, std::size_t capacity);

bool FormatLogFileStartedMessage(
	char *out,
	std::size_t capacity,
	const char *fileName,
	const char *gameFolder,
	int protocolVersion,
	const char *engineVersion,
	int buildNumber);

bool FormatLogFileClosedMessage(char *out, std::size_t capacity);

}
}
}

#endif
