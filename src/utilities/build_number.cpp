#include "utilities/build_number.hpp"

#include <stdio.h>

namespace xash
{
namespace utilities
{

namespace
{

static const int kMonthDays[12] = {
	31, 28, 31, 30, 31, 30,
	31, 31, 30, 31, 30, 31
};

}

int BuildNumberFromIsoDate(const char *date)
{
	int y;
	int m;
	int d;

	if (!date || sscanf(date, "%d-%d-%d", &y, &m, &d) != 3 ||
		y <= 1900 || m <= 0 || m > 12 || d <= 0)
		return -1;

	--m;
	--d;

	for (int i = 0; i < m; ++i)
		d += kMonthDays[i];

	y -= 1900;
	int build = d + static_cast<int>((y - 1) * 365.25f);

	if (((y % 4) == 0) && m > 1)
		build += 1;

	build -= 41728;
	return build;
}

}
}
