#include "common.h"

#include "main/sound.h"

#include "actors/actor_403900.h"

/// Cue word the countdown's expiry queues, a separate `D_` symbol in the
/// overlay's data.
extern s32 D_actor_403900_8013846C;

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_3", func_actor_403900_80132E34);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_3", func_actor_403900_8013314C);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_3", func_actor_403900_80133AEC);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_3", func_actor_403900_80134194);

/// Cue body of the enemy's attack: state 0 arms animation `field_6C0`, sets
/// the cue state and rolls the countdown `field_6D4` from `Gp_LcgState`,
/// raising the hit descriptor `field_494`/`field_49A` while no flinch is
/// already running. State 1 ticks the countdown down and, on the frame it
/// runs out, arms the `field_6DA`/`field_6DC`/`field_6DE`/`field_6E0` timers,
/// clears the cue state and `field_6CC`, and queues the actor's cue, panned
/// and depth-attenuated from the display object.
void func_actor_403900_801347F4(Actor403900* arg0)
{
    Actor403900Work*  work;
    Actor403900Coord* coord;
    s32               state;
    s32               pan;
    u32               random;
    s16               timer;

    *(u32*)0x1F8003FC -= 8;
    work               = arg0->field_1C;
    state              = work->field_6CE;
    coord              = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            work->field_6C0 = 0xB;
            work->field_6CE = 1;
            random          = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState     = random;
            work->field_6D4 = (u16)(((random >> 16) & 0x1F) + 0x2D);
            if (work->field_6C6 == 0) {
                work->field_49A |= 0x8000;
                work->field_494  = work->field_716 | 0x30000;
            }
            break;
        case 1:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6DA = 3;
                work->field_6DC = 0xA;
                work->field_6CC = 0;
                work->field_6CE = 0;
                work->field_6DE = 5;
                work->field_6E0 = 0;
                work->field_6BC = D_actor_403900_8013846C | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            break;
    }
    *(u32*)0x1F8003FC += 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_3", func_actor_403900_80134968);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_3", func_actor_403900_8013539C);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_3", func_actor_403900_801354B0);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_3", func_actor_403900_80135630);
