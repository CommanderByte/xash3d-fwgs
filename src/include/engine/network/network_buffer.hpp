#ifndef XASH_ENGINE_NETWORK_NETWORK_BUFFER_HPP
#define XASH_ENGINE_NETWORK_NETWORK_BUFFER_HPP

#include <stddef.h>
#include <stdint.h>

namespace xash
{
namespace engine
{
namespace network
{

size_t NetworkBufferBitsToBytes(size_t bits);

class NetworkBitBuffer
{
public:
	NetworkBitBuffer(void *data, size_t dataBits, size_t cursorBit = 0);

	void clear();
	bool overflow() const;
	size_t tellBit() const;
	size_t maxBits() const;
	size_t bitsLeft() const;

	bool seekToBit(size_t bitPosition);

	void startAlternateSign();
	bool endAlternateSign();
	int alternateSignDepth() const;

	void writeOneBit(int value);
	void writeUnsigned(uint32_t value, int bitCount);
	void writeSigned(int32_t value, int bitCount);
	bool writeBits(const void *data, size_t bitCount);

	int readOneBit();
	uint32_t readUnsigned(int bitCount);
	int32_t readSigned(int bitCount);
	bool readBits(void *outData, size_t bitCount);

	bool exciseBits(size_t startBit, size_t bitsToRemove);

private:
	unsigned char *m_data;
	size_t m_dataBits;
	size_t m_cursorBit;
	bool m_overflow;
	int m_alternateSignDepth;

	bool checkOverflow(size_t bitCount);
	void setOverflowAtEnd();
	int getBit(size_t bitPosition) const;
	void setBit(size_t bitPosition, int value);
};

}
}
}

#endif
