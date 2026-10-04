#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/dryfield_night_junk_yard.h"
#define STALKER_ZEBRA_IVORY_KIND STALKER_ZEBRA
#include "../../shared/stalker_zebra_ivory.h"

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

/// Per-actor state block for the `actor_400600` overlay.
///
/// `func_actor_400600_80133434` allocates it with `memCalloc(0x770)` and
/// stores it in the `Task::work` slot (0x1C): an enemy actor reuses that
/// pointer field for its own work block.
/// Reach it with `(Actor400600Work*)task->work`.
typedef struct Actor400600Work {
    /* 0x000 */ MATRIX                matrix_0;  // copy of the root coordinate's local matrix
    /* 0x020 */ MATRIX                matrix_20; // color matrix for the child models
    /* 0x040 */ MATRIX                matrix_40; // light matrix for the child models
    /* 0x060 */ byte                  pad_60[0x10];
    /* 0x070 */ VECTOR                field_70;  // copy of the root coordinate's translation
    /* 0x080 */ u16                   pitch;     // pitch, see stalkerZebraIvoryApplyRotation
    /* 0x082 */ u16                   yaw;       // yaw, see stalkerZebraIvoryApplyRotation
    /* 0x084 */ u16                   roll;      // roll, see stalkerZebraIvoryApplyRotation
    /* 0x086 */ byte                  pad_86[0x2];
    /* 0x088 */ SVECTOR3              anchorPos;
    /* 0x08E */ byte                  pad_8E[0x2];
    /* 0x090 */ u16                   field_90; // spawn position X (low half)
    /* 0x092 */ u16                   field_92; // seeds field_73E on state entry
    /* 0x094 */ u16                   field_94; // spawn position Z (low half)
    /* 0x096 */ byte                  pad_96[0x2];
    /* 0x098 */ u16                   field_98; // low half of the root coordinate's world X
    /* 0x09A */ u16                   field_9A; // copy of field_92
    /* 0x09C */ u16                   field_9C; // low half of the root coordinate's world Z
    /* 0x09E */ byte                  pad_9E[0xA];
    /* 0x0A8 */ SVECTOR               field_A8; // copied to the stack for stalkerZebraIvoryTurnToward
    /* 0x0B0 */ AnimationContext      anim;     // slots 1..0x11 reset by stalkerZebraIvoryRestartClip
    /* 0x0C4 */ AnimationSlot         slots[0x12];
    /* 0x394 */ byte                  pad_394[0x120];
    /* 0x4B4 */ WorldCollisionBody    obj_4B4;                // collision node; flags bit 0x8000 cleared
    /* 0x4D4 */ WorldCollisionContact rec_4D4[8];             // occupancy cleared by func_actor_400600_80138D78
    /* 0x594 */ WorldCollisionBody    obj_594;                // collision node; flags bit 0x8000 cleared
    /* 0x5B4 */ WorldCollisionContact rec_5B4[1];             // obj_594's table (flags kind 1)
    /* 0x5CC */ WorldCollisionBody    obj_5CC;                // collision node; flags bit 0x8000 cleared
    /* 0x5EC */ WorldCollisionContact rec_5EC[1];             // obj_5CC's table (flags kind 1)
    /* 0x604 */ WorldCollisionBody    capsuleBody;            // collision node; flags bit 0x4000 cleared
    /* 0x624 */ WorldCollisionCapsule capsule;                // capsuleBody's payload (flags kind 3)
    /* 0x63C */ WorldCollisionContact capsuleContacts[8];     // occupancy cleared by func_actor_400600_80138D78
    /* 0x6FC */ EffectSpawnArg        eff_6FC;                // fourth model part's coordinate
    /* 0x704 */ Task*                 field_704;              // child task, killed on death
    /* 0x708 */ Task*                 field_708;              // child task, killed on death
    /* 0x70C */ byte                  pad_70C[0x4];
    /* 0x710 */ s16                   ceilingCooldown;        // frames left before it may next jump to the ceiling or start a move from it; set to 210..241 as each jump up and drop ends
    /* 0x712 */ u16                   previousAnimationFlags; // slot 1's ANIMATION_SLOT_* results as the last running update's tick left them
    /* 0x714 */ s16                   field_714;              // reset to 0x1000 on death
    /* 0x716 */ u16                   field_716;              // frame counter, bumped by func_actor_400600_80138D78
    /* 0x718 */ u16                   field_718;              // per-state frame counter
    /* 0x71A */ s16                   field_71A;
    /* 0x71C */ u16                   state;                  // state index
    /* 0x71E */ u16                   subState;               // sub-state index
    /* 0x720 */ s16                   animBlend;
    /* 0x722 */ s16                   field_722;              // velocity step (can go negative)
    /* 0x724 */ s16                   field_724;              // accumulated step
    /* 0x726 */ s16                   animStep;
    /* 0x728 */ s16                   playerDistance;
    /* 0x72A */ u16                   field_72A;
    /* 0x72C */ u16                   field_72C;
    /* 0x72E */ s16                   pendingArmed;
    /* 0x730 */ s16                   pendingAction;
    /* 0x732 */ s16                   timer;     // countdown seeded by stalkerZebraIvorySeedTimer
    /* 0x734 */ s16                   field_734; // model slot id handed to func_actor_400600_80139FE0
    /* 0x736 */ byte                  pad_736[0x4];
    /* 0x73A */ s16                   field_73A; // fade level, lerped toward 0xFF
    /* 0x73C */ s16                   field_73C;
    /* 0x73E */ u16                   field_73E;
    /* 0x740 */ s16                   field_740;
    /* 0x742 */ s16                   animRequest; // animation request kind
    /* 0x744 */ s16                   animPlaying; // animation id now playing
    /* 0x746 */ s16                   animClip;    // animation id
    /* 0x748 */ s16                   animFrame;   // sound step index (func_actor_400600_801361AC)
    /* 0x74A */ s16                   field_74A;   // hit cooldown, seeded from Gp_GetIdParam2
    /* 0x74C */ s16                   field_74C;
    /* 0x74E */ s16                   field_74E;
    /* 0x750 */ u16                   countdown; // countdown to state 0xB
    /* 0x752 */ s16                   field_752;
    /* 0x754 */ s16                   field_754;
    /* 0x756 */ u16                   field_756; // countdown to the next state-2 transition
    /* 0x758 */ s16                   field_758;
    /* 0x75A */ s16                   field_75A;
    /* 0x75C */ s16                   holdLoops;   // hold-clip repeats completed; the hold ends at 3
    /* 0x75E */ u8                    cloaked;     // cloak target (0 visible, 1 cloaked)
    /* 0x75F */ u8                    cloakFading; // set while the fade toward `cloaked` runs
    /* 0x760 */ s8                    field_760;
    /* 0x761 */ byte                  pad_761;
    /* 0x762 */ u8                    roomCommand;
    /* 0x763 */ u8                    field_763;
    /* 0x764 */ u8                    holdKilledPlayer;
    /* 0x765 */ s8                    rightArmOut;
    /* 0x766 */ s8                    leftArmOut;
    /* 0x767 */ s8                    holding;
    /* 0x768 */ u8                    onCeiling;
    /* 0x769 */ u8                    onBack;       // sub-variant flag, gates state indices
    /* 0x76A */ u8                    distanceMode; // distance mode: 0 none, 1 XZ, 2 XY
    /* 0x76B */ u8                    field_76B;
    /* 0x76C */ u8                    field_76C;    // nonzero: landing spawns the dust ring
    /* 0x76D */ u8                    field_76D;
    /* 0x76E */ u8                    field_76E;    // set when spawned in map 0x0314
    /* 0x76F */ byte                  pad_76F;
} Actor400600Work;
STATIC_ASSERT_SIZEOF(Actor400600Work, 0x770);

/// The Stalker library's name for this package's work block (see stalker_zebra_ivory.h).
typedef Actor400600Work StalkerZebraIvoryWork;

/// 0x3C-byte scratchpad frame `func_actor_400600_801383E4` carves off
/// the scratch stack: the four widened corners of the quad and
/// `RotTransPers4`'s outputs. Same tail as `ActorsShared80163354Scratch`.
typedef struct Actor400600QuadScratch {
    /* 0x00 */ SVECTOR corner0;
    /* 0x08 */ SVECTOR corner1;
    /* 0x10 */ SVECTOR corner2;
    /* 0x18 */ SVECTOR corner3;
    /* 0x20 */ long    screen0;
    /* 0x24 */ long    screen1;
    /* 0x28 */ long    screen2;
    /* 0x2C */ long    screen3;
    /* 0x30 */ long    perspective;
    /* 0x34 */ long    flags;
    /* 0x38 */ s32     depth;
} Actor400600QuadScratch;
STATIC_ASSERT_SIZEOF(Actor400600QuadScratch, 0x3C);

extern ActorZone D_actor_400600_80151B40[];

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`; `gSceneCombatState.zebraStalkerGroupPhase` and `gSceneCombatState.zebraStalkerDeathAlert` are
 * bytes of the run of gameplay flags at 0x80115408..0x8011541B.
 *
 * Storing to a bare `extern` global next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `Actor400600Work` loads
 * that follow. Two remedies work and which one is needed was measured, not
 * chosen: the byte store to `gSceneCombatState.zebraStalkerDeathAlert` matches with `SOFT_BARRIER()` after
 * it, so that one is declared as the scalar it is; the pointer store to
 * `D_800678F0` checksums wrong with the barrier and matches only as an
 * aggregate, so its one-element array stays and is doing real work.
 * `gSceneCombatState.zebraStalkerGroupPhase` is the aggregate case too: one of its stores sits between
 * struct stores on both sides, and the barrier trades the sink for a hoist
 * above the preceding flag updates. */
extern void* D_800678F0[1];

extern DamageAttack D_actor_400600_80144EA8[2];
extern EnemyParams  D_actor_400600_80144EB0;      // the enemy's parameter record

extern AnimationSet* D_actor_400600_80151A54[35]; // animation bank handed to animationInitContext
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_400600_80151AE0[3];

extern AnimationSet* D_actor_400600_80151A48[3];

extern TaskDesc D_actor_400600_80151AF8[];

/// The records closing four of the overlay's model streams, selected through
/// `D_800678F0`.
static TmdSource _gActor400600ZebraStalkerBurstHead;
static TmdSource _gActor400600StalkerEffect;
static TmdSource _gActor400600StalkerBurstHandLeft;
static TmdSource _gActor400600StalkerBurstFootRight;

extern u8 gStalkerZebraIvoryResumeClips[];

/* Part indices into the model's coordinate array, terminated by -1. */
extern s16 D_actor_400600_80151B88[];

static void func_actor_400600_8013203C(Task* arg0);
static void func_actor_400600_80132294(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade);
static void func_actor_400600_80132704(Task* arg0, s16 arg1, u8 arg2);
static void func_actor_400600_801328A8(Task* arg0);
static void func_actor_400600_801329EC(Task* arg0);
static void func_actor_400600_80132B3C(Task* arg0);
static void func_actor_400600_80132C70(Task* arg0);
static void func_actor_400600_80132E10(Task* arg0);
static void func_actor_400600_80132F3C(Task* arg0);
static void func_actor_400600_80133118(Task* arg0);
static void func_actor_400600_801332F4(Task* arg0);
static void func_actor_400600_80133434(Task* arg0);
static void func_actor_400600_801337A8(Task* arg0);
static void func_actor_400600_80133B88(Task* arg0);
static void func_actor_400600_80133CB0(Task* arg0);
static void func_actor_400600_80133E38(Task* arg0);
static void func_actor_400600_80133FC0(Task* arg0);
static void func_actor_400600_80134218(Task* arg0);
static void func_actor_400600_80134570(Task* arg0);
static void func_actor_400600_8013479C(Task* arg0);
static void func_actor_400600_80134970(Task* arg0);
static void func_actor_400600_80134E28(Task* arg0);
static void func_actor_400600_801350F4(Task* arg0);
static void func_actor_400600_80135450(Task* arg0);
static void func_actor_400600_80135578(Task* arg0);
static void func_actor_400600_801356E0(Task* arg0);
static void func_actor_400600_80135998(Task* arg0, s16 arg1);
static void func_actor_400600_801361AC(Task* arg0);
static void func_actor_400600_80136558(Task* arg0);
static void func_actor_400600_80136670(Task* arg0);
static void func_actor_400600_80136968(Task* arg0);
static s32  func_actor_400600_80136FA8(Task* arg0);
static void func_actor_400600_80137240(Task* arg0);
static void func_actor_400600_80137498(Task* arg0, s16 arg1);
static void func_actor_400600_80137840(Task* arg0);
static s32  func_actor_400600_80137AF0(Task* arg0);
static s32  func_actor_400600_80137C34(Task* arg0);
static void func_actor_400600_80137EF0(Task* arg0);
static void func_actor_400600_80138224(Task* arg0, s16 arg1, u8 arg2);
static void func_actor_400600_801383E4(SVECTOR* arg0, SVECTOR* arg1, s16 width, u8 shade);
static void func_actor_400600_801387DC(Task* arg0, s32 arg1);
static s32  func_actor_400600_8013886C(Task* arg0);
static s32  func_actor_400600_8013892C(Task* arg0);
static void func_actor_400600_8013896C(Task* arg0, s16 arg1);
static void func_actor_400600_80138A24(Task* arg0, s16 arg1);
static void func_actor_400600_80138AB8(Task* arg0);
static void func_actor_400600_80138B5C(Task* arg0, s32 arg1);
static void func_actor_400600_80138C34(Task* arg0);
static void func_actor_400600_80138D78(Task* arg0);
static void func_actor_400600_80138EA0(Task* arg0);
static void func_actor_400600_80138FD4(Task* arg0);
static void func_actor_400600_801390FC(Task* arg0);
static void func_actor_400600_80139110(Task* arg0);
static void func_actor_400600_80139218(Task* arg0);
static void func_actor_400600_80139280(Task* arg0);
static void func_actor_400600_801392E8(Task* arg0);
static void func_actor_400600_8013935C(Task* arg0);
static void func_actor_400600_801393D0(Task* arg0);
static void func_actor_400600_80139444(Task* arg0);
static void func_actor_400600_801394E0(Task* arg0);
static void func_actor_400600_80139560(Task* arg0);
static void func_actor_400600_80139608(Task* arg0);
static void func_actor_400600_80139670(Task* arg0);
static void func_actor_400600_801396E4(Task* arg0);
static void func_actor_400600_80139764(Task* arg0);
static void func_actor_400600_80139878(Task* arg0);
static void func_actor_400600_801398E0(Task* arg0);
static void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_actor_400600_80139F4C(Task* arg0, s16 arg1, SVECTOR3* arg2);
static void func_actor_400600_80139FE0(Task* arg0, s16 arg1, SVECTOR3* arg2);
void        func_actor_400600_8013A0F0(Task* arg0);
static void func_actor_400600_8013A170(Task* arg0);
static void func_actor_400600_8013A26C(Task* arg0);
void        func_actor_400600_8013A3A8(Task* arg0, s32 msgId, s32 arg2, s32 arg3);
void        func_actor_400600_8013A3B8(Task* task);
void        func_actor_400600_8013A3C0(Task* task);
static void func_actor_400600_8013A3C8(Task* arg0);
static void func_actor_400600_8013A570(Task* arg0);
static void func_actor_400600_8013A638(Task* arg0);
static void func_actor_400600_8013A6C4(Task* arg0);
static void func_actor_400600_8013A808(Task* arg0);
static void func_actor_400600_8013A820(Task* arg0);
static void func_actor_400600_8013A864(Task* arg0);
static void func_actor_400600_8013A908(Task* arg0);
static void func_actor_400600_8013A990(Task* arg0);
static void func_actor_400600_8013AA5C(Task* arg0);
static void func_actor_400600_8013AAD8(Task* arg0);
static void func_actor_400600_8013AB44(Task* arg0);
static void func_actor_400600_8013AB98(Task* arg0);
static void func_actor_400600_8013AC14(Task* arg0);
static void func_actor_400600_8013AD3C(Task* arg0);
static void func_actor_400600_8013ADA4(Task* arg0);
static void func_actor_400600_8013AE88(Task* arg0);
static void func_actor_400600_8013AF04(Task* arg0);
static void func_actor_400600_8013B018(Task* arg0);
static void func_actor_400600_8013B0FC(Task* arg0);
static void func_actor_400600_8013B150(Task* arg0);
static void func_actor_400600_8013B1DC(Task* arg0);
static void func_actor_400600_8013B2A8(Task* arg0);
static void func_actor_400600_8013B394(Task* arg0);
static void func_actor_400600_8013B410(Task* arg0);
static void func_actor_400600_8013B520(Task* arg0);
static void func_actor_400600_8013B640(void);
static void func_actor_400600_8013B6F4(Task* arg0);
static void func_actor_400600_8013B740(Task* arg0);
static void func_actor_400600_8013B830(Task* arg0);
static void func_actor_400600_8013B8AC(Task* arg0);
static void func_actor_400600_8013B984(Task* arg0);
static void func_actor_400600_8013BA00(Task* arg0);
static void func_actor_400600_8013BA6C(Task* arg0);
static void func_actor_400600_8013BAEC(Task* arg0);
static void func_actor_400600_8013BB88(Task* arg0);
static void func_actor_400600_8013BBF4(Task* arg0);
static void func_actor_400600_8013BC68(Task* arg0);
static void func_actor_400600_8013BCD8(Task* arg0);
static void func_actor_400600_8013BD54(Task* arg0);
static void func_actor_400600_8013BDF0(Task* arg0);
static void func_actor_400600_8013BE58(Task* arg0);
static void func_actor_400600_8013BF48(Task* arg0);
static void func_actor_400600_8013BFD4(Task* arg0);
static void func_actor_400600_8013C038(Task* arg0);
static void func_actor_400600_8013C074(Task* arg0);
static void func_actor_400600_8013C104(Task* arg0);
static void func_actor_400600_8013C124(Task* arg0);
static void func_actor_400600_8013C1C0(Task* arg0);
static void func_actor_400600_8013C238(Task* arg0);
static void func_actor_400600_8013C394(Task* arg0);
static void func_actor_400600_8013C410(Task* arg0);
static void func_actor_400600_8013C4AC(Task* arg0);
static void func_actor_400600_8013C518(Task* arg0);
static void func_actor_400600_8013C598(Task* arg0);
static void func_actor_400600_8013C5F8(Task* arg0);
static void func_actor_400600_8013C6B0(SVECTOR* pos, WorldCollisionContact* rec, SVECTOR* out);
static s32  func_actor_400600_8013C7E8(s16 arg0, s16 arg1);
static void func_actor_400600_8013C874(Task* arg0);
static void func_actor_400600_8013C940(Task* arg0);
static void func_actor_400600_8013C9DC(Task* arg0);
static void func_actor_400600_8013CB40(Task* arg0, u8 arg1);
static void func_actor_400600_8013CB70(Task* arg0, s32 arg1);
static void func_actor_400600_8013CC04(Task* arg0, s16 arg1);

static TmdSource _gActor400600ZebraStalkerBody;
void             func_actor_400600_8013A0F0(Task*);

static TmdSource _gActor400600ZebraStalkerBurstArmLeft;
static TmdSource _gActor400600ZebraStalkerBurstArmLeft13064;
void             func_actor_400600_8013A3A8(Task*, s32, s32, s32);
void             func_actor_400600_8013A3B8(Task*);
void             func_actor_400600_8013A3C0(Task*);

static TmdBone _gActor400600ZebraStalkerBodySkeleton[18] = {
#include "assets/zebra_stalker_body_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBodyPartVerts[18] = {
#include "assets/zebra_stalker_body_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBodyVerts[258] = {
#include "assets/zebra_stalker_body_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBodyNormals[290] = {
#include "assets/zebra_stalker_body_normals.inc"
};

static u32 _gActor400600ZebraStalkerBodyStream[3683] = {
#include "assets/zebra_stalker_body_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBody = {
    0,
    18184,
    6868,
    18,
    _gActor400600ZebraStalkerBodyPartVerts,
    _gActor400600ZebraStalkerBodyVerts,
    _gActor400600ZebraStalkerBodyNormals,
    _gActor400600ZebraStalkerBodySkeleton,
    _gActor400600ZebraStalkerBodyStream,
};

static TmdBone _gActor400600ZebraStalkerBurstHeadSkeleton[1] = {
#include "assets/zebra_stalker_burst_head_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBurstHeadPartVerts[1] = {
#include "assets/zebra_stalker_burst_head_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstHeadVerts[37] = {
#include "assets/zebra_stalker_burst_head_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstHeadNormals[45] = {
#include "assets/zebra_stalker_burst_head_normals.inc"
};

static u32 _gActor400600ZebraStalkerBurstHeadStream[359] = {
#include "assets/zebra_stalker_burst_head_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBurstHead = {
    0,
    2408,
    0,
    1,
    _gActor400600ZebraStalkerBurstHeadPartVerts,
    _gActor400600ZebraStalkerBurstHeadVerts,
    _gActor400600ZebraStalkerBurstHeadNormals,
    _gActor400600ZebraStalkerBurstHeadSkeleton,
    _gActor400600ZebraStalkerBurstHeadStream,
};

static TmdBone _gActor400600StalkerBurstTorsoSkeleton[1] = {
#include "assets/stalker_burst_torso_skeleton.inc"
};

static u32 _gActor400600StalkerBurstTorsoPartVerts[1] = {
#include "assets/stalker_burst_torso_partVerts.inc"
};

static SVECTOR _gActor400600StalkerBurstTorsoVerts[55] = {
#include "assets/stalker_burst_torso_verts.inc"
};

static SVECTOR _gActor400600StalkerBurstTorsoNormals[62] = {
#include "assets/stalker_burst_torso_normals.inc"
};

static u32 _gActor400600StalkerBurstTorsoStream[600] = {
#include "assets/stalker_burst_torso_stream.inc"
};

static TmdSource _gActor400600StalkerBurstTorso = {
    0,
    3988,
    0,
    1,
    _gActor400600StalkerBurstTorsoPartVerts,
    _gActor400600StalkerBurstTorsoVerts,
    _gActor400600StalkerBurstTorsoNormals,
    _gActor400600StalkerBurstTorsoSkeleton,
    _gActor400600StalkerBurstTorsoStream,
};

static TmdBone _gActor400600StalkerEffectSkeleton[1] = {
#include "assets/stalker_effect_skeleton.inc"
};

static u32 _gActor400600StalkerEffectPartVerts[1] = {
#include "assets/stalker_effect_partVerts.inc"
};

static SVECTOR _gActor400600StalkerEffectVerts[27] = {
#include "assets/stalker_effect_verts.inc"
};

static SVECTOR _gActor400600StalkerEffectNormals[34] = {
#include "assets/stalker_effect_normals.inc"
};

static u32 _gActor400600StalkerEffectStream[284] = {
#include "assets/stalker_effect_stream.inc"
};

static TmdSource _gActor400600StalkerEffect = {
    0,
    1860,
    0,
    1,
    _gActor400600StalkerEffectPartVerts,
    _gActor400600StalkerEffectVerts,
    _gActor400600StalkerEffectNormals,
    _gActor400600StalkerEffectSkeleton,
    _gActor400600StalkerEffectStream,
};

static TmdBone _gActor400600StalkerBurstHandLeftSkeleton[1] = {
#include "assets/stalker_burst_hand_left_skeleton.inc"
};

static u32 _gActor400600StalkerBurstHandLeftPartVerts[1] = {
#include "assets/stalker_burst_hand_left_partVerts.inc"
};

static SVECTOR _gActor400600StalkerBurstHandLeftVerts[23] = {
#include "assets/stalker_burst_hand_left_verts.inc"
};

static SVECTOR _gActor400600StalkerBurstHandLeftNormals[26] = {
#include "assets/stalker_burst_hand_left_normals.inc"
};

static u32 _gActor400600StalkerBurstHandLeftStream[211] = {
#include "assets/stalker_burst_hand_left_stream.inc"
};

static TmdSource _gActor400600StalkerBurstHandLeft = {
    0,
    1400,
    0,
    1,
    _gActor400600StalkerBurstHandLeftPartVerts,
    _gActor400600StalkerBurstHandLeftVerts,
    _gActor400600StalkerBurstHandLeftNormals,
    _gActor400600StalkerBurstHandLeftSkeleton,
    _gActor400600StalkerBurstHandLeftStream,
};

static TmdBone _gActor400600ZebraStalkerBurstHandRightSkeleton[1] = {
#include "assets/zebra_stalker_burst_hand_right_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBurstHandRightPartVerts[1] = {
#include "assets/zebra_stalker_burst_hand_right_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstHandRightVerts[23] = {
#include "assets/zebra_stalker_burst_hand_right_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstHandRightNormals[29] = {
#include "assets/zebra_stalker_burst_hand_right_normals.inc"
};

static u32 _gActor400600ZebraStalkerBurstHandRightStream[211] = {
#include "assets/zebra_stalker_burst_hand_right_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBurstHandRight = {
    0,
    1400,
    0,
    1,
    _gActor400600ZebraStalkerBurstHandRightPartVerts,
    _gActor400600ZebraStalkerBurstHandRightVerts,
    _gActor400600ZebraStalkerBurstHandRightNormals,
    _gActor400600ZebraStalkerBurstHandRightSkeleton,
    _gActor400600ZebraStalkerBurstHandRightStream,
};

static TmdBone _gActor400600ZebraStalkerBurstFootLeftSkeleton[1] = {
#include "assets/zebra_stalker_burst_foot_left_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBurstFootLeftPartVerts[1] = {
#include "assets/zebra_stalker_burst_foot_left_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstFootLeftVerts[19] = {
#include "assets/zebra_stalker_burst_foot_left_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstFootLeftNormals[25] = {
#include "assets/zebra_stalker_burst_foot_left_normals.inc"
};

static u32 _gActor400600ZebraStalkerBurstFootLeftStream[188] = {
#include "assets/zebra_stalker_burst_foot_left_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBurstFootLeft = {
    0,
    1220,
    0,
    1,
    _gActor400600ZebraStalkerBurstFootLeftPartVerts,
    _gActor400600ZebraStalkerBurstFootLeftVerts,
    _gActor400600ZebraStalkerBurstFootLeftNormals,
    _gActor400600ZebraStalkerBurstFootLeftSkeleton,
    _gActor400600ZebraStalkerBurstFootLeftStream,
};

static TmdBone _gActor400600StalkerBurstFootRightSkeleton[1] = {
#include "assets/stalker_burst_foot_right_skeleton.inc"
};

static u32 _gActor400600StalkerBurstFootRightPartVerts[1] = {
#include "assets/stalker_burst_foot_right_partVerts.inc"
};

static SVECTOR _gActor400600StalkerBurstFootRightVerts[19] = {
#include "assets/stalker_burst_foot_right_verts.inc"
};

static SVECTOR _gActor400600StalkerBurstFootRightNormals[25] = {
#include "assets/stalker_burst_foot_right_normals.inc"
};

static u32 _gActor400600StalkerBurstFootRightStream[188] = {
#include "assets/stalker_burst_foot_right_stream.inc"
};

static TmdSource _gActor400600StalkerBurstFootRight = {
    0,
    1220,
    0,
    1,
    _gActor400600StalkerBurstFootRightPartVerts,
    _gActor400600StalkerBurstFootRightVerts,
    _gActor400600StalkerBurstFootRightNormals,
    _gActor400600StalkerBurstFootRightSkeleton,
    _gActor400600StalkerBurstFootRightStream,
};

static TmdBone _gActor400600ZebraStalkerBurstArmLeftSkeleton[1] = {
#include "assets/zebra_stalker_burst_arm_left_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBurstArmLeftPartVerts[1] = {
#include "assets/zebra_stalker_burst_arm_left_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstArmLeftVerts[10] = {
#include "assets/zebra_stalker_burst_arm_left_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstArmLeftNormals[17] = {
#include "assets/zebra_stalker_burst_arm_left_normals.inc"
};

static u32 _gActor400600ZebraStalkerBurstArmLeftStream[85] = {
#include "assets/zebra_stalker_burst_arm_left_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBurstArmLeft = {
    0,
    528,
    0,
    1,
    _gActor400600ZebraStalkerBurstArmLeftPartVerts,
    _gActor400600ZebraStalkerBurstArmLeftVerts,
    _gActor400600ZebraStalkerBurstArmLeftNormals,
    _gActor400600ZebraStalkerBurstArmLeftSkeleton,
    _gActor400600ZebraStalkerBurstArmLeftStream,
};

static TmdBone _gActor400600ZebraStalkerBurstArmLeft13064Skeleton[1] = {
#include "assets/zebra_stalker_burst_arm_left_13064_skeleton.inc"
};

static u32 _gActor400600ZebraStalkerBurstArmLeft13064PartVerts[1] = {
#include "assets/zebra_stalker_burst_arm_left_13064_partVerts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstArmLeft13064Verts[10] = {
#include "assets/zebra_stalker_burst_arm_left_13064_verts.inc"
};

static SVECTOR _gActor400600ZebraStalkerBurstArmLeft13064Normals[17] = {
#include "assets/zebra_stalker_burst_arm_left_13064_normals.inc"
};

static u32 _gActor400600ZebraStalkerBurstArmLeft13064Stream[85] = {
#include "assets/zebra_stalker_burst_arm_left_13064_stream.inc"
};

static TmdSource _gActor400600ZebraStalkerBurstArmLeft13064 = {
    0,
    528,
    0,
    1,
    _gActor400600ZebraStalkerBurstArmLeft13064PartVerts,
    _gActor400600ZebraStalkerBurstArmLeft13064Verts,
    _gActor400600ZebraStalkerBurstArmLeft13064Normals,
    _gActor400600ZebraStalkerBurstArmLeft13064Skeleton,
    _gActor400600ZebraStalkerBurstArmLeft13064Stream,
};

DamageAttack D_actor_400600_80144EA8[2] = {
    { 26, 7 },
    { 10, 7 },
};

EnemyParams D_actor_400600_80144EB0 = { D_actor_400600_80144EA8, 180, 106, 36, 5, 100, 10, 100, 10 };

static AnimationPackedPose _gActor400600Animation136D4Bank1[14] = {
#include "assets/actor_400600_animation_136D4_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation136D4Bank4[144] = {
#include "assets/actor_400600_animation_136D4_bank4.inc"
};

static AnimationRecord _gActor400600Animation136D4Records[202] = {
#include "assets/actor_400600_animation_136D4_records.inc"
};

static u16 _gActor400600Animation136D4Indices[18] = {
#include "assets/actor_400600_animation_136D4_indices.inc"
};

static AnimationSet _gActor400600Animation136D4 = {
    _gActor400600Animation136D4Records,
    _gActor400600Animation136D4Indices,
    { NULL, _gActor400600Animation136D4Bank1, NULL, NULL, _gActor400600Animation136D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation13DA0Bank1[22] = {
#include "assets/actor_400600_animation_13DA0_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation13DA0Bank4[144] = {
#include "assets/actor_400600_animation_13DA0_bank4.inc"
};

static AnimationRecord _gActor400600Animation13DA0Records[206] = {
#include "assets/actor_400600_animation_13DA0_records.inc"
};

static u16 _gActor400600Animation13DA0Indices[18] = {
#include "assets/actor_400600_animation_13DA0_indices.inc"
};

static AnimationSet _gActor400600Animation13DA0 = {
    _gActor400600Animation13DA0Records,
    _gActor400600Animation13DA0Indices,
    { NULL, _gActor400600Animation13DA0Bank1, NULL, NULL, _gActor400600Animation13DA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation14210Bank1[8] = {
#include "assets/actor_400600_animation_14210_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation14210Bank4[95] = {
#include "assets/actor_400600_animation_14210_bank4.inc"
};

static AnimationRecord _gActor400600Animation14210Records[146] = {
#include "assets/actor_400600_animation_14210_records.inc"
};

static u16 _gActor400600Animation14210Indices[18] = {
#include "assets/actor_400600_animation_14210_indices.inc"
};

static AnimationSet _gActor400600Animation14210 = {
    _gActor400600Animation14210Records,
    _gActor400600Animation14210Indices,
    { NULL, _gActor400600Animation14210Bank1, NULL, NULL, _gActor400600Animation14210Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation14980Bank1[21] = {
#include "assets/actor_400600_animation_14980_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation14980Bank4[164] = {
#include "assets/actor_400600_animation_14980_bank4.inc"
};

static AnimationRecord _gActor400600Animation14980Records[230] = {
#include "assets/actor_400600_animation_14980_records.inc"
};

static u16 _gActor400600Animation14980Indices[18] = {
#include "assets/actor_400600_animation_14980_indices.inc"
};

static AnimationSet _gActor400600Animation14980 = {
    _gActor400600Animation14980Records,
    _gActor400600Animation14980Indices,
    { NULL, _gActor400600Animation14980Bank1, NULL, NULL, _gActor400600Animation14980Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation151E4Bank1[21] = {
#include "assets/actor_400600_animation_151E4_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation151E4Bank4[186] = {
#include "assets/actor_400600_animation_151E4_bank4.inc"
};

static AnimationRecord _gActor400600Animation151E4Records[269] = {
#include "assets/actor_400600_animation_151E4_records.inc"
};

static u16 _gActor400600Animation151E4Indices[18] = {
#include "assets/actor_400600_animation_151E4_indices.inc"
};

static AnimationSet _gActor400600Animation151E4 = {
    _gActor400600Animation151E4Records,
    _gActor400600Animation151E4Indices,
    { NULL, _gActor400600Animation151E4Bank1, NULL, NULL, _gActor400600Animation151E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation15954Bank1[20] = {
#include "assets/actor_400600_animation_15954_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation15954Bank4[174] = {
#include "assets/actor_400600_animation_15954_bank4.inc"
};

static AnimationRecord _gActor400600Animation15954Records[223] = {
#include "assets/actor_400600_animation_15954_records.inc"
};

static u16 _gActor400600Animation15954Indices[18] = {
#include "assets/actor_400600_animation_15954_indices.inc"
};

static AnimationSet _gActor400600Animation15954 = {
    _gActor400600Animation15954Records,
    _gActor400600Animation15954Indices,
    { NULL, _gActor400600Animation15954Bank1, NULL, NULL, _gActor400600Animation15954Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation16118Bank1[16] = {
#include "assets/actor_400600_animation_16118_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation16118Bank4[188] = {
#include "assets/actor_400600_animation_16118_bank4.inc"
};

static AnimationRecord _gActor400600Animation16118Records[242] = {
#include "assets/actor_400600_animation_16118_records.inc"
};

static u16 _gActor400600Animation16118Indices[18] = {
#include "assets/actor_400600_animation_16118_indices.inc"
};

static AnimationSet _gActor400600Animation16118 = {
    _gActor400600Animation16118Records,
    _gActor400600Animation16118Indices,
    { NULL, _gActor400600Animation16118Bank1, NULL, NULL, _gActor400600Animation16118Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation169A8Bank1[16] = {
#include "assets/actor_400600_animation_169A8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation169A8Bank4[212] = {
#include "assets/actor_400600_animation_169A8_bank4.inc"
};

static AnimationRecord _gActor400600Animation169A8Records[269] = {
#include "assets/actor_400600_animation_169A8_records.inc"
};

static u16 _gActor400600Animation169A8Indices[18] = {
#include "assets/actor_400600_animation_169A8_indices.inc"
};

static AnimationSet _gActor400600Animation169A8 = {
    _gActor400600Animation169A8Records,
    _gActor400600Animation169A8Indices,
    { NULL, _gActor400600Animation169A8Bank1, NULL, NULL, _gActor400600Animation169A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation16E3CBank1[9] = {
#include "assets/actor_400600_animation_16E3C_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation16E3CBank4[106] = {
#include "assets/actor_400600_animation_16E3C_bank4.inc"
};

static AnimationRecord _gActor400600Animation16E3CRecords[141] = {
#include "assets/actor_400600_animation_16E3C_records.inc"
};

static u16 _gActor400600Animation16E3CIndices[18] = {
#include "assets/actor_400600_animation_16E3C_indices.inc"
};

static AnimationSet _gActor400600Animation16E3C = {
    _gActor400600Animation16E3CRecords,
    _gActor400600Animation16E3CIndices,
    { NULL, _gActor400600Animation16E3CBank1, NULL, NULL, _gActor400600Animation16E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation17820Bank1[19] = {
#include "assets/actor_400600_animation_17820_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation17820Bank4[245] = {
#include "assets/actor_400600_animation_17820_bank4.inc"
};

static AnimationRecord _gActor400600Animation17820Records[312] = {
#include "assets/actor_400600_animation_17820_records.inc"
};

static u16 _gActor400600Animation17820Indices[18] = {
#include "assets/actor_400600_animation_17820_indices.inc"
};

static AnimationSet _gActor400600Animation17820 = {
    _gActor400600Animation17820Records,
    _gActor400600Animation17820Indices,
    { NULL, _gActor400600Animation17820Bank1, NULL, NULL, _gActor400600Animation17820Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation17D50Bank1[9] = {
#include "assets/actor_400600_animation_17D50_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation17D50Bank4[120] = {
#include "assets/actor_400600_animation_17D50_bank4.inc"
};

static AnimationRecord _gActor400600Animation17D50Records[166] = {
#include "assets/actor_400600_animation_17D50_records.inc"
};

static u16 _gActor400600Animation17D50Indices[18] = {
#include "assets/actor_400600_animation_17D50_indices.inc"
};

static AnimationSet _gActor400600Animation17D50 = {
    _gActor400600Animation17D50Records,
    _gActor400600Animation17D50Indices,
    { NULL, _gActor400600Animation17D50Bank1, NULL, NULL, _gActor400600Animation17D50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation18584Bank1[15] = {
#include "assets/actor_400600_animation_18584_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation18584Bank4[202] = {
#include "assets/actor_400600_animation_18584_bank4.inc"
};

static AnimationRecord _gActor400600Animation18584Records[259] = {
#include "assets/actor_400600_animation_18584_records.inc"
};

static u16 _gActor400600Animation18584Indices[18] = {
#include "assets/actor_400600_animation_18584_indices.inc"
};

static AnimationSet _gActor400600Animation18584 = {
    _gActor400600Animation18584Records,
    _gActor400600Animation18584Indices,
    { NULL, _gActor400600Animation18584Bank1, NULL, NULL, _gActor400600Animation18584Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1892CBank1[7] = {
#include "assets/actor_400600_animation_1892C_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1892CBank4[83] = {
#include "assets/actor_400600_animation_1892C_bank4.inc"
};

static AnimationRecord _gActor400600Animation1892CRecords[111] = {
#include "assets/actor_400600_animation_1892C_records.inc"
};

static u16 _gActor400600Animation1892CIndices[18] = {
#include "assets/actor_400600_animation_1892C_indices.inc"
};

static AnimationSet _gActor400600Animation1892C = {
    _gActor400600Animation1892CRecords,
    _gActor400600Animation1892CIndices,
    { NULL, _gActor400600Animation1892CBank1, NULL, NULL, _gActor400600Animation1892CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation18D74Bank1[9] = {
#include "assets/actor_400600_animation_18D74_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation18D74Bank4[100] = {
#include "assets/actor_400600_animation_18D74_bank4.inc"
};

static AnimationRecord _gActor400600Animation18D74Records[128] = {
#include "assets/actor_400600_animation_18D74_records.inc"
};

static u16 _gActor400600Animation18D74Indices[18] = {
#include "assets/actor_400600_animation_18D74_indices.inc"
};

static AnimationSet _gActor400600Animation18D74 = {
    _gActor400600Animation18D74Records,
    _gActor400600Animation18D74Indices,
    { NULL, _gActor400600Animation18D74Bank1, NULL, NULL, _gActor400600Animation18D74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation195A8Bank1[12] = {
#include "assets/actor_400600_animation_195A8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation195A8Bank4[191] = {
#include "assets/actor_400600_animation_195A8_bank4.inc"
};

static AnimationRecord _gActor400600Animation195A8Records[279] = {
#include "assets/actor_400600_animation_195A8_records.inc"
};

static u16 _gActor400600Animation195A8Indices[18] = {
#include "assets/actor_400600_animation_195A8_indices.inc"
};

static AnimationSet _gActor400600Animation195A8 = {
    _gActor400600Animation195A8Records,
    _gActor400600Animation195A8Indices,
    { NULL, _gActor400600Animation195A8Bank1, NULL, NULL, _gActor400600Animation195A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation199E0Bank1[6] = {
#include "assets/actor_400600_animation_199E0_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation199E0Bank4[100] = {
#include "assets/actor_400600_animation_199E0_bank4.inc"
};

static AnimationRecord _gActor400600Animation199E0Records[133] = {
#include "assets/actor_400600_animation_199E0_records.inc"
};

static u16 _gActor400600Animation199E0Indices[18] = {
#include "assets/actor_400600_animation_199E0_indices.inc"
};

static AnimationSet _gActor400600Animation199E0 = {
    _gActor400600Animation199E0Records,
    _gActor400600Animation199E0Indices,
    { NULL, _gActor400600Animation199E0Bank1, NULL, NULL, _gActor400600Animation199E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1A5D8Bank1[15] = {
#include "assets/actor_400600_animation_1A5D8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1A5D8Bank4[315] = {
#include "assets/actor_400600_animation_1A5D8_bank4.inc"
};

static AnimationRecord _gActor400600Animation1A5D8Records[387] = {
#include "assets/actor_400600_animation_1A5D8_records.inc"
};

static u16 _gActor400600Animation1A5D8Indices[18] = {
#include "assets/actor_400600_animation_1A5D8_indices.inc"
};

static AnimationSet _gActor400600Animation1A5D8 = {
    _gActor400600Animation1A5D8Records,
    _gActor400600Animation1A5D8Indices,
    { NULL, _gActor400600Animation1A5D8Bank1, NULL, NULL, _gActor400600Animation1A5D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1ABE8Bank1[11] = {
#include "assets/actor_400600_animation_1ABE8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1ABE8Bank4[153] = {
#include "assets/actor_400600_animation_1ABE8_bank4.inc"
};

static AnimationRecord _gActor400600Animation1ABE8Records[183] = {
#include "assets/actor_400600_animation_1ABE8_records.inc"
};

static u16 _gActor400600Animation1ABE8Indices[18] = {
#include "assets/actor_400600_animation_1ABE8_indices.inc"
};

static AnimationSet _gActor400600Animation1ABE8 = {
    _gActor400600Animation1ABE8Records,
    _gActor400600Animation1ABE8Indices,
    { NULL, _gActor400600Animation1ABE8Bank1, NULL, NULL, _gActor400600Animation1ABE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1AF6CBank1[5] = {
#include "assets/actor_400600_animation_1AF6C_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1AF6CBank4[79] = {
#include "assets/actor_400600_animation_1AF6C_bank4.inc"
};

static AnimationRecord _gActor400600Animation1AF6CRecords[112] = {
#include "assets/actor_400600_animation_1AF6C_records.inc"
};

static u16 _gActor400600Animation1AF6CIndices[18] = {
#include "assets/actor_400600_animation_1AF6C_indices.inc"
};

static AnimationSet _gActor400600Animation1AF6C = {
    _gActor400600Animation1AF6CRecords,
    _gActor400600Animation1AF6CIndices,
    { NULL, _gActor400600Animation1AF6CBank1, NULL, NULL, _gActor400600Animation1AF6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1B474Bank1[10] = {
#include "assets/actor_400600_animation_1B474_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1B474Bank4[122] = {
#include "assets/actor_400600_animation_1B474_bank4.inc"
};

static AnimationRecord _gActor400600Animation1B474Records[151] = {
#include "assets/actor_400600_animation_1B474_records.inc"
};

static u16 _gActor400600Animation1B474Indices[18] = {
#include "assets/actor_400600_animation_1B474_indices.inc"
};

static AnimationSet _gActor400600Animation1B474 = {
    _gActor400600Animation1B474Records,
    _gActor400600Animation1B474Indices,
    { NULL, _gActor400600Animation1B474Bank1, NULL, NULL, _gActor400600Animation1B474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1B89CBank1[10] = {
#include "assets/actor_400600_animation_1B89C_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1B89CBank4[93] = {
#include "assets/actor_400600_animation_1B89C_bank4.inc"
};

static AnimationRecord _gActor400600Animation1B89CRecords[124] = {
#include "assets/actor_400600_animation_1B89C_records.inc"
};

static u16 _gActor400600Animation1B89CIndices[18] = {
#include "assets/actor_400600_animation_1B89C_indices.inc"
};

static AnimationSet _gActor400600Animation1B89C = {
    _gActor400600Animation1B89CRecords,
    _gActor400600Animation1B89CIndices,
    { NULL, _gActor400600Animation1B89CBank1, NULL, NULL, _gActor400600Animation1B89CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1C300Bank1[40] = {
#include "assets/actor_400600_animation_1C300_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1C300Bank4[233] = {
#include "assets/actor_400600_animation_1C300_bank4.inc"
};

static AnimationRecord _gActor400600Animation1C300Records[293] = {
#include "assets/actor_400600_animation_1C300_records.inc"
};

static u16 _gActor400600Animation1C300Indices[18] = {
#include "assets/actor_400600_animation_1C300_indices.inc"
};

static AnimationSet _gActor400600Animation1C300 = {
    _gActor400600Animation1C300Records,
    _gActor400600Animation1C300Indices,
    { NULL, _gActor400600Animation1C300Bank1, NULL, NULL, _gActor400600Animation1C300Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1C654Bank1[6] = {
#include "assets/actor_400600_animation_1C654_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1C654Bank4[75] = {
#include "assets/actor_400600_animation_1C654_bank4.inc"
};

static AnimationRecord _gActor400600Animation1C654Records[101] = {
#include "assets/actor_400600_animation_1C654_records.inc"
};

static u16 _gActor400600Animation1C654Indices[18] = {
#include "assets/actor_400600_animation_1C654_indices.inc"
};

static AnimationSet _gActor400600Animation1C654 = {
    _gActor400600Animation1C654Records,
    _gActor400600Animation1C654Indices,
    { NULL, _gActor400600Animation1C654Bank1, NULL, NULL, _gActor400600Animation1C654Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1CB40Bank1[11] = {
#include "assets/actor_400600_animation_1CB40_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1CB40Bank4[111] = {
#include "assets/actor_400600_animation_1CB40_bank4.inc"
};

static AnimationRecord _gActor400600Animation1CB40Records[152] = {
#include "assets/actor_400600_animation_1CB40_records.inc"
};

static u16 _gActor400600Animation1CB40Indices[18] = {
#include "assets/actor_400600_animation_1CB40_indices.inc"
};

static AnimationSet _gActor400600Animation1CB40 = {
    _gActor400600Animation1CB40Records,
    _gActor400600Animation1CB40Indices,
    { NULL, _gActor400600Animation1CB40Bank1, NULL, NULL, _gActor400600Animation1CB40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1D0E4Bank1[10] = {
#include "assets/actor_400600_animation_1D0E4_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1D0E4Bank4[139] = {
#include "assets/actor_400600_animation_1D0E4_bank4.inc"
};

static AnimationRecord _gActor400600Animation1D0E4Records[173] = {
#include "assets/actor_400600_animation_1D0E4_records.inc"
};

static u16 _gActor400600Animation1D0E4Indices[18] = {
#include "assets/actor_400600_animation_1D0E4_indices.inc"
};

static AnimationSet _gActor400600Animation1D0E4 = {
    _gActor400600Animation1D0E4Records,
    _gActor400600Animation1D0E4Indices,
    { NULL, _gActor400600Animation1D0E4Bank1, NULL, NULL, _gActor400600Animation1D0E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1D2A8Bank1[2] = {
#include "assets/actor_400600_animation_1D2A8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1D2A8Bank4[16] = {
#include "assets/actor_400600_animation_1D2A8_bank4.inc"
};

static AnimationRecord _gActor400600Animation1D2A8Records[72] = {
#include "assets/actor_400600_animation_1D2A8_records.inc"
};

static u16 _gActor400600Animation1D2A8Indices[18] = {
#include "assets/actor_400600_animation_1D2A8_indices.inc"
};

static AnimationSet _gActor400600Animation1D2A8 = {
    _gActor400600Animation1D2A8Records,
    _gActor400600Animation1D2A8Indices,
    { NULL, _gActor400600Animation1D2A8Bank1, NULL, NULL, _gActor400600Animation1D2A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1D46CBank1[2] = {
#include "assets/actor_400600_animation_1D46C_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1D46CBank4[16] = {
#include "assets/actor_400600_animation_1D46C_bank4.inc"
};

static AnimationRecord _gActor400600Animation1D46CRecords[72] = {
#include "assets/actor_400600_animation_1D46C_records.inc"
};

static u16 _gActor400600Animation1D46CIndices[18] = {
#include "assets/actor_400600_animation_1D46C_indices.inc"
};

static AnimationSet _gActor400600Animation1D46C = {
    _gActor400600Animation1D46CRecords,
    _gActor400600Animation1D46CIndices,
    { NULL, _gActor400600Animation1D46CBank1, NULL, NULL, _gActor400600Animation1D46CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1D630Bank1[2] = {
#include "assets/actor_400600_animation_1D630_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1D630Bank4[16] = {
#include "assets/actor_400600_animation_1D630_bank4.inc"
};

static AnimationRecord _gActor400600Animation1D630Records[72] = {
#include "assets/actor_400600_animation_1D630_records.inc"
};

static u16 _gActor400600Animation1D630Indices[18] = {
#include "assets/actor_400600_animation_1D630_indices.inc"
};

static AnimationSet _gActor400600Animation1D630 = {
    _gActor400600Animation1D630Records,
    _gActor400600Animation1D630Indices,
    { NULL, _gActor400600Animation1D630Bank1, NULL, NULL, _gActor400600Animation1D630Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1D7F4Bank1[2] = {
#include "assets/actor_400600_animation_1D7F4_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1D7F4Bank4[16] = {
#include "assets/actor_400600_animation_1D7F4_bank4.inc"
};

static AnimationRecord _gActor400600Animation1D7F4Records[72] = {
#include "assets/actor_400600_animation_1D7F4_records.inc"
};

static u16 _gActor400600Animation1D7F4Indices[18] = {
#include "assets/actor_400600_animation_1D7F4_indices.inc"
};

static AnimationSet _gActor400600Animation1D7F4 = {
    _gActor400600Animation1D7F4Records,
    _gActor400600Animation1D7F4Indices,
    { NULL, _gActor400600Animation1D7F4Bank1, NULL, NULL, _gActor400600Animation1D7F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1DC70Bank1[8] = {
#include "assets/actor_400600_animation_1DC70_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1DC70Bank4[100] = {
#include "assets/actor_400600_animation_1DC70_bank4.inc"
};

static AnimationRecord _gActor400600Animation1DC70Records[144] = {
#include "assets/actor_400600_animation_1DC70_records.inc"
};

static u16 _gActor400600Animation1DC70Indices[18] = {
#include "assets/actor_400600_animation_1DC70_indices.inc"
};

static AnimationSet _gActor400600Animation1DC70 = {
    _gActor400600Animation1DC70Records,
    _gActor400600Animation1DC70Indices,
    { NULL, _gActor400600Animation1DC70Bank1, NULL, NULL, _gActor400600Animation1DC70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1E468Bank1[19] = {
#include "assets/actor_400600_animation_1E468_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1E468Bank4[188] = {
#include "assets/actor_400600_animation_1E468_bank4.inc"
};

static AnimationRecord _gActor400600Animation1E468Records[246] = {
#include "assets/actor_400600_animation_1E468_records.inc"
};

static u16 _gActor400600Animation1E468Indices[18] = {
#include "assets/actor_400600_animation_1E468_indices.inc"
};

static AnimationSet _gActor400600Animation1E468 = {
    _gActor400600Animation1E468Records,
    _gActor400600Animation1E468Indices,
    { NULL, _gActor400600Animation1E468Bank1, NULL, NULL, _gActor400600Animation1E468Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1ECE8Bank1[22] = {
#include "assets/actor_400600_animation_1ECE8_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1ECE8Bank4[199] = {
#include "assets/actor_400600_animation_1ECE8_bank4.inc"
};

static AnimationRecord _gActor400600Animation1ECE8Records[260] = {
#include "assets/actor_400600_animation_1ECE8_records.inc"
};

static u16 _gActor400600Animation1ECE8Indices[18] = {
#include "assets/actor_400600_animation_1ECE8_indices.inc"
};

static AnimationSet _gActor400600Animation1ECE8 = {
    _gActor400600Animation1ECE8Records,
    _gActor400600Animation1ECE8Indices,
    { NULL, _gActor400600Animation1ECE8Bank1, NULL, NULL, _gActor400600Animation1ECE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1F4ECBank1[11] = {
#include "assets/actor_400600_animation_1F4EC_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1F4ECBank4[183] = {
#include "assets/actor_400600_animation_1F4EC_bank4.inc"
};

static AnimationRecord _gActor400600Animation1F4ECRecords[277] = {
#include "assets/actor_400600_animation_1F4EC_records.inc"
};

static u16 _gActor400600Animation1F4ECIndices[20] = {
#include "assets/actor_400600_animation_1F4EC_indices.inc"
};

static AnimationSet _gActor400600Animation1F4EC = {
    _gActor400600Animation1F4ECRecords,
    _gActor400600Animation1F4ECIndices,
    { NULL, _gActor400600Animation1F4ECBank1, NULL, NULL, _gActor400600Animation1F4ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400600Animation1FC00Bank1[14] = {
#include "assets/actor_400600_animation_1FC00_bank1.inc"
};

static AnimationPackedRotation _gActor400600Animation1FC00Bank4[169] = {
#include "assets/actor_400600_animation_1FC00_bank4.inc"
};

static AnimationRecord _gActor400600Animation1FC00Records[222] = {
#include "assets/actor_400600_animation_1FC00_records.inc"
};

static u16 _gActor400600Animation1FC00Indices[20] = {
#include "assets/actor_400600_animation_1FC00_indices.inc"
};

static AnimationSet _gActor400600Animation1FC00 = {
    _gActor400600Animation1FC00Records,
    _gActor400600Animation1FC00Indices,
    { NULL, _gActor400600Animation1FC00Bank1, NULL, NULL, _gActor400600Animation1FC00Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_400600_80151A48[3] = {
    NULL,
    &_gActor400600Animation1F4EC,
    &_gActor400600Animation1FC00,
};

AnimationSet* D_actor_400600_80151A54[35] = {
    NULL,
    &_gActor400600Animation136D4,
    &_gActor400600Animation13DA0,
    &_gActor400600Animation14210,
    &_gActor400600Animation14980,
    &_gActor400600Animation151E4,
    &_gActor400600Animation15954,
    &_gActor400600Animation16118,
    &_gActor400600Animation169A8,
    &_gActor400600Animation16E3C,
    &_gActor400600Animation17820,
    &_gActor400600Animation17D50,
    &_gActor400600Animation18584,
    &_gActor400600Animation1892C,
    &_gActor400600Animation18D74,
    &_gActor400600Animation195A8,
    &_gActor400600Animation199E0,
    &_gActor400600Animation1A5D8,
    &_gActor400600Animation1ABE8,
    &_gActor400600Animation1AF6C,
    &_gActor400600Animation1B474,
    &_gActor400600Animation1B89C,
    &_gActor400600Animation1C300,
    NULL,
    NULL,
    &_gActor400600Animation1C654,
    &_gActor400600Animation1CB40,
    &_gActor400600Animation1D0E4,
    &_gActor400600Animation1D2A8,
    &_gActor400600Animation1D46C,
    &_gActor400600Animation1D630,
    &_gActor400600Animation1D7F4,
    &_gActor400600Animation1DC70,
    &_gActor400600Animation1E468,
    &_gActor400600Animation1ECE8,
};

TaskMessageEntry D_actor_400600_80151AE0[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, stalkerZebraIvorySetMoveMode },
    { 2014, func_actor_400600_8013A3A8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_400600_80151AF8[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_400600_8013A3B8, { .model = &_gActor400600ZebraStalkerBurstArmLeft13064 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_400600_8013A3C0, { .model = &_gActor400600ZebraStalkerBurstArmLeft } },
};

TaskDesc D_actor_400600_80151B10 = { { { TASK_BODY_TMD, 96 } }, func_actor_400600_8013A0F0, { .model = &_gActor400600ZebraStalkerBody } };

u8 gStalkerZebraIvoryResumeClips[36] = {
    26,
    26,
    26,
    27,
    27,
    15,
    15,
    26,
    26,
    26,
    26,
    27,
    27,
    15,
    15,
    26,
    26,
    27,
    27,
    30,
    27,
    26,
    26,
    26,
    26,
    26,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    0,
};

ActorZone D_actor_400600_80151B40[7] = {
    { 3000, -1500, 3000, 3000, 6 },
    { -3000, -3000, 6000, 6000, 1 },
    { 3000, -6000, 3000, 0x2EE0, 2 },
    { -6000, -6000, 9000, 3000, 3 },
    { -6000, -3000, 3000, 9000, 4 },
    { -3000, 3000, 5400, 3000, 5 },
    { 0, 0, 0, 0, -1 },
};

s16 D_actor_400600_80151B88[12] = {
    1,
    3,
    5,
    6,
    8,
    9,
    11,
    12,
    14,
    15,
    17,
    -1,
};

static inline void _actor400600SetCoordRotation(GfxCoord* coord, s16 angle);

#include "../../shared/stalker_zebra_ivory_inlines.inc.c"

static void func_actor_400600_8013203C(Task* arg0)
{
    Actor400600Work* work;

    work                           = (Actor400600Work*)arg0->work;
    work->obj_4B4.coord            = &arg0->extra.tmd->coords[3];
    work->obj_4B4.context.contacts = work->rec_4D4;
    work->obj_4B4.pos.vx           = 0;
    work->obj_4B4.pos.vy           = 0x96;
    work->obj_4B4.pos.vz           = 0x110;
    work->obj_4B4.key              = 0x30006;
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT && (gGameSession->location.loc.area == 0x1F || gGameSession->location.loc.area == 0x1D)) {
        work->obj_4B4.radius = 0x260;
    } else {
        work->obj_4B4.radius = 0x200;
    }
    work->obj_4B4.flags = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj_4B4);
    Gp_InitRec18Table(work->rec_4D4, 8, 0);
    work->capsule.ends[0].vz          = 0xBB8;
    work->capsule.end0Radius          = 0xA;
    work->capsule.end1Radius          = 0xA;
    work->capsule.ends[0].vx          = 0;
    work->capsule.ends[1].vz          = 0;
    work->capsule.ends[1].vx          = 0;
    work->capsule.contacts            = work->capsuleContacts;
    work->obj_4B4.flags              |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->capsuleBody.coord           = arg0->extra.tmd->coords;
    work->capsuleBody.context.capsule = &work->capsule;
    work->capsuleBody.pos.vx          = 0;
    work->capsuleBody.pos.vy          = -0x190;
    work->capsuleBody.pos.vz          = 0;
    work->capsuleBody.key             = 0x30006;
    work->capsuleBody.radius          = 0;
    work->capsuleBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(2, &work->capsuleBody);
    Gp_InitRec18Table(work->capsuleContacts, 8, 0);
    work->capsuleBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->obj_594.key              = Gp_PackPair(D_actor_400600_80144EA8, 0);
    work->obj_594.coord            = &arg0->extra.tmd->coords[7];
    work->obj_594.context.contacts = work->rec_5B4;
    work->obj_594.pos.vx           = -0x200;
    work->obj_594.pos.vy           = 0;
    work->obj_594.pos.vz           = 0;
    work->obj_594.radius           = 0x190;
    work->obj_594.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj_594);
    Gp_InitRec18Table(work->rec_5B4, 1, 0);
    work->obj_594.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_5CC.key              = Gp_PackPair(D_actor_400600_80144EA8, 0);
    work->obj_5CC.coord            = &arg0->extra.tmd->coords[10];
    work->obj_5CC.context.contacts = work->rec_5EC;
    work->obj_5CC.pos.vx           = 0x200;
    work->obj_5CC.pos.vy           = 0;
    work->obj_5CC.pos.vz           = 0;
    work->obj_5CC.radius           = 0x190;
    work->obj_5CC.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj_5CC);
    Gp_InitRec18Table(work->rec_5EC, 1, 0);
    work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

static void func_actor_400600_80132294(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    ActorBeamScratch* s;
    s16               angle;
    GfxCoord*         secondCoord;
    GfxCoord*         firstCoord;
    s32               offset0;
    s32               offset1;
    s32               offset2;
    s32               offset3;
    s32               halfX;
    s32               halfY;
    GfxCoord*         coords;
    POLY_FT4*         poly;

    coords      = task->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (ActorBeamScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(ActorBeamScratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->first.vx                = s->firstMatrix.t[0];
        s->first.vy                = s->firstMatrix.t[1];
        s->second.vx               = s->secondMatrix.t[0];
        s->second.vy               = s->secondMatrix.t[1];
        s->first.vz                = height;
        s->second.vz               = height;
        angle                      = ratan2(s->second.vx - s->first.vx, s->second.vy - s->first.vy);
        halfX                      = (s->first.vx - s->second.vx) / 2;
        halfY                      = (s->first.vy - s->second.vy) / 2;
        s->corner0.vx              = halfX + (s->first.vx - ((s32)(rcos(angle) * width) >> 0xC));
        offset0                    = rsin(angle) * width;
        s->corner0.vz              = height;
        s->corner0.vy              = halfY + (s->first.vy + (offset0 >> 0xC));
        s->corner1.vx              = halfX + (s->first.vx + ((s32)(rcos(angle) * width) >> 0xC));
        offset1                    = rsin(angle) * width;
        s->corner1.vz              = height;
        s->corner1.vy              = halfY + (s->first.vy - (offset1 >> 0xC));
        s->corner2.vx              = (s->second.vx - ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
        offset2                    = rsin(angle) * width;
        s->corner2.vz              = height;
        s->corner2.vy              = (s->second.vy + (offset2 >> 0xC)) - halfY;
        s->corner3.vx              = (s->second.vx + ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
        offset3                    = rsin(angle) * width;
        s->corner3.vz              = height;
        s->corner3.vy              = (s->second.vy - (offset3 >> 0xC)) - halfY;
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 9);
            poly->code                     = 0x2E;
            GPU_PRIMITIVE_XY_WORD(poly, 0) = s->screen0;
            GPU_PRIMITIVE_XY_WORD(poly, 1) = s->screen1;
            GPU_PRIMITIVE_XY_WORD(poly, 2) = s->screen2;
            GPU_PRIMITIVE_XY_WORD(poly, 3) = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
        SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorBeamScratch));
    }
}

static void func_actor_400600_80132704(Task* arg0, s16 arg1, u8 arg2)
{
    func_actor_400600_80132294(arg0, 3, 9, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 9, 0xA, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xA, 0xB, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 3, 6, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 6, 7, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 7, 8, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 5, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 0xC, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xC, 0xD, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xD, 0xE, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 0xF, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xF, 0x10, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0x10, 0x11, 0x80, arg1, arg2);
}

static void func_actor_400600_801328A8(Task* arg0)
{
    GfxCoord*        coords;
    Actor400600Work* work;
    s32              sound;
    s32              pan;

    coords              = arg0->extra.tmd->coords;
    work                = (Actor400600Work*)arg0->work;
    coords->coord.t[0] += (0x4364 - coords->coord.t[0]) >> 2;
    coords->coord.t[2] += (0x760 - coords->coord.t[2]) >> 2;
    work->field_722    += 2;
    work->field_724    += work->field_722;
    coords->coord.t[1] += work->field_724;
    if (coords->coord.t[1] >= -0x508) {
        Gp_SpawnPadLerp(0xA, 0xC0, 0x80);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x531A0009;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        func_dryfield_night_junk_yard_8017D9B8(1);
        stalkerZebraIvoryPlayClip(arg0, 0x19, 0x30);
        coords->coord.t[1] = -0x508;
        work->state++;
    }
}

static void func_actor_400600_801329EC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    u32              sound;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    if ((s16)work->field_718 == 0) {
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x531A000A;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    work->field_718++;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40060004;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->state         = 0;
        work2->subState      = 0;
        work3                = (Actor400600Work*)arg0->work;
        work3->state         = 2;
        work3->subState      = 0;
    }
}

static void func_actor_400600_80132B3C(Task* arg0)
{
    GfxCoord*        coords;
    Actor400600Work* work;
    s32              sound;
    s32              pan;

    coords              = arg0->extra.tmd->coords;
    work                = (Actor400600Work*)arg0->work;
    coords->coord.t[0] += (0x1C54 - coords->coord.t[0]) >> 2;
    coords->coord.t[2] += (0xED5 - coords->coord.t[2]) >> 2;
    work->field_722    += 2;
    work->field_724    += work->field_722;
    coords->coord.t[1] += work->field_724;
    if (coords->coord.t[1] >= 0) {
        Gp_SpawnPadLerp(0x10, 0x80, 0x40);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x531A000A;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        stalkerZebraIvoryPlayClip(arg0, 0x19, 0x10);
        coords->coord.t[1] = 0;
        work->state++;
    }
}

static void func_actor_400600_80132C70(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    Actor400600Work* work4;
    GfxCoord*        coords;
    u8               mode;
    s16              yaw;

    work   = (Actor400600Work*)arg0->work;
    mode   = work->roomCommand;
    coords = arg0->extra.tmd->coords;
    if (mode == 1) {
        coords->coord.t[0] = 0x36B0;
        coords->coord.t[1] = -0x320;
        coords->coord.t[2] = -0x7D0;
        work->pitch        = 0;
        work->yaw          = 0x400;
        work->roll         = 0x400;
        work->field_718    = 0;
        stalkerZebraIvoryPlayClip(arg0, 2, 0x10);
        work->field_73C = -0x7D0;
        work->state++;
    } else if (mode == 2) {
        coords->coord.t[0] = 0x4A38;
        coords->coord.t[1] = -0x320;
        coords->coord.t[2] = -0xFA0;
        yaw                = -0x400;
        work->yaw          = yaw;
        work->pitch        = 0;
        work->roll         = 0x400;
        work->field_718    = 0;
        stalkerZebraIvoryPlayClip(arg0, 2, 0x10);
        work->field_73C = -0xFA0;
        work2           = (Actor400600Work*)arg0->work;
        work2->state    = 5;
        work2->subState = 0;
    } else if (mode == 3) {
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        coords->coord.t[0]   = 0x2AF8;
        coords->coord.t[2]   = -0x3E8;
        coords->coord.t[1]   = 0;
        work->pitch          = 0;
        work->yaw            = 0xC00;
        work3                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work3->state         = 0;
        work3->subState      = 0;
    } else if (mode == 4) {
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        coords->coord.t[0]   = 0x3A98;
        coords->coord.t[2]   = -0xBB8;
        coords->coord.t[1]   = 0;
        work->pitch          = 0;
        work->yaw            = 0x400;
        work4                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work4->state         = 0;
        work4->subState      = 0;
    }
}

static void func_actor_400600_80132E10(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    u32              sound;
    s32              pan;

    work->field_718 = work->field_718 + 1;
    func_actor_400600_801361AC(arg0);
    if ((s16)work->field_718 == 0x26) {
        func_actor_400600_80138B5C(arg0, 0);
    }
    if ((s16)work->field_718 >= 0x27) {
        work->field_73A = (u16)work->field_73A + ((0xFF - work->field_73A) >> 4);
    }
    if ((s16)work->field_718 == 0x5A) {
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A0004;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_718 = 0;
        work->field_722 = -0xA;
        work->field_724 = 0;
        func_actor_400600_80139DB0(arg0, 0x15, 0x10, 4);
        work->state = work->state + 1;
    }
}

static void func_actor_400600_80132F3C(Task* arg0)
{
    Actor400600Work* work;
    GfxCoord*        coords;
    s32              sound;
    s32              pan;

    work   = (Actor400600Work*)arg0->work;
    coords = arg0->extra.tmd->coords;
    work->field_718++;
    if ((s16)work->field_718 >= 0x11) {
        work->field_73A    += -work->field_73A >> 3;
        work->field_722    += 2;
        work->field_724    += work->field_722;
        coords->coord.t[1] += work->field_724;
        coords->coord.t[2] += (-0xBB8 - coords->coord.t[2]) >> 3;
        coords->coord.t[0] += (0x4588 - coords->coord.t[0]) >> 2;
        work->yaw          += (0xC00 - (s16)work->yaw) >> 2;
        work->roll         += -(s16)work->roll >> 3;
        if ((s16)work->field_718 == 0x22) {
            func_actor_400600_8013896C(arg0, 0);
        }
        if ((s16)work->field_718 == 0x23 || (s16)work->field_718 == 0x25) {
            Gp_SpawnEff(gRoomEffectWaterRippleId, coords, 0x38, NULL);
        }
        if (coords->coord.t[1] >= 0) {
            sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x404A0003;
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            stalkerZebraIvoryPlayClip(arg0, 0x19, 0x10);
            coords->coord.t[1] = 0;
            work->state++;
        }
    }
}

static void func_actor_400600_80133118(Task* arg0)
{
    Actor400600Work* work;
    GfxCoord*        coords;
    s32              sound;
    s32              pan;

    work   = (Actor400600Work*)arg0->work;
    coords = arg0->extra.tmd->coords;
    work->field_718++;
    if ((s16)work->field_718 >= 0x11) {
        work->field_73A    += -work->field_73A >> 3;
        work->field_722    += 2;
        work->field_724    += work->field_722;
        coords->coord.t[1] += work->field_724;
        coords->coord.t[2] += (-0xBB8 - coords->coord.t[2]) >> 3;
        coords->coord.t[0] += (0x3A98 - coords->coord.t[0]) >> 2;
        work->yaw          += (0x400 - (s16)work->yaw) >> 2;
        work->roll         += -(s16)work->roll >> 3;
        if ((s16)work->field_718 == 0x22) {
            func_actor_400600_8013896C(arg0, 0);
        }
        if ((s16)work->field_718 == 0x23 || (s16)work->field_718 == 0x25) {
            Gp_SpawnEff(gRoomEffectWaterRippleId, coords, 0x38, NULL);
        }
        if (coords->coord.t[1] >= 0) {
            sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x404A0003;
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            stalkerZebraIvoryPlayClip(arg0, 0x19, 0x10);
            coords->coord.t[1] = 0;
            work->state++;
        }
    }
}

static void func_actor_400600_801332F4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GfxCoord*        coord;
    s32              mode;

    work  = (Actor400600Work*)arg0->work;
    model = arg0->extra.tmd;
    mode  = work->roomCommand;
    coord = model->coords;
    if (mode == 1) {
        Gp_SpawnPadLerp(0x14, 0xFF, 0x80);
        coord->coord.t[0] = 0xCE4;
        coord->coord.t[1] = -0xBB8;
        coord->coord.t[2] = 0;
        work->pitch       = 0;
        work->yaw         = 0xC00;
        work->roll        = 0;
        model->flags     &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_718   = 0;
        work->field_722   = 0;
        work->field_724   = 0;
        stalkerZebraIvoryPlayClip(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->state = work->state + 1;
    } else if (mode == 2) {
        work->obj_4B4.flags                     |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        coord->coord.t[0]                        = -0x6A4;
        coord->coord.t[2]                        = -0x514;
        coord->coord.t[1]                        = 0;
        work->yaw                                = 0x400;
        work->field_73A                          = 0xFF;
        work->pitch                              = 0;
        work->roll                               = 0;
        gSceneCombatState.zebraStalkerGroupPhase = mode;
        work2                                    = (Actor400600Work*)arg0->work;
        arg0->state                              = 1;
        work2->state                             = 0;
        work2->subState                          = 0;
    }
}

static const TaskFuncTable12 D_actor_400600_80131E24 = { {
    func_actor_400600_8013A3C8,
    stalkerZebraIvoryResumeClip,
    stalkerZebraIvoryAnimateUntilDone,
    func_actor_400600_8013A570,
    func_actor_400600_8013A638,
    func_actor_400600_8013A6C4,
    func_actor_400600_8013A808,
    func_actor_400600_8013A820,
    func_actor_400600_8013A864,
    func_actor_400600_8013A908,
    func_actor_400600_8013A990,
    func_actor_400600_8013AA5C,
} };

static const TaskFuncTable6 D_actor_400600_80131E54 = { {
    func_actor_400600_8013AB98,
    func_actor_400600_8013AC14,
    func_actor_400600_801328A8,
    func_actor_400600_8013AD3C,
    func_actor_400600_8013ADA4,
    func_actor_400600_801329EC,
} };

static const TaskFuncTable4 D_actor_400600_80131E6C = { {
    func_actor_400600_8013AE88,
    func_actor_400600_8013AF04,
    func_actor_400600_80132B3C,
    func_actor_400600_8013B018,
} };

static const TaskFuncTable8 D_actor_400600_80131E7C = { {
    func_actor_400600_8013B0FC,
    func_actor_400600_80132C70,
    func_actor_400600_80132E10,
    func_actor_400600_80132F3C,
    func_actor_400600_8013B150,
    func_actor_400600_8013B1DC,
    func_actor_400600_80133118,
    func_actor_400600_8013B2A8,
} };

static const TaskFuncTable4 D_actor_400600_80131E9C = { {
    func_actor_400600_8013B394,
    func_actor_400600_801332F4,
    func_actor_400600_8013B410,
    func_actor_400600_8013B520,
} };

static const TaskFuncTable9 D_actor_400600_80131EAC = { {
    func_actor_400600_80133434,
    func_actor_400600_801337A8,
    func_actor_400600_8013A170,
    func_actor_400600_8013A26C,
    func_actor_400600_80138C34,
    func_actor_400600_80138D78,
    func_actor_400600_80138EA0,
    func_actor_400600_80138FD4,
    func_actor_400600_80137EF0,
} };

static void func_actor_400600_80133434(Task* arg0)
{
    TmdObject*       model;
    Enemy*           enemy;
    GfxCoord*        coord;
    Actor400600Work* work;
    Actor400600Work* w2;
    Actor400600Work* w3;
    Actor400600Work* w4;
    u32              rnd;

    model      = arg0->extra.tmd;
    enemy      = (Enemy*)arg0->spawnArg2.pointer;
    coord      = model->coords;
    arg0->work = memCalloc(0x770U, false);
    work       = (Actor400600Work*)arg0->work;
    if (work == NULL) {
        enemyDestroy(enemy, arg0);
        return;
    }
    func_actor_400600_8013B640();
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 20, 0, 0)) {
        work->field_76E = 1;
    }
    model->lightMtx   = &work->matrix_40;
    model->colorMtx   = &work->matrix_20;
    model->flags      = 0;
    arg0->msgTable    = D_actor_400600_80151AE0;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &arg0->extra.tmd->coords[3];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
    enemy->param                  = &D_actor_400600_80144EB0;
    enemy->recs                   = work->rec_4D4;
    work->eff_6FC.coord           = &arg0->extra.tmd->coords[3];
    work->eff_6FC.spawnArgLo      = 0x300;
    work->eff_6FC.spawnArgHi      = 2;
    enemy->hp = enemy->hpMax = D_actor_400600_80144EB0.hpMax;
    animationInitContext(&work->anim, D_actor_400600_80151A54, model, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->pad_394, work->slots);

    w2              = (Actor400600Work*)arg0->work;
    w2->animStep    = 0x10;
    w2->animClip    = 1;
    w2->animRequest = 2;

    stalkerZebraIvoryTickAnimInline(arg0);

    coord->parent = &gGfxViewCoord;
    work->yaw     = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    func_actor_400600_8013203C(arg0);
    work->obj_4B4.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    func_actor_400600_801356E0(arg0);
    (Gp_IncStateF0Ref)(0);
    func_actor_400600_80138A24(arg0, 1);
    w3              = (Actor400600Work*)arg0->work;
    w3->state       = 0;
    w3->subState    = 0;
    work->field_90  = coord->coord.t[0];
    work->field_92  = coord->coord.t[1];
    work->field_94  = coord->coord.t[2];
    rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState = rnd;
    work->field_716 = rnd >> 0x10;
    work->field_73E = work->field_92;
    switch ((u8)arg0->spawnArg1.value >> 4) {
        case 0:
            w4           = (Actor400600Work*)arg0->work;
            arg0->state  = 1;
            w4->state    = 0;
            w4->subState = 0;
            break;
        case 1:
            work->field_76C = 1;
            w4              = (Actor400600Work*)arg0->work;
            arg0->state     = 4;
            w4->state       = 0;
            w4->subState    = 0;
            break;
        case 2:
            w4           = (Actor400600Work*)arg0->work;
            arg0->state  = 5;
            w4->state    = 0;
            w4->subState = 0;
            break;
        case 3:
            w4           = (Actor400600Work*)arg0->work;
            arg0->state  = 6;
            w4->state    = 0;
            w4->subState = 0;
            break;
        case 4:
            w4           = (Actor400600Work*)arg0->work;
            arg0->state  = 7;
            w4->state    = 0;
            w4->subState = 0;
            break;
        case 5:
            w4           = (Actor400600Work*)arg0->work;
            arg0->state  = 8;
            w4->state    = 0;
            w4->subState = 0;
            break;
    }
}

static const TaskFuncTable18 D_actor_400600_80131EEC = { {
    func_actor_400600_801390FC,
    func_actor_400600_80133B88,
    func_actor_400600_80139110,
    func_actor_400600_80139218,
    func_actor_400600_80139280,
    func_actor_400600_801392E8,
    func_actor_400600_8013935C,
    func_actor_400600_801393D0,
    func_actor_400600_80139444,
    func_actor_400600_801394E0,
    func_actor_400600_80139560,
    func_actor_400600_80139608,
    func_actor_400600_80139670,
    func_actor_400600_801396E4,
    func_actor_400600_80139764,
    stalkerZebraIvoryRunSubStates,
    func_actor_400600_80139878,
    func_actor_400600_801398E0,
} };

static void func_actor_400600_801337A8(Task* arg0)
{
    TmdObject*       model = arg0->extra.tmd;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    Enemy*           enemy = (Enemy*)arg0->spawnArg2.pointer;
    TaskFuncTable18  fns   = D_actor_400600_80131EEC;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            func_actor_400600_801387DC(arg0, -1);
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            fns.funcs[(s16)work->state](arg0);
            func_actor_400600_80138AB8(arg0);
            func_actor_400600_80137840(arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryTickAnimInline(arg0);
            work->previousAnimationFlags = work->slots[1].status.fields.flags;
            stalkerZebraIvoryApplyRotationInline(arg0);
            func_actor_400600_80136968(arg0);
            if (enemy->hp <= 0 && (u8)work->holding == 0) {
                Actor400600Work* w = (Actor400600Work*)arg0->work;
                arg0->state        = 2;
                w->state           = 0;
                w->subState        = 0;
            }
        case SCENE_COMBAT_ACTORS_PAUSED:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->capsuleContacts);
            actorUpdateModelColor(arg0);
            func_actor_400600_80138224(arg0, work->field_73E, work->field_73A);
            if (work->cloaked == 0) {
                model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                func_actor_400600_801387DC(arg0, -1);
            }
            break;
    }
}

static void func_actor_400600_80133B88(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    s32              sound;
    s32              pan;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if (work->playerDistance < 0xBB8 || gSceneCombatState.zebraStalkerDeathAlert != 0) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40060004;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        func_actor_400600_80138B5C(arg0, 0);
        Gp_ArmStateF0(1);
        rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        work->field_758 = ((rnd >> 0x10) & 0x3F) + 0x1E;
        work2           = (Actor400600Work*)arg0->work;
        work2->state    = 2;
        work2->subState = 0;
        return;
    }
    if ((s16)func_actor_400600_80136FA8(arg0) != 0) {
        Gp_ArmStateF0(1);
    }
}

static void func_actor_400600_80133CB0(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    work->field_718++;
    if ((s16)work->field_718 == 0x15) {
        id = 0x40060005;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->obj_5CC.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if ((s16)work->field_718 == 0x1C) {
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        stalkerZebraIvorySeedTimer(arg0, 0x2D);
        stalkerZebraIvoryClearQueued(arg0);
        if (work->onCeiling == 0 && work->playerDistance < 0x578 && (u16)(work->field_72C - 0x200) > 0xC00 && (u16)(work->field_72A - 0x200) > 0xC00) {
            work2           = (Actor400600Work*)arg0->work;
            work2->state    = 9;
            work2->subState = 0;
        } else {
            work3           = (Actor400600Work*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_80133E38(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    work->field_718++;
    if ((s16)work->field_718 == 0x15) {
        id = 0x40060005;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->obj_594.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if ((s16)work->field_718 == 0x1C) {
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        stalkerZebraIvorySeedTimer(arg0, 0x2D);
        stalkerZebraIvoryClearQueued(arg0);
        if (work->onCeiling == 0 && work->playerDistance < 0x578 && (u16)(work->field_72C - 0x200) > 0xC00 && (u16)(work->field_72A - 0x200) > 0xC00) {
            work2           = (Actor400600Work*)arg0->work;
            work2->state    = 9;
            work2->subState = 0;
        } else {
            work3           = (Actor400600Work*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_80133FC0(Task* arg0)
{
    AnimationPlayRequest     msg;
    GameActorButtonPressHold query;
    Actor400600Work*         work;
    Actor400600Work*         work2;
    Actor400600Work*         work3;
    s32                      base;
    s32                      sound;
    s32                      pan;

    work = (Actor400600Work*)arg0->work;
    if (((GameActor*)gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->work)->mode == GAME_ACTOR_MODE_SCRIPTED || (stalkerZebraIvoryWallDistance(arg0) << 0x10) != 0 || work->playerDistance >= 0x7D0 || (u32)(work->field_72C - 0x200) < 0xC01U) {
        stalkerZebraIvoryDropCapsuleGrid(arg0);
        work2           = (Actor400600Work*)arg0->work;
        work2->state    = 2;
        work2->subState = 0;
        func_actor_400600_80135998(arg0, work->field_752);
        return;
    }
    query.pressCount = 8;
    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &query, 0) != 0) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        if (work->onCeiling == 0) {
            work3           = (Actor400600Work*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
            return;
        }
        work2           = (Actor400600Work*)arg0->work;
        work2->state    = 0xD;
        work2->subState = 0;
        return;
    }
    work->field_73E = work->field_92;
    stalkerZebraIvoryDropCapsuleGrid(arg0);
    work->onCeiling          = 0;
    Gp_StateC08.flags       |= ATTACHMENT_FLAG_EVENT_LOCK;
    work->holding            = 1;
    work->field_9A           = work->field_92;
    msg.source.sets          = D_actor_400600_80151A48;
    msg.blend                = ANIMATION_BLEND_RESET;
    msg.blendFrames          = 0;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    msg.animationId          = 1;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &msg, 0);
    work->obj_4B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work2                = (Actor400600Work*)arg0->work;
    work2->animBlend     = 4;
    work2->animStep      = 0x10;
    work2->animClip      = 5;
    work2->animRequest   = 1;
    work->field_718      = 0;
    work->field_71A      = 0;
    base                 = 0x40060004;
    if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
        base = 0x404A0004;
    }
    sound = base | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->holdLoops = 0;
    work->subState++;
}

static void func_actor_400600_80134218(Task* arg0)
{
    AnimationPlayRequest msg;
    SVECTOR              vec;
    Actor400600Work*     work;
    Actor400600Work*     work2;
    Enemy*               enemy;
    GfxCoord*            coord;
    GfxCoord*            player;
    GfxCoord*            root;
    s32                  id;
    s32                  sound;
    s32                  pan;
    s32                  sound2;
    s32                  pan2;
    s32                  y;
    s32                  ty;

    work               = (Actor400600Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    enemy              = (Enemy*)arg0->spawnArg2.pointer;
    player             = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work->roll        += -(s16)work->roll >> 2;
    coord->coord.t[0] += (player->coord.t[0] - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += (player->coord.t[2] - coord->coord.t[2]) >> 2;
    y                  = coord->coord.t[1];
    ty                 = y + 900;
    coord->coord.t[1]  = y + ((player->coord.t[1] - ty) >> 2);
    work->field_718++;
    if (++work->field_71A == 8) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->field_763 == 1 || work->holdKilledPlayer == 1 || enemy->hp <= 0 || work->holdLoops >= 3) {
        work->field_763 = 0;
        if (work->holdKilledPlayer == 0) {
            msg.source.sets          = D_actor_400600_80151A48;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 8;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            msg.animationId          = 2;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
        }
        work2                = (Actor400600Work*)arg0->work;
        work2->animBlend     = 8;
        work2->animStep      = 0x10;
        work2->animClip      = 6;
        work2->animRequest   = 1;
        work->field_722      = 0;
        work->field_724      = 0;
        work->field_718      = 0;
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->subState++;
        return;
    }
    if ((s16)work->field_718 == 0xD || (s16)work->field_718 == 0x1A) {
        root = &gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords[4];
        Gp_SpawnPadLerp(0xA, 0xC0, 8);
        id = 0x40060009;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0009;
        }
        sound2 = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 1), 0) != 0) {
            work->holdKilledPlayer = 1;
        }
        vec.vx = 0;
        vec.vy = -200;
        vec.vz = 0;
        Gp_SpawnEff(EFFECT_HIT_SPLATTER_SPRAY, root, 0x10100, &vec);
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->field_718 = 0;
        work->holdLoops++;
    }
}

static void func_actor_400600_80134570(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GfxCoord*        coord;
    GfxCoord*        player;
    GfxCoord*        root;
    SVECTOR          vec;
    s32              i;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;
    s16              vy;

    work        = (Actor400600Work*)arg0->work;
    coord       = arg0->extra.tmd->coords;
    player      = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work->roll += -(s16)work->roll >> 2;
    work->field_718++;
    if ((s16)work->field_718 >= 8) {
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->field_722     += 2;
        work->field_724     += work->field_722;
        y                    = coord->coord.t[1] + work->field_724;
        coord->coord.t[1]    = y;
        if (y >= (s16)work->field_9A) {
            coord->coord.t[1]    = (s16)work->field_9A;
            player->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(player);
            if (work->field_76C != 0) {
                vy   = -0x1A4;
                root = arg0->extra.tmd->coords;
                Gp_SpawnEff(gRoomEffectWaterRippleId, root, 0x40, NULL);
                for (i = 0; i < 16; i++) {
                    vec.vx = (u32)rsin(i << 8) >> 3;
                    vec.vy = vy;
                    vec.vz = (u32)rcos(i << 8) >> 3;
                    Gp_SpawnEff(gRoomEffectWaterSprayId, root, 0x01202148, &vec);
                }
            }
            id = 0x40060003;
            if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
                id = 0x404A0003;
            }
            sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            work->obj_4B4.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work2                = (Actor400600Work*)arg0->work;
            work2->animBlend     = 2;
            work2->animClip      = 0x19;
            work2->animStep      = 0x10;
            work2->animRequest   = 1;
            work->subState++;
        }
    }
}

static void func_actor_400600_8013479C(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s16              v;

    work = (Actor400600Work*)arg0->work;
    v    = stalkerZebraIvoryWallDistance(arg0);
    if (v != 0) {
        if ((u16)(v - 0x4E9) >= 0x6D0U) {
            stalkerZebraIvoryDropCapsuleGrid(arg0);
            work3           = (Actor400600Work*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
            return;
        }
        work->field_98 = ((rsin((s16)work->yaw + 0x800) * (v - 0x100)) >> 12) / 20;
        work->field_9C = ((rcos((s16)work->yaw + 0x800) * (v - 0x100)) >> 12) / 20;
    } else {
        work->field_98 = ((rsin((s16)work->yaw + 0x800) * 3000) >> 12) / 20;
        work->field_9C = ((rcos((s16)work->yaw + 0x800) * 3000) >> 12) / 20;
    }
    stalkerZebraIvoryDropCapsuleGrid(arg0);
    work2              = (Actor400600Work*)arg0->work;
    work2->animBlend   = 4;
    work2->animStep    = 0x10;
    work2->animClip    = 0x15;
    work2->animRequest = 1;
    work->field_722    = -0x2A;
    work->field_724    = 0;
    work->field_718    = 0;
    work->subState++;
}

static void func_actor_400600_80134970(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GfxCoord*        coord;
    GfxCoord*        root;
    SVECTOR          vec;
    s32              i;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;
    s16              vy;

    work  = (Actor400600Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_718++;
    if ((s16)work->field_718 < 0x11) {
        func_actor_400600_80136FA8(arg0);
        return;
    }
    if ((s16)work->field_718 == 0x11) {
        work->pendingAction = 0;
        work->holding       = 1;
    }
    coord->coord.t[0] += (s16)work->field_98;
    coord->coord.t[2] += (s16)work->field_9C;
    work->field_722   += 6;
    work->field_724   += work->field_722;
    y                  = coord->coord.t[1] + work->field_724;
    coord->coord.t[1]  = y;
    if (y >= (s16)work->field_92) {
        coord->coord.t[1] = (s16)work->field_92;
        vy                = -0x1A4;
        if (work->field_76C != 0) {
            root = arg0->extra.tmd->coords;
            Gp_SpawnEff(gRoomEffectWaterRippleId, root, 0x40, NULL);
            for (i = 0; i < 16; i++) {
                vec.vx = (u32)rsin(i << 8) >> 3;
                vec.vy = vy;
                vec.vz = (u32)rcos(i << 8) >> 3;
                Gp_SpawnEff(gRoomEffectWaterSprayId, root, 0x01202148, &vec);
            }
        }
        id = 0x40060003;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work2              = (Actor400600Work*)arg0->work;
        work2->animBlend   = 2;
        work2->animClip    = 0x19;
        work2->animStep    = 0x10;
        work2->animRequest = 1;
        work->holding      = 0;
        work->subState++;
    }
}

#include "../../shared/stalker_zebra_ivory_right_itself.inc.c"

static void func_actor_400600_80134E28(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GfxCoord*        coord;
    s32              id;
    s32              sound;
    s32              pan;
    s32              y;

    work  = (Actor400600Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_718++;
    if ((s16)work->field_718 < 0x11) {
        func_actor_400600_80136FA8(arg0);
        return;
    }
    if ((s16)work->field_718 == 0x11) {
        work->pendingAction  = 0;
        work->holding        = 1;
        work->field_73E      = work->field_9A;
        work->obj_4B4.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    /* A separate statement: written inline, fold turns `a - (y + 400)` into
     * `(a - 400) - y`. */
    y                  = coord->coord.t[1] + 0x190;
    coord->coord.t[1] += ((s16)work->field_9A - y) >> 3;
    work->pitch       += (0x800 - (s16)work->pitch) >> 3;
    if ((s16)work->field_9A >= coord->coord.t[1]) {
        id = 0x40060003;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        coord->coord.t[1]    = (s16)work->field_9A;
        work->pitch          = 0;
        work->roll           = 0x800;
        work->yaw           += 0x800;
        stalkerZebraIvoryApplyRotationInline(arg0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        work2              = (Actor400600Work*)arg0->work;
        work2->animBlend   = 2;
        work2->animStep    = 0x10;
        work2->animClip    = 0x19;
        work2->animRequest = 1;
        work->holding      = 0;
        work->onCeiling    = 1;
        work->subState++;
    }
}

static void func_actor_400600_801350F4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GfxCoord*        coord;

    work  = (Actor400600Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_718++;
    if ((s16)work->field_718 < 8) {
        stalkerZebraIvoryReadPartViewXZ(arg0, 3, &work->anchorPos);
        return;
    }
    work->anchorPos.vx += ((s16)work->field_98 - work->anchorPos.vx) >> 2;
    work->anchorPos.vz += ((s16)work->field_9C - work->anchorPos.vz) >> 2;
    stalkerZebraIvoryPinPartXZ(arg0, 3, &work->anchorPos);
    work->field_722   += 2;
    work->field_724   += work->field_722;
    coord->coord.t[1] += work->field_724;
    if ((work->pitch & 0xFFF) != 0x800) {
        work->pitch -= 0x80;
    }
    if ((s16)work->field_92 < coord->coord.t[1]) {
        work->onCeiling   = 0;
        coord->coord.t[0] = (s16)work->field_98;
        coord->coord.t[1] = (s16)work->field_92;
        coord->coord.t[2] = (s16)work->field_9C;
        work->pitch       = 0;
        work->roll        = 0;
        work->yaw        += 0x800;
        stalkerZebraIvoryApplyRotationInline(arg0);
        work2              = (Actor400600Work*)arg0->work;
        work2->animBlend   = 2;
        work2->animStep    = 0x10;
        work2->animClip    = 0x19;
        work2->animRequest = 1;
        stalkerZebraIvoryTickAnimInline(arg0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        work->field_718 = 0;
        work->subState++;
    }
}

static void func_actor_400600_80135450(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u32              sound;
    s32              id;
    s32              pan;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if ((s16)work->field_718 == 0) {
        id = 0x40060003;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->holding = 0;
        work->field_718++;
    }
    if ((stalkerZebraIvoryTakePending(arg0) << 0x10) == 0 && (stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        rnd                   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState       = rnd;
        work->ceilingCooldown = ((rnd >> 0x10) & 0x1F) + 0xD2;
        work2                 = (Actor400600Work*)arg0->work;
        work2->state          = 2;
        work2->subState       = 0;
    }
}

static void func_actor_400600_80135578(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    u32              sound;
    s32              id;
    s32              pan;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if ((s16)work->field_718 == 0) {
        work->holding = 0;
        id            = 0x40060006;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0006;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_718++;
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->pendingAction != 3) {
            work2              = (Actor400600Work*)arg0->work;
            work2->animBlend   = 2;
            work2->animStep    = 0x10;
            work2->animClip    = 0x14;
            work2->animRequest = 1;
            work->subState++;
            return;
        }
        work->pendingAction = 0;
        rnd                 = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState     = rnd;
        work->countdown     = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work3               = (Actor400600Work*)arg0->work;
        work3->state        = 5;
        work3->subState     = 0;
    }
}

/// Spawn the two child models from `D_actor_400600_80151AF8`, parent them to
/// root parts 10 and 7 at +/-0x200 along X, turn each by -/+0x180 from an
/// identity rotation, copy the parent's texture page and CLUT row, and point
/// their light / color matrices at this actor's own.
static void func_actor_400600_801356E0(Task* arg0)
{
    Actor400600Work* work;
    GfxCoord*        coord;
    GfxCoord*        root;
    GfxCoord*        parent;
    GfxCoord*        parent2;
    Task*            task;
    TmdObject*       obj;
    TmdObject*       dst;
    TmdObject*       src;
    MATRIX*          mdst;
    GfxMatrix*       pm;
    GfxMatrix*       pm2;
    GfxMatrix        m;

    root            = arg0->extra.tmd->coords;
    work            = (Actor400600Work*)arg0->work;
    parent          = &root[7];
    parent2         = &root[10];
    task            = Task_SpawnFromTable(D_actor_400600_80151AF8, 0, 0, 0);
    work->field_704 = task;
    if (task != NULL) {
        obj                      = task->extra.tmd;
        coord                    = obj->coords;
        obj->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        coord->coord.t[0]        = 0x200;
        coord->parent            = parent2;
        coord->coord.t[1]        = 0;
        coord->coord.t[2]        = 0;
        pm                       = &m;
        pm->rotationWords.m00M01 = ONE;
        pm->rotationWords.m02M10 = 0;
        pm->rotationWords.m11M12 = ONE;
        pm->rotationWords.m20M21 = 0;
        pm->rotationWords.m22    = ONE;
        RotMatrixY(-0x180, &pm->mat);
        mdst                   = &coord->coord;
        mdst->m[0][0]          = pm->mat.m[0][0];
        mdst->m[0][1]          = pm->mat.m[0][1];
        mdst->m[0][2]          = pm->mat.m[0][2];
        mdst->m[1][0]          = pm->mat.m[1][0];
        mdst->m[1][1]          = pm->mat.m[1][1];
        mdst->m[1][2]          = pm->mat.m[1][2];
        mdst->m[2][0]          = pm->mat.m[2][0];
        mdst->m[2][1]          = pm->mat.m[2][1];
        mdst->m[2][2]          = pm->mat.m[2][2];
        src                    = arg0->extra.tmd;
        dst                    = task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
        obj->lightMtx = &work->matrix_40;
        obj->colorMtx = &work->matrix_20;
    }
    task = work->field_708 = Task_SpawnFromTable(D_actor_400600_80151AF8, 1, 0, 0);
    if (task != NULL) {
        obj                    = task->extra.tmd;
        coord                  = obj->coords;
        obj->flags             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        coord->parent          = parent;
        coord->coord.t[0]      = -0x200;
        coord->coord.t[1]      = 0;
        coord->coord.t[2]      = 0;
        src                    = arg0->extra.tmd;
        dst                    = task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
        pm2                       = &m;
        pm2->rotationWords.m00M01 = ONE;
        pm2->rotationWords.m02M10 = 0;
        pm2->rotationWords.m11M12 = ONE;
        pm2->rotationWords.m20M21 = 0;
        pm2->rotationWords.m22    = ONE;
        RotMatrixY(0x180, &pm2->mat);
        mdst          = &coord->coord;
        mdst->m[0][0] = pm2->mat.m[0][0];
        mdst->m[0][1] = pm2->mat.m[0][1];
        mdst->m[0][2] = pm2->mat.m[0][2];
        mdst->m[1][0] = pm2->mat.m[1][0];
        mdst->m[1][1] = pm2->mat.m[1][1];
        mdst->m[1][2] = pm2->mat.m[1][2];
        mdst->m[2][0] = pm2->mat.m[2][0];
        mdst->m[2][1] = pm2->mat.m[2][1];
        mdst->m[2][2] = pm2->mat.m[2][2];
        obj->lightMtx = &work->matrix_40;
        obj->colorMtx = &work->matrix_20;
    }
}

/// Animation state 2: the landing slam. Same two sound/tracking windows as
/// `stalkerZebraIvoryStepClip4`, plus the dust ring each window spawns when
/// `field_76C` says the actor is over ground.
static void func_actor_400600_80135998(Task* arg0, s16 arg1)
{
    Actor400600Work* work;
    GfxCoord*        coord;
    /* The first window starts at frame 0, written as the literal both of its
     * tests fold against; the wider first temp is what keeps each zero arm a
     * fresh constant instead of a copy of the previous one. */
    u32     tmp0;
    u8      tmp1;
    u8      tmp2;
    u8      end0;
    u8      start1;
    u8      end1;
    s32     id;
    u32     sound;
    u32     voice;
    s32     pan;
    SVECTOR vec;

    work  = (Actor400600Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->animClip != 2) {
        work->animStep    = 0x10;
        work->animClip    = 2;
        work->animRequest = 2;
        stalkerZebraIvoryTickAnimInline(arg0);
    }
    if (((Actor400600Work*)arg0->work)->animStep == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((Actor400600Work*)arg0->work)->animStep) >> 4;
    }
    end0 = tmp0;
    if (((Actor400600Work*)arg0->work)->animStep == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((Actor400600Work*)arg0->work)->animStep) >> 4;
    }
    start1 = tmp1;
    if (((Actor400600Work*)arg0->work)->animStep == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((Actor400600Work*)arg0->work)->animStep) >> 4;
    }
    end1 = tmp2;
    if ((func_actor_400600_8013892C(arg0) << 0x10) != 0) {
        work->animFrame = 0;
        work->animStep  = arg1;
    }
    if (work->animFrame == 0) {
        stalkerZebraIvoryReadPartViewXZ(arg0, 0xB, &work->anchorPos);
        id = 0x40060001;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0001;
        }
        /* `voice` is a plain copy that the compiler propagates away; writing
         * `sound = id | sound` instead swaps the operands of the `or`. */
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        if (work->field_76C != 0) {
            vec.vx = 0;
            vec.vy = -0x1A4;
            vec.vz = 0;
            Gp_SpawnEff(gRoomEffectWaterRippleId, coord, 0x40, &vec);
        }
    }
    if (work->animFrame == start1) {
        stalkerZebraIvoryReadPartViewXZ(arg0, 8, &work->anchorPos);
        id = 0x40060002;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0002;
        }
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        if (work->field_76C != 0) {
            vec.vx = 0;
            vec.vy = -0x1A4;
            vec.vz = 0;
            Gp_SpawnEff(gRoomEffectWaterRippleId, coord, 0x40, &vec);
        }
    }
    if (work->animFrame >= 0 && work->animFrame <= end0) {
        stalkerZebraIvoryPinPartXZ(arg0, 0xB, &work->anchorPos);
        work->field_734 = 8;
    }
    if (work->animFrame >= start1 && work->animFrame <= end1) {
        stalkerZebraIvoryPinPartXZ(arg0, 8, &work->anchorPos);
        work->field_734 = 0xB;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/stalker_zebra_ivory_step_clip4.inc.c"

/// Drives the two sound/tracking windows of the current animation. The window
/// bounds are frame counts derived from the playback rate (`animStep`), each
/// read back through `arg0->work` rather than the cached `work` pointer.
static void func_actor_400600_801361AC(Task* arg0)
{
    Actor400600Work* work;
    GfxCoord*        coord;
    /* The bounds are computed into their own temporaries first; a `u8` temp is
     * what keeps the zero arm of each test out of the surrounding block. */
    u8  tmp0;
    u8  tmp1;
    u8  tmp2;
    u8  end0;
    u8  start1;
    u8  end1;
    s32 start0;
    u32 sound;
    s32 pan;

    work  = (Actor400600Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->animClip != 2) {
        work->animStep    = 0x10;
        work->animClip    = 2;
        work->animRequest = 2;
        stalkerZebraIvoryTickAnimInline(arg0);
    }
    if (((Actor400600Work*)arg0->work)->animStep == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((Actor400600Work*)arg0->work)->animStep) >> 4;
    }
    end0 = tmp0;
    if (((Actor400600Work*)arg0->work)->animStep == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((Actor400600Work*)arg0->work)->animStep) >> 4;
    }
    start1 = tmp1;
    if (((Actor400600Work*)arg0->work)->animStep == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((Actor400600Work*)arg0->work)->animStep) >> 4;
    }
    end1 = tmp2;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->animFrame = 0;
    }
    /* The first window starts at frame 0, and the original compares against it
     * in a register: a literal 0 is folded into `$zero` by CSE. */
    SOFT_MOVE_ZERO(start0);
    if (work->animFrame == start0) {
        func_actor_400600_80139F4C(arg0, 0xB, &work->anchorPos);
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A000A;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->animFrame == start1) {
        func_actor_400600_80139F4C(arg0, 8, &work->anchorPos);
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A000B;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->animFrame >= start0 && work->animFrame <= end0) {
        func_actor_400600_80139FE0(arg0, 0xB, &work->anchorPos);
        work->field_734 = 8;
    }
    if (work->animFrame >= start1 && work->animFrame <= end1) {
        func_actor_400600_80139FE0(arg0, 8, &work->anchorPos);
        work->field_734 = 0xB;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_400600_80136558(Task* arg0)
{
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TmdObject*       model = arg0->extra.tmd;
    Enemy*           enemy = (Enemy*)arg0->spawnArg2.pointer;
    s16              count;

    if (work->cloaked == 0 && work->cloakFading == 1) {
        work->field_73A = (u16)work->field_73A + ((0xFF - work->field_73A) >> 5);
        work->field_740++;
        if (work->field_740 >= 0x20) {
            model->flags &= ~TMD_OBJECT_SEMI_TRANS;
            func_actor_400600_801387DC(arg0, -1);
            work->field_740   = 0;
            work->cloakFading = 0;
        }
    } else if (work->cloaked == 1 && work->cloakFading == 1) {
        work->field_73A = (u16)work->field_73A + (-work->field_73A >> 3);
        count           = work->field_740 + 1;
        work->field_740 = count;
        if (count >= 0x12) {
            if (work->field_75A != 0) {
                enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
            } else {
                enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
            }
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            func_actor_400600_801387DC(arg0, -1);
            work->field_740   = 0;
            work->cloakFading = 0;
            work->field_73A   = 0;
        }
    }
}

static void func_actor_400600_80136670(Task* arg0)
{
    Actor400600Work* work;
    GfxCoord*        coord;
    GfxCoord*        player;
    GameActor*       actor;
    Task*            slot;
    SVECTOR          v;
    s16              a;
    s16              b;

    work              = (Actor400600Work*)arg0->work;
    coord             = arg0->extra.tmd->coords;
    slot              = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work->field_70.vx = coord->coord.t[0];
    work->field_70.vy = coord->coord.t[1];
    work->field_70.vz = coord->coord.t[2];
    if (slot == NULL) {
        return;
    }
    player = slot->extra.tmd->coords;
    actor  = slot->work;
    if (work->field_76E != 0) {
        a = func_actor_400600_8013886C(arg0);
        b = func_actor_400600_8013886C(slot);
        if (a == 2 && (b == 1 || b == 6)) {
            work->field_A8.vx = 0x1194;
            work->field_A8.vy = 0;
            work->field_A8.vz = 0;
        } else if (a == 3 && ((b >= 1 && b <= 2) || b == 6)) {
            work->field_A8.vx = 0x1194;
            work->field_A8.vy = 0;
            work->field_A8.vz = -0x1194;
        } else if (a == 4 && ((b >= 1 && b <= 3) || b == 6)) {
            work->field_A8.vx = -0x125C;
            work->field_A8.vy = 0;
            work->field_A8.vz = -0x1194;
        } else if (a == 5 && ((b >= 1 && b <= 4) || b == 6)) {
            work->field_A8.vx = -0x1194;
            work->field_A8.vy = 0;
            work->field_A8.vz = 0x1194;
        } else if (b == 1 && a == 6) {
            work->field_A8.vx = 0;
            work->field_A8.vy = 0;
            work->field_A8.vz = 0;
        } else if (b != 1 && a == 1) {
            work->field_A8.vx = 0x1194;
            work->field_A8.vy = 0;
            work->field_A8.vz = 0;
        } else {
            work->field_A8.vx = player->coord.t[0];
            work->field_A8.vy = player->coord.t[1];
            work->field_A8.vz = player->coord.t[2];
        }
    } else {
        work->field_A8.vx = player->coord.t[0];
        work->field_A8.vy = player->coord.t[1];
        work->field_A8.vz = player->coord.t[2];
    }
    v.vx                 = work->field_A8.vx - coord->coord.t[0];
    v.vy                 = work->field_A8.vy - coord->coord.t[1];
    v.vz                 = work->field_A8.vz - coord->coord.t[2];
    work->playerDistance = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
    VectorNormalSS(&v, &v);
    work->field_72C = (ratan2(v.vx, v.vz) - work->yaw) & 0xFFF;
    work->field_72A = (ratan2(-v.vx, -v.vz) - actor->rotation.vy) & 0xFFF;
}

static const TaskFuncTable3 D_actor_400600_80131F34 = { {
    func_actor_400600_8013BA6C,
    func_actor_400600_8013BAEC,
    func_actor_400600_8013BB88,
} };

static const TaskFuncTable8 D_actor_400600_80131F40 = { {
    func_actor_400600_8013BCD8,
    func_actor_400600_8013BD54,
    func_actor_400600_8013BDF0,
    func_actor_400600_8013BE58,
    func_actor_400600_80133FC0,
    func_actor_400600_80134218,
    func_actor_400600_80134570,
    stalkerZebraIvoryReleaseHold,
} };

static const TaskFuncTable4 D_actor_400600_80131F60 = { {
    func_actor_400600_8013BF48,
    func_actor_400600_8013479C,
    func_actor_400600_80134970,
    stalkerZebraIvoryWaitClip,
} };

static const TaskFuncTable3 D_actor_400600_80131F70 = { {
    func_actor_400600_8013C038,
    func_actor_400600_80134E28,
    func_actor_400600_8013C074,
} };

static const TaskFuncTable4 D_actor_400600_80131F7C = { {
    func_actor_400600_8013C104,
    func_actor_400600_8013C124,
    func_actor_400600_801350F4,
    func_actor_400600_80135450,
} };

static const TaskFuncTable4 D_actor_400600_80131F8C = { {
    func_actor_400600_8013C1C0,
    func_actor_400600_8013C238,
    func_actor_400600_80135578,
    stalkerZebraIvoryWaitClipThenRest,
} };

static const TaskFuncTable3 gStalkerZebraIvorySubStates = { {
    func_actor_400600_8013C394,
    func_actor_400600_8013C410,
    func_actor_400600_8013C4AC,
} };

static void func_actor_400600_80136968(Task* arg0)
{
    SVECTOR             push;
    SVECTOR             pos;
    WorldCollisionDelta delta;
    GfxCoord*           eff;
    s16                 maxX;
    s16                 maxZ;
    s16                 stepX;
    s16                 stepZ;
    u8                  blocked;
    Actor400600Work*    work;
    Enemy*              enemy;
    GfxCoord*           coord;
    s16                 amount;
    s32                 dmg;
    s32                 tmp;
    s16                 tick;
    s32                 i;

    maxX               = 0;
    maxZ               = 0;
    stepX              = 0;
    stepZ              = 0;
    blocked            = 0;
    coord              = arg0->extra.tmd->coords;
    work               = (Actor400600Work*)arg0->work;
    enemy              = (Enemy*)arg0->spawnArg2.pointer;
    work->pendingArmed = 0;
    eff                = &coord[3];

    for (i = 0; i < 8; i++) {
        switch (work->rec_4D4[i].key.value & 0xFFFF0000) {
            case 0x10000:
            case 0x30000:
                pos.vx = coord->workm.t[0];
                pos.vy = coord->workm.t[1];
                pos.vz = coord->workm.t[2];
                func_actor_400600_8013C6B0(&pos, &work->rec_4D4[i], &push);
                if (ABS(maxX) < ABS(push.vx)) {
                    maxX = push.vx;
                }
                if (ABS(maxZ) < ABS(push.vz)) {
                    maxZ = push.vz;
                }
                break;
            case 0x20000:
                if (work->field_74A == 0) {
                    work->pendingArmed = 1;
                    dmg                = Gp_ComputeDamage(work->rec_4D4[i].key.value, work->playerDistance, 0, 0);
                    amount             = dmg;
                    work->field_74A    = Gp_GetIdParam2(work->rec_4D4[i].key.value);
                    if (Gp_RollEnemyChance(enemy, work->rec_4D4[i].key.value, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                    func_800E2C78(enemy, work->rec_4D4[i].key.value, amount, 0);
                    func_800DA6E8(&enemy->node, amount, 0);
                    enemy->hp -= amount;
                    if (enemy->hp < 0) {
                        enemy->hp = 0;
                    }
                    if ((work->rec_4D4[i].key.value & 0x7F) == 0xE) {
                        if (!(work->rec_4D4[i].key.value & 0x8000)) {
                            work->field_75A = 0x258;
                        }
                    } else {
                        func_800FDB18(Gp_GetIdParam1(work->rec_4D4[i].key.value) & 0xFFFF,
                                      &arg0->extra.tmd->coords[4], NULL, &work->eff_6FC);
                    }
                    if (amount >= 0x64) {
                        work->pendingAction = 2;
                    } else {
                        work->pendingAction = 1;
                    }
                    switch (Gp_GetIdParam0(work->rec_4D4[i].key.value) & 0xFFFF) {
                        case 0:
                            break;
                        case 1:
                            Gp_SetObjFlag1(enemy);
                            break;
                        case 2:
                            Gp_SetObjFlag2(enemy, work->rec_4D4[i].key.value, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4(enemy, work->rec_4D4[i].key.value, 0);
                            break;
                        case 4:
                            work->pendingAction = 4;
                            break;
                        case 5:
                            work->pendingAction = 2;
                            break;
                        case 6:
                            work->pendingAction = 4;
                            break;
                        case 7:
                            work->pendingAction = 2;
                            break;
                        case 8:
                            work->pendingAction = 3;
                            break;
                        case 9:
                            work->pendingAction = 3;
                            break;
                    }
                } else if ((Gp_GetIdParam1(work->rec_4D4[i].key.value) & 0xFFFF) == 0xD) {
                    func_800FDB18(0xD, &arg0->extra.tmd->coords[1], NULL, &work->eff_6FC);
                }
                break;
        }
    }

    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->pendingAction   = 5;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->pendingAction   = 3;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        work->field_760 = 1;
        tmp             = Gp_TickObjFlag4(enemy);
        tick            = tmp;
        if (tick != 0) {
            enemy->hp -= tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->pendingArmed  = 1;
            work->pendingAction = 2;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }

    switch (func_800E0C10(work->rec_4D4, &delta, 8, NULL)) {
        case 0:
            break;
        case 1:
            stepZ = delta.fixed.vz.halves.integer;
            stepX = delta.fixed.vx.word >> 16;
            if (delta.fixed.vx.word & 0xFFFF) {
                if (delta.fixed.vx.word > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.fixed.vz.word & 0xFFFF) {
                if (delta.fixed.vz.word > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case 2:
            coord->coord.t[0]   = work->field_70.vx;
            coord->coord.t[2]   = work->field_70.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            blocked             = 1;
            break;
    }

    Gp_ClearRec18Occupied(work->rec_4D4);
    if (work->field_74A > 0) {
        work->field_74A--;
    } else {
        work->field_74A = 0;
    }
    if (blocked == 0) {
        work->anchorPos.vx += (s16)func_actor_400600_8013C7E8(stepX, maxX);
        work->anchorPos.vz += (s16)func_actor_400600_8013C7E8(stepZ, maxZ);
        coord->coord.t[0]  += (s16)func_actor_400600_8013C7E8(stepX, maxX);
        coord->coord.t[2]  += (s16)func_actor_400600_8013C7E8(stepZ, maxZ);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->field_75A != 0) {
        work->field_75A--;
        if ((work->field_75A & 7) == 1) {
            Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, eff, 0x10200, NULL);
        }
    }
}

static s32 func_actor_400600_80136FA8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work = (Actor400600Work*)arg0->work;
    if (work->pendingArmed != 1) {
        return 0;
    }
    if (work->onCeiling == 0) {
        switch (work->pendingAction) {
            case 1:
                work2           = (Actor400600Work*)arg0->work;
                work2->state    = 3;
                work2->subState = 0;
                break;
            case 2:
                work2           = (Actor400600Work*)arg0->work;
                work2->state    = 4;
                work2->subState = 0;
                break;
            case 3:
                work2           = (Actor400600Work*)arg0->work;
                work2->state    = 5;
                work2->subState = 0;
                break;
            case 4:
                work2           = (Actor400600Work*)arg0->work;
                work2->state    = 4;
                work2->subState = 0;
                break;
            case 5:
                work2           = (Actor400600Work*)arg0->work;
                work2->state    = 0xF;
                work2->subState = 0;
                break;
        }
        work->pendingAction = 0;
    } else {
        switch (work->pendingAction) {
            case 1:
                work2               = (Actor400600Work*)arg0->work;
                work2->state        = 3;
                work2->subState     = 0;
                work->pendingAction = 0;
                break;
            case 2:
                work2               = (Actor400600Work*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = 0;
                break;
            case 3:
                work2           = (Actor400600Work*)arg0->work;
                work2->state    = 0xE;
                work2->subState = 0;
                break;
            case 4:
                work2               = (Actor400600Work*)arg0->work;
                work2->state        = 4;
                work2->subState     = 0;
                work->pendingAction = 0;
                break;
            case 5:
                work2               = (Actor400600Work*)arg0->work;
                work2->state        = 0xE;
                work2->subState     = 0;
                work->pendingAction = 0;
                break;
        }
    }
    work->field_76D = 0;
    stalkerZebraIvoryDropCapsuleGrid(arg0);
    return 1;
}

#include "../../shared/stalker_zebra_ivory_take_pending.inc.c"

static void func_actor_400600_80137240(Task* arg0)
{
    EffectWork* eff;
    EffectWork* eff2;
    EffectWork* eff3;
    EffectWork* eff4;
    TmdObject*  dst;
    TmdObject*  dst2;
    TmdObject*  dst3;
    TmdObject*  dst4;
    TmdObject*  src;
    TmdObject*  src2;
    TmdObject*  src3;
    TmdObject*  src4;

    D_800678F0[0] = &_gActor400600ZebraStalkerBurstHead;
    eff           = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[4], 0x200, NULL);
    if (eff != NULL) {
        src                    = arg0->extra.tmd;
        dst                    = eff->task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    D_800678F0[0] = &_gActor400600StalkerEffect;
    eff2          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[2], 0x200, NULL);
    if (eff2 != NULL) {
        src2                    = arg0->extra.tmd;
        dst2                    = eff2->task->extra.tmd;
        dst2->texturePageOffset = src2->texturePageOffset;
        dst2->clutRowOffset     = src2->clutRowOffset;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    D_800678F0[0] = &_gActor400600StalkerBurstHandLeft;
    eff3          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[16], 0x200, NULL);
    if (eff3 != NULL) {
        src3                    = arg0->extra.tmd;
        dst3                    = eff3->task->extra.tmd;
        dst3->texturePageOffset = src3->texturePageOffset;
        dst3->clutRowOffset     = src3->clutRowOffset;
        if (dst3->buffer != NULL) {
            tmdProcessStream(dst3);
            tmdProcessStream(dst3);
        }
    }
    D_800678F0[0] = &_gActor400600StalkerBurstFootRight;
    eff4          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[10], 0x200, NULL);
    if (eff4 != NULL) {
        src4                    = arg0->extra.tmd;
        dst4                    = eff4->task->extra.tmd;
        dst4->texturePageOffset = src4->texturePageOffset;
        dst4->clutRowOffset     = src4->clutRowOffset;
        if (dst4->buffer != NULL) {
            tmdProcessStream(dst4);
            tmdProcessStream(dst4);
        }
    }
    Gp_SpawnEff(EFFECT_030, &arg0->extra.tmd->coords[1], 0x200, NULL);
    Gp_SpawnEff(EFFECT_030, &arg0->extra.tmd->coords[2], 0x200, NULL);
    Gp_SpawnEff(EFFECT_030, &arg0->extra.tmd->coords[3], 0x200, NULL);
}

/* `n` is one variable carrying first `out.vz` and then the speed: the reuse is
 * an anti-dependence that keeps the `field_4` store ahead of `li 10` in sched1,
 * so the tail matches case 2's and jump2 cross-jumps them. Each branch keeps
 * its own matrix pointer so local-alloc puts it in `$s0` ahead of `work`. */
static void func_actor_400600_80137498(Task* arg0, s16 arg1)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    SVECTOR          v;
    SVECTOR          out;
    GfxMatrix        rot;
    s16              n;

    work->distanceMode = arg1;
    switch (arg1) {
        case 0:
            if (work->onCeiling == 0) {
                GfxMatrix* m = &rot;

                v.vx                     = work->field_A8.vx - arg0->extra.tmd->coords->coord.t[0];
                v.vy                     = work->field_A8.vy - arg0->extra.tmd->coords->coord.t[1] - 0x384;
                v.vz                     = work->field_A8.vz - arg0->extra.tmd->coords->coord.t[2];
                rot.rotationWords.m00M01 = ONE;
                rot.rotationWords.m02M10 = 0;
                m->rotationWords.m11M12  = ONE;
                rot.rotationWords.m20M21 = 0;
                m->rotationWords.m22     = ONE;
                rot.mat.t[0]             = 0;
                rot.mat.t[1]             = 0;
                rot.mat.t[2]             = 0;
                RotMatrixY(-(s16)work->yaw, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            } else {
                GfxMatrix* m = &rot;

                v.vx                     = work->field_A8.vx - arg0->extra.tmd->coords->coord.t[0];
                v.vy                     = work->field_A8.vy - arg0->extra.tmd->coords->coord.t[1] - 0x640;
                v.vz                     = work->field_A8.vz - arg0->extra.tmd->coords->coord.t[2];
                rot.rotationWords.m00M01 = ONE;
                rot.rotationWords.m02M10 = 0;
                m->rotationWords.m11M12  = ONE;
                rot.rotationWords.m20M21 = 0;
                m->rotationWords.m22     = ONE;
                rot.mat.t[0]             = 0;
                rot.mat.t[1]             = 0;
                rot.mat.t[2]             = 0;
                RotMatrixY(-(s16)work->yaw, &m->mat);
                RotMatrixZ(-(s16)work->roll, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            }
            work->capsule.ends[0].vx = out.vx;
            work->capsule.ends[0].vy = out.vy;
            n                        = out.vz;
            work->capsule.ends[0].vz = n;
            n                        = 0xA;
            work->capsule.end0Radius = n;
            work->capsule.end1Radius = n;
            break;
        case 1:
            work->capsule.ends[0].vx = 0;
            work->capsule.ends[0].vy = 0x190;
            work->capsule.ends[0].vz = -0xBB8;
            work->capsule.end0Radius = 0x50;
            work->capsule.end1Radius = 0x50;
            break;
        case 2:
            work->capsule.ends[0].vx = 0;
            work->capsule.ends[0].vy = -0xBB8;
            work->capsule.ends[0].vz = 0;
            work->capsule.end0Radius = 0xA;
            work->capsule.end1Radius = 0xA;
            break;
    }
    work->capsule.ends[1].vx = 0;
    work->capsule.ends[1].vy = 0x64;
    work->capsule.ends[1].vz = 0;
    Gp_ClearRec18Occupied(work->capsuleContacts);
    work->capsuleBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
}

#include "../../shared/stalker_zebra_ivory_wall_distance.inc.c"

/// Replaces the rotation of `coord` with the one `RotMatrixY` applies to an
/// identity matrix for `angle`.
static inline void _actor400600SetCoordRotation(GfxCoord* coord, s16 angle)
{
    GfxMatrix  rot;
    GfxMatrix* m;
    MATRIX*    dst;

    rot.rotationWords.m00M01 = ONE;
    rot.rotationWords.m02M10 = 0;
    m                        = &rot;
    m->rotationWords.m11M12  = ONE;
    rot.rotationWords.m20M21 = 0;
    m->rotationWords.m22     = ONE;
    RotMatrixY(angle, &m->mat);
    dst          = &coord->coord;
    dst->m[0][0] = rot.mat.m[0][0];
    dst->m[0][1] = rot.mat.m[0][1];
    dst->m[0][2] = rot.mat.m[0][2];
    dst->m[1][0] = rot.mat.m[1][0];
    dst->m[1][1] = rot.mat.m[1][1];
    dst->m[1][2] = rot.mat.m[1][2];
    dst->m[2][0] = rot.mat.m[2][0];
    dst->m[2][1] = rot.mat.m[2][1];
    dst->m[2][2] = rot.mat.m[2][2];
}

/// Sets the root rotation of the child task held in work field `child`, if it
/// has been spawned.
#define _ACTOR400600_ROTATE_CHILD(task, child, angle)                         \
    do {                                                                      \
        Task* _child = ((Actor400600Work*)(task)->work)->child;               \
                                                                              \
        if (_child != NULL) {                                                 \
            _actor400600SetCoordRotation(_child->extra.tmd->coords, (angle)); \
        }                                                                     \
    } while (0)

static void func_actor_400600_80137840(Task* arg0)
{
    Actor400600Work* work;
    s32              angle;

    work = (Actor400600Work*)arg0->work;
    if ((u8)work->rightArmOut != 0) {
        work->field_74E += (0x380 - work->field_74E) >> 2;
        _ACTOR400600_ROTATE_CHILD(arg0, field_708, work->field_74E);
    } else {
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_74E     += -work->field_74E >> 3;
        angle                = work->field_74E;
        _ACTOR400600_ROTATE_CHILD(arg0, field_708, angle);
    }
    if ((u8)work->leftArmOut != 0) {
        work->field_74C += (0x380 - work->field_74C) >> 2;
        _ACTOR400600_ROTATE_CHILD(arg0, field_704, -work->field_74C);
    } else {
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_74C     += -work->field_74C >> 3;
        _ACTOR400600_ROTATE_CHILD(arg0, field_704, -work->field_74C);
    }
    if ((u32)(work->state - 6) >= 2U) {
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
}

static s32 func_actor_400600_80137AF0(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    TmdObject*       model2;
    Enemy*           enemy;

    work = (Actor400600Work*)arg0->work;
    if (work->field_758 > 0) {
        work->field_758--;
        return 0;
    }
    if ((u32)(work->field_72C - 0x400) >= 0x801U && (u32)(work->field_72A - 0x300) >= 0xA01U) {
        if (work->playerDistance < 0xBB8) {
            model = arg0->extra.tmd;
            if (work->cloaked != 1) {
                work->cloaked     = 1;
                work->cloakFading = 1;
                work->field_740   = 0;
                model->flags     |= TMD_OBJECT_SEMI_TRANS;
                Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
                func_actor_400600_801387DC(arg0, 2);
            }
            work2           = (Actor400600Work*)arg0->work;
            work2->state    = 0x11;
            work2->subState = 0;
            return 1;
        }
    } else {
        work = (Actor400600Work*)arg0->work;
    }
    model2 = arg0->extra.tmd;
    enemy  = (Enemy*)arg0->spawnArg2.pointer;
    if (work->cloaked != 0) {
        work->cloaked     = 0;
        work->cloakFading = 1;
        work->field_740   = 0;
        model2->flags     = (model2->flags | TMD_OBJECT_SEMI_TRANS) & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
        func_actor_400600_801387DC(arg0, 0);
    }
    return 0;
}

static s32 func_actor_400600_80137C34(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    GfxCoord*        coord;
    u32              rnd1;
    u32              rnd2;
    u32              rnd;
    s32              dist;
    s16              y;

    work            = (Actor400600Work*)arg0->work;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    coord           = arg0->extra.tmd->coords;
    rnd             = gRandomLcgState >> 0x10;
    switch (work->field_76D) {
        case 0:
            if (work->timer != 0) {
                return 0;
            }
            if (work->onCeiling == 0) {
                if ((rnd & 0xF) == 0) {
                    if (!(arg0->spawnArg1.value & 1) && work->ceilingCooldown == 0) {
                        func_actor_400600_80137498(arg0, 2);
                        work->field_76D = 1;
                    }
                    return 0;
                }
                if ((rnd & 7) == 1 || (rnd & 7) == 2) {
                    if (work->playerDistance < 0x5DC && (u32)(work->field_72C - 0x200) >= 0xC01U) {
                        work2           = (Actor400600Work*)arg0->work;
                        work2->state    = 8;
                        work2->subState = 0;
                        return 1;
                    }
                } else if (work->playerDistance < 0x5DC) {
                    if ((s16)work->field_72C < 0x400) {
                        work2           = (Actor400600Work*)arg0->work;
                        work2->state    = 6;
                        work2->subState = 0;
                        return 1;
                    }
                    if ((s16)work->field_72C > 0xC00) {
                        work2           = (Actor400600Work*)arg0->work;
                        work2->state    = 7;
                        work2->subState = 0;
                        return 1;
                    }
                }
            } else if ((rnd & 7) == 0) {
                if (work->ceilingCooldown == 0) {
                    work2           = (Actor400600Work*)arg0->work;
                    work2->state    = 0x10;
                    work2->subState = 0;
                    return 1;
                }
            } else if ((rnd & 0xF) == 1) {
                if (work->ceilingCooldown == 0) {
                    work2           = (Actor400600Work*)arg0->work;
                    work2->state    = 0xD;
                    work2->subState = 0;
                    return 1;
                }
            } else {
                rnd1                                  = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                rnd2                                  = (rnd1 * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState                       = rnd2;
                ((Actor400600Work*)arg0->work)->timer = 0x3C + ((rnd1 >> 0x10) & 0x3F) + ((rnd2 >> 0x10) & 0xF);
                return 0;
            }
            return 0;
        case 1:
            work->field_76D = 0;
            dist            = stalkerZebraIvoryWallDistance(arg0);
            if ((u16)(dist - 0x7D1) < 0x3E8) {
                if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 8, 0, 0)) {
                    y = -0x9C4;
                } else {
                    y = coord->coord.t[1] - dist;
                }
                work->field_9A                                     = y;
                work3                                              = (Actor400600Work*)arg0->work;
                work3->state                                       = 0xC;
                work3->subState                                    = 0;
                ((Actor400600Work*)arg0->work)->capsuleBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                return 1;
            }
            ((Actor400600Work*)arg0->work)->capsuleBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            return 0;
    }
    return 0;
}

static const TaskFuncTable3 D_actor_400600_80132030 = { {
    func_actor_400600_8013C874,
    func_actor_400600_8013C940,
    func_actor_400600_8013C9DC,
} };

static void func_actor_400600_80137EF0(Task* arg0)
{
    TmdObject*       model = arg0->extra.tmd;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TaskFuncTable3   fns   = D_actor_400600_80132030;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            stalkerZebraIvoryTickAnimInline(arg0);
            fns.funcs[(s16)work->state](arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryApplyRotationInline(arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->capsuleContacts);
            actorUpdateModelColor(arg0);
            func_actor_400600_80138224(arg0, 0, work->field_73A);
            break;
    }
}

/// Refreshes the parts listed in `D_actor_400600_80151B88`, projects each into
/// view space with `arg1` as the Y, and passes nine fixed pairs of the resulting
/// points to `func_actor_400600_801383E4` along with `arg2` (the fade level at
/// every call site).
static void func_actor_400600_80138224(Task* arg0, s16 arg1, u8 arg2)
{
    MATRIX    mtx;
    SVECTOR   pts[11];
    GfxCoord* coord;
    GfxCoord* root;
    s32       i;

    root                       = arg0->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    root->composeStamp         = GRAPHICS_COORD_DIRTY;
    for (i = 0; D_actor_400600_80151B88[i] != -1; i++) {
        coord               = &arg0->extra.tmd->coords[D_actor_400600_80151B88[i]];
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &mtx);
        pts[i].vx = mtx.t[0];
        pts[i].vy = arg1;
        pts[i].vz = mtx.t[2];
    }
    func_actor_400600_801383E4(&pts[1], &pts[5], 0x80, arg2);
    func_actor_400600_801383E4(&pts[5], &pts[6], 0x80, arg2);
    func_actor_400600_801383E4(&pts[1], &pts[3], 0x80, arg2);
    func_actor_400600_801383E4(&pts[3], &pts[4], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[2], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[7], 0x80, arg2);
    func_actor_400600_801383E4(&pts[7], &pts[8], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[9], 0x80, arg2);
    func_actor_400600_801383E4(&pts[9], &pts[10], 0x80, arg2);
}

/// Draws a semi-transparent textured quad along the segment from `arg0` to
/// `arg1`: widened by `width` either side, pulled in by half its length at both
/// ends, and shaded grey `shade`. The per-model counterpart of
/// `ActorsShared80163354`, taking view-space points instead of joints.
static void func_actor_400600_801383E4(SVECTOR* arg0, SVECTOR* arg1, s16 width, u8 shade)
{
    Actor400600QuadScratch* s;
    s16                     angle;
    s32                     halfX;
    s32                     halfZ;
    POLY_FT4*               poly;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    s                          = (Actor400600QuadScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor400600QuadScratch));
    Gp_UpdateCoord(&gGfxViewCoord);
    angle         = ratan2(arg1->vx - arg0->vx, arg1->vz - arg0->vz);
    halfX         = (arg0->vx - arg1->vx) / 2;
    halfZ         = (arg0->vz - arg1->vz) / 2;
    s->corner0.vx = halfX + (arg0->vx - ((s32)(rcos(angle) * width) >> 0xC));
    s->corner0.vy = arg0->vy;
    s->corner0.vz = halfZ + (arg0->vz + ((s32)(rsin(angle) * width) >> 0xC));
    s->corner1.vx = halfX + (arg0->vx + ((s32)(rcos(angle) * width) >> 0xC));
    s->corner1.vy = arg0->vy;
    s->corner1.vz = halfZ + (arg0->vz - ((s32)(rsin(angle) * width) >> 0xC));
    s->corner2.vx = (arg1->vx - ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
    s->corner2.vy = arg1->vy;
    s->corner2.vz = (arg1->vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
    s->corner3.vx = (arg1->vx + ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
    s->corner3.vy = arg1->vy;
    s->corner3.vz = (arg1->vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                             &s->screen2, &s->screen3, &s->perspective, &s->flags);
    if (s->flags >= 0) {
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 9);
        poly->code                     = 0x2E;
        GPU_PRIMITIVE_XY_WORD(poly, 0) = s->screen0;
        GPU_PRIMITIVE_XY_WORD(poly, 1) = s->screen1;
        GPU_PRIMITIVE_XY_WORD(poly, 2) = s->screen2;
        GPU_PRIMITIVE_XY_WORD(poly, 3) = s->screen3;
        setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
        poly->tpage = 0x48;
        poly->clut  = 0x4283;
        setRGB0(poly, shade, shade, shade);
        addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor400600QuadScratch));
}

/// Copies this actor's model flags onto both child tasks' models and, for a
/// non-negative `arg1`, sets the children's light mode to it.
static void func_actor_400600_801387DC(Task* arg0, s32 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    Task*            child;

    work  = (Actor400600Work*)arg0->work;
    model = arg0->extra.tmd;
    if (work->field_704 != NULL) {
        child                   = work->field_704;
        child->extra.tmd->flags = model->flags;
        if (arg1 >= 0) {
            Gp_SetLightMode(child->spawnArg2.pointer, arg1);
        }
    }
    if (work->field_708 != NULL) {
        child                   = work->field_708;
        child->extra.tmd->flags = model->flags;
        if (arg1 >= 0) {
            Gp_SetLightMode(child->spawnArg2.pointer, arg1);
        }
    }
}

/// Returns the id of the first `D_actor_400600_80151B40` zone containing the
/// actor's world XZ position, or 0 if none does.
static s32 func_actor_400600_8013886C(Task* arg0)
{
    GfxCoord*  coord;
    ActorZone* zone;
    s16        x;
    s16        z;

    coord = arg0->extra.tmd->coords;
    x     = (u16)coord->coord.t[0];
    z     = (u16)coord->coord.t[2];
    for (zone = D_actor_400600_80151B40; zone->id != -1; zone++) {
        if (zone->x <= x && x <= zone->x + zone->w && zone->z <= z && z <= zone->z + zone->h) {
            return zone->id;
        }
    }
    return 0;
}

static s32 func_actor_400600_8013892C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if ((work->previousAnimationFlags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->previousAnimationFlags & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (work->previousAnimationFlags & ANIMATION_SLOT_SETTLED)) {
        return 1;
    }
    return 0;
}

static void func_actor_400600_8013896C(Task* arg0, s16 arg1)
{
    GfxCoord* coord;
    SVECTOR   vec;
    s32       i;

    coord = arg0->extra.tmd->coords;
    Gp_SpawnEff(gRoomEffectWaterRippleId, coord, 0x40, NULL);
    for (i = 0; i < 16; i++) {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = arg1;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(gRoomEffectWaterSprayId, coord, 0x01202148, &vec);
    }
}

static void func_actor_400600_80138A24(Task* arg0, s16 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    Enemy*           enemy;

    work  = (Actor400600Work*)arg0->work;
    model = arg0->extra.tmd;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if (arg1 != 0) {
        enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        func_actor_400600_801387DC(arg0, 2);
        work->cloaked     = 1;
        work->field_740   = 0;
        work->cloakFading = 0;
        work->field_73A   = 0;
    }
}

#include "../../shared/stalker_zebra_ivory_clear_queued.inc.c"

static void func_actor_400600_80138AB8(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->timer > 0) {
        work->timer = (u16)work->timer - 1;
    }
    if (work->ceilingCooldown > 0) {
        work->ceilingCooldown--;
    }
}

#include "../../shared/stalker_zebra_ivory_seed_timer.inc.c"

#include "../../shared/stalker_zebra_ivory_drop_capsule_grid.inc.c"

static void func_actor_400600_80138B5C(Task* arg0, s32 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    Enemy*           enemy;

    model = arg0->extra.tmd;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (Actor400600Work*)arg0->work;
    if (!(arg1 & 0xFF)) {
        if (work->cloaked != 0) {
            work->cloaked     = 0;
            work->cloakFading = 1;
            work->field_740   = 0;
            model->flags      = (model->flags | TMD_OBJECT_SEMI_TRANS) & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
            enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
            func_actor_400600_801387DC(arg0, 0);
        }
    } else if (work->cloaked != 1) {
        work->cloaked     = 1;
        work->cloakFading = 1;
        work->field_740   = 0;
        model->flags     |= TMD_OBJECT_SEMI_TRANS;
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        func_actor_400600_801387DC(arg0, 2);
    }
}

static void func_actor_400600_80138C34(Task* arg0)
{
    TmdObject*       model = arg0->extra.tmd;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TaskFuncTable8   fns   = D_actor_400600_80131E7C;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            stalkerZebraIvoryTickAnim(arg0);
            fns.funcs[(s16)work->state](arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryApplyRotation(arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->capsuleContacts);
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_400600_80132704(arg0, work->field_73C, work->field_73A);
            break;
    }
}

static void func_actor_400600_80138D78(Task* arg0)
{
    TmdObject*       model    = arg0->extra.tmd;
    Actor400600Work* work     = (Actor400600Work*)arg0->work;
    TaskFuncTable4   handlers = D_actor_400600_80131E9C;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            stalkerZebraIvoryTickAnim(arg0);
            handlers.funcs[(s16)work->state](arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryApplyRotation(arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->capsuleContacts);
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_400600_80138224(arg0, 0, work->field_73A);
            break;
    }
}

static void func_actor_400600_80138EA0(Task* arg0)
{
    TmdObject*       model = arg0->extra.tmd;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TaskFuncTable6   fns   = D_actor_400600_80131E54;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            fns.funcs[(s16)work->state](arg0);
            stalkerZebraIvoryTickAnim(arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryApplyRotation(arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->capsuleContacts);
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_400600_80138224(arg0, 0, (u8)work->field_73A);
            break;
    }
}

static void func_actor_400600_80138FD4(Task* arg0)
{
    TmdObject*       model    = arg0->extra.tmd;
    Actor400600Work* work     = (Actor400600Work*)arg0->work;
    TaskFuncTable4   handlers = D_actor_400600_80131E6C;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            handlers.funcs[(s16)work->state](arg0);
            stalkerZebraIvoryTickAnim(arg0);
            func_actor_400600_80136558(arg0);
            stalkerZebraIvoryApplyRotation(arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->capsuleContacts);
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_400600_80138224(arg0, 0, work->field_73A);
            break;
    }
}

static void func_actor_400600_801390FC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->state    = 1;
    work->subState = 0;
}

static void func_actor_400600_80139110(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B6F4, func_actor_400600_8013B740 };

    stalkerZebraIvoryClearQueued(arg0);
    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->subState](arg0);
        if ((s16)func_actor_400600_80137C34(arg0) == 0 && (s16)func_actor_400600_80137AF0(arg0) == 0 && (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 8, 0, 0) && work->onCeiling != 0 && arg0->extra.tmd->coords->coord.t[0] > 10000) {
            Actor400600Work* cur = (Actor400600Work*)arg0->work;

            cur->state    = 0xD;
            cur->subState = 0;
        }
    }
}

static void func_actor_400600_80139218(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B830, func_actor_400600_8013B8AC };

    stalkerZebraIvoryClearQueued(arg0);
    fns[(s16)work->subState](arg0);
}

static void func_actor_400600_80139280(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B984, func_actor_400600_8013BA00 };

    stalkerZebraIvoryClearQueued(arg0);
    fns[(s16)work->subState](arg0);
}

static void func_actor_400600_801392E8(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    TaskFuncTable3   fns  = D_actor_400600_80131F34;

    stalkerZebraIvoryClearQueued(arg0);
    fns.funcs[(s16)work->subState](arg0);
}

static void func_actor_400600_8013935C(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BBF4, func_actor_400600_80133CB0 };

    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->subState](arg0);
    }
}

static void func_actor_400600_801393D0(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BC68, func_actor_400600_80133E38 };

    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->subState](arg0);
    }
}

static void func_actor_400600_80139444(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    TaskFuncTable8   fns  = D_actor_400600_80131F40;

    stalkerZebraIvoryClearQueued(arg0);
    fns.funcs[(s16)work->subState](arg0);
}

static void func_actor_400600_801394E0(Task* arg0)
{
    Actor400600Work* work     = (Actor400600Work*)arg0->work;
    TaskFuncTable4   handlers = D_actor_400600_80131F60;

    stalkerZebraIvoryClearQueued(arg0);
    handlers.funcs[(s16)work->subState](arg0);
}

static void func_actor_400600_80139560(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    SVECTOR          pos;
    s16              count;

    work = (Actor400600Work*)arg0->work;
    stalkerZebraIvoryClearQueued(arg0);
    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        count           = work->countdown - 1;
        work->countdown = count;
        if (count == 0) {
            work2           = (Actor400600Work*)arg0->work;
            work2->state    = 0xB;
            work2->subState = 0;
            return;
        }
        pos.vx = work->field_A8.vx;
        pos.vy = work->field_A8.vy;
        pos.vz = work->field_A8.vz;
        stalkerZebraIvoryTurnToward(arg0, &pos, 0x18);
        stalkerZebraIvoryStepClip4(arg0);
    }
}

static void func_actor_400600_80139608(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BFD4, stalkerZebraIvoryRightItself };

    stalkerZebraIvoryClearQueued(arg0);
    fns[(s16)work->subState](arg0);
}

static void func_actor_400600_80139670(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    TaskFuncTable3   fns  = D_actor_400600_80131F70;

    stalkerZebraIvoryClearQueued(arg0);
    fns.funcs[(s16)work->subState](arg0);
}

static void func_actor_400600_801396E4(Task* arg0)
{
    Actor400600Work* work     = (Actor400600Work*)arg0->work;
    TaskFuncTable4   handlers = D_actor_400600_80131F7C;

    stalkerZebraIvoryClearQueued(arg0);
    handlers.funcs[(s16)work->subState](arg0);
}

static void func_actor_400600_80139764(Task* arg0)
{
    Actor400600Work* work     = (Actor400600Work*)arg0->work;
    TaskFuncTable4   handlers = D_actor_400600_80131F8C;

    stalkerZebraIvoryClearQueued(arg0);
    handlers.funcs[(s16)work->subState](arg0);
}

#include "../../shared/stalker_zebra_ivory_run_sub_states.inc.c"

static void func_actor_400600_80139878(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013C518, stalkerZebraIvoryPickRange };

    stalkerZebraIvoryClearQueued(arg0);
    fns[(s16)work->subState](arg0);
}

static void func_actor_400600_801398E0(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013C598, func_actor_400600_8013C5F8 };

    stalkerZebraIvoryClearQueued(arg0);
    fns[(s16)work->subState](arg0);
}

#include "../../shared/stalker_zebra_ivory_apply_rotation.inc.c"

#include "../../shared/stalker_zebra_ivory_restart_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_blend_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_scale_frame.inc.c"

#include "../../shared/stalker_zebra_ivory_turn_toward.inc.c"

#include "../../shared/stalker_zebra_ivory_tick_anim.inc.c"

#include "../../shared/stalker_zebra_ivory_play_clip.inc.c"

static void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->animBlend   = arg3;
    work->animStep    = arg2;
    work->animClip    = arg1;
    work->animRequest = 1;
}

#include "../../shared/stalker_zebra_ivory_part_view_xz.inc.c"

#include "../../shared/stalker_zebra_ivory_pin_part_xz.inc.c"

static void func_actor_400600_80139F4C(Task* arg0, s16 arg1, SVECTOR3* arg2)
{
    MATRIX    local;
    GfxCoord* coord;
    GfxCoord* coords;

    coords = arg0->extra.tmd->coords;
    coord  = &coords[arg1];
    Gp_UpdateCoord(coord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &local);
    arg2->vx            = local.t[0];
    arg2->vy            = local.t[1];
    arg2->vz            = coords[0].coord.t[2];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_400600_80139FE0(Task* arg0, s16 arg1, SVECTOR3* arg2)
{
    MATRIX    root;
    MATRIX    local;
    GfxCoord* coord;
    GfxCoord* coords;

    coords = arg0->extra.tmd->coords;
    coord  = &coords[arg1];
    Gp_UpdateCoord(coord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[0].workm, &root);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &local);
    coords[0].coord.t[0]   = arg2->vx - (local.t[0] - root.t[0]);
    coords[0].coord.t[1]   = arg2->vy - (local.t[1] - root.t[1]);
    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    coord->composeStamp    = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    Gp_UpdateCoord(coords);
}

#include "../../shared/stalker_zebra_ivory_clip_done.inc.c"

void func_actor_400600_8013A0F0(Task* arg0)
{
    TaskFuncTable9 states = D_actor_400600_80131EAC;

    states.funcs[arg0->state](arg0);
}

static void func_actor_400600_8013A170(Task* arg0)
{
    TmdObject*       model = arg0->extra.tmd;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TaskFuncTable12  fns   = D_actor_400600_80131E24;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            func_actor_400600_801387DC(arg0, -1);
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            fns.funcs[(s16)work->state](arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_400600_80138224(arg0, work->field_73E, work->field_73A);
            break;
    }
}

static void func_actor_400600_8013A26C(Task* arg0)
{
    Actor400600Work* work                = (Actor400600Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_400600_8013AAD8,
        func_actor_400600_8013AB44,
    };

    states[(s16)work->state](arg0);
}

#include "../../shared/stalker_zebra_ivory_update_color.inc.c"

#include "../../shared/stalker_zebra_ivory_set_move_mode.inc.c"

void func_actor_400600_8013A3A8(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    ((Actor400600Work*)arg0->work)->field_763 = 1;
}

void func_actor_400600_8013A3B8(Task* task)
{
}

void func_actor_400600_8013A3C0(Task* task)
{
}

static void func_actor_400600_8013A3C8(Task* arg0)
{
    Enemy*           enemy;
    Actor400600Work* work;
    TmdObject*       model;

    enemy                = (Enemy*)arg0->spawnArg2.pointer;
    work                 = (Actor400600Work*)arg0->work;
    model                = arg0->extra.tmd;
    work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldTargetUnlinkNode(&enemy->node);
    if (work->pendingAction == 4) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        func_actor_400600_801387DC(arg0, -1);
        work->field_718 = 0;
        func_actor_400600_8013CC04(arg0, 7);
    } else if (work->onCeiling == 0) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        func_actor_400600_801387DC(arg0, 0);
        work->state = work->state + 1;
    } else {
        func_actor_400600_8013CC04(arg0, 9);
    }
}

#include "../../shared/stalker_zebra_ivory_resume_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_animate_until_done.inc.c"

static void func_actor_400600_8013A570(Task* arg0)
{
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    GfxCoord*        coord = arg0->extra.tmd->coords;

    ((Enemy*)arg0->spawnArg2.pointer)->recs = 0;
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_5CC);
    Gp_UnlinkObj(&work->capsuleBody);
    work->field_714 = 0x1000;
    work->matrix_0  = coord->coord;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->field_718 = 0;
    work->state++;
}

static void func_actor_400600_8013A638(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;
    u16              frame;

    work            = (Actor400600Work*)arg0->work;
    model           = arg0->extra.tmd;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 0x18) {
        model->flags |= TMD_OBJECT_SEMI_TRANS;
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        func_actor_400600_801387DC(arg0, 2);
        work->field_718 = 0;
        work->state     = work->state + 1;
    }
}

static void func_actor_400600_8013A6C4(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;
    GfxCoord*        coord;
    VECTOR           scale;
    SVECTOR          rot;

    work             = (Actor400600Work*)arg0->work;
    model            = arg0->extra.tmd;
    coord            = model->coords;
    work->field_73A  = (u16)work->field_73A + (-work->field_73A >> 2);
    work->field_714 -= 0x30;
    scale.vx         = 0x1000;
    scale.vy         = work->field_714;
    scale.vz         = 0x1000;
    coord->coord     = work->matrix_0;
    ScaleMatrix(&coord->coord, &scale);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_718++;
    if ((s16)work->field_718 == 8) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = 0;
        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 4, &rot);
    }
    if ((s16)work->field_718 >= 0x11) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        func_actor_400600_801387DC(arg0, -1);
        work->state = work->state + 1;
    }
}

static void func_actor_400600_8013A808(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    arg0->state    = 3;
    work->state    = 0;
    work->subState = 0;
}

static void func_actor_400600_8013A820(Task* arg0)
{
    Actor400600Work* work;
    u16              frame;

    work            = (Actor400600Work*)arg0->work;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 2) {
        work->state = work->state + 1;
    }
}

static void func_actor_400600_8013A864(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    Enemy*           enemy;

    model = arg0->extra.tmd;
    work  = (Actor400600Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    Tmd_FreeBuffers(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    func_actor_400600_80137240(arg0);
    Gp_ReleaseStateF0Add(arg0, 0);
    enemy->recs = 0;
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_5CC);
    Gp_UnlinkObj(&work->capsuleBody);
    work2           = (Actor400600Work*)arg0->work;
    arg0->state     = 3;
    work2->state    = 0;
    work2->subState = 0;
}

static void func_actor_400600_8013A908(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    model         = arg0->extra.tmd;
    work          = (Actor400600Work*)arg0->work;
    model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
    func_actor_400600_80139DB0(arg0, 9, 0x10, 2);
    work->field_722 = 0;
    work->field_724 = 0;
    work->field_73E = work->field_92;
    stalkerZebraIvoryTickAnim(arg0);
    work->state = work->state + 1;
}

static void func_actor_400600_8013A990(Task* arg0)
{
    Actor400600Work* work;
    GfxCoord*        coord;

    work               = (Actor400600Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_722   += 2;
    work->field_724   += work->field_722;
    coord->coord.t[1] += work->field_724;
    if ((s16)work->field_92 < coord->coord.t[1]) {
        coord->coord.t[1] = (s16)work->field_92;
        stalkerZebraIvoryPlayClip(arg0, 0x13, 0x10);
        work->roll += 0x800;
        stalkerZebraIvoryApplyRotation(arg0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        work->field_718 = 0;
        work->onCeiling = 0;
        work->onBack    = 1;
        work->state++;
    }
    stalkerZebraIvoryTickAnim(arg0);
}

static void func_actor_400600_8013AA5C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if ((s16)work->field_718 == 0) {
        func_actor_400600_8013CB70(arg0, 0x40060006);
        work->field_718 = work->field_718 + 1;
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        func_actor_400600_8013CC04(arg0, 1);
    }
    stalkerZebraIvoryTickAnim(arg0);
}

static void func_actor_400600_8013AAD8(Task* arg0)
{
    Actor400600Work* work;
    Task*            child;
    Task*            child2;

    work                                     = (Actor400600Work*)arg0->work;
    gSceneCombatState.zebraStalkerDeathAlert = 1;
    child                                    = work->field_704;
    if (child != NULL) {
        taskKill(child);
    }
    child2 = work->field_708;
    if (child2 != NULL) {
        taskKill(child2);
    }
    work->field_718 = 0;
    work->state     = work->state + 1;
}

static void func_actor_400600_8013AB44(Task* arg0)
{
    Actor400600Work* work;
    u16              frame;

    work            = (Actor400600Work*)arg0->work;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 0x97) {
        enemyDestroy(arg0->spawnArg2.pointer, arg0);
    }
}

static void func_actor_400600_8013AB98(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work            = (Actor400600Work*)arg0->work;
    model           = arg0->extra.tmd;
    work->field_73A = 0;
    func_actor_400600_80138B5C(arg0, 1);
    work->obj_4B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    model->flags        |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state          = work->state + 1;
}

static void func_actor_400600_8013AC14(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GfxCoord*        coord;

    work  = (Actor400600Work*)arg0->work;
    model = arg0->extra.tmd;
    coord = model->coords;
    if (work->roomCommand == 1) {
        func_dryfield_night_junk_yard_8017D9B8(0);
        coord->coord.t[0] = 0x40C8;
        coord->coord.t[1] = -0x708;
        coord->coord.t[2] = -0x3E8;
        work->pitch       = 0;
        work->yaw         = 0;
        work->roll        = 0;
        model->flags     &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_722   = 0;
        work->field_724   = 0;
        stalkerZebraIvoryPlayClip(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->state = work->state + 1;
    } else if (work->roomCommand == 3) {
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        coord->coord.t[0]    = 0x32C2;
        coord->coord.t[2]    = 0x960;
        coord->coord.t[1]    = 0;
        work->pitch          = 0;
        work->yaw            = 0xC00;
        work->roll           = 0;
        work->field_73A      = 0;
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->state         = 0;
        work2->subState      = 0;
    }
}

static void func_actor_400600_8013AD3C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->field_718 = 0;
        work->field_722 = 0;
        work->field_724 = 0;
        stalkerZebraIvoryPlayClip(arg0, 0x22, 0x10);
        work->state = work->state + 1;
    }
}

static void func_actor_400600_8013ADA4(Task* arg0)
{
    Actor400600Work* work;
    GfxCoord*        coord;
    s32              x;
    s32              y;
    u16              frame;
    u16              step;
    u16              accum;

    work            = (Actor400600Work*)arg0->work;
    coord           = arg0->extra.tmd->coords;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 5) {
        x                 = coord->coord.t[0];
        coord->coord.t[0] = x + ((0x40B5 - x) >> 3);
        coord->coord.t[2] = coord->coord.t[2] + 0x78;
        step              = (u16)work->field_722 + 2;
        accum             = (u16)work->field_724 + step;
        work->field_724   = accum;
        work->field_722   = step;
        y                 = coord->coord.t[1] + (s16)accum;
        coord->coord.t[1] = y;
        if (y >= 0) {
            Gp_SpawnPadLerp(0x10, 0x80, 0x20);
            work->field_718 = 0;
            stalkerZebraIvoryPlayClip(arg0, 0x19, 0x10);
            coord->coord.t[1] = 0;
            work->state       = work->state + 1;
        }
    }
}

static void func_actor_400600_8013AE88(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work            = (Actor400600Work*)arg0->work;
    model           = arg0->extra.tmd;
    work->field_73A = 0;
    func_actor_400600_80138B5C(arg0, 1);
    work->obj_4B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    model->flags        |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state          = work->state + 1;
}

static void func_actor_400600_8013AF04(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GfxCoord*        coord;

    work  = (Actor400600Work*)arg0->work;
    model = arg0->extra.tmd;
    coord = model->coords;
    if (work->roomCommand == 2) {
        coord->coord.t[0] = 0x1FDD;
        coord->coord.t[1] = -0xE38;
        coord->coord.t[2] = 0x5CE;
        work->pitch       = 0;
        work->yaw         = 0x400;
        work->roll        = 0;
        model->flags     &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_722   = 0;
        work->field_724   = 0;
        stalkerZebraIvoryPlayClip(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->state = work->state + 1;
    } else if (work->roomCommand == 3) {
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        coord->coord.t[0]    = 0x640;
        coord->coord.t[2]    = 0x87A;
        coord->coord.t[1]    = 0;
        work->pitch          = 0;
        work->yaw            = 0x400;
        work->roll           = 0;
        work->field_73A      = 0;
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->state         = 0;
        work2->subState      = 0;
    }
}

static void func_actor_400600_8013B018(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40060004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->state         = 0;
        work2->subState      = 0;
        work3                = (Actor400600Work*)arg0->work;
        work3->state         = 2;
        work3->subState      = 0;
    }
}

static void func_actor_400600_8013B0FC(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work                 = (Actor400600Work*)arg0->work;
    model                = arg0->extra.tmd;
    work->field_73A      = 0;
    work->obj_4B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    model->flags        |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state          = work->state + 1;
}

static void func_actor_400600_8013B150(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ArmStateF0(1);
        work2           = (Actor400600Work*)arg0->work;
        arg0->state     = 1;
        work2->state    = 0;
        work2->subState = 0;
        work3           = (Actor400600Work*)arg0->work;
        work3->state    = 2;
        work3->subState = 0;
    }
}

static void func_actor_400600_8013B1DC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_718 = work->field_718 + 1;
    func_actor_400600_801361AC(arg0);
    if ((s16)work->field_718 == 0x26) {
        func_actor_400600_80138B5C(arg0, 0);
    }
    if ((s16)work->field_718 >= 0x27) {
        work->field_73A = (u16)work->field_73A + ((0xFF - work->field_73A) >> 4);
    }
    if ((s16)work->field_718 == 0x50) {
        work->field_718 = 0;
        work->field_722 = -0xA;
        work->field_724 = 0;
        func_actor_400600_80139DB0(arg0, 0x15, 0x10, 4);
        work->state = work->state + 1;
    }
}

static void func_actor_400600_8013B2A8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x404A0004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ArmStateF0(1);
        work2           = (Actor400600Work*)arg0->work;
        arg0->state     = 1;
        work2->state    = 0;
        work2->subState = 0;
        work3           = (Actor400600Work*)arg0->work;
        work3->state    = 2;
        work3->subState = 0;
    }
}

static void func_actor_400600_8013B394(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work            = (Actor400600Work*)arg0->work;
    model           = arg0->extra.tmd;
    work->field_73A = 0;
    func_actor_400600_80138B5C(arg0, 1);
    work->obj_4B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    model->flags        |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state          = work->state + 1;
}

static void func_actor_400600_8013B410(Task* arg0)
{
    Actor400600Work* work;
    GfxCoord*        coords;
    s32              soundId;
    s32              pan;

    work                = (Actor400600Work*)arg0->work;
    coords              = arg0->extra.tmd->coords;
    work->field_73A    += (0xFF - work->field_73A) >> 5;
    work->field_722    += 1;
    work->field_724    += work->field_722;
    coords->coord.t[1] += work->field_724;
    if (coords->coord.t[1] >= 0) {
        work->field_718 = 0;
        soundId         = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40060003;
        pan             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        stalkerZebraIvoryPlayClip(arg0, 0x19, 0x10);
        coords->coord.t[1] = 0;
        work->state++;
    }
}

static void func_actor_400600_8013B520(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work            = (Actor400600Work*)arg0->work;
    work->field_718 = work->field_718 + 1;
    if ((s16)work->field_718 == 1) {
        Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40060004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->obj_4B4.flags                     |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        gSceneCombatState.zebraStalkerGroupPhase = SCENE_COMBAT_ZEBRA_STALKER_DELAYED;
        work2                                    = (Actor400600Work*)arg0->work;
        arg0->state                              = 1;
        work2->state                             = 0;
        work2->subState                          = 0;
        work3                                    = (Actor400600Work*)arg0->work;
        work3->state                             = 2;
        work3->subState                          = 0;
    }
}

static void func_actor_400600_8013B640(void)
{
    u8 param1[8];
    u8 param2[8];

    if (gSceneCombatState.enemySoundBankQueued == 0) {
        /* Same shape as ActorsShared801692e8: each branch makes its own call
         * and jump2's cross-jumping merges the identical tails. */
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 32, 0, 0) && gGameSession->location.loc.variant == 1) {
            param1[2] = 0x28;
            param1[0] = 2;
            param1[3] = 0;
            param2[0] = 6;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        } else {
            param1[2] = 0x28;
            param1[0] = 1;
            param1[3] = 0;
            param2[0] = 6;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        }
        gSceneCombatState.enemySoundBankQueued = 1;
    }
}

static void func_actor_400600_8013B6F4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_754 = 0x18;
    work->field_76B = 0;
    work->field_752 = 0x10;
    func_actor_400600_80135998(arg0, 0x10);
    work->subState = work->subState + 1;
}

static void func_actor_400600_8013B740(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    SVECTOR          pos;
    s16              min;
    s16              step;
    u32              rnd;

    min = 0x10;
    if (work->playerDistance > 0xBB8 && work->field_76B == 0) {
        rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        if ((rnd >> 0x10) & 1) {
            min  = 0x18;
            step = 0x24;
        } else {
            min  = 0x14;
            step = 0x1E;
        }
        work->field_754 = step;
        work->field_76B = 1;
    }
    if (work->field_752 < min) {
        work->field_752 = min;
    }
    pos.vx = work->field_A8.vx;
    pos.vy = work->field_A8.vy;
    pos.vz = work->field_A8.vz;
    stalkerZebraIvoryTurnToward(arg0, &pos, work->field_754);
    func_actor_400600_80135998(arg0, work->field_752);
}

static void func_actor_400600_8013B830(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->onBack == 0) {
        work->animBlend   = 2;
        work->animStep    = 0x20;
        work->animClip    = 9;
        work->animRequest = 1;
    } else {
        work->animBlend   = 2;
        work->animStep    = 0x20;
        work->animClip    = 0xB;
        work->animRequest = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->subState = work->subState + 1;
}

static void func_actor_400600_8013B8AC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if (work->pendingArmed != 0 && work->pendingAction == 1) {
        if (work->onBack == 0) {
            work->animStep    = 0x20;
            work->animClip    = 9;
            work->animRequest = 2;
        } else {
            work->animStep    = 0x20;
            work->animClip    = 0xB;
            work->animRequest = 2;
        }
        return;
    }
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0 && (stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (Actor400600Work*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (Actor400600Work*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_8013B984(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->onBack == 0) {
        work->animBlend   = 3;
        work->animStep    = 0x10;
        work->animClip    = 0xA;
        work->animRequest = 1;
    } else {
        work->animBlend   = 3;
        work->animStep    = 0x10;
        work->animClip    = 0xC;
        work->animRequest = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->subState = work->subState + 1;
}

static void func_actor_400600_8013BA00(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (Actor400600Work*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (Actor400600Work*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_8013BA6C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->onBack == 0) {
        work->animBlend   = 8;
        work->animStep    = 0x10;
        work->animClip    = 0xF;
        work->animRequest = 1;
    } else {
        work->animBlend   = 3;
        work->animStep    = 0x10;
        work->animClip    = 0x11;
        work->animRequest = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->subState = work->subState + 1;
}

static void func_actor_400600_8013BAEC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
        if (work->onBack == 0) {
            work2              = (Actor400600Work*)arg0->work;
            work2->animBlend   = 8;
            work2->animStep    = 0x10;
            work2->animClip    = 0x10;
            work2->animRequest = 1;
        } else {
            work3              = (Actor400600Work*)arg0->work;
            work3->animBlend   = 8;
            work3->animStep    = 0x10;
            work3->animClip    = 0x12;
            work3->animRequest = 1;
        }
        work->subState = work->subState + 1;
    }
}

static void func_actor_400600_8013BB88(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (Actor400600Work*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (Actor400600Work*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_8013BBF4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->animBlend   = 8;
    work->animStep    = 0x10;
    work->animClip    = 7;
    work->animRequest = 1;
    work->field_718   = 0;
    func_actor_400600_8013CB40(arg0, 0);
    func_actor_400600_80138B5C(arg0, 0);
    work->subState = work->subState + 1;
}

static void func_actor_400600_8013BC68(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->animBlend   = 8;
    work->animStep    = 0x10;
    work->animClip    = 8;
    work->animRequest = 1;
    work->field_718   = 0;
    func_actor_400600_8013CB40(arg0, 1);
    func_actor_400600_80138B5C(arg0, 0);
    work->subState = work->subState + 1;
}

static void func_actor_400600_8013BCD8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0) {
        work->field_718 = 0;
        func_actor_400600_80138B5C(arg0, 0);
        work2              = (Actor400600Work*)arg0->work;
        work2->animBlend   = 4;
        work2->animStep    = 0x10;
        work2->animClip    = 1;
        work2->animRequest = 1;
        work->subState     = work->subState + 1;
    }
}

static void func_actor_400600_8013BD54(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u16              frame;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0) {
        frame           = work->field_718 + 1;
        work->field_718 = frame;
        if ((s16)frame >= 0x11) {
            work->field_718    = 0;
            work2              = (Actor400600Work*)arg0->work;
            work2->animBlend   = 4;
            work2->animStep    = 0x10;
            work2->animClip    = 0x15;
            work2->animRequest = 1;
            work->subState     = work->subState + 1;
        }
    }
}

static void func_actor_400600_8013BDF0(Task* arg0)
{
    Actor400600Work* work;
    u16              frame;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0) {
        frame           = work->field_718 + 1;
        work->field_718 = frame;
        if ((s16)frame >= 0x11) {
            work->subState = work->subState + 1;
        }
    }
}

static void func_actor_400600_8013BE58(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    func_actor_400600_80137498(arg0, 0);
    work->field_763 = 0;
    work->subState  = work->subState + 1;
}

#include "../../shared/stalker_zebra_ivory_release_hold.inc.c"

static void func_actor_400600_8013BF48(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    func_actor_400600_80137498(arg0, 1);
    work->subState = work->subState + 1;
}

#include "../../shared/stalker_zebra_ivory_wait_clip.inc.c"

static void func_actor_400600_8013BFD4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work               = (Actor400600Work*)arg0->work;
    work->field_718    = 0;
    work2              = (Actor400600Work*)arg0->work;
    work2->animBlend   = 2;
    work2->animStep    = 0x10;
    work2->animClip    = 0x16;
    work2->animRequest = 1;
    stalkerZebraIvoryReadPartViewXZ(arg0, 0xE, &work->anchorPos);
    work->subState = work->subState + 1;
}

static void func_actor_400600_8013C038(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->animBlend   = 4;
    work->animStep    = 0x10;
    work->animClip    = 0x15;
    work->animRequest = 1;
    work->field_718   = 0;
    work->subState    = work->subState + 1;
}

static void func_actor_400600_8013C074(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if (((stalkerZebraIvoryTakePending(arg0) << 0x10) == 0) && ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0)) {
        rnd                   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState       = rnd;
        work->ceilingCooldown = ((rnd >> 0x10) & 0x1F) + 0xD2;
        work2                 = (Actor400600Work*)arg0->work;
        work2->state          = 2;
        work2->subState       = 0;
    }
}

static void func_actor_400600_8013C104(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->holding  = 1;
    work->subState = work->subState + 1;
}

static void func_actor_400600_8013C124(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GfxCoord*        coord;

    work  = (Actor400600Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    stalkerZebraIvoryDropCapsuleGrid(arg0);
    work2              = (Actor400600Work*)arg0->work;
    work2->animBlend   = 4;
    work2->animStep    = 0x10;
    work2->animClip    = 0x20;
    work2->animRequest = 1;
    work->field_722    = 0x40;
    work->field_724    = 0;
    work->field_718    = 0;
    work->field_98     = coord->coord.t[0];
    work->field_9C     = coord->coord.t[2];
    work->field_73E    = work->field_92;
    work->subState     = work->subState + 1;
}

static void func_actor_400600_8013C1C0(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work = (Actor400600Work*)arg0->work;
    func_actor_400600_80138B5C(arg0, 0);
    work2              = (Actor400600Work*)arg0->work;
    work2->animBlend   = 2;
    work2->animStep    = 0x10;
    work2->animClip    = 9;
    work2->animRequest = 1;
    work->holding      = 1;
    work->field_722    = 0;
    work->field_724    = 0;
    work->subState     = work->subState + 1;
    work->field_73E    = work->field_92;
}

static void func_actor_400600_8013C238(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GfxCoord*        coord;

    work               = (Actor400600Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_722    = work->field_722 + 2;
    work->field_724    = work->field_724 + work->field_722;
    coord->coord.t[1] += work->field_724;
    if ((s16)work->field_92 < coord->coord.t[1]) {
        coord->coord.t[1]  = (s16)work->field_92;
        work2              = (Actor400600Work*)arg0->work;
        work2->animStep    = 0x10;
        work2->animClip    = 0x13;
        work2->animRequest = 2;
        work->onBack       = 1;
        work->field_718    = 0;
        work->onCeiling    = 0;
        work->roll         = work->roll + 0x800;
        work->subState     = work->subState + 1;
    }
}

#include "../../shared/stalker_zebra_ivory_wait_clip_then_rest.inc.c"

static void func_actor_400600_8013C394(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->onBack == 0) {
        work->animBlend   = 3;
        work->animStep    = 0x10;
        work->animClip    = 0x1A;
        work->animRequest = 1;
    } else {
        work->animBlend   = 3;
        work->animStep    = 0x10;
        work->animClip    = 0x1B;
        work->animRequest = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->subState = work->subState + 1;
}

static void func_actor_400600_8013C410(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2              = (Actor400600Work*)arg0->work;
            work2->animBlend   = 0x1E;
            work2->animStep    = 0x10;
            work2->animClip    = 0x10;
            work2->animRequest = 1;
        } else {
            work3              = (Actor400600Work*)arg0->work;
            work3->animBlend   = 0x1E;
            work3->animStep    = 8;
            work3->animClip    = 0x14;
            work3->animRequest = 1;
        }
        work->subState = work->subState + 1;
    }
}

static void func_actor_400600_8013C4AC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (Actor400600Work*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (Actor400600Work*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_400600_8013C518(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->subState = work->subState + 1;
}

#include "../../shared/stalker_zebra_ivory_pick_range.inc.c"

static void func_actor_400600_8013C598(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    u32              rnd;

    work->animBlend   = 4;
    work->animStep    = 0x10;
    work->animClip    = 1;
    work->animRequest = 1;
    rnd               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState   = rnd;
    work->field_756   = ((rnd >> 0x10) & 0x3F) + 0x5A;
    work->subState    = work->subState + 1;
}

static void func_actor_400600_8013C5F8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u16              count;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if (((func_actor_400600_80136FA8(arg0) << 0x10) == 0) && ((func_actor_400600_80137C34(arg0) << 0x10) == 0)) {
        count           = work->field_756 - 1;
        work->field_756 = count;
        if ((count << 0x10) == 0) {
            rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rnd;
            work->field_758 = ((rnd >> 0x10) & 0x3F) + 0x1E;
            func_actor_400600_80138B5C(arg0, 0);
            work2           = (Actor400600Work*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        }
    }
}

static void func_actor_400600_8013C6B0(SVECTOR* pos, WorldCollisionContact* rec, SVECTOR* out)
{
    VECTOR v;
    VECTOR n;
    s32    dx;
    s32    dz;
    s32    dist;
    s32    t;

    dx   = pos->vx - rec->point.vx;
    v.vy = 0;
    v.vx = dx;
    dz   = pos->vz - rec->point.vz;
    v.vz = dz;
    dist = rec->distance - SquareRoot0(dx * dx + dz * dz);
    t    = dist;
    if (dist <= 0) {
        t = 0;
    }
    dist = t;
    v.vx = pos->vx - rec->point.vx;
    v.vy = pos->vy - rec->point.vy;
    v.vz = pos->vz - rec->point.vz;
    VectorNormal(&v, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &n, &v);
    out->vx = (dist * v.vx) >> 12;
    out->vy = 0;
    out->vz = (dist * v.vz) >> 12;
}

static s32 func_actor_400600_8013C7E8(s16 arg0, s16 arg1)
{
    s16 v;

    v = arg1 >> 3;
    if (arg0 == 0) {
        return v;
    }
    if ((arg0 > 0 && v < 0) || (arg0 < 0 && v > 0)) {
        return arg0;
    }
    if (arg0 > 0) {
        if (v < arg0) {
            return arg0;
        }
        return v;
    }
    if (v < arg0) {
        return v;
    }
    return arg0;
}

static void func_actor_400600_8013C874(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    TmdObject*       model2;

    work            = (Actor400600Work*)arg0->work;
    model           = arg0->extra.tmd;
    work->field_73A = 0;
    work2           = (Actor400600Work*)arg0->work;
    model2          = arg0->extra.tmd;
    if (work2->cloaked != 1) {
        work2->cloaked     = 1;
        work2->cloakFading = 1;
        work2->field_740   = 0;
        model2->flags     |= TMD_OBJECT_SEMI_TRANS;
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        func_actor_400600_801387DC(arg0, 2);
    }
    work->obj_4B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    model->flags        |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state          = work->state + 1;
}

static void func_actor_400600_8013C940(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if (gSceneCombatState.zebraStalkerGroupPhase == SCENE_COMBAT_ZEBRA_STALKER_DELAYED) {
        rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        work->field_718 = ((rnd >> 0x10) & 7) + 0x14;
        work->state     = work->state + 1;
    } else if (gSceneCombatState.zebraStalkerGroupPhase == SCENE_COMBAT_ZEBRA_STALKER_ACTIVE) {
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->state         = 0;
        work2->subState      = 0;
    }
}

static void func_actor_400600_8013C9DC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    work->field_718--;
    if ((s16)work->field_718 == 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40060003;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_5CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->state         = 0;
        work2->subState      = 0;
        work3                = (Actor400600Work*)arg0->work;
        work3->state         = 2;
        work3->subState      = 0;
    }
}

#include "../../shared/stalker_zebra_ivory_take_armed_pending.inc.c"

static void func_actor_400600_8013CB40(Task* arg0, u8 arg1)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    s32              mode = arg1;

    if (mode == 0) {
        work->leftArmOut = 1;
    } else if (mode == 1) {
        work->rightArmOut = mode;
    }
}

static void func_actor_400600_8013CB70(Task* arg0, s32 arg1)
{
    s32 soundId;
    s32 pan;

    if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
        arg1 &= 0xFF00FFFF;
        arg1 |= 0x4A0000;
    }
    soundId = arg1 | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
}

static void func_actor_400600_8013CC04(Task* arg0, s16 arg1)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->state    = arg1;
    work->subState = 0;
}
