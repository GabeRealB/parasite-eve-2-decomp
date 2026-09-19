#include "common.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/session.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "actors/actor_510900.h"

/// State 0 (animations 0xA/8/0x19): the idle-to-walk start. Sub-state 0 queues
/// the step cue at blend 0xA and past 0x64 hands sub-state 1 the animation 0xA;
/// sub-state 1 pushes both body objects into their flagged pose with the
/// animation 8 past blend 0x14. Sub-states 2 and 3 are the two walk cycles:
/// each feeds `field_5A2` 0x38 over a 0x13-blend window, mutes it while
/// `field_5B2` is latched, queues the footfall cue, and at the end of the cycle
/// either hands on to the next one or - when `field_5B2` is set - clears both
/// flagged poses and restarts the cycle with the animation 0x19. Sub-state 4
/// leaves for state 3 with the animation 0xF.
void func_actor_510900_80135E90(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    s32               snd;
    s32               pair;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    switch (work->field_590) {
        case 0:
            if (work->field_58A == 0xA) {
                snd = (((u16)arg0->field_20->placeKey >> 0xC) << 8) | 0x40780006;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord),
                                    (s8)gpGetObjDepth((GsCOORDINATE2*)coord));
            }
            if (work->field_58A >= 0x64) {
                work->field_590 = 1;
                work->field_586 = 0xA;
            }
            break;
        case 1:
            if (work->field_58A >= 0x14) {
                work->field_590     = 2;
                work->field_586     = 8;
                work->obj4E4.flags |= 0x8000;
                work->obj504.flags |= 0x8000;
                pair                = Gp_PackPair(&D_actor_510900_80167968, 2);
                work->obj4E4.key    = pair;
                work->obj504.key    = pair;
            }
            break;
        case 2:
            work->field_5A2 = ((u32)((u16)work->field_58A - 9) < 0x13U) ? 0x38 : 0;
            if (work->field_5B2 != 0) {
                work->field_5A2 = 0;
            }
            if (work->field_58A == 0xA) {
                snd = (((u16)arg0->field_20->placeKey >> 0xC) << 8) | 0x4078000B;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord),
                                    (s8)gpGetObjDepth((GsCOORDINATE2*)coord));
            }
            if (work->field_58A >= 0x1D) {
                if (work->field_5B2 != 0) {
                    work->field_58E     = 2;
                    work->field_590     = 3;
                    work->field_586     = 0x19;
                    work->field_5B2     = 0;
                    work->field_598     = 0;
                    work->obj4E4.flags &= 0x7FFF;
                    work->obj504.flags &= 0x7FFF;
                } else {
                    work->field_590 = 3;
                    work->field_586 = 9;
                }
            }
            break;
        case 3:
            work->field_5A2 = ((u32)((u16)work->field_58A - 5) < 0x13U) ? 0x38 : 0;
            if (work->field_5B2 != 0) {
                work->field_5A2 = 0;
            }
            if (work->field_58A == 6) {
                snd = (((u16)arg0->field_20->placeKey >> 0xC) << 8) | 0x4078000B;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord),
                                    (s8)gpGetObjDepth((GsCOORDINATE2*)coord));
            }
            if (work->field_58A >= 0x27) {
                if (work->field_5B2 != 0) {
                    work->field_58E     = 2;
                    work->field_590     = 3;
                    work->field_586     = 0x19;
                    work->field_5B2     = 0;
                    work->field_598     = 0;
                    work->obj4E4.flags &= 0x7FFF;
                    work->obj504.flags &= 0x7FFF;
                } else {
                    work->field_590 = 4;
                }
            }
            break;
        case 4:
            work->field_586 = 0xF;
            work->field_58E = 3;
            work->field_590 = 1;
            break;
    }
}

/* Closes this unit's .rodata after the jump table above so actor_510900_4's
 * table starts at 0x80131EA8. Nothing reads it. */
const u32 D_actor_510900_80131EA4 = 0;
