/*
network_buffer_adapter.cpp - private bridge from legacy MSG_* C API to C++ bits
*/

#include "network_buffer_adapter.h"

#include "engine/network/network_buffer.hpp"

extern "C" int NetworkBufferAdapter_ExciseBits( void *data, int data_bits, int startbit, int bits_to_remove )
{
	if( !data || data_bits < 0 || startbit < 0 || bits_to_remove < 0 )
		return -1;

	xash::engine::network::NetworkBitBuffer buffer(
		data,
		static_cast<size_t>(data_bits));

	if( !buffer.exciseBits(
		static_cast<size_t>(startbit),
		static_cast<size_t>(bits_to_remove)))
	{
		return -1;
	}

	return static_cast<int>(buffer.maxBits());
}
