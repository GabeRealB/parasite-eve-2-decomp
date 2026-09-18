#include "common.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/session.h"
#include "main/fs.h"

#include "gameplay/D4.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/gameflag.h"
#include "main/wipsys.h"
#include "gameplay/gameplay.h"
#include "actors/actor_510900.h"
#include "actors/actors_shared_8013bbe4.h"

#include <psyq/inline_c.h>

/* `gte_ApplyMatrix` / `gte_MulMatrix0` from `psyq/gtemac.h`, except with the
 * real `rtv0` / `rtir` encodings this toolchain assembles correctly. */
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

void func_actor_510900_80135744(Actor510900* arg0);
void func_actor_510900_8013864C(Actor510900* arg0);
void func_actor_510900_801387F4(Actor510900* arg0);
void func_actor_510900_80138978(Actor510900* arg0);
void func_actor_510900_80138A9C(Actor510900* arg0);
void func_actor_510900_80138BF0(Actor510900* arg0);
void func_actor_510900_80138D38(Actor510900* arg0);
void func_actor_510900_80138F44(Actor510900* arg0);
void func_actor_510900_8013B804(Actor510900* arg0);
void func_actor_510900_8013BB20(Actor510900* arg0);
void func_actor_510900_8013BC38(Actor510900* arg0, Actor510900Coord* arg1);
void func_actor_510900_8013BC80(Actor510900* arg0);

extern u8 D_801153F4;

/// The script block pair `Gp_SpawnScript18` is handed at blend 0x58; both live
/// in the room overlay, not here.
extern s32 D_80187D34;
extern s32 D_80187D3C;
extern u32 Gp_LcgState;
extern s16 D_80073BA0;

/// The player's world matrix; its third column is the direction they face.
extern MATRIX* D_80073B8C;

/// The pair source the context's `field_50` points at; its `field_4` seeds the
/// enemy's HP.
extern GpPairSrcE D_actor_510900_80167980;

/// The block the tick handler reaches through `Task::field_24`.
extern u32 D_actor_510900_80167A6C;

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_510900_80167AA4[];

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_80180A64(GsCOORDINATE2* arg0);

/// `field_59C` reload tables, indexed by four bits of `Gp_LcgState`.
extern s16 D_actor_510900_80167990[];
extern s16 D_actor_510900_801679B0[];
extern u8  D_80073BAA;

s32  func_actor_510900_8013691C(Actor510900* arg0);
void func_actor_510900_8013B424(s32 arg0);
void func_actor_510900_8013B524(Actor510900* arg0);

/// Sub-state machine on `field_590`. Off the corner (`field_5A8` != `field_5AA`)
/// a player range `field_5AE` of 0xBB8 or more forces sub-state 4, except from
/// 8. Sub-state 0 counts `field_59C` down and, unless
/// `func_actor_510900_8013691C` hands over, picks 1 or 5 from that range;
/// 2, 3 and 6 drain `field_59C` by 0x1D a frame (3 also spawns an effect every
/// 0x28 frames through `field_59E`). Each reload of `field_59C` is a 4-bit
/// `Gp_LcgState` draw from `D_actor_510900_80167990` or `D_actor_510900_801679B0`.
void func_actor_510900_80136184(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    GpEffWork*        eff;
    s32               snd;
    s32               rng;
    s16               speed;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (work->field_5AA != work->field_5A8 && work->field_590 != 8 && work->field_5AE >= 0xBB8) {
        work->field_590 = 4;
    }
    switch (work->field_590) {
        case 0:
            work->field_5A2 = 0;
            work->field_5B2 = 0;
            if (--work->field_59C <= 0) {
                if (work->field_594 == 2) {
                    work->field_594 = 1;
                }
                if (func_actor_510900_8013691C(arg0) == 0) {
                    if (work->field_5AE < 0x7D0) {
                        work->field_590 = 1;
                        work->field_586 = 2;
                        work->field_59C = 0;
                    } else if (arg0->field_20->node.field_5 == 1 && (u32)(D_80073BAA - 0xA) < 3U) {
                        work->field_590 = 5;
                        work->field_586 = 0x15;
                        snd             = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780003;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                            (s8)Gp_GetObjDepth((GpObj38*)coord));
                    } else {
                        work->field_590 = 1;
                        work->field_586 = 2;
                        work->field_59C = 1;
                    }
                }
            }
            break;
        case 1:
            work->field_5A2 = 0;
            if (work->field_58A >= 0xD) {
                if (work->field_59C == 0) {
                    work->field_590 = 2;
                    work->field_586 = 3;
                } else {
                    work->field_590 = 3;
                    work->field_586 = 4;
                    work->field_59E = 0x1D;
                }
                work->field_59C = D_actor_510900_80167990[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            }
            break;
        case 2:
            work->field_5A2  = 0x1D;
            work->field_59C -= 0x1D;
            if (work->field_59C <= 0 || work->field_5A6 > 0xB478U) {
                work->field_586 = 1;
                work->field_590 = 0;
                work->field_59C = D_actor_510900_801679B0[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            }
            if (work->field_5AE >= 0x7D0 && work->field_5AC < 0x3E8 && work->field_5A6 <= 0xAE9CU &&
                work->field_5A6 > 0xB477U) {
                work->field_590 = 0;
                work->field_586 = 1;
                work->field_59C = 0;
            }
            break;
        case 3:
            work->field_5A2  = 0x1D;
            work->field_59C -= 0x1D;
            if (--work->field_59E == 0) {
                eff = Gp_SpawnEff(0x80060044, &arg0->field_2C->field_8[4].field_0, 0, NULL);
                if (eff != NULL) {
                    Task_Reparent((Task*)arg0, eff->field_0);
                }
                work->field_59E = 0x28;
                snd             = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000E;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (arg0->field_20->node.field_5 == 1 && (u32)(D_80073BAA - 0xA) < 3U) {
                work->field_590 = 6;
                work->field_586 = 0x15;
                snd             = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780003;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_59C <= 0 || work->field_5A6 > 0xB478U) {
                work->field_586 = 1;
                work->field_590 = 0;
                work->field_59C = D_actor_510900_801679B0[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            }
            if (work->field_5AE >= 0x7D0 && work->field_5AC < 0x3E8 && work->field_5A6 <= 0xAE9CU &&
                work->field_5A6 > 0xB477U) {
                work->field_590 = 0;
                work->field_586 = 1;
                work->field_59C = 0;
            }
            break;
        case 4:
            speed           = 0;
            work->field_586 = 6;
            if (work->field_58A >= 8) {
                speed = 0xA0;
            }
            work->field_5A2 = speed;
            if (work->field_5AE < 0x5DC) {
                work->field_590 = 0;
                work->field_586 = 1;
                work->field_59C = 0;
            }
            if (work->field_5AE >= 0x7D0 && work->field_5AC < 0x3E8 && work->field_5A6 <= 0xAE9CU &&
                work->field_5A6 > 0xB477U) {
                work->field_590 = 0;
                work->field_586 = 1;
                work->field_59C = 0;
            }
            break;
        case 5:
            if (work->field_58A >= 0xF) {
                work->field_590 = 6;
                work->field_586 = 0x16;
                work->field_59C = D_actor_510900_80167990[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            }
            break;
        case 6:
            work->field_5A2  = 0x1D;
            work->field_59C -= 0x1D;
            if (work->field_59C <= 0 || work->field_5A6 > 0xB478U ||
                (work->field_5AE >= 0x7D0 && work->field_5AC < 0x3E8 && work->field_5A6 <= 0xAE9CU)) {
                work->field_590 = 7;
                work->field_586 = 0x17;
            }
            break;
        case 7:
            work->field_5A2 = 0;
            if (work->field_58A >= 0xD) {
                work->field_590 = 0;
                work->field_586 = 1;
                if (work->field_5AE >= 0x7D0 && work->field_5AC < 0x3E8 && work->field_5A6 <= 0xAE9CU &&
                    work->field_5A6 > 0xB477U) {
                    work->field_59C = 0;
                } else {
                    work->field_59C = D_actor_510900_801679B0[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
                }
            }
            break;
        case 8:
            if (work->field_5B0 < 0xC8 || work->field_5AA != work->field_5A8 || work->field_58A < 0xB ||
                work->field_58A >= 0x18) {
                work->field_5A2 = 0;
            } else {
                work->field_5A2 = -0xA7;
            }
            if (work->field_58A == 0x19) {
                snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780008;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_58A >= 0x32) {
                work->field_58E = 1;
                work->field_590 = 0;
                work->field_586 = 1;
                work->field_59C = 0;
            }
            break;
    }
}

/// Picks the handler the patrol hands over to, from the distance `field_5A6`
/// walked around the patrol square and the player's facing. Two spots on the
/// lap - 0x4B00 and 0x7F00, each with a 0x120 window - fire once apiece through
/// the `field_5BE` latch and send the actor into state 2; past 0xB477 the lap is
/// over and state 6 takes it. Otherwise, only on the frame the corner has been
/// reached (`field_5A8` == `field_5AA`), the player's range `field_5AC` and
/// whether they face the actor choose between states 2..5, each with the
/// animation the handler starts on. Returns 1 when a handler was selected.
s32 func_actor_510900_8013691C(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    s32               diff;
    s32               ret;
    s16               dist;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (work->field_5BE == 0) {
        diff = work->field_5A6 - 0x4B00;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff < 0x120) {
            work->field_5BE = 1;
            work->field_58E = 2;
            work->field_590 = 0;
            work->field_586 = 0xA;
            return 1;
        }
        diff = work->field_5A6 - 0x7F00;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff < 0x120) {
            work->field_5BE = 2;
            work->field_58E = 2;
            work->field_590 = 0;
            work->field_586 = 0xA;
            return 1;
        }
    }
    if (work->field_5A6 > 0xB477U) {
        work->field_58E = 6;
        work->field_590 = 0;
        work->field_586 = 0xE;
        return 1;
    }
    if (work->field_5A6 > 0xAE9CU) {
        return 0;
    }
    if (work->field_5A8 != work->field_5AA) {
        return 0;
    }
    if (D_80073B8C->m[0][2] * coord->field_0.coord.m[0][2] +
            D_80073B8C->m[2][2] * coord->field_0.coord.m[2][2] <
        0) {
        dist = work->field_5AC;
        if (dist < 0xAF0) {
            ret             = 1;
            work->field_58E = 2;
            work->field_590 = 0;
            work->field_586 = 0xA;
            goto done;
        }
        if (dist < 0xED8) {
            ret             = 1;
            work->field_58E = 3;
            work->field_590 = 0;
            work->field_586 = 0xE;
            goto done;
        }
        if (dist < 0x12C0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 0x10) & 1) {
                work->field_58E = 3;
                work->field_590 = 0;
                work->field_586 = 0xE;
            } else {
                work->field_58E = 4;
                work->field_590 = 0;
                work->field_586 = 0x1B;
            }
        } else {
            ret = 0;
            if (work->field_5BA == 0) {
                ret             = 1;
                work->field_58E = 4;
                work->field_590 = 0;
                work->field_586 = 0x1B;
            }
            goto done;
        }
    } else {
        if (work->field_5AC < 0xAF0) {
            work->field_58E = 2;
            work->field_590 = 0;
            work->field_586 = 0xA;
        } else {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (((Gp_LcgState >> 0x10) & 0xF) < 0xAU) {
                work->field_58E = 4;
                work->field_590 = 0;
                work->field_586 = 0x1B;
            } else {
                work->field_58E = 5;
                work->field_590 = 0;
                work->field_586 = 0xB;
            }
        }
    }
    ret = 1;
done:
    return ret;
}

/// State 2 (sub-states 0..3): the patrol walk. Sub-state 0 stops the actor and
/// hands sub-state 1 the animation 8 past blend 0x14. Sub-states 1 and 2 are
/// the two walk cycles: each feeds `field_5A2` 0x38 over a 0x13-blend window,
/// mutes it while `field_5B2` is latched and queues the footfall cue; sub-state
/// 1 also arms `field_5C0` when a grab candidate is pending and pushes both
/// body objects into their flagged pose at blend 9. At the end of a cycle a
/// 4-bit `Gp_LcgState` draw against `D_actor_510900_80167A10[field_59C]`
/// decides whether to walk another cycle - bumping `field_59C`, which sub-state
/// 2 caps at three - or to leave for state 1 with a fresh `field_59C` from
/// `D_actor_510900_801679D0`. A latched `field_5B2` instead sends sub-state 3,
/// the turn, with the animation 0x19; it queues its cue at blend 0x2D and past
/// 0x5A leaves the same way.
void func_actor_510900_80136B70(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    s32               snd;
    s32               pair;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    switch (work->field_590) {
        case 0:
            work->field_5A2 = 0;
            if (work->field_58A >= 0x14) {
                work->field_590 = 1;
                work->field_586 = 8;
                work->field_59C = 0;
            }
            break;
        case 1:
            work->field_5A2 = ((u32)((u16)work->field_58A - 9) < 0x13U) ? 0x38 : 0;
            if (work->field_5B2 != 0) {
                work->field_5A2 = 0;
            }
            if (work->field_58A == 0xA) {
                snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000B;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if ((work->field_5BE > 0) && (work->field_58A == 8)) {
                work->field_5C0 = 1;
            }
            if (work->field_58A == 9) {
                work->obj4E4.flags   |= 0x8000;
                work->obj504.flags   |= 0x8000;
                pair                  = Gp_PackPair(&D_actor_510900_80167968, 2);
                work->obj4E4.field_18 = pair;
                work->obj504.field_18 = pair;
            }
            if (work->field_58A >= 0x1D) {
                if (work->field_5B2 != 0) {
                    work->field_590     = 3;
                    work->field_586     = 0x19;
                    work->field_5B2     = 0;
                    work->obj4E4.flags &= 0x7FFF;
                    work->obj504.flags &= 0x7FFF;
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if (D_actor_510900_80167A10[work->field_59C] < (s32)((Gp_LcgState >> 0x10) & 0xF)) {
                        u16* tbl            = D_actor_510900_801679D0;
                        work->field_58E     = 1;
                        work->field_586     = 1;
                        work->field_590     = 0;
                        Gp_LcgState         = Gp_LcgState * 5 + 0x71357911;
                        work->field_59C     = tbl[(Gp_LcgState >> 0x10) & 0xF];
                        work->obj4E4.flags &= 0x7FFF;
                        work->obj504.flags &= 0x7FFF;
                    } else {
                        work->field_590 = 2;
                        work->field_586 = 9;
                        work->field_59C++;
                    }
                }
            }
            break;
        case 2:
            work->field_5A2 = ((u32)((u16)work->field_58A - 5) < 0x13U) ? 0x38 : 0;
            if (work->field_5B2 != 0) {
                work->field_5A2 = 0;
            }
            if (work->field_58A == 6) {
                snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000B;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_58A >= 0x27) {
                if (work->field_5B2 != 0) {
                    work->field_590     = 3;
                    work->field_586     = 0x19;
                    work->field_5B2     = 0;
                    work->obj4E4.flags &= 0x7FFF;
                    work->obj504.flags &= 0x7FFF;
                } else {
                    if ((D_actor_510900_80167A10[work->field_59C] <
                         (s32)(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 0x10) & 0xF)) ||
                        (work->field_59C >= 3)) {
                        u16* tbl            = D_actor_510900_801679D0;
                        work->field_58E     = 1;
                        work->field_590     = 0;
                        work->field_586     = 1;
                        Gp_LcgState         = Gp_LcgState * 5 + 0x71357911;
                        work->field_59C     = tbl[(Gp_LcgState >> 0x10) & 0xF];
                        work->obj4E4.flags &= 0x7FFF;
                        work->obj504.flags &= 0x7FFF;
                    } else {
                        work->field_59C++;
                        work->field_590 = 1;
                        work->field_586 = 8;
                    }
                }
            }
            break;
        case 3:
            if (work->field_58A == 0x2D) {
                snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000A;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_58A >= 0x5A) {
                u16* tbl        = D_actor_510900_801679D0;
                work->field_58E = 1;
                work->field_590 = 0;
                work->field_586 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_59C = tbl[(Gp_LcgState >> 0x10) & 0xF];
            }
            break;
    }
}

/// State 3 (sub-state 0/1): the wind-up. Sub-state 0 queues the two-part
/// charge sound at blend 0x39 - setting `field_594`/`field_598` and pushing
/// both body objects into their flagged pose - a second cue at 0x41, and past
/// 0x5A hands sub-state 1 the animation 0xF. Sub-state 1 converts `field_5AC`
/// into the `field_59C` budget at blend 0xE, feeds `field_5A2` from it over
/// blends 0x17..0x2F, queues two more cues, and past 0x46 leaves for either
/// state 8 (when `field_5B2` is set) or state 1 with a fresh `field_59C`,
/// clearing both flagged poses on the way out.
void func_actor_510900_80137008(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    s32               snd;
    s32               pair;
    s32               val;
    s32               cur;
    u32               rng;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    switch (work->field_590) {
        case 0:
            work->field_5A2 = 0;
            if (work->field_58A == 0x39) {
                work->field_594 = 1;
                work->field_598 = 0x55;
                snd             = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000E;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->field_57C = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000D;
                SndEvt_EnqueueType6(work->field_57C, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->obj4E4.flags   |= 0x8000;
                work->obj504.flags   |= 0x8000;
                pair                  = Gp_PackPair(&D_actor_510900_80167968, 5);
                work->obj4E4.field_18 = pair;
                work->obj504.field_18 = pair;
            }
            if (work->field_58A == 0x41) {
                snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780006;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_58A >= 0x5A) {
                work->field_586 = 0xF;
                work->field_590 = 1;
            }
            break;
        case 1:
            if (work->field_58A == 0xE) {
                cur = work->field_5AC;
                val = 0x1388;
                if (cur < 0x1389) {
                    val = cur;
                }
                work->field_59C = (val - 0x4B0) / 24;
            }
            work->field_5A2 = ((u32)((u16)work->field_58A - 0x17) < 0x19U) ? work->field_59C : 0;
            if (work->field_58A == 0x19) {
                work->obj4E4.flags   |= 0x8000;
                work->obj504.flags   |= 0x8000;
                pair                  = Gp_PackPair(&D_actor_510900_80167968, 1);
                work->obj4E4.field_18 = pair;
                work->obj504.field_18 = pair;
                snd                   = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780007;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_58A == 0x2F) {
                snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780008;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_58A >= 0x46) {
                if (work->field_5B2 != 0) {
                    work->field_58E = 1;
                    work->field_590 = 8;
                    work->field_5B2 = 0;
                    work->field_586 = 5;
                } else {
                    work->field_58E = 1;
                    work->field_586 = 1;
                    work->field_590 = 0;
                    work->field_59C = D_actor_510900_801679D0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 0x10) & 0xF];
                    Gp_LcgState     = rng;
                }
                work->obj4E4.flags &= 0x7FFF;
                work->obj504.flags &= 0x7FFF;
            }
            break;
    }
}

/// Handler that spawns the actor's companion enemy once the 0x14 animation has
/// blended in (`field_58A` == 0x14): the spawned task's model takes its texture
/// page and CLUT row from the current area record, indexed by the enemy's own
/// bank nibble, and a sound is queued from the actor's attach coordinate. Past
/// blend 0x46 the handler leaves for either state 5 (animation 0xB) or, on a
/// failed `Gp_LcgState` roll, state 1 with a fresh `field_59C`.
void func_actor_510900_801373B8(Actor510900* arg0)
{
    Actor510900Work*  work;
    GpEnemy*          enemy;
    Actor510900Coord* coord;
    GameSessionFrom4* sessionKey;
    TmdObject*        model;
    GpAreaRec*        rec;
    GpCdRec10*        entry;
    GpAreaKey         key;
    s32               idx;
    s32               snd;
    s32               pan;
    s32               roll;
    u32               rng;

    roll  = 0;
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    enemy = arg0->field_20;

    work->field_5A2 = 0;
    if (work->field_58A == 0x14) {
        work->field_5BA = 1;
        model           = Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 4, roll, enemy)->task->extra;
        idx             = (u16)enemy->field_8 >> 0xC;
        sessionKey      = (GameSessionFrom4*)&Game_Session->field_4;
        key.field_3     = sessionKey->field_3;
        key.field_2     = sessionKey->field_2;
        key.field_1     = sessionKey->field_1;
        key.field_0     = Game_Session->field_4;
        Gp_SyncAreaKeyIndex(&key);
        rec = Gp_GetNestedAreaRec(&key);
        /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled
           index onto the table (`addu s0, s0, v0`). */
        entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = entry->field_D;
        model->field_25 = entry->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
        snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780012;
        pan = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    if (work->field_58A >= 0x46) {
        if (work->field_5AC >= 0xEA7) {
            /* Reading the global back is what keeps the store ahead of the
               shift: written as a local, the store sinks into the branch
               delay slot and the draw lands in a different register. */
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (((Gp_LcgState >> 0x10) & 0xF) < 0xCU) {
                roll = 1;
            }
        }
        if (roll == 0) {
            work->field_58E = 1;
            work->field_586 = 1;
            work->field_590 = 0;
            work->field_59C = D_actor_510900_801679D0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 0x10) & 0xF];
            Gp_LcgState     = rng;
            return;
        }
        work->field_58E = 5;
        work->field_590 = 0;
        work->field_586 = 0xB;
    }
}

/// State 5 (animation 0xB): the actor rears up, holds, then either drops back
/// to state 1 or commits. Sub-state 0 ramps `field_5A2` at blend 0x47, queues
/// the rear-up sound at 0x4A and hands sub-state 1 the animation 0xC at 0x52.
/// Sub-state 1 pushes both body objects into their flagged pose while
/// `field_5AC` is short; otherwise it bleeds `field_59C` down by 0x84 a frame
/// and, once that runs out (or `field_5AE` drops below 0x384), returns to
/// state 1 with a fresh `Gp_LcgState` draw. Sub-state 2 queues the landing
/// sound at blend 0xE and leaves for state 8 past 0x3B.
void func_actor_510900_801375D8(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    s32               snd;
    s32               pair;
    s32               val;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    switch (work->field_590) {
        case 0:
            work->field_5A2 = (work->field_58A < 0x47) ? 0 : 0x84;
            if (work->field_58A == 0x4A) {
                snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000F;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_58A >= 0x52) {
                work->field_586 = 0xC;
                work->field_590 = 1;
                work->field_59C = 0xEA6;
            }
            break;
        case 1:
            work->field_5A2 = 0x84;
            if (work->field_5AC < 0x514) {
                work->field_590       = 2;
                work->field_586       = 0xD;
                work->obj4E4.flags   |= 0x8000;
                work->obj504.flags   |= 0x8000;
                pair                  = Gp_PackPair(&D_actor_510900_80167968, 0);
                work->obj4E4.field_18 = pair;
                work->obj504.field_18 = pair;
            } else {
                work->field_59C -= 0x84;
                if (work->field_59C < 0 || work->field_5AE < 0x384) {
                    work->field_58E     = 1;
                    work->field_590     = 0;
                    work->field_586     = 1;
                    Gp_LcgState         = Gp_LcgState * 5 + 0x71357911;
                    val                 = D_actor_510900_801679D0[(Gp_LcgState >> 16) & 0xF];
                    work->obj4E4.flags &= 0x7FFF;
                    work->obj504.flags &= 0x7FFF;
                    work->field_59C     = val;
                }
            }
            break;
        case 2:
            work->field_5A2 = 0;
            if (work->field_58A == 0xE) {
                snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000C;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_58A >= 0x3B) {
                work->field_590     = 8;
                work->field_586     = 5;
                work->field_58E     = 1;
                work->obj4E4.flags &= 0x7FFF;
                work->obj504.flags &= 0x7FFF;
            }
            break;
    }
}

/// Handler for the scripted sequence the actor plays out through `field_590`:
/// state 0 opens with the two 0x4078 cues at blend 0x39 and hands over at 0x5A,
/// state 1 plays one more cue at blend 0xA and hands over at 0x64, state 2 runs
/// the long beat - a pair swap and a screen fade at 0x50, a cue at 0x53 whose id
/// depends on `field_594`, a script spawn at 0x58, the pair blanked at 0x60 -
/// and either enters state 3 when `field_5B2` is latched or, past blend 0xB3,
/// drops back to `field_58E` state 1 with a fresh `field_59C`. State 3 keeps the
/// walk speed inside its two blend windows and, every sixth frame, restarts the
/// effect on the player's model; its `field_5B6` sub-state queues file 0x1E and
/// plays the arrival cue once the drive goes idle.
///
/// `blend` is one temp on purpose: the `lh` of `field_586` leaves its high bits
/// unknown to combine, which is what keeps the `andi 0xFFFF` on the second
/// window test of each chain.
void func_actor_510900_80137868(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    s32               snd;
    s32               pair;
    s32               blend;
    u32               rng;

    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;

    *(u8**)G_SCRATCH_HEAD -= 0x10;

    switch (work->field_590) {
        case 0:
            work->field_5B4 = 1;
            work->field_5A2 = 0;
            if (work->field_58A == 0x39) {
                work->field_594 = 1;
                work->field_598 = 0x10E;
                snd             = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000E;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->field_57C = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000D;
                SndEvt_EnqueueType6(work->field_57C, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->obj4E4.flags   |= 0x8000;
                work->obj504.flags   |= 0x8000;
                pair                  = Gp_PackPair(&D_actor_510900_80167968, 5);
                work->obj4E4.field_18 = pair;
                work->obj504.field_18 = pair;
            }
            if (work->field_58A >= 0x5A) {
                work->field_586 = 0x1A;
                work->field_590 = 1;
            }
            break;
        case 1:
            if (work->field_58A == 0xA) {
                snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780006;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (work->field_58A >= 0x64) {
                work->field_586 = 7;
                work->field_590 = 2;
            }
            break;
        case 2:
            if (work->field_58A == 0x50) {
                work->field_5B4       = 2;
                work->obj4E4.flags   |= 0x8000;
                work->obj504.flags   |= 0x8000;
                pair                  = Gp_PackPair(&D_actor_510900_80167968, 3);
                work->obj4E4.field_18 = pair;
                work->obj504.field_18 = pair;

                Game_Session->field_12F = 0x80;
                Game_Session->field_12D = 0x7F;
            }
            if (work->field_58A == 0x53) {
                if (work->field_594 == 1) {
                    snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780010;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                        (s8)Gp_GetObjDepth((GpObj38*)coord));
                } else {
                    snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000C;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord),
                                        (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
            }
            if (work->field_58A == 0x58) {
                Gp_SpawnScript18((s32)&D_80187D34, (s32)&D_80187D3C);
            }
            if (work->field_58A == 0x60) {
                work->obj4E4.flags &= 0x7FFF;
                work->obj504.flags &= 0x7FFF;
            }
            blend = (u16)work->field_58A;
            if (blend - 0x4B < 0x10U) {
                work->field_5A2 = 0x3C;
            } else if (((blend - 0x8F) & 0xFFFF) < 0xFU) {
                work->field_5A2 = -0x40;
            } else {
                work->field_5A2 = 0;
            }
            if (work->field_58A >= 0x51) {
                if (work->field_5B2 == 1) {
                    work->field_590 = 3;
                    work->field_5B2 = 0;
                    work->field_5B6 = 1;
                    D_80073BA0      = 0;
                    Gp_PulseState1C80();
                }
            } else {
                work->field_5B2 = 0;
            }
            if (work->field_58A >= 0xB3) {
                work->field_58E = 1;
                work->field_586 = 1;
                work->field_590 = 0;
                work->field_59C = D_actor_510900_801679D0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 0x10) & 0xF];
                Gp_LcgState     = rng;
            }
            break;
        case 3:
            blend = work->field_586;
            if (blend == 7) {
                blend = (u16)work->field_58A;
                if (blend - 0x4B < 0x10U) {
                    work->field_5A2 = 0x3C;
                } else if (((blend - 0x8F) & 0xFFFF) < 0xFU) {
                    work->field_5A2 = -0x10;
                } else {
                    work->field_5A2 = 0;
                }
                if (work->field_58A >= 0xAA) {
                    work->field_586 = 0x19;
                }
            }
            work->field_59C++;
            if (work->field_59C >= 6) {
                work->field_59C                 = 0;
                D_actor_510900_80167B7C.field_0 = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
                func_800FDB18(5, &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[4], NULL,
                              &D_actor_510900_80167B7C);
            }
            switch (work->field_5B6) {
                case 0:
                    break;
                case 1:
                    CdCmd_EnqueueLoadFile(9, 0x1E, 3);
                    work->field_5B6 = 2;
                    break;
                case 2:
                    if (CdCmd_IsIdle() == 1) {
                        coord = (Actor510900Coord*)((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
                        SndEvt_EnqueueType6(0x70010001, (s8)Gp_GetObjPan((GpObj38*)coord),
                                            (s8)Gp_GetObjDepth((GpObj38*)coord));
                        work->field_5B6 = 0;
                    }
                    break;
            }
            break;
    }

    *(u8**)G_SCRATCH_HEAD += 0x10;
}

void func_actor_510900_80137E20(Actor510900* arg0)
{
    Actor510900Work*  work;
    GpEnemy*          enemy;
    Actor510900Coord* coord;
    s32               snd;
    s32               pan;
    s32               rng;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    coord = arg0->field_2C->field_8;
    switch (work->field_590) {
        case 0:
            work->field_586 = 0x10;
            work->field_5A2 = 0;
            work->field_590 = 1;
            snd             = (((u16)enemy->field_8 >> 0xC) << 8) | 0x40780005;
            pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            break;
        case 1:
            if (work->field_5B0 >= 0xC8 && work->field_5AA == work->field_5A8) {
                if ((u16)work->field_58A >= 3 && (u16)work->field_58A < 13) {
                    work->field_5A2 = -0x2C;
                } else if ((u16)work->field_58A >= 0x12 && (u16)work->field_58A < 0x27) {
                    work->field_5A2 = -0x1E;
                } else {
                    work->field_5A2 = 0;
                }
            } else {
                work->field_5A2 = 0;
            }
            if (work->field_58A >= 0x53) {
                if (enemy->field_40 <= 0) {
                    work->field_58E = 0xC;
                    work->field_590 = 0;
                    work->field_586 = 0x18;
                } else {
                    work->field_58E = 1;
                    work->field_586 = 1;
                    work->field_590 = 0;
                    work->field_59C =
                        D_actor_510900_801679F0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
                    Gp_LcgState = rng;
                }
            }
            break;
    }
}

void func_actor_510900_80137FBC(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    GpEnemy*          enemy;
    SVECTOR*          rot;
    void*             head;
    s32               startPan;
    s32               loopPan;
    s16               step;
    u32               rng;

    head                = *(void**)0x1F8003FC;
    *(void**)0x1F8003FC = (u8*)head - 8;
    work                = arg0->field_1C;
    enemy               = arg0->field_20;
    coord               = arg0->field_2C->field_8;
    switch (work->field_590) {
        case 0:
            work->field_586 = 0x12;
            work->field_5A2 = 0;
            work->field_590 = 1;
            work->field_59C = 0;
            work->field_578 = ((enemy->field_8 >> 0xC) << 8) | 0x40780009;
            startPan        = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(work->field_578, startPan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            break;
        case 1:
            if (work->field_58A & 1) {
                rot         = (SVECTOR*)((u8*)head - 8);
                rot->vx     = 0;
                rot->vz     = 0;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                rot->vy     = ((Gp_LcgState >> 0x10) & 0x2FF) - 0x680;
                Gp_SpawnEff(0x600E0, &coord->field_0, 0x100, rot);
            }
            work->field_59C++;
            if (work->field_59C >= 3) {
                loopPan = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueTypeA(work->field_578, loopPan, (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->field_59C = 0;
            }
            if ((work->field_5B0 < 0xC8) || (work->field_5AA != work->field_5A8)) {
                work->field_5A2 = 0;
            } else {
                step = 0;
                if ((u32)((u16)work->field_58A - 0x47) < 7) {
                    step = -0x53;
                }
                work->field_5A2 = step;
            }
            if (work->field_58A >= 0x51) {
                if (work->field_578 != 0) {
                    SndEvt_EnqueueType7(work->field_578, 0);
                    work->field_578 = 0;
                }
                if (enemy->field_40 <= 0) {
                    work->field_58E = 0xC;
                    work->field_590 = 0;
                    work->field_586 = 0x18;
                } else {
                    work->field_58E = 1;
                    work->field_586 = 1;
                    work->field_590 = 0;
                    work->field_59C =
                        D_actor_510900_801679F0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 0x10) & 0xF];
                    Gp_LcgState = rng;
                }
            }
            break;
    }
    *(u32*)0x1F8003FC += 8;
}

void func_actor_510900_80138250(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    GpEnemy*          enemy;
    SVECTOR*          rot;
    void*             head;
    s32               startPan;
    s32               loopPan;
    u32               rng;

    head                = *(void**)0x1F8003FC;
    *(void**)0x1F8003FC = (u8*)head - 8;
    work                = arg0->field_1C;
    enemy               = arg0->field_20;
    coord               = arg0->field_2C->field_8;
    switch (work->field_590) {
        case 0:
            work->field_5A2 = 0;
            work->field_586 = 0x13;
            work->field_590 = 1;
            work->field_59C = 0;
            work->field_578 = ((enemy->field_8 >> 0xC) << 8) | 0x40780009;
            startPan        = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(work->field_578, startPan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            break;
        case 1:
            if (work->field_58A & 1) {
                rot         = (SVECTOR*)((u8*)head - 8);
                rot->vx     = 0;
                rot->vz     = 0;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                rot->vy     = ((Gp_LcgState >> 0x10) & 0x2FF) - 0x680;
                Gp_SpawnEff(0x600E0, &coord->field_0, 0x100, rot);
            }
            work->field_59C++;
            if (work->field_59C >= 3) {
                loopPan = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueTypeA(work->field_578, loopPan, (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->field_59C = 0;
            }
            if (work->field_58A >= 0x1E) {
                if (work->field_578 != 0) {
                    SndEvt_EnqueueType7(work->field_578, 0);
                    work->field_578 = 0;
                }
                work->field_586 = 0x14;
                work->field_590 = 2;
            }
            break;
        case 2:
            if (work->field_58A >= 0x3B) {
                if (enemy->field_40 <= 0) {
                    work->field_58E = 0xC;
                    work->field_590 = 0;
                    work->field_586 = 0x18;
                } else {
                    work->field_58E = 1;
                    work->field_586 = 1;
                    work->field_590 = 0;
                    work->field_59C =
                        D_actor_510900_801679F0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 0x10) & 0xF];
                    Gp_LcgState = rng;
                }
            }
            break;
    }
    *(u32*)0x1F8003FC += 8;
}

void func_actor_510900_801384C4(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    s32               snd;
    s32               pan;
    u32               rng;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (D_80073BA0 <= 0) {
        arg0->field_20->field_40 = 1;
        work->field_58E          = 1;
        work->field_586          = 1;
        work->field_590          = 0;
        work->field_59C          = D_actor_510900_801679F0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
        Gp_LcgState              = rng;
        return;
    }
    if (work->field_594 != 0) {
        work->field_594 = 3;
    }
    work->field_598 = 0;
    if (work->field_578 != 0) {
        SndEvt_EnqueueType7(work->field_578, 0);
        work->field_578 = 0;
    }
    if (work->field_57C != 0) {
        SndEvt_EnqueueType7(work->field_57C, 0);
        work->field_57C = 0;
    }
    if (work->field_580 != 0) {
        SndEvt_EnqueueType7(work->field_580, 0);
        work->field_580 = 0;
    }
    arg0->field_20->node.field_4 = 1;
    if (work->field_58A == 0x70) {
        snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780007;
        pan = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    Gp_StateC08.field_6 &= 1;
    work->field_592      = 0;
}

/// Latches which of the four `D_actor_510900_80167BA4` boxes the player stands
/// in into `field_5AA`, records how far `field_5A6` is from the near and far
/// ends of the current patrol side, and measures the straight-line distance
/// from the actor's attach coordinate to the player into `field_5AC`.
void func_actor_510900_8013864C(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    VECTOR*           delta;
    void*             head;
    s32               i;
    s32               dx;
    s32               dz;

    work                = arg0->field_1C;
    head                = *(void**)0x1F8003FC;
    coord               = arg0->field_2C->field_8;
    *(void**)0x1F8003FC = (u8*)head - 0x10;
    delta               = (VECTOR*)((u8*)head - 0x10);

    for (i = 0; i < 4; i++) {
        if (D_actor_510900_80167BA4[i].minX < Player_Status.coordMtx->t[0] &&
            Player_Status.coordMtx->t[0] < D_actor_510900_80167BA4[i].maxX &&
            D_actor_510900_80167BA4[i].minZ < Player_Status.coordMtx->t[2] &&
            Player_Status.coordMtx->t[2] < D_actor_510900_80167BA4[i].maxZ) {
            work->field_5AA = i;
            break;
        }
    }

    work->field_5B0 = __builtin_abs(work->field_5A8 * 13200 - work->field_5A6);
    work->field_5AE = __builtin_abs((work->field_5A8 + 1) * 13200 - work->field_5A6);

    dx                 = Player_Status.coordMtx->t[0] - coord->field_0.coord.t[0];
    delta->vx          = dx;
    delta->vy          = 0;
    dz                 = Player_Status.coordMtx->t[2] - coord->field_0.coord.t[2];
    delta->vz          = dz;
    work->field_5AC    = SquareRoot0(dx * dx + dz * dz);
    *(u32*)0x1F8003FC += 0x10;
}

void func_actor_510900_801387F4(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    SVECTOR*          rot;
    void*             head;
    u16               target;
    u16               cur;
    s32               d;
    s16               diff;
    s32               sdiff;
    s32               mag;
    s32               prev;
    s32               prev2;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (work->field_5A2 != 0) {
        head                = *(void**)0x1F8003FC;
        *(void**)0x1F8003FC = (u8*)head - 8;
        rot                 = (SVECTOR*)((u8*)head - 8);
        if (work->field_5AE < 0x3E8) {
            target = D_actor_510900_80167B9C[work->field_5A8 + 1];
        } else {
            target = D_actor_510900_80167B9C[work->field_5A8];
        }
        cur             = ratan2(coord->field_0.coord.m[0][2], coord->field_0.coord.m[2][2]) & 0xFFF;
        d               = target - cur;
        diff            = d;
        sdiff           = (s16)d;
        mag             = __builtin_abs(sdiff);
        work->field_5A0 = cur;
        if (mag < 0x800) {
            if (mag < 0x1F) {
                work->field_5A0 = target;
            } else {
                prev = work->field_5A0;
                if (sdiff > 0) {
                    work->field_5A0 = prev + 0x1E;
                } else {
                    work->field_5A0 = prev - 0x1E;
                }
            }
        } else if (sdiff > 0 ? (0x1000 - sdiff) < 0x1F : (sdiff + 0x1000) < 0x1F) {
            work->field_5A0 = target;
        } else {
            prev2 = work->field_5A0;
            if (diff > 0) {
                work->field_5A0 = prev2 - 0x1E;
            } else {
                work->field_5A0 = prev2 + 0x1E;
            }
        }
        rot->vx = 0;
        rot->vy = work->field_5A0;
        rot->vz = 0;
        RotMatrix(rot, &coord->field_0.coord);
        *(u32*)0x1F8003FC += 8;
    }
}

/// Walks the actor once around a fixed square patrol path: `field_5A6` is the
/// distance travelled, advanced by `field_5A2` and clamped, and its quotient by
/// the side length selects the corner (also latched into `field_5A8` for the
/// turn handler) while the remainder is the offset along that side.
void func_actor_510900_80138978(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    s16               side;
    s16               along;
    u16               dist;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;

    dist            = work->field_5A6 + (u16)work->field_5A2;
    work->field_5A6 = dist;
    if (dist < 0xC8) {
        work->field_5A6 = 0xC8;
    } else if (dist > 0xB66C) {
        work->field_5A6 = 0xB66C;
    }

    side            = work->field_5A6 / 13200;
    work->field_5A8 = side;
    along           = work->field_5A6 % 13200;

    coord->field_0.coord.t[0] = D_actor_510900_80167B84[side].x + (along * D_actor_510900_80167B94[side].x);
    coord->field_0.coord.t[1] = 0;
    coord->field_0.coord.t[2] =
        D_actor_510900_80167B84[work->field_5A8].z + (along * D_actor_510900_80167B94[work->field_5A8].z);
}

/// Fires the actor's step sounds: while the current animation record carries
/// `field_3` bit 0x20 or 0x10, a sound is queued on the frame that bit has just
/// dropped from `Actor510900Work::field_59A`, panned and depth-attenuated from
/// the actor's attach coordinate. The record's two bits are latched for the
/// next frame at the end.
void func_actor_510900_80138A9C(Actor510900* arg0)
{
    s32               snd;
    s32               pan;
    s32               pan2;
    Actor510900Work*  work;
    Actor510900Coord* coord;
    GpAnimRec*        rec;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    rec   = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->obj38.prev);
    if (rec != NULL) {
        if (!(rec->field_3 & 0x20) && (work->field_59A & 0x20)) {
            snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780001;
            pan = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        if (!(rec->field_3 & 0x10) && (work->field_59A & 0x10)) {
            snd  = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780002;
            pan2 = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        work->field_59A = (u16)(rec->field_3 & 0x30);
    }
}

/// Aims the head coordinate (`field_8[4]`) at the player. Takes the head into
/// view space, offsets the player position by 0x600 in Y, rotates that delta
/// into the body's frame, clamps it to +/-0x400 yaw, +/-0x300 pitch and a
/// minimum 0x200 forward, then builds the head rotation from it.
///
/// `head` is kept as its own pointer rather than indexing `coord` twice: CSE
/// folds `head->workm` back onto `coord + 0x164` while `head` stays live, which
/// is what puts the `coord += 0x140` in the clamp's branch delay slot. The
/// `+ 0x600` likewise needs the temporary, or it is sunk into the subtrahend as
/// `- 0x600` on the player coordinate.
void func_actor_510900_80138BF0(Actor510900* arg0)
{
    Actor510900AimScratch* scratch;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         head;
    s32                    offsetY;

    coord                   = &arg0->field_2C->field_8->field_0;
    head                    = &coord[4];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor510900AimScratch);
    scratch                 = (Actor510900AimScratch*)*(void**)G_SCRATCH_HEAD;

    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &head->workm, &scratch->view);
    scratch->delta.vx = Player_Status.coordMtx->t[0] - scratch->view.t[0];
    offsetY           = scratch->view.t[1] + 0x600;
    scratch->delta.vy = Player_Status.coordMtx->t[1] - offsetY;
    scratch->delta.vz = Player_Status.coordMtx->t[2] - scratch->view.t[2];
    ApplyTransposeMatrixLV(&coord->coord, &scratch->delta, &scratch->local);

    if (scratch->local.vx < -0x400) {
        scratch->local.vx = -0x400;
    } else if (scratch->local.vx > 0x400) {
        scratch->local.vx = 0x400;
    }
    if (scratch->local.vy < -0x300) {
        scratch->local.vy = -0x300;
    } else if (scratch->local.vy > 0x300) {
        scratch->local.vy = 0x300;
    }
    if (scratch->local.vz < 0x200) {
        scratch->local.vz = 0x200;
    }
    Gp_OrientAlong(&scratch->local, &head->coord, 0);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor510900AimScratch);
}

/// Yaws the head coordinate (`field_8[3]`) by the residual rotation in
/// `Actor510900Work::field_570` and then walks that residual 0x20 back towards
/// zero on each axis, snapping to zero inside the last step. `field_584` is
/// cleared on the frame both axes have come to rest.
///
/// The scratch head is taken through `Actor510900ScratchStack` rather than as a
/// bare pointer: the struct store keeps the `arg0->field_1C` load below it.
void func_actor_510900_80138D38(Actor510900* arg0)
{
    Actor510900Work* work;
    GsCOORDINATE2*   coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    matrix                                         = (MATRIX*)(((Actor510900ScratchStack*)G_SCRATCH_HEAD)->sp - 0x20);
    ((Actor510900ScratchStack*)G_SCRATCH_HEAD)->sp = (u32)matrix;
    active                                         = 0;
    work                                           = arg0->field_1C;
    coord                                          = &arg0->field_2C->field_8->field_0;
    RotMatrix(&work->field_570, matrix);
    USE_REG(matrix);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(matrix);
    gte_rtir_real();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&matrix->m[0][1]);
    gte_rtir_real();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&matrix->m[0][2]);
    gte_rtir_real();
    gte_stclmv(&coord[3].coord.m[0][2]);

    angleX = work->field_570.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->field_570.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->field_570.vx = nextX;
            active             = 1;
        }
    }

    angleY = work->field_570.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->field_570.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->field_570.vy = nextY;
            active             = 1;
        }
    }

    if (active == 0) {
        work->field_584 = 0;
    }
    (*(u32*)G_SCRATCH_HEAD) += 0x20;
}

/// Rebuilds the three collision faces this actor occupies in the grid, at the
/// body's current position and facing. `center` is the fixed local footprint
/// offset rotated into world space, translated by the coordinate and clamped to
/// the grid extent; the twelve corners in `D_actor_510900_80167BDC` are rotated
/// and offset from it into `Gp_GridParams->field_8`, and the three face normals
/// in `D_actor_510900_80167BC4` are rotated in place into `field_4`.
void func_actor_510900_80138F44(Actor510900* arg0)
{
    Actor510900GridScratch* scratch;
    GsCOORDINATE2*          coord;
    SVECTOR*                normals;
    SVECTOR*                corners;
    s32                     i;

    scratch = (Actor510900GridScratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(Actor510900GridScratch));
    coord   = &arg0->field_2C->field_8->field_0;
    normals = Gp_GridParams->field_4;
    corners = Gp_GridParams->field_8;

    scratch->center.vx = -0x258;
    scratch->center.vy = 0;
    scratch->center.vz = 0x280;

    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->center);
    gte_rtv0_real();
    gte_stsv(&scratch->rotated);

    scratch->center.vx = coord->coord.t[0] + scratch->rotated.vx;
    scratch->center.vy = coord->coord.t[1] + scratch->rotated.vy;
    scratch->center.vz = coord->coord.t[2] + scratch->rotated.vz;

    if (scratch->center.vx > 0x1770) {
        scratch->center.vx = 0x1770;
    } else if (scratch->center.vx < -0x1770) {
        scratch->center.vx = -0x1770;
    }
    if (scratch->center.vz > 0x1770) {
        scratch->center.vz = 0x1770;
    } else if (scratch->center.vz < -0x1770) {
        scratch->center.vz = -0x1770;
    }

    for (i = 0; i < 12; i++) {
        gte_SetRotMatrix(&coord->coord);
        gte_ldv0(&D_actor_510900_80167BDC[i]);
        gte_rtv0_real();
        gte_stsv(&scratch->rotated);
        corners[i].vx = scratch->rotated.vx + scratch->center.vx;
        corners[i].vy = scratch->rotated.vy + scratch->center.vy;
        corners[i].vz = scratch->rotated.vz + scratch->center.vz;
    }

    for (i = 0; i < 3; i++) {
        gte_SetRotMatrix(&coord->coord);
        gte_ldv0(&D_actor_510900_80167BC4[i]);
        gte_rtv0_real();
        gte_stsv(&normals[i]);
    }

    *(u8**)G_SCRATCH_HEAD += sizeof(Actor510900GridScratch);
}

/// Message 0x7D7 handler (entry in `D_actor_510900_80167A6C`). `arg2` picks
/// between three visibility/liveness states of the boss and its two companion
/// enemies, whose models are reached through `field_568` / `field_56C`.
///
/// 0 parks the actor: both models and its own drop to blend 0 and `field_5A4`
/// becomes 1, after taking a `Gp_StateF0` reference (release id 0x1B).
/// 1 wakes it: the same blend reset, then the patrol is restarted from
/// distance 0x11F8 with animation 0x1A and yaw 0x400, the head coordinate is
/// rebuilt from that yaw with its translation cleared, and slots 1..0x12 are
/// reseeded. The first wake (`field_594` still 0) also pushes both body objects
/// into their flagged pose and queues the charge cue.
/// 2 puts it away: blend 0x80 everywhere, the linked task is handed state 4 and
/// dropped, the flagged poses are cleared, and the collision grid this actor
/// edited is restored - the three faces `func_actor_510900_8013B524` copies back
/// plus the fourth `func_actor_510900_8013B424` zeroes, both written out inline.
///
/// `vec` is one `SVECTOR*` serving two unrelated roles - the scratch rotation
/// state 1 builds the head matrix from, and the extra face normal state 2
/// clears - which is what puts it in `$a0` in both. The `do`/`while (0)` cuts
/// the basic block so the second `Gp_GridParams->field_4` read is scheduled on
/// its own; without it the corner pointer and its walking copy coalesce.
s32 func_actor_510900_801391B8(Actor510900* arg0, s32 arg1, s32 arg2)
{
    Actor510900Work*  work;
    Actor510900Obj2C* obj;
    Actor510900Coord* coord;
    GpEnemy*          enemy;
    SVECTOR*          rot;
    void*             head;
    SVECTOR*          normals;
    SVECTOR*          verts;
    GpGridFace*       faces;
    s32               pair;
    s32               i;
    s32               j;
    s32               k;
    SVECTOR*          vec;
    SVECTOR*          corners;

    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = (u8*)head - sizeof(SVECTOR);
    rot                     = (SVECTOR*)*(void**)G_SCRATCH_HEAD;
    obj                     = arg0->field_2C;
    work                    = arg0->field_1C;
    enemy                   = arg0->field_20;
    coord                   = obj->field_8;

    switch (arg2) {
        case 0:
            ((void (*)(s32))Gp_IncStateF0Ref)(0x1B);
            obj->field_C                                  = 0;
            ((TmdObject*)work->field_568->extra)->field_C = 0;
            ((TmdObject*)work->field_56C->extra)->field_C = 0;
            work->field_5A4                               = 1;
            break;
        case 1:
            obj->field_C                                  = 0;
            ((TmdObject*)work->field_568->extra)->field_C = 0;
            ((TmdObject*)work->field_56C->extra)->field_C = 0;
            work->field_5A4                               = 2;
            work->field_5A6                               = 0x11F8;
            vec                                           = rot;
            work->field_586                               = 0x1A;
            work->field_588                               = 0x1A;
            work->field_5A0                               = 0x400;
            work->field_58E                               = 0;
            work->field_590                               = 0;
            work->field_5A2                               = 0;
            work->field_58A                               = 0;
            work->obj47C.flags                           |= 0x8000;
            enemy->node.field_4                           = 8;
            vec->vx                                       = 0;
            vec->vy                                       = work->field_5A0;
            vec->vz                                       = 0;
            RotMatrix(vec, &coord->field_0.coord);
            coord->field_0.coord.t[0] = 0;
            coord->field_0.coord.t[1] = 0;
            coord->field_0.coord.t[2] = 0;
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot((GpAnimCtx*)work, i, work->field_586);
            }
            if (work->field_594 == 0) {
                work->field_594 = 1;
                if (work->field_564 != NULL) {
                    work->field_564[0xD] = 1;
                }
                work->field_598       = 0xF0;
                work->obj4E4.flags   |= 0x8000;
                work->obj504.flags   |= 0x8000;
                pair                  = Gp_PackPair(&D_actor_510900_80167968, 5);
                work->obj4E4.field_18 = pair;
                work->obj504.field_18 = pair;
                work->field_57C       = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4078000D;
                SndEvt_EnqueueType6(work->field_57C, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            break;
        case 2:
            obj->field_C                                  = 0x80;
            ((TmdObject*)work->field_568->extra)->field_C = 0x80;
            ((TmdObject*)work->field_56C->extra)->field_C = 0x80;
            work->field_5A4                               = 0;
            if (work->field_564 != NULL) {
                work->field_564[0xD] = 4;
            }
            work->field_564     = NULL;
            work->obj47C.flags &= 0x7FFF;
            work->obj4E4.flags &= 0x7FFF;
            work->obj504.flags &= 0x7FFF;
            enemy->node.field_4 = 1;

            normals = Gp_GridParams->field_4;
            verts   = Gp_GridParams->field_8;
            faces   = Gp_GridParams->field_C;
            for (j = 0; j < 12; j++) {
                verts[j] = D_actor_510900_80167BDC[j];
            }
            for (j = 0; j < 3; j++) {
                normals[j] = D_actor_510900_80167BC4[j];
                faces[j]   = D_actor_510900_80167C3C[j];
            }

            do {
                vec = Gp_GridParams->field_4;
            } while (0);
            corners   = Gp_GridParams->field_8;
            vec[3].vx = 0;
            vec[3].vy = 0;
            vec[3].vz = 0;
            for (k = 0; k < 4; k++) {
                corners[12 + k].vx = 0;
                corners[12 + k].vy = 0;
                corners[12 + k].vz = 0;
            }
            break;
    }
    *(u32*)G_SCRATCH_HEAD += sizeof(SVECTOR);
    return 0;
}

/// Frame handler of the muzzle-flash child task, dispatched from
/// `func_actor_510900_8013BE98` once the spawn has run. The parent's animation
/// id (`field_586`) selects the behaviour: below 0x1C the model is hidden,
/// 0x1C/0x1D hold, 0x1E unhides it once the blend weight reaches 0xC8, and 0x1F
/// drives the coordinate. There the weight's position within its 40-frame cycle
/// picks between an identity frame parented to the body's coordinate 12 (with
/// frame 0xE re-deriving the stashed local matrix from the view) and the stashed
/// matrix parented to the view, lifted along y by `3*(n - 0xC)^2 - 0x1B0`.
/// Weight 0x52 restores the parented identity frame.
void func_actor_510900_801395AC(void* enemy, Task* task)
{
    TmdObject*              obj;
    Actor510900Work*        work;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          parentCoord;
    Actor510900MatrixWords* mat;
    Actor510900MatrixWords* mat2;
    s16                     blend;
    s16                     r;
    s32                     dy;

    work  = (Actor510900Work*)task->parent->idMap;
    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    if (work->field_586 < 0x1C) {
        obj->field_C = 0x80;
        return;
    }
    *(u8**)G_SCRATCH_HEAD -= 0x20;
    switch (work->field_586) {
        case 0x1C:
        case 0x1D:
            break;

        case 0x1E:
            if (work->field_58A == 0xC8) {
                obj->field_C = 0;
            }
            break;

        case 0x1F:
            parentCoord = &((TmdObject*)task->parent->extra)->field_8[12];
            blend       = work->field_58A;
            if (blend < 0x50) {
                r = blend % 40;
                if (r < 0xF) {
                    mat               = (Actor510900MatrixWords*)&coord->coord;
                    mat->m00_m01      = 0x1000;
                    mat->m11_m12      = 0x1000;
                    mat->m22          = 0x1000;
                    mat->m02_m10      = 0;
                    mat->m20_m21      = 0;
                    coord->coord.t[0] = 0;
                    coord->coord.t[1] = 0;
                    coord->coord.t[2] = 0;
                    coord->sub        = parentCoord;
                    if (r == 0xE) {
                        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &parentCoord->workm, &work->field_544);
                    }
                } else {
                    r                  = r - 0xF;
                    dy                 = ((r - 0xC) * (r - 0xC) * 3) - 0x1B0;
                    coord->coord       = work->field_544;
                    coord->sub         = &Gfx_ViewCoord;
                    coord->coord.t[1] += dy;
                }
                coord->flg = 0;
                Gp_UpdateCoord(coord);
            } else if (blend == 0x52) {
                mat2              = (Actor510900MatrixWords*)&coord->coord;
                mat2->m00_m01     = 0x1000;
                mat2->m02_m10     = 0;
                mat2->m11_m12     = 0x1000;
                mat2->m20_m21     = 0;
                mat2->m22         = 0x1000;
                coord->coord.t[0] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[2] = 0;
                coord->sub        = parentCoord;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
            }
            break;
    }
    *(u8**)G_SCRATCH_HEAD += 0x20;
}

/// Spawn state of the child effect task: allocates its `Actor510900ChildFx`
/// work block, places the child on the parent's fourth coordinate offset by a
/// fixed local vector and yawed -0x160, and links its two collision objects.
/// `field_CE` comes from `D_actor_510900_80167C94` indexed by the horizontal
/// distance to the player in units of 1000, clamped to the last entry.
void func_actor_510900_801397F0(GpEnemy* arg0, Task* arg1)
{
    Actor510900ChildFx*        work;
    Actor510900ChildFxScratch* scratch;
    TmdObject*                 tmd;
    GsCOORDINATE2*             coord;
    GsCOORDINATE2*             parentCoords;
    GsCOORDINATE2*             parentCoord;
    s32                        dx;
    s32                        dz;
    s32                        idx;

    tmd          = arg1->extra;
    coord        = tmd->field_8;
    parentCoords = ((TmdObject*)arg1->parent->extra)->field_8;
    parentCoord  = &parentCoords[3];
    work         = Mem_Calloc(sizeof(Actor510900ChildFx), false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap   = (TaskIdMap*)work;
    tmd->field_C  = 0;
    scratch       = (Actor510900ChildFxScratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(Actor510900ChildFxScratch));
    tmd->field_1C = &work->lightMtx;
    tmd->field_20 = &work->colorMtx;

    Gfx_ViewCoord.flg = 0;
    Gp_UpdateCoord(&Gfx_ViewCoord);
    parentCoord->flg = 0;
    Gp_UpdateCoord(parentCoord);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &parentCoord->workm, &coord->coord);

    scratch->rot.vx = -0xA5;
    scratch->rot.vy = -0x235;
    scratch->rot.vz = 0xA0;
    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->rot);
    gte_rtv0_real();
    gte_stlvnl(&scratch->pos);
    coord->sub         = &Gfx_ViewCoord;
    coord->coord.t[0] += scratch->pos.vx;
    coord->coord.t[1] += scratch->pos.vy;
    coord->coord.t[2] += scratch->pos.vz;

    scratch->rot.vx = -0x160;
    scratch->rot.vy = 0;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &scratch->mtx);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->mtx);
    gte_rtir_real();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->mtx.m[0][1]);
    gte_rtir_real();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->mtx.m[0][2]);
    gte_rtir_real();
    gte_stclmv(&coord->coord.m[0][2]);

    dx              = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    scratch->pos.vy = 0;
    scratch->pos.vx = dx;
    dz              = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    scratch->pos.vz = dz;
    idx             = SquareRoot0((dx * dx) + (dz * dz)) / 1000;
    if (idx >= 0xC) {
        idx = 0xB;
    }

    work->field_CE       = D_actor_510900_80167C94[idx];
    work->obj40.field_8  = coord;
    work->obj40.field_C  = &work->rec60;
    work->obj40.field_10 = 0;
    work->obj40.field_12 = 0;
    work->obj40.field_14 = 0;
    work->obj40.field_18 = 0;
    work->obj40.field_1C = 0xC8;
    work->obj40.flags    = 1;
    Gp_LinkObj(3, &work->obj40);
    Gp_InitRec18Table(&work->rec60, 1, 0);

    work->d4rec.field_0  = 0;
    work->d4rec.field_2  = 0;
    work->d4rec.field_4  = 0;
    work->d4rec.field_8  = 0;
    work->d4rec.field_A  = 0x1F4;
    work->d4rec.field_C  = 0;
    work->d4rec.field_10 = 1;
    work->d4rec.field_12 = 1;
    work->d4rec.field_14 = &work->recB0;
    work->obj78.field_C  = (GpRec18*)&work->d4rec;
    work->obj78.field_8  = coord;
    work->obj78.field_10 = 0;
    work->obj78.field_12 = 0;
    work->obj78.field_14 = 0;
    work->obj78.field_18 = 0;
    work->obj78.field_1C = 0;
    work->obj78.flags    = 3;
    work->obj40.flags   |= 0x8000;
    Gp_LinkObj(3, &work->obj78);
    Gp_InitRec18Table(&work->recB0, 1, 0);
    work->obj78.flags |= 0x4000;

    arg1->state            = 1;
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor510900ChildFxScratch);
}

/// Per-frame handler of the effect child while it is alive: spins the object by
/// `field_CE` about X, drags it 150 units down its own Y axis and drips a trail
/// effect every third frame. Once it has fallen past -0x514 and come back up,
/// or either `GpRec18` table reports a hit, it fires the impact effects,
/// reparents the task under the spawned one and hands the actor to state 2.
/// A parent that has stopped (`field_592` == 0) tears the object down the same
/// way. `D_801153F4` 1 only refreshes the colour and 2 only hides the model.
void func_actor_510900_80139C10(GpEnemy* enemy, Task* task)
{
    VECTOR                         pos;
    GpEffWork*                     eff;
    GsCOORDINATE2*                 coord;
    u8*                            head;
    Actor510900ChildFxTickScratch* scratch;
    TmdObject*                     tmd;
    Actor510900ChildFx*            work;
    Actor510900Work*               parent;
    s32                            angle;
    s32                            snd;
    s32                            done;
    u16                            tick;

    tmd    = task->extra;
    work   = (Actor510900ChildFx*)task->idMap;
    coord  = tmd->field_8;
    parent = (Actor510900Work*)task->parent->idMap;
    done   = 0;
    switch (D_801153F4) {
        case 0:
            tmd->field_C = 0;
            break;
        case 1:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            Gp_UpdateActorColor(task->spawnArg2, &pos, 0, 0);
            return;
        case 2:
            tmd->field_C = 0x80;
            return;
    }

    head                  = *(u8**)G_SCRATCH_HEAD;
    scratch               = (Actor510900ChildFxTickScratch*)(head - sizeof(Actor510900ChildFxTickScratch));
    *(u8**)G_SCRATCH_HEAD = (u8*)scratch;
    angle                 = -work->field_CE;
    scratch->rot.vx       = angle;
    scratch->rot.vy       = 0;
    scratch->rot.vz       = 0;
    RotMatrix(&scratch->rot, &scratch->mtx);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->mtx);
    gte_rtir_real();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->mtx.m[0][1]);
    gte_rtir_real();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->mtx.m[0][2]);
    gte_rtir_real();
    gte_stclmv(&coord->coord.m[0][2]);
    coord->flg         = 0;
    coord->coord.t[0] += -(coord->coord.m[0][1] * 0x96) >> 12;
    coord->coord.t[1] += -(coord->coord.m[1][1] * 0x96) >> 12;
    coord->coord.t[2] += -(coord->coord.m[2][1] * 0x96) >> 12;
    tick               = work->field_C8 + 1;
    work->field_C8     = tick;
    if (tick >= 3) {
        scratch->rot.vx = 0;
        scratch->rot.vy = 0x64;
        scratch->rot.vz = 0;
        Gp_SpawnEff(0x60070, coord, 0x01001600, &scratch->rot);
        work->field_C8 = 0;
    }
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(task->spawnArg2, &pos, 0, 0);
    if (coord->coord.t[1] < -0x514) {
        work->field_CA = 1;
    }
    if (work->field_CA != 0 && coord->coord.t[1] >= -0x513) {
        done = 1;
    }
    if (done != 0 || (work->rec60.field_4 & 0xFFFF0000) == 0x10000 || work->recB0.field_4 != 0) {
        Gp_SpawnEff(0x6005C, coord, 0x10002200, NULL);
        Gp_SpawnEff(0x60070, coord, 0xC1001200, NULL);
        eff = Gp_SpawnEff(0x80060185, coord, 0, NULL);
        if (eff != NULL) {
            Task_Reparent(task, eff->field_0);
        }
        if (work->rec60.field_4 != 0) {
            work->field_CA = 2;
        } else {
            work->field_CA = 0;
        }
        work->obj40.flags &= 0x7FFF;
        Gp_ClearRec18Occupied(&work->rec60);
        Gp_UnlinkObj(&work->obj78);
        work->field_C8                     = 0;
        ((TmdObject*)task->extra)->field_C = 0x80;
        snd                                = (((u16)((GpEnemy*)task->spawnArg2)->field_8 >> 0xC) << 8) | 0x51100009;
        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
        task->state = 2;
    }
    Gp_ClearRec18Occupied(&work->rec60);
    if (parent->field_592 == 0) {
        work->obj40.flags &= 0x7FFF;
        Gp_UnlinkObj(&work->obj78);
        work->field_C8                     = 0;
        ((TmdObject*)task->extra)->field_C = 0x80;
        task->state                        = 2;
        work->field_CA                     = 3;
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor510900ChildFxTickScratch);
}

void func_actor_510900_8013A310(Task* task);

/// Frame handler of the effect child task: state 0 fades the object in over
/// 0x10 frames, state 1 holds it until its `GpRec18` reports a hit or 0x1F
/// frames pass, state 2 runs the hit handler, and state 3 unlinks the object
/// and destroys the enemy.
void func_actor_510900_8013A100(GpEnemy* enemy, Task* task)
{
    Actor510900ChildFx* work;
    Actor510900Work*    parent;
    u16                 tick;

    work   = (Actor510900ChildFx*)task->idMap;
    parent = (Actor510900Work*)task->parent->idMap;
    if (D_801153F4 == 0) {
        switch (work->field_CA) {
            case 0:
                tick           = work->field_C8 + 1;
                work->field_C8 = tick;
                if (tick >= 0x10) {
                    work->obj40.field_1C = 0x258;
                    work->field_CA       = 1;
                    work->field_C8       = 0;
                    work->obj40.flags   |= 0x8000;
                    return;
                }
                if (parent->field_592 == 0) {
                    work->obj40.flags &= 0x7FFF;
                    Gp_UnlinkObj(&work->obj78);
                    work->field_C8                            = 0;
                    ((Actor510900Obj2C*)task->extra)->field_C = 0x80;
                    task->state                               = 2;
                    work->field_CA                            = 3;
                    return;
                }
                break;
            case 1:
                if ((work->rec60.field_4 & 0xFFFF0000) == 0x10000) {
                    work->field_CA     = 2;
                    work->field_CC     = 0;
                    work->obj40.flags &= 0x7FFF;
                } else {
                    tick           = work->field_C8 + 1;
                    work->field_C8 = tick;
                    if (tick >= 0x1F) {
                        work->field_CA = 3;
                        work->field_C8 = 0;
                    } else if (parent->field_592 == 0) {
                        work->obj40.flags &= 0x7FFF;
                        Gp_UnlinkObj(&work->obj78);
                        work->field_C8                            = 0;
                        ((Actor510900Obj2C*)task->extra)->field_C = 0x80;
                        task->state                               = 2;
                        work->field_CA                            = 3;
                    }
                }
                Gp_ClearRec18Occupied(&work->rec60);
                return;
            case 2:
                func_actor_510900_8013A310(task);
                return;
            case 3:
                tick           = work->field_C8 + 1;
                work->field_C8 = tick;
                if (tick >= 0x1F) {
                    parent->field_5BA = 0;
                    Gp_UnlinkObj(&work->obj40);
                    Gp_DestroyEnemy(enemy, task);
                }
                break;
        }
    }
}

/// Runs the player-hold sequence the effect's state 2 drives, on a 0x2C-byte
/// scratch block: `field_CC` 0 asks the player for the hold (message 0x3F8) and
/// on success starts the grab animation and its sound, 1 holds until the parent
/// reports the hit or 0x3C frames pass and then switches to the second
/// animation, and 2 waits 0x14 frames before releasing the player. Any refused
/// message leaves the effect in state 3 so the caller tears it down.
void func_actor_510900_8013A310(Task* task)
{
    Actor510900ChildFx*    work;
    Actor510900Work*       parent;
    Task*                  player;
    void*                  head;
    Actor510900HitScratch* scratch;
    GpObj38*               obj;
    u16                    tick;
    s32                    snd;
    s32                    pan;

    work                    = (Actor510900ChildFx*)task->idMap;
    parent                  = (Actor510900Work*)task->parent->idMap;
    player                  = Game_GetPtrSlot(3);
    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = (u8*)head - sizeof(Actor510900HitScratch);
    scratch                 = (Actor510900HitScratch*)*(void**)G_SCRATCH_HEAD;

    switch (work->field_CC) {
        case 0:
            if (((GpActorWork*)player)->actor->field_954 != 2) {
                scratch->query.field_14 = 0xC;
                if (Gp_DispatchMsg(player, 0x3F8, (s32)scratch, 0) != 0) {
                    work->field_CA = 3;
                    break;
                }
                Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_actor_510900_80167968, 4), 0);
                scratch->anim.field_0  = D_actor_510900_80167B2C;
                scratch->anim.field_4  = 1;
                scratch->anim.field_8  = 0;
                scratch->anim.field_C  = 0;
                scratch->anim.field_10 = 1;
                Gp_DispatchMsg(player, 0x3FF, (s32)&scratch->anim, 0);
                work->field_CC = 1;
                work->field_CE = 0;
                obj            = (GpObj38*)((TmdObject*)player->extra)->field_8;
                snd            = (((u16)((GpEnemy*)task->spawnArg2)->field_8 >> 0xC) << 8) | 0x5110000A;
                pan            = (s8)Gp_GetObjPan(obj);
                SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(obj));
            }
            break;
        case 1:
            tick           = work->field_CE + 1;
            work->field_CE = tick;
            if ((s16)tick < 0x3D && parent->field_5BC != 1 && parent->field_592 != 0) {
                break;
            }
            parent->field_5BC      = 0;
            scratch->anim.field_0  = D_actor_510900_80167B2C;
            scratch->anim.field_4  = 2;
            scratch->anim.field_8  = 0;
            scratch->anim.field_C  = 0;
            scratch->anim.field_10 = 1;
            Gp_DispatchMsg(player, 0x3FF, (s32)&scratch->anim, 0);
            work->field_CC = 2;
            work->field_CE = 0;
            break;
        case 2:
            tick           = work->field_CE + 1;
            work->field_CE = tick;
            if ((s16)tick >= 0x15) {
                if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                    Gp_DispatchMsg(player, 0x3F1, 0, 0);
                    work->field_CA = 3;
                }
            }
            break;
    }
    *(u32*)G_SCRATCH_HEAD += sizeof(Actor510900HitScratch);
}

/// Spawn handler of the child task: allocates the animation work block, seeds
/// the model's root coordinate from the spawn-index tables, resets animation
/// slots 1..10 and links the two render objects.
void func_actor_510900_8013A5B8(GpEnemy* enemy, Task* task)
{
    TmdObject*            tmd;
    GsCOORDINATE2*        coords;
    Actor510900ChildAnim* work;
    GsCOORDINATE2*        coord;
    SVECTOR*              rot;
    void*                 head;
    s32                   i;

    tmd    = task->extra;
    coords = tmd->field_8;
    work   = Mem_Calloc(sizeof(Actor510900ChildAnim), 0);
    coord  = &coords[10];
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->idMap         = (TaskIdMap*)work;
    tmd->field_C        = 0x80;
    coords->flg         = 0;
    tmd->field_1C       = &work->lightMtx;
    tmd->field_20       = &work->colorMtx;
    head                = *(void**)0x1F8003FC;
    enemy->field_4      = &coords->coord;
    rot                 = (SVECTOR*)(head - 8);
    *(void**)0x1F8003FC = head - 8;
    enemy->field_48     = 0;
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4        = 1;
    enemy->field_18            = coord;
    enemy->field_1C.vx         = -0xC8;
    enemy->field_1C.vy         = 0;
    enemy->field_1C.vz         = 0;
    work->field_334            = task->spawnArg1;
    ((SVECTOR*)(head - 8))->vx = 0;
    rot->vy                    = D_actor_510900_80167CD0[work->field_334];
    rot->vz                    = 0;
    RotMatrix(rot, &coords->coord);
    i                  = 1;
    coords->coord.t[0] = D_actor_510900_80167CB8[work->field_334].vx;
    coords->coord.t[1] = D_actor_510900_80167CB8[work->field_334].vy;
    coords->coord.t[2] = D_actor_510900_80167CB8[work->field_334].vz;
    coords->sub        = &Gfx_ViewCoord;
    func_800B3F84(&work->anim, D_actor_510900_80167CAC, (GpAnimObj*)tmd, work->poses, work->slots);
    do {
        Gp_AnimResetSlot(&work->anim, i, 1);
        i++;
    } while (i < 0xB);
    work->obj2BC.field_10 = -0xC8;
    work->obj2BC.field_8  = coord;
    work->obj2BC.field_12 = 0;
    work->obj2BC.field_14 = 0;
    work->obj2BC.field_C  = &work->rec2DC;
    work->obj2BC.field_18 = 0;
    work->obj2BC.field_1C = 0xC8;
    work->obj2BC.flags    = 1;
    Gp_LinkObj(2, &work->obj2BC);
    Gp_InitRec18Table(&work->rec2DC, 1, 0);
    work->obj2F4.field_18 = 0x50002;
    work->obj2F4.field_8  = coord;
    work->obj2F4.field_10 = 0;
    work->obj2F4.field_12 = 0;
    work->obj2F4.field_14 = 0;
    work->obj2F4.field_C  = &work->rec314;
    work->obj2F4.field_1C = 0x15E;
    work->obj2F4.flags    = 1;
    work->obj2BC.flags   &= 0x7FFF;
    Gp_LinkObj(8, &work->obj2F4);
    Gp_InitRec18Table(&work->rec314, 1, 0);
    work->obj2F4.flags &= 0x7FFF;
    task->exitCallback  = (TaskFunc)func_actor_510900_8013C380;
    task->state         = 1;
    *(u32*)0x1F8003FC  += 8;
}

void func_actor_510900_8013A9BC(Task* task);
s32  func_actor_510900_8013C240(Task* task);
void func_actor_510900_8013C338(Actor510900* arg0, Actor510900Coord* arg1);

/// Frame handler (state 1) of the child task. Mode 1 of `D_801153F4` only
/// redraws, mode 2 hides the model and flags the context, and mode 0 ticks the
/// animation until `func_actor_510900_8013C240` reports ready before falling
/// into the normal body.
void func_actor_510900_8013A85C(Actor510900Ctx* arg0, Task* arg1)
{
    Actor510900Obj2C*     obj;
    Actor510900ChildAnim* work;
    Actor510900Coord*     coord;
    Actor510900Work*      parent;
    s32                   mode;
    s32                   i;
    s32                   one;

    obj    = (Actor510900Obj2C*)arg1->extra;
    work   = (Actor510900ChildAnim*)arg1->idMap;
    coord  = obj->field_8;
    parent = (Actor510900Work*)arg1->parent->idMap;
    mode   = D_801153F4;
    one    = 1;
    if (mode == one) {
        goto case1;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto case0;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto case2;
    }
    goto body;
case0:
    if (func_actor_510900_8013C240(arg1) == 0) {
        i = 1;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 0xB);
        return;
    }
    ((Actor510900Obj2C*)arg1->extra)->field_C = 0;
    arg0->node.field_4                        = one;
    goto body;
case2:
    obj->field_C       = 0x80;
    arg0->node.field_4 = one;
    return;
body:
    func_actor_510900_8013A9BC(arg1);
    if (work->field_334 < 2) {
        GameFlag_SetNibble(work->field_334 + 0xB, work->field_336);
    } else {
        parent->field_5C2 = work->field_336;
    }
    i = 1;
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 0xB);
    coord->field_0.flg = 0;
    Gp_UpdateCoord(&coord->field_0);
case1:
    func_actor_510900_8013C338((Actor510900*)arg1, coord);
}

/// Grab state machine of the child task, run from the frame handler above.
/// State 0 waits for the grab: the player has to be inside the `rec2DC` node
/// (and survive `Gp_ComputeDamage`) or the parent has to request this child by
/// number through `field_5BE`; on a hit it resets the animation slots, spawns
/// the grab effect and its sound and starts the `field_332` countdown. State 1
/// runs that countdown, keeping the four trailing part coordinates updated, and
/// hands the held effect task its exit state once the timer runs out or the
/// camera cuts away. State 2 only releases the held task. `field_334` 2 mirrors
/// the state back to the parent's `field_5C4`.
void func_actor_510900_8013A9BC(Task* task)
{
    Actor510900ChildAnim*   work;
    Actor510900Work*        parent;
    Actor510900Ctx*         ctx;
    Actor510900GrabScratch* scratch;
    void*                   head;
    GsCOORDINATE2*          coord;
    GpEffWork*              eff;
    Task*                   spawned;
    s16                     next;
    s32                     grabbed;
    s32                     i;
    s32                     one;
    s32                     snd;
    s32                     pan;
    s32                     dmg;
    s16                     state;

    grabbed             = 0;
    head                = *(void**)0x1F8003FC;
    coord               = &((TmdObject*)task->extra)->field_8[10];
    *(void**)0x1F8003FC = (u8*)head - 0x18;
    scratch             = (Actor510900GrabScratch*)((u8*)head - 0x18);
    work                = (Actor510900ChildAnim*)task->idMap;
    ctx                 = (Actor510900Ctx*)task->spawnArg2;
    state               = work->field_330;
    parent              = (Actor510900Work*)task->parent->idMap;
    one                 = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto end;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto end;
case0:
    if (parent->field_592 == 0) {
        work->field_336 = 3;
        work->field_330 = 2;
        goto end;
    }
    ctx->node.field_4   = Gp_StateF0.field_0 != 1;
    dmg                 = work->rec2DC.field_4;
    work->obj2BC.flags |= 0x8000;
    if ((dmg & 0xFFFF8000) == 0x20000 && ctx->node.field_5 == one &&
        Gp_ComputeDamage(dmg, 0x3E8, 0, 0) != 0) {
        grabbed = 1;
    }
    if (parent->field_5BE == work->field_334 + 1 && parent->field_5C0 == 1) {
        grabbed           = 1;
        parent->field_5BE = -1;
    }
    if (grabbed == 1) {
        work->field_336 = grabbed;
        work->field_330 = grabbed;
        i               = 1;
        do {
            func_800B4114(&work->anim, i, 2, 0, 0);
            i++;
        } while (i < 0xB);
        scratch->rot.vx = 0;
        scratch->rot.vy = 0x80;
        scratch->rot.vz = 0;
        eff             = Gp_SpawnEff(0x8006005B, coord, 0, &scratch->rot);
        if (eff != NULL) {
            spawned         = eff->field_0;
            work->field_32C = spawned;
            Task_Reparent(task, spawned);
        }
        Gp_SpawnEff(0x6005C, coord, 0x200, &scratch->rot);
        work->field_332     = 0x78;
        work->obj2BC.flags &= 0x7FFF;
        work->obj2F4.flags |= 0x8000;
        snd                 = (((u16)ctx->field_8 >> 0xC) << 8) | 0x51100004;
        pan                 = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    Gp_ClearRec18Occupied(&work->rec2DC);
    goto end;
case1:
    if (Gp_FindRec18(&work->rec314, 0) != 0) {
        work->obj2F4.flags &= 0x7FFF;
    }
    Gp_ClearRec18Occupied(&work->rec314);
    func_80180A64(&((TmdObject*)task->extra)->field_8[9]);
    func_80180A64(&((TmdObject*)task->extra)->field_8[8]);
    func_80180A64(&((TmdObject*)task->extra)->field_8[7]);
    func_80180A64(&((TmdObject*)task->extra)->field_8[6]);
    work->field_332--;
    next = 2;
    if (work->field_332 <= 0) {
        Task* held;

        work->field_336     = next;
        work->obj2F4.flags &= 0x7FFF;
        held                = work->field_32C;
        if (held != NULL) {
            held->state = state;
        }
    } else {
        Task* held;

        if (parent->field_592 != 0) {
            goto end;
        }
        work->field_336     = next;
        work->obj2F4.flags &= 0x7FFF;
        held                = work->field_32C;
        if (held != NULL) {
            held->state     = 2;
            work->field_32C = NULL;
        }
    }
    work->field_330 = next;
    goto end;
case2:
    if (parent->field_592 == 0) {
        Task* held;

        held = work->field_32C;
        if (held != NULL) {
            held->state     = state;
            work->field_32C = NULL;
        }
    }
end:
    if (work->field_334 == 2) {
        parent->field_5C4 = work->field_330;
    }
    *(u32*)0x1F8003FC += 0x18;
}

void func_actor_510900_8013AD90(GpEnemy* enemy, Task* task)
{
    GsCOORDINATE2*          coord;
    Actor510900MatrixWords* mat;
    Actor510900ChildWork*   work;

    coord = ((Actor510900Obj2C*)task->extra)->field_8;
    work  = Mem_Calloc(sizeof(Actor510900ChildWork), false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    mat               = (Actor510900MatrixWords*)&coord->coord;
    task->idMap       = (TaskIdMap*)work;
    mat->m00_m01      = 0x1000;
    mat->m11_m12      = 0x1000;
    mat->m22          = 0x1000;
    mat->m02_m10      = 0;
    mat->m20_m21      = 0;
    coord->coord.t[0] = -0x17D4;
    coord->coord.t[1] = -0x456;
    coord->coord.t[2] = 0x17C;
    coord->sub        = &Gfx_ViewCoord;
    coord->flg        = 0;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    Gp_LinkNode(&enemy->node);
    enemy->field_18     = coord;
    enemy->node.field_4 = 1;
    enemy->field_1C.vx  = 0;
    enemy->field_1C.vy  = 0;
    enemy->field_1C.vz  = 0;
    work->obj0.field_8  = coord;
    work->obj0.field_10 = 0;
    work->obj0.field_12 = 0;
    work->obj0.field_14 = 0;
    work->obj0.field_C  = &work->rec20;
    work->obj0.field_18 = 0;
    work->obj0.field_1C = 0x12C;
    work->obj0.flags    = 1;
    Gp_LinkObj(2, &work->obj0);
    Gp_InitRec18Table(&work->rec20, 1, 0);
    work->obj38.field_12 = -0x200;
    work->obj38.field_8  = coord;
    work->obj38.field_10 = 0;
    work->obj38.field_14 = 0;
    work->obj38.field_C  = &work->rec58;
    work->obj38.field_18 = 0;
    work->obj38.field_1C = 0x200;
    work->obj38.flags    = 1;
    work->obj0.flags    &= 0x7FFF;
    Gp_LinkObj(8, &work->obj38);
    Gp_InitRec18Table(&work->rec58, 1, 0);
    work->obj38.flags &= 0x7FFF;
    task->exitCallback = (TaskFunc)func_actor_510900_8013C430;
    task->state        = 1;
}

s32  Gp_GetViewIndex(void);
void func_actor_510900_8013B0D8(Task* arg0);

/// View index the child keeps running in; any other view parks it.
extern u16 D_actor_510900_80167CE4;
/// Game-flag nibble 0xD values, indexed by the child's `field_74` and the
/// parent work's `field_5C2`.
extern u16 D_actor_510900_80167CEC[][4];

void func_actor_510900_8013AF38(Actor510900Ctx* arg0, Task* arg1)
{
    Actor510900ChildWork* work;
    Actor510900Work*      parent;
    s32                   mode;
    s32                   one;
    u32                   random;
    Task*                 child;

    work   = (Actor510900ChildWork*)arg1->idMap;
    parent = (Actor510900Work*)arg1->parent->idMap;
    mode   = D_801153F4;
    one    = 1;
    if (mode == one) {
        return;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto case0;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto case2;
    }
    goto body;
case0:
    if ((Gp_GetViewIndex() & 0xFF) != D_actor_510900_80167CE4) {
        arg0->node.field_4 = one;
        work->obj0.flags  &= 0x7FFF;
        work->obj38.flags &= 0x7FFF;
        if (work->field_76 != 0) {
            work->field_76--;
        }
        child = work->field_70;
        if (child != NULL) {
            child->state   = 3;
            work->field_70 = NULL;
        }
        return;
    }
    arg0->node.field_4 = one;
    goto body;
case2:
    arg0->node.field_4 = one;
    return;
body:
    func_actor_510900_8013B0D8(arg1);
    GameFlag_SetNibble(0xD, D_actor_510900_80167CEC[work->field_74][parent->field_5C2]);
    random      = Gp_LcgState * 5 + 0x71357911;
    Gp_LcgState = random;
    if ((u16)((random >> 16) % 3) == 0) {
        Gp_SpawnEff(0x6005A, ((Actor510900Obj2C*)arg1->extra)->field_8, 0, NULL);
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_510900/actor_510900_4", ActorsShared80135df4Table);
