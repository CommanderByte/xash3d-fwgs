// xash3dpp — gameinfo_parser tests
// Covers: apply_gameinfo_fixups
// Note: parse_gameinfo_txt, parse_liblist_gam, serialise_gameinfo are
//       currently stub implementations — they are declared but not yet
//       functional, so no behaviour tests are written for them here.

#include <xash3dpp/utilities/gameinfo_parser.hpp>
#include <cstdio>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// apply_gameinfo_fixups — clamp budgets to legal ranges
// ---------------------------------------------------------------------------

static void test_apply_gameinfo_fixups()
{
    // max_edicts: clamp to [64, 8192].
    {
        xash::GameInfo g;
        g.max_edicts = 10;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_edicts == 64 );
    }
    {
        xash::GameInfo g;
        g.max_edicts = 99999;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_edicts == 8192 );
    }
    // Default value 900 is in range — no clamping.
    {
        xash::GameInfo g;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_edicts == 900 );
    }

    // max_tents: clamp to [32, 4096].
    {
        xash::GameInfo g;
        g.max_tents = 1;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_tents == 32 );
    }
    {
        xash::GameInfo g;
        g.max_tents = 10000;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_tents == 4096 );
    }
    // Default 500 stays unchanged.
    {
        xash::GameInfo g;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_tents == 500 );
    }

    // max_beams: clamp to [16, 2048].
    {
        xash::GameInfo g;
        g.max_beams = 0;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_beams == 16 );
    }
    {
        xash::GameInfo g;
        g.max_beams = 9999;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_beams == 2048 );
    }
    // Default 128 stays unchanged.
    {
        xash::GameInfo g;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_beams == 128 );
    }

    // max_particles: clamp to [256, 65536].
    {
        xash::GameInfo g;
        g.max_particles = 10;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_particles == 256 );
    }
    {
        xash::GameInfo g;
        g.max_particles = 999999;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_particles == 65536 );
    }
    // Default 4096 stays unchanged.
    {
        xash::GameInfo g;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_particles == 4096 );
    }

    // All fields simultaneously at legal boundary values.
    {
        xash::GameInfo g;
        g.max_edicts    = 64;
        g.max_tents     = 32;
        g.max_beams     = 16;
        g.max_particles = 256;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_edicts    == 64  );
        CHECK( g.max_tents     == 32  );
        CHECK( g.max_beams     == 16  );
        CHECK( g.max_particles == 256 );
    }
    {
        xash::GameInfo g;
        g.max_edicts    = 8192;
        g.max_tents     = 4096;
        g.max_beams     = 2048;
        g.max_particles = 65536;
        xash::apply_gameinfo_fixups(g);
        CHECK( g.max_edicts    == 8192  );
        CHECK( g.max_tents     == 4096  );
        CHECK( g.max_beams     == 2048  );
        CHECK( g.max_particles == 65536 );
    }
}

int main()
{
    test_apply_gameinfo_fixups();

    std::printf( "gameinfo_parser: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
