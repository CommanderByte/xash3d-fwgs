#pragma once
// xash3dpp — vendored frozen SDK struct: weapon_data_t
// Legacy reference: common/weaponinfo.h (Valve HLSDK, layout-frozen)

#include <cstddef>

// @annotation-exempt: abi-pod — weapon_data_t is a byte-exact, pointer-free
// mirror of the frozen SDK struct; the QN annotation matrix does not apply and
// @thread-safety is a caller contract (decisions-style QN).
namespace xash::abi {

// Info about weapons player might have in his/her possession
struct weapon_data_t
{
    int   m_iId;
    int   m_iClip;

    float m_flNextPrimaryAttack;
    float m_flNextSecondaryAttack;
    float m_flTimeWeaponIdle;

    int   m_fInReload;
    int   m_fInSpecialReload;
    float m_flNextReload;
    float m_flPumpTime;
    float m_fReloadTime;

    float m_fAimedDamage;
    float m_fNextAimBonus;
    int   m_fInZoom;
    int   m_iWeaponState;

    int   iuser1;
    int   iuser2;
    int   iuser3;
    int   iuser4;
    float fuser1;
    float fuser2;
    float fuser3;
    float fuser4;
};

static_assert( sizeof( weapon_data_t ) == 88 );
static_assert( offsetof( weapon_data_t, m_fInReload ) == 20 );
static_assert( offsetof( weapon_data_t, fuser4 )      == 84 );

} // namespace xash::abi
