#include <stdlib.h>

#include "engine/console/system_console_message.hpp"

using namespace xash::engine::console;

static SystemConsoleMessageInput Message(SystemConsoleMessageKind kind, const char *text)
{
	SystemConsoleMessageInput input;
	input.kind = kind;
	input.formattedText = text;
	return input;
}

static bool TestDefaultContextBlocksNormalOutput()
{
	const SystemConsoleMessageResult result =
		PrepareSystemConsoleMessage(Message(SystemConsoleMessageKind::Normal, "hello\n"));

	return !result.emit &&
		result.reason == SystemConsoleFilterReason::ConsoleDisabled &&
		result.text.empty();
}

static bool TestNormalOutputRequiresAllowConsole()
{
	SystemConsoleMessageInput input = Message(SystemConsoleMessageKind::Normal, "visible\n");
	input.context.allowConsole = true;

	const SystemConsoleMessageResult result = PrepareSystemConsoleMessage(input);

	return result.emit &&
		result.reason == SystemConsoleFilterReason::Accepted &&
		result.text == "visible\n";
}

static bool TestDeveloperOutputIgnoresAllowConsoleButRequiresDeveloperLevel()
{
	SystemConsoleMessageInput input = Message(SystemConsoleMessageKind::Developer, "debug\n");

	SystemConsoleMessageResult result = PrepareSystemConsoleMessage(input);
	if (result.emit || result.reason != SystemConsoleFilterReason::DeveloperDisabled)
		return false;

	input.context.developerLevel = 1;
	result = PrepareSystemConsoleMessage(input);

	return result.emit &&
		result.reason == SystemConsoleFilterReason::Accepted &&
		result.text == "debug\n";
}

static bool TestReportOutputRequiresExtendedDeveloperLevel()
{
	SystemConsoleMessageInput input = Message(SystemConsoleMessageKind::Report, "report\n");
	input.context.developerLevel = 1;

	SystemConsoleMessageResult result = PrepareSystemConsoleMessage(input);
	if (result.emit || result.reason != SystemConsoleFilterReason::ReportDisabled)
		return false;

	input.context.developerLevel = 2;
	result = PrepareSystemConsoleMessage(input);

	return result.emit &&
		result.reason == SystemConsoleFilterReason::Accepted &&
		result.text == "report\n";
}

static bool TestDeveloperSpamSuppression()
{
	SystemConsoleMessageInput input = Message(SystemConsoleMessageKind::Developer, "0\n");
	input.context.developerLevel = 1;

	const SystemConsoleMessageResult result = PrepareSystemConsoleMessage(input);

	return !result.emit &&
		result.reason == SystemConsoleFilterReason::SuppressedDebugSpam &&
		result.text.empty();
}

static bool TestFormattingTruncationAppendsCompatibilityNewline()
{
	SystemConsoleMessageInput input = Message(SystemConsoleMessageKind::Normal, "partial");
	input.context.allowConsole = true;
	input.formattingTruncated = true;

	const SystemConsoleMessageResult result = PrepareSystemConsoleMessage(input);

	return result.emit &&
		result.reason == SystemConsoleFilterReason::Accepted &&
		result.text == "partial\n";
}

int main()
{
	if (!TestDefaultContextBlocksNormalOutput() ||
		!TestNormalOutputRequiresAllowConsole() ||
		!TestDeveloperOutputIgnoresAllowConsoleButRequiresDeveloperLevel() ||
		!TestReportOutputRequiresExtendedDeveloperLevel() ||
		!TestDeveloperSpamSuppression() ||
		!TestFormattingTruncationAppendsCompatibilityNewline())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
