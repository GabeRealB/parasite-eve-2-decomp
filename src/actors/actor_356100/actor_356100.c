#include "common.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// One XZ pair of `Actor356100Work::field_C`; same shape as
/// `Actor01900Waypoint`.
typedef struct Actor356100Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor356100Waypoint;

/// 8-byte row of the `D_actor_356100_8016A994` table `func_actor_356100_8016382C`
/// picks the re-entry pair from on the spawn argument; the same role
/// `Actor01900TintRow` has for `Actor01900_D0AC64`.
typedef struct Actor356100TintRow {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} Actor356100TintRow;
STATIC_ASSERT_SIZEOF(Actor356100TintRow, 0x8);

/// Head of the work block this overlay hangs off `Task::work`. `field_4` is
/// the live-actor flag `func_actor_356100_8016A1D8` tests, where
/// `Actor00100Work::field_4` sits. `field_0` / `field_5A` / `field_68` are the
/// same state, clip-id and flag halfwords `Actor01900Work` keeps at those
/// offsets; `field_974` is the `field_5A & 0x3FF` snapshot
/// `func_actor_356100_8016A468` stores (same role as `Actor01900Work.field_8B4`).
/// The halfwords at 0x978..0x982 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes at 0x898..0x8A2; `field_984` is the halfword
/// `func_actor_356100_8016A5DC` snapshots into `field_982`, where
/// `Actor01900Work.field_8A4` sits, and `field_98E` / `field_990` the zero-pair
/// that function clears at +0x8AE / +0x8B0. `field_9BC` holds the 0x180 it
/// writes; `Actor01900_Fn0AA78` puts the same constant in the neighbouring
/// `GpObj` field it names `field_8C8.field_1C` (+0x8E4), not this one.
/// `field_B5C` / `field_B60` are the two helper tasks the exit callback
/// kills; same pair as `Actor01900Work` at +0xC38 / +0xC3C, without the three
/// `GpObj` nodes that teardown unlinks.
typedef struct Actor356100Work {
    /* 0x000 */ s16 field_0;
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// Countdown `func_actor_356100_8016A668` decrements every frame and
    /// tests with `(s16)` — the 0x0F / 0x10 state it picks when the counter
    /// wraps is the transition into the state 0xB / 0xC clip it is running.
    /// Signed, like `Actor01900Work.field_6` / `Actor401300Work.field_6`;
    /// `func_actor_356100_80167584` reads the same slot as a `u16` when it
    /// increments it, so it casts there.
    /* 0x006 */ s16 field_6;
    /// Latch `func_actor_356100_80164158` raises together with the `field_97E = 9`
    /// reseed once the wrapped turn from the actor's own facing to the player's
    /// exceeds 0x200, and tests before raising so the state change fires once.
    /// Cleared on the state-7 entry next to `field_6`; same slot as
    /// `Actor01900Work.field_8`.
    /* 0x008 */ s16 field_8;
    /* 0x00A */ s16 field_A;
    /// The two XZ pairs `func_actor_356100_8016382C` seeds on entering state
    /// 0x10: the model root's X/Z, then the same pair pushed one normalised
    /// unit along the facing. Same role as `Actor01900Work.field_C`.
    /* 0x00C */ Actor356100Waypoint field_C[2];
    /// Zeroed on the same entry, next to the pair above; same slot as
    /// `Actor01900Work.field_14`.
    /* 0x014 */ s16 field_14;
    /// Heading the placement handler derives from the root coordinate's
    /// Z axis (`ratan2` of its X and Z terms) once the spawn rotations are in.
    /* 0x016 */ s16  yaw;
    /* 0x018 */ byte pad_18[0x42];
    /* 0x05A */ u16  field_5A;
    /* 0x05C */ byte pad_5C[0xC];
    /* 0x068 */ u16  field_68;
    /* 0x06A */ byte pad_6A[0x90A];
    /* 0x974 */ s32  field_974;
    /* 0x978 */ s16  field_978;
    /* 0x97A */ s16  field_97A;
    /* 0x97C */ byte pad_97C[2];
    /* 0x97E */ s16  field_97E;
    /* 0x980 */ byte pad_980[2];
    /* 0x982 */ s16  field_982;
    /* 0x984 */ s16  field_984;
    /* 0x986 */ byte pad_986[8];
    /* 0x98E */ s16  field_98E;
    /* 0x990 */ s16  field_990;
    /* 0x992 */ byte pad_992[2];
    /// Snapshot of `field_5A & 0x3FF` `func_actor_356100_80167818` tests
    /// before re-storing it, so the clip-4 branch fires once per change.
    /// Distinct from `field_974`, which the state 0xE tick snapshots.
    /* 0x994 */ s32  field_994;
    /* 0x998 */ byte pad_998[0x24];
    /* 0x9BC */ s16  field_9BC;
    /* 0x9BE */ byte pad_9BE[2];
    /// First `GpRec18` of the body-part record table, the address
    /// `func_actor_356100_8016382C` publishes in the enemy's `recs` slot
    /// and the exit callback drops. Same slot as `Actor01900Work.field_8E8`.
    /* 0x9C0 */ GpRec18 field_9C0;
    /* 0x9D8 */ byte    pad_9D8[0x80];
    /// Collision record `func_actor_356100_801668FC` hands `func_800E0C10`,
    /// 0x98 past `field_9C0` — one body-part record rather than one `GpRec18`,
    /// the stride `Actor00100Obj` gives the same table in the 00100 overlay.
    /* 0xA58 */ GpRec18 field_A58;
    /* 0xA70 */ byte    pad_A70[0x68];
    /// Light matrix `func_actor_356100_8016382C` binds to the model's
    /// `TmdObject::lightMtx` (the color matrix is `field_AF8`).
    /* 0xAD8 */ MATRIX field_AD8;
    /// Color matrix bound to the model's `TmdObject::colorMtx`.
    /* 0xAF8 */ MATRIX field_AF8;
    /// Copy `func_actor_356100_80167584` saves `field_AF8` into when it enters
    /// its state; same role as `Actor401300Work.field_C68` has for `field_C48`.
    /* 0xB18 */ MATRIX field_B18;
    /* 0xB38 */ byte   pad_B38[2];
    /// Threshold `func_actor_356100_8016A834` tests once the enemy is still
    /// alive (`field_40 > 0`) to choose clip 4 or 0x11; the analogue of the
    /// `Actor00100Ctx.field_4C & 2` bit `Actor00100_Fn0BB2C` tests there.
    /* 0xB3A */ s16  field_B3A;
    /* 0xB3C */ byte pad_B3C[4];
    /// Direction `Gfx_MatrixCol2` takes off the model's root colour-matrix Y
    /// column and `VectorNormalSS` normalises; `func_actor_356100_80165B30`
    /// then GPF-scales it by `field_B52` into its aim scratch, the same
    /// normalise-then-scale pair `Actor01900_MoveForward` runs.
    /* 0xB40 */ SVECTOR field_B40;
    /// The yaw pair `func_actor_356100_801653F4` walks: `field_B48` is the
    /// angle fed to `Gfx_RotMatrixY` and slewed 0x89 at a time toward
    /// `field_B4A`, the target it re-derives each entry as the root's facing
    /// plus twice the wrapped turn. Same slots as `Actor01900Work.field_C20` /
    /// `field_C22`.
    /* 0xB48 */ s16 field_B48;
    /* 0xB4A */ s16 field_B4A;
    /// Push distance `func_actor_356100_8016804C` normalises the root's own
    /// colour-matrix column by and walks down by 0xA per frame until the
    /// state moves on; same role as `Actor401300Work.field_C98`.
    /* 0xB4C */ s16 field_B4C;
    /// Clip-phase `func_actor_356100_80164ACC` walks 8 -> -1 -> 0 against
    /// `field_982` (at 0x18 and 0x12); same slot as `Actor01900Work.field_C26`.
    /* 0xB4E */ s16 field_B4E;
    /// Side `func_actor_356100_80165B30`'s turn settles on: the zero it draws
    /// once from `Gp_LcgState` at entry, then -1 / 1 for the two clip directions
    /// it alternates each time it re-enters with the same clip.
    /* 0xB50 */ s16 field_B50;
    /// Uniform GTE scale `func_actor_356100_80165B30` applies to the normalised
    /// direction: 0xDE whole while `field_97A` is clear, halved when it is set.
    /* 0xB52 */ u16 field_B52;
    /// Random reload `func_actor_356100_8016A668` adds a 4-bit `Gp_LcgState`
    /// draw to when the work block's `field_4` flag is set.
    /* 0xB54 */ u16 field_B54;
    /// Second half of the pair: the re-entry delay `func_actor_356100_8016382C`
    /// copies off `D_actor_356100_8016A994` next to `field_B54`.
    /* 0xB56 */ u16 field_B56;
    /// Copy of the first three bytes of the last event
    /// `func_actor_356100_8016A0B8` handled.
    /* 0xB58 */ u8    field_B58[3];
    /* 0xB5B */ byte  pad_B5B;
    /* 0xB5C */ Task* field_B5C;
    /* 0xB60 */ Task* field_B60;
    /// Halfword `func_actor_356100_80166CF0` clears after its state entry has
    /// run, next to the `field_B66` / `field_B68` latch pair the state-4 tick
    /// clears; same slot as `Actor401300Work.field_D1C`.
    /* 0xB64 */ s16 field_B64;
    /// Cleared next to `field_B68` on the state-4 entry; same slot as
    /// `Actor401300Work.field_D1E`.
    /* 0xB66 */ s16 field_B66;
    /// Latch `func_actor_356100_801668FC` and `func_actor_356100_80166018`
    /// clear after dispatching message 0x3F1, and set so that the next tick
    /// dispatches it once.
    /* 0xB68 */ s16  field_B68;
    /* 0xB6A */ byte pad_B6A[2];
    /// Ring buffer of the model root's world positions, one slot per frame:
    /// `func_actor_356100_80169854` writes the transformed coordinate there at
    /// the `field_BBC` index and reads the slot back as the enemy's local
    /// position unless `field_97E` is 0x14 or 0x15.
    /* 0xB6C */ SVECTOR field_B6C[7];
    /* 0xBA4 */ byte    pad_BA4[0x18];
    /// Write index into the `field_B6C` ring above; `func_actor_356100_8016382C`
    /// zeroes it on the state-0x10 entry and the per-frame tick wraps it at 7.
    /// Same tail slot as `Actor01900Work.field_C98`.
    /* 0xBBC */ s16  field_BBC;
    /* 0xBBE */ byte pad_BBE[2];
} Actor356100Work;
STATIC_ASSERT_SIZEOF(Actor356100Work, 0xBC0);

/// Event record `func_actor_356100_8016A0B8` dispatches on: the first three
/// bytes are copied raw into `Actor356100Work::field_B58`, `w[0]` is the
/// event kind and `w[1]` its sub-code. Same shape as `Actor401300Event`.
typedef union Actor356100Event {
    u8  b[3];
    u16 w[2];
} Actor356100Event;

/// Binds the model's light and colour matrices to the pair kept in the work
/// block. Same body as `Actor01900_BindMatrices`.
static __inline__ void Actor356100_BindMatrices(Task* actor)
{
    Actor356100Work* work;
    TmdObject*       obj;

    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->field_AD8;
    obj->colorMtx = &work->field_AF8;
}

/// Animation view of the work block above, as `func_actor_356100_801633DC`
/// reads it: the `Actor01900AnimWork` layout 0xE0 bytes later, so the two
/// `GpAnimCtx` blocks at 0x1C / 0x4C8 each sit 0x14 bytes before their
/// 0x28-byte slot array. `field_982` is the clip id the slot loop copies
/// minus 3 into `slots[i].field_9`, `field_98A` the clip written whole into
/// `blendSlots[i].field_9`, and `field_98C` the blend weight, the same three
/// roles `Actor01900AnimWork.field_8A2` / `field_8AA` / `field_8AC` have.
typedef struct Actor356100AnimWork {
    /* 0x000 */ byte       pad_0[0x1C];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[24];
    /* 0x3F0 */ byte       pad_3F0[0xD8];
    /* 0x4C8 */ GpAnimCtx  blendAnim;
    /* 0x4DC */ GpAnimSlot blendSlots[24];
    /* 0x89C */ byte       pad_89C[0xDC];
    /// The animation-state halfwords `func_actor_356100_80163508` drives, the
    /// same slots `Actor356100Work` names: `field_978` is the state it leaves
    /// at 3 once the 1/2 entry has been served, `field_97C` the clip id the
    /// slots were last seeded with, `field_97E` the one being seeded now and
    /// `field_980` the per-frame counter it bumps. The 0x97C / 0x97E pair is
    /// what makes the state-1 reseed fire once per clip change.
    /* 0x978 */ s16 field_978;
    /* 0x97A */ s16 field_97A;
    /* 0x97C */ s16 field_97C;
    /* 0x97E */ s16 field_97E;
    /* 0x980 */ u16 field_980;
    /* 0x982 */ s16 field_982;
    /* 0x984 */ s16 field_984;
    /// Blend-context state `func_actor_356100_80163508` restarts at 2 with
    /// speed 0x30 / weight 0x800: `field_986` the state, `field_988` the clip
    /// id its slots are reset to, and `field_98A` / `field_98C` the pair the
    /// reset copies into the primary slots and the blend weight.
    /* 0x986 */ s16 field_986;
    /* 0x988 */ s16 field_988;
    /* 0x98A */ s16 field_98A;
    /* 0x98C */ s16 field_98C;
    /// The eased yaw pair `func_actor_356100_80163508` walks: `field_98E` is
    /// the aim the root coordinates are turned to and `field_990` the running
    /// value moved toward it by 0x100 per frame.
    /* 0x98E */ s16  field_98E;
    /* 0x990 */ s16  field_990;
    /* 0x992 */ byte pad_992[2];
    /* 0x994 */ s32  field_994;
} Actor356100AnimWork;

/// Blends pose slots 1..0x14: the first eleven copy the two clip ids into
/// their slot records and are written from both animation contexts with
/// `0x1000 - field_98C` as the blend weight, the rest only tick. Same body as
/// `Actor01900_Fn01950` / `func_actor_403000_801336B4` with this overlay's
/// slot count.
void func_actor_356100_801633DC(Task* arg0);

/// Reseeds changed clips, ticks or blends their slots, and eases the two
/// upper-body coordinates toward the requested yaw.
void func_actor_356100_80163508(Task* arg0);

/// Per-clip transition values indexed by the current and requested clip.
extern s8 D_actor_356100_801728CC[][45];

/// Player-to-`coord` vector, in the 16-bit `SVECTOR` view of both matrices.
static __inline__ void Actor356100_PositionDelta(GpCoord* coord, SVECTOR* pos)
{
    pos->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    pos->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    pos->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
}

/// 0x10-byte `G_SCRATCH_HEAD` block `func_actor_356100_80168E44` takes: the
/// offset from the actor to the player, then the facing yaw it settles on.
typedef struct Actor356100AimScratch {
    /* 0x0 */ SVECTOR delta;
    /// The two facing yaws `func_actor_356100_80164158` compares: `target` is the
    /// player's root facing (`ratan2` of its matrix column) and `current` this
    /// actor's own, re-derived from `delta` and wrapped 0x800 off it. Their
    /// wrapped difference is the turn onto the player, which is what separates
    /// the state-0xA and state-0xB transitions.
    /* 0x8 */ s16 target;
    /* 0xA */ s16 current;
    /* 0xC */ s16 angle;
    /// The root's facing yaw `func_actor_356100_801653F4` reads back to seed
    /// `Actor356100Work::field_B48`; `ActorChaseScratch` names the same
    /// pair `turn` / `angle`.
    /* 0xE */ s16 facing;
} Actor356100AimScratch;
STATIC_ASSERT_SIZEOF(Actor356100AimScratch, 0x10);

/// 0x68-byte `G_SCRATCH_HEAD` block `func_actor_356100_80169854` takes while it
/// builds the ground coordinate it draws an effect quad on: the coordinate the
/// function fills (`coord.sub` parented to `gGfxViewCoord`) plus the world
/// position `v` its two parent walks leave there.
typedef struct Actor356100GroundCoord {
    /* 0x00 */ GpCoord coord;
    /* 0x50 */ byte    pad_50[0x10];
    /* 0x60 */ SVECTOR v;
} Actor356100GroundCoord;
STATIC_ASSERT_SIZEOF(Actor356100GroundCoord, 0x68);

/// The overlay's only message-0x3E9 instance; all eight words are zero in the
/// image, so it is a work area rather than a table.
extern GpXformArg D_actor_356100_801732B0;

/// Reply buffer for the message-0x3F8 query above; the six words after it are
/// zero in the image.
extern GpDelayArg D_actor_356100_801732D0;

/// Player-character flag selecting which animation block
/// `func_actor_356100_80166018` points `D_actor_356100_80173244.field_0` at:
/// the second block when it is 1, the first otherwise.
extern s32 D_actor_356100_80173228;
extern s32 D_actor_356100_80173230;

/// `s8` selector for those two blocks; same slot and role as
/// `Actor403100AnimTable`'s base index.
extern s8 D_8007218A;

/// Zeroed word `func_actor_356100_80167818` clears when the actor goes live.
/// The 0x74 bytes after it are zero in the image too, so the whole run is a
/// work area rather than a table.
extern s32 D_actor_356100_801731B0;

/// Free-running scroll `func_actor_356100_80164ACC` accumulates `field_B4C`
/// into each frame, and zeroes on the live-actor entry. Same role as
/// `Actor01900_D172FC`.
extern u16 D_actor_356100_80173290;

/// Whole-unit part of the last movement step `func_actor_356100_80162AEC`
/// applied, rounded away from zero when the step had a fraction.
extern SVECTOR D_actor_356100_801732A0;

/// Effect record `func_actor_356100_80167818` fills for `func_800FDB18`:
/// coordinate index 5 of the model, scale 0x100 and count 2. Same shape and
/// roles as `Actor401300Work.field_910`.
extern GpEffArg D_actor_356100_801732A8;

/// Animation bank `func_800B3F84` seeds both of the work block's slot arrays
/// from. Same role as `Actor01900_D17174`.
extern u8 D_actor_356100_801730B8[];

/// Enemy descriptor `func_actor_356100_8016382C` publishes in the enemy's
/// `field_50` slot and takes `field_40` off. Same role as `Actor01900_D0AC54`.
extern GpPairSrcE D_actor_356100_8016A984;

/// The three rows `func_actor_356100_8016382C` picks its re-entry pair from on
/// the spawn sub-type. Same role as `Actor01900_D0AC64`.
extern Actor356100TintRow D_actor_356100_8016A994[];

/// Event-handler table `func_actor_356100_8016382C` hands the task as
/// `Task::msgTable`. Same shape and role as `Actor01900_D1728C`.
extern void* D_actor_356100_80173258;

/// Initialisation for the state-0x10 clip run: allocates the work block, binds
/// the light / colour matrices, re-seeds the enemy descriptor and both
/// animation contexts, copies the model root's XZ pair into the work block and
/// rebuilds the root's Y rotation as a uniform 0x1194 scale. The spawn
/// sub-type picks the clip and re-entry pair, and the finished entry advances
/// the state.
void func_actor_356100_8016382C(GpEnemy* enemy, Task* actor);

/// Separation tick: when the work block's `field_4` flag is set, pushes this
/// actor one normalised unit away from the player along the player-to-actor
/// direction in XZ (recentring it on the player first), clears the model's
/// root `flg`, and sends the player message 0x3E9 with its own position and
/// the resulting heading. Bit 0 of `field_68` then forces `field_0` to 0xD.
void func_actor_356100_801666B4(Task* arg0);

/// Approach tick, and the sibling of `func_actor_356100_80167584` above it. Going
/// live clears `D_actor_356100_801731B0` and re-seeds the animation slots at
/// clip 2 / speed 0x10 with the enemy's link node cleared; otherwise a single
/// sound 0x51030008 is queued the first time through, keyed on the enemy's
/// `field_8 >> 12` bank. Each frame then snapshots `field_5A & 0x3FF` into
/// `field_994`, and the frame that first lands on clip 4 spawns the
/// `D_actor_356100_801732A8` effect at model coordinate 5. Once the player is
/// further than 3000 away it plays 0x51030008 as a type-7 event and enters
/// state 6. Same shape as `func_actor_401300_801397F8`.
void func_actor_356100_80167818(Task* arg0);

/// Event handler: copies the event's first three bytes into the work block's
/// `field_B58`, then dispatches on `w[0] == 0xB05` and `w[1]` — sub-code 1
/// puts the actor in state 0x1E, 0 and 2 in state 0. Anything else returns 0.
s32 func_actor_356100_8016A0B8(Task* arg0, s32 arg1, Actor356100Event* arg2);

/// `Task::exitCallback` teardown: kill the two helper tasks, drop the
/// enemy's `recs` slot, then `Gp_DestroyEnemy`. Same shape as
/// `Actor01900_Fn0A6CC` without the three `Gp_UnlinkObj` calls.
void func_actor_356100_8016A158(Task* task);

/// When the work block's `field_4` flag is set, flags the enemy's link node
/// and raises bit 0x80 of the model's `field_C`. Same shape as
/// `ActorsShared80164c20` / `Actor00100_Fn0B4D8` without extra flag masks.
void func_actor_356100_8016A1D8(Task* arg0);

/// When the work block's `field_4` flag is set, clears the enemy's link node,
/// reallocates the model buffers and writes the 0x978..0x982 animation
/// slots; otherwise clears the model's root `flg`. Same shape as
/// `Actor01900_Fn0A7C0` without the two `GpObj` flag masks.
void func_actor_356100_8016A21C(Task* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 3`.
void func_actor_356100_8016A2AC(Task* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 0xB`.
void func_actor_356100_8016A340(Task* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 0xB`.
void func_actor_356100_8016A3D4(Task* arg0);

/// Message 0x3FF payload `func_actor_356100_8016A468` sends the slot-3 task.
/// `field_0` points at `D_actor_356100_80173228`; the function overwrites
/// `field_4` with 2 before the dispatch.
extern GpAnimArg D_actor_356100_80173244;

/// When the work block's `field_4` flag is set, writes the 0x978..0x982
/// animation slots, sends message 0x3FF then 0x3F9 at slot 3, and snapshots
/// `field_5A & 0x3FF` into `field_974`. Bit 1 of `field_68` forces `field_0`
/// to 0xE.
void func_actor_356100_8016A468(Task* arg0);

/// When the work block's `field_4` flag is set, clears the model's `field_C`,
/// clears the enemy's link node and writes the 0x978..0x982 animation slots
/// with `field_9BC` forced to 0x180. Bit 0 of `field_68` forces `field_0` to 7.
/// Same shape as `Actor01900_Fn0AA78` without its two `GpObj` flag masks.
void func_actor_356100_8016A5DC(Task* arg0);

/// Per-frame tick run under the death-throes clip 0xB / 0xC pair.
void func_actor_356100_80163CD4(Task* arg0);

/// Per-frame tick of the state-6 clip run.
void func_actor_356100_80163E2C(Task* arg0);

/// Per-frame tick of the state-7 clip run.
void func_actor_356100_80164158(Task* arg0);

/// Turn-aim tick of the state-8 clip run, the 356100 twin of
/// `Actor01900_Fn04D14`.
void func_actor_356100_80164ACC(Task* arg0);

/// Turn tick that slews the root yaw 0x89 at a time onto `field_B4A`.
void func_actor_356100_801653F4(Task* arg0);

/// Aim tick: normalises a root colour-matrix column and GPF-scales it by
/// `field_B52` into the aim scratch.
void func_actor_356100_80165B30(Task* arg0);

/// Tick of the state-0xB aim run.
void func_actor_356100_80166018(Task* arg0);

/// Tick that hands `func_800E0C10` the `field_A58` collision record.
void func_actor_356100_801668FC(Task* arg0);

/// Tick that dispatches message 0x3F1 and clears the `field_B68` latch.
void func_actor_356100_8016A550(Task* arg0);

/// Tick that decrements `field_6` and reloads it from `field_B54` plus a
/// 4-bit `Gp_LcgState` draw.
void func_actor_356100_8016A668(Task* arg0);

/// Tick that runs the `field_978` clip and halves `field_982` once the actor
/// is no longer live.
void func_actor_356100_80166CF0(Task* arg0);

/// Tick of the state-0x13 clip run.
void func_actor_356100_8016A710(Task* arg0);

/// Tick that picks clip 4 or 0x11 off `field_B3A` once the enemy is still
/// alive.
void func_actor_356100_8016A834(Task* arg0);

/// Per-frame tick of the state-0x15 clip run.
void func_actor_356100_80167358(Task* arg0);

/// Per-frame tick of the state-0x16 clip run.
void func_actor_356100_80167584(Task* arg0);

/// Per-frame tick of the state-0x18 clip run.
void func_actor_356100_80167A7C(Task* arg0);

/// Per-frame tick of the state-0x19 clip run.
void func_actor_356100_801684F0(Task* arg0);

/// Tick that pushes the actor off any collision record and turns it onto the
/// player.
void func_actor_356100_8016804C(Task* arg0);

/// Turn tick that slews the root yaw onto the player 0x28 at a time.
void func_actor_356100_80168AFC(Task* arg0);

/// Turn tick that slews the root yaw onto the player in one step and rescales
/// the root coordinate to 0x1194.
void func_actor_356100_80168E44(Task* arg0);

/// The death-throes tick: runs the per-frame clip, walks part 1's coordinate
/// and fires the 0x600FB effect burst over the model's part coordinates.
void func_actor_356100_80169180(Task* arg0);

/// The 31-entry state table the per-frame tick copies onto its own stack
/// before dispatching `f[work->field_0]`; every handler takes the task alone.
/// Entry 0x1D is a null hole.
typedef struct Actor356100StateTable {
    /* 0x00 */ TaskFunc f[31];
} Actor356100StateTable;
STATIC_ASSERT_SIZEOF(Actor356100StateTable, 0x7C);

/// The per-frame tick: rebinds the model colour matrix from part 1's world
/// translation, draws the ground quad under the model unless the actor is
/// already dying, walks part 1's parent chain into a scratch coordinate and
/// draws the second quad there, advances the state and dispatches it through
/// the table copy, then walks part 2's chain and rings the result into
/// `Actor356100Work::field_B6C` as the enemy's next local position.
void func_actor_356100_80169854(GpEnemy* arg0, Task* arg1);

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
void func_actor_356100_80161F4C(GpCoord* coord, s16 yaw)
{
    MATRIX*  rotation;
    GpCoord* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

/// Walks the first `count` contact records (stopping at a zero key) and keeps,
/// in a scratch block carved off `G_SCRATCH_HEAD`, the push that would move
/// `coord` out of the last record of kind 0x10000 or 0x30000, scaled down to
/// 0x100 units when longer. Returns whether any such record was found; returns
/// 0 at once when `gGameSession->viewReady` or `Mc_SaveData.field_5C1` is 1.
s32 func_actor_356100_80162258(GpCoord* coord, GpRec18* recs, s16 count)
{
    ActorRepelScratch* head;
    ActorRepelScratch* s;
    ActorRepelScratch* blk;
    SVECTOR*           offset;

    if (Mc_SaveData.field_5C1 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->flg                      = 0;
    head                            = SCRATCH_HEAD(ActorRepelScratch);
    blk                             = head - 1;
    SCRATCH_HEAD(ActorRepelScratch) = blk;
    s                               = blk;
    Gp_UpdateCoord(coord);
    s->pos.vx  = coord->workm.t[0];
    s->pos.vy  = coord->workm.t[1];
    s->pos.vz  = coord->workm.t[2];
    s->last.vz = 0;
    s->last.vy = 0;
    s->last.vx = 0;
    s->hit     = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            actorCalcPush(&s->pos, &recs[s->i], &s->offset);
            s->last.vx = s->offset.vx;
            s->last.vz = s->offset.vz;
        }
    }
    s->len = SquareRoot0(s->offset.vx * s->offset.vx + s->offset.vy * s->offset.vy +
                         s->offset.vz * s->offset.vz);
    if (s->len > 0x100) {
        offset = &s->offset;
        VectorNormalSS(offset, offset);
        gte_lddp(0x100);
        gte_ldsv(offset);
        gte_gpf12();
        gte_stsv(offset);
    }
    coord->flg = 0;
    SCRATCH_POP(ActorRepelScratch);
    return s->hit;
}

/// Steers `coord` away from the obstacles among the first `count` contact
/// records: collects the bearing of up to eight records of kind 0x10000 or
/// 0x30000 (in the XZ plane, or XY when the facing column is near vertical),
/// discards any pair more than 0x400 apart, and for each remaining bearing
/// nudges both `coord`'s translation and `*pos` a short step away from it.
/// `*pos` accumulates the total nudge. Returns whether any record was of kind
/// 0x10000; returns 0 at once when `gGameSession->viewReady` or `Mc_SaveData.field_5C1`
/// is 1.
s32 func_actor_356100_801625A0(GpCoord* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                  head;
    OverlayAvoidScratch* s;
    s16                  diff;
    s16                  t;
    s32                  mag;

    if (gGameSession->viewReady == 1 || Mc_SaveData.field_5C1 == 1) {
        return 0;
    }

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(OverlayAvoidScratch);
    s                = (OverlayAvoidScratch*)SCRATCH_HEAD(u8);
    s->blocked       = 0;
    pos->vz          = 0;
    pos->vy          = 0;
    pos->vx          = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = *(u16*)&coord->workm.t[0];
    s->eye.vy = *(u16*)&coord->workm.t[1];
    s->eye.vz = *(u16*)&coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_POP_BYTES(sizeof(OverlayAvoidScratch));
    return s->blocked != 0;
}

/// Steps `coord` by the movement the first `arg2` `GpRec18` records of
/// `movement` resolve to, and keeps the whole-unit part of that step in
/// `D_actor_356100_801732A0`. Returns 1 when the X or Z step is nonzero; a
/// step with a fractional part moves the coordinate and the kept step one
/// unit further from zero.
s32 func_actor_356100_80162AEC(GpCoord* coord, GpRec18* movement, s16 arg2)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    p                              = head - 0x14;
    s                              = p;
    SCRATCH_HEAD_AT(scratch, void) = p;
    s->moved                       = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_356100_801732A0.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_356100_801732A0.vy = s->delta.vy.w >> 16;
        D_actor_356100_801732A0.vz = s->delta.vz.w >> 16;
        val                        = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_356100_801732A0.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_356100_801732A0.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_356100_801732A0.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_356100_801732A0.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

/// Pushes `coord` `push` units away from each obstacle among the first
/// `count` contact records (kind 0x10000 or 0x30000) whose bearing lies within
/// 0x400 of every other obstacle's. Bearings are taken in world space from the
/// frame's position, relative to the point one unit in front of it. Returns
/// whether any push was applied; returns 0 at once when
/// `gGameSession->viewReady` is 1.
s32 func_actor_356100_80162C90(GpCoord* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                  scratch;
    void**                  tail;
    u8*                     head;
    OverlayBisectorScratch* st;
    u16                     vz;
    s16                     d;
    s16                     dz;
    s32                     t;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    scratch = SCRATCH_HEAD_ADDR;
    head    = SCRATCH_HEAD_AT(scratch, void);
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(OverlayBisectorScratch);
        st  = (OverlayBisectorScratch*)tmp;
    }
    st->eye.vx                     = *(u16*)&coord->coord.t[0];
    st->eye.vy                     = *(u16*)&coord->coord.t[1];
    vz                             = *(u16*)&coord->coord.t[2];
    SCRATCH_HEAD_AT(scratch, void) = st;
    st->eye.vz                     = vz;

    overlayToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = (u16)recs[st->i].point.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)recs[st->i].point.vy - (u16)st->eye.vy;
            dz               = (u16)recs[st->i].point.vz - (u16)st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = ratan2(st->delta.vx, dz);

            st->delta.vx     = (u16)st->aim.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)st->aim.vy - (u16)st->eye.vy;
            dz               = (u16)st->aim.vz - (u16)st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = (u16)st->angle[st->i] - ratan2(st->delta.vx, dz);

            d = st->angle[st->i];
            if (st->angle[st->i] < 0) {
            wrapUp1:
                if (d < -0x800) {
                    d += 0x1000;
                    goto wrapUp1;
                }
            } else {
            wrapDown1:
                if (d > 0x800) {
                    d -= 0x1000;
                    goto wrapDown1;
                }
            }
            st->angle[st->i] = d;
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                d        = st->diff;
                if (st->diff < 0) {
                wrapUp2:
                    if (d < -0x800) {
                        d += 0x1000;
                        goto wrapUp2;
                    }
                } else {
                wrapDown2:
                    if (d > 0x800) {
                        d -= 0x1000;
                        goto wrapDown2;
                    }
                }
                t        = d;
                st->diff = t;
                SOFT_BARRIER();
                if (t < 0) {
                    t = -t;
                }
                if (t >= 0x401) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    tail = SCRATCH_HEAD_ADDR;
    hit  = st->hit;
    SCRATCH_POP_BYTES_AT(tail, sizeof(OverlayBisectorScratch));
    return hit;
}

void func_actor_356100_801633DC(Task* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor356100AnimWork* work;

    work   = (Actor356100AnimWork*)arg0->work;
    weight = work->field_98C;
    anim   = &work->anim;
    for (i = 1; i < 0x15; i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = (u8)work->field_98A;
            work->slots[i].rate      = (u8)(work->field_982 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].rate = (u8)(work->field_982 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

void func_actor_356100_80163508(Task* arg0)
{
    Actor356100AnimWork* work;
    s16                  yaw;

    work = (Actor356100AnimWork*)arg0->work;
    if (work->field_978 == 1) {
        Actor356100AnimWork* anim;
        s32                  i;

        anim = (Actor356100AnimWork*)arg0->work;
        if (work->field_97C != work->field_97E) {
            for (i = 1; i < 0x15; i++) {
                anim->slots[i].rate = (u8)anim->field_982;
                func_800B4114(&anim->anim, i, anim->field_97E, 0,
                              D_actor_356100_801728CC[anim->field_97C][anim->field_97E]);
            }
            anim->field_97C = anim->field_97E;
        }
        work->field_978 = 3;
        work->field_980 = 0;
        work->field_994 = 0;
    } else if (work->field_978 == 2) {
        Actor356100AnimWork* anim;
        s32                  i;

        anim = (Actor356100AnimWork*)arg0->work;
        for (i = 1; i < 0x15; i++) {
            anim->slots[i].rate = (u8)anim->field_982;
            Gp_AnimResetSlot(&anim->anim, i, anim->field_97E);
        }
        anim->field_97C = anim->field_97E;
        work->field_978 = 3;
        work->field_980 = 0;
        work->field_994 = 0;
    }
    if (work->field_986 == 2) {
        Actor356100AnimWork* blend;
        s32                  i;

        blend            = (Actor356100AnimWork*)arg0->work;
        blend->field_98A = 0x30;
        blend->field_98C = 0x800;
        for (i = 1; i < 0x15; i++) {
            blend->slots[i].rate = (u8)blend->field_98A;
            Gp_AnimResetSlot(&blend->blendAnim, i, blend->field_988);
        }
        work->field_986 = 3;
    }
    work->field_980++;
    if (work->field_97A == 0) {
        Actor356100AnimWork* tick;
        s32                  i;

        tick = (Actor356100AnimWork*)arg0->work;
        for (i = 1; i < 0x15; i++) {
            tick->slots[i].rate = (u8)tick->field_982;
            Gp_AnimTickIndex(&tick->anim, i);
        }
    } else {
        func_actor_356100_801633DC(arg0);
        if (work->blendSlots[1].flags & 1) {
            work->field_97A = 0;
        }
    }
    if (work->field_98E > work->field_990) {
        if (work->field_98E - work->field_990 > 0x100) {
            work->field_990 += 0x100;
        } else {
            work->field_990 = work->field_98E;
        }
    } else if (work->field_990 - work->field_98E > 0x100) {
        work->field_990 -= 0x100;
    } else {
        work->field_990 = work->field_98E;
    }
    if (work->field_990 != 0) {
        yaw = work->field_990;
        if (work->field_990 > 0x400) {
            yaw = 0x400;
        }
        if (work->field_990 < -0x400) {
            yaw = -0x400;
        }
        func_actor_356100_80161F4C(&arg0->extra.tmd->coords[5], (yaw * 2) / 3);
        func_actor_356100_80161F4C(&arg0->extra.tmd->coords[2], yaw / 2);
        arg0->extra.tmd->coords[5].flg = 0;
        arg0->extra.tmd->coords[4].flg = 0;
        arg0->extra.tmd->coords[3].flg = 0;
        arg0->extra.tmd->coords[2].flg = 0;
    }
}

/// Initialisation for the state-0x10 clip run: allocates the work block, binds
/// the model's light / colour matrices, re-seeds the enemy descriptor and both
/// animation contexts, copies the model root's XZ pair into the work block and
/// rebuilds the root's Y rotation as a uniform 0x1194 scale. The sub-type in
/// the high half of `Task::spawnArg1` picks the clip and the spawn argument the re-entry pair, and the
/// finished entry advances the state. Same body as `Actor01900_Fn02018`.
void func_actor_356100_8016382C(GpEnemy* enemy, Task* actor)
{
    SVECTOR          dir;
    SVECTOR*         v;
    VECTOR           pos;
    TmdObject*       obj;
    GpCoord*         root;
    Actor356100Work* work;
    s32              kind;

    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(0xBC0, 0);
    actor->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, actor);
        return;
    }
    actor->exitCallback = func_actor_356100_8016A158;
    Actor356100_BindMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 1;
    enemy->reactionFlags      = 0;
    enemy->hp                 = (s16)D_actor_356100_8016A984.hpMax;
    enemy->param              = &D_actor_356100_8016A984;
    enemy->recs               = &work->field_9C0;
    func_800B3F84(&((Actor356100AnimWork*)work)->anim, D_actor_356100_801730B8, obj,
                  &((Actor356100AnimWork*)work)->slots[21], ((Actor356100AnimWork*)work)->slots);
    func_800B3F84(&((Actor356100AnimWork*)work)->blendAnim, D_actor_356100_801730B8, obj,
                  &((Actor356100AnimWork*)work)->blendSlots[21], ((Actor356100AnimWork*)work)->blendSlots);
    work->field_978 = 2;
    work->field_97E = 1;
    work->field_97A = 0;
    work->field_990 = 0;
    work->field_98E = 0;
    work->field_984 = 0x10;
    work->field_982 = 0x10;
    func_actor_356100_80163508(actor);
    work->field_14     = 0;
    work->field_C[0].x = actor->extra.tmd->coords->coord.t[0];
    work->field_C[0].z = actor->extra.tmd->coords->coord.t[2];
    Gfx_MatrixCol2(&actor->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    v      = &dir;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->field_C[1].x = actor->extra.tmd->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->extra.tmd->coords->coord.t[2] + dir.vz;
    actor->msgTable    = &D_actor_356100_80173258;
    root->sub          = &gGfxViewCoord;
    root->flg          = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    D_actor_356100_801732A8.coord      = actor->extra.tmd->coords;
    D_actor_356100_801732A8.spawnArgLo = 0x100;
    D_actor_356100_801732A8.spawnArgHi = 2;
    kind                               = actor->spawnArg1 >> 16;
    switch (kind & 0xF) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x16;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (actor->spawnArg1 & 0xF) {
        case 2:
            work->field_B54 = D_actor_356100_8016A994[0].field_0;
            work->field_B56 = D_actor_356100_8016A994[0].field_2;
            break;
        case 1:
            work->field_B54 = D_actor_356100_8016A994[2].field_0;
            work->field_B56 = D_actor_356100_8016A994[2].field_2;
            break;
        case 0:
        default:
            work->field_B54 = D_actor_356100_8016A994[1].field_0;
            work->field_B56 = D_actor_356100_8016A994[2].field_2;
            break;
    }
    actorRescaleYaw(actor->extra.tmd->coords, 0x1194);
    work->field_BBC = 0;
    actor->state++;
}

/// Runs the clip the work block's `field_978` halfword selects and holds this
/// state until it ends: while the actor is live, reset the model (`node.state.b.flags`
/// / `obj->field_C`, `Tmd_AllocBuffers`), start clip 2 at speed 0x10, and tick
/// until clip 0xB has reached frame 6 or clip 0xC frame 9, then park `field_982`
/// at 0x20. Once the actor is no longer live the same slot is halved per frame as
/// a scale ramp that bounces between 0x10 and -0x10 — ending the state with
/// `field_0 = 0x11` when `Gp_TickObjFlag2` reports the flag has expired.
void func_actor_356100_80163CD4(Task* arg0)
{
    Actor356100Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    s16              animA;
    s16              animB;
    s32              value;

    work = arg0->work;
    ctx  = arg0->spawnArg2;
    if (work->field_4 != 0) {
        animA                   = 0xB;
        animB                   = 0xC;
        obj                     = arg0->extra.tmd;
        ctx->node.state.b.flags = 0;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        do {
            func_actor_356100_80163508(arg0);
        } while (((work->field_97E != animA) || ((u32)(work->field_5A & 0x3FF) < 6U)) &&
                 ((work->field_97E != animB) || ((u32)(work->field_5A & 0x3FF) < 9U)));
        work->field_982 = 0x20;
        return;
    }
    arg0->extra.tmd->coords->flg = 0;
    value                        = (s16)work->field_982 / 2;
    work->field_982              = (u16)value;
    if (value == 1) {
        work->field_982 = -0x10U;
    }
    if ((s16)work->field_982 == -1) {
        work->field_982 = 0x10;
    }
    func_actor_356100_80163508(arg0);
    if (Gp_TickObjFlag2(ctx) == 1) {
        ctx->reactionFlags &= 0xFD;
        work->field_0       = 0x11;
    }
}

/// Turns the actor's facing onto the player in one step and rescales the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 1 at
/// speed 0x10 with the 9 state parked in `field_97E`, otherwise the aim scratch
/// takes the player offset, `actorPositionYaw` gives the wrapped turn,
/// `field_98E` snapshots it, it is clamped to [-0x10, 0x10] and the root yaw is
/// re-derived from it. Same body as `func_actor_401300_8013AAE8`.
void func_actor_356100_80163E2C(Task* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GpCoord*               coord;
    Actor356100AimScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 1;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 9;
        func_actor_356100_80163508(arg0);
        work->field_9BC = 0x180;
        Gp_ArmStateF0(1);
        return;
    }
    SCRATCH_PUSH(Actor356100AimScratch);
    aim                          = SCRATCH_HEAD(Actor356100AimScratch);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
    aim->angle      = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_98E = aim->angle;
    if (aim->angle >= 0x11) {
        aim->angle = 0x10;
    }
    if (aim->angle < -0x10) {
        aim->angle = -0x10;
    }
    coord       = arg0->extra.tmd->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, aim->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    func_actor_356100_80163508(arg0);
    SCRATCH_POP(Actor356100AimScratch);
}

/// Steps `coord` `amount` units along its own root colour-matrix column unless
/// movement is frozen, normalising the column with the GTE first and giving the
/// 8-byte `G_SCRATCH_HEAD` block back afterwards. The guardless sibling of
/// `actorMoveForwardNonzero`, reading the X component back through
/// `vec`; same body as `Actor01900_StepForward` / `actorMoveForward`.
static __inline__ void Actor356100_StepForward(GpCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (Mc_SaveData.field_5C1 != 1) {
        head                  = SCRATCH_HEAD(SVECTOR);
        SCRATCH_HEAD(SVECTOR) = head - 1;
        vec                   = head - 1;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0] += vec->vx;
        coord->coord.t[1] += vec->vy;
        coord->coord.t[2] += vec->vz;
        coord->flg         = 0;
        SCRATCH_POP(SVECTOR);
    }
}

/// Pushes `coord` out of the `GpRec18` records `rec` by `func_800E0C10`'s
/// averaged 16.16 delta, then lifts it by `height`. `head` is read before the
/// 0x14-byte `OverlayDeltaFlag` block is reserved off `G_SCRATCH_HEAD`, so
/// the two spellings of the block in the body reach it the same way the
/// original does — the negative offsets off `head` for the X component and the
/// flag, `s` for the rest. Same body as `Actor01900_Fn00E00`'s push without
/// its mask argument.
static __inline__ void Actor356100_PushRecords(GpCoord* coord, GpRec18* rec, s32 count, s16 height)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    s32               val;

    if (Mc_SaveData.field_5C1 != 1) {
        scratch = SCRATCH_HEAD_ADDR;
        head    = SCRATCH_HEAD_AT(scratch, void);
        SCRATCH_PUSH(OverlayDeltaFlag);
        s        = SCRATCH_HEAD(OverlayDeltaFlag);
        s->moved = 0;
        if (func_800E0C10(rec, &s->delta, count, NULL) != 0) {
            coord->coord.t[0] += ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
            coord->coord.t[1] += s->delta.vy.h.hi;
            coord->coord.t[2] += s->delta.vz.h.hi;
            val                = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[0]++;
                } else {
                    coord->coord.t[0]--;
                }
            }
            val = s->delta.vz.w;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[2]++;
                } else {
                    coord->coord.t[2]--;
                }
            }
        }
        coord->coord.t[1] += height;
        if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
            s->moved = 1;
        }
        SCRATCH_POP(OverlayDeltaFlag);
    }
}

/// `Actor356100_PushRecords` without the freeze guard, returning `field_10`
/// after the scratch is given back. The caller names `Mc_SaveData.field_5C1` first so the
/// compare interleaves with the coordinate load.
static __inline__ s32 Actor356100_PushRecordsAlways(GpCoord* coord, GpRec18* rec, s32 count, s16 height)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    s32               val;

    scratch = SCRATCH_HEAD_ADDR;
    head    = SCRATCH_HEAD_AT(scratch, void);
    SCRATCH_PUSH(OverlayDeltaFlag);
    s        = SCRATCH_HEAD(OverlayDeltaFlag);
    s->moved = 0;
    if (func_800E0C10(rec, &s->delta, count, NULL) != 0) {
        coord->coord.t[0] += ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[1] += s->delta.vy.h.hi;
        coord->coord.t[2] += s->delta.vz.h.hi;
        val                = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    coord->coord.t[1] += height;
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP(OverlayDeltaFlag);
    return s->moved;
}

void func_actor_356100_80164158(Task* arg0)
{
    Actor356100Work*       work;
    Actor356100AimScratch* aim;
    TmdObject*             obj;
    s16                    yaw;
    s32                    diff;
    s32                    range;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 0x12;
        work->field_97A = 0;
        work->field_97E = 3;
        func_actor_356100_80163508(arg0);
        work->field_B64 = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        return;
    }
    work->field_6 = (u16)work->field_6 + 1;
    SCRATCH_PUSH(Actor356100AimScratch);
    aim = SCRATCH_HEAD(Actor356100AimScratch);
    Actor356100_PushRecords(arg0->extra.tmd->coords, &work->field_A58, 3, 0x10);
    actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->flg = 0;
    func_actor_356100_80163508(arg0);
    aim->target = ratan2(-(gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][0],
                         (gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][2]);
    actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &aim->delta);
    yaw             = ratan2(aim->delta.vx, aim->delta.vz) + 0x800;
    aim->current    = yaw;
    aim->current    = actorNormalizeYaw(yaw);
    aim->angle      = actorYawTo(arg0->extra.tmd->coords, aim->delta.vx, aim->delta.vz);
    work->field_98E = aim->angle;
    diff            = aim->current - aim->target;
    if (ABS(diff) < 0x44 && (((s16)work->field_B66 / 2) + 3) < work->field_6 && ABS(aim->angle) < 0x80) {
        if (overlayOutOfRange(&aim->delta, 0x708)) {
            work->field_0 = 0xA;
        }
    }
    range = actorNormalizeYaw((u16)aim->current - (u16)aim->target);
    if (ABS(range) >= 0x201 && (((s16)work->field_B66 / 2) + 3) < work->field_6 && work->field_8 == 0) {
        work->field_8   = 1;
        work->field_97E = 9;
        work->field_978 = 1;
    }
    if (aim->angle < 0x200) {
        if (!overlayOutOfRange(&aim->delta, 0x44C)) {
            work->field_0 = 0xB;
        }
    }
    if (aim->angle > 0x40) {
        aim->angle = 0x40;
    }
    if (aim->angle < -0x40) {
        aim->angle = -0x40;
    }
    aim->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, aim->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_97E == 3) {
        if (work->field_97A == 0) {
            Actor356100_StepForward(arg0->extra.tmd->coords, 0x78);
        } else {
            Actor356100_StepForward(arg0->extra.tmd->coords, 0x1E);
        }
    } else if (work->field_68 & 1) {
        work->field_97E = 3;
        work->field_978 = 1;
    }
    SCRATCH_POP(Actor356100AimScratch);
}

/// Turn-aim state body, the 356100 twin of `Actor01900_Fn04D14`: take a 0x10
/// chase scratch off `G_SCRATCH_HEAD` and, on the live-actor flag, key the
/// animation nodes, the frame counter and the `field_B4E` clip phase. Once
/// `field_8` has counted 7 frames the arm aims at the player — the yaw toward
/// `gameGetPtrSlot(3)` goes in `target`, the wrapped yaw toward
/// `Player_Status.coordMtx` in `current` — and the root is turned by the facing
/// yaw plus a +-0x60 clamp of the turn's 1000 bias. The forward draw
/// `field_B4C` is the doubled frame parameter (halved while `field_97A` is
/// up, forced to 2 while the frame counter runs), and the actor slides along
/// it unless movement is frozen. `field_B4E` walks 8 -> -1 -> 0 as `field_982`
/// passes 0x18 and 0x12, and the 0 arm runs the five-frame exit window that
/// re-aims once more and picks state 0xB when the actor faces away from the
/// player, else state 0x1A.
void func_actor_356100_80164ACC(Task* arg0)
{
    Actor356100Work*       work;
    Actor356100AimScratch* head;
    Actor356100AimScratch* s;
    TmdObject*             obj;
    GpCoord*               coord;
    GpCoord*               facing;
    GpCoord*               pushCoord;
    s32                    turn;
    s32                    diffPos;
    s32                    diffNeg;
    s32                    yaw;
    s32                    hit;
    s32                    paused;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0xC0;
        work->field_978 = 1;
        work->field_97A = 0;
        work->field_97E = 3;
        func_actor_356100_80163508(arg0);
        work->field_B4E         = 8;
        work->field_6           = 0;
        work->field_8           = 0;
        D_actor_356100_80173290 = 0;
        work->field_B64++;
        return;
    }
    head                                = SCRATCH_HEAD(Actor356100AimScratch);
    SCRATCH_HEAD(Actor356100AimScratch) = head - 1;
    s                                   = head - 1;
    arg0->extra.tmd->coords->flg        = 0;
    func_actor_356100_80163508(arg0);
    paused    = Mc_SaveData.field_5C1;
    pushCoord = arg0->extra.tmd->coords;
    hit       = (s32)&work->field_A58;
    if (paused == 1) {
        hit = 0;
    } else {
        hit = Actor356100_PushRecordsAlways(pushCoord, (GpRec18*)hit, 3, 0x10);
    }
    if (hit != 0) {
        work->field_8++;
    }
    actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &s->delta);
    if (work->field_8 >= 7) {
        s->target     = ratan2(-(gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][0],
                               (gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][2]);
        s->current    = ratan2(s->delta.vx, s->delta.vz) + 0x800;
        s->current    = actorNormalizeYaw(s->current);
        work->field_0 = 0x1A;
    }
    coord    = arg0->extra.tmd->coords;
    s->angle = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn     = s->angle;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (((diffPos < 0) ? -diffPos : diffPos) < 0x60) {
            s->facing = s->angle - 1000;
        } else if (diffPos > 0) {
            s->facing = 0x60;
        } else {
            s->facing = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (((diffNeg < 0) ? -diffNeg : diffNeg) < 0x60) {
            s->facing = s->angle + 1000;
        } else if (diffNeg > 0) {
            s->facing = 0x60;
        } else {
            s->facing = -0x60;
        }
    }
    facing     = arg0->extra.tmd->coords;
    s->facing += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, s->facing, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    coord                        = arg0->extra.tmd->coords;
    work->field_98E              = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->extra.tmd->coords->flg = 0;
    work->field_B4C              = work->field_982 * 8;
    if (work->field_97A != 0) {
        work->field_B4C = work->field_B4C >> 1;
    }
    if (work->field_8 != 0) {
        work->field_B4C = 2;
    }
    actorMoveForwardNonzero(arg0->extra.tmd->coords, work->field_B4C);
    D_actor_356100_80173290 += work->field_B4C;
    if (work->field_B4E == 8 && work->field_982 >= 0x18) {
        work->field_B4E = -1;
    }
    if (work->field_B4E == -1 && work->field_982 == 0x12) {
        work->field_B4E = 0;
        work->field_6   = 0;
    }
    if (work->field_B4E == 0) {
        if (++work->field_6 == 5) {
            s->target = ratan2(-(gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][0],
                               (gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][2]);
            actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &s->delta);
            s->current = ratan2(s->delta.vx, s->delta.vz) + 0x800;
            yaw        = actorNormalizeYaw(s->current);
            s->current = yaw;
            yaw        = yaw - s->target;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw >= 0x401) {
                work->field_0 = 0xB;
            } else {
                work->field_0 = 0x1A;
                work->field_2 = -1;
            }
        }
    }
    work->field_982 += (u16)work->field_B4E;
    SCRATCH_POP(Actor356100AimScratch);
}

/// Aim tick: going live resets the model and starts clip 1 at speed 0x10 with
/// the 3 state parked in `field_97E` and `field_98E` cleared; otherwise the
/// aim scratch takes the player offset, and the wrapped turn from it is paired
/// with the root's own facing yaw — snapshotted into `field_B48` and, plus
/// twice the turn, into the `field_B4A` the yaw is then slewed toward. Same
/// body as `Actor01900_Fn0551C`, whose aim tick this is the live-arm half of:
/// the settling yaw is re-derived from the player each entry while that one
/// only re-seeds the pair.
void func_actor_356100_801653F4(Task* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GpCoord*               coord;
    GpCoord*               cur;
    GpCoord*               facing;
    Actor356100AimScratch* head;
    Actor356100AimScratch* s;
    s32                    value;

    work = arg0->work;
    if (work->field_4 != 0) {
        head                                            = SCRATCH_HEAD(Actor356100AimScratch);
        obj                                             = arg0->extra.tmd;
        SCRATCH_HEAD(Actor356100AimScratch)             = head - 1;
        s                                               = head - 1;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 3;
        work->field_98E = 0;
        func_actor_356100_80163508(arg0);
        cur               = arg0->extra.tmd->coords;
        head[-1].delta.vx = Player_Status.coordMtx->t[0] - cur->coord.t[0];
        s->delta.vy       = Player_Status.coordMtx->t[1] - cur->coord.t[1];
        s->delta.vz       = Player_Status.coordMtx->t[2] - cur->coord.t[2];
        coord             = arg0->extra.tmd->coords;
        s->angle          = actorYawTo(coord, head[-1].delta.vx, s->delta.vz);
        facing            = arg0->extra.tmd->coords;
        s->facing         = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_B48   = s->facing;
        work->field_B4A   = s->facing + (u16)s->angle * 2;
        SCRATCH_POP(Actor356100AimScratch);
        return;
    }
    head                                = SCRATCH_HEAD(Actor356100AimScratch);
    SCRATCH_HEAD(Actor356100AimScratch) = head - 1;
    s                                   = head - 1;
    func_actor_356100_80163508(arg0);
    cur               = arg0->extra.tmd->coords;
    head[-1].delta.vx = Player_Status.coordMtx->t[0] - cur->coord.t[0];
    s->delta.vy       = Player_Status.coordMtx->t[1] - cur->coord.t[1];
    s->delta.vz       = Player_Status.coordMtx->t[2] - cur->coord.t[2];
    if (work->field_B48 == work->field_B4A) {
        if (work->field_B64 < 2 || overlayOutOfRange(&s->delta, 0x384)) {
            value = 8;
        } else {
            value = 0xB;
        }
        work->field_0 = value;
    }
    if (work->field_B48 > work->field_B4A) {
        work->field_B48 -= 0x89;
        if (work->field_B48 < work->field_B4A) {
            work->field_B48 = work->field_B4A;
        }
    }
    if (work->field_B48 < work->field_B4A) {
        work->field_B48 += 0x89;
        if (work->field_B48 > work->field_B4A) {
            work->field_B48 = work->field_B4A;
        }
    }
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, work->field_B48, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_97A == 0) {
        Actor356100_StepForward(arg0->extra.tmd->coords, 0x28);
    } else {
        Actor356100_StepForward(arg0->extra.tmd->coords, 0x14);
    }
    Actor356100_PushRecords(arg0->extra.tmd->coords, &work->field_A58, 3, 0x10);
    SCRATCH_POP(Actor356100AimScratch);
}

/// Turn-and-close tick: going live writes the 0x978..0x982 animation slots with
/// `field_9BC` forced to 0xC0 and the enemy's link node cleared, takes the
/// player offset into the aim scratch and turns the root onto it with
/// `ratan2`, then settles `field_B50` on the 12-bit side the `Gp_LcgState`
/// draw picks and leans the yaw by `field_B56` either way, before rebuilding
/// its Y rotation at the fixed 0xDE GPF scale and bumping `field_B66`. Each
/// frame then re-runs the animation and, while the clip sits in 0xC..0x15,
/// takes the aim and pushes the root out of the `field_A58` collision records
/// by 0x10. Past clip 0x1E the state moves to 7. Same body as
/// `Actor01900_Fn05B4C`, whose `head[-1]` / `aim` spelling of the 0x10-byte
/// scratch block this matches.
void func_actor_356100_80165B30(Task* arg0)
{
    Actor356100Work*       work;
    Actor356100AimScratch* head;
    Actor356100AimScratch* aim;
    TmdObject*             obj;
    GpCoord*               coord;
    SVECTOR*               dir;
    MATRIX                 mat;
    u16                    angle;

    head                                = SCRATCH_HEAD(Actor356100AimScratch);
    work                                = arg0->work;
    SCRATCH_HEAD(Actor356100AimScratch) = head - 1;
    aim                                 = head - 1;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0xC0;
        work->field_6   = 0;
        actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &aim->delta);
        aim->angle = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_B50 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_B50 = 1;
            } else {
                work->field_B50 = -1;
            }
        }
        if (work->field_B50 == 1) {
            work->field_97E = 0x15;
            if (work->field_B66 == 0) {
                angle      = aim->angle + 0x171;
                aim->angle = work->field_B56 + angle;
            } else {
                aim->angle += work->field_B56;
            }
            work->field_B50 = -1;
        } else {
            work->field_97E = 0x14;
            if (work->field_B66 == 0) {
                angle      = aim->angle - 0x171;
                aim->angle = angle - work->field_B56;
            } else {
                aim->angle -= work->field_B56;
            }
            work->field_B50 = 1;
        }
        work->field_978 = 1;
        work->field_982 = 0xC;
        work->field_97A = 0;
        func_actor_356100_80163508(arg0);
        Gfx_RotMatrixY(&mat, aim->angle, 1);
        dir = &work->field_B40;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_B52 = 0xDE;
        work->field_B66++;
    }
    arg0->extra.tmd->coords->flg = 0;
    func_actor_356100_80163508(arg0);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_97A == 0) {
        gte_lddp(work->field_B52);
        gte_ldsv(&work->field_B40);
        gte_gpf12();
        gte_stsv(aim);
    } else {
        gte_lddp((s16)work->field_B52 >> 1);
        gte_ldsv(&work->field_B40);
        gte_gpf12();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[2] += aim->delta.vz;
        Actor356100_PushRecords(arg0->extra.tmd->coords, &work->field_A58, 3, 0x10);
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
    }
    SCRATCH_POP(Actor356100AimScratch);
}

/// Turn-and-close tick, and the sibling of `func_actor_356100_801666B4` above
/// it. Going live writes the 0x978..0x982 animation slots with `field_9BC`
/// forced to 0x180 and the enemy's link node cleared, then turns the root
/// coordinate onto the player through `actorPositionYaw` and rebuilds
/// its Y rotation at a uniform 0x1194 scale, re-seeding the offset from the
/// player and clearing the two halfwords next to `field_B68`. Each frame then
/// re-runs the animation and, while the clip sits on 0x10 and the player is not
/// in mode 2, takes the player offset again through
/// `actorMatrixPositionYaw` and — if the turn is within 0x10 and the
/// player is closer than 0x44C — points `D_actor_356100_80173244.field_0` at
/// one of the two blocks `D_8007218A` selects, then queries message 0x3F8 and
/// on acceptance moves to state 0xC, sets `field_B68` and re-sends the handler
/// as message 0x3FF. Bit 0 of `field_68` forces `field_0` to 7 on clip 4, and
/// past clip 0x10 the actor is pushed one normalised unit away from the player
/// unless it is further than 0x578.
void func_actor_356100_80166018(Task* arg0)
{
    SVECTOR          pos;
    SVECTOR*         p;
    Actor356100Work* work;
    GpEnemy*         enemy;
    GameActor*       player;
    PlayerStatus*    config;
    GpCoord*         coord;
    s16              angle;

    enemy  = arg0->spawnArg2;
    work   = arg0->work;
    player = (GameActor*)(gameGetPtrSlot(3))->work;
    config = &Player_Status;
    if (work->field_4 != 0) {
        work->field_9BC           = 0x180;
        enemy->node.state.b.flags = 0;
        work->field_978           = 1;
        work->field_982           = 0x10;
        work->field_97E           = 4;
        func_actor_356100_80163508(arg0);
        Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, actorPositionYaw(arg0, &pos, config), 0);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        pos.vx                       = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy                       = 0;
        pos.vz                       = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        work->field_98E              = 0;
        work->field_990              = 0;
        arg0->extra.tmd->coords->flg = 0;
        work->field_B66              = 0;
        work->field_B68              = 0;
    }
    func_actor_356100_80163508(arg0);
    if ((work->field_5A & 0x3FF) == 0x10 && player->field_954 != 2) {
        angle = actorMatrixPositionYaw(arg0, &pos, Player_Status.coordMtx);
        if (abs(angle) < 0x10 && !overlayOutOfRange(&pos, 0x44C)) {
            if (D_8007218A == 1) {
                D_actor_356100_80173244.animBlock.ptr = &D_actor_356100_80173230;
            } else {
                D_actor_356100_80173244.animBlock.ptr = &D_actor_356100_80173228;
            }
            D_actor_356100_801732D0.field_14 = 8;
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&D_actor_356100_801732D0, 0) == 0) {
                work->field_0                   = 0xC;
                work->field_B68                 = 1;
                D_actor_356100_80173244.field_4 = 1;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&D_actor_356100_80173244, 0);
            }
        }
    }
    if (work->field_97E == 4 && (work->field_68 & 1)) {
        work->field_0 = 7;
    }
    if ((work->field_5A & 0x3FF) > 0x10) {
        p      = &pos;
        pos.vx = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy = 0;
        pos.vz = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        if (!overlayOutOfRange(p, 0x578)) {
            VectorNormalSS(p, p);
            gte_lddp(10);
            gte_ldsv(p);
            gte_gpf12();
            gte_stsv(p);
            coord                        = arg0->extra.tmd->coords;
            coord->coord.t[0]           += pos.vx;
            coord                        = arg0->extra.tmd->coords;
            coord->coord.t[2]           += pos.vz;
            arg0->extra.tmd->coords->flg = 0;
        }
    }
}

void func_actor_356100_801666B4(Task* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    Task*            player;
    SVECTOR*         vecp;
    SVECTOR          vec;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        player                         = gameGetPtrSlot(3);
        work->field_9BC                = 0x180;
        enemy->node.state.b.flags      = 0;
        work->field_978                = 1;
        work->field_982                = 0x10;
        work->field_97E                = 5;
        player->extra.tmd->coords->flg = 0;
        Gp_UpdateCoord(player->extra.tmd->coords);
        D_actor_356100_801732B0.pos.vx = player->extra.tmd->coords->coord.t[0];
        D_actor_356100_801732B0.pos.vy = player->extra.tmd->coords->coord.t[1];
        D_actor_356100_801732B0.pos.vz = player->extra.tmd->coords->coord.t[2];
        vecp                           = &vec;
        /* Order matters: the vy store must follow the vx loads in RTL, or
           sched1 fills its anti-dependency chain from the earlier stores and
           hoists it above the D.z store. */
        vec.vx = ((GpCoordPos*)arg0->extra.tmd->coords)->x - ((GpCoordPos*)player->extra.tmd->coords)->x;
        vec.vy = 0;
        vec.vz = ((GpCoordPos*)arg0->extra.tmd->coords)->z - ((GpCoordPos*)player->extra.tmd->coords)->z;
        VectorNormalSS(vecp, vecp);
        gte_lddp(0x3E8);
        gte_ldsv(vecp);
        gte_gpf12();
        gte_stsv(vecp);
        arg0->extra.tmd->coords->coord.t[0] = player->extra.tmd->coords->coord.t[0] + vec.vx;
        arg0->extra.tmd->coords->coord.t[2] = player->extra.tmd->coords->coord.t[2] + vec.vz;
        arg0->extra.tmd->coords->flg        = 0;
        D_actor_356100_801732B0.rot.vx      = 0;
        D_actor_356100_801732B0.rot.vy      = ratan2(vec.vx, vec.vz);
        D_actor_356100_801732B0.rot.vz      = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)&D_actor_356100_801732B0, 0);
    }
    func_actor_356100_80163508(arg0);
    if (work->field_97E == 5 && (work->field_68 & 1)) {
        work->field_0 = 0xD;
    }
}

void func_actor_356100_801668FC(Task* arg0)
{
    Actor356100Work*  work;
    GpEnemy*          enemy;
    PlayerStatus*     cfg;
    McSaveData*       save;
    GpCoord*          coord;
    GpCoord*          root;
    u8*               base;
    u8*               base2;
    u8*               slot;
    u8*               slot2;
    SVECTOR*          v;
    SVECTOR*          next;
    OverlayDeltaFlag* next2;
    OverlayDeltaFlag* blk;
    OverlayDeltaFlag* s;
    s32               val;
    s32               mode;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    cfg   = &Player_Status;
    if (work->field_4 != 0) {
        work->field_982 = 0x10;
        work->field_97E = 7;
        work->field_978 = 2;
        func_actor_356100_80163508(arg0);
        D_actor_356100_80173244.field_4 = 3;
        if (cfg->hp > 0) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&D_actor_356100_80173244, 0);
        }
        work->field_6 = 0;
    } else if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0 &&
               cfg->hp > 0 && work->field_B68 == 1) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
        work->field_B68 = 0;
    }
    if ((u32)(work->field_5A & 0x3FF) - 0x10 < 7U) {
        save  = &Mc_SaveData;
        coord = arg0->extra.tmd->coords;
        if (save->field_5C1 != 1) {
            base                       = PSX_SCRATCH;
            slot                       = *(u8**)(base + 0x3FC);
            base                       = slot;
            v                          = (SVECTOR*)(slot - 8);
            base                       = PSX_SCRATCH;
            *(SVECTOR**)(base + 0x3FC) = v;
            base                       = slot;
            Gfx_MatrixCol2(&coord->coord, v);
            VectorNormalSS(v, v);
            gte_lddp(-0x78);
            gte_ldsv(v);
            gte_gpf12();
            gte_stsv(v);
            coord->coord.t[0]         += ((SVECTOR*)(slot - 8))->vx;
            coord->coord.t[1]         += v->vy;
            coord->coord.t[2]         += v->vz;
            coord->flg                 = 0;
            base                       = PSX_SCRATCH;
            next                       = (SVECTOR*)(*(SVECTOR**)(base + 0x3FC) + 1);
            base                       = slot;
            base                       = PSX_SCRATCH;
            *(SVECTOR**)(base + 0x3FC) = next;
            base                       = slot;
        }
        root = arg0->extra.tmd->coords;
        if (save->field_5C1 != 1) {
            base2                                = PSX_SCRATCH;
            slot2                                = *(u8**)(base2 + 0x3FC);
            base2                                = slot2;
            blk                                  = (OverlayDeltaFlag*)(slot2 - 0x14);
            base2                                = PSX_SCRATCH;
            *(OverlayDeltaFlag**)(base2 + 0x3FC) = blk;
            base2                                = slot2;
            s                                    = blk;
            blk->moved                           = 0;
            if (func_800E0C10(&work->field_A58, &blk->delta, 3, NULL) != 0) {
                root->coord.t[0] += ((OverlayDeltaFlag*)(slot2 - 0x14))->delta.vx.h.hi;
                root->coord.t[1] += blk->delta.vy.h.hi;
                root->coord.t[2] += blk->delta.vz.h.hi;
                val               = ((OverlayDeltaFlag*)(slot2 - 0x14))->delta.vx.w;
                if ((val & 0xFFFF) != 0) {
                    if (val > 0) {
                        root->coord.t[0]++;
                    } else {
                        root->coord.t[0]--;
                    }
                }
                val = s->delta.vz.w;
                if ((val & 0xFFFF) != 0) {
                    if (val > 0) {
                        root->coord.t[2]++;
                    } else {
                        root->coord.t[2]--;
                    }
                }
            }
            root->coord.t[1] += 0x10;
            if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
                s->moved = 1;
            }
            base2                                = PSX_SCRATCH;
            next2                                = (OverlayDeltaFlag*)(*(OverlayDeltaFlag**)(base2 + 0x3FC) + 1);
            base2                                = slot2;
            base2                                = PSX_SCRATCH;
            *(OverlayDeltaFlag**)(base2 + 0x3FC) = next2;
            base2                                = slot2;
        }
        arg0->extra.tmd->coords->flg = 0;
    }
    func_actor_356100_80163508(arg0);
    if (work->field_68 & 1) {
        mode = 1;

        val = enemy->node.state.b.targeted;
        if (val != mode) {
            mode = 6;
        } else {
            mode = 0xA;
        }
        work->field_0 = mode;
        if (cfg->hp > 0 && work->field_B68 == 1) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            work->field_B68 = 0;
        }
    }
}

/// Release tick, the sibling of `func_actor_356100_801684F0` below it and the
/// same body as `Actor01900_Fn06100`. Going live resets the model and starts
/// clip 3 at speed 8 with `field_97A` cleared and `field_B64` zeroed;
/// otherwise the aim scratch takes the player offset, the root is pushed out of
/// the `field_A58` collision records and `actorYawTo` gives the wrapped
/// turn, which `field_98E` snapshots. A turn under 0x200 while the player is
/// still within 0x384 moves the state to 0xB; the turn is then clamped to
/// [-0x40, 0x40], the root yaw is re-derived from it and the root rescaled to a
/// uniform 0x1194 before being stepped 0x78 along its own column, or 0x3C when
/// `field_97A` is set.
void func_actor_356100_80166CF0(Task* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    Actor356100AimScratch* aim;
    s16                    ang;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 8;
        work->field_97A = 0;
        work->field_97E = 3;
        func_actor_356100_80163508(arg0);
        work->field_B64 = 0;
        return;
    }
    SCRATCH_PUSH(Actor356100AimScratch);
    aim = SCRATCH_HEAD(Actor356100AimScratch);
    Actor356100_PushRecords(arg0->extra.tmd->coords, &work->field_A58, 3, 0x10);
    actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->flg = 0;
    func_actor_356100_80163508(arg0);
    ang             = actorYawTo(arg0->extra.tmd->coords, aim->delta.vx, aim->delta.vz);
    aim->angle      = ang;
    work->field_98E = ang;
    if (aim->angle < 0x200) {
        if (!overlayOutOfRange(&aim->delta, 0x384)) {
            work->field_0 = 0xB;
        }
    }
    if (aim->angle > 0x40) {
        aim->angle = 0x40;
    }
    if (aim->angle < -0x40) {
        aim->angle = -0x40;
    }
    aim->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, aim->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_97A == 0) {
        Actor356100_StepForward(arg0->extra.tmd->coords, 0x78);
    } else {
        Actor356100_StepForward(arg0->extra.tmd->coords, 0x3C);
    }
    SCRATCH_POP(Actor356100AimScratch);
}

/// Rotation-collapse tick: going live clears the model's `field_C`, flags the
/// enemy's link node and re-seeds `field_6`. Each frame then bumps `field_6`
/// and fires its milestone — 0x18 releases state F0 (arg 0xA), 0x1D switches
/// light mode 1 and spawns effect 0x600A5 at model coordinate 2, 0x29 sets
/// `field_C` to 2, 0x2F switches light mode 2 and 0x33 sets `field_C` to 0x80.
/// From 0x1A on, the root rotation is rebuilt in the 0x34-byte scratch block
/// as a uniform 0x1194 scale whose Y shrinks by 0xB per frame past 0x14, and
/// written back into the root coordinate with `flg` cleared. Same body as
/// `Actor01900_Fn06904`.
void func_actor_356100_80167358(Task* arg0)
{
    Actor356100Work*      work;
    GpEnemy*              enemy;
    TmdObject*            obj;
    GpCoord*              coord;
    ActorScaleRotScratch* blk;
    u8*                   head;
    u8*                   tail;
    void*                 scratch_base;
    s16                   temp_v0;
    s16                   ang;
    s16                   cur;
    s32                   k;
    s32                   sy;
    u16                   temp_v1;
    u16                   m22;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags                = 0;
        enemy->node.state.b.flags = 1;
        work->field_6             = 0;
    }
    temp_v1      = (u16)work->field_6;
    scratch_base = PSX_SCRATCH;
    if (work->field_6 < 0x401) {
        work->field_6 = (s16)(temp_v1 + 1);
        temp_v0       = temp_v1 - 0x18;
        switch (temp_v0) {
            case 0:
                Gp_ReleaseStateF0Add(arg0, 0xA);
                break;
            case 5:
                Gp_SetLightMode(enemy, 1);
                Gp_SpawnEff(0x600A5, arg0->extra.tmd->coords + 2, 3, NULL);
                break;
            case 23:
                arg0->extra.tmd->flags = 2;
                break;
            case 17:
                Gp_SetLightMode(enemy, 2);
                break;
            case 39:
                arg0->extra.tmd->flags = 0x80;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            k                                  = 0x1194;
            head                               = scratch_base;
            head                               = *(u8**)(head + 0x3FC);
            coord                              = arg0->extra.tmd->coords;
            blk                                = (ActorScaleRotScratch*)(head - 0x34);
            sy                                 = k - (cur - 0x14) * 0xB;
            SCRATCH_HEAD(ActorScaleRotScratch) = blk;
            ang                                = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
            blk->angle                         = ang;
            Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
            blk->scale.vx = k;
            blk->scale.vy = (s32)(s16)sy;
            blk->scale.vz = k;
            ScaleMatrix(&blk->m, &((ActorScaleRotScratch*)(head - 0x34))->scale);
            coord->coord.m[0][0] = (u16)((ActorScaleRotScratch*)(head - 0x34))->m.m[0][0];
            coord->coord.m[0][1] = (u16)blk->m.m[0][1];
            coord->coord.m[0][2] = (u16)blk->m.m[0][2];
            coord->coord.m[1][0] = (u16)blk->m.m[1][0];
            coord->coord.m[1][1] = (u16)blk->m.m[1][1];
            coord->coord.m[1][2] = (u16)blk->m.m[1][2];
            coord->coord.m[2][0] = (u16)blk->m.m[2][0];
            coord->coord.m[2][1] = (u16)blk->m.m[2][1];
            __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
            tail       = *(u8**)(tail + 0x3FC);
            m22        = (u16)blk->m.m[2][2];
            coord->flg = 0;
            tail       = tail + 0x34;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
            coord->coord.m[2][2] = m22;
        }
    }
}

/// Range tick, and the sibling of `func_actor_356100_80167818` below it. Going
/// live clears the model's `field_C`, reallocates its buffers, clears the
/// enemy's link node, saves the `field_AF8` colour matrix into `field_B18` and
/// starts clip 0xE at speed 1 with `field_9BC` forced to 0x180. Each frame then
/// bumps `field_6` until it passes 0x960, after which a 4-bit `Gp_LcgState`
/// draw thins the tick to one frame in 16. A tick that runs drops to state 6
/// while the player is still within 3000 of the actor, then flips the clip
/// between 0xE and 0xF on a 50/50 draw gated by bits 2 and 1 of `field_68`.
/// Same shape as `func_actor_401300_80139520`.
void func_actor_356100_80167584(Task* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GpCoord*         coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj        = arg0->extra.tmd;
        enemy      = arg0->spawnArg2;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC           = 0x180;
        enemy->node.state.b.flags = 0;
        work->field_6             = 0;
        work->field_B18           = work->field_AF8;
        work->field_97E           = 0xE;
        work->field_978           = 1;
        work->field_982           = work->field_984;
    }
    if (work->field_6 > 0x960) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6 = (s16)((u16)work->field_6 + 1);
    }
    coord = arg0->extra.tmd->coords;
    d     = &delta;
    Actor356100_PositionDelta(coord, d);
    if (!overlayOutOfRange(d, 3000)) {
        work->field_0 = 6;
    }
    func_actor_356100_80163508(arg0);
    if (work->field_97E == 0xE && (work->field_68 & 2)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            work->field_97E = 0xF;
            work->field_978 = 1;
            func_actor_356100_80163508(arg0);
        }
    }
    if (work->field_97E == 0xF && (work->field_68 & 1)) {
        work->field_97E = 0xE;
        work->field_978 = 1;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_80167818(Task* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GpCoord*         coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                     = arg0->extra.tmd;
        D_actor_356100_801731B0 = 0;
        work->field_97E         = 0x10;
        work->field_978         = 2;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC           = 0x180;
        enemy->node.state.b.flags = 0;
        work->field_990           = 0;
        work->field_982           = 0x10;
        work->field_98E           = 0;
        work->field_6             = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->placeKey >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_6 = 1;
    }
    func_actor_356100_80163508(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_994 != (work->field_5A & 0x3FF)) {
        D_actor_356100_801732A8.coord      = arg0->extra.tmd->coords;
        D_actor_356100_801732A8.spawnArgLo = 0x100;
        D_actor_356100_801732A8.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->extra.tmd->coords + 5, NULL,
                      &D_actor_356100_801732A8);
    }
    work->field_994 = work->field_5A & 0x3FF;
    coord           = arg0->extra.tmd->coords;
    d               = &delta;
    Actor356100_PositionDelta(coord, d);
    if (!overlayOutOfRange(d, 3000)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        work->field_0 = 6;
    }
}

static __inline__ void Actor356100_MoveForward(GpCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (Mc_SaveData.field_5C1 != 1) {
        head                  = SCRATCH_HEAD(SVECTOR) - 1;
        vec                   = head;
        SCRATCH_HEAD(SVECTOR) = vec;
        if (amount != 0) {
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(vec);
            gte_gpf12();
            gte_stsv(vec);
            coord->coord.t[0] += head->vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        SCRATCH_POP(SVECTOR);
    }
}

void func_actor_356100_80167A7C(Task* arg0)
{
    Actor356100Work*  work;
    TmdObject*        obj;
    GpCoord*          coord;
    u8*               head;
    ActorTurnScratch* turn;
    s16               angle;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 2;
        func_actor_356100_80163508(arg0);
        return;
    }
    head                                        = SCRATCH_HEAD(u8);
    ((ActorTurnScratch*)(head - 0xC))->delta.vx = work->field_C[work->field_14].x - arg0->extra.tmd->coords->coord.t[0];
    turn                                        = (ActorTurnScratch*)SCRATCH_PUSH_BYTES(0xC);
    turn->delta.vy                              = 0;
    turn->delta.vz                              = work->field_C[work->field_14].z - arg0->extra.tmd->coords->coord.t[2];
    if (!overlayOutOfRange(&turn->delta, 0xA0)) {
        if (work->field_14 == 0) {
            work->field_14 = 1;
        } else {
            work->field_14 = 0;
        }
    }
    func_actor_356100_80163508(arg0);
    coord           = arg0->extra.tmd->coords;
    angle           = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    turn->angle     = actorNormalizeYaw(angle);
    work->field_98E = turn->angle;
    if (turn->angle >= 0x21) {
        turn->angle = 0x20;
    }
    if (turn->angle < -0x20) {
        turn->angle = -0x20;
    }
    turn->angle = (u16)turn->angle + ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    if (work->field_97A == 0) {
        Actor356100_MoveForward(arg0->extra.tmd->coords, 10);
    }
    Actor356100_PushRecords(arg0->extra.tmd->coords, &work->field_A58, 3, 0x10);
    SCRATCH_POP(ActorTurnScratch);
    arg0->extra.tmd->coords->flg = 0;
}

/// Turn-and-push tick, the sibling of `func_actor_356100_80163E2C` above it and
/// the same body as `func_actor_401300_8013A208`. The live branch resets the
/// model and starts clip 1 at speed 0x10 with the 0x12 state parked in
/// `field_97E`; otherwise the turn scratch takes the player offset,
/// `actorPositionYaw` gives the wrapped turn, `field_98E` snapshots it,
/// it is clamped to [-0x40, 0x40] and the root yaw is re-derived from it. The
/// root is then pushed out of the `field_A58` collision records and one
/// normalised unit along its own Y column scaled by `field_B4C`, which decays
/// by 0xA per frame — once it reaches zero, or bit 0 of `field_68` is set, the
/// state moves to 9 and the turn scratch is given back.
void func_actor_356100_8016804C(Task* arg0)
{
    Actor356100Work*  work;
    GpEnemy*          enemy;
    TmdObject*        obj;
    GpCoord*          coord;
    ActorTurnScratch* turn;
    u16               next;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy           = arg0->spawnArg2;
        obj             = arg0->extra.tmd;
        work->field_97E = 0x12;
        work->field_978 = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC           = 0x180;
        enemy->node.state.b.flags = 0;
        work->field_990           = 0;
        work->field_982           = 0x1E;
    }
    SCRATCH_PUSH(ActorTurnScratch);
    turn            = SCRATCH_HEAD(ActorTurnScratch);
    turn->angle     = actorPositionYaw(arg0, &turn->delta, &Player_Status);
    work->field_98E = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->extra.tmd->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    Actor356100_PushRecords(arg0->extra.tmd->coords, &work->field_A58, 3, 0x10);
    actorMoveForwardNonzero(arg0->extra.tmd->coords, work->field_B4C);
    if (work->field_B4C > 0) {
        next            = work->field_B4C - 0xA;
        work->field_B4C = next;
        if ((s16)next < 0) {
            work->field_B4C = 0;
        }
    }
    func_actor_356100_80163508(arg0);
    if ((work->field_68 & 1) || work->field_B4C == 0) {
        work->field_0 = 9;
    }
    SCRATCH_POP(ActorTurnScratch);
}

/// `actorMoveForwardNonzero` testing the freeze flag through a
/// `McSaveData*` rather than `Mc_SaveData.field_5C1`, and without its zero-amount guard.
/// Reads the X component back through `vec`, as `Actor01900_StepForward` does —
/// the `head[-1]` spelling gives the scratch release value a register of its
/// own and costs three instructions here. Same body as
/// `Actor401300_MoveForwardSave`.
static __inline__ void Actor356100_StepForwardSave(McSaveData* save, GpCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (save->field_5C1 != 1) {
        head                  = SCRATCH_HEAD(SVECTOR);
        vec                   = head - 1;
        SCRATCH_HEAD(SVECTOR) = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0] += vec->vx;
        coord->coord.t[1] += vec->vy;
        coord->coord.t[2] += vec->vz;
        coord->flg         = 0;
        SCRATCH_POP(SVECTOR);
    }
}

/// `Actor356100_PushRecords` testing the freeze flag through a `McSaveData*`,
/// and giving the 0x14 bytes back through `G_SCRATCH_HEAD` itself rather than a
/// saved `void**` — the saved pointer keeps the 0x1F8003FC constant live in a
/// register across the release.
static __inline__ void Actor356100_PushRecordsSave(McSaveData* save, GpCoord* coord, GpRec18* rec, s32 count, s16 height)
{
    u8*               head;
    OverlayDeltaFlag* s;
    s32               val;

    if (save->field_5C1 != 1) {
        head = SCRATCH_HEAD(u8);
        SCRATCH_PUSH(OverlayDeltaFlag);
        s        = SCRATCH_HEAD(OverlayDeltaFlag);
        s->moved = 0;
        if (func_800E0C10(rec, &s->delta, count, NULL) != 0) {
            coord->coord.t[0] += ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
            coord->coord.t[1] += s->delta.vy.h.hi;
            coord->coord.t[2] += s->delta.vz.h.hi;
            val                = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[0]++;
                } else {
                    coord->coord.t[0]--;
                }
            }
            val = s->delta.vz.w;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[2]++;
                } else {
                    coord->coord.t[2]--;
                }
            }
        }
        coord->coord.t[1] += height;
        if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
            s->moved = 1;
        }
        SCRATCH_POP_BYTES(0x14);
    }
}

/// Turn-and-rescale tick, the sibling of `func_actor_356100_8016804C` above it
/// and the same body as `func_actor_401300_8013A5C0`. Going live resets the
/// model and starts clip 1 at speed 0x10 with the 0x13 state parked in
/// `field_97E`; otherwise the aim scratch takes the player offset,
/// `actorYawTo` gives the wrapped turn, `field_98E` snapshots it, it is
/// clamped to [-0x80, 0x80] and halved, the root yaw is re-derived from it and
/// the root coordinate rescaled to a uniform 0x1194. Once the state has settled
/// on 0x11 the collision step pushes the root out of the `field_A58` records
/// and one normalised unit back along its own Y column, both frozen while the
/// save flag is set, and past clip 0x13 the actor is leaned by ±0x4B0 into
/// state 7.
void func_actor_356100_801684F0(Task* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GpCoord*               coord;
    GpCoord*               cur;
    GpCoord*               root;
    SVECTOR**              scratch;
    Actor356100AimScratch* head;
    Actor356100AimScratch* aim;
    McSaveData*            save;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 0x16;
        work->field_97A = 0;
        work->field_97E = 2;
        func_actor_356100_80163508(arg0);
        return;
    }
    func_actor_356100_80163508(arg0);
    scratch           = (SVECTOR**)SCRATCH_HEAD_ADDR;
    cur               = arg0->extra.tmd->coords;
    head              = (Actor356100AimScratch*)SCRATCH_HEAD_AT(scratch, SVECTOR);
    head[-1].delta.vx = Player_Status.coordMtx->t[0] - cur->coord.t[0];
    aim               = (Actor356100AimScratch*)(SCRATCH_HEAD_AT(scratch, SVECTOR) = (SVECTOR*)(head - 1));
    aim->delta.vy     = Player_Status.coordMtx->t[1] - cur->coord.t[1];
    aim->delta.vz     = Player_Status.coordMtx->t[2] - cur->coord.t[2];
    aim->angle        = actorYawTo(arg0->extra.tmd->coords, head[-1].delta.vx, aim->delta.vz);
    work->field_98E   = aim->angle;
    if (ABS(aim->angle) <= 0x80 && work->field_97E == 2) {
        work->field_982 = 0x16;
        work->field_97E = 0x11;
        work->field_978 = 1;
        work->field_6   = 0;
        func_actor_356100_80163508(arg0);
    }
    if (aim->angle > 0x80) {
        aim->angle = 0x80;
    }
    if (aim->angle < -0x80) {
        aim->angle = -0x80;
    } else {
        aim->angle = aim->angle >> 1;
    }
    aim->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, aim->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_97E == 0x11) {
        work->field_6++;
        save  = &Mc_SaveData;
        coord = arg0->extra.tmd->coords;
        if (save->field_5C1 != 1) {
            Actor356100_StepForwardSave(save, coord, -0x10);
        }
        root = arg0->extra.tmd->coords;
        if (save->field_5C1 != 1) {
            Actor356100_PushRecordsSave(save, root, &work->field_A58, 3, 0x10);
        }
        arg0->extra.tmd->coords->flg = 0;
        if ((s16)work->field_6 >= 0x13) {
            if (work->field_98E <= 0) {
                Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, 0x4B0, 0);
            } else {
                Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    SCRATCH_POP(Actor356100AimScratch);
}

/// Turn the actor's facing onto the player in one step and rescale the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 1 at
/// speed 0x10 with the 0x13 state parked in `field_97E`, otherwise `field_6`
/// ticks over for the 0xB-frame transition, the aim scratch takes the player
/// offset, `actorPositionYaw` gives the wrapped turn, `field_98E`
/// snapshots it, the turn is clamped to [-0x20, 0x20] and the root yaw is
/// re-derived from it before `field_0` moves to `state` once the count-down
/// expires. Same body as `func_actor_401300_8013AAE8`.
void func_actor_356100_80168AFC(Task* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GpCoord*               coord;
    Actor356100AimScratch* aim;
    int                    state;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0x13;
        func_actor_356100_80163508(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6 = (s16)((u16)work->field_6 + 1);
    state         = 0xB;
    SCRATCH_PUSH(Actor356100AimScratch);
    aim                          = SCRATCH_HEAD(Actor356100AimScratch);
    arg0->extra.tmd->coords->flg = 0;
    if ((work->field_68 & 1) || ((s16)work->field_6 >= state)) {
        work->field_0 = state;
    }
    aim->angle      = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_98E = aim->angle;
    if (aim->angle >= 0x21) {
        aim->angle = 0x20;
    }
    if (aim->angle < -0x20) {
        aim->angle = -0x20;
    }
    coord      = arg0->extra.tmd->coords;
    aim->angle = (u16)aim->angle + ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, aim->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    func_actor_356100_80163508(arg0);
    SCRATCH_POP(Actor356100AimScratch);
}

/// Turn the actor's facing onto the player in 0x28 steps and rescale the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 2 at
/// speed 0x10 with the 0x13 state parked in `field_97E`, otherwise the aim
/// scratch takes the player offset, `actorPositionYaw` gives the wrapped
/// turn, `field_98E` walks toward it by at most 0x28 and the state flips to 0xB
/// once it has caught up. Same body as `func_actor_401300_8013AE48`.
void func_actor_356100_80168E44(Task* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GpCoord*               coord;
    Actor356100AimScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 1;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0x13;
        func_actor_356100_80163508(arg0);
        func_actor_356100_80163508(arg0);
        work->field_6   = 0;
        work->field_990 = 0;
        return;
    }
    SCRATCH_PUSH(Actor356100AimScratch);
    aim        = SCRATCH_HEAD(Actor356100AimScratch);
    aim->angle = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    if (work->field_98E < aim->angle) {
        if (aim->angle - work->field_98E > 0x28) {
            work->field_98E += 0x28;
        } else {
            work->field_98E = aim->angle;
        }
    } else if (work->field_98E - aim->angle > 0x28) {
        work->field_98E -= 0x28;
    } else {
        work->field_98E = aim->angle;
    }
    if (work->field_98E == aim->angle) {
        work->field_0 = 0xB;
    }
    coord      = arg0->extra.tmd->coords;
    aim->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, aim->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    work->field_978 = 2;
    func_actor_356100_80163508(arg0);
    SCRATCH_POP(Actor356100AimScratch);
}

/// The overlay's death-throes tick, the sibling of `func_actor_356100_80168E44`:
/// going live re-seeds the model (the enemy's link node, `obj->field_C`, the
/// 0x978..0x982 animation slots) and queues sound 0x550B0007 against the root
/// part, whose coordinate the live arm clears outright. Each frame then bumps
/// `field_6`, runs the clip and walks part 1's coordinate by a fixed 0x1044 /
/// 0x4AA per frame. Four frames each fire their own sound (0x550B0008 with the
/// 6/0xFF/0x80 pad rumble, 0x400D0002 with 8/0x7F/0x30, 0x400D0001 with
/// 6/0x7F/0x30, 0x550B0009 bare), and `field_6` 0x29..0x2D drives a 16-effect
/// 0x600FB burst over the model's part coordinates — 0x2E..0x31 the same burst
/// with six effects, alternating on the frame's parity.
///
/// The parity test re-reads `field_6` from memory rather than reusing the range
/// test's value (the two reads are what the original emits), so the read is
/// spelled volatile.
void func_actor_356100_80169180(Task* arg0)
{
    Actor356100Work* work;
    GpEnemy*         ctx;
    GpCoord*         coord;

    work = arg0->work;
    if (work->field_4 != 0) {
        s32 pan;

        ctx                    = arg0->spawnArg2;
        arg0->extra.tmd->flags = 0;
        Tmd_AllocBuffers(arg0->extra.tmd);
        ctx->node.state.b.flags = 1;
        work->field_97E         = 1;
        work->field_978         = 2;
        Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, 0x800, 1);
        coord                        = arg0->extra.tmd->coords;
        coord->coord.t[2]            = 0;
        coord->coord.t[1]            = 0;
        coord->coord.t[0]            = 0;
        arg0->extra.tmd->coords->flg = 0;
        Gp_UpdateCoord(arg0->extra.tmd->coords);
        work->field_6 = 0;
        pan           = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(0x550B0007, pan, (s8)gpGetObjDepth(&arg0->extra.tmd->coords[1]));
    }
    work->field_6 = (s16)((u16)work->field_6 + 1);
    func_actor_356100_80163508(arg0);
    arg0->extra.tmd->coords[1].coord.t[0] += 0x1044;
    arg0->extra.tmd->coords[1].coord.t[2] += 0x4AA;
    arg0->extra.tmd->coords[1].flg         = 0;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
    if (work->field_6 == 0x31) {
        s32 pan;

        pan = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(0x550B0008, pan, (s8)gpGetObjDepth(&arg0->extra.tmd->coords[1]));
        Gp_SpawnPadLerp(6, 0xFF, 0x80);
    }
    if (work->field_6 == 0x4D) {
        s32 pan;

        pan = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(0x400D0002, pan, (s8)gpGetObjDepth(&arg0->extra.tmd->coords[1]));
        Gp_SpawnPadLerp(8, 0x7F, 0x30);
    }
    if (work->field_6 == 0x58) {
        s32 pan;

        pan = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(0x400D0001, pan, (s8)gpGetObjDepth(&arg0->extra.tmd->coords[1]));
        Gp_SpawnPadLerp(6, 0x7F, 0x30);
    }
    if (work->field_6 == 0xCE) {
        s32 pan;

        pan = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(0x550B0009, pan, (s8)gpGetObjDepth(&arg0->extra.tmd->coords[1]));
    }
    if ((u32)((u16)work->field_6 - 0x29) < 5U) {
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[3], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x10], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[1], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x12], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[2], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x11], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[3], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[4], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[5], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x10], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[1], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x13], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x11], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x10], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[5], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x12], 0, 0);
    }
    if ((u32)((u16)work->field_6 - 0x2E) < 4U) {
        if (!(*(volatile u16*)&work->field_6 & 1)) {
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[2], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x11], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[3], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[4], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[5], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x10], 0, 0);
        } else {
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[1], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x13], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x11], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x10], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[5], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->extra.tmd->coords[0x12], 0, 0);
        }
    }
}

/// The 31 state handlers `func_actor_356100_80169854` dispatches through, in
/// state order; entry 0x1D has no handler and the `field_0` values the ticks
/// park (0, 6, 7, 8, 9, 0xB, 0xC, 0x10, 0x11, 0x13, 0x15, 0x16, 0x18, 0x19,
/// 0x1E) are its live entries. Same role as `Actor01900_D1728C`.
const Actor356100StateTable D_actor_356100_80161EC4 = {
    {
        func_actor_356100_8016A1D8,
        func_actor_356100_8016A21C,
        func_actor_356100_8016A2AC,
        func_actor_356100_8016A340,
        func_actor_356100_80163CD4,
        func_actor_356100_8016A3D4,
        func_actor_356100_80163E2C,
        func_actor_356100_80164158,
        func_actor_356100_80164ACC,
        func_actor_356100_801653F4,
        func_actor_356100_80165B30,
        func_actor_356100_80166018,
        func_actor_356100_801666B4,
        func_actor_356100_8016A468,
        func_actor_356100_801668FC,
        func_actor_356100_8016A550,
        func_actor_356100_8016A5DC,
        func_actor_356100_8016A668,
        func_actor_356100_80166CF0,
        func_actor_356100_8016A710,
        func_actor_356100_8016A834,
        func_actor_356100_80167358,
        func_actor_356100_80167584,
        func_actor_356100_80167818,
        func_actor_356100_80167A7C,
        func_actor_356100_801684F0,
        func_actor_356100_8016804C,
        func_actor_356100_80168AFC,
        func_actor_356100_80168E44,
        NULL,
        func_actor_356100_80169180,
    }
};

/// The enemy's three task-state handlers, which `func_actor_356100_8016A910`
/// runs by `Task::state`: setup, per-frame tick and teardown.
const GpEnemyTaskFuncTable3 D_actor_356100_80161F40 = {
    func_actor_356100_8016382C,
    func_actor_356100_80169854,
    Gp_DestroyEnemy,
};

void func_actor_356100_80169854(GpEnemy* arg0, Task* arg1)
{
    VECTOR                  pos;
    Actor356100StateTable   tbl;
    Actor356100Work*        work;
    Actor356100GroundCoord* blk;
    s16                     next;

    work   = arg1->work;
    tbl    = D_actor_356100_80161EC4;
    pos.vx = arg1->extra.tmd->coords[1].workm.t[0];
    pos.vy = arg1->extra.tmd->coords[1].workm.t[1];
    pos.vz = arg1->extra.tmd->coords[1].workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    switch (Gp_StateF0.field_4) {
        case 0:
            if (work->field_0 != 0 && work->field_0 != 0x15 && work->field_0 != 0x1E) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->extra.tmd->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            break;
        case 1:
            if (work->field_0 != 0 && work->field_0 != 0x15 && work->field_0 != 0x1E) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->extra.tmd->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            return;
        case 2:
            arg1->extra.tmd->flags = 0x80;
            return;
    }
    SCRATCH_PUSH(Actor356100GroundCoord);
    blk = SCRATCH_HEAD(Actor356100GroundCoord);
    if (work->field_0 == 0x1E) {
        MATRIX* m;

        blk->v.vx = blk->v.vy = blk->v.vz = 0;
        actorTransformToView(&arg1->extra.tmd->coords[1], &blk->v);
        m                     = &blk->coord.coord;
        *(s32*)&m->m[0][0]    = 0x1000;
        *(s32*)&m->m[0][2]    = 0;
        *(s32*)&m->m[1][1]    = 0x1000;
        *(s32*)&m->m[2][0]    = 0;
        m->m[2][2]            = 0x1000;
        blk->coord.sub        = &gGfxViewCoord;
        blk->coord.coord.t[0] = blk->v.vx;
        blk->coord.coord.t[1] = 0;
        blk->coord.coord.t[2] = blk->v.vz;
        blk->coord.flg        = 0;
        Gp_UpdateCoord(&blk->coord);
        Gp_DrawEffGroundQuad((VECTOR3*)blk->coord.workm.t, 0x280, Gp_State1C->groundShade);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    tbl.f[work->field_0](arg1);
    if (Gp_StateF0.field_3 == 1) {
        if (work->field_0 == 0x18) {
            work->field_0 = 6;
        }
    }
    blk->v.vx = 0;
    blk->v.vy = 0;
    blk->v.vz = 0;
    actorTransformToView(&arg1->extra.tmd->coords[2], &blk->v);
    work->field_B6C[work->field_BBC].vx = blk->v.vx;
    work->field_B6C[work->field_BBC].vy = blk->v.vy;
    work->field_B6C[work->field_BBC].vz = blk->v.vz;
    SCRATCH_POP(Actor356100GroundCoord);
    next            = (u16)work->field_BBC + 1;
    work->field_BBC = next;
    if (next == 7) {
        work->field_BBC = 0;
    }
    if ((u32)((u16)work->field_97E - 0x14) < 2U) {
        arg0->bodyPos.vx = work->field_B6C[work->field_BBC].vx;
        arg0->bodyPos.vy = work->field_B6C[work->field_BBC].vy;
        arg0->bodyPos.vz = work->field_B6C[work->field_BBC].vz;
    } else {
        arg0->bodyPos.vx = blk->v.vx;
        arg0->bodyPos.vy = blk->v.vy;
        arg0->bodyPos.vz = blk->v.vz;
    }
    arg0->coord = &gGfxViewCoord;
}

s32 func_actor_356100_80169E5C(void)
{
    return 0;
}

/// Applies a model-visibility request to the actor: 0 hides the model and
/// rebuilds its buffers, 1 shows it and rebuilds them with the state set to
/// 0x18, 2 sets model flag 4 on top of the current flags, and 3 clears every
/// other flag before setting 4. All but 1 reset the state to 0.
s32 func_actor_356100_80169E64(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor356100Work* work;

    obj  = task->extra.tmd;
    work = (Actor356100Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

/// Whether the actor should keep acting: 1 while its enemy still has HP;
/// once it is down, 0 when the model is flagged for teardown (0x80) or has
/// flag 2 set, 1 otherwise.
s32 func_actor_356100_80169F24(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        return 1;
    }

    flags   = task->extra.tmd->flags;
    mask80  = flags;
    mask80 &= 0x80;
    mask2   = flags & 2;
    if (mask80 != 0) {
        return 0;
    }

    ret = 0;
    if (mask2 == 0) {
        ret = 1;
        SOFT_BARRIER();
    }
    return ret;
}

/// Places the model's root coordinate from `placement` (translation, then the
/// X, Y and Z rotations in turn) and stores the resulting heading, `ratan2`
/// of the rotation's Z axis, in the work block's `yaw`.
s32 func_actor_356100_80169F74(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord*         coord;
    s32              mx;
    s32              mz;
    Actor356100Work* work;

    work                                = (Actor356100Work*)task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, 0);
    task->extra.tmd->coords->flg = 0;
    coord                        = task->extra.tmd->coords;
    mx                           = coord->coord.m[2][0];
    mz                           = coord->coord.m[2][2];
    work->yaw                    = ratan2(-mx, mz);
    return 1;
}

/// Moves the actor out of state 0xD: to 0xE while the player has HP left,
/// to 0x16 once it has run out.
s32 func_actor_356100_8016A074(Task* task)
{
    Actor356100Work* work = (Actor356100Work*)task->work;
    PlayerStatus*    cfg  = &Player_Status;

    if (work->field_0 == 0xD) {
        if (cfg->hp > 0) {
            work->field_0 = 0xE;
        } else {
            work->field_0 = 0x16;
        }
    }
    return 1;
}

s32 func_actor_356100_8016A0B8(Task* arg0, s32 arg1, Actor356100Event* arg2)
{
    Actor356100Work* work = arg0->work;
    s32              code;

    work->field_B58[0] = arg2->b[0];
    work->field_B58[1] = arg2->b[1];
    work->field_B58[2] = arg2->b[2];
    if (arg2->w[0] == 0xB05) {
        code = arg2->w[1];
        switch (code) {
            case 0:
            case 2:
                work->field_0 = 0;
                return 1;
            case 1:
                work->field_0   = 0x1E;
                work->field_97E = code;
                work->field_978 = 2;
                return 1;
            default:
                return 0;
        }
    }
    return 0;
}

void func_actor_356100_8016A158(Task* task)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = (Actor356100Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_B5C != NULL) {
            taskKill(work->field_B5C);
        }
        if (work->field_B60 != NULL) {
            taskKill(work->field_B60);
        }
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_356100_8016A1D8(Task* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 1;
        obj->flags                                     |= 0x80;
    }
}

void func_actor_356100_8016A21C(Task* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_97A = 0;
        work->field_982 = 0x10;
        work->field_97E = 2;
        func_actor_356100_80163508(arg0);
    } else {
        arg0->extra.tmd->coords->flg = 0;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_8016A2AC(Task* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 3;
        func_actor_356100_80163508(arg0);
    } else {
        arg0->extra.tmd->coords->flg = 0;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_8016A340(Task* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0xB;
        func_actor_356100_80163508(arg0);
    } else {
        arg0->extra.tmd->coords->flg = 0;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_8016A3D4(Task* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0xB;
        func_actor_356100_80163508(arg0);
    } else {
        arg0->extra.tmd->coords->flg = 0;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_8016A468(Task* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    GpAnimArg*       msg;
    void*            player;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_982 = 0x10;
        work->field_97E = 6;
        work->field_978 = 2;
        msg             = &D_actor_356100_80173244;
        msg->field_4    = 2;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)msg, 0);
        player = gameGetPtrSlot(3);
        Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair(enemy, 0), 0);
    }
    if (work->field_68 & 2) {
        work->field_0 = 0xE;
    }
    work->field_974 = work->field_5A & 0x3FF;
    func_actor_356100_80163508(arg0);
}

void func_actor_356100_8016A550(Task* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0;
        work->field_9BC           = 0x180;
        enemy->node.state.b.flags = 0;
        work->field_978           = 2;
        work->field_97E           = 8;
        work->field_990           = 0;
        work->field_98E           = 0;
        work->field_982           = work->field_984;
    }
    func_actor_356100_80163508(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

void func_actor_356100_8016A5DC(Task* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0;
        work->field_9BC           = 0x180;
        enemy->node.state.b.flags = 0;
        work->field_978           = 2;
        work->field_97E           = 0x16;
        work->field_990           = 0;
        work->field_98E           = 0;
        work->field_982           = work->field_984;
    }
    func_actor_356100_80163508(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

void func_actor_356100_8016A668(Task* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->field_6 = work->field_B54 + (((u32)Gp_LcgState >> 16) & 0xF);
    }
    if ((s16)--work->field_6 < 0) {
        switch (work->field_97E) {
            case 0xB:
                work->field_0 = 0xF;
                break;
            case 0xC:
                work->field_0 = 0x10;
                break;
        }
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x15;
    }
}

void func_actor_356100_8016A710(Task* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0;
        work->field_9BC           = 0x180;
        enemy->node.state.b.flags = 0;
        work->field_978           = 1;
        work->field_97E           = 0xA;
        work->field_982           = 0x10;
        work->field_990           = 0;
        work->field_98E           = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    func_actor_356100_80163508(arg0);
    if (work->field_68 & 1) {
        if (work->field_97E == 0xA) {
            work->field_97E = 0xB;
            work->field_978 = 2;
            func_actor_356100_80163508(arg0);
        }
        if ((work->field_68 & 1) && (work->field_97E == 0xB)) {
            if (enemy->hp > 0) {
                if (work->field_B3A <= 0) {
                    work->field_0 = 0x11;
                } else {
                    work->field_0 = 4;
                }
            } else {
                work->field_0 = 0x15;
            }
        }
    }
}

void func_actor_356100_8016A834(Task* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0;
        work->field_9BC           = 0x180;
        enemy->node.state.b.flags = 0;
        work->field_978           = 1;
        work->field_97E           = 0xC;
        work->field_982           = 0x10;
        work->field_990           = 0;
        work->field_98E           = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    func_actor_356100_80163508(arg0);
    if (work->field_68 & 1) {
        if (enemy->hp > 0) {
            if (work->field_B3A <= 0) {
                work->field_0 = 0x11;
            } else {
                work->field_0 = 4;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

/// Runs the handler for the task's current state, copying the table onto the
/// stack before the call.
void func_actor_356100_8016A910(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_356100_80161F40;
    sp.funcs[task->state](task->spawnArg2, task);
}
