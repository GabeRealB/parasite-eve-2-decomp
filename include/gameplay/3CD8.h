#ifndef GAMEPLAY_3CD8_H
#define GAMEPLAY_3CD8_H

#include "common.h"

#include "main/task.h"

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
    /* 0x10 */ s16 field_10;
    /* 0x12 */ s16 field_12;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
    /* 0x18 */ s16 field_18;
    /* 0x1A */ s16 field_1A;
} GpState1C;
STATIC_ASSERT_SIZEOF(GpState1C, 0x1C);

extern GpState1C* D_80115740;

void func_800EA3A0(s32 arg0);
void func_800EC7E4(void* arg0, Task* arg1);

#endif // GAMEPLAY_3CD8_H
