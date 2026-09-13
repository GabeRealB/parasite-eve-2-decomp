#include "common.h"

#include "main/sound.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_402200.h"

/// Cue-id table: `Actor402200Work::field_712` picks two adjacent words,
/// `[field_712 * 2 - 1]` for the `field_3` bit 0x20 cue and `[field_712 * 2]`
/// for the 0x10 one.
extern s32 D_actor_402200_80138420[];

/// Cue word `func_actor_402200_8013539C` queues, a separate `D_` symbol in the
/// overlay's data 0x48 past the table above.
extern s32 D_actor_402200_80138468;

INCLUDE_RODATA("actors/nonmatchings/actor_402200/actor_402200", D_actor_402200_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80131F54);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_801324E8);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80132688);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_801329A4);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80132D78);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80132E34);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_8013314C);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80133AEC);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80134194);

/// Rolls the actor's cue countdown. State 0 puts the slot set on animation
/// 0xB and drops the state to 1, arming `field_6D4` from the `Gp_LcgState` LCG
/// (0x4B..0x6A); while no flinch is already running it also raises the hit
/// descriptor `field_494`/`field_49A`. State 1 ticks `field_6D4` down and, on
/// the frame it runs out, arms the `field_6DA`/`field_6DC`/`field_6DE`/
/// `field_6E0` timers, clears the state and `field_6CC`, and queues the actor's
/// cue, panned and depth-attenuated from the display object.
void func_actor_402200_801347F4(Actor402200* arg0)
{
    Actor402200Work*  work;
    Actor402200Coord* coord;
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
            work->field_6D4 = (u16)(((random >> 16) & 0x1F) + 0x4B);
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
                work->field_6BC = D_actor_402200_80138468 | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            break;
    }
    *(u32*)0x1F8003FC += 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80134968);

/// Runs the actor's animation-reseed sequence. State 0 puts the slot set on
/// animation 8, clears `field_6C8` and drops the state to 1; unless the mode at
/// `field_6EC` is already 1 it also arms the `field_6DA`/`field_6DC`/`field_6DE`
/// timers and queues the actor's cue, panned and depth-attenuated from the
/// display object. State 1 waits for the animation to reach 0x37 frames and
/// then puts the state back to 0, flipping the mode to 2 and raising
/// `field_6CC` if it was 1.
void func_actor_402200_8013539C(Actor402200* arg0)
{
    Actor402200Work*  work;
    Actor402200Coord* coord;
    s32               state;
    s32               pan;

    work  = arg0->field_1C;
    state = work->field_6CE;
    coord = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            work->field_6C0 = 8;
            work->field_6CE = 1;
            work->field_6C8 = 0;
            if (work->field_6EC != 1) {
                work->field_6DA = 3;
                work->field_6DC = 0x1E;
                work->field_6DE = 0xF;
                work->field_6BC = D_actor_402200_80138468 | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
                break;
            }
            break;
        case 1:
            if (work->field_6C4 >= 0x37) {
                if (work->field_6EC == state) {
                    work->field_6CC = 4;
                    work->field_6EC = 2;
                } else {
                    work->field_6CC = 0;
                }
                work->field_6CE = 0;
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_801354B0);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80135630);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_8013592C);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80135A24);

/// Fires the cue pair the work block's `field_712` selects: while the second
/// animation slot carries `field_3` bit 0x20 or 0x10, a sound is queued on the
/// frame that bit has just dropped from `Actor402200Work::field_6CA`, panned
/// and depth-attenuated from the actor's display object. The cue id is the
/// matching word of `D_actor_402200_80138420` with the `GpEnemy` work id's high
/// nibble in bits 8-11, and a zero `field_712` disarms the body. The record's
/// two bits are latched for the next frame at the end.
void func_actor_402200_80135BE0(Actor402200* arg0)
{
    s32               snd;
    s32               pan;
    s32               pan2;
    Actor402200Work*  work;
    Actor402200Coord* coord;
    GpAnimRec*        rec;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (work->field_712 != 0) {
        rec = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->field_3C);
        if (rec != NULL) {
            if (!(rec->field_3 & 0x20) && (work->field_6CA & 0x20)) {
                snd = D_actor_402200_80138420[work->field_712 * 2 - 1] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (!(rec->field_3 & 0x10) && (work->field_6CA & 0x10)) {
                snd  = D_actor_402200_80138420[work->field_712 * 2] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            work->field_6CA = (u16)(rec->field_3 & 0x30);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80135D5C);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80136184);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_801368E0);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80136D9C);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200", func_actor_402200_80137444);

INCLUDE_RODATA("actors/nonmatchings/actor_402200/actor_402200", ActorsShared80135df4Table);
