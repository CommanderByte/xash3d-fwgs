#ifndef XASH_ENGINE_SERVER_GAME_DLL_ADAPTER_SHARED_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_ADAPTER_SHARED_HPP

namespace xash
{
namespace engine
{
namespace server
{
namespace adapter
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

}
}
}
}

#endif
