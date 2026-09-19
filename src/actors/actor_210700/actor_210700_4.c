#include "common.h"

#include "actors/actor_210700.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

extern GpImgRec D_actor_210700_80157F4C;
extern GpImgRec D_actor_210700_8015826C;
extern GpImgRec D_actor_210700_8015858C;

/// Message-0x7D5 handler: the four-way visibility/mode switch on the message's
/// mode word, run against the `TmdObject` parked in `Task::extra`. `field_C`
/// bit 0x80 marks the model hidden -- its `Tmd_Create` initial value, and the
/// bit `taskKill`'s type-1 path sets -- while bit 0x4 is the one modes 2 and 3
/// raise. Mode 0 hides the model and drops 0x4, 1 shows it, reinstates the aux
/// buffers through `Tmd_AllocBuffers` and drops 0x4, 2 hides it and latches the
/// mode into the work block's `field_53E`, and 3 shows it while raising 0x4.
/// Anything else returns 1 and leaves the object alone; the handled modes
/// return 0.
/// The handler reads the `Task::actor` pointer before the switch even though
/// mode 2 is its only use, so retail's `lw $v1,0x1C($a0)` sits in the entry
/// block. The same body shape as `ActorsShared80162bc4` and
/// `func_actor_511000_801327A0`.
s32 func_actor_210700_8014A3D4(GpActorWork* arg0, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    Actor210700Work* work;
    s32              ret;

    obj  = arg0->extra;
    work = (Actor210700Work*)arg0->actor;
    ret  = 0;

    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags     |= 0x80;
            work->field_53E = mode;
            obj->flags     |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message-0x7E0 handler: uploads one of the actor's three texture records over
/// the 0x18x0x10 rect at y 0x28 -- `D_actor_210700_8015858C` for mode 1,
/// `D_actor_210700_80157F4C` for modes 0 and 2, and `D_actor_210700_8015826C`
/// for mode 3, which sets the work block's `field_53C` / `field_538` to 1
/// first. Any other mode leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_210700_8014A4B0(GpActorWork* arg0, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            img = &D_actor_210700_8015858C;
            break;
        case 0:
        case 2:
            img = &D_actor_210700_80157F4C;
            break;
        case 3:
            ((Actor210700Work*)arg0->actor)->field_53C = 1;
            ((Actor210700Work*)arg0->actor)->field_538 = 1;
            img                                        = &D_actor_210700_8015826C;
            break;
        default:
            img = NULL;
            break;
    }

    if (img != NULL) {
        ret = Gp_LoadActorImage(arg0, img, &rect);
    }
    return ret;
}
