#include "common.h"

#include "actors/actor_113000.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// The actor's three texture records, one per mode of the message-0x7E0
/// handler below. Each is a lone `GpImgRec` whose 0x20x0x10 source rect repeats
/// the size the handler's scratch `RECT` carries and whose `data` points at its
/// pixel blob; the three sit 0x420 bytes apart in the overlay's data segment.
extern GpImgRec D_actor_113000_8013A32C;
extern GpImgRec D_actor_113000_8013A74C;
extern GpImgRec D_actor_113000_8013AB6C;

INCLUDE_ASM("actors/nonmatchings/actor_113000/actor_113000_3", func_actor_113000_80132398);

/// Message-0x7E0 handler: uploads one of the actor's three texture records over
/// the 0x20x0x10 rect at y 0x28 -- `D_actor_113000_8013AB6C` for mode 1,
/// `D_actor_113000_8013A32C` for modes 0 and 2, and `D_actor_113000_8013A74C`
/// for mode 3, which sets the work block's `field_4C4` / `field_4C0` to 1
/// first. Any other mode leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_113000_80132474(GpActorWork* arg0, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x20;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            img = &D_actor_113000_8013AB6C;
            break;
        case 0:
        case 2:
            img = &D_actor_113000_8013A32C;
            break;
        case 3:
            ((Actor113000Work*)arg0->actor)->field_4C4 = 1;
            ((Actor113000Work*)arg0->actor)->field_4C0 = 1;
            img                                        = &D_actor_113000_8013A74C;
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
