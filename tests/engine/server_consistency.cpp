#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/resources/server_consistency_list.hpp"
#include "engine/server/resources/server_consistency_policy.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

namespace
{

static ConsistencyListRequest ListRequest(
	const int *indexes,
	std::size_t count)
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

static bool BytesEqual(
	const std::uint8_t *lhs,
	const std::uint8_t *rhs,
	std::size_t size)
{
	return std::memcmp(lhs, rhs, size) == 0;
}

static bool TestDisabledWritesStopBit()
{
	unsigned char data[8] = {};
	int indexes[] = { 3 };
	ConsistencyListRequest request = ListRequest(indexes, 1);
	NetworkBitBuffer writer(data, sizeof(data) << 3);

	request.maxClients = 1;
	ConsistencyListWriteResult result = WriteConsistencyList(writer, request);
	if (result.forceUnmodified || writer.tellBit() != 1 || data[0] != 0)
		return false;

	writer.clear();
	std::memset(data, 0, sizeof(data));
	request = ListRequest(indexes, 1);
	request.consistencyEnabled = false;
	result = WriteConsistencyList(writer, request);
	if (result.forceUnmodified || writer.tellBit() != 1 || data[0] != 0)
		return false;

	writer.clear();
	std::memset(data, 0, sizeof(data));
	request = ListRequest(indexes, 1);
	request.consistencyCount = 0;
	result = WriteConsistencyList(writer, request);
	if (result.forceUnmodified || writer.tellBit() != 1 || data[0] != 0)
		return false;

	writer.clear();
	std::memset(data, 0, sizeof(data));
	request = ListRequest(indexes, 1);
	request.hltvProxy = true;
	result = WriteConsistencyList(writer, request);
	return !result.forceUnmodified && writer.tellBit() == 1 && data[0] == 0;
}

static bool TestEnabledEmptyListWritesTerminator()
{
	unsigned char data[8] = {};
	ConsistencyListRequest request = ListRequest(nullptr, 0);
	NetworkBitBuffer writer(data, sizeof(data) << 3);

	const ConsistencyListWriteResult result =
		WriteConsistencyList(writer, request);
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

	const ConsistencyListWriteResult result =
		WriteConsistencyList(writer, ListRequest(indexes, 2));
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

	const ConsistencyListWriteResult result =
		WriteConsistencyList(writer, ListRequest(indexes, 2));
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
	const ConsistencyListWriteResult result =
		WriteConsistencyList(writer, ListRequest(indexes, 4));

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
			static_cast<int>(
				reader.readUnsigned(kConsistencyListDeltaBits)) + lastCheck :
			static_cast<int>(
				reader.readUnsigned(kConsistencyListResourceIndexBits));

		if (count >= 4 || index != indexes[count])
			return false;

		lastCheck = index;
		++count;
	}

	return count == 4 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow();
}

static bool TestOverflow()
{
	unsigned char data[1] = {};
	int indexes[] = { 0, 1 };
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteConsistencyList(writer, ListRequest(indexes, 2));
	return writer.overflow();
}

static bool TestSetupGates()
{
	return ConsistencyResourceNeedsSetup(false, true) &&
		!ConsistencyResourceNeedsSetup(true, true) &&
		!ConsistencyResourceNeedsSetup(false, false);
}

static bool TestReservationExactAndIfAvailableRemainMd5Only()
{
	ConsistencyReservationRequest request = {};
	request.resourceType = ResourceType::Model;
	request.forceType = kConsistencyForceExactFile;

	ConsistencyReservationResult result = BuildConsistencyReservation(request);
	if (result.requiresModelBounds ||
		result.hasReservedData ||
		result.unsupportedForceType)
	{
		return false;
	}

	request.forceType = kConsistencyForceModelSpecifyBoundsIfAvailable;
	result = BuildConsistencyReservation(request);
	return !result.requiresModelBounds &&
		!result.hasReservedData &&
		!result.unsupportedForceType;
}

static bool TestReservationSameBoundsUsesModelSnapshot()
{
	const float mins[] = { -1.0f, -2.0f, -3.0f };
	const float maxs[] = { 1.0f, 2.0f, 3.0f };
	ConsistencyReservationRequest request = {};
	request.resourceType = ResourceType::Model;
	request.forceType = kConsistencyForceModelSameBounds;

	ConsistencyReservationResult result = BuildConsistencyReservation(request);
	if (!result.requiresModelBounds || result.hasReservedData)
		return false;

	request.modelMins = mins;
	request.modelMaxs = maxs;
	request.modelBoundsAvailable = true;
	result = BuildConsistencyReservation(request);

	return result.requiresModelBounds &&
		result.hasReservedData &&
		result.reservedData[0] == kConsistencyForceModelSameBounds &&
		BytesEqual(
			result.reservedData + kConsistencyPolicyMinsOffset,
			reinterpret_cast<const std::uint8_t *>(mins),
			sizeof(mins)) &&
		BytesEqual(
			result.reservedData + kConsistencyPolicyMaxsOffset,
			reinterpret_cast<const std::uint8_t *>(maxs),
			sizeof(maxs));
}

static bool TestReservationSpecifyBoundsUsesRequestedBounds()
{
	const float mins[] = { -4.0f, -5.0f, -6.0f };
	const float maxs[] = { 4.0f, 5.0f, 6.0f };
	ConsistencyReservationRequest request = {};
	request.resourceType = ResourceType::Model;
	request.forceType = kConsistencyForceModelSpecifyBounds;
	request.specifiedMins = mins;
	request.specifiedMaxs = maxs;

	const ConsistencyReservationResult result =
		BuildConsistencyReservation(request);
	return result.hasReservedData &&
		result.reservedData[0] == kConsistencyForceModelSpecifyBounds &&
		BytesEqual(
			result.reservedData + kConsistencyPolicyMinsOffset,
			reinterpret_cast<const std::uint8_t *>(mins),
			sizeof(mins)) &&
		BytesEqual(
			result.reservedData + kConsistencyPolicyMaxsOffset,
			reinterpret_cast<const std::uint8_t *>(maxs),
			sizeof(maxs));
}

static bool TestChecksumComparesOnlyFirstFourBytes()
{
	std::uint8_t hash[kConsistencyPolicyHashSize] = {};
	std::uint8_t prefix[kConsistencyPolicyHashPrefixSize] = {};

	for (std::size_t i = 0; i < kConsistencyPolicyHashSize; ++i)
		hash[i] = static_cast<std::uint8_t>(0x10 + i);

	std::memcpy(prefix, hash, sizeof(prefix));
	ConsistencyEntryDecision decision =
		EvaluateConsistencyChecksum(7, hash, prefix, sizeof(prefix));
	if (decision.status != ConsistencyEntryStatus::Match ||
		decision.resourceIndex != 7)
	{
		return false;
	}

	hash[4] ^= 0xFF;
	decision = EvaluateConsistencyChecksum(7, hash, prefix, sizeof(prefix));
	if (decision.status != ConsistencyEntryStatus::Match)
		return false;

	prefix[3] ^= 0xFF;
	decision = EvaluateConsistencyChecksum(7, hash, prefix, sizeof(prefix));
	return decision.status == ConsistencyEntryStatus::BadResource &&
		decision.resourceIndex == 7;
}

static bool TestBoundsPolicy()
{
	const float mins[] = { -1.0f, -2.0f, -3.0f };
	const float maxs[] = { 1.0f, 2.0f, 3.0f };
	const float insideMins[] = { -0.5f, -2.0f, -1.0f };
	const float insideMaxs[] = { 0.5f, 1.5f, 3.0f };
	const float outsideMins[] = { -2.0f, -2.0f, -3.0f };
	ConsistencyReservationRequest request = {};
	request.resourceType = ResourceType::Model;
	request.forceType = kConsistencyForceModelSpecifyBounds;
	request.specifiedMins = mins;
	request.specifiedMaxs = maxs;

	ConsistencyReservationResult reservation =
		BuildConsistencyReservation(request);
	ConsistencyEntryDecision decision =
		EvaluateConsistencyBounds(
			3,
			reservation.reservedData,
			insideMins,
			insideMaxs);
	if (decision.status != ConsistencyEntryStatus::Match)
		return false;

	decision = EvaluateConsistencyBounds(
		3,
		reservation.reservedData,
		outsideMins,
		insideMaxs);
	if (decision.status != ConsistencyEntryStatus::BadResource)
		return false;

	request.forceType = kConsistencyForceModelSameBounds;
	request.modelMins = mins;
	request.modelMaxs = maxs;
	request.modelBoundsAvailable = true;
	reservation = BuildConsistencyReservation(request);
	decision = EvaluateConsistencyBounds(
		3,
		reservation.reservedData,
		mins,
		maxs);
	if (decision.status != ConsistencyEntryStatus::Match)
		return false;

	decision = EvaluateConsistencyBounds(
		3,
		reservation.reservedData,
		insideMins,
		maxs);
	return decision.status == ConsistencyEntryStatus::BadResource;
}

static bool TestInvalidTypeAndCountMismatch()
{
	std::uint8_t reserved[kConsistencyPolicyReservedSize] = {};
	const float mins[] = { 0.0f, 0.0f, 0.0f };
	const float maxs[] = { 1.0f, 1.0f, 1.0f };

	reserved[0] = 99;
	const ConsistencyEntryDecision decision =
		EvaluateConsistencyBounds(4, reserved, mins, maxs);

	return decision.status == ConsistencyEntryStatus::InvalidType &&
		ConsistencyResponseCountMatches(3, 3) &&
		!ConsistencyResponseCountMatches(3, 2);
}

}

int main()
{
	if (!TestDisabledWritesStopBit() ||
		!TestEnabledEmptyListWritesTerminator() ||
		!TestSmallDeltaGoldenBytes() ||
		!TestLargeDeltaGoldenBytes() ||
		!TestReaderRoundTrip() ||
		!TestOverflow() ||
		!TestSetupGates() ||
		!TestReservationExactAndIfAvailableRemainMd5Only() ||
		!TestReservationSameBoundsUsesModelSnapshot() ||
		!TestReservationSpecifyBoundsUsesRequestedBounds() ||
		!TestChecksumComparesOnlyFirstFourBytes() ||
		!TestBoundsPolicy() ||
		!TestInvalidTypeAndCountMismatch())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
