#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actors_shared_80135b58.h"
#include "actors/actor_104600.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Ramps the second enemy's light blend `field_2A4` up or down as `field_2A6`
/// says. Rising, the first frame switches the enemy to light mode 2 and the
/// blend saturates at 0x12, where the enemy's node flag and the model's 0x80
/// bit are set. Falling, leaving 0x12 clears the node flag and returns to light
/// mode 0, and the blend bottoms out at 0 with the model bits cleared.
void Actor04600_Fn03F30(Task* task)
{
    Actor104600Enemy2Work* work;
    GpEnemy*               enemy;
    TmdObject*             obj;

    work  = (Actor104600Enemy2Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    obj   = (TmdObject*)task->extra;

    if (work->field_2A6 != 0) {
        if (work->field_2A4 == 0) {
            work->field_2A4++;
            obj->flags = 2;
            Gp_SetLightMode(task->spawnArg2, 2);
        } else {
            work->field_2A4++;
            if (work->field_2A4 >= 0x12) {
                work->field_2A4   = 0x12;
                enemy->node.flags = 1;
                obj->flags        = 0x80;
            }
        }
    } else {
        if (work->field_2A4 == 0x12) {
            work->field_2A4--;
            enemy->node.flags = 0;
            obj->flags        = 2;
            Gp_SetLightMode(task->spawnArg2, 0);
        } else {
            work->field_2A4--;
            if (work->field_2A4 <= 0) {
                work->field_2A4 = 0;
                obj->flags      = 0;
            }
        }
    }
}

/// Rebuilds the second enemy's root coordinate from the matrix saved in
/// `field_264`, scaled along Y by `field_2A0`, which decays by 0x50 a frame
/// while above 0x200. The scaling matrix and its vector are staged in 0x30
/// bytes of the scratch stack; the node's `flg` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
void Actor04600_Fn0400C(Task* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor104600Enemy2Work*      work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->work;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = (*(TmdObject**)&arg0->extra)->coords;
    if (work->field_2A0 >= 0x201) {
        work->field_2A0 = (u16)work->field_2A0 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2A0;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_264;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}

/// Exit callback of the second enemy: detaches the enemy's contact records,
/// unlinks its node and the work's three bodies, then runs the common enemy
/// task exit.
void Actor04600_Fn04100(Task* task)
{
    Actor104600Enemy2Work* work;
    GpEnemy*               enemy;

    enemy = task->spawnArg2;
    work  = (Actor104600Enemy2Work*)task->work;

    enemy->recs = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_14C);
    Gp_UnlinkObj(&work->field_FC);
    Gp_UnlinkObj(&work->field_184);
    Gp_EnemyTaskExit(task);
}
