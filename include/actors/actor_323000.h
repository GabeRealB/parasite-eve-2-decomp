#ifndef ACTOR_323000_H
#define ACTOR_323000_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Head of the work block this overlay hangs behind `Task::work`. Only the
/// fields the handlers touch are known: `field_4` is the live-actor flag
/// every state handler tests, and `field_6` / `field_828` / `field_82E` /
/// `field_832` / `field_83E` / `field_840` are the animation-state slots the
/// `Actor00100Work` family keeps at the same offsets, where
/// `Actor00100_Fn0B52C` writes them in the same order.
///
/// The block is the `Actor01900AnimWork` layout: the 0x14-byte `GpAnimCtx`
/// and its 18-slot array at 0x1C, the 0x120-byte pose buffer `func_800B3F84`
/// is handed as its arg3 (one 0x10-byte record per slot), and the same pair
/// again at 0x420 for the blend context. The light / colour matrices
/// `func_actor_323000_80163EA0` binds to `TmdObject::lightMtx` / `field_20`
/// sit between the animation block and the tail.
typedef struct Actor323000Work {
    /// Animation state, the same slot `ActorShared80164af0Work` names
    /// `field_0`; `func_actor_323000_80164A54` picks it from a message, and
    /// the animation handler `ActorsShared80164af0` restarts it.
    /* 0x000 */ s16 field_0;
    /// State `func_actor_323000_801645A4` ran last frame; `field_4` is set
    /// when `field_0` differs from it.
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// Frame counter `func_actor_323000_80163A30` advances; zeroed by the
    /// re-init handler below.
    /* 0x006 */ s16        field_6;
    /* 0x008 */ byte       pad_8[0x14];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[18];
    /// Pose buffer `func_800B3F84` takes as its arg3, `GpAnimCtx.poses`.
    /* 0x300 */ byte       poses[0x120];
    /* 0x420 */ GpAnimCtx  blendAnim;
    /* 0x434 */ GpAnimSlot blendSlots[18];
    /* 0x704 */ byte       blendPoses[0x120];
    /* 0x824 */ byte       pad_824[4];
    /// Animation-state slots the spawn handler seeds and the tick pair keeps,
    /// the same eight halfwords `Actor01900AnimWork` names `field_978`..
    /// `field_990`: the state to run, the clip id the slots were last seeded
    /// with, the one being seeded now, and the two 0x10 speeds.
    /* 0x828 */ s16 field_828;
    /* 0x82A */ s16 field_82A;
    /* 0x82C */ s16 field_82C;
    /* 0x82E */ s16 field_82E;
    /* 0x830 */ u16 field_830;
    /* 0x832 */ s16 field_832;
    /* 0x834 */ s16 field_834;
    /* 0x836 */ s16 field_836;
    /* 0x838 */ s16 field_838;
    /* 0x83A */ s16 field_83A;
    /* 0x83C */ s16 field_83C;
    /* 0x83E */ s16 field_83E;
    /* 0x840 */ s16 field_840;
    /* 0x842 */ s16 field_842;
    /// Cleared next to `field_840` by the spawn handler below.
    /* 0x844 */ s16  field_844;
    /* 0x846 */ byte pad_846[2];
    /// Clip id each slot was last seen playing by `func_actor_323000_80163448`,
    /// indexed like `slots`; zeroed (18 entries) when no watched clip plays.
    /* 0x848 */ s32 field_848[19];
    /// Light / colour matrices `func_actor_323000_80163EA0` binds to the
    /// model, the same pair `Actor356100_BindMatrices` points at.
    /* 0x894 */ MATRIX light;
    /* 0x8B4 */ MATRIX color;
    /* 0x8D4 */ byte   pad_8D4[0x48];
    /// Three bytes `func_actor_323000_80164A54` takes from a message payload
    /// one at a time; nothing else in this overlay reads them.
    /* 0x91C */ u8   field_91C;
    /* 0x91D */ u8   field_91D;
    /* 0x91E */ u8   field_91E;
    /* 0x91F */ byte pad_91F[0x15];
} Actor323000Work;
STATIC_ASSERT_SIZEOF(Actor323000Work, 0x934);

/// Payload of message 0x7DB, the handler table `D_actor_323000_801739D0`
/// carries for this overlay. `code` is the sub-command the handler selects on
/// (0x202 here) and `mode` its variation; the sender writes both as words, and
/// `func_actor_323000_80164A54` also reads their three leading bytes
/// individually, which is what `Actor323000MsgBytes` is for.
typedef struct Actor323000Msg {
    /* 0x0 */ u16 code;
    /* 0x2 */ u16 mode;
} Actor323000Msg;

/// Byte view of `Actor323000Msg`: `b0` and `b1` are the halves of `code` and
/// `b2` the low half of `mode`.
typedef struct Actor323000MsgBytes {
    /* 0x0 */ u8 b0;
    /* 0x1 */ u8 b1;
    /* 0x2 */ u8 b2;
} Actor323000MsgBytes;

/// 0x1C-byte block `func_actor_323000_801645A4` pushes on `G_SCRATCH_HEAD`:
/// the model root's world position for `Gp_UpdateActorColor`, and the local
/// point walked up the coordinate chain into view space.
typedef struct Actor323000TickScratch {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR local;
    /* 0x18 */ s32     pad_18;
} Actor323000TickScratch;
STATIC_ASSERT_SIZEOF(Actor323000TickScratch, 0x1C);

/// State handlers `func_actor_323000_801645A4` dispatches through by
/// `Actor323000Work::field_0`.
extern GpEnemyTaskFuncTable4 D_actor_323000_80161E24;

/// Animation source `func_800B3F84` is handed for both of the work block's
/// contexts.
extern u8 D_actor_323000_8017387C[];

/// Effect record the spawn handler fills for `Gp_SpawnEff`: the model root, a
/// 0x100 scale and a count of 2. Same shape and roles as
/// `Actor356100Work`'s `D_actor_356100_801732A8`.
extern GpEffArg D_actor_323000_80173A24;

/// Message table published as `Task::msgTable` by the spawn handler.
extern void* D_actor_323000_801739D0;

/// Enemy pair source `GpEnemy::param` is pointed at by the spawn handler.
extern GpPairSrcE D_actor_323000_80164D54;

/// Spawn body (descriptor entry for this overlay): allocates the 0x934-byte
/// work block, stores it in `Task::work`, binds the model's light and colour
/// matrices to the block's tail, re-seeds the enemy descriptor and both
/// animation contexts, then takes the model root's world position as the
/// actor colour. The four parent slots are the model root (`field_4`), the
/// root's third part coordinate (`field_18`, used by the link transforms) and
/// the `GPEnemyWait` link node; `field_1C` is zeroed because the link
/// transforms fill it. The finished entry advances the task's state.
void func_actor_323000_80163EA0(GpEnemy* enemy, Task* task);

/// Per-state animation table `func_actor_323000_80163A30` reads when it
/// re-seeds the slots: 0x2D bytes per `field_82C`, indexed by `field_82E`.
extern s8 D_actor_323000_80173090[];

/// Sound step of the tick: returns the `SndEvt_EnqueueType6` id to play for
/// this frame, or 0.
s32 func_actor_323000_80163448(Task* task, Actor323000Work* work);

/// Per-frame tick of the live actor, run once the work block's animation
/// slots are set up.
void func_actor_323000_80163A30(Task* task);

/// State handler of the same table as the re-init handler below. When the work
/// block's `field_4` flag is set it clears the enemy's link flag (the re-init
/// handler sets it), drops the model root's `field_C` and rebuilds its buffers,
/// then seeds the animation-state slots before the tick. Otherwise it ticks,
/// re-seeds `field_82E` once the flag in `slot[1]`'s `field_10` - the same slot
/// actor 01900 names `field_68` - reports the 0xF step, and once `field_82E`
/// reads 0xE it spawns the actor's effect 0x60054 at the model root's eighth
/// coordinate: the jump vector (-0x3E8, 0x28A, 0xC8) for the step `slot[1]`'s
/// `field_2` (actor 01900's `field_5A`) reads as 7 or 9, and 0x80003400 for 8.
void func_actor_323000_8016409C(GpEnemy* enemy, Task* task);

/// Re-init handler (table `D_actor_323000_80161E24`, index 2): when the work
/// block's `field_4` flag is set, flags the enemy's link node, drops the
/// model's root `field_C` and reallocates its buffers, then writes the
/// animation-state slots and clears the frame counter. Either way the tick
/// above runs. Reached as `fns[state](task->spawnArg2, task)`, like the other
/// handler tables in this family.
void func_actor_323000_80164C58(GpEnemy* enemy, Task* task);

/// Spawn the actor's effect (id 0x60054) at its own coordinate slot, with the
/// offset vector the state picks: y 0x28A and z 0xC8 for the two states that
/// jump, which differ in x alone (-0x1F4 for 9, -0x3E8 for 7); y 0x258 with x
/// and z zero for the pair that shares a tail (14/17); and zero for the idle
/// pair (0/1). The switch has no `default`, so the other states reach the
/// spawn with the vector as the frame left it. The coordinate is
/// `TmdObject::coords[arg1]`, the same `TmdObject` the re-init handler above
/// clears `field_C` of, and the vector is a local rather than the work-block
/// slot actor 00100 keeps at 0x898 - the same body as actor 421600's
/// `func_actor_421600_8013E700`, with different offsets.
void func_actor_323000_80164B40(Task* task, s16 arg1, s16 arg2);

/// Handler for message 0x7DB in the same table: copies the payload's three
/// leading bytes into the work block and, when its `code` word is 0x202,
/// selects the animation state from `mode` -- 1 starts state 2, 0 and 2
/// restart state 0, and 3 keeps state `mode` as it stands. Every other code
/// only stores the bytes. Reached as `fns[code](task, arg1, msg, arg3)`.
s32 func_actor_323000_80164A54(Task* task, s32 arg1, Actor323000Msg* msg, s32 arg3);

#endif
