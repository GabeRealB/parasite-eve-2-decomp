/* The Desert Chaser, one enemy built three ways; a package selects its build
 * with DESERT_CHASER_BUILD before including this header:
 *
 * - DESERT_CHASER_UNARMED: actor_323000 (placed in dryfield_main_street) and
 *   actor_323400 (dryfield_breezeway). Its spawn sets 0 HP and
 *   WORLD_TARGET_NOT_LOCKABLE, and message 2005 toggles its display flags.
 * - DESERT_CHASER_REGULAR: actor_00100 (packages actor_400100 and actor_407500).
 * - DESERT_CHASER_WATER_TOWER: actor_421600, the dryfield_water_tower build.
 *
 * All three drive an 18-slot rig. The animation driver seeds its slots from a
 * per-transition start-frame table and cross-fades a second animation context
 * into slots 1-10; it eases a torso twist spread over joints 2-4 and a head
 * turn on joint 10, and plays the sound its per-package cue step returns.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_DESERT_CHASER_H
#define SRC_SHARED_DESERT_CHASER_H

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

#define DESERT_CHASER_UNARMED     1
#define DESERT_CHASER_REGULAR     2
#define DESERT_CHASER_WATER_TOWER 3
#ifndef DESERT_CHASER_BUILD
#error "define DESERT_CHASER_BUILD (DESERT_CHASER_UNARMED, _REGULAR or _WATER_TOWER) before including desert_chaser.h"
#endif

/// A route point, the placement and the one a fixed step ahead of it.
typedef struct DesertChaserWaypoint {
    s16 x;
    s16 z;
} DesertChaserWaypoint;
STATIC_ASSERT_SIZEOF(DesertChaserWaypoint, 0x4);

#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
/// Sphere body and all five results supplied by its owner.
typedef struct DesertChaserSphereBody {
    WorldCollisionBody    obj;         // Linked sphere collision body
    WorldCollisionContact contacts[5]; // Complete initialized contact table
} DesertChaserSphereBody;
STATIC_ASSERT_SIZEOF(DesertChaserSphereBody, 0x98);

/// Capsule body, its shape and all five results supplied by its owner.
typedef struct DesertChaserCapsuleBody {
    WorldCollisionBody    obj;         // Linked capsule collision body
    WorldCollisionCapsule shape;       // Endpoints, radii and contact-table pointer
    WorldCollisionContact contacts[5]; // Complete initialized contact table
} DesertChaserCapsuleBody;
STATIC_ASSERT_SIZEOF(DesertChaserCapsuleBody, 0xB0);

typedef struct DesertChaserAnimCommand {
    AnimationSet* sets[9];
} DesertChaserAnimCommand;
STATIC_ASSERT_SIZEOF(DesertChaserAnimCommand, 0x24);
#elif DESERT_CHASER_BUILD == DESERT_CHASER_WATER_TOWER
/// The actor id word at 0xE90, read two ways: masked to 24 bits and compared
/// with 0x11402, or its third byte alone tested against 2.
typedef union DesertChaserIdWord {
    s32 word;
    u8  bytes[4];
} DesertChaserIdWord;
STATIC_ASSERT_SIZEOF(DesertChaserIdWord, 0x4);

typedef struct DesertChaserAnimCommand {
    AnimationSet* entries[4];
    AnimationSet* field_10;
    AnimationSet* field_14;
} DesertChaserAnimCommand;
#endif

/// The work block the spawn handler allocates and hangs behind `Task::work`:
/// 0x934 bytes in the unarmed build, 0xC30 in the regular one and 0xEB0 in
/// the Water Tower one. The three share the head up to 0x890 -- the state
/// words, the route points, both animation contexts with their 18 slots and
/// pose buffers, and the animation-state words the handlers seed and the tick
/// keeps -- and differ after it.
typedef struct DesertChaserWork {
    /// Animation state, the index the per-frame tick dispatches on.
    s16 field_0;
    /// State the tick ran last frame; `field_4` is set when `field_0`
    /// differs from it.
    s16 field_2;
    s16 field_4;
    /// Frame counter the state handlers time their effects with.
#if DESERT_CHASER_BUILD == DESERT_CHASER_UNARMED
    s16 field_6;
#elif DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    s16 field_6;
#elif DESERT_CHASER_BUILD == DESERT_CHASER_WATER_TOWER
    u16 field_6;
#endif
    /// Retry counter of the Water Tower build's contact walk.
    s16  field_8;
    byte pad_A[2];
    /// The point the actor was placed at and one a fixed step ahead of it;
    /// `field_14` picks the one it walks toward.
    DesertChaserWaypoint field_C[2];
    s16                  field_14;
    /// Yaw of the root coordinate as the placement handler leaves it, read
    /// back from the matrix.
    s16              field_16;
    byte             pad_18[4];
    AnimationContext anim;
    AnimationSlot    slots[18];
    /// Pose buffer `func_800B3F84` takes as its arg3, `AnimationContext.poseBuffer`.
    byte             poses[0x120];
    AnimationContext blendAnim;
    AnimationSlot    blendSlots[18];
    byte             blendPoses[0x120];
    byte             pad_824[4];
    /// Animation-state slots the handlers seed and the tick keeps: the seed
    /// mode the tick acts on (1 re-seeds from the per-state table, 2 resets
    /// the slots, 3 runs), whether the blend context is live, the clip the
    /// slots were last seeded with and the one to seed next, and the slot
    /// rate.
    u16 field_828;
    s16 field_82A;
    s16 field_82C;
    s16 field_82E;
    u16 field_830;
    u16 field_832;
    u16 field_834;
    s16 field_836;
    s16 field_838;
    u16 field_83A;
    s16 field_83C;
    u16 field_83E;
#if DESERT_CHASER_BUILD == DESERT_CHASER_UNARMED
    s16 field_840;
#elif DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    u16 field_840;
#elif DESERT_CHASER_BUILD == DESERT_CHASER_WATER_TOWER
    u16 field_840;
#endif
    s16 field_842;
    /// Turn angle the tick eases toward `field_840` and splits over the body
    /// joints; cleared by the spawn handler.
#if DESERT_CHASER_BUILD == DESERT_CHASER_UNARMED
    s16 field_844;
#elif DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    u16 field_844;
#elif DESERT_CHASER_BUILD == DESERT_CHASER_WATER_TOWER
    s16 field_844;
#endif
    byte pad_846[2];
    /// Clip id each slot was last seen playing, indexed like `slots`; zeroed
    /// (18 entries) when no watched clip plays.
    s32 field_848[18];
#if DESERT_CHASER_BUILD == DESERT_CHASER_UNARMED
    byte pad_890[4];
    /// Light / colour matrices the spawn handler binds to the model.
    MATRIX light;
    MATRIX color;
    byte   pad_8D4[0x48];
    /// Three bytes the message handler takes from a payload one at a time;
    /// nothing else reads them.
    u8   field_91C;
    u8   field_91D;
    u8   field_91E;
    byte pad_91F[0x15];
#elif DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    /// Argument record the hit effect fills for `func_800FDB18`.
    EffectSpawnArg field_890;
    SVECTOR        field_898;
    /// Hit position the hit effect hands to `func_800FDB18` as its rotation.
    SVECTOR                  field_8A0;
    SVECTOR                  field_8A8;
    SVECTOR                  field_8B0;
    byte                     pad_8B8[8];
    s32                      field_8C0;
    s32                      field_8C4;
    s32                      field_8C8;
    byte                     pad_8CC[4];
    s16                      field_8D0;
    s16                      field_8D2;
    s16                      field_8D4;
    byte                     pad_8D6[2];
    s32                      field_8D8;
    s32                      field_8DC;
    s32                      field_8E0;
    byte                     pad_8E4[4];
    s16                      field_8E8;
    byte                     field_8EA;
    byte                     pad_8EB[0x15];
    s32                      field_900;
    u8                       field_904;
    u8                       field_905;
    s16                      field_906;
    DesertChaserSphereBody   objs[3];
    DesertChaserCapsuleBody  capsuleBody;
    MATRIX                   field_B80;
    MATRIX                   field_BA0;
    byte                     pad_BC0[0x20];
    s16                      field_BE0;
    u16                      field_BE2;
    u16                      field_BE4;
    byte                     pad_BE6[0xA];
    s16                      field_BF0;
    s16                      field_BF2;
    s16                      field_BF4;
    byte                     pad_BF6[2];
    DesertChaserAnimCommand* field_BF8;
    s32                      field_BFC;
    s32                      field_C00;
    s32                      field_C04;
    s32                      field_C08;
    /// Last message opcode/operands, kept for the debug display: the three
    /// bytes of `ActorCommand` are latched here verbatim.
    u8   field_C0C;
    u8   field_C0D;
    u8   field_C0E;
    byte pad_C0F[9];
    s16  field_C18;
    s16  field_C1A;
    byte pad_C1C[2];
    s16  field_C1E;
    u16  field_C20;
    s16  field_C22;
    u16  field_C24;
    s16  field_C26;
    s16  field_C28;
    s16  field_C2A;
    byte pad_C2C[4];
#elif DESERT_CHASER_BUILD == DESERT_CHASER_WATER_TOWER
    /// Argument record the hit effect fills for `func_800FDB18`.
    EffectSpawnArg field_890;
    /// Hit position the hit effect hands to `func_800FDB18` as its rotation.
    SVECTOR field_898;
    s8      field_8A0;
    byte    pad_8A1[3];
    /// World X and Z of the gte-rotated route vector, around a zeroed Y.
    s32  field_8A4;
    s32  field_8A8;
    s32  field_8AC;
    byte pad_8B0[4];
    /// Pose id / blend flag pair, the pair the regular build keeps at
    /// 0x8E8 / 0x8EA.
    s16  field_8B4;
    s8   field_8B6;
    byte pad_8B7;
    /// Player position and rotation sent together as message 0x3E9.
    VECTOR  field_8B8;
    SVECTOR field_8C8;
    /// Reply buffer for message 0x3F8; field_8E4 selects query mode 8.
    byte field_8D0[0x14];
    s32  field_8E4;
    u8   field_8E8;
    u8   field_8E9;
    s16  field_8EA;
    /// Three sphere nodes, each with its 12-entry contact table, and a fourth
    /// node carrying a capsule.
    WorldCollisionBody    field_8EC;
    WorldCollisionContact field_90C;
    byte                  pad_924[0x108];
    WorldCollisionBody    field_A2C;
    WorldCollisionContact field_A4C;
    byte                  pad_A64[0x108];
    WorldCollisionBody    field_B6C;
    WorldCollisionContact field_B8C;
    byte                  pad_BA4[0x108];
    WorldCollisionBody    field_CAC;
    /// Its second endpoint's Z offset at 0xCD8 is 0x2BC at spawn and -0x320
    /// in the movement tick.
    WorldCollisionCapsule field_CCC;
    /// The capsule's 12 contacts, scanned for a key reading 0x100000.
    WorldCollisionContact    field_CE4[12];
    MATRIX                   field_E04;
    MATRIX                   field_E24;
    byte                     pad_E44[0x20];
    s16                      field_E64;
    u16                      field_E66;
    byte                     pad_E68[8];
    s16                      field_E70;
    s16                      field_E72;
    s16                      field_E74;
    byte                     pad_E76[2];
    s16                      field_E78;
    byte                     pad_E7A[2];
    DesertChaserAnimCommand* field_E7C;
    s32                      field_E80;
    s32                      field_E84;
    s32                      field_E88;
    s32                      field_E8C;
    DesertChaserIdWord       field_E90;
    Task*                    field_E94;
    Task*                    field_E98;
    /// One-shot "already reported" latch the message handler clears.
    s16 field_E9C;
    /// Distance clamped to 0xFA0 after the gte rotation.
    s16  field_E9E;
    byte pad_EA0[2];
    u16  field_EA2;
    /// Halfword the idle tick reseeds `field_6` from.
    u16 field_EA4;
    u16 field_EA6;
    /// Halfword pair forwarded under message 0x109, one step behind.
    u16  field_EA8;
    u16  field_EAA;
    s16  field_EAC;
    byte pad_EAE[2];
#endif
} DesertChaserWork;
#if DESERT_CHASER_BUILD == DESERT_CHASER_UNARMED
STATIC_ASSERT_SIZEOF(DesertChaserWork, 0x934);
#elif DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
STATIC_ASSERT_SIZEOF(DesertChaserWork, 0xC30);
#else
STATIC_ASSERT_SIZEOF(DesertChaserWork, 0xEB0);
#endif

/// 0x1C-byte block `func_actor_323000_801645A4` pushes on the scratch stack:
/// the model root's world position for `Gp_UpdateActorColor`, and the local
/// point walked up the coordinate chain into view space.
typedef struct DesertChaserTickScratch {
    VECTOR  pos;
    SVECTOR local;
    s32     pad_18;
} DesertChaserTickScratch;
STATIC_ASSERT_SIZEOF(DesertChaserTickScratch, 0x1C);

void desertChaserBlendTick(Task* task);
void desertChaserAnimTick(Task* task);
void desertChaserSpawn(Enemy* enemy, Task* task);
s32  desertChaserSetVisibility(Task* task, s32 arg1, s32 arg2);

/* Defined by each package. */
s32 desertChaserAnimCues(Task* task, DesertChaserWork* work);

void desertChaserFrameState(Enemy* enemy, Task* task);
void desertChaserPartEffect(Task* arg0, s16 part, s16 flags);
void desertChaserTask(Task* task);
void desertChaserHideState(Enemy* arg0, Task* arg1);
s32  desertChaserMsgPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);
void desertChaserExit(Task* task);

#endif /* SRC_SHARED_DESERT_CHASER_H */
