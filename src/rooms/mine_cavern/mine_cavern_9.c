#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/gfx.h"
#include "rooms/mine_cavern.h"

extern void func_mine_cavern_80181864(void);
extern void func_mine_cavern_80182184(void);
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

/// Queues the cavern's darkness overlay: a semi-transparent flat quad filling
/// the screen with the tint `D_mine_cavern_8018E3E0` holds for the number of
/// `GameFlag_GetNibble(0xE2)` bits set, followed by the drawing-mode packet
/// that restores the room's texture page (`0xE100004A`). Both go into the head
/// of the current OT, and the cavern's own two passes are run afterwards.
void func_mine_cavern_80182454(void)
{
    POLY_F4* poly;
    DR_MODE* dr;
    s32      flags;
    s16      i;
    s16      count;

    flags = GameFlag_GetNibble(0xE2);
    count = 0;

    poly           = (POLY_F4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(poly + 1);
    setlen(poly, 5);
    setcode(poly, 0x2A);

    for (i = 0; i < 4; i++) {
        if ((flags >> i) & 1) {
            count++;
        }
    }

    poly->r0 = D_mine_cavern_8018E3E0[count].r;
    poly->g0 = D_mine_cavern_8018E3E0[count].g;
    poly->b0 = D_mine_cavern_8018E3E0[count].b;

    poly->x0 = -0xA0;
    poly->y0 = -0x78;
    poly->x1 = 0xA0;
    poly->y1 = -0x78;
    poly->x2 = -0xA0;
    poly->y2 = 0x78;
    poly->x3 = 0xA0;
    poly->y3 = 0x78;
    addPrim(Gpu_CurrentOt, poly);

    dr             = (DR_MODE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(dr + 1);
    setlen(dr, 1);
    dr->code[0] = 0xE100004A;
    addPrim(Gpu_CurrentOt, dr);

    func_mine_cavern_80181864();
    func_mine_cavern_80182184();
}

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", D_mine_cavern_8017D65C);

void func_mine_cavern_801825C8(s16 arg0)
{
    GsCOORDINATE2 coord;
    s32           view;

    view             = Gp_GetViewIndex() & 0xFF;
    coord.sub        = &gGfxViewCoord;
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
/// dispatches it). It allocates the work block, parks it at `Task::work` and
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

    mem        = (MineCavernWork*)Mem_Calloc(0x14C, false);
    work       = mem;
    arg1->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    ((TmdObject*)arg1->extra)->coords->sub        = &gGfxViewCoord;
    ((TmdObject*)arg1->extra)->flags              = 0;
    ((TmdObject*)arg1->extra)->lightMtx           = &work->light;
    ((TmdObject*)arg1->extra)->colorMtx           = &work->color;
    ((TmdObject*)arg1->extra)->coords->coord.t[0] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vx;
    ((TmdObject*)arg1->extra)->coords->coord.t[1] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vy;
    ((TmdObject*)arg1->extra)->coords->coord.t[2] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1].vz;
    ((TmdObject*)arg1->extra)->coords->flg        = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
    vec.vx = ((TmdObject*)arg1->extra)->coords->workm.t[0];
    vec.vy = ((TmdObject*)arg1->extra)->coords->workm.t[1];
    vec.vz = ((TmdObject*)arg1->extra)->coords->workm.t[2];
    func_800D7A9C(arg1->extra, &vec, 0, 3);
    arg1->state++;
}

void func_mine_cavern_80183860(Task* arg0)
{
    MineCavernWork* work;

    work = (MineCavernWork*)arg0->work;
    if (work != NULL) {
        Gp_UnlinkObj(&work->obj40);
    }
}

void func_mine_cavern_80183890(GpEnemy* enemy, Task* task)
{
    MineCavernWork* work;

    work                = (MineCavernWork*)task->work;
    work->obj40.flags  &= 0x7FFF;
    enemy->node.field_4 = 1;
    Gp_UnlinkObj(&work->obj40);
    work->field_148 = 0;
    task->state++;
}

/// The two cue lines the bomb prints on its first two ticks. They are the run
/// this function's own assembly file carried at the head of the unit's rodata,
/// so they are written here, ahead of the two `INCLUDE_RODATA` blobs that
/// follow them in address order. The section attribute is load-bearing: at 8
/// bytes these fall under the compiler's small-data threshold and would
/// otherwise be emitted into `.sdata`, which the linker script does not lay out.
const char D_mine_cavern_8017D7E8[8] __attribute__((section(".rodata"))) = "BOMB1\n";
const char D_mine_cavern_8017D7F0[8] __attribute__((section(".rodata"))) = "BOMB2\n";

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", D_mine_cavern_8017D7F8);

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_9", RoomsShared80183c10Table);

/// Fourth state handler of `D_mine_cavern_8017D7F8` (`func_mine_cavern_80183A68`
/// dispatches it). It parks the model hidden (`field_C = 0x80`) and walks
/// `work->field_148` down its 0x3C-step countdown, one case per tick: 0 prints
/// "BOMB1", drops the model to y = -0x258 and spawns effect 0x01001200; 1
/// prints "BOMB2" and spawns 0x01000580, parking that effect's own first three
/// halfwords; 2 and 4 spawn 0x01002500; 3 and 5 clear the hidden bit on the
/// work block's second object (`objC0`); 9 hands `objC0` to `Gp_UnlinkObj`;
/// 0x3B advances `Task::state`.
void func_mine_cavern_801838F4(GpEnemy* arg0, Task* arg1)
{
    MineCavernWork* work;
    GpEffWork*      eff;
    u16             state;

    work = (MineCavernWork*)arg1->work;

    ((TmdObject*)arg1->extra)->flags = 0x80;

    state           = work->field_148;
    work->field_148 = state + 1;

    switch ((s16)state) {
        case 0:
            printf(D_mine_cavern_8017D7E8);
            ((TmdObject*)arg1->extra)->coords->coord.t[1] = -0x258;
            ((TmdObject*)arg1->extra)->coords->flg        = 0;
            Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
            Gp_SpawnEff(0x6005C, ((TmdObject*)arg1->extra)->coords, 0x01001200, NULL);
            return;

        case 1:
            printf(D_mine_cavern_8017D7F0);
            eff = Gp_SpawnEff(0x6005C, ((TmdObject*)arg1->extra)->coords, 0x01000580, NULL);
            if (eff != NULL) {
                eff->field_10 = 0;
                eff->field_12 = -0xA;
                eff->field_14 = 0;
            }
            return;

        case 2:
        case 4:
            Gp_SpawnEff(0x6005C, ((TmdObject*)arg1->extra)->coords, 0x01002500, NULL);
            return;

        case 3:
        case 5:
            work->objC0.flags &= 0x7FFF;
            return;

        case 9:
            Gp_UnlinkObj(&work->objC0);
            return;

        case 0x3B:
            arg1->state++;
            break;

        default:
            return;
    }
}

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

    work = (MineCavernWork*)task->work;

    ((TmdObject*)task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
    vec.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    vec.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    vec.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    func_800D7A9C(task->extra, &vec, 0, 3);

    if (!((GameFlag_GetNibble(0xE2) >> (u16)task->spawnArg1) & 1)) {
        ((TmdObject*)task->extra)->flags = 0x80;
    } else {
        m                         = &work->coord.coord;
        *(s32*)&work->coord.coord = 0x1000;
        *(s32*)&m->m[0][2]        = 0;
        *(s32*)&m->m[1][1]        = 0x1000;
        *(s32*)&m->m[2][0]        = 0;
        m->m[2][2]                = 0x1000;
        work->coord.sub           = ((TmdObject*)task->extra)->coords;
        work->coord.coord.t[2]    = 0;
        work->coord.coord.t[0]    = 0;
        work->coord.coord.t[1]    = -0x320;
        work->coord.flg           = 0;
        Gp_UpdateCoord(&work->coord);
        work->field_148++;
        ((TmdObject*)task->extra)->flags = 0;
    }
}
