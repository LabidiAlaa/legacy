/**
 * \defgroup fixedpoint Fixed-point based animated plasma patterns.
 */
/*@{*/

/**
 * @file fpmath_patterns.c
 * @brief Routines for drawing patterns generated by fixed-point math functions.
 * @author Christian Kroll
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../config.h"
#include "../pixel.h"
#include "../util.h"
#include "fpmath_patterns.h"


/**
 * Double buffering helps in reducing the effect of visibly redrawing every
 * frame. With this option turned on, a frame is rendered into an off-screen
 * buffer first and then copied to the actual frame buffer in one piece.
 * However, given the borg's graphics architecture, half painted frames may
 * still occur, but they are barely noticeable with this option enabled.
 *
 * Turn this off (#undef DOUBLE_BUFFERING) if you prefer speed over beauty.
 */
#define DOUBLE_BUFFERING


#ifdef LOW_PRECISION
	#undef LOW_PRECISION
#endif

#if NUM_COLS <= 16 && NUM_ROWS <= 16
	/**
	 * Low precision means that we use Q10.5 values and 16 bit types for almost
	 * every calculation (with multiplication and division as notable exceptions
	 * as they and their interim results utilize 32 bit).
	 *
	 * Use this precision mode with care as image quality will suffer
	 * noticeably. It produces leaner and faster code, though. This mode should
	 * not be used with resolutions higher than 16x16 as overflows are likely to
	 * occur in interim calculations.
	 *
	 * Normal precision (i.e. #undef LOW_PRECISION) conforms to Q7.8 with the
	 * ability to store every interim result as Q23.8. Most operations like
	 * square root, sine, cosine, multiplication etc. utilize 32 bit types.
	 */
	#define LOW_PRECISION
#endif

#ifdef LOW_PRECISION
	/** This is the type we expect ordinary integers to be. */
	typedef int16_t  ordinary_int_t;
	/** This is the type which we use for fixed-point values. */
	typedef int16_t  fixp_t;
	/** This type covers arguments of fixSin() and fixCos(). */
	typedef int16_t  fixp_trig_t;
	/** This type covers interim results of fixed-point operations. */
	typedef uint32_t  fixp_interim_t;
	/** This type covers interim results of the fixSqrt() function. */
	typedef uint16_t ufixp_interim_t;
	/** Number of bits the fixSqrt() function can handle. */
	#define SQRT_BITS 16

	// NOTE: If you change the following values, don't forget to adapt the sine
	// lookup table as well!

	/** Multiply a number by this factor to convert it to a fixed-point value.*/
	#define FIX           32
	/**	Number of fractional bits of a value (i.e. ceil(log_2(FIX))). */
	#define FIX_FRACBITS  5
	/**
	 * The number of temporal quantization steps of the sine lookup table. It
	 * must be a divisor of (FIX * 2 * pi) and this divisor must be divisable by
	 * 4 itself. Approximate this value as close as possible to keep rounding
	 * errors at a minimum.
	 */
	#define FIX_SIN_COUNT  200
	/** The rounded down quotient of (FIX * 2 * pi) and FIX_SIN_COUNT */
	#define FIX_SIN_DIVIDER 1

	/** Type of the lookup table elements. */
	typedef uint8_t lut_t;

	/**
	 * Lookup table of fractional parts which model the first quarter of a
	 * sine period. The rest of that period is calculated by mirroring those
	 * values. These values are intended for Q5 types.
	 */
	static lut_t const fix_sine_lut[FIX_SIN_COUNT / 4] =
			{ 0,  1,  2,  3,  4,  5,  6,  7,
			  8,  9, 10, 11, 12, 13, 14, 15,
			 15, 16, 17, 18, 19, 20, 20, 21,
			 22, 22, 23, 24, 24, 25, 26, 26,
			 27, 27, 28, 28, 29, 29, 29, 30,
			 30, 30, 31, 31, 31, 31, 31, 31,
			 31, 31};
#else
	/** This is the type we expect ordinary integers to be. */
	typedef int16_t  ordinary_int_t;
	/** This is the type which we use for fixed-point values. */
	typedef int16_t  fixp_t;
	/** This type covers arguments of fixSin() and fixCos(). */
	typedef int32_t  fixp_trig_t;
	/** This type covers interim results of fixed-point operations. */
	typedef int32_t  fixp_interim_t;
	/** This type covers interim results of the fixSqrt() function. */
	typedef uint32_t ufixp_interim_t;
	/** Number of bits the fixSqrt() function can handle. */
	#define SQRT_BITS 32

	// NOTE: If you change the following values, don't forget to adapt the sine
	// lookup table as well!

	/** Multiply a number by this factor to convert it to a fixed-point value.*/
	#define FIX           256
	/**	Number of fractional bits of a value (i.e. ceil(log_2(FIX))). */
	#define FIX_FRACBITS  8
	/**
	 * The number of temporal quantization steps of the sine lookup table. It
	 * must be a divisor of (FIX * 2 * pi) and this divisor must be divisable by
	 * 4 itself. Approximate this value as close as possible to keep rounding
	 * errors at a minimum.
	 */
	#define FIX_SIN_COUNT  200
	/** The rounded down quotient of (FIX * 2 * pi) and FIX_SIN_COUNT */
	#define FIX_SIN_DIVIDER 8

	/** Type of the lookup table elements. */
	typedef uint8_t lut_t;

	/**
	 * Lookup table of fractional parts which model the first quarter of a
	 * sine period. The rest of that period is calculated by mirroring those
	 * values. These values are intended for Q8 types.
	 */
	static lut_t const fix_sine_lut[FIX_SIN_COUNT / 4] =
			{  0,   9,  17,  24,  32,  40,  48,  56,
			  64,  72,  79,  87,  94, 102, 109, 116,
			 123, 130, 137, 144, 150, 157, 163, 169,
			 175, 181, 186, 192, 197, 202, 207, 211,
			 216, 220, 224, 228, 231, 235, 238, 240,
			 243, 245, 247, 249, 251, 252, 253, 254,
			 255, 255};

#endif


/** The ordinary pi constant. */
#define PI                  3.14159265358979323846
/** Fixed-point version of (pi / 2). */
#define FIX_PI_2            ((fixp_t)(PI * FIX / 2))
/** Fixed-point version of pi. */
#define FIX_PI              ((fixp_t)(PI * FIX))
/** Fixed-point version of (2 * pi). */
#define FIX_2PI             ((fixp_t)(2 * PI * FIX))


/**
 * Scales an ordinary integer up to its fixed-point format.
 * @param a An ordinary integer to be scaled up.
 * @return The given value in fixed-point format.
 */
inline static fixp_t fixScaleUp(ordinary_int_t a)
{
	return (fixp_t)a * FIX;
}


/**
 * Scales a fixed-point value down to an ordinary integer (omitting the
 * fractional part).
 * @param a Fixed-point value to be scaled down to an integer.
 * @return The given value as an integer.
 */
inline static ordinary_int_t fixScaleDown(fixp_t const a)
{
	return a / FIX;
}


/**
 * Multiplies two fixed-point values.
 * @param a A multiplicand.
 * @param b A multiplying factor.
 * @return Product of a and b.
 */
inline static fixp_interim_t fixMul(fixp_t const a, fixp_t const b)
{
	return ((fixp_interim_t)a * (fixp_interim_t)b) / FIX;
}


/**
 * Divides two fixed-point values.
 * @param a A dividend.
 * @param b A divisor.
 * @return Quotient of a and b.
 */
inline static fixp_t fixDiv(fixp_interim_t const a, fixp_interim_t const b)
{
	return (a * FIX) / b;
}


/**
 * Fixed-point variant of the sine function which receives a fixed-point angle
 * (radian). It uses a lookup table which models the first quarter of a full
 * sine period and calculates the rest from that quarter.
 * @param fAngle A fixed-point value in radian.
 * @return Result of the sine function normalized to a range from -FIX to FIX.
 */
static fixp_t fixSin(fixp_trig_t fAngle)
{
	// convert given fixed-point angle to its corresponding quantization step
	int8_t nSign = 1;
	if (fAngle < 0)
	{
		// take advantage of sin(-x) == -sin(x) to avoid neg. operands for "%"
		fAngle = -fAngle;
		nSign = -1;
	}
	uint8_t nIndex = (fAngle / FIX_SIN_DIVIDER) % FIX_SIN_COUNT;

	// now convert that quantization step to an index of our quartered array
	if ((nIndex >= (FIX_SIN_COUNT / 4)))
	{
		if (nIndex < (FIX_SIN_COUNT / 2))
		{
			nIndex = (FIX_SIN_COUNT / 2 - 1) - nIndex;
		}
		else
		{
			// an angle > PI means that we have to toggle the sign of the result
			nSign *= -1;
			if (nIndex < (FIX_SIN_COUNT - (FIX_SIN_COUNT / 4)))
			{
				nIndex = nIndex - (FIX_SIN_COUNT / 2);
			}
			else
			{
				nIndex = (FIX_SIN_COUNT - 1) - nIndex;
			}
		}
	}
	assert(nIndex < (FIX_SIN_COUNT / 4));

	return ((fixp_t)fix_sine_lut[nIndex]) * nSign;
}


/**
 * Fixed-point variant of the cosine function which takes a fixed-point angle
 * (radian). It adds FIX_PI_2 to the given angle and consults the fixSin()
 * function for the final result.
 * @param fAngle A fixed-point value in radian.
 * @return Result of the cosine function normalized to a range from -FIX to FIX.
 */
static fixp_t fixCos(fixp_trig_t const fAngle)
{
	return fixSin(fAngle + FIX_PI_2);
}


/**
 * Fixed-point square root algorithm as proposed by Ken Turkowski:
 * http://www.realitypixels.com/turk/computergraphics/FixedSqrt.pdf
 * @param a The radicant we want the square root of.
 * @return The square root of the given value.
 */
static fixp_t fixSqrt(ufixp_interim_t const a)
{
	ufixp_interim_t nRoot, nRemainingHigh, nRemainingLow, nTestDiv, nCount;
	nRoot = 0;  // clear root
	nRemainingHigh = 0; // clear high part of partial remainder
	nRemainingLow = a; // get argument into low part of partial remainder
	nCount = (SQRT_BITS / 2 - 1) + (FIX_FRACBITS >> 1); // load loop counter
	do
	{
		nRemainingHigh =
				(nRemainingHigh << 2) | (nRemainingLow >> (SQRT_BITS - 2));
		nRemainingLow <<= 2; // get 2 bits of the argument
		nRoot <<= 1;  // get ready for the next bit in the root
		nTestDiv = (nRoot << 1) + 1; // test radical
		if (nRemainingHigh >= nTestDiv)
		{
			nRemainingHigh -= nTestDiv;
			nRoot++;
		}
	} while (nCount-- != 0);
	return (nRoot);
}


/**
 * Calculates the distance between two points.
 * @param x1 x-coordinate of the first point
 * @param y1 y-coordinate of the first point
 * @param x2 x-coordinate of the second point
 * @param y2 y-coordinate of the second point
 * @return The distance between the given points.
 */
static fixp_t fixDist(fixp_t const x1,
                      fixp_t const y1,
                      fixp_t const x2,
                      fixp_t const y2)
{
	return fixSqrt(fixMul((x1 - x2), (x1 - x2)) + fixMul((y1 - y2), (y1 - y2)));
}


/**
 * This pointer type covers functions which return a brightness value for the
 * given coordinates and a "step" value. Applied to all coordinates of the
 * borg's display this actually results in a more or less beautiful pattern.
 * @param x x-coordinate
 * @param y y-coordinate
 * @param t A step value which changes for each frame, allowing for animations.
 * @param r A pointer to persistent data required by the pattern function.
 * @return The brightness value (0 < n <= NUM_PLANES) of the given coordinate.
 */
typedef unsigned char (*fpmath_pattern_func_t)(unsigned char const x,
                                               unsigned char const y,
                                               fixp_t const t,
                                               void *const r);

#ifdef DOUBLE_BUFFERING
#    define BUFFER pixmap_buffer
#else
#    define BUFFER pixmap
#endif


/**
 * Draws an animated two dimensional graph for a given function f(x, y, t).
 * @param t_start  A start value for the function's step variable.
 * @param t_stop A stop value for the function's step variable.
 * @param t_delta Value by which the function's step variable gets incremented.
 * @param frame_delay The frame delay in milliseconds.
 * @param fpPattern Function which generates a pattern depending on x, y and t.
 * @param r A pointer to persistent data required by the fpPattern function.
 */
static void fixPattern(fixp_t const t_start,
                       fixp_t const t_stop,
                       fixp_t const t_delta,
                       int const frame_delay,
                       fpmath_pattern_func_t fpPattern,
                       void *r)
{
#ifdef DOUBLE_BUFFERING
	// double buffering to reduce half painted pictures
	unsigned char pixmap_buffer[NUMPLANE][NUM_ROWS][LINEBYTES];
#endif

	for (fixp_t t = t_start; t < t_stop; t += t_delta)
	{
		for (unsigned char y = 0; y < NUM_ROWS; ++y)
		{
			unsigned char nChunk[NUMPLANE + 1][LINEBYTES] = {{0}};
			for (unsigned char x = 0; x < (LINEBYTES * 8); ++x)
			{
				assert (y < 16);
				nChunk[fpPattern(x, y, t, r) - 1][x / 8u] |= shl_table[x % 8u];
			}
			for (unsigned char p = NUMPLANE; p--;)
			{
				for (unsigned char col = LINEBYTES; col--;)
				{
					nChunk[p][col] |= nChunk[p + 1][col];
					BUFFER[p][y][col] = nChunk[p][col];
				}
			}
		}

#ifdef DOUBLE_BUFFERING
		memcpy(pixmap, pixmap_buffer, sizeof(pixmap));
#endif

		wait(frame_delay);
	}
}


#ifdef ANIMATION_PLASMA

/**
 * This type maintains values relevant for the Plasma animation which need to be
 * persistent over consecutive invocations.
 * @see fixAnimPlasma()
 */
typedef struct fixp_plasma_s
{
	/**
	 * This array holds column dependent results of the first internal pattern
	 * function. Those results only need to be calculated for the first row of
	 * the current frame and are then reused for the remaining rows.
	 */
	fixp_t fFunc1[NUM_COLS];
	/**
	 * This value is part of the formula for the second internal pattern
	 * function. It needs to be calculated only once per frame.
	 */
	fixp_t fFunc2CosArg;
	/**
	 * This value is part of the formula for the second internal pattern
	 * function. It needs to be calculated only once per frame.
	 */
	fixp_t fFunc2SinArg;
} fixp_plasma_t;


/**
 * Draws a plasma like pattern (sort of... four shades of grey are pretty
 * scarce for a neat plasma animation). This is realized by superimposing two
 * functions which generate independent patterns for themselves.
 *
 * The first function draws horizontally moving waves and the second function
 * draws zoomed-in radiating curls. Together they create a wobbly, plasma like
 * pattern.
 *
 * @param x x-coordinate
 * @param y y-coordinate
 * @param t A Step value which changes for each frame, allowing for animations.
 * @param r A pointer to persistent interim results.
 * @return The brightness value (0 < n <= NUM_PLANES) of the given coordinate.
 */
static unsigned char fixAnimPlasma(unsigned char const x,
                                   unsigned char const y,
                                   fixp_t const t,
                                   void *const r)
{
	assert(x < NUM_COLS);
	assert(y < NUM_ROWS);

	// scaling factor
	static fixp_t const fPlasmaX = (2 * PI * FIX) / NUM_COLS;

	// reentrant data
	fixp_plasma_t *const p = (fixp_plasma_t *)r;

	if (x == 0 && y == 0)
	{
		p->fFunc2CosArg = NUM_ROWS * fixCos(t) + fixScaleUp(NUM_ROWS);
		p->fFunc2SinArg = NUM_COLS * fixSin(t) + fixScaleUp(NUM_COLS);
	}
	if (y == 0)
	{
		p->fFunc1[x] = fixSin(fixMul(fixScaleUp(x), fPlasmaX) + t);
	}

	fixp_t const fFunc2 = fixSin(fixMul(fixDist(fixScaleUp(x), fixScaleUp(y),
			p->fFunc2SinArg, p->fFunc2CosArg), fPlasmaX));

	uint8_t const nRes = fixScaleDown(fixDiv(fixMul(p->fFunc1[x] + fFunc2 +
			fixScaleUp(2), fixScaleUp(NUMPLANE - 1)), fixScaleUp(2)));
	assert (nRes <= 3);

	return nRes;
}

/**
 * Starting point for the Plasma animation.
 */
void plasma(void)
{
	fixp_plasma_t r;
#ifndef __AVR__
	fixPattern(0, fixScaleUp(75), 0.1 * FIX, 80, fixAnimPlasma, &r);
#else
	fixPattern(0, fixScaleUp(60), 0.1 * FIX, 1, fixAnimPlasma, &r);
#endif /* __AVR__ */
}

#endif /* ANIMATION_PLASMA */


#ifdef ANIMATION_PSYCHEDELIC

/**
 * This type maintains values relevant for the Psychedelic animation which need
 * to be persistent over consecutive invocations.
 */
typedef struct fixp_psychedelic_s
{
	fixp_t fCos;         /**< One of the column factors of the curl. */
	fixp_t fSin;         /**< One of the row factors of the curl. */
	fixp_interim_t ft10; /**< A value involved in rotating the curl's center. */
} fixp_psychedelic_t;


/**
 * Draws flowing circular waves with a rotating center.
 * @param x x-coordinate
 * @param y y-coordinate
 * @param t A step value which changes for each frame, allowing for animations.
 * @param r A pointer to persistent interim results.
 * @return The brightness value (0 < n <= NUM_PLANES) of the given coordinate.
 */
static unsigned char fixAnimPsychedelic(unsigned char const x,
                                        unsigned char const y,
                                        fixp_t const t,
                                        void *const r)
{
	assert(x < NUM_COLS);
	assert(y < NUM_ROWS);
	fixp_psychedelic_t *p = (fixp_psychedelic_t *)r;

	if (x == 0 && y == 0)
	{
		p->fCos = NUM_COLS/2 * fixCos(t);
		p->fSin = NUM_ROWS/2 * fixSin(t);
		p->ft10 = fixMul(t, fixScaleUp(10));
	}

	uint8_t const nResult =
			fixScaleDown(fixMul(fixSin((fixp_interim_t)fixDist(fixScaleUp(x),
			fixScaleUp(y), p->fCos, p->fSin) - p->ft10) + fixScaleUp(1),
			fixScaleUp(NUMPLANE - 1)));
	assert(nResult <= NUMPLANE);

	return nResult;
}


/**
 * Starting point for the Psychedelic animation.
 */
void psychedelic(void)
{
	fixp_psychedelic_t r;
#ifndef __AVR__
	fixPattern(0, fixScaleUp(75), 0.1 * FIX, 80, fixAnimPsychedelic, &r);
#else
	fixPattern(0, fixScaleUp(60), 0.1 * FIX, 15, fixAnimPsychedelic, &r);
#endif /* __AVR__ */
}

#endif /* ANIMATION_PSYCHEDELIC */

/*@}*/
