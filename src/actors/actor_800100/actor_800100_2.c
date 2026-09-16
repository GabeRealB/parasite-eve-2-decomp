#include "common.h"
#include "actors/actor_800100.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/mem.h"

s32  func_8010BC70(GsCOORDINATE2* arg0);
s32  func_8010BCF4(Task* arg0, VECTOR3* arg1);
void func_8010BE5C(GpActorWork* arg0, VECTOR3* arg1);
s32  func_80105ED4(GpActorWork* arg0);
void Gp_PlayObjSfx(GpObj38* arg0, s32 arg1, s32 arg2);
s32  rand();

extern GpActorFuncTable12 D_actor_800100_80161E58;
extern s32                D_80115738;
extern s32                D_8011574C;
extern s16                D_80072830;
extern s8                 D_8007272F;
extern s16                D_actor_800100_80167218[];
extern u8                 D_actor_800100_80167230[];
extern u8*                D_actor_800100_801672F8[];
extern u8                 D_actor_800100_80167308[];
extern u8                 D_actor_800100_80167310[];

void func_actor_800100_80163D54(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    s32            flag;
    s32            dist;
    s32            val;
    s16            count;

    actor  = arg0->actor;
    coord  = arg0->extra->field_8;
    target = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    flag   = (*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x042A0000;
    if (arg0->actor->field_910->field_C4 <= 0) {
        func_8010BF7C(arg0, 0xA, 0x1F);
        dist = func_8010BC70(coord);
        if ((dist >= 0x600 && (rand() & 0xFF) >= 0xF1) || (dist >= 0x400 && flag != 0)) {
            func_actor_800100_801656C8(arg0);
        } else {
            count = (u16)actor->field_942 + 1;
            do {
                actor->field_942 = count;
            } while (0);
            if (count >= ((rand() & 3) + 3)) {
                if (actor->field_95E == 0) {
                    actor->field_95E = 1;
                    Gp_AnimPlayChildSlotsEx(arg0, 0x17, 0, 5);
                } else if ((rand() & 0xFF) >= 0xD0) {
                    func_actor_800100_80165720(arg0);
                }
            } else {
                val = func_8010BCF4((Task*)arg0, (VECTOR3*)target->coord.t);
                if (val < 0) {
                    val = -val;
                }
                if (val >= 0x200) {
                    actor->field_90C = NULL;
                    func_actor_800100_801656F4(arg0);
                }
            }
        }
    }
    func_8010BE5C(arg0, (VECTOR3*)target->coord.t);
}

INCLUDE_RODATA("actors/nonmatchings/actor_800100/actor_800100_2", D_actor_800100_80161E4C);

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "nonmatching D_actor_800100_80161E58\n"
        "dlabel D_actor_800100_80161E58\n"
        "    .word func_actor_800100_80165748\n"
        "    .word func_actor_800100_80164184\n"
        "    .word func_actor_800100_801643F4\n"
        "    .word func_actor_800100_80164580\n"
        "    .word func_actor_800100_801657D8\n"
        "    .word func_actor_800100_80164710\n"
        "    .word func_actor_800100_80164940\n"
        "    .word func_actor_800100_80164B9C\n"
        "    .word func_actor_800100_80165818\n"
        "    .word func_actor_800100_80164E60\n"
        "    .word func_actor_800100_80165010\n"
        "    .word func_actor_800100_801652B0\n"
        "enddlabel D_actor_800100_80161E58\n"
        ".section .text");
#endif

/// Drives the actor's `field_954`/`field_956` callback tables while the
/// `field_944` countdown runs, spawning the drip effect every tenth frame.
/// `sp40` / `sp48` hold the effect position: it rides the water surface
/// (`Game_Session.field_122`) minus the actor coordinate's world Y.
void func_actor_800100_80163F04(GpActorWork* arg0)
{
    GpActorFuncTable12 sp;
    SVECTOR            sp40;
    SVECTOR            sp48;
    GameActor*         actor;
    GpActorD4*         d4;
    GsCOORDINATE2*     coord;
    s16                temp;
    s16                rem;
    s32                pan;

    sp    = D_actor_800100_80161E58;
    actor = arg0->actor;
    coord = arg0->extra->field_8;
    d4    = actor->field_910;
    if (d4->field_C4 > 0) {
        d4->field_C4--;
    }
    sp.funcs[actor->field_956](arg0);
    if ((u32)(func_80105ED4(arg0) + 0xEFFFFF77) < 4) {
        actor->field_944 = 0x78;
        sp40.vx          = 0;
        sp40.vy          = (u16)Game_Session->field_122 - (u16)coord->coord.t[1];
        sp40.vz          = 0;
        Gp_SpawnEff(D_80115738, coord, 0x1202180, &sp40);
        Gp_SpawnEff(D_8011574C, coord, (rand() & 0x1F) | 0x40, &sp40);
    }
    temp = (u16)actor->field_944;
    if (temp != 0) {
        actor->field_944--;
        rem = temp % 10;
        if (rem == 0) {
            sp48.vx = 0;
            sp48.vy = (u16)Game_Session->field_122 - (u16)coord->coord.t[1];
            sp48.vz = 0;
            Gp_SpawnEff(D_8011574C, coord, (rand() & 0x1F) | 0x40, &sp48);
        }
    }
    if ((s8)actor->field_97A == 0) {
        func_80109BB4(arg0, actor->field_17C);
        if ((u16)actor->field_96C != 0) {
            func_8010B9A4(arg0);
            pan = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(((D_8007272F - 1) << 16) + 0x4065000A, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
    }
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
    if (D_80072830 <= 0) {
        Gp_StopPlayerAnim(arg0, 0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100_2", func_actor_800100_80164184);

/// Lock-on drive for the actor's `field_95E` state machine. Builds a `VECTOR3`
/// at `G_SCRATCH_HEAD - 0x10` from the lock node (`Gp_GetLockPos`, or the
/// linked object's coord when `actor->field_90C` is set but flagged), plays the
/// 5/6 child-slot animation on entry, mirrors `field_93E` into `field_975` and
/// resets the actor's move once the aim is close enough.
void func_actor_800100_801643F4(GpActorWork* arg0)
{
    void**         scratch;
    u8*            head;
    VECTOR3*       pos;
    GameActor*     actor;
    GpLinkNode*    node;
    TmdObject*     extra;
    GsCOORDINATE2* src;
    s32            val;
    s32            arg;
    s32            flag;

    actor    = arg0->actor;
    extra    = (TmdObject*)((Task*)Game_GetPtrSlot(3))->extra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = (u8*)head - 0x10;
    pos      = (VECTOR3*)((u8*)head - 0x10);
    node     = actor->field_90C;
    src      = extra->field_8;
    if (node != NULL) {
        if (!(node->field_4 & 1)) {
            Gp_GetLockPos((GpLockPos*)node, pos);
        } else {
            actor->field_95E = 2;
        }
    } else {
        pos->vx = src->coord.t[0];
        pos->vy = src->coord.t[1];
        pos->vz = src->coord.t[2];
    }
    switch (actor->field_95E) {
        case 0:
            flag             = 1;
            actor->field_95E = flag;
            if (func_8010BCF4((Task*)arg0, pos) < 0) {
                actor->field_93E = -1;
                arg              = 5;
            } else {
                actor->field_93E = 1;
                arg              = 6;
            }
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
            actor->field_975 = (u8)actor->field_93E;
            val              = func_8010BCF4((Task*)arg0, pos);
            if (val < 0) {
                val = -val;
            }
            if ((val < 0x81) || (actor->field_95E == 2)) {
                Gp_ResetActorMove(arg0, 0);
            }
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)src->coord.t);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

/// Second arm of the lock-on drive: builds the lock position at
/// `G_SCRATCH_HEAD - 0x10` (`Gp_GetLockPos`, or `Gp_FindLockNodePad` when
/// `field_90C` is flagged) and measures the distance to it with
/// `func_8010BCF4`. Close enough latches `field_95E` to 1 and plays the slot-7
/// child animation; otherwise the target is handed to `Gp_TrackAllyLockTarget`
/// with 1. `field_95E` 2/3 waits for the chain to reach 3, which resets the
/// move fields and plays the slots 9/6 pair.
///
/// `track:` sits between the state store and `case 1` so the hand-off is
/// emitted after the store; the store's fall into case 1 is therefore a jump.
void func_actor_800100_80164580(GpActorWork* arg0)
{
    void**     scratch;
    u8*        head;
    VECTOR3*   pos;
    GameActor* actor;
    s32        flag;
    s32        arg;
    s32        val;

    actor    = arg0->actor;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = (u8*)head - 0x10;
    pos      = (VECTOR3*)((u8*)head - 0x10);
    flag     = 1;

    switch (actor->field_95E) {
        case 0:
            if (actor->field_90C != NULL) {
                if (actor->field_90C->field_4 & 1) {
                    actor->field_90C = Gp_FindLockNodePad(arg0);
                }
                Gp_GetLockPos((GpLockPos*)actor->field_90C, pos);
                val = func_8010BCF4((Task*)arg0, pos);
                if (val < 0) {
                    val = -val;
                }
                if (val >= 0x201) {
                    goto track;
                }
            }
            actor->field_95E = flag;
            goto caseOne;
        track:
            Gp_TrackAllyLockTarget(arg0, 1);
            break;
        caseOne:
        case 1:
            arg               = 7;
            actor->field_95C  = arg;
            actor->field_95E += 1;
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 3);
            /* fallthrough */
        case 2:
        case 3:
            Gp_TrackAllyLockTarget(arg0, 3);
            if (actor->field_95E == 3) {
                GameActor* actor2 = arg0->actor;
                actor2->field_954 = 0;
                actor2->field_956 = 4;
                actor2->field_95C = 0;
                actor2->field_95E = 0;
                actor2->field_973 = 0;
                actor2->field_975 = 0;
                Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void func_actor_800100_80164710(GpActorWork* arg0)
{
    GameActor*              actor;
    GameActor*              actor2;
    GameActor*              actor3;
    GpActorD4*              d4;
    GpLinkNode*             node;
    GpLockPos*              lock;
    GsCOORDINATE2*          coord;
    Actor800100LockScratch* scratch;
    VECTOR3*                head;
    s32*                    scratchHead;
    s32                     dist;
    u16                     state;

    head                    = *(VECTOR3**)G_SCRATCH_HEAD;
    actor                   = arg0->actor;
    scratch                 = (Actor800100LockScratch*)((u8*)head - 0x20);
    *(void**)G_SCRATCH_HEAD = scratch;
    d4                      = actor->field_910;
    Gp_TrackAllyLockTarget(arg0, 3);
    state = actor->field_95E;
    if (state != 0) {
        if (state != 1) {
            scratchHead = (s32*)G_SCRATCH_HEAD;
        } else {
            goto block_10;
        }
    } else {
        lock = (GpLockPos*)actor->field_90C;
        if ((lock == NULL) || (coord = arg0->extra->field_8, Gp_GetLockPos(lock, &scratch->lock), func_80103C74(coord, &scratch->lock, (VECTOR3*)((u8*)head - 0x10)), ((func_80103D8C(scratch->rot.vx, scratch->rot.vz) < 0x301) != 0))) {
            actor2            = arg0->actor;
            actor2->field_954 = 0;
            actor2->field_956 = 4;
            actor2->field_95C = 0;
            actor2->field_95E = 0;
            actor2->field_973 = 0;
            actor2->field_975 = 0;
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
        } else {
            dist = func_8010BCF4((Task*)arg0, &scratch->lock);
            if (dist < 0) {
                dist = -dist;
            }
            if (dist < 0x181) {
                actor->field_95E += 1;
            block_10:
                if (((s8)d4->field_CC <= 0) || (node = actor->field_90C, node == NULL) || (node->field_4 & 1)) {
                    *(volatile GpLockPos**)&actor->field_90C = NULL;
                    actor->field_97E                         = 1;
                    actor->field_12A                        &= 0x3FFF;
                    if ((u8)D_8007272F == 4) {
                        func_80106350(arg0, D_actor_800100_80167218[D_8007272F], 0);
                    }
                    actor3            = arg0->actor;
                    actor3->field_954 = 0;
                    actor3->field_956 = 4;
                    actor3->field_95C = 0;
                    actor3->field_95E = 0;
                    actor3->field_973 = 0;
                    actor3->field_975 = 0;
                    Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
                } else if (actor->field_940 == 0) {
                    func_actor_800100_80166EE8(arg0);
                }
            }
        }
        scratchHead = (s32*)G_SCRATCH_HEAD;
    }
    *scratchHead += 0x20;
}

/// Third arm of the lock-on drive, running the actor's `field_95E` state
/// machine over a `VECTOR3` carved from `G_SCRATCH_HEAD`. Case 0 latches the
/// state and `field_95A`, aims at the lock node and plays the 5/6 child-slot
/// animation with a random `field_934` hold; it falls into case 1, which
/// mirrors `field_93E` into `field_975` and, once the aim distance reaches
/// `field_934`, advances the state and plays slot 4. Case 2 counts
/// `field_934` down and, while it runs, asks `func_actor_800100_8016709C` for
/// the angle to the ally block at `field_910->field_A0`: under 0x281 the actor
/// hands over to the `0xA` / slot-1 chain (`field_97E` set, `field_956` kept
/// in `field_960`), otherwise `field_975` is cleared; a spent counter resets
/// the state to slot 9.
void func_actor_800100_80164940(GpActorWork* arg0)
{
    void**         scratch;
    u8*            head;
    VECTOR3*       pos;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GpActorD4*     d4;
    u16            old;
    s16            angle;
    s32            flag;
    s32            arg;
    s32            val;
    s32            count;
    s32            dist;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = (u8*)head - 0x10;
    pos      = (VECTOR3*)((u8*)head - 0x10);
    actor    = arg0->actor;
    coord    = arg0->extra->field_8;
    flag     = 1;
    switch (actor->field_95E) {
        case 0:
            actor->field_95E = flag;
            actor->field_95A = flag;
            Gp_GetLockPos((GpLockPos*)actor->field_90C, pos);
            if (func_8010BCF4((Task*)arg0, pos) < 0) {
                actor->field_93E = flag;
                arg              = 6;
            } else {
                actor->field_93E = -1;
                arg              = 5;
            }
            actor->field_934 = (rand() & 0x1FF) + 0x400;
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
            actor->field_975 = (u8)actor->field_93E;
            Gp_GetLockPos((GpLockPos*)actor->field_90C, pos);
            val  = func_8010BCF4((Task*)arg0, pos);
            dist = actor->field_934;
            if (val < 0) {
                val = -val;
            }
            if (val >= dist) {
                actor->field_958  = 3;
                actor->field_95E += 1;
                actor->field_934  = (rand() & 0x1F) + 0x14;
                Gp_AnimPlayChildSlotsEx(arg0, 4, 0, 5);
            }
            break;
        case 2:
            actor->field_973 = flag;
            count            = actor->field_934 - 1;
            actor->field_934 = count;
            if (count <= 0) {
                GameActor* actor2 = arg0->actor;
                actor2->field_954 = 0;
                actor2->field_956 = 4;
                actor2->field_95C = 0;
                actor2->field_95E = 0;
                actor2->field_973 = 0;
                actor2->field_975 = 0;
                Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
            } else {
                angle = func_actor_800100_8016709C(coord, &actor->field_910->field_A0, NULL);
                if (angle != 0) {
                    if (angle < 0x281) {
                        s16        anim   = 1;
                        GameActor* actor3 = arg0->actor;
                        old               = actor3->field_956;
                        actor3->field_956 = 0xA;
                        actor3->field_97E = anim;
                        d4                = actor3->field_910;
                        actor3->field_954 = 0;
                        actor3->field_95A = flag;
                        actor3->field_95C = 0;
                        actor3->field_95E = 0;
                        actor3->field_960 = old;
                        d4->field_CA      = -1;
                        d4->field_C6      = 0;
                        Gp_AnimPlayChildSlotsEx(arg0, anim, 0, 6);
                    }
                } else {
                    actor->field_975 = 0;
                }
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100_2", func_actor_800100_80164B9C);

void func_actor_800100_80164E60(GpActorWork* arg0)
{
    GameActor*     actor;
    GameActor*     target;
    GpActorD4*     d4;
    GpAnimRec*     rec;
    GsCOORDINATE2* coord;
    s16            sel;

    actor = arg0->actor;
    d4    = actor->field_910;
    rec   = Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1);
    coord = (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8;
    sel   = D_actor_800100_80167218[D_8007272F];

    switch (sel) {
        case 3:
            if (rec != NULL) {
                if (rec != actor->field_92C) {
                    actor->field_92C = rec;
                    if ((rec->field_3 & 0x30) == 0x30) {
                        if (actor->field_95E == 0) {
                            actor->field_95E = 1;
                        }
                    }
                }
            }
            break;
        case 12:
            if (actor->field_95E == 0) {
                actor->field_95E = 1;
                Gp_SpawnEff(0x6006E, coord, 0xC, NULL);
            }
            if (rec != NULL) {
                if (rec != actor->field_92C) {
                    actor->field_92C = rec;
                }
            }
            break;
        default:
            if (actor->field_95E == 0) {
                actor->field_95E = 1;
            } else {
                if (rec != NULL) {
                    if (rec != actor->field_92C) {
                        actor->field_92C = rec;
                    }
                }
            }
            break;
    }

    d4->field_CD = D_actor_800100_80167230[D_8007272F];
    if (rec != NULL && func_80105894(arg0, 1, 0, 0) == 0) {
        target            = arg0->actor;
        target->field_954 = 0;
        target->field_956 = 4;
        target->field_95C = 0;
        target->field_95E = 0;
        target->field_973 = 0;
        target->field_975 = 0;
        Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100_2", func_actor_800100_80165010);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100_2", func_actor_800100_801652B0);

extern GpActorFuncTable3 D_actor_800100_80161E4C;

void func_actor_800100_80165528(GpActorWork* arg0)
{
    GameActor*        actor;
    GpActorFuncTable3 sp;

    sp    = D_actor_800100_80161E4C;
    actor = arg0->actor;
    if (actor->field_940 > 0) {
        actor->field_940--;
    }
    if ((s8)actor->field_97A > 0) {
        actor->field_97A--;
    }
    actor->field_973 = 0;
    actor->field_975 = 0;
    sp.funcs[actor->field_954](arg0);
    actor->field_986 = 0;
}

void func_actor_800100_801655C0(GpActorWork* arg0)
{
    GameActor* actor;

    actor                      = arg0->actor;
    actor->field_956           = 3;
    actor->field_954           = 0;
    actor->field_95C           = 0;
    actor->field_95E           = 0;
    actor->field_910->field_CC = 0;
    actor->field_97E           = 2;
    actor->field_90C           = Gp_FindLockNode(arg0);
    Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 6);
}

void func_actor_800100_80165630(GpActorWork* arg0)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_956 = 7;
    actor->field_954 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_93E = 0;
    actor->field_97E = 1;
    actor->field_973 = 0;
    actor->field_975 = 0;
}

void func_actor_800100_80165664(GpActorWork* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    u16        temp;

    actor            = arg0->actor;
    temp             = actor->field_956;
    actor->field_956 = 0xA;
    actor->field_95A = 1;
    actor->field_97E = 1;
    d4               = actor->field_910;
    actor->field_954 = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_960 = temp;
    d4->field_CA     = -1;
    d4->field_C6     = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 6);
}

void func_actor_800100_801656C8(GpActorWork* arg0)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_956 = 1;
    actor->field_95A = 1;
    actor->field_954 = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
    actor->field_93E = 0x3C;
}

void func_actor_800100_801656F4(GpActorWork* arg0)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_956 = 2;
    actor->field_954 = 0;
    actor->field_958 = 0;
    actor->field_95A = 1;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
}

void func_actor_800100_80165720(GpActorWork* arg0)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_956 = 0xB;
    actor->field_954 = 0;
    actor->field_95A = 1;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
}

void func_actor_800100_80165748(GpActorWork* arg0)
{
    GameActor* actor;

    if (Gp_StateF0.field_0 == 1) {
        actor                      = arg0->actor;
        actor->field_956           = 3;
        actor->field_954           = 0;
        actor->field_95C           = 0;
        actor->field_95E           = 0;
        actor->field_910->field_CC = 0;
        actor->field_97E           = 2;
        actor->field_90C           = Gp_FindLockNode(arg0);
        Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 6);
        return;
    }
    func_actor_800100_80163D54(arg0);
}

void func_actor_800100_801657D8(GpActorWork* arg0)
{
    if (Gp_StateF0.field_0 != 1) {
        func_actor_800100_80166E14(arg0);
        return;
    }
    func_actor_800100_801659EC(arg0);
}

void func_actor_800100_80165818(GpActorWork* arg0)
{
    GameActor* actor;

    actor = arg0->actor;
    if (actor->field_95E != 0) {
        Gp_ResetActorMove(arg0, 0);
    }
}

extern GpActorFuncTable4 D_actor_800100_80161E88;

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "nonmatching D_actor_800100_80161E88\n"
        "dlabel D_actor_800100_80161E88\n"
        "    .word func_actor_800100_801658E8\n"
        "    .word func_actor_800100_801658E8\n"
        "    .word func_actor_800100_801658E8\n"
        "    .word func_actor_800100_80165928\n"
        "enddlabel D_actor_800100_80161E88\n"
        ".section .text");
#endif

void func_actor_800100_80165850(GpActorWork* arg0)
{
    GameActor*        actor;
    GpActorFuncTable4 sp;

    sp    = D_actor_800100_80161E88;
    actor = arg0->actor;
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    sp.funcs[(u16)actor->field_96C](arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

void func_actor_800100_801658E8(GpActorWork* arg0)
{
    GameActor* actor;
    u16        value;

    actor = arg0->actor;
    value = actor->field_95E;
    if (value == 0) {
        return;
    }
    if (value == 1) {
        func_8010C180(arg0);
    }
}

void func_actor_800100_80165928(void)
{
}

extern GpActorFuncTable7 D_actor_800100_80161E98;

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "nonmatching D_actor_800100_80161E98\n"
        "dlabel D_actor_800100_80161E98\n"
        "    .word 0x80108B80\n"
        "    .word 0x80108BAC\n"
        "    .word 0x80108BD8\n"
        "    .word 0x80108BAC\n"
        "    .word 0x80107E1C\n"
        "    .word 0x80108BAC\n"
        "    .word 0x80108D68\n"
        "enddlabel D_actor_800100_80161E98\n"
        ".section .text");
#endif

void func_actor_800100_80165930(GpActorWork* arg0)
{
    GameActor*        actor;
    GpActorFuncTable7 sp;

    sp    = D_actor_800100_80161E98;
    actor = arg0->actor;
    sp.funcs[(u16)actor->field_956](arg0);
    Gp_TurnPlayer(arg0);
    if (D_80072830 <= 0) {
        func_8010BFCC(arg0);
        Gp_StopPlayerAnim(arg0, 0);
    }
}

void func_actor_800100_801659EC(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GpLinkNode*    node;
    GsCOORDINATE2* coord;
    VECTOR3*       lock;
    VECTOR3*       head;
    s32            entry;
    s32            offset;
    s32            kind;
    s32            angle;
    s32            index;
    s32            inRange;
    s32            mode;

    head                       = *(VECTOR3**)G_SCRATCH_HEAD;
    lock                       = (VECTOR3*)((u8*)head - 0x10);
    *(VECTOR3**)G_SCRATCH_HEAD = lock;
    actor                      = arg0->actor;
    d4                         = actor->field_910;
    coord                      = arg0->extra->field_8;
    node                       = Gp_FindLockNode(arg0);
    actor->field_90C           = node;
    if (node != NULL) {
        Gp_GetLockPos((GpLockPos*)node, lock);
        func_80103C74(coord, lock, lock);
        kind = func_80103D8C(*(s32*)lock, lock->vz);
        mode = 2;
        if (kind >= 0x381) {
            if (kind < 0) {
                index  = kind;
                index += 0x3FF;
            } else {
                index = kind;
            }
            angle = index >> 0xA;
            if (angle >= 3) {
                if (angle < 5) {
                    angle = 3;
                }
            }
            inRange = angle < 4;
            if (inRange != 0) {
                entry  = (s32)D_actor_800100_801672F8[angle];
                offset = entry + (func_8010C058() * 0x10);
                mode   = *(u8*)(offset + (rand() & 0xF));
            } else {
                mode = 3;
            }
        } else {
            mode = 2;
        }
    } else {
        if ((s8)actor->field_97E == 2) {
            actor->field_97E = 1;
        }
        mode = D_actor_800100_80167308[rand() & 7];
        if (mode == 3) {
            actor->field_90C = NULL;
        }
    }
    switch (mode) {
        case 0:
            break;
        case 1:
            if ((s8)d4->field_CD <= 0) {
                func_actor_800100_80166E94(arg0, 0);
            } else {
                actor->field_97E = 2;
                actor->field_940 = (rand() & 0x1F) + 0xF;
                d4->field_CC     = D_actor_800100_80167310[rand() & 7];
                func_actor_800100_80166DD0(arg0);
            }
            break;
        case 2:
            func_actor_800100_80166DF0(arg0);
            func_8010BF7C(arg0, 0x14, 0x3F);
            break;
        case 3:
            func_actor_800100_80165630(arg0);
            break;
        case 4:
            func_actor_800100_80165664(arg0);
            break;
    }
    *(u8**)G_SCRATCH_HEAD += 0x10;
}

void func_actor_800100_80165C38(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* place;
    u16            state;

    place                            = (GsCOORDINATE2*)((u8*)*(void**)G_SCRATCH_HEAD - 0x50);
    *(GsCOORDINATE2**)G_SCRATCH_HEAD = place;

    actor = arg0->actor;
    d4    = actor->field_910;
    state = actor->field_960;
    coord = ((TmdObject*)actor->field_91C->extra)->field_8;

    switch (state) {
        case 0:
            actor->field_954 = 0;
            actor->field_958 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_960 = 1;
            d4->field_CD    -= 1;
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 3);
            func_80106238(arg0, 0, 0);
            actor->field_12A |= 0xC800;
            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x40650001, 1);
            Gp_SpawnEff(0x6002B, coord, 0x21, NULL);
            break;

        case 1:
            actor->field_960  = 2;
            actor->field_12A &= 0x3FFF;
            if (func_actor_800100_80166B40(actor->field_32C, coord, place) != 0) {
                Gp_PlayObjSfx((GpObj38*)place, 0x17, 1);
            }
            /* fallthrough */

        case 2:
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                actor->field_940 = 0xA;
                d4->field_CC    -= 1;
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}

void func_actor_800100_80165DE8(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    u16            state;
    GsCOORDINATE2* coord;

    actor = arg0->actor;
    d4    = actor->field_910;
    state = actor->field_960;
    coord = ((TmdObject*)actor->field_91C->extra)->field_8;

    switch (state) {
        case 0:
            actor->field_954 = 0;
            actor->field_958 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            /* fallthrough */
        case 1:
            actor->field_960 = 2;
            d4->field_CD    -= 1;
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 3);
            Gp_PlayObjSfx((GpObj38*)coord, 0x40660001, 1);
            if (coord != NULL) {
                actor->field_940 = 0x28;
                Gp_SpawnEff(0x6006C, coord, D_actor_800100_80167218[D_8007272F] | 0x10000, NULL);
                func_80104490(arg0, 1, 2, 0x110C0A);
                return;
            }
            return;
        case 2:
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                actor->field_940 = 0x12;
                d4->field_CC    -= 1;
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_800100/actor_800100_2", D_actor_800100_80161EC8);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100_2", func_actor_800100_80165F50);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100_2", func_actor_800100_80166190);

void func_actor_800100_80166514(GpActorWork* arg0)
{
    void**                   scratch;
    void*                    head;
    GameActor*               actor;
    GsCOORDINATE2            sp10;
    GsCOORDINATE2*           src;
    GpObj*                   obj;
    Actor800100PlaceScratch* blk;
    s16                      angle;

    actor       = arg0->actor;
    src         = ((TmdObject*)actor->field_91C->extra)->field_8;
    obj         = (GpObj*)actor->field_12C;
    sp10        = *src;
    obj->flags |= 0xC000;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor800100PlaceScratch*)((u8*)head - 0x5C);
    *scratch = blk;

    Gp_FindRec18(((GpActorD4Rec*)obj->field_C)->field_14, 0);
    Gfx_RotMatrixX(&sp10.workm, 0x400, 0);
    blk->rot.vx = 0;
    blk->rot.vy = 0x120;
    blk->rot.vz = 0x20;
    Gp_PlaceCoordOffset(&sp10, &blk->coord, (SVECTOR*)((u8*)head - 0xC));
    angle      = func_actor_800100_8016709C(&blk->coord, (GpRec18*)actor->pad_3BC, NULL);
    blk->angle = angle;
    func_actor_800100_8016666C(&blk->coord, angle);
    blk->rot.vx = 0;
    blk->rot.vz = 0;
    blk->rot.vy = blk->angle + 0x38;
    Gp_PlaceCoordOffset(&blk->coord, &blk->coord, (SVECTOR*)((u8*)head - 0xC));
    func_actor_800100_801668C0(&blk->coord);
    Gp_ClearRec18Occupied((GpRec18*)actor->pad_3BC);
    *scratch = (u8*)*scratch + 0x5C;
}

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100_2", func_actor_800100_8016666C);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100_2", func_actor_800100_801668C0);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100_2", func_actor_800100_80166B40);

void func_actor_800100_80166DD0(GpActorWork* arg0)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_954 = 0;
    actor->field_956 = 5;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_960 = 0;
}

void func_actor_800100_80166DF0(GpActorWork* arg0)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_956 = 6;
    actor->field_954 = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_97E = 1;
}

void func_actor_800100_80166E14(GpActorWork* arg0)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_956 = 8;
    actor->field_95C = 7;
    actor->field_954 = 0;
    actor->field_95E = 0;
    actor->field_90C = NULL;
    actor->field_97E = 1;
    func_80106350(arg0, D_actor_800100_80167218[D_8007272F], 0);
    Gp_AnimPlayChildSlotsEx(arg0, 8, 1, 6);
}

void func_actor_800100_80166E94(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_956 = 9;
    actor->field_960 = arg1;
    actor->field_954 = 0;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_97E = 1;
    Gp_AnimPlayChildSlotsEx(arg0, arg1 + 0xE, 0, 1);
}

extern GpActorFuncTable5 D_actor_800100_80161EC8;

void func_actor_800100_80166EE8(GpActorWork* arg0)
{
    GpActorFuncTable5 sp;

    sp = D_actor_800100_80161EC8;
    sp.funcs[D_8007272F](arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100_2", func_actor_800100_80166F50);

s32 func_actor_800100_8016709C(GsCOORDINATE2* arg0, GpRec18* arg1, GpRec18* arg2)
{
    s32 ret;

    if (arg1->field_4 != 0) {
        ret = func_80103D8C(arg0->workm.t[0] - arg1->field_8, arg0->workm.t[2] - arg1->field_C);
        if (arg2 != NULL) {
            arg2->field_0 = arg1->field_8;
            arg2->field_2 = arg1->field_A;
            arg2->field_0 = arg1->field_C;
        }
    } else {
        ret = 0;
    }
    return ret;
}
