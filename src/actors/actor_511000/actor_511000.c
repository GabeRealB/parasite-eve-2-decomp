#include "actors/actor_511000.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/model_placement.h"
#include "../../shared/actor_messages.h"

extern GpuImageUpload D_actor_511000_80146F94[2];

extern GpuImageUpload D_actor_511000_80146C74[2];

extern GpuImageUpload D_actor_511000_801472B4[2];

/// Work block of the No. 9 golem, the package's enemy.
///
/// The enemy's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. The golem's model and the three models chained under
/// its parts borrow `light` and `color` for as long as the block lives, which
/// is what lets one fade darken all four.
///
/// While the clip is below 3 the golem is lit from the area at its position
/// and casts a ground shadow. A clip from 3 up is its exit: the lighting is no
/// longer refreshed, both matrices are scaled by 15/16 every third tick, and
/// the enemy is destroyed once `animTicks` passes 96.
typedef struct {
    ActorAnimRig19 rig;            // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    MATRIX         color;          // Light-colour matrix lent to the golem's model and its attached models
    MATRIX         light;          // Light-direction matrix lent to the same models
    s32            animId;         // Clip the last play request seeded the slots with (0 none yet: the slots are not ticked; 3 and up fade the model out)
    s16            animTicks;      // Ticks since the last play request; paces the fade and ends the enemy
    byte           unknown_482[6]; // Allocated but never accessed; role unproven
} _Actor511000No9GolemWork;
STATIC_ASSERT_SIZEOF(_Actor511000No9GolemWork, 0x488);

/// Colours in the helicopter's faded palette row.
#define ACTOR_511000_PALETTE_COLORS 16

/// Bytes of a palette row: each colour is a 15-bit value stored low byte
/// first, which is how the fade reads and writes it.
#define ACTOR_511000_PALETTE_BYTES (ACTOR_511000_PALETTE_COLORS * 2)

/// `_Actor511000HelicopterWork::paletteFade` with the palette fully faded,
/// 1.0 in 4.12 fixed point.
#define ACTOR_511000_PALETTE_FADE_FULL 0x1000

/// Ticks the helicopter's palette is held fully faded before it snaps back.
#define ACTOR_511000_PALETTE_HOLD_TICKS 30

/// Work block of the helicopter.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. The helicopter's model, and through it the three
/// models chained under it, borrow `light` and `color` for as long as the
/// block lives.
///
/// While the helicopter is drawn, one sixteen-colour row of video memory is
/// rewritten every tick from `palette`, a blend of the package's two stored
/// palettes. The blend snaps to the first palette, fades to the second over
/// three ticks, holds there for `ACTOR_511000_PALETTE_HOLD_TICKS` and snaps
/// back, so whatever is textured with that row flashes once a cycle. The
/// upload record borrows `palette`, which has to stay live while the task
/// runs.
typedef struct {
    byte   unknown_0[8];                        // Allocated but never accessed; role unproven
    s32    freeCountdown;                       // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    u8     palette[ACTOR_511000_PALETTE_BYTES]; // Blended palette row as uploaded: `ACTOR_511000_PALETTE_COLORS` colours, low byte first
    s16    paletteFade;                         // Share of the second stored palette in the blend, 4.12 fixed point (0 first palette only, `ACTOR_511000_PALETTE_FADE_FULL` second only)
    s8     paletteHold;                         // Ticks the full fade still holds; counted down only once the fade is full, and at 0 the blend snaps back
    s8     partsSpawned;                        // Set once the first show command has spawned the three attached models, so a later one does not repeat it
    MATRIX light;                               // Light-direction matrix lent to the model object
    MATRIX color;                               // Light-colour matrix lent to the model object
} _Actor511000HelicopterWork;
STATIC_ASSERT_SIZEOF(_Actor511000HelicopterWork, 0x70);

/// `_Actor511000RupertBroderickWork::blinkStep`: the eye image the blink
/// posts next.
///
/// The command that starts a blink posts the half-open eyes itself; the steps
/// then post the closed, half-open and open eyes in turn.
enum {
    ACTOR_511000_BLINK_NONE   = 0, // No blink in progress
    ACTOR_511000_BLINK_CLOSED = 1, // The closed eyes are posted next
    ACTOR_511000_BLINK_HALF   = 2, // The half-open eyes are posted next
    ACTOR_511000_BLINK_OPEN   = 3, // The open eyes are posted next, which ends the blink
};

/// Work block of Rupert Broderick's body.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. The model object borrows `light` and `color` for as
/// long as the block lives; the lighting is taken at model part 1, not at the
/// root coordinate.
///
/// A play request rebinds the rig when its bank differs from `bank` and
/// reseeds the slots when its clip differs from `animId`; both start as
/// `ACTOR_MODEL_STATE_NONE`, so the first request does both. The rest is the
/// two models the body carries, the blink that swaps the eye texture of the
/// face, and the delayed free of the model's buffers once it has been hidden.
typedef struct {
    ActorAnimRig20 rig;             // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    s32            ticking;         // Set once a clip has been applied, never cleared: the slots are ticked each frame
    s32            animId;          // Clip the slots were last seeded with, within `bank`
    s32            bank;            // Index, in the package's animation bank table, of the bank the rig is bound to
    s32            freeCountdown;   // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    MATRIX         light;           // Light-direction matrix lent to the model object
    MATRIX         color;           // Light-colour matrix lent to the model object
    Task*          gunTask;         // Task of the revolver model chained under body part 8; the shot's sound and muzzle flash are placed at its root
    Task*          propTask;        // Task of the second model, chained under body part 12; an actor command shows it as it hides the revolver
    s16            blinkFrameDelay; // Value `blinkCountdown` restarts from after the closed and the half-open eyes: each is shown for this many ticks plus one
    s16            blinkCountdown;  // Ticks left before the blink posts its next eye image, which the tick taking it below 0 does; not reset as a blink starts or ends
    s16            blinkStep;       // Eye image the blink posts next (0 `ACTOR_511000_BLINK_NONE`, else `_CLOSED`, `_HALF` or `_OPEN`)
    s16            shotTicks;       // Ticks spent playing clip 1; the sixteenth fires the revolver's sound and muzzle flash. Never reset
} _Actor511000RupertBroderickWork;
STATIC_ASSERT_SIZEOF(_Actor511000RupertBroderickWork, 0x4D4);

static void _modelPlacementAttachPartTask(Task* childTask);
static void _modelPlacementMirrorParentDrawFlags(Task* childTask);
static void func_actor_511000_80131E78(Task* arg0);
static void func_actor_511000_80132048(Task* arg0);
static void func_actor_511000_80132224(Task* task);
static void func_actor_511000_80132480(Task* task);
static void func_actor_511000_801325A4(Task* task);
static void func_actor_511000_801329C4(Task* task);
static void func_actor_511000_80132B14(Task* task, CVECTOR* col, s8* rgb);
static void func_actor_511000_80133034(Task* task);
static void func_actor_511000_801330F0(Task* task);
static void func_actor_511000_80133220(Task* task);
static void func_actor_511000_80133240(Task* task);
static void func_actor_511000_801332E4(Task* task);
static void func_actor_511000_801333A4(Task* task);
static void func_actor_511000_801333C4(Task* task);
static void func_actor_511000_80133498(Task* task);
static void func_actor_511000_801336E0(Task* task, SVECTOR* rots, SVECTOR* trans, s32 index);
static void func_actor_511000_80133760(Task* task);
static void func_actor_511000_801337F0(Task* task);
static void func_actor_511000_80133958(Enemy* enemy, Task* task);
static void func_actor_511000_80133B80(Enemy* enemy, Task* task);
static void func_actor_511000_80133F48(Enemy* enemy, Task* task);
static void func_actor_511000_80133F88(Enemy* enemy, Task* task);
static void func_actor_511000_8013401C(Enemy* enemy, Task* task);
static void func_actor_511000_8013405C(Enemy* enemy, Task* task);
static void func_actor_511000_801340F0(Enemy* enemy, Task* task);
static void func_actor_511000_80134130(Enemy* enemy, Task* task);

/// State table of a child chained under a part of its spawner's model: the
/// attach state, an empty tick and the kill.
static const TaskFuncTable3 D_actor_511000_80131E24 = {
    _modelPlacementAttachPartTask,
    func_actor_511000_80132224,
    taskKill,
};

/// State table of a child chained under a part of its spawner's model that
/// also follows the spawner's active-draw and buffer flags: the attach state,
/// the flag-mirroring tick and the kill.
static const TaskFuncTable3 D_actor_511000_80131E30 = {
    modelPlacementAttachChild,
    _modelPlacementMirrorParentDrawFlags,
    taskKill,
};

/// State table of the task that owns the `_Actor511000RupertBroderickWork` block: its spawn
/// state, the per-frame tick and the enemy task exit.
static const TaskFuncTable3 D_actor_511000_80131E3C = {
    func_actor_511000_80132480,
    func_actor_511000_80131E78,
    enemyTaskExit,
};

/// State table of the task that owns the `_Actor511000HelicopterWork` block: its spawn
/// state, the per-frame tick and the kill.
static const TaskFuncTable3 D_actor_511000_80131E48 = {
    func_actor_511000_80133034,
    func_actor_511000_801330F0,
    func_actor_511000_80133220,
};

/// State table of a child placed at a translation preset under its spawner:
/// the attach state, the spinning tick and the kill.
static const TaskFuncTable3 D_actor_511000_80131E54 = {
    func_actor_511000_80133240,
    func_actor_511000_801332E4,
    func_actor_511000_801333A4,
};

/// State table of a child posed from the kill-countdown rotations: the attach
/// state, the tick that follows the countdown and the kill.
static const TaskFuncTable3 D_actor_511000_80131E60 = {
    func_actor_511000_801333C4,
    func_actor_511000_801329C4,
    func_actor_511000_80133498,
};

/// The enemy's three state handlers - spawn, per-frame tick and teardown.
static const EnemyTaskFuncTable3 D_actor_511000_80131E6C = {
    func_actor_511000_80133958,
    func_actor_511000_80133B80,
    enemyDestroy,
};

/// Camera path `func_actor_511000_801330F0` walks once the session reaches
/// mode 0x18, one 0x24-byte `ViewCamera` per step of the kill countdown: the
/// rotation and projection plane repeat down the table while the translation
/// descends, so the spawn of a view task per index pans the camera as the
/// actor goes down. Handed straight to `Gp_TrySpawnViewTask`, exactly as
/// `Gp_SpawnViewTasks` hands its own stage record.
extern ViewCamera D_actor_511000_80147EE4[];

/// The three texture upload lists used by the tick state and message handler.
/// Each contains a 24-word by 16-row copy and a terminator; `pixels` points at
/// the packed texture words.

/// Animation sources the animation message handler selects by index.
extern AnimationSet*  D_actor_511000_801472D4[4];
extern AnimationSet** D_actor_511000_801472E4[1];

/// Spawn table `func_actor_511000_80132480` starts its two child tasks from,
/// and the message table it parks in `Task::msgTable`.
extern TaskDesc D_actor_511000_801472E8[];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_511000_8014730C[6];

/// Offset `Gp_SpawnEff` places the tick state's effect at.
extern SVECTOR D_actor_511000_8014733C;

extern SVECTOR D_actor_511000_80147344[];
extern SVECTOR D_actor_511000_80147704[];
extern SVECTOR D_actor_511000_80147AC4[];
extern u8      D_actor_511000_80147E84[ACTOR_511000_PALETTE_BYTES];
extern u8      D_actor_511000_80147EC4[ACTOR_511000_PALETTE_BYTES];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_511000_80148FC4[];

/// Translation presets `func_actor_511000_80133760` copies onto the root
/// coordinate; `Task::spawnArg1` selects the entry.
extern SVECTOR D_actor_511000_80148FE4[];

extern CVECTOR D_actor_511000_80149004[];
extern DVECTOR D_actor_511000_80149014[];

/// Spawn table and per-child args for the children spawned on message 1.
extern TaskDesc D_actor_511000_80139924[];
extern s32      D_actor_511000_80149054[];

/// Spawn table for the three children `func_actor_511000_80133958` creates.
extern TaskDesc D_actor_511000_80155070[];
/// Message table and animation data `func_actor_511000_80133958` installs.
extern TaskMessageEntry D_actor_511000_801550A0[4];
extern AnimationSet*    D_actor_511000_801550C0[4];

static TmdSource _gActor511000RupertBroderickBody2;
static TmdSource _gActor511000RupertBroderickMongoose;
static TmdSource _gActor511000Prop1;
s32              func_actor_511000_80132604(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_511000_801327A0(Task*, s32, s32, s32);
s32              func_actor_511000_8013287C(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
s32              func_actor_511000_80132904(Task*, s32, s32, s32);
void             func_actor_511000_80132150(Task*);
void             func_actor_511000_8013222C(Task*);
void             func_actor_511000_80132428(Task*);

static AnimationSet _gActor511000Animation1121C;
static AnimationSet _gActor511000Animation130D4;
static AnimationSet _gActor511000Animation14B2C;

s32  func_actor_511000_801334B8(Task*, s32, s32, s32);
s32  func_actor_511000_801334C4(Task* task, s32 msgId, ActorTransform* args, s32);
s32  func_actor_511000_80133554(Task*, s32, s32, s32);
s32  func_actor_511000_80133DEC(Task*, s32, AnimationPlayRequest*, s32);
s32  func_actor_511000_80133EAC(Task*, s32, s32, s32);
void func_actor_511000_80133D90(Task*);
void func_actor_511000_80133EF4(Task*);
void func_actor_511000_80133FC8(Task*);
void func_actor_511000_8013409C(Task*);

static TmdSource _gActor511000HelicopterBase;
static TmdSource _gActor511000Prop2;
static TmdSource _gActor511000Prop3;
static TmdSource _gActor511000Model0A41C;
void             func_actor_511000_80133850(Task*);
void             func_actor_511000_801338A8(Task*);
void             func_actor_511000_80133900(Task*);

static AnimationPackedPose _gActor511000Animation04CC8Bank1[84] = {
#include "assets/actor_511000_animation_04CC8_bank1.inc"
};

static AnimationPackedRotation _gActor511000Animation04CC8Bank4[1028] = {
#include "assets/actor_511000_animation_04CC8_bank4.inc"
};

static AnimationRecord _gActor511000Animation04CC8Records[1364] = {
#include "assets/actor_511000_animation_04CC8_records.inc"
};

static u16 _gActor511000Animation04CC8Indices[20] = {
#include "assets/actor_511000_animation_04CC8_indices.inc"
};

AnimationSet gActor511000Animation04CC8 = {
    _gActor511000Animation04CC8Records,
    _gActor511000Animation04CC8Indices,
    { NULL, _gActor511000Animation04CC8Bank1, NULL, NULL, _gActor511000Animation04CC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor511000Animation07ADCBank1[75] = {
#include "assets/actor_511000_animation_07ADC_bank1.inc"
};

static AnimationPackedRotation _gActor511000Animation07ADCBank4[1201] = {
#include "assets/actor_511000_animation_07ADC_bank4.inc"
};

static AnimationRecord _gActor511000Animation07ADCRecords[1503] = {
#include "assets/actor_511000_animation_07ADC_records.inc"
};

static u16 _gActor511000Animation07ADCIndices[20] = {
#include "assets/actor_511000_animation_07ADC_indices.inc"
};

AnimationSet gActor511000Animation07ADC = {
    _gActor511000Animation07ADCRecords,
    _gActor511000Animation07ADCIndices,
    { NULL, _gActor511000Animation07ADCBank1, NULL, NULL, _gActor511000Animation07ADCBank4, NULL, NULL, NULL },
};

TaskDesc D_actor_511000_80139924[4] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_511000_80133850, { .model = &_gActor511000HelicopterBase } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_511000_801338A8, { .model = &_gActor511000Prop2 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_511000_801338A8, { .model = &_gActor511000Prop3 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_511000_80133900, { .model = &_gActor511000Model0A41C } },
};

static TmdBone _gActor511000HelicopterBaseSkeleton[1] = {
#include "assets/actor_511000_helicopter_base_skeleton.inc"
};

static u32 _gActor511000HelicopterBasePartVerts[1] = {
#include "assets/actor_511000_helicopter_base_partVerts.inc"
};

static SVECTOR _gActor511000HelicopterBaseVerts[262] = {
#include "assets/actor_511000_helicopter_base_verts.inc"
};

static SVECTOR _gActor511000HelicopterBaseNormals[163] = {
#include "assets/actor_511000_helicopter_base_normals.inc"
};

static u32 _gActor511000HelicopterBaseStream[1299] = {
#include "assets/actor_511000_helicopter_base_stream.inc"
};

static TmdSource _gActor511000HelicopterBase = {
    0,
    9368,
    0,
    1,
    _gActor511000HelicopterBasePartVerts,
    _gActor511000HelicopterBaseVerts,
    _gActor511000HelicopterBaseNormals,
    _gActor511000HelicopterBaseSkeleton,
    _gActor511000HelicopterBaseStream,
};

static TmdBone _gActor511000Prop2Skeleton[1] = {
#include "assets/actor_511000_prop_2_skeleton.inc"
};

static u32 _gActor511000Prop2PartVerts[1] = {
#include "assets/actor_511000_prop_2_partVerts.inc"
};

static SVECTOR _gActor511000Prop2Verts[59] = {
#include "assets/actor_511000_prop_2_verts.inc"
};

static SVECTOR _gActor511000Prop2Normals[1] = {
#include "assets/actor_511000_prop_2_normals.inc"
};

static u32 _gActor511000Prop2Stream[111] = {
#include "assets/actor_511000_prop_2_stream.inc"
};

static TmdSource _gActor511000Prop2 = {
    0,
    840,
    0,
    1,
    _gActor511000Prop2PartVerts,
    _gActor511000Prop2Verts,
    _gActor511000Prop2Normals,
    _gActor511000Prop2Skeleton,
    _gActor511000Prop2Stream,
};

static TmdBone _gActor511000Prop3Skeleton[1] = {
#include "assets/actor_511000_prop_3_skeleton.inc"
};

static u32 _gActor511000Prop3PartVerts[1] = {
#include "assets/actor_511000_prop_3_partVerts.inc"
};

static SVECTOR _gActor511000Prop3Verts[12] = {
#include "assets/actor_511000_prop_3_verts.inc"
};

static SVECTOR _gActor511000Prop3Normals[2] = {
#include "assets/actor_511000_prop_3_normals.inc"
};

static u32 _gActor511000Prop3Stream[41] = {
#include "assets/actor_511000_prop_3_stream.inc"
};

static TmdSource _gActor511000Prop3 = {
    0,
    280,
    0,
    1,
    _gActor511000Prop3PartVerts,
    _gActor511000Prop3Verts,
    _gActor511000Prop3Normals,
    _gActor511000Prop3Skeleton,
    _gActor511000Prop3Stream,
};

static TmdBone _gActor511000Model0A41CSkeleton[1] = {
#include "assets/actor_511000_model_0A41C_skeleton.inc"
};

static u32 _gActor511000Model0A41CPartVerts[1] = {
#include "assets/actor_511000_model_0A41C_partVerts.inc"
};

static SVECTOR _gActor511000Model0A41CVerts[36] = {
#include "assets/actor_511000_model_0A41C_verts.inc"
};

static SVECTOR _gActor511000Model0A41CNormals[15] = {
#include "assets/actor_511000_model_0A41C_normals.inc"
};

static u32 _gActor511000Model0A41CStream[264] = {
#include "assets/actor_511000_model_0A41C_stream.inc"
};

static TmdSource _gActor511000Model0A41C = {
    0,
    1576,
    0,
    1,
    _gActor511000Model0A41CPartVerts,
    _gActor511000Model0A41CVerts,
    _gActor511000Model0A41CNormals,
    _gActor511000Model0A41CSkeleton,
    _gActor511000Model0A41CStream,
};

static TmdBone _gActor511000RupertBroderickBody2Skeleton[20] = {
#include "assets/rupert_broderick_body_2_skeleton.inc"
};

static u32 _gActor511000RupertBroderickBody2PartVerts[20] = {
#include "assets/rupert_broderick_body_2_partVerts.inc"
};

static SVECTOR _gActor511000RupertBroderickBody2Verts[386] = {
#include "assets/rupert_broderick_body_2_verts.inc"
};

static SVECTOR _gActor511000RupertBroderickBody2Normals[385] = {
#include "assets/rupert_broderick_body_2_normals.inc"
};

static u32 _gActor511000RupertBroderickBody2Stream[4327] = {
#include "assets/rupert_broderick_body_2_stream.inc"
};

static TmdSource _gActor511000RupertBroderickBody2 = {
    0,
    23980,
    6012,
    20,
    _gActor511000RupertBroderickBody2PartVerts,
    _gActor511000RupertBroderickBody2Verts,
    _gActor511000RupertBroderickBody2Normals,
    _gActor511000RupertBroderickBody2Skeleton,
    _gActor511000RupertBroderickBody2Stream,
};

static TmdBone _gActor511000RupertBroderickMongooseSkeleton[1] = {
#include "assets/rupert_broderick_mongoose_skeleton.inc"
};

static u32 _gActor511000RupertBroderickMongoosePartVerts[1] = {
#include "assets/rupert_broderick_mongoose_partVerts.inc"
};

static SVECTOR _gActor511000RupertBroderickMongooseVerts[28] = {
#include "assets/rupert_broderick_mongoose_verts.inc"
};

static SVECTOR _gActor511000RupertBroderickMongooseNormals[28] = {
#include "assets/rupert_broderick_mongoose_normals.inc"
};

static u32 _gActor511000RupertBroderickMongooseStream[211] = {
#include "assets/rupert_broderick_mongoose_stream.inc"
};

static TmdSource _gActor511000RupertBroderickMongoose = {
    0,
    1464,
    0,
    1,
    _gActor511000RupertBroderickMongoosePartVerts,
    _gActor511000RupertBroderickMongooseVerts,
    _gActor511000RupertBroderickMongooseNormals,
    _gActor511000RupertBroderickMongooseSkeleton,
    _gActor511000RupertBroderickMongooseStream,
};

static TmdBone _gActor511000Prop1Skeleton[1] = {
#include "assets/actor_511000_prop_1_skeleton.inc"
};

static u32 _gActor511000Prop1PartVerts[1] = {
#include "assets/actor_511000_prop_1_partVerts.inc"
};

static SVECTOR _gActor511000Prop1Verts[14] = {
#include "assets/actor_511000_prop_1_verts.inc"
};

static u32 _gActor511000Prop1Stream[74] = {
#include "assets/actor_511000_prop_1_stream.inc"
};

static TmdSource _gActor511000Prop1 = {
    0,
    504,
    0,
    1,
    _gActor511000Prop1PartVerts,
    _gActor511000Prop1Verts,
    &_gActor511000Prop1Verts[14],
    _gActor511000Prop1Skeleton,
    _gActor511000Prop1Stream,
};

static AnimationPackedPose _gActor511000Animation1121CBank1[5] = {
#include "assets/actor_511000_animation_1121C_bank1.inc"
};

static AnimationPackedRotation _gActor511000Animation1121CBank4[80] = {
#include "assets/actor_511000_animation_1121C_bank4.inc"
};

static AnimationRecord _gActor511000Animation1121CRecords[121] = {
#include "assets/actor_511000_animation_1121C_records.inc"
};

static u16 _gActor511000Animation1121CIndices[20] = {
#include "assets/actor_511000_animation_1121C_indices.inc"
};

static AnimationSet _gActor511000Animation1121C = {
    _gActor511000Animation1121CRecords,
    _gActor511000Animation1121CIndices,
    { NULL, _gActor511000Animation1121CBank1, NULL, NULL, _gActor511000Animation1121CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor511000Animation130D4Bank1[118] = {
#include "assets/actor_511000_animation_130D4_bank1.inc"
};

static AnimationPackedRotation _gActor511000Animation130D4Bank4[422] = {
#include "assets/actor_511000_animation_130D4_bank4.inc"
};

static AnimationRecord _gActor511000Animation130D4Records[1170] = {
#include "assets/actor_511000_animation_130D4_records.inc"
};

static u16 _gActor511000Animation130D4Indices[20] = {
#include "assets/actor_511000_animation_130D4_indices.inc"
};

static AnimationSet _gActor511000Animation130D4 = {
    _gActor511000Animation130D4Records,
    _gActor511000Animation130D4Indices,
    { NULL, _gActor511000Animation130D4Bank1, NULL, NULL, _gActor511000Animation130D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor511000Animation14B2CBank1[19] = {
#include "assets/actor_511000_animation_14B2C_bank1.inc"
};

static AnimationPackedRotation _gActor511000Animation14B2CBank4[703] = {
#include "assets/actor_511000_animation_14B2C_bank4.inc"
};

static AnimationRecord _gActor511000Animation14B2CRecords[906] = {
#include "assets/actor_511000_animation_14B2C_records.inc"
};

static u16 _gActor511000Animation14B2CIndices[20] = {
#include "assets/actor_511000_animation_14B2C_indices.inc"
};

static AnimationSet _gActor511000Animation14B2C = {
    _gActor511000Animation14B2CRecords,
    _gActor511000Animation14B2CIndices,
    { NULL, _gActor511000Animation14B2CBank1, NULL, NULL, _gActor511000Animation14B2CBank4, NULL, NULL, NULL },
};

u_long D_actor_511000_80146974[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA6A2A28D,
    0xADA5ADA5,
    0xB1B1B1AE,
    0xAEAEADAE,
    0x65929AA9,
    0xADA68766,
    0xADA5A5AD,
    0xADADADAD,
    0x8B969EA5,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0xA5958C8D,
    0xFFB9C29C,
    0xAE61B5FF,
    0xA9A9A9A5,
    0x7C788F95,
    0xA7958785,
    0x9FA5A68B,
    0xFFFFB561,
    0x9890C5BF,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0x9B8C828D,
    0xFFB21716,
    0xCECCFFFF,
    0x9595A890,
    0x7B65864E,
    0x998D8665,
    0xCEACA997,
    0xB7FFFFC4,
    0x9BCCD5D0,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9894028A,
    0x1CCB0B1,
    0x81958B9B,
    0x72677882,
    0x998E8568,
    0xCC9B95A2,
    0x98AE7EC6,
    0x87891601,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x87879A9B,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpuImageUpload D_actor_511000_80146C74[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 24, 16 }, D_actor_511000_80146974 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_511000_80146C94[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA2A2A28D,
    0x8C8C8C8C,
    0xA9A9A68C,
    0xAEAEA9A9,
    0x65929AA9,
    0xADA68766,
    0xA9A9A5AD,
    0x9595A9A9,
    0x95959595,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0x95958C8D,
    0xB0C2A995,
    0xC2C2B0B0,
    0xA9A9A9A9,
    0x7C788F95,
    0xA7958785,
    0xA9A9A68B,
    0xC2B0B0C2,
    0x959595C2,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0xC28C828D,
    0xB0B0B0B0,
    0xB0B0B0B0,
    0x9595A8B0,
    0x7B65864E,
    0x998D8665,
    0xB0B0A997,
    0xB0B0B0B0,
    0x9BC2B0B0,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9894028A,
    0x1CCB0B1,
    0x81958B9B,
    0x72677882,
    0x998E8568,
    0xCC9B95A2,
    0x98AE7EC6,
    0x87891601,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x879A9A9A,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpuImageUpload D_actor_511000_80146F94[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 24, 16 }, D_actor_511000_80146C94 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_511000_80146FB4[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA2A2A28D,
    0xA2A2A2A2,
    0xB19898A2,
    0xAEAEADAE,
    0x65929AA9,
    0xADA68766,
    0xADA5A5AD,
    0x8B8B9898,
    0xA2A2A2A2,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0xA2958C8D,
    0x788FA2A2,
    0xA2A28F8F,
    0xA9A9A9A6,
    0x7C788F95,
    0xA7958785,
    0xA2A2A7A7,
    0x8F788F8F,
    0x959595A2,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0x958C828D,
    0x61ADADA9,
    0xAD616161,
    0xA9A99898,
    0x7B65864E,
    0x998D8665,
    0x61AD9897,
    0x61616161,
    0x95A9ADAD,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9898ADAD,
    0x98A9A9A9,
    0x818198AD,
    0x72677882,
    0x998E8568,
    0x98AD9898,
    0x98A9A9A9,
    0x87ADADAD,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x87879A9B,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpuImageUpload D_actor_511000_801472B4[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 24, 16 }, D_actor_511000_80146FB4 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

AnimationSet* D_actor_511000_801472D4[4] = {
    NULL,
    &_gActor511000Animation1121C,
    &_gActor511000Animation130D4,
    &_gActor511000Animation14B2C,
};

AnimationSet** D_actor_511000_801472E4[1] = {
    D_actor_511000_801472D4,
};

TaskDesc D_actor_511000_801472E8[3] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_511000_80132428, { .model = &_gActor511000RupertBroderickBody2 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_511000_80132150, { .model = &_gActor511000RupertBroderickMongoose } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_511000_8013222C, { .model = &_gActor511000Prop1 } },
};

TaskMessageEntry D_actor_511000_8014730C[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_511000_80132604 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_511000_801327A0 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_511000_8013287C },
    { 2016, func_actor_511000_80132904 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_actor_511000_8014733C = { -30, 440, 140, 0 };

SVECTOR D_actor_511000_80147344[120] = {
    { 0, 0, 0, 0 },
    { 0, -9, 0, 0 },
    { 0, -19, 0, 0 },
    { 0, -30, 0, 0 },
    { 0, -41, 0, 0 },
    { 0, -52, 0, 0 },
    { 0, -64, 0, 0 },
    { 0, -77, 0, 0 },
    { 0, -89, 0, 0 },
    { 0, -102, 0, 0 },
    { 0, -116, 0, 0 },
    { 0, -130, 0, 0 },
    { 0, -144, 0, 0 },
    { 0, -158, 0, 0 },
    { 0, -172, 0, 0 },
    { 0, -187, 0, 0 },
    { 0, -202, 0, 0 },
    { 0, -217, 0, 0 },
    { 0, -232, 0, 0 },
    { 0, -248, 0, 0 },
    { 0, -263, 0, 0 },
    { 0, -279, 0, 0 },
    { 0, -294, 0, 0 },
    { 0, -310, 0, 0 },
    { 0, -325, 0, 0 },
    { 0, -341, 0, 0 },
    { 0, -356, 0, 0 },
    { 0, -372, 0, 0 },
    { 0, -387, 0, 0 },
    { 0, -402, 0, 0 },
    { 0, -417, 0, 0 },
    { 0, -432, 0, 0 },
    { 0, -447, 0, 0 },
    { 0, -461, 0, 0 },
    { 0, -475, 0, 0 },
    { 0, -489, 0, 0 },
    { 0, -503, 0, 0 },
    { 0, -516, 0, 0 },
    { 0, -529, 0, 0 },
    { 0, -542, 0, 0 },
    { 0, -554, 0, 0 },
    { 0, -566, 0, 0 },
    { 0, -577, 0, 0 },
    { 0, -588, 0, 0 },
    { 0, -599, 0, 0 },
    { 0, -609, 0, 0 },
    { 0, -618, 0, 0 },
    { 0, -627, 0, 0 },
    { 0, -635, 0, 0 },
    { 0, -643, 0, 0 },
    { 0, -650, 0, 0 },
    { 0, -657, 0, 0 },
    { 0, -662, 0, 0 },
    { 0, -668, 0, 0 },
    { 0, -672, 0, 0 },
    { 0, -676, 0, 0 },
    { 0, -678, 0, 0 },
    { 0, -680, 0, 0 },
    { 0, -682, 0, 0 },
    { 0, -682, 0, 0 },
    { 0, -682, 0, 0 },
    { 0, -682, 0, 0 },
    { 0, -682, 0, 0 },
    { 0, -681, 0, 0 },
    { 0, -681, 0, 0 },
    { 0, -681, 0, 0 },
    { 0, -680, 0, 0 },
    { 0, -679, 0, 0 },
    { 0, -679, 0, 0 },
    { 0, -678, 0, 0 },
    { 0, -677, 0, 0 },
    { 0, -676, 0, 0 },
    { 0, -675, 0, 0 },
    { 0, -674, 0, 0 },
    { 0, -673, 0, 0 },
    { 0, -672, 0, 0 },
    { 0, -671, 0, 0 },
    { 0, -670, 0, 0 },
    { 0, -669, 0, 0 },
    { 0, -667, 0, 0 },
    { 0, -666, 0, 0 },
    { 0, -665, 0, 0 },
    { 0, -663, 0, 0 },
    { 0, -662, 0, 0 },
    { 0, -661, 0, 0 },
    { 0, -659, 0, 0 },
    { 0, -658, 0, 0 },
    { 0, -657, 0, 0 },
    { 0, -655, 0, 0 },
    { 0, -654, 0, 0 },
    { 0, -652, 0, 0 },
    { 0, -651, 0, 0 },
    { 0, -649, 0, 0 },
    { 0, -648, 0, 0 },
    { 0, -647, 0, 0 },
    { 0, -645, 0, 0 },
    { 0, -644, 0, 0 },
    { 0, -643, 0, 0 },
    { 0, -641, 0, 0 },
    { 0, -640, 0, 0 },
    { 0, -639, 0, 0 },
    { 0, -638, 0, 0 },
    { 0, -636, 0, 0 },
    { 0, -635, 0, 0 },
    { 0, -634, 0, 0 },
    { 0, -633, 0, 0 },
    { 0, -632, 0, 0 },
    { 0, -631, 0, 0 },
    { 0, -630, 0, 0 },
    { 0, -629, 0, 0 },
    { 0, -629, 0, 0 },
    { 0, -628, 0, 0 },
    { 0, -627, 0, 0 },
    { 0, -627, 0, 0 },
    { 0, -626, 0, 0 },
    { 0, -626, 0, 0 },
    { 0, -626, 0, 0 },
    { 0, -625, 0, 0 },
    { 0, -625, 0, 0 },
    { 0, -625, 0, 0 },
};

SVECTOR D_actor_511000_80147704[120] = {
    { 0, -0x6284, 0, 0 },
    { 0, -0x6284, 0, 0 },
    { 0, -0x6284, 0, 0 },
    { 0, -0x6286, 0, 0 },
    { 0, -0x6287, 0, 0 },
    { 0, -0x628A, 0, 0 },
    { 0, -0x628D, 0, 0 },
    { 0, -0x6290, 0, 0 },
    { 0, -0x6294, 0, 0 },
    { 0, -0x6299, 0, 0 },
    { 0, -0x629E, 0, 0 },
    { 0, -0x62A4, 0, 0 },
    { 0, -0x62AA, 0, 0 },
    { 0, -0x62B1, 0, 0 },
    { 0, -0x62B9, 0, 0 },
    { 0, -0x62C2, 0, 0 },
    { 0, -0x62CB, 0, 0 },
    { 0, -0x62D5, 0, 0 },
    { 0, -0x62E0, 0, 0 },
    { 0, -0x62EC, 0, 0 },
    { 0, -0x62F8, 0, 0 },
    { 0, -0x6306, 0, 0 },
    { 0, -0x6314, 0, 0 },
    { 0, -0x6323, 0, 0 },
    { 0, -0x6332, 0, 0 },
    { 0, -0x6343, 0, 0 },
    { 0, -0x6355, 0, 0 },
    { 0, -0x6367, 0, 0 },
    { 0, -0x637B, 0, 0 },
    { 0, -0x638F, 0, 0 },
    { 0, -0x63A4, 0, 0 },
    { 0, -0x63BB, 0, 0 },
    { 0, -0x63D2, 0, 0 },
    { 0, -0x63EA, 0, 0 },
    { 0, -0x6404, 0, 0 },
    { 0, -0x641E, 0, 0 },
    { 0, -0x643A, 0, 0 },
    { 0, -0x6456, 0, 0 },
    { 0, -0x6474, 0, 0 },
    { 0, -0x6493, 0, 0 },
    { 0, -0x64A8, 0, 0 },
    { 0, -0x6487, 0, 0 },
    { 0, -0x6464, 0, 0 },
    { 0, -0x6441, 0, 0 },
    { 0, -0x641C, 0, 0 },
    { 0, -0x63F6, 0, 0 },
    { 0, -0x63CF, 0, 0 },
    { 0, -0x63A6, 0, 0 },
    { 0, -0x637D, 0, 0 },
    { 0, -0x6352, 0, 0 },
    { 0, -0x6325, 0, 0 },
    { 0, -0x62F7, 0, 0 },
    { 0, -0x62C8, 0, 0 },
    { 0, -0x6298, 0, 0 },
    { 0, -0x6266, 0, 0 },
    { 0, -0x6233, 0, 0 },
    { 0, -0x61FE, 0, 0 },
    { 0, -0x61C8, 0, 0 },
    { 0, -0x6191, 0, 0 },
    { 0, -0x6158, 0, 0 },
    { 0, -0x611D, 0, 0 },
    { 0, -0x60E2, 0, 0 },
    { 0, -0x60A6, 0, 0 },
    { 0, -0x606A, 0, 0 },
    { 0, -0x602C, 0, 0 },
    { 0, -0x5FED, 0, 0 },
    { 0, -0x5FAD, 0, 0 },
    { 0, -0x5F6D, 0, 0 },
    { 0, -0x5F2B, 0, 0 },
    { 0, -0x5EE8, 0, 0 },
    { 0, -0x5EA5, 0, 0 },
    { 0, -0x5E60, 0, 0 },
    { 0, -0x5E1A, 0, 0 },
    { 0, -0x5DD3, 0, 0 },
    { 0, -0x5D8B, 0, 0 },
    { 0, -0x5D42, 0, 0 },
    { 0, -0x5CF7, 0, 0 },
    { 0, -0x5CAC, 0, 0 },
    { 0, -0x5C5F, 0, 0 },
    { 0, -0x5C11, 0, 0 },
    { 0, -0x5BC2, 0, 0 },
    { 0, -0x5B71, 0, 0 },
    { 0, -0x5B1F, 0, 0 },
    { 0, -0x5ACC, 0, 0 },
    { 0, -0x5A78, 0, 0 },
    { 0, -0x5A22, 0, 0 },
    { 0, -0x59CB, 0, 0 },
    { 0, -0x5973, 0, 0 },
    { 0, -0x5919, 0, 0 },
    { 0, -0x58BD, 0, 0 },
    { 0, -0x5861, 0, 0 },
    { 0, -0x5803, 0, 0 },
    { 0, -0x57A3, 0, 0 },
    { 0, -0x5742, 0, 0 },
    { 0, -0x56DF, 0, 0 },
    { 0, -0x567B, 0, 0 },
    { 0, -0x5615, 0, 0 },
    { 0, -0x55AD, 0, 0 },
    { 0, -0x5545, 0, 0 },
    { 0, -0x54DA, 0, 0 },
    { 0, -0x546E, 0, 0 },
    { 0, -0x5400, 0, 0 },
    { 0, -0x5390, 0, 0 },
    { 0, -0x531F, 0, 0 },
    { 0, -0x52AC, 0, 0 },
    { 0, -0x5237, 0, 0 },
    { 0, -0x51C1, 0, 0 },
    { 0, -0x5148, 0, 0 },
    { 0, -0x50CE, 0, 0 },
    { 0, -0x5053, 0, 0 },
    { 0, -0x4FD5, 0, 0 },
    { 0, -0x4F55, 0, 0 },
    { 0, -0x4ED4, 0, 0 },
    { 0, -0x4E50, 0, 0 },
    { 0, -0x4DCB, 0, 0 },
    { 0, -0x4D44, 0, 0 },
    { 0, -0x4CBB, 0, 0 },
    { 0, -0x4C30, 0, 0 },
    { 0, -0x4BA3, 0, 0 },
    { 0, -0x4B14, 0, 0 },
};

SVECTOR D_actor_511000_80147AC4[120] = {
    { -1027, 0, -245, 0 },
    { -1028, 0, -244, 0 },
    { -1029, 0, -243, 0 },
    { -1031, 0, -241, 0 },
    { -1033, 0, -238, 0 },
    { -1036, 0, -235, 0 },
    { -1040, 0, -231, 0 },
    { -1044, 0, -227, 0 },
    { -1048, 0, -222, 0 },
    { -1053, 0, -216, 0 },
    { -1059, 0, -210, 0 },
    { -1065, 0, -203, 0 },
    { -1072, 0, -196, 0 },
    { -1079, 0, -188, 0 },
    { -1086, 0, -180, 0 },
    { -1094, 0, -171, 0 },
    { -1103, 0, -162, 0 },
    { -1112, 0, -152, 0 },
    { -1121, 0, -142, 0 },
    { -1131, 0, -131, 0 },
    { -1141, 0, -120, 0 },
    { -1151, 0, -109, 0 },
    { -1162, 0, -97, 0 },
    { -1173, 0, -84, 0 },
    { -1185, 0, -72, 0 },
    { -1196, 0, -58, 0 },
    { -1209, 0, -45, 0 },
    { -1221, 0, -31, 0 },
    { -1234, 0, -17, 0 },
    { -1247, 0, -2, 0 },
    { -1260, 0, 12, 0 },
    { -1273, 0, 27, 0 },
    { -1287, 0, 43, 0 },
    { -1301, 0, 59, 0 },
    { -1315, 0, 75, 0 },
    { -1330, 0, 92, 0 },
    { -1344, 0, 109, 0 },
    { -1359, 0, 126, 0 },
    { -1374, 0, 143, 0 },
    { -1389, 0, 160, 0 },
    { -1404, 0, 178, 0 },
    { -1420, 0, 196, 0 },
    { -1435, 0, 214, 0 },
    { -1451, 0, 233, 0 },
    { -1466, 0, 251, 0 },
    { -1481, 0, 269, 0 },
    { -1495, 0, 286, 0 },
    { -1506, 0, 302, 0 },
    { -1517, 0, 317, 0 },
    { -1525, 0, 330, 0 },
    { -1533, 0, 343, 0 },
    { -1538, 0, 355, 0 },
    { -1543, 0, 365, 0 },
    { -1546, 0, 374, 0 },
    { -1547, 0, 383, 0 },
    { -1547, 0, 390, 0 },
    { -1546, 0, 396, 0 },
    { -1543, 0, 401, 0 },
    { -1539, 0, 405, 0 },
    { -1534, 0, 408, 0 },
    { -1527, 0, 411, 0 },
    { -1519, 0, 412, 0 },
    { -1510, 0, 411, 0 },
    { -1499, 0, 410, 0 },
    { -1488, 0, 408, 0 },
    { -1474, 0, 405, 0 },
    { -1460, 0, 401, 0 },
    { -1445, 0, 396, 0 },
    { -1428, 0, 390, 0 },
    { -1410, 0, 383, 0 },
    { -1391, 0, 375, 0 },
    { -1371, 0, 366, 0 },
    { -1350, 0, 356, 0 },
    { -1328, 0, 345, 0 },
    { -1305, 0, 333, 0 },
    { -1280, 0, 320, 0 },
    { -1255, 0, 307, 0 },
    { -1228, 0, 292, 0 },
    { -1201, 0, 276, 0 },
    { -1172, 0, 259, 0 },
    { -1143, 0, 242, 0 },
    { -1112, 0, 223, 0 },
    { -1081, 0, 204, 0 },
    { -1049, 0, 184, 0 },
    { -1015, 0, 162, 0 },
    { -981, 0, 140, 0 },
    { -946, 0, 117, 0 },
    { -911, 0, 93, 0 },
    { -874, 0, 68, 0 },
    { -837, 0, 43, 0 },
    { -801, 0, 17, 0 },
    { -769, 0, -5, 0 },
    { -742, 0, -26, 0 },
    { -718, 0, -45, 0 },
    { -698, 0, -62, 0 },
    { -681, 0, -78, 0 },
    { -667, 0, -92, 0 },
    { -656, 0, -105, 0 },
    { -648, 0, -116, 0 },
    { -642, 0, -125, 0 },
    { -639, 0, -134, 0 },
    { -637, 0, -141, 0 },
    { -638, 0, -147, 0 },
    { -640, 0, -152, 0 },
    { -644, 0, -156, 0 },
    { -649, 0, -159, 0 },
    { -655, 0, -161, 0 },
    { -662, 0, -163, 0 },
    { -669, 0, -164, 0 },
    { -677, 0, -165, 0 },
    { -685, 0, -165, 0 },
    { -694, 0, -164, 0 },
    { -702, 0, -164, 0 },
    { -709, 0, -163, 0 },
    { -716, 0, -162, 0 },
    { -722, 0, -161, 0 },
    { -728, 0, -160, 0 },
    { -732, 0, -159, 0 },
    { -734, 0, -159, 0 },
    { -735, 0, -159, 0 },
};

u8 D_actor_511000_80147E84[ACTOR_511000_PALETTE_BYTES] = {
    255,
    255,
    2,
    128,
    3,
    128,
    6,
    128,
    9,
    128,
    13,
    128,
    16,
    128,
    127,
    148,
    127,
    177,
    18,
    132,
    63,
    169,
    223,
    156,
    23,
    132,
    27,
    132,
    29,
    132,
    0,
    128,
};

GpuImageUpload D_actor_511000_80147EA4[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 264, 16, 1 }, (u_long*)D_actor_511000_80147E84 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u8 D_actor_511000_80147EC4[ACTOR_511000_PALETTE_BYTES] = {
    255,
    255,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    0,
    128,
};

ViewCamera D_actor_511000_80147EE4[120] = {
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5598, 5926, 124 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5599, 5926, 122 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5599, 5926, 118 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5601, 5927, 110 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5602, 5927, 100 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5605, 5928, 86 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5607, 5930, 70 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5610, 5931, 52 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5614, 5933, 30 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5617, 5934, 6 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5622, 5936, -19 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5626, 5938, -47 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5630, 5940, -78 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5635, 5943, -111 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5640, 5945, -146 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5645, 5947, -184 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5651, 5950, -223 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5656, 5953, -264 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5661, 5955, -306 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5667, 5958, -351 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5672, 5961, -397 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5677, 5963, -444 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5682, 5966, -493 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5688, 5969, -543 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5693, 5972, -595 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5697, 5975, -648 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5702, 5977, -702 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5707, 5980, -756 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5711, 5983, -812 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5715, 5985, -869 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5719, 5988, -926 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5722, 5990, -985 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5725, 5993, -1043 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5728, 5995, -1103 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5730, 5997, -1162 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5732, 5999, -1222 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5734, 6001, -1282 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5735, 6003, -1343 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5736, 6005, -1403 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5736, 6006, -1464 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5736, 6008, -1524 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5735, 6009, -1585 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5734, 6010, -1644 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5732, 6011, -1704 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5730, 6012, -1763 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5727, 6012, -1822 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5724, 6013, -1880 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5719, 6013, -1937 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5714, 6012, -1993 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5708, 6012, -2048 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5701, 6011, -2103 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5694, 6010, -2156 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5685, 6009, -2207 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5675, 6007, -2258 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5663, 6005, -2306 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5650, 6002, -2353 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5634, 5998, -2397 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5616, 5993, -2439 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5593, 5987, -2476 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5564, 5979, -2505 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5527, 5968, -2521 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5486, 5956, -2521 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5446, 5944, -2510 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5407, 5931, -2492 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5370, 5919, -2469 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5334, 5908, -2443 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5299, 5897, -2414 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5265, 5885, -2383 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5232, 5875, -2350 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5199, 5864, -2316 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5167, 5853, -2281 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5136, 5843, -2245 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5105, 5833, -2207 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5074, 5822, -2169 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5045, 5812, -2131 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5015, 5802, -2091 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4986, 5793, -2051 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4958, 5783, -2011 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4930, 5774, -1971 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4902, 5764, -1930 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4875, 5755, -1889 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4849, 5746, -1847 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4823, 5737, -1806 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4797, 5728, -1765 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4772, 5720, -1724 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4748, 5711, -1683 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4724, 5703, -1642 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4701, 5695, -1601 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4678, 5687, -1561 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4656, 5679, -1521 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4634, 5672, -1481 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4614, 5665, -1442 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4593, 5657, -1404 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4573, 5651, -1366 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4554, 5644, -1329 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4536, 5637, -1292 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4518, 5631, -1257 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4501, 5625, -1222 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4485, 5619, -1188 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4469, 5614, -1155 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4454, 5608, -1123 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4440, 5603, -1092 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4426, 5598, -1063 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4413, 5594, -1034 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4401, 5589, -1007 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4389, 5585, -981 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4379, 5581, -957 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4368, 5578, -934 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4359, 5574, -913 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4351, 5571, -893 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4343, 5568, -874 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4336, 5566, -858 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4330, 5564, -843 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4324, 5562, -830 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4320, 5560, -819 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4316, 5559, -810 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4313, 5558, -802 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4311, 5557, -797 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4309, 5556, -794 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4309, 5556, -793 } }, 447 },
};

TaskMessageEntry D_actor_511000_80148FC4[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_511000_801334B8 },
    { ACTOR_MESSAGE_PLACE, func_actor_511000_801334C4 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_511000_80133554 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_actor_511000_80148FE4[4] = {
    { 0, -0x3A98, 0, 0 },
    { 0, -1350, 1360, 0 },
    { 340, -230, -5790, 0 },
    { 0, 1520, 50, 0 },
};

CVECTOR D_actor_511000_80149004[4] = {
    { 255, 255, 255, 0 },
    { 200, 200, 200, 0 },
    { 150, 150, 150, 0 },
    { 60, 60, 60, 0 },
};

DVECTOR D_actor_511000_80149014[16] = {
    { 4096, 0 },
    { 3784, 1567 },
    { 2896, 2896 },
    { 1567, 3784 },
    { 0, 4096 },
    { -1567, 3784 },
    { -2896, 2896 },
    { -3784, 1567 },
    { -4096, 0 },
    { -3784, -1567 },
    { -2896, -2896 },
    { -1567, -3784 },
    { 0, -4096 },
    { 1567, -3784 },
    { 2896, -2896 },
    { 3784, -1567 },
};

s32 D_actor_511000_80149054[3] = {
    1,
    2,
    3,
};

static TmdBone _gActor511000No9GolemAkropolisBodySkeleton[19] = {
#include "assets/no9_golem_akropolis_body_skeleton.inc"
};

static u32 _gActor511000No9GolemAkropolisBodyPartVerts[19] = {
#include "assets/no9_golem_akropolis_body_partVerts.inc"
};

static SVECTOR _gActor511000No9GolemAkropolisBodyVerts[358] = {
#include "assets/no9_golem_akropolis_body_verts.inc"
};

static SVECTOR _gActor511000No9GolemAkropolisBodyNormals[356] = {
#include "assets/no9_golem_akropolis_body_normals.inc"
};

static u32 _gActor511000No9GolemAkropolisBodyStream[3928] = {
#include "assets/no9_golem_akropolis_body_stream.inc"
};

static TmdSource _gActor511000No9GolemAkropolisBody = {
    0,
    21588,
    5944,
    19,
    _gActor511000No9GolemAkropolisBodyPartVerts,
    _gActor511000No9GolemAkropolisBodyVerts,
    _gActor511000No9GolemAkropolisBodyNormals,
    _gActor511000No9GolemAkropolisBodySkeleton,
    _gActor511000No9GolemAkropolisBodyStream,
};

static TmdBone _gActor511000Actor510900Model0FE60Skeleton[1] = {
#include "assets/actor_510900_model_0FE60_skeleton.inc"
};

static u32 _gActor511000Actor510900Model0FE60PartVerts[1] = {
#include "assets/actor_510900_model_0FE60_partVerts.inc"
};

static SVECTOR _gActor511000Actor510900Model0FE60Verts[14] = {
#include "assets/actor_510900_model_0FE60_verts.inc"
};

static SVECTOR _gActor511000Actor510900Model0FE60Normals[12] = {
#include "assets/actor_510900_model_0FE60_normals.inc"
};

static u32 _gActor511000Actor510900Model0FE60Stream[98] = {
#include "assets/actor_510900_model_0FE60_stream.inc"
};

static TmdSource _gActor511000Actor510900Model0FE60 = {
    0,
    652,
    0,
    1,
    _gActor511000Actor510900Model0FE60PartVerts,
    _gActor511000Actor510900Model0FE60Verts,
    _gActor511000Actor510900Model0FE60Normals,
    _gActor511000Actor510900Model0FE60Skeleton,
    _gActor511000Actor510900Model0FE60Stream,
};

static TmdBone _gActor511000No9GolemAkropolisPropSkeleton[1] = {
#include "assets/no9_golem_akropolis_prop_skeleton.inc"
};

static u32 _gActor511000No9GolemAkropolisPropPartVerts[1] = {
#include "assets/no9_golem_akropolis_prop_partVerts.inc"
};

static SVECTOR _gActor511000No9GolemAkropolisPropVerts[14] = {
#include "assets/no9_golem_akropolis_prop_verts.inc"
};

static SVECTOR _gActor511000No9GolemAkropolisPropNormals[19] = {
#include "assets/no9_golem_akropolis_prop_normals.inc"
};

static u32 _gActor511000No9GolemAkropolisPropStream[114] = {
#include "assets/no9_golem_akropolis_prop_stream.inc"
};

static TmdSource _gActor511000No9GolemAkropolisProp = {
    0,
    724,
    0,
    1,
    _gActor511000No9GolemAkropolisPropPartVerts,
    _gActor511000No9GolemAkropolisPropVerts,
    _gActor511000No9GolemAkropolisPropNormals,
    _gActor511000No9GolemAkropolisPropSkeleton,
    _gActor511000No9GolemAkropolisPropStream,
};

static TmdBone _gActor511000Actor510900Model10468Skeleton[1] = {
#include "assets/actor_510900_model_10468_skeleton.inc"
};

static u32 _gActor511000Actor510900Model10468PartVerts[1] = {
#include "assets/actor_510900_model_10468_partVerts.inc"
};

static SVECTOR _gActor511000Actor510900Model10468Verts[18] = {
#include "assets/actor_510900_model_10468_verts.inc"
};

static SVECTOR _gActor511000Actor510900Model10468Normals[17] = {
#include "assets/actor_510900_model_10468_normals.inc"
};

static u32 _gActor511000Actor510900Model10468Stream[126] = {
#include "assets/actor_510900_model_10468_stream.inc"
};

static TmdSource _gActor511000Actor510900Model10468 = {
    0,
    860,
    0,
    1,
    _gActor511000Actor510900Model10468PartVerts,
    _gActor511000Actor510900Model10468Verts,
    _gActor511000Actor510900Model10468Normals,
    _gActor511000Actor510900Model10468Skeleton,
    _gActor511000Actor510900Model10468Stream,
};

static AnimationPackedPose _gActor511000Animation208ECBank1[95] = {
#include "assets/actor_511000_animation_208EC_bank1.inc"
};

static AnimationPackedRotation _gActor511000Animation208ECBank4[1397] = {
#include "assets/actor_511000_animation_208EC_bank4.inc"
};

static AnimationRecord _gActor511000Animation208ECRecords[1813] = {
#include "assets/actor_511000_animation_208EC_records.inc"
};

static u16 _gActor511000Animation208ECIndices[20] = {
#include "assets/actor_511000_animation_208EC_indices.inc"
};

static AnimationSet _gActor511000Animation208EC = {
    _gActor511000Animation208ECRecords,
    _gActor511000Animation208ECIndices,
    { NULL, _gActor511000Animation208ECBank1, NULL, NULL, _gActor511000Animation208ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor511000Animation21EBCBank1[38] = {
#include "assets/actor_511000_animation_21EBC_bank1.inc"
};

static AnimationPackedRotation _gActor511000Animation21EBCBank4[559] = {
#include "assets/actor_511000_animation_21EBC_bank4.inc"
};

static AnimationRecord _gActor511000Animation21EBCRecords[703] = {
#include "assets/actor_511000_animation_21EBC_records.inc"
};

static u16 _gActor511000Animation21EBCIndices[20] = {
#include "assets/actor_511000_animation_21EBC_indices.inc"
};

static AnimationSet _gActor511000Animation21EBC = {
    _gActor511000Animation21EBCRecords,
    _gActor511000Animation21EBCIndices,
    { NULL, _gActor511000Animation21EBCBank1, NULL, NULL, _gActor511000Animation21EBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor511000Animation23228Bank1[109] = {
#include "assets/actor_511000_animation_23228_bank1.inc"
};

static AnimationPackedRotation _gActor511000Animation23228Bank4[368] = {
#include "assets/actor_511000_animation_23228_bank4.inc"
};

static AnimationRecord _gActor511000Animation23228Records[528] = {
#include "assets/actor_511000_animation_23228_records.inc"
};

static u16 _gActor511000Animation23228Indices[20] = {
#include "assets/actor_511000_animation_23228_indices.inc"
};

static AnimationSet _gActor511000Animation23228 = {
    _gActor511000Animation23228Records,
    _gActor511000Animation23228Indices,
    { NULL, _gActor511000Animation23228Bank1, NULL, NULL, _gActor511000Animation23228Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_511000_80155070[4] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_511000_80133D90, { .model = &_gActor511000No9GolemAkropolisBody } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_511000_80133EF4, { .model = &_gActor511000Actor510900Model0FE60 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_511000_80133FC8, { .model = &_gActor511000Actor510900Model10468 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_511000_8013409C, { .model = &_gActor511000No9GolemAkropolisProp } },
};

TaskMessageEntry D_actor_511000_801550A0[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_511000_80133DEC },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_511000_80133EAC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_actor_511000_801550C0[4] = {
    NULL,
    &_gActor511000Animation208EC,
    &_gActor511000Animation21EBC,
    &_gActor511000Animation23228,
};

static void func_actor_511000_80132E6C(_Actor511000HelicopterWork* work);

/// Tick state: while `ticking` is set, steps animation slots 1..19; while
/// clip 1 plays it counts `shotTicks` up and, on the sixteenth tick, plays the
/// sound and spawns the muzzle flash at the revolver's model. Then draws the
/// ground shadow under model part 1, refreshes that part's coordinate and
/// colour when the session asks, runs the blink, and ticks the
/// `freeCountdown` that frees the model's buffers when it reaches zero.
static void func_actor_511000_80131E78(Task* arg0)
{
    _Actor511000RupertBroderickWork* work;
    TmdObject*                       extra;
    GfxCoord*                        coord;
    GfxCoord*                        obj;
    VECTOR3                          groundPoint;
    s32                              i;
    s32                              pan;

    extra = arg0->extra.tmd;
    work  = arg0->work;
    coord = &extra->coords[1];
    if (work->ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        if (work->animId == 1) {
            if (++work->shotTicks == 0x10) {
                obj = work->gunTask->extra.tmd->coords;
                pan = (s8)worldCoordGetOriginAudioPan(obj);
                sndEvtRequestScriptStart(0x313A0003, pan, (s8)worldCoordGetOriginAudioDepth(obj));
                Gp_SpawnEff(EFFECT_ACTOR_MUZZLE_FLASH, obj, 0, &D_actor_511000_8014733C);
            }
        }
    }
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint((VECTOR3*)(arg0->extra.tmd)->coords[1].workm.t, &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, 0x300, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        worldCoordSetModelLighting(extra, coord->workm.t, 0, 3);
    }
    func_actor_511000_80132048(arg0);
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(extra);
        }
        work->freeCountdown--;
    }
}

/// Blink state: runs `blinkCountdown` down one a frame while `blinkStep`
/// names the eye image due next, and on the frame it goes below zero posts
/// that image over the 0x18x0x10 rect at y 0x28 -- restarting the countdown
/// from `blinkFrameDelay` and advancing `blinkStep` after the closed and
/// half-open eyes, or clearing `blinkStep` after the open eyes. The first two
/// steps share their whole tail, which is what makes the compiler emit one
/// copy of it that the first jumps into; the last only differs in clearing
/// the step instead of advancing it.
static void func_actor_511000_80132048(Task* arg0)
{
    _Actor511000RupertBroderickWork* work;
    RECT                             rect;

    work   = arg0->work;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (work->blinkStep) {
        case ACTOR_511000_BLINK_CLOSED:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(arg0, &D_actor_511000_801472B4[0], &rect);
                work->blinkCountdown = work->blinkFrameDelay;
                work->blinkStep      = work->blinkStep + 1;
            }
            break;
        case ACTOR_511000_BLINK_HALF:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(arg0, &D_actor_511000_80146F94[0], &rect);
                work->blinkCountdown = work->blinkFrameDelay;
                work->blinkStep      = work->blinkStep + 1;
            }
            break;
        case ACTOR_511000_BLINK_OPEN:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(arg0, &D_actor_511000_80146C74[0], &rect);
                work->blinkStep = ACTOR_511000_BLINK_NONE;
            }
            break;
    }
}

void func_actor_511000_80132150(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Tick state of the first state table's child: nothing to do, the chained
/// coordinate follows the spawner by itself.
static void func_actor_511000_80132224(Task* task)
{
}

void func_actor_511000_8013222C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E30;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

void func_actor_511000_80132428(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E3C;
    sp.funcs[task->state](task);
}

/// Spawn handler: allocates the work block, seeds its head, excludes the model
/// from active drawing, starts the actor's two child tasks and
/// hands the model's matrices to the light/color rebuilder, then advances to the
/// tick handler. The retained shadow branch cannot run with this bit set.
static void func_actor_511000_80132480(Task* task)
{
    _Actor511000RupertBroderickWork* work;
    TmdObject*                       extra;
    VECTOR3                          pos;
    u16                              flags;

    extra = task->extra.tmd;
    work  = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work          = work;
    work->animId        = ACTOR_MODEL_STATE_NONE;
    work->bank          = ACTOR_MODEL_STATE_NONE;
    work->shotTicks     = 0;
    work->freeCountdown = -1;
    flags               = extra->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    extra->flags        = flags;
    if (!(flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            effectDrawGroundShadow(&pos, 0x200, gRoomEffectState->groundShadowShade);
        }
    }
    work->gunTask  = taskSpawnFromTable(D_actor_511000_801472E8, 1, 8, task);
    work->propTask = taskSpawnFromTable(D_actor_511000_801472E8, 2, 0xC, task);
    func_actor_511000_801325A4(task);
    task->msgTable     = D_actor_511000_8014730C;
    task->exitCallback = enemyTaskExit;
    task->state++;
}

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
static void func_actor_511000_801325A4(Task* task)
{
    _Actor511000RupertBroderickWork* work;
    GfxCoord*                        coords;
    TmdObject*                       extra;

    work                   = task->work;
    extra                  = task->extra.tmd;
    coords                 = extra->coords;
    extra->lightMtx        = &work->light;
    extra->colorMtx        = &work->color;
    coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&coords[1]);
    worldCoordSetModelLighting(extra, coords[1].workm.t, 0, 3);
}

/// Animation message handler: when the source index in the payload changes,
/// reseeds the animation context from that entry of the source table; when the
/// animation id changes, restarts slots 1..19 on it, blended when the
/// payload's third word is set, steps them once and turns on the tick state's
/// per-frame stepping.
s32 func_actor_511000_80132604(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    _Actor511000RupertBroderickWork* work;
    s32                              i;
    TmdObject*                       ext;

    work = task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->bank) {
        work->bank   = msg->source.index;
        work->animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_511000_801472E4[work->bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    if (msg->animationId != work->animId) {
        work->animId = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET) {
            for (i = 1; i < 0x14; i++) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->animId, 0, 6);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                animationResetSlot(&work->rig.anim, i, work->animId);
            }
        }
        for (i = 1; i < 0x14; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        work->ticking = 1;
    }
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Message-0x7D5 handler: the four-way visibility/mode switch on the message's
/// mode word, run against the `TmdObject` parked in `Task::extra`. Mode 0 hides
/// the model and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`, 1 shows it, allocates the
/// buffers and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`, 2 hides it, sets
/// `TMD_OBJECT_SKIP_AUTO_BUFFER` and starts `freeCountdown` at two ticks, and 3
/// shows it while setting `TMD_OBJECT_SKIP_AUTO_BUFFER`. Anything
/// else returns 1 and leaves the object alone; the handled modes return 0.
/// The handler reads `work` before the switch even though mode 2 is its only
/// use, so retail's `lw $v1,0x1C($a0)` sits in the entry block. The same body
/// shape as `func_actor_141000_80133E8C` / `func_actor_503500_80132584`.
s32 func_actor_511000_801327A0(Task* arg0, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*                       obj;
    _Actor511000RupertBroderickWork* work;
    s32                              ret;

    obj  = arg0->extra.tmd;
    work = arg0->work;
    ret  = 0;

    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = mode;
            obj->flags         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message-0x7DB handler: command 0 shows the revolver model `gunTask`
/// carries; command 1 hides it and shows the model `propTask` carries
/// instead. Either task may be missing, in which case its model is skipped.
/// Any other command leaves both alone.
/// The `default:` arm jumps straight to the shared `return 0` instead of
/// falling through the hide block: retail's single epilogue is only reached
/// that way, the hide block and the shared return merging into one block whose
/// first label sits on the value store.
s32 func_actor_511000_8013287C(Task* arg0, s32 arg1, ActorCommand* msg, s32 arg3)
{
    _Actor511000RupertBroderickWork* work;
    Task*                            child;
    u16                              mode;

    mode = msg->command;
    work = arg0->work;

    switch (mode) {
        case 0:
            child = work->gunTask;
            if (child != NULL) {
                child->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case 1:
            child = work->gunTask;
            if (child != NULL) {
                child->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            child = work->propTask;
            if (child != NULL) {
                child->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
    }
    return 0;
}

/// Message-0x7E0 handler: uploads one of the actor's three texture records
/// over the 0x18x0x10 rect at y 0x28 -- `D_actor_511000_801472B4[0]` for mode 1,
/// `D_actor_511000_80146C74[0]` for modes 0 and 2, and `D_actor_511000_80146F94[0]`
/// for mode 3, which first starts a blink: `blinkStep` at the closed eyes and
/// `blinkFrameDelay` at 1. Any other mode leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_511000_80132904(Task* arg0, s32 arg1, s32 mode, s32 arg3)
{
    RECT            rect;
    GpuImageUpload* uploadList;
    s32             ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            uploadList = &D_actor_511000_801472B4[0];
            break;
        case 0:
        case 2:
            uploadList = &D_actor_511000_80146C74[0];
            break;
        case 3:
            ((_Actor511000RupertBroderickWork*)arg0->work)->blinkStep       = ACTOR_511000_BLINK_CLOSED;
            ((_Actor511000RupertBroderickWork*)arg0->work)->blinkFrameDelay = 1;
            uploadList                                                      = &D_actor_511000_80146F94[0];
            break;
        default:
            uploadList = NULL;
            break;
    }

    if (uploadList != NULL) {
        ret = actorRenderUploadTexture(arg0, uploadList, &rect);
    }
    return ret;
}

/// Per-frame tick for a child of the spawner: mirrors the parent model's
/// visibility bit (`field_C` 0x80) onto its own model, and once the session
/// reaches mode 0x18 poses its root coordinate from the parent's
/// `killCountdown` entry in `D_actor_511000_80147AC4`. Within three steps of
/// countdown 0x59 it also picks that distance's colour from
/// `D_actor_511000_80149004`, darkened by 0x1E per channel, and hands both to
/// `func_actor_511000_80132B14`.
/// The table is loaded into its own local before indexing: `&table[d]` on the
/// symbol directly shifts `d` ahead of the `lui`/`addiu` pair.
static void func_actor_511000_801329C4(Task* task)
{
    Task*      parent;
    TmdObject* extra;
    GfxCoord*  coord;
    CVECTOR*   col;
    CVECTOR*   tbl;
    s32        d;
    s8         rgb[3];

    parent = (Task*)task->spawnArg2.pointer;
    extra  = task->extra.tmd;
    coord  = extra->coords;

    if (!(parent->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        extra->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (gGameSession->location.loc.view == 0x18) {
        coord->param.rot.vx = D_actor_511000_80147AC4[parent->killCountdown].vx;
        coord->param.rot.vy = D_actor_511000_80147AC4[parent->killCountdown].vy;
        coord->param.rot.vz = D_actor_511000_80147AC4[parent->killCountdown].vz;
        RotMatrix(&coord->param.rot, &coord->coord);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;

        d = parent->killCountdown - 0x59;
        if (d < 0) {
            d = 0x59 - parent->killCountdown;
        }
        if (d < 4) {
            tbl    = D_actor_511000_80149004;
            col    = &tbl[d];
            rgb[0] = col->r - 0x1E;
            rgb[1] = col->g - 0x1E;
            rgb[2] = col->b - 0x1E;
            func_actor_511000_80132B14(task, col, rgb);
        }
    }
}

/// Draws a semi-transparent gradient disc at the model's root: projects the
/// parent-composed origin, scales the 16 unit offsets in
/// `D_actor_511000_80149014` by 0x12C/0x1000 around it, and fans 16 `POLY_G3`
/// from the centre (`col`) to the rim (`rgb`) into one OT slot, followed by an
/// additive draw-mode `DR_TPAGE`. Both `pts` and the offset table walk by
/// pointer and `scale` is a variable, which is what keeps the `mult` and the
/// retail induction-variable order.
static void func_actor_511000_80132B14(Task* task, CVECTOR* col, s8* rgb)
{
    SVECTOR   pos;
    DVECTOR   pts[16];
    MATRIX    mtx;
    long      sxy;
    long      p;
    long      flag;
    s32       otz;
    POLY_G3*  prim;
    DR_TPAGE* dr;
    u16       x;
    u16       y;
    s32       scale;
    u32*      ot;
    s32       i;
    DVECTOR*  pt;
    DVECTOR*  src;

    gfxComposeNodeWorldTransform(task->extra.tmd->coords, &mtx, &pos);
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    otz   = RotTransPers(&pos, &sxy, &p, &flag);
    x     = sxy;
    y     = sxy >> 16;
    src   = D_actor_511000_80149014;
    pt    = pts;
    scale = 0x12C;
    for (i = 0; i < 16; i++) {
        pt->vx = x + scale * src->vx / 0x1000;
        pt->vy = y + scale * src->vy / 0x1000;
        pt++;
        src++;
    }
    ot = (u32*)((u8*)gGpuCurrentOt + (((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) - 30;
    pt = pts;
    for (i = 0; i < 15; i++, pt++) {
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG3(prim);
        setSemiTrans(prim, 1);
        prim->x0 = x;
        prim->y0 = y;
        prim->x1 = pt->vx;
        prim->y1 = pt->vy;
        prim->x2 = pt[1].vx;
        prim->y2 = pt[1].vy;
        prim->r0 = col->r;
        prim->g0 = col->g;
        prim->b0 = col->b;
        prim->r1 = rgb[0];
        prim->g1 = rgb[1];
        prim->b1 = rgb[2];
        prim->r2 = rgb[0];
        prim->g2 = rgb[1];
        prim->b2 = rgb[2];
        addPrim(ot, prim);
    }
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG3(prim);
    setSemiTrans(prim, 1);
    prim->x0 = x;
    prim->y0 = y;
    prim->x1 = pt->vx;
    prim->y1 = pt->vy;
    prim->x2 = pts[0].vx;
    prim->y2 = pts[0].vy;
    prim->r0 = col->r;
    prim->g0 = col->g;
    prim->b0 = col->b;
    prim->r1 = rgb[0];
    prim->g1 = rgb[1];
    prim->b1 = rgb[2];
    prim->r2 = rgb[0];
    prim->g2 = rgb[1];
    prim->b2 = rgb[2];
    addPrim(ot, prim);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 1, 0, 0x2A);
    addPrim(ot, dr);
}

/// Blends one little-endian 15-bit colour: `src0` weighted by `inv` and `src1`
/// by `fade` (4.12 fixed point), written to `dst` low byte first.
static inline void _actor511000BlendPaletteColor(u8* dst, u8* src0, u8* src1, s32 inv, s32 fade)
{
    CVECTOR col[3];
    s32     c;

    c        = src0[0] | (src0[1] << 8);
    col[0].r = ((u16)c >> 10) & 0x1F;
    col[0].g = ((u16)c >> 5) & 0x1F;
    col[0].b = c & 0x1F;
    c        = src1[0] | (src1[1] << 8);
    col[1].r = ((u16)c >> 10) & 0x1F;
    col[1].g = ((u16)c >> 5) & 0x1F;
    col[1].b = c & 0x1F;
    LoadAverageCol(&col[0], &col[1], inv, fade, &col[2]);
    c      = col[2].b + ((col[2].r << 10) + (col[2].g << 5));
    dst[1] = (u32)c >> 8;
    dst[0] = c;
}

/// Palette fade: steps `paletteFade` up by 0x555 per frame while the
/// `paletteHold` counter is live (counting it down once the fade is full),
/// otherwise snaps it back to 0 and re-arms the hold. Each of the 16
/// little-endian 15-bit colours is then blended between
/// `D_actor_511000_80147E84` and `D_actor_511000_80147EC4` by that weight into
/// `palette`, which `D_actor_511000_80147EA4[0]` uploads.
static void func_actor_511000_80132E6C(_Actor511000HelicopterWork* work)
{
    s32 i;
    s32 inv;
    s32 fade;

    if (work->paletteHold != 0) {
        work->paletteFade += 0x555;
        i                  = 0;
        if (work->paletteFade >= ACTOR_511000_PALETTE_FADE_FULL) {
            work->paletteFade = ACTOR_511000_PALETTE_FADE_FULL;
            if (--work->paletteHold < 0) {
                work->paletteHold = 0;
            }
        }
    } else {
        work->paletteFade -= ACTOR_511000_PALETTE_FADE_FULL;
        i                  = 0;
        if (work->paletteFade <= 0) {
            work->paletteFade = 0;
            work->paletteHold = ACTOR_511000_PALETTE_HOLD_TICKS;
        }
    }
    inv  = ACTOR_511000_PALETTE_FADE_FULL - work->paletteFade;
    fade = work->paletteFade;
    do {
        _actor511000BlendPaletteColor(&work->palette[i], &D_actor_511000_80147E84[i], &D_actor_511000_80147EC4[i], inv, fade);
        i += 2;
    } while (i < ACTOR_511000_PALETTE_BYTES);
    gpuUploadImages(&D_actor_511000_80147EA4[0]);
}

/// Spawn/setup state: allocates the work block, parks it in `work`, leaves
/// `freeCountdown` with no free pending, hides the model, places it at
/// rot/trans index 0, binds light/color, installs the message table, and
/// publishes `work->palette` through `D_actor_511000_80147EA4[0].pixels`
/// before advancing to the per-frame state.
static void func_actor_511000_80133034(Task* task)
{
    _Actor511000HelicopterWork* work;
    TmdObject*                  extra;

    extra = task->extra.tmd;
    work  = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work          = work;
    work->freeCountdown = -1;
    extra->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    func_actor_511000_801336E0(task, D_actor_511000_80147344, D_actor_511000_80147704, 0);
    func_actor_511000_801337F0(task);
    do {
        task->msgTable                    = D_actor_511000_80148FC4;
        D_actor_511000_80147EA4[0].pixels = (u_long*)work->palette;
    } while (0);
    task->state += 1;
}

/// Per-frame state: while the model is hidden (`field_C` bit 0x80 clear) it
/// refreshes the root coordinate, rebuilds the colour matrix from that
/// coordinate's own translation, and runs the work block's follow-up. Once the
/// session reaches mode 0x18 it walks `killCountdown` up to 0x77, spawning a
/// view task for the camera record at each index and re-posing the model from
/// the matching rotations, and finally runs the `tmdFreePrimitiveBuffer` countdown the
/// spawn state armed at -1, freeing the buffers and latching the field back to
/// -1 on the frame the countdown reaches zero.
static void func_actor_511000_801330F0(Task* task)
{
    _Actor511000HelicopterWork* work;
    TmdObject*                  obj;
    GfxCoord*                   coord;
    s32                         countdown;
    s16                         frame;

    obj   = task->extra.tmd;
    work  = task->work;
    coord = obj->coords;

    if (!(obj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        actorRenderComposeCoord(coord);
        worldCoordSetModelLighting(obj, coord->workm.t, 0, 3);
        func_actor_511000_80132E6C(task->work);
    }
    if (gGameSession->location.loc.view == 0x18) {
        frame               = task->killCountdown + 1;
        task->killCountdown = frame;
        if (frame >= 0x78) {
            task->killCountdown = 0x77;
        }
        Gp_TrySpawnViewTask(&D_actor_511000_80147EE4[task->killCountdown]);
        func_actor_511000_801336E0(task, D_actor_511000_80147344, D_actor_511000_80147704, task->killCountdown);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    countdown = work->freeCountdown;
    if (countdown >= 0) {
        if (countdown == 0) {
            tmdFreePrimitiveBuffer(obj);
            countdown = work->freeCountdown;
        }
        work->freeCountdown = countdown - 1;
    }
}

static void func_actor_511000_80133220(Task* task)
{
    taskKill(task);
}

/// Inherits the parent model's light/color and visibility bit, chains this
/// actor's root coordinate under the parent's, places it at the spawnArg1
/// translation, and reparents the task.
static void func_actor_511000_80133240(Task* task)
{
    Task*      parent;
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;
    GfxCoord*  dest;

    parent          = (Task*)task->spawnArg2.pointer;
    parentExtra     = parent->extra.tmd;
    extra           = task->extra.tmd;
    dest            = parentExtra->coords;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    extra->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord           = extra->coords;
    if (!(parentExtra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        extra->flags = 0;
    }
    func_actor_511000_80133760(task);
    coord->parent = dest;
    taskReparent(parent, task);
    task->state += 1;
}

/// Tracks the parent model's visibility bit every frame: while the parent model
/// is hidden (`field_C` bit 0x80 clear) this clears its own bit and, for
/// spawnArg1 1 or 2, spins the root coordinate's yaw (0x46) by 0x294 or its
/// pitch (0x44) by 0x3E8, wrapping each to 0x1000. The rotation matrix is then
/// rebuilt from the angles and the coordinate's `composeStamp` cleared. With the parent
/// visible the rotation is left alone and the visibility bit is set instead.
static void func_actor_511000_801332E4(Task* task)
{
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;

    extra       = task->extra.tmd;
    coord       = extra->coords;
    parentExtra = ((Task*)task->spawnArg2.pointer)->extra.tmd;

    if (!(parentExtra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        extra->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;

        switch (task->spawnArg1.value) {
            case 1:
                coord->param.rot.vy = ((u16)coord->param.rot.vy + 0x294) & 0xFFF;
                break;
            case 2:
                coord->param.rot.vx = ((u16)coord->param.rot.vx + 0x3E8) & 0xFFF;
                break;
        }

        RotMatrix(&coord->param.rot, &coord->coord);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        return;
    }
    extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

static void func_actor_511000_801333A4(Task* task)
{
    taskKill(task);
}

/// Inherits the parent model's light/color and visibility bit, chains this
/// actor's root coordinate under the parent's, places it at the spawnArg1
/// translation, copies `D_actor_511000_80147AC4` onto the Euler angles,
/// rebuilds the rotation matrix, and reparents the task.
static void func_actor_511000_801333C4(Task* task)
{
    Task*      parent;
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;
    GfxCoord*  dest;

    parent          = (Task*)task->spawnArg2.pointer;
    parentExtra     = parent->extra.tmd;
    extra           = task->extra.tmd;
    dest            = parentExtra->coords;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    extra->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord           = extra->coords;
    if (!(parentExtra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        extra->flags = 0;
    }
    func_actor_511000_80133760(task);
    (coord)->parent = dest;
    taskReparent(parent, task);
    coord->param.rot.vx = D_actor_511000_80147AC4[0].vx;
    coord->param.rot.vy = D_actor_511000_80147AC4[0].vy;
    coord->param.rot.vz = D_actor_511000_80147AC4[0].vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state        += 1;
}

static void func_actor_511000_80133498(Task* task)
{
    taskKill(task);
}

s32 func_actor_511000_801334B8(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    arg0->killCountdown = 0;
    return 0;
}

/// Placement message handler that also shows the model: writes the payload's
/// translation and Euler angles into the root coordinate, rebuilds its
/// rotation, clears `composeStamp` so the world matrix is recomputed and clears the
/// model's hidden bit 0x80.
s32 func_actor_511000_801334C4(Task* task, s32 arg1, ActorTransform* args, s32 arg3)
{
    GfxCoord*  coord;
    TmdObject* extra;

    extra               = task->extra.tmd;
    coord               = extra->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return 0;
}

s32 func_actor_511000_80133554(Task* task, s32 arg1, s32 msg, s32 arg3)
{
    TmdObject*                  obj;
    _Actor511000HelicopterWork* work;
    Task*                       child;
    s32                         ret;
    s32                         i;

    obj  = task->extra.tmd;
    work = task->work;
    ret  = 0;
    switch (msg) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = msg;
            obj->flags         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    if (msg == 1 && work->partsSpawned == 0) {
        for (i = 1; i < 4; i++) {
            child = taskSpawnFromTable(D_actor_511000_80139924, D_actor_511000_80149054[i - 1], i, task);
            if (child != NULL) {
                child->extra.tmd->flags &= ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            }
        }
        work->partsSpawned = 1;
    }
    return ret;
}

/// Places the task's model at the indexed rotation and translation: copies
/// `rots[index]` onto the root coordinate's Euler angles, `trans[index]` into
/// its local translation, rebuilds the rotation matrix and marks the
/// coordinate dirty.
static void func_actor_511000_801336E0(Task* task, SVECTOR* rots, SVECTOR* trans, s32 index)
{
    GfxCoord* coord;
    s16       idx;

    idx                 = index;
    coord               = task->extra.tmd->coords;
    coord->param.rot.vx = rots[idx].vx;
    coord->param.rot.vy = rots[idx].vy;
    coord->param.rot.vz = rots[idx].vz;
    coord->coord.t[0]   = trans[idx].vx;
    coord->coord.t[1]   = trans[idx].vy;
    coord->coord.t[2]   = trans[idx].vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Places the task's model at the indexed translation: copies
/// `D_actor_511000_80148FE4[spawnArg1]` into the root coordinate's local
/// translation, zeros the Euler angles, rebuilds the rotation matrix and
/// marks the coordinate dirty.
static void func_actor_511000_80133760(Task* task)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = D_actor_511000_80148FE4[task->spawnArg1.value].vx;
    coord->coord.t[1]   = D_actor_511000_80148FE4[task->spawnArg1.value].vy;
    coord->coord.t[2]   = D_actor_511000_80148FE4[task->spawnArg1.value].vz;
    coord->param.rot.vx = 0;
    coord->param.rot.vy = 0;
    coord->param.rot.vz = 0;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Binds the task's TMD object to the work-block light/color matrices, clears
/// the root coordinate flag, and rebuilds lighting from the world translation.
static void func_actor_511000_801337F0(Task* task)
{
    GfxCoord*                   coord;
    _Actor511000HelicopterWork* work;
    TmdObject*                  extra;

    work                = task->work;
    extra               = task->extra.tmd;
    coord               = extra->coords;
    extra->lightMtx     = &work->light;
    extra->colorMtx     = &work->color;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    worldCoordSetModelLighting(extra, coord->workm.t, 0, 3);
}

void func_actor_511000_80133850(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E48;
    sp.funcs[task->state](task);
}

void func_actor_511000_801338A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E54;
    sp.funcs[task->state](task);
}

void func_actor_511000_80133900(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E60;
    sp.funcs[task->state](task);
}

/// Spawn handler: allocates the 0x488-byte work block and parks it in
/// `Task::work`, binds the task's model to the block's matrices and
/// animation state, installs the message table, then spawns table entries 1
/// and 2 - tinting each child's model from the current area's record - and
/// entry 3, and advances to state 1. An allocation failure destroys the enemy.
static void func_actor_511000_80133958(Enemy* enemy, Task* task)
{
    enum { MODEL_HIDDEN = 0x80,
           STATE_UPDATE = 1 };
    GameLocationKey           key;
    GameLocationKey*          sessionKey;
    u8                        view;
    u8                        stage;
    AreaVariant*              layout;
    TmdObject*                model;
    GfxCoord*                 coord;
    _Actor511000No9GolemWork* work;
    TaskDesc*                 table;
    u32                       placeIndex;
    u32                       placeIndex2;
    AreaPlacement*            placements;
    Enemy*                    spawned;
    GameSession*              session;

    model = task->extra.tmd;
    coord = model->coords;
    work  = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work      = work;
    model->flags    = MODEL_HIDDEN;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    animationInitContext(&work->rig.anim, D_actor_511000_801550C0, model, work->rig.poses, work->rig.slots);
    work->animId        = 0;
    task->msgTable      = D_actor_511000_801550A0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    table   = D_actor_511000_80155070;
    spawned = Gp_SpawnEnemyFromTable(table, 1, 0, enemy);
    session = gGameSession;
    // Child models inherit the texture relocation of the parent's placement.
    // Reusing the spawn result preserves its register preference at the task load.
    // Each child has its own index local. With one local shared by both
    // children the scaled index and the element address leave its register.
    spawned    = (Enemy*)spawned->task;
    sessionKey = &session->location.loc;
    placeIndex = enemy->placeKey;
    stage      = sessionKey->stage;
    model      = ((Task*)spawned)->extra.tmd;
    key.stage  = stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    view       = session->location.loc.view;
    placeIndex = placeIndex >> ENEMY_PLACE_INDEX_SHIFT;
    key.view   = view;
    areaSyncLocationVariant(&key);
    layout                   = Gp_GetNestedAreaRec(&key);
    placements               = layout->placements;
    model->texturePageOffset = placements[placeIndex].texturePageOffset;
    model->clutRowOffset     = placements[placeIndex].clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    spawned     = Gp_SpawnEnemyFromTable(table, 2, 0, enemy);
    session     = gGameSession;
    spawned     = (Enemy*)spawned->task;
    sessionKey  = &session->location.loc;
    placeIndex2 = enemy->placeKey;
    stage       = sessionKey->stage;
    model       = ((Task*)spawned)->extra.tmd;
    key.stage   = stage;
    key.area    = sessionKey->area;
    key.room    = sessionKey->room;
    view        = session->location.loc.view;
    placeIndex2 = placeIndex2 >> ENEMY_PLACE_INDEX_SHIFT;
    key.view    = view;
    areaSyncLocationVariant(&key);
    layout                   = Gp_GetNestedAreaRec(&key);
    placements               = layout->placements;
    model->texturePageOffset = placements[placeIndex2].texturePageOffset;
    model->clutRowOffset     = placements[placeIndex2].clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    Gp_SpawnEnemyFromTable(table, 3, 0, enemy);
    task->state = STATE_UPDATE;
}

static void func_actor_511000_80133B80(Enemy* enemy, Task* task)
{
    TmdObject*                extra;
    VECTOR*                   pos;
    VECTOR*                   out;
    _Actor511000No9GolemWork* work;
    GfxCoord*                 coords;
    GfxCoord*                 coord;
    s32                       i;
    s32                       j;
    s32                       flag;

    extra = task->extra.tmd;
    SCRATCH_STACK_RESERVE_BYTES(0x20);
    work   = task->work;
    coords = extra->coords;
    coord  = &coords[1];
    flag   = work->animId;
    pos    = SCRATCH_STACK_CURSOR(VECTOR);
    if (flag != 0) {
        for (i = 1; i < 19; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (work->animId < 3) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        pos->vx = coord->workm.t[0];
        pos->vy = coord->workm.t[1];
        pos->vz = coord->workm.t[2];
        worldCoordUpdateActorColor(enemy, pos, 0, 0);
        pos->vx = coord->workm.t[0];
        pos->vy = coord->workm.t[1];
        pos->vz = coord->workm.t[2];
        out     = pos + 1;
        if (worldCollisionProjectGroundPoint((VECTOR3*)pos, (VECTOR3*)out) != 0) {
            effectDrawGroundShadow((VECTOR3*)out, 0x400, gRoomEffectState->groundShadowShade);
        }
    } else {
        if ((s16)(work->animTicks % 3) == 0) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    work->color.m[i][j] = (work->color.m[i][j] * 15) >> 4;
                    work->light.m[i][j] = (work->light.m[i][j] * 15) >> 4;
                }
            }
        }
        if (work->animTicks > 96) {
            task->state = 2;
        }
    }
    work->animTicks++;
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Runs the enemy's current state handler, copying the table onto the stack
/// before the call.
void func_actor_511000_80133D90(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_511000_80131E6C;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Copies the animation id from `preset` into the work block parked in
/// `task->work`, reseeds slots 1..0x12 through `animationResetSlot`, and
/// restarts `animTicks`.
s32 func_actor_511000_80133DEC(Task* task, s32 arg1, AnimationPlayRequest* preset, s32 arg3)
{
    _Actor511000No9GolemWork* work;
    s32                       i;

    work         = task->work;
    work->animId = preset->animationId;
    i            = 1;
    do {
        animationResetSlot(&work->rig.anim, i, work->animId);
        i++;
    } while (i < 0x13);
    work->animTicks = 0;
    return 0;
}

#include "../../shared/actor_messages_place_rot_matrix.inc.c"

/// Visibility message handler: bit 0 of `arg2` shows the model (flags 0)
/// instead of hiding it (0x80); bit 1 also sets `TMD_OBJECT_SKIP_AUTO_BUFFER`.
s32 func_actor_511000_80133EAC(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (!(arg2 & 1)) {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj         = task->extra.tmd;
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

void func_actor_511000_80133EF4(Task* task)
{
    EnemyTaskFunc fns[2] = { func_actor_511000_80133F48, func_actor_511000_80133F88 };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Spawn state of the model child attached to the spawner's part 8: chains
/// the root coordinate under that part, takes the spawner work block's light
/// and colour matrices, shows the model and advances to the tick state.
static void func_actor_511000_80133F48(Enemy* enemy, Task* task)
{
    Task*                     parent;
    TmdObject*                obj;
    _Actor511000No9GolemWork* work;
    GfxCoord*                 coord;
    GfxCoord*                 parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = parent->work;

    coord->parent = &parentCoords[8];
    obj->lightMtx = &work->light;
    obj->flags    = 0;
    obj->colorMtx = &work->color;
    task->state   = 1;
}

static void func_actor_511000_80133F88(Enemy* arg0, Task* arg1)
{
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1->extra.tmd->coords);
}

void func_actor_511000_80133FC8(Task* task)
{
    EnemyTaskFunc fns[2] = { func_actor_511000_8013401C, func_actor_511000_8013405C };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_511000_8013401C(Enemy* enemy, Task* task)
{
    Task*                     parent;
    TmdObject*                obj;
    _Actor511000No9GolemWork* work;
    GfxCoord*                 coord;
    GfxCoord*                 parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = parent->work;

    coord->parent = &parentCoords[3];
    obj->lightMtx = &work->light;
    obj->flags    = 0;
    obj->colorMtx = &work->color;
    task->state   = 1;
}

static void func_actor_511000_8013405C(Enemy* arg0, Task* arg1)
{
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1->extra.tmd->coords);
}

void func_actor_511000_8013409C(Task* task)
{
    EnemyTaskFunc fns[2] = { func_actor_511000_801340F0, func_actor_511000_80134130 };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_511000_801340F0(Enemy* enemy, Task* task)
{
    Task*                     parent;
    TmdObject*                obj;
    _Actor511000No9GolemWork* work;
    GfxCoord*                 coord;
    GfxCoord*                 parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = parent->work;

    coord->parent = &parentCoords[12];
    obj->lightMtx = &work->light;
    obj->flags    = 0;
    obj->colorMtx = &work->color;
    task->state   = 1;
}

static void func_actor_511000_80134130(Enemy* arg0, Task* arg1)
{
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1->extra.tmd->coords);
}
