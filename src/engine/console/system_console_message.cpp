#include "engine/console/system_console_message.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace console
{

namespace
{

const int kDeveloperNormal = 1;
const int kDeveloperExtended = 2;

bool IsSuppressedDebugSpam(const char *text)
{
	return text && std::strcmp(text, "0\n") == 0;
}

SystemConsoleFilterReason FilterMessage(const SystemConsoleMessageInput &input)
{
	switch (input.kind)
	{
	case SystemConsoleMessageKind::Normal:
		return input.context.allowConsole ?
			SystemConsoleFilterReason::Accepted :
			SystemConsoleFilterReason::ConsoleDisabled;
	case SystemConsoleMessageKind::Developer:
		if (input.context.developerLevel < kDeveloperNormal)
			return SystemConsoleFilterReason::DeveloperDisabled;
		if (IsSuppressedDebugSpam(input.formattedText))
			return SystemConsoleFilterReason::SuppressedDebugSpam;
		return SystemConsoleFilterReason::Accepted;
	case SystemConsoleMessageKind::Report:
		return input.context.developerLevel >= kDeveloperExtended ?
			SystemConsoleFilterReason::Accepted :
			SystemConsoleFilterReason::ReportDisabled;
	}

	return SystemConsoleFilterReason::DeveloperDisabled;
}

}

SystemConsoleMessageContext::SystemConsoleMessageContext()
	: allowConsole(false)
	, developerLevel(0)
{
}

SystemConsoleMessageInput::SystemConsoleMessageInput()
	: kind(SystemConsoleMessageKind::Normal)
	, context()
	, formattedText("")
	, formattingTruncated(false)
{
}

SystemConsoleMessageResult::SystemConsoleMessageResult()
	: emit(false)
	, reason(SystemConsoleFilterReason::ConsoleDisabled)
	, text()
{
}

SystemConsoleMessageResult PrepareSystemConsoleMessage(const SystemConsoleMessageInput &input)
{
	SystemConsoleMessageResult result;

	result.reason = FilterMessage(input);
	if (result.reason != SystemConsoleFilterReason::Accepted)
		return result;

	result.emit = true;
	if (input.formattedText)
		result.text = input.formattedText;

	if (input.formattingTruncated)
		result.text += '\n';

	return result;
}

}
}
}
