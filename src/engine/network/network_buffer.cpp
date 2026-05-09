#include "engine/network/network_buffer.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace network
{

size_t NetworkBufferBitsToBytes(size_t bits)
{
	return (bits + 7) >> 3;
}

NetworkBitBuffer::NetworkBitBuffer(void *data, size_t dataBits, size_t cursorBit)
	: m_data(static_cast<unsigned char *>(data))
	, m_dataBits(dataBits)
	, m_cursorBit(cursorBit)
	, m_overflow(false)
	, m_alternateSignDepth(0)
{
	if (!m_data || m_cursorBit > m_dataBits)
	{
		m_cursorBit = m_dataBits;
		m_overflow = true;
	}
}

void NetworkBitBuffer::clear()
{
	m_cursorBit = 0;
	m_overflow = false;
	m_alternateSignDepth = 0;
}

bool NetworkBitBuffer::overflow() const
{
	return m_overflow;
}

size_t NetworkBitBuffer::tellBit() const
{
	return m_cursorBit;
}

size_t NetworkBitBuffer::maxBits() const
{
	return m_dataBits;
}

size_t NetworkBitBuffer::bitsLeft() const
{
	if (m_cursorBit > m_dataBits)
		return 0;

	return m_dataBits - m_cursorBit;
}

bool NetworkBitBuffer::seekToBit(size_t bitPosition)
{
	if (bitPosition > m_dataBits)
		return false;

	m_cursorBit = bitPosition;
	return true;
}

void NetworkBitBuffer::startAlternateSign()
{
	++m_alternateSignDepth;
}

bool NetworkBitBuffer::endAlternateSign()
{
	--m_alternateSignDepth;

	if (m_alternateSignDepth < 0)
	{
		m_alternateSignDepth = 0;
		return false;
	}

	if ((m_cursorBit & 7) != 0)
		seekToBit(m_cursorBit + (8 - (m_cursorBit & 7)));

	return true;
}

int NetworkBitBuffer::alternateSignDepth() const
{
	return m_alternateSignDepth;
}

void NetworkBitBuffer::writeOneBit(int value)
{
	if (checkOverflow(1))
		return;

	setBit(m_cursorBit, value);
	++m_cursorBit;
}

void NetworkBitBuffer::writeUnsigned(uint32_t value, int bitCount)
{
	if (bitCount < 1 || bitCount > 32)
	{
		setOverflowAtEnd();
		return;
	}

	if (checkOverflow(static_cast<size_t>(bitCount)))
	{
		m_cursorBit = m_dataBits;
		return;
	}

	for (int i = 0; i < bitCount; ++i)
	{
		setBit(m_cursorBit, (value >> i) & 1U);
		++m_cursorBit;
	}
}

void NetworkBitBuffer::writeSigned(int32_t value, int bitCount)
{
	if (bitCount < 1 || bitCount > 32)
	{
		setOverflowAtEnd();
		return;
	}

	if (m_alternateSignDepth != 0)
	{
		const uint32_t magnitude = value < 0 ?
			static_cast<uint32_t>(-static_cast<int64_t>(value)) :
			static_cast<uint32_t>(value);

		writeOneBit(value < 0 ? 1 : 0);
		writeUnsigned(magnitude, bitCount - 1);
		return;
	}

	if (value < 0)
	{
		writeUnsigned(static_cast<uint32_t>(0x80000000U + value), bitCount - 1);
		writeOneBit(1);
	}
	else
	{
		writeUnsigned(static_cast<uint32_t>(value), bitCount - 1);
		writeOneBit(0);
	}
}

bool NetworkBitBuffer::writeBits(const void *data, size_t bitCount)
{
	const unsigned char *source = static_cast<const unsigned char *>(data);

	if (!source && bitCount != 0)
	{
		setOverflowAtEnd();
		return false;
	}

	for (size_t i = 0; i < bitCount; ++i)
		writeOneBit((source[i >> 3] >> (i & 7)) & 1);

	return !m_overflow;
}

int NetworkBitBuffer::readOneBit()
{
	if (checkOverflow(1))
		return 0;

	const int value = getBit(m_cursorBit);
	++m_cursorBit;
	return value;
}

uint32_t NetworkBitBuffer::readUnsigned(int bitCount)
{
	if (bitCount == 8 && bitsLeft() < 8)
		return 0;

	if (bitCount < 1 || bitCount > 32)
	{
		setOverflowAtEnd();
		return 0;
	}

	if (checkOverflow(static_cast<size_t>(bitCount)))
	{
		m_cursorBit = m_dataBits;
		return 0;
	}

	uint32_t value = 0;

	for (int i = 0; i < bitCount; ++i)
	{
		if (getBit(m_cursorBit))
			value |= (1U << i);

		++m_cursorBit;
	}

	return value;
}

int32_t NetworkBitBuffer::readSigned(int bitCount)
{
	if (bitCount < 1 || bitCount > 32)
	{
		setOverflowAtEnd();
		return 0;
	}

	if (m_alternateSignDepth != 0)
	{
		const int sign = readOneBit();
		const uint32_t value = readUnsigned(bitCount - 1);
		return sign ? -static_cast<int32_t>(value) : static_cast<int32_t>(value);
	}

	const uint32_t value = readUnsigned(bitCount - 1);

	if (readOneBit())
		return -static_cast<int32_t>((1U << (bitCount - 1)) - value);

	return static_cast<int32_t>(value);
}

bool NetworkBitBuffer::readBits(void *outData, size_t bitCount)
{
	unsigned char *out = static_cast<unsigned char *>(outData);

	if (!out && bitCount != 0)
	{
		setOverflowAtEnd();
		return false;
	}

	std::memset(out, 0, NetworkBufferBitsToBytes(bitCount));

	for (size_t i = 0; i < bitCount; ++i)
	{
		if (readOneBit())
			out[i >> 3] |= static_cast<unsigned char>(1U << (i & 7));
	}

	return !m_overflow;
}

bool NetworkBitBuffer::exciseBits(size_t startBit, size_t bitsToRemove)
{
	if (startBit > m_dataBits || bitsToRemove > m_dataBits - startBit)
	{
		m_overflow = true;
		return false;
	}

	const size_t endBit = startBit + bitsToRemove;
	const size_t remainingToEnd = m_dataBits - endBit;

	for (size_t i = 0; i < remainingToEnd; ++i)
		setBit(startBit + i, getBit(endBit + i));

	m_dataBits -= bitsToRemove;
	m_cursorBit = startBit;
	return true;
}

bool NetworkBitBuffer::checkOverflow(size_t bitCount)
{
	if (!m_data || bitCount > m_dataBits - m_cursorBit)
	{
		m_overflow = true;
		return true;
	}

	return m_overflow;
}

void NetworkBitBuffer::setOverflowAtEnd()
{
	m_overflow = true;
	m_cursorBit = m_dataBits;
}

int NetworkBitBuffer::getBit(size_t bitPosition) const
{
	return (m_data[bitPosition >> 3] >> (bitPosition & 7)) & 1;
}

void NetworkBitBuffer::setBit(size_t bitPosition, int value)
{
	if (value)
		m_data[bitPosition >> 3] |= static_cast<unsigned char>(1U << (bitPosition & 7));
	else
		m_data[bitPosition >> 3] &= static_cast<unsigned char>(~(1U << (bitPosition & 7)));
}

}
}
}
