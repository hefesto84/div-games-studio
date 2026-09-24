#ifndef _SHIM_MATH_H
#define _SHIM_MATH_H

/* Replaces the hand-rolled Watcom math_.h. Install the standard <math.h>. */

#include <math.h>

#ifndef _HugeValue
#define _HugeValue HUGE_VAL
#endif
#ifndef HugeValue
#define HugeValue HUGE_VAL
#endif
#ifndef _HugeVal
#define _HugeVal HUGE_VAL
#endif

#endif /* _SHIM_MATH_H */