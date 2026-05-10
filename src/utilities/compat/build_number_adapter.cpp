#include "utilities/build_number.hpp"

extern "C" int Q_buildnum_iso(const char *date)
{
	return xash::utilities::BuildNumberFromIsoDate(date);
}
