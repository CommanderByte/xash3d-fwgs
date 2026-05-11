#include "engine/server/messaging/server_userinfo_message.hpp"
#include "engine/server/messaging/server_resource_message.hpp"
#include "engine/server/messaging/server_customization_message.hpp"

#include "engine/server/messaging/server_message_envelope.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{

void WriteUserinfoUpdatePayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const UserinfoUpdatePayload &payload)
{
	buffer.writeUnsigned(static_cast<std::uint32_t>(payload.clientIndex), kUserinfoClientIndexBits);
	buffer.writeSigned(payload.userId, 32);
	buffer.writeOneBit(payload.active ? 1 : 0);

	if (!payload.active)
		return;

	WriteServerMessageString(buffer, payload.userinfo);
	WriteServerMessageBytes(
		buffer,
		payload.hashedCdKeyDigest,
		kUserinfoDigestSize);
}

bool ResourceMessageRowHasReservedData(const ResourceMessageRow &row)
{
	if (!row.reservedData)
		return false;

	static const std::uint8_t zeros[kResourceMessageReservedSize] = {};
	return std::memcmp(row.reservedData, zeros, kResourceMessageReservedSize) != 0;
}

void WriteResourceMessageRow(
	xash::engine::network::NetworkBitBuffer &buffer,
	const ResourceMessageRow &row)
{
	const unsigned int transmittedFlags =
		row.flags & (kResourceMessageFlagFatalIfMissing | kResourceMessageFlagWasMissing);

	buffer.writeUnsigned(static_cast<std::uint32_t>(row.type), 4);
	WriteServerMessageString(buffer, row.name);
	buffer.writeUnsigned(static_cast<std::uint32_t>(row.index), kResourceMessageModelIndexBits);
	buffer.writeSigned(row.downloadSize, kResourceMessageDownloadSizeBits);
	buffer.writeUnsigned(transmittedFlags, kResourceMessageFlagsBits);

	if ((row.flags & kResourceMessageFlagCustom) != 0)
		WriteServerMessageBytes(buffer, row.md5Hash, kResourceMessageHashSize);

	const bool hasReservedData = ResourceMessageRowHasReservedData(row);
	buffer.writeOneBit(hasReservedData ? 1 : 0);

	if (hasReservedData)
		WriteServerMessageBytes(buffer, row.reservedData, kResourceMessageReservedSize);
}

void WriteCustomizationMessagePayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const CustomizationMessage &message)
{
	WriteServerMessageByte(buffer, static_cast<unsigned int>(message.playerNumber));
	WriteServerMessageByte(buffer, static_cast<unsigned int>(message.type));
	WriteServerMessageString(buffer, message.name);
	buffer.writeSigned(message.index, 16);
	buffer.writeSigned(message.downloadSize, 32);
	WriteServerMessageByte(buffer, message.flags);

	if ((message.flags & kCustomizationMessageFlagCustom) != 0)
		WriteServerMessageBytes(buffer, message.md5Hash, kCustomizationMessageHashSize);
}

}
}
}
