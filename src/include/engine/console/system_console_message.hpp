#ifndef XASH_ENGINE_CONSOLE_SYSTEM_CONSOLE_MESSAGE_HPP
#define XASH_ENGINE_CONSOLE_SYSTEM_CONSOLE_MESSAGE_HPP

#include <string>

namespace xash
{
namespace engine
{
namespace console
{

enum class SystemConsoleMessageKind
{
	Normal,
	Developer,
	Report,
};

enum class SystemConsoleFilterReason
{
	Accepted,
	ConsoleDisabled,
	DeveloperDisabled,
	ReportDisabled,
	SuppressedDebugSpam,
};

struct SystemConsoleMessageContext
{
	SystemConsoleMessageContext();

	bool allowConsole;
	int developerLevel;
};

struct SystemConsoleMessageInput
{
	SystemConsoleMessageInput();

	SystemConsoleMessageKind kind;
	SystemConsoleMessageContext context;
	const char *formattedText;
	bool formattingTruncated;
};

struct SystemConsoleMessageResult
{
	SystemConsoleMessageResult();

	bool emit;
	SystemConsoleFilterReason reason;
	std::string text;
};

SystemConsoleMessageResult PrepareSystemConsoleMessage(const SystemConsoleMessageInput &input);

}
}
}

#endif
