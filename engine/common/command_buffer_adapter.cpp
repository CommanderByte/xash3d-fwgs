/*
command_buffer_adapter.cpp - private bridge from legacy Cbuf C API to C++ buffer
*/

#include "command_buffer_adapter.h"

#include "engine/commands/command_buffer.hpp"

#include <algorithm>
#include <cstring>

namespace
{

xash::engine::commands::CommandBuffer g_privilegedBuffer;
xash::engine::commands::CommandBuffer g_filteredBuffer;

xash::engine::commands::CommandBuffer &SelectBuffer( cbuf_adapter_buffer_t buffer )
{
	if( buffer == CBUF_ADAPTER_FILTERED )
		return g_filteredBuffer;

	return g_privilegedBuffer;
}

cbuf_adapter_status_t ToAdapterStatus( const xash::engine::commands::CommandBufferStatus &status )
{
	cbuf_adapter_status_t adapterStatus;
	adapterStatus.ok = status.ok ? 1 : 0;
	adapterStatus.overflow = status.overflow ? 1 : 0;
	return adapterStatus;
}

}

extern "C" void CommandBufferAdapter_ClearAll( void )
{
	g_privilegedBuffer.clear();
	g_filteredBuffer.clear();
}

extern "C" size_t CommandBufferAdapter_Size( cbuf_adapter_buffer_t buffer )
{
	return SelectBuffer( buffer ).size();
}

extern "C" cbuf_adapter_status_t CommandBufferAdapter_AddText( cbuf_adapter_buffer_t buffer, const char *text )
{
	return ToAdapterStatus( SelectBuffer( buffer ).addText( text ));
}

extern "C" cbuf_adapter_status_t CommandBufferAdapter_InsertText( cbuf_adapter_buffer_t buffer, const char *text, size_t length, size_t requested_length )
{
	return ToAdapterStatus( SelectBuffer( buffer ).insertText( text, length, requested_length ));
}

extern "C" int CommandBufferAdapter_PopLine( cbuf_adapter_buffer_t buffer, char *line, size_t line_size, int *overflow )
{
	xash::engine::commands::CommandLine commandLine;

	if( !SelectBuffer( buffer ).popLine( commandLine ))
		return 0;

	if( line && line_size > 0 )
	{
		const size_t textLength = std::strlen( commandLine.text );
		const size_t copyLength = std::min( textLength, line_size - 1 );

		std::memcpy( line, commandLine.text, copyLength );
		line[copyLength] = '\0';
	}

	if( overflow )
		*overflow = commandLine.overflow ? 1 : 0;

	return 1;
}
