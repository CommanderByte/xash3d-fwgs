#ifndef XASH_ENGINE_SERVER_CONNECTION_RESPONSE_ADAPTER_H
#define XASH_ENGINE_SERVER_CONNECTION_RESPONSE_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

int SV_ConnectionResponse_FormatChallenge(
	char *out,
	unsigned int capacity,
	int challenge,
	int skip_bandwidth_test);

int SV_ConnectionResponse_FormatRejectReport(
	char *out,
	unsigned int capacity,
	const char *address,
	const char *reason);

int SV_ConnectionResponse_FormatRejectError(
	char *out,
	unsigned int capacity,
	const char *reason);

int SV_ConnectionResponse_FormatRejectPrint(
	char *out,
	unsigned int capacity,
	const char *reason);

int SV_ConnectionResponse_FormatRejectDisconnect(char *out, unsigned int capacity);

#ifdef __cplusplus
}
#endif

#endif
