#include <stdlib.h>

#include "utilities/build_number.hpp"

using namespace xash::utilities;

int main()
{
	if (BuildNumberFromIsoDate("2015-04-02 21:19:10 +0300") != 1)
		return 1;

	if (BuildNumberFromIsoDate("2023-04-17 21:19:10 +0300") != 2938)
		return 2;

	if (BuildNumberFromIsoDate("1900-01-01") != -1)
		return 3;

	if (BuildNumberFromIsoDate("2023-00-01") != -1)
		return 4;

	if (BuildNumberFromIsoDate("2023-04-00") != -1)
		return 5;

	if (BuildNumberFromIsoDate("2023-13-01") != -1)
		return 6;

	if (BuildNumberFromIsoDate("not-a-date") != -1)
		return 7;

	if (BuildNumberFromIsoDate(nullptr) != -1)
		return 8;

	return EXIT_SUCCESS;
}
