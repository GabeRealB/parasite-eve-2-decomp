#include "common.h"

#include "main/session.h"

typedef struct {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} GarbageIncineratorState;

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_3", func_shelter_b3_garbage_incinerator_8017E70C);

extern void func_shelter_b3_garbage_incinerator_80185220(void);

void func_shelter_b3_garbage_incinerator_8017E7A4(GarbageIncineratorState* arg0)
{
    func_shelter_b3_garbage_incinerator_80185220();
    arg0->field_30 = 5;
}
