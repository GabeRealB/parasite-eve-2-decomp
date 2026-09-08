#include "common.h"

#include "actors/actors_shared_801324fc.h"
#include "main/sound.h"
#include "main/tmd.h"

/// Plays a step sound whenever animation slot 1 rolls onto a new record whose
/// flags nibble is 0x10 or 0x20 - the two feet - panned and attenuated from
/// the second coordinate of the task's model. The record is latched in
/// `field_4B8` so each one fires once.
void ActorsShared801324fc(Task* task)
{
    ActorsShared801324fcWork* work;
    GpObj38*                  obj;
    GpAnimRec*                rec;
    s32                       kind;
    s32                       id;
    s32                       pan;

    work = (ActorsShared801324fcWork*)task->idMap;
    obj  = (GpObj38*)(((TmdObject*)task->extra)->field_8 + 1);
    rec  = Gp_AnimGetRec(&work->anim, &work->slots[1]);
    if (rec == NULL || rec == work->field_4B8) {
        return;
    }
    work->field_4B8 = rec;
    kind            = rec->field_3 & 0x30;
    if (kind != 0x10 && kind != 0x20) {
        return;
    }
    id = 0x1000000F;
    if (kind == 0x10) {
        id = 0x10000010;
    }
    id += 0x64;
    pan = (s8)Gp_GetObjPan(obj);
    SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth(obj));
}
