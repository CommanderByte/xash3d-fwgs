#include "engine/server/resources/server_consistency_list.hpp"
#include "engine/server/resources/server_consistency_policy.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

void CopyVectorToReserved(std::uint8_t *reserved, std::size_t offset, const float *value)
{
	if (!value)
		return;

	std::memcpy(
		reserved + offset,
		value,
		sizeof(float) * kConsistencyPolicyVectorComponents);
}

void CopyVectorFromReserved(float *out, const std::uint8_t *reserved, std::size_t offset)
{
	std::memcpy(
		out,
		reserved + offset,
		sizeof(float) * kConsistencyPolicyVectorComponents);
}

bool VectorsEqual(const float *lhs, const float *rhs)
{
	for (std::size_t i = 0; i < kConsistencyPolicyVectorComponents; ++i)
	{
		if (lhs[i] != rhs[i])
			return false;
	}

	return true;
}

}

bool ConsistencyListShouldSend(const ConsistencyListRequest &request)
{
	return request.maxClients != 1 &&
		request.consistencyEnabled &&
		request.consistencyCount != 0 &&
		!request.hltvProxy;
}

ConsistencyListWriteResult WriteConsistencyList(
	xash::engine::network::NetworkBitBuffer &buffer,
	const ConsistencyListRequest &request)
{
	ConsistencyListWriteResult result = {};
	result.forceUnmodified = ConsistencyListShouldSend(request);

	if (!result.forceUnmodified)
	{
		buffer.writeOneBit(0);
		return result;
	}

	buffer.writeOneBit(1);

	int lastCheck = 0;
	for (std::size_t i = 0; i < request.resourceIndexCount; ++i)
	{
		const int index = request.resourceIndexes[i];
		const int delta = index - lastCheck;

		buffer.writeOneBit(1);

		if (delta > kConsistencyListMaxDelta)
		{
			buffer.writeOneBit(0);
			buffer.writeUnsigned(static_cast<unsigned int>(index), kConsistencyListResourceIndexBits);
		}
		else
		{
			buffer.writeOneBit(1);
			buffer.writeUnsigned(static_cast<unsigned int>(delta), kConsistencyListDeltaBits);
		}

		lastCheck = index;
	}

	buffer.writeOneBit(0);
	return result;
}

bool ConsistencyResourceNeedsSetup(bool alreadyCheckFile, bool inConsistencyList)
{
	return !alreadyCheckFile && inConsistencyList;
}

bool ConsistencyForceTypeRequiresModelBounds(ResourceType resourceType, int forceType)
{
	return resourceType == ResourceType::Model &&
		forceType == kConsistencyForceModelSameBounds;
}

ConsistencyReservationResult BuildConsistencyReservation(
	const ConsistencyReservationRequest &request)
{
	ConsistencyReservationResult result = {};

	if (request.resourceType != ResourceType::Model)
		return result;

	switch (request.forceType)
	{
	case kConsistencyForceExactFile:
		return result;
	case kConsistencyForceModelSameBounds:
		result.requiresModelBounds = true;
		if (!request.modelBoundsAvailable)
			return result;

		result.hasReservedData = true;
		result.reservedData[0] = static_cast<std::uint8_t>(request.forceType);
		CopyVectorToReserved(result.reservedData, kConsistencyPolicyMinsOffset, request.modelMins);
		CopyVectorToReserved(result.reservedData, kConsistencyPolicyMaxsOffset, request.modelMaxs);
		return result;
	case kConsistencyForceModelSpecifyBounds:
		result.hasReservedData = true;
		result.reservedData[0] = static_cast<std::uint8_t>(request.forceType);
		CopyVectorToReserved(result.reservedData, kConsistencyPolicyMinsOffset, request.specifiedMins);
		CopyVectorToReserved(result.reservedData, kConsistencyPolicyMaxsOffset, request.specifiedMaxs);
		return result;
	case kConsistencyForceModelSpecifyBoundsIfAvailable:
		return result;
	default:
		result.unsupportedForceType = true;
		return result;
	}
}

bool ConsistencyReservedDataIsEmpty(const std::uint8_t *reservedData)
{
	static const std::uint8_t empty[kConsistencyPolicyReservedSize] = {};

	if (!reservedData)
		return true;

	return std::memcmp(reservedData, empty, sizeof(empty)) == 0;
}

ConsistencyEntryDecision EvaluateConsistencyChecksum(
	int resourceIndex,
	const std::uint8_t *expectedHash,
	const std::uint8_t *receivedPrefix,
	std::size_t receivedPrefixSize)
{
	ConsistencyEntryDecision decision = {};
	decision.status = ConsistencyEntryStatus::Match;
	decision.resourceIndex = resourceIndex;

	if (!expectedHash || !receivedPrefix || receivedPrefixSize < kConsistencyPolicyHashPrefixSize)
	{
		decision.status = ConsistencyEntryStatus::BadResource;
		return decision;
	}

	if (std::memcmp(expectedHash, receivedPrefix, kConsistencyPolicyHashPrefixSize) != 0)
		decision.status = ConsistencyEntryStatus::BadResource;

	return decision;
}

ConsistencyEntryDecision EvaluateConsistencyBounds(
	int resourceIndex,
	const std::uint8_t *reservedData,
	const float *clientMins,
	const float *clientMaxs)
{
	ConsistencyEntryDecision decision = {};
	decision.status = ConsistencyEntryStatus::Match;
	decision.resourceIndex = resourceIndex;

	if (!reservedData || !clientMins || !clientMaxs)
	{
		decision.status = ConsistencyEntryStatus::InvalidType;
		return decision;
	}

	float expectedMins[kConsistencyPolicyVectorComponents] = {};
	float expectedMaxs[kConsistencyPolicyVectorComponents] = {};
	CopyVectorFromReserved(expectedMins, reservedData, kConsistencyPolicyMinsOffset);
	CopyVectorFromReserved(expectedMaxs, reservedData, kConsistencyPolicyMaxsOffset);

	switch (reservedData[0])
	{
	case kConsistencyForceModelSameBounds:
		if (!VectorsEqual(clientMins, expectedMins) || !VectorsEqual(clientMaxs, expectedMaxs))
			decision.status = ConsistencyEntryStatus::BadResource;
		return decision;
	case kConsistencyForceModelSpecifyBounds:
		for (std::size_t i = 0; i < kConsistencyPolicyVectorComponents; ++i)
		{
			if (clientMins[i] < expectedMins[i] || clientMaxs[i] > expectedMaxs[i])
			{
				decision.status = ConsistencyEntryStatus::BadResource;
				break;
			}
		}
		return decision;
	default:
		decision.status = ConsistencyEntryStatus::InvalidType;
		return decision;
	}
}

bool ConsistencyResponseCountMatches(int expectedCount, int actualCount)
{
	return expectedCount == actualCount;
}

}
}
}
