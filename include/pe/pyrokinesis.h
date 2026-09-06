#ifndef PE_PYROKINESIS_H
#define PE_PYROKINESIS_H

#include "common.h"

#include "gameplay/3A34.h"

/// Collision pair allocated by `func_pyrokinesis_8012EF48` (`Mem_Calloc(0x58)`)
/// and stored in `Task::idMap`. `obj` is linked on list 1 and carries the
/// packed combo id, `obj2` on list 7 with the 0x4400 flags the cone uses to
/// probe for a wall; both point `field_C` at the one-element `rec` table
/// (terminator `field_0 = 2`).
typedef struct PyroWork {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpObj   obj2;
    /* 0x40 */ GpRec18 rec;
} PyroWork;
STATIC_ASSERT_SIZEOF(PyroWork, 0x58);

/// One 8-byte row of the gameplay table `D_80113D40`, indexed by
/// `GpEffWork.field_20` (`Gp_StateC08.field_0 % 10 - 1`, the combo counter).
/// Only `field_4` is read here: the cone burns while `GpEffWork.field_22 * 6`
/// is still within it, so the three rows (0x41 / 0x5A / 0xC8) are how long
/// each combo level sustains the flame.
typedef struct PyroLevel {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} PyroLevel;
STATIC_ASSERT_SIZEOF(PyroLevel, 8);

extern PyroLevel D_80113D40[];

/// The `SndEvt_EnqueueType6` id of the ignition roar, indexed by
/// `GpEffWork.field_20 * 3 + Task::spawnArg1` (combo level by cast variant).
extern s32 D_pyrokinesis_80131DD8[];

/// Per-flame jitter of the cone, one 8-bit LCG roll each, re-rolled as a block
/// when the cast starts.
extern s16 D_pyrokinesis_80131DFC[16];

#endif /* PE_PYROKINESIS_H */
