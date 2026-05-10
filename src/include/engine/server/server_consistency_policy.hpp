#ifndef XASH_ENGINE_SERVER_SERVER_CONSISTENCY_POLICY_HPP
#define XASH_ENGINE_SERVER_SERVER_CONSISTENCY_POLICY_HPP

#include "engine/server/resource_identity.hpp"

#include <cstddef>
#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr std::size_t kConsistencyPolicyHashSize = 16;
constexpr std::size_t kConsistencyPolicyHashPrefixSize = 4;
constexpr std::size_t kConsistencyPolicyReservedSize = 32;
constexpr std::size_t kConsistencyPolicyVectorComponents = 3;
constexpr std::size_t kConsistencyPolicyMinsOffset = 0x01;
constexpr std::size_t kConsistencyPolicyMaxsOffset = 0x0D;

constexpr int kConsistencyForceExactFile = 0;
constexpr int kConsistencyForceModelSameBounds = 1;
constexpr int kConsistencyForceModelSpecifyBounds = 2;
constexpr int kConsistencyForceModelSpecifyBoundsIfAvailable = 3;

enum class ConsistencyEntryStatus
{
	Match,
	BadResource,
	InvalidType,
};

struct ConsistencyReservationRequest
{
	ResourceType resourceType;
	int forceType;
	const float *specifiedMins;
	const float *specifiedMaxs;
	const float *modelMins;
	const float *modelMaxs;
	bool modelBoundsAvailable;
};

struct ConsistencyReservationResult
{
	bool requiresModelBounds;
	bool hasReservedData;
	bool unsupportedForceType;
	std::uint8_t reservedData[kConsistencyPolicyReservedSize];
};

struct ConsistencyEntryDecision
{
	ConsistencyEntryStatus status;
	int resourceIndex;
};

bool ConsistencyResourceNeedsSetup(bool alreadyCheckFile, bool inConsistencyList);
bool ConsistencyForceTypeRequiresModelBounds(ResourceType resourceType, int forceType);
ConsistencyReservationResult BuildConsistencyReservation(
	const ConsistencyReservationRequest &request);
bool ConsistencyReservedDataIsEmpty(const std::uint8_t *reservedData);
ConsistencyEntryDecision EvaluateConsistencyChecksum(
	int resourceIndex,
	const std::uint8_t *expectedHash,
	const std::uint8_t *receivedPrefix,
	std::size_t receivedPrefixSize);
ConsistencyEntryDecision EvaluateConsistencyBounds(
	int resourceIndex,
	const std::uint8_t *reservedData,
	const float *clientMins,
	const float *clientMaxs);
bool ConsistencyResponseCountMatches(int expectedCount, int actualCount);

}
}
}

#endif
