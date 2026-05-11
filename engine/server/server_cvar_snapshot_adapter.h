#ifndef XASH_ENGINE_SERVER_CVAR_SNAPSHOT_ADAPTER_H
#define XASH_ENGINE_SERVER_CVAR_SNAPSHOT_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_readonly_cvar_snapshot_s
{
	int exists;
	const char *string_value;
	float numeric_value;
	int integer_value;
	int boolean_value;
} sv_readonly_cvar_snapshot_t;

sv_readonly_cvar_snapshot_t SV_ReadOnlyCvar_BuildSnapshot(
	const struct convar_s *var);
int SV_ReadOnlyCvar_BooleanValue(const struct convar_s *var);
float SV_ReadOnlyCvar_NumericValue(const struct convar_s *var);
int SV_ReadOnlyCvar_IntegerValue(const struct convar_s *var);
const char *SV_ReadOnlyCvar_StringValue(const struct convar_s *var);

#ifdef __cplusplus
}
#endif

#endif
