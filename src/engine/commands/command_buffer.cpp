#include "engine/commands/command_buffer.hpp"

#include <algorithm>
#include <cstring>

namespace xash
{
namespace engine
{
namespace commands
{

CommandBufferStatus::CommandBufferStatus()
	: ok(true)
	, overflow(false)
{
}

CommandLine::CommandLine()
	: overflow(false)
{
	text[0] = '\0';
}

CommandBuffer::CommandBuffer()
	: m_size(0)
{
	clear();
}

void CommandBuffer::clear()
{
	std::memset(m_data, 0, sizeof(m_data));
	m_size = 0;
}

size_t CommandBuffer::size() const
{
	return m_size;
}

bool CommandBuffer::empty() const
{
	return m_size == 0;
}

CommandBufferStatus CommandBuffer::addText(const char *text)
{
	return addText(text, text ? std::strlen(text) : 0);
}

CommandBufferStatus CommandBuffer::addText(const char *text, size_t length)
{
	CommandBufferStatus status;

	if (length == 0)
		return status;

	if (!text || length >= kCommandBufferCapacity - m_size)
	{
		status.ok = false;
		status.overflow = true;
		return status;
	}

	std::memcpy(m_data + m_size, text, length);
	m_size += length;
	return status;
}

CommandBufferStatus CommandBuffer::insertText(const char *text)
{
	const size_t length = text ? std::strlen(text) : 0;
	return insertText(text, length, length);
}

CommandBufferStatus CommandBuffer::insertText(const char *text, size_t length, size_t requestedLength)
{
	CommandBufferStatus status;

	if (length == 0)
		return status;

	const size_t remaining = kCommandBufferCapacity - m_size;

	if (!text || requestedLength >= remaining || length >= remaining)
	{
		status.ok = false;
		status.overflow = true;
		return status;
	}

	std::memmove(m_data + length, m_data, m_size);
	std::memcpy(m_data, text, length);
	m_size += length;
	return status;
}

bool CommandBuffer::popLine(CommandLine &line)
{
	if (m_size == 0)
		return false;

	size_t split = 0;
	bool quotes = false;
	bool hasComment = false;
	size_t commentOffset = 0;

	for (; split < m_size; ++split)
	{
		const char value = static_cast<char>(m_data[split]);

		if (!hasComment)
		{
			if (value == '"')
				quotes = !quotes;

			if (quotes)
			{
				if (split < m_size - 1 &&
					value == '\\' &&
					(m_data[split + 1] == '"' || m_data[split + 1] == '\\'))
				{
					++split;
				}
			}
			else
			{
				if (value == '/' &&
					split + 1 < m_size &&
					m_data[split + 1] == '/' &&
					(split == 0 || m_data[split - 1] <= ' '))
				{
					hasComment = true;
					commentOffset = split;
				}

				if (value == ';')
					break;
			}
		}

		if (m_data[split] == '\n' || m_data[split] == '\r')
			break;
	}

	if (split >= kCommandLineCapacity - 1)
	{
		line.text[0] = '\0';
		line.overflow = true;
	}
	else
	{
		const size_t copyLength = hasComment ? commentOffset : split;
		std::memcpy(line.text, m_data, copyLength);
		line.text[copyLength] = '\0';
		line.overflow = false;
	}

	if (split == m_size)
	{
		m_size = 0;
	}
	else
	{
		const size_t consumed = split + 1;
		m_size -= consumed;
		std::memmove(m_data, m_data + consumed, m_size);
	}

	return true;
}

}
}
}
