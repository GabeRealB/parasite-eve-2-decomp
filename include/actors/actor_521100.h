#ifndef ACTOR_521100_H
#define ACTOR_521100_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include <psyq/inline_c.h>
#include <psyq/libgte.h>

extern u8 D_80072729;

/// Sparse view of the actor's `TmdObject` (`Actor521100::field_2C`): the two
/// pointers `TmdObject::next` / `prev` are padded over and the display object's
/// own fields are read through it. `field_8` is the model's coordinate array
/// `TmdObject::field_8` - the same `GsCOORDINATE2*` every other actor overlay
/// indexes, with the update body `func_actor_521100_80136290` reading the
/// second entry and the walk body `func_actor_521100_801358D4` the first.
typedef struct Actor521100Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor521100Obj2C;

/// A `MATRIX` plus the word-wise view `func_actor_521100_801368B0` uses to
/// splat an identity rotation before `ScaleMatrix` scales it: five aligned
/// stores instead of nine halfword ones, each word holding two adjacent
/// `m[][]` entries. The other actor overlays spell the same union
/// `ActorShared80135b58Mat`.
typedef union Actor521100ScaleMat {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} Actor521100ScaleMat;
STATIC_ASSERT_SIZEOF(Actor521100ScaleMat, 0x20);

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `func_actor_521100_801368B0`:
/// an identity `mat` scaled down by the work block's `field_488`, then
/// multiplied into the actor's attach coordinate.
typedef struct Actor521100ScaleScratch {
    /* 0x00 */ Actor521100ScaleMat mat;
    /* 0x20 */ VECTOR              scale;
} Actor521100ScaleScratch;
STATIC_ASSERT_SIZEOF(Actor521100ScaleScratch, 0x30);

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by the yaw-facing body
/// `func_actor_521100_80134C38`: only the `SVECTOR` at +0x10 is written, and it
/// is the pure-yaw rotation `RotMatrix` builds into the attach coordinate. The
/// rotation the function aims is the coordinate's own Z axis read back through
/// `ratan2`, and the scratch below it goes unused. Same shape as
/// `Actor02500RotScratch` / `Actor300700RotScratch`.
typedef struct Actor521100RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor521100RotScratch;
STATIC_ASSERT_SIZEOF(Actor521100RotScratch, 0x18);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by the aim body
/// `func_actor_521100_80134EDC`: `view` is the player-relative position
/// `Gp_WorldToLocal` produces for the head coordinate's `workm`, `delta` the
/// player position with the 0x600 head offset applied, and `local` the same
/// delta rotated into the body's frame by `ApplyTransposeMatrixLV` and then
/// clamped. Same shape as `Actor510900AimScratch`.
typedef struct Actor521100AimScratch {
    /* 0x00 */ MATRIX view;
    /* 0x20 */ VECTOR delta;
    /* 0x30 */ VECTOR local;
} Actor521100AimScratch;
STATIC_ASSERT_SIZEOF(Actor521100AimScratch, 0x40);

/// The scratch-pad stack head at 0x1F8003FC, named rather than written as a
/// plain `u32` dereference because the untwist body
/// `func_actor_521100_80135024` reads it through a member: a component access
/// is an in-struct memory reference (`MEM_IN_STRUCT_P`), which keeps the
/// scheduler from treating that load and store as aliases of every other
/// memory operation in the block. Written as a scalar, the block gains false
/// dependences, the `work` / `coord` chains lose a priority step and the
/// prologue is scheduled in the wrong order. Same shape as
/// `Actor02000ScratchStack` / `Actor510900ScratchStack`.
typedef struct {
    u32 sp;
} Actor521100ScratchStack;

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block below, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object. Same shape as the
/// other actor overlays' contexts.
typedef struct Actor521100 {
    /* 0x00 */ byte                    pad_0[0x1C];
    /* 0x1C */ struct Actor521100Work* field_1C;
    /* 0x20 */ GpEnemy*                field_20;
    /* 0x24 */ byte                    pad_24[8];
    /* 0x2C */ Actor521100Obj2C*       field_2C;
} Actor521100;

/// Per-actor work block. `Mem_Calloc(0x4B4, 0)` in `func_actor_521100_80135DDC`
/// stores the pointer both in `D_actor_521100_8016A3D8` and in the task's
/// 0x1C slot. `anim` sits at 0x40 and the slot array at 0x54, the same layout
/// as `Actor202900Work`; the nineteen slots are the ones
/// `func_actor_521100_80136724` ticks. The step / playing-id / `animId` /
/// clear-flag fields at 0x47C / 0x47E / 0x480 / 0x482 are the same layout
/// `Actor202900Work` uses.
///
/// `yaw` and `travel` are the cache the "walk to" placement opcode writes:
/// the heading it applied to the root coordinate and the remaining distance,
/// scaled by 20. Same offsets as `ActorsShared80133678Work`.
typedef struct Actor521100Work {
    /// The block opens with the two matrices the spawn body hands to
    /// `TmdObject::field_1C` / `field_20` (a `light` / `color` pair, as in the
    /// family's other work blocks), but the overlay's own step bodies view the
    /// same bytes as their animation context plus slots, so the region stays
    /// byte-addressed.
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       pad_34C[0x130];
    /* 0x47C */ s16        field_47C; // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x47E */ u16        field_47E; // animation id currently playing
    /* 0x480 */ u16        animId;    // animation id the slots are seeded with
    /* 0x482 */ s16        field_482; // cleared when a step body is started
    /* 0x484 */ s16        field_484; // scale-in step func_actor_521100_801360C4 switches on: 0 seeds, 1 shrinks, 2 is done
    /* 0x486 */ u16        field_486; // frames the shrink has run, counted to 0xA and 0xF by the step-1 body
    /* 0x488 */ u16        field_488; // scale the shrink applies, stepped down by 0x10 per frame from 0x1000
    /* 0x48A */ byte       pad_48A[2];
    /* 0x48C */ MATRIX     field_48C; // attach rotation the step-0 body snapshots and the step-1 body scales
    /* 0x4AC */ byte       pad_4AC[2];
    /* 0x4AE */ u16        yaw;
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        travel;
    /* 0x4B4 */ byte       pad_4B4[0xC8];
    /// The two collision nodes the burn-out sequence arms, the pair
    /// `Actor510900Work`'s `obj4E4` / `obj504` carry. `ActorsShared80131e24Sub0`
    /// fills both - the two pointers, the three halfwords at 0x10 and
    /// `field_18` / `field_1C` - and links them. The state bodies then raise
    /// `flags` bit 0x8000 on the frame their effect fires, hand both back with
    /// an `&= 0x7FFF` when the sequence advances, and take the word
    /// `Gp_PackPair` returns into `field_18`.
    /* 0x57C */ GpObj obj57C;
    /// See `obj57C`.
    /* 0x59C */ GpObj obj59C;
    /* 0x5BC */ byte  pad_5BC[0x88];
    /// `func_800FDB18` argument record `func_actor_521100_80135230` refreshes
    /// on the effect frames of the burn-out sequence.
    /* 0x644 */ GpEffArg     eff;
    /* 0x64C */ s16          field_64C; // the attach coordinate's translation, snapshotted each frame
    /* 0x64E */ s16          field_64E;
    /* 0x650 */ s16          field_650;
    /* 0x652 */ byte         pad_652[2];
    /* 0x654 */ Actor521100* field_654;
    /* 0x658 */ byte         pad_658[0x20];
    /// Residual twist of the coordinate at `field_8[3]`, two angles of the
    /// +/-(0x40..0xBF) range the hit body `func_actor_521100_801322F8` draws
    /// from `Gp_LcgState` on the frame it takes a hit. It writes them here and
    /// arms `field_680`; the untwist body `func_actor_521100_80135024` then
    /// rotates that coordinate's matrix back by them, stepping each angle 0x20
    /// towards zero per frame until both arrive and it clears the flag. Same
    /// pair as `Actor510900Work::field_570` / `field_584` and
    /// `Actor02000Work::field_688` / `field_6B4`.
    /* 0x678 */ SVECTOR field_678;
    /* 0x680 */ s16     field_680;
    /* 0x682 */ s16     field_682; // non-zero while the tick in func_actor_521100_80135B80 remaps the model's field_C
    /* 0x684 */ byte    pad_684[2];
    /// The clip the slots are blended to and the clip they currently carry.
    /// The preset handler `func_actor_521100_80135C14` stores one clip id into
    /// both, so the blend is skipped; `func_actor_521100_80135964` later walks
    /// every slot towards `field_686` while the two differ, then ticks them
    /// once they agree. `field_68A` counts the ticks, and is cleared when a new
    /// clip is latched into `field_688`.
    /* 0x686 */ s16 field_686;
    /* 0x688 */ s16 field_688;
    /* 0x68A */ u16 field_68A;
    /* 0x68C */ s16 field_68C;
    /* 0x68E */ s16 field_68E;
    /* 0x690 */ s16 field_690;
    /* 0x692 */ s16 field_692;
    /* 0x694 */ s16 field_694;
    /// 12-bit angles. The step-1 entry body `func_actor_521100_80135680`
    /// subtracts them, wraps the difference into [-0x800, 0x800] and reads
    /// `field_6AA` when the result is under 0x200.
    /* 0x696 */ u16  field_696;
    /* 0x698 */ u16  field_698;
    /* 0x69A */ s16  field_69A; // forward speed, in 12-bit fixed point
    /* 0x69C */ s16  field_69C; // cleared together with the forward speed
    /* 0x69E */ s16  field_69E;
    /* 0x6A0 */ s16  field_6A0;
    /* 0x6A2 */ s16  field_6A2;
    /* 0x6A4 */ byte pad_6A4[2];
    /// Parked animation the burn-out body `func_actor_521100_80133104` clears
    /// on its own frame, the same slot `actor_102000` and `actor_105700` park
    /// into.
    /* 0x6A6 */ s16  field_6A6;
    /* 0x6A8 */ s16  field_6A8;
    /* 0x6AA */ s16  field_6AA;
    /* 0x6AC */ byte pad_6AC[2];
    /// Armed by `func_actor_521100_80133104` on the frame the burn-out sound
    /// fires and cleared again when the sequence advances.
    /* 0x6AE */ s16 field_6AE;
    /* 0x6B0 */ s16 field_6B0;
    /* 0x6B2 */ s16 field_6B2;
    /// The animation record's flag nibble (`rec->field_3 & 0x30`) latched for
    /// the next frame by the footstep cue body `func_actor_521100_80134D88`, so
    /// each foot fires on the frame its bit has just dropped.
    /* 0x6B4 */ u16  field_6B4;
    /* 0x6B6 */ byte pad_6B6[4];
    /// Non-zero asks the burn-out bodies to hand the actor on to state 6
    /// (`field_69E = 6`) instead of back to the idle state 0; `field_6BC` is
    /// the sub-state they then start at.
    /* 0x6BA */ s16 field_6BA;
    /* 0x6BC */ u16 field_6BC;
    /* 0x6BE */ s16 field_6BE;
} Actor521100Work;
STATIC_ASSERT_SIZEOF(Actor521100Work, 0x6C0);

/// Payload of the 0x3F8 query `func_actor_521100_80132C70` sends the player
/// before it takes the hold; `field_14` is the range it asks for. The same
/// shape as `Actor103700Msg3F8`, `Actor510900Msg3F8` and `Actor400600Msg3F8`.
typedef struct Actor521100Msg3F8 {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
} Actor521100Msg3F8;
STATIC_ASSERT_SIZEOF(Actor521100Msg3F8, 0x18);

/// 4-byte message record; the handler switches on the halfword at 0x2,
/// the same shape as `Actor361100Msg`.
typedef struct Actor521100Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor521100Msg;
STATIC_ASSERT_SIZEOF(Actor521100Msg, 0x4);

typedef struct Actor521100Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor521100Ctx;

/// Argument block of the "walk to" script opcode: the world position to walk
/// to. Only the horizontal components are read.
typedef struct Actor521100Target {
    /* 0x00 */ VECTOR pos;
} Actor521100Target;
STATIC_ASSERT_SIZEOF(Actor521100Target, 0x10);

/// Argument block of the message handler `func_actor_521100_801369B8`
/// implements: which animation to start. Same 4-byte-id prefix as
/// `Actor202900AnimArgs`, and the same `(u16)` narrowing on the store into the
/// work block's `animId`. The stored id is `args->animId + 1`.
typedef struct Actor521100AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
} Actor521100AnimArgs;

/// Argument block of the "start animation" script opcode 0x7D3, whose handler
/// is `func_actor_521100_80135C14`. `field_0` is the animation bank and picks
/// the clip the bank starts at (`0` maps to 0x14, every other bank to 0x1D);
/// `field_4` is the offset of the clip inside that bank; `field_8`, when
/// non-zero, blends to it over `field_C` frames instead of snapping. Same
/// four-word shape as `Actor361100AnimPreset` and `Actor503500AnimPreset`.
typedef struct Actor521100AnimPreset {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ u16  field_4;
    /* 0x06 */ byte pad_6[2];
    /* 0x08 */ s32  field_8;
    /* 0x0C */ s32  field_C;
} Actor521100AnimPreset;
STATIC_ASSERT_SIZEOF(Actor521100AnimPreset, 0x10);

extern Actor521100Work* D_actor_521100_8016A3D8;

/// The game-wide 32-bit LCG. `func_actor_521100_80136290` draws it three times
/// in a row, scaling the second coordinate's world position by `field_488`
/// through the top half of each draw.
extern u32 Gp_LcgState;

/// Blend length in frames `func_actor_521100_80135964` seeds the slot walk
/// with when the clip changes, indexed by the incoming clip id; ids from 0x15
/// up keep the zero `val` starts at.
extern s16 D_actor_521100_8015F894[];

/// The three waypoints the state-6 body `func_actor_521100_80134774` walks the
/// actor to, one per phase `field_6A0` it switches on: `(-4000, 0, -2000)` for
/// phases 0 and 2, and `(-5250, 0, -1200)` for phase 1. Only `vx` and `vz` are
/// read, and only when the actor is too far from the player for that phase to
/// aim at it; the y of all three is zero, as the positions are on the floor.
extern VECTOR D_actor_521100_8015F654[];

/// Sixteen frames of the burn-out effect the state bodies at `field_6A0 == 1`
/// pick between on their last frame, indexed by the 4 bits under the top half
/// of an LCG draw (`(rng >> 16) & 0xF`). The sibling state body
/// `func_actor_521100_801357F0` reads the table one slot down at 0x8015F5F4.
extern u16 D_actor_521100_8015F634[];

/// The other sixteen-frame burn-out table, read by the state-5 body
/// `func_actor_521100_801357F0` off the same LCG draw bits the state-3 body
/// `func_actor_521100_8013570C` indexes `D_actor_521100_8015F634` with.
extern u16 D_actor_521100_8015F5F4[];

/// The burn-out effect table the sequence resets read, one 0x20-byte table
/// below `D_actor_521100_8015F5F4`: the state-1 body
/// `func_actor_521100_801335B4` draws from it both on the frame the actor
/// catches alight and on the frame the reset latch `field_6AA` has run out.
extern u16 D_actor_521100_8015F5D4[];

/// The 4-byte pair `func_actor_521100_801335B4` packs a type-2 record into and
/// copies onto both `obj57C` / `obj59C` at `field_18`, where the sibling
/// overlays' burn-out bodies put the same pair. Same shape as
/// `D_actor_510900_80167968` and `D_actor_400100_*`.
extern GpU16Pair D_actor_521100_8015F550;

/// Frames between the burn-out effects `func_actor_521100_80135230` drops on
/// the attach coordinate, indexed by the sequence state `field_68C`: every
/// frame in state 0, then 7 / 0xE / 0x1C as the body burns out.
extern s16 D_actor_521100_8015F8CC[];

/// Coordinate slots the burn-out effects splash across when the sequence is in
/// state 1, chosen by the top 3 bits of an LCG draw.
extern s16 D_actor_521100_8015F8BC[];

/// State table the overlay dispatches through, indexed by `Task::state`.
/// `D_actor_521100_80131E68` is its 3 words: create
/// (`func_actor_521100_80135DDC`), update (`func_actor_521100_80136680`)
/// and teardown (`func_actor_521100_801360C4`). Both handlers take the
/// task's 0x20 spawn argument first, like `Actor210600StateFuncTable3`.
typedef void (*Actor521100StateFunc)(void* spawnArg2, Task* task);

typedef struct Actor521100StateFuncTable3 {
    Actor521100StateFunc funcs[3];
} Actor521100StateFuncTable3;
STATIC_ASSERT_SIZEOF(Actor521100StateFuncTable3, 0xC);

extern const Actor521100StateFuncTable3 D_actor_521100_80131E68;

/// Stack copy `func_actor_521100_80136604` makes before the indirect call.
/// The copy itself moves only the 3 words of `D_actor_521100_80131E68`, but
/// the dispatcher's frame is 0x30 with `$ra` at 0x28, which needs 17-24 bytes
/// of locals. The trailing `u8`/`u8`/`u16` at 0x10 are written to 2, 9, 1;
/// `field_C` is unread. Same 20-byte table-plus-context shape as
/// `Actor210600DispatchCtx`.
typedef struct Actor521100DispatchCtx {
    /* 0x00 */ Actor521100StateFuncTable3 table;
    /* 0x0C */ s32                        field_C;
    /* 0x10 */ u8                         field_10;
    /* 0x11 */ u8                         field_11;
    /* 0x12 */ u16                        field_12;
} Actor521100DispatchCtx;
STATIC_ASSERT_SIZEOF(Actor521100DispatchCtx, 0x14);

/// Steps `coord` `amount` units along its local Z axis unless movement is
/// frozen. The walk-to cache `Actor521100Work::yaw` / `travel` scales by 20,
/// which is the step `func_actor_521100_80135F2C` hands this body. Same body
/// as `Actor01900_MoveForward`, `Actor01900_StepForward` and
/// `Actor00100_MoveForward`, which take the same 8-byte SVECTOR off
/// `G_SCRATCH_HEAD`.
///
/// The inlining is load bearing: inside an inlined body the compiler folds the
/// constant `G_SCRATCH_HEAD` address straight into each memory operand
/// (`lui $s1,%hi` + `lw $s1,%lo($s1)`, `lui $at,%hi` + `sw ...%lo($at)`),
/// where a plain call site materialises the address once with `lui`/`ori` into
/// a register and reuses it across the calls. Same C, same compiler, different
/// instructions - do not lift the body into the caller.
static __inline__ void Actor521100_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_521100_80135414(Actor521100Ctx* arg0, Actor521100* arg1);
void func_actor_521100_80135478(Actor521100Ctx* arg0, Actor521100* arg1);
void func_actor_521100_801355C8(Actor521100* arg0);
void func_actor_521100_80135F2C(Task* task);
void func_actor_521100_80136724(void);
void func_actor_521100_8013677C(void);
void func_actor_521100_80136820(void);
s32  func_actor_521100_801369B8(Task* task, s32 arg1, Actor521100AnimArgs* args);

/// Message 0x7DB handler, listed in `D_actor_521100_8016A358` -- the
/// `{id, handler}` table the create body `func_actor_521100_80135DDC` installs
/// at `Task::field_24`. `msg->field_2` picks the sub-command: 0 puts the task
/// back on its update state; 1 and 4 spawn one of the two companion tasks out
/// of the `D_actor_521100_8016A388` desc table into `D_actor_521100_8016A3E0` /
/// `D_actor_521100_8016A3E4`, leaving the state alone; 2 clears the scale-in
/// step (`Actor521100Work::field_484`) and sends the task to state 2, the
/// teardown entry `func_actor_521100_801360C4`; 3 kills both companions and
/// then falls into 0, sharing its state store.
s32 func_actor_521100_80136AE0(Task* task, s32 arg1, Actor521100Msg* msg);

s32 func_actor_521100_80136BE8(Task* task, s32 arg1, Actor521100Target* target);

#endif
