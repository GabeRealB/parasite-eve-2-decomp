#ifndef PE_PLASMA_H
#define PE_PLASMA_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>
#include "main/coord.h"

/// Per-ring radius scale for `func_plasma_8012F568`, indexed by ring number
/// (0..2). `rInner` widens the inner radius (`GpEffWork::angle`), `rExtra`
/// the outer radius on top of that (`+ GpEffWork::step`), and `yOff` raises
/// the inner edge above `GpEffWork::period`.
typedef struct PlasmaRingScale {
    /* 0x0 */ s16 rInner;
    /* 0x2 */ s16 yOff;
    /* 0x4 */ s16 rExtra;
} PlasmaRingScale;
STATIC_ASSERT_SIZEOF(PlasmaRingScale, 0x6);

#endif /* PE_PLASMA_H */
