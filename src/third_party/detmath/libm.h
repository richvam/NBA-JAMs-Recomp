/* libm.h - musl 1.2.5's src/internal/libm.h, cut down for detmath (roadmap R5.4): the parts the
 * double-precision functions here need, no long double, no <endian.h>, and every function renamed
 * (det_sin, det__sin, ...) so these never clash with the C library's own. FMA is never used, so
 * the results are the same bits on every processor and compiler. Original: MIT, COPYRIGHT.musl. */
#ifndef _LIBM_H
#define _LIBM_H

#include <stdint.h>
#include <float.h>
#include <math.h>
#include "detmath.h"

#undef __FP_FAST_FMA
#define __FP_FAST_FMA 0
#define hidden

#define sin det_sin
#define cos det_cos
#define tan det_tan
#define asin det_asin
#define acos det_acos
#define atan det_atan
#define atan2 det_atan2
#define exp det_exp
#define log det_log
#define log2 det_log2
#define log10 det_log10
#define pow det_pow
#define __sin det__sin
#define __cos det__cos
#define __tan det__tan
#define __rem_pio2 det__rem_pio2
#define __rem_pio2_large det__rem_pio2_large
#define __math_oflow det__math_oflow
#define __math_uflow det__math_uflow
#define __math_xflow det__math_xflow
#define __math_invalid det__math_invalid
#define __math_divzero det__math_divzero
#define __exp_data det__exp_data
#define __log_data det__log_data
#define __log2_data det__log2_data
#define __pow_log_data det__pow_log_data
/* Support non-nearest rounding mode.  */

#define WANT_ROUNDING 1
#define TOINT_INTRINSICS 0
#define issignaling_inline(x) 0

#ifdef __GNUC__
#define predict_true(x) __builtin_expect(!!(x), 1)
#define predict_false(x) __builtin_expect(x, 0)
#else
#define predict_true(x) (x)
#define predict_false(x) (x)
#endif

static inline double eval_as_double(double x) { double y = x; return y; }
static inline double fp_barrier(double x) { volatile double y = x; return y; }
static inline void fp_force_eval(double x) { volatile double y; y = x; (void)y; }
static inline void fp_force_evalf(float x) { volatile float y; y = x; (void)y; }
#define FORCE_EVAL(x) do {                        \
	if (sizeof(x) == sizeof(float)) {         \
		fp_force_evalf(x);                \
	} else {                                  \
		fp_force_eval(x);                 \
	}                                         \
} while(0)

#define asuint(f) ((union{float _f; uint32_t _i;}){f})._i
#define asfloat(i) ((union{uint32_t _i; float _f;}){i})._f
#define asuint64(f) ((union{double _f; uint64_t _i;}){f})._i
#define asdouble(i) ((union{uint64_t _i; double _f;}){i})._f

#define EXTRACT_WORDS(hi,lo,d)                    \
do {                                              \
  uint64_t __u = asuint64(d);                     \
  (hi) = __u >> 32;                               \
  (lo) = (uint32_t)__u;                           \
} while (0)

#define GET_HIGH_WORD(hi,d)                       \
do {                                              \
  (hi) = asuint64(d) >> 32;                       \
} while (0)

#define GET_LOW_WORD(lo,d)                        \
do {                                              \
  (lo) = (uint32_t)asuint64(d);                   \
} while (0)

#define INSERT_WORDS(d,hi,lo)                     \
do {                                              \
  (d) = asdouble(((uint64_t)(hi)<<32) | (uint32_t)(lo)); \
} while (0)

#define SET_HIGH_WORD(d,hi)                       \
  INSERT_WORDS(d, hi, (uint32_t)asuint64(d))

#define SET_LOW_WORD(d,lo)                        \
  INSERT_WORDS(d, asuint64(d)>>32, lo)

#define GET_FLOAT_WORD(w,d)                       \
do {                                              \
  (w) = asuint(d);                                \
} while (0)

#define SET_FLOAT_WORD(d,w)                       \
do {                                              \
  (d) = asfloat(w);                               \
} while (0)

hidden int    __rem_pio2_large(double*,double*,int,int,int);

hidden int    __rem_pio2(double,double*);
hidden double __sin(double,double,int);
hidden double __cos(double,double);
hidden double __tan(double,double,int);
hidden double __expo2(double,double);




extern int __signgam;

/* error handling functions */
hidden double __math_xflow(uint32_t, double);
hidden double __math_uflow(uint32_t);
hidden double __math_oflow(uint32_t);
hidden double __math_divzero(uint32_t);
hidden double __math_invalid(double);

#endif
