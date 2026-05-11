#include "engine/server/client/connection_response.hpp"

#include <cstdarg>
#include <cstdio>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

const char *SafeString(const char *value)
{
	if (!value)
		return "";

	return value;
}

bool Format(char *out, std::size_t capacity, const char *format, ...)
{
	if (!out || capacity == 0)
		return false;

	va_list args;
	va_start(args, format);
	const int written = std::vsnprintf(out, capacity, format, args);
	va_end(args);

	out[capacity - 1] = '\0';
	return written >= 0;
}

}

bool FormatChallengeResponse(
	char *out,
	std::size_t capacity,
	int challenge,
	bool skipBandwidthTest)
{
	return Format(out, capacity, "challenge %i %i", challenge, skipBandwidthTest ? 0 : 1);
}

bool FormatRejectReportMessage(
	char *out,
	std::size_t capacity,
	const char *address,
	const char *reason)
{
	return Format(
		out,
		capacity,
		"%s connection refused. Reason: %s\n",
		SafeString(address),
		SafeString(reason));
}

bool FormatRejectErrorMessage(
	char *out,
	std::size_t capacity,
	const char *reason)
{
	return Format(
		out,
		capacity,
		"errormsg\n^1Server was reject the connection:^7 %s",
		SafeString(reason));
}

bool FormatRejectPrintMessage(
	char *out,
	std::size_t capacity,
	const char *reason)
{
	return Format(
		out,
		capacity,
		"print\n^1Server was reject the connection:^7 %s",
		SafeString(reason));
}

bool FormatRejectDisconnectMessage(char *out, std::size_t capacity)
{
	return Format(out, capacity, "disconnect\n");
}

}
}
}
