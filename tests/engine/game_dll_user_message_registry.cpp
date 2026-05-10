#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll_user_message_registry.hpp"

using namespace xash::engine::server;

namespace
{

static GameDllUserMessageSlot Slot(
	const char *name,
	int number,
	int size)
{
	GameDllUserMessageSlot slot = {};
	slot.name = name;
	slot.number = number;
	slot.size = size;
	return slot;
}

static GameDllUserMessageRegistrationRequest Request(
	const GameDllUserMessageSlot *slots,
	int slotCount,
	const char *name,
	int size,
	bool serverActive = false)
{
	GameDllUserMessageRegistrationRequest request = {};
	request.name = name;
	request.requestedSize = size;
	request.slots = slots;
	request.slotCount = slotCount;
	request.nameCapacity = kGameDllUserMessageNameCapacity;
	request.serverActive = serverActive;
	return request;
}

static bool TestRejectsInvalidNames()
{
	GameDllUserMessageSlot slots[4] = {};
	const char tooLong[] = "12345678901234567890123456789012";

	const GameDllUserMessageRegistrationPlan nullName =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, nullptr, 1));
	const GameDllUserMessageRegistrationPlan emptyName =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, "", 1));
	const GameDllUserMessageRegistrationPlan longName =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, tooLong, 1));

	return nullName.action ==
			GameDllUserMessageRegistrationAction::Reject &&
		nullName.rejectReason ==
			GameDllUserMessageRejectReason::EmptyName &&
		emptyName.rejectReason ==
			GameDllUserMessageRejectReason::EmptyName &&
		longName.rejectReason ==
			GameDllUserMessageRejectReason::NameTooLong;
}

static bool TestRejectsOversizedMessage()
{
	GameDllUserMessageSlot slots[4] = {};
	const GameDllUserMessageRegistrationPlan plan =
		BuildGameDllUserMessageRegistrationPlan(
			Request(
				slots,
				4,
				"HudText",
				kGameDllUserMessageMaxPayloadBytes + 1));

	return plan.action == GameDllUserMessageRegistrationAction::Reject &&
		plan.rejectReason == GameDllUserMessageRejectReason::SizeTooLarge &&
		plan.messageNumber == kGameDllUserMessageBadMessage;
}

static bool TestDuplicateReturnsExistingNumber()
{
	GameDllUserMessageSlot slots[4] = {};
	slots[1] = Slot("HudText", 75, -1);
	slots[2] = Slot("CurWeapon", 76, 3);

	const GameDllUserMessageRegistrationPlan plan =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, "CurWeapon", 10, true));

	return plan.action ==
			GameDllUserMessageRegistrationAction::ReturnExisting &&
		plan.slotIndex == 2 &&
		plan.messageNumber == 76 &&
		plan.storedSize == 3 &&
		!plan.resendRegistration;
}

static bool TestRegistersNewFixedAndVariableSizes()
{
	GameDllUserMessageSlot slots[4] = {};
	slots[1] = Slot("HudText", 60, -1);

	const GameDllUserMessageRegistrationPlan fixed =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, "CurWeapon", 3));
	const GameDllUserMessageRegistrationPlan variable =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, "SayText", -1));
	const GameDllUserMessageRegistrationPlan clampedLow =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, "Battery", -50));

	return fixed.action ==
			GameDllUserMessageRegistrationAction::RegisterNew &&
		fixed.slotIndex == 2 &&
		fixed.messageNumber == kGameDllUserMessageLastServiceMessage + 2 &&
		fixed.storedSize == 3 &&
		variable.storedSize == -1 &&
		clampedLow.storedSize == -1;
}

static bool TestCapacityExceeded()
{
	GameDllUserMessageSlot slots[3] = {};
	slots[1] = Slot("One", 60, 1);
	slots[2] = Slot("Two", 61, 1);

	const GameDllUserMessageRegistrationPlan plan =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 3, "Three", 1));

	return plan.action == GameDllUserMessageRegistrationAction::Reject &&
		plan.rejectReason ==
			GameDllUserMessageRejectReason::CapacityExceeded;
}

static bool TestStopsAtFirstEmptySlot()
{
	GameDllUserMessageSlot slots[5] = {};
	slots[1] = Slot("One", 60, 1);
	slots[3] = Slot("HiddenAfterHole", 62, 1);

	const GameDllUserMessageRegistrationPlan plan =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 5, "HiddenAfterHole", 1));

	return plan.action ==
			GameDllUserMessageRegistrationAction::RegisterNew &&
		plan.slotIndex == 2 &&
		plan.messageNumber == kGameDllUserMessageLastServiceMessage + 2;
}

static bool TestActiveServerResendPlanning()
{
	GameDllUserMessageSlot slots[4] = {};
	slots[1] = Slot("HudText", 60, -1);

	const GameDllUserMessageRegistrationPlan inactive =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, "SayText", -1, false));
	const GameDllUserMessageRegistrationPlan active =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, "SayText", -1, true));

	return inactive.action ==
			GameDllUserMessageRegistrationAction::RegisterNew &&
		!inactive.resendRegistration &&
		active.action ==
			GameDllUserMessageRegistrationAction::RegisterNew &&
		active.resendRegistration;
}

static bool TestDisplayNames()
{
	return std::strcmp(
			GameDllUserMessageRegistrationActionName(
				GameDllUserMessageRegistrationAction::RegisterNew),
			"register-new") == 0 &&
		std::strcmp(
			GameDllUserMessageRejectReasonName(
				GameDllUserMessageRejectReason::CapacityExceeded),
			"capacity-exceeded") == 0;
}

}

int main()
{
	if (!TestRejectsInvalidNames() ||
		!TestRejectsOversizedMessage() ||
		!TestDuplicateReturnsExistingNumber() ||
		!TestRegistersNewFixedAndVariableSizes() ||
		!TestCapacityExceeded() ||
		!TestStopsAtFirstEmptySlot() ||
		!TestActiveServerResendPlanning() ||
		!TestDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
