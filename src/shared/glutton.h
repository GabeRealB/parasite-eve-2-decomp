/* The Glutton (actor_403200 and actor_444000), fought in the Shelter B3
 * dumping hole / garbage incinerator (area 0x27). A large jointed host with seven escort models charges along the
 * arena's x axis, turns and charges back, pushing a collision wall rebuilt in
 * front of it. It yaws and pitches its neck parts toward the player and swings
 * a seven-segment limb whose extension is a shared reach counter. It cross-
 * fades three animation pairs. It flings sub-enemies: a chunk thrown forward
 * from escort 0 that falls with a ground shadow; a glob spat from escort 1
 * that bounces, stretches and, if the player is in reach, engulfs them by
 * installing a caught animation on the player; debris chunks from the owner's
 * part 3 that drop, slide and settle with smoke puffs; blobs launched high
 * off-screen that rain onto points on a ring around the host and splat flat;
 * and spinners that wait hidden, then spiral toward a target point. A shared
 * end flag makes every sub-enemy tear itself down when the fight ends. It uses
 * ActorContact_TurnJoint and ActorContact_PushContact from the existing
 * actor_contacts library.
 *
 * GLUTTON_ROOM configures the shared code for one encounter at compile time.
 * Each carrier must define it before including this header, using
 * GLUTTON_DUMPING_HOLE (actor_403200) or GLUTTON_INCINERATOR (actor_444000),
 * and retain that binding through every shared fragment. This header defines
 * the selector values and rejects unsupported values before selecting the
 * encounter's hit-effect dimensions. The fragments use the same selection
 * for damage, escort and shake behavior.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_GLUTTON_H
#define SRC_SHARED_GLUTTON_H

/// Binds the shared shake helper to the current instance's borrowed host task.
///
/// Must expand to a side-effect-free `Task*` expression. The default selects
/// actor_403200's private pointer; actor_444000 overrides it before this header
/// with its storage record's `task` member. Both carriers declare the storage
/// before including the shake helper. Evaluated once per shake request; the
/// host and its `GluttonWork` must be alive. Spawn publishes the pointer,
/// and teardown leaves it stale, so requests require a successfully spawned,
/// still-live host. The binding neither owns the task nor checks for NULL.
#ifndef GLUTTON_HOST_TASK
#define GLUTTON_HOST_TASK (_gGluttonHostTask)
#endif

/// Compile-time `GLUTTON_ROOM` selector for the Shelter B3 dumping-hole Glutton.
///
/// `actor_403200` binds `GLUTTON_ROOM` to this value before including this
/// header and keeps that binding for the shared fragments. It selects the
/// encounter's hit, escort and shake behavior. This dimensionless integer must
/// remain a macro because the fragments compare it in `#if` directives.
#define GLUTTON_DUMPING_HOLE 1

/// Compile-time `GLUTTON_ROOM` selector for the Shelter B3 garbage-incinerator Glutton.
///
/// `actor_444000` binds `GLUTTON_ROOM` to this value before including this
/// header and keeps that binding for all shared fragments. It selects the
/// encounter's hit reactions, escort setup and shake handling. This
/// dimensionless integer must remain a macro because the fragments compare
/// `GLUTTON_ROOM` in `#if` directives.
#define GLUTTON_INCINERATOR 2
#ifndef GLUTTON_ROOM
#error "define GLUTTON_ROOM (GLUTTON_DUMPING_HOLE or GLUTTON_INCINERATOR) before including glutton.h"
#elif GLUTTON_ROOM != GLUTTON_DUMPING_HOLE && GLUTTON_ROOM != GLUTTON_INCINERATOR
#error "GLUTTON_ROOM must be GLUTTON_DUMPING_HOLE or GLUTTON_INCINERATOR"
#endif

/* Extent of the 0x6009C effect a group-0 hit spawns; the Incinerator's is
 * half the Dumping Hole's. */
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
#define GLUTTON_GROUP0_HIT_FX_Y 0x320
#define GLUTTON_GROUP0_HIT_FX_Z 0x3E8
#else
#define GLUTTON_GROUP0_HIT_FX_Y 0x190
#define GLUTTON_GROUP0_HIT_FX_Z 0x1F4
#endif

#include "types.h"

#include "actors/actor.h"

#include "main/coord.h"
#include "main/gfx_types.h"
#include "main/task_types.h"

/// Graphics coordinate node with packed word access to its local rotation.
///
/// `node` is the transform node itself: what composition, parenting and every
/// `GfxCoord` consumer take. `packed` views the same storage with the local
/// matrix as a `GfxMatrix`, so its 3x3 can be written as whole words; an
/// identity is four word stores and one halfword store. Both views are live at
/// once and neither owns anything the other does not.
///
/// The Glutton uses this wherever it builds a node's rotation in place: the
/// host's free coordinate, the coordinate hung beneath its part 4, and the
/// frame-local node under a falling rain blob's ground shadow.
typedef union {
    GfxCoord node;
    struct {
        u32       composeStamp; // `node.composeStamp`
        GfxMatrix coord;        // `node.coord`, with word access to its coefficients
    } packed;
} GluttonCoord;
STATIC_ASSERT_SIZEOF(GluttonCoord, 0x50);

/// One of the nine back-to-back collision groups in `GluttonWork` at
/// 0x7F4. `obj` is the `WorldCollisionBody` the gameplay collision list carries and `recs`
/// is the `WorldCollisionContact` table it fills in for that part, which is why the stride is
/// 0x98. `obj.field_8` is the part's own coordinate -- what the hit handler
/// spawns the hit effect on. The same shape as `GluttonHitGroup`.
typedef struct GluttonHitGroup {
    WorldCollisionBody    obj;
    WorldCollisionContact recs[5];
} GluttonHitGroup;
STATIC_ASSERT_SIZEOF(GluttonHitGroup, 0x98);

/// 0x30-byte scratchpad frame the group-0 hit handler
/// `gluttonHitGroup0` carves off `SCRATCH_STACK_CURSOR` for the one hit it
/// takes this frame. `pos` is the contact point copied out of the `WorldCollisionContact`;
/// `delta` is the player-relative offset whose length is `dist`, the range
/// `Gp_ComputeDamage` scales `damage` by. `rot` doubles as `Gp_SpawnEff`'s
/// rotation argument and, afterwards, as the workspace for the contact point
/// relative to the part's world translation, which `angle` is the yaw of.
typedef struct GluttonHitScratch {
    VECTOR3 delta;
    byte    pad_C[0x4];
    SVECTOR rot;
    SVECTOR pos;
    s32     id;     // attack id of the hit that landed, 0 for none
    u32     damage; // HP taken off the enemy
    s32     dist;   // distance from the player, in world units
    s16     angle;  // yaw of the contact point, wrapped to +/-0x800
    byte    pad_2E[0x2];
} GluttonHitScratch;
STATIC_ASSERT_SIZEOF(GluttonHitScratch, 0x30);

/// 0xC-byte scratchpad frame `func_actor_403200_8013EF6C` carves off
/// the scratch-pad stack for the escort-spawn tick: `delta` is the player-relative
/// offset the tick yaws the host by, and `i` is the escort slot the loop and
/// the 0x7DB message both index `GluttonWork::field_EE8` with.
typedef struct GluttonSpawnScratch {
    SVECTOR delta;
    byte    pad_8[0x2];
    s16     i; // escort slot, 0 or 1
} GluttonSpawnScratch;
STATIC_ASSERT_SIZEOF(GluttonSpawnScratch, 0xC);

/// Scratchpad frame the hit-effect spawner `func_actor_403200_80134044` carves
/// off the scratch-pad stack to hand `func_800FDB18` an effect rotation together with
/// the `EffectSpawnArg` naming the coordinate the effect hangs off.
typedef struct GluttonEffScratch {
    SVECTOR        rot; // effect rotation, chosen from the attack's param 0
    EffectSpawnArg eff; // coordinate, 0x500, 3
} GluttonEffScratch;
STATIC_ASSERT_SIZEOF(GluttonEffScratch, 0x10);

/// Work block of the enemies spawned through `D_actor_403200_80131E90`,
/// `D_actor_403200_80131E9C` and `D_actor_403200_80131F04`: their spawn states
/// allocate it with `memCalloc(0x1C0, 0)` and park it in the task's
/// `Task::work` slot, so the size below is the allocation, not a guess.
///
/// The spawn states drop the model onto the view coordinate and hang one or two
/// `WorldCollisionBody` collision bodies off it. `rec0` is the table the first node carries,
/// `rec1` the second's; the two matrices are handed out through the task's
/// `TmdObject::lightMtx` / `colorMtx`. `field_1AA` is a ninth of the model's
/// height and `field_1AC` the step counter, both re-read by the states that
/// follow the spawn.
typedef struct GluttonGrabWork {
    /// Horizontal gap to the player, a fifteenth of which the later states add
    /// to the model each step; only `vx` and `vz` are filled in here.
    VECTOR3 vel;
    byte    pad_C[0x54];
    /// The work block's own coordinate, parented to the view coordinate and
    /// kept tracking the model's world position so the ground marker under it
    /// can be drawn from `coord.workm.t`.
    GfxCoord coord;
    /// The two collision bodies on object lists 3 and 2.
    WorldCollisionBody obj0;
    WorldCollisionBody obj1;
    /// Their collision-record tables.
    WorldCollisionContact rec0;
    WorldCollisionContact rec1;
    byte                  pad_120[0x30];
    /// The colour and light matrices borrowed through the task's
    /// `TmdObject::colorMtx` and `TmdObject::lightMtx`.
    MATRIX colorMtx;
    MATRIX lightMtx;
    byte   pad_190[0x4];
    /// Message 0x3FF payload the hold states send the player, by address.
    AnimationPlayRequest anim;
    /// Armed to 1 by the spawn state `func_actor_403200_8013509C` once the
    /// model has been stood up on its escort's part 1; the states that follow
    /// re-arm the step counter and the first display node on the tick they see
    /// it set. Same slot and role as `GluttonGrabWork::field_1A8`.
    s16  field_1A8;
    s16  field_1AA;
    s16  field_1AC;
    byte pad_1AE[0x2];
    /// Radius of the ground marker, in eighths once shifted down.
    u16 field_1B0;
    /// Set while the player animation this enemy sent is installed, so only
    /// the state that set it sends the cancel.
    s16 field_1B2;
    /// The state the dispatcher last ran, so it can spot a change.
    s16  field_1B4;
    byte pad_1B6[0xA];
} GluttonGrabWork;
STATIC_ASSERT_SIZEOF(GluttonGrabWork, 0x1C0);

/// Work block of the enemy dispatched through `D_actor_403200_80131F14`, the one
/// that rises out of view and slams back down onto the floor. Its spawn state
/// allocates it with `memCalloc(0x1C0, 0)` and parks it in the task's
/// `Task::work` slot, so the size is the allocation.
///
/// `target` is the landing point the spawn state picks; `coord` is the
/// coordinate its shadow marker is drawn at, kept on the floor directly under
/// the model and refreshed every step; `obj` is its collision node, whose
/// `radius` is the marker size, carrying the one-entry `rec` table. `timer` is
/// the step counter of the current state.
typedef struct GluttonDropWork {
    VECTOR3               target;
    byte                  pad_C[0x4];
    GfxCoord              coord;
    byte                  pad_60[0x50];
    WorldCollisionBody    obj;
    byte                  pad_D0[0x20];
    WorldCollisionContact rec;
    byte                  pad_108[0x88];
    /// The effect the spawn state starts, reparented onto the task so it dies
    /// with it; the landing state tells it to finish.
    EffectWork* eff;
    byte        pad_194[0x16];
    s16         field_1AA;
    u16         timer;
    /// Per-step bias of the rise and fall, rolled off the LCG.
    s16  field_1AE;
    byte pad_1B0[0x10];
} GluttonDropWork;
STATIC_ASSERT_SIZEOF(GluttonDropWork, 0x1C0);

/// Work block of the spinner enemy dispatched through `D_actor_403200_80131F28`:
/// its spawn state allocates it with `memCalloc(0xA0, 0)` and parks it in the
/// task's `Task::work` slot. `spin` counts down while the model only yaws in
/// place and is also the phase that yaw follows; `field_98` is the homing
/// speed and radius and `field_96` the step count that accelerates it.
typedef struct GluttonSpinnerWork {
    byte   pad_0[0x50];
    MATRIX colorMtx;
    MATRIX lightMtx;
    /// Set when the dispatcher sees the state change, cleared when it has not.
    s16  field_90;
    byte pad_92[0x2];
    /// The state the dispatcher last ran, so it can spot the change.
    s16  field_94;
    s16  field_96;
    s16  field_98;
    byte pad_9A[0x2];
    u8   spin;
    byte pad_9D[0x3];
} GluttonSpinnerWork;
STATIC_ASSERT_SIZEOF(GluttonSpinnerWork, 0xA0);

/// Work block of the boss itself, allocated zeroed by its spawn state and
/// kept at `Task::work`. The leading halfwords are the state dispatcher's:
/// `field_0` is the state index the per-frame dispatcher runs, `field_2` the
/// state it ran on the previous tick, `field_4` is raised on the tick the
/// state changes, and `field_6` counts the ticks spent in the current state.
/// Six animation blocks follow, pairing up so that each even member drives
/// the model and the odd one is the pose blended into it; then the escort
/// pose driver, the animation and blend state, the collision groups, the
/// lighting, the boss's counters and its escorts.
typedef struct GluttonWork {
    /// State index. `func_actor_403200_8013FB54` indexes the local copy of
    /// `D_actor_403200_80132154` with it, and whenever it differs from
    /// `field_2` it flags the change in `field_4` and re-arms the `field_6`
    /// counter. The same three fields and that same test appear in the sibling
    /// actor overlays that share this dispatcher.
    /* 0x000 */ s16 field_0;
    /// The state index `func_actor_403200_8013FB54` ran on the previous tick,
    /// so it can spot the change.
    /* 0x002 */ s16 field_2;
    /// Set on the tick the dispatcher sees a state change, cleared on every
    /// other tick. `func_actor_403200_8014123C` reads it to re-arm `field_6`
    /// once more, which the dispatcher has already done.
    /* 0x004 */ s16 field_4;
    /// Per-state counter: cleared on a state change, otherwise incremented
    /// (saturating at 0x7FFF). State handlers fire one-shot cues on the ticks
    /// it reaches a given value.
    /* 0x006 */ s16  field_6;
    /* 0x008 */ byte pad_8[0x4];
    /// Six back-to-back animation blocks, each an `AnimationContext` followed by its
    /// own `AnimationSlot[N]` and an N-entry 0x10-byte pose table -- the three
    /// argument groups the spawn state hands `animationInitContext`. They pair up
    /// (0/1, 2/3, 4/5), eight slots in the first pair and four in the others;
    /// the even member drives the model and the odd one is the pose blended
    /// into it. The states latch their one-shot cues on `slots0[n].currentPose.indices.recordIndex`.
    /* 0x00C */ AnimationContext anim0;
    /* 0x020 */ AnimationSlot    slots0[8];
    /* 0x160 */ byte             aux0[0x80];
    /* 0x1E0 */ AnimationContext anim1;
    /* 0x1F4 */ AnimationSlot    slots1[8];
    /* 0x334 */ byte             aux1[0x80];
    /* 0x3B4 */ AnimationContext anim2;
    /* 0x3C8 */ AnimationSlot    slots2[4];
    /* 0x468 */ byte             aux2[0x40];
    /* 0x4A8 */ AnimationContext anim3;
    /* 0x4BC */ AnimationSlot    slots3[4];
    /* 0x55C */ byte             aux3[0x40];
    /* 0x59C */ AnimationContext anim4;
    /* 0x5B0 */ AnimationSlot    slots4[4];
    /* 0x650 */ byte             aux4[0x40];
    /* 0x690 */ AnimationContext anim5;
    /* 0x6A4 */ AnimationSlot    slots5[4];
    /* 0x744 */ byte             aux5[0x40];
    /// Per-part yaw the fifth escort's model is being driven to, one entry per
    /// part, and the angle each part is currently at. The escort pose driver
    /// picks the targets from `field_7A4` and walks every `field_794` toward
    /// its `field_784` by at most `field_7A6` a call.
    /* 0x784 */ s16  field_784[7];
    /* 0x792 */ byte pad_792[0x2];
    /* 0x794 */ s16  field_794[7];
    /* 0x7A2 */ byte pad_7A2[0x2];
    /// Escort pose index, written 3 by the re-arm path of the per-frame body
    /// and cleared once the shared countdown below has run out.
    /* 0x7A4 */ s16 field_7A4;
    /// Most a `field_794` entry may move in one call.
    /* 0x7A6 */ s16 field_7A6;
    /// The masked `slots0[3].currentPose.indices.recordIndex` frame the launch state last saw, so each of its
    /// four one-shot cues only fires on the step the animation first reaches
    /// that frame.
    /* 0x7A8 */ s32 field_7A8;
    /// The masked `slots0[1]` / `slots0[2]` frame the stand-up tick last saw, so
    /// each of its one-shot cues only fires on the step the animation first
    /// reaches that frame.
    /* 0x7AC */ s32 field_7AC;
    /* 0x7B0 */ s8  field_7B0;
    /// Set while the blended animation path runs.
    /* 0x7B1 */ s8 field_7B1;
    /// Animation id currently playing; the slot reseed latches `field_7B3`
    /// here once it has reseeded every slot.
    /* 0x7B2 */ s8 field_7B2;
    /* 0x7B3 */ s8 field_7B3;
    /// Frames since the animation block was re-armed.
    /* 0x7B4 */ u16 field_7B4;
    /// The `AnimationSlot.rate` the even animation members tick at; the launch
    /// state arms it to 0x40 and then to 0x10.
    /* 0x7B6 */ s16 field_7B6;
    /* 0x7B8 */ s16 field_7B8;
    /// Set to 2 to start a blend on the next animation step, which then moves
    /// it on to 3.
    /* 0x7BA */ s16 field_7BA;
    /// Animation id the blend seeds the odd members' slots with.
    /* 0x7BC */ s16 field_7BC;
    /// The `AnimationSlot.rate` the odd members tick at while blending.
    /* 0x7BE */ s16 field_7BE;
    /// Blend weight of the odd member in the pose written to the even one, out
    /// of 0x1000.
    /* 0x7C0 */ s16  field_7C0;
    /* 0x7C2 */ byte pad_7C2[0x2];
    /// Cleared alongside `field_7C8` by the group-0 hit handler.
    /* 0x7C4 */ s16  field_7C4;
    /* 0x7C6 */ byte pad_7C6[0x2];
    /// Cleared alongside `field_7C4` by the group-0 hit handler.
    /* 0x7C8 */ s16 field_7C8;
    /// Frames since the arena tick last sent the player its message 0x3FF
    /// animation; the retries in `func_actor_403200_8013FB54` are bounded by
    /// it.
    /* 0x7CA */ u16  field_7CA;
    /* 0x7CC */ byte pad_7CC[0x4];
    /// Start of a 0x20-byte run cleared whenever the animation block is
    /// re-armed.
    /* 0x7D0 */ byte field_7D0[0x8];
    /// The masked `slots0[2].currentPose.indices.recordIndex` frame the per-frame body last saw, so each of its
    /// two one-shot cues only fires on the step the animation first reaches
    /// that frame.
    /* 0x7D8 */ s32  field_7D8;
    /* 0x7DC */ byte pad_7DC[0x16];
    /// Set once the session reports the view ready, so the one-shot setup runs
    /// a single time. The spawn state clears the same byte.
    /* 0x7F2 */ s8 field_7F2;
    /// Cleared by the state-change reset to mark the work block as re-armed.
    /* 0x7F3 */ u8 field_7F3;
    /// The nine back-to-back collision groups, one per model part: each is the
    /// `WorldCollisionBody` the gameplay collision list carries plus the `WorldCollisionContact` table it
    /// fills in.
    /* 0x7F4 */ GluttonHitGroup hits[9];
    /// The tenth collision object, the one the swipe tick raises `flags` bit
    /// 0x8000 on while the swipe is live.
    /* 0xD4C */ WorldCollisionBody    obj;
    /* 0xD6C */ WorldCollisionCapsule d4rec;
    /// The five records the tenth collision object carries, walked by the swipe
    /// tick for the one whose high half is 0x10000.
    /* 0xD84 */ WorldCollisionContact recs2[5];
    /// The light and colour matrices the spawn state points the host model
    /// and its escorts at (`TmdObject::lightMtx` / `colorMtx`).
    /* 0xDFC */ MATRIX lightMtx;
    /* 0xE1C */ MATRIX colorMtx;
    /// Free coordinate the swipe tick clears and pushes through
    /// `Gp_UpdateCoord` every step; `coord` is the matrix `gfxRotMatrixY`
    /// rebuilds from `field_7C8`. The spawn state seeds it with the identity
    /// through the word view.
    /* 0xE3C */ GluttonCoord field_E3C;
    /// `Gp_GetIdParam2` of the hit the group-0 handler took this frame; the
    /// sibling slots carry the other groups' ids.
    /* 0xE8C */ s16 field_E8C;
    /* 0xE8E */ s16 field_E8E;
    /* 0xE90 */ s16 field_E90;
    /* 0xE92 */ s16 field_E92;
    /// Yaw the upkeep tick walks toward `field_E96` in steps of 0x32, snapping
    /// once the two are within 0x33 of each other.
    /* 0xE94 */ s16 field_E94;
    /// Yaw target the re-arm path arms to 0xC80.
    /* 0xE96 */ s16 field_E96;
    /// Companion value handed to the follow helper alongside `field_E94`.
    /* 0xE98 */ s16  field_E98;
    /* 0xE9A */ byte pad_E9A[0x12];
    /// Screen-shake level `gluttonShakeTick` drives, and the level
    /// armed last tick in `field_EAD`; a change from the armed level starts a
    /// shake.
    /* 0xEAC */ u8 field_EAC;
    /* 0xEAD */ u8 field_EAD;
    /* 0xEAE */ u8 field_EAE;
    /* 0xEAF */ s8 field_EAF;
    /// Message 0x3FF payload the launch state sends the player.
    /* 0xEB0 */ AnimationPlayRequest anim;
    /// First three bytes of the last 0x7DB payload received.
    /* 0xEC4 */ u8   field_EC4;
    /* 0xEC5 */ u8   field_EC5;
    /* 0xEC6 */ u8   field_EC6;
    /* 0xEC7 */ byte pad_EC7;
    /// Set to 1 while the player holds the animation the stand-up state hands
    /// over in its message 0x3FF.
    /* 0xEC8 */ s16 field_EC8;
    /// The reply the swipe tick's hold request (message 0x3F9) came back with,
    /// 1 when the player took it.
    /* 0xECA */ s16 field_ECA;
    /// The seven escorts the spawn state starts; the state-change reset walks
    /// them to push the host's `TmdObject::flags` onto each escort's own model
    /// object.
    /* 0xECC */ Enemy* field_ECC[7];
    /// Two nearby-enemy slots the spawn tick fills, each dropped once its HP
    /// runs out.
    /* 0xEE8 */ Enemy* field_EE8[2];
    /// The enemy the state-change reset spawns from `D_actor_403200_8015E858`
    /// for the three states that launch it.
    /* 0xEF0 */ Enemy* field_EF0;
    /* 0xEF4 */ s16    field_EF4;
    /* 0xEF6 */ s16    field_EF6;
    /// Armed to 1 alongside `field_EF6` by the swipe tick's reset half.
    /* 0xEF8 */ s16 field_EF8;
    /// Armed to 1 by the per-frame body's re-arm path.
    /* 0xEFA */ s16 field_EFA;
    /// The `field_EFA` the colour update last ran for.
    /* 0xEFC */ s16 field_EFC;
    /// Cleared by the per-frame body's re-arm path.
    /* 0xEFE */ s16 field_EFE;
    /// Pitch the head tracker walks toward its request, clamped to 0..0x500.
    /* 0xF00 */ s16 field_F00;
    /// Raised to 1 with the message 0x3F4 the launch tick sends the player
    /// once the hold has been taken.
    /* 0xF02 */ s16 field_F02;
    /// Armed by the model-reset path in func_actor_403200_8013E2FC.
    /* 0xF04 */ s16 field_F04;
    /// Cleared by the per-frame body once `field_6` has passed 0x14.
    /* 0xF06 */ s16 field_F06;
    /// The step index of the per-frame body's walk-out: state 0 runs the model
    /// out to x 0x1CCA, state 1 to x 0x2882, and each step that arrives
    /// advances it and re-arms `field_0`.
    /* 0xF08 */ s16 field_F08;
    /// Damage pool the hit handler for collision groups 3, 4 and 5
    /// (`func_actor_403200_8013A4A0`) draws down alongside the host's HP, and
    /// refills to 0x32 when it runs out.
    /* 0xF0A */ s16 field_F0A;
    /// Damage pool the hit handler for collision groups 6, 7 and 8
    /// (`gluttonHitGroups6To8`) draws down alongside the host's HP, and
    /// refills to 0x3C when it runs out.
    /* 0xF0C */ s16 field_F0C;
    /// Damage pool the hit handler for collision groups 1 and 2
    /// (`gluttonHitGroups1To2`) draws down alongside the host's HP.
    /* 0xF0E */ s16 field_F0E;
    /// Start-of-state countdown the attack state reads against `field_6`: the
    /// state body only runs once `field_6` has reached it, and it is seeded to
    /// 0x28 if still zero.
    /* 0xF10 */ s16 field_F10;
    /// Death-cinematic step counter; reaching 8 starts the pending-position
    /// handoff.
    /* 0xF12 */ s16 field_F12;
    /// Quarters of it is how many extra re-arm steps the launch state runs,
    /// calling the per-frame body once per step.
    /* 0xF14 */ s16 field_F14;
    /// Re-armed to 2 by the upkeep handler `func_actor_403200_80141A94` once
    /// the `field_F1C` countdown has run out.
    /* 0xF16 */ s16  field_F16;
    /* 0xF18 */ byte pad_F18[0x2];
    /// Free-running counter bumped on every heal tick by
    /// `func_actor_403200_80141A94`.
    /* 0xF1A */ u8 field_F1A;
    /// Count of escorts this overlay has spawned; the spawn tick refuses a
    /// new one once it has reached 8.
    /* 0xF1B */ s8 field_F1B;
    /// Countdown, decremented while positive; when it reaches zero the handler
    /// re-arms `field_F16`. The spawn tick also uses it as a live-escort cap
    /// of 2.
    /* 0xF1C */ s8 field_F1C;
    /// Armed to 6 by the state-change reset, the pair shown while the enemy
    /// stands up.
    /* 0xF1D */ s8   field_F1D;
    /* 0xF1E */ byte pad_F1E[0x6];
} GluttonWork;
STATIC_ASSERT_SIZEOF(GluttonWork, 0xF24);

void gluttonBuildWall(Task* task, s16 scale, s16 drop, s16 index);
void gluttonPoseLimb(Task* task);
void gluttonTurnNeck(Task* task, s16 arg1);
void gluttonPitchNeck(Task* task, s16 arg1);
void gluttonSeedBlend(Task* task);
void gluttonSwitchAnim(Task* arg0);
void gluttonTickBlended(Task* arg0);
void gluttonTickAnim(Task* arg0);
void gluttonHitEffect(GfxCoord* coord, s32 id);
void gluttonThrowSpawn(Enemy* enemy, Task* task);
void gluttonThrowFly(Enemy* enemy, Task* task);
void gluttonGlobSpawn(Enemy* enemy, Task* task);
void gluttonGlobFall(Enemy* enemy, Task* task);
void gluttonGlobEngulf(Enemy* enemy, Task* task);
void gluttonGlobHold(Enemy* enemy, Task* task);
void gluttonChunkSpawn(Enemy* enemy, Task* task);
void gluttonChunkFall(Enemy* enemy, Task* task);
void gluttonChunkSettle(Enemy* enemy, Task* task);
void gluttonRainSpawn(Enemy* enemy, Task* task);
void gluttonRainRise(Enemy* enemy, Task* task);
void gluttonRainFall(Enemy* enemy, Task* task);
void gluttonRainSplat(Enemy* enemy, Task* task);
void gluttonSpinnerSpawn(Enemy* enemy, Task* task);
void gluttonSpinnerChase(Enemy* enemy, Task* task);
void gluttonExit(Task* arg0);
void gluttonPropSetup(Enemy* enemy, Task* task);
void gluttonPropTick(Enemy* enemy, Task* arg1);
void gluttonSpinnerWait(Enemy* arg0, Task* arg1);

static inline void gluttonShrinkRotation(GfxCoord* coord);
static inline void gluttonScaleRotation(GfxCoord* coord, s16 xz, s32 y);
static inline void gluttonGapToCamera(GfxCoord* coord, SVECTOR* out);

void gluttonGlobTask(Task* arg0);
void gluttonChunkTask(Task* arg0);
void gluttonSpinnerTask(Task* arg0);
void gluttonRainTask(Task* arg0);
void gluttonThrowTask(Task* arg0);
void gluttonPropTask(Task* arg0);
void gluttonSetQuadHeights(s32 arg0, s16 arg1);
void gluttonSetShakeLevel(s8 arg0);
void gluttonSetSpinnersReleased(s16 arg0);
s16  gluttonGetSpinnersReleased(void);

/// Gives the escort the texture page and palette of the current area's
/// third placement, and refreshes its existing model stream.
static __inline__ void gluttonTintEscort(TmdObject* model)
{
    AreaPlacement* entry;

    entry                    = &(actorGetCurrentAreaRec()->placements)[2];
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

#endif /* SRC_SHARED_GLUTTON_H */
