#ifndef XASH_ENGINE_COMMON_NETWORK_BUFFER_ADAPTER_H
#define XASH_ENGINE_COMMON_NETWORK_BUFFER_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

int NetworkBufferAdapter_ExciseBits( void *data, int data_bits, int startbit, int bits_to_remove );

#ifdef __cplusplus
}
#endif

#endif
