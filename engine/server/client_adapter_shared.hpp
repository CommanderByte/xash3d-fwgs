#ifndef XASH_ENGINE_SERVER_CLIENT_ADAPTER_SHARED_HPP
#define XASH_ENGINE_SERVER_CLIENT_ADAPTER_SHARED_HPP

#include <cstddef>

namespace xash
{
namespace engine
{
namespace server
{
namespace adapter
{
namespace client
{

inline bool FromLegacyBool(int value)
{
	return value != 0;
}

inline int ToLegacyBool(bool value)
{
	return value ? 1 : 0;
}

template <typename Enum>
inline int ToLegacyEnum(Enum value)
{
	return static_cast<int>(value);
}

inline int ToLegacySize(std::size_t value)
{
	if (value == 0 || value > static_cast<std::size_t>(0x7fffffff))
		return 0;

	return static_cast<int>(value);
}

}
}
}
}
}

#endif
