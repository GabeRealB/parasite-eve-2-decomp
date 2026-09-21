#include "common.h"

#include "actors/actor_104900.h"
#include "actors/actors_shared_801357f0.h"
#include "actors/actors_shared_80135fdc.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/tmd.h"

extern u32 Gp_LcgState;

void ActorsShared80135fdc(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work)
{
    GsCOORDINATE2* coord;
    GsCOORDINATE2* yaw;
    s32            oy;
    s32            sy;
    GsCOORDINATE2* other;
    Task*          player;
    SVECTOR*       vec;
    u8*            head;
    u8*            tail;
    s32            delta;
    s32            lt;
    u16            angle;
    u16            time;
    s32            dist;
    s32            ret;
    u16            selfT;
    u16            otherT;
    s8             kind;
    u16            wait;
    u32            rng;

    wait = 0x3C;
    if ((s8)((Actor104900SpawnWork*)work)->field_BBB == 0x31) {
        wait = 0xA;
    }
    if (work->field_BA8 == 0) {
        work->field_BA4     = 4;
        work->field_BA8     = (u8)work->field_BA8 + 1;
        task->killCountdown = wait;
    }
    ActorsShared801357f0(enemy, task, work);

    delta = work->field_B90;
    lt    = delta < 0x11;
    yaw   = ((TmdObject*)task->extra)->coords;
    if (lt == 0) {
        angle = (u16)((GpCoordPose*)yaw)->field_46 + 0x10;
    } else if (delta < -0x10) {
        angle = (u16)((GpCoordPose*)yaw)->field_46 - 0x10;
    } else {
        angle = (u16)((GpCoordPose*)yaw)->field_46 + delta;
    }
    ((GpCoordPose*)yaw)->field_46 = angle;
    SCHED_BARRIER();
    angle                         = *(volatile u16*)&((GpCoordPose*)yaw)->field_46 & 0xFFF;
    ((GpCoordPose*)yaw)->field_46 = angle;
    Gfx_RotMatrixY(&yaw->coord, angle, 1);
    yaw->flg = 0;

    if ((u16)(work->field_B90 + 0x7F) < 0xFFU) {
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        work->state = ((rng >> 0x10) & 4) ? 0xC : 0xD;
        Gp_ArmStateF0(1);
        work->field_BA8 = 0;
        return;
    }

    time                = (u16)task->killCountdown - 1;
    task->killCountdown = time;
    if ((s16)time > 0) {
        return;
    }

    coord  = ((TmdObject*)task->extra)->coords;
    player = gameGetPtrSlot(3);
    if (player == NULL) {
        dist = 0x7FFFFFFF;
    } else {
        other  = ((TmdObject*)player->extra)->coords;
        selfT  = (u16)coord->workm.t[0];
        otherT = (u16)other->workm.t[0];
        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        head                       = *(u8**)(head + 0x3FC);
        ((SVECTOR*)(head - 8))->vx = otherT - selfT;
        oy                         = (u16)other->workm.t[1];
        sy                         = (u16)coord->workm.t[1];
        SOFT_TOUCH_REG2(oy, sy);
        vec = (SVECTOR*)(head - 8);
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(vec) : "memory");
        vec->vy = oy - sy;
        vec->vz = (u16)other->workm.t[2] - (u16)coord->workm.t[2];
        ret     = Gfx_ApplyMatrixNoSf(vec, vec);
        __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
        tail = *(u8**)(tail + 0x3FC);
        dist = ret;
        tail = tail + 8;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
    }

    kind = (s8)((Actor104900SpawnWork*)work)->field_BBB;
    if (((kind == 0xB) && (dist <= 0x89543F)) || ((kind == 0x31) && (dist <= 0x22550F))) {
        Gp_ArmStateF0(1);
        work->state         = 0xB;
        work->field_BA8     = 0;
        task->killCountdown = 0;
        return;
    }
    task->killCountdown = wait;
}
