#include <cstdlib>
#include <cstring>

#include "engine/server/user_agent_policy.hpp"

using namespace xash::engine::server;

static const char *ValidUuid()
{
	return "0123456789abcdef0123456789abcdef";
}

static UserAgentPolicy AllowAllPolicy()
{
	UserAgentPolicy policy = {};
	policy.allowNoInputDevices = true;
	policy.allowTouch = true;
	policy.allowMouse = true;
	policy.allowJoystick = true;
	policy.allowVr = true;
	return policy;
}

static bool TestUuidValidation()
{
	return UserAgentUuidIsValid(ValidUuid()) &&
		!UserAgentUuidIsValid(nullptr) &&
		!UserAgentUuidIsValid("") &&
		!UserAgentUuidIsValid("0123456789abcdef0123456789abcde") &&
		!UserAgentUuidIsValid("0123456789abcdef0123456789abcdef0") &&
		!UserAgentUuidIsValid("0123456789abcdef0123456789abcdeg") &&
		!UserAgentUuidIsValid("0123456789ABCDEF0123456789abcdef");
}

static bool TestAcceptedAndBanned()
{
	UserAgentPolicy policy = AllowAllPolicy();

	if (ValidateUserAgent(ValidUuid(), nullptr, policy) != UserAgentValidationCode::Accepted)
		return false;

	policy.bannedId = true;
	return ValidateUserAgent(ValidUuid(), "0", policy) == UserAgentValidationCode::BannedId;
}

static bool TestMissingInputDevices()
{
	UserAgentPolicy policy = AllowAllPolicy();
	policy.allowNoInputDevices = false;

	if (ValidateUserAgent(ValidUuid(), nullptr, policy) !=
		UserAgentValidationCode::MissingInputDevices)
		return false;

	if (ValidateUserAgent(ValidUuid(), "", policy) !=
		UserAgentValidationCode::MissingInputDevices)
		return false;

	return ValidateUserAgent(ValidUuid(), "0", policy) == UserAgentValidationCode::Accepted;
}

static bool TestInputDeviceRejectionOrder()
{
	UserAgentPolicy policy = AllowAllPolicy();

	policy.allowTouch = false;
	policy.allowMouse = false;
	if (ValidateUserAgent(ValidUuid(), "3", policy) != UserAgentValidationCode::TouchDisallowed)
		return false;

	policy = AllowAllPolicy();
	policy.allowMouse = false;
	if (ValidateUserAgent(ValidUuid(), "1", policy) != UserAgentValidationCode::MouseDisallowed)
		return false;

	policy = AllowAllPolicy();
	policy.allowJoystick = false;
	if (ValidateUserAgent(ValidUuid(), "4", policy) != UserAgentValidationCode::JoystickDisallowed)
		return false;

	policy = AllowAllPolicy();
	policy.allowVr = false;
	if (ValidateUserAgent(ValidUuid(), "8", policy) != UserAgentValidationCode::VrDisallowed)
		return false;

	policy = AllowAllPolicy();
	policy.allowTouch = false;
	return ValidateUserAgent(ValidUuid(), "0x2", policy) ==
		UserAgentValidationCode::TouchDisallowed;
}

static bool TestMessages()
{
	return std::strcmp(
			UserAgentRejectionMessage(UserAgentValidationCode::InvalidAuthenticationCertificate),
			"invalid authentication certificate\n") == 0 &&
		std::strcmp(
			UserAgentRejectionMessage(UserAgentValidationCode::BannedId),
			"You are banned!\n") == 0 &&
		std::strcmp(
			UserAgentRejectionMessage(UserAgentValidationCode::MissingInputDevices),
			"This server does not allow\nconnect without input devices list.\nPlease update your engine.\n") == 0 &&
		std::strcmp(
			UserAgentRejectionMessage(UserAgentValidationCode::TouchDisallowed),
			"This server does not allow touch\nDisable it (touch_enable 0)\nto play on this server\n") == 0 &&
		std::strcmp(
			UserAgentRejectionMessage(UserAgentValidationCode::MouseDisallowed),
			"This server does not allow mouse\nDisable it(m_ignore 1)\nto play on this server\n") == 0 &&
		std::strcmp(
			UserAgentRejectionMessage(UserAgentValidationCode::JoystickDisallowed),
			"This server does not allow joystick\nDisable it(joy_enable 0)\nto play on this server\n") == 0 &&
		std::strcmp(
			UserAgentRejectionMessage(UserAgentValidationCode::VrDisallowed),
			"This server does not allow VR\n") == 0 &&
		std::strcmp(
			UserAgentRejectionMessage(UserAgentValidationCode::Accepted),
			"") == 0;
}

int main()
{
	if (!TestUuidValidation() ||
		!TestAcceptedAndBanned() ||
		!TestMissingInputDevices() ||
		!TestInputDeviceRejectionOrder() ||
		!TestMessages())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
