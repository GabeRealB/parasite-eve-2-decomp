#include "common.h"
#include "actors/actor_800100.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>
#include <psyq/libgs.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

s32  func_8010BC70(GsCOORDINATE2* arg0);
s32  func_8010BCF4(Task* arg0, VECTOR3* arg1);
void func_8010BD88(GpActorWork* arg0, VECTOR3* arg1);
void func_8010BE5C(GpActorWork* arg0, VECTOR3* arg1);
s32  func_80105ED4(GpActorWork* arg0);
void Gp_PlayObjSfx(GsCOORDINATE2* coord, s32 sfx, s32 arg2);
s32  rand();

extern u32 Gp_LcgState;

extern GpActorFuncTable12 D_actor_800100_80161E58;
extern s32                D_80115738;
extern s32                D_8011574C;
extern s16                D_80072830;
extern s8                 D_8007272F;
extern u8                 D_80073BA9;
extern u16                D_80112F60[];
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
    coord  = arg0->extra->coords;
    target = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    flag   = (*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x042A0000;
    if (arg0->actor->field_910->decisionTimer <= 0) {
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
/// (`gGameSession.waterY`) minus the actor coordinate's world Y.
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
    coord = arg0->extra->coords;
    d4    = actor->field_910;
    if (d4->decisionTimer > 0) {
        d4->decisionTimer--;
    }
    sp.funcs[actor->field_956](arg0);
    if ((u32)(func_80105ED4(arg0) + 0xEFFFFF77) < 4) {
        actor->field_944 = 0x78;
        sp40.vx          = 0;
        sp40.vy          = (u16)gGameSession->waterY - (u16)coord->coord.t[1];
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
            sp48.vy = (u16)gGameSession->waterY - (u16)coord->coord.t[1];
            sp48.vz = 0;
            Gp_SpawnEff(D_8011574C, coord, (rand() & 0x1F) | 0x40, &sp48);
        }
    }
    if ((s8)actor->field_97A == 0) {
        func_80109BB4(arg0, actor->field_17C);
        if ((u16)actor->field_96C != 0) {
            func_8010B9A4(arg0);
            pan = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(((D_8007272F - 1) << 16) + 0x4065000A, pan, (s8)Gp_GetObjDepth(coord));
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

/// Lock-on entry and step of the actor's `field_95E` state machine. An aim
/// within `0x301` of the lock node re-arms the actor: `field_956` takes the
/// 10-frame delay, `field_95A`/`field_97E` latch the turn and decay, `field_960`
/// keeps the old `field_956`, and the `field_910` record's `scanDist` /
/// `scanAngle` are re-armed for the next sweep before the slot-1 child
/// animation. Otherwise state 0 zeroes `field_934` and picks state 2 (with
/// `field_958` 3) or state 1 (with `field_958` 1) from `func_8010BC70`'s
/// distance, states 1-3 only raise `field_973`. The shared drive then resets
/// the move when the target is within `0x301`, counts `field_934` up to `0xB4`
/// before latching state 3 through the state-1 entry, and otherwise drops
/// `field_93E` while it is positive, re-arming it to `0x3C` from a `rand()`
/// window and returning to state 0. The target's coordinate goes to
/// `func_8010BD88` and `func_8010BE5C`.
void func_actor_800100_80164184(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    s32            flag;
    s32            dist;
    s32            r;
    s32            arg;
    u16            timer;

    coord  = arg0->extra->coords;
    target = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    actor  = arg0->actor;
    flag   = (*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x042A0000;
    dist   = func_actor_800100_8016709C(coord, &actor->field_910->contact, NULL);
    if (dist != 0 && dist < 0x301 && flag == 0) {
        GameActor* actor2 = arg0->actor;

        timer             = actor2->field_956;
        actor2->field_956 = 0xA;
        actor2->field_95A = 1;
        actor2->field_97E = 1;
        d4                = actor2->field_910;
        actor2->field_954 = 0;
        actor2->field_95C = 0;
        actor2->field_95E = 0;
        actor2->field_960 = timer;
        d4->scanDist      = -1;
        d4->scanAngle     = 0;
        Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 6);
        return;
    }
    switch (actor->field_95E) {
        case 1:
        case 2:
        case 3:
            actor->field_973 = 1;
            goto drive;
        case 0:
            actor->field_934 = 0;
            if (func_8010BC70(coord) >= 0x1600) {
                actor->field_95E = 2;
                actor->field_958 = 3;
                arg              = 4;
            } else {
            enter:
                if (actor->field_95E != 3) {
                    actor->field_95E = 1;
                }
                actor->field_958 = 1;
                arg              = 2;
            }
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 5);
            actor->field_973 = 1;
        drive:
            dist = func_8010BC70(coord);
            if (dist < 0x301) {
                Gp_ResetActorMove(arg0, 0);
            } else if (actor->field_95E != 3) {
                if (++actor->field_934 == 0xB4) {
                    actor->field_95E = 3;
                    goto enter;
                }
                if (actor->field_93E > 0) {
                    actor->field_93E = (u16)actor->field_93E - 1;
                } else {
                    r = rand() & 0x3FF;
                    if ((0x1000 - r) < dist || actor->field_95E != 2) {
                        if (dist < r + 0x1400 || actor->field_95E != 1) {
                            goto done;
                        }
                    }
                    actor->field_95E = 0;
                    actor->field_93E = 0x3C;
                }
            }
    }
done:
    func_8010BD88(arg0, (VECTOR3*)target->coord.t);
    func_8010BE5C(arg0, (VECTOR3*)target->coord.t);
}

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
    extra    = (TmdObject*)(gameGetPtrSlot(3))->extra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = (u8*)head - 0x10;
    pos      = (VECTOR3*)((u8*)head - 0x10);
    node     = actor->field_90C;
    src      = extra->coords;
    if (node != NULL) {
        if (!(node->flags & 1)) {
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
                if (actor->field_90C->flags & 1) {
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
        if ((lock == NULL) || (coord = arg0->extra->coords, Gp_GetLockPos(lock, &scratch->lock), func_80103C74(coord, &scratch->lock, (VECTOR3*)((u8*)head - 0x10)), ((func_80103D8C(scratch->rot.vx, scratch->rot.vz) < 0x301) != 0))) {
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
                if (((s8)d4->repeatCount <= 0) || (node = actor->field_90C, node == NULL) || (node->flags & 1)) {
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
/// the angle to what the ally block's `field_910->contact` recorded: under
/// 0x281 the actor hands over to the `0xA` / slot-1 chain (`field_97E` set,
/// `field_956` kept in `field_960`), otherwise `field_975` is cleared; a spent
/// counter resets the state to slot 9.
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
    coord    = arg0->extra->coords;
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
                angle = func_actor_800100_8016709C(coord, &actor->field_910->contact, NULL);
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
                        d4->scanDist      = -1;
                        d4->scanAngle     = 0;
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

/// Aim/lock drive for the actor's `field_95E` phase machine. While the angle
/// to what the ally block's `field_910->contact` recorded is nonzero and under
/// `0x301`, `field_93E` counts up and the LCG decides the next aim window:
/// once the step passes `((Gp_LcgState >> 16) & 0x3F) + 0x28` the actor
/// latches into the `0xA` / child-slot-1 chain, keeping the old `field_956` in
/// `field_960` and clearing the aim offset on `field_910`. Otherwise it carves
/// a 0x20-byte `Actor800100LockScratch` off `G_SCRATCH_HEAD`, fills `lock`
/// either from the lock node (`Gp_GetLockPos`) or from the player's model
/// coordinate, runs the `field_95E` switch, measures the aim spread across
/// `rot`, and drops back to child slot 9 when the roll loses. Both arms end by
/// handing `lock` to `func_8010BD88` / `func_8010BE5C` and returning the
/// scratch.
void func_actor_800100_80164B9C(GpActorWork* arg0)
{
    GameActor*              actor;
    GameActor*              actor2;
    GameActor*              actor3;
    GpActorD4*              d4;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          target;
    Actor800100LockScratch* block;
    GpLinkNode*             node;
    void**                  scratch;
    u8*                     head;
    u16                     step;
    u16                     old;
    s16                     anim;
    u32                     random;
    s32                     angle;
    s32                     val;

    coord  = arg0->extra->coords;
    target = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    actor  = arg0->actor;
    angle  = func_actor_800100_8016709C(coord, &actor->field_910->contact, NULL);
    if (angle != 0 && angle < 0x301) {
        step             = actor->field_93E + 1;
        actor->field_93E = step;
        random           = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState      = random;
        if ((s16)step >= (s32)(((random >> 16) & 0x3F) + 0x28)) {
            anim              = 1;
            actor2            = arg0->actor;
            old               = actor2->field_956;
            actor2->field_956 = 0xA;
            actor2->field_95A = anim;
            actor2->field_97E = anim;
            d4                = actor2->field_910;
            actor2->field_954 = 0;
            actor2->field_95C = 0;
            actor2->field_95E = 0;
            actor2->field_960 = old;
            d4->scanDist      = -1;
            d4->scanAngle     = 0;
            Gp_AnimPlayChildSlotsEx(arg0, anim, 0, 6);
            return;
        }
    }
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x20;
    block    = (Actor800100LockScratch*)(head - 0x20);
    node     = actor->field_90C;
    if (node != NULL) {
        if ((node->flags & 1) == 0) {
            Gp_GetLockPos((GpLockPos*)node, &block->lock);
        } else {
            actor->field_95E = 2;
        }
    } else {
        block->lock.vx = target->coord.t[0];
        block->lock.vy = target->coord.t[1];
        block->lock.vz = target->coord.t[2];
    }
    switch (actor->field_95E) {
        case 0:
            actor->field_95E = 1;
            actor->field_934 = 0;
            actor->field_958 = 3;
            Gp_AnimPlayChildSlotsEx(arg0, 0xC, 0, 5);
        case 1:
        case 2:
            break;
        default:
            goto tail;
    }
    actor->field_973 = 1;
    func_80103C74(coord, &block->lock, &block->rot);
    angle = func_80103D8C(block->rot.vx, block->rot.vz);
    if (actor->field_90C != NULL) {
        val = (rand() & 0x3FF) + 0xB00;
    } else {
        val = 0xB00;
    }
    if (val >= angle || actor->field_95E == 2) {
        actor3            = arg0->actor;
        actor3->field_954 = 0;
        actor3->field_956 = 4;
        actor3->field_95C = 0;
        actor3->field_95E = 0;
        actor3->field_973 = 0;
        actor3->field_975 = 0;
        Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
    }
tail:
    func_8010BD88(arg0, &block->lock);
    func_8010BE5C(arg0, &block->lock);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
}

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
    rec   = Gp_AnimGetRec((GpAnimCtx*)actor->field_424, actor->field_438 + 1);
    coord = (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->coords;
    sel   = D_actor_800100_80167218[D_8007272F];

    switch (sel) {
        case 3:
            if (rec != NULL) {
                if (rec != actor->field_92C) {
                    actor->field_92C = rec;
                    if ((rec->flags & 0x30) == 0x30) {
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

    d4->actionCount = D_actor_800100_80167230[D_8007272F];
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

    actor                         = arg0->actor;
    actor->field_956              = 3;
    actor->field_954              = 0;
    actor->field_95C              = 0;
    actor->field_95E              = 0;
    actor->field_910->repeatCount = 0;
    actor->field_97E              = 2;
    actor->field_90C              = Gp_FindLockNode(arg0);
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
    d4->scanDist     = -1;
    d4->scanAngle    = 0;
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
        actor                         = arg0->actor;
        actor->field_956              = 3;
        actor->field_954              = 0;
        actor->field_95C              = 0;
        actor->field_95E              = 0;
        actor->field_910->repeatCount = 0;
        actor->field_97E              = 2;
        actor->field_90C              = Gp_FindLockNode(arg0);
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
    coord                      = arg0->extra->coords;
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
            if ((s8)d4->actionCount <= 0) {
                func_actor_800100_80166E94(arg0, 0);
            } else {
                actor->field_97E = 2;
                actor->field_940 = (rand() & 0x1F) + 0xF;
                d4->repeatCount  = D_actor_800100_80167310[rand() & 7];
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
    coord = ((TmdObject*)actor->field_91C->extra)->coords;

    switch (state) {
        case 0:
            actor->field_954 = 0;
            actor->field_958 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_960 = 1;
            d4->actionCount -= 1;
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 3);
            func_80106238(arg0, 0, 0);
            actor->field_12A |= 0xC800;
            Gp_PlayObjSfx(arg0->extra->coords, 0x40650001, 1);
            Gp_SpawnEff(0x6002B, coord, 0x21, NULL);
            break;

        case 1:
            actor->field_960  = 2;
            actor->field_12A &= 0x3FFF;
            if (func_actor_800100_80166B40(actor->field_32C, coord, place) != 0) {
                Gp_PlayObjSfx(place, 0x17, 1);
            }
            /* fallthrough */

        case 2:
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                actor->field_940 = 0xA;
                d4->repeatCount -= 1;
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
    coord = ((TmdObject*)actor->field_91C->extra)->coords;

    switch (state) {
        case 0:
            actor->field_954 = 0;
            actor->field_958 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            /* fallthrough */
        case 1:
            actor->field_960 = 2;
            d4->actionCount -= 1;
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 3);
            Gp_PlayObjSfx(coord, 0x40660001, 1);
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
                d4->repeatCount -= 1;
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_800100/actor_800100_2", D_actor_800100_80161EC8);

void func_actor_800100_80165F50(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* place;
    u16            state;
    void**         scratch;
    void*          head;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = (u8*)head - 0x50;
    place    = (GsCOORDINATE2*)((u8*)head - 0x50);

    actor = arg0->actor;
    d4    = actor->field_910;
    state = actor->field_960;
    coord = ((TmdObject*)actor->field_91C->extra)->coords;

    switch (state) {
        case 0:
            actor->field_954  = 0;
            actor->field_958  = 0;
            actor->field_95A  = 2;
            actor->field_95C  = 0;
            d4->repeatCount   = (rand() & 7) + 3;
            actor->field_12A |= 0x800;
            /* fallthrough */

        case 1:
        block_4:
            actor->field_960 = 2;
            actor->field_940 = 0;
            actor->field_934 = 3;
            func_80106238(arg0, 0, 0);
            /* fallthrough */

        case 2:
            actor->field_934 -= 1;
            if (actor->field_934 == 0) {
                actor->field_960 += 1;
                d4->actionCount  -= 1;
                actor->field_12A |= 0xC000;
                Gp_PlayObjSfx(arg0->extra->coords, 0x40670001, 1);
                Gp_SpawnEff(0x6002B, coord, D_actor_800100_80167218[D_8007272F] | 0x10000, NULL);
                Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 2);
            }
            break;

        case 3:
            actor->field_960 += 1;
            actor->field_12A &= 0x3FFF;
            if (func_actor_800100_80166B40(actor->field_32C, coord, place) != 0) {
                Gp_PlayObjSfx(place, 0x17, 1);
            }
            /* fallthrough */

        case 4:
            d4->repeatCount -= 1;
            if ((s8)d4->repeatCount > 0) {
                if ((s8)d4->actionCount > 0) {
                    goto block_4;
                }
            }
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}

void func_actor_800100_80166190(GpActorWork* arg0)
{
    void**         scratch;
    void*          head;
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* place;
    u16            state;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = (u8*)head - 0x50;
    place    = (GsCOORDINATE2*)((u8*)head - 0x50);

    actor = arg0->actor;
    d4    = actor->field_910;
    state = actor->field_960;
    coord = ((TmdObject*)actor->field_91C->extra)->coords;

    switch (state) {
        case 0:
            actor->field_954  = 0;
            actor->field_958  = 0;
            actor->field_95C  = 0;
            actor->field_960 += 1;
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 1);
            break;

        case 1:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424,
                              actor->field_438 + 1) != NULL) {
                actor->field_960 += 1;
            }
            break;

        case 2:
            Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
            if (((Gp_LcgState >> 16) & 0xFF) < 0x3F) {
                actor->field_960 = 5;
                actor->field_95A = 2;
                actor->field_940 = 0x28;
                actor->field_979 = 0x1C;
                actor->field_93E = 0x14;
                Gp_PlayObjSfx(coord, 0x40680002, 1);
                if (actor->field_914 != NULL) {
                    actor->field_914->spawnArg1 = 2;
                }
                break;
            }
            actor->field_960 = 3;
            actor->field_95A = 0;
            actor->field_934 = 0;
            actor->field_979 = 9;
            actor->field_93E = 3;
            func_80106238(arg0, 0, 1);
            actor->field_12A |= 0x800;
            /* fallthrough */

        case 3:
            if (actor->field_93E != 0) {
                if (actor->field_934 == 0) {
                    actor->field_93E  = (u16)actor->field_93E - 1;
                    actor->field_934  = 3;
                    actor->field_12A |= 0xC000;
                    d4->actionCount  -= 1;
                    if ((s8)d4->actionCount == 0) {
                        actor->field_93E = 0;
                    }
                    Gp_PlayObjSfx(coord, 0x40680001, 1);
                    Gp_SpawnEff(0x6006B, coord, D_actor_800100_80167218[D_8007272F] | 0x10000, NULL);
                    Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 2);
                    break;
                } else {
                    actor->field_934 -= 1;
                    if (actor->field_934 != 0) {
                        break;
                    }
                }
                actor->field_12A &= 0x3FFF;
                if (func_actor_800100_80166B40(actor->field_32C, coord, place) != 0) {
                    Gp_PlayObjSfx(place, 0x17, 1);
                }
                break;
            }
            /* fallthrough */

        case 4:
            actor->field_960  = 6;
            actor->field_12A &= 0x3FFF;
            if (func_actor_800100_80166B40(actor->field_32C, coord, place) != 0) {
                Gp_PlayObjSfx(place, 0x17, 1);
            }
            break;

        case 5:
            if (actor->field_93E == 0) {
                actor->field_960 = 6;
                if (actor->field_914 != NULL) {
                    actor->field_914->spawnArg1 = 3;
                }
                SndEvt_EnqueueType7(0x40680002, 1);
                Gp_AnimPlayChildSlotsEx(arg0, 0xB, 0, 2);
            } else {
                actor->field_93E = (u16)actor->field_93E - 1;
            }
            break;

        case 6:
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                actor->field_940 = 0xF;
                d4->repeatCount  = (actor->field_97F == 1) ? d4->repeatCount - 1 : 0;
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}

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
    src         = ((TmdObject*)actor->field_91C->extra)->coords;
    obj         = (GpObj*)actor->field_12C;
    sp10        = *src;
    obj->flags |= 0xC000;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor800100PlaceScratch*)((u8*)head - 0x5C);
    *scratch = blk;

    Gp_FindRec18(obj->ctx.d4rec->recs, 0);
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

/* The 0x1C bytes are carved off `head` into `newhead` and stored there, but the
   GTE calls address them through the typed `blk` view: the ROM keeps that typed
   pointer as a copy of `newhead` in `$a3`, and one variable for both drops it.
   The post-`rcos` reads go through `newhead` for the same reason - naming `blk`
   there would keep the copy live across the call - and the two `sxy0` reads are
   spelled off `head`, whose folded address is the one the ROM uses. */
void func_actor_800100_8016666C(GsCOORDINATE2* arg0, s16 arg1)
{
    void**                  scratch;
    u8*                     head;
    u8*                     newhead;
    Actor800100LineScratch* blk;
    LINE_G2*                prim;
    s16                     angle;
    s32                     sy0;
    s32                     sy1;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    newhead  = head - sizeof(Actor800100LineScratch);
    blk      = (Actor800100LineScratch*)newhead;
    *scratch = newhead;

    angle = arg1;
    if (arg1 == 0) {
        angle = D_80112F60[D_80073BA9];
    }
    blk->tip.vy    = angle;
    blk->origin.vx = 0;
    blk->origin.vy = 0;
    blk->origin.vz = 0;
    blk->tip.vx    = 0;
    blk->tip.vz    = 0;

    gte_SetTransMatrix(&arg0->workm);
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(&blk->origin);
    gte_rtps_real();
    gte_stsxy(&blk->sxy0);
    gte_ldv0(&blk->tip);
    gte_rtps_real();
    gte_stsxy(&blk->sxy1);
    gte_stszotz(&blk->otz);

    if (((Actor800100LineScratch*)newhead)->otz >= 0x20) {
        prim           = (LINE_G2*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setLineG2(prim);
        prim->x0 = ((Actor800100LineScratch*)(head - sizeof(Actor800100LineScratch)))->sxy0.vx;
        /* Both `vy` loads sign-extend, which needs the `s32` locals: a direct
           16-bit field copy assembles to `lhu` for either of them. */
        sy0      = ((Actor800100LineScratch*)(head - sizeof(Actor800100LineScratch)))->sxy0.vy;
        prim->y0 = sy0;
        prim->x1 = ((Actor800100LineScratch*)newhead)->sxy1.vx;
        sy1      = ((Actor800100LineScratch*)newhead)->sxy1.vy;
        prim->y1 = sy1;
        /* Both ends pulse with the frame counter, the far one 0x50 darker. */
        prim->r0 = (rcos(gDisplayState.gameTick) & 0x1F) - 0x80;
        prim->g0 = 0x20;
        prim->b0 = 0x20;
        prim->r1 = prim->r0 - 0x50;
        prim->g1 = 0;
        prim->b1 = 0;
        addPrim((u_long*)(((((u32)((Actor800100LineScratch*)newhead)->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, ((Actor800100LineScratch*)newhead)->otz);
    }
    *scratch = (u8*)*scratch + sizeof(Actor800100LineScratch);
}

/// The four (y, z) corners of the quad `func_actor_800100_801668C0` draws,
/// offset off the placed coordinate's world translation. The table sits in the
/// unit's .rodata right after the jump tables, so it is written here rather
/// than left to the split: nothing else refers to it.
const Actor800100QuadCorner D_actor_800100_80161F10[4] = {
    { -62, 0 },
    { -62, 124 },
    { 62, 0 },
    { 62, 124 },
};

/* The beam quad `func_actor_800100_80166514` places: the four
   `D_actor_800100_80161F10` (y, z) offsets, raised to the coordinate's world
   translation, projected through `GsWSMATRIX` and textured with one 0x20
   square of the atlas.
   The 0x44 bytes are carved off the scratch head and written back in one
   chained assignment: the ROM keeps the allocated pointer as a copy of the
   store's temporary in `$t1`, and splitting the two into separate statements
   drops that copy. */
void func_actor_800100_801668C0(GsCOORDINATE2* arg0)
{
    void**                  scratch;
    Actor800100QuadScratch* blk;
    POLY_FT4*               prim;
    s32                     i;
    s32                     ay;
    s32                     az;
    s32                     sy;

    scratch = (void**)G_SCRATCH_HEAD;
    blk     = (*scratch = (Actor800100QuadScratch*)((u8*)*scratch - sizeof(Actor800100QuadScratch)));

    for (i = 0; i < 4; i++) {
        ay           = D_actor_800100_80161F10[i].vy;
        az           = D_actor_800100_80161F10[i].vz;
        blk->work.vx = 0;
        blk->work.vy = ay;
        blk->work.vz = az;
        blk->v[i].vx = *(u16*)&blk->work.vx + *(u16*)&arg0->workm.t[0];
        blk->v[i].vy = *(u16*)&blk->work.vy + *(u16*)&arg0->workm.t[1];
        blk->v[i].vz = *(u16*)&blk->work.vz + *(u16*)&arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);

    gte_ldv0(&blk->v[0]);
    gte_rtps_real();

    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setPolyFT4(prim);

    gte_stsxy2(&blk->sxy[0]);

    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt_real();
    prim->tpage = 0x27;
    prim->clut  = 0x3CCE;
    setUV4(prim, 0x20, 0x80, 0x3F, 0x80, 0x20, 0x9F, 0x3F, 0x9F);
    prim->code |= 3;

    gte_stsxy3(&blk->sxy[1], &blk->sxy[2], &blk->sxy[3]);
    gte_stszotz(&blk->otz);

    /* Both `vy` loads sign-extend, which the `s32` locals keep: a direct
       16-bit field copy assembles to `lhu` for either of them. */
    prim->x0 = blk->sxy[0].vx;
    sy       = blk->sxy[0].vy;
    prim->y0 = sy;
    prim->x1 = blk->sxy[1].vx;
    sy       = blk->sxy[1].vy;
    prim->y1 = sy;
    prim->x2 = blk->sxy[2].vx;
    sy       = blk->sxy[2].vy;
    prim->y2 = sy;
    prim->x3 = blk->sxy[3].vx;
    sy       = blk->sxy[3].vy;
    prim->y3 = sy;

    addPrim((u_long*)&gGpuCurrentOt[blk->otz >> 4], prim);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor800100QuadScratch);
}

s32 func_actor_800100_80166B40(GpRec18* arg0, GsCOORDINATE2* arg1, GsCOORDINATE2* arg2)
{
    s32 minDist;

    minDist = 0x7FFFFFFF;
    if (Gp_CountRec18Hi(arg0, 0x30000) == 0) {
        s32               idx;
        s32*              pidx;
        register void**   scratch asm("v1");
        GpPickScratch*    block;
        register GpRec18* rec asm("s2");
        s32               i;
        s32               bestIdx;
        s32               dist;
        GpRec18*          picked;

        scratch = (void**)G_SCRATCH_HEAD;
        i       = 0;
        bestIdx = i;
        pidx    = &idx;
        rec     = arg0;
        {
            register void* p asm("v0");
            p        = *scratch;
            p        = (u8*)p - 0x68;
            block    = p;
            *scratch = p;
        }
        do {
            if (rec->key & 0x100000) {
                s32 fy;
                s32 dy;
                {
                    register s32 dx asm("v0");
                    dx   = arg1->workm.t[0] - rec->point.vx;
                    fy   = rec->point.vy;
                    dist = dx;
                    if (dx < 0) {
                        dist = -dist;
                    }
                }
                {
                    register s32 t2 asm("v0");
                    register s32 fz asm("a0");
                    dy = arg1->workm.t[1] - fy;
                    t2 = arg1->workm.t[2];
                    if (dy < 0) {
                        dy = -dy;
                    }
                    fz    = rec->point.vz;
                    dist += dy;
                    t2    = t2 - fz;
                    TOUCH_REG2(t2, dist);
                    if (t2 < 0) {
                        t2 = -t2;
                    }
                    dist += t2;
                }
                if (dist < minDist) {
                    func_800E0FEC(rec, (GpDeltaScratch*)block, 1, pidx);
                    idx = func_800E1ACC((u8*)pidx);
                    {
                        GameSession* session = gGameSession;
                        if (Gp_RoomParamTables[session->at4.loc.stage - 1][session->at4.loc.area - 1][idx]->field_2 != 0) {
                            minDist = dist;
                            bestIdx = i;
                        }
                    }
                }
            }
            i++;
            rec++;
        } while (i < 6);
        if (minDist != 0x7FFFFFFF) {
            i                = 1;
            picked           = (GpRec18*)(bestIdx * 0x18 + (s32)arg0);
            block->sub       = 0;
            block->flg       = i;
            block->t[0]      = picked->point.vx;
            block->t[1]      = picked->point.vy;
            block->t[2]      = picked->point.vz;
            block->offset.vx = rand() & 7;
            block->offset.vy = rand() & 7;
            block->offset.vz = rand() & 7;
            if (arg2 != 0) {
                arg2->workm.t[0] = block->t[0] + block->offset.vx;
                arg2->workm.t[1] = block->t[1] + block->offset.vy;
                arg2->workm.t[2] = block->t[2] + block->offset.vz;
            }
            Gp_SpawnEff(0x6003B, (GsCOORDINATE2*)&block->flg, 0, &block->offset);
        } else {
            i = 0;
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x68;
        return i;
    }
    return 0;
}

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

    if (arg1->key != 0) {
        ret = func_80103D8C(arg0->workm.t[0] - arg1->point.vx, arg0->workm.t[2] - arg1->point.vz);
        if (arg2 != NULL) {
            arg2->flags = arg1->point.vx;
            arg2->depth = arg1->point.vy;
            arg2->flags = arg1->point.vz;
        }
    } else {
        ret = 0;
    }
    return ret;
}
