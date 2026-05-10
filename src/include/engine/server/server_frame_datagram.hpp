#ifndef XASH_ENGINE_SERVER_SERVER_FRAME_DATAGRAM_HPP
#define XASH_ENGINE_SERVER_SERVER_FRAME_DATAGRAM_HPP

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kFrameResendUserinfoFlag = 1 << 0;
constexpr int kFrameResendMovevarsFlag = 1 << 1;
constexpr int kFrameSkipNetMessageFlag = 1 << 2;
constexpr int kFrameSendNetMessageFlag = 1 << 3;
constexpr int kFrameFakeClientFlag = 1 << 7;
constexpr int kFrameHltvProxyFlag = 1 << 8;
constexpr int kFrameClientStateZombie = 1;
constexpr int kFrameClientStateConnected = 2;
constexpr int kFrameClientStateSpawned = 4;
constexpr int kFrameUserinfoReliableOverheadBytes = 6;
constexpr double kFrameOverflowWarnIntervalSeconds = 5.0;
constexpr double kFrameHosedMessageIntervalSeconds = 2.0;

enum class FrameTransferAction
{
	None = 0,
	Copy = 1,
	Fragment = 2,
	Ignore = 3,
	SourceOverflow = 4,
	ClearOverflow = 5
};

struct FrameTransferPlan
{
	FrameTransferAction action;
	bool clearSource;
	bool warn;
	bool updateOverflowWarnTime;
	double nextOverflowWarnTime;
};

struct FrameReliableResendPlan
{
	bool sendUserinfo;
	bool clearUserinfoFlag;
	bool updateNextSendInfoTime;
	double nextSendInfoTime;
	bool sendMovevars;
	bool clearMovevarsFlag;
};

FrameTransferPlan BuildClientDatagramAppendPlan(
	bool sourceOverflowed,
	int sourceBytesWritten,
	int destinationBytesLeft,
	double realtime,
	double overflowWarnTime);
FrameTransferPlan BuildServerReliableDatagramPlan(
	int sourceBytesWritten,
	int destinationBytesLeft);
FrameTransferPlan BuildServerUnreliableDatagramPlan(
	int sourceBytesWritten,
	int destinationBytesLeft);
FrameTransferPlan BuildServerSpectatorDatagramPlan(
	bool hltvProxy,
	int sourceBytesWritten,
	int destinationBytesLeft);
FrameTransferPlan BuildOverflowClearPlan(bool overflowed);

FrameReliableResendPlan BuildReliableResendPlan(
	int flags,
	double nextSendInfoTime,
	double realtime,
	int reliableBytesLeft,
	int userinfoLength);

bool ShouldProcessFrameClient(int clientState, bool fakeClient);
bool ShouldClearSkipNetMessage(int flags);
bool ShouldForceLocalClientSend(bool limitLocal, bool localAddress);
bool ShouldScheduleSpawnedClientMessage(
	int clientState,
	double nextMessageTime,
	double realtime,
	double frameTime);
bool ShouldDropReliableOverflow(bool overflowed);
bool ShouldClearSendAfterFailureTimeout(
	int flags,
	double failureTime,
	double realtime,
	double lastReceived);
bool ShouldSendClientFrame(int flags);

}
}
}

#endif
