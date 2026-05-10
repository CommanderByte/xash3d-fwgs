#include <cstdlib>

#include "engine/server/server_frame_datagram.hpp"

using namespace xash::engine::server;

namespace
{

static bool TestReliableDatagramCopiesWhenItFitsAndFragmentsWhenFull()
{
	const FrameTransferPlan copy =
		BuildServerReliableDatagramPlan(31, 32);
	const FrameTransferPlan fragmentEqual =
		BuildServerReliableDatagramPlan(32, 32);
	const FrameTransferPlan fragmentLarge =
		BuildServerReliableDatagramPlan(33, 32);

	return copy.action == FrameTransferAction::Copy &&
		fragmentEqual.action == FrameTransferAction::Fragment &&
		fragmentLarge.action == FrameTransferAction::Fragment;
}

static bool TestUnreliableDatagramCopiesOrWarnsAndIgnores()
{
	const FrameTransferPlan copy =
		BuildServerUnreliableDatagramPlan(15, 16);
	const FrameTransferPlan ignore =
		BuildServerUnreliableDatagramPlan(16, 16);

	return copy.action == FrameTransferAction::Copy &&
		!copy.warn &&
		ignore.action == FrameTransferAction::Ignore &&
		ignore.warn;
}

static bool TestSpectatorDatagramOnlyAppliesToHltvProxy()
{
	const FrameTransferPlan skipped =
		BuildServerSpectatorDatagramPlan(false, 8, 64);
	const FrameTransferPlan copy =
		BuildServerSpectatorDatagramPlan(true, 8, 64);
	const FrameTransferPlan ignore =
		BuildServerSpectatorDatagramPlan(true, 64, 64);

	return skipped.action == FrameTransferAction::None &&
		copy.action == FrameTransferAction::Copy &&
		ignore.action == FrameTransferAction::Ignore &&
		ignore.warn;
}

static bool TestClientDatagramOverflowAndThrottle()
{
	const FrameTransferPlan overflow =
		BuildClientDatagramAppendPlan(true, 4, 64, 10.0, 0.0);
	const FrameTransferPlan copy =
		BuildClientDatagramAppendPlan(false, 4, 64, 10.0, 0.0);
	const FrameTransferPlan throttled =
		BuildClientDatagramAppendPlan(false, 64, 64, 10.0, 12.0);
	const FrameTransferPlan warned =
		BuildClientDatagramAppendPlan(false, 64, 64, 13.0, 12.0);

	return overflow.action == FrameTransferAction::SourceOverflow &&
		overflow.clearSource &&
		overflow.warn &&
		copy.action == FrameTransferAction::Copy &&
		copy.clearSource &&
		!copy.warn &&
		throttled.action == FrameTransferAction::Ignore &&
		throttled.clearSource &&
		!throttled.warn &&
		!throttled.updateOverflowWarnTime &&
		warned.action == FrameTransferAction::Ignore &&
		warned.warn &&
		warned.updateOverflowWarnTime &&
		warned.nextOverflowWarnTime == 18.0;
}

static bool TestOverflowClearPlan()
{
	const FrameTransferPlan clean = BuildOverflowClearPlan(false);
	const FrameTransferPlan overflow = BuildOverflowClearPlan(true);

	return clean.action == FrameTransferAction::None &&
		!clean.clearSource &&
		overflow.action == FrameTransferAction::ClearOverflow &&
		overflow.clearSource &&
		overflow.warn;
}

static bool TestReliableResendPlan()
{
	const FrameReliableResendPlan tooSoon =
		BuildReliableResendPlan(
			kFrameResendUserinfoFlag | kFrameResendMovevarsFlag,
			12.0,
			10.0,
			64,
			8);
	const FrameReliableResendPlan noRoom =
		BuildReliableResendPlan(
			kFrameResendUserinfoFlag,
			9.0,
			10.0,
			13,
			8);
	const FrameReliableResendPlan ready =
		BuildReliableResendPlan(
			kFrameResendUserinfoFlag | kFrameResendMovevarsFlag,
			9.0,
			10.0,
			14,
			8);

	return !tooSoon.sendUserinfo &&
		tooSoon.sendMovevars &&
		tooSoon.clearMovevarsFlag &&
		!noRoom.sendUserinfo &&
		ready.sendUserinfo &&
		ready.clearUserinfoFlag &&
		ready.updateNextSendInfoTime &&
		ready.nextSendInfoTime == 11.0 &&
		ready.sendMovevars &&
		ready.clearMovevarsFlag;
}

static bool TestClientLoopGates()
{
	return !ShouldProcessFrameClient(kFrameClientStateZombie, false) &&
		!ShouldProcessFrameClient(kFrameClientStateSpawned, true) &&
		ShouldProcessFrameClient(kFrameClientStateConnected, false) &&
		ShouldClearSkipNetMessage(kFrameSkipNetMessageFlag) &&
		!ShouldClearSkipNetMessage(0) &&
		ShouldForceLocalClientSend(false, true) &&
		!ShouldForceLocalClientSend(true, true);
}

static bool TestSpawnedScheduleAndFailureTimeout()
{
	return ShouldScheduleSpawnedClientMessage(
			kFrameClientStateSpawned,
			10.5,
			10.0,
			0.5) &&
		ShouldScheduleSpawnedClientMessage(
			kFrameClientStateSpawned,
			13.0,
			10.0,
			0.5) &&
		!ShouldScheduleSpawnedClientMessage(
			kFrameClientStateConnected,
			10.5,
			10.0,
			0.5) &&
		!ShouldScheduleSpawnedClientMessage(
			kFrameClientStateSpawned,
			11.0,
			10.0,
			0.5) &&
		ShouldDropReliableOverflow(true) &&
		!ShouldDropReliableOverflow(false) &&
		ShouldClearSendAfterFailureTimeout(
			kFrameSendNetMessageFlag,
			4.0,
			10.0,
			5.0) &&
		!ShouldClearSendAfterFailureTimeout(
			0,
			4.0,
			10.0,
			5.0) &&
		ShouldSendClientFrame(kFrameSendNetMessageFlag) &&
		!ShouldSendClientFrame(0);
}

}

int main()
{
	if (!TestReliableDatagramCopiesWhenItFitsAndFragmentsWhenFull() ||
		!TestUnreliableDatagramCopiesOrWarnsAndIgnores() ||
		!TestSpectatorDatagramOnlyAppliesToHltvProxy() ||
		!TestClientDatagramOverflowAndThrottle() ||
		!TestOverflowClearPlan() ||
		!TestReliableResendPlan() ||
		!TestClientLoopGates() ||
		!TestSpawnedScheduleAndFailureTimeout())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
