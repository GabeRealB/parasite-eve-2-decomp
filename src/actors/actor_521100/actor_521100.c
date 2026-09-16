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
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80132C70);
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
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_8013334C);
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
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80134658);
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
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80134C38);
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
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80134EDC);
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
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80135024);
/// The burn-out tick `func_actor_521100_80135414` runs while the sequence state
/// `field_68C` is non-zero. `field_68E` counts the frames since the last effect
/// and fires one once it reaches `D_actor_521100_8015F8CC[field_68C]` — every 7
/// frames while the body is alight in state 1, then 0xE and 0x1C as it burns
/// down. Every effect splashes part 3 of the model's coordinate array; in state
/// 1 a second one lands on a random other part, picked out of
/// `D_actor_521100_8015F8BC` by the top three bits of an LCG draw. `field_690`
/// is the sequence's own clock, walking the state 1 -> 2 at 0xF0 frames, 2 -> 3
/// at 0x14A and 3 -> 0 at 0x1A4, where the tick stops.
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80135230);
INCLUDE_RODATA("actors/nonmatchings/actor_521100/actor_521100", ActorsShared80135df4Table);
