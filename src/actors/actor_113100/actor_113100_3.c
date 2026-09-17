#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_113100.h"

#include "main/gameflag.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

extern void func_actor_113100_801331E8(Task*, s32, Actor113100AnimPreset*, s32);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100_3", D_actor_113100_80131E48);

void func_actor_113100_80132F40(Task* arg0)
{
    Actor113100Work* work;
    s32              flag;

    work = (Actor113100Work*)arg0->idMap;
    flag = GameFlag_GetNibble(0xED);
    if (flag > 0 && work->field_53E == 0) {
        func_actor_113100_80132790(arg0, 0, 1, 0);
        func_80183BAC(0);
    }
    work->field_53E = flag;
}

/// Dispatches the actor's four main-body handlers by the animation slot index
/// `field_532` counts up in `func_actor_113100_8013301C`.
void func_actor_113100_80132FB4(Task* arg0)
{
    Actor113100Work* work;
    TaskFuncTable4   sp;

    work = (Actor113100Work*)arg0->idMap;
    sp   = D_actor_113100_80131E48;
    sp.funcs[(s16)work->field_532](arg0);
}

/// Builds the offset from the actor's own translation (work + 0x4F0) to the
/// root part's coordinate translation and stores its yaw into the work block,
/// then dispatches animation preset 0x7D3 through `func_actor_113100_801331E8`
/// and counts the frame. The preset is built on this function's stack: it
/// carries the slot index, the animation id and the two per-slot arguments.
///
/// `preset` is declared before `delta` / `dir` on purpose -- the stack slots
/// land at 0x10, 0x28 and 0x38 only in that order (GCC assigns the frame in
/// declaration order, and the 16-byte `VECTOR` is 8-byte aligned).
void func_actor_113100_8013301C(Task* arg0)
{
    Actor113100Work*      work;
    GsCOORDINATE2*        coord;
    Actor113100AnimPreset preset;
    VECTOR                delta;
    SVECTOR               dir;

    work  = (Actor113100Work*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;

    delta.vx = work->field_4F0 - coord->coord.t[0];
    delta.vy = work->field_4F4 - coord->coord.t[1];
    delta.vz = work->field_4F8 - coord->coord.t[2];
    VectorNormalS(&delta, &dir);
    work->field_53A = ratan2(dir.vx, dir.vz);

    preset.field_0  = 0;
    preset.field_4  = 0x16;
    preset.field_8  = 1;
    preset.field_C  = 4;
    preset.field_10 = 0;
    func_actor_113100_801331E8(arg0, 0x7D3, &preset, 0);
    work->field_532++;
}

void func_actor_113100_801330E8(Task* arg0)
{
    Actor113100Work*      work;
    Actor113100MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor113100AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor113100Work*)arg0->idMap;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_52A - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy          = work->field_52A;
        preset.field_0  = 0;
        preset.field_4  = work->field_477;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_113100_801331E8(arg0, 0x7D3, &preset, 0);
        work->field_530 = 0;
        work->field_532 = 0;
    }

    words          = (Actor113100MatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100_3", func_actor_113100_801331E8);
