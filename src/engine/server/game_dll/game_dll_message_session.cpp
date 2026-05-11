#include "engine/server/game_dll/game_dll_message_session.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

constexpr size_t kNoPayloadSizePatch = static_cast<size_t>(-1);

int ClampInt(int value, int minValue, int maxValue)
{
	if (value < minValue)
		return minValue;

	if (value > maxValue)
		return maxValue;

	return value;
}

}

int ClampGameDllMessageNumber(int messageNumber)
{
	return ClampInt(messageNumber, kGameDllMessageSvcBad, 255);
}

int BoundGameDllMessageDestination(int destination)
{
	return ClampInt(
		destination,
		kGameDllMessageDestinationBroadcast,
		kGameDllMessageDestinationSpectator);
}

int NormalizeGameDllMessageByte(int value)
{
	return value == -1 ? 0xFF : value;
}

int GameDllMessageWritePayloadBytes(GameDllMessageWriteKind kind)
{
	switch (kind)
	{
	case GameDllMessageWriteKind::Byte:
	case GameDllMessageWriteKind::Char:
	case GameDllMessageWriteKind::Angle:
		return 1;
	case GameDllMessageWriteKind::Short:
	case GameDllMessageWriteKind::Coord:
	case GameDllMessageWriteKind::Entity:
		return 2;
	case GameDllMessageWriteKind::Long:
		return 4;
	}

	return 0;
}

size_t GameDllMessageStringPayloadBytes(const char *text)
{
	return text ? std::strlen(text) + 1 : 1;
}

bool IsGameDllMessageEntityIndexValid(int entityIndex, int entityCount)
{
	return entityIndex >= 0 && entityIndex < entityCount;
}

int RewriteGameDllMessageTarget(bool rewriteEnabled, int messageNumber)
{
	if (!rewriteEnabled)
		return 0;

	switch (messageNumber)
	{
	case kGameDllMessageGoldSrcSpawnStaticSound:
		return kGameDllMessageSvcSound;
	default:
		return 0;
	}
}

const char *GameDllMessageSessionStatusName(
	GameDllMessageSessionStatus status)
{
	switch (status)
	{
	case GameDllMessageSessionStatus::Ok:
		return "ok";
	case GameDllMessageSessionStatus::AlreadyStarted:
		return "already-started";
	case GameDllMessageSessionStatus::NotStarted:
		return "not-started";
	case GameDllMessageSessionStatus::Overflow:
		return "overflow";
	case GameDllMessageSessionStatus::RewriteFailed:
		return "rewrite-failed";
	case GameDllMessageSessionStatus::PayloadTooLarge:
		return "payload-too-large";
	case GameDllMessageSessionStatus::NegativePayloadSize:
		return "negative-payload-size";
	case GameDllMessageSessionStatus::FixedSizeMismatch:
		return "fixed-size-mismatch";
	case GameDllMessageSessionStatus::InvalidEntity:
		return "invalid-entity";
	case GameDllMessageSessionStatus::InternalError:
		return "internal-error";
	}

	return "unknown";
}

GameDllMessageSession::GameDllMessageSession(void *buffer, size_t bufferBytes)
	: m_buffer(static_cast<unsigned char *>(buffer))
	, m_bufferBytes(bufferBytes)
{
	resetState();
	if (!m_buffer && m_bufferBytes != 0)
		markOverflow();
}

void GameDllMessageSession::clear()
{
	m_cursorBytes = 0;
	m_overflow = false;
}

bool GameDllMessageSession::active() const
{
	return m_active;
}

bool GameDllMessageSession::overflow() const
{
	return m_overflow;
}

size_t GameDllMessageSession::cursorBytes() const
{
	return m_cursorBytes;
}

size_t GameDllMessageSession::bufferBytes() const
{
	return m_bufferBytes;
}

int GameDllMessageSession::payloadBytes() const
{
	return m_payloadBytes;
}

int GameDllMessageSession::messageNumber() const
{
	return m_messageNumber;
}

int GameDllMessageSession::effectiveMessageIndex() const
{
	return m_effectiveMessageIndex;
}

int GameDllMessageSession::rewriteOriginalMessage() const
{
	return m_rewriteOriginalMessage;
}

const char *GameDllMessageSession::messageName() const
{
	return m_messageName;
}

GameDllMessageSessionStatus GameDllMessageSession::begin(
	const GameDllMessageBeginRequest &request)
{
	if (m_active)
		return GameDllMessageSessionStatus::AlreadyStarted;

	const int messageNumber = ClampGameDllMessageNumber(
		request.messageNumber);
	const int rewriteTarget = RewriteGameDllMessageTarget(
		request.rewriteEnabled,
		messageNumber);

	m_active = true;
	m_systemMessage = request.systemMessage;
	m_messageNumber = messageNumber;
	m_destination = request.destination;
	m_messageName = rewriteTarget != 0 && request.rewriteName
		? request.rewriteName
		: request.name;
	m_rewriteOriginalMessage = rewriteTarget != 0 ? messageNumber : 0;
	m_effectiveMessageIndex = request.systemMessage
		? -(rewriteTarget != 0 ? rewriteTarget : messageNumber)
		: messageNumber;
	m_payloadSize = request.payloadSize;
	m_payloadBytes = 0;
	m_payloadSizePatchOffset = kNoPayloadSizePatch;

	if (!writeRawByte(static_cast<unsigned char>(messageNumber)))
		return GameDllMessageSessionStatus::Overflow;

	if (request.payloadSize == -1)
	{
		m_payloadSizePatchOffset = m_cursorBytes;
		if (!writeRawWord(0))
			return GameDllMessageSessionStatus::Overflow;
	}

	return GameDllMessageSessionStatus::Ok;
}

GameDllMessageSessionStatus GameDllMessageSession::writeByte(int value)
{
	if (!m_active)
		return GameDllMessageSessionStatus::NotStarted;

	if (!writeRawByte(static_cast<unsigned char>(
		NormalizeGameDllMessageByte(value))))
	{
		return GameDllMessageSessionStatus::Overflow;
	}

	addPayloadBytes(
		GameDllMessageWritePayloadBytes(GameDllMessageWriteKind::Byte));
	return GameDllMessageSessionStatus::Ok;
}

GameDllMessageSessionStatus GameDllMessageSession::writeChar(int value)
{
	if (!m_active)
		return GameDllMessageSessionStatus::NotStarted;

	if (!writeRawByte(static_cast<unsigned char>(value)))
		return GameDllMessageSessionStatus::Overflow;

	addPayloadBytes(
		GameDllMessageWritePayloadBytes(GameDllMessageWriteKind::Char));
	return GameDllMessageSessionStatus::Ok;
}

GameDllMessageSessionStatus GameDllMessageSession::writeShort(int value)
{
	if (!m_active)
		return GameDllMessageSessionStatus::NotStarted;

	if (!writeRawWord(static_cast<uint16_t>(value)))
		return GameDllMessageSessionStatus::Overflow;

	addPayloadBytes(
		GameDllMessageWritePayloadBytes(GameDllMessageWriteKind::Short));
	return GameDllMessageSessionStatus::Ok;
}

GameDllMessageSessionStatus GameDllMessageSession::writeLong(int value)
{
	if (!m_active)
		return GameDllMessageSessionStatus::NotStarted;

	if (!writeRawLong(static_cast<uint32_t>(value)))
		return GameDllMessageSessionStatus::Overflow;

	addPayloadBytes(
		GameDllMessageWritePayloadBytes(GameDllMessageWriteKind::Long));
	return GameDllMessageSessionStatus::Ok;
}

GameDllMessageSessionStatus GameDllMessageSession::writeAngle(float value)
{
	if (!m_active)
		return GameDllMessageSessionStatus::NotStarted;

	const int angle = static_cast<int>(value * 256.0f / 360.0f) & 255;
	if (!writeRawByte(static_cast<unsigned char>(angle)))
		return GameDllMessageSessionStatus::Overflow;

	addPayloadBytes(
		GameDllMessageWritePayloadBytes(GameDllMessageWriteKind::Angle));
	return GameDllMessageSessionStatus::Ok;
}

GameDllMessageSessionStatus GameDllMessageSession::writeCoord(float value)
{
	if (!m_active)
		return GameDllMessageSessionStatus::NotStarted;

	const int coord = static_cast<int>(value * 8.0f);
	if (!writeRawWord(static_cast<uint16_t>(coord)))
		return GameDllMessageSessionStatus::Overflow;

	addPayloadBytes(
		GameDllMessageWritePayloadBytes(GameDllMessageWriteKind::Coord));
	return GameDllMessageSessionStatus::Ok;
}

GameDllMessageSessionStatus GameDllMessageSession::writeString(
	const char *text)
{
	if (!m_active)
		return GameDllMessageSessionStatus::NotStarted;

	const size_t bytes = GameDllMessageStringPayloadBytes(text);
	if (text && text[0])
	{
		for (size_t i = 0; i < bytes; ++i)
		{
			if (!writeRawByte(static_cast<unsigned char>(text[i])))
				return GameDllMessageSessionStatus::Overflow;
		}
	}
	else if (!writeRawByte(0))
	{
		return GameDllMessageSessionStatus::Overflow;
	}

	addPayloadBytes(static_cast<int>(bytes));
	return GameDllMessageSessionStatus::Ok;
}

GameDllMessageSessionStatus GameDllMessageSession::writeEntity(
	int entityIndex,
	int entityCount)
{
	if (!m_active)
		return GameDllMessageSessionStatus::NotStarted;

	if (!IsGameDllMessageEntityIndexValid(entityIndex, entityCount))
		return GameDllMessageSessionStatus::InvalidEntity;

	if (!writeRawWord(static_cast<uint16_t>(entityIndex)))
		return GameDllMessageSessionStatus::Overflow;

	addPayloadBytes(
		GameDllMessageWritePayloadBytes(GameDllMessageWriteKind::Entity));
	return GameDllMessageSessionStatus::Ok;
}

GameDllMessageEndResult GameDllMessageSession::end(
	const GameDllMessageEndRequest &request)
{
	GameDllMessageEndResult result = {};
	result.status = GameDllMessageSessionStatus::Ok;
	result.payloadSizePatchOffset = kNoPayloadSizePatch;
	result.payloadSize = m_payloadBytes;
	result.boundedDestination = BoundGameDllMessageDestination(m_destination);
	result.rewriteAttempted = m_rewriteOriginalMessage != 0;

	if (!m_active)
	{
		result.status = GameDllMessageSessionStatus::NotStarted;
		return result;
	}

	m_active = false;

	if (request.bufferOverflow || m_overflow)
	{
		result.status = GameDllMessageSessionStatus::Overflow;
		result.shouldClearBuffer = true;
		clear();
		return result;
	}

	if (result.rewriteAttempted && !request.rewriteSucceeded)
	{
		result.status = GameDllMessageSessionStatus::RewriteFailed;
		result.shouldClearBuffer = true;
		clear();
		return result;
	}

	if (m_payloadSize == -1)
	{
		if (m_payloadBytes > kGameDllMessageMaxPayloadBytes)
		{
			result.status = GameDllMessageSessionStatus::PayloadTooLarge;
			result.shouldClearBuffer = true;
			clear();
			return result;
		}

		if (m_payloadBytes < 0)
		{
			result.status = GameDllMessageSessionStatus::NegativePayloadSize;
			result.shouldClearBuffer = true;
			clear();
			return result;
		}

		if (m_payloadSizePatchOffset == kNoPayloadSizePatch ||
			!patchWord(
				m_payloadSizePatchOffset,
				static_cast<uint16_t>(m_payloadBytes)))
		{
			result.status = GameDllMessageSessionStatus::InternalError;
			result.shouldClearBuffer = true;
			clear();
			return result;
		}

		result.shouldPatchPayloadSize = true;
		result.payloadSizePatchOffset = m_payloadSizePatchOffset;
	}
	else if (!m_systemMessage && m_payloadSize != m_payloadBytes)
	{
		result.status = GameDllMessageSessionStatus::FixedSizeMismatch;
		result.shouldClearBuffer = true;
		clear();
		return result;
	}

	if (m_systemMessage &&
		(m_effectiveMessageIndex == -kGameDllMessageSvcFinale ||
			m_effectiveMessageIndex == -kGameDllMessageSvcCutscene) &&
		m_payloadBytes == 0)
	{
		if (!writeRawByte(0))
		{
			result.status = GameDllMessageSessionStatus::Overflow;
			result.shouldClearBuffer = true;
			clear();
			return result;
		}

		result.appendedCompatibilityNullString = true;
	}

	result.shouldMulticast = true;
	return result;
}

void GameDllMessageSession::resetState()
{
	m_cursorBytes = 0;
	m_payloadSizePatchOffset = kNoPayloadSizePatch;
	m_payloadBytes = 0;
	m_payloadSize = 0;
	m_messageNumber = kGameDllMessageSvcBad;
	m_effectiveMessageIndex = 0;
	m_destination = kGameDllMessageDestinationBroadcast;
	m_rewriteOriginalMessage = 0;
	m_messageName = nullptr;
	m_active = false;
	m_systemMessage = false;
	m_overflow = false;
}

void GameDllMessageSession::markOverflow()
{
	m_overflow = true;
	m_cursorBytes = m_bufferBytes;
}

bool GameDllMessageSession::writeRawByte(unsigned char value)
{
	if (!m_buffer || m_cursorBytes >= m_bufferBytes)
	{
		markOverflow();
		return false;
	}

	m_buffer[m_cursorBytes++] = value;
	return true;
}

bool GameDllMessageSession::writeRawWord(uint16_t value)
{
	return writeRawByte(static_cast<unsigned char>(value & 0xFFU)) &&
		writeRawByte(static_cast<unsigned char>((value >> 8) & 0xFFU));
}

bool GameDllMessageSession::writeRawLong(uint32_t value)
{
	return writeRawByte(static_cast<unsigned char>(value & 0xFFU)) &&
		writeRawByte(static_cast<unsigned char>((value >> 8) & 0xFFU)) &&
		writeRawByte(static_cast<unsigned char>((value >> 16) & 0xFFU)) &&
		writeRawByte(static_cast<unsigned char>((value >> 24) & 0xFFU));
}

bool GameDllMessageSession::patchWord(size_t offset, uint16_t value)
{
	if (!m_buffer || offset + 1 >= m_bufferBytes)
		return false;

	m_buffer[offset] = static_cast<unsigned char>(value & 0xFFU);
	m_buffer[offset + 1] =
		static_cast<unsigned char>((value >> 8) & 0xFFU);
	return true;
}

void GameDllMessageSession::addPayloadBytes(int bytes)
{
	m_payloadBytes += bytes;
}

}
}
}
