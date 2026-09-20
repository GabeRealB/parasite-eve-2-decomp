#include "common.h"

#include "actors/actors_shared_80137fb8.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/tmd.h"

extern u8 D_801153F4;
extern u8 D_8007216C;

void ActorsShared80137fb8(Task* task)
{
    ActorsShared80137fb8Work* work;
    GpActorD4Rec*             d4;
    GpRec18*                  rec;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            soundCoord;
    Task*                     child;
    u32                       map;
    u32                       mask;
    s32                       flag;
    s32                       id;
    s16                       countdown;

    work = (ActorsShared80137fb8Work*)task->work;
    map  = *(u32*)&D_8007216C;
    mask = 0xFFFF0000;
    SOFT_TOUCH_REG(mask);
    map       &= mask;
    flag       = map == 0x03200000;
    coord      = ((TmdObject*)task->extra)->coords;
    soundCoord = (GsCOORDINATE2*)coord;
    d4         = &work->rec;
    if (D_801153F4 == 0) {
        d4->end1.vx        = -(u16)work->vel.vx;
        d4->end1.vy        = -(u16)work->vel.vy;
        d4->end1.vz        = -(u16)work->vel.vz;
        coord->coord.t[0] += (s16)work->vel.vx;
        rec                = work->rec18;
        coord->coord.t[1] += (s16)work->vel.vy;
        coord->coord.t[2] += (s16)work->vel.vz;
        coord->flg         = 0;
        work->vel.vy       = (u16)work->vel.vy + 0xA;
        if (Gp_CountRec18Hi(rec, 0x10000) != 0) {
            child = task->firstChild;
            if (child != NULL) {
                child->spawnArg1 = 3;
            }
            goto fire;
        }
        if (Gp_FindRec18(rec, 0) != 0) {
            child = task->firstChild;
            if (child != NULL) {
                if (rec->at10.normal.vy >= -0xC00) {
                    child->spawnArg1 = 3;
                } else {
                    child->spawnArg1 = 2;
                }
            }
        fire:
            id = (flag << 22) | (0x400B000B | (ActorsShared80137fb8ActorId << 8));
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(soundCoord), (s8)gpGetObjDepth(soundCoord));
            work->obj.flags    &= 0x3FFF;
            task->killCountdown = 0x1E;
            task->state        += 1;
        }
        Gp_ClearRec18Occupied(work->rec18);
        countdown           = (u16)task->killCountdown - 1;
        task->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            Task_CallExit(task);
        }
    }
}
