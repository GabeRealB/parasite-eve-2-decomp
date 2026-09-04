#ifndef M4A1_JAVELIN_H
#define M4A1_JAVELIN_H

#include "common.h"
#include <psyq/libgte.h>

/// Low halves of a `VECTOR3` (typically `GsCOORDINATE2.workm.t`).
typedef struct M4a1JavelinVecLo {
    /* 0x0 */ u16  vx;
    /* 0x2 */ byte pad_2[2];
    /* 0x4 */ u16  vy;
    /* 0x6 */ byte pad_6[2];
    /* 0x8 */ u16  vz;
} M4a1JavelinVecLo;

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by
/// `func_m4a1_javelin_8011EE78` for one projected tracer line. Each endpoint is
/// pushed through `GsWSMATRIX` with a single `RTPS`; `flag` is the shared
/// `gte_stflg` of whichever projection just ran, `otz0` / `otz1` are the two
/// `gte_stszotz` results (each biased by 1) and `sx*` / `sy*` are the two
/// packed `SXY2` screen coordinates the `LINE_G2` is built from.
typedef struct M4a1JavelinLineScratch {
    /* 0x00 */ s32 otz0;
    /* 0x04 */ s32 otz1;
    /* 0x08 */ s32 flag;
    /* 0x0C */ u16 sx0;
    /* 0x0E */ u16 sy0;
    /* 0x10 */ u16 sx1;
    /* 0x12 */ u16 sy1;
} M4a1JavelinLineScratch;
STATIC_ASSERT_SIZEOF(M4a1JavelinLineScratch, 0x14);

extern SVECTOR D_m4a1_javelin_8012EB68;
extern s32     D_m4a1_javelin_8012EB70;

/// Links one Gouraud `LINE_G2` between the world-space points `p0` and `p1`
/// into `Gpu_CurrentOt`, dropped entirely if either endpoint fails its `RTPS`
/// `FLAG` check. Only the first vertex is lit: `brightness` goes into blue,
/// half of it into green and a quarter into red, so the tracer fades from a
/// blue-white head to black.
void func_m4a1_javelin_8011EE78(SVECTOR* p0, SVECTOR* p1, u16 brightness);
void func_m4a1_javelin_8011F4A4(M4a1JavelinVecLo* arg0);

#endif
