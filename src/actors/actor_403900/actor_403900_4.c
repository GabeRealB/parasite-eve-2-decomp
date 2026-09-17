#include "common.h"

#include "main/sound.h"
#include "main/wipsys.h"

#include "actors/actor_403900.h"

/// Cue word the countdown's expiry queues, a separate `D_` symbol in the
/// overlay's data.
extern s32 D_actor_403900_8013846C;

/// Cue word the approach's state 0 queues as it plants the actor on its box.
extern s32 D_actor_403900_80138468;

/// Cue word the approach's frame 0x12 queues.
extern s32 D_actor_403900_80138474;

/// Pair the approach's frame 0x14 packs into the work block's `field_57C`.
extern GpU16Pair D_actor_403900_80153BF0;

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80132E34);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_8013314C);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80133AEC);

/// Runs the actor's approach sequence off the box it last hit. State 0 plants
/// the display object on that box, faces it along the box heading and queues
/// the cue `field_6B8`, parking the state at 1 when the player is within 0xDAC
/// and at 2 otherwise. States 1 and 2 re-aim for `field_6D6` frames; state 2
/// closes in until the player is within 0xA8C (state 3) or turns away by more
/// than 0x180 (state 4). State 3 steps `field_6C8` through the frame table
/// `D_actor_403900_801383DC` and fires its per-frame events; state 4 counts
/// `field_6D4` down back to state 0.
void func_actor_403900_80134194(Actor403900* arg0)
{
    u8*                       head;
    Actor403900OffsetScratch* sc;
    s32                       state;
    Actor403900Work*          work;
    Actor403900Coord*         coord;
    s32                       pan;
    s32                       snd;
    s32                       i;
    s16                       diff;
    s16                       dist;
    s32                       adiff;
    s32                       val;
    s16                       timer;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor403900OffsetScratch);
    sc                    = (Actor403900OffsetScratch*)(head - sizeof(Actor403900OffsetScratch));
    work                  = arg0->field_1C;
    state                 = work->field_6CE;
    coord                 = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            coord->field_0.coord.t[0] = work->field_6B4[work->field_708].field_4;
            coord->field_0.coord.t[1] = Wip_SysConfig.field_4->t[1];
            coord->field_0.coord.t[2] = work->field_6B4[work->field_708].field_6;
            sc->in.vx                 = 0;
            sc->in.vy                 = work->field_6B4[work->field_708].field_2;
            sc->in.vz                 = 0;
            RotMatrix(&sc->in, &coord->field_0.coord);
            sc->out.vx = Wip_SysConfig.field_4->t[0] - coord->field_0.coord.t[0];
            sc->out.vz = Wip_SysConfig.field_4->t[2] - coord->field_0.coord.t[2];
            if ((s16)SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vz * sc->out.vz) < 0xDAC) {
                work->field_6C0 = 4;
                work->field_6CE = 1;
                work->field_6D4 = 0x1E;
            } else {
                work->field_6C0 = 6;
                work->field_6CE = 2;
            }
            work->field_6DA = 1;
            work->field_6DC = 0x14;
            work->field_6DE = 0xA;
            work->field_6B8 = D_actor_403900_80138468 | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
            SndEvt_EnqueueType6(work->field_6B8, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            Gp_ArmStateF0(1);
            if (work->field_6C6 == 0) {
                work->field_49A |= 0x8000;
                work->field_494  = work->field_716 | 0x30000;
            }
            work->field_6D6 = 0xA;
            work->field_70A = 0;
            work->field_6F2 = 1;
            break;
        case 1:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6C0 = 6;
                work->field_6CE = 2;
            }
            if (work->field_6D6 > 0) {
                work->field_6D6--;
                func_actor_403900_80135D5C(arg0);
            }
            if (work->field_70A >= 0xFA) {
                work->field_6CC  = 4;
                work->field_6CE  = 0;
                work->field_6DA  = 7;
                work->field_6C8  = 0;
                work->field_62A &= 0x3FFF;
            }
            break;
        case 2:
            val = 0;
            if (work->field_6C4 >= 8) {
                val = 0x78;
            }
            work->field_6C8 = val;
            if (work->field_6D6 > 0) {
                work->field_6D6--;
                func_actor_403900_80135D5C(arg0);
            }
            sc->out.vx = Wip_SysConfig.field_4->t[0] - coord->field_0.coord.t[0];
            sc->out.vz = Wip_SysConfig.field_4->t[2] - coord->field_0.coord.t[2];
            if ((s16)SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vz * sc->out.vz) < 0xA8C) {
                work->field_6C0  = 7;
                work->field_6CE  = 3;
                work->field_62A &= 0x3FFF;
            } else {
                diff = (ratan2((s16)sc->out.vx, (s16)sc->out.vz) & 0xFFF) - work->field_6B4[work->field_708].field_2;
                dist = (abs(diff) >= 0x800) ? ((diff > 0) ? 0x1000 - diff : diff + 0x1000) : abs(diff);
                if (dist > 0x180) {
                    work->field_6C0  = 4;
                    work->field_6CE  = 4;
                    work->field_6DA  = 3;
                    work->field_6DC  = 0x14;
                    work->field_6DE  = 0xA;
                    work->field_6F2  = 0;
                    work->field_6D4  = work->field_6DC + 0xA;
                    work->field_62A &= 0x3FFF;
                    work->field_6BC  = D_actor_403900_8013846C | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                    SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
            }
            if (work->field_70A >= 0xFA) {
                work->field_6CC  = 4;
                work->field_6CE  = 0;
                work->field_6DA  = 7;
                work->field_6C8  = 0;
                work->field_62A &= 0x3FFF;
            }
            break;
        case 3:
            for (i = 0; work->field_6C4 > D_actor_403900_801383DC[i].frame; i++) {
            }
            work->field_6C8 = D_actor_403900_801383DC[i].value;
            if (work->field_6C4 == 0x12) {
                snd = D_actor_403900_80138474 | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_6C4 == 0x14) {
                work->field_56C  = arg0->field_2C->field_8;
                work->field_576  = -0x4B0;
                work->field_578  = 0x1F4;
                work->field_574  = 0;
                work->field_580  = 0x3E8;
                work->field_57C  = Gp_PackPair(&D_actor_403900_80153BF0, 2);
                work->field_582 |= 0x8000;
            }
            if (work->field_6C4 == 0x20) {
                work->field_6F2  = 0;
                work->field_582 &= 0x7FFF;
            }
            if (work->field_6C4 == 0x5A) {
                work->field_6DA = 3;
                work->field_6DC = 0x14;
                work->field_6DE = 0xA;
                work->field_6CE = 4;
                work->field_6D4 = work->field_6DC + 0xA;
                work->field_6BC = D_actor_403900_8013846C | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            break;
        case 4:
            work->field_6C8 = 0;
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6CC = 0;
                work->field_6CE = 0;
            }
            break;
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor403900OffsetScratch);
}

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

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80134968);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_8013539C);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_801354B0);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80135630);
