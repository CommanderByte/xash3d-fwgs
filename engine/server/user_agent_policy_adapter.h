#ifndef XASH_ENGINE_SERVER_USER_AGENT_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_USER_AGENT_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	SV_USER_AGENT_ACCEPTED = 0,
	SV_USER_AGENT_INVALID_AUTHENTICATION_CERTIFICATE = 1,
	SV_USER_AGENT_BANNED_ID = 2,
	SV_USER_AGENT_MISSING_INPUT_DEVICES = 3,
	SV_USER_AGENT_TOUCH_DISALLOWED = 4,
	SV_USER_AGENT_MOUSE_DISALLOWED = 5,
	SV_USER_AGENT_JOYSTICK_DISALLOWED = 6,
	SV_USER_AGENT_VR_DISALLOWED = 7,
};

typedef struct sv_user_agent_policy_s
{
	int allow_no_input_devices;
	int allow_touch;
	int allow_mouse;
	int allow_joystick;
	int allow_vr;
	int banned_id;
} sv_user_agent_policy_t;

int SV_UserAgentPolicy_UuidIsValid(const char *uuid);
int SV_UserAgentPolicy_Validate(
	const char *uuid,
	const char *input_devices,
	const sv_user_agent_policy_t *policy);
const char *SV_UserAgentPolicy_RejectionMessage(int code);

#ifdef __cplusplus
}
#endif

#endif
