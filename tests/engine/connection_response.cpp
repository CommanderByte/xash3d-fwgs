#include <cstdlib>
#include <cstring>

#include "engine/server/connection_response.hpp"

using xash::engine::server::FormatChallengeResponse;
using xash::engine::server::FormatRejectDisconnectMessage;
using xash::engine::server::FormatRejectErrorMessage;
using xash::engine::server::FormatRejectPrintMessage;
using xash::engine::server::FormatRejectReportMessage;

static bool Equals(const char *lhs, const char *rhs)
{
	return std::strcmp(lhs, rhs) == 0;
}

static bool TestChallengeResponse()
{
	char out[64] = {};

	if (!FormatChallengeResponse(out, sizeof(out), 123456, true) ||
		!Equals(out, "challenge 123456 0"))
	{
		return false;
	}

	return FormatChallengeResponse(out, sizeof(out), -42, false) &&
		Equals(out, "challenge -42 1");
}

static bool TestRejectPacketMessages()
{
	char out[256] = {};

	if (!FormatRejectErrorMessage(out, sizeof(out), "invalid password\n") ||
		!Equals(out, "errormsg\n^1Server was reject the connection:^7 invalid password\n"))
	{
		return false;
	}

	if (!FormatRejectPrintMessage(out, sizeof(out), "invalid password\n") ||
		!Equals(out, "print\n^1Server was reject the connection:^7 invalid password\n"))
	{
		return false;
	}

	return FormatRejectDisconnectMessage(out, sizeof(out)) &&
		Equals(out, "disconnect\n");
}

static bool TestRejectReportMessage()
{
	char out[256] = {};

	return FormatRejectReportMessage(out, sizeof(out), "127.0.0.1:27015", "server is full\n") &&
		Equals(out, "127.0.0.1:27015 connection refused. Reason: server is full\n\n");
}

static bool TestNullsBecomeEmptyStrings()
{
	char out[128] = {};

	return FormatRejectReportMessage(out, sizeof(out), nullptr, nullptr) &&
		Equals(out, " connection refused. Reason: \n") &&
		FormatRejectErrorMessage(out, sizeof(out), nullptr) &&
		Equals(out, "errormsg\n^1Server was reject the connection:^7 ");
}

static bool TestTruncationStillTerminates()
{
	char out[12] = {};

	if (!FormatChallengeResponse(out, sizeof(out), 123456789, false))
		return false;

	return out[sizeof(out) - 1] == '\0' &&
		std::strncmp(out, "challenge ", 10) == 0;
}

int main()
{
	if (!TestChallengeResponse() ||
		!TestRejectPacketMessages() ||
		!TestRejectReportMessage() ||
		!TestNullsBecomeEmptyStrings() ||
		!TestTruncationStillTerminates())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
