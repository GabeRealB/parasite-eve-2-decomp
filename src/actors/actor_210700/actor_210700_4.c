#include "common.h"

#include "actors/actor_210700.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

extern GpImgRec D_actor_210700_80157F4C;
extern GpImgRec D_actor_210700_8015826C;
extern GpImgRec D_actor_210700_8015858C;

INCLUDE_ASM("actors/nonmatchings/actor_210700/actor_210700_4", func_actor_210700_8014A3D4);

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
