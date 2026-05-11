#include "server_cvar_snapshot_adapter.h"

#include "common.h"
#include "engine/cvar_snapshot.hpp"

namespace
{

sv_readonly_cvar_snapshot_t ToLegacySnapshot(
	const xash::engine::ReadOnlyCvarSnapshot &snapshot,
	const char *stringValue)
{
	sv_readonly_cvar_snapshot_t legacy = {};
	legacy.exists = snapshot.exists ? 1 : 0;
	legacy.string_value = stringValue ? stringValue : "";
	legacy.numeric_value = static_cast<float>(snapshot.number);
	legacy.integer_value = snapshot.integer;
	legacy.boolean_value = snapshot.boolean ? 1 : 0;
	return legacy;
}

}

extern "C" sv_readonly_cvar_snapshot_t SV_ReadOnlyCvar_BuildSnapshot(
	const struct convar_s *var)
{
	if (!var)
		return ToLegacySnapshot(
			xash::engine::BuildMissingReadOnlyCvarSnapshot(),
			"");

	return ToLegacySnapshot(
		xash::engine::BuildReadOnlyCvarSnapshot(
			var->string,
			var->value,
			true),
		var->string);
}

extern "C" int SV_ReadOnlyCvar_BooleanValue(const struct convar_s *var)
{
	return SV_ReadOnlyCvar_BuildSnapshot(var).boolean_value;
}

extern "C" float SV_ReadOnlyCvar_NumericValue(const struct convar_s *var)
{
	return SV_ReadOnlyCvar_BuildSnapshot(var).numeric_value;
}

extern "C" int SV_ReadOnlyCvar_IntegerValue(const struct convar_s *var)
{
	return SV_ReadOnlyCvar_BuildSnapshot(var).integer_value;
}

extern "C" const char *SV_ReadOnlyCvar_StringValue(const struct convar_s *var)
{
	if (!var || !var->string)
		return "";

	return var->string;
}
