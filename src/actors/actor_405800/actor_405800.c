#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/actors_shared_80131fc8.h"

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
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_lighting.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/frame_capture.h"
#include "../../shared/limb_shadows.h"
#define STALKER_ZEBRA_IVORY_KIND STALKER_IVORY
#include "../../shared/stalker_zebra_ivory.h"

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

/// Status flags at + 0x83C of the work block, read through two widths: bit 0
/// as a halfword, then bits 0x102 as a word (`func_actor_405800_80137908`).
/// The high half is `field_83E`, the scale reset to 0x1000 on death.
typedef union Actor405800Flags83C {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
    struct {
        /* 0x0 */ u16 pad;
        /* 0x2 */ s16 field_83E;
    } h;
} Actor405800Flags83C;
STATIC_ASSERT_SIZEOF(Actor405800Flags83C, 0x4);

/// 0x18-byte scratch from the scratch stack used by `func_actor_405800_80133800`
/// to project the third model part's origin. `vec` is the zero vector fed to
/// RTPS through that part's `workm`; `sxy` is `gte_stsxy`, `p` is `gte_stdp`,
/// `flag` is `gte_stflg`, and `otz` is `gte_stszotz`.
typedef struct Actor405800PerspScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     sxy;
    /* 0x0C */ s32     p;
    /* 0x10 */ s32     flag;
    /* 0x14 */ s32     otz;
} Actor405800PerspScratch;
STATIC_ASSERT_SIZEOF(Actor405800PerspScratch, 0x18);

/// Per-actor work block for the `actor_405800` overlay.
///
/// `func_actor_405800_801334B8` allocates it with `memCalloc(0x89C, 0)` and
/// stores the result straight into the `Task::work` slot (0x1C), so the size
/// below is the allocation and not a guess: this actor reuses that pointer
/// field for its own work block. It is the
/// only allocator in the overlay, so every function reaches the same block
/// with `(Actor405800Work*)task->work`.
///
/// `state` / `subState` are the state and sub-state indices the handler
/// table walks and `field_842` is the per-state frame counter.
typedef struct Actor405800Work {
    /* 0x000 */ MATRIX                   matrix_0;  // copy of the root coordinate's local matrix
    /* 0x020 */ MATRIX                   matrix_20; // color matrix for the child models
    /* 0x040 */ MATRIX                   matrix_40; // light matrix for the child models
    /* 0x060 */ byte                     pad_60[0x10];
    /* 0x070 */ VECTOR                   field_70;  // copy of the root coordinate's translation
    /* 0x080 */ u16                      pitch;     // pitch, see stalkerZebraIvoryApplyRotation
    /* 0x082 */ u16                      yaw;       // yaw, see stalkerZebraIvoryApplyRotation
    /* 0x084 */ u16                      roll;      // roll, see stalkerZebraIvoryApplyRotation
    /* 0x086 */ byte                     pad_86[2];
    /* 0x088 */ StalkerZebraIvoryViewPos field_88;
    /* 0x08E */ byte                     pad_8E[2];
    /* 0x090 */ u16                      field_90; // spawn position X (low half)
    /* 0x092 */ u16                      field_92; // copied into field_86A on state entry
    /* 0x094 */ u16                      field_94; // spawn position Z (low half)
    /* 0x096 */ byte                     pad_96[2];
    /* 0x098 */ u16                      field_98; // low half of the root coordinate's world X
    /* 0x09A */ s16                      field_9A;
    /* 0x09C */ u16                      field_9C; // low half of the root coordinate's world Z
    /* 0x09E */ byte                     pad_9E[0xA];
    /* 0x0A8 */ SVECTOR                  field_A8; // world point `stalkerZebraIvoryTurnToward` turns to face (it reads `vx` / `vz`)
    /* 0x0B0 */ AnimationContext         anim;     // slots 1..0x11 reset by stalkerZebraIvoryRestartClip
    /* 0x0C4 */ AnimationSlot            slots[0x12];
    /* 0x394 */ byte                     pad_394[0x120];
    /* 0x4B4 */ WorldCollisionBody       obj_4B4;            // collision node; unlinked on death
    /* 0x4D4 */ WorldCollisionContact    rec_4D4[8];         // obj_4B4 table
    /* 0x594 */ WorldCollisionBody       obj_594;            // collision node; unlinked on death
    /* 0x5B4 */ WorldCollisionContact    rec_5B4[8];         // obj_594 table
    /* 0x674 */ WorldCollisionBody       obj_674;            // collision node; unlinked on death
    /* 0x694 */ WorldCollisionBody       obj_694;            // collision node; unlinked on death
    /* 0x6B4 */ WorldCollisionBody       obj_6B4;            // collision node; unlinked on death
    /* 0x6D4 */ WorldCollisionBody       obj_6D4;            // collision node; unlinked on death
    /* 0x6F4 */ WorldCollisionContact    rec_6F4[1];         // obj_6B4 / obj_674 table
    /* 0x70C */ WorldCollisionContact    rec_70C[1];         // obj_6D4 / obj_694 table
    /* 0x724 */ WorldCollisionBody       capsuleBody;        // collision node; flags bit 0x4000 cleared by stalkerZebraIvoryDropCapsuleGrid
    /* 0x744 */ WorldCollisionCapsule    capsule;            // capsuleBody payload (flags kind 3)
    /* 0x75C */ WorldCollisionContact    capsuleContacts[8]; // occupancy table behind capsule
    /* 0x81C */ EffectSpawnArg           eff_81C;            // fourth model part's coordinate
    /* 0x824 */ Task*                    field_824;          // child task, killed on state exit
    /* 0x828 */ Task*                    field_828;          // child task, killed on state exit
    /* 0x82C */ byte                     pad_82C[4];
    /* 0x830 */ u16                      field_830;          // phase timer; lhu-incremented, compared as s16
    /* 0x832 */ s16                      field_832;
    /* 0x834 */ s16                      field_834;
    /* 0x836 */ s16                      field_836;
    /* 0x838 */ s16                      field_838;
    /* 0x83A */ s16                      field_83A; // nonzero: skip the field_895 / field_896 reset
    /* 0x83C */ Actor405800Flags83C      flags_83C;
    /* 0x840 */ u16                      field_840; // LCG draw at spawn
    /* 0x842 */ u16                      field_842; // per-state frame counter
    /* 0x844 */ s16                      field_844; // cleared with field_842 on state entry
    /* 0x846 */ u16                      state;     // state index
    /* 0x848 */ u16                      subState;  // sub-state index
    /* 0x84A */ s16                      animBlend;
    /* 0x84C */ s16                      field_84C;
    /* 0x84E */ s16                      field_84E;
    /* 0x850 */ s16                      animStep;       // animation speed / step scale
    /* 0x852 */ s16                      playerDistance; // compared against 2000 to pick state 8 vs 0xD
    /* 0x854 */ u16                      field_854;      // facing-delta halfword, range-checked vs 0x200..0xE00
    /* 0x856 */ u16                      field_856;      // facing-delta halfword, range-checked vs 0x200..0xE00
    /* 0x858 */ s16                      pendingArmed;   // must be 1 for the pending pendingAction transition
    /* 0x85A */ s16                      pendingAction;  // pending transition: 3 -> state 5, 5 -> state 0xF
    /* 0x85C */ s16                      timer;          // countdown seeded by stalkerZebraIvorySeedTimer, ticked by func_actor_405800_8013795C
    /* 0x85E */ s16                      field_85E;      // countdown, ticked by func_actor_405800_8013795C
    /* 0x860 */ s16                      field_860;      // model slot id handed to stalkerZebraIvoryPinPartXZ
    /* 0x862 */ byte                     pad_862[0x4];
    /* 0x866 */ s16                      field_866;
    /* 0x868 */ byte                     pad_868[0x2];
    /* 0x86A */ s16                      field_86A;   // seeded from field_92; set to -0x9C4 during the hop
    /* 0x86C */ byte                     pad_86C[0x2];
    /* 0x86E */ s16                      animRequest; // animation request kind
    /* 0x870 */ s16                      animPlaying; // animation id now playing
    /* 0x872 */ s16                      animClip;    // animation id
    /* 0x874 */ s16                      animFrame;   // sound step index
    /* 0x876 */ s16                      field_876;   // damage cooldown
    /* 0x878 */ s16                      field_878;
    /* 0x87A */ s16                      field_87A;
    /* 0x87C */ u16                      countdown; // down-counter
    /* 0x87E */ s16                      field_87E;
    /* 0x880 */ s16                      field_880;
    /* 0x882 */ s16                      field_882; // randomised hold, 0x5A .. 0x99 frames
    /* 0x884 */ s16                      field_884; // cleared on the state-entry path
    /* 0x886 */ byte                     pad_886[0x2];
    /* 0x888 */ s8                       field_888; // damage-over-time reaction active
    /* 0x889 */ byte                     pad_889;
    /* 0x88A */ u8                       moveMode;
    /* 0x88B */ s8                       field_88B;
    /* 0x88C */ u8                       holdTaken;
    /* 0x88D */ s8                       queuedMode;
    /* 0x88E */ s8                       queuedFlag;
    /* 0x88F */ s8                       holding;
    /* 0x890 */ u8                       onCeiling;    // nonzero: allow the state-0xD transition when root X > 10000
    /* 0x891 */ u8                       onBack;
    /* 0x892 */ u8                       distanceMode; // distance mode: 0 none, 1 XZ, 2 XY
    /* 0x893 */ u8                       field_893;
    /* 0x894 */ byte                     pad_894;
    /* 0x895 */ u8                       field_895;
    /* 0x896 */ u8                       field_896;
    /* 0x897 */ u8                       field_897; // 0: vanished and visible poses keep the node scanned; otherwise vanished is not lockable and visible is clear
    /* 0x898 */ u8                       field_898;
    /* 0x899 */ byte                     pad_899[0x3];
} Actor405800Work;
STATIC_ASSERT_SIZEOF(Actor405800Work, 0x89C);

/// The Stalker library's name for this package's work block (see stalker_zebra_ivory.h).
typedef Actor405800Work StalkerZebraIvoryWork;

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the loads that follow. The
 * one-element array is the remedy measured on `actor_400600`, where a
 * `SOFT_BARRIER()` was enough for a byte store but not for this pointer one. */
extern void* D_800678F0[1];

extern TaskDesc      D_actor_405800_801514B4[];
extern EnemyParams   D_actor_405800_801418FC;
extern AnimationSet* D_actor_405800_801513F8[6];
extern AnimationSet* D_actor_405800_80151410[35];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_405800_8015149C[3];
extern u8               gStalkerZebraIvoryResumeClips[];

/* The records closing four of the overlay's model streams, selected through
   `D_800678F0`. */
static TmdSource _gActor405800IvoryStalkerBurstHead;
static TmdSource _gActor405800StalkerBurstTorso;
static TmdSource _gActor405800StalkerEffect;
static TmdSource _gActor405800StalkerBurstHandLeft;

static void func_actor_405800_80132670(Task* arg0);
static void func_actor_405800_80132E3C(Task* arg0, s16 arg1, u8 arg2);
static void func_actor_405800_80132FE0(Task* arg0);
static void func_actor_405800_8013315C(Task* arg0);
static void func_actor_405800_8013340C(Task* arg0);
static void func_actor_405800_801334B8(Task* arg0);
static void func_actor_405800_80133800(Task* arg0);
static void func_actor_405800_80133CD0(Task* arg0);
static void func_actor_405800_80133DB0(Task* arg0);
static void func_actor_405800_80133F48(Task* arg0);
static void func_actor_405800_801340E0(Task* arg0);
static void func_actor_405800_80134314(Task* arg0);
static void func_actor_405800_8013471C(Task* arg0);
static void func_actor_405800_801348E4(Task* arg0);
static void func_actor_405800_80134A64(Task* arg0);
static void func_actor_405800_80134E80(Task* arg0);
static void func_actor_405800_801351BC(Task* arg0);
static void func_actor_405800_80135558(Task* arg0);
static void func_actor_405800_801356A8(Task* arg0);
static void func_actor_405800_80135780(Task* arg0);
static void func_actor_405800_80135A3C(Task* arg0, s16 arg1);
static void func_actor_405800_801361F8(Task* arg0);
static void func_actor_405800_80136388(Task* arg0);
static s32  func_actor_405800_80136A1C(Task* arg0);
static s32  func_actor_405800_80136CE0(Task* arg0);
static void func_actor_405800_80136E14(Task* task);
static void func_actor_405800_8013706C(Task* arg0, s16 arg1);
static s32  func_actor_405800_801373E0(Task* arg0);
static void func_actor_405800_801375C4(Task* task);
static s32  func_actor_405800_80137908(Task* arg0);
static void func_actor_405800_8013795C(Task* task);
static void func_actor_405800_80137A14(Task* task);
static void func_actor_405800_80137A60(Task* task);
static void func_actor_405800_80137B34(Task* task);
static void func_actor_405800_80137B9C(Task* task);
static void func_actor_405800_80137C04(Task* task);
static void func_actor_405800_80137C78(Task* task);
static void func_actor_405800_80137CEC(Task* task);
static void func_actor_405800_80137D60(Task* task);
static void func_actor_405800_80137DE4(Task* task);
static void func_actor_405800_80137E64(Task* task);
static void func_actor_405800_80137EF0(Task* task);
static void func_actor_405800_80137F58(Task* task);
static void func_actor_405800_80137FCC(Task* task);
static void func_actor_405800_80138040(Task* task);
static void func_actor_405800_80138154(Task* task);
static void func_actor_405800_801381BC(Task* task);
void        func_actor_405800_80138634(Task* task);
static void func_actor_405800_80138698(Task* arg0);
static void func_actor_405800_80138788(Task* arg0);
void        func_actor_405800_801388C4(Task* task, s32 msgId, s32 arg2, s32 arg3);
void        func_actor_405800_801388D4(Task* task);
void        func_actor_405800_801388DC(Task* task);
static void func_actor_405800_801388E4(Task* task);
static void func_actor_405800_80138A70(Task* task);
static void func_actor_405800_80138B50(Task* task);
static void func_actor_405800_80138BD4(Task* task);
static void func_actor_405800_80138BEC(Task* task);
static void func_actor_405800_80138C30(Task* task);
static void func_actor_405800_80138CF0(Task* task);
static void func_actor_405800_80138D54(Task* task);
static void func_actor_405800_80138E20(Task* task);
static void func_actor_405800_80138EF0(Task* task);
static void func_actor_405800_80138F54(Task* task);
static void func_actor_405800_80138FA8(Task* task);
static void func_actor_405800_8013902C(Task* task);
static void func_actor_405800_801390FC(Task* arg0);
static void func_actor_405800_80139188(Task* arg0);
static void func_actor_405800_80139260(Task* arg0);
static void func_actor_405800_801392EC(Task* arg0);
static void func_actor_405800_80139358(Task* arg0);
static void func_actor_405800_801393E8(Task* arg0);
static void func_actor_405800_801394E4(Task* arg0);
static void func_actor_405800_80139550(Task* task);
static void func_actor_405800_801395E8(Task* task);
static void func_actor_405800_8013967C(Task* task);
static void func_actor_405800_801397B8(Task* task);
static void func_actor_405800_80139844(Task* task);
static void func_actor_405800_80139880(Task* task);
static void func_actor_405800_801398C0(Task* task);
static void func_actor_405800_80139928(Task* task);
static void func_actor_405800_801399C4(Task* arg0);
static void func_actor_405800_80139AC4(Task* arg0);
static void func_actor_405800_80139B3C(Task* arg0);
static void func_actor_405800_80139C98(Task* arg0);
static void func_actor_405800_80139D24(Task* arg0);
static void func_actor_405800_80139DC0(Task* arg0);
static void func_actor_405800_80139E2C(Task* task);
static void func_actor_405800_80139EAC(Task* arg0);
static void func_actor_405800_80139F0C(Task* task, u8 arg1);
static void func_actor_405800_80139FB0(Task* task, s16 arg1);
static void func_actor_405800_8013A1F8(Task* task, s16 arg1, s16 arg2, s16 arg3);

static TmdSource _gActor405800IvoryStalkerBody;
void             func_actor_405800_80138634(Task*);

static TmdSource _gActor405800IvoryStalkerBurstArmRight;
static TmdSource _gActor405800IvoryStalkerBurstArmLeft;
void             func_actor_405800_801388C4(Task*, s32, s32, s32);
void             func_actor_405800_801388D4(Task*);
void             func_actor_405800_801388DC(Task*);

static TmdBone _gActor405800IvoryStalkerBodySkeleton[18] = {
#include "assets/ivory_stalker_body_skeleton.inc"
};

static u32 _gActor405800IvoryStalkerBodyPartVerts[18] = {
#include "assets/ivory_stalker_body_partVerts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBodyVerts[257] = {
#include "assets/ivory_stalker_body_verts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBodyNormals[249] = {
#include "assets/ivory_stalker_body_normals.inc"
};

static u32 _gActor405800IvoryStalkerBodyStream[3666] = {
#include "assets/ivory_stalker_body_stream.inc"
};

static TmdSource _gActor405800IvoryStalkerBody = {
    0,
    36152,
    13736,
    18,
    _gActor405800IvoryStalkerBodyPartVerts,
    _gActor405800IvoryStalkerBodyVerts,
    _gActor405800IvoryStalkerBodyNormals,
    _gActor405800IvoryStalkerBodySkeleton,
    _gActor405800IvoryStalkerBodyStream,
};

static TmdBone _gActor405800IvoryStalkerBurstArmRightSkeleton[1] = {
#include "assets/ivory_stalker_burst_arm_right_skeleton.inc"
};

static u32 _gActor405800IvoryStalkerBurstArmRightPartVerts[1] = {
#include "assets/ivory_stalker_burst_arm_right_partVerts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstArmRightVerts[10] = {
#include "assets/ivory_stalker_burst_arm_right_verts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstArmRightNormals[12] = {
#include "assets/ivory_stalker_burst_arm_right_normals.inc"
};

static u32 _gActor405800IvoryStalkerBurstArmRightStream[85] = {
#include "assets/ivory_stalker_burst_arm_right_stream.inc"
};

static TmdSource _gActor405800IvoryStalkerBurstArmRight = {
    0,
    528,
    0,
    1,
    _gActor405800IvoryStalkerBurstArmRightPartVerts,
    _gActor405800IvoryStalkerBurstArmRightVerts,
    _gActor405800IvoryStalkerBurstArmRightNormals,
    _gActor405800IvoryStalkerBurstArmRightSkeleton,
    _gActor405800IvoryStalkerBurstArmRightStream,
};

static TmdBone _gActor405800IvoryStalkerBurstArmLeftSkeleton[1] = {
#include "assets/ivory_stalker_burst_arm_left_skeleton.inc"
};

static u32 _gActor405800IvoryStalkerBurstArmLeftPartVerts[1] = {
#include "assets/ivory_stalker_burst_arm_left_partVerts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstArmLeftVerts[10] = {
#include "assets/ivory_stalker_burst_arm_left_verts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstArmLeftNormals[17] = {
#include "assets/ivory_stalker_burst_arm_left_normals.inc"
};

static u32 _gActor405800IvoryStalkerBurstArmLeftStream[85] = {
#include "assets/ivory_stalker_burst_arm_left_stream.inc"
};

static TmdSource _gActor405800IvoryStalkerBurstArmLeft = {
    0,
    528,
    0,
    1,
    _gActor405800IvoryStalkerBurstArmLeftPartVerts,
    _gActor405800IvoryStalkerBurstArmLeftVerts,
    _gActor405800IvoryStalkerBurstArmLeftNormals,
    _gActor405800IvoryStalkerBurstArmLeftSkeleton,
    _gActor405800IvoryStalkerBurstArmLeftStream,
};

static TmdBone _gActor405800IvoryStalkerBurstHeadSkeleton[1] = {
#include "assets/ivory_stalker_burst_head_skeleton.inc"
};

static u32 _gActor405800IvoryStalkerBurstHeadPartVerts[1] = {
#include "assets/ivory_stalker_burst_head_partVerts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstHeadVerts[36] = {
#include "assets/ivory_stalker_burst_head_verts.inc"
};

static SVECTOR _gActor405800IvoryStalkerBurstHeadNormals[50] = {
#include "assets/ivory_stalker_burst_head_normals.inc"
};

static u32 _gActor405800IvoryStalkerBurstHeadStream[342] = {
#include "assets/ivory_stalker_burst_head_stream.inc"
};

static TmdSource _gActor405800IvoryStalkerBurstHead = {
    0,
    2300,
    0,
    1,
    _gActor405800IvoryStalkerBurstHeadPartVerts,
    _gActor405800IvoryStalkerBurstHeadVerts,
    _gActor405800IvoryStalkerBurstHeadNormals,
    _gActor405800IvoryStalkerBurstHeadSkeleton,
    _gActor405800IvoryStalkerBurstHeadStream,
};

static TmdBone _gActor405800StalkerBurstTorsoSkeleton[1] = {
#include "assets/stalker_burst_torso_skeleton.inc"
};

static u32 _gActor405800StalkerBurstTorsoPartVerts[1] = {
#include "assets/stalker_burst_torso_partVerts.inc"
};

static SVECTOR _gActor405800StalkerBurstTorsoVerts[55] = {
#include "assets/stalker_burst_torso_verts.inc"
};

static SVECTOR _gActor405800StalkerBurstTorsoNormals[62] = {
#include "assets/stalker_burst_torso_normals.inc"
};

static u32 _gActor405800StalkerBurstTorsoStream[600] = {
#include "assets/stalker_burst_torso_stream.inc"
};

static TmdSource _gActor405800StalkerBurstTorso = {
    0,
    3988,
    0,
    1,
    _gActor405800StalkerBurstTorsoPartVerts,
    _gActor405800StalkerBurstTorsoVerts,
    _gActor405800StalkerBurstTorsoNormals,
    _gActor405800StalkerBurstTorsoSkeleton,
    _gActor405800StalkerBurstTorsoStream,
};

static TmdBone _gActor405800StalkerEffectSkeleton[1] = {
#include "assets/stalker_effect_skeleton.inc"
};

static u32 _gActor405800StalkerEffectPartVerts[1] = {
#include "assets/stalker_effect_partVerts.inc"
};

static SVECTOR _gActor405800StalkerEffectVerts[27] = {
#include "assets/stalker_effect_verts.inc"
};

static SVECTOR _gActor405800StalkerEffectNormals[34] = {
#include "assets/stalker_effect_normals.inc"
};

static u32 _gActor405800StalkerEffectStream[284] = {
#include "assets/stalker_effect_stream.inc"
};

static TmdSource _gActor405800StalkerEffect = {
    0,
    1860,
    0,
    1,
    _gActor405800StalkerEffectPartVerts,
    _gActor405800StalkerEffectVerts,
    _gActor405800StalkerEffectNormals,
    _gActor405800StalkerEffectSkeleton,
    _gActor405800StalkerEffectStream,
};

static TmdBone _gActor405800StalkerBurstHandLeftSkeleton[1] = {
#include "assets/stalker_burst_hand_left_skeleton.inc"
};

static u32 _gActor405800StalkerBurstHandLeftPartVerts[1] = {
#include "assets/stalker_burst_hand_left_partVerts.inc"
};

static SVECTOR _gActor405800StalkerBurstHandLeftVerts[23] = {
#include "assets/stalker_burst_hand_left_verts.inc"
};

static SVECTOR _gActor405800StalkerBurstHandLeftNormals[26] = {
#include "assets/stalker_burst_hand_left_normals.inc"
};

static u32 _gActor405800StalkerBurstHandLeftStream[211] = {
#include "assets/stalker_burst_hand_left_stream.inc"
};

static TmdSource _gActor405800StalkerBurstHandLeft = {
    0,
    1400,
    0,
    1,
    _gActor405800StalkerBurstHandLeftPartVerts,
    _gActor405800StalkerBurstHandLeftVerts,
    _gActor405800StalkerBurstHandLeftNormals,
    _gActor405800StalkerBurstHandLeftSkeleton,
    _gActor405800StalkerBurstHandLeftStream,
};

static TmdBone _gActor405800StalkerBurstFootRightSkeleton[1] = {
#include "assets/stalker_burst_foot_right_skeleton.inc"
};

static u32 _gActor405800StalkerBurstFootRightPartVerts[1] = {
#include "assets/stalker_burst_foot_right_partVerts.inc"
};

static SVECTOR _gActor405800StalkerBurstFootRightVerts[19] = {
#include "assets/stalker_burst_foot_right_verts.inc"
};

static SVECTOR _gActor405800StalkerBurstFootRightNormals[25] = {
#include "assets/stalker_burst_foot_right_normals.inc"
};

static u32 _gActor405800StalkerBurstFootRightStream[188] = {
#include "assets/stalker_burst_foot_right_stream.inc"
};

static TmdSource _gActor405800StalkerBurstFootRight = {
    0,
    1220,
    0,
    1,
    _gActor405800StalkerBurstFootRightPartVerts,
    _gActor405800StalkerBurstFootRightVerts,
    _gActor405800StalkerBurstFootRightNormals,
    _gActor405800StalkerBurstFootRightSkeleton,
    _gActor405800StalkerBurstFootRightStream,
};

DamageAttack D_actor_405800_801418F0[3] = {
    { 30, 3 },
    { 25, 10 },
    { 12, 2 },
};

EnemyParams D_actor_405800_801418FC = { D_actor_405800_801418F0, 1000, 600, 300, 15, 100, 8, 0, 0 };

static AnimationPackedPose _gActor405800Animation107B4Bank1[26] = {
#include "assets/actor_405800_animation_107B4_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation107B4Bank4[323] = {
#include "assets/actor_405800_animation_107B4_bank4.inc"
};

static AnimationRecord _gActor405800Animation107B4Records[408] = {
#include "assets/actor_405800_animation_107B4_records.inc"
};

static u16 _gActor405800Animation107B4Indices[18] = {
#include "assets/actor_405800_animation_107B4_indices.inc"
};

static AnimationSet _gActor405800Animation107B4 = {
    _gActor405800Animation107B4Records,
    _gActor405800Animation107B4Indices,
    { NULL, _gActor405800Animation107B4Bank1, NULL, NULL, _gActor405800Animation107B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation10EECBank1[22] = {
#include "assets/actor_405800_animation_10EEC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation10EECBank4[156] = {
#include "assets/actor_405800_animation_10EEC_bank4.inc"
};

static AnimationRecord _gActor405800Animation10EECRecords[221] = {
#include "assets/actor_405800_animation_10EEC_records.inc"
};

static u16 _gActor405800Animation10EECIndices[18] = {
#include "assets/actor_405800_animation_10EEC_indices.inc"
};

static AnimationSet _gActor405800Animation10EEC = {
    _gActor405800Animation10EECRecords,
    _gActor405800Animation10EECIndices,
    { NULL, _gActor405800Animation10EECBank1, NULL, NULL, _gActor405800Animation10EECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation115D0Bank1[14] = {
#include "assets/actor_405800_animation_115D0_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation115D0Bank4[157] = {
#include "assets/actor_405800_animation_115D0_bank4.inc"
};

static AnimationRecord _gActor405800Animation115D0Records[223] = {
#include "assets/actor_405800_animation_115D0_records.inc"
};

static u16 _gActor405800Animation115D0Indices[18] = {
#include "assets/actor_405800_animation_115D0_indices.inc"
};

static AnimationSet _gActor405800Animation115D0 = {
    _gActor405800Animation115D0Records,
    _gActor405800Animation115D0Indices,
    { NULL, _gActor405800Animation115D0Bank1, NULL, NULL, _gActor405800Animation115D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation11E68Bank1[25] = {
#include "assets/actor_405800_animation_11E68_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation11E68Bank4[191] = {
#include "assets/actor_405800_animation_11E68_bank4.inc"
};

static AnimationRecord _gActor405800Animation11E68Records[265] = {
#include "assets/actor_405800_animation_11E68_records.inc"
};

static u16 _gActor405800Animation11E68Indices[18] = {
#include "assets/actor_405800_animation_11E68_indices.inc"
};

static AnimationSet _gActor405800Animation11E68 = {
    _gActor405800Animation11E68Records,
    _gActor405800Animation11E68Indices,
    { NULL, _gActor405800Animation11E68Bank1, NULL, NULL, _gActor405800Animation11E68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation12728Bank1[17] = {
#include "assets/actor_405800_animation_12728_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation12728Bank4[214] = {
#include "assets/actor_405800_animation_12728_bank4.inc"
};

static AnimationRecord _gActor405800Animation12728Records[276] = {
#include "assets/actor_405800_animation_12728_records.inc"
};

static u16 _gActor405800Animation12728Indices[18] = {
#include "assets/actor_405800_animation_12728_indices.inc"
};

static AnimationSet _gActor405800Animation12728 = {
    _gActor405800Animation12728Records,
    _gActor405800Animation12728Indices,
    { NULL, _gActor405800Animation12728Bank1, NULL, NULL, _gActor405800Animation12728Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation130A0Bank1[19] = {
#include "assets/actor_405800_animation_130A0_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation130A0Bank4[233] = {
#include "assets/actor_405800_animation_130A0_bank4.inc"
};

static AnimationRecord _gActor405800Animation130A0Records[297] = {
#include "assets/actor_405800_animation_130A0_records.inc"
};

static u16 _gActor405800Animation130A0Indices[18] = {
#include "assets/actor_405800_animation_130A0_indices.inc"
};

static AnimationSet _gActor405800Animation130A0 = {
    _gActor405800Animation130A0Records,
    _gActor405800Animation130A0Indices,
    { NULL, _gActor405800Animation130A0Bank1, NULL, NULL, _gActor405800Animation130A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation13574Bank1[9] = {
#include "assets/actor_405800_animation_13574_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation13574Bank4[114] = {
#include "assets/actor_405800_animation_13574_bank4.inc"
};

static AnimationRecord _gActor405800Animation13574Records[149] = {
#include "assets/actor_405800_animation_13574_records.inc"
};

static u16 _gActor405800Animation13574Indices[18] = {
#include "assets/actor_405800_animation_13574_indices.inc"
};

static AnimationSet _gActor405800Animation13574 = {
    _gActor405800Animation13574Records,
    _gActor405800Animation13574Indices,
    { NULL, _gActor405800Animation13574Bank1, NULL, NULL, _gActor405800Animation13574Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation14154Bank1[23] = {
#include "assets/actor_405800_animation_14154_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation14154Bank4[295] = {
#include "assets/actor_405800_animation_14154_bank4.inc"
};

static AnimationRecord _gActor405800Animation14154Records[377] = {
#include "assets/actor_405800_animation_14154_records.inc"
};

static u16 _gActor405800Animation14154Indices[18] = {
#include "assets/actor_405800_animation_14154_indices.inc"
};

static AnimationSet _gActor405800Animation14154 = {
    _gActor405800Animation14154Records,
    _gActor405800Animation14154Indices,
    { NULL, _gActor405800Animation14154Bank1, NULL, NULL, _gActor405800Animation14154Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation146ACBank1[9] = {
#include "assets/actor_405800_animation_146AC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation146ACBank4[125] = {
#include "assets/actor_405800_animation_146AC_bank4.inc"
};

static AnimationRecord _gActor405800Animation146ACRecords[171] = {
#include "assets/actor_405800_animation_146AC_records.inc"
};

static u16 _gActor405800Animation146ACIndices[18] = {
#include "assets/actor_405800_animation_146AC_indices.inc"
};

static AnimationSet _gActor405800Animation146AC = {
    _gActor405800Animation146ACRecords,
    _gActor405800Animation146ACIndices,
    { NULL, _gActor405800Animation146ACBank1, NULL, NULL, _gActor405800Animation146ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1506CBank1[18] = {
#include "assets/actor_405800_animation_1506C_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1506CBank4[242] = {
#include "assets/actor_405800_animation_1506C_bank4.inc"
};

static AnimationRecord _gActor405800Animation1506CRecords[309] = {
#include "assets/actor_405800_animation_1506C_records.inc"
};

static u16 _gActor405800Animation1506CIndices[18] = {
#include "assets/actor_405800_animation_1506C_indices.inc"
};

static AnimationSet _gActor405800Animation1506C = {
    _gActor405800Animation1506CRecords,
    _gActor405800Animation1506CIndices,
    { NULL, _gActor405800Animation1506CBank1, NULL, NULL, _gActor405800Animation1506CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation15498Bank1[9] = {
#include "assets/actor_405800_animation_15498_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation15498Bank4[95] = {
#include "assets/actor_405800_animation_15498_bank4.inc"
};

static AnimationRecord _gActor405800Animation15498Records[126] = {
#include "assets/actor_405800_animation_15498_records.inc"
};

static u16 _gActor405800Animation15498Indices[18] = {
#include "assets/actor_405800_animation_15498_indices.inc"
};

static AnimationSet _gActor405800Animation15498 = {
    _gActor405800Animation15498Records,
    _gActor405800Animation15498Indices,
    { NULL, _gActor405800Animation15498Bank1, NULL, NULL, _gActor405800Animation15498Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation15A04Bank1[9] = {
#include "assets/actor_405800_animation_15A04_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation15A04Bank4[136] = {
#include "assets/actor_405800_animation_15A04_bank4.inc"
};

static AnimationRecord _gActor405800Animation15A04Records[165] = {
#include "assets/actor_405800_animation_15A04_records.inc"
};

static u16 _gActor405800Animation15A04Indices[18] = {
#include "assets/actor_405800_animation_15A04_indices.inc"
};

static AnimationSet _gActor405800Animation15A04 = {
    _gActor405800Animation15A04Records,
    _gActor405800Animation15A04Indices,
    { NULL, _gActor405800Animation15A04Bank1, NULL, NULL, _gActor405800Animation15A04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation163BCBank1[15] = {
#include "assets/actor_405800_animation_163BC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation163BCBank4[227] = {
#include "assets/actor_405800_animation_163BC_bank4.inc"
};

static AnimationRecord _gActor405800Animation163BCRecords[331] = {
#include "assets/actor_405800_animation_163BC_records.inc"
};

static u16 _gActor405800Animation163BCIndices[18] = {
#include "assets/actor_405800_animation_163BC_indices.inc"
};

static AnimationSet _gActor405800Animation163BC = {
    _gActor405800Animation163BCRecords,
    _gActor405800Animation163BCIndices,
    { NULL, _gActor405800Animation163BCBank1, NULL, NULL, _gActor405800Animation163BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation16928Bank1[10] = {
#include "assets/actor_405800_animation_16928_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation16928Bank4[129] = {
#include "assets/actor_405800_animation_16928_bank4.inc"
};

static AnimationRecord _gActor405800Animation16928Records[169] = {
#include "assets/actor_405800_animation_16928_records.inc"
};

static u16 _gActor405800Animation16928Indices[18] = {
#include "assets/actor_405800_animation_16928_indices.inc"
};

static AnimationSet _gActor405800Animation16928 = {
    _gActor405800Animation16928Records,
    _gActor405800Animation16928Indices,
    { NULL, _gActor405800Animation16928Bank1, NULL, NULL, _gActor405800Animation16928Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation176B4Bank1[16] = {
#include "assets/actor_405800_animation_176B4_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation176B4Bank4[363] = {
#include "assets/actor_405800_animation_176B4_bank4.inc"
};

static AnimationRecord _gActor405800Animation176B4Records[437] = {
#include "assets/actor_405800_animation_176B4_records.inc"
};

static u16 _gActor405800Animation176B4Indices[18] = {
#include "assets/actor_405800_animation_176B4_indices.inc"
};

static AnimationSet _gActor405800Animation176B4 = {
    _gActor405800Animation176B4Records,
    _gActor405800Animation176B4Indices,
    { NULL, _gActor405800Animation176B4Bank1, NULL, NULL, _gActor405800Animation176B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation17D44Bank1[12] = {
#include "assets/actor_405800_animation_17D44_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation17D44Bank4[167] = {
#include "assets/actor_405800_animation_17D44_bank4.inc"
};

static AnimationRecord _gActor405800Animation17D44Records[198] = {
#include "assets/actor_405800_animation_17D44_records.inc"
};

static u16 _gActor405800Animation17D44Indices[18] = {
#include "assets/actor_405800_animation_17D44_indices.inc"
};

static AnimationSet _gActor405800Animation17D44 = {
    _gActor405800Animation17D44Records,
    _gActor405800Animation17D44Indices,
    { NULL, _gActor405800Animation17D44Bank1, NULL, NULL, _gActor405800Animation17D44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation180CCBank1[5] = {
#include "assets/actor_405800_animation_180CC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation180CCBank4[79] = {
#include "assets/actor_405800_animation_180CC_bank4.inc"
};

static AnimationRecord _gActor405800Animation180CCRecords[113] = {
#include "assets/actor_405800_animation_180CC_records.inc"
};

static u16 _gActor405800Animation180CCIndices[18] = {
#include "assets/actor_405800_animation_180CC_indices.inc"
};

static AnimationSet _gActor405800Animation180CC = {
    _gActor405800Animation180CCRecords,
    _gActor405800Animation180CCIndices,
    { NULL, _gActor405800Animation180CCBank1, NULL, NULL, _gActor405800Animation180CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1866CBank1[11] = {
#include "assets/actor_405800_animation_1866C_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1866CBank4[139] = {
#include "assets/actor_405800_animation_1866C_bank4.inc"
};

static AnimationRecord _gActor405800Animation1866CRecords[169] = {
#include "assets/actor_405800_animation_1866C_records.inc"
};

static u16 _gActor405800Animation1866CIndices[18] = {
#include "assets/actor_405800_animation_1866C_indices.inc"
};

static AnimationSet _gActor405800Animation1866C = {
    _gActor405800Animation1866CRecords,
    _gActor405800Animation1866CIndices,
    { NULL, _gActor405800Animation1866CBank1, NULL, NULL, _gActor405800Animation1866CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation18AE8Bank1[10] = {
#include "assets/actor_405800_animation_18AE8_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation18AE8Bank4[104] = {
#include "assets/actor_405800_animation_18AE8_bank4.inc"
};

static AnimationRecord _gActor405800Animation18AE8Records[134] = {
#include "assets/actor_405800_animation_18AE8_records.inc"
};

static u16 _gActor405800Animation18AE8Indices[18] = {
#include "assets/actor_405800_animation_18AE8_indices.inc"
};

static AnimationSet _gActor405800Animation18AE8 = {
    _gActor405800Animation18AE8Records,
    _gActor405800Animation18AE8Indices,
    { NULL, _gActor405800Animation18AE8Bank1, NULL, NULL, _gActor405800Animation18AE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation19654Bank1[40] = {
#include "assets/actor_405800_animation_19654_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation19654Bank4[266] = {
#include "assets/actor_405800_animation_19654_bank4.inc"
};

static AnimationRecord _gActor405800Animation19654Records[326] = {
#include "assets/actor_405800_animation_19654_records.inc"
};

static u16 _gActor405800Animation19654Indices[18] = {
#include "assets/actor_405800_animation_19654_indices.inc"
};

static AnimationSet _gActor405800Animation19654 = {
    _gActor405800Animation19654Records,
    _gActor405800Animation19654Indices,
    { NULL, _gActor405800Animation19654Bank1, NULL, NULL, _gActor405800Animation19654Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation199C0Bank1[6] = {
#include "assets/actor_405800_animation_199C0_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation199C0Bank4[78] = {
#include "assets/actor_405800_animation_199C0_bank4.inc"
};

static AnimationRecord _gActor405800Animation199C0Records[104] = {
#include "assets/actor_405800_animation_199C0_records.inc"
};

static u16 _gActor405800Animation199C0Indices[18] = {
#include "assets/actor_405800_animation_199C0_indices.inc"
};

static AnimationSet _gActor405800Animation199C0 = {
    _gActor405800Animation199C0Records,
    _gActor405800Animation199C0Indices,
    { NULL, _gActor405800Animation199C0Bank1, NULL, NULL, _gActor405800Animation199C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation19EC0Bank1[11] = {
#include "assets/actor_405800_animation_19EC0_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation19EC0Bank4[113] = {
#include "assets/actor_405800_animation_19EC0_bank4.inc"
};

static AnimationRecord _gActor405800Animation19EC0Records[155] = {
#include "assets/actor_405800_animation_19EC0_records.inc"
};

static u16 _gActor405800Animation19EC0Indices[18] = {
#include "assets/actor_405800_animation_19EC0_indices.inc"
};

static AnimationSet _gActor405800Animation19EC0 = {
    _gActor405800Animation19EC0Records,
    _gActor405800Animation19EC0Indices,
    { NULL, _gActor405800Animation19EC0Bank1, NULL, NULL, _gActor405800Animation19EC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1A4BCBank1[10] = {
#include "assets/actor_405800_animation_1A4BC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1A4BCBank4[150] = {
#include "assets/actor_405800_animation_1A4BC_bank4.inc"
};

static AnimationRecord _gActor405800Animation1A4BCRecords[184] = {
#include "assets/actor_405800_animation_1A4BC_records.inc"
};

static u16 _gActor405800Animation1A4BCIndices[18] = {
#include "assets/actor_405800_animation_1A4BC_indices.inc"
};

static AnimationSet _gActor405800Animation1A4BC = {
    _gActor405800Animation1A4BCRecords,
    _gActor405800Animation1A4BCIndices,
    { NULL, _gActor405800Animation1A4BCBank1, NULL, NULL, _gActor405800Animation1A4BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1A680Bank1[2] = {
#include "assets/actor_405800_animation_1A680_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1A680Bank4[16] = {
#include "assets/actor_405800_animation_1A680_bank4.inc"
};

static AnimationRecord _gActor405800Animation1A680Records[72] = {
#include "assets/actor_405800_animation_1A680_records.inc"
};

static u16 _gActor405800Animation1A680Indices[18] = {
#include "assets/actor_405800_animation_1A680_indices.inc"
};

static AnimationSet _gActor405800Animation1A680 = {
    _gActor405800Animation1A680Records,
    _gActor405800Animation1A680Indices,
    { NULL, _gActor405800Animation1A680Bank1, NULL, NULL, _gActor405800Animation1A680Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1A844Bank1[2] = {
#include "assets/actor_405800_animation_1A844_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1A844Bank4[16] = {
#include "assets/actor_405800_animation_1A844_bank4.inc"
};

static AnimationRecord _gActor405800Animation1A844Records[72] = {
#include "assets/actor_405800_animation_1A844_records.inc"
};

static u16 _gActor405800Animation1A844Indices[18] = {
#include "assets/actor_405800_animation_1A844_indices.inc"
};

static AnimationSet _gActor405800Animation1A844 = {
    _gActor405800Animation1A844Records,
    _gActor405800Animation1A844Indices,
    { NULL, _gActor405800Animation1A844Bank1, NULL, NULL, _gActor405800Animation1A844Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1AA08Bank1[2] = {
#include "assets/actor_405800_animation_1AA08_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1AA08Bank4[16] = {
#include "assets/actor_405800_animation_1AA08_bank4.inc"
};

static AnimationRecord _gActor405800Animation1AA08Records[72] = {
#include "assets/actor_405800_animation_1AA08_records.inc"
};

static u16 _gActor405800Animation1AA08Indices[18] = {
#include "assets/actor_405800_animation_1AA08_indices.inc"
};

static AnimationSet _gActor405800Animation1AA08 = {
    _gActor405800Animation1AA08Records,
    _gActor405800Animation1AA08Indices,
    { NULL, _gActor405800Animation1AA08Bank1, NULL, NULL, _gActor405800Animation1AA08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1ABCCBank1[2] = {
#include "assets/actor_405800_animation_1ABCC_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1ABCCBank4[16] = {
#include "assets/actor_405800_animation_1ABCC_bank4.inc"
};

static AnimationRecord _gActor405800Animation1ABCCRecords[72] = {
#include "assets/actor_405800_animation_1ABCC_records.inc"
};

static u16 _gActor405800Animation1ABCCIndices[18] = {
#include "assets/actor_405800_animation_1ABCC_indices.inc"
};

static AnimationSet _gActor405800Animation1ABCC = {
    _gActor405800Animation1ABCCRecords,
    _gActor405800Animation1ABCCIndices,
    { NULL, _gActor405800Animation1ABCCBank1, NULL, NULL, _gActor405800Animation1ABCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1B074Bank1[8] = {
#include "assets/actor_405800_animation_1B074_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1B074Bank4[105] = {
#include "assets/actor_405800_animation_1B074_bank4.inc"
};

static AnimationRecord _gActor405800Animation1B074Records[150] = {
#include "assets/actor_405800_animation_1B074_records.inc"
};

static u16 _gActor405800Animation1B074Indices[18] = {
#include "assets/actor_405800_animation_1B074_indices.inc"
};

static AnimationSet _gActor405800Animation1B074 = {
    _gActor405800Animation1B074Records,
    _gActor405800Animation1B074Indices,
    { NULL, _gActor405800Animation1B074Bank1, NULL, NULL, _gActor405800Animation1B074Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1C248Bank1[36] = {
#include "assets/actor_405800_animation_1C248_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1C248Bank4[451] = {
#include "assets/actor_405800_animation_1C248_bank4.inc"
};

static AnimationRecord _gActor405800Animation1C248Records[563] = {
#include "assets/actor_405800_animation_1C248_records.inc"
};

static u16 _gActor405800Animation1C248Indices[18] = {
#include "assets/actor_405800_animation_1C248_indices.inc"
};

static AnimationSet _gActor405800Animation1C248 = {
    _gActor405800Animation1C248Records,
    _gActor405800Animation1C248Indices,
    { NULL, _gActor405800Animation1C248Bank1, NULL, NULL, _gActor405800Animation1C248Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1CAE4Bank1[20] = {
#include "assets/actor_405800_animation_1CAE4_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1CAE4Bank4[213] = {
#include "assets/actor_405800_animation_1CAE4_bank4.inc"
};

static AnimationRecord _gActor405800Animation1CAE4Records[259] = {
#include "assets/actor_405800_animation_1CAE4_records.inc"
};

static u16 _gActor405800Animation1CAE4Indices[18] = {
#include "assets/actor_405800_animation_1CAE4_indices.inc"
};

static AnimationSet _gActor405800Animation1CAE4 = {
    _gActor405800Animation1CAE4Records,
    _gActor405800Animation1CAE4Indices,
    { NULL, _gActor405800Animation1CAE4Bank1, NULL, NULL, _gActor405800Animation1CAE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1D83CBank1[25] = {
#include "assets/actor_405800_animation_1D83C_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1D83CBank4[343] = {
#include "assets/actor_405800_animation_1D83C_bank4.inc"
};

static AnimationRecord _gActor405800Animation1D83CRecords[416] = {
#include "assets/actor_405800_animation_1D83C_records.inc"
};

static u16 _gActor405800Animation1D83CIndices[20] = {
#include "assets/actor_405800_animation_1D83C_indices.inc"
};

static AnimationSet _gActor405800Animation1D83C = {
    _gActor405800Animation1D83CRecords,
    _gActor405800Animation1D83CIndices,
    { NULL, _gActor405800Animation1D83CBank1, NULL, NULL, _gActor405800Animation1D83CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1E084Bank1[15] = {
#include "assets/actor_405800_animation_1E084_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1E084Bank4[203] = {
#include "assets/actor_405800_animation_1E084_bank4.inc"
};

static AnimationRecord _gActor405800Animation1E084Records[262] = {
#include "assets/actor_405800_animation_1E084_records.inc"
};

static u16 _gActor405800Animation1E084Indices[20] = {
#include "assets/actor_405800_animation_1E084_indices.inc"
};

static AnimationSet _gActor405800Animation1E084 = {
    _gActor405800Animation1E084Records,
    _gActor405800Animation1E084Indices,
    { NULL, _gActor405800Animation1E084Bank1, NULL, NULL, _gActor405800Animation1E084Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1E25CBank1[2] = {
#include "assets/actor_405800_animation_1E25C_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1E25CBank4[16] = {
#include "assets/actor_405800_animation_1E25C_bank4.inc"
};

static AnimationRecord _gActor405800Animation1E25CRecords[76] = {
#include "assets/actor_405800_animation_1E25C_records.inc"
};

static u16 _gActor405800Animation1E25CIndices[20] = {
#include "assets/actor_405800_animation_1E25C_indices.inc"
};

static AnimationSet _gActor405800Animation1E25C = {
    _gActor405800Animation1E25CRecords,
    _gActor405800Animation1E25CIndices,
    { NULL, _gActor405800Animation1E25CBank1, NULL, NULL, _gActor405800Animation1E25CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1EDF4Bank1[20] = {
#include "assets/actor_405800_animation_1EDF4_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1EDF4Bank4[292] = {
#include "assets/actor_405800_animation_1EDF4_bank4.inc"
};

static AnimationRecord _gActor405800Animation1EDF4Records[370] = {
#include "assets/actor_405800_animation_1EDF4_records.inc"
};

static u16 _gActor405800Animation1EDF4Indices[20] = {
#include "assets/actor_405800_animation_1EDF4_indices.inc"
};

static AnimationSet _gActor405800Animation1EDF4 = {
    _gActor405800Animation1EDF4Records,
    _gActor405800Animation1EDF4Indices,
    { NULL, _gActor405800Animation1EDF4Bank1, NULL, NULL, _gActor405800Animation1EDF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor405800Animation1F5B0Bank1[15] = {
#include "assets/actor_405800_animation_1F5B0_bank1.inc"
};

static AnimationPackedRotation _gActor405800Animation1F5B0Bank4[191] = {
#include "assets/actor_405800_animation_1F5B0_bank4.inc"
};

static AnimationRecord _gActor405800Animation1F5B0Records[239] = {
#include "assets/actor_405800_animation_1F5B0_records.inc"
};

static u16 _gActor405800Animation1F5B0Indices[20] = {
#include "assets/actor_405800_animation_1F5B0_indices.inc"
};

static AnimationSet _gActor405800Animation1F5B0 = {
    _gActor405800Animation1F5B0Records,
    _gActor405800Animation1F5B0Indices,
    { NULL, _gActor405800Animation1F5B0Bank1, NULL, NULL, _gActor405800Animation1F5B0Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_405800_801513F8[6] = {
    NULL,
    &_gActor405800Animation1D83C,
    &_gActor405800Animation1E084,
    &_gActor405800Animation1E25C,
    &_gActor405800Animation1EDF4,
    &_gActor405800Animation1F5B0,
};

AnimationSet* D_actor_405800_80151410[35] = {
    NULL,
    &_gActor405800Animation107B4,
    &_gActor405800Animation10EEC,
    &_gActor405800Animation115D0,
    &_gActor405800Animation11E68,
    NULL,
    NULL,
    &_gActor405800Animation12728,
    &_gActor405800Animation130A0,
    &_gActor405800Animation13574,
    &_gActor405800Animation14154,
    &_gActor405800Animation146AC,
    &_gActor405800Animation1506C,
    &_gActor405800Animation15498,
    &_gActor405800Animation15A04,
    &_gActor405800Animation163BC,
    &_gActor405800Animation16928,
    &_gActor405800Animation176B4,
    &_gActor405800Animation17D44,
    &_gActor405800Animation180CC,
    &_gActor405800Animation1866C,
    &_gActor405800Animation18AE8,
    &_gActor405800Animation19654,
    NULL,
    NULL,
    &_gActor405800Animation199C0,
    &_gActor405800Animation19EC0,
    &_gActor405800Animation1A4BC,
    &_gActor405800Animation1A680,
    &_gActor405800Animation1A844,
    &_gActor405800Animation1AA08,
    &_gActor405800Animation1ABCC,
    &_gActor405800Animation1B074,
    &_gActor405800Animation1C248,
    &_gActor405800Animation1CAE4,
};

TaskMessageEntry D_actor_405800_8015149C[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, stalkerZebraIvorySetMoveMode },
    { 2014, func_actor_405800_801388C4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_405800_801514B4[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_405800_801388D4, { .model = &_gActor405800IvoryStalkerBurstArmLeft } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_405800_801388DC, { .model = &_gActor405800IvoryStalkerBurstArmRight } },
};

TaskDesc D_actor_405800_801514CC = { { { TASK_BODY_TMD, 96 } }, func_actor_405800_80138634, { .model = &_gActor405800IvoryStalkerBody } };

u8 gStalkerZebraIvoryResumeClips[35] = { 26, 26, 26, 27, 27, 15, 15, 26, 26, 26, 26, 27, 27, 15, 15, 26, 26, 27, 27, 30, 27, 26, 26, 26, 26, 26, 15, 15, 15, 15, 15, 15, 15, 15, 15 };

static __inline__ void Actor405800_ProjectPart(GfxCoord* part);
static __inline__ void _actor405800SetBehaviour(Task* task, s16 id);

#include "../../shared/stalker_zebra_ivory_inlines.inc.c"

#include "../../shared/frame_capture.inc.c"

static void func_actor_405800_80132670(Task* arg0)
{
    Actor405800Work* work;

    work                           = (Actor405800Work*)arg0->work;
    work->obj_4B4.coord            = &arg0->extra.tmd->coords[3];
    work->obj_4B4.context.contacts = work->rec_4D4;
    work->obj_4B4.pos.vx           = 0;
    work->obj_4B4.pos.vy           = 0;
    work->obj_4B4.pos.vz           = 0x110;
    work->obj_4B4.key              = 0x3003A;
    work->obj_4B4.radius           = 0x2F0;
    work->obj_4B4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj_4B4);
    Gp_InitRec18Table(work->rec_4D4, 8, 0);
    work->obj_4B4.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->obj_594.coord            = arg0->extra.tmd->coords;
    work->obj_594.context.contacts = work->rec_5B4;
    work->obj_594.pos.vx           = 0;
    work->obj_594.pos.vy           = -0x220;
    work->obj_594.pos.vz           = 0;
    work->obj_594.key              = 0x3003A;
    work->obj_594.radius           = 0x460;
    work->obj_594.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj_594);
    Gp_InitRec18Table(work->rec_5B4, 8, 0);
    work->capsule.ends[0].vz          = 0xBB8;
    work->capsule.end0Radius          = 0xA;
    work->capsule.end1Radius          = 0xA;
    work->capsule.ends[0].vx          = 0;
    work->capsule.ends[1].vz          = 0;
    work->capsule.ends[1].vx          = 0;
    work->capsule.contacts            = work->capsuleContacts;
    work->obj_594.flags              |= WORLD_COLLISION_BODY_GRID_ENABLED;
    work->capsuleBody.coord           = arg0->extra.tmd->coords;
    work->capsuleBody.context.capsule = &work->capsule;
    work->capsuleBody.pos.vx          = 0;
    work->capsuleBody.pos.vy          = -0x190;
    work->capsuleBody.pos.vz          = 0;
    work->capsuleBody.key             = 0x30005;
    work->capsuleBody.radius          = 0;
    work->capsuleBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(2, &work->capsuleBody);
    Gp_InitRec18Table(work->capsuleContacts, 8, 0);
    work->capsuleBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->obj_6B4.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->obj_6B4.coord            = &arg0->extra.tmd->coords[7];
    work->obj_6B4.context.contacts = work->rec_6F4;
    work->obj_6B4.pos.vx           = -0x460;
    work->obj_6B4.pos.vy           = 0;
    work->obj_6B4.pos.vz           = 0;
    work->obj_6B4.radius           = 0x290;
    work->obj_6B4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj_6B4);
    Gp_InitRec18Table(work->rec_6F4, 1, 0);
    work->obj_6B4.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_674.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->obj_674.coord            = &arg0->extra.tmd->coords[7];
    work->obj_674.context.contacts = work->rec_6F4;
    work->obj_674.pos.vx           = -0x200;
    work->obj_674.pos.vy           = 0;
    work->obj_674.pos.vz           = 0;
    work->obj_674.radius           = 0x250;
    work->obj_674.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj_674);
    Gp_InitRec18Table(work->rec_6F4, 1, 0);
    work->obj_674.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_6D4.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->obj_6D4.coord            = &arg0->extra.tmd->coords[10];
    work->obj_6D4.context.contacts = work->rec_70C;
    work->obj_6D4.pos.vx           = 0x460;
    work->obj_6D4.pos.vy           = 0;
    work->obj_6D4.pos.vz           = 0;
    work->obj_6D4.radius           = 0x290;
    work->obj_6D4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj_6D4);
    Gp_InitRec18Table(work->rec_70C, 1, 0);
    work->obj_6D4.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_694.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->obj_694.coord            = &arg0->extra.tmd->coords[10];
    work->obj_694.context.contacts = work->rec_70C;
    work->obj_694.pos.vx           = 0x200;
    work->obj_694.pos.vy           = 0;
    work->obj_694.pos.vz           = 0;
    work->obj_694.radius           = 0x250;
    work->obj_694.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj_694);
    Gp_InitRec18Table(work->rec_70C, 1, 0);
    work->obj_694.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

#include "../../shared/limb_shadows_segment.inc.c"

static void func_actor_405800_80132E3C(Task* arg0, s16 arg1, u8 arg2)
{
    limbShadowDrawSegment(arg0, 3, 9, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 9, 0xA, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 0xA, 0xB, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 3, 6, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 6, 7, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 7, 8, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 1, 5, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 1, 0xC, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 0xC, 0xD, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 0xD, 0xE, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 1, 0xF, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 0xF, 0x10, 0x100, arg1, arg2);
    limbShadowDrawSegment(arg0, 0x10, 0x11, 0x100, arg1, arg2);
}

static void func_actor_405800_80132FE0(Task* arg0)
{
    Actor405800Work* work;
    TmdObject*       model;
    GfxCoord*        coord;
    VECTOR           scale;
    SVECTOR          rot;

    work                      = (Actor405800Work*)arg0->work;
    model                     = arg0->extra.tmd;
    coord                     = model->coords;
    work->field_832           = (u16)work->field_832 + ((s16)(0xFF - (u16)work->field_832) >> 4);
    work->field_834           = (u16)work->field_834 + ((s16)(-(u16)work->field_834) >> 4);
    work->field_866           = (u16)work->field_866 + (-work->field_866 >> 2);
    model->shading.colorBlend = work->field_834;
    func_8009EA50(work->field_832);
    work->flags_83C.h.field_83E -= 0x30;
    scale.vx                     = 0x1000;
    scale.vy                     = work->flags_83C.h.field_83E;
    scale.vz                     = 0x1000;
    coord->coord                 = work->matrix_0;
    ScaleMatrix(&coord->coord, &scale);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_842++;
    if ((s16)work->field_842 == 8) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = 0;
        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 3, &rot);
    }
    if ((s16)work->field_842 >= 0x41) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->state++;
    }
}

static void func_actor_405800_8013315C(Task* arg0)
{
    Enemy*           enemy;
    Actor405800Work* work;
    TmdObject*       model;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (Actor405800Work*)arg0->work;
    model = arg0->extra.tmd;
    if ((s8)work->field_895 < 0) {
        if (!(work->field_895 & 1)) {
            switch ((s8)work->field_896) {
                case 0:
                    work->field_832 = (u16)work->field_832 + ((s16)(0xFF - (u16)work->field_832) >> 2);
                    if (work->field_832 >= 0xF8) {
                        work->field_832 = 0xFF;
                        work->field_830 = 0;
                        work->field_896++;
                    }
                    goto block_32;
                case 1:
                    work->field_830++;
                    if (work->field_836 < (s16)work->field_830) {
                        goto block_28;
                    }
                    break;
                case 2:
                    work->field_834 = (u16)work->field_834 + ((s16)(-(u16)work->field_834) >> 2);
                    work->field_866 = (u16)work->field_866 + (-work->field_866 >> 2);
                    if (work->field_834 == 0) {
                        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                        if (work->field_897 == 0) {
                            enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
                        }
                        work->field_866 = 0;
                        work->field_895 = 0;
                    }
                    goto block_26;
            }
        } else {
            switch ((s8)work->field_896) {
                case 0:
                    enemy->node.state.parts.flags = 0;
                    if (work->field_897 == 0) {
                        enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
                    }
                    work->field_834 = (u16)work->field_834 + ((s16)(TMD_OBJECT_COLOR_BLEND_ONE - (u16)work->field_834) >> 2);
                    work->field_866 = (u16)work->field_866 + ((0xFF - work->field_866) >> 2);
                    if (work->field_834 >= TMD_OBJECT_COLOR_BLEND_ONE - 16) {
                        work->field_866 = 0xFF;
                        work->field_834 = TMD_OBJECT_COLOR_BLEND_ONE;
                        work->field_830 = 0;
                        work->field_896++;
                    }
                block_26:
                    model->shading.colorBlend = work->field_834;
                    break;
                case 1:
                    work->field_830++;
                    if ((s16)work->field_830 >= 0x11) {
                    block_28:
                        work->field_896++;
                    }
                    break;
                case 2:
                    work->field_832 = (u16)work->field_832 + ((s16)(-(u16)work->field_832) >> 2);
                    if (work->field_832 < 9) {
                        work->field_832 = 0;
                        work->field_895 = 0;
                        func_actor_405800_8013340C(arg0);
                        if (work->field_83A == 0) {
                            work->field_83A = (u16)work->field_838;
                        }
                    }
                block_32:
                    func_8009EA50(work->field_832);
                    break;
            }
        }
    }
    if (work->field_83A > 0) {
        work->field_83A = (u16)work->field_83A - 1;
    }
}

static void func_actor_405800_8013340C(Task* arg0)
{
    Enemy*           enemy;
    Actor405800Work* work;
    s16              hp;
    s32              maxHp;
    s32              quarter;

    enemy   = (Enemy*)arg0->spawnArg2.pointer;
    hp      = enemy->hp;
    work    = (Actor405800Work*)arg0->work;
    maxHp   = enemy->hpMax << 0x10;
    quarter = maxHp >> 0x12;
    if ((quarter + (maxHp >> 0x11)) < hp) {
        work->field_836 = 0x10;
        work->field_838 = 0;
        return;
    }
    if (quarter < hp) {
        work->field_836 = 0x20;
        work->field_838 = 0x40;
        return;
    }
    if ((maxHp >> 0x13) < hp) {
        work->field_836 = 0x30;
        work->field_838 = 0x80;
        return;
    }
    if ((maxHp >> 0x14) < hp) {
        work->field_836 = 0x40;
        work->field_838 = 0xC0;
        return;
    }
    work->field_836 = 0x50;
    work->field_838 = 0x100;
}

static void func_actor_405800_801334B8(Task* arg0)
{
    TmdObject*       model;
    Enemy*           enemy;
    GfxCoord*        coord;
    Actor405800Work* work;
    Actor405800Work* w2;
    Actor405800Work* w3;
    Actor405800Work* w4;
    TmdObject*       extra;
    u32              rnd;

    model = arg0->extra.tmd;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    coord = model->coords;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(4, 8, 0, 0)) {
        enemyDestroy(enemy, arg0);
        return;
    }
    arg0->work = memCalloc(0x89CU, false);
    work       = (Actor405800Work*)arg0->work;
    if (work == NULL) {
        enemyDestroy(enemy, arg0);
        return;
    }
    gStageSceneMusicEntry = 2;
    model->lightMtx       = &work->matrix_40;
    model->colorMtx       = &work->matrix_20;
    model->flags          = 0;
    arg0->msgTable        = D_actor_405800_8015149C;
    enemy->field_4        = &coord->coord;
    enemy->field_48       = 0;
    enemy->bodyPos.vx     = 0;
    enemy->bodyPos.vy     = 0;
    enemy->bodyPos.vz     = 0;
    enemy->coord          = &arg0->extra.tmd->coords[3];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
    enemy->param                  = &D_actor_405800_801418FC;
    enemy->recs                   = work->rec_4D4;
    work->eff_81C.coord           = &arg0->extra.tmd->coords[3];
    work->eff_81C.spawnArgLo      = 0x100;
    work->eff_81C.spawnArgHi      = 2;
    enemy->hp = enemy->hpMax = D_actor_405800_801418FC.hpMax;
    animationInitContext(&work->anim, D_actor_405800_80151410, model, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->pad_394, work->slots);

    w2              = (Actor405800Work*)arg0->work;
    w2->animStep    = 0x10;
    w2->animClip    = 1;
    w2->animRequest = 2;

    stalkerZebraIvoryTickAnimInline(arg0);

    coord->parent = &gGfxViewCoord;
    func_actor_405800_80132670(arg0);
    func_actor_405800_80135780(arg0);
    (Gp_IncStateF0Ref)(0);
    w3           = (Actor405800Work*)arg0->work;
    w3->state    = 0;
    w3->subState = 0;
    if (gGameSession->location.loc.warp == 1) {
        coord->coord.t[0] = 0x14B4;
        coord->coord.t[2] = 0xD7A;
        coord->coord.t[1] = 0;
        work->yaw         = 0x400;
    } else {
        coord->coord.t[0] = 0x514;
        coord->coord.t[1] = 0;
        coord->coord.t[2] = 0x251C;
        work->yaw         = 0;
    }
    work->field_90  = coord->coord.t[0];
    work->field_92  = coord->coord.t[1];
    work->field_94  = coord->coord.t[2];
    rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState = rnd;
    work->field_840 = rnd >> 0x10;
    work->field_86A = work->field_92;
    w4              = (Actor405800Work*)arg0->work;
    extra           = arg0->extra.tmd;
    w4->field_832   = 0xFF;
    w4->field_834   = 0;
    w4->field_866   = 0;
    w4->field_836   = 0x10;
    func_8009EA50(w4->field_832);
    extra->shading.colorBlend = w4->field_834;
    w3                        = (Actor405800Work*)arg0->work;
    arg0->state               = 1;
    w3->state                 = 0;
    w3->subState              = 0;
}

/// Behaviour handlers `func_actor_405800_80138698` runs by `state`.
static const TaskFuncTable12 D_actor_405800_80131E24 = {
    {
        func_actor_405800_801388E4,
        stalkerZebraIvoryResumeClip,
        stalkerZebraIvoryAnimateUntilDone,
        func_actor_405800_80138A70,
        func_actor_405800_80138B50,
        func_actor_405800_80132FE0,
        func_actor_405800_80138BD4,
        func_actor_405800_80138BEC,
        func_actor_405800_80138C30,
        func_actor_405800_80138CF0,
        func_actor_405800_80138D54,
        func_actor_405800_80138E20,
    },
};

/// The task's four state handlers, run by `func_actor_405800_80138634`.
static const TaskFuncTable4 D_actor_405800_80131E54 = {
    {
        func_actor_405800_801334B8,
        func_actor_405800_80133800,
        func_actor_405800_80138698,
        func_actor_405800_80138788,
    },
};

/// Behaviour handlers `func_actor_405800_80133800` runs by `state`.
static const TaskFuncTable18 D_actor_405800_80131E64 = {
    {
        func_actor_405800_80137A14,
        func_actor_405800_80133CD0,
        func_actor_405800_80137A60,
        func_actor_405800_80137B34,
        func_actor_405800_80137B9C,
        func_actor_405800_80137C04,
        func_actor_405800_80137C78,
        func_actor_405800_80137CEC,
        func_actor_405800_80137D60,
        func_actor_405800_80137DE4,
        func_actor_405800_80137E64,
        func_actor_405800_80137EF0,
        func_actor_405800_80137F58,
        func_actor_405800_80137FCC,
        func_actor_405800_80138040,
        stalkerZebraIvoryRunSubStates,
        func_actor_405800_80138154,
        func_actor_405800_801381BC,
    },
};

/// Sub-state handlers of `func_actor_405800_80137C04`, by `subState`.
static const TaskFuncTable3 D_actor_405800_80131EAC = {
    {
        func_actor_405800_80139358,
        func_actor_405800_801393E8,
        func_actor_405800_801394E4,
    },
};

/// Sub-state handlers of `func_actor_405800_80137D60`, by `subState`.
static const TaskFuncTable5 D_actor_405800_80131EB8 = {
    {
        func_actor_405800_8013967C,
        func_actor_405800_801340E0,
        func_actor_405800_80134314,
        func_actor_405800_8013471C,
        stalkerZebraIvoryReleaseHold,
    },
};

/// Sub-state handlers of `func_actor_405800_80137DE4`, by `subState`.
static const TaskFuncTable4 D_actor_405800_80131ECC = {
    {
        func_actor_405800_801397B8,
        func_actor_405800_801348E4,
        func_actor_405800_80134A64,
        stalkerZebraIvoryWaitClip,
    },
};

/// Sub-state handlers of `func_actor_405800_80137F58`, by `subState`.
static const TaskFuncTable3 D_actor_405800_80131EDC = {
    {
        func_actor_405800_80139880,
        func_actor_405800_80134E80,
        func_actor_405800_801398C0,
    },
};

/// Sub-state handlers of `func_actor_405800_80137FCC`, by `subState`.
static const TaskFuncTable3 D_actor_405800_80131EE8 = {
    {
        func_actor_405800_80139928,
        func_actor_405800_801351BC,
        func_actor_405800_801399C4,
    },
};

/// Sub-state handlers of `func_actor_405800_80138040`, by `subState`.
static const TaskFuncTable4 D_actor_405800_80131EF4 = {
    {
        func_actor_405800_80139AC4,
        func_actor_405800_80139B3C,
        func_actor_405800_80135558,
        stalkerZebraIvoryWaitClipThenRest,
    },
};

/// Sub-state handlers of `stalkerZebraIvoryRunSubStates`, by `subState`.
static const TaskFuncTable3 gStalkerZebraIvorySubStates = {
    {
        func_actor_405800_80139C98,
        func_actor_405800_80139D24,
        func_actor_405800_80139DC0,
    },
};

static __inline__ void Actor405800_ProjectPart(GfxCoord* part)
{
    void**                   scratch;
    u8*                      head;
    Actor405800PerspScratch* block;
    SVECTOR*                 vec;
    MATRIX*                  wm;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (Actor405800PerspScratch*)(head - 0x18);
    SCRATCH_HEAD_AT(scratch, void) = block;
    block->vec.vx                  = 0;
    block->vec.vy                  = 0;
    block->vec.vz                  = 0;
    Gp_UpdateCoord(part);
    vec = &block->vec;
    wm  = &part->workm;
    gte_SetRotMatrix(wm);
    gte_SetTransMatrix(wm);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((Actor405800PerspScratch*)(head - 0x18))->sxy);
    gte_stdp(&((Actor405800PerspScratch*)(head - 0x18))->p);
    gte_stflg(&((Actor405800PerspScratch*)(head - 0x18))->flag);
    gte_stszotz(&((Actor405800PerspScratch*)(head - 0x18))->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> 4) + 0x1E;
    frameCaptureQueue(block->otz);
    SCRATCH_STACK_RELEASE_BLOCK(Actor405800PerspScratch);
}

static void func_actor_405800_80133800(Task* arg0)
{
    TmdObject*       model = arg0->extra.tmd;
    Actor405800Work* work  = (Actor405800Work*)arg0->work;
    GfxCoord*        coord = model->coords;
    Enemy*           enemy = (Enemy*)arg0->spawnArg2.pointer;
    GfxCoord*        part  = &coord[2];
    GfxCoord*        root  = coord;
    TaskFuncTable18  fns   = D_actor_405800_80131E64;
    Actor405800Work* w;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_840++;
            func_actor_405800_801361F8(arg0);
            fns.funcs[(s16)work->state](arg0);
            func_actor_405800_8013795C(arg0);
            func_actor_405800_801375C4(arg0);
            func_actor_405800_8013315C(arg0);
            stalkerZebraIvoryTickAnimInline(arg0);
            work->flags_83C.half = work->slots[1].status.fields.flags;
            root->composeStamp   = GRAPHICS_COORD_DIRTY;
            stalkerZebraIvoryApplyRotationInline(arg0);
            func_actor_405800_80136388(arg0);
            if (enemy->hp <= 0 && (u8)work->holding == 0) {
                w           = (Actor405800Work*)arg0->work;
                arg0->state = 2;
                w->state    = 0;
                w->subState = 0;
            }
        case SCENE_COMBAT_ACTORS_PAUSED:
            actorUpdateModelColor(arg0);
            func_actor_405800_80132E3C(arg0, work->field_86A, work->field_866);
            Actor405800_ProjectPart(part);
            model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

static void func_actor_405800_80133CD0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    u32              sound;
    s32              pan;

    if (((Actor405800Work*)arg0->work)->playerDistance < 0x1450) {
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40050004;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work = (Actor405800Work*)arg0->work;
        if (((s8)work->field_895 >= 0) || ((work->field_895 & 0x7F) != 1)) {
            work->field_895 = 0x81;
            work->field_896 = 0;
        }
        Gp_ArmStateF0(1);
        work2           = (Actor405800Work*)arg0->work;
        work2->state    = 0xC;
        work2->subState = 0;
    }
}

static void func_actor_405800_80133DB0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor405800Work*)arg0->work;
    work->field_842++;
    if ((s16)work->field_842 == 0x16) {
        id = 0x40050005;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->obj_6D4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_694.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if ((s16)work->field_842 == 0x1C) {
        work->obj_6D4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_694.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        stalkerZebraIvorySeedTimer(arg0, 0);
        stalkerZebraIvoryClearQueued(arg0);
        if (work->onCeiling == 0 && work->playerDistance < 0x578 && (u16)(work->field_856 - 0x200) > 0xC00 && (u16)(work->field_854 - 0x200) > 0xC00) {
            work2           = (Actor405800Work*)arg0->work;
            work2->state    = 9;
            work2->subState = 0;
        } else {
            work3           = (Actor405800Work*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_80133F48(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor405800Work*)arg0->work;
    work->field_842++;
    if ((s16)work->field_842 == 0x16) {
        id = 0x40050005;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->obj_6B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_674.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if ((s16)work->field_842 == 0x1C) {
        work->obj_6B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_674.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        stalkerZebraIvorySeedTimer(arg0, 0);
        stalkerZebraIvoryClearQueued(arg0);
        if (work->onCeiling == 0 && work->playerDistance < 0x578 && (u16)(work->field_856 - 0x200) > 0xC00 && (u16)(work->field_854 - 0x200) > 0xC00) {
            work2           = (Actor405800Work*)arg0->work;
            work2->state    = 9;
            work2->subState = 0;
        } else {
            work3           = (Actor405800Work*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_801340E0(Task* arg0)
{
    AnimationPlayRequest     msg;
    GameActorButtonPressHold query;
    Actor405800Work*         work;
    Actor405800Work*         work2;
    Actor405800Work*         work3;
    s32                      base;
    s32                      sound;
    s32                      pan;

    work = (Actor405800Work*)arg0->work;
    if (((GameActor*)gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->work)->mode == GAME_ACTOR_MODE_SCRIPTED || (stalkerZebraIvoryWallDistance(arg0) << 0x10) != 0) {
        stalkerZebraIvoryDropCapsuleGrid(arg0);
        work3           = (Actor405800Work*)arg0->work;
        work3->state    = 2;
        work3->subState = 0;
        func_actor_405800_80135A3C(arg0, work->field_87E);
        return;
    }
    query.pressCount = 0x18;
    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &query, 0) != 0) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        if (work->onCeiling == 0) {
            work3           = (Actor405800Work*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
            return;
        }
        work2           = (Actor405800Work*)arg0->work;
        work2->state    = 0xD;
        work2->subState = 0;
        return;
    }
    work->field_86A = work->field_92;
    stalkerZebraIvoryDropCapsuleGrid(arg0);
    work->onCeiling          = 0;
    Gp_StateC08.flags       |= ATTACHMENT_FLAG_EVENT_LOCK;
    work->holding            = 1;
    work->field_9A           = work->field_92;
    msg.source.sets          = D_actor_405800_801513F8;
    msg.blend                = ANIMATION_BLEND_RESET;
    msg.blendFrames          = 0;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    msg.animationId          = 4;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &msg, 0);
    work->obj_4B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_594.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    work2                = (Actor405800Work*)arg0->work;
    work2->animStep      = 0x10;
    work2->animClip      = 0x21;
    work2->animBlend     = 4;
    work2->animRequest   = 1;
    work->field_842      = 0;
    work->field_844      = 0;
    base                 = 0x40050004;
    if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
        base = 0x404A0004;
    }
    sound = base | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->field_884 = 0;
    work->subState++;
}

static void func_actor_405800_80134314(Task* arg0)
{
    AnimationPlayRequest msg;
    SVECTOR              vec;
    Actor405800Work*     work;
    Actor405800Work*     work2;
    Enemy*               enemy;
    GfxCoord*            coord;
    GfxCoord*            player;
    GfxCoord*            root;
    PlayerStatus*        cfg;
    s32                  id;
    s32                  sound;
    s32                  pan;
    s32                  sound2;
    s32                  pan2;

    work               = (Actor405800Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    enemy              = (Enemy*)arg0->spawnArg2.pointer;
    player             = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work->roll        += -(s16)work->roll >> 2;
    coord->coord.t[0] += (player->coord.t[0] - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += (player->coord.t[2] - coord->coord.t[2]) >> 2;
    coord->coord.t[1] += (player->coord.t[1] - coord->coord.t[1]) >> 2;
    work->field_842++;
    cfg = &gPlayerStatus;
    if (++work->field_844 == 8) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((u8)work->field_88B == 1 || work->holdTaken == 1 || enemy->hp <= 0 || work->field_884 >= 4) {
        work->field_88B = 0;
        if (work->holdTaken == 0) {
            msg.source.sets          = D_actor_405800_801513F8;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 8;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            msg.animationId          = 5;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
        }
        work2              = (Actor405800Work*)arg0->work;
        work2->animBlend   = 8;
        work2->animStep    = 0x10;
        work2->animClip    = 0x22;
        work2->animRequest = 1;
        work->field_84C    = -0x2A;
        work->field_84E    = 0;
        work->field_842    = 0;
        work->subState++;
        return;
    }
    if ((s16)work->field_842 == 1 || (s16)work->field_842 == 0x10 || (s16)work->field_842 == 0x25) {
        root = &gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords[4];
        Gp_SpawnPadLerp(0xA, 0xC0, 8);
        id = 0x40050009;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0009;
        }
        sound2 = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 2), 0);
        vec.vx         = 0;
        vec.vy         = -200;
        vec.vz         = 0;
        work->field_98 = ((rsin((s16)work->yaw + 0x800) * 3000) >> 12) / 20;
        work->field_9C = ((rcos((s16)work->yaw + 0x800) * 3000) >> 12) / 20;
        Gp_SpawnEff(EFFECT_HIT_SPLATTER_SPRAY, root, 0x10100, &vec);
        if (cfg->hp <= 0) {
            work->holdTaken = 1;
        }
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->field_842 = 0;
        work->field_884++;
    }
}

static void func_actor_405800_8013471C(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GfxCoord*        coord;
    GfxCoord*        player;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;

    work        = (Actor405800Work*)arg0->work;
    coord       = arg0->extra.tmd->coords;
    player      = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work->roll += -(s16)work->roll >> 2;
    work->field_842++;
    if ((s16)work->field_842 >= 8) {
        work->obj_594.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        coord->coord.t[0]   += (s16)work->field_98;
        coord->coord.t[2]   += (s16)work->field_9C;
        work->obj_4B4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->field_84C     += 6;
        work->field_84E     += work->field_84C;
        y                    = coord->coord.t[1] + work->field_84E;
        coord->coord.t[1]    = y;
        if (y >= work->field_9A) {
            coord->coord.t[1]    = work->field_9A;
            player->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(player);
            id = 0x40050003;
            if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
                id = 0x404A0003;
            }
            sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            work->obj_594.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work2                = (Actor405800Work*)arg0->work;
            work2->animBlend     = 2;
            work2->animClip      = 0x19;
            work2->animStep      = 0x10;
            work2->animRequest   = 1;
            work->subState++;
        }
    }
}

static void func_actor_405800_801348E4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    GfxCoord*        coord;
    s16              v;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    v     = stalkerZebraIvoryWallDistance(arg0);
    if (v != 0) {
        if (v < 0x4E9) {
            stalkerZebraIvoryDropCapsuleGrid(arg0);
            work3           = (Actor405800Work*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
            return;
        }
        work->field_98 = (u16)coord->coord.t[0] + ((rsin((s16)work->yaw + 0x800) * (v - 0x100)) >> 12);
        work->field_9C = (u16)coord->coord.t[2] + ((rcos((s16)work->yaw + 0x800) * (v - 0x100)) >> 12);
    } else {
        work->field_98 = (u16)coord->coord.t[0] + ((rsin((s16)work->yaw + 0x800) * 0x1770) >> 12);
        work->field_9C = (u16)coord->coord.t[2] + ((rcos((s16)work->yaw + 0x800) * 0x1770) >> 12);
    }
    stalkerZebraIvoryDropCapsuleGrid(arg0);
    work2              = (Actor405800Work*)arg0->work;
    work2->animBlend   = 4;
    work2->animStep    = 0x10;
    work2->animClip    = 0x15;
    work2->animRequest = 1;
    work->field_84C    = -0x2A;
    work->field_84E    = 0;
    work->field_842    = 0;
    work->subState++;
}

static void func_actor_405800_80134A64(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GfxCoord*        coord;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_842++;
    if ((s16)work->field_842 < 0x11) {
        func_actor_405800_80136A1C(arg0);
        return;
    }
    if ((s16)work->field_842 == 0x11) {
        work->pendingAction = 0;
        work->holding       = 1;
    }
    coord->coord.t[0] += ((s16)work->field_98 - coord->coord.t[0]) >> 4;
    coord->coord.t[2] += ((s16)work->field_9C - coord->coord.t[2]) >> 4;
    work->field_84C   += 6;
    work->field_84E   += work->field_84C;
    y                  = coord->coord.t[1] + work->field_84E;
    coord->coord.t[1]  = y;
    if (y >= (s16)work->field_92) {
        coord->coord.t[1] = (s16)work->field_92;
        id                = 0x40050003;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work2              = (Actor405800Work*)arg0->work;
        work2->animBlend   = 2;
        work2->animClip    = 0x19;
        work2->animStep    = 0x10;
        work2->animRequest = 1;
        work->holding      = 0;
        work->subState++;
    }
}

#include "../../shared/stalker_zebra_ivory_right_itself.inc.c"

static void func_actor_405800_80134E80(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GfxCoord*        coord;
    s32              id;
    s32              sound;
    s32              pan;
    s32              y;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_842++;
    if ((s16)work->field_842 < 0x11) {
        func_actor_405800_80136A1C(arg0);
        return;
    }
    if ((s16)work->field_842 == 0x11) {
        work->pendingAction  = 0;
        work->holding        = 1;
        work->obj_594.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if ((u32)(work->field_842 - 0x11) < 4U) {
        work->field_866 = (u16)work->field_866 + (-work->field_866 >> 1);
    }
    if ((s16)work->field_842 == 0x15) {
        work->field_86A = -0x9C4;
    }
    if ((s16)work->field_842 >= 0x15) {
        work->field_866 = (u16)work->field_866 + ((0xFF - work->field_866) >> 1);
    }
    y                  = coord->coord.t[1] + 0x190;
    coord->coord.t[1] += ((s16)work->field_9A - y) >> 3;
    work->pitch       += (0x800 - (s16)work->pitch) >> 3;
    if ((s16)work->field_9A >= coord->coord.t[1]) {
        work->field_866 = 0xFF;
        id              = 0x40050003;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->obj_594.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        coord->coord.t[1]    = (s16)work->field_9A;
        work->pitch          = 0;
        work->roll           = 0x800;
        work->yaw           += 0x800;
        stalkerZebraIvoryApplyRotationInline(arg0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        work2              = (Actor405800Work*)arg0->work;
        work2->animBlend   = 2;
        work2->animStep    = 0x10;
        work2->animClip    = 0x19;
        work2->animRequest = 1;
        work->holding      = 0;
        work->onCeiling    = 1;
        work->subState++;
    }
}

static void func_actor_405800_801351BC(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GfxCoord*        coord;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_842++;
    if ((s16)work->field_842 < 8) {
        work->field_866 = (u16)work->field_866 + (-work->field_866 >> 1);
        stalkerZebraIvoryReadPartViewXZ(arg0, 3, &work->field_88);
        return;
    }
    work->field_866   = (u16)work->field_866 + ((0xFF - work->field_866) >> 1);
    work->field_86A   = work->field_92;
    work->field_88.x += ((s16)work->field_98 - work->field_88.x) >> 2;
    work->field_88.z += ((s16)work->field_9C - work->field_88.z) >> 2;
    stalkerZebraIvoryPinPartXZ(arg0, 3, &work->field_88);
    work->field_84C   += 2;
    work->field_84E   += work->field_84C;
    coord->coord.t[1] += work->field_84E;
    if ((work->pitch & 0xFFF) != 0x800) {
        work->pitch -= 0x80;
    }
    if ((s16)work->field_92 < coord->coord.t[1]) {
        work->onCeiling   = 0;
        work->field_866   = 0xFF;
        coord->coord.t[0] = (s16)work->field_98;
        coord->coord.t[1] = (s16)work->field_92;
        coord->coord.t[2] = (s16)work->field_9C;
        work->pitch       = 0;
        work->roll        = 0;
        work->yaw        += 0x800;
        stalkerZebraIvoryApplyRotationInline(arg0);
        work2              = (Actor405800Work*)arg0->work;
        work2->animBlend   = 2;
        work2->animStep    = 0x10;
        work2->animClip    = 0x19;
        work2->animRequest = 1;
        stalkerZebraIvoryTickAnimInline(arg0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        work->field_842 = 0;
        work->subState++;
    }
}

static void func_actor_405800_80135558(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    u32              sound;
    s32              pan;
    u32              rnd;

    work = (Actor405800Work*)arg0->work;
    if ((s16)work->field_842 == 0) {
        work->holding = 0;
        sound         = 0x40050006 | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan           = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan         >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_842++;
    }
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->pendingAction != 3) {
            work2              = (Actor405800Work*)arg0->work;
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
        work3               = (Actor405800Work*)arg0->work;
        work3->state        = 5;
        work3->subState     = 0;
    }
}

static void func_actor_405800_801356A8(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    u16              count;
    u32              rnd;

    work = (Actor405800Work*)arg0->work;
    if (((func_actor_405800_80136A1C(arg0) << 0x10) == 0) && ((func_actor_405800_801373E0(arg0) << 0x10) == 0)) {
        count           = (u16)work->field_882 - 1;
        work->field_882 = count;
        if ((count << 0x10) == 0) {
            rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            work->field_838 = ((rnd >> 0x10) & 0x3F) + 0x1E;
            work2           = (Actor405800Work*)arg0->work;
            gRandomLcgState = rnd;
            if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
                work2->field_895 = 0x81;
                work2->field_896 = 0;
            }
            work3           = (Actor405800Work*)arg0->work;
            work3->state    = 2;
            work3->subState = 0;
        }
    }
}

/// Spawn the two child models from `D_actor_405800_801514B4`, parent them to
/// root parts 10 and 7 at +/-0x400 along X, turn each by -/+0x180 from an
/// identity rotation, copy the parent's texture page and CLUT row, and point
/// their light / color matrices at this actor's own.
static void func_actor_405800_80135780(Task* arg0)
{
    Actor405800Work* work;
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

    root                     = arg0->extra.tmd->coords;
    work                     = (Actor405800Work*)arg0->work;
    parent                   = &root[7];
    parent2                  = &root[10];
    task                     = Task_SpawnFromTable(D_actor_405800_801514B4, 0, 0, 0);
    work->field_824          = task;
    obj                      = task->extra.tmd;
    coord                    = obj->coords;
    obj->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->parent            = parent2;
    coord->coord.t[0]        = 0x400;
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
    task = work->field_828 = Task_SpawnFromTable(D_actor_405800_801514B4, 1, 0, 0);
    obj                    = task->extra.tmd;
    coord                  = obj->coords;
    obj->flags             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->parent          = parent;
    coord->coord.t[0]      = -0x400;
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

/// Animation state 2: the landing slam. Same two sound/tracking windows as
/// `func_actor_400600_80135998`, one frame-count pair per sound event.
static void func_actor_405800_80135A3C(Task* arg0, s16 arg1)
{
    Actor405800Work* work;
    GfxCoord*        coord;
    /* The first window starts at frame 0. `start0` is still its own `u8`: the
     * width is what folds both of its tests against a literal zero, and the
     * wider first temp below is what keeps the zero arm a fresh constant
     * instead of a copy of it. */
    u8  start0;
    u32 tmp0;
    u8  tmp1;
    u8  tmp2;
    u8  end0;
    u8  start1;
    u8  end1;
    s32 id;
    u32 sound;
    u32 voice;
    s32 pan;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->animClip != 2) {
        work->animStep    = 0x10;
        work->animClip    = 2;
        work->animRequest = 2;
        stalkerZebraIvoryTickAnimInline(arg0);
    }
    start0 = 0;
    if (((Actor405800Work*)arg0->work)->animStep == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((Actor405800Work*)arg0->work)->animStep) >> 4;
    }
    end0 = tmp0;
    if (((Actor405800Work*)arg0->work)->animStep == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((Actor405800Work*)arg0->work)->animStep) >> 4;
    }
    start1 = tmp1;
    if (((Actor405800Work*)arg0->work)->animStep == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((Actor405800Work*)arg0->work)->animStep) >> 4;
    }
    end1 = tmp2;
    if ((func_actor_405800_80137908(arg0) << 0x10) != 0) {
        work->animFrame = 0;
        work->animStep  = arg1;
    }
    if (work->animFrame == start0) {
        stalkerZebraIvoryReadPartViewXZ(arg0, 0xB, &work->field_88);
        id = 0x40050001;
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
    }
    if (work->animFrame == start1) {
        stalkerZebraIvoryReadPartViewXZ(arg0, 8, &work->field_88);
        id = 0x40050002;
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
    }
    if (work->animFrame >= start0 && work->animFrame <= end0) {
        stalkerZebraIvoryPinPartXZ(arg0, 0xB, &work->field_88);
        work->field_860 = 8;
    }
    if (work->animFrame >= start1 && work->animFrame <= end1) {
        stalkerZebraIvoryPinPartXZ(arg0, 8, &work->field_88);
        work->field_860 = 0xB;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/stalker_zebra_ivory_step_clip4.inc.c"

static void func_actor_405800_801361F8(Task* arg0)
{
    Actor405800Work* work;
    GfxCoord*        coord;
    GfxCoord*        player;
    GameActor*       actor;
    SVECTOR          v;
    s16              py;

    work              = (Actor405800Work*)arg0->work;
    coord             = arg0->extra.tmd->coords;
    arg0              = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work->field_70.vx = coord->coord.t[0];
    work->field_70.vy = coord->coord.t[1];
    work->field_70.vz = coord->coord.t[2];
    if (arg0 == NULL) {
        return;
    }
    player = arg0->extra.tmd->coords;
    actor  = arg0->work;
    if (player->coord.t[0] < 0x3A98 || gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_IDLE) {
        work->field_A8.vx = (u16)player->coord.t[0];
        work->field_A8.vy = (u16)player->coord.t[1];
        work->field_A8.vz = (u16)player->coord.t[2];
    } else {
        work->field_A8.vx = 0x834;
        py                = (u16)player->coord.t[1];
        work->field_A8.vz = 0xD48;
        work->timer       = 2;
        work->field_A8.vy = py;
    }
    v.vx                 = (u16)work->field_A8.vx - (u16)coord->coord.t[0];
    v.vy                 = (u16)work->field_A8.vy - (u16)coord->coord.t[1];
    v.vz                 = (u16)work->field_A8.vz - (u16)coord->coord.t[2];
    work->playerDistance = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
    VectorNormalSS(&v, &v);
    work->field_856 = (ratan2(v.vx, v.vz) - work->yaw) & 0xFFF;
    work->field_854 = (ratan2(-v.vx, -v.vz) - actor->rotation.vy) & 0xFFF;
}

static void func_actor_405800_80136388(Task* arg0)
{
    WorldCollisionDelta delta;
    s16                 maxX;
    s16                 maxZ;
    s16                 stepX;
    s16                 stepZ;
    GfxCoord*           coord;
    u8                  blocked;
    Actor405800Work*    work;
    Enemy*              enemy;
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
    work               = (Actor405800Work*)arg0->work;
    enemy              = (Enemy*)arg0->spawnArg2.pointer;
    work->pendingArmed = 0;

    for (i = 0; i < 8; i++) {
        if ((work->rec_4D4[i].key.value & 0xFFFF0000) == 0x20000) {
            if (work->field_876 == 0) {
                work->pendingArmed = 1;
                dmg                = Gp_ComputeDamage(work->rec_4D4[i].key.value, work->playerDistance, 0, 0);
                amount             = dmg;
                work->field_876    = Gp_GetIdParam2(work->rec_4D4[i].key.value);
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
                func_800FDB18(Gp_GetIdParam1(work->rec_4D4[i].key.value) & 0xFFFF,
                              &arg0->extra.tmd->coords[4], NULL, &work->eff_81C);
                if (amount >= 0xB4) {
                    work->pendingAction = 2;
                } else if (amount >= 0x78) {
                    work->pendingAction = 1;
                } else {
                    work->pendingAction = 0;
                }
                switch (Gp_GetIdParam0(work->rec_4D4[i].key.value) & 0xFFFF) {
                    case 0:
                        break;
                    case 1:
                        Gp_SetObjFlag1(enemy);
                        break;
                    case 2:
                        Gp_SetObjFlag2(enemy, work->rec_4D4[i].key.value, 0);
                        work->field_898 = 2;
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
                    case 9:
                        if (work->field_898 != 2) {
                            work->field_898     = 1;
                            work->pendingAction = 3;
                        }
                        break;
                }
            } else if ((Gp_GetIdParam1(work->rec_4D4[i].key.value) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &arg0->extra.tmd->coords[1], NULL, &work->eff_81C);
            }
        }
    }

    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->pendingAction   = 5;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->pendingAction   = 3;
        work->field_898       = 2;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        work->field_888 = 1;
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

    switch (func_800E0C10(work->rec_5B4, &delta, 8, NULL)) {
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
            coord->coord.t[0] = work->field_70.vx;
            blocked           = 1;
            coord->coord.t[2] = work->field_70.vz;
            break;
    }

    Gp_ClearRec18Occupied(work->rec_4D4);
    Gp_ClearRec18Occupied(work->rec_5B4);
    if (work->field_876 > 0) {
        work->field_876--;
    } else {
        work->field_876 = 0;
    }
    if (blocked == 0) {
        work->field_88.x   += actorPickStep(stepX, maxX >> 3);
        work->field_88.z   += actorPickStep(stepZ, maxZ >> 3);
        coord->coord.t[0]  += actorPickStep(stepX, (u16)maxX >> 3);
        coord->coord.t[2]  += actorPickStep(stepZ, (u16)maxZ >> 3);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

static s32 func_actor_405800_80136A1C(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->pendingArmed == 1) {
        if (work->onCeiling == 0) {
            switch (work->pendingAction) {
                case 1:
                    work->pendingAction = 0;
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 3;
                    work2->subState     = 0;
                    return 1;
                case 2:
                    work->pendingAction = 0;
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    return 1;
                case 3:
                    work->pendingAction = 0;
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 5;
                    work2->subState     = 0;
                    return 1;
                case 4:
                    work->pendingAction = 0;
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    return 1;
                case 5:
                    work->pendingAction = 0;
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 0xF;
                    work2->subState     = 0;
                    return 1;
            }
            return 0;
        } else {
            switch (work->pendingAction) {
                case 1:
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 3;
                    work2->subState     = 0;
                    work->pendingAction = 0;
                    return 1;
                case 2:
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    work->pendingAction = 0;
                    return 1;
                case 3:
                    work2           = (Actor405800Work*)arg0->work;
                    work2->state    = 0xE;
                    work2->subState = 0;
                    return 1;
                case 4:
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    work->pendingAction = 0;
                    return 1;
                case 5:
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 0xE;
                    work2->subState     = 0;
                    work->pendingAction = 0;
                    return 1;
            }
            return 0;
        }
    }
    return 0;
}

#include "../../shared/stalker_zebra_ivory_take_pending.inc.c"

static s32 func_actor_405800_80136CE0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->pendingArmed == 1) {
        if (work->onCeiling == 0) {
            switch (work->pendingAction) {
                case 1:
                    work->pendingAction = 0;
                    return 0;
                case 2:
                    work->pendingAction = 0;
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    return 1;
                case 3:
                    work->pendingAction = 0;
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 5;
                    work2->subState     = 0;
                    return 1;
                case 4:
                    work->pendingAction = 0;
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    return 1;
                case 5:
                    work->pendingAction = 0;
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 0xF;
                    work2->subState     = 0;
                    return 1;
            }
            work->pendingAction = 0;
            return 0;
        } else {
            switch (work->pendingAction) {
                case 1:
                    work->pendingAction = 0;
                    return 0;
                case 2:
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    work->pendingAction = 0;
                    return 1;
                case 3:
                    work2           = (Actor405800Work*)arg0->work;
                    work2->state    = 0xE;
                    work2->subState = 0;
                    return 1;
                case 4:
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 4;
                    work2->subState     = 0;
                    work->pendingAction = 0;
                    return 1;
                case 5:
                    work2               = (Actor405800Work*)arg0->work;
                    work2->state        = 0xE;
                    work2->subState     = 0;
                    work->pendingAction = 0;
                    return 1;
            }
            return 0;
        }
    }
    return 0;
}

static void func_actor_405800_80136E14(Task* task)
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

    D_800678F0[0] = &_gActor405800IvoryStalkerBurstHead;
    eff           = Gp_SpawnEff(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[5], 0x200, NULL);
    if (eff != NULL) {
        src                    = task->extra.tmd;
        dst                    = eff->task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    D_800678F0[0] = &_gActor405800StalkerBurstTorso;
    eff2          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[13], 0x200, NULL);
    if (eff2 != NULL) {
        src2                    = task->extra.tmd;
        dst2                    = eff2->task->extra.tmd;
        dst2->texturePageOffset = src2->texturePageOffset;
        dst2->clutRowOffset     = src2->clutRowOffset;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    D_800678F0[0] = &_gActor405800StalkerEffect;
    eff3          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[16], 0x200, NULL);
    if (eff3 != NULL) {
        src3                    = task->extra.tmd;
        dst3                    = eff3->task->extra.tmd;
        dst3->texturePageOffset = src3->texturePageOffset;
        dst3->clutRowOffset     = src3->clutRowOffset;
        if (dst3->buffer != NULL) {
            tmdProcessStream(dst3);
            tmdProcessStream(dst3);
        }
    }
    D_800678F0[0] = &_gActor405800StalkerBurstHandLeft;
    eff4          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[10], 0x200, NULL);
    if (eff4 != NULL) {
        src4                    = task->extra.tmd;
        dst4                    = eff4->task->extra.tmd;
        dst4->texturePageOffset = src4->texturePageOffset;
        dst4->clutRowOffset     = src4->clutRowOffset;
        if (dst4->buffer != NULL) {
            tmdProcessStream(dst4);
            tmdProcessStream(dst4);
        }
    }
    Gp_SpawnEff(EFFECT_030, &task->extra.tmd->coords[1], 0x200, NULL);
    Gp_SpawnEff(EFFECT_030, &task->extra.tmd->coords[2], 0x200, NULL);
    Gp_SpawnEff(EFFECT_030, &task->extra.tmd->coords[3], 0x200, NULL);
}

static void func_actor_405800_8013706C(Task* arg0, s16 arg1)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;
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
            work->capsule.ends[0].vz = -0x1770;
            work->capsule.end0Radius = 0x50;
            work->capsule.end1Radius = 0x50;
            break;
    }
    work->capsule.ends[1].vx = 0;
    work->capsule.ends[1].vy = 0;
    work->capsule.ends[1].vz = 0;
    Gp_ClearRec18Occupied(work->capsuleContacts);
    work->capsuleBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
}

#include "../../shared/stalker_zebra_ivory_wall_distance.inc.c"

/// `func_actor_405800_80139FB0`'s body, inlined: switch to behaviour `id`
/// and restart its step counter.
static __inline__ void _actor405800SetBehaviour(Task* task, s16 id)
{
    Actor405800Work* work = (Actor405800Work*)task->work;

    work->state    = id;
    work->subState = 0;
}

static s32 func_actor_405800_801373E0(Task* arg0)
{
    Actor405800Work* work;
    GfxCoord*        coord;
    u32              rnd;
    u32              bits;
    s16              ang;

    work            = (Actor405800Work*)arg0->work;
    coord           = arg0->extra.tmd->coords;
    rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    bits            = rnd >> 0x10;
    gRandomLcgState = rnd;
    if (work->timer == 0) {
        if (work->onCeiling == 0) {
            if ((bits & 0xF) == 0) {
                if ((work->field_85E == 0) && (coord->coord.t[0] < 0x2710)) {
                    _actor405800SetBehaviour(arg0, 0xC);
                    return 1;
                }
                return 0;
            }
            if ((bits & 7) >= 1 && (bits & 7) <= 3) {
                if ((work->playerDistance < 0x7D0) && (work->field_856 < 0x200 || work->field_856 > 0xE00) && (work->field_854 > 0x600 && work->field_854 < 0xA00)) {
                    _actor405800SetBehaviour(arg0, 8);
                    return 1;
                }
            } else if (work->playerDistance < 0x640) {
                ang = (s16)work->field_856;
                if (ang < 0x400) {
                    _actor405800SetBehaviour(arg0, 6);
                    return 1;
                }
                if (ang >= 0xC01) {
                    _actor405800SetBehaviour(arg0, 7);
                    return 1;
                }
            }
        } else if ((bits & 7) == 0) {
            if (work->field_85E == 0) {
                _actor405800SetBehaviour(arg0, 0x10);
                return 1;
            }
            return 0;
        } else if ((bits & 0xF) == 1) {
            if (work->field_85E == 0) {
                _actor405800SetBehaviour(arg0, 0xD);
                return 1;
            }
            return 0;
        } else if (work->playerDistance < 0x640) {
            ang = (s16)work->field_856;
            if (ang >= 0xC01) {
                _actor405800SetBehaviour(arg0, 6);
                return 1;
            }
            if (ang < 0x400) {
                _actor405800SetBehaviour(arg0, 7);
                return 1;
            }
            return 0;
        } else {
            return 0;
        }
    }
    return 0;
}

static void func_actor_405800_801375C4(Task* task)
{
    Actor405800Work* work;
    Task*            child;
    TmdObject*       extra;
    GfxMatrix        rot;
    GfxMatrix*       m;
    GfxMatrix*       m2;
    s16              angle;
    MATRIX*          local;

    work = (Actor405800Work*)task->work;
    if ((u8)work->queuedMode != 0) {
        angle                    = (u16)work->field_87A + ((0x380 - work->field_87A) >> 2);
        work->field_87A          = angle;
        child                    = ((Actor405800Work*)task->work)->field_828;
        child->extra.tmd->flags  = 0;
        extra                    = child->extra.tmd;
        local                    = &extra->coords->coord;
        m                        = &rot;
        rot.rotationWords.m00M01 = ONE;
        rot.rotationWords.m02M10 = 0;
        m->rotationWords.m11M12  = ONE;
        rot.rotationWords.m20M21 = 0;
        m->rotationWords.m22     = ONE;
        RotMatrixY(angle, &m->mat);
        local->m[0][0] = rot.mat.m[0][0];
        local->m[0][1] = rot.mat.m[0][1];
        local->m[0][2] = rot.mat.m[0][2];
        local->m[1][0] = rot.mat.m[1][0];
        local->m[1][1] = rot.mat.m[1][1];
        local->m[1][2] = rot.mat.m[1][2];
        local->m[2][0] = rot.mat.m[2][0];
        local->m[2][1] = rot.mat.m[2][1];
        local->m[2][2] = rot.mat.m[2][2];
    } else {
        work->obj_6B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_674.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        angle                = (u16)work->field_87A + (-work->field_87A >> 3);
        work->field_87A      = angle;
        if (angle < 9) {
            ((Actor405800Work*)task->work)->field_828->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            child                    = ((Actor405800Work*)task->work)->field_828;
            child->extra.tmd->flags  = 0;
            extra                    = child->extra.tmd;
            local                    = &extra->coords->coord;
            m                        = &rot;
            rot.rotationWords.m00M01 = ONE;
            rot.rotationWords.m02M10 = 0;
            m->rotationWords.m11M12  = ONE;
            rot.rotationWords.m20M21 = 0;
            m->rotationWords.m22     = ONE;
            RotMatrixY(angle, &m->mat);
            local->m[0][0] = rot.mat.m[0][0];
            local->m[0][1] = rot.mat.m[0][1];
            local->m[0][2] = rot.mat.m[0][2];
            local->m[1][0] = rot.mat.m[1][0];
            local->m[1][1] = rot.mat.m[1][1];
            local->m[1][2] = rot.mat.m[1][2];
            local->m[2][0] = rot.mat.m[2][0];
            local->m[2][1] = rot.mat.m[2][1];
            local->m[2][2] = rot.mat.m[2][2];
        }
    }

    if ((u8)work->queuedFlag != 0) {
        angle                    = (u16)work->field_878 + ((0x380 - work->field_878) >> 2);
        work->field_878          = angle;
        child                    = ((Actor405800Work*)task->work)->field_824;
        angle                    = -angle;
        child->extra.tmd->flags  = 0;
        extra                    = child->extra.tmd;
        local                    = &extra->coords->coord;
        m                        = &rot;
        rot.rotationWords.m00M01 = ONE;
        rot.rotationWords.m02M10 = 0;
        m->rotationWords.m11M12  = ONE;
        rot.rotationWords.m20M21 = 0;
        m->rotationWords.m22     = ONE;
        RotMatrixY(angle, &m->mat);
        local->m[0][0] = rot.mat.m[0][0];
        local->m[0][1] = rot.mat.m[0][1];
        local->m[0][2] = rot.mat.m[0][2];
        local->m[1][0] = rot.mat.m[1][0];
        local->m[1][1] = rot.mat.m[1][1];
        local->m[1][2] = rot.mat.m[1][2];
        local->m[2][0] = rot.mat.m[2][0];
        local->m[2][1] = rot.mat.m[2][1];
        local->m[2][2] = rot.mat.m[2][2];
    } else {
        work->obj_6D4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_694.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        angle                = (u16)work->field_878 + (-work->field_878 >> 3);
        work->field_878      = angle;
        if (angle < 9) {
            ((Actor405800Work*)task->work)->field_824->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            child                    = ((Actor405800Work*)task->work)->field_824;
            child->extra.tmd->flags  = 0;
            extra                    = child->extra.tmd;
            local                    = &extra->coords->coord;
            m2                       = &rot;
            rot.rotationWords.m00M01 = ONE;
            rot.rotationWords.m02M10 = 0;
            m2->rotationWords.m11M12 = ONE;
            rot.rotationWords.m20M21 = 0;
            m2->rotationWords.m22    = ONE;
            RotMatrixY((s16)-angle, &m2->mat);
            local->m[0][0] = rot.mat.m[0][0];
            local->m[0][1] = rot.mat.m[0][1];
            local->m[0][2] = rot.mat.m[0][2];
            local->m[1][0] = rot.mat.m[1][0];
            local->m[1][1] = rot.mat.m[1][1];
            local->m[1][2] = rot.mat.m[1][2];
            local->m[2][0] = rot.mat.m[2][0];
            local->m[2][1] = rot.mat.m[2][1];
            local->m[2][2] = rot.mat.m[2][2];
        }
    }

    if ((u32)(work->state - 6) >= 2U) {
        work->obj_6B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_6D4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_674.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj_694.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
}

static s32 func_actor_405800_80137908(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;

    if ((work->flags_83C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_83C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}

#include "../../shared/stalker_zebra_ivory_clear_queued.inc.c"

static void func_actor_405800_8013795C(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    if (work->timer > 0) {
        work->timer = work->timer - 1;
    }
    if (work->field_85E > 0) {
        work->field_85E = work->field_85E - 1;
    }
}

#include "../../shared/stalker_zebra_ivory_seed_timer.inc.c"

#include "../../shared/stalker_zebra_ivory_drop_capsule_grid.inc.c"

static void func_actor_405800_80137A14(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* cur;

    work = (Actor405800Work*)task->work;
    if (((s8)work->field_895 >= 0 || (work->field_895 & 0x7F)) && work->field_83A == 0) {
        work->field_895 = 0x80;
        work->field_896 = 0;
    }
    cur           = (Actor405800Work*)task->work;
    cur->state    = 1;
    cur->subState = 0;
}

/// Per-frame entry point for one of this actor's states: clears the animation
/// request flags, then runs the sub-state handler `subState` selects unless
/// `func_actor_405800_80136A1C` or `func_actor_405800_801373E0` already
/// consumed the frame. After the handler, a set `onCeiling` plus a root
/// world X past 10000 switches to state 0xD. The two-entry table is small
/// enough that GCC materialises each callback with its own `lui`/`addiu`
/// pair instead of copying a `.rodata` pool.
static void func_actor_405800_80137A60(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80138FA8, func_actor_405800_8013902C };

    stalkerZebraIvoryClearQueued(task);
    if ((s16)func_actor_405800_80136A1C(task) == 0 && (s16)func_actor_405800_801373E0(task) == 0) {
        states[(s16)work->subState](task);
        if (work->onCeiling != 0 && task->extra.tmd->coords->coord.t[0] > 10000) {
            Actor405800Work* cur = (Actor405800Work*)task->work;

            cur->state    = 0xD;
            cur->subState = 0;
        }
    }
}

/// Per-frame entry point for one of this actor's states: clears the animation
/// request flags, then runs the sub-state handler `subState` selects. The
/// two-entry table is small enough that GCC materialises each callback with its
/// own `lui`/`addiu` pair instead of copying a `.rodata` pool.
static void func_actor_405800_80137B34(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_801390FC, func_actor_405800_80139188 };

    stalkerZebraIvoryClearQueued(task);
    states[(s16)work->subState](task);
}

static void func_actor_405800_80137B9C(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80139260, func_actor_405800_801392EC };

    stalkerZebraIvoryClearQueued(task);
    states[(s16)work->subState](task);
}

/// Per-frame handler for one of this actor's behaviours: clears `queuedMode` /
/// `queuedFlag` through `stalkerZebraIvoryClearQueued`, then runs the sub-state
/// handler of `D_actor_405800_80131EAC` that `subState` selects.
static void func_actor_405800_80137C04(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable3   states = D_actor_405800_80131EAC;

    stalkerZebraIvoryClearQueued(task);
    states.funcs[(s16)work->subState](task);
}

static void func_actor_405800_80137C78(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80139550, func_actor_405800_80133DB0 };

    if ((s16)func_actor_405800_80136CE0(task) == 0) {
        states[(s16)work->subState](task);
    }
}

static void func_actor_405800_80137CEC(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_801395E8, func_actor_405800_80133F48 };

    if ((s16)func_actor_405800_80136CE0(task) == 0) {
        states[(s16)work->subState](task);
    }
}

static void func_actor_405800_80137D60(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable5   states = D_actor_405800_80131EB8;

    stalkerZebraIvoryClearQueued(task);
    states.funcs[(s16)work->subState](task);
}

static void func_actor_405800_80137DE4(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable4   states = D_actor_405800_80131ECC;

    stalkerZebraIvoryClearQueued(task);
    states.funcs[(s16)work->subState](task);
}

static void func_actor_405800_80137E64(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    s16              count;

    work = (Actor405800Work*)task->work;
    stalkerZebraIvoryClearQueued(task);
    if ((s16)func_actor_405800_80136A1C(task) == 0) {
        count           = work->countdown - 1;
        work->countdown = count;
        if (count == 0) {
            work2           = (Actor405800Work*)task->work;
            work2->state    = 0xB;
            work2->subState = 0;
            return;
        }
        stalkerZebraIvoryTurnToward(task, &work->field_A8, 0x18);
        stalkerZebraIvoryStepClip4(task);
    }
}

static void func_actor_405800_80137EF0(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80139844, stalkerZebraIvoryRightItself };

    stalkerZebraIvoryClearQueued(task);
    states[(s16)work->subState](task);
}

static void func_actor_405800_80137F58(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable3   states = D_actor_405800_80131EDC;

    stalkerZebraIvoryClearQueued(task);
    states.funcs[(s16)work->subState](task);
}

static void func_actor_405800_80137FCC(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable3   states = D_actor_405800_80131EE8;

    stalkerZebraIvoryClearQueued(task);
    states.funcs[(s16)work->subState](task);
}

static void func_actor_405800_80138040(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable4   states = D_actor_405800_80131EF4;

    stalkerZebraIvoryClearQueued(task);
    states.funcs[(s16)work->subState](task);
}

#include "../../shared/stalker_zebra_ivory_run_sub_states.inc.c"

static void func_actor_405800_80138154(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80139E2C, stalkerZebraIvoryPickRange };

    stalkerZebraIvoryClearQueued(task);
    states[(s16)work->subState](task);
}

static void func_actor_405800_801381BC(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80139EAC, func_actor_405800_801356A8 };

    stalkerZebraIvoryClearQueued(task);
    states[(s16)work->subState](task);
}

#include "../../shared/stalker_zebra_ivory_restart_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_blend_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_scale_frame.inc.c"

#include "../../shared/stalker_zebra_ivory_turn_toward.inc.c"

#include "../../shared/stalker_zebra_ivory_part_view_xz.inc.c"

#include "../../shared/stalker_zebra_ivory_pin_part_xz.inc.c"

#include "../../shared/stalker_zebra_ivory_clip_done.inc.c"

/// Per-frame entry point of the actor's task: runs whichever of the four
/// handlers in `D_actor_405800_80131E54` the task's `state` selects. The table
/// is a local, so GCC copies it from `.rodata` onto the stack every frame.
void func_actor_405800_80138634(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_405800_80131E54;
    states.funcs[task->state](task);
}

static void func_actor_405800_80138698(Task* arg0)
{
    TmdObject*       model = arg0->extra.tmd;
    Actor405800Work* work  = (Actor405800Work*)arg0->work;
    TaskFuncTable12  fns   = D_actor_405800_80131E24;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            fns.funcs[(s16)work->state](arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            stalkerZebraIvoryUpdateColor(arg0);
            func_actor_405800_80132E3C(arg0, work->field_86A, work->field_866);
            break;
    }
}

static void func_actor_405800_80138788(Task* arg0)
{
    Actor405800Work* work                = (Actor405800Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_405800_80138EF0,
        func_actor_405800_80138F54,
    };

    states[(s16)work->state](arg0);
}

#include "../../shared/stalker_zebra_ivory_update_color.inc.c"

#include "../../shared/stalker_zebra_ivory_set_move_mode.inc.c"

void func_actor_405800_801388C4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    ((Actor405800Work*)task->work)->field_88B = 1;
}

void func_actor_405800_801388D4(Task* task)
{
}

void func_actor_405800_801388DC(Task* task)
{
}

static void func_actor_405800_801388E4(Task* task)
{
    Enemy*           enemy;
    Actor405800Work* work;
    TmdObject*       model;

    enemy                = (Enemy*)task->spawnArg2.pointer;
    work                 = (Actor405800Work*)task->work;
    model                = task->extra.tmd;
    work->obj_6D4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_6B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_694.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_674.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldTargetUnlinkNode(&enemy->node);
    if (work->pendingAction == 4) {
        work->field_842 = 0;
        model->flags   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        func_actor_405800_80139FB0(task, 7);
    } else if (work->onCeiling == 0) {
        work->state = work->state + 1;
    } else {
        func_actor_405800_80139FB0(task, 9);
    }
}

#include "../../shared/stalker_zebra_ivory_resume_clip.inc.c"

#include "../../shared/stalker_zebra_ivory_animate_until_done.inc.c"

static void func_actor_405800_80138A70(Task* task)
{
    Actor405800Work* work  = (Actor405800Work*)task->work;
    GfxCoord*        coord = task->extra.tmd->coords;

    ((Enemy*)task->spawnArg2.pointer)->recs = 0;
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_6B4);
    Gp_UnlinkObj(&work->obj_6D4);
    Gp_UnlinkObj(&work->obj_674);
    Gp_UnlinkObj(&work->obj_694);
    Gp_UnlinkObj(&work->capsuleBody);
    work->flags_83C.h.field_83E = 0x1000;
    work->matrix_0              = coord->coord;
    Gp_SetLightMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->field_842 = 0;
    work->state++;
}

static void func_actor_405800_80138B50(Task* task)
{
    Actor405800Work* work;
    TmdObject*       ext;
    u16              count;

    work            = (Actor405800Work*)task->work;
    ext             = task->extra.tmd;
    count           = work->field_842 + 1;
    work->field_842 = count;
    if ((s16)count >= 0x18) {
        work->field_832 = 0;
        work->field_834 = TMD_OBJECT_COLOR_BLEND_ONE;
        work->field_866 = 0xFF;
        func_8009EA50(work->field_832);
        ext->shading.colorBlend = work->field_834;
        work->field_842         = 0;
        work->state             = work->state + 1;
    }
}

static void func_actor_405800_80138BD4(Task* task)
{
    Actor405800Work* work;

    work           = (Actor405800Work*)task->work;
    task->state    = 3;
    work->state    = 0;
    work->subState = 0;
}

static void func_actor_405800_80138BEC(Task* task)
{
    Actor405800Work* work;
    u16              count;

    work            = (Actor405800Work*)task->work;
    count           = work->field_842 + 1;
    work->field_842 = count;
    if ((s16)count >= 2) {
        work->state = work->state + 1;
    }
}

static void func_actor_405800_80138C30(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    TmdObject*       model;
    Enemy*           enemy;

    model = task->extra.tmd;
    work  = (Actor405800Work*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    Tmd_FreeBuffers(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    func_actor_405800_80136E14(task);
    work->field_866 = 0;
    Gp_ReleaseStateF0Add(task, 0);
    enemy->recs = 0;
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_6B4);
    Gp_UnlinkObj(&work->obj_6D4);
    Gp_UnlinkObj(&work->obj_674);
    Gp_UnlinkObj(&work->obj_694);
    Gp_UnlinkObj(&work->capsuleBody);
    work2           = (Actor405800Work*)task->work;
    task->state     = 3;
    work2->state    = 0;
    work2->subState = 0;
}

static void func_actor_405800_80138CF0(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    func_actor_405800_8013A1F8(task, 9, 0x10, 2);
    work->field_84C = 0;
    work->field_84E = 0;
    work->field_86A = work->field_92;
    stalkerZebraIvoryTickAnim(task);
    work->state = work->state + 1;
}

static void func_actor_405800_80138D54(Task* task)
{
    Actor405800Work* work;
    GfxCoord*        coord;

    work               = (Actor405800Work*)task->work;
    coord              = task->extra.tmd->coords;
    work->field_84C   += 2;
    work->field_84E   += work->field_84C;
    coord->coord.t[1] += work->field_84E;
    if ((s16)work->field_92 < coord->coord.t[1]) {
        coord->coord.t[1] = (s16)work->field_92;
        stalkerZebraIvoryPlayClip(task, 0x13, 0x10);
        work->roll += 0x800;
        stalkerZebraIvoryApplyRotation(task);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        work->field_842 = 0;
        work->onCeiling = 0;
        work->onBack    = 1;
        work->state++;
    }
    stalkerZebraIvoryTickAnim(task);
}

static void func_actor_405800_80138E20(Task* task)
{
    Actor405800Work* work;
    u32              sound;
    s32              pan;

    work = (Actor405800Work*)task->work;
    if ((s16)work->field_842 == 0) {
        sound   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40050006;
        pan     = worldCoordGetOriginAudioPan(task->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->field_842++;
    }
    if ((stalkerZebraIvoryClipDone(task) << 0x10) != 0) {
        func_actor_405800_80139FB0(task, 1);
    }
    stalkerZebraIvoryTickAnim(task);
}

static void func_actor_405800_80138EF0(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    if (work->field_824 != NULL) {
        taskKill(work->field_824);
    }
    if (work->field_828 != NULL) {
        taskKill(work->field_828);
    }
    work->field_842 = 0;
    work->state     = work->state + 1;
}

static void func_actor_405800_80138F54(Task* task)
{
    Actor405800Work* work;
    u16              count;

    work            = (Actor405800Work*)task->work;
    count           = work->field_842 + 1;
    work->field_842 = count;
    if ((s16)count >= 0x12D) {
        enemyDestroy(task->spawnArg2.pointer, task);
    }
}

static void func_actor_405800_80138FA8(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    if (((s8)work->field_895 >= 0 || (work->field_895 & 0x7F)) && work->field_83A == 0) {
        work->field_895 = 0x80;
        work->field_896 = 0;
    }
    work->field_880 = 0x18;
    work->field_893 = 0;
    work->field_87E = 0x10;
    func_actor_405800_80135A3C(task, 0x10);
    work->subState = work->subState + 1;
}

static void func_actor_405800_8013902C(Task* task)
{
    Actor405800Work* work;
    s16              min;
    s16              step;
    u32              rnd;

    work = (Actor405800Work*)task->work;
    min  = 0x10;
    if (work->playerDistance > 0xBB8 && work->field_893 == 0) {
        rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        if ((rnd >> 0x10) & 1) {
            min  = 0x18;
            step = 0x24;
        } else {
            min  = 0x14;
            step = 0x1E;
        }
        work->field_880 = step;
        work->field_893 = 1;
    }
    if (work->field_87E < min) {
        work->field_87E = min;
    }
    stalkerZebraIvoryTurnToward(task, &work->field_A8, work->field_880);
    func_actor_405800_80135A3C(task, work->field_87E);
}

static void func_actor_405800_801390FC(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
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
    work2 = (Actor405800Work*)arg0->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->subState = work->subState + 1;
}

static void func_actor_405800_80139188(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->work;
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
    if ((func_actor_405800_80136A1C(arg0) << 0x10) == 0 && (stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (Actor405800Work*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (Actor405800Work*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_80139260(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
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
    work2 = (Actor405800Work*)arg0->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->subState = work->subState + 1;
}

static void func_actor_405800_801392EC(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (Actor405800Work*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (Actor405800Work*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_80139358(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
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
    work2 = (Actor405800Work*)arg0->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->field_842 = 0;
    work->subState  = work->subState + 1;
}

static void func_actor_405800_801393E8(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    Actor405800Work* work4;
    Actor405800Work* work5;
    u16              count;

    work = (Actor405800Work*)arg0->work;
    if (work->field_898 == 2) {
        if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
            if (work->onBack == 0) {
                work2              = (Actor405800Work*)arg0->work;
                work2->animBlend   = 8;
                work2->animStep    = 0x10;
                work2->animClip    = 0x10;
                work2->animRequest = 1;
            } else {
                work3              = (Actor405800Work*)arg0->work;
                work3->animBlend   = 8;
                work3->animStep    = 0x10;
                work3->animClip    = 0x12;
                work3->animRequest = 1;
            }
            work->subState = work->subState + 1;
        }
    } else {
        count           = work->field_842 + 1;
        work->field_842 = count;
        if ((s16)count >= 0x15) {
            if (work->onBack == 0) {
                work4              = (Actor405800Work*)arg0->work;
                work4->animBlend   = 8;
                work4->animStep    = 0x10;
                work4->animClip    = 0x10;
                work4->animRequest = 1;
            } else {
                work5              = (Actor405800Work*)arg0->work;
                work5->animBlend   = 8;
                work5->animStep    = 0x10;
                work5->animClip    = 0x12;
                work5->animRequest = 1;
            }
            work->subState = work->subState + 1;
        }
    }
}

static void func_actor_405800_801394E4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->field_898 = 0;
        if (work->onBack == 0) {
            work2           = (Actor405800Work*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (Actor405800Work*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_80139550(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work              = (Actor405800Work*)task->work;
    work->animBlend   = 8;
    work->animStep    = 0x10;
    work->animClip    = 7;
    work->animRequest = 1;
    work->field_842   = 0;
    func_actor_405800_80139F0C(task, 0);
    work2 = (Actor405800Work*)task->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->subState = work->subState + 1;
}

static void func_actor_405800_801395E8(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work              = (Actor405800Work*)task->work;
    work->animBlend   = 8;
    work->animStep    = 0x10;
    work->animClip    = 8;
    work->animRequest = 1;
    work->field_842   = 0;
    func_actor_405800_80139F0C(task, 1);
    work2 = (Actor405800Work*)task->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->subState = work->subState + 1;
}

static void func_actor_405800_8013967C(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)task->work;
    func_actor_405800_8013706C(task, 0);
    work->field_88B = 0;
    work2           = (Actor405800Work*)task->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    func_actor_405800_80135A3C(task, work->field_87E);
    work->subState = work->subState + 1;
}

#include "../../shared/stalker_zebra_ivory_release_hold.inc.c"

static void func_actor_405800_801397B8(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    func_actor_405800_8013706C(task, 1);
    work->subState = work->subState + 1;
}

#include "../../shared/stalker_zebra_ivory_wait_clip.inc.c"

static void func_actor_405800_80139844(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work               = (Actor405800Work*)task->work;
    work->field_842    = 0;
    work2              = (Actor405800Work*)task->work;
    work2->animStep    = 0x10;
    work2->animClip    = 0x16;
    work2->animRequest = 2;
    work->subState     = work->subState + 1;
}

static void func_actor_405800_80139880(Task* task)
{
    Actor405800Work* work;

    work              = (Actor405800Work*)task->work;
    work->animBlend   = 4;
    work->animStep    = 0x10;
    work->animClip    = 0x15;
    work->animRequest = 1;
    work->field_842   = 0;
    work->field_9A    = -0x9C4;
    work->subState    = work->subState + 1;
}

static void func_actor_405800_801398C0(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)task->work;
    if (((s16)stalkerZebraIvoryTakePending(task) == 0) && ((s16)stalkerZebraIvoryClipDone(task) != 0)) {
        work->field_85E = 0x12C;
        work2           = (Actor405800Work*)task->work;
        work2->state    = 2;
        work2->subState = 0;
    }
}

static void func_actor_405800_80139928(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GfxCoord*        coord;
    u16              next;

    work  = (Actor405800Work*)task->work;
    coord = task->extra.tmd->coords;
    stalkerZebraIvoryDropCapsuleGrid(task);
    work2              = (Actor405800Work*)task->work;
    work2->animBlend   = 4;
    work2->animStep    = 0x10;
    work2->animClip    = 0x20;
    work2->animRequest = 1;
    work->field_84C    = 0x40;
    work->field_84E    = 0;
    work->field_842    = 0;
    work->field_98     = coord->coord.t[0];
    next               = work->subState;
    work->field_9C     = coord->coord.t[2];
    work->holding      = 1;
    work->subState     = next + 1;
}

static void func_actor_405800_801399C4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    u32              sound;
    s32              id;
    s32              pan;

    work = (Actor405800Work*)arg0->work;
    if ((s16)work->field_842 == 0) {
        id = 0x40050003;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan   = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->holding = 0;
        work->field_842++;
    }
    if ((stalkerZebraIvoryTakePending(arg0) << 0x10) == 0 && (stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->field_85E = 0x12C;
        work2           = (Actor405800Work*)arg0->work;
        work2->state    = 2;
        work2->subState = 0;
    }
}

static void func_actor_405800_80139AC4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (((s8)work->field_895 >= 0) || ((work->field_895 & 0x7F) != 1)) {
        work->field_895 = 0x81;
        work->field_896 = 0;
    }
    work2              = (Actor405800Work*)arg0->work;
    work2->animBlend   = 2;
    work2->animStep    = 0x10;
    work2->animClip    = 9;
    work2->animRequest = 1;
    work->holding      = 1;
    work->field_84C    = 0;
    work->field_84E    = 0;
    work->subState     = work->subState + 1;
    work->field_86A    = work->field_92;
}

static void func_actor_405800_80139B3C(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GfxCoord*        coord;

    work               = (Actor405800Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_84C    = work->field_84C + 2;
    work->field_84E    = work->field_84E + work->field_84C;
    coord->coord.t[1] += work->field_84E;
    if ((s16)work->field_92 < coord->coord.t[1]) {
        coord->coord.t[1]  = (s16)work->field_92;
        work2              = (Actor405800Work*)arg0->work;
        work2->animStep    = 0x10;
        work2->animClip    = 0x13;
        work2->animRequest = 2;
        work->onBack       = 1;
        work->field_842    = 0;
        work->onCeiling    = 0;
        work->roll         = work->roll + 0x800;
        work->subState     = work->subState + 1;
    }
}

#include "../../shared/stalker_zebra_ivory_wait_clip_then_rest.inc.c"

static void func_actor_405800_80139C98(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
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
    work2 = (Actor405800Work*)arg0->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->subState = work->subState + 1;
}

static void func_actor_405800_80139D24(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2              = (Actor405800Work*)arg0->work;
            work2->animBlend   = 0x1E;
            work2->animStep    = 0x10;
            work2->animClip    = 0x10;
            work2->animRequest = 1;
        } else {
            work3              = (Actor405800Work*)arg0->work;
            work3->animBlend   = 0x1E;
            work3->animStep    = 8;
            work3->animClip    = 0x14;
            work3->animRequest = 1;
        }
        work->subState = work->subState + 1;
    }
}

static void func_actor_405800_80139DC0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->work;
    if ((stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        if (work->onBack == 0) {
            work2           = (Actor405800Work*)arg0->work;
            work2->state    = 2;
            work2->subState = 0;
        } else {
            work3           = (Actor405800Work*)arg0->work;
            work3->state    = 0xA;
            work3->subState = 0;
        }
    }
}

static void func_actor_405800_80139E2C(Task* task)
{
    Actor405800Work* work;

    work           = (Actor405800Work*)task->work;
    work->subState = work->subState + 1;
}

#include "../../shared/stalker_zebra_ivory_pick_range.inc.c"

static void func_actor_405800_80139EAC(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;
    u32              rnd;

    work->animBlend   = 4;
    work->animStep    = 0x10;
    work->animClip    = 1;
    work->animRequest = 1;
    rnd               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState   = rnd;
    work->field_882   = ((rnd >> 0x10) & 0x3F) + 0x5A;
    work->subState    = work->subState + 1;
}

static void func_actor_405800_80139F0C(Task* task, u8 arg1)
{
    Actor405800Work* work = (Actor405800Work*)task->work;
    s32              mode = arg1;

    if (mode == 0) {
        work->queuedFlag = 1;
    } else if (mode == 1) {
        work->queuedMode = mode;
    }
}

#include "../../shared/stalker_zebra_ivory_take_armed_pending.inc.c"

static void func_actor_405800_80139FB0(Task* task, s16 arg1)
{
    Actor405800Work* work;

    work           = (Actor405800Work*)task->work;
    work->state    = arg1;
    work->subState = 0;
}

#include "../../shared/stalker_zebra_ivory_apply_rotation.inc.c"

#include "../../shared/stalker_zebra_ivory_tick_anim.inc.c"

#include "../../shared/stalker_zebra_ivory_play_clip.inc.c"

static void func_actor_405800_8013A1F8(Task* task, s16 arg1, s16 arg2, s16 arg3)
{
    Actor405800Work* work;

    work              = (Actor405800Work*)task->work;
    work->animBlend   = arg3;
    work->animStep    = arg2;
    work->animClip    = arg1;
    work->animRequest = 1;
}
