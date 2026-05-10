#ifndef XASH_ENGINE_SERVER_CONNECTIONLESS_CLASSIFIER_ADAPTER_H
#define XASH_ENGINE_SERVER_CONNECTIONLESS_CLASSIFIER_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sv_connectionless_command_e
{
	SV_CONNLESS_IGNORE = 0,
	SV_CONNLESS_SOURCE_QUERY,
	SV_CONNLESS_NETAPI_INFO,
	SV_CONNLESS_LEGACY_INFO,
	SV_CONNLESS_BANDWIDTH_TEST,
	SV_CONNLESS_CHALLENGE_REQUEST,
	SV_CONNLESS_CONNECT,
	SV_CONNLESS_PING,
	SV_CONNLESS_GOLDSRC_PING,
	SV_CONNLESS_REMOTE_COMMAND,
	SV_CONNLESS_ACKNOWLEDGEMENT,
	SV_CONNLESS_MASTER_CHALLENGE,
	SV_CONNLESS_MASTER_NAT_CONNECT,
	SV_CONNLESS_GAME_DLL_PACKET,
} sv_connectionless_command_t;

sv_connectionless_command_t SV_ConnectionlessClassifier_Classify(
	const char *full_command_line,
	const char *first_token,
	int server_initialized,
	int from_master_server);

#ifdef __cplusplus
}
#endif

#endif
