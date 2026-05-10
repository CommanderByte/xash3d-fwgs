#include "game_dll_user_message_registry_adapter.h"

#include "common.h"
#include "engine/server/game_dll_user_message_registry.hpp"
#include "protocol.h"

static_assert(svc_bad == xash::engine::server::kGameDllUserMessageBadMessage,
	"svc_bad value changed");
static_assert(svc_lastmsg ==
	xash::engine::server::kGameDllUserMessageLastServiceMessage,
	"svc_lastmsg value changed");
static_assert(MAX_USER_MESSAGES ==
	xash::engine::server::kGameDllUserMessageMaxMessages,
	"MAX_USER_MESSAGES value changed");
static_assert(MAX_USERMSG_LENGTH ==
	xash::engine::server::kGameDllUserMessageMaxPayloadBytes,
	"MAX_USERMSG_LENGTH value changed");

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
