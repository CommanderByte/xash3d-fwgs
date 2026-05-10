#ifndef XASH_ENGINE_SERVER_USER_AGENT_POLICY_HPP
#define XASH_ENGINE_SERVER_USER_AGENT_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kUserAgentInputMouse = 1 << 0;
constexpr int kUserAgentInputTouch = 1 << 1;
constexpr int kUserAgentInputJoystick = 1 << 2;
constexpr int kUserAgentInputVr = 1 << 3;

enum class UserAgentValidationCode
{
	Accepted = 0,
	InvalidAuthenticationCertificate = 1,
	BannedId = 2,
	MissingInputDevices = 3,
	TouchDisallowed = 4,
	MouseDisallowed = 5,
	JoystickDisallowed = 6,
	VrDisallowed = 7,
};

struct UserAgentPolicy
{
	bool allowNoInputDevices;
	bool allowTouch;
	bool allowMouse;
	bool allowJoystick;
	bool allowVr;
	bool bannedId;
};

bool UserAgentUuidIsValid(const char *uuid);
UserAgentValidationCode ValidateUserAgent(
	const char *uuid,
	const char *inputDevices,
	const UserAgentPolicy &policy);
const char *UserAgentRejectionMessage(UserAgentValidationCode code);

}
}
}

#endif
