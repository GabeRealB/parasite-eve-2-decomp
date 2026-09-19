#include "common.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actor_402200.h"
#include "actors/actors_shared_80137ca4.h"

#include "psyq/inline_c.h"

/// `rtv0`: rotate V0 through the loaded rotation matrix, no translation. The
/// `inline_c.h` macro of that name assembles to a different word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Cue word `func_actor_402200_8013539C` and `func_actor_402200_801354B0`
/// queue, a separate `D_` symbol in the overlay's data 0x48 past the cue-id
/// table `D_actor_402200_80138420`.
extern s32 D_actor_402200_80138468;

/// Cue word the fade-out in `func_actor_402200_80134968` queues.
extern s32 D_actor_402200_8013846C;

/// Cue-id table, indexed from `Actor402200Work::field_712`.
extern s32 D_actor_402200_80138420[];

/// Cue words `func_actor_402200_80134194` queues next to
/// `D_actor_402200_80138468`.
extern s32 D_actor_402200_80138464;
extern s32 D_actor_402200_80138470;

extern GpU16Pair D_actor_402200_80153BEC;

/// Remaining-enemy count; a grab only starts while it is positive.
extern s16 D_80073BA0;

/// Difficulty index into `D_actor_402200_80153C0C`.
extern u8 D_8011541B;

/// Per-difficulty HP above which the player always breaks the grab.
extern s16 D_actor_402200_80153C0C[];

/// Animation block the grab's 0x3FF messages hand the player.
extern s32 D_actor_402200_8015415C;

/// `func_800FDB18` argument record for the grab's finishing spark.
extern GpEffArg D_actor_402200_80154170;

/// Parks the actor's target position off the player (`gameGetPtrSlot(3)`).
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
        coord           = ((Actor402200*)gameGetPtrSlot(3))->field_2C->field_8;
        work->field_6E6 = ratan2(coord->field_0.coord.m[0][2], coord->field_0.coord.m[2][2]) & 0xFFF;
        sc->in.vz       = -0x5AA;
        sc->in.vx       = 0;
        sc->in.vy       = 0;
        gte_SetRotMatrix(&coord->field_0.coord);
        gte_ldv0(&sc->in);
        gte_rtv0_real();
        gte_stlvnl(&sc->out);
        work->field_6A4  = Player_Status.coordMtx->t[0] + sc->out.vx;
        work->field_6A8  = Player_Status.coordMtx->t[1];
        work->field_6AC  = Player_Status.coordMtx->t[2] + sc->out.vz;
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
        coord            = ((Actor402200*)gameGetPtrSlot(3))->field_2C->field_8;
        work->field_6E6  = (work->field_6E6 + (ratan2(coord->field_0.coord.m[0][2], coord->field_0.coord.m[2][2]) & 0xFFF)) & 0xFFF;
        sc->in.vx        = (u32)(rsin(work->field_6E6) * 0x4B) >> 8;
        sc->in.vz        = (u32)(rcos(work->field_6E6) * 0x4B) >> 8;
        work->field_6A4  = Player_Status.coordMtx->t[0] + sc->in.vx;
        work->field_6A8  = Player_Status.coordMtx->t[1];
        work->field_6AC  = Player_Status.coordMtx->t[2] + sc->in.vz;
        work->field_5BA |= 0x4000;
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor402200OffsetScratch);
}

/// Runs the actor's hold sequence on the player (the same 0x3F8 / 0x3FF
/// message pair `func_actor_103700_80134F50` uses to take a hold). State 0
/// asks the player for range 0x19 while enemies remain; on success it plants
/// the display object at `field_6A4`, places the player 0x5AA in front of it
/// with message 0x3E9 and queues a cue. States 1 and 2 step the player's
/// animation. State 3 waits out `field_6D6`, then every 0x1E frames decides
/// whether the hold ends: always when `Player_Status.hp` is above the
/// per-difficulty `D_actor_402200_80153C0C`, otherwise by an LCG roll whose
/// chance grows with the attempt count `field_6F6`; a raised `field_6F4`
/// ends it early. State 5 either reacts to `field_6F4` or, at frame 0x1A,
/// spawns the spark, sends message 0x400 and clears `D_80073BA0`; state 7
/// then loads file 9/0x1E and queues cue 0x70010001 once the CD is idle.
void func_actor_402200_8013314C(Actor402200* arg0)
{
    Actor402200Work*        work;
    Actor402200Coord*       coord;
    Task*                   player;
    Actor402200GrabScratch* sc;
    Actor402200Coord*       pcoord;
    s32                     flag;
    s32                     snd;
    s32                     chance;
    u32                     random;
    s16                     timer;
    s16                     val;
    s16                     sub;

    work                   = arg0->field_1C;
    coord                  = arg0->field_2C->field_8;
    player                 = gameGetPtrSlot(3);
    *(u8**)G_SCRATCH_HEAD -= sizeof(Actor402200GrabScratch);
    sc                     = *(Actor402200GrabScratch**)G_SCRATCH_HEAD;
    pcoord                 = ((Actor402200*)player)->field_2C->field_8;
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
            sc->anim.field_0  = &D_actor_402200_8015415C;
            sc->anim.field_4  = 1;
            sc->anim.field_8  = 0;
            sc->anim.field_C  = 0;
            sc->anim.field_10 = 1;
            Gp_DispatchMsg(player, 0x3FF, (s32)&sc->anim, 0);
            work->field_6CE = 2;
            work->field_6DC = 0x3C;
            work->field_6DA = 1;
            work->field_6DE = 0x1E;
            work->field_6B8 = D_actor_402200_80138464 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
            SndEvt_EnqueueType6(work->field_6B8, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
            break;
        case 2:
            if (work->field_6C4 >= 0x29) {
                work->field_6C0   = 2;
                work->field_6CE   = 3;
                work->field_6D6   = 0x1E;
                work->field_6D4   = 0;
                work->field_6F6   = 0;
                sc->anim.field_0  = &D_actor_402200_8015415C;
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
                    if (Player_Status.hp > D_actor_402200_80153C0C[D_8011541B]) {
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
                        sc->anim.field_0  = &D_actor_402200_8015415C;
                        sc->anim.field_4  = 3;
                        sc->anim.field_8  = 0;
                        sc->anim.field_C  = 0;
                        sc->anim.field_10 = 1;
                        Gp_DispatchMsg(player, 0x3FF, (s32)&sc->anim, 0);
                    } else {
                        work->field_6D4 = 0x1E;
                        Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_actor_402200_80153BEC, 0), 0);
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
                        work->field_6BC = D_actor_402200_80138468 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                        SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
                    }
                } else {
                    func_actor_402200_801324E8(arg0, work->field_70A);
                    work->field_71A = 0;
                }
                work->field_718   = 2;
                work->field_6F4   = 0;
                sc->anim.field_0  = &D_actor_402200_8015415C;
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
            func_actor_402200_801380D8(arg0);
            break;
        case 5:
            if (work->field_6C4 < 0x1A) {
                if (work->field_6F4 != 0) {
                    work->field_6C0   = 0xC;
                    work->field_6F4   = 0;
                    sc->anim.field_0  = &D_actor_402200_8015415C;
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
                    work->field_6BC = D_actor_402200_80138468 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
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
                func_800FDB18(1, &((Actor402200*)gameGetPtrSlot(3))->field_2C->field_8->field_140, &sc->in, &D_actor_402200_80154170);
                Gp_SpawnPadLerp(0xA, 0xFF, 8);
                Gp_DispatchMsg(player, 0x400, 0, 0);
                D_80073BA0 = 0;
            }
            break;
        case 6:
            work->field_6C8 = -0xA;
            func_actor_402200_801380D8(arg0);
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
                        coord = ((Actor402200*)gameGetPtrSlot(3))->field_2C->field_8;
                        SndEvt_EnqueueType6(0x70010001, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
                        work->field_6D4 = 2;
                    }
                    break;
                case 2:
                    break;
            }
            break;
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor402200GrabScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_4", func_actor_402200_80133AEC);

/// Runs the actor's approach sequence off the box it last hit. State 0 plants
/// the display object on that box, faces it along the box heading and queues
/// the cue `field_6B8`, parking the state at 1 when the player is within 0xDAC
/// and at 2 otherwise. States 1 and 2 re-aim for `field_6D6` frames; state 2
/// closes in until the player is within 0xA8C (state 3) or turns away by more
/// than 0x180 (state 4). State 3 steps `field_6C8` through the frame table
/// `D_actor_402200_801383D8` and fires its per-frame events; state 4 counts
/// `field_6D4` down back to state 0.
void func_actor_402200_80134194(Actor402200* arg0)
{
    u8*                       head;
    Actor402200OffsetScratch* sc;
    s32                       state;
    Actor402200Work*          work;
    Actor402200Coord*         coord;
    s32                       pan;
    s32                       snd;
    s32                       i;
    s16                       diff;
    s16                       dist;
    s32                       adiff;
    s32                       val;
    s16                       timer;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor402200OffsetScratch);
    sc                    = (Actor402200OffsetScratch*)(head - sizeof(Actor402200OffsetScratch));
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
            work->field_6B8 = D_actor_402200_80138464 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
            SndEvt_EnqueueType6(work->field_6B8, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
            Gp_ArmStateF0(1);
            if (work->field_6C6 == 0) {
                work->field_49A |= 0x8000;
                work->field_494  = work->field_716 | 0x30000;
            }
            work->field_6D6 = 0x14;
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
                func_actor_402200_80135D5C(arg0);
            }
            if (work->field_70A >= 0xA0) {
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
                func_actor_402200_80135D5C(arg0);
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
                    work->field_6BC  = D_actor_402200_80138468 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                    SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
                }
            }
            if (work->field_70A >= 0xA0) {
                work->field_6CC  = 4;
                work->field_6CE  = 0;
                work->field_6DA  = 7;
                work->field_6C8  = 0;
                work->field_62A &= 0x3FFF;
            }
            break;
        case 3:
            for (i = 0; work->field_6C4 > D_actor_402200_801383D8[i].frame; i++) {
            }
            work->field_6C8 = D_actor_402200_801383D8[i].value;
            if (work->field_6C4 == 0x12) {
                snd = D_actor_402200_80138470 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
            }
            if (work->field_6C4 == 0x14) {
                work->field_56C  = arg0->field_2C->field_8;
                work->field_576  = -0x4B0;
                work->field_578  = 0x1F4;
                work->field_574  = 0;
                work->field_580  = 0x3E8;
                work->field_57C  = Gp_PackPair(&D_actor_402200_80153BEC, 2);
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
                work->field_6BC = D_actor_402200_80138468 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
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
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor402200OffsetScratch);
}

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
                work->field_6BC = D_actor_402200_80138468 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                pan             = (s8)Gp_GetObjPan((GsCOORDINATE2*)coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
            }
            break;
    }
    *(u32*)0x1F8003FC += 8;
}

/// Runs the actor's fade sequence off `field_6DA`. States 1 / 3 fade the
/// display object's `field_2C` and the `field_6D8` / `field_6E2` shades up and
/// down, releasing the queued cues as they finish; state 4 fades to 0xB00 and
/// snapshots the root matrix into `field_674`, and state 6 winds `field_694` /
/// `field_698` down before resetting the root matrix to identity. States 7-9
/// flicker between two LCG-rolled timings, spawning effect 0x600E0 at the
/// fourth part on odd animation frames.
void func_actor_402200_80134968(Actor402200* arg0)
{
    SVECTOR*                sc;
    Actor402200Work*        work;
    Actor402200Obj2C*       obj;
    Actor402200Coord*       coord;
    Actor402200MatrixWords* m;
    s32                     snd;
    s32                     pan;
    s32                     v;
    s32                     w;
    s32                     sy;
    s32                     y;
    u32                     random;
    s16                     t;

    sc    = (SVECTOR*)(*(u32*)0x1F8003FC -= 8);
    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    coord = obj->field_8;
    switch (work->field_6DA) {
        case 0:
            arg0->field_2C->field_C = 0x80;
            work->field_6E2         = -1;
            work->field_49A        &= 0x7FFF;
            if (work->field_6B8 != 0) {
                SndEvt_EnqueueType7(work->field_6B8, 1);
                work->field_6B8 = 0;
            }
            if (work->field_6BC != 0) {
                SndEvt_EnqueueType7(work->field_6BC, 1);
                work->field_6BC = 0;
            }
            break;
        case 1:
            obj->field_2C += 0x1000 / work->field_6DE;
            if (obj->field_2C >= 0x1000) {
                obj->field_2C   = 0x1000;
                t               = work->field_6D8 - 0xFF / work->field_6DC;
                work->field_6D8 = t;
                if (t <= 0) {
                    work->field_6D8 = 0;
                    work->field_6DA = 2;
                    if (work->field_6B8 != 0) {
                        SndEvt_EnqueueType7(work->field_6B8, 1);
                        work->field_6B8 = 0;
                    }
                }
            }
            t               = work->field_6E2 + 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t >= 0x80) {
                work->field_6E2 = 0x80;
            }
            break;
        case 2:
            work->field_6E2 = 0x80;
            if (work->field_6B8 != 0) {
                SndEvt_EnqueueType7(work->field_6B8, 1);
                work->field_6B8 = 0;
            }
            if (work->field_6BC != 0) {
                SndEvt_EnqueueType7(work->field_6BC, 1);
                work->field_6BC = 0;
            }
            break;
        case 3:
            t               = work->field_6D8 + 0xFF / work->field_6DC;
            work->field_6D8 = t;
            if (t >= 0xFF) {
                work->field_6D8 = 0xFF;
                obj->field_2C  -= 0x1000 / work->field_6DE;
                if (obj->field_2C <= 0) {
                    obj->field_2C   = 0;
                    work->field_6DA = 0;
                    if (work->field_6BC != 0) {
                        SndEvt_EnqueueType7(work->field_6BC, 1);
                        work->field_6BC = 0;
                    }
                    snd = D_actor_402200_8013846C | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan((GsCOORDINATE2*)coord);
                    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
                }
            }
            t               = work->field_6E2 - 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t < 0) {
                work->field_6E2 = -1;
            }
            break;
        case 4:
            obj->field_2C += 0xB00 / work->field_6DE;
            if (obj->field_2C >= 0xB00) {
                obj->field_2C   = 0xB00;
                t               = work->field_6D8 - 0xFF / work->field_6DC;
                work->field_6D8 = t;
                if (t <= 0) {
                    work->field_6DA = 5;
                    work->field_6D8 = 0;
                    work->field_694 = 0x1000;
                    work->field_698 = 0x1000;
                    work->field_69C = 0x1000;
                    work->field_674 = arg0->field_2C->field_8->field_0.coord;
                    work->field_6D0 = 0;
                }
            }
            work->field_6E2 = -1;
            break;
        case 5:
            work->field_6E2 = -1;
            break;
        case 6:
            work->field_49A &= 0x7FFF;
            switch (work->field_6D0) {
                case 0:
                    y = work->field_698;
                    if (work->field_6E8 != 0) {
                        sy = y - 0x400;
                    } else {
                        sy = y - 0x200;
                    }
                    work->field_698 = sy;
                    if (sy <= 0x800) {
                        work->field_6D0 = 1;
                    }
                    break;
                case 1:
                    if (work->field_6E8 != 0) {
                        v               = work->field_694 - 0x200;
                        w               = work->field_698 + 0x400;
                        work->field_694 = v;
                        work->field_698 = w;
                    } else {
                        v               = work->field_694 - 0x100;
                        w               = work->field_698 + 0x200;
                        work->field_694 = v;
                        work->field_698 = w;
                    }
                    if (work->field_694 <= 0x800) {
                        work->field_6D0 = 2;
                    }
                    break;
            }
            ActorsShared80137ca4((ActorShared80137ca4*)arg0);
            t               = work->field_6D8 + 0xFF / work->field_6DC;
            work->field_6D8 = t;
            if (t >= 0xFF) {
                work->field_6D8 = 0xFF;
                obj->field_2C  -= 0x1000 / work->field_6DE;
                if (obj->field_2C <= 0) {
                    obj->field_2C           = 0;
                    work->field_6DA         = 0;
                    m                       = (Actor402200MatrixWords*)&arg0->field_2C->field_8->field_0.coord;
                    m->field_0              = 0x1000;
                    m->field_4              = 0;
                    m->field_8              = 0x1000;
                    m->field_C              = 0;
                    m->field_10             = 0x1000;
                    arg0->field_2C->field_C = 0x80;
                }
            }
            work->field_6E2 = -1;
            break;
        case 7:
            work->field_6DA = 8;
            work->field_6E0 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) + 2;
            t               = work->field_6E0 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
            work->field_6DC = t;
            work->field_6DE = t;
            break;
        case 8:
            t               = work->field_6D8 + 0xFF / work->field_6DC;
            work->field_6D8 = t;
            if (t >= 0x80) {
                work->field_6D8 = 0x80;
                obj->field_2C  -= 0x1000 / work->field_6DE;
                if (obj->field_2C <= 0x800) {
                    obj->field_2C = 0x800;
                }
            }
            t               = work->field_6E2 - 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t < 0) {
                work->field_6E2 = -1;
            }
            t               = work->field_6E0 - 1;
            work->field_6E0 = t;
            if (t <= 0) {
                work->field_6DA = 9;
                work->field_6E0 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) + 2;
                t               = work->field_6E0 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
                work->field_6DC = t;
                work->field_6DE = t;
            }
            if (work->field_6EA == 0) {
                work->field_6EA = 1;
            }
            if (work->field_6C4 & 1) {
                sc->vx = 0;
                sc->vy = -(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF);
                sc->vz = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF;
                Gp_SpawnEff(0x600E0, &arg0->field_2C->field_8->field_F0, 0x100, sc);
            }
            break;
        case 9:
            obj->field_2C += 0x1000 / work->field_6DE;
            if (obj->field_2C >= 0x1000) {
                obj->field_2C   = 0x1000;
                t               = work->field_6D8 - 0xFF / work->field_6DC;
                work->field_6D8 = t;
                if (t <= 0) {
                    work->field_6D8 = 0;
                }
            }
            t               = work->field_6E2 + 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t >= 0x80) {
                work->field_6E2 = 0x80;
            }
            t               = work->field_6E0 - 1;
            work->field_6E0 = t;
            if (t <= 0) {
                work->field_6DA = 8;
                work->field_6E0 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) + 2;
                t               = work->field_6E0 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
                work->field_6DC = t;
                work->field_6DE = t;
            }
            if (work->field_6C4 & 1) {
                sc->vx = 0;
                sc->vy = -(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF);
                sc->vz = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF;
                Gp_SpawnEff(0x600E0, &arg0->field_2C->field_8->field_F0, 0x100, sc);
            }
            break;
    }
    *(u32*)0x1F8003FC += 8;
}

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
                work->field_6BC = D_actor_402200_80138468 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                pan             = (s8)Gp_GetObjPan((GsCOORDINATE2*)coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
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
                work->field_6BC = D_actor_402200_80138468 | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                pan             = (s8)Gp_GetObjPan((GsCOORDINATE2*)coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
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
                snd = D_actor_402200_80138420[work->field_712 + 8] | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
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
                snd = D_actor_402200_80138420[work->field_712 + 8] | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GsCOORDINATE2*)coord), (s8)Gp_GetObjDepth((GsCOORDINATE2*)coord));
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
