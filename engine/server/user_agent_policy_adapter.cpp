#include "user_agent_policy_adapter.h"

#include "engine/server/client/user_agent_policy.hpp"

namespace
{

xash::engine::server::UserAgentValidationCode ToCode(int code)
{
	switch (code)
	{
	case SV_USER_AGENT_ACCEPTED:
		return xash::engine::server::UserAgentValidationCode::Accepted;
	case SV_USER_AGENT_INVALID_AUTHENTICATION_CERTIFICATE:
		return xash::engine::server::UserAgentValidationCode::InvalidAuthenticationCertificate;
	case SV_USER_AGENT_BANNED_ID:
		return xash::engine::server::UserAgentValidationCode::BannedId;
	case SV_USER_AGENT_MISSING_INPUT_DEVICES:
		return xash::engine::server::UserAgentValidationCode::MissingInputDevices;
	case SV_USER_AGENT_TOUCH_DISALLOWED:
		return xash::engine::server::UserAgentValidationCode::TouchDisallowed;
	case SV_USER_AGENT_MOUSE_DISALLOWED:
		return xash::engine::server::UserAgentValidationCode::MouseDisallowed;
	case SV_USER_AGENT_JOYSTICK_DISALLOWED:
		return xash::engine::server::UserAgentValidationCode::JoystickDisallowed;
	case SV_USER_AGENT_VR_DISALLOWED:
		return xash::engine::server::UserAgentValidationCode::VrDisallowed;
	default:
		return xash::engine::server::UserAgentValidationCode::InvalidAuthenticationCertificate;
	}
}

}

extern "C" int SV_UserAgentPolicy_UuidIsValid(const char *uuid)
{
	return xash::engine::server::UserAgentUuidIsValid(uuid);
}

extern "C" int SV_UserAgentPolicy_Validate(
	const char *uuid,
	const char *input_devices,
	const sv_user_agent_policy_t *policy)
{
	if (!policy)
		return SV_USER_AGENT_INVALID_AUTHENTICATION_CERTIFICATE;

	xash::engine::server::UserAgentPolicy modern;
	modern.allowNoInputDevices = policy->allow_no_input_devices != 0;
	modern.allowTouch = policy->allow_touch != 0;
	modern.allowMouse = policy->allow_mouse != 0;
	modern.allowJoystick = policy->allow_joystick != 0;
	modern.allowVr = policy->allow_vr != 0;
	modern.bannedId = policy->banned_id != 0;

	return static_cast<int>(xash::engine::server::ValidateUserAgent(
		uuid,
		input_devices,
		modern));
}

extern "C" const char *SV_UserAgentPolicy_RejectionMessage(int code)
{
	return xash::engine::server::UserAgentRejectionMessage(ToCode(code));
}
