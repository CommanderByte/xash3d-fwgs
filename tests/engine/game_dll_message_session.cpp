#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll_message_session.hpp"

using namespace xash::engine::server;

namespace
{

static GameDllMessageBeginRequest BeginRequest(
	int messageNumber,
	int payloadSize,
	bool systemMessage)
{
	GameDllMessageBeginRequest request = {};
	request.destination = kGameDllMessageDestinationBroadcast;
	request.messageNumber = messageNumber;
	request.name = "TestMessage";
	request.payloadSize = payloadSize;
	request.systemMessage = systemMessage;
	request.rewriteEnabled = false;
	request.rewriteTargetMessage = 0;
	request.rewriteName = nullptr;
	return request;
}

static bool TestMalformedSequences()
{
	unsigned char data[16] = {};
	GameDllMessageSession session(data, sizeof(data));

	if (session.end({ false, true }).status !=
		GameDllMessageSessionStatus::NotStarted)
	{
		return false;
	}

	if (session.writeByte(1) != GameDllMessageSessionStatus::NotStarted)
		return false;

	if (session.begin(BeginRequest(60, 1, false)) !=
		GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	return session.begin(BeginRequest(61, 1, false)) ==
		GameDllMessageSessionStatus::AlreadyStarted;
}

static bool TestFixedSizeMismatchClearsBuffer()
{
	unsigned char data[16] = {};
	GameDllMessageSession session(data, sizeof(data));

	if (session.begin(BeginRequest(60, 2, false)) !=
		GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	if (session.writeByte(1) != GameDllMessageSessionStatus::Ok)
		return false;

	const GameDllMessageEndResult result = session.end({ false, true });
	return result.status == GameDllMessageSessionStatus::FixedSizeMismatch &&
		result.shouldClearBuffer &&
		session.cursorBytes() == 0;
}

static bool TestVariableSizePatching()
{
	unsigned char data[16] = {};
	GameDllMessageSession session(data, sizeof(data));

	if (session.begin(
		BeginRequest(kGameDllMessageSvcTempEntity, -1, true)) !=
		GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	if (session.writeByte(0x7f) != GameDllMessageSessionStatus::Ok ||
		session.writeShort(0x1234) != GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	const GameDllMessageEndResult result = session.end({ false, true });
	return result.status == GameDllMessageSessionStatus::Ok &&
		result.shouldPatchPayloadSize &&
		result.payloadSizePatchOffset == 1 &&
		result.payloadSize == 3 &&
		data[0] == kGameDllMessageSvcTempEntity &&
		data[1] == 3 &&
		data[2] == 0 &&
		data[3] == 0x7f &&
		data[4] == 0x34 &&
		data[5] == 0x12;
}

static bool TestOverflowClearing()
{
	unsigned char data[2] = {};
	GameDllMessageSession session(data, sizeof(data));

	if (session.begin(BeginRequest(60, -1, false)) !=
		GameDllMessageSessionStatus::Overflow)
	{
		return false;
	}

	const GameDllMessageEndResult result = session.end({ true, true });
	return result.status == GameDllMessageSessionStatus::Overflow &&
		result.shouldClearBuffer &&
		session.cursorBytes() == 0;
}

static bool TestWriteByteMinusOneAndStringAccounting()
{
	unsigned char data[16] = {};
	GameDllMessageSession session(data, sizeof(data));

	if (session.begin(BeginRequest(60, -1, false)) !=
		GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	if (session.writeByte(-1) != GameDllMessageSessionStatus::Ok ||
		session.writeString(nullptr) != GameDllMessageSessionStatus::Ok ||
		session.writeString("") != GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	const GameDllMessageEndResult result = session.end({ false, true });
	return result.status == GameDllMessageSessionStatus::Ok &&
		result.payloadSize == 3 &&
		data[3] == 0xff &&
		data[4] == 0 &&
		data[5] == 0;
}

static bool TestFinaleAndCutsceneCompatibilityNull()
{
	unsigned char data[16] = {};
	GameDllMessageSession session(data, sizeof(data));

	if (session.begin(
		BeginRequest(kGameDllMessageSvcFinale, 0, true)) !=
		GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	const GameDllMessageEndResult finale = session.end({ false, true });
	if (finale.status != GameDllMessageSessionStatus::Ok ||
		!finale.appendedCompatibilityNullString ||
		session.cursorBytes() != 2 ||
		data[1] != 0)
	{
		return false;
	}

	session.clear();
	if (session.begin(
		BeginRequest(kGameDllMessageSvcCutscene, 0, true)) !=
		GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	const GameDllMessageEndResult cutscene = session.end({ false, true });
	return cutscene.status == GameDllMessageSessionStatus::Ok &&
		cutscene.appendedCompatibilityNullString &&
		session.cursorBytes() == 2;
}

static bool TestEntityBounds()
{
	unsigned char data[16] = {};
	GameDllMessageSession session(data, sizeof(data));

	if (session.begin(BeginRequest(60, 2, false)) !=
		GameDllMessageSessionStatus::Ok)
	{
		return false;
	}

	if (session.writeEntity(-1, 10) !=
		GameDllMessageSessionStatus::InvalidEntity)
	{
		return false;
	}

	if (session.writeEntity(10, 10) !=
		GameDllMessageSessionStatus::InvalidEntity)
	{
		return false;
	}

	return session.writeEntity(9, 10) == GameDllMessageSessionStatus::Ok;
}

static bool TestRewriteAdmission()
{
	unsigned char data[16] = {};
	GameDllMessageSession session(data, sizeof(data));
	GameDllMessageBeginRequest request =
		BeginRequest(kGameDllMessageGoldSrcSpawnStaticSound, 0, true);
	request.rewriteEnabled = true;
	request.rewriteTargetMessage = kGameDllMessageSvcSound;
	request.rewriteName = "svc_goldsrc_spawnstaticsound";

	if (RewriteGameDllMessageTarget(
		false,
		kGameDllMessageGoldSrcSpawnStaticSound) != 0)
	{
		return false;
	}

	if (RewriteGameDllMessageTarget(
		true,
		kGameDllMessageGoldSrcSpawnStaticSound) !=
		kGameDllMessageSvcSound)
	{
		return false;
	}

	if (session.begin(request) != GameDllMessageSessionStatus::Ok)
		return false;

	if (session.rewriteOriginalMessage() !=
		kGameDllMessageGoldSrcSpawnStaticSound ||
		session.effectiveMessageIndex() != -kGameDllMessageSvcSound)
	{
		return false;
	}

	const GameDllMessageEndResult failed = session.end({ false, false });
	return failed.status == GameDllMessageSessionStatus::RewriteFailed &&
		failed.shouldClearBuffer;
}

static bool TestUtilityFunctions()
{
	return ClampGameDllMessageNumber(-10) == kGameDllMessageSvcBad &&
		ClampGameDllMessageNumber(999) == 255 &&
		BoundGameDllMessageDestination(-5) ==
			kGameDllMessageDestinationBroadcast &&
		BoundGameDllMessageDestination(999) ==
			kGameDllMessageDestinationSpectator &&
		NormalizeGameDllMessageByte(-1) == 0xff &&
		GameDllMessageWritePayloadBytes(
			GameDllMessageWriteKind::Long) == 4 &&
		GameDllMessageStringPayloadBytes(nullptr) == 1 &&
		std::strcmp(
			GameDllMessageSessionStatusName(
				GameDllMessageSessionStatus::FixedSizeMismatch),
			"fixed-size-mismatch") == 0;
}

}

int main()
{
	if (!TestMalformedSequences() ||
		!TestFixedSizeMismatchClearsBuffer() ||
		!TestVariableSizePatching() ||
		!TestOverflowClearing() ||
		!TestWriteByteMinusOneAndStringAccounting() ||
		!TestFinaleAndCutsceneCompatibilityNull() ||
		!TestEntityBounds() ||
		!TestRewriteAdmission() ||
		!TestUtilityFunctions())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
