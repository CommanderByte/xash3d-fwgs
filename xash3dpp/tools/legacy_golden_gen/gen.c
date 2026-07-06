/*
 * legacy_golden_gen/gen.c — verbatim-legacy golden-value generator (Q-18).
 *
 * Compiles the FROZEN legacy studio-math kernels (public/xash3d_mathlib.c +
 * public/matrixlib.c, reference-only) and emits bit-exact expected values for
 * the xash3dpp studio bone-solver port to cross-check against ->
 *   xash3dpp/tests/goldens/studio_math_goldens.inc
 *
 * This is the deferred Q-18 "verbatim-legacy cross-check harness": the studio
 * bone math has trig-interior paths (non-axis-aligned quaternions, slerp
 * interpolation, rotated CreateFromEntity) that cannot be hand-derived to
 * bit-exactness without hand-executing double-precision sin/cos. Rather than
 * copy a golden from our own port's output (forbidden by the parity rule), we
 * run the legacy kernel and emit its result.
 *
 * DEV-ONLY: not part of the xash3dpp build. It reaches into the reference-only
 * legacy tree by design. Regenerate via regen.ps1 whenever the case tables here
 * change; the emitted .inc is committed source (not a binary fixture).
 *
 * Emission: hex-float literals ("%a" + 'f'). A float promoted to double prints
 * an exact hex-float; the C++ compiler parses it back to the identical float
 * (the value is float-representable, so no double-rounding). Bit-exact.
 *
 * Usage: gen <output-path>
 */
#include "port.h"
#include "xash3d_types.h"
#include "com_model.h"
#include "xash3d_mathlib.h"
#include "studio.h"

#include <stdio.h>

/* ------------------------------------------------------------------------- */
/* emit helpers                                                              */
/* ------------------------------------------------------------------------- */

static void emitf( FILE *f, float v )
{
	fprintf( f, "%af", (double)v );
}

static void emit_vec( FILE *f, const float *v, int n )
{
	int i;
	fprintf( f, "{ " );
	for( i = 0; i < n; i++ )
	{
		emitf( f, v[i] );
		if( i + 1 < n ) fprintf( f, ", " );
	}
	fprintf( f, " }" );
}

static void emit_mat( FILE *f, const matrix3x4 m )
{
	float flat[12];
	int r, c, i = 0;
	for( r = 0; r < 3; r++ )
		for( c = 0; c < 4; c++ )
			flat[i++] = m[r][c];
	emit_vec( f, flat, 12 );
}

/* ------------------------------------------------------------------------- */
/* group 1 — AngleQuaternion( angles, q, studio=true )                       */
/* studio branch: angles are RADIANS (bone value[3..5] are radians).         */
/* ------------------------------------------------------------------------- */

static const vec3_t g_angle_quat_studio[] = {
	{ 0.0f, 0.0f, 0.0f },
	{ 0.5f, 0.0f, 0.0f },
	{ 0.0f, 0.5f, 0.0f },
	{ 0.0f, 0.0f, 0.5f },
	{ 0.3f, -0.4f, 0.7f },
	{ 1.2f, 0.8f, -0.6f },
	{ -0.9f, 1.5f, 0.2f },
};

static void gen_angle_quaternion_studio( FILE *f )
{
	int i, n = (int)( sizeof( g_angle_quat_studio ) / sizeof( g_angle_quat_studio[0] ) );
	fprintf( f, "struct AngleQuatCase { float angles_rad[3]; float q[4]; };\n" );
	fprintf( f, "inline constexpr AngleQuatCase k_angle_quaternion_studio[] = {\n" );
	for( i = 0; i < n; i++ )
	{
		vec4_t q;
		AngleQuaternion( g_angle_quat_studio[i], q, true );
		fprintf( f, "\t{ " );
		emit_vec( f, g_angle_quat_studio[i], 3 );
		fprintf( f, ", " );
		emit_vec( f, q, 4 );
		fprintf( f, " },\n" );
	}
	fprintf( f, "};\n\n" );
}

/* ------------------------------------------------------------------------- */
/* group 2 — QuaternionSlerp( p, q, t, out )                                 */
/* p/q are computed via AngleQuaternion so they are realistic unit quats;    */
/* the negate flag forces a dot<0 pair (exercises QuaternionAlign).          */
/* ------------------------------------------------------------------------- */

struct slerp_spec { vec3_t ap; vec3_t aq; float t; int negate_q; };
static const struct slerp_spec g_slerp[] = {
	{ { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.0f, 0.0f }, 0.5f, 0 },
	{ { 0.2f, 0.1f, 0.0f }, { -0.3f, 0.4f, 0.1f }, 0.25f, 0 },
	{ { 0.1f, 0.0f, 0.0f }, { 0.11f, 0.0f, 0.0f }, 0.5f, 0 },  /* near-parallel */
	{ { 0.0f, 0.0f, 0.0f }, { 0.6f, 0.0f, 0.0f }, 0.4f, 1 },    /* align (dot<0) */
	{ { 0.3f, -0.2f, 0.5f }, { -0.5f, 0.6f, -0.1f }, 0.75f, 0 },
};

static void gen_quaternion_slerp( FILE *f )
{
	int i, n = (int)( sizeof( g_slerp ) / sizeof( g_slerp[0] ) );
	fprintf( f, "struct SlerpCase { float p[4]; float q[4]; float t; float out[4]; };\n" );
	fprintf( f, "inline constexpr SlerpCase k_quaternion_slerp[] = {\n" );
	for( i = 0; i < n; i++ )
	{
		vec4_t p, q, out;
		int k;
		AngleQuaternion( g_slerp[i].ap, p, true );
		AngleQuaternion( g_slerp[i].aq, q, true );
		if( g_slerp[i].negate_q )
			for( k = 0; k < 4; k++ ) q[k] = -q[k];
		QuaternionSlerp( p, q, g_slerp[i].t, out );
		fprintf( f, "\t{ " );
		emit_vec( f, p, 4 );
		fprintf( f, ", " );
		emit_vec( f, q, 4 );
		fprintf( f, ", " );
		emitf( f, g_slerp[i].t );
		fprintf( f, ", " );
		emit_vec( f, out, 4 );
		fprintf( f, " },\n" );
	}
	fprintf( f, "};\n\n" );
}

/* ------------------------------------------------------------------------- */
/* group 3 — Matrix3x4_CreateFromEntity( out, angles, origin, scale )        */
/* angles are DEGREES; cases hit each of the 4 branches (roll/pitch/yaw/id). */
/* ------------------------------------------------------------------------- */

struct cfe_spec { vec3_t angles; vec3_t origin; float scale; };
static const struct cfe_spec g_cfe[] = {
	{ { 10.0f, 20.0f, 30.0f }, { 1.0f, 2.0f, 3.0f }, 1.0f },   /* roll branch */
	{ { -15.0f, 45.0f, -60.0f }, { 0.0f, 0.0f, 0.0f }, 2.0f }, /* roll branch */
	{ { 10.0f, 20.0f, 0.0f }, { 4.0f, -5.0f, 6.0f }, 1.0f },   /* pitch branch */
	{ { 0.0f, 20.0f, 0.0f }, { 7.0f, 8.0f, 9.0f }, 1.0f },     /* yaw branch */
	{ { 0.0f, 0.0f, 0.0f }, { -1.0f, -2.0f, -3.0f }, 1.5f },   /* identity branch */
	{ { 90.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f },     /* pitch=90 (near gimbal) */
};

static void gen_create_from_entity( FILE *f )
{
	int i, n = (int)( sizeof( g_cfe ) / sizeof( g_cfe[0] ) );
	fprintf( f, "struct CreateFromEntityCase { float angles_deg[3]; float origin[3]; float scale; float m[12]; };\n" );
	fprintf( f, "inline constexpr CreateFromEntityCase k_create_from_entity[] = {\n" );
	for( i = 0; i < n; i++ )
	{
		matrix3x4 m;
		Matrix3x4_CreateFromEntity( m, g_cfe[i].angles, g_cfe[i].origin, g_cfe[i].scale );
		fprintf( f, "\t{ " );
		emit_vec( f, g_cfe[i].angles, 3 );
		fprintf( f, ", " );
		emit_vec( f, g_cfe[i].origin, 3 );
		fprintf( f, ", " );
		emitf( f, g_cfe[i].scale );
		fprintf( f, ", " );
		emit_mat( f, m );
		fprintf( f, " },\n" );
	}
	fprintf( f, "};\n\n" );
}

/* ------------------------------------------------------------------------- */
/* group 4 — Matrix3x4_FromOriginQuat( out, quat, origin )                   */
/* non-dyadic quats (from AngleQuaternion) — the dyadic quats are hand-      */
/* derived in-test; these cover the general algebra.                         */
/* ------------------------------------------------------------------------- */

struct foq_spec { vec3_t angles_rad; vec3_t origin; };
static const struct foq_spec g_foq[] = {
	{ { 0.3f, 0.0f, 0.0f }, { 1.0f, 2.0f, 3.0f } },
	{ { 0.1f, 0.2f, 0.3f }, { 0.0f, 0.0f, 0.0f } },
	{ { -0.5f, 0.6f, -0.2f }, { -4.0f, 5.0f, -6.0f } },
	{ { 1.0f, -1.0f, 0.5f }, { 10.0f, -10.0f, 0.0f } },
};

static void gen_from_origin_quat( FILE *f )
{
	int i, n = (int)( sizeof( g_foq ) / sizeof( g_foq[0] ) );
	fprintf( f, "struct FromOriginQuatCase { float q[4]; float origin[3]; float m[12]; };\n" );
	fprintf( f, "inline constexpr FromOriginQuatCase k_from_origin_quat[] = {\n" );
	for( i = 0; i < n; i++ )
	{
		vec4_t q;
		matrix3x4 m;
		AngleQuaternion( g_foq[i].angles_rad, q, true );
		Matrix3x4_FromOriginQuat( m, q, g_foq[i].origin );
		fprintf( f, "\t{ " );
		emit_vec( f, q, 4 );
		fprintf( f, ", " );
		emit_vec( f, g_foq[i].origin, 3 );
		fprintf( f, ", " );
		emit_mat( f, m );
		fprintf( f, " },\n" );
	}
	fprintf( f, "};\n\n" );
}

/* ------------------------------------------------------------------------- */
/* group 5 — Matrix3x4_AnglesFromMatrix( in, out )                           */
/* inputs are matrices built by the legacy kernels above; one case forces    */
/* the xyDist<0.001 gimbal-lock branch (pitch=90 via CreateFromEntity).      */
/* ------------------------------------------------------------------------- */

struct afm_spec { vec3_t angles_deg; vec3_t origin; float scale; int use_quat; vec3_t angles_rad; };
static const struct afm_spec g_afm[] = {
	{ { 10.0f, 20.0f, 30.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, { 0.0f, 0.0f, 0.0f } },
	{ { -15.0f, 45.0f, -60.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, { 0.0f, 0.0f, 0.0f } },
	{ { 90.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, { 0.0f, 0.0f, 0.0f } }, /* gimbal */
	{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, 1, { 0.2f, -0.3f, 0.5f } }, /* from quat */
};

static void gen_angles_from_matrix( FILE *f )
{
	int i, n = (int)( sizeof( g_afm ) / sizeof( g_afm[0] ) );
	fprintf( f, "struct AnglesFromMatrixCase { float m[12]; float angles[3]; };\n" );
	fprintf( f, "inline constexpr AnglesFromMatrixCase k_angles_from_matrix[] = {\n" );
	for( i = 0; i < n; i++ )
	{
		matrix3x4 m;
		vec3_t out;
		if( g_afm[i].use_quat )
		{
			vec4_t q;
			vec3_t zero = { 0.0f, 0.0f, 0.0f };
			AngleQuaternion( g_afm[i].angles_rad, q, true );
			Matrix3x4_FromOriginQuat( m, q, zero );
		}
		else
		{
			Matrix3x4_CreateFromEntity( m, g_afm[i].angles_deg, g_afm[i].origin, g_afm[i].scale );
		}
		Matrix3x4_AnglesFromMatrix( m, out );
		fprintf( f, "\t{ " );
		emit_mat( f, m );
		fprintf( f, ", " );
		emit_vec( f, out, 3 );
		fprintf( f, " },\n" );
	}
	fprintf( f, "};\n\n" );
}

/* ------------------------------------------------------------------------- */

int main( int argc, char **argv )
{
	FILE *f;
	const char *path;

	if( argc < 2 )
	{
		fprintf( stderr, "usage: gen <output-path>\n" );
		return 2;
	}
	path = argv[1];
	f = fopen( path, "wb" );  /* wb => no CRLF translation, clean LF output */
	if( !f )
	{
		fprintf( stderr, "gen: cannot open '%s' for writing\n", path );
		return 2;
	}

	fprintf( f,
		"// studio_math_goldens.inc — AUTO-GENERATED. DO NOT EDIT BY HAND.\n"
		"//\n"
		"// Bit-exact expected values from the verbatim legacy studio-math kernels\n"
		"// (public/xash3d_mathlib.c, public/matrixlib.c), for the xash3dpp bone-solver\n"
		"// parity gate (Q-18). Regenerate: xash3dpp/tools/legacy_golden_gen/regen.ps1.\n"
		"// Values are hex-float literals (exact IEEE-754 single-precision round-trip).\n"
		"//\n"
		"// Hand-derivable-exact cases (dyadic quats, position-only decode, slerp\n"
		"// endpoints, identity CreateFromEntity, hull planes) live directly in the\n"
		"// tests; this file carries ONLY the trig-interior values that cannot be\n"
		"// hand-derived without executing double-precision sin/cos.\n"
		"\n"
		"namespace xash::goldens {\n"
		"\n" );

	gen_angle_quaternion_studio( f );
	gen_quaternion_slerp( f );
	gen_create_from_entity( f );
	gen_from_origin_quat( f );
	gen_angles_from_matrix( f );

	fprintf( f, "} // namespace xash::goldens\n" );

	fclose( f );
	fprintf( stderr, "gen: wrote %s\n", path );
	return 0;
}
