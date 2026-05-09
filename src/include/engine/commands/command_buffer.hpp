#ifndef XASH_ENGINE_COMMANDS_COMMAND_BUFFER_HPP
#define XASH_ENGINE_COMMANDS_COMMAND_BUFFER_HPP

#include <stddef.h>

namespace xash
{
namespace engine
{
namespace commands
{

static const size_t kCommandBufferCapacity = 32768;
static const size_t kCommandLineCapacity = 2048;

struct CommandBufferStatus
{
	CommandBufferStatus();

	bool ok;
	bool overflow;
};

struct CommandLine
{
	CommandLine();

	char text[kCommandLineCapacity];
	bool overflow;
};

class CommandBuffer
{
public:
	CommandBuffer();

	void clear();
	size_t size() const;
	bool empty() const;

	CommandBufferStatus addText(const char *text);
	CommandBufferStatus addText(const char *text, size_t length);
	CommandBufferStatus insertText(const char *text);
	CommandBufferStatus insertText(const char *text, size_t length, size_t requestedLength);
	bool popLine(CommandLine &line);

private:
	unsigned char m_data[kCommandBufferCapacity];
	size_t m_size;
};

}
}
}

#endif
