#include "game_dll_message_session_adapter.h"
#include "game_dll_user_message_registry_adapter.h"

#include "common.h"
#include "const.h"
#include "engine/server/game_dll_message_session.hpp"
#include "engine/server/game_dll_user_message_registry.hpp"
#include "protocol.h"

static_assert(MSG_BROADCAST == xash::engine::server::kGameDllMessageDestinationBroadcast,
	"MSG_BROADCAST value changed");
static_assert(MSG_SPEC == xash::engine::server::kGameDllMessageDestinationSpectator,
	"MSG_SPEC value changed");
static_assert(svc_bad == xash::engine::server::kGameDllMessageSvcBad,
	"svc_bad value changed");
static_assert(svc_bad == xash::engine::server::kGameDllUserMessageBadMessage,
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
static_assert(svc_lastmsg ==
	xash::engine::server::kGameDllUserMessageLastServiceMessage,
	"svc_lastmsg value changed");
static_assert(svc_goldsrc_spawnstaticsound ==
	xash::engine::server::kGameDllMessageGoldSrcSpawnStaticSound,
	"svc_goldsrc_spawnstaticsound value changed");
static_assert(MAX_USER_MESSAGES ==
	xash::engine::server::kGameDllUserMessageMaxMessages,
	"MAX_USER_MESSAGES value changed");
static_assert(MAX_USERMSG_LENGTH ==
	xash::engine::server::kGameDllUserMessageMaxPayloadBytes,
	"MAX_USERMSG_LENGTH value changed");

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

extern "C" sv_gamedll_user_message_registration_plan_t
SV_GameDllUserMessage_BuildRegistrationPlan(
	const sv_gamedll_user_message_slot_t *slots,
	int slot_count,
	const char *name,
	int requested_size,
	int name_capacity,
	int server_active)
{
	xash::engine::server::GameDllUserMessageSlot modernSlots[
		MAX_USER_MESSAGES];
	int normalizedSlotCount = slot_count;

	if (normalizedSlotCount < 0)
		normalizedSlotCount = 0;
	else if (normalizedSlotCount > MAX_USER_MESSAGES)
		normalizedSlotCount = MAX_USER_MESSAGES;

	for (int i = 0; i < normalizedSlotCount; ++i)
	{
		modernSlots[i].name = slots ? slots[i].name : nullptr;
		modernSlots[i].number = slots ? slots[i].number : 0;
		modernSlots[i].size = slots ? slots[i].size : 0;
	}

	xash::engine::server::GameDllUserMessageRegistrationRequest request = {};
	request.name = name;
	request.requestedSize = requested_size;
	request.slots = modernSlots;
	request.slotCount = normalizedSlotCount;
	request.nameCapacity = name_capacity;
	request.serverActive = server_active != 0;

	const xash::engine::server::GameDllUserMessageRegistrationPlan modern =
		xash::engine::server::BuildGameDllUserMessageRegistrationPlan(
			request);

	sv_gamedll_user_message_registration_plan_t legacy = {};
	legacy.action = static_cast<int>(modern.action);
	legacy.reject_reason = static_cast<int>(modern.rejectReason);
	legacy.slot_index = modern.slotIndex;
	legacy.message_number = modern.messageNumber;
	legacy.stored_size = modern.storedSize;
	legacy.resend_registration = modern.resendRegistration ? 1 : 0;
	return legacy;
}
