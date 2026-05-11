#ifndef XASH_ENGINE_SERVER_GAME_DLL_MESSAGE_SESSION_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_MESSAGE_SESSION_HPP

#include <stddef.h>
#include <stdint.h>

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kGameDllMessageMaxPayloadBytes = 2048;
constexpr int kGameDllMessageSvcBad = 0;
constexpr int kGameDllMessageSvcSound = 6;
constexpr int kGameDllMessageSvcTempEntity = 23;
constexpr int kGameDllMessageSvcFinale = 31;
constexpr int kGameDllMessageSvcCutscene = 34;
constexpr int kGameDllMessageSvcLast = 59;
constexpr int kGameDllMessageGoldSrcSpawnStaticSound = 29;
constexpr int kGameDllMessageDestinationBroadcast = 0;
constexpr int kGameDllMessageDestinationSpectator = 9;

enum class GameDllMessageSessionStatus
{
	Ok,
	AlreadyStarted,
	NotStarted,
	Overflow,
	RewriteFailed,
	PayloadTooLarge,
	NegativePayloadSize,
	FixedSizeMismatch,
	InvalidEntity,
	InternalError
};

enum class GameDllMessageWriteKind
{
	Byte,
	Char,
	Short,
	Long,
	Angle,
	Coord,
	Entity
};

struct GameDllMessageBeginRequest
{
	int destination;
	int messageNumber;
	const char *name;
	int payloadSize;
	bool systemMessage;
	bool rewriteEnabled;
	int rewriteTargetMessage;
	const char *rewriteName;
};

struct GameDllMessageEndRequest
{
	bool bufferOverflow;
	bool rewriteSucceeded;
};

struct GameDllMessageEndResult
{
	GameDllMessageSessionStatus status;
	bool shouldClearBuffer;
	bool shouldMulticast;
	bool shouldPatchPayloadSize;
	size_t payloadSizePatchOffset;
	int payloadSize;
	int boundedDestination;
	bool appendedCompatibilityNullString;
	bool rewriteAttempted;
};

int ClampGameDllMessageNumber(int messageNumber);
int BoundGameDllMessageDestination(int destination);
int NormalizeGameDllMessageByte(int value);
int GameDllMessageWritePayloadBytes(GameDllMessageWriteKind kind);
size_t GameDllMessageStringPayloadBytes(const char *text);
bool IsGameDllMessageEntityIndexValid(int entityIndex, int entityCount);
int RewriteGameDllMessageTarget(bool rewriteEnabled, int messageNumber);
const char *GameDllMessageSessionStatusName(
	GameDllMessageSessionStatus status);

class GameDllMessageSession
{
public:
	GameDllMessageSession(void *buffer, size_t bufferBytes);

	void clear();
	bool active() const;
	bool overflow() const;
	size_t cursorBytes() const;
	size_t bufferBytes() const;
	int payloadBytes() const;
	int messageNumber() const;
	int effectiveMessageIndex() const;
	int rewriteOriginalMessage() const;
	const char *messageName() const;

	GameDllMessageSessionStatus begin(
		const GameDllMessageBeginRequest &request);
	GameDllMessageSessionStatus writeByte(int value);
	GameDllMessageSessionStatus writeChar(int value);
	GameDllMessageSessionStatus writeShort(int value);
	GameDllMessageSessionStatus writeLong(int value);
	GameDllMessageSessionStatus writeAngle(float value);
	GameDllMessageSessionStatus writeCoord(float value);
	GameDllMessageSessionStatus writeString(const char *text);
	GameDllMessageSessionStatus writeEntity(int entityIndex, int entityCount);
	GameDllMessageEndResult end(const GameDllMessageEndRequest &request);

private:
	unsigned char *m_buffer;
	size_t m_bufferBytes;
	size_t m_cursorBytes;
	size_t m_payloadSizePatchOffset;
	int m_payloadBytes;
	int m_payloadSize;
	int m_messageNumber;
	int m_effectiveMessageIndex;
	int m_destination;
	int m_rewriteOriginalMessage;
	const char *m_messageName;
	bool m_active;
	bool m_systemMessage;
	bool m_overflow;

	void resetState();
	void markOverflow();
	bool writeRawByte(unsigned char value);
	bool writeRawWord(uint16_t value);
	bool writeRawLong(uint32_t value);
	bool patchWord(size_t offset, uint16_t value);
	void addPayloadBytes(int bytes);
};

}
}
}

#endif
