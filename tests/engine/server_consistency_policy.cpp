#include <cstdlib>
#include <cstring>

#include "engine/server/resources/server_consistency_policy.hpp"

using namespace xash::engine::server;

static bool BytesEqual(const std::uint8_t *lhs, const std::uint8_t *rhs, std::size_t size)
{
	return std::memcmp(lhs, rhs, size) == 0;
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
	if (result.requiresModelBounds || result.hasReservedData || result.unsupportedForceType)
		return false;

	request.forceType = kConsistencyForceModelSpecifyBoundsIfAvailable;
	result = BuildConsistencyReservation(request);
	return !result.requiresModelBounds && !result.hasReservedData && !result.unsupportedForceType;
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
		BytesEqual(result.reservedData + kConsistencyPolicyMinsOffset,
			reinterpret_cast<const std::uint8_t *>(mins), sizeof(mins)) &&
		BytesEqual(result.reservedData + kConsistencyPolicyMaxsOffset,
			reinterpret_cast<const std::uint8_t *>(maxs), sizeof(maxs));
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

	const ConsistencyReservationResult result = BuildConsistencyReservation(request);
	return result.hasReservedData &&
		result.reservedData[0] == kConsistencyForceModelSpecifyBounds &&
		BytesEqual(result.reservedData + kConsistencyPolicyMinsOffset,
			reinterpret_cast<const std::uint8_t *>(mins), sizeof(mins)) &&
		BytesEqual(result.reservedData + kConsistencyPolicyMaxsOffset,
			reinterpret_cast<const std::uint8_t *>(maxs), sizeof(maxs));
}

static bool TestChecksumComparesOnlyFirstFourBytes()
{
	std::uint8_t hash[kConsistencyPolicyHashSize] = {};
	std::uint8_t prefix[kConsistencyPolicyHashPrefixSize] = {};

	for (std::size_t i = 0; i < kConsistencyPolicyHashSize; ++i)
		hash[i] = static_cast<std::uint8_t>(0x10 + i);

	std::memcpy(prefix, hash, sizeof(prefix));
	ConsistencyEntryDecision decision = EvaluateConsistencyChecksum(7, hash, prefix, sizeof(prefix));
	if (decision.status != ConsistencyEntryStatus::Match || decision.resourceIndex != 7)
		return false;

	hash[4] ^= 0xFF;
	decision = EvaluateConsistencyChecksum(7, hash, prefix, sizeof(prefix));
	if (decision.status != ConsistencyEntryStatus::Match)
		return false;

	prefix[3] ^= 0xFF;
	decision = EvaluateConsistencyChecksum(7, hash, prefix, sizeof(prefix));
	return decision.status == ConsistencyEntryStatus::BadResource && decision.resourceIndex == 7;
}

static bool TestBoundsPolicy()
{
	const float mins[] = { -1.0f, -2.0f, -3.0f };
	const float maxs[] = { 1.0f, 2.0f, 3.0f };
	const float insideMins[] = { -0.5f, -2.0f, -1.0f };
	const float insideMaxs[] = { 0.5f, 1.5f, 3.0f };
	const float outsideMins[] = { -2.0f, -2.0f, -3.0f };
	const float outsideMaxs[] = { 1.0f, 2.0f, 3.0f };
	ConsistencyReservationRequest request = {};
	request.resourceType = ResourceType::Model;
	request.forceType = kConsistencyForceModelSpecifyBounds;
	request.specifiedMins = mins;
	request.specifiedMaxs = maxs;

	ConsistencyReservationResult reservation = BuildConsistencyReservation(request);
	ConsistencyEntryDecision decision =
		EvaluateConsistencyBounds(3, reservation.reservedData, insideMins, insideMaxs);
	if (decision.status != ConsistencyEntryStatus::Match)
		return false;

	decision = EvaluateConsistencyBounds(3, reservation.reservedData, outsideMins, insideMaxs);
	if (decision.status != ConsistencyEntryStatus::BadResource)
		return false;

	request.forceType = kConsistencyForceModelSameBounds;
	request.modelMins = mins;
	request.modelMaxs = maxs;
	request.modelBoundsAvailable = true;
	reservation = BuildConsistencyReservation(request);
	decision = EvaluateConsistencyBounds(3, reservation.reservedData, mins, maxs);
	if (decision.status != ConsistencyEntryStatus::Match)
		return false;

	decision = EvaluateConsistencyBounds(3, reservation.reservedData, insideMins, maxs);
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

int main()
{
	if (!TestSetupGates() ||
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
