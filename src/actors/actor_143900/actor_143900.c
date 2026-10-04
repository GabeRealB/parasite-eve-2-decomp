#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_r49.h"
#define SCRIPTED_WALK_MODE gScriptedWalkModeValue
#include "../../shared/scripted_walk.h"
#include "../../shared/walker.h"

/// Bytes at the scripted-walk mode's address. The first halfword is
/// `gScriptedWalkModeValue`. Nothing reads or writes the second halfword;
/// its role, including padding, is unproven.
extern s16 gScriptedWalkMode[2];

/// Approach mode the last `scriptedWalkTo` selected for the first variant
/// (0 faces the target, step 60; 1 faces away, step 15 backward; 2 faces the
/// target, step 25). Halfword view of `gScriptedWalkMode`, which is how the
/// walk code addresses the mode.
extern s16 gScriptedWalkModeValue __asm__("gScriptedWalkMode");

/// Work block of the overlay's first actor variant, allocated zeroed by its
/// spawn routine and kept both in `gScriptedWalkWork` and at
/// `Task::work`; every other function of the variant reaches it through the
/// global.
///
/// `light` and `color` are the two matrices the block supplies to the model:
/// the spawn routine points the object's `lightMtx` / `colorMtx` at them.
/// `rig` and `st` are the model's animation rig and state, and `turnFrames`
/// the frames of turning left while animation 3 plays, which the 0x7DB
/// handler latches.
typedef struct Actor143900Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
    s16             turnFrames;
    byte            pad_4EE[0x2];
} Actor143900Work;
STATIC_ASSERT_SIZEOF(Actor143900Work, 0x4F0);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Spawn table this overlay hands to `Task_SpawnFromTable`. It sits at an
/// absolute address outside the actor slot - offset 0x440 into the loaded room
/// overlay, whose base is 0x8017D5C0 - so splat cannot name it and it keeps its
/// raw `D_` form.

/// The first variant's work block, published by its dispatcher
/// `func_actor_143900_80132324` and its spawn routine.
extern Actor143900Work* gScriptedWalkWork;

/// The first variant's task, published by its spawn routine so the
/// visibility and play-animation handlers can reach it.
extern Task* D_actor_143900_801496BC;

/// Reset argument the first variant forwards to the reseed: its play-animation
/// handler latches the preset's `field_C` here, and the update sets it to 10
/// when a walk ends.
extern s16 gScriptedWalkBlendFrames;

/// The first variant's message table; its spawn routine publishes it as
/// `Task::msgTable`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_143900_801413BC[];

/// Animation stream the first variant's spawn routine binds into its work
/// block's animation context with `animationInitContext`.
extern u8 D_actor_143900_801413F8[];

/// The second variant's work block, published by its dispatcher
/// `func_actor_143900_80132DEC` and its spawn routine.
extern ScriptedWalkAttachmentsWork* D_actor_143900_801496C4;

/// The second variant's task, published by its spawn routine so the placement,
/// visibility and play-animation handlers can reach it.
extern Task* D_actor_143900_801496C8;

/// Approach mode the last `func_actor_143900_801333C4` call selected; the
/// second variant's update picks its walk distance from it.
extern s16 D_actor_143900_801496CC;

/// Reset argument the second variant forwards to the reseed: its
/// play-animation handler latches the preset's `field_C` here, and the update
/// sets it to 10 when a walk ends.
extern s16 D_actor_143900_80149630;

/// The second variant's message table; its spawn routine publishes it as
/// `Task::msgTable`.
extern TaskMessageEntry D_actor_143900_80149634[];

/// Spawn table the second variant's spawn routine starts its two attachment tasks
/// from, indices 1 and 2; the tasks are parked in `attachment1` / `attachment2`.
extern TaskDesc D_actor_143900_80149664[];

/// Animation stream the second variant's spawn routine binds into its work
/// block's animation context with `animationInitContext`.
extern u8 D_actor_143900_80149688[];

static void func_actor_143900_80132380(Enemy* enemy, Task* task);
static void func_actor_143900_80132404(Task* task);
static void func_actor_143900_80132A9C(Task* task);
static void func_actor_143900_80132E48(Enemy* enemy, Task* task);
static void func_actor_143900_80132ECC(Task* task);
static void func_actor_143900_80132F14(Task* task);
static void func_actor_143900_80133068(void);
static void func_actor_143900_801330B4(void);
static void func_actor_143900_80133144(void);

static TmdSource _gActor143900Body2;
static TmdSource _gActor143900Model173A8;
static TmdSource _gActor143900Model17688;
s32              func_actor_143900_801331C4(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_143900_80133254(Task*, s32, s32, s32);
s32              func_actor_143900_801332E4(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);
s32              func_actor_143900_80133360(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
s32              func_actor_143900_801333C4(Task*, s32, VECTOR*, s32);
void             func_actor_143900_80132DEC(Task*);
void             func_actor_143900_80132FB0(Task*);

s32  func_actor_143900_80132624(Task*, s32, AnimationPlayRequest*, s32);
s32  func_actor_143900_801326B4(Task*, s32, s32, s32);
s32  func_actor_143900_80132778(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void func_actor_143900_80132324(Task*);

void func_actor_143900_80131E24(void);

void func_actor_143900_80131E24(void);

AnimationPlayRequest D_actor_143900_801334FC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_80133510 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_80133524 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_80133538 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_8013354C = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_actor_143900_80133560[32] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54310002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143900_801334FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_143900_80133510 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54310001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54310003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_143900_80133524 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_143900_80133538 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_143900_8013354C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143900_80131E24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_143900_80133860[12] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143900_80131E24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor143900Body1Skeleton[20] = {
#include "assets/actor_143900_body_1_skeleton.inc"
};

static u32 _gActor143900Body1PartVerts[20] = {
#include "assets/actor_143900_body_1_partVerts.inc"
};

static SVECTOR _gActor143900Body1Verts[342] = {
#include "assets/actor_143900_body_1_verts.inc"
};

static SVECTOR _gActor143900Body1Normals[442] = {
#include "assets/actor_143900_body_1_normals.inc"
};

static u32 _gActor143900Body1Stream[4280] = {
#include "assets/actor_143900_body_1_stream.inc"
};

static TmdSource _gActor143900Body1 = {
    0,
    21544,
    8516,
    20,
    _gActor143900Body1PartVerts,
    _gActor143900Body1Verts,
    _gActor143900Body1Normals,
    _gActor143900Body1Skeleton,
    _gActor143900Body1Stream,
};

static AnimationPackedPose _gActor143900Animation07C70Bank1[2] = {
#include "assets/actor_143900_animation_07C70_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation07C70Bank4[31] = {
#include "assets/actor_143900_animation_07C70_bank4.inc"
};

static AnimationRecord _gActor143900Animation07C70Records[108] = {
#include "assets/actor_143900_animation_07C70_records.inc"
};

static u16 _gActor143900Animation07C70Indices[20] = {
#include "assets/actor_143900_animation_07C70_indices.inc"
};

static AnimationSet _gActor143900Animation07C70 = {
    _gActor143900Animation07C70Records,
    _gActor143900Animation07C70Indices,
    { NULL, _gActor143900Animation07C70Bank1, NULL, NULL, _gActor143900Animation07C70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0800CBank1[2] = {
#include "assets/actor_143900_animation_0800C_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0800CBank4[56] = {
#include "assets/actor_143900_animation_0800C_bank4.inc"
};

static AnimationRecord _gActor143900Animation0800CRecords[149] = {
#include "assets/actor_143900_animation_0800C_records.inc"
};

static u16 _gActor143900Animation0800CIndices[20] = {
#include "assets/actor_143900_animation_0800C_indices.inc"
};

static AnimationSet _gActor143900Animation0800C = {
    _gActor143900Animation0800CRecords,
    _gActor143900Animation0800CIndices,
    { NULL, _gActor143900Animation0800CBank1, NULL, NULL, _gActor143900Animation0800CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation08398Bank1[2] = {
#include "assets/actor_143900_animation_08398_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation08398Bank4[58] = {
#include "assets/actor_143900_animation_08398_bank4.inc"
};

static AnimationRecord _gActor143900Animation08398Records[143] = {
#include "assets/actor_143900_animation_08398_records.inc"
};

static u16 _gActor143900Animation08398Indices[20] = {
#include "assets/actor_143900_animation_08398_indices.inc"
};

static AnimationSet _gActor143900Animation08398 = {
    _gActor143900Animation08398Records,
    _gActor143900Animation08398Indices,
    { NULL, _gActor143900Animation08398Bank1, NULL, NULL, _gActor143900Animation08398Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation087A4Bank1[2] = {
#include "assets/actor_143900_animation_087A4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation087A4Bank4[59] = {
#include "assets/actor_143900_animation_087A4_bank4.inc"
};

static AnimationRecord _gActor143900Animation087A4Records[174] = {
#include "assets/actor_143900_animation_087A4_records.inc"
};

static u16 _gActor143900Animation087A4Indices[20] = {
#include "assets/actor_143900_animation_087A4_indices.inc"
};

static AnimationSet _gActor143900Animation087A4 = {
    _gActor143900Animation087A4Records,
    _gActor143900Animation087A4Indices,
    { NULL, _gActor143900Animation087A4Bank1, NULL, NULL, _gActor143900Animation087A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation09138Bank1[11] = {
#include "assets/actor_143900_animation_09138_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation09138Bank4[245] = {
#include "assets/actor_143900_animation_09138_bank4.inc"
};

static AnimationRecord _gActor143900Animation09138Records[315] = {
#include "assets/actor_143900_animation_09138_records.inc"
};

static u16 _gActor143900Animation09138Indices[20] = {
#include "assets/actor_143900_animation_09138_indices.inc"
};

static AnimationSet _gActor143900Animation09138 = {
    _gActor143900Animation09138Records,
    _gActor143900Animation09138Indices,
    { NULL, _gActor143900Animation09138Bank1, NULL, NULL, _gActor143900Animation09138Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation09364Bank1[2] = {
#include "assets/actor_143900_animation_09364_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation09364Bank4[33] = {
#include "assets/actor_143900_animation_09364_bank4.inc"
};

static AnimationRecord _gActor143900Animation09364Records[80] = {
#include "assets/actor_143900_animation_09364_records.inc"
};

static u16 _gActor143900Animation09364Indices[20] = {
#include "assets/actor_143900_animation_09364_indices.inc"
};

static AnimationSet _gActor143900Animation09364 = {
    _gActor143900Animation09364Records,
    _gActor143900Animation09364Indices,
    { NULL, _gActor143900Animation09364Bank1, NULL, NULL, _gActor143900Animation09364Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation099B4Bank1[5] = {
#include "assets/actor_143900_animation_099B4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation099B4Bank4[147] = {
#include "assets/actor_143900_animation_099B4_bank4.inc"
};

static AnimationRecord _gActor143900Animation099B4Records[222] = {
#include "assets/actor_143900_animation_099B4_records.inc"
};

static u16 _gActor143900Animation099B4Indices[20] = {
#include "assets/actor_143900_animation_099B4_indices.inc"
};

static AnimationSet _gActor143900Animation099B4 = {
    _gActor143900Animation099B4Records,
    _gActor143900Animation099B4Indices,
    { NULL, _gActor143900Animation099B4Bank1, NULL, NULL, _gActor143900Animation099B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0A040Bank1[12] = {
#include "assets/actor_143900_animation_0A040_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0A040Bank4[152] = {
#include "assets/actor_143900_animation_0A040_bank4.inc"
};

static AnimationRecord _gActor143900Animation0A040Records[211] = {
#include "assets/actor_143900_animation_0A040_records.inc"
};

static u16 _gActor143900Animation0A040Indices[20] = {
#include "assets/actor_143900_animation_0A040_indices.inc"
};

static AnimationSet _gActor143900Animation0A040 = {
    _gActor143900Animation0A040Records,
    _gActor143900Animation0A040Indices,
    { NULL, _gActor143900Animation0A040Bank1, NULL, NULL, _gActor143900Animation0A040Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0A21CBank1[2] = {
#include "assets/actor_143900_animation_0A21C_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0A21CBank4[25] = {
#include "assets/actor_143900_animation_0A21C_bank4.inc"
};

static AnimationRecord _gActor143900Animation0A21CRecords[68] = {
#include "assets/actor_143900_animation_0A21C_records.inc"
};

static u16 _gActor143900Animation0A21CIndices[20] = {
#include "assets/actor_143900_animation_0A21C_indices.inc"
};

static AnimationSet _gActor143900Animation0A21C = {
    _gActor143900Animation0A21CRecords,
    _gActor143900Animation0A21CIndices,
    { NULL, _gActor143900Animation0A21CBank1, NULL, NULL, _gActor143900Animation0A21CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0A738Bank1[2] = {
#include "assets/actor_143900_animation_0A738_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0A738Bank4[123] = {
#include "assets/actor_143900_animation_0A738_bank4.inc"
};

static AnimationRecord _gActor143900Animation0A738Records[178] = {
#include "assets/actor_143900_animation_0A738_records.inc"
};

static u16 _gActor143900Animation0A738Indices[20] = {
#include "assets/actor_143900_animation_0A738_indices.inc"
};

static AnimationSet _gActor143900Animation0A738 = {
    _gActor143900Animation0A738Records,
    _gActor143900Animation0A738Indices,
    { NULL, _gActor143900Animation0A738Bank1, NULL, NULL, _gActor143900Animation0A738Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0A8D0Bank1[2] = {
#include "assets/actor_143900_animation_0A8D0_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0A8D0Bank4[16] = {
#include "assets/actor_143900_animation_0A8D0_bank4.inc"
};

static AnimationRecord _gActor143900Animation0A8D0Records[60] = {
#include "assets/actor_143900_animation_0A8D0_records.inc"
};

static u16 _gActor143900Animation0A8D0Indices[20] = {
#include "assets/actor_143900_animation_0A8D0_indices.inc"
};

static AnimationSet _gActor143900Animation0A8D0 = {
    _gActor143900Animation0A8D0Records,
    _gActor143900Animation0A8D0Indices,
    { NULL, _gActor143900Animation0A8D0Bank1, NULL, NULL, _gActor143900Animation0A8D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0AB6CBank1[2] = {
#include "assets/actor_143900_animation_0AB6C_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0AB6CBank4[40] = {
#include "assets/actor_143900_animation_0AB6C_bank4.inc"
};

static AnimationRecord _gActor143900Animation0AB6CRecords[101] = {
#include "assets/actor_143900_animation_0AB6C_records.inc"
};

static u16 _gActor143900Animation0AB6CIndices[20] = {
#include "assets/actor_143900_animation_0AB6C_indices.inc"
};

static AnimationSet _gActor143900Animation0AB6C = {
    _gActor143900Animation0AB6CRecords,
    _gActor143900Animation0AB6CIndices,
    { NULL, _gActor143900Animation0AB6CBank1, NULL, NULL, _gActor143900Animation0AB6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0B2B4Bank1[11] = {
#include "assets/actor_143900_animation_0B2B4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0B2B4Bank4[175] = {
#include "assets/actor_143900_animation_0B2B4_bank4.inc"
};

static AnimationRecord _gActor143900Animation0B2B4Records[238] = {
#include "assets/actor_143900_animation_0B2B4_records.inc"
};

static u16 _gActor143900Animation0B2B4Indices[20] = {
#include "assets/actor_143900_animation_0B2B4_indices.inc"
};

static AnimationSet _gActor143900Animation0B2B4 = {
    _gActor143900Animation0B2B4Records,
    _gActor143900Animation0B2B4Indices,
    { NULL, _gActor143900Animation0B2B4Bank1, NULL, NULL, _gActor143900Animation0B2B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0B4A0Bank1[2] = {
#include "assets/actor_143900_animation_0B4A0_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0B4A0Bank4[24] = {
#include "assets/actor_143900_animation_0B4A0_bank4.inc"
};

static AnimationRecord _gActor143900Animation0B4A0Records[73] = {
#include "assets/actor_143900_animation_0B4A0_records.inc"
};

static u16 _gActor143900Animation0B4A0Indices[20] = {
#include "assets/actor_143900_animation_0B4A0_indices.inc"
};

static AnimationSet _gActor143900Animation0B4A0 = {
    _gActor143900Animation0B4A0Records,
    _gActor143900Animation0B4A0Indices,
    { NULL, _gActor143900Animation0B4A0Bank1, NULL, NULL, _gActor143900Animation0B4A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0BC24Bank1[15] = {
#include "assets/actor_143900_animation_0BC24_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0BC24Bank4[177] = {
#include "assets/actor_143900_animation_0BC24_bank4.inc"
};

static AnimationRecord _gActor143900Animation0BC24Records[239] = {
#include "assets/actor_143900_animation_0BC24_records.inc"
};

static u16 _gActor143900Animation0BC24Indices[20] = {
#include "assets/actor_143900_animation_0BC24_indices.inc"
};

static AnimationSet _gActor143900Animation0BC24 = {
    _gActor143900Animation0BC24Records,
    _gActor143900Animation0BC24Indices,
    { NULL, _gActor143900Animation0BC24Bank1, NULL, NULL, _gActor143900Animation0BC24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0C718Bank1[12] = {
#include "assets/actor_143900_animation_0C718_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0C718Bank4[255] = {
#include "assets/actor_143900_animation_0C718_bank4.inc"
};

static AnimationRecord _gActor143900Animation0C718Records[390] = {
#include "assets/actor_143900_animation_0C718_records.inc"
};

static u16 _gActor143900Animation0C718Indices[20] = {
#include "assets/actor_143900_animation_0C718_indices.inc"
};

static AnimationSet _gActor143900Animation0C718 = {
    _gActor143900Animation0C718Records,
    _gActor143900Animation0C718Indices,
    { NULL, _gActor143900Animation0C718Bank1, NULL, NULL, _gActor143900Animation0C718Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0D1C4Bank1[14] = {
#include "assets/actor_143900_animation_0D1C4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0D1C4Bank4[263] = {
#include "assets/actor_143900_animation_0D1C4_bank4.inc"
};

static AnimationRecord _gActor143900Animation0D1C4Records[358] = {
#include "assets/actor_143900_animation_0D1C4_records.inc"
};

static u16 _gActor143900Animation0D1C4Indices[20] = {
#include "assets/actor_143900_animation_0D1C4_indices.inc"
};

static AnimationSet _gActor143900Animation0D1C4 = {
    _gActor143900Animation0D1C4Records,
    _gActor143900Animation0D1C4Indices,
    { NULL, _gActor143900Animation0D1C4Bank1, NULL, NULL, _gActor143900Animation0D1C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0ED80Bank1[103] = {
#include "assets/actor_143900_animation_0ED80_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0ED80Bank4[620] = {
#include "assets/actor_143900_animation_0ED80_bank4.inc"
};

static AnimationRecord _gActor143900Animation0ED80Records[826] = {
#include "assets/actor_143900_animation_0ED80_records.inc"
};

static u16 _gActor143900Animation0ED80Indices[20] = {
#include "assets/actor_143900_animation_0ED80_indices.inc"
};

static AnimationSet _gActor143900Animation0ED80 = {
    _gActor143900Animation0ED80Records,
    _gActor143900Animation0ED80Indices,
    { NULL, _gActor143900Animation0ED80Bank1, NULL, NULL, _gActor143900Animation0ED80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0F188Bank1[2] = {
#include "assets/actor_143900_animation_0F188_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0F188Bank4[93] = {
#include "assets/actor_143900_animation_0F188_bank4.inc"
};

static AnimationRecord _gActor143900Animation0F188Records[139] = {
#include "assets/actor_143900_animation_0F188_records.inc"
};

static u16 _gActor143900Animation0F188Indices[20] = {
#include "assets/actor_143900_animation_0F188_indices.inc"
};

static AnimationSet _gActor143900Animation0F188 = {
    _gActor143900Animation0F188Records,
    _gActor143900Animation0F188Indices,
    { NULL, _gActor143900Animation0F188Bank1, NULL, NULL, _gActor143900Animation0F188Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0F570Bank1[2] = {
#include "assets/actor_143900_animation_0F570_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0F570Bank4[89] = {
#include "assets/actor_143900_animation_0F570_bank4.inc"
};

static AnimationRecord _gActor143900Animation0F570Records[135] = {
#include "assets/actor_143900_animation_0F570_records.inc"
};

static u16 _gActor143900Animation0F570Indices[20] = {
#include "assets/actor_143900_animation_0F570_indices.inc"
};

static AnimationSet _gActor143900Animation0F570 = {
    _gActor143900Animation0F570Records,
    _gActor143900Animation0F570Indices,
    { NULL, _gActor143900Animation0F570Bank1, NULL, NULL, _gActor143900Animation0F570Bank4, NULL, NULL, NULL },
};

s16 gScriptedWalkBlendFrames = 8;

TaskMessageEntry D_actor_143900_801413BC[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_143900_80132624 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_143900_801326B4 },
    { ACTOR_MESSAGE_PLACE, scriptedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_143900_80132778 },
    { ACTOR_MESSAGE_WALK_TO, scriptedWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_143900_801413EC = { { { TASK_BODY_TMD, 192 } }, func_actor_143900_80132324, { .model = &_gActor143900Body1 } };

u8 D_actor_143900_801413F8[80] = {
    0,
    0,
    0,
    0,
    144,
    154,
    19,
    128,
    44,
    158,
    19,
    128,
    184,
    161,
    19,
    128,
    196,
    165,
    19,
    128,
    88,
    175,
    19,
    128,
    132,
    177,
    19,
    128,
    212,
    183,
    19,
    128,
    96,
    190,
    19,
    128,
    240,
    198,
    19,
    128,
    140,
    201,
    19,
    128,
    212,
    208,
    19,
    128,
    192,
    210,
    19,
    128,
    68,
    218,
    19,
    128,
    56,
    229,
    19,
    128,
    228,
    239,
    19,
    128,
    160,
    11,
    20,
    128,
    60,
    192,
    19,
    128,
    88,
    197,
    19,
    128,
    168,
    15,
    20,
    128,
};

static TmdBone _gActor143900Body2Skeleton[20] = {
#include "assets/actor_143900_body_2_skeleton.inc"
};

static u32 _gActor143900Body2PartVerts[20] = {
#include "assets/actor_143900_body_2_partVerts.inc"
};

static SVECTOR _gActor143900Body2Verts[339] = {
#include "assets/actor_143900_body_2_verts.inc"
};

static SVECTOR _gActor143900Body2Normals[389] = {
#include "assets/actor_143900_body_2_normals.inc"
};

static u32 _gActor143900Body2Stream[4166] = {
#include "assets/actor_143900_body_2_stream.inc"
};

static TmdSource _gActor143900Body2 = {
    0,
    21036,
    8232,
    20,
    _gActor143900Body2PartVerts,
    _gActor143900Body2Verts,
    _gActor143900Body2Normals,
    _gActor143900Body2Skeleton,
    _gActor143900Body2Stream,
};

static AnimationPackedPose _gActor143900Animation15330Bank1[2] = {
#include "assets/actor_143900_animation_15330_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation15330Bank4[30] = {
#include "assets/actor_143900_animation_15330_bank4.inc"
};

static AnimationRecord _gActor143900Animation15330Records[77] = {
#include "assets/actor_143900_animation_15330_records.inc"
};

static u16 _gActor143900Animation15330Indices[20] = {
#include "assets/actor_143900_animation_15330_indices.inc"
};

static AnimationSet _gActor143900Animation15330 = {
    _gActor143900Animation15330Records,
    _gActor143900Animation15330Indices,
    { NULL, _gActor143900Animation15330Bank1, NULL, NULL, _gActor143900Animation15330Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation157D4Bank1[2] = {
#include "assets/actor_143900_animation_157D4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation157D4Bank4[107] = {
#include "assets/actor_143900_animation_157D4_bank4.inc"
};

static AnimationRecord _gActor143900Animation157D4Records[164] = {
#include "assets/actor_143900_animation_157D4_records.inc"
};

static u16 _gActor143900Animation157D4Indices[20] = {
#include "assets/actor_143900_animation_157D4_indices.inc"
};

static AnimationSet _gActor143900Animation157D4 = {
    _gActor143900Animation157D4Records,
    _gActor143900Animation157D4Indices,
    { NULL, _gActor143900Animation157D4Bank1, NULL, NULL, _gActor143900Animation157D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation15B1CBank1[2] = {
#include "assets/actor_143900_animation_15B1C_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation15B1CBank4[41] = {
#include "assets/actor_143900_animation_15B1C_bank4.inc"
};

static AnimationRecord _gActor143900Animation15B1CRecords[143] = {
#include "assets/actor_143900_animation_15B1C_records.inc"
};

static u16 _gActor143900Animation15B1CIndices[20] = {
#include "assets/actor_143900_animation_15B1C_indices.inc"
};

static AnimationSet _gActor143900Animation15B1C = {
    _gActor143900Animation15B1CRecords,
    _gActor143900Animation15B1CIndices,
    { NULL, _gActor143900Animation15B1CBank1, NULL, NULL, _gActor143900Animation15B1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation15DECBank1[7] = {
#include "assets/actor_143900_animation_15DEC_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation15DECBank4[26] = {
#include "assets/actor_143900_animation_15DEC_bank4.inc"
};

static AnimationRecord _gActor143900Animation15DECRecords[113] = {
#include "assets/actor_143900_animation_15DEC_records.inc"
};

static u16 _gActor143900Animation15DECIndices[20] = {
#include "assets/actor_143900_animation_15DEC_indices.inc"
};

static AnimationSet _gActor143900Animation15DEC = {
    _gActor143900Animation15DECRecords,
    _gActor143900Animation15DECIndices,
    { NULL, _gActor143900Animation15DECBank1, NULL, NULL, _gActor143900Animation15DECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation162D4Bank1[2] = {
#include "assets/actor_143900_animation_162D4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation162D4Bank4[120] = {
#include "assets/actor_143900_animation_162D4_bank4.inc"
};

static AnimationRecord _gActor143900Animation162D4Records[168] = {
#include "assets/actor_143900_animation_162D4_records.inc"
};

static u16 _gActor143900Animation162D4Indices[20] = {
#include "assets/actor_143900_animation_162D4_indices.inc"
};

static AnimationSet _gActor143900Animation162D4 = {
    _gActor143900Animation162D4Records,
    _gActor143900Animation162D4Indices,
    { NULL, _gActor143900Animation162D4Bank1, NULL, NULL, _gActor143900Animation162D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation16564Bank1[2] = {
#include "assets/actor_143900_animation_16564_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation16564Bank4[39] = {
#include "assets/actor_143900_animation_16564_bank4.inc"
};

static AnimationRecord _gActor143900Animation16564Records[99] = {
#include "assets/actor_143900_animation_16564_records.inc"
};

static u16 _gActor143900Animation16564Indices[20] = {
#include "assets/actor_143900_animation_16564_indices.inc"
};

static AnimationSet _gActor143900Animation16564 = {
    _gActor143900Animation16564Records,
    _gActor143900Animation16564Indices,
    { NULL, _gActor143900Animation16564Bank1, NULL, NULL, _gActor143900Animation16564Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation166ACBank1[2] = {
#include "assets/actor_143900_animation_166AC_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation166ACBank4[16] = {
#include "assets/actor_143900_animation_166AC_bank4.inc"
};

static AnimationRecord _gActor143900Animation166ACRecords[40] = {
#include "assets/actor_143900_animation_166AC_records.inc"
};

static u16 _gActor143900Animation166ACIndices[20] = {
#include "assets/actor_143900_animation_166AC_indices.inc"
};

static AnimationSet _gActor143900Animation166AC = {
    _gActor143900Animation166ACRecords,
    _gActor143900Animation166ACIndices,
    { NULL, _gActor143900Animation166ACBank1, NULL, NULL, _gActor143900Animation166ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation168ECBank1[2] = {
#include "assets/actor_143900_animation_168EC_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation168ECBank4[33] = {
#include "assets/actor_143900_animation_168EC_bank4.inc"
};

static AnimationRecord _gActor143900Animation168ECRecords[85] = {
#include "assets/actor_143900_animation_168EC_records.inc"
};

static u16 _gActor143900Animation168ECIndices[20] = {
#include "assets/actor_143900_animation_168EC_indices.inc"
};

static AnimationSet _gActor143900Animation168EC = {
    _gActor143900Animation168ECRecords,
    _gActor143900Animation168ECIndices,
    { NULL, _gActor143900Animation168ECBank1, NULL, NULL, _gActor143900Animation168ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation16E10Bank1[12] = {
#include "assets/actor_143900_animation_16E10_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation16E10Bank4[104] = {
#include "assets/actor_143900_animation_16E10_bank4.inc"
};

static AnimationRecord _gActor143900Animation16E10Records[169] = {
#include "assets/actor_143900_animation_16E10_records.inc"
};

static u16 _gActor143900Animation16E10Indices[20] = {
#include "assets/actor_143900_animation_16E10_indices.inc"
};

static AnimationSet _gActor143900Animation16E10 = {
    _gActor143900Animation16E10Records,
    _gActor143900Animation16E10Indices,
    { NULL, _gActor143900Animation16E10Bank1, NULL, NULL, _gActor143900Animation16E10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation17280Bank1[9] = {
#include "assets/actor_143900_animation_17280_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation17280Bank4[95] = {
#include "assets/actor_143900_animation_17280_bank4.inc"
};

static AnimationRecord _gActor143900Animation17280Records[142] = {
#include "assets/actor_143900_animation_17280_records.inc"
};

static u16 _gActor143900Animation17280Indices[20] = {
#include "assets/actor_143900_animation_17280_indices.inc"
};

static AnimationSet _gActor143900Animation17280 = {
    _gActor143900Animation17280Records,
    _gActor143900Animation17280Indices,
    { NULL, _gActor143900Animation17280Bank1, NULL, NULL, _gActor143900Animation17280Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor143900Model173A8Skeleton[1] = {
#include "assets/actor_143900_model_173A8_skeleton.inc"
};

static u32 _gActor143900Model173A8PartVerts[1] = {
#include "assets/actor_143900_model_173A8_partVerts.inc"
};

static SVECTOR _gActor143900Model173A8Verts[18] = {
#include "assets/actor_143900_model_173A8_verts.inc"
};

static SVECTOR _gActor143900Model173A8Normals[9] = {
#include "assets/actor_143900_model_173A8_normals.inc"
};

static u32 _gActor143900Model173A8Stream[121] = {
#include "assets/actor_143900_model_173A8_stream.inc"
};

static TmdSource _gActor143900Model173A8 = {
    0,
    944,
    0,
    1,
    _gActor143900Model173A8PartVerts,
    _gActor143900Model173A8Verts,
    _gActor143900Model173A8Normals,
    _gActor143900Model173A8Skeleton,
    _gActor143900Model173A8Stream,
};

static TmdBone _gActor143900Model17688Skeleton[1] = {
#include "assets/actor_143900_model_17688_skeleton.inc"
};

static u32 _gActor143900Model17688PartVerts[1] = {
#include "assets/actor_143900_model_17688_partVerts.inc"
};

static SVECTOR _gActor143900Model17688Verts[14] = {
#include "assets/actor_143900_model_17688_verts.inc"
};

static SVECTOR _gActor143900Model17688Normals[8] = {
#include "assets/actor_143900_model_17688_normals.inc"
};

static u32 _gActor143900Model17688Stream[89] = {
#include "assets/actor_143900_model_17688_stream.inc"
};

static TmdSource _gActor143900Model17688 = {
    0,
    680,
    0,
    1,
    _gActor143900Model17688PartVerts,
    _gActor143900Model17688Verts,
    _gActor143900Model17688Normals,
    _gActor143900Model17688Skeleton,
    _gActor143900Model17688Stream,
};

s16 D_actor_143900_80149630 = 8;

TaskMessageEntry D_actor_143900_80149634[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_143900_801331C4 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_143900_80133254 },
    { ACTOR_MESSAGE_PLACE, func_actor_143900_801332E4 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_143900_80133360 },
    { ACTOR_MESSAGE_WALK_TO, func_actor_143900_801333C4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_143900_80149664[3] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_143900_80132DEC, { .model = &_gActor143900Body2 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_143900_80132FB0, { .model = &_gActor143900Model173A8 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_143900_80132FB0, { .model = &_gActor143900Model17688 } },
};

u8 D_actor_143900_80149688[48] = {
    0,
    0,
    0,
    0,
    60,
    121,
    20,
    128,
    12,
    124,
    20,
    128,
    244,
    128,
    20,
    128,
    132,
    131,
    20,
    128,
    204,
    132,
    20,
    128,
    12,
    135,
    20,
    128,
    48,
    140,
    20,
    128,
    160,
    144,
    20,
    128,
    80,
    113,
    20,
    128,
    244,
    117,
    20,
    128,
    0,
    0,
    0,
    0,
};

Actor143900Work* gScriptedWalkWork = NULL;

Task* D_actor_143900_801496BC = NULL;

s16 gScriptedWalkMode[2] = {
    0,
    0x49E7,
};

ScriptedWalkAttachmentsWork* D_actor_143900_801496C4 = NULL;

Task* D_actor_143900_801496C8;

s16 D_actor_143900_801496CC;

static void func_actor_143900_80131E70(Enemy* enemy, Task* task);
static void func_actor_143900_801328D4(Enemy* enemy, Task* task);

/// Arms `sceneEvent` and starts the room's spawn-table task, unless
/// `demoScene` is 9, so this story trigger is skipped while the attract demo
/// plays.
void func_actor_143900_80131E24(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0x14;
        Task_SpawnFromTable(D_shelter_r49_8017DA00, 0, 0, 0);
    }
}

/// Spawn routine of the first variant (state 0 of `func_actor_143900_80132324`):
/// allocates the work block and publishes it in `gScriptedWalkWork` and
/// the task's `work` slot, binds the model's coordinate to the view and hands
/// the object its light and colour matrices out of the block, publishes the
/// task in `D_actor_143900_801496BC`, relights the model from a point 0x320
/// above its translation, binds the animation stream and runs the first update
/// with the reset mode 2 / id 1 it seeds.
///
/// Every access to the block after the allocation goes through the global
/// rather than the `memCalloc` result, which is why the pointer is reloaded at
/// each use instead of staying in a callee-saved register.
static void func_actor_143900_80131E70(Enemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor143900Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;

    obj               = task->extra.tmd;
    coord             = obj->coords;
    work              = memCalloc(0x4F0, 0);
    gScriptedWalkWork = work;
    task->work        = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_143900_80132404;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->flags                       = 0;
    obj->lightMtx                    = &gScriptedWalkWork->light;
    obj->colorMtx                    = &gScriptedWalkWork->color;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    D_actor_143900_801496BC          = task;
    vec.vz                           = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    animationInitContext(&gScriptedWalkWork->rig.anim, (AnimationSet**)D_actor_143900_801413F8, obj,
                         gScriptedWalkWork->rig.poses, gScriptedWalkWork->rig.slots);
    gScriptedWalkWork->st.animId  = 1;
    gScriptedWalkWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
    gScriptedWalkWork->st.travel  = 0;
    gScriptedWalkWork->turnFrames = 0;
    task->msgTable                = D_actor_143900_801413BC;
    scriptedWalkUpdate(task);
    task->state += 1;
}

#include "../../shared/scripted_walk_update.inc.c"

/// Two-state dispatcher of the first variant: publishes the task's work block
/// in `gScriptedWalkWork` on the way through, then calls the handler its
/// state selects from a table built on the stack.
void func_actor_143900_80132324(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_143900_80131E70,
        func_actor_143900_80132380,
    };

    gScriptedWalkWork = (Actor143900Work*)task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_143900_80132380
#define walkerUpdate     scriptedWalkUpdate
#define walkerDrawShadow walkerDrawShadowShaded
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// `Task::exitCallback` of the first variant: hands the task's `Enemy`
/// (parked in `Task::spawnArg2`) back to `enemyDestroy`.
static void func_actor_143900_80132404(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/walker_shadow_shaded.inc.c"

#include "../../shared/scripted_walk_tick_anim.inc.c"

#include "../../shared/scripted_walk_reset_anim.inc.c"

#include "../../shared/scripted_walk_blend_anim.inc.c"

/// Message 0x7D3 handler of the first variant: adopts `preset`'s animation id
/// when it is one of the first 0x14, latches the reset mode and the reset
/// argument the reseed uses, then hands the published task to the per-frame
/// update. Ids past the range are rejected with -1 and leave the work block
/// untouched.
s32 func_actor_143900_80132624(Task* task, s32 arg1, AnimationPlayRequest* preset, s32 arg3)
{
    if (preset->animationId < 0x14) {
        gScriptedWalkWork->st.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET) {
            gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            gScriptedWalkBlendFrames    = preset->blendFrames;
        } else {
            gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        gScriptedWalkWork->st.field_6 = 0;
        scriptedWalkUpdate(D_actor_143900_801496BC);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler of the first variant: applies `arg2` to the model of the
/// task published in `D_actor_143900_801496BC` - bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden), bit 1 ORs in 0x4.
s32 func_actor_143900_801326B4(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* obj;

    obj = D_actor_143900_801496BC->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/scripted_walk_place.inc.c"

/// Message 0x7DB handler of the first variant: when the payload's halfword at
/// 0x2 is zero, starts a 0x14-step turn, which the update performs while the
/// model plays animation 3.
s32 func_actor_143900_80132778(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    if (msg->command == 0) {
        gScriptedWalkWork->turnFrames = 0x14;
    }
    return 0;
}

#include "../../shared/scripted_walk_to.inc.c"

/// Spawn routine of the second variant (state 0 of `func_actor_143900_80132DEC`):
/// allocates the work block and publishes it in `D_actor_143900_801496C4`
/// and the task's `work` slot, binds the model's coordinate to the view and
/// hands the object its light and colour matrices out of the block, publishes
/// the task in `D_actor_143900_801496C8`, relights the model from a point 0x320
/// above its translation and binds the animation stream. It then starts the two
/// attachment tasks from the overlay's spawn table and runs the first update with
/// the reset mode 2 / id 1 it seeds.
static void func_actor_143900_801328D4(Enemy* enemy, Task* task)
{
    VECTOR                       vec;
    ScriptedWalkAttachmentsWork* work;
    GfxCoord*                    coord;
    TmdObject*                   obj;
    Task*                        helper;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(sizeof(ScriptedWalkAttachmentsWork), false);
    D_actor_143900_801496C4 = work;
    task->work              = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_143900_80132ECC;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    obj->otOffset                    = 0x10;
    obj->lightMtx                    = &D_actor_143900_801496C4->light;
    obj->colorMtx                    = &D_actor_143900_801496C4->color;
    obj->flags                       = 0;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    vec.vz                           = coord->workm.t[2];
    D_actor_143900_801496C8          = task;
    func_800D7A9C(obj, &vec, 0, 3);
    animationInitContext(&D_actor_143900_801496C4->rig.anim, (AnimationSet**)D_actor_143900_80149688, obj,
                         D_actor_143900_801496C4->rig.poses, D_actor_143900_801496C4->rig.slots);
    D_actor_143900_801496C4->st.animId = 1;
    D_actor_143900_801496C4->st.state  = ACTOR_ENEMY_ANIM_RESET;
    helper                             = Task_SpawnFromTable(D_actor_143900_80149664, 1, 1, 0);
    if (helper != NULL) {
        D_actor_143900_801496C4->attachment1 = helper;
    }
    helper = Task_SpawnFromTable(D_actor_143900_80149664, 2, 0xC, 0);
    if (helper != NULL) {
        D_actor_143900_801496C4->attachment2 = helper;
    }
    D_actor_143900_801496C4->st.travel  = 0;
    D_actor_143900_801496C4->turnFrames = 0;
    task->msgTable                      = D_actor_143900_80149634;
    func_actor_143900_80132A9C(task);
    task->state++;
}

/// The second walker's copy.
#define scriptedWalkUpdate       func_actor_143900_80132A9C
#define scriptedWalkTickAnim     func_actor_143900_80133068
#define scriptedWalkResetAnim    func_actor_143900_801330B4
#define scriptedWalkBlendAnim    func_actor_143900_80133144
#define gScriptedWalkWork        D_actor_143900_801496C4
#define gScriptedWalkBlendFrames D_actor_143900_80149630
#undef SCRIPTED_WALK_MODE
#define SCRIPTED_WALK_MODE D_actor_143900_801496CC
#include "../../shared/scripted_walk_update.inc.c"
#undef scriptedWalkUpdate
#undef scriptedWalkTickAnim
#undef scriptedWalkResetAnim
#undef scriptedWalkBlendAnim
#undef gScriptedWalkWork
#undef gScriptedWalkBlendFrames
#undef SCRIPTED_WALK_MODE
#define SCRIPTED_WALK_MODE gScriptedWalkModeValue

/// Two-state dispatcher of the second variant: publishes the task's work block
/// in `D_actor_143900_801496C4` on the way through, then calls the handler its
/// state selects from a table built on the stack.
void func_actor_143900_80132DEC(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_143900_801328D4,
        func_actor_143900_80132E48,
    };
    u8 scratch[0x40]; /* never referenced; only reserves the frame */

    D_actor_143900_801496C4 = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_143900_80132E48
#define walkerUpdate     func_actor_143900_80132A9C
#define walkerDrawShadow func_actor_143900_80132F14
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// `Task::exitCallback` of the second variant: hands the task's `Enemy`
/// (parked in `Task::spawnArg2`) back to `enemyDestroy`, then kills the two
/// attachment tasks the spawn routine started.
static void func_actor_143900_80132ECC(Task* task)
{
    ScriptedWalkAttachmentsWork* work = task->work;

    enemyDestroy(task->spawnArg2.pointer, task);
    taskKill(work->attachment1);
    taskKill(work->attachment2);
}

/// A further copy of the shadow, under this file's own name.
#define walkerDrawShadowShaded func_actor_143900_80132F14
#include "../../shared/walker_shadow_shaded.inc.c"
#undef walkerDrawShadowShaded

/// Attachment-task handler of the second variant: state 0 hangs the task's own
/// coordinate frame off part `spawnArg1` of the second variant's model and
/// steps to state 1; every later tick relights the attachment's model from a point
/// 0x320 above that model's root translation.
void func_actor_143900_80132FB0(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = D_actor_143900_801496C8->extra.tmd->coords;
    GfxCoord*  part  = parts + task->spawnArg1.value;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            extra->flags        = 0;
            extra->otOffset     = 0xF;
            coord->parent       = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// The second walker's copy.
#define scriptedWalkTickAnim func_actor_143900_80133068
#define gScriptedWalkWork    D_actor_143900_801496C4
#include "../../shared/scripted_walk_tick_anim.inc.c"
#undef scriptedWalkTickAnim
#undef gScriptedWalkWork

/// The second walker's copy.
#define scriptedWalkResetAnim func_actor_143900_801330B4
#define gScriptedWalkWork     D_actor_143900_801496C4
#include "../../shared/scripted_walk_reset_anim.inc.c"
#undef scriptedWalkResetAnim
#undef gScriptedWalkWork

/// The second walker's copy.
#define scriptedWalkBlendAnim    func_actor_143900_80133144
#define gScriptedWalkWork        D_actor_143900_801496C4
#define gScriptedWalkBlendFrames D_actor_143900_80149630
#include "../../shared/scripted_walk_blend_anim.inc.c"
#undef scriptedWalkBlendAnim
#undef gScriptedWalkWork
#undef gScriptedWalkBlendFrames

/// Message 0x7D3 handler of the second variant: adopts `preset`'s animation id
/// when it is one of the first 0xC, latches the reset mode and the reset
/// argument the reseed uses, then hands the published task to the per-frame
/// update. Ids past the range are rejected with -1 and leave the work block
/// untouched.
s32 func_actor_143900_801331C4(Task* task, s32 arg1, AnimationPlayRequest* preset, s32 arg3)
{
    if (preset->animationId < 0xC) {
        D_actor_143900_801496C4->st.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET) {
            D_actor_143900_801496C4->st.state = ACTOR_ENEMY_ANIM_BLEND;
            D_actor_143900_80149630           = preset->blendFrames;
        } else {
            D_actor_143900_801496C4->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        D_actor_143900_801496C4->st.field_6 = 0;
        func_actor_143900_80132A9C(D_actor_143900_801496C8);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler of the second variant: applies `arg2` to the three
/// models it owns - its own task's and the two attachment tasks'. Bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden); bit 1 ORs in 0x4.
s32 func_actor_143900_80133254(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* own    = D_actor_143900_801496C8->extra.tmd;
    TmdObject* first  = D_actor_143900_801496C4->attachment1->extra.tmd;
    TmdObject* second = D_actor_143900_801496C4->attachment2->extra.tmd;

    if (arg2 & 1) {
        own->flags    = 0;
        first->flags  = 0;
        second->flags = 0;
    } else {
        own->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        first->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        second->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        own->flags    |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        first->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        second->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// The second walker's copy.
#define scriptedWalkPlace func_actor_143900_801332E4
#define gScriptedWalkWork D_actor_143900_801496C4
#include "../../shared/scripted_walk_place.inc.c"
#undef scriptedWalkPlace
#undef gScriptedWalkWork

/// Message 0x7DB handler of the second variant: the payload's halfword at 0x2
/// picks which of the two attachment tasks' models is shown - 0 shows the second
/// (`attachment2`) and hides the first, 1 the reverse; any other value leaves
/// both.
s32 func_actor_143900_80133360(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    TmdObject* first;
    TmdObject* second;

    first  = D_actor_143900_801496C4->attachment1->extra.tmd;
    second = D_actor_143900_801496C4->attachment2->extra.tmd;
    switch (msg->command) {
        case 0:
            second->flags = 0;
            first->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 1:
            first->flags  = 0;
            second->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    return 0;
}

/// The second walker's copy.
#define scriptedWalkTo func_actor_143900_801333C4
#undef SCRIPTED_WALK_MODE
#define SCRIPTED_WALK_MODE D_actor_143900_801496CC
#include "../../shared/scripted_walk_to.inc.c"
#undef scriptedWalkTo
#undef SCRIPTED_WALK_MODE
#define SCRIPTED_WALK_MODE gScriptedWalkModeValue
