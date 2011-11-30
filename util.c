#include "util.h"

/**
 * Fast log base 2 for a 32 bit integer thanks to Eric Cole and Mark Dickinson
 */
unsigned int log_2(uint32_t num)
{
	static const unsigned int MultiplyDeBruijnBitPosition[32] =
		{
			0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
			8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
		};

	num |= num >> 1; // first round down to one less than a power of 2
	num |= num >> 2;
	num |= num >> 4;
	num |= num >> 8;
	num |= num >> 16;

	return MultiplyDeBruijnBitPosition[(uint32_t)(num * 0x07C4ACDDU) >> 27];
}
