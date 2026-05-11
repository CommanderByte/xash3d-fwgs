#include "connection_response_adapter.h"

#include "client_adapter_shared.hpp"
#include "engine/server/client/connection_response.hpp"

using xash::engine::server::adapter::client::FromLegacyBool;
using xash::engine::server::adapter::client::ToLegacyBool;

extern "C" int SV_ConnectionResponse_FormatChallenge(
	char *out,
	unsigned int capacity,
	int challenge,
	int skip_bandwidth_test)
{
	return ToLegacyBool(xash::engine::server::FormatChallengeResponse(
		out,
		capacity,
		challenge,
		FromLegacyBool(skip_bandwidth_test)));
}

extern "C" int SV_ConnectionResponse_FormatRejectReport(
	char *out,
	unsigned int capacity,
	const char *address,
	const char *reason)
{
	return ToLegacyBool(xash::engine::server::FormatRejectReportMessage(
		out,
		capacity,
		address,
		reason));
}

extern "C" int SV_ConnectionResponse_FormatRejectError(
	char *out,
	unsigned int capacity,
	const char *reason)
{
	return ToLegacyBool(xash::engine::server::FormatRejectErrorMessage(
		out,
		capacity,
		reason));
}

extern "C" int SV_ConnectionResponse_FormatRejectPrint(
	char *out,
	unsigned int capacity,
	const char *reason)
{
	return ToLegacyBool(xash::engine::server::FormatRejectPrintMessage(
		out,
		capacity,
		reason));
}

extern "C" int SV_ConnectionResponse_FormatRejectDisconnect(
	char *out,
	unsigned int capacity)
{
	return ToLegacyBool(xash::engine::server::FormatRejectDisconnectMessage(out, capacity));
}
