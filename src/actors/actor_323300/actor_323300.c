#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_323300.h"

#include "gameplay/gameplay.h"

/// Allocates the 0x504 `Actor323300Work` this actor's whole lifetime runs on,
/// seeds the `GpRec18` collision table and the display node at +0x480, then
/// binds the three message handlers and the animation presets the state
/// functions drive. Bails out through `Gp_EnemyTaskExit` when the room flag
/// 0x60 is already set (the actor already spawned) or the allocation fails.
void func_actor_323300_80161E78(Task* arg0)
{
    Actor323300Work* work;
    TmdObject*       extra;
    GpObj*           obj;

    if (GameFlag_GetNibble(0x60) != 0 || (work = Mem_Calloc(0x504, 0)) == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->idMap     = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_500 = 1;
    work->field_502 = -1;
    func_actor_323300_801626D0(arg0);
    extra         = arg0->extra;
    obj           = &work->obj;
    obj->field_8  = extra->field_8 + 1;
    obj->field_C  = &work->rec;
    obj->field_18 = 0x30000;
    obj->field_10 = 0;
    obj->field_12 = 0;
    obj->field_14 = 0;
    obj->field_1C = 0x100;
    obj->flags    = 1;
    Gp_LinkObj(2, obj);
    obj->flags |= 0x8000;
    Gp_InitRec18Table(obj->field_C, 1, 0);
    arg0->field_24 = &D_actor_323300_80172574;
    func_actor_323300_80162208(arg0, 0x7D5, 0, 0);
    func_actor_323300_801629F0(arg0, 0x7D3, &D_actor_323300_8017259C, 0);
    func_actor_323300_801628B8(arg0, 0x7D3, &D_actor_323300_801725B4, 0);
    SndEvt_EnqueueType6(0x52100006, 0, 0x28);
    arg0->exitCallback = func_actor_323300_8016269C;
    arg0->state       += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80161FE8);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162208);

INCLUDE_RODATA("actors/nonmatchings/actor_323300/actor_323300", D_actor_323300_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_323300/actor_323300", D_actor_323300_80161E24);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162360);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162630);

void func_actor_323300_8016269C(Task* arg0)
{
    Gp_UnlinkObj(&((Actor323300Work*)arg0->idMap)->obj);
    Gp_EnemyTaskExit(arg0);
}

void func_actor_323300_801626D0(Task* arg0)
{
    TmdObject*       ext;
    Actor323300Work* work;

    ext           = arg0->extra;
    work          = (Actor323300Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_323300_801626EC(void)
{
}

void func_actor_323300_801626F4(Task* arg0)
{
    Actor323300Work* work                = (Actor323300Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_323300_80162748,
        func_actor_323300_801627B4,
    };

    states[(s16)work->field_4FE](arg0);
}

void func_actor_323300_80162748(Task* arg0)
{
    Actor323300Work* work;
    s32              i;

    work = (Actor323300Work*)arg0->idMap;
    func_actor_323300_801628B8(arg0, 0x7D3, &D_actor_323300_801725C8, 0);
    for (i = 1; i < 0x13; i++) {
        work->slots[i].field_9 = 8;
    }
    work->field_4CC = 0;
    work->field_4D0 = 0;
    work->field_4D4 = 0;
    work->field_4FE++;
}

/// State handler at index 1 of the two-entry table `func_actor_323300_801626F4`
/// dispatches, the turn-to-face body whose twins are
/// `func_actor_335800_801631A4`, `func_actor_113100_801330E8` and
/// `func_actor_350700_80162764`. Euler-extracts the root coordinate into `vec`
/// and, while the yaw gap to the target `work->field_4F6` stays under 0x41,
/// snaps `vec.vy` to that target, plays anim 0x7D3 through
/// `func_actor_323300_801628B8` and parks all 18 animation slots at 0x16 --
/// `field_4FC` and `field_4FE` go back to zero, so the handler re-runs. A wider
/// gap steps `vec.vy` toward the target by 0x40 instead. Either way the root
/// coordinate is rebuilt as the identity matrix rotated by `vec`, with `flg`
/// cleared so the next `Gp_UpdateCoord` recomputes it.
void func_actor_323300_801627B4(Task* arg0)
{
    Actor323300Work*   work;
    Actor323300Matrix* words;
    GsCOORDINATE2*     coord;
    SVECTOR            vec;
    s16                diff;
    s32                vy;
    s32                i;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor323300Work*)arg0->idMap;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4F6 - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy = work->field_4F6;
        func_actor_323300_801628B8(arg0, 0x7D3, &D_actor_323300_801725DC, 0);
        for (i = 1; i < 0x13; i++) {
            work->slots[i].field_9 = 0x16;
        }
        work->field_4FC = 0;
        work->field_4FE = 0;
    }

    words                = (Actor323300Matrix*)&coord->coord;
    words->ident.m00_m01 = 0x1000;
    words->ident.m02_m10 = 0;
    words->ident.m11_m12 = 0x1000;
    words->ident.m20_m21 = 0;
    words->ident.m22     = 0x1000;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_801628B8);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_801629F0);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162A6C);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162BE4);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162DF0);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80163188);

void func_actor_323300_801634B0(Task* arg0)
{
    GsCOORDINATE2* base;
    GsCOORDINATE2* node;
    GsCOORDINATE2* sub;

    do {
        base      = ((TmdObject*)arg0->extra)->field_8;
        sub       = base + 3;
        node      = base + 4;
        node->sub = sub;
    } while (0);
    sub                                       = ((TmdObject*)arg0->extra)->field_8 + 5;
    sub->sub                                  = node;
    ((TmdObject*)arg0->extra)->field_8[6].sub = sub;
    Task_Kill(arg0);
}

/// Splats an identity light/colour pair into the `Mem_Calloc(0x6B0)` work block
/// `func_actor_323300_80162BE4` parked in `Task::idMap`, republishes them onto
/// `TmdObject::field_1C` / `field_20`, then re-derives model part 1's world
/// matrix -- clearing its dirty flag, rebuilding it from its parent and
/// rebinding the actor's shading to the part's translation.
void func_actor_323300_80163510(Task* arg0)
{
    Actor323300MtxWork* work;
    Actor323300Matrix*  light;
    Actor323300Matrix*  color;
    GsCOORDINATE2*      coords;
    TmdObject*          extra;

    extra  = arg0->extra;
    work   = (Actor323300MtxWork*)arg0->idMap;
    coords = extra->field_8;

    work->light.ident.m00_m01 = 0x1000;
    light                     = &work->light;
    light->ident.m02_m10      = 0;
    light->ident.m11_m12      = 0x1000;
    light->ident.m20_m21      = 0;
    light->ident.m22          = 0x1000;

    work->color.ident.m00_m01 = 0x1000;
    color                     = &work->color;
    color->ident.m02_m10      = 0;
    color->ident.m11_m12      = 0x1000;
    color->ident.m20_m21      = 0;
    color->ident.m22          = 0x1000;

    extra->field_1C = &light->mat;
    extra->field_20 = &color->mat;

    coords[1].flg = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// Re-aims the per-part coordinate nodes at index 5 and index 2 from one turn
/// angle: the angle is clamped to +-0x400 -- a quarter turn either way -- then
/// `func_actor_323300_80163188` rebuilds node 5 from two thirds of it and node
/// 2 from half, and nodes 5 down to 2 have their dirty flag cleared so the next
/// `Gp_UpdateCoord` re-derives them. The lower clamp tests `arg1` rather than
/// the clamped copy; that is the same test, because the upper clamp has already
/// pinned the copy to 0x400 whenever the angle was out of range upwards.
void func_actor_323300_8016359C(Task* arg0, s16 arg1)
{
    s16 var;

    var = arg1;
    if (var > 0x400) {
        var = 0x400;
    }
    if (arg1 < -0x400) {
        var = -0x400;
    }

    func_actor_323300_80163188(&((TmdObject*)arg0->extra)->field_8[5], (var * 2) / 3);
    func_actor_323300_80163188(&((TmdObject*)arg0->extra)->field_8[2], var / 2);

    ((TmdObject*)arg0->extra)->field_8[5].flg = 0;
    ((TmdObject*)arg0->extra)->field_8[4].flg = 0;
    ((TmdObject*)arg0->extra)->field_8[3].flg = 0;
    ((TmdObject*)arg0->extra)->field_8[2].flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_8016369C);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80163718);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80163840);
