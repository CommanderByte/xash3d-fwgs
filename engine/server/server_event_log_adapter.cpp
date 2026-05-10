#include "server_event_log_adapter.h"

#include "engine/server/server_event_log.hpp"

namespace
{

int ToLegacyBool(bool value)
{
	return value ? 1 : 0;
}

xash::engine::server::ServerLogTimestamp ToModernTimestamp(
	const sv_server_log_timestamp_t &timestamp)
{
	xash::engine::server::ServerLogTimestamp modern = {};
	modern.month = timestamp.month;
	modern.day = timestamp.day;
	modern.year = timestamp.year;
	modern.hour = timestamp.hour;
	modern.minute = timestamp.minute;
	modern.second = timestamp.second;
	return modern;
}

}

extern "C" int SV_ServerEventLog_FormatLine(
	char *out,
	unsigned int capacity,
	const sv_server_log_timestamp_t *timestamp,
	const char *message)
{
	if (!timestamp)
		return 0;

	return ToLegacyBool(xash::engine::server::FormatServerLogLine(
		out,
		capacity,
		ToModernTimestamp(*timestamp),
		message));
}

extern "C" int SV_ServerEventLog_FormatServerCvarMessage(
	char *out,
	unsigned int capacity,
	const char *name,
	const char *value)
{
	return ToLegacyBool(xash::engine::server::FormatServerCvarMessage(
		out,
		capacity,
		name,
		value));
}

extern "C" int SV_ServerEventLog_FormatServerCvarsStartMessage(
	char *out,
	unsigned int capacity)
{
	return ToLegacyBool(xash::engine::server::FormatServerCvarsStartMessage(out, capacity));
}

extern "C" int SV_ServerEventLog_FormatServerCvarsEndMessage(
	char *out,
	unsigned int capacity)
{
	return ToLegacyBool(xash::engine::server::FormatServerCvarsEndMessage(out, capacity));
}

extern "C" int SV_ServerEventLog_FormatLogFileStartedMessage(
	char *out,
	unsigned int capacity,
	const char *file_name,
	const char *game_folder,
	int protocol_version,
	const char *engine_version,
	int build_number)
{
	return ToLegacyBool(xash::engine::server::FormatLogFileStartedMessage(
		out,
		capacity,
		file_name,
		game_folder,
		protocol_version,
		engine_version,
		build_number));
}

extern "C" int SV_ServerEventLog_FormatLogFileClosedMessage(
	char *out,
	unsigned int capacity)
{
	return ToLegacyBool(xash::engine::server::FormatLogFileClosedMessage(out, capacity));
}
