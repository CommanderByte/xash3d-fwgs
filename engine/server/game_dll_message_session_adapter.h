#ifndef XASH_ENGINE_SERVER_GAME_DLL_MESSAGE_SESSION_ADAPTER_H
#define XASH_ENGINE_SERVER_GAME_DLL_MESSAGE_SESSION_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	SV_GAMEDLL_MESSAGE_WRITE_BYTE = 0,
	SV_GAMEDLL_MESSAGE_WRITE_CHAR = 1,
	SV_GAMEDLL_MESSAGE_WRITE_SHORT = 2,
	SV_GAMEDLL_MESSAGE_WRITE_LONG = 3,
	SV_GAMEDLL_MESSAGE_WRITE_ANGLE = 4,
	SV_GAMEDLL_MESSAGE_WRITE_COORD = 5,
	SV_GAMEDLL_MESSAGE_WRITE_ENTITY = 6
};

int SV_GameDllMessageSession_NormalizeByte(int value);
int SV_GameDllMessageSession_FixedWritePayloadBytes(int kind);
int SV_GameDllMessageSession_StringPayloadBytes(const char *text);
int SV_GameDllMessageSession_IsEntityIndexValid(
	int entity_index,
	int entity_count);
int SV_GameDllMessageSession_BoundDestination(int destination);
int SV_GameDllMessageSession_CanRewriteMessage(
	int rewrite_enabled,
	int message_number);

#ifdef __cplusplus
}
#endif

#endif
