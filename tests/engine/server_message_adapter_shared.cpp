#include <cstdlib>

#include "server/server_message_adapter_shared.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server::adapter;

namespace
{

struct TestWriteResult
{
	int current_bit;
	int overflow;
};

bool TestWriteWithNetworkBitBufferReturnsCursorAndOverflow()
{
	unsigned char data[2] = {};
	const TestWriteResult result =
		WriteWithNetworkBitBuffer<TestWriteResult>(
			data,
			static_cast<int>(sizeof(data) << 3),
			8,
			[](NetworkBitBuffer &buffer)
			{
				buffer.writeUnsigned(0xABU, 8);
			});

	return result.current_bit == 16 &&
		result.overflow == 0 &&
		data[0] == 0 &&
		data[1] == 0xAB;
}

bool TestWriteWithNetworkBitBufferClampsNegativeInputs()
{
	unsigned char data[1] = {};
	const TestWriteResult result =
		WriteWithNetworkBitBuffer<TestWriteResult>(
			data,
			8,
			-12,
			[](NetworkBitBuffer &buffer)
			{
				buffer.writeOneBit(1);
			});

	return result.current_bit == 1 &&
		result.overflow == 0 &&
		(data[0] & 1U) != 0;
}

bool TestWriteWithNetworkBitBufferPreservesOverflow()
{
	const TestWriteResult result =
		WriteWithNetworkBitBuffer<TestWriteResult>(
			nullptr,
			8,
			0,
			[](NetworkBitBuffer &buffer)
			{
				buffer.writeUnsigned(0xFFU, 8);
			});

	return result.current_bit == 8 &&
		result.overflow == 1;
}

void WriteTestByte(NetworkBitBuffer &buffer, unsigned int value)
{
	buffer.writeUnsigned(value, 8);
}

bool TestWritePayloadWithNetworkBitBufferForwardsArguments()
{
	unsigned char data[1] = {};
	const TestWriteResult result =
		WritePayloadWithNetworkBitBuffer<TestWriteResult>(
			data,
			static_cast<int>(sizeof(data) << 3),
			0,
			WriteTestByte,
			0x5AU);

	return result.current_bit == 8 &&
		result.overflow == 0 &&
		data[0] == 0x5A;
}

}

int main()
{
	if (!TestWriteWithNetworkBitBufferReturnsCursorAndOverflow() ||
		!TestWriteWithNetworkBitBufferClampsNegativeInputs() ||
		!TestWriteWithNetworkBitBufferPreservesOverflow() ||
		!TestWritePayloadWithNetworkBitBufferForwardsArguments())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
