// xash3dpp — user-message registry + multicast pipeline tests (Chunk 6 S9).
// Pins SV_RegUserMsg index assignment, the pfnMessageBegin/Write*/MessageEnd
// state machine (variable-size back-patch + fixed-size mismatch drop), and
// SV_Multicast routing into a spawned client's reliable stream.

#include <xash3dpp/private/server/clients.hpp>

#include <xash3dpp/abi/eiface.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/networking/message_buf.hpp>
#include <xash3dpp/private/server/engine_bridge.hpp>

#include "../../test_helpers.hpp"

#include <memory>

namespace sv  = xash::server;
namespace abi = xash::abi;
namespace net = xash::networking;

static int g_pass = 0, g_fail = 0;

// A minimal bridge over a ClientMachinery with one spawned, non-fake client.
struct MsgFixture
{
    std::unique_ptr<sv::ClientMachinery> cm = std::make_unique<sv::ClientMachinery>();
    sv::EngineBridge bridge{};
    abi::edict_t     edict{};

    MsgFixture()
    {
        sv::clients_init( *cm );
        cm->maxclients = 1;
        cm->clients[0].state      = sv::ClientState::Spawned;
        cm->clients[0].fakeclient = false;
        cm->clients[0].edict      = &edict;

        bridge.clients      = cm.get();
        bridge.server_state = 2; // ss_active
        bridge.max_clients  = 1;
    }
};

static void test_registry_index_and_dedup()
{
    MsgFixture fx;
    const int a = sv::reg_user_msg( fx.bridge, "TestMsg", -1 );
    const int b = sv::reg_user_msg( fx.bridge, "OtherMsg", 4 );
    const int a2 = sv::reg_user_msg( fx.bridge, "TestMsg", -1 ); // dedup

    CHECK_EQ( a, sv::k_svc_lastmsg + 1 ); // 60
    CHECK_EQ( b, sv::k_svc_lastmsg + 2 ); // 61
    CHECK_EQ( a2, a );                     // same number on re-register

    // bad name / oversized size ⇒ svc_bad.
    CHECK_EQ( sv::reg_user_msg( fx.bridge, "", 1 ), sv::k_svc_bad );
    CHECK_EQ( sv::reg_user_msg( fx.bridge, "TooBig", 999999 ), sv::k_svc_bad );
}

static void test_variable_message_roundtrip()
{
    MsgFixture fx;
    const int num = sv::reg_user_msg( fx.bridge, "VarMsg", -1 );

    sv::message_begin( fx.bridge, sv::k_msg_all, num, nullptr, nullptr );
    sv::message_write_byte( *fx.cm, 0x42 );
    sv::message_write_short( *fx.cm, 0x1234 );
    sv::message_end( fx.bridge );

    // The spawned client's reliable stream now carries:
    //   [num][size word = 3][0x42][0x34 0x12]  → 6 bytes.
    const sv::ServerClient &cl = fx.cm->clients[0];
    CHECK_EQ( cl.reliable_bits, std::size_t{ 6 * 8 } );

    net::MessageBuf r;
    r.rebind_read( { cl.reliable, 6 } );
    CHECK_EQ( static_cast<int>( r.read_byte() ), num );
    CHECK_EQ( static_cast<int>( r.read_word() ), 3 ); // patched realsize
    CHECK_EQ( static_cast<int>( r.read_byte() ), 0x42 );
    CHECK_EQ( static_cast<int>( r.read_short() ), 0x1234 );
    CHECK( !r.overflowed() );

    // multicast scratch is cleared after the send.
    CHECK_EQ( fx.cm->multicast.num_bits_written(), std::size_t{ 0 } );
}

static void test_fixed_size_mismatch_drops()
{
    MsgFixture fx;
    const int num = sv::reg_user_msg( fx.bridge, "Fixed", 2 ); // exactly 2 bytes

    sv::message_begin( fx.bridge, sv::k_msg_all, num, nullptr, nullptr );
    sv::message_write_byte( *fx.cm, 0xAA ); // only 1 byte → mismatch
    sv::message_end( fx.bridge );

    // message dropped: nothing delivered, scratch cleared.
    CHECK_EQ( fx.cm->clients[0].reliable_bits, std::size_t{ 0 } );
    CHECK_EQ( fx.cm->multicast.num_bits_written(), std::size_t{ 0 } );
}

static void test_multicast_skips_fake_and_unspawned()
{
    MsgFixture fx;
    fx.cm->clients[0].fakeclient = true; // fake clients never receive
    const int num = sv::reg_user_msg( fx.bridge, "M", -1 );

    sv::message_begin( fx.bridge, sv::k_msg_all, num, nullptr, nullptr );
    sv::message_write_byte( *fx.cm, 1 );
    sv::message_end( fx.bridge );

    CHECK_EQ( fx.cm->clients[0].reliable_bits, std::size_t{ 0 } );
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_registry_index_and_dedup );
    RUN_TEST( test_variable_message_roundtrip );
    RUN_TEST( test_fixed_size_mismatch_drops );
    RUN_TEST( test_multicast_skips_fake_and_unspawned );
    std::printf( "server_messages: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
