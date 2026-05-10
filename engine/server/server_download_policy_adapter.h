#ifndef XASH_ENGINE_SERVER_DOWNLOAD_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_DOWNLOAD_POLICY_ADAPTER_H

#include "custom.h"

#ifdef __cplusplus
extern "C" {
#endif

enum sv_download_policy_action_e
{
	SV_DOWNLOAD_POLICY_IGNORE = 0,
	SV_DOWNLOAD_POLICY_REJECT,
	SV_DOWNLOAD_POLICY_SEND_FILE,
	SV_DOWNLOAD_POLICY_SEND_FILE_WITH_MODEL_TEXTURE,
	SV_DOWNLOAD_POLICY_LOOKUP_CUSTOM_LOGO,
};

typedef struct sv_download_policy_decision_s
{
	enum sv_download_policy_action_e action;
	int resource_index;
	const char *file_name;
	const char *model_texture_name;
	unsigned char custom_hash[16];
} sv_download_policy_decision_t;

int SV_ServerDownloadPolicy_NeedsModelTextureProbe(
	const char *requested_name,
	int allow_download,
	int send_resources,
	const resource_t *resources,
	int resource_count);

sv_download_policy_decision_t SV_ServerDownloadPolicy_Decide(
	const char *requested_name,
	int allow_download,
	int send_resources,
	int send_logos,
	const resource_t *resources,
	int resource_count,
	const char *model_texture_name,
	int model_texture_available);

#ifdef __cplusplus
}
#endif

#endif
