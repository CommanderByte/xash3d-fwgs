#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/game_dll_message_bridge.hpp"
#include "engine/server/server_multicast_policy.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

namespace
{

GameDllUserMessageSlot Slot(
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

GameDllUserMessageRegistrationRequest Request(
	const GameDllUserMessageSlot *slots,
	int slotCount,
	const char *name,
	int size,
	bool serverActive)
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

bool ReadCString(NetworkBitBuffer &reader, const char *expected)
{
	for (std::size_t i = 0; ; ++i)
	{
		const unsigned int value = reader.readUnsigned(8);
		if (value != static_cast<unsigned char>(expected[i]))
			return false;

		if (value == 0)
			return true;
	}
}

bool TestRegisteredFixedUserMessageMulticasts()
{
	GameDllUserMessageSlot slots[4] = {};
	slots[1] = Slot("HudText", kGameDllUserMessageLastServiceMessage + 1, 3);

	const GameDllUserMessageRegistrationPlan plan =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, "HudText", 3, false));

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

bool TestRegisteredVariableUserMessagePatchesSize()
{
	GameDllUserMessageSlot slots[4] = {};
	slots[1] = Slot("HudText", kGameDllUserMessageLastServiceMessage + 1, -1);

	const GameDllUserMessageRegistrationPlan plan =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, "SayText", -1, false));

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

bool TestActiveRegistrationBuildsResendPayload()
{
	GameDllUserMessageSlot slots[4] = {};
	const GameDllUserMessageRegistrationPlan plan =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 4, "SayText", -1, true));
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
		reader.readUnsigned(8) == static_cast<unsigned int>(plan.messageNumber) &&
		reader.readUnsigned(16) ==
			static_cast<unsigned int>(
				static_cast<unsigned short>(plan.storedSize)) &&
		ReadCString(reader, "SayText") &&
		!writer.overflow() &&
		!reader.overflow();
}

bool TestInactiveOrRejectedRegistrationDoesNotBroadcast()
{
	GameDllUserMessageSlot slots[2] = {};
	const GameDllUserMessageRegistrationPlan inactive =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 2, "HudText", 1, false));
	const GameDllUserMessageRegistrationPlan rejected =
		BuildGameDllUserMessageRegistrationPlan(
			Request(slots, 2, nullptr, 1, true));

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

bool TestRegisteredSizeMismatchClearsBuffer()
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

bool TestRewrittenSystemMessageKeepsLegacyCommand()
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
	if (!TestRegisteredFixedUserMessageMulticasts() ||
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
