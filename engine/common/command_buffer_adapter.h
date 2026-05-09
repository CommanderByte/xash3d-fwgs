#ifndef XASH_ENGINE_COMMON_COMMAND_BUFFER_ADAPTER_H
#define XASH_ENGINE_COMMON_COMMAND_BUFFER_ADAPTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	CBUF_ADAPTER_PRIVILEGED = 0,
	CBUF_ADAPTER_FILTERED = 1,
} cbuf_adapter_buffer_t;

typedef struct
{
	int ok;
	int overflow;
} cbuf_adapter_status_t;

void CommandBufferAdapter_ClearAll( void );
size_t CommandBufferAdapter_Size( cbuf_adapter_buffer_t buffer );
cbuf_adapter_status_t CommandBufferAdapter_AddText( cbuf_adapter_buffer_t buffer, const char *text );
cbuf_adapter_status_t CommandBufferAdapter_InsertText( cbuf_adapter_buffer_t buffer, const char *text, size_t length, size_t requested_length );
int CommandBufferAdapter_PopLine( cbuf_adapter_buffer_t buffer, char *line, size_t line_size, int *overflow );

#ifdef __cplusplus
}
#endif

#endif
