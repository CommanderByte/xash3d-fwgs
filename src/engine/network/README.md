# Engine Network

Reserved for future network buffer, channel, socket, and protocol helpers.

This area is intentionally late-stage until protocol and timing behavior have
targeted tests.

Current helper:

- `NetworkBitBuffer`: private modern primitive for bit-level message storage,
  golden-vector tests, and the first `MSG_ExciseBits` compatibility route.
