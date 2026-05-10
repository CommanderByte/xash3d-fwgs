#include "server_consistency_policy_adapter.h"

#include "engine/server/server_consistency_policy.hpp"

#include <cstring>

namespace
{

xash::engine::server::ResourceType ToModernResourceType(resourcetype_t type)
{
	using xash::engine::server::ResourceType;

	switch (type)
	{
	case t_sound:
		return ResourceType::Sound;
	case t_skin:
		return ResourceType::Skin;
	case t_model:
		return ResourceType::Model;
	case t_decal:
		return ResourceType::Decal;
	case t_generic:
		return ResourceType::Generic;
	case t_eventscript:
		return ResourceType::EventScript;
	case t_world:
		return ResourceType::World;
	default:
		return ResourceType::Unknown;
	}
}

int ToLegacyStatus(xash::engine::server::ConsistencyEntryStatus status)
{
	using xash::engine::server::ConsistencyEntryStatus;

	switch (status)
	{
	case ConsistencyEntryStatus::Match:
		return SV_CONSISTENCY_ENTRY_MATCH;
	case ConsistencyEntryStatus::BadResource:
		return SV_CONSISTENCY_ENTRY_BAD_RESOURCE;
	case ConsistencyEntryStatus::InvalidType:
		return SV_CONSISTENCY_ENTRY_INVALID_TYPE;
	default:
		return SV_CONSISTENCY_ENTRY_INVALID_TYPE;
	}
}

sv_consistency_entry_decision_t ToLegacyDecision(
	const xash::engine::server::ConsistencyEntryDecision &decision)
{
	sv_consistency_entry_decision_t legacy = {};
	legacy.status = ToLegacyStatus(decision.status);
	legacy.resource_index = decision.resourceIndex;
	return legacy;
}

}

extern "C" int SV_ConsistencyPolicy_ResourceNeedsSetup(
	int already_check_file,
	int in_consistency_list)
{
	return xash::engine::server::ConsistencyResourceNeedsSetup(
		already_check_file != 0,
		in_consistency_list != 0) ? 1 : 0;
}

extern "C" int SV_ConsistencyPolicy_RequiresModelBounds(
	resourcetype_t resource_type,
	int force_type)
{
	return xash::engine::server::ConsistencyForceTypeRequiresModelBounds(
		ToModernResourceType(resource_type),
		force_type) ? 1 : 0;
}

extern "C" sv_consistency_reservation_result_t SV_ConsistencyPolicy_BuildReservation(
	resourcetype_t resource_type,
	int force_type,
	const float *specified_mins,
	const float *specified_maxs,
	const float *model_mins,
	const float *model_maxs,
	int model_bounds_available)
{
	xash::engine::server::ConsistencyReservationRequest request = {};
	request.resourceType = ToModernResourceType(resource_type);
	request.forceType = force_type;
	request.specifiedMins = specified_mins;
	request.specifiedMaxs = specified_maxs;
	request.modelMins = model_mins;
	request.modelMaxs = model_maxs;
	request.modelBoundsAvailable = model_bounds_available != 0;

	const xash::engine::server::ConsistencyReservationResult result =
		xash::engine::server::BuildConsistencyReservation(request);

	sv_consistency_reservation_result_t legacy = {};
	legacy.requires_model_bounds = result.requiresModelBounds ? 1 : 0;
	legacy.has_reserved_data = result.hasReservedData ? 1 : 0;
	legacy.unsupported_force_type = result.unsupportedForceType ? 1 : 0;
	std::memcpy(legacy.reserved_data, result.reservedData, sizeof(legacy.reserved_data));
	return legacy;
}

extern "C" int SV_ConsistencyPolicy_ReservedDataIsEmpty(const unsigned char *reserved_data)
{
	return xash::engine::server::ConsistencyReservedDataIsEmpty(reserved_data) ? 1 : 0;
}

extern "C" sv_consistency_entry_decision_t SV_ConsistencyPolicy_EvaluateChecksum(
	int resource_index,
	const unsigned char *expected_hash,
	const unsigned char *received_prefix,
	int received_prefix_size)
{
	return ToLegacyDecision(xash::engine::server::EvaluateConsistencyChecksum(
		resource_index,
		expected_hash,
		received_prefix,
		received_prefix_size < 0 ? 0U : static_cast<std::size_t>(received_prefix_size)));
}

extern "C" sv_consistency_entry_decision_t SV_ConsistencyPolicy_EvaluateBounds(
	int resource_index,
	const unsigned char *reserved_data,
	const float *client_mins,
	const float *client_maxs)
{
	return ToLegacyDecision(xash::engine::server::EvaluateConsistencyBounds(
		resource_index,
		reserved_data,
		client_mins,
		client_maxs));
}

extern "C" int SV_ConsistencyPolicy_ResponseCountMatches(
	int expected_count,
	int actual_count)
{
	return xash::engine::server::ConsistencyResponseCountMatches(
		expected_count,
		actual_count) ? 1 : 0;
}
