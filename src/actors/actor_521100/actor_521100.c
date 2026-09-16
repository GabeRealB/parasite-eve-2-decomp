#include "common.h"

#include "main/sound.h"
#include "main/session.h"
#include "main/task.h"
#include "main/wipsys.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_521100.h"
#include "actors/actors_shared_80134934.h"

#include <psyq/inline_c.h>

/* `gte_MulMatrix0` from `psyq/gtemac.h`, except with the real `rtir` encoding
 * this toolchain assembles correctly. */
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s16 D_80073BA0;

void func_actor_521100_801322F8(Actor521100* arg0, Actor521100Obj2C* arg1, s32 arg2);
void func_actor_521100_80134C38(Actor521100* arg0);
void func_actor_521100_80134D88(Actor521100* arg0);
void func_actor_521100_80134EDC(Actor521100* arg0);
void func_actor_521100_80135024(Actor521100* arg0);
void func_actor_521100_80132958(void);
void func_actor_521100_80132DE8(void);
void func_actor_521100_801339B0(void);
void func_actor_521100_80134658(Actor521100* arg0);
/* Reads the caller's `Actor521100*` from $a0; the call passes no argument. */
void func_actor_521100_80134774();

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", ActorsShared80131e24Sub0);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_801322F8);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80132958);

/// Asks the player for the hold (message 0x3F8, range 0x19) once the actor has
/// swung its heading to within 0x20 of the slot-3 task's own and is lined up
/// to latch on. The heading error is the 12-bit difference between the work
/// block's `field_698` and `field_696`, wrapped into [-0x800, 0x800] and then
/// narrowed by `field_69C` being armed with 0x50; the request goes out only
/// while fewer than 0x4E2 units of the actor's health are left, the latch
/// `field_6BE` is clear, `D_80073BA0` (the remaining-enemy count) is positive
/// and the player's own `GameActor::field_954` is not its mode 2. On acceptance
/// the body rearms the motion state (2 into `field_69E`, 0xA frames of blend
/// into `field_686`, the 0xA/0xFF/0x80 pad lerp) and returns 1; the 0x3F8
/// query buffer is the 0x18 bytes `SCRATCH_SP` is pushed by.
s32 func_actor_521100_80132C70(Actor521100* arg0)
{
    Actor521100Work*   work;
    Task*              player;
    Actor521100Msg3F8* msg;
    s16                diff;
    s32                adiff;
    s16                wrap;
    s32                ret;

    work   = arg0->field_1C;
    player = Game_GetPtrSlot(3);
    msg    = (Actor521100Msg3F8*)(SCRATCH_SP -= 0x18);

    diff  = work->field_698 - work->field_696;
    adiff = diff >= 0 ? diff : -diff;
    ret   = 0;
    if (adiff < 0x800) {
        wrap = adiff;
    } else if (diff > 0) {
        wrap = 0x1000 - diff;
    } else {
        wrap = diff + 0x1000;
    }
    if ((wrap < 0x400) && (work->field_6AA < 0x4E2) && (work->field_6BE == 0) && (D_80073BA0 > 0) && (work->field_69C = 0x50, (wrap < 0x20)) && (((GpActorWork*)player)->actor->field_954 != 2)) {
        msg->field_14 = 0x19;
        if (Gp_DispatchMsg(player, 0x3F8, (s32)msg, 0) == 0) {
            ret             = 1;
            work->field_6A8 = 0;
            work->field_69E = 2;
            work->field_6A0 = 0;
            work->field_6A2 = 0;
            work->field_686 = 0xA;
            work->field_69A = 0;
            work->field_69C = 0;
            Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
        }
    }
    SCRATCH_SP += 0x18;
    return ret;
}
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80132DE8);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80133104);

/// Runs one frame of the burn-out sequence timed off the clip the slots are
/// playing: `D_actor_521100_8015F894[field_686]` is the clip's own length, read
/// signed and again unsigned because the cue frames below need it both ways,
/// and `field_68A` is the frame counter the blend in
/// `func_actor_521100_80135964` ticks. The counter is re-read at each cue
/// rather than carried, so the effects spawned in between cannot leave a stale
/// copy behind.
///
/// The cues, all offsets from that length: under +0x28 the turn limit
/// `field_69C` is held at 0x50; at +0x23 effect 0x60188 drops onto the attach
/// coordinate eight slots along and the 0xA/0x40/0xFF pad lerp starts; at
/// +0x27 the 8-byte scratch `SVECTOR` is thrown to (-0x320, 0x64, 0) and handed
/// to effect 0x6009C on the coordinate `field_654`'s own display object
/// carries; and +0x23 again, this time against the unsigned length, arms
/// `field_6AE` and raises the two record flags at 0x59A / 0x5BA together, then
/// cues `SndEvt_EnqueueType6` with the actor's pan and depth narrowed to bytes.
/// +0x2D hands the flags back down and clears the parked animation `field_6A6`.
///
/// The two ends are the motion: `field_69A` is held at 0x88 of forward speed
/// while the counter is between +0x20 and +0x2A of the length, and is zero
/// everywhere else, and past +0x90 the sequence starts over - clip 1, a fresh
/// effect id out of `D_actor_521100_8015F5F4` (the top four bits of an LCG
/// draw) into `field_68E`, and the state latch `field_69E`, its phase
/// `field_6A0` and the armed flag all cleared.
void func_actor_521100_8013334C(Actor521100* arg0)
{
    Actor521100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         head;
    SVECTOR*         vec;
    u16*             tbl;
    u32              rng;
    s16              clip;
    u16              clipId;
    s16              turn;
    s16              speed;
    s16              frame;
    s16              frame2;
    s32              frame3;
    s32              snd;
    s32              pan;

    work                       = arg0->field_1C;
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 1;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    clip                       = D_actor_521100_8015F894[work->field_686];
    clipId                     = D_actor_521100_8015F894[work->field_686];
    coord                      = arg0->field_2C->field_8;

    turn = 0;
    if ((s16)work->field_68A < clip + 0x28) {
        turn = 0x50;
    }
    work->field_69C = turn;

    frame = (s16)work->field_68A;
    if (frame == clip + 0x23) {
        Gp_SpawnEff(0x60188, arg0->field_2C->field_8 + 8, 0xC, NULL);
        Gp_SpawnPadLerp(0xA, 0x40, 0xFF);
    } else if (frame == clip + 0x27) {
        vec->vx = -0x320;
        vec->vy = 0x64;
        vec->vz = 0;
        Gp_SpawnEff(0x6009C, work->field_654->field_2C->field_8, 0, vec);
    }

    frame2 = (s16)work->field_68A;
    if (frame2 == (s16)clipId + 0x23) {
        work->field_6AE     = 1;
        work->obj57C.flags |= 0x8000;
        work->obj59C.flags |= 0x8000;
        snd                 = (((u32)arg0->field_20->field_8 >> 12) << 8) | 0x401C0008;
        pan                 = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        speed = 0;
    } else {
        speed = 0;
        if (frame2 == (s16)clipId + 0x2D) {
            work->field_6A6     = 0;
            work->obj57C.flags &= 0x7FFF;
            work->obj59C.flags &= 0x7FFF;
        }
    }

    frame3 = (s16)work->field_68A;
    if ((s16)clipId + 0x20 < frame3) {
        if ((s16)clipId + 0x2A >= frame3) {
            speed = 0x88;
        }
    }
    work->field_69A = speed;
    if ((s16)work->field_68A >= (s16)clipId + 0x90) {
        work->field_686 = 1;
        work->field_69E = 0;
        work->field_6A0 = 0;
        tbl             = D_actor_521100_8015F5F4;
        rng             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState     = rng;
        work->field_68E = tbl[(rng >> 16) & 0xF];
        work->field_6AE = 0;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 1;
}
/// Step-0 body of the burn-out sequence: the transition into it and the two
/// respawn draws. `field_6A2` is a four-phase latch. Phase 0 waits out the clip
/// long enough for the actor to commit (`D_actor_521100_8015F894[field_686]`
/// plus 0x38) and then latches clip 4 and hands phase 1 a fresh effect id out
/// of `D_actor_521100_8015F5D4`, cueing the 0x401C0007 sound with the actor's
/// own pan and depth. Phase 1 counts the effect id down (unsigned `field_68E`,
/// tested as a halfword) and, when it lands, either arms the two collision
/// nodes with the type-2 pair and asks for clip 8, or - once `field_6AA` has
/// run out - drops the actor back to idle with a draw out of
/// `D_actor_521100_8015F5F4`.
///
/// Phase 2 walks the turn limit `field_69C` 0x3C up while the clip is young,
/// fires the 0x401C0009 cue, the effect 0x60188 on the eighth coordinate and
/// the 0xA/0x40/0xFF pad lerp together on the clip's 0x20th frame, holds the
/// forward speed at 0x64 across the 0x22..0x26 window, and at 0x27 latches
/// phase 3 and hands both record flags back. Phase 3 waits 0x5E frames and then
/// picks the finish off `coord->coord.t[0]`: under -0xFA0 the actor stays
/// burning (phase 2 of the latch, or 1 in the session's mode 2), otherwise it
/// resets to idle with the clip-1 draw.
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_801335B4);
INCLUDE_RODATA("actors/nonmatchings/actor_521100/actor_521100", D_actor_521100_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_801339B0);

/// Step-4 body of the burn-out sequence, the fourth of the ones the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is a
/// three-phase latch: phase 0 hands the record flags at 0x59A / 0x5BA back and
/// asks the slot blend for clip 0xE, phase 1 waits out 5 blended frames and
/// asks for clip 0xF, and phase 2 waits out 0x26 of them and then either drops
/// the actor to the idle state or, when `field_6BA` asks for it, on to state 6
/// at sub-state `field_6BC`. Phase 2 latches clip 1 for the blend and picks this
/// frame's effect out of `D_actor_521100_8015F634`, the same 4-bit draw
/// `func_actor_521100_8013570C` makes.
void func_actor_521100_80134658(Actor521100* arg0)
{
    Actor521100Work* work;
    u16*             tbl;
    u32              rng;

    work = arg0->field_1C;
    switch (work->field_6A0) {
        case 0:
            work->field_686     = 0xE;
            work->field_6A0     = 1;
            work->field_69A     = 0;
            work->field_69C     = 0;
            work->obj57C.flags &= 0x7FFF;
            work->obj59C.flags &= 0x7FFF;
            return;
        case 1:
            if ((s16)work->field_68A >= 5) {
                work->field_686 = 0xF;
                work->field_6A0 = 2;
            }
            return;
        case 2:
            if ((s16)work->field_68A >= 0x26) {
                if (work->field_6BA == 0) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else {
                    work->field_69E = 6;
                    work->field_6A0 = work->field_6BC;
                }
                work->field_686 = 1;
                tbl             = D_actor_521100_8015F634;
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            return;
    }
}
/// State-6 body of the burn-out sequence, the last one the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is a
/// three-phase latch again and `D_actor_521100_8015F654` holds one waypoint per
/// phase; every phase seeds the slot blend the same way (`field_686` 0x12,
/// forward speed `field_69A` 0x14, turn limit `field_69C` 0x78) and builds the
/// vector from the attach coordinate's translation to its target into the
/// 0x18-byte scratch, of which only the `vec` half is written.
///
/// Phase 0 aims at the player (`Wip_SysConfig.field_4->t`) and hands the actor
/// back to state 1, speed zeroed, once it is within 0x7D0 of it and the
/// player's own Z is past -0x5DC; otherwise it aims at waypoint 0 and steps the
/// phase to 1 on arrival within 0x3C. Those two paths leave the switch
/// directly, while the ones that reach neither clear `field_69E` and
/// `field_6A0` - or raise `field_6BA` only, in a session whose `field_4` is 2 -
/// and then clear `field_6BC`. Phase 1 aims at the player and falls back to
/// waypoint 1 past 0x7D0, re-aiming at the player from 0x3C of that waypoint;
/// the within-0x7D0 path and the re-aim one share the epilogue that stops the
/// actor (`field_698` re-aimed, `field_69A` 0, `field_69C` 0x78, `field_69E` 1,
/// `field_6A0` 0), while the far one goes to `game`, where the phase steps to 2
/// and both `field_6BA` / `field_6BC` are cleared, or both raised when
/// `field_4` is 2. Phase 2 aims at waypoint 2 and, from the coordinate's X past
/// -0xFA0, clears `field_69E` and `field_6A0` (the session check there only
/// clears `field_6A0`), then drops both flags.
///
/// `sc2` is a second view of the same scratch that only phase 0's else branch
/// reads: CSE folds its initialisation into a copy of `sc`, and the
/// `do { ... } while (0)` around phase 1's `ratan2` is what keeps `head` ahead
/// of that copy in the register allocator's order - see
/// `DECOMPILATION_LEARNINGS.md`, "loop_depth as an allocation weight".
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80134774);
/// Steers the actor's heading towards the work block's `field_698` at up to
/// `field_69C` of turn per frame, then builds the result into the attach
/// coordinate as a pure-yaw rotation. The heading error is `field_698` minus
/// the coordinate's own Z-axis yaw (`ratan2` of `m[0][2]` over `m[2][2]`,
/// masked to the 12 bits the rotation is measured in), taken signed; the new
/// `field_696` is the target when the error is within the turn limit, and the
/// current yaw stepped by that limit otherwise. Errors past half a turn take
/// the short way round the wrap: the limit only has to beat `0x1000` minus the
/// error (or the error plus `0x1000`) to snap, so the turn never crosses into
/// the far half. `field_696` is read back as a signed half, the form the
/// sibling overlays' work blocks declare their yaw in; this body is the same
/// one `Actor02500_Fn016FC` and `func_actor_300700_80164794` carry.
void func_actor_521100_80134C38(Actor521100* arg0)
{
    Actor521100Work*       work;
    GsCOORDINATE2*         coord;
    Actor521100RotScratch* sc;
    s32                    ang;
    u16                    want;
    s16                    diff;
    s32                    adiff;
    s32                    step;
    s32                    cur;
    s32                    next;
    s32                    wrapStep;

    sc    = (Actor521100RotScratch*)(SCRATCH_SP -= 0x18);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_698;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_696 = ang;
    if (adiff < 0x800) {
        step = work->field_69C;
        if (step >= adiff) {
            work->field_696 = want;
        } else {
            next = (s16)work->field_696;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_696 = next;
        }
    } else {
        step = work->field_69C;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_696 = work->field_698;
        goto done;
    turn:
        wrapStep = work->field_69C;
        cur      = (s16)work->field_696;
        if (diff > 0) {
            work->field_696 = cur - wrapStep;
        } else {
            work->field_696 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_696;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}
/// Plays the actor's footstep cues: while the animation record the cue body
/// reads carries `field_3` bit 0x20 (or 0x10), a sound is queued on the frame
/// that bit has just dropped from `Actor521100Work::field_6B4`, panned and
/// depth-attenuated from the actor's display coordinate. The record is the one
/// `Gp_AnimGetRec` returns for the slot at 0x3C - the second of the 0x28-byte
/// slots the actor work blocks lay out from 0x14, the same one the other actor
/// overlays' cue bodies play from. The cue id is the `GpEnemy` work id's bits
/// 12+ placed in bits 8-11 with the overlay's 0x401C tag, 1 for the 0x20 foot
/// and 2 for the 0x10 one, and the record's two bits are latched for the next
/// frame at the end.
void func_actor_521100_80134D88(Actor521100* arg0)
{
    s32              snd;
    s32              pan;
    s32              pan2;
    Actor521100Work* work;
    GsCOORDINATE2*   coord;
    GpAnimRec*       rec;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    rec   = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->pad_0[0x3C]);
    if (rec != NULL) {
        if (!(rec->field_3 & 0x20) && (work->field_6B4 & 0x20)) {
            snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x401C0001;
            pan = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        if (!(rec->field_3 & 0x10) && (work->field_6B4 & 0x10)) {
            snd  = ((arg0->field_20->field_8 >> 12) << 8) | 0x401C0002;
            pan2 = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        work->field_6B4 = (u16)(rec->field_3 & 0x30);
    }
}

/// Aims the actor's head coordinate (`field_8[4]`) at the player. Takes that
/// coordinate's `workm` into view space, offsets the player position by 0x600
/// in Y, rotates the delta into the body's frame, clamps it to +/-0x400 yaw,
/// +/-0x300 pitch and a minimum 0x200 forward, then builds the head rotation
/// from it. Same body as `func_actor_510900_80138BF0`.
///
/// `head` is kept as its own pointer rather than indexing `coord` twice: CSE
/// folds `head->workm` back onto `coord + 0x164` while `head` stays live, which
/// is what puts the `coord += 0x140` in the clamp's branch delay slot. The
/// `+ 0x600` likewise needs the temporary, or it is sunk into the subtrahend as
/// `- 0x600` on the player coordinate.
void func_actor_521100_80134EDC(Actor521100* arg0)
{
    Actor521100AimScratch* scratch;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         head;
    s32                    offsetY;

    coord                   = arg0->field_2C->field_8;
    head                    = &coord[4];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor521100AimScratch);
    scratch                 = (Actor521100AimScratch*)*(void**)G_SCRATCH_HEAD;

    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &head->workm, &scratch->view);
    scratch->delta.vx = Wip_SysConfig.field_4->t[0] - scratch->view.t[0];
    offsetY           = scratch->view.t[1] + 0x600;
    scratch->delta.vy = Wip_SysConfig.field_4->t[1] - offsetY;
    scratch->delta.vz = Wip_SysConfig.field_4->t[2] - scratch->view.t[2];
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor521100AimScratch);
}
/// Untwists the coordinate at `field_8[3]`, which `func_actor_521100_801322F8`
/// left rotated by the random residual in `Actor521100Work::field_678` on the
/// frame the actor took a hit. The residual is turned into a matrix and
/// multiplied into that coordinate's own by `Gp_MulMatrix0`'s three `rtir`
/// passes - `rtir` multiplies the GTE rotation matrix by the vector in
/// `IR1..IR3`, so the body loads the coordinate's matrix, then each row of the
/// scratch matrix in turn, storing each result back over the coordinate. The
/// two angles are then stepped 0x20 towards zero; `field_680`, the flag the hit
/// body armed, survives while either is still moving and is cleared on the
/// frame both arrive, which is what the update body tests before calling this.
///
/// The scratch stack head is read through `Actor521100ScratchStack` rather than
/// as a `u32` - see that type for why the shape matters.
///
/// Same body as `Actor02000_Fn01698` and `func_actor_510900_80138D38`.
void func_actor_521100_80135024(Actor521100* arg0)
{
    Actor521100Work* work;
    GsCOORDINATE2*   coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    matrix                                     = (MATRIX*)(((Actor521100ScratchStack*)0x1F8003FC)->sp - 0x20);
    ((Actor521100ScratchStack*)0x1F8003FC)->sp = (u32)matrix;
    active                                     = 0;
    work                                       = arg0->field_1C;
    coord                                      = arg0->field_2C->field_8;
    RotMatrix(&work->field_678, matrix);
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
    angleX = work->field_678.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->field_678.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->field_678.vx = nextX;
            active             = 1;
        }
    }
    angleY = work->field_678.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->field_678.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->field_678.vy = nextY;
            active             = 1;
        }
    }
    if (active == 0) {
        work->field_680 = 0;
    }
    SCRATCH_SP += 0x20;
}
/// The burn-out tick `func_actor_521100_80135414` runs while the sequence state
/// `field_68C` is non-zero. `field_68E` counts the frames since the last effect
/// and fires one once it reaches `D_actor_521100_8015F8CC[field_68C]` — every 7
/// frames while the body is alight in state 1, then 0xE and 0x1C as it burns
/// down. Every effect splashes part 3 of the model's coordinate array; in state
/// 1 a second one lands on a random other part, picked out of
/// `D_actor_521100_8015F8BC` by the top three bits of an LCG draw. `field_690`
/// is the sequence's own clock, walking the state 1 -> 2 at 0xF0 frames, 2 -> 3
/// at 0x14A and 3 -> 0 at 0x1A4, where the tick stops.
void func_actor_521100_80135230(Actor521100* arg0)
{
    Actor521100Work* work;
    u16              timer;
    s16*             tbl;
    s16              part;

    work            = arg0->field_1C;
    timer           = work->field_68E + 1;
    work->field_68E = timer;
    if ((s16)timer >= D_actor_521100_8015F8CC[work->field_68C]) {
        work->field_68E = 0U;
        func_800FDB18(3, &arg0->field_2C->field_8[3], NULL, &work->eff);
        if (work->field_68C == 1) {
            tbl         = D_actor_521100_8015F8BC;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            part        = tbl[(Gp_LcgState >> 16) & 7];
            func_800FDB18(3, &arg0->field_2C->field_8[part], NULL, &work->eff);
        }
    }
    timer           = work->field_690 + 1;
    work->field_690 = timer;
    if ((s16)timer == 0xF0) {
        work->field_68C = 2;
    }
    if ((s16)work->field_690 == 0x14A) {
        work->field_68C = 3;
    }
    if ((s16)work->field_690 >= 0x1A4) {
        work->field_68C = 0;
    }
}
INCLUDE_RODATA("actors/nonmatchings/actor_521100/actor_521100", ActorsShared80135df4Table);
