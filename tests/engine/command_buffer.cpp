#include <stdlib.h>
#include <string.h>

#include <string>

#include "engine/commands/command_buffer.hpp"

using namespace xash::engine::commands;

static bool ExpectPop(CommandBuffer &buffer, const char *expected, bool overflow = false)
{
	CommandLine line;

	if (!buffer.popLine(line))
		return false;

	if (line.overflow != overflow)
		return false;

	return strcmp(line.text, expected) == 0;
}

static bool TestBasicSplitting()
{
	CommandBuffer buffer;

	if (!buffer.addText("one;two\nthree\rfour").ok)
		return false;

	return ExpectPop(buffer, "one") &&
		ExpectPop(buffer, "two") &&
		ExpectPop(buffer, "three") &&
		ExpectPop(buffer, "four") &&
		buffer.empty();
}

static bool TestQuotesEscapesAndComments()
{
	CommandBuffer buffer;

	if (!buffer.addText("say \"a;b\";next\n").ok)
		return false;
	if (!buffer.addText(R"(quote "a\";b";after)" "\n").ok)
		return false;
	if (!buffer.addText("cmd // ; ignored\n").ok)
		return false;
	if (!buffer.addText("path//still;done\n").ok)
		return false;

	return ExpectPop(buffer, "say \"a;b\"") &&
		ExpectPop(buffer, "next") &&
		ExpectPop(buffer, R"(quote "a\";b")") &&
		ExpectPop(buffer, "after") &&
		ExpectPop(buffer, "cmd ") &&
		ExpectPop(buffer, "path//still") &&
		ExpectPop(buffer, "done") &&
		buffer.empty();
}

static bool TestInsertFront()
{
	CommandBuffer buffer;

	if (!buffer.addText("three\n").ok)
		return false;
	if (!buffer.insertText("one\ntwo\n").ok)
		return false;

	return ExpectPop(buffer, "one") &&
		ExpectPop(buffer, "two") &&
		ExpectPop(buffer, "three") &&
		buffer.empty();
}

static bool TestOverflowRejection()
{
	CommandBuffer buffer;
	const std::string payload(kCommandBufferCapacity - 2, 'x');

	if (!buffer.addText(payload.c_str()).ok)
		return false;

	const size_t sizeBefore = buffer.size();
	const CommandBufferStatus status = buffer.insertText("y", 1, 2);

	return !status.ok &&
		status.overflow &&
		buffer.size() == sizeBefore;
}

static bool TestLineOverflow()
{
	CommandBuffer buffer;
	const std::string longLine(kCommandLineCapacity - 1, 'x');

	if (!buffer.addText(longLine.c_str()).ok)
		return false;
	if (!buffer.addText("\nnext\n").ok)
		return false;

	return ExpectPop(buffer, "", true) &&
		ExpectPop(buffer, "next") &&
		buffer.empty();
}

int main()
{
	if (!TestBasicSplitting() ||
		!TestQuotesEscapesAndComments() ||
		!TestInsertFront() ||
		!TestOverflowRejection() ||
		!TestLineOverflow())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
