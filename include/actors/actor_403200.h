#ifndef ACTOR_403200_H
#define ACTOR_403200_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

typedef struct Actor403200Obj Actor403200Obj;

/// Scratch coordinate with word access to its identity rotation matrix.
typedef union Actor403200DropCoord {
    GsCOORDINATE2 c;
    struct {
        /* 0x00 */ s32 flg;
        /* 0x04 */ s32 m00_m01;
        /* 0x08 */ s32 m02_m10;
        /* 0x0C */ s32 m11_m12;
        /* 0x10 */ s32 m20_m21;
        /* 0x14 */ s16 m22;
    } ident;
} Actor403200DropCoord;
STATIC_ASSERT_SIZEOF(Actor403200DropCoord, 0x50);

/// Rotation matrix view used for the aligned identity stores.
typedef union Actor403200Matrix {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} Actor403200Matrix;
STATIC_ASSERT_SIZEOF(Actor403200Matrix, 0x20);

/// Reference positions used by the distance-based view selector.
typedef struct Actor403200ViewPoints {
    /* 0x00 */ SVECTOR v[4];
} Actor403200ViewPoints;
STATIC_ASSERT_SIZEOF(Actor403200ViewPoints, 0x20);

extern Actor403200ViewPoints D_actor_403200_80131E64;

/// One of the nine models and spawn arguments used by the formation spawner.
typedef struct Actor403200SpawnRec {
    /* 0x0 */ void* model;
    /* 0x4 */ s16   spawnArg;
    /* 0x6 */ s16   field_6;
} Actor403200SpawnRec;
STATIC_ASSERT_SIZEOF(Actor403200SpawnRec, 0x8);

/// Payload `func_actor_403200_8013C84C` passes as `Gp_DispatchMsg`'s `arg2`
/// for message 0x7DA, which the slot-4 task forwards to the 0x7DB handlers.
/// The same four bytes as `Actor444000Msg7DA`: two id bytes followed by a
/// halfword the receiver switches on.
typedef struct Actor403200Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor403200Msg7DA;
STATIC_ASSERT_SIZEOF(Actor403200Msg7DA, 0x4);

/// The forwarded 0x7DB payload, saved bytewise and dispatched by its two
/// halfwords: the sender id and action.
typedef union Actor403200Msg7DB {
    u8 b[4];
    struct {
        /* 0x0 */ u16 id;
        /* 0x2 */ u16 action;
    } h;
} Actor403200Msg7DB;
STATIC_ASSERT_SIZEOF(Actor403200Msg7DB, 0x4);

/// Payload of the 0x3F8 query the stand-up state's swipe tick sends the player
/// before it asks for the hold; `field_14` is the range it asks for. The same
/// shape as `Actor103700Msg3F8` and `Actor444000Msg3F8`.
typedef struct Actor403200Msg3F8 {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
} Actor403200Msg3F8;
STATIC_ASSERT_SIZEOF(Actor403200Msg3F8, 0x18);

/// One of the nine back-to-back collision groups in `Actor403200Work` at
/// 0x7F4. `obj` is the `GpObj` the gameplay collision list carries and `recs`
/// is the `GpRec18` table it fills in for that part, which is why the stride is
/// 0x98. `obj.field_8` is the part's own coordinate -- what the hit handler
/// spawns the hit effect on. The same shape as `Actor444000HitGroup`.
typedef struct Actor403200HitGroup {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 recs[5];
} Actor403200HitGroup;
STATIC_ASSERT_SIZEOF(Actor403200HitGroup, 0x98);

/// 0x30-byte scratchpad frame the group-0 hit handler
/// `func_actor_403200_80139A60` carves off `SCRATCH_HEAD` for the one hit it
/// takes this frame. `pos` is the contact point copied out of the `GpRec18`;
/// `delta` is the player-relative offset whose length is `dist`, the range
/// `Gp_ComputeDamage` scales `damage` by. `rot` doubles as `Gp_SpawnEff`'s
/// rotation argument and, afterwards, as the workspace for the contact point
/// relative to the part's world translation, which `angle` is the yaw of.
typedef struct Actor403200HitScratch {
    /* 0x00 */ VECTOR3 delta;
    /* 0x0C */ byte    pad_C[0x4];
    /* 0x10 */ SVECTOR rot;
    /* 0x18 */ SVECTOR pos;
    /* 0x20 */ s32     id;     // attack id of the hit that landed, 0 for none
    /* 0x24 */ u32     damage; // HP taken off the enemy
    /* 0x28 */ s32     dist;   // distance from the player, in world units
    /* 0x2C */ s16     angle;  // yaw of the contact point, wrapped to +/-0x800
    /* 0x2E */ byte    pad_2E[0x2];
} Actor403200HitScratch;
STATIC_ASSERT_SIZEOF(Actor403200HitScratch, 0x30);

/// 0x20-byte scratchpad frame the state-selecting tick
/// `func_actor_403200_8013EB64` carves off `SCRATCH_SP`. `delta` is the
/// player-relative offset whose length is `dist`, the range the three
/// `field_F08` sub-states door the enemy through;
/// `view` is the camera-relative offset the yaw written to `field_7C4` is
/// taken from. Both are read back out of the frame rather than kept in
/// registers, which is what puts them in the scratch in the first place.
typedef struct Actor403200ApproachScratch {
    /* 0x00 */ VECTOR  delta; // player position minus this part's, in world units
    /* 0x10 */ SVECTOR view;  // camera position minus this part's
    /* 0x18 */ s32     dist;  // length of `delta`
    /* 0x1C */ byte    pad_1C[0x4];
} Actor403200ApproachScratch;
STATIC_ASSERT_SIZEOF(Actor403200ApproachScratch, 0x20);

/// 0xC-byte scratchpad frame the launch state's reset half
/// `func_actor_403200_8013B3C8` carves off `SCRATCH_SP` for the one yaw it
/// takes this tick. `dir` is the player-relative offset of the host model's
/// root part, the pair `ratan2` turns into the yaw written to `field_7C4`; the
/// trailing word is not read back, and is only here because the frame the code
/// carves is 0xC, not the 8 the vector alone needs.
typedef struct Actor403200TurnScratch {
    /* 0x00 */ SVECTOR dir; // player position minus the host root part's
    /* 0x08 */ s32     field_8;
} Actor403200TurnScratch;
STATIC_ASSERT_SIZEOF(Actor403200TurnScratch, 0xC);

/// Per-actor state block for the `actor_403200` overlay.
///
/// `func_actor_403200_80138AFC` allocates it with `memCalloc(0xF24, 0)` and
/// stores the result in the `Task::work` slot (0x1C), which this enemy actor
/// reuses for its own work block, so it is *not* a `TaskIdMap` here. Reach it
/// with `(Actor403200Work*)task->work`. The size below is the allocation, not
/// a guess.
typedef struct Actor403200Work {
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
    /* 0x008 */ byte pad_8[0x42];
    /// The animation frame the stand-up state latches its swipe cue on, masked
    /// to 10 bits against the frame armed last tick in `field_7AC`. Same slot
    /// as `Actor444000Work::slots0[1].field_2`.
    /* 0x04A */ u16  field_4A;
    /* 0x04C */ byte pad_4C[0xC];
    /// The trailing halfword of the animation slot the per-frame body tests for
    /// the `field_0 = 0xA` re-arm. Same slot as
    /// `Actor444000Work::slots0[1].field_10`.
    /* 0x058 */ u16  field_58;
    /* 0x05A */ byte pad_5A[0x18];
    /// The animation frame the per-frame body's two one-shot sound cues are
    /// latched on, masked to 10 bits, and the same value it publishes to
    /// `field_7D8`. Same slot as `Actor444000Work::slots0[2].field_2`.
    /* 0x072 */ u16  field_72;
    /* 0x074 */ byte pad_74[0x26];
    /// The animation frame the launch state latches its four one-shot cues on,
    /// masked to 10 bits against the frame armed last tick in `field_7A8`.
    /// The third animation slot's frame halfword, the same slot family as
    /// `field_72`.
    /* 0x09A */ u16  field_9A;
    /* 0x09C */ byte pad_9C[0x708];
    /// Escort pose index, written 3 by the re-arm path of the per-frame body
    /// and cleared once the shared countdown below has run out. Same slot and
    /// role as `Actor444000Work::field_7A4`.
    /* 0x7A4 */ s16  field_7A4;
    /* 0x7A6 */ byte pad_7A6[0x2];
    /// The masked `field_9A` frame the launch state last saw, so each of its
    /// four one-shot cues only fires on the step the animation first reaches
    /// that frame. Same slot and role as `Actor444000Work::field_7A8`.
    /* 0x7A8 */ s32 field_7A8;
    /// The masked `field_4A` / `field_72` frame the stand-up tick last saw, so
    /// each of its one-shot cues only fires on the step the animation first
    /// reaches that frame. Same slot and role as `Actor444000Work::field_7AC`.
    /* 0x7AC */ s32  field_7AC;
    /* 0x7B0 */ s8   field_7B0;
    /* 0x7B1 */ byte pad_7B1[0x2];
    /* 0x7B3 */ s8   field_7B3;
    /* 0x7B4 */ byte pad_7B4[0x2];
    /// The animation slot selector the launch state arms to 0x40 and then to
    /// 0x10. Same slot and role as `Actor444000Work::field_7B6`.
    /* 0x7B6 */ s16  field_7B6;
    /* 0x7B8 */ byte pad_7B8[0xC];
    /// Cleared alongside `field_7C8` by the group-0 hit handler, the same pair
    /// `Actor444000Work::field_7C4` is.
    /* 0x7C4 */ s16  field_7C4;
    /* 0x7C6 */ byte pad_7C6[0x2];
    /// Cleared alongside `field_7C4` by the group-0 hit handler, the same pair
    /// `Actor444000Work::field_7C8` is.
    /* 0x7C8 */ s16 field_7C8;
    /// Frames since the arena tick last sent the player its message 0x3FF
    /// animation; the retries in `func_actor_403200_8013FB54` are bounded by
    /// it. Same slot and role as `Actor444000Work::field_7CA`.
    /* 0x7CA */ u16  field_7CA;
    /* 0x7CC */ byte pad_7CC[0xC];
    /// The masked `field_72` frame the per-frame body last saw, so each of its
    /// two one-shot cues only fires on the step the animation first reaches
    /// that frame. Same slot and role as `Actor444000Work::field_7D8`.
    /* 0x7D8 */ s32  field_7D8;
    /* 0x7DC */ byte pad_7DC[0x17];
    /// Cleared by the state-change reset to mark the work block as re-armed.
    /// Same slot and role as `Actor444000Work::field_7F3`.
    /* 0x7F3 */ u8 field_7F3;
    /// The nine back-to-back collision groups, one per model part, the same
    /// run `Actor444000Work::hits` holds: each is the `GpObj` the gameplay
    /// collision list carries plus the `GpRec18` table it fills in.
    /* 0x7F4 */ Actor403200HitGroup hits[9];
    /// The tenth collision object, the one the swipe tick raises `flags` bit
    /// 0x8000 on while the swipe is live. Same slot and role as
    /// `Actor444000Work::obj`.
    /* 0xD4C */ GpObj obj;
    /* 0xD6C */ byte  pad_D6C[0x18];
    /// The five records the tenth collision object carries, walked by the swipe
    /// tick for the one whose high half is 0x10000. Same slots and role as
    /// `Actor444000Work::recs2`.
    /* 0xD84 */ GpRec18 recs2[5];
    /* 0xDFC */ byte    pad_DFC[0x40];
    /// Free coordinate the swipe tick clears and pushes through
    /// `Gp_UpdateCoord` every step; `coord` is the matrix `Gfx_RotMatrixY`
    /// rebuilds from `field_7C8`. Same slot and role as
    /// `Actor444000Work::field_E3C`.
    /* 0xE3C */ GsCOORDINATE2 field_E3C;
    /// `Gp_GetIdParam2` of the hit the group-0 handler took this frame; the
    /// sibling slots carry the other groups' ids. Same slots and role as
    /// `Actor444000Work::field_E8C`.
    /* 0xE8C */ s16 field_E8C;
    /* 0xE8E */ s16 field_E8E;
    /* 0xE90 */ s16 field_E90;
    /* 0xE92 */ s16 field_E92;
    /// Yaw the upkeep tick walks toward `field_E96` in steps of 0x32, snapping
    /// once the two are within 0x33 of each other. Same slot and role as
    /// `Actor444000Work::field_E94`.
    /* 0xE94 */ s16 field_E94;
    /// Yaw target the re-arm path arms to 0xC80. Same slot and role as
    /// `Actor444000Work::field_E96`.
    /* 0xE96 */ s16 field_E96;
    /// Companion value handed to the follow helper alongside `field_E94`. Same
    /// slot and role as `Actor444000Work::field_E98`.
    /* 0xE98 */ s16  field_E98;
    /* 0xE9A */ byte pad_E9A[0x12];
    /// Screen-shake level `func_actor_403200_80138284` drives, and the level
    /// armed last tick in `field_EAD`; a change from the armed level starts a
    /// shake. Same slots, types and role as `Actor444000Work::field_EAC`.
    /* 0xEAC */ u8 field_EAC;
    /* 0xEAD */ u8 field_EAD;
    /* 0xEAE */ u8 field_EAE;
    /* 0xEAF */ s8 field_EAF;
    /// Message 0x3FF payload the launch state sends the player. Same slot and
    /// role as `Actor444000Work::anim`.
    /* 0xEB0 */ GpAnimArg field_EB0;
    /// First three bytes of the last 0x7DB payload received.
    /* 0xEC4 */ u8   field_EC4;
    /* 0xEC5 */ u8   field_EC5;
    /* 0xEC6 */ u8   field_EC6;
    /* 0xEC7 */ byte pad_EC7;
    /// Set to 1 while the player holds the animation the stand-up state hands
    /// over in its message 0x3FF. Same slot and role as
    /// `Actor444000Work::field_EC8`.
    /* 0xEC8 */ s16 field_EC8;
    /// The reply the swipe tick's hold request (message 0x3F9) came back with,
    /// 1 when the player took it. Same slot and role as
    /// `Actor444000Work::field_ECA`.
    /* 0xECA */ s16 field_ECA;
    /// The escorts the state-change reset walks to push the host's
    /// `TmdObject::flags` onto each escort's own model object; the same
    /// seven-slot run as `Actor444000Work::field_ECC`.
    /* 0xECC */ GpEnemy* field_ECC[7];
    /// The two escorts the upkeep tick drops once their HP has run out, by
    /// clearing the slot when `GpEnemy::hp` is not positive. Same slots
    /// and role as `Actor444000Work::field_EE8`.
    /* 0xEE8 */ GpEnemy* field_EE8;
    /* 0xEEC */ GpEnemy* field_EEC;
    /// The enemy the state-change reset spawns from `D_actor_403200_8015E858`
    /// for the three states that launch it.
    /* 0xEF0 */ GpEnemy* field_EF0;
    /* 0xEF4 */ s16      field_EF4;
    /* 0xEF6 */ s16      field_EF6;
    /// Armed to 1 alongside `field_EF6` by the swipe tick's reset half. Same
    /// slot and role as `Actor444000Work::field_EF8`.
    /* 0xEF8 */ s16 field_EF8;
    /// Armed to 1 by the per-frame body's re-arm path. Same slot and role as
    /// `Actor444000Work::field_EFA`.
    /* 0xEFA */ s16  field_EFA;
    /* 0xEFC */ byte pad_EFC[0x2];
    /// Cleared by the per-frame body's re-arm path. Same slot and role as
    /// `Actor444000Work::field_EFE`.
    /* 0xEFE */ s16  field_EFE;
    /* 0xF00 */ byte pad_F00[0x4];
    /// Armed by the model-reset path in func_actor_403200_8013E2FC.
    /* 0xF04 */ s16 field_F04;
    /// Cleared by the per-frame body once `field_6` has passed 0x14. Same slot
    /// and role as `Actor444000Work::field_F06`.
    /* 0xF06 */ s16 field_F06;
    /// The step index of the per-frame body's walk-out: state 0 runs the model
    /// out to x 0x1CCA, state 1 to x 0x2882, and each step that arrives
    /// advances it and re-arms `field_0`. Same slot and role as
    /// `Actor444000Work::field_F08`.
    /* 0xF08 */ s16 field_F08;
    /// Damage pool the hit handler for collision groups 3, 4 and 5
    /// (`func_actor_403200_8013A4A0`) draws down alongside the host's HP, and
    /// refills to 0x32 when it runs out. Same slot and role as
    /// `Actor444000Work::field_F0A`.
    /* 0xF0A */ u16 field_F0A;
    /// Damage pool the hit handler for collision groups 6, 7 and 8
    /// (`func_actor_403200_8013AB70`) draws down alongside the host's HP, and
    /// refills to 0x3C when it runs out. Same slot and role as
    /// `Actor444000Work::field_F0C`.
    /* 0xF0C */ u16 field_F0C;
    /// Damage pool the hit handler for collision groups 1 and 2
    /// (`func_actor_403200_80139E94`) draws down alongside the host's HP. Same
    /// slot and role as `Actor444000Work::field_F0E`.
    /* 0xF0E */ u16 field_F0E;
    /// Start-of-state countdown the attack state reads against `field_6`: the
    /// state body only runs once `field_6` has reached it, and it is seeded to
    /// 0x28 if still zero. Same slot and role as `Actor444000Work::field_F10`.
    /* 0xF10 */ s16  field_F10;
    /* 0xF12 */ byte pad_F12[0x2];
    /// Quarters of it is how many extra re-arm steps the launch state runs,
    /// calling the per-frame body once per step. Same slot and role as
    /// `Actor444000Work::field_F14`.
    /* 0xF14 */ s16 field_F14;
    /// Re-armed to 2 by the upkeep handler `func_actor_403200_80141A94` once
    /// the `field_F1C` countdown has run out. Same slot and role as
    /// `Actor444000Work::field_F16`.
    /* 0xF16 */ s16  field_F16;
    /* 0xF18 */ byte pad_F18[0x2];
    /// Free-running counter bumped on every heal tick by
    /// `func_actor_403200_80141A94`.
    /* 0xF1A */ u8   field_F1A;
    /* 0xF1B */ byte pad_F1B[0x1];
    /// Countdown, decremented while positive; when it reaches zero the handler
    /// re-arms `field_F16`.
    /* 0xF1C */ s8 field_F1C;
    /// Armed to 6 by the state-change reset, the pair shown while the enemy
    /// stands up.
    /* 0xF1D */ s8   field_F1D;
    /* 0xF1E */ byte pad_F1E[0x6];
} Actor403200Work;
STATIC_ASSERT_SIZEOF(Actor403200Work, 0xF24);

/// Work block of the enemy `func_actor_403200_8013669C` stands up: that state
/// allocates it with `memCalloc(0x1C0, 0)` and parks it in its task's
/// `Task::work` slot, so the size below is the allocation, not a guess.
///
/// The state drops the model onto the view coordinate and hangs two `GpObj`
/// display nodes off it. `rec0` is the table the first node carries, `rec1`
/// the second's; the two matrices are handed back out through the task's
/// `TmdObject::lightMtx` / `field_20`, as the sibling spawn states do.
/// `field_1AA` is a ninth of the model's height and `field_1AC` the step
/// counter, both re-read by the states that follow this one.
typedef struct Actor403200GrabWork {
    /// Horizontal gap to the player, a fifteenth of which the later states add
    /// to the model each step; only `vx` and `vz` are filled in here.
    /* 0x000 */ VECTOR3 vel;
    /* 0x00C */ byte    pad_C[0xA4];
    /// The two display nodes, linked with `prio` 3 and 2.
    /* 0x0B0 */ GpObj obj0;
    /* 0x0D0 */ GpObj obj1;
    /// Their collision-record tables.
    /* 0x0F0 */ GpRec18 rec0;
    /* 0x108 */ GpRec18 rec1;
    /* 0x120 */ byte    pad_120[0x30];
    /// The colour and light matrices: `field_1C` of the task's `TmdObject` is
    /// handed `lightMtx` and `field_20` `colorMtx`.
    /* 0x150 */ MATRIX colorMtx;
    /* 0x170 */ MATRIX lightMtx;
    /* 0x190 */ byte   pad_190[0x18];
    /// Armed to 1 by the spawn state `func_actor_403200_8013509C` once the
    /// model has been stood up on its escort's part 1; the states that follow
    /// re-arm the step counter and the first display node on the tick they see
    /// it set. Same slot and role as `Actor444000GrabWork::field_1A8`.
    /* 0x1A8 */ s16  field_1A8;
    /* 0x1AA */ s16  field_1AA;
    /* 0x1AC */ s16  field_1AC;
    /* 0x1AE */ byte pad_1AE[0x12];
} Actor403200GrabWork;
STATIC_ASSERT_SIZEOF(Actor403200GrabWork, 0x1C0);

s32 func_actor_403200_801344C4(Task* arg0, s16 arg1);

/// The actor's per-frame body: runs the animation resets and the collision /
/// damage ticks. Takes the task, and reaches the work block through its
/// `work` slot, as `func_actor_403200_8014123C` does.
void func_actor_403200_80133DD8(Task* task);

extern MATRIX* D_80073B8C;

#endif
