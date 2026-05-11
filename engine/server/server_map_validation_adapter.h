#ifndef XASH_ENGINE_SERVER_MAP_VALIDATION_ADAPTER_H
#define XASH_ENGINE_SERVER_MAP_VALIDATION_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

enum sv_map_validation_result_e
{
	SV_MAP_VALIDATION_VALID = 0,
	SV_MAP_VALIDATION_INVALID_VERSION = 1,
	SV_MAP_VALIDATION_MISSING = 2
};

typedef struct sv_changelevel_map_validation_s
{
	enum sv_map_validation_result_e result;
	int can_continue;
	int missing_landmark_for_smooth;
	int disable_smooth;
	int exists;
	int has_landmark;
	int invalid_version;
} sv_changelevel_map_validation_t;

int SV_MapValidation_MapExists(unsigned int map_flags);
int SV_MapValidation_MapHasLandmark(unsigned int map_flags);
int SV_MapValidation_MapHasInvalidVersion(unsigned int map_flags);
int SV_MapValidation_MapExistsForGameDll(unsigned int map_flags);
enum sv_map_validation_result_e SV_MapValidation_Classify(
	unsigned int map_flags);
sv_changelevel_map_validation_t SV_MapValidation_BuildChangeLevelDecision(
	unsigned int map_flags,
	int smooth_requested,
	int validate_changelevel);

#ifdef __cplusplus
}
#endif

#endif
