#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/player_detection.h"
#include "../../shared/actor_messages.h"
#include "../../shared/actor_contacts.h"
// Which of the two Odd Stranger builds this package is (odd_stranger.h).
#define ODD_STRANGER_VARIANT 1
#include "../../shared/odd_stranger.h"

/// Animation view of `OddStrangerWork`'s prefix. `func_800B3F84` is handed the
/// context, the pose buffer just past its slot array, and the array itself;
/// the work block's own fields at 0x898 and up are not repeated here. Same
/// shape as `Actor401300AnimWork`, 4 bytes earlier.
typedef struct Actor401000AnimWork {
    /* 0x000 */ byte           pad_0[0x1C];
    /* 0x01C */ ActorAnimRig19 rig;
    /* 0x458 */ ActorAnimRig19 blend;
    /* 0x894 */ byte           pad_894[0x4];
} Actor401000AnimWork;
STATIC_ASSERT_SIZEOF(Actor401000AnimWork, 0x898);

/// Animation bank `func_actor_401000_80133274` hands to both `func_800B3F84`
/// calls; the same `s32` the 401300 sibling keeps in `D_actor_401300_80158838`.
extern AnimationSet* gOddStrangerAnimSets[46];

/// Parameter pair `func_actor_401000_80133274` installs as `Enemy::param`
/// and reads `hpMax` out of as the actor's initial `field_40`.
extern EnemyParams D_actor_401000_8013E09C;

/// Three combat-parameter records `func_actor_401000_80133274` picks between
/// with `Task::spawnArg1 & 0xF`.
extern ActorSpawnParamRow D_actor_401000_8013E0AC[3];

/// Animation table `func_actor_401000_80133274` writes to `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32  (*call0)(Task*);
        s32  (*call1)(Task*, s32, AnimationPlayRequest*);
        s32  (*call2)(Task*, s32, ActorTransform*);
        s32  (*call3)(Task*, s32, s32);
        s32  (*call4)(Task*, s32, u16*);
        void (*call5)(void);
    } handler;
} Actor401000MessageEntry;
STATIC_ASSERT_SIZEOF(Actor401000MessageEntry, 8);

extern Actor401000MessageEntry D_actor_401000_80154F90[8];

/// Overlay-data word `oddStrangerDormant` points
/// `gOddStrangerAnimSets[16]` at on entering its state.
extern AnimationSet gOddStrangerDormantAnimSet;

/// Message 0x3FF payload of `oddStrangerGrabHold` and
/// `oddStrangerGrabRelease`: the animation argument the player task reads
/// when the actor's live-actor flag goes up.
extern AnimationPlayRequest gOddStrangerPlayerAnim;

/// Animation blocks selected for the grab by the player-character flag.
extern AnimationSet* D_actor_401000_80154F00[7];

/// Free-running scroll the actor's forward draw accumulates into:
/// `oddStrangerChase` adds `field_C04` to it every frame, and the
/// walk state zeroes it on entry. The same slot `Actor01900` keeps in
/// `Actor01900_D172FC`.
extern u16 gOddStrangerChaseDistance;

/// Twelve `SVECTOR` hit positions `oddStrangerSpawnHitEffect` picks from by
/// damage magnitude. The fourth halfword (`pad`, unused by the effect) is the
/// model part index the spawned effect anchors to. Same table as the
/// `Actor00100_D1B9F4` one `Actor00100_Fn03340` reads.
extern SVECTOR gOddStrangerHitOffsets[12];

/// The two `ActorHeightClamp` rows `func_actor_401000_801352DC` and
/// `func_actor_401000_80135374` walk.
extern ActorHeightClamp D_actor_401000_80154FD0[];

/// Message 0x3E9 payload of `oddStrangerGrab`: the player task's
/// world position, then the yaw from the actor to it, handed straight to the
/// slot-3 handler. The 401000 twin of the block `func_actor_401300_80138800`
/// keeps inline at `Actor401300Work.field_CD4` / `.field_CE4`.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401000_8013B1E4`.

/// The records closing four of the overlay's model streams, which
/// `func_actor_401000_8013B1E4` points `D_80114B34[5].data.model` at before spawning, one per
/// animation-latch key frame (`field_6` 3, 5, 7, 8).
extern TmdSource gOddStrangerBurstModelA;
extern TmdSource D_actor_401000_80144830;
extern TmdSource gOddStrangerBurstModelC;
extern TmdSource gOddStrangerBurstModelB;

/// Handlers defined after the state table and the init that name them.
static void func_actor_401000_8013DB10(Task* arg0);
static void func_actor_401000_8013DEC8(Task* arg0);

/// Integer part of the last delta `ActorContact_PushContact` resolved.
extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

extern OddStrangerTransformStorage gOddStrangerGrabTransform;

extern GpDelayArg D_actor_401000_80155038;

/// Transition table the clip change seeks through: one byte per
/// (playing clip, requested clip) pair, 0x2D requested clips to a row.
extern s8 gOddStrangerTransitions[45][45];

extern TmdSource D_actor_401000_80143614;
void             func_actor_401000_8013E038(Task*);

extern AnimationSet D_actor_401000_80152480;
extern AnimationSet D_actor_401000_80152E78;
extern AnimationSet D_actor_401000_801537D8;

extern AnimationSet D_actor_401000_80146CB0;
extern AnimationSet D_actor_401000_80147734;
extern AnimationSet D_actor_401000_801481A4;
extern AnimationSet D_actor_401000_80148784;
extern AnimationSet D_actor_401000_80148D68;
extern AnimationSet D_actor_401000_801497BC;
extern AnimationSet D_actor_401000_8014A500;
extern AnimationSet D_actor_401000_8014AD70;
extern AnimationSet D_actor_401000_8014B100;
extern AnimationSet D_actor_401000_8014B938;
extern AnimationSet D_actor_401000_8014BE88;
extern AnimationSet D_actor_401000_8014CDA0;
extern AnimationSet D_actor_401000_8014D144;
extern AnimationSet D_actor_401000_8014D4F8;
extern AnimationSet D_actor_401000_8014D87C;
extern AnimationSet D_actor_401000_8014DF70;
extern AnimationSet D_actor_401000_8014E7D4;
extern AnimationSet D_actor_401000_8014F04C;
extern AnimationSet D_actor_401000_8014F69C;
extern AnimationSet D_actor_401000_80150070;
extern AnimationSet D_actor_401000_80150EE8;
extern AnimationSet D_actor_401000_80151DF8;

DamageAttack D_actor_401000_8013E094[2] = {
    { 20, 7 },
    { 18, 0 },
};

EnemyParams D_actor_401000_8013E09C = { D_actor_401000_8013E094, 180, 42, 82, 4, 100, 10, 100, 0 };

ActorSpawnParamRow D_actor_401000_8013E0AC[3] = {
    { 20, 900, 12, 2000, { 0, 0, 0, 0 } },
    { 10, 800, 12, 2500, { 0, 0, 0, 0 } },
    { 0, 500, 12, 3000, { 0, 0, 0, 0 } },
};

TmdBone D_actor_401000_8013E0D0[19] = {
#include "assets/actor_401000_model_117F4_skeleton.inc"
};

u32 D_actor_401000_8013E37C[19] = {
#include "assets/actor_401000_model_117F4_partVerts.inc"
};

SVECTOR D_actor_401000_8013E3C8[311] = {
#include "assets/actor_401000_model_117F4_verts.inc"
};

SVECTOR D_actor_401000_8013ED80[309] = {
#include "assets/actor_401000_model_117F4_normals.inc"
};

u32 D_actor_401000_8013F728[4027] = {
#include "assets/actor_401000_model_117F4_stream.inc"
};

TmdSource D_actor_401000_80143614 = {
    0,
    20180,
    7860,
    19,
    D_actor_401000_8013E37C,
    D_actor_401000_8013E3C8,
    D_actor_401000_8013ED80,
    D_actor_401000_8013E0D0,
    D_actor_401000_8013F728,
};

TmdBone D_actor_401000_80143638[3] = {
#include "assets/actor_401000_model_12094_skeleton.inc"
};

u32 D_actor_401000_801436A4[3] = {
#include "assets/actor_401000_model_12094_partVerts.inc"
};

SVECTOR D_actor_401000_801436B0[33] = {
#include "assets/actor_401000_model_12094_verts.inc"
};

SVECTOR D_actor_401000_801437B8[45] = {
#include "assets/actor_401000_model_12094_normals.inc"
};

u32 D_actor_401000_80143920[357] = {
#include "assets/actor_401000_model_12094_stream.inc"
};

TmdSource gOddStrangerBurstModelA = {
    0,
    2008,
    304,
    3,
    D_actor_401000_801436A4,
    D_actor_401000_801436B0,
    D_actor_401000_801437B8,
    D_actor_401000_80143638,
    D_actor_401000_80143920,
};

TmdBone D_actor_401000_80143ED8[3] = {
#include "assets/actor_401000_model_12A10_skeleton.inc"
};

u32 D_actor_401000_80143F44[3] = {
#include "assets/actor_401000_model_12A10_partVerts.inc"
};

SVECTOR D_actor_401000_80143F50[37] = {
#include "assets/actor_401000_model_12A10_verts.inc"
};

SVECTOR D_actor_401000_80144078[50] = {
#include "assets/actor_401000_model_12A10_normals.inc"
};

u32 D_actor_401000_80144208[394] = {
#include "assets/actor_401000_model_12A10_stream.inc"
};

TmdSource D_actor_401000_80144830 = {
    0,
    2228,
    368,
    3,
    D_actor_401000_80143F44,
    D_actor_401000_80143F50,
    D_actor_401000_80144078,
    D_actor_401000_80143ED8,
    D_actor_401000_80144208,
};

TmdBone D_actor_401000_80144854[3] = {
#include "assets/actor_401000_model_13B7C_skeleton.inc"
};

u32 D_actor_401000_801448C0[3] = {
#include "assets/actor_401000_model_13B7C_partVerts.inc"
};

SVECTOR D_actor_401000_801448CC[76] = {
#include "assets/actor_401000_model_13B7C_verts.inc"
};

SVECTOR D_actor_401000_80144B2C[76] = {
#include "assets/actor_401000_model_13B7C_normals.inc"
};

u32 D_actor_401000_80144D8C[772] = {
#include "assets/actor_401000_model_13B7C_stream.inc"
};

TmdSource gOddStrangerBurstModelC = {
    0,
    4600,
    688,
    3,
    D_actor_401000_801448C0,
    D_actor_401000_801448CC,
    D_actor_401000_80144B2C,
    D_actor_401000_80144854,
    D_actor_401000_80144D8C,
};

TmdBone D_actor_401000_801459C0[1] = {
#include "assets/actor_401000_model_14370_skeleton.inc"
};

u32 D_actor_401000_801459E4[1] = {
#include "assets/actor_401000_model_14370_partVerts.inc"
};

SVECTOR D_actor_401000_801459E8[37] = {
#include "assets/actor_401000_model_14370_verts.inc"
};

SVECTOR D_actor_401000_80145B10[42] = {
#include "assets/actor_401000_model_14370_normals.inc"
};

u32 D_actor_401000_80145C60[332] = {
#include "assets/actor_401000_model_14370_stream.inc"
};

TmdSource gOddStrangerBurstModelB = {
    0,
    2192,
    0,
    1,
    D_actor_401000_801459E4,
    D_actor_401000_801459E8,
    D_actor_401000_80145B10,
    D_actor_401000_801459C0,
    D_actor_401000_80145C60,
};

AnimationPackedPose D_actor_401000_801461B4[28] = {
#include "assets/actor_401000_animation_14E90_bank1.inc"
};

AnimationPackedRotation D_actor_401000_80146304[247] = {
#include "assets/actor_401000_animation_14E90_bank4.inc"
};

AnimationRecord D_actor_401000_801466E0[362] = {
#include "assets/actor_401000_animation_14E90_records.inc"
};

u16 D_actor_401000_80146C88[20] = {
#include "assets/actor_401000_animation_14E90_indices.inc"
};

AnimationSet D_actor_401000_80146CB0 = {
    D_actor_401000_801466E0,
    D_actor_401000_80146C88,
    { NULL, D_actor_401000_801461B4, NULL, NULL, D_actor_401000_80146304, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_80146CD8[32] = {
#include "assets/actor_401000_animation_15914_bank1.inc"
};

AnimationPackedRotation D_actor_401000_80146E58[223] = {
#include "assets/actor_401000_animation_15914_bank4.inc"
};

AnimationRecord D_actor_401000_801471D4[334] = {
#include "assets/actor_401000_animation_15914_records.inc"
};

u16 D_actor_401000_8014770C[20] = {
#include "assets/actor_401000_animation_15914_indices.inc"
};

AnimationSet D_actor_401000_80147734 = {
    D_actor_401000_801471D4,
    D_actor_401000_8014770C,
    { NULL, D_actor_401000_80146CD8, NULL, NULL, D_actor_401000_80146E58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014775C[24] = {
#include "assets/actor_401000_animation_16384_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014787C[248] = {
#include "assets/actor_401000_animation_16384_bank4.inc"
};

AnimationRecord D_actor_401000_80147C5C[328] = {
#include "assets/actor_401000_animation_16384_records.inc"
};

u16 D_actor_401000_8014817C[20] = {
#include "assets/actor_401000_animation_16384_indices.inc"
};

AnimationSet D_actor_401000_801481A4 = {
    D_actor_401000_80147C5C,
    D_actor_401000_8014817C,
    { NULL, D_actor_401000_8014775C, NULL, NULL, D_actor_401000_8014787C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_801481CC[10] = {
#include "assets/actor_401000_animation_16964_bank1.inc"
};

AnimationPackedRotation D_actor_401000_80148244[138] = {
#include "assets/actor_401000_animation_16964_bank4.inc"
};

AnimationRecord D_actor_401000_8014846C[188] = {
#include "assets/actor_401000_animation_16964_records.inc"
};

u16 D_actor_401000_8014875C[20] = {
#include "assets/actor_401000_animation_16964_indices.inc"
};

AnimationSet D_actor_401000_80148784 = {
    D_actor_401000_8014846C,
    D_actor_401000_8014875C,
    { NULL, D_actor_401000_801481CC, NULL, NULL, D_actor_401000_80148244, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_801487AC[8] = {
#include "assets/actor_401000_animation_16F48_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014880C[151] = {
#include "assets/actor_401000_animation_16F48_bank4.inc"
};

AnimationRecord D_actor_401000_80148A68[182] = {
#include "assets/actor_401000_animation_16F48_records.inc"
};

u16 D_actor_401000_80148D40[20] = {
#include "assets/actor_401000_animation_16F48_indices.inc"
};

AnimationSet D_actor_401000_80148D68 = {
    D_actor_401000_80148A68,
    D_actor_401000_80148D40,
    { NULL, D_actor_401000_801487AC, NULL, NULL, D_actor_401000_8014880C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_80148D90[20] = {
#include "assets/actor_401000_animation_1799C_bank1.inc"
};

AnimationPackedRotation D_actor_401000_80148E80[228] = {
#include "assets/actor_401000_animation_1799C_bank4.inc"
};

AnimationRecord D_actor_401000_80149210[353] = {
#include "assets/actor_401000_animation_1799C_records.inc"
};

u16 D_actor_401000_80149794[20] = {
#include "assets/actor_401000_animation_1799C_indices.inc"
};

AnimationSet D_actor_401000_801497BC = {
    D_actor_401000_80149210,
    D_actor_401000_80149794,
    { NULL, D_actor_401000_80148D90, NULL, NULL, D_actor_401000_80148E80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_801497E4[23] = {
#include "assets/actor_401000_animation_186E0_bank1.inc"
};

AnimationPackedRotation D_actor_401000_801498F8[334] = {
#include "assets/actor_401000_animation_186E0_bank4.inc"
};

AnimationRecord D_actor_401000_80149E30[426] = {
#include "assets/actor_401000_animation_186E0_records.inc"
};

u16 D_actor_401000_8014A4D8[20] = {
#include "assets/actor_401000_animation_186E0_indices.inc"
};

AnimationSet D_actor_401000_8014A500 = {
    D_actor_401000_80149E30,
    D_actor_401000_8014A4D8,
    { NULL, D_actor_401000_801497E4, NULL, NULL, D_actor_401000_801498F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014A528[14] = {
#include "assets/actor_401000_animation_18F50_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014A5D0[216] = {
#include "assets/actor_401000_animation_18F50_bank4.inc"
};

AnimationRecord D_actor_401000_8014A930[262] = {
#include "assets/actor_401000_animation_18F50_records.inc"
};

u16 D_actor_401000_8014AD48[20] = {
#include "assets/actor_401000_animation_18F50_indices.inc"
};

AnimationSet D_actor_401000_8014AD70 = {
    D_actor_401000_8014A930,
    D_actor_401000_8014AD48,
    { NULL, D_actor_401000_8014A528, NULL, NULL, D_actor_401000_8014A5D0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014AD98[5] = {
#include "assets/actor_401000_animation_192E0_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014ADD4[82] = {
#include "assets/actor_401000_animation_192E0_bank4.inc"
};

AnimationRecord D_actor_401000_8014AF1C[111] = {
#include "assets/actor_401000_animation_192E0_records.inc"
};

u16 D_actor_401000_8014B0D8[20] = {
#include "assets/actor_401000_animation_192E0_indices.inc"
};

AnimationSet D_actor_401000_8014B100 = {
    D_actor_401000_8014AF1C,
    D_actor_401000_8014B0D8,
    { NULL, D_actor_401000_8014AD98, NULL, NULL, D_actor_401000_8014ADD4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014B128[16] = {
#include "assets/actor_401000_animation_19B18_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014B1E8[199] = {
#include "assets/actor_401000_animation_19B18_bank4.inc"
};

AnimationRecord D_actor_401000_8014B504[259] = {
#include "assets/actor_401000_animation_19B18_records.inc"
};

u16 D_actor_401000_8014B910[20] = {
#include "assets/actor_401000_animation_19B18_indices.inc"
};

AnimationSet D_actor_401000_8014B938 = {
    D_actor_401000_8014B504,
    D_actor_401000_8014B910,
    { NULL, D_actor_401000_8014B128, NULL, NULL, D_actor_401000_8014B1E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014B960[10] = {
#include "assets/actor_401000_animation_1A068_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014B9D8[113] = {
#include "assets/actor_401000_animation_1A068_bank4.inc"
};

AnimationRecord D_actor_401000_8014BB9C[177] = {
#include "assets/actor_401000_animation_1A068_records.inc"
};

u16 D_actor_401000_8014BE60[20] = {
#include "assets/actor_401000_animation_1A068_indices.inc"
};

AnimationSet D_actor_401000_8014BE88 = {
    D_actor_401000_8014BB9C,
    D_actor_401000_8014BE60,
    { NULL, D_actor_401000_8014B960, NULL, NULL, D_actor_401000_8014B9D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014BEB0[26] = {
#include "assets/actor_401000_animation_1AF80_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014BFE8[330] = {
#include "assets/actor_401000_animation_1AF80_bank4.inc"
};

AnimationRecord D_actor_401000_8014C510[538] = {
#include "assets/actor_401000_animation_1AF80_records.inc"
};

u16 D_actor_401000_8014CD78[20] = {
#include "assets/actor_401000_animation_1AF80_indices.inc"
};

AnimationSet D_actor_401000_8014CDA0 = {
    D_actor_401000_8014C510,
    D_actor_401000_8014CD78,
    { NULL, D_actor_401000_8014BEB0, NULL, NULL, D_actor_401000_8014BFE8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014CDC8[10] = {
#include "assets/actor_401000_animation_1B324_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014CE40[73] = {
#include "assets/actor_401000_animation_1B324_bank4.inc"
};

AnimationRecord D_actor_401000_8014CF64[110] = {
#include "assets/actor_401000_animation_1B324_records.inc"
};

u16 D_actor_401000_8014D11C[20] = {
#include "assets/actor_401000_animation_1B324_indices.inc"
};

AnimationSet D_actor_401000_8014D144 = {
    D_actor_401000_8014CF64,
    D_actor_401000_8014D11C,
    { NULL, D_actor_401000_8014CDC8, NULL, NULL, D_actor_401000_8014CE40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014D16C[6] = {
#include "assets/actor_401000_animation_1B6D8_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014D1B4[81] = {
#include "assets/actor_401000_animation_1B6D8_bank4.inc"
};

AnimationRecord D_actor_401000_8014D2F8[118] = {
#include "assets/actor_401000_animation_1B6D8_records.inc"
};

u16 D_actor_401000_8014D4D0[20] = {
#include "assets/actor_401000_animation_1B6D8_indices.inc"
};

AnimationSet D_actor_401000_8014D4F8 = {
    D_actor_401000_8014D2F8,
    D_actor_401000_8014D4D0,
    { NULL, D_actor_401000_8014D16C, NULL, NULL, D_actor_401000_8014D1B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014D520[13] = {
#include "assets/actor_401000_animation_1BA5C_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014D5BC[58] = {
#include "assets/actor_401000_animation_1BA5C_bank4.inc"
};

AnimationRecord D_actor_401000_8014D6A4[108] = {
#include "assets/actor_401000_animation_1BA5C_records.inc"
};

u16 D_actor_401000_8014D854[20] = {
#include "assets/actor_401000_animation_1BA5C_indices.inc"
};

AnimationSet D_actor_401000_8014D87C = {
    D_actor_401000_8014D6A4,
    D_actor_401000_8014D854,
    { NULL, D_actor_401000_8014D520, NULL, NULL, D_actor_401000_8014D5BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014D8A4[15] = {
#include "assets/actor_401000_animation_1C150_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014D958[159] = {
#include "assets/actor_401000_animation_1C150_bank4.inc"
};

AnimationRecord D_actor_401000_8014DBD4[221] = {
#include "assets/actor_401000_animation_1C150_records.inc"
};

u16 D_actor_401000_8014DF48[20] = {
#include "assets/actor_401000_animation_1C150_indices.inc"
};

AnimationSet D_actor_401000_8014DF70 = {
    D_actor_401000_8014DBD4,
    D_actor_401000_8014DF48,
    { NULL, D_actor_401000_8014D8A4, NULL, NULL, D_actor_401000_8014D958, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014DF98[20] = {
#include "assets/actor_401000_animation_1C9B4_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014E088[190] = {
#include "assets/actor_401000_animation_1C9B4_bank4.inc"
};

AnimationRecord D_actor_401000_8014E380[267] = {
#include "assets/actor_401000_animation_1C9B4_records.inc"
};

u16 D_actor_401000_8014E7AC[20] = {
#include "assets/actor_401000_animation_1C9B4_indices.inc"
};

AnimationSet D_actor_401000_8014E7D4 = {
    D_actor_401000_8014E380,
    D_actor_401000_8014E7AC,
    { NULL, D_actor_401000_8014DF98, NULL, NULL, D_actor_401000_8014E088, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014E7FC[20] = {
#include "assets/actor_401000_animation_1D22C_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014E8EC[196] = {
#include "assets/actor_401000_animation_1D22C_bank4.inc"
};

AnimationRecord D_actor_401000_8014EBFC[266] = {
#include "assets/actor_401000_animation_1D22C_records.inc"
};

u16 D_actor_401000_8014F024[20] = {
#include "assets/actor_401000_animation_1D22C_indices.inc"
};

AnimationSet D_actor_401000_8014F04C = {
    D_actor_401000_8014EBFC,
    D_actor_401000_8014F024,
    { NULL, D_actor_401000_8014E7FC, NULL, NULL, D_actor_401000_8014E8EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014F074[11] = {
#include "assets/actor_401000_animation_1D87C_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014F0F8[150] = {
#include "assets/actor_401000_animation_1D87C_bank4.inc"
};

AnimationRecord D_actor_401000_8014F350[201] = {
#include "assets/actor_401000_animation_1D87C_records.inc"
};

u16 D_actor_401000_8014F674[20] = {
#include "assets/actor_401000_animation_1D87C_indices.inc"
};

AnimationSet D_actor_401000_8014F69C = {
    D_actor_401000_8014F350,
    D_actor_401000_8014F674,
    { NULL, D_actor_401000_8014F074, NULL, NULL, D_actor_401000_8014F0F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_8014F6C4[18] = {
#include "assets/actor_401000_animation_1E250_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8014F79C[232] = {
#include "assets/actor_401000_animation_1E250_bank4.inc"
};

AnimationRecord D_actor_401000_8014FB3C[323] = {
#include "assets/actor_401000_animation_1E250_records.inc"
};

u16 D_actor_401000_80150048[20] = {
#include "assets/actor_401000_animation_1E250_indices.inc"
};

AnimationSet D_actor_401000_80150070 = {
    D_actor_401000_8014FB3C,
    D_actor_401000_80150048,
    { NULL, D_actor_401000_8014F6C4, NULL, NULL, D_actor_401000_8014F79C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_80150098[33] = {
#include "assets/actor_401000_animation_1F0C8_bank1.inc"
};

AnimationPackedRotation D_actor_401000_80150224[326] = {
#include "assets/actor_401000_animation_1F0C8_bank4.inc"
};

AnimationRecord D_actor_401000_8015073C[481] = {
#include "assets/actor_401000_animation_1F0C8_records.inc"
};

u16 D_actor_401000_80150EC0[20] = {
#include "assets/actor_401000_animation_1F0C8_indices.inc"
};

AnimationSet D_actor_401000_80150EE8 = {
    D_actor_401000_8015073C,
    D_actor_401000_80150EC0,
    { NULL, D_actor_401000_80150098, NULL, NULL, D_actor_401000_80150224, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_80150F10[26] = {
#include "assets/actor_401000_animation_1FFD8_bank1.inc"
};

AnimationPackedRotation D_actor_401000_80151048[351] = {
#include "assets/actor_401000_animation_1FFD8_bank4.inc"
};

AnimationRecord D_actor_401000_801515C4[515] = {
#include "assets/actor_401000_animation_1FFD8_records.inc"
};

u16 D_actor_401000_80151DD0[20] = {
#include "assets/actor_401000_animation_1FFD8_indices.inc"
};

AnimationSet D_actor_401000_80151DF8 = {
    D_actor_401000_801515C4,
    D_actor_401000_80151DD0,
    { NULL, D_actor_401000_80150F10, NULL, NULL, D_actor_401000_80151048, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_80151E20[15] = {
#include "assets/actor_401000_animation_20660_bank1.inc"
};

AnimationPackedRotation D_actor_401000_80151ED4[154] = {
#include "assets/actor_401000_animation_20660_bank4.inc"
};

AnimationRecord D_actor_401000_8015213C[199] = {
#include "assets/actor_401000_animation_20660_records.inc"
};

u16 D_actor_401000_80152458[20] = {
#include "assets/actor_401000_animation_20660_indices.inc"
};

AnimationSet D_actor_401000_80152480 = {
    D_actor_401000_8015213C,
    D_actor_401000_80152458,
    { NULL, D_actor_401000_80151E20, NULL, NULL, D_actor_401000_80151ED4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_801524A8[19] = {
#include "assets/actor_401000_animation_21058_bank1.inc"
};

AnimationPackedRotation D_actor_401000_8015258C[250] = {
#include "assets/actor_401000_animation_21058_bank4.inc"
};

AnimationRecord D_actor_401000_80152974[311] = {
#include "assets/actor_401000_animation_21058_records.inc"
};

u16 D_actor_401000_80152E50[20] = {
#include "assets/actor_401000_animation_21058_indices.inc"
};

AnimationSet D_actor_401000_80152E78 = {
    D_actor_401000_80152974,
    D_actor_401000_80152E50,
    { NULL, D_actor_401000_801524A8, NULL, NULL, D_actor_401000_8015258C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_80152EA0[21] = {
#include "assets/actor_401000_animation_219B8_bank1.inc"
};

AnimationPackedRotation D_actor_401000_80152F9C[233] = {
#include "assets/actor_401000_animation_219B8_bank4.inc"
};

AnimationRecord D_actor_401000_80153340[284] = {
#include "assets/actor_401000_animation_219B8_records.inc"
};

u16 D_actor_401000_801537B0[20] = {
#include "assets/actor_401000_animation_219B8_indices.inc"
};

AnimationSet D_actor_401000_801537D8 = {
    D_actor_401000_80153340,
    D_actor_401000_801537B0,
    { NULL, D_actor_401000_80152EA0, NULL, NULL, D_actor_401000_80152F9C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401000_80153800[18] = {
#include "assets/actor_401000_animation_22814_bank1.inc"
};

AnimationPackedRotation D_actor_401000_801538D8[361] = {
#include "assets/actor_401000_animation_22814_bank4.inc"
};

AnimationRecord D_actor_401000_80153E7C[484] = {
#include "assets/actor_401000_animation_22814_records.inc"
};

u16 D_actor_401000_8015460C[20] = {
#include "assets/actor_401000_animation_22814_indices.inc"
};

AnimationSet gOddStrangerDormantAnimSet = {
    D_actor_401000_80153E7C,
    D_actor_401000_8015460C,
    { NULL, D_actor_401000_80153800, NULL, NULL, D_actor_401000_801538D8, NULL, NULL, NULL },
};

s8 gOddStrangerTransitions[45][45] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 4, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 3, 3, 0, 0, 0, 0, 15, 5, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 4, 0, 0, 4, 3, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 3, 3, 3, 3, 5, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 10, 10, 3, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 12, 12, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 8, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AnimationSet* gOddStrangerAnimSets[46] = {
    NULL,
    NULL,
    &D_actor_401000_80146CB0,
    &D_actor_401000_80147734,
    &D_actor_401000_80151DF8,
    &D_actor_401000_8014F69C,
    &D_actor_401000_80150070,
    &D_actor_401000_80150EE8,
    &D_actor_401000_8014A500,
    &D_actor_401000_8014B938,
    &D_actor_401000_8014D144,
    &D_actor_401000_8014D4F8,
    &D_actor_401000_80148D68,
    &D_actor_401000_80148784,
    &D_actor_401000_8014CDA0,
    &D_actor_401000_801497BC,
    NULL,
    &D_actor_401000_8014BE88,
    &D_actor_401000_801481A4,
    &D_actor_401000_8014DF70,
    &D_actor_401000_8014E7D4,
    &D_actor_401000_8014F04C,
    &D_actor_401000_8014AD70,
    &D_actor_401000_8014D4F8,
    &D_actor_401000_80148D68,
    &D_actor_401000_8014B100,
    &D_actor_401000_8014D87C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationSet* D_actor_401000_80154F00[7] = {
    NULL,
    NULL,
    NULL,
    &D_actor_401000_80152480,
    &D_actor_401000_80152E78,
    &D_actor_401000_801537D8,
    NULL,
};

AnimationPlayRequest gOddStrangerPlayerAnim = { { .sets = D_actor_401000_80154F00 }, 1, ANIMATION_BLEND_RESET, 3, ANIMATION_WORLD_COLLISION_DISABLE };

SVECTOR gOddStrangerHitOffsets[12] = {
    { 60, -12, 30, 2 },
    { -50, -130, 29, 2 },
    { 20, -70, 25, 2 },
    { -30, -65, 25, 2 },
    { 60, -120, 30, 2 },
    { 20, -20, -5, 2 },
    { -15, -50, 0, 2 },
    { 2, 10, -15, 2 },
    { 14, 0, 0, 7 },
    { 25, 0, 0, 2 },
    { -14, 0, 0, 9 },
    { -25, 0, 0, 2 },
};

void func_actor_401000_8013D68C(void);
s32  oddStrangerApplyCommand(Task*, s32, u16*);

Actor401000MessageEntry D_actor_401000_80154F90[8] = {
    { 2015, { .call5 = func_actor_401000_8013D68C } },
    { 2003, { .call1 = oddStrangerPlayMessage } },
    { 2005, { .call3 = actorMsgSetVisibility } },
    { 2006, { .call0 = actorMsgIsPresent } },
    { 2004, { .call2 = actorMsgPlaceRecordYaw } },
    { 2014, { .call0 = actorMsgReleaseHold } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call4 = oddStrangerApplyCommand } },
    { 2147483647, { .call0 = NULL } },
};

ActorHeightClamp D_actor_401000_80154FD0[3] = {
    { 1, 3, -300, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 5, 29, 0, 300, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
};

u16 gOddStrangerChaseDistance = 0;

TaskDesc D_actor_401000_80155004 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER), 96 } }, func_actor_401000_8013E038, { .model = &D_actor_401000_80143614 } };

SVECTOR ActorContact_ScratchPosition;

OddStrangerTransformStorage gOddStrangerGrabTransform;

GpDelayArg D_actor_401000_80155038;

static __inline__ void Actor401000_BindMatrices(Task* actor);
static __inline__ void Actor401000_InitPose(GfxCoord* coord, OddStrangerWork* work);
static void            func_actor_401000_80133274(Enemy* enemy, Task* actor);
static void            oddStrangerTakeHit(Task* arg0);
static void            func_actor_401000_801352DC(GameLocationKey* session, GfxCoord* coord);
static __inline__ s32  Actor401000_HasHeightClamp(GameLocationKey* session);
static s32             func_actor_401000_80135374(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3);
static void            func_actor_401000_80135AA4(Task* arg0);
static void            func_actor_401000_801378DC(Task* arg0);
static void            func_actor_401000_801388F4(Task* arg0);
static void            func_actor_401000_80138BB4(Task* arg0);
static void            func_actor_401000_80138F50(Task* arg0);
static void            func_actor_401000_8013A930(Task* arg0);
static void            func_actor_401000_8013B1E4(Task* arg0);
static void            func_actor_401000_8013CD9C(Task* arg0);
static void            func_actor_401000_8013CEF0(Task* arg0);

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/player_detection_reach.inc.c"

#include "../../shared/player_detection_sight.inc.c"

#include "../../shared/odd_stranger_tick_blended.inc.c"

#include "../../shared/odd_stranger_anim_event.inc.c"

#include "../../shared/odd_stranger_drive.inc.c"

/// Points the model's light and color matrices at the work block's copies.
static __inline__ void Actor401000_BindMatrices(Task* actor)
{
    OddStrangerWork* work;
    TmdObject*       obj;

    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->field_B88;
    obj->colorMtx = &work->field_BA8;
}

/// Rebuilds the root coordinate's Y rotation at the actor's 0x1194 scale and
/// drops both obstacle tables.
static __inline__ void Actor401000_InitPose(GfxCoord* coord, OddStrangerWork* work)
{
    actorRescaleYaw(coord, 0x1194);
    work->field_C7C = 0;
    Gp_ClearRec18Occupied(work->field_A30);
    Gp_ClearRec18Occupied(work->field_8F0);
}

/// Enemy init: allocates the 0xC80-byte work block, binds the model's light
/// and colour matrices to its copies, seeds both animation contexts and the
/// three `WorldCollisionBody` nodes, then picks the opening clip from the low bits of
/// `Enemy::placeKey` and the `field_C10` parameter run from the spawn flags.
/// The tail rebuilds the root coordinate through `Actor401000_InitPose`.
static void func_actor_401000_80133274(Enemy* enemy, Task* actor)
{
    SVECTOR             dir;
    VECTOR              pos;
    SVECTOR*            v;
    TmdObject*          obj;
    GfxCoord*           root;
    OddStrangerWork*    work;
    WorldCollisionBody* body;
    WorldCollisionBody* head;
    s32                 variant;

    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(0xC80, 0);
    actor->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, actor);
        return;
    }
    (Gp_IncStateF0Ref)(0);
    actor->exitCallback = oddStrangerExit;
    Actor401000_BindMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)D_actor_401000_8013E09C.hpMax;
    enemy->param                  = &D_actor_401000_8013E09C;
    enemy->recs                   = work->field_8F0;
    func_800B3F84(&((Actor401000AnimWork*)work)->rig.anim, gOddStrangerAnimSets, obj,
                  ((Actor401000AnimWork*)work)->rig.poses, ((Actor401000AnimWork*)work)->rig.slots);
    func_800B3F84(&((Actor401000AnimWork*)work)->blend.anim, gOddStrangerAnimSets,
                  obj, ((Actor401000AnimWork*)work)->blend.poses,
                  ((Actor401000AnimWork*)work)->blend.slots);
    work->field_898 = 2;
    work->field_89E = 2;
    work->field_89A = 0;
    work->field_8B0 = 0;
    work->field_8AE = 0;
    work->field_8A4 = 0x10;
    work->field_8A2 = 0x10;
    switch (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) % 5) {
        case 0:
            work->field_8A4 = 0x11;
            break;
        case 1:
            work->field_8A4 = 0xF;
            break;
        case 2:
            work->field_8A4 = 0x10;
            break;
        case 3:
            work->field_8A4 = 0x12;
            break;
        case 4:
        default:
            work->field_8A4 = 0xE;
            break;
    }
    oddStrangerDrive(actor);

    work->field_A10.context.contacts = work->field_A30;
    work->field_A10.coord            = root;
    work->field_A10.pos.vx           = 0;
    work->field_A10.pos.vy           = -0xAC;
    work->field_A10.pos.vz           = 0;
    work->field_A10.key              = 0x30000;
    work->field_A10.radius           = 0x12C;
    work->field_A10.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_A10);
    work->field_BE8        = 0;
    work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_InitRec18Table(work->field_A10.context.contacts, 0xC, 0);

    body                   = &work->field_8D0;
    body->coord            = &actor->extra.tmd->coords[2];
    body->context.contacts = work->field_8F0;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = 0x3000A;
    body->radius           = 0x1AE;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(body->context.contacts, 0xC, 0);
    work->field_8D0.key = 0x30000;

    dir.vx                 = 0;
    dir.vy                 = 0;
    dir.vz                 = 0;
    head                   = &work->field_B50;
    head->coord            = &actor->extra.tmd->coords[6];
    head->context.contacts = &work->field_B70;
    v                      = &dir;
    head->pos.vx           = v->vx;
    head->pos.vy           = v->vy;
    head->pos.vz           = v->vz;
    head->radius           = 0x180;
    head->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->context.contacts, 1, 0);

    work->field_14     = 0;
    work->field_C[0].x = actor->extra.tmd->coords->coord.t[0];
    work->field_C[0].z = actor->extra.tmd->coords->coord.t[2];
    Gfx_MatrixCol2(&actor->extra.tmd->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->field_C[1].x = actor->extra.tmd->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->extra.tmd->coords->coord.t[2] + dir.vz;

    actor->msgTable    = D_actor_401000_80154F90;
    root->parent       = &gGfxViewCoord;
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->field_8B8.coord      = &actor->extra.tmd->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    variant                    = (actor->spawnArg1.value >> 16);
    switch (variant & 0xF) {
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
    switch (actor->spawnArg1.value & 0xF) {
        case 2:
            work->field_C10 = D_actor_401000_8013E0AC[0].field_0;
            work->field_C12 = D_actor_401000_8013E0AC[0].field_2;
            work->field_C14 = D_actor_401000_8013E0AC[0].field_4;
            work->field_C16 = D_actor_401000_8013E0AC[0].field_6;
            break;
        case 1:
            work->field_C10 = D_actor_401000_8013E0AC[2].field_0;
            work->field_C12 = D_actor_401000_8013E0AC[2].field_2;
            work->field_C14 = D_actor_401000_8013E0AC[2].field_4;
            work->field_C16 = D_actor_401000_8013E0AC[2].field_6;
            break;
        case 0:
        default:
            work->field_C10 = D_actor_401000_8013E0AC[1].field_0;
            work->field_C12 = D_actor_401000_8013E0AC[1].field_2;
            work->field_C14 = D_actor_401000_8013E0AC[1].field_4;
            work->field_C16 = D_actor_401000_8013E0AC[1].field_6;
            break;
    }

    Actor401000_InitPose(actor->extra.tmd->coords, work);

    actor->state++;
}

#include "../../shared/odd_stranger_spawn_hit_effect.inc.c"

static void oddStrangerTakeHit(Task* arg0)
{
    PlayerStatus*    config = &gPlayerStatus;
    OddStrangerWork* work;
    Enemy*           enemy;
    ActorHitScratch* head;
    ActorHitScratch* s;
    GfxCoord*        coord;
    Task*            player;
    SVECTOR*         dir;
    s16              z;
    s32              yaw;
    s32              dx;
    s32              dy;
    s32              dz;
    s32              deathSound;
    s32              deathPan;
    s32              hitSound;
    s32              hitPan;
    s32              mag;
    s16              state;
    s16              effect;
    s16              timer;
    u32              damage;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if (enemy->hp > 0) {
        head  = SCRATCH_STACK_CURSOR(ActorHitScratch);
        s     = (SCRATCH_STACK_CURSOR(ActorHitScratch) = head - 1);
        s->id = actorFindHit(&head[-1].hitPos, work->field_8F0);
        if (s->id != 0) {
            if (s->id & 0x8000) {
                player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                s->hitPos.vx = player->extra.tmd->coords->workm.t[0];
                s->hitPos.vy = player->extra.tmd->coords->workm.t[1];
                s->hitPos.vz = player->extra.tmd->coords->workm.t[2];
            }
            if (work->field_C28 == 1) {
                Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 0, 0);
                work->field_C28 = 0;
                if (work->field_0 == 0xB || work->field_0 == 0xC || work->field_0 == 0xD || work->field_0 == 0xE) {
                    work->field_0 = 0x13;
                }
            }
            work->field_C24                       = 0;
            work->field_C26                       = 0;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
            s->dir.vx = arg0->extra.tmd->coords->workm.t[0];
            s->dir.vy = arg0->extra.tmd->coords->workm.t[1];
            s->dir.vz = arg0->extra.tmd->coords->workm.t[2];
            s->dir.vx = s->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            s->dir.vy = s->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            z         = s->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            s->dir.vz = z;
            yaw       = ratan2(s->dir.vx, z);
            coord     = arg0->extra.tmd->coords;
            s->yaw    = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->yaw    = actorNormalizeYaw(s->yaw);
            oddStrangerSpawnHitEffect(arg0, s->yaw, s->id);
            work->field_8B0 = 0;
            work->field_8AE = 0;
            s->effect       = -1;
            state           = work->field_0;
            if (state != 0x13 && state != 0x14 && state != 0x11 && state != 0x1F && state != 0x20 && state != 0xF && state != 0x10 && state != 4) {
                s->m = arg0->extra.tmd->coords->coord;
                gfxRotMatrixY(&s->m, s->yaw, 0);
                dir = &s->dir;
                Gfx_MatrixCol2(&s->m, dir);
                VectorNormalSS(dir, dir);
                if (work->field_BEC > 0) {
                    gte_lddp(-0x19);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                } else {
                    gte_lddp(-0x64);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                }
                arg0->extra.tmd->coords->coord.t[0]  += s->dir.vx;
                arg0->extra.tmd->coords->coord.t[1]  += s->dir.vy;
                arg0->extra.tmd->coords->coord.t[2]  += s->dir.vz;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            dx        = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            s->dx     = dx;
            dy        = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            s->dy     = dy;
            dz        = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            s->dz     = dz;
            s->dist   = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage = Gp_ComputeDamage(s->id, s->dist, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->id, 0) != 0) {
                s->crit    = 1;
                s->effect  = 0;
                s->damage *= 4;
            } else {
                s->crit = 0;
            }
            mag = s->yaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x501) {
                state = work->field_0;
                if (state != 0x13) {
                    if (state != 0x14 && state != 0x11 && state != 0x1F && state != 0x20 && state != 0xF && state != 0x10 && state != 4) {
                        damage    = s->damage * 2;
                        s->damage = damage;
                        if (damage != 0) {
                            s->effect = 4;
                        }
                    }
                }
            }
            func_800E2C78(enemy, s->id, s->damage, 0);
            enemy->hp -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            work->field_BEA += s->damage;
            effect           = s->effect;
            if (effect != -1) {
                Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[2], (s32)(effect), NULL);
            }
            if (work->field_0 == 0x17) {
                SndEvt_EnqueueType7(0x51030008, 1);
            }
            if ((work->field_0 == 0xC || work->field_0 == 0xD || work->field_0 == 0xE) && config->hp > 0 && work->field_C28 == 1) {
                Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0008;
                deathPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(deathSound, deathPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            } else {
                hitSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0007;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(hitSound, hitPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            work->field_BE8 = Gp_GetIdParam2(s->id);
            switch (Gp_GetIdParam0(s->id) & 0xFFFF) {
                case 4:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
                    state = work->field_0;
                    if (state == 0x13 || state == 0x14 || state == 0xF || state == 0x10 || state == 4 || state == 0x11) {
                        if (work->field_89E == 0xB || work->field_89E == 0x17 || work->field_89E == 8 || work->field_89E == 0xA) {
                            work->field_89A = 1;
                            work->field_8A8 = 0xB;
                        } else {
                            work->field_89A = 1;
                            work->field_8A8 = 0x19;
                        }
                        work->field_8A6 = 2;
                    } else if (work->field_BEA >= 0x4C || s->crit == 1) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    } else {
                        work->field_8A8 = 0xD;
                        work->field_89A = 1;
                        work->field_8A6 = 2;
                    }
                    break;
                case 2:
                    Gp_SetObjFlag2(enemy, s->id, 0);
                    state = work->field_0;
                    if (state == 0x11 || state == 4) {
                        work->field_0 = 4;
                    } else if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                        work->field_0 = 0x20;
                    } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                        work->field_0 = 0x1F;
                    } else {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                    }
                    break;
                case 3:
                    state = work->field_0;
                    if (state == 0x18 || state == 0x16 || state == 0x17) {
                        work->field_0 = 6;
                    }
                    Gp_SetObjFlag4(enemy, s->id, 0);
                    break;
                case 1:
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    state                 = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
                case 8:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x501) {
                            if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                                work->field_0 = 0x20;
                            } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                                work->field_0 = 0x1F;
                            } else {
                                mag = s->yaw;
                                if (mag < 0) {
                                    mag = -mag;
                                }
                                work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                            }
                        }
                    }
                    break;
                case 9:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
            }
            timer = 5;
        } else if (work->field_BEC > 0) {
            timer = (u16)work->field_BEC - 1;
        } else {
            work->field_BEA = 0;
            goto block_bec;
        }
        work->field_BEC = timer;
    block_bec:
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            s->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (s->damage != 0) {
                enemy->hp -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
                if (work->field_0 == 7 || work->field_0 == 0x1E || work->field_0 == 0xB || work->field_0 == 0x1B) {
                    work->field_0 = 5;
                } else if (work->field_0 == 4) {
                    work->field_2 = -1;
                } else {
                    if (work->field_0 == 0x13 || work->field_0 == 0x14 || work->field_0 == 0xF || work->field_0 == 0x10 || work->field_0 == 0x11) {
                        if (work->field_89E == 0xB || work->field_89E == 0x17 || work->field_89E == 8 || work->field_89E == 0xA) {
                            work->field_89A = 1;
                            work->field_8A8 = 0xB;
                        } else {
                            work->field_89A = 1;
                            work->field_8A8 = 0x19;
                        }
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0xD;
                    }
                    work->field_8A6 = 2;
                }
            }
        }
        if (enemy->hp <= 0) {
            if (s->id != 0) {
                if ((Gp_GetIdParam0(s->id) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->id) & 0xFFFF) == 6) {
                    if ((u16)(work->field_89E - 2) < 2) {
                        work->field_0 = 0x21;
                    } else {
                        work->field_0 = 0x1D;
                    }
                } else {
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 4 && state != 0x11) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                }
            } else {
                if (work->field_C28 == 1) {
                    Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 2, 0);
                    work->field_C28 = 0;
                }
                state = work->field_0;
                if (state != 0x13 && state != 0x14 && state != 0x15 && state != 0x1D && state != 0 && state != 4 && state != 0x1F && state != 0x20 && state != 0x11) {
                    if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                        work->field_0 = 0x20;
                    } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                        work->field_0 = 0x1F;
                    } else {
                        work->field_0 = 0x14;
                    }
                }
            }
        }
        SCRATCH_STACK_RELEASE_BYTES(0x54);
    }
}

#include "../../shared/odd_stranger_stunned.inc.c"

#include "../../shared/odd_stranger_face_player.inc.c"

static void func_actor_401000_801352DC(GameLocationKey* session, GfxCoord* coord)
{
    ActorHeightClamp* row;
    s32               offset;
    s32               lo;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401000_80154FD0[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            lo     = row->lo;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->hi < offset) {
                coord->coord.t[1] = row->hi;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
        }
    }
}

/// Whether `D_actor_401000_80154FD0` has a row matching the session's
/// `GameLocationKey::stage` / `area` pair. The helper behind both
/// height-clamp probes of `func_actor_401000_80135374`; the second probe is
/// followed by the `func_actor_401000_801352DC` call itself, which walks the
/// same rows to clamp the root Y. Same helper as `Actor401300_HasHeightClamp`.
static __inline__ s32 Actor401000_HasHeightClamp(GameLocationKey* session)
{
    ActorHeightClamp* row;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401000_80154FD0[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            return 1;
        }
    }
    return 0;
}

/// Root-coordinate step, the 401000 twin of `func_actor_401300_80132C78`:
/// carve the 0x20-byte `ActorStepDelta` off the scratch stack, fill its delta
/// from the `rec` obstacle record, clamp the Y step to ±0x12C while a
/// height-clamp row matches, hand the XZ step to the GTE normalisation once it
/// passes 0x96, and step the root coordinate by each component. Reports
/// whether anything moved.
///
/// Both the repeated clamp and the `clamped` temporary are load-bearing for
/// register allocation, not style. The first clamp only runs while a clamp row
/// matches and its in-range arm skips the second copy entirely, so folding the
/// two (or letting the add re-read `s->step.vy`) swaps `$s0`/`$s1`: the block
/// pointer against the `step` local. The temporary keeps one reference to the
/// block pointer out of the RTL, which is what tips that fight the other way.
static s32 func_actor_401000_80135374(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3)
{
    ActorStepDelta* head;
    ActorStepDelta* s;
    s16             vy;
    s16             clamped;
    SVECTOR*        step;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }
    head                                 = SCRATCH_STACK_CURSOR(ActorStepDelta);
    SCRATCH_STACK_CURSOR(ActorStepDelta) = head - 1;
    s                                    = head - 1;
    s->moved                             = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.vx.word >> 16;
        s->step.vy = s->delta.vy.word >> 16;
        s->step.vz = s->delta.vz.word >> 16;
        if (Actor401000_HasHeightClamp(&gGameSession->location.loc)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) <= 0x12C) {
                goto addStep;
            }
            clamped    = (vy <= 0) ? -0x12C : 0x12C;
            s->step.vy = clamped;
        }
        vy = s->step.vy;
        if (((vy >= 0) ? vy : -vy) <= 0x12C) {
            goto addStep;
        }
        s->step.vy = (vy <= 0) ? -0x12C : 0x12C;
    addStep:
        coord->coord.t[1] += s->step.vy;
        s->len             = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->len             = SquareRoot0(s->len);
        step               = &s->step;
        if (s->len >= 0x96) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0x96);
            gte_ldsv(step);
            gte_gpf12();
            gte_stsv(step);
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        } else {
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        }
        if (s->delta.vx.word & 0xFFFF) {
            if (s->delta.vx.word > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (s->delta.vz.word & 0xFFFF) {
            if (s->delta.vz.word > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (Actor401000_HasHeightClamp(&gGameSession->location.loc)) {
        func_actor_401000_801352DC(&gGameSession->location.loc, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.vx.word != 0 || s->delta.vz.word != 0) {
        s->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorStepDelta);
    return s->moved;
}

s32 oddStrangerPushContacts(Task* arg0, WorldCollisionContact* recs, s16 count)
{
    ActorPushScratch* head;
    ActorPushScratch* s;
    ActorPushScratch* blk;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    head                                    = SCRATCH_STACK_CURSOR(ActorPushScratch);
    blk                                     = head - 1;
    SCRATCH_STACK_CURSOR(ActorPushScratch)  = blk;
    s                                       = blk;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
    s->pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
    s->pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
    s->pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key.value == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key.value & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            actorCalcPush(&s->pos, &recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0x6B) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x6B);
                gte_ldsv(&s->offset);
                gte_gpf12();
                gte_stsv(&s->offset);
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx >> 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz >> 2;
            } else {
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx >> 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz >> 2;
            }
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPushScratch);
    return s->hit;
}

static void func_actor_401000_80135AA4(Task* arg0)
{
    ActorChaseScratch* head;
    ActorChaseScratch* chase;
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    s32                angle;
    s32                diff;
    s32                kind;

    kind = (arg0->spawnArg1.value >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        work->field_89A        = 0;
        work->field_89E        = 3;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_8A2        = work->field_8A4;
        oddStrangerDrive(arg0);
        work->field_C24 = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        return;
    }
    work->field_6++;
    work->field_8++;
    head  = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    chase = (SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &head[-1].delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    chase->playerYaw = ratan2(-gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0],
                              gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    chase->yaw      = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
    chase->yaw      = actorNormalizeYaw(chase->yaw);
    coord           = arg0->extra.tmd->coords;
    chase->turn     = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = chase->turn;
    diff            = chase->yaw - chase->playerYaw;
    if (ABS(diff) < 0x44) {
        if (work->field_C14 + work->field_C26 / 2 < work->field_6) {
            angle = chase->turn;
            if (angle < 0) {
                angle = -angle;
            }
            if (angle < 0x80) {
                if (oddStrangerOutOfRange(&chase->delta, 0x708) && detectSightBlocked(arg0) != 1) {
                    work->field_0 = 0xA;
                }
            }
        }
    }
    if (detectSightBlocked(arg0) != 1) {
        work->field_6++;
        coord           = arg0->extra.tmd->coords;
        chase->turn     = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = chase->turn;
        if (chase->turn < 0x200) {
            if (!oddStrangerOutOfRange(&chase->delta, 0x44C) && work->field_C1B == 0) {
                work->field_0 = 0xB;
            }
        }
    } else {
        work->field_6   = 0;
        coord           = arg0->extra.tmd->coords;
        chase->turn     = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = chase->turn;
        if (work->field_C08 == 1) {
            chase->turn += 0x300;
        } else {
            chase->turn -= 0x300;
        }
        if (work->field_6 >= 0x169) {
            work->field_6   = 0;
            work->field_C08 = -work->field_C08;
        }
    }
    if (chase->turn > 0x40) {
        chase->turn = 0x40;
    }
    if (chase->turn < -0x40) {
        chase->turn = -0x40;
    }
    chase->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, chase->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89E == 3) {
        if (work->field_89A == 0) {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, ((work->field_8A4 + 2) * 0x78) / 0x12) != 0) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, ((work->field_8A4 + 2) * 0x78) / 0x12);
            }
        } else {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, ((work->field_8A4 + 2) * 0x78) / 0x12 >> 2) != 0) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, ((work->field_8A4 + 2) * 0x78) / 0x12 >> 2);
            }
        }
    } else if (work->flags_68.half & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_89E = 3;
        work->field_898 = 1;
    }
    if (func_actor_401000_80135374(arg0->extra.tmd->coords, work->field_A30, 0xC, 0x4B) != 1 && ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC) != 1) {
        oddStrangerPushContacts(arg0, work->field_8F0, 0xC);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg0->extra.tmd->coords);
    Gp_ClearRec18Occupied(work->field_8F0);
    if (work->field_C1B != 0) {
        work->field_C1B--;
    }
    if (work->field_8 >= 0x4C) {
        work->field_0 = 6;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#include "../../shared/odd_stranger_chase.inc.c"

#include "../../shared/odd_stranger_turn_around.inc.c"

#include "../../shared/odd_stranger_sidestep.inc.c"

static void func_actor_401000_801378DC(Task* arg0)
{
    SVECTOR          delta;
    OddStrangerWork* work;
    Enemy*           enemy;
    GameActor*       player;
    PlayerStatus*    config;
    GfxCoord*        coord;
    SVECTOR*         p;
    s16              angle;

    enemy  = arg0->spawnArg2.pointer;
    work   = arg0->work;
    player = (GameActor*)gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    config = &gPlayerStatus;
    if (work->field_4 != 0) {
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 1;
        work->field_8A2               = 0x10;
        work->field_89E               = 4;
        oddStrangerDrive(arg0);
        work->field_8AE                       = 0;
        work->field_8B0                       = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(arg0->extra.tmd->coords);
        work->field_C26 = 0;
        work->field_C28 = 0;
        work->field_C1B = 0xA;
        ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
        work->field_6 = 0;
        return;
    }
    if (++work->field_6 == 1) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
        work->field_BF8.vx                    = arg0->extra.tmd->coords->coord.t[0];
        work->field_BF8.vy                    = arg0->extra.tmd->coords->coord.t[1];
        work->field_BF8.vz                    = arg0->extra.tmd->coords->coord.t[2];
        work->field_8D0.radius                = 0x1AE;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, actorPositionYaw(arg0, &delta, config), 0);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        delta.vx                              = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        delta.vy                              = 0;
        delta.vz                              = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        work->field_8AE                       = 0;
        work->field_8B0                       = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_C26                       = 0;
        work->field_C28                       = 0;
        work->field_C1B                       = 0xA;
    }
    oddStrangerDrive(arg0);
    if ((work->field_5A & 0x3FF) == 0x10 && player->mode != GAME_ACTOR_MODE_SCRIPTED) {
        angle = actorMatrixPositionYaw(arg0, &delta, gPlayerStatus.coordMtx);
        if (abs(angle) < 0x10 && !oddStrangerOutOfRange(&delta, 0x44C)) {
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                gOddStrangerPlayerAnim.source.sets = &D_actor_401000_80154F00[2];
            } else {
                gOddStrangerPlayerAnim.source.sets = D_actor_401000_80154F00;
            }
            D_actor_401000_80155038.field_14 = 8;
            if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, &D_actor_401000_80155038, 0) == 0) {
                work->field_0                      = 0xC;
                work->field_C28                    = 1;
                gOddStrangerPlayerAnim.animationId = 1;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &gOddStrangerPlayerAnim, 0);
            }
        }
    }
    if (work->field_89E == 4 && (work->flags_68.half & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->field_0 = 7;
    }
    if ((u32)(work->field_5A & 0x3FF) >= 0x11) {
        p        = &delta;
        delta.vx = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        delta.vy = 0;
        delta.vz = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        if (!oddStrangerOutOfRange(p, 0x578)) {
            VectorNormalSS(p, p);
            gte_lddp(10);
            gte_ldsv(p);
            gte_gpf12();
            gte_stsv(p);
            coord                                 = arg0->extra.tmd->coords;
            coord->coord.t[0]                    += delta.vx;
            coord                                 = arg0->extra.tmd->coords;
            coord->coord.t[2]                    += delta.vz;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
            oddStrangerPushContacts(arg0, work->field_8F0, 0xC);
        }
    }
}

#include "../../shared/odd_stranger_grab.inc.c"

#include "../../shared/odd_stranger_grab_hold.inc.c"

#include "../../shared/odd_stranger_grab_release.inc.c"

/// State 8 body, the 401000 twin of `func_actor_401300_80138CF8`: on the
/// live-actor flag, reset the two animation nodes, the root coordinate and the
/// model's facing, then slide the root along both obstacle tables and take one
/// forward step while the 0x12C probe is still in range. The tail keys the
/// actor's next state (`field_0`) off `Enemy.hp` / `.reactionFlags` whenever
/// the work block's pending-request bit is up.
static void func_actor_401000_801388F4(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = 0x1AE;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 1;
        work->field_89E               = 0xA;
        work->field_89A               = 0;
        work->field_8A2               = 0x10;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
    if (work->field_89E == 0xA && (s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, -0x57) != 0) {
        actorMoveForward(arg0->extra.tmd->coords, -0x57);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->flags_68.half & ANIMATION_SLOT_REACHED_BOUNDARY) {
        if (work->field_89E == 0xA) {
            work->field_89E = 0xB;
            work->field_898 = 2;
            oddStrangerDrive(arg0);
        }
        if ((work->flags_68.half & ANIMATION_SLOT_REACHED_BOUNDARY) && work->field_89E == 0xB) {
            work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            if (enemy->hp > 0) {
                if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                    work->field_0 = 4;
                } else {
                    work->field_0 = 0x11;
                }
            } else {
                work->field_0 = 0x15;
            }
        }
    }
}

/// State 9 body, the 401000 twin of `func_actor_401300_80140300` and
/// `Actor01900_Fn09BE8`: on the live-actor flag, reset the two animation nodes,
/// the root coordinate and the model's facing, then hand the root to the
/// obstacle helper once per record table. The tail keys the actor's next state
/// (`field_0`) off `Enemy.hp` / `.reactionFlags` whenever the work block's
/// pending-request bit is up.
static void func_actor_401000_80138BB4(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = 0x1AE;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 1;
        work->field_89E               = 0xC;
        work->field_8A2               = 0x10;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->flags_68.half & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

#include "../../shared/odd_stranger_die.inc.c"

/// Per-frame tick of the 0xE/0xF animation pair, the same shape as
/// `oddStrangerDormant`. The live-actor arm allocates the model
/// buffers, restores the saved pose matrix `field_BA8` over the live
/// `field_BC8`, and restarts the 0xE / 0x898 animation slots; the body is then
/// gated on the `field_6` countdown and a 0-15 `gRandomLcgState` draw. The XZ
/// offset to `gPlayerStatus.coordMtx` is probed against `field_C16`, and an armed
/// `gSceneCombatState` bit 0x50000, each dropping the actor to state 6. The tail runs
/// `oddStrangerDrive` and swaps `field_89E` between 0xE and 0xF on
/// `flags_68` bits 1 and 2, re-running the tick after each swap.
/// Same body as `func_actor_401300_80139520`, with the pose matrix in place of
/// that one's `field_C48` / `field_C68` pair and a `field_C16` radius in place
/// of its literal 3000.
static void func_actor_401000_80138F50(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj        = arg0->extra.tmd;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius        = 0x1AE;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_6                 = 0;
        work->field_BC8               = work->field_BA8;
        work->field_89E               = 0xE;
        work->field_898               = 1;
        work->field_8A2               = work->field_8A4;
    }
    if (work->field_6 > 0x960) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6 = (u16)work->field_6 + 1;
    }
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy    = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!oddStrangerOutOfRange(d, work->field_C16)) {
        work->field_0 = 6;
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    oddStrangerDrive(arg0);
    if (work->field_89E == 0xE && (work->flags_68.half & 2)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            work->field_89E = 0xF;
            work->field_898 = 1;
            oddStrangerDrive(arg0);
        }
    }
    if (work->field_89E == 0xF && (work->flags_68.half & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->field_89E = 0xE;
        work->field_898 = 1;
        oddStrangerDrive(arg0);
    }
}

#include "../../shared/odd_stranger_dormant.inc.c"

#include "../../shared/odd_stranger_patrol.inc.c"

#include "../../shared/odd_stranger_advance.inc.c"

#include "../../shared/odd_stranger_back_off.inc.c"

#include "../../shared/odd_stranger_hold_aim.inc.c"

/// Turn the actor toward the player in two stages: while the `field_6`
/// countdown is under 0x32 the five part coordinates are reset to fixed
/// pitches, and afterwards each one unwinds by shifting its pitch down a step
/// every four frames; the turn itself is clamped to +-0x24 and drops the actor
/// to state 7 once it lines up. The `field_8AE` slot it drives is what
/// `oddStrangerHoldAim` writes whole; here it slides toward the target
/// by at most 0x28 a frame. Same body as `func_actor_401300_8013AE48`, with the
/// spawn arm arming the 0x13 clip, `field_8D0.field_1C` written before the other
/// state words, and `field_A10.flags |= 0x4000` in place of the sibling's
/// `&= 0xBFFF`.
static void func_actor_401000_8013A930(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    TmdObject*         obj;
    GfxCoord*          coord;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 2;
        work->field_8A2        = 8;
        work->field_89E        = 0x13;
        work->field_89A        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        oddStrangerDrive(arg0);
        work->field_6   = 0;
        work->field_8B0 = 0;
        return;
    }
    work->field_6++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    if (work->field_8AE < aim->turn) {
        if (aim->turn - work->field_8AE > 0x28) {
            work->field_8AE += 0x28;
        } else {
            work->field_8AE = aim->turn;
        }
    } else if (work->field_8AE - aim->turn > 0x28) {
        work->field_8AE -= 0x28;
    } else {
        work->field_8AE = aim->turn;
    }
    coord     = arg0->extra.tmd->coords;
    aim->turn = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    oddStrangerDrive(arg0);
    if (work->field_6 < 0x32) {
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[1].coord, 0x40, 0);
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[2].coord, 0x80, 0);
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[2]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[3].coord, 0x80, 0);
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[3]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[4].coord, 0x80, 0);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[4]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[5].coord, 0x100, 0);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[4]);
    } else {
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[1].coord, 0x40 >> ((work->field_6 - 0x31) / 4), 0);
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[2].coord, 0x80 >> ((work->field_6 - 0x30) / 4), 0);
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[2]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[3].coord, 0x80 >> ((work->field_6 - 0x2F) / 4), 0);
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[3]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[4].coord, 0x80 >> ((work->field_6 - 0x2E) / 4), 0);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[4]);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[5].coord, 0x100 >> ((work->field_6 - 0x31) / 4), 0);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[4]);
        aim->turn = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
        if (aim->turn > 0x24) {
            aim->turn = 0x24;
        } else if (aim->turn < -0x24) {
            aim->turn = -0x24;
        }
        if (ABS(aim->turn) < 0x24 || work->field_6 >= 0x4F) {
            work->field_0 = 7;
        }
        coord      = arg0->extra.tmd->coords;
        aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Clip-0x2D body: on the live-actor flag it resets the effect node and the
/// spawn offset, then walks the animation latch `field_6` from 0 to 0x3D and
/// spawns one effect per key frame, each tinted by `actorTintEffect`.
/// At 0x3D the actor returns to state 0. The 401000 twin of
/// `func_actor_401300_8013B6E8`: same five clips, three of them at the same
/// node offsets (`+1`, `+9`, `+12`, `+1`, `+3` off the root coordinate) and the
/// same 0x64/0/0 spawn vector, but it reads the offset from the work block
/// rather than a stack `SVECTOR` and has no `field_D20` guard on the tail.
static void func_actor_401000_8013B1E4(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    u16              next;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_8D0.radius        = 0x1AE;
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_8AE               = 0;
        work->field_6                 = 0;
        work->field_8C0.vx            = 0x64;
        work->field_8C0.vz            = 0;
        work->field_8C0.vy            = 0;
        Gp_SpawnEff(0x60030, arg0->extra.tmd->coords + 1, 0x10300, &work->field_8C0);
        Gp_ReleaseStateF0Add(arg0, 0xA);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelA;
        work->field_8C0.vz       = 0x64;
        work->field_8C0.vy       = 0;
        work->field_8C0.vx       = 0;
        actorTintEffect(Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 9, 0x200, &work->field_8C0), enemy);
    }
    if ((s16)work->field_6 == 5) {
        D_80114B34[5].data.model = &D_actor_401000_80144830;
        work->field_8C0.vy       = 0;
        work->field_8C0.vx       = 0;
        actorTintEffect(Gp_SpawnEff(0xA0000 | 5, arg0->extra.tmd->coords + 12, 0x200, &work->field_8C0), enemy);
    }
    if ((s16)work->field_6 == 7) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelB;
        actorTintEffect(Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 1, 0x200, NULL), enemy);
    }
    if ((s16)work->field_6 == 8) {
        D_80114B34[5].data.model = &gOddStrangerBurstModelC;
        actorTintEffect(Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 3, 0x200, NULL), enemy);
    }
    if ((s16)work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

#include "../../shared/odd_stranger_walking_death.inc.c"

#include "../../shared/odd_stranger_stalk.inc.c"

/// State 9 clip-0xB body, the 401000 twin of `func_actor_401000_80138BB4` and
/// `func_actor_401000_8013CEF0`: on the live-actor flag it resets the two
/// animation nodes and the root coordinate like the state 9 body, but keys the
/// node pair off clip 0xB / slot 2 and tests the request bit `0x100` rather
/// than bit 0. Same tail: `Enemy.hp` / `.reactionFlags` pick the next
/// `field_0` whenever the request bit is up.
static void func_actor_401000_8013CD9C(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = 0x1AE;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 2;
        work->field_89E               = 0xB;
        work->field_8A2               = 0x10;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->flags_68.half & ANIMATION_SLOT_SETTLED) {
        work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// State 10 body, the 401000 twin of `func_actor_401000_80138BB4` and
/// `func_actor_401300_8014046C`: on the live-actor flag it resets the two
/// animation nodes and the root coordinate like the state 9 body, but keys the
/// node pair off clip 0x19 / slot 2 and tests the request bit `0x100` rather
/// than bit 0. Same tail: `Enemy.hp` / `.reactionFlags` pick the next
/// `field_0` whenever the request bit is up.
static void func_actor_401000_8013CEF0(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = 0x1AE;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 2;
        work->field_89E               = 0x19;
        work->field_8A2               = 0x10;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->flags_68.half & ANIMATION_SLOT_SETTLED) {
        work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// The actor's state handlers, indexed by `OddStrangerWork::field_0`. Copied to
/// the frame by `oddStrangerTick` before the dispatch, so the
/// handler may overwrite the live table entry.
static const OddStrangerStateTable gOddStrangerStates = { {
    func_actor_401000_8013DB10,
    oddStrangerScriptPose2,
    oddStrangerScriptPose3,
    oddStrangerScriptPoseB,
    oddStrangerStunned,
    oddStrangerScriptPoseD,
    oddStrangerFacePlayer,
    func_actor_401000_80135AA4,
    oddStrangerChase,
    oddStrangerTurnAround,
    oddStrangerSidestep,
    func_actor_401000_801378DC,
    oddStrangerGrab,
    oddStrangerGrabHold,
    oddStrangerGrabRelease,
    oddStrangerScriptPose8,
    func_actor_401000_8013DEC8,
    oddStrangerIdle,
    NULL,
    func_actor_401000_801388F4,
    func_actor_401000_80138BB4,
    oddStrangerDie,
    func_actor_401000_80138F50,
    oddStrangerDormant,
    oddStrangerPatrol,
    oddStrangerBackOff,
    oddStrangerAdvance,
    oddStrangerHoldAim,
    func_actor_401000_8013A930,
    func_actor_401000_8013B1E4,
    oddStrangerStalk,
    func_actor_401000_8013CD9C,
    func_actor_401000_8013CEF0,
    oddStrangerWalkingDeath,
} };

#include "../../shared/odd_stranger_tick.inc.c"

void func_actor_401000_8013D68C(void)
{
}

/// The task's handlers, indexed by `Task::state` in
/// `func_actor_401000_8013E038`: the first allocates and sets up the work
/// block, the second runs the per-state logic every frame, and the third tears
/// the enemy down.
static const GpEnemyTaskFuncTable3 D_actor_401000_8013207C = { {
    func_actor_401000_80133274,
    oddStrangerTick,
    Gp_DestroyEnemy,
} };

#include "../../shared/odd_stranger_play_message.inc.c"

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw.inc.c"

#include "../../shared/actor_messages_release_hold.inc.c"

#include "../../shared/odd_stranger_apply_command.inc.c"

#include "../../shared/odd_stranger_exit.inc.c"

static void func_actor_401000_8013DB10(Task* arg0)
{
    TmdObject*       obj;
    OddStrangerWork* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->field_B50.flags                                     = (u16)(work->field_B50.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A10.flags                                     = (u16)(work->field_A10.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

#include "../../shared/odd_stranger_script_pose_2.inc.c"

#include "../../shared/odd_stranger_script_pose_3.inc.c"

#include "../../shared/odd_stranger_script_pose_b.inc.c"

#include "../../shared/odd_stranger_script_pose_d.inc.c"

#include "../../shared/odd_stranger_script_pose_8.inc.c"

static void func_actor_401000_8013DEC8(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = 0x1AE;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 2;
        work->field_89E               = 0x16;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        work->field_8A2               = work->field_8A4;
    }
    oddStrangerDrive(arg0);
    if (work->flags_68.half & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 7;
    }
}

#include "../../shared/odd_stranger_idle.inc.c"

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401000_8013E038(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_401000_8013207C;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
