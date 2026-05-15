// xash3dpp — cmd_cvar: null compatibility policy
// Linked when CMake option XASH_GOLDSRC_COMPAT=0.
// All methods are no-ops / return the "no quirk applies" sentinel.

#include <xash3dpp/private/cmd_cvar/compat_policy.hpp>

namespace xash::cmd_cvar {

class NullCompatPolicy final : public ICompatPolicy {
public:
    const char *redirect_cvar_name(const char * /*name*/) const noexcept override
    {
        return nullptr;
    }

    bool is_filterable_exempt(const char * /*cmd_name*/) const noexcept override
    {
        return false;
    }

    bool is_overridable_command(const char * /*cmd_name*/) const noexcept override
    {
        return false;
    }
};

ICompatPolicy &get_compat_policy() noexcept
{
    static NullCompatPolicy instance;
    return instance;
}

} // namespace xash::cmd_cvar
