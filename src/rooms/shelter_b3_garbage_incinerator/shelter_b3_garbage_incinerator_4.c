#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

typedef struct {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} GarbageIncineratorState;

/// Work block of the task in `D_shelter_b3_garbage_incinerator_8018FC3C`;
/// only the child task stored into it is known.
typedef struct {
    byte  pad_0[0x30];
    Task* child;
} GarbageIncineratorWork;

extern TaskDesc D_shelter_b3_garbage_incinerator_80187150[];
extern Task*    D_shelter_b3_garbage_incinerator_8018FC3C;

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017F0A8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017F318);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017F410);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017F588);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017F6D8);

void func_shelter_b3_garbage_incinerator_8017F8A4(GarbageIncineratorState* arg0, s32 arg1, s32 arg2)
{
    arg0->field_30 = arg2;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017F8AC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017F930);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017F968);

void func_shelter_b3_garbage_incinerator_8017F9B4(s32 arg0)
{
    GarbageIncineratorWork* work = D_shelter_b3_garbage_incinerator_8018FC3C->work;

    if (arg0 == 0) {
        SndEvt_EnqueueType6(0x54280008, 0, 0);
        Gp_PulseState1C();
        gGameSession->enemyCullZone = 0x10;
        work->child                 = Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 3, 0, 0);
        return;
    }
    work->child->spawnArg1 = 1;
}

void func_shelter_b3_garbage_incinerator_8017FA3C(void)
{
    D_80073BA0                = 0;
    gGameSession->restartMode = 3;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017FA58);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017FB80);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017FC5C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", RoomsShared801830f0Sub);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_8017FE74);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_801808A8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180994);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180B18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180C28);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180D44);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180E0C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180EC4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180F18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80180F54);
