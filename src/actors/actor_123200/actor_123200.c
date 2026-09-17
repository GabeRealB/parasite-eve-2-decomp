#include "common.h"

#include "actors/actor_123200.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_actor_123200_801332E0(Task* task);

extern u8 D_80072729;

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_8013215C);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801324A4);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801329F0);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80132B94);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801332E0);

s32 func_actor_123200_80133450(Actor123200Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_220 == v) {
                goto same;
            }
            arg0->field_220 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_220 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_220 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_8013352C);

/// Steps `coord` 5/0x1000 of the way along its own forward axis (column 2 of
/// its rotation, normalised and GPF-scaled) and flags it for rebuild. The
/// direction vector lives in an `SVECTOR` carved off the scratch head and
/// handed straight back.
static __inline__ void Actor123200_StepForward(GsCOORDINATE2* coord)
{
    u8*      head;
    SVECTOR* dir;

    head       = (u8*)SCRATCH_SP;
    dir        = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_SP = (u32)dir;

    Gfx_MatrixCol2(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(5);
    gte_ldsv(dir);
    gte_gpf12_real();
    gte_stsv(dir);

    coord->coord.t[0] += dir->vx;
    coord->coord.t[1] += dir->vy;
    coord->coord.t[2] += dir->vz;
    coord->flg         = 0;

    SCRATCH_SP = (u32)((u8*)SCRATCH_SP + sizeof(SVECTOR));
}

/// Per-frame handler of a `actor_123200` instance. A pending restart on the
/// work block's `field_4` re-arms the model -- clearing `TmdObject.field_C`
/// and its buffers, rewriting the 0x1B0/0x1B2/0x1B4 pair, the motion state
/// `field_170`/`field_174` and the frame counter `field_6`, and raising
/// `arg0->field_14` -- and returns. Otherwise the frame counter runs, 0xC bytes
/// are reserved off the scratch head, and unless the game is frozen the model
/// is stepped forward along its facing; the reservation is released around the
/// animation update and the model's coordinate is flagged for rebuild.
void func_actor_123200_80133820(Actor123200Ctx* arg0, Task* task)
{
    Actor123200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    work = (Actor123200Work*)task->idMap;
    if (work->field_4 != 0) {
        obj            = (TmdObject*)task->extra;
        arg0->field_14 = 1;
        obj->field_C   = 0;
        Tmd_AllocBuffers(obj);
        work->field_1B0 = 0x115D;
        work->field_1B2 = 1;
        work->field_1B4 = 0x12D5;
        work->field_174 = 2;
        work->field_170 = 2;
        func_actor_123200_801332E0(task);
        ((TmdObject*)task->extra)->field_8->flg = 0;
        work->field_6                           = 0;
        return;
    }
    work->field_6++;
    SCRATCH_SP -= 0xC;
    coord       = ((TmdObject*)task->extra)->field_8;
    if (D_80072729 != 1) {
        Actor123200_StepForward(coord);
    }
    func_actor_123200_801332E0(task);
    SCRATCH_SP                             += 0xC;
    ((TmdObject*)task->extra)->field_8->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801339F0);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80133BA0);

s32 func_actor_123200_80133E30(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor123200Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor123200Work*)task->idMap;
    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 2:
            obj->field_C |= 4;
            work->field_0 = 0;
            break;
        case 3:
        case 4:
            obj->field_C  = 0;
            work->field_0 = 0;
            obj->field_C |= 4;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80133EDC);
