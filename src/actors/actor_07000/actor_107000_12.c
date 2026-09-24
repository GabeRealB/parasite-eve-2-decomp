#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Dormant arm of the caged specimen, the `field_2B2 == 0` arm of
/// `Actor07000_Fn00478`. The collision record at `field_11C` is polled for an
/// occupant of kind 0x10000; once one has been seen `field_2D8` stays latched,
/// and a latched specimen moves `field_2B2`/`field_2C8` to the live stage,
/// clears the 0x8000 bit of the render node's flag word and arms the global
/// state through `Gp_ArmStateF0`. The record is released either way.
///
/// While the work plays animation 1 the arm also counts `field_2D0` down and,
/// when it expires, re-rolls it from `Gp_LcgState` to between 0x50 and 0xB3
/// frames and cues a sound event, `field_2D6` choosing between the two ids.
/// `field_2BE` is then set from the frames spent on the animation - 0x14 in
/// the first window, -0x14 in the second, 0 outside both - the frame count
/// wraps at 0x63, and the root part takes one step through
/// `Actor07000_Fn027D0`. Eight bytes of the scratch stack are held across the
/// whole arm.
void Actor07000_Fn00654(Task* arg0)
{
    Actor107000Work* work;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s32              soundId;
    u32              rng;

    coord              = ((TmdObject*)arg0->extra)->coords;
    work               = (Actor107000Work*)arg0->work;
    *(u32*)0x1F8003FC -= 8;
    if (Gp_CountRec18Hi(&work->field_11C, 0x10000) != 0) {
        work->field_2D8 = 1;
    }
    if (work->field_2D8 != 0) {
        work->field_2B2 = 1;
        work->field_2C8 = 1;
        work->field_11A = (u16)(work->field_11A & 0x7FFF);
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(&work->field_11C);
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
        Actor07000_Fn027D0(arg0);
    }
    *(u32*)0x1F8003FC += 8;
}
