#include "game_dll_message_session_adapter.h"

#include "const.h"
#include "engine/server/game_dll_message_session.hpp"
#include "protocol.h"

static_assert(MSG_BROADCAST == xash::engine::server::kGameDllMessageDestinationBroadcast,
	"MSG_BROADCAST value changed");
static_assert(MSG_SPEC == xash::engine::server::kGameDllMessageDestinationSpectator,
	"MSG_SPEC value changed");
static_assert(svc_bad == xash::engine::server::kGameDllMessageSvcBad,
	"svc_bad value changed");
static_assert(svc_sound == xash::engine::server::kGameDllMessageSvcSound,
	"svc_sound value changed");
static_assert(svc_temp_entity == xash::engine::server::kGameDllMessageSvcTempEntity,
	"svc_temp_entity value changed");
static_assert(svc_finale == xash::engine::server::kGameDllMessageSvcFinale,
	"svc_finale value changed");
static_assert(svc_cutscene == xash::engine::server::kGameDllMessageSvcCutscene,
	"svc_cutscene value changed");
static_assert(svc_lastmsg == xash::engine::server::kGameDllMessageSvcLast,
	"svc_lastmsg value changed");
static_assert(svc_goldsrc_spawnstaticsound ==
	xash::engine::server::kGameDllMessageGoldSrcSpawnStaticSound,
	"svc_goldsrc_spawnstaticsound value changed");

namespace
{

xash::engine::server::GameDllMessageWriteKind ConvertWriteKind(int kind)
{
	using xash::engine::server::GameDllMessageWriteKind;

	switch (kind)
	{
	case SV_GAMEDLL_MESSAGE_WRITE_BYTE:
		return GameDllMessageWriteKind::Byte;
	case SV_GAMEDLL_MESSAGE_WRITE_CHAR:
		return GameDllMessageWriteKind::Char;
	case SV_GAMEDLL_MESSAGE_WRITE_SHORT:
		return GameDllMessageWriteKind::Short;
	case SV_GAMEDLL_MESSAGE_WRITE_LONG:
		return GameDllMessageWriteKind::Long;
	case SV_GAMEDLL_MESSAGE_WRITE_ANGLE:
		return GameDllMessageWriteKind::Angle;
	case SV_GAMEDLL_MESSAGE_WRITE_COORD:
		return GameDllMessageWriteKind::Coord;
	case SV_GAMEDLL_MESSAGE_WRITE_ENTITY:
		return GameDllMessageWriteKind::Entity;
	default:
		return GameDllMessageWriteKind::Byte;
	}
}

}

extern "C" int SV_GameDllMessageSession_NormalizeByte(int value)
{
	return xash::engine::server::NormalizeGameDllMessageByte(value);
}

extern "C" int SV_GameDllMessageSession_FixedWritePayloadBytes(int kind)
{
	return xash::engine::server::GameDllMessageWritePayloadBytes(
		ConvertWriteKind(kind));
}

extern "C" int SV_GameDllMessageSession_StringPayloadBytes(const char *text)
{
	return static_cast<int>(
		xash::engine::server::GameDllMessageStringPayloadBytes(text));
}

extern "C" int SV_GameDllMessageSession_IsEntityIndexValid(
	int entity_index,
	int entity_count)
{
	return xash::engine::server::IsGameDllMessageEntityIndexValid(
		entity_index,
		entity_count) ? 1 : 0;
}

extern "C" int SV_GameDllMessageSession_BoundDestination(int destination)
{
	return xash::engine::server::BoundGameDllMessageDestination(destination);
}

extern "C" int SV_GameDllMessageSession_CanRewriteMessage(
	int rewrite_enabled,
	int message_number)
{
	return xash::engine::server::RewriteGameDllMessageTarget(
		rewrite_enabled != 0,
		message_number);
}
