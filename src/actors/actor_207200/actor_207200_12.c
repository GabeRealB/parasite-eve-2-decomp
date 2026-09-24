#include "common.h"
#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actors_shared_80135b58.h"
#include "actors/actors_shared_8014df20.h"
#include "actors/actor_207200.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Ramps the small enemy's light blend up or down depending on the flag at
/// `field_2A6`. Rising, the first frame switches the display object to light
/// mode 2 and the counter saturates at 0x12, where it sets the enemy node's
/// flag and the model's 0x80 bit. Falling, leaving 0x12 clears the node flag
/// and returns the object to light mode 0, and the counter bottoms out at 0
/// with the model bits cleared.
void func_actor_207200_8014B04C(Task* task)
{
    ActorShared8014df20Work* work;
    GpEnemy*                 enemy;
    TmdObject*               obj;

    work  = (ActorShared8014df20Work*)task->work;
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

/// Rebuilds the model's root coordinate from the matrix saved in
/// `field_264`, scaled along Y by `field_2A0`, which decays by 0x50 a frame
/// while above 0x200. The scaling matrix and its vector are staged in 0x30
/// bytes of the scratch stack; the node's `flg` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
void func_actor_207200_8014B128(Task* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor207200Work*            work;

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

/// Exit callback of the small enemy: detaches the enemy's hit records,
/// unlinks its node and the work's three objects, then runs the common enemy
/// exit.
void func_actor_207200_8014B21C(Task* task)
{
    ActorShared8014df20Work* work;
    GpEnemy*                 enemy;

    enemy = task->spawnArg2;
    work  = (ActorShared8014df20Work*)task->work;

    enemy->recs = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_14C);
    Gp_UnlinkObj(&work->field_FC);
    Gp_UnlinkObj(&work->field_184);
    Gp_EnemyTaskExit(task);
}
