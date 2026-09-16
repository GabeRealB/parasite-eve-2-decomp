#include "common.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_402200.h"

#include "psyq/inline_c.h"

/// `rtv0`: rotate V0 through the loaded rotation matrix, no translation. The
/// `inline_c.h` macro of that name assembles to a different word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Cue word `func_actor_402200_8013539C` and `func_actor_402200_801354B0`
/// queue, a separate `D_` symbol in the overlay's data 0x48 past the cue-id
/// table `D_actor_402200_80138420`.
extern s32 D_actor_402200_80138468;

/// Cue-id table, indexed from `Actor402200Work::field_712`.
extern s32 D_actor_402200_80138420[];

/// Parks the actor's target position off the player (`Game_GetPtrSlot(3)`).
/// State 3 takes `field_6E6` from the player's heading and rotates a -0x5AA
/// z offset through the player's root matrix; state 4 rolls `field_6E6` from
/// `Gp_LcgState`, derives `field_5DC`/`field_5E0` from it, adds the player's
/// heading and places the target 0x4B out along the result. Both write the
/// position to `field_6A4`..`field_6AC` and raise bit 0x4000 of `field_5BA`.
void func_actor_402200_80132E34(Actor402200* arg0)
{
    u8*                       head;
    Actor402200OffsetScratch* sc;
    Actor402200Work*          work;
    Actor402200Coord*         coord;
    u32                       random;
    s32                       angle;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor402200OffsetScratch);
    sc                    = (Actor402200OffsetScratch*)(head - sizeof(Actor402200OffsetScratch));
    work                  = arg0->field_1C;
    if (work->field_6CE == 3) {
        coord           = ((Actor402200*)Game_GetPtrSlot(3))->field_2C->field_8;
        work->field_6E6 = ratan2(coord->field_0.coord.m[0][2], coord->field_0.coord.m[2][2]) & 0xFFF;
        sc->in.vz       = -0x5AA;
        sc->in.vx       = 0;
        sc->in.vy       = 0;
        gte_SetRotMatrix(&coord->field_0.coord);
        gte_ldv0(&sc->in);
        gte_rtv0_real();
        gte_stlvnl(&sc->out);
        work->field_6A4  = Wip_SysConfig.field_4->t[0] + sc->out.vx;
        work->field_6A8  = Wip_SysConfig.field_4->t[1];
        work->field_6AC  = Wip_SysConfig.field_4->t[2] + sc->out.vz;
        work->field_5DE  = -0x3E8;
        work->field_5E0  = -0x7D0;
        work->field_5DC  = 0;
        work->field_5BA |= 0x4000;
        work->field_5DA |= 0x4000;
    } else if (work->field_6CE == 4) {
        if (work->field_6E8 != 0) {
            Gp_LcgState     = (Gp_LcgState * 5) + 0x71357911;
            work->field_6E6 = (Gp_LcgState >> 16) & 0xFFF;
        } else {
            random      = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState = random;
            angle       = (random >> 16) & 0x3FF;
            if (!((random >> 16) & 0x400)) {
                angle = -angle;
            }
            work->field_6E6 = angle;
        }
        work->field_5DC  = (u32)(rsin(work->field_6E6) * 0x7D) >> 8;
        work->field_5DE  = -0x3E8;
        work->field_5E0  = (u32)(rcos(work->field_6E6) * 0x7D) >> 8;
        coord            = ((Actor402200*)Game_GetPtrSlot(3))->field_2C->field_8;
        work->field_6E6  = (work->field_6E6 + (ratan2(coord->field_0.coord.m[0][2], coord->field_0.coord.m[2][2]) & 0xFFF)) & 0xFFF;
        sc->in.vx        = (u32)(rsin(work->field_6E6) * 0x4B) >> 8;
        sc->in.vz        = (u32)(rcos(work->field_6E6) * 0x4B) >> 8;
        work->field_6A4  = Wip_SysConfig.field_4->t[0] + sc->in.vx;
        work->field_6A8  = Wip_SysConfig.field_4->t[1];
        work->field_6AC  = Wip_SysConfig.field_4->t[2] + sc->in.vz;
        work->field_5BA |= 0x4000;
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor402200OffsetScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_4", func_actor_402200_8013314C);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_4", func_actor_402200_80133AEC);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_4", func_actor_402200_80134194);

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

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_4", func_actor_402200_80134968);

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

/// Runs the actor's attack sequence. State 0 puts the slot set on animation 9
/// or 0xA, whichever `field_6D2` selects, and parks the state at 1 or 2 to
/// match; unless the mode at `field_6EC` is already 1 it also arms the
/// `field_6DA`/`field_6DC`/`field_6DE` timers and queues the actor's cue,
/// panned and depth-attenuated from the display object. States 1 and 2 wait out
/// their own animation - `field_6C4` at 0x50 and 0x3B frames - and then put the
/// state back to 0, flipping the mode to 2 and raising `field_6CC` when it was
/// still 1.
void func_actor_402200_801354B0(Actor402200* arg0)
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
            if (work->field_6D2 == 1) {
                work->field_6C0 = 9;
                work->field_6CE = 1;
            } else {
                work->field_6C0 = 0xA;
                work->field_6CE = 2;
            }
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
            if (work->field_6C4 >= 0x50) {
                if (work->field_6EC == state) {
                    work->field_6CC = 4;
                    work->field_6EC = 2;
                } else {
                    work->field_6CC = 0;
                }
                work->field_6CE = 0;
            }
            break;
        case 2:
            if (work->field_6C4 >= 0x3B) {
                if (work->field_6EC == 1) {
                    work->field_6CC = 4;
                    work->field_6EC = state;
                } else {
                    work->field_6CC = 0;
                }
                work->field_6CE = 0;
            }
            break;
    }
}

/// Runs the actor's branch sequence. State 0 puts the slot set on animation
/// 0xD or 0x11, whichever `field_6D2` selects, and parks the state at 1 or 2 to
/// match. States 1 and 2 queue the actor's cue at frame 0x2C / 0x19 and, once
/// `field_6C4` reaches 0x42 / 0x31, move to state 3 with an LCG-rolled
/// `field_6D4` countdown; states 3 and 4 then alternate on that countdown.
void func_actor_402200_80135630(Actor402200* arg0)
{
    Actor402200Work*  work;
    Actor402200Coord* coord;
    s32               state;
    s32               snd;
    s32               anim;
    u32               random;
    s16               timer;

    work  = arg0->field_1C;
    state = work->field_6CE;
    coord = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            if (work->field_6D2 == 0) {
                work->field_6C0 = 0xD;
                work->field_6CE = 1;
                work->field_6F0 = 1;
                work->field_490 = -0xA7;
            } else {
                work->field_6C0 = 0x11;
                work->field_6CE = 2;
                work->field_6F0 = 2;
                work->field_490 = 0x109;
            }
            work->field_498  = 0x15E;
            work->field_714  = 1;
            work->field_6DA  = 7;
            work->field_6F2  = 2;
            work->field_6C8  = 0;
            work->field_49A |= 0x4000;
            work->field_502 &= 0xBFFF;
            break;
        case 1:
            if (work->field_6C4 == 0x2C) {
                snd = D_actor_402200_80138420[work->field_712 + 8] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_6C4 >= 0x42) {
                work->field_6C0 = 0x10;
                work->field_6CE = 3;
                work->field_6F2 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_6D4 = (random >> 16) & 0x3F;
            }
            if (work->field_714 == 1) {
                work->field_714 = 2;
            }
            break;
        case 2:
            if (work->field_6C4 == 0x19) {
                snd = D_actor_402200_80138420[work->field_712 + 8] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_6C4 >= 0x31) {
                work->field_6C0 = 0x14;
                work->field_6CE = 3;
                work->field_6F2 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_6D4 = (random >> 16) & 0x3F;
            }
            if (work->field_714 == 1) {
                work->field_714 = 2;
            }
            break;
        case 3:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                anim = 0x13;
                if (work->field_6F0 == 1) {
                    anim = 0xF;
                }
                work->field_6D4 = 0xA;
                work->field_6C0 = anim;
                work->field_6CE = 4;
            }
            break;
        case 4:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                anim = 0x14;
                if (work->field_6F0 == 1) {
                    anim = 0x10;
                }
                work->field_6C0 = anim;
                work->field_6CE = 3;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_6D4 = (random >> 16) & 0x3F;
            }
            break;
    }
}
