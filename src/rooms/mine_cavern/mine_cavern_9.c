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

/// The four spots the cavern's enemy can be parked at, indexed by the low half
/// of `Task::spawnArg1` (the spawn table `D_mine_cavern_8018EB38` packs the
/// slot there, so a 32-bit read would index past the end).
extern SVECTOR D_mine_cavern_8018EB18[4];

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

/// Second state handler of `D_mine_cavern_8017D7F8` (`func_mine_cavern_80183A68`
/// dispatches it). It allocates the work block, parks it at `Task::idMap` and
/// hands its two matrices to the model, then seats the model on the spawn spot
/// `Task::spawnArg1` names: the block's own coordinate adopts that spot with the
/// model's coordinate hung under it, and the model is republished through
/// `func_800D7A9C`.
///
/// `mem` and `work` are the same block: the original build tests and parks the
/// allocation through `mem` and reaches the block through `work` afterwards,
/// which is what keeps the two live ranges - and so `$v0` / `$a0` - apart.
void func_mine_cavern_801836D0(GpEnemy* arg0, Task* arg1)
{
    MineCavernWork* mem;
    MineCavernWork* work;
    VECTOR          vec;

    mem         = (MineCavernWork*)Mem_Calloc(0x14C, false);
    work        = mem;
    arg1->idMap = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    ((TmdObject*)arg1->extra)->field_8->sub        = &Gfx_ViewCoord;
    ((TmdObject*)arg1->extra)->field_C             = 0;
    ((TmdObject*)arg1->extra)->field_1C            = &work->light;
    ((TmdObject*)arg1->extra)->field_20            = &work->color;
    ((TmdObject*)arg1->extra)->field_8->coord.t[0] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vx;
    ((TmdObject*)arg1->extra)->field_8->coord.t[1] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vy;
    ((TmdObject*)arg1->extra)->field_8->coord.t[2] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vz;
    ((TmdObject*)arg1->extra)->field_8->flg        = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->field_8);
    vec.vx = ((TmdObject*)arg1->extra)->field_8->workm.t[0];
    vec.vy = ((TmdObject*)arg1->extra)->field_8->workm.t[1];
    vec.vz = ((TmdObject*)arg1->extra)->field_8->workm.t[2];
    func_800D7A9C(arg1->extra, &vec, 0, 3);
    arg1->state++;
}

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

/// Third state handler of `D_mine_cavern_8017D7F8` (`func_mine_cavern_80183A68`
/// dispatches it). It republishes the model's world position through
/// `func_800D7A9C`, then settles the work block's own coordinate: when the
/// `GameFlag_GetNibble(0xE2)` bit selected by `Task::spawnArg1` is set the
/// coordinate is reset to an identity rotation parked at (0, -0x320, 0) under
/// the model's own coordinate, `field_148` ticks, and the model's `field_C` is
/// cleared; otherwise the model is flagged hidden with `field_C = 0x80`.
///
/// `ang` is declared and never read - the original build's frame reserved 8
/// bytes for it ahead of nothing, so dropping it shrinks the frame from 0x38 to
/// 0x30 and moves every spill.
void func_mine_cavern_80183AD4(GpEnemy* enemy, Task* task)
{
    MineCavernWork* work;
    MATRIX*         m;
    VECTOR          vec;
    SVECTOR         ang;

    work = (MineCavernWork*)task->idMap;

    ((TmdObject*)task->extra)->field_8->flg = 0;
    Gp_UpdateCoord(((TmdObject*)task->extra)->field_8);
    vec.vx = ((TmdObject*)task->extra)->field_8->workm.t[0];
    vec.vy = ((TmdObject*)task->extra)->field_8->workm.t[1];
    vec.vz = ((TmdObject*)task->extra)->field_8->workm.t[2];
    func_800D7A9C(task->extra, &vec, 0, 3);

    if (!((GameFlag_GetNibble(0xE2) >> (u16)task->spawnArg1) & 1)) {
        ((TmdObject*)task->extra)->field_C = 0x80;
    } else {
        m                         = &work->coord.coord;
        *(s32*)&work->coord.coord = 0x1000;
        *(s32*)&m->m[0][2]        = 0;
        *(s32*)&m->m[1][1]        = 0x1000;
        *(s32*)&m->m[2][0]        = 0;
        m->m[2][2]                = 0x1000;
        work->coord.sub           = ((TmdObject*)task->extra)->field_8;
        work->coord.coord.t[2]    = 0;
        work->coord.coord.t[0]    = 0;
        work->coord.coord.t[1]    = -0x320;
        work->coord.flg           = 0;
        Gp_UpdateCoord(&work->coord);
        work->field_148++;
        ((TmdObject*)task->extra)->field_C = 0;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", RoomsShared80183c10Table);

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", jtbl_mine_cavern_8017D818);
