#ifndef GAMEPLAY_3CD8_H
#define GAMEPLAY_3CD8_H

#include "common.h"

#include "main/task.h"

/// Object stored in `Task::spawnArg2` for `func_800E712C`. `field_2` is a
/// signed completion flag: when non-zero the task calls `func_8003F6F8`
/// (`Stage_SetEndingFlag`) and kills itself.
typedef struct _GpEndWait {
    /* 0x00 */ byte pad_0[2];
    /* 0x02 */ s8   field_2;
} GpEndWait;

/// 0x34-byte dual-script state allocated by `func_800E8758` (`func_8002D978(0x34)`)
/// and stored on the owner task at +0x1C (`Task::idMap`).
/// `field_10` / `field_11` are delay counters for scripts A / B.
typedef struct _GpState34 {
    /* 0x00 */ void* field_0;  // script table (from Task::spawnArg2)
    /* 0x04 */ void* field_4;  // secondary table
    /* 0x08 */ s16   field_8;
    /* 0x0A */ u16   field_A;  // current command (script A)
    /* 0x0C */ u16   field_C;  // current command (script B)
    /* 0x0E */ u8    field_E;  // script A PC
    /* 0x0F */ u8    field_F;  // script B PC
    /* 0x10 */ u8    field_10; // delay A
    /* 0x11 */ u8    field_11; // delay B
    /* 0x12 */ u8    field_12;
    /* 0x13 */ u8    field_13;
    /* 0x14 */ u8    field_14; // loop counter A
    /* 0x15 */ u8    field_15; // loop counter B
    /* 0x16 */ byte  pad_16[0x12];
    /* 0x28 */ s32   field_28;
    /* 0x2C */ s32   field_2C;
    /* 0x30 */ s32   field_30;
} GpState34;
STATIC_ASSERT_SIZEOF(GpState34, 0x34);

/// 0x1C-byte halfword state allocated by `func_800E9CC8` (`func_8002D978(0x1C)`)
/// and stored in `D_80115740` (also written to the owner task at +0x1C).
typedef struct _GpState1C {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ s16 field_4;
    /* 0x06 */ s16 field_6;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ s16 field_C;
    /* 0x0E */ s16 field_E;
    /* 0x10 */ s16 field_10; // flags (bit 0 checked by func_800EC9C8)
    /* 0x12 */ s16 field_12;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
    /* 0x18 */ s16 field_18; // PE bit written by func_800ECA10
    /* 0x1A */ u16 field_1A; // flags (0x80 by func_800FC6C0, 0x100 by func_800EC868)
} GpState1C;
STATIC_ASSERT_SIZEOF(GpState1C, 0x1C);

extern GpState1C* D_80115740;

void func_800EA3A0(s32 arg0);
void func_800EC7E4(void* arg0, Task* arg1);
void func_800EC868(void);
void func_800EC9C8(void);
void func_800ECA10(s32 arg0);

#endif // GAMEPLAY_3CD8_H
