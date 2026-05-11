#ifndef XASH_ENGINE_SERVER_CONNECTION_RESPONSE_HPP
#define XASH_ENGINE_SERVER_CONNECTION_RESPONSE_HPP

#include <cstddef>

namespace xash
{
namespace engine
{
namespace server
{

bool FormatChallengeResponse(
	char *out,
	std::size_t capacity,
	int challenge,
	bool skipBandwidthTest);

bool FormatRejectReportMessage(
	char *out,
	std::size_t capacity,
	const char *address,
	const char *reason);

bool FormatRejectErrorMessage(
	char *out,
	std::size_t capacity,
	const char *reason);

bool FormatRejectPrintMessage(
	char *out,
	std::size_t capacity,
	const char *reason);

bool FormatRejectDisconnectMessage(char *out, std::size_t capacity);

}
}
}

#endif
