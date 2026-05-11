#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/game_dll/game_dll_message_bridge.hpp"
#include "engine/server/game_dll/game_dll_user_message_registry.hpp"
#include "engine/server/messaging/server_multicast_policy.hpp"
#include "game_dll_user_message_test_support.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;
using xash::tests::engine::ReadCString;
using xash::tests::engine::RegistrationRequest;
using xash::tests::engine::Slot;

namespace
{

static bool TestRejectsInvalidNames()
{
	GameDllUserMessageSlot slots[4] = {};
	const char tooLong[] = "12345678901234567890123456789012";

	const GameDllUserMessageRegistrationPlan nullName =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 4, nullptr, 1));
	const GameDllUserMessageRegistrationPlan emptyName =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 4, "", 1));
	const GameDllUserMessageRegistrationPlan longName =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 4, tooLong, 1));

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
			RegistrationRequest(
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
			RegistrationRequest(slots, 4, "CurWeapon", 10, true));

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
			RegistrationRequest(slots, 4, "CurWeapon", 3));
	const GameDllUserMessageRegistrationPlan variable =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 4, "SayText", -1));
	const GameDllUserMessageRegistrationPlan clampedLow =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 4, "Battery", -50));

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
			RegistrationRequest(slots, 3, "Three", 1));

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
			RegistrationRequest(slots, 5, "HiddenAfterHole", 1));

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
			RegistrationRequest(slots, 4, "SayText", -1, false));
	const GameDllUserMessageRegistrationPlan active =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 4, "SayText", -1, true));

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

static bool TestRegisteredFixedUserMessageMulticasts()
{
	GameDllUserMessageSlot slots[4] = {};
	slots[1] = Slot("HudText", kGameDllUserMessageLastServiceMessage + 1, 3);

	const GameDllUserMessageRegistrationPlan plan =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 4, "HudText", 3, false));

	if (plan.action != GameDllUserMessageRegistrationAction::ReturnExisting)
		return false;

	unsigned char data[16] = {};
	GameDllMessageSession session(data, sizeof(data));

	if (session.begin(BuildGameDllUserMessageBeginRequest(
		kMulticastDestinationAll,
		slots[plan.slotIndex])) != GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	if (session.writeByte(0x12) != GameDllMessageSessionStatus::Ok ||
		session.writeShort(0x3456) != GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	const GameDllMessageEndResult end = session.end({ false, true });
	const MulticastDestinationPlan multicast =
		BuildMulticastDestinationPlan({
			end.boundedDestination,
			false,
			false
		});

	return end.status == GameDllMessageSessionStatus::Ok &&
		end.shouldMulticast &&
		end.payloadSize == 3 &&
		data[0] == slots[1].number &&
		data[1] == 0x12 &&
		data[2] == 0x56 &&
		data[3] == 0x34 &&
		multicast.action == MulticastDestinationAction::SendToClients &&
		multicast.reliable &&
		multicast.clearMulticast;
}

static bool TestRegisteredVariableUserMessagePatchesSize()
{
	GameDllUserMessageSlot slots[4] = {};
	slots[1] = Slot("HudText", kGameDllUserMessageLastServiceMessage + 1, -1);

	const GameDllUserMessageRegistrationPlan plan =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 4, "SayText", -1, false));

	if (plan.action != GameDllUserMessageRegistrationAction::RegisterNew)
		return false;

	const GameDllUserMessageSlot newSlot =
		Slot("SayText", plan.messageNumber, plan.storedSize);

	unsigned char data[16] = {};
	GameDllMessageSession session(data, sizeof(data));

	if (session.begin(BuildGameDllUserMessageBeginRequest(
		kMulticastDestinationBroadcast,
		newSlot)) != GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	if (session.writeString("hi") != GameDllMessageSessionStatus::Ok)
		return false;

	const GameDllMessageEndResult end = session.end({ false, true });
	return end.status == GameDllMessageSessionStatus::Ok &&
		end.shouldPatchPayloadSize &&
		end.payloadSizePatchOffset == 1 &&
		end.payloadSize == 3 &&
		data[0] == plan.messageNumber &&
		data[1] == 3 &&
		data[2] == 0 &&
		data[3] == 'h' &&
		data[4] == 'i' &&
		data[5] == 0;
}

static bool TestActiveRegistrationBuildsResendPayload()
{
	GameDllUserMessageSlot slots[4] = {};
	const GameDllUserMessageRegistrationPlan plan =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 4, "SayText", -1, true));
	const GameDllUserMessageRegistrationBroadcast broadcast =
		BuildGameDllUserMessageRegistrationBroadcast(plan, "SayText");

	unsigned char data[32] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteGameDllUserMessageRegistrationBroadcast(writer, broadcast);

	NetworkBitBuffer reader(data, writer.tellBit());

	return plan.action == GameDllUserMessageRegistrationAction::RegisterNew &&
		plan.resendRegistration &&
		broadcast.shouldWrite &&
		reader.readUnsigned(8) == kGameDllUserMessageRegistrationCommand &&
		reader.readUnsigned(8) ==
			static_cast<unsigned int>(plan.messageNumber) &&
		reader.readUnsigned(16) ==
			static_cast<unsigned int>(
				static_cast<unsigned short>(plan.storedSize)) &&
		ReadCString(reader, "SayText") &&
		!writer.overflow() &&
		!reader.overflow();
}

static bool TestInactiveOrRejectedRegistrationDoesNotBroadcast()
{
	GameDllUserMessageSlot slots[2] = {};
	const GameDllUserMessageRegistrationPlan inactive =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 2, "HudText", 1, false));
	const GameDllUserMessageRegistrationPlan rejected =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 2, nullptr, 1, true));

	const GameDllUserMessageRegistrationBroadcast inactiveBroadcast =
		BuildGameDllUserMessageRegistrationBroadcast(inactive, "HudText");
	const GameDllUserMessageRegistrationBroadcast rejectedBroadcast =
		BuildGameDllUserMessageRegistrationBroadcast(rejected, nullptr);

	unsigned char data[8] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteGameDllUserMessageRegistrationBroadcast(writer, inactiveBroadcast);
	WriteGameDllUserMessageRegistrationBroadcast(writer, rejectedBroadcast);

	return inactive.action == GameDllUserMessageRegistrationAction::RegisterNew &&
		!inactive.resendRegistration &&
		!inactiveBroadcast.shouldWrite &&
		rejected.action == GameDllUserMessageRegistrationAction::Reject &&
		!rejectedBroadcast.shouldWrite &&
		writer.tellBit() == 0 &&
		!writer.overflow();
}

static bool TestRegisteredSizeMismatchClearsBuffer()
{
	const GameDllUserMessageSlot fixedSlot =
		Slot("CurWeapon", kGameDllUserMessageLastServiceMessage + 2, 2);

	unsigned char data[16] = {};
	GameDllMessageSession session(data, sizeof(data));

	if (session.begin(BuildGameDllUserMessageBeginRequest(
		kMulticastDestinationAll,
		fixedSlot)) != GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	if (session.writeByte(0x7f) != GameDllMessageSessionStatus::Ok)
		return false;

	const GameDllMessageEndResult end = session.end({ false, true });
	return end.status == GameDllMessageSessionStatus::FixedSizeMismatch &&
		end.shouldClearBuffer &&
		session.cursorBytes() == 0 &&
		!end.shouldMulticast;
}

static bool TestRewrittenSystemMessageKeepsLegacyCommand()
{
	unsigned char data[16] = {};
	GameDllMessageSession session(data, sizeof(data));

	GameDllMessageBeginRequest request = {};
	request.destination = kMulticastDestinationAll;
	request.messageNumber = kGameDllMessageGoldSrcSpawnStaticSound;
	request.name = "svc_goldsrc_spawnstaticsound";
	request.payloadSize = 0;
	request.systemMessage = true;
	request.rewriteEnabled = true;
	request.rewriteTargetMessage =
		RewriteGameDllMessageTarget(true, request.messageNumber);
	request.rewriteName = "svc_goldsrc_spawnstaticsound";

	if (session.begin(request) != GameDllMessageSessionStatus::Ok)
		return false;

	const GameDllMessageEndResult end = session.end({ false, true });

	return data[0] == kGameDllMessageGoldSrcSpawnStaticSound &&
		session.rewriteOriginalMessage() ==
			kGameDllMessageGoldSrcSpawnStaticSound &&
		session.effectiveMessageIndex() == -kGameDllMessageSvcSound &&
		end.status == GameDllMessageSessionStatus::Ok &&
		end.rewriteAttempted &&
		end.shouldMulticast;
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
		!TestDisplayNames() ||
		!TestRegisteredFixedUserMessageMulticasts() ||
		!TestRegisteredVariableUserMessagePatchesSize() ||
		!TestActiveRegistrationBuildsResendPayload() ||
		!TestInactiveOrRejectedRegistrationDoesNotBroadcast() ||
		!TestRegisteredSizeMismatchClearsBuffer() ||
		!TestRewrittenSystemMessageKeepsLegacyCommand())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
