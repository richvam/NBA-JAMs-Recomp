/* detmath.h - the same math results on every processor, compiler and C library (roadmap R5.4).
 * The C library's sin, exp, pow, ... may differ in the last bit between glibc, musl, the
 * Microsoft runtime and Apple's; a Lua mod that turns such a result into a write to game memory
 * would then make an online game go out of step. These are musl 1.2.5's implementations
 * (MIT, COPYRIGHT.musl), compiled with no fused multiply-add, used by Lua instead (luaconf.h). */
#ifndef DETMATH_H
#define DETMATH_H
#ifdef __cplusplus
extern "C" {
#endif
double det_sin(double); double det_cos(double); double det_tan(double);
double det_asin(double); double det_acos(double); double det_atan(double); double det_atan2(double, double);
double det_exp(double); double det_log(double); double det_log2(double); double det_log10(double);
double det_pow(double, double);
#ifdef __cplusplus
}
#endif
#endif
