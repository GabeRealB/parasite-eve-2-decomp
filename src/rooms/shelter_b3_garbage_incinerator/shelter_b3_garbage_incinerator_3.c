#include "common.h"

#include "actors/actors_shared_80133c6c.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

typedef struct {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} GarbageIncineratorState;

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

/// Places the task's model: re-parents its coordinate to the world frame, takes
/// the three longs of `placement` as the translation and applies the three
/// shorts as yaw, pitch and roll. The actors' shared library carries the same
/// body.
void func_shelter_b3_garbage_incinerator_8017E70C(Task* task, s32 arg1, ActorShared80133c6cPlacement* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}

extern void func_shelter_b3_garbage_incinerator_80185220(void);

void func_shelter_b3_garbage_incinerator_8017E7A4(GarbageIncineratorState* arg0)
{
    func_shelter_b3_garbage_incinerator_80185220();
    arg0->field_30 = 5;
}
