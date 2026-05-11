#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/resources/server_consistency_list.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

static ConsistencyListRequest Request(const int *indexes, std::size_t count)
{
	ConsistencyListRequest request = {};
	request.maxClients = 2;
	request.consistencyEnabled = true;
	request.consistencyCount = 1;
	request.hltvProxy = false;
	request.resourceIndexes = indexes;
	request.resourceIndexCount = count;
	return request;
}

static bool TestDisabledWritesStopBit()
{
	unsigned char data[8] = {};
	int indexes[] = { 3 };
	ConsistencyListRequest request = Request(indexes, 1);
	NetworkBitBuffer writer(data, sizeof(data) << 3);

	request.maxClients = 1;
	ConsistencyListWriteResult result = WriteConsistencyList(writer, request);
	if (result.forceUnmodified || writer.tellBit() != 1 || data[0] != 0)
		return false;

	writer.clear();
	std::memset(data, 0, sizeof(data));
	request = Request(indexes, 1);
	request.consistencyEnabled = false;
	result = WriteConsistencyList(writer, request);
	if (result.forceUnmodified || writer.tellBit() != 1 || data[0] != 0)
		return false;

	writer.clear();
	std::memset(data, 0, sizeof(data));
	request = Request(indexes, 1);
	request.consistencyCount = 0;
	result = WriteConsistencyList(writer, request);
	if (result.forceUnmodified || writer.tellBit() != 1 || data[0] != 0)
		return false;

	writer.clear();
	std::memset(data, 0, sizeof(data));
	request = Request(indexes, 1);
	request.hltvProxy = true;
	result = WriteConsistencyList(writer, request);
	return !result.forceUnmodified && writer.tellBit() == 1 && data[0] == 0;
}

static bool TestEnabledEmptyListWritesTerminator()
{
	unsigned char data[8] = {};
	ConsistencyListRequest request = Request(nullptr, 0);
	NetworkBitBuffer writer(data, sizeof(data) << 3);

	ConsistencyListWriteResult result = WriteConsistencyList(writer, request);
	return result.forceUnmodified &&
		writer.tellBit() == 2 &&
		data[0] == 0x01 &&
		!writer.overflow();
}

static bool TestSmallDeltaGoldenBytes()
{
	static const unsigned char kExpected[] = { 0x1F, 0x1F };
	unsigned char data[8] = {};
	int indexes[] = { 3, 10 };
	NetworkBitBuffer writer(data, sizeof(data) << 3);

	ConsistencyListWriteResult result = WriteConsistencyList(writer, Request(indexes, 2));
	return result.forceUnmodified &&
		writer.tellBit() == 16 &&
		std::memcmp(data, kExpected, sizeof(kExpected)) == 0 &&
		!writer.overflow();
}

static bool TestLargeDeltaGoldenBytes()
{
	static const unsigned char kExpected[] = { 0x1F, 0xA1, 0x00 };
	unsigned char data[8] = {};
	int indexes[] = { 3, 40 };
	NetworkBitBuffer writer(data, sizeof(data) << 3);

	ConsistencyListWriteResult result = WriteConsistencyList(writer, Request(indexes, 2));
	return result.forceUnmodified &&
		writer.tellBit() == 23 &&
		std::memcmp(data, kExpected, sizeof(kExpected)) == 0 &&
		!writer.overflow();
}

static bool TestReaderRoundTrip()
{
	unsigned char data[16] = {};
	int indexes[] = { 0, 1, 40, 4095 };
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	ConsistencyListWriteResult result = WriteConsistencyList(writer, Request(indexes, 4));

	if (!result.forceUnmodified || writer.overflow())
		return false;

	NetworkBitBuffer reader(data, writer.tellBit());
	int lastCheck = 0;
	std::size_t count = 0;

	if (reader.readOneBit() != 1)
		return false;

	while (reader.readOneBit())
	{
		const bool isDelta = reader.readOneBit() != 0;
		const int index = isDelta ?
			static_cast<int>(reader.readUnsigned(kConsistencyListDeltaBits)) + lastCheck :
			static_cast<int>(reader.readUnsigned(kConsistencyListResourceIndexBits));

		if (count >= 4 || index != indexes[count])
			return false;

		lastCheck = index;
		++count;
	}

	return count == 4 && reader.tellBit() == writer.tellBit() && !reader.overflow();
}

static bool TestOverflow()
{
	unsigned char data[1] = {};
	int indexes[] = { 0, 1 };
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteConsistencyList(writer, Request(indexes, 2));
	return writer.overflow();
}

int main()
{
	if (!TestDisabledWritesStopBit() ||
		!TestEnabledEmptyListWritesTerminator() ||
		!TestSmallDeltaGoldenBytes() ||
		!TestLargeDeltaGoldenBytes() ||
		!TestReaderRoundTrip() ||
		!TestOverflow())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
