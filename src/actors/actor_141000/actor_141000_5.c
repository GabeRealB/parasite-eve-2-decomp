#include "common.h"

#include "actors/actor_141000.h"
#include "gameplay/3A34.h"

#include "main/task.h"
#include "main/tmd.h"

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`.
/// Mode 0 shows the model and clears the 4 flag, 1 hides it, frees the aux
/// buffers and clears the flag, 2 does both plus latching the mode into the
/// work block's `field_4C9`, and 3 hides it while setting the flag. Anything
/// else returns 1 and leaves the object alone; the handled modes return 0.
/// The same body shape as `func_actor_503500_80132584`.
s32 func_actor_141000_80133E8C(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            obj->field_C |= 0x80;
            obj->field_C &= ~4;
            break;
        case 1:
            obj->field_C &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->field_C &= ~4;
            break;
        case 2:
            obj->field_C                              |= 0x80;
            ((Actor141000Work*)task->idMap)->field_4C9 = mode;
            obj->field_C                              |= 4;
            break;
        case 3:
            obj->field_C &= ~0x80;
            obj->field_C |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_141000_80133F6C(Task* task, s32 arg1, Actor141000Msg* msg)
{
    Actor141000Work* work;

    work = (Actor141000Work*)task->idMap;
    switch (msg->field_2) {
        case 1:
            work->field_4C8 = 0;
            break;
        case 2:
            work->field_4C8 = 1;
            break;
    }
    return 0;
}

extern GpImgRec D_actor_141000_8013CA7C;
extern GpImgRec D_actor_141000_8013D28C;
extern GpImgRec D_actor_141000_8013D4DC;
extern GpImgRec D_actor_141000_8013D72C;

/// Image-load handler: picks one of the overlay's texture uploads by `mode`
/// and posts it through `Gp_LoadActorImage` over a scratch `RECT` -- the
/// 0x19x0x14 rect at (0, 0x40) for modes 0-3, the 0xEx0x14 rect at (0xC, 0x60)
/// for 4 and 5. Mode 3 also arms the work block's upload step and countdown
/// source. Unknown modes load nothing and return 0.
s32 func_actor_141000_80133FA8(Task* task, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret = 0;
    switch (mode) {
        case 0:
            img = &D_actor_141000_8013CA7C;
            goto small;
        case 1:
            img = &D_actor_141000_8013D28C;
            goto small;
        case 2:
            img = &D_actor_141000_8013D4DC;
        small:
            rect.y = 0x40;
            rect.w = 0x19;
            rect.x = 0;
            rect.h = 0x14;
            break;
        case 3:
            img                                        = &D_actor_141000_8013D72C;
            rect.y                                     = 0x40;
            rect.w                                     = 0x19;
            rect.x                                     = 0;
            rect.h                                     = 0x14;
            ((Actor141000Work*)task->idMap)->field_4CA = 1;
            ((Actor141000Work*)task->idMap)->field_4C4 = 1;
            break;
        case 4:
            img = &D_actor_141000_8013D72C;
            goto big;
        case 5:
            img = &D_actor_141000_8013D4DC;
        big:
            rect.x = 0xC;
            rect.y = 0x60;
            rect.w = 0xE;
            rect.h = 0x14;
            break;
        default:
            img = NULL;
            break;
    }
    if (img != NULL) {
        ret = Gp_LoadActorImage((GpActorWork*)task, img, &rect);
    }
    return ret;
}
