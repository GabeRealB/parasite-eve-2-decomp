#include "common.h"

#include "actors/actor_461800.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Footstep sounds of the second variant: whenever animation slot 1 rolls onto
/// a new record whose flags nibble is 0x10 or 0x20, plays the matching step
/// sound, panned and attenuated from the model's second coordinate. The record
/// is latched in `field_4B8` so each one fires once.
void func_actor_461800_8013365C(Task* task)
{
    Actor461800Work2* work;
    GsCOORDINATE2*    obj;
    GpAnimRec*        rec;
    s32               kind;
    s32               id;
    s32               pan;

    work = (Actor461800Work2*)task->work;
    obj  = ((TmdObject*)task->extra)->coords + 1;
    rec  = Gp_AnimGetRec(&work->anim, &work->slots[1]);
    if (rec == NULL || rec == work->field_4B8) {
        return;
    }
    work->field_4B8 = rec;
    kind            = rec->flags & 0x30;
    if (kind != 0x10 && kind != 0x20) {
        return;
    }
    id = 0x1000000F;
    if (kind == 0x10) {
        id = 0x10000010;
    }
    id += 0x64;
    pan = (s8)Gp_GetObjPan(obj);
    SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(obj));
}
