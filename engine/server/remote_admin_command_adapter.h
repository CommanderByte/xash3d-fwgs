#ifndef XASH_ENGINE_SERVER_REMOTE_ADMIN_COMMAND_ADAPTER_H
#define XASH_ENGINE_SERVER_REMOTE_ADMIN_COMMAND_ADAPTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SV_REMOTE_ADMIN_AUTH_IGNORE_DISABLED 0
#define SV_REMOTE_ADMIN_AUTH_REJECT_BAD_PASSWORD 1
#define SV_REMOTE_ADMIN_AUTH_ACCEPT 2

int SV_RemoteAdmin_BuildAuthAction(
	int enabled,
	const char *configured_password,
	const char *supplied_password);

size_t SV_RemoteAdmin_ResetCommand(char *buffer, size_t size);

size_t SV_RemoteAdmin_AppendCommandArgument(
	char *buffer,
	size_t size,
	size_t used,
	const char *argument);

#ifdef __cplusplus
}
#endif

#endif
