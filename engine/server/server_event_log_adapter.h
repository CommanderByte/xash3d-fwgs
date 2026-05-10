#ifndef XASH_ENGINE_SERVER_EVENT_LOG_ADAPTER_H
#define XASH_ENGINE_SERVER_EVENT_LOG_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_server_log_timestamp_s
{
	int month;
	int day;
	int year;
	int hour;
	int minute;
	int second;
} sv_server_log_timestamp_t;

int SV_ServerEventLog_FormatLine(
	char *out,
	unsigned int capacity,
	const sv_server_log_timestamp_t *timestamp,
	const char *message);

int SV_ServerEventLog_FormatServerCvarMessage(
	char *out,
	unsigned int capacity,
	const char *name,
	const char *value);

int SV_ServerEventLog_FormatServerCvarsStartMessage(char *out, unsigned int capacity);
int SV_ServerEventLog_FormatServerCvarsEndMessage(char *out, unsigned int capacity);

int SV_ServerEventLog_FormatLogFileStartedMessage(
	char *out,
	unsigned int capacity,
	const char *file_name,
	const char *game_folder,
	int protocol_version,
	const char *engine_version,
	int build_number);

int SV_ServerEventLog_FormatLogFileClosedMessage(char *out, unsigned int capacity);

#ifdef __cplusplus
}
#endif

#endif
