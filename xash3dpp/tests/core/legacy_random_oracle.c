// Test-only C oracle.  The block between the markers is copied verbatim from
// engine/common/common.c:54-153; verify_legacy_random_oracle.py rejects drift.

#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef uint32_t dword;
#define GAME_EXPORT

static time_t g_legacy_oracle_wall_seconds = 1;

static time_t legacy_oracle_time( time_t *out )
{
	if( out != NULL ) *out = g_legacy_oracle_wall_seconds;
	return g_legacy_oracle_wall_seconds;
}

#define time legacy_oracle_time
#define COM_SetRandomSeed legacy_oracle_set_seed
#define COM_RandomFloat legacy_oracle_random_float
#define COM_RandomLong legacy_oracle_random_long

/* XASH3DPP_LEGACY_RANDOM_ORACLE_BEGIN */
static int idum = 0;

#define MAX_RANDOM_RANGE	0x7FFFFFFFUL
#define IA		16807
#define IM		2147483647
#define IQ		127773
#define IR		2836
#define NTAB		32
#define EPS		1.2e-7
#define NDIV		(1 + (IM - 1) / NTAB)
#define AM		(1.0 / IM)
#define RNMX		(1.0 - EPS)

static int lran1( void )
{
	static int	iy = 0;
	static int	iv[NTAB];
	int		j;
	int		k;

	if( idum <= 0 || !iy )
	{
		if( -(idum) < 1 ) idum = 1;
		else idum = -(idum);

		for( j = NTAB + 7; j >= 0; j-- )
		{
			k = (idum) / IQ;
			idum = IA * (idum - k * IQ) - IR * k;
			if( idum < 0 ) idum += IM;
			if( j < NTAB ) iv[j] = idum;
		}

		iy = iv[0];
	}

	k = (idum) / IQ;
	idum = IA * (idum - k * IQ) - IR * k;
	if( idum < 0 ) idum += IM;
	j = iy / NDIV;
	iy = iv[j];
	iv[j] = idum;

	return iy;
}

// fran1 -- return a random floating-point number on the interval [0,1]
static float fran1( void )
{
	float temp = (float)AM * lran1();
	if( temp > RNMX )
		return (float)RNMX;
	return temp;
}

void GAME_EXPORT COM_SetRandomSeed( int lSeed )
{
	if( lSeed ) idum = lSeed;
	else idum = -time( NULL );

	if( 1000 < idum )
		idum = -idum;
	else if( -1000 < idum )
		idum -= 22261048;
}

float GAME_EXPORT COM_RandomFloat( float flLow, float flHigh )
{
	float	fl;

	if( idum == 0 ) COM_SetRandomSeed( 0 );

	fl = fran1(); // float in [0,1]
	return (fl * (flHigh - flLow)) + flLow; // float in [low, high)
}

int GAME_EXPORT COM_RandomLong( int lLow, int lHigh )
{
	dword	maxAcceptable;
	dword	n, x = lHigh - lLow + 1;

	if( idum == 0 ) COM_SetRandomSeed( 0 );

	if( x <= 0 || MAX_RANDOM_RANGE < x - 1 )
		return lLow;

	// The following maps a uniform distribution on the interval [0, MAX_RANDOM_RANGE]
	// to a smaller, client-specified range of [0,x-1] in a way that doesn't bias
	// the uniform distribution unfavorably. Even for a worst case x, the loop is
	// guaranteed to be taken no more than half the time, so for that worst case x,
	// the average number of times through the loop is 2. For cases where x is
	// much smaller than MAX_RANDOM_RANGE, the average number of times through the
	// loop is very close to 1.
	maxAcceptable = MAX_RANDOM_RANGE - ((MAX_RANDOM_RANGE + 1) % x );
	do
	{
		n = lran1();
	} while( n > maxAcceptable );

	return lLow + (n % x);
}
/* XASH3DPP_LEGACY_RANDOM_ORACLE_END */

#undef COM_RandomLong
#undef COM_RandomFloat
#undef COM_SetRandomSeed
#undef time

void legacy_oracle_set_wall_seconds( int64_t seconds )
{
	g_legacy_oracle_wall_seconds = (time_t)seconds;
}

int legacy_oracle_next_raw( void )
{
	return lran1();
}
