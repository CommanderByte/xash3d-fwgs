#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/game_dll/game_dll_changelevel_policy.hpp"
#include "engine/server/game_dll/game_dll_client_info_policy.hpp"
#include "engine/server/game_dll/game_dll_enginefuncs.hpp"
#include "engine/server/game_dll/game_dll_entity_lifecycle.hpp"
#include "engine/server/game_dll/game_dll_entity_parse.hpp"
#include "engine/server/game_dll/game_dll_load_policy.hpp"
#include "engine/server/game_dll/game_dll_message_bridge.hpp"
#include "engine/server/game_dll/game_dll_movement_policy.hpp"
#include "engine/server/game_dll/game_dll_output_policy.hpp"
#include "engine/server/game_dll/game_dll_payload_policy.hpp"
#include "engine/server/game_dll/game_dll_resource_policy.hpp"
#include "engine/server/game_dll/game_dll_string_pool_compat.hpp"
#include "engine/server/game_dll/game_dll_visibility_trace_policy.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

namespace
{

bool NearlyEqualDouble(double a, double b, double tolerance = 0.00001)
{
	return std::fabs(a - b) <= tolerance;
}

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

GameDllUserMessageRegistrationRequest RegistrationRequest(
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

bool TestAbiMetadataFeedsMessageRegistrationFlow()
{
	const EnginefuncSlotMetadata *messageBegin =
		FindEnginefuncSlot("pfnMessageBegin");
	const EnginefuncSlotMetadata *messageWrite =
		FindEnginefuncSlot("pfnWriteByte");
	const EnginefuncSlotMetadata *registerMessage =
		FindEnginefuncSlot("pfnRegUserMsg");

	if (!messageBegin || !messageWrite || !registerMessage)
		return false;

	if (messageBegin->domain != EnginefuncDomain::MessageSession ||
		messageBegin->adapterOwner != EnginefuncAdapterOwner::MessageBuffers ||
		messageBegin->readiness != EnginefuncReadiness::StartNow ||
		registerMessage->domain != EnginefuncDomain::MessageSession ||
		registerMessage->adapterOwner !=
			EnginefuncAdapterOwner::MessageBuffers ||
		messageWrite->readiness != EnginefuncReadiness::StartNow)
	{
		return false;
	}

	GameDllUserMessageSlot slots[4] = {};
	slots[1] = Slot(
		"HudText",
		kGameDllUserMessageLastServiceMessage + 1,
		3);

	const GameDllUserMessageRegistrationPlan plan =
		BuildGameDllUserMessageRegistrationPlan(
			RegistrationRequest(slots, 4, "SayText", -1, true));

	if (plan.action != GameDllUserMessageRegistrationAction::RegisterNew ||
		plan.slotIndex != 2 ||
		plan.messageNumber != kGameDllUserMessageLastServiceMessage + 2 ||
		plan.storedSize != -1 ||
		!plan.resendRegistration)
	{
		return false;
	}

	const GameDllUserMessageSlot newSlot =
		Slot("SayText", plan.messageNumber, plan.storedSize);

	unsigned char messageBytes[16] = {};
	GameDllMessageSession session(messageBytes, sizeof(messageBytes));
	if (session.begin(BuildGameDllUserMessageBeginRequest(
			kGameDllMessageDestinationBroadcast,
			newSlot)) != GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	if (session.writeString("ok") != GameDllMessageSessionStatus::Ok)
		return false;

	const GameDllMessageEndResult end = session.end({ false, true });

	const GameDllUserMessageRegistrationBroadcast broadcast =
		BuildGameDllUserMessageRegistrationBroadcast(plan, "SayText");
	unsigned char registrationBytes[32] = {};
	NetworkBitBuffer writer(
		registrationBytes,
		sizeof(registrationBytes) << 3);
	WriteGameDllUserMessageRegistrationBroadcast(writer, broadcast);
	NetworkBitBuffer reader(registrationBytes, writer.tellBit());

	return end.status == GameDllMessageSessionStatus::Ok &&
		end.shouldMulticast &&
		end.shouldPatchPayloadSize &&
		end.payloadSizePatchOffset == 1 &&
		end.payloadSize == 3 &&
		messageBytes[0] == plan.messageNumber &&
		messageBytes[1] == 3 &&
		messageBytes[2] == 0 &&
		messageBytes[3] == 'o' &&
		messageBytes[4] == 'k' &&
		messageBytes[5] == 0 &&
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

bool TestResourceOutputPayloadAndClientInfoPoliciesStayComposable()
{
	const GameDllResourceNameDecision optionalModel =
		BuildGameDllResourceNameDecision(
			"!models\\barney.mdl",
			GameDllResourceNameMode::ModelPrecache,
			0);
	const GameDllSoundRoute multiplayerSound =
		BuildGameDllStartSoundRoute(0, 2, 8, false);
	const GameDllSoundRoute spawningSound =
		BuildGameDllStartSoundRoute(
			kGameDllPayloadSoundSpawningFlag,
			2,
			8,
			false);
	const GameDllClientKeyValuePlan clientKey =
		BuildGameDllClientKeyValuePlan(false, true, 2, 4, true);
	const GameDllPlayerStats stats =
		BuildGameDllPlayerStats(true, 0.123f, 7);

	return optionalModel.action == GameDllResourceNameAction::UseName &&
		optionalModel.optional &&
		optionalModel.normalizedName == "models/barney.mdl" &&
		multiplayerSound.destination ==
			kGameDllPayloadDestinationPasReliable &&
		!multiplayerSound.filterClient &&
		spawningSound.destination == kGameDllPayloadDestinationInit &&
		BuildGameDllClientCommandAction(
			true,
			true,
			false,
			true) == GameDllClientCommandAction::StuffText &&
		BuildGameDllAlertOutputAction(
			kGameDllAlertLogged,
			2,
			0.0f) == GameDllAlertOutputAction::Log &&
		BuildGameDllInfoBufferRoute(true, false, true) ==
			GameDllInfoBufferRoute::ClientUserinfo &&
		clientKey.action == GameDllClientKeyValueAction::UpdateAndResend &&
		clientKey.clientIndex == 1 &&
		stats.ping == 123 &&
		stats.packetLoss == 7;
}

bool TestEntityMovementVisibilityAndChangelevelPoliciesStayComposable()
{
	const GameDllPrivateDataAllocationPlan allocation =
		BuildGameDllPrivateDataAllocationPlan(17);
	const GameDllAngleRewritePlan angle =
		BuildGameDllAngleRewritePlan("angle", "90", 0.0f, 0.0f);
	GameDllStringPoolCompatibilityModel pool(64, false);
	const GameDllStringAllocation text = pool.allocate("models\\nbarney");
	const GameDllVector3 viewAngles = { 1.0f, 2.0f, 3.0f };
	const GameDllRunPlayerMovePlan move =
		BuildGameDllRunPlayerMovePlan(
			true,
			true,
			20.0,
			0.1,
			viewAngles,
			10.0f,
			1.0f,
			0.0f,
			3,
			4,
			5);
	const GameDllTraceModelPlan trace =
		BuildGameDllTraceModelPlan(true, -4, true, false);
	const GameDllQueuedChangeLevelPlan changelevel =
		BuildGameDllQueuedChangeLevelPlan(
			"c1a1",
			"lm",
			"c1a0",
			kGameDllMapExists | kGameDllMapHasLandmark,
			true,
			1,
			20);

	return allocation.action ==
			GameDllPrivateDataAllocationAction::AllocateRoundedBlock &&
		allocation.shouldFreeExisting &&
		allocation.roundedBytes == 32 &&
		angle.rewritten &&
		angle.keyName == "angles" &&
		angle.value == "0 90 0" &&
		!text.duplicate &&
		pool.isValidHandle(text.handle) &&
		std::strcmp(pool.getString(text.handle), "models\nbarney") == 0 &&
		move.action == GameDllRunPlayerMoveAction::RunFakeClientMove &&
		NearlyEqualDouble(move.timebase, 20.095) &&
		move.command.viewAngles.x == 1.0f &&
		move.command.buttons == 3 &&
		move.command.impulse == 4 &&
		move.command.msec == 5 &&
		trace.action == GameDllTraceModelAction::RunCustomClip &&
		trace.hullNumber == 0 &&
		trace.clampedHull &&
		BuildGameDllCanSkipPlayerResult(true, true) &&
		changelevel.action == GameDllQueuedChangeLevelAction::QueueSmooth &&
		changelevel.smoothRequested &&
		changelevel.smoothQueued;
}

bool TestLoadPolicyKeepsLegacyLifetimeExternal()
{
	const GameDllRequiredSymbolPlan publish =
		BuildGameDllRequiredSymbolPlan(true, true, false, true);
	const GameDllRequiredSymbolPlan missing =
		BuildGameDllRequiredSymbolPlan(true, false, false, true);
	const GameDllUnloadPlan unload =
		BuildGameDllUnloadPlan(true, true);

	return publish.action ==
			GameDllRequiredSymbolAction::PublishEngineFunctions &&
		!publish.freeLibrary &&
		!publish.freeMempool &&
		!publish.clearLibraryHandle &&
		missing.action == GameDllRequiredSymbolAction::RejectMissingEntityApi &&
		missing.freeLibrary &&
		missing.freeMempool &&
		missing.clearLibraryHandle &&
		unload.action == GameDllUnloadAction::UnloadLibraryAndClearState &&
		unload.callGameShutdown &&
		unload.freeLibrary &&
		unload.freeMempool &&
		unload.clearServerGameState;
}

}

int main()
{
	if (!TestAbiMetadataFeedsMessageRegistrationFlow())
		return 1;

	if (!TestResourceOutputPayloadAndClientInfoPoliciesStayComposable())
		return 2;

	if (!TestEntityMovementVisibilityAndChangelevelPoliciesStayComposable())
		return 3;

	if (!TestLoadPolicyKeepsLegacyLifetimeExternal())
		return 4;

	return EXIT_SUCCESS;
}
