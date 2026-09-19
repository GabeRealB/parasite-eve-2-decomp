#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_311900.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_311900_80161E3C(Task* task, s32 arg1, s16 arg2);

void func_actor_311900_80162100(Task* task);

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_311900_8016EBF4[];

/// Non-zero while the game is paused, which freezes the per-frame step below.
extern u8 D_80072729;

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// The `ActorsShared80135df4Table` spawn handler -- the actor's second setup
/// path, reached through the three-entry table whose tick is
/// `func_actor_311900_801625F0`. It is the same setup `func_actor_311900_8016228C`
/// performs for the first table, under different conditions: the enemy is torn
/// down instead while game flag 1 has already reached nibble 3, and the work
/// block gets the light / colour pair `func_actor_311900_8016281C` splats
/// (rather than `func_actor_311900_8016278C`'s) from a different animation run
/// (`D_actor_311900_8016EBF4`, not `D_actor_311900_8016EBE8`).
///
/// The 0x4CC-byte block goes into `Task::work` -- that slot is not a
/// `TaskIdMap` here. `GpEnemy::field_4` takes the model's root coordinate's
/// matrix, the root's `sub` is re-parented to `gGfxViewCoord`, the animation
/// context is built over the block's slot array and packed-pose run, and the
/// two work halfwords 0x474 / 0x478 seed the tick's state. Note this handler,
/// unlike `func_actor_311900_8016228C`, does not touch `field_4C4` / `field_4C6`
/// or the model's `field_C`.
void func_actor_311900_801624F8(GpEnemy* enemy, Task* task)
{
    Actor311900Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (GameFlag_GetNibble(1) >= 3 ||
        (work = memCalloc(0x4CC, 0), task->work = (TaskIdMap*)work, work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    func_actor_311900_8016281C(task);
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    obj->flags      = 0;
    func_800B3F84((GpAnimCtx*)work, D_actor_311900_8016EBF4, obj, work->anim.poses,
                  work->anim.slots);
    coord->sub      = &gGfxViewCoord;
    work->field_474 = 2;
    work->field_478 = 1;
    func_actor_311900_80162100(task);
    task->state += 1;
}

void func_actor_311900_801625F0(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;

    obj = (TmdObject*)task->extra;
    func_actor_311900_80161E3C(task, 4, 2);
    if ((Gp_GetViewIndex() & 0xFF) == 0xB) {
        obj->flags = 0;
    } else {
        obj->flags = 0x80;
    }
    func_actor_311900_80162100(task);
}

/// Takes `arg1` as a signed 16-bit step, builds a direction vector from
/// `arg0->coord`'s rotation with `Gfx_MatrixCol2`, normalizes it with
/// `VectorNormalSS`, scales it by the step on the GTE, adds it to
/// `arg0->coord.t` and clears `arg0->flg`. Returns the step, or 0 having
/// touched nothing while the game is paused (`D_80072729 == 1`) or when the
/// step is zero. `arg0` is the per-part `GsCOORDINATE2` the caller takes from
/// `TmdObject::coords`.
///
/// The scratch-pad vector is carved out under two names: `vec`, which the
/// frame update stores and the calls normalize, and `gte`, which the GTE round
/// trip reads and writes back. The object keeps them apart, and that is what
/// the copy ahead of the `if` is.
s32 func_actor_311900_80162658(GsCOORDINATE2* arg0, s16 arg1)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gte;

    if (D_80072729 == 1) {
        return 0;
    }
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 1;
    gte                        = head - 1;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    if (arg1 != 0) {
        SOFT_TOUCH_REG(vec);
        Gfx_MatrixCol2(&arg0->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(arg1);
        gte_ldsv(gte);
        gte_gpf12_real();
        gte_stsv(gte);
        arg0->coord.t[0] += head[-1].vx;
        arg0->coord.t[1] += vec->vy;
        arg0->coord.t[2] += vec->vz;
        arg0->flg         = 0;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 1;
    return arg1;
}

/// Splats an identity light / colour matrix pair into the work block the spawn
/// state carved out of `Task::work`, republishes both onto the
/// `TmdObject::lightMtx` / `field_20` slots that the renderer otherwise reads
/// from `Gp_DefaultMtx` / `Gp_DefaultMtx2`, and then overwrites each 3x3 with
/// the values the actor lights its model with -- the light matrix flat except
/// for `m[1][0]` and `m[2][2]`, the colour matrix fully pass-through.
void func_actor_311900_8016278C(Task* task)
{
    Actor311900MatWords* color;
    Actor311900MatWords* light;
    TmdObject*           ext;
    Actor311900Work*     work;

    work  = (Actor311900Work*)task->work;
    ext   = task->extra;
    light = (Actor311900MatWords*)&work->light;
    color = (Actor311900MatWords*)&work->color;

    light->ident.m00_m01 = 0x1000;
    light->ident.m02_m10 = 0;
    light->ident.m11_m12 = 0x1000;
    light->ident.m20_m21 = 0;
    light->ident.m22     = 0x1000;

    color->ident.m00_m01 = 0x1000;
    color->ident.m02_m10 = 0;
    color->ident.m11_m12 = 0x1000;
    color->ident.m20_m21 = 0;
    color->ident.m22     = 0x1000;

    ext->lightMtx = &work->light;

    work->color.m[0][0] = 0x1000;
    work->color.m[0][1] = 0x1000;
    work->color.m[0][2] = 0x1000;
    work->color.m[1][0] = 0x1000;
    work->color.m[1][1] = 0x1000;
    work->color.m[1][2] = 0x1000;
    work->color.m[2][0] = 0x1000;
    work->color.m[2][1] = 0x1000;
    work->color.m[2][2] = 0x1000;

    work->light.m[0][0] = 0x1000;
    work->light.m[0][1] = 0x1000;
    work->light.m[0][2] = 0x1000;
    work->light.m[1][0] = 0;
    work->light.m[1][1] = 0x1000;
    work->light.m[1][2] = 0x1000;
    work->light.m[2][0] = 0x1000;
    work->light.m[2][1] = 0x1000;
    work->light.m[2][2] = 0;

    ext->colorMtx = &work->color;
}

/// Same splat as `func_actor_311900_8016278C`, republishing the light / colour
/// pair onto `TmdObject::lightMtx` / `field_20` between the identity seed and
/// the per-actor values: the colour matrix goes fully pass-through, the light
/// matrix flat except for a negated `m[0][0]`.
void func_actor_311900_8016281C(Task* task)
{
    Actor311900MatWords* color;
    Actor311900MatWords* light;
    TmdObject*           ext;
    Actor311900Work*     work;

    work  = (Actor311900Work*)task->work;
    ext   = task->extra;
    light = (Actor311900MatWords*)&work->light;
    color = (Actor311900MatWords*)&work->color;

    light->ident.m00_m01 = 0x1000;
    light->ident.m02_m10 = 0;
    light->ident.m11_m12 = 0x1000;
    light->ident.m20_m21 = 0;
    light->ident.m22     = 0x1000;

    color->ident.m00_m01 = 0x1000;
    color->ident.m02_m10 = 0;
    color->ident.m11_m12 = 0x1000;
    color->ident.m20_m21 = 0;
    color->ident.m22     = 0x1000;

    ext->lightMtx = &work->light;

    work->color.m[0][0] = 0x1000;
    work->color.m[0][1] = 0x1000;
    work->color.m[0][2] = 0x1000;
    work->color.m[1][0] = 0x1000;
    work->color.m[1][1] = 0x1000;
    work->color.m[1][2] = 0x1000;
    work->color.m[2][0] = 0x1000;
    work->color.m[2][1] = 0x1000;
    work->color.m[2][2] = 0x1000;

    work->light.m[0][0] = -0x1000;
    work->light.m[0][1] = 0x1000;
    work->light.m[0][2] = 0x1000;
    work->light.m[1][0] = 0x1000;
    work->light.m[1][1] = 0x1000;
    work->light.m[1][2] = 0x1000;
    work->light.m[2][0] = 0x1000;
    work->light.m[2][1] = 0x1000;
    work->light.m[2][2] = 0x1000;

    ext->colorMtx = &work->color;
}
