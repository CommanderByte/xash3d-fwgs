#include "engine/server/messaging/server_frame_datagram.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool HasFlag(int flags, int flag)
{
	return (flags & flag) != 0;
}

FrameTransferPlan MakeTransferPlan(FrameTransferAction action)
{
	FrameTransferPlan plan = {};
	plan.action = action;
	return plan;
}

FrameTransferPlan MakeCopyOrIgnorePlan(
	int sourceBytesWritten,
	int destinationBytesLeft)
{
	return sourceBytesWritten < destinationBytesLeft
		? MakeTransferPlan(FrameTransferAction::Copy)
		: MakeTransferPlan(FrameTransferAction::Ignore);
}

}

FrameTransferPlan BuildClientDatagramAppendPlan(
	bool sourceOverflowed,
	int sourceBytesWritten,
	int destinationBytesLeft,
	double realtime,
	double overflowWarnTime)
{
	if (sourceOverflowed)
	{
		FrameTransferPlan plan =
			MakeTransferPlan(FrameTransferAction::SourceOverflow);
		plan.clearSource = true;
		plan.warn = true;
		return plan;
	}

	FrameTransferPlan plan =
		MakeCopyOrIgnorePlan(sourceBytesWritten, destinationBytesLeft);
	plan.clearSource = true;

	if (plan.action == FrameTransferAction::Ignore &&
		realtime > overflowWarnTime)
	{
		plan.warn = true;
		plan.updateOverflowWarnTime = true;
		plan.nextOverflowWarnTime =
			realtime + kFrameOverflowWarnIntervalSeconds;
	}

	return plan;
}

FrameTransferPlan BuildServerReliableDatagramPlan(
	int sourceBytesWritten,
	int destinationBytesLeft)
{
	return sourceBytesWritten < destinationBytesLeft
		? MakeTransferPlan(FrameTransferAction::Copy)
		: MakeTransferPlan(FrameTransferAction::Fragment);
}

FrameTransferPlan BuildServerUnreliableDatagramPlan(
	int sourceBytesWritten,
	int destinationBytesLeft)
{
	FrameTransferPlan plan =
		MakeCopyOrIgnorePlan(sourceBytesWritten, destinationBytesLeft);
	plan.warn = plan.action == FrameTransferAction::Ignore;
	return plan;
}

FrameTransferPlan BuildServerSpectatorDatagramPlan(
	bool hltvProxy,
	int sourceBytesWritten,
	int destinationBytesLeft)
{
	if (!hltvProxy)
		return MakeTransferPlan(FrameTransferAction::None);

	return BuildServerUnreliableDatagramPlan(
		sourceBytesWritten,
		destinationBytesLeft);
}

FrameTransferPlan BuildOverflowClearPlan(bool overflowed)
{
	if (!overflowed)
		return MakeTransferPlan(FrameTransferAction::None);

	FrameTransferPlan plan = MakeTransferPlan(FrameTransferAction::ClearOverflow);
	plan.clearSource = true;
	plan.warn = true;
	return plan;
}

FrameReliableResendPlan BuildReliableResendPlan(
	int flags,
	double nextSendInfoTime,
	double realtime,
	int reliableBytesLeft,
	int userinfoLength)
{
	FrameReliableResendPlan plan = {};

	if (HasFlag(flags, kFrameResendUserinfoFlag) &&
		nextSendInfoTime <= realtime &&
		reliableBytesLeft >= userinfoLength + kFrameUserinfoReliableOverheadBytes)
	{
		plan.sendUserinfo = true;
		plan.clearUserinfoFlag = true;
		plan.updateNextSendInfoTime = true;
		plan.nextSendInfoTime = realtime + 1.0;
	}

	if (HasFlag(flags, kFrameResendMovevarsFlag))
	{
		plan.sendMovevars = true;
		plan.clearMovevarsFlag = true;
	}

	return plan;
}

bool ShouldProcessFrameClient(int clientState, bool fakeClient)
{
	return clientState > kFrameClientStateZombie && !fakeClient;
}

bool ShouldClearSkipNetMessage(int flags)
{
	return HasFlag(flags, kFrameSkipNetMessageFlag);
}

bool ShouldForceLocalClientSend(bool limitLocal, bool localAddress)
{
	return !limitLocal && localAddress;
}

bool ShouldScheduleSpawnedClientMessage(
	int clientState,
	double nextMessageTime,
	double realtime,
	double frameTime)
{
	if (clientState != kFrameClientStateSpawned)
		return false;

	const double timeUntilNextMessage =
		nextMessageTime - (realtime + frameTime);
	return timeUntilNextMessage <= 0.0 ||
		timeUntilNextMessage > kFrameHosedMessageIntervalSeconds;
}

bool ShouldDropReliableOverflow(bool overflowed)
{
	return overflowed;
}

bool ShouldClearSendAfterFailureTimeout(
	int flags,
	double failureTime,
	double realtime,
	double lastReceived)
{
	return HasFlag(flags, kFrameSendNetMessageFlag) &&
		failureTime < (realtime - lastReceived);
}

bool ShouldSendClientFrame(int flags)
{
	return HasFlag(flags, kFrameSendNetMessageFlag);
}

}
}
}
