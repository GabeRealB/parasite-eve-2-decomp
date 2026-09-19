#include "common.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actor_403900.h"

#include "psyq/inline_c.h"

/// `rtv0`: rotate V0 through the loaded rotation matrix, no translation. The
/// `inline_c.h` macro of that name assembles to a different word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Cue word the countdown's expiry queues, a separate `D_` symbol in the
/// overlay's data.
extern s32 D_actor_403900_8013846C;

/// Cue word the approach's state 0 queues as it plants the actor on its box.
extern s32 D_actor_403900_80138468;

/// Cue word the approach's frame 0x12 queues.
extern s32 D_actor_403900_80138474;

/// Pair the approach's frame 0x14 packs into the work block's `field_57C`.
extern GpU16Pair D_actor_403900_80153BF0;

/// Remaining-enemy count; a grab only starts while it is positive.
extern s16 D_80073BA0;

/// Difficulty index into `D_actor_403900_80153C10`.
extern u8 D_8011541B;

/// Per-difficulty HP above which the player always breaks the grab.
extern s16 D_actor_403900_80153C10[];

/// Animation block the grab's 0x3FF messages hand the player.
extern s32 D_actor_403900_801540B4;

/// `func_800FDB18` argument record for the grab's finishing spark.
extern GpEffArg D_actor_403900_801540C8;

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80132E34);

/// Runs the actor's hold sequence on the player (the same 0x3F8 / 0x3FF
/// message pair `func_actor_402200_8013314C` uses to take a hold). State 0
/// asks the player for range 0x19 while enemies remain; on success it plants
/// the display object at `field_6A4`, places the player 0x5AA in front of it
/// with message 0x3E9 and queues a cue. States 1 and 2 step the player's
/// animation. State 3 waits out `field_6D6`, then every 0x14 frames decides
/// whether the hold ends: always when `Player_Status.hp` is above the
/// per-difficulty `D_actor_403900_80153C10`, otherwise by an LCG roll whose
/// chance grows with the attempt count `field_6F6`; a raised `field_6F4`
/// ends it early. State 5 either reacts to `field_6F4` or, at frame 0x1A,
/// spawns the spark, sends message 0x400 and clears `D_80073BA0`; state 7
/// then loads file 9/0x1E and queues cue 0x70010001 once the CD is idle.
void func_actor_403900_8013314C(Actor403900* arg0)
{
    Actor403900Work*        work;
    Actor403900Coord*       coord;
    Task*                   player;
    Actor403900GrabScratch* sc;
    Actor403900Coord*       pcoord;
    s32                     flag;
    s32                     snd;
    s32                     chance;
    u32                     random;
    s16                     timer;
    s16                     val;
    s16                     sub;

    work                   = arg0->field_1C;
    coord                  = arg0->field_2C->field_8;
    player                 = Game_GetPtrSlot(3);
    *(u8**)G_SCRATCH_HEAD -= sizeof(Actor403900GrabScratch);
    sc                     = *(Actor403900GrabScratch**)G_SCRATCH_HEAD;
    pcoord                 = ((Actor403900*)player)->field_2C->field_8;
    flag                   = 0;
    switch (work->field_6CE) {
        case 0:
            if (((GpActorWork*)player)->actor->field_954 != 2 && D_80073BA0 > 0) {
                sc->query.field_14 = 0x19;
                if (Gp_DispatchMsg(player, 0x3F8, (s32)sc, 0) == 0) {
                    work->field_6C0 = 1;
                    work->field_6CE = 1;
                    work->field_6F4 = 0;
                    work->field_6E8 = 0;
                    work->field_718 = 1;
                    sc->in.vx       = 0;
                    sc->in.vy       = work->field_6E6;
                    sc->in.vz       = 0;
                    RotMatrix(&sc->in, &coord->field_0.coord);
                    coord->field_0.coord.t[0] = work->field_6A4;
                    coord->field_0.coord.t[1] = work->field_6A8;
                    coord->field_0.coord.t[2] = work->field_6AC;
                    sc->in.vx                 = 0;
                    sc->in.vy                 = 0;
                    sc->in.vz                 = 0x5AA;
                    gte_SetRotMatrix(&coord->field_0.coord);
                    gte_ldv0(&sc->in);
                    gte_rtv0_real();
                    gte_stlvnl(&sc->out);
                    sc->place.pos.vx = coord->field_0.coord.t[0] + sc->out.vx;
                    sc->place.pos.vy = coord->field_0.coord.t[1] + sc->out.vy;
                    sc->place.pos.vz = coord->field_0.coord.t[2] + sc->out.vz;
                    sc->place.rot.vx = 0;
                    sc->place.rot.vy = work->field_6E6;
                    sc->place.rot.vz = 0;
                    Gp_DispatchMsg(player, 0x3E9, (s32)&sc->place, 0);
                    Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
                    snd = (((u16)arg0->field_20->placeKey >> 0xC) << 8) | 6;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GsCOORDINATE2*)pcoord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)pcoord));
                } else {
                    work->field_6CC = 0;
                    work->field_6CE = 0;
                }
            }
            break;
        case 1:
            sc->anim.field_0  = &D_actor_403900_801540B4;
            sc->anim.field_4  = 1;
            sc->anim.field_8  = 0;
            sc->anim.field_C  = 0;
            sc->anim.field_10 = 1;
            Gp_DispatchMsg(player, 0x3FF, (s32)&sc->anim, 0);
            work->field_6CE = 2;
            work->field_6DC = 0x3C;
            work->field_6DA = 1;
            work->field_6DE = 0x1E;
            work->field_6B8 = D_actor_403900_80138468 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
            SndEvt_EnqueueType6(work->field_6B8, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
            break;
        case 2:
            if (work->field_6C4 >= 0x29) {
                work->field_6C0   = 2;
                work->field_6CE   = 3;
                work->field_6D6   = 0x1E;
                work->field_6D4   = 0;
                work->field_6F6   = 0;
                sc->anim.field_0  = &D_actor_403900_801540B4;
                sc->anim.field_4  = 2;
                sc->anim.field_8  = 0;
                sc->anim.field_C  = 0;
                sc->anim.field_10 = 1;
                Gp_DispatchMsg(player, 0x3FF, (s32)&sc->anim, 0);
                Gp_ArmStateF0(1);
                work->field_70A = 0;
                if (work->field_6C6 == 0) {
                    work->field_49A |= 0x8000;
                    work->field_494  = work->field_716 | 0x30000;
                }
            }
            break;
        case 3:
            if (work->field_6D6 > 0) {
                work->field_6D6--;
            } else {
                if ((s16)work->field_6D4 == 2) {
                    Gp_SpawnPadLerp(5, 0xC0, 0x80);
                }
                timer           = work->field_6D4 - 1;
                work->field_6D4 = timer;
                if (timer <= 0) {
                    if (Player_Status.hp > D_actor_403900_80153C10[D_8011541B]) {
                        if (work->field_6F8 == 0) {
                            work->field_6F8 = 1;
                        } else {
                            chance = work->field_6F6 * (0x32 - (Player_Status.hp * 100) / Player_Status.hpMax) / 2;
                            if (chance > 0) {
                                chance      = (chance * 0xFFF) / 100;
                                Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                                if ((s32)((Gp_LcgState >> 16) & 0xFFF) < chance) {
                                    flag = 1;
                                }
                            }
                        }
                    } else {
                        flag = 1;
                    }
                    if (flag != 0) {
                        work->field_6C0   = 3;
                        work->field_6CE   = 5;
                        sc->anim.field_0  = &D_actor_403900_801540B4;
                        sc->anim.field_4  = 3;
                        sc->anim.field_8  = 0;
                        sc->anim.field_C  = 0;
                        sc->anim.field_10 = 1;
                        Gp_DispatchMsg(player, 0x3FF, (s32)&sc->anim, 0);
                    } else {
                        work->field_6D4 = 0x14;
                        Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_actor_403900_80153BF0, 0), 0);
                        work->field_6F6++;
                    }
                }
            }
            if (work->field_6F4 != 0) {
                if (work->field_6F4 == 1) {
                    if (work->field_6D6 > 0 && work->field_6EE == 0) {
                        work->field_6C0 = 0x15;
                        work->field_6CE = 6;
                        work->field_71A = 0;
                        if (work->field_6B8 != 0) {
                            SndEvt_EnqueueType7(work->field_6B8, 1);
                            work->field_6B8 = 0;
                        }
                        work->field_6DA = 7;
                    } else {
                        work->field_6C0 = 0xC;
                        work->field_6CE = 4;
                        work->field_6D4 = 0x69;
                        work->field_6DA = 3;
                        work->field_6DC = 0x4B;
                        work->field_71A = 0;
                        work->field_6DE = 0x1E;
                        work->field_6BC = D_actor_403900_8013846C | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                        SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
                    }
                } else {
                    func_actor_403900_801324E8(arg0, work->field_70A);
                    work->field_71A = 0;
                }
                work->field_718   = 2;
                work->field_6F4   = 0;
                sc->anim.field_0  = &D_actor_403900_801540B4;
                sc->anim.field_4  = 4;
                sc->anim.field_8  = 0;
                sc->anim.field_C  = 0;
                sc->anim.field_10 = 1;
                Gp_DispatchMsg(player, 0x3FF, (s32)&sc->anim, 0);
            }
            break;
        case 4:
            val = 0;
            if (work->field_6C4 < 0x5F) {
                val = -0xA;
            }
            work->field_6C8 = val;
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6CC = 0;
                work->field_6CE = 0;
            }
            func_actor_403900_801380DC(arg0);
            break;
        case 5:
            if (work->field_6C4 < 0x1A) {
                if (work->field_6F4 != 0) {
                    work->field_6C0   = 0xC;
                    work->field_6F4   = 0;
                    sc->anim.field_0  = &D_actor_403900_801540B4;
                    sc->anim.field_4  = 4;
                    sc->anim.field_8  = 0;
                    sc->anim.field_C  = 0;
                    sc->anim.field_10 = 1;
                    Gp_DispatchMsg(player, 0x3FF, (s32)&sc->anim, 0);
                    work->field_6D4 = 0x69;
                    work->field_6DA = 3;
                    work->field_6DC = 0x4B;
                    work->field_6CE = 4;
                    work->field_6DE = 0x1E;
                    work->field_6BC = D_actor_403900_8013846C | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                    SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
                }
            } else if (work->field_6C4 == 0x1A) {
                ((GpActorWork*)player)->actor->field_956 = 0xA;
                work->field_6CE                          = 7;
                work->field_6D4                          = 0;
                gGameSession->deathRestartDelay          = 0x5A;
                gGameSession->areaBgmCountdown           = 0x7F;
                sc->in.vy                                = -0x96;
                sc->in.vx                                = 0;
                sc->in.vz                                = 0xC8;
                func_800FDB18(1, &((Actor403900*)Game_GetPtrSlot(3))->field_2C->field_8->field_140, &sc->in, &D_actor_403900_801540C8);
                Gp_SpawnPadLerp(0xA, 0xFF, 8);
                Gp_DispatchMsg(player, 0x400, 0, 0);
                D_80073BA0 = 0;
            }
            break;
        case 6:
            work->field_6C8 = -0xA;
            func_actor_403900_801380DC(arg0);
            if (work->field_718 == 0) {
                work->field_6C8 = 0;
                work->field_6CC = 4;
                work->field_6CE = 0;
            }
            break;
        case 7:
            sub = work->field_6D4;
            switch (sub) {
                case 0:
                    CdCmd_EnqueueLoadFile(9, 0x1E, 3);
                    work->field_6D4 = 1;
                    break;
                case 1:
                    if ((CdCmd_IsIdle() & 0xFFFF) == 1) {
                        coord = ((Actor403900*)Game_GetPtrSlot(3))->field_2C->field_8;
                        SndEvt_EnqueueType6(0x70010001, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
                        work->field_6D4 = 2;
                    }
                    break;
                case 2:
                    break;
            }
            break;
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor403900GrabScratch);
}

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
            coord->field_0.coord.t[1] = Player_Status.coordMtx->t[1];
            coord->field_0.coord.t[2] = work->field_6B4[work->field_708].field_6;
            sc->in.vx                 = 0;
            sc->in.vy                 = work->field_6B4[work->field_708].field_2;
            sc->in.vz                 = 0;
            RotMatrix(&sc->in, &coord->field_0.coord);
            sc->out.vx = Player_Status.coordMtx->t[0] - coord->field_0.coord.t[0];
            sc->out.vz = Player_Status.coordMtx->t[2] - coord->field_0.coord.t[2];
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
            work->field_6B8 = D_actor_403900_80138468 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
            SndEvt_EnqueueType6(work->field_6B8, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
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
            sc->out.vx = Player_Status.coordMtx->t[0] - coord->field_0.coord.t[0];
            sc->out.vz = Player_Status.coordMtx->t[2] - coord->field_0.coord.t[2];
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
                    work->field_6BC  = D_actor_403900_8013846C | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                    SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
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
                snd = D_actor_403900_80138474 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
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
                work->field_6BC = D_actor_403900_8013846C | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
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
                work->field_6BC = D_actor_403900_8013846C | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                pan             = (s8)Gp_GetObjPan((GsCOORDINATE2*)coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
            }
            break;
    }
    *(u32*)0x1F8003FC += 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80134968);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_8013539C);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_801354B0);

INCLUDE_ASM("actors/nonmatchings/actor_403900/actor_403900_4", func_actor_403900_80135630);
