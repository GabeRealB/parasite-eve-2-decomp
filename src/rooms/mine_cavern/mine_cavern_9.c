#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "rooms/mine_cavern.h"

extern void func_mine_cavern_80182454(void);

/// Current screen id at 0x8007218B.
extern s8 D_8007218B;

/// The mine task's three state handlers, dispatched through by state. Copied
/// onto the stack by `func_mine_cavern_80182DC8` before the call, the way every
/// other room drives its own task family.
extern const TaskFuncTable3 D_mine_cavern_8017D65C;

/// Sound emitter positions for the cavern's four ambient loops, indexed by the
/// emitter id `func_mine_cavern_801825C8` and its siblings are called with.
extern SVECTOR D_mine_cavern_8018E39C[4];

/// The cavern enemy's five state handlers, dispatched through by state.
extern GpEnemyTaskFuncTable5 D_mine_cavern_8017D7F8;

/// Enemy spawn table the cavern's ambush draws from, on the `GameFlag_GetNibble(0xE2)`
/// bits.
extern TaskDesc D_mine_cavern_8018EB38;

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_9", func_mine_cavern_80181864);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_9", func_mine_cavern_80181CAC);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_9", func_mine_cavern_80181D80);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_9", func_mine_cavern_80182184);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_9", func_mine_cavern_80182454);

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", D_mine_cavern_8017D65C);

void func_mine_cavern_801825C8(s16 arg0)
{
    GsCOORDINATE2 coord;
    s32           view;

    view             = Gp_GetViewIndex() & 0xFF;
    coord.sub        = &Gfx_ViewCoord;
    coord.coord.t[0] = D_mine_cavern_8018E39C[arg0].vx;
    coord.coord.t[1] = D_mine_cavern_8018E39C[arg0].vy;
    coord.coord.t[2] = D_mine_cavern_8018E39C[arg0].vz;
    coord.flg        = 0;
    Gp_UpdateCoord(&coord);

    switch (arg0) {
        case 0:
            switch (view) {
                case 2:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 20:
                    SndEvt_EnqueueType7(0x5402000F, 1);
                    break;
                case 3:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    break;
                case 4:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    break;
                case 5:
                case 25:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    break;
                case 18:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    break;
                case 19:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0xD);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0xD);
                    break;
                case 21:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 22:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 23:
                case 24:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    break;
            }
            break;
        case 1:
            switch (view) {
                case 2:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 4:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    break;
                case 20:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 22:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    break;
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:
                case 21:
                case 23:
                case 24:
                case 25:
                    SndEvt_EnqueueType7(0x5402000E, 1);
                    break;
            }
            break;
        case 2:
            switch (view) {
                case 5:
                case 25:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 6:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 7:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    break;
                case 14:
                case 15:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    break;
                case 16:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    break;
                case 17:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    break;
                case 8:
                case 21:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    break;
                case 23:
                case 24:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    break;
                case 2:
                case 3:
                case 4:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 18:
                case 19:
                case 20:
                case 22:
                default:
                    SndEvt_EnqueueType7(0x54020010, 1);
                    break;
            }
            break;
        case 3:
            switch (view) {
                case 2:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    break;
                case 7:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 8:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    break;
                case 6:
                case 20:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    break;
                case 3:
                case 4:
                case 5:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:
                case 21:
                case 22:
                case 23:
                case 24:
                case 25:
                default:
                    SndEvt_EnqueueType7(0x54020011, 1);
                    break;
            }
            break;
    }
}

void func_mine_cavern_80182CEC(Task* arg0)
{
    s16 i;
    s32 flags;

    flags = GameFlag_GetNibble(0xE2);
    for (i = 0; i < 4; i++) {
        if (!((flags >> i) & 1)) {
            Gp_SpawnEnemyFromTable(&D_mine_cavern_8018EB38, 0, i, NULL);
        }
        Gp_SpawnEnemyFromTable(&D_mine_cavern_8018EB38, 1, i, NULL);
    }
    arg0->state++;
}

void func_mine_cavern_80182DA8(void)
{
    func_mine_cavern_80182454();
}

/// Mine task dispatcher: runs the state handler this task's `state` selects,
/// unless the screen id says the room is being left.
void func_mine_cavern_80182DC8(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_mine_cavern_8017D65C;
    if (D_8007218B != 3) {
        sp.funcs[arg0->state](arg0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_9", func_mine_cavern_80182E34);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_9", func_mine_cavern_801830F0);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_9", func_mine_cavern_801836D0);

void func_mine_cavern_80183860(Task* arg0)
{
    MineCavernWork* work;

    work = (MineCavernWork*)arg0->idMap;
    if (work != NULL) {
        Gp_UnlinkObj(&work->obj40);
    }
}

void func_mine_cavern_80183890(GpEnemy* enemy, Task* task)
{
    MineCavernWork* work;

    work                = (MineCavernWork*)task->idMap;
    work->obj40.flags  &= 0x7FFF;
    enemy->node.field_4 = 1;
    Gp_UnlinkObj(&work->obj40);
    work->field_148 = 0;
    task->state++;
}

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_9", func_mine_cavern_801838F4);

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", D_mine_cavern_8017D7F8);

void func_mine_cavern_80183A68(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;

    sp = D_mine_cavern_8017D7F8;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_9", func_mine_cavern_80183AD4);

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", RoomsShared80183c10Table);

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", jtbl_mine_cavern_8017D818);
