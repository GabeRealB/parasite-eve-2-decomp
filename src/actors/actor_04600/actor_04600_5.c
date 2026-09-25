#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actor_104600.h"

/// Dormant arm of the first enemy's reaction dispatch. A 0x10000-class contact
/// on the record at `rec11C` latches `field_2D8`; a latched enemy moves to the
/// live stage, drops the 0x8000 bit of its first body and arms state 0xF0. The
/// record is released either way. While animation 1 plays, `field_2D0` counts
/// down to an idle sound (re-rolled to 0x50..0xB3 frames, `field_2D6` picking
/// the sound set), the step length follows the frame count - 0x14 in the first
/// window, -0x14 in the second - the count wraps at 0x63, and the root takes one
/// step. Eight bytes of the scratch stack are held across the whole arm.
void Actor04600_Fn005B0(Task* arg0)
{
    Actor104600Work* work;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s32              soundId;
    u32              rng;

    coord              = ((TmdObject*)arg0->extra)->coords;
    work               = (Actor104600Work*)arg0->work;
    *(u32*)0x1F8003FC -= 8;
    if (Gp_CountRec18Hi(&work->rec11C, 0x10000) != 0) {
        work->field_2D8 = 1;
    }
    if (work->field_2D8 != 0) {
        work->field_2B2   = 1;
        work->field_2C8   = 1;
        work->objFC.flags = (u16)(work->objFC.flags & 0x7FFF);
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(&work->rec11C);
    if (work->field_2B8 == 1) {
        countdown       = work->field_2D0 - 1;
        work->field_2D0 = countdown;
        if ((countdown << 16) <= 0) {
            rng             = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState     = rng;
            work->field_2D0 = (u16)((rng >> 16) % 100 + 0x50);
            if (work->field_2D6 != 0) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40460009;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x402E0001;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
        }
        work->field_2C6 = 1;
        work->field_2BE = 0;
        if ((u32)(work->field_2BC - 1) < 0x29) {
            work->field_2BE = 0x14;
        }
        if ((u32)(work->field_2BC - 0x33) < 0x29) {
            work->field_2BE = -0x14;
        }
        if ((s16)work->field_2BC >= 0x63) {
            work->field_2BC = 0;
        }
        Actor04600_Fn0272C(arg0);
    }
    *(u32*)0x1F8003FC += 8;
}
