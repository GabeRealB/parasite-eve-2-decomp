#include "common.h"

#include "actors/actor_311900.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_311900_80161E3C(Task* task, s32 arg1, s32 arg2);

void func_actor_311900_80162100(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_311900/actor_311900_2", func_actor_311900_801624F8);

void func_actor_311900_801625F0(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;

    obj = (TmdObject*)task->extra;
    func_actor_311900_80161E3C(task, 4, 2);
    if ((Gp_GetViewIndex() & 0xFF) == 0xB) {
        obj->field_C = 0;
    } else {
        obj->field_C = 0x80;
    }
    func_actor_311900_80162100(task);
}

INCLUDE_ASM("actors/nonmatchings/actor_311900/actor_311900_2", func_actor_311900_80162658);

/// Splats an identity light / colour matrix pair into the work block the spawn
/// state carved out of `Task::idMap`, republishes both onto the
/// `TmdObject::field_1C` / `field_20` slots that the renderer otherwise reads
/// from `Gp_DefaultMtx` / `Gp_DefaultMtx2`, and then overwrites each 3x3 with
/// the values the actor lights its model with -- the light matrix flat except
/// for `m[1][0]` and `m[2][2]`, the colour matrix fully pass-through.
void func_actor_311900_8016278C(Task* task)
{
    Actor311900MatWords* color;
    Actor311900MatWords* light;
    TmdObject*           ext;
    Actor311900Work*     work;

    work  = (Actor311900Work*)task->idMap;
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

    ext->field_1C = &work->light;

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

    ext->field_20 = &work->color;
}

/// Same splat as `func_actor_311900_8016278C`, republishing the light / colour
/// pair onto `TmdObject::field_1C` / `field_20` between the identity seed and
/// the per-actor values: the colour matrix goes fully pass-through, the light
/// matrix flat except for a negated `m[0][0]`.
void func_actor_311900_8016281C(Task* task)
{
    Actor311900MatWords* color;
    Actor311900MatWords* light;
    TmdObject*           ext;
    Actor311900Work*     work;

    work  = (Actor311900Work*)task->idMap;
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

    ext->field_1C = &work->light;

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

    ext->field_20 = &work->color;
}
