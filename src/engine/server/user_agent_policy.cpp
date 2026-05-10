#include "engine/server/user_agent_policy.hpp"

#include "utilities/conversion.hpp"

#include <stddef.h>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool StringEmpty(const char *value)
{
	return !value || value[0] == '\0';
}

bool IsLowercaseHex(char value)
{
	return (value >= '0' && value <= '9') ||
		(value >= 'a' && value <= 'f');
}

bool HasInputDevice(const char *inputDevices, int device)
{
	if (!inputDevices)
		return false;

	return (xash::utilities::LegacyAtoi(inputDevices) & device) != 0;
}

}

bool UserAgentUuidIsValid(const char *uuid)
{
	if (!uuid)
		return false;

	for (size_t i = 0; i < 32; ++i)
	{
		if (!uuid[i] || !IsLowercaseHex(uuid[i]))
			return false;
	}

	return uuid[32] == '\0';
}

UserAgentValidationCode ValidateUserAgent(
	const char *uuid,
	const char *inputDevices,
	const UserAgentPolicy &policy)
{
	if (!UserAgentUuidIsValid(uuid))
		return UserAgentValidationCode::InvalidAuthenticationCertificate;

	if (policy.bannedId)
		return UserAgentValidationCode::BannedId;

	if (!policy.allowNoInputDevices && StringEmpty(inputDevices))
		return UserAgentValidationCode::MissingInputDevices;

	if (inputDevices)
	{
		if (!policy.allowTouch && HasInputDevice(inputDevices, kUserAgentInputTouch))
			return UserAgentValidationCode::TouchDisallowed;

		if (!policy.allowMouse && HasInputDevice(inputDevices, kUserAgentInputMouse))
			return UserAgentValidationCode::MouseDisallowed;

		if (!policy.allowJoystick && HasInputDevice(inputDevices, kUserAgentInputJoystick))
			return UserAgentValidationCode::JoystickDisallowed;

		if (!policy.allowVr && HasInputDevice(inputDevices, kUserAgentInputVr))
			return UserAgentValidationCode::VrDisallowed;
	}

	return UserAgentValidationCode::Accepted;
}

const char *UserAgentRejectionMessage(UserAgentValidationCode code)
{
	switch (code)
	{
	case UserAgentValidationCode::InvalidAuthenticationCertificate:
		return "invalid authentication certificate\n";
	case UserAgentValidationCode::BannedId:
		return "You are banned!\n";
	case UserAgentValidationCode::MissingInputDevices:
		return "This server does not allow\nconnect without input devices list.\nPlease update your engine.\n";
	case UserAgentValidationCode::TouchDisallowed:
		return "This server does not allow touch\nDisable it (touch_enable 0)\nto play on this server\n";
	case UserAgentValidationCode::MouseDisallowed:
		return "This server does not allow mouse\nDisable it(m_ignore 1)\nto play on this server\n";
	case UserAgentValidationCode::JoystickDisallowed:
		return "This server does not allow joystick\nDisable it(joy_enable 0)\nto play on this server\n";
	case UserAgentValidationCode::VrDisallowed:
		return "This server does not allow VR\n";
	case UserAgentValidationCode::Accepted:
	default:
		return "";
	}
}

}
}
}
