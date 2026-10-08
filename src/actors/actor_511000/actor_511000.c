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
static void _actor511000TickRupertBlink(Task* task);
static void _actor511000IdleRupertRevolver(Task* task);
static void func_actor_511000_80132480(Task* task);
static void func_actor_511000_801325A4(Task* task);
static void _actor511000TickHelicopterSearchlight(Task* task);
static void _actor511000DrawHelicopterSearchlightGlow(Task* task, const CVECTOR* centerColor, const u8* rimRgb);
static void func_actor_511000_80133034(Task* task);
static void func_actor_511000_801330F0(Task* task);
static void _actor511000KillHelicopter(Task* task);
static void _actor511000AttachHelicopterRotor(Task* task);
static void _actor511000TickHelicopterRotor(Task* task);
static void _actor511000KillHelicopterRotor(Task* task);
static void _actor511000AttachHelicopterSearchlight(Task* task);
static void _actor511000KillHelicopterSearchlight(Task* task);
static void _actor511000PoseHelicopterSequenceFrame(Task* task, const SVECTOR* rotations, const SVECTOR* translations, s32 frameIndex);
static void _actor511000PlaceHelicopterPart(Task* task);
static void func_actor_511000_801337F0(Task* task);
static void _actor511000SpawnNo9Golem(Enemy* enemy, Task* task);
static void _actor511000TickNo9Golem(Enemy* enemy, Task* task);
static void _actor511000AttachNo9GolemPart8Model(Enemy* enemy, Task* task);
static void _actor511000TickNo9GolemPart8Model(Enemy* enemy, Task* task);
static void _actor511000AttachNo9GolemPart3Model(Enemy* enemy, Task* task);
static void _actor511000TickNo9GolemPart3Model(Enemy* enemy, Task* task);
static void _actor511000AttachNo9GolemPart12Model(Enemy* enemy, Task* task);
static void _actor511000TickNo9GolemPart12Model(Enemy* enemy, Task* task);

/// State table of a child chained under a part of its spawner's model: the
/// attach state, an empty tick and the kill.
static const TaskFuncTable3 D_actor_511000_80131E24 = {
    _modelPlacementAttachPartTask,
    _actor511000IdleRupertRevolver,
    taskKill,
};

/// State table of a child chained under a part of its spawner's model that
/// also follows the spawner's active-draw and buffer flags: the attach state,
/// the flag-mirroring tick and the kill.
static const TaskFuncTable3 D_actor_511000_80131E30 = {
    _modelPlacementAttachChild,
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
    _actor511000KillHelicopter,
};

/// State table of a child placed at a translation preset under its spawner:
/// the attach state, the spinning tick and the kill.
static const TaskFuncTable3 D_actor_511000_80131E54 = {
    _actor511000AttachHelicopterRotor,
    _actor511000TickHelicopterRotor,
    _actor511000KillHelicopterRotor,
};

/// State table of a child posed from the kill-countdown rotations: the attach
/// state, the tick that follows the countdown and the kill.
static const TaskFuncTable3 D_actor_511000_80131E60 = {
    _actor511000AttachHelicopterSearchlight,
    _actor511000TickHelicopterSearchlight,
    _actor511000KillHelicopterSearchlight,
};

/// The enemy's three state handlers - spawn, per-frame tick and teardown.
static const EnemyTaskFuncTable3 D_actor_511000_80131E6C = {
    _actor511000SpawnNo9Golem,
    _actor511000TickNo9Golem,
    enemyDestroy,
};

/// Camera path `func_actor_511000_801330F0` walks once the session reaches
/// mode 0x18, one 0x24-byte `ViewCamera` per step of the kill countdown: the
/// rotation and projection plane repeat down the table while the translation
/// descends, so the spawn of a view task per index pans the camera as the
/// actor goes down. Handed straight to `viewQueueCamera`, exactly as
/// `viewQueueCurrentCameraAndPackets` queues the current area's mapped camera.
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

/// Offset `effectSpawn` places the tick state's effect at.
extern SVECTOR D_actor_511000_8014733C;

extern SVECTOR D_actor_511000_80147344[];
extern SVECTOR D_actor_511000_80147704[];
extern SVECTOR D_actor_511000_80147AC4[];
extern u8      D_actor_511000_80147E84[ACTOR_511000_PALETTE_BYTES];
extern u8      D_actor_511000_80147EC4[ACTOR_511000_PALETTE_BYTES];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_511000_80148FC4[];

/// Translation presets `_actor511000PlaceHelicopterPart` copies onto the root
/// coordinate; `Task::spawnArg1` selects the entry.
extern SVECTOR D_actor_511000_80148FE4[];

extern CVECTOR D_actor_511000_80149004[];
extern DVECTOR D_actor_511000_80149014[];

/// Spawn table and per-child args for the children spawned on message 1.
extern TaskDesc D_actor_511000_80139924[];
extern s32      D_actor_511000_80149054[];

/// Spawn table for the three children `_actor511000SpawnNo9Golem` creates.
extern TaskDesc D_actor_511000_80155070[];
/// Message table and animation data `_actor511000SpawnNo9Golem` installs.
extern TaskMessageEntry D_actor_511000_801550A0[4];
extern AnimationSet*    D_actor_511000_801550C0[4];

static TmdSource _gActor511000RupertBroderickBody2;
static TmdSource _gActor511000RupertBroderickMongoose;
static TmdSource _gActor511000Prop1;
static s32       _actor511000PlayRupertAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32       _actor511000SetRupertModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32       _actor511000ApplyRupertCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static s32       _actor511000SetRupertEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedArg);
static void      _actor511000RupertRevolverTask(Task* task);
static void      _actor511000RupertPropTask(Task* task);
static void      _actor511000RupertBodyTask(Task* task);

static AnimationSet _gActor511000Animation1121C;
static AnimationSet _gActor511000Animation130D4;
static AnimationSet _gActor511000Animation14B2C;

static s32  _actor511000RestartHelicopterSequence(Task* task, s32 messageId, s32 unusedRequest, s32 unusedArg);
static s32  _actor511000PlaceHelicopter(Task* task, s32 messageId, const ActorTransform* transform, s32 unusedArg);
s32         func_actor_511000_80133554(Task*, s32, s32, s32);
static s32  _actor511000PlayNo9GolemAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _actor511000SetNo9GolemModelDraw(Task* task, s32 messageId, s32 drawFlags, s32 unusedArg);
static void _actor511000No9GolemTask(Task* task);
static void _actor511000No9GolemPart8Task(Task* task);
static void _actor511000No9GolemPart3Task(Task* task);
static void _actor511000No9GolemPart12Task(Task* task);

static TmdSource _gActor511000HelicopterBase;
static TmdSource _gActor511000Prop2;
static TmdSource _gActor511000Prop3;
static TmdSource _gActor511000Model0A41C;
static void      _actor511000HelicopterTask(Task* task);
static void      _actor511000HelicopterRotorTask(Task* task);
static void      _actor511000HelicopterSearchlightTask(Task* task);

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
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor511000HelicopterTask, { .model = &_gActor511000HelicopterBase } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor511000HelicopterRotorTask, { .model = &_gActor511000Prop2 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor511000HelicopterRotorTask, { .model = &_gActor511000Prop3 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor511000HelicopterSearchlightTask, { .model = &_gActor511000Model0A41C } },
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
#include "assets/actor_210700_image_0DE2C.inc"
};

GpuImageUpload D_actor_511000_80146C74[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 24, 16 }, D_actor_511000_80146974 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_511000_80146C94[192] = {
#include "assets/actor_210700_image_0E14C.inc"
};

GpuImageUpload D_actor_511000_80146F94[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 24, 16 }, D_actor_511000_80146C94 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_511000_80146FB4[192] = {
#include "assets/actor_210700_image_0E46C.inc"
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
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor511000RupertBodyTask, { .model = &_gActor511000RupertBroderickBody2 } },
    { { { TASK_BODY_TMD, 192 } }, _actor511000RupertRevolverTask, { .model = &_gActor511000RupertBroderickMongoose } },
    { { { TASK_BODY_TMD, 192 } }, _actor511000RupertPropTask, { .model = &_gActor511000Prop1 } },
};

TaskMessageEntry D_actor_511000_8014730C[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor511000PlayRupertAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor511000SetRupertModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor511000ApplyRupertCommand },
    { 2016, _actor511000SetRupertEyes },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_actor_511000_8014733C = { -30, 440, 140, 0 };

SVECTOR D_actor_511000_80147344[120] = {
#include "assets/actor_511000_motion_15524.inc"
};

SVECTOR D_actor_511000_80147704[120] = {
#include "assets/actor_511000_motion_158E4.inc"
};

SVECTOR D_actor_511000_80147AC4[120] = {
#include "assets/actor_511000_motion_15CA4.inc"
};

u8 D_actor_511000_80147E84[ACTOR_511000_PALETTE_BYTES] = {
#include "assets/actor_511000_clut_16064.inc"
};

GpuImageUpload D_actor_511000_80147EA4[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 264, 16, 1 }, (u_long*)D_actor_511000_80147E84 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u8 D_actor_511000_80147EC4[ACTOR_511000_PALETTE_BYTES] = {
#include "assets/actor_511000_clut_160A4.inc"
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
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor511000RestartHelicopterSequence },
    { ACTOR_MESSAGE_PLACE, _actor511000PlaceHelicopter },
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
    { { { TASK_BODY_TMD, 96 } }, _actor511000No9GolemTask, { .model = &_gActor511000No9GolemAkropolisBody } },
    { { { TASK_BODY_TMD, 96 } }, _actor511000No9GolemPart8Task, { .model = &_gActor511000Actor510900Model0FE60 } },
    { { { TASK_BODY_TMD, 96 } }, _actor511000No9GolemPart3Task, { .model = &_gActor511000Actor510900Model10468 } },
    { { { TASK_BODY_TMD, 96 } }, _actor511000No9GolemPart12Task, { .model = &_gActor511000No9GolemAkropolisProp } },
};

TaskMessageEntry D_actor_511000_801550A0[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor511000PlayNo9GolemAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor511000SetNo9GolemModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_actor_511000_801550C0[4] = {
    NULL,
    &_gActor511000Animation208EC,
    &_gActor511000Animation21EBC,
    &_gActor511000Animation23228,
};

static void _actor511000TickHelicopterPaletteFlash(_Actor511000HelicopterWork* work);

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
                effectSpawn(EFFECT_ACTOR_MUZZLE_FLASH, obj, 0, &D_actor_511000_8014733C);
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
    _actor511000TickRupertBlink(arg0);
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(extra);
        }
        work->freeCountdown--;
    }
}

/// Posts the next timed blink image and restarts its display interval.
///
/// Requires a live Rupert TMD task and its initialized work in CLOSED or HALF.
/// Resets the signed countdown from blinkFrameDelay and advances to the next
/// step; a nonnegative delay lasts that many later blink ticks plus one.
/// The writable, terminated upload list and rectangle are borrowed through the call; image
/// pixels remain live until GPU transfer completes. Rectangle units follow
/// `actorRenderUploadTexture`. The caller expires the countdown; this does not
/// test it. The upload rewrites the first list entry's destination.
/// Upload results are ignored; this does not wait for the GPU.
static inline void _actor511000AdvanceRupertBlinkImage(Task* task, _Actor511000RupertBroderickWork* work,
                                                       GpuImageUpload* uploadList, const RECT* eyeRect)
{
    actorRenderUploadTexture(task, uploadList, eyeRect);
    work->blinkCountdown = work->blinkFrameDelay;
    work->blinkStep      = work->blinkStep + 1;
}

/// Advances Rupert's closed, half-open and open eye sequence by one tick.
///
/// Requires a live TMD body and initialized work. Active steps decrement the
/// signed-halfword countdown and post their image when it becomes negative.
/// Closed and half-open restart the countdown from `blinkFrameDelay`; open
/// ends the blink without resetting it. Other steps do nothing. The eye patch
/// spans 24 VRAM words by 16 rows at local Y 40. Pixel storage remains borrowed
/// until GPU transfer completes; uploads do not wait and their results are ignored.
static void _actor511000TickRupertBlink(Task* task)
{
    enum { ACTOR_511000_BLINK_EYES_Y_ROWS      = 40,
           ACTOR_511000_BLINK_EYES_WIDTH_WORDS = 24,
           ACTOR_511000_BLINK_EYES_HEIGHT_ROWS = 16 };
    _Actor511000RupertBroderickWork* work;
    RECT                             eyeRect;

    work      = task->work;
    eyeRect.x = 0;
    eyeRect.y = ACTOR_511000_BLINK_EYES_Y_ROWS;
    eyeRect.w = ACTOR_511000_BLINK_EYES_WIDTH_WORDS;
    eyeRect.h = ACTOR_511000_BLINK_EYES_HEIGHT_ROWS;

    switch (work->blinkStep) {
        case ACTOR_511000_BLINK_CLOSED:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                _actor511000AdvanceRupertBlinkImage(task, work, &D_actor_511000_801472B4[0], &eyeRect);
            }
            break;
        case ACTOR_511000_BLINK_HALF:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                _actor511000AdvanceRupertBlinkImage(task, work, &D_actor_511000_80146F94[0], &eyeRect);
            }
            break;
        case ACTOR_511000_BLINK_OPEN:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(task, &D_actor_511000_80146C74[0], &eyeRect);
                work->blinkStep = ACTOR_511000_BLINK_NONE;
            }
            break;
    }
}

/// Runs Rupert Broderick's revolver attachment states.
///
/// `task->state` is 0 attach, 1 idle or 2 kill. At attachment,
/// `spawnArg2.pointer` borrows the live parent TMD task and `spawnArg1.value`
/// selects its model coordinate (part 8 in the body descriptor's spawn).
/// The child's local transform and draw policy are retained. Attachment joins
/// the parent's teardown tree; its coordinate and lighting must outlive the child.
static void _actor511000RupertRevolverTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_511000_80131E24;
    states.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Leaves the revolver attachment unchanged between attachment and teardown.
///
/// Its chained coordinate follows the parent model part without a task update.
static void _actor511000IdleRupertRevolver(Task* task)
{
}

/// Runs the states of Rupert Broderick's second held model.
///
/// `task->state` is 0 attach, 1 mirror parent draw policy or 2 kill.
/// `spawnArg2.pointer` borrows the live parent TMD task; `spawnArg1.value`
/// selects its model coordinate (part 12 in the body descriptor's spawn).
/// The child borrows the parent's coordinate and lighting, joins its teardown
/// tree and sorts two OT entries earlier. Missing buffers are requested when
/// the parent permits automatic allocation, including while drawing is hidden.
static void _actor511000RupertPropTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_511000_80131E30;
    states.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

/// Runs Rupert Broderick's body initialization, frame update or teardown.
///
/// Requires the live descriptor-created twenty-part TMD body and state 0
/// initialize, 1 update or 2 exit. State is not bounds-checked. Initialization
/// creates the work and two held-model children; later states require them live.
/// `spawnArg2.pointer` borrows the owning enemy used by the exit handler.
static void _actor511000RupertBodyTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_511000_80131E3C;
    states.funcs[task->state](task);
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

/// Reseeds Rupert's driven slots, updates their poses and enables ticking.
///
/// Requires the work's rig bound to a live twenty-part TMD body and a set table
/// containing the requested clip. Slots 1..19 are driven; slot 0 is retained.
/// A nonzero blend requires previously initialized slots, captures each slot's
/// existing pose and blends to the selected track's start over six normal-rate
/// frames; reset initializes the slots instead. Each slot is ticked before
/// future frame updates are enabled. Only clip and blend choice are read from
/// the request, through this call; bank data and playback storage remain borrowed.
static inline void _actor511000ReseedRupertAnimation(_Actor511000RupertBroderickWork* work, const AnimationPlayRequest* request)
{
    enum { ACTOR_511000_RUPERT_BLEND_FRAMES = 6 };
    s32 slotIndex;

    work->animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, ACTOR_511000_RUPERT_BLEND_FRAMES);
        }
    } else {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationResetSlot(&work->rig.anim, slotIndex, work->animId);
        }
    }
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->ticking = true;
}

/// Applies an animation request to Rupert Broderick's twenty-part body.
///
/// Requires a live TMD body and initialized work. The borrowed request selects
/// bank 0 and a valid clip in that bank's four-entry set table. A bank change
/// rebinds the rig; a clip change reseeds and immediately ticks slots 1..19.
/// Nonzero blend uses six frames, ignoring the requested duration. Repeating the
/// current bank and clip leaves playback alone. The ID and second payload are
/// ignored; returns 0. Bank resources must outlive playback.
static s32 _actor511000PlayRupertAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor511000RupertBroderickWork* work;
    TmdObject*                       model;

    work  = task->work;
    model = task->extra.tmd;
    // A new bank invalidates the selected clip before its pose is seeded.
    if (request->source.index != work->bank) {
        work->bank   = request->source.index;
        work->animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_511000_801472E4[work->bank], model, work->rig.poses,
                             work->rig.slots);
    }
    if (request->animationId != work->animId) {
        _actor511000ReseedRupertAnimation(work, request);
    }
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Changes Rupert's drawing and primitive-buffer policy.
///
/// Requires a live TMD body and initialized work. Mode 0 hides and enables
/// automatic buffer recovery; 1 shows, attempts missing-buffer allocation and
/// enables recovery; 2 hides, disables recovery and schedules release on the
/// third update; 3 shows with recovery disabled. Other flags are preserved.
/// Showing does not cancel a pending release. The ID and second payload are
/// ignored. Returns 0 for modes 0..3 even if allocation fails, otherwise 1.
static s32 _actor511000SetRupertModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum { ACTOR_511000_DRAW_SHOW_SKIP_AUTO_BUFFER = 3 };
    TmdObject*                       model;
    _Actor511000RupertBroderickWork* work;
    s32                              result;

    model  = task->extra.tmd;
    work   = task->work;
    result = 0;

    switch (drawMode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            // This mode also supplies the two-tick release countdown.
            work->freeCountdown = drawMode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_511000_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Selects Rupert's held model for an actor command.
///
/// Command 0 shows the revolver; 1 hides it and shows the second prop. Context
/// tags, the message ID and second payload are ignored. Missing child tasks are
/// skipped, other model flags are retained, and other commands change nothing.
/// Requires initialized body work and a borrowed command live through dispatch.
/// Returns 0 for both handled and ignored commands.
static s32 _actor511000ApplyRupertCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum { ACTOR_511000_RUPERT_SHOW_REVOLVER = 0,
           ACTOR_511000_RUPERT_SHOW_PROP     = 1 };
    _Actor511000RupertBroderickWork* work;
    Task*                            childTask;
    u16                              commandId;

    commandId = command->command;
    work      = task->work;

    switch (commandId) {
        case ACTOR_511000_RUPERT_SHOW_REVOLVER:
            childTask = work->gunTask;
            if (childTask != NULL) {
                childTask->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case ACTOR_511000_RUPERT_SHOW_PROP:
            childTask = work->gunTask;
            if (childTask != NULL) {
                childTask->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            childTask = work->propTask;
            if (childTask != NULL) {
                childTask->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
    }
    return 0;
}

/// Sets Rupert's eye image or starts a timed blink for message 0x7E0.
///
/// Requires a live TMD body and initialized work. Modes 0 and 2 post open eyes,
/// 1 closed eyes, and 3 half-open eyes followed by closed, half-open and open on
/// subsequent updates. Closed and the following half-open each last two ticks.
/// Starting a blink retains the current countdown; a static image does not cancel
/// a blink. Pixel data stays borrowed through the GPU transfer. The message ID
/// and second payload are ignored. Returns the upload result (0 for these lists);
/// unknown modes change nothing and return 0.
static s32 _actor511000SetRupertEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedArg)
{
    enum { ACTOR_511000_EYES_OPEN           = 0,
           ACTOR_511000_EYES_CLOSED         = 1,
           ACTOR_511000_EYES_OPEN_ALTERNATE = 2,
           ACTOR_511000_EYES_BLINK          = 3,
           ACTOR_511000_EYES_Y_ROWS         = 40,
           ACTOR_511000_EYES_WIDTH_WORDS    = 24,
           ACTOR_511000_EYES_HEIGHT_ROWS    = 16,
           ACTOR_511000_BLINK_FRAME_DELAY   = 1 };
    RECT            eyeRect;
    GpuImageUpload* uploadList;
    s32             result;

    result    = 0;
    eyeRect.x = 0;
    eyeRect.y = ACTOR_511000_EYES_Y_ROWS;
    eyeRect.w = ACTOR_511000_EYES_WIDTH_WORDS;
    eyeRect.h = ACTOR_511000_EYES_HEIGHT_ROWS;

    switch (eyeMode) {
        case ACTOR_511000_EYES_CLOSED:
            uploadList = &D_actor_511000_801472B4[0];
            break;
        case ACTOR_511000_EYES_OPEN:
        case ACTOR_511000_EYES_OPEN_ALTERNATE:
            uploadList = &D_actor_511000_80146C74[0];
            break;
        case ACTOR_511000_EYES_BLINK: {
            _Actor511000RupertBroderickWork* stepWork;
            _Actor511000RupertBroderickWork* delayWork;

            stepWork                   = task->work;
            stepWork->blinkStep        = ACTOR_511000_BLINK_CLOSED;
            delayWork                  = task->work;
            delayWork->blinkFrameDelay = ACTOR_511000_BLINK_FRAME_DELAY;
            uploadList                 = &D_actor_511000_80146F94[0];
            break;
        }
        default:
            uploadList = NULL;
            break;
    }

    if (uploadList != NULL) {
        result = actorRenderUploadTexture(task, uploadList, &eyeRect);
    }
    return result;
}

/// Mirrors helicopter visibility and aims its searchlight during view 24.
///
/// `spawnArg2.pointer` borrows the parent TMD task. Its `killCountdown` is
/// the sequence frame, constrained to 0..119 by the parent's update. Other
/// views retain the searchlight's last pose. The glow spans frames 86..92,
/// peaking at 89; its rim channels are 30 darker than the centre (30..225).
static void _actor511000TickHelicopterSearchlight(Task* task)
{
    enum { SEQUENCE_VIEW   = 0x18,
           GLOW_PEAK_FRAME = 89,
           RIM_SHADE_DROP  = 30 };
    Task*          parentTask;
    TmdObject*     model;
    GfxCoord*      root;
    const CVECTOR* centerColor;
    const CVECTOR* colorTable;
    s32            frameDistance;
    u8             rimRgb[3];

    parentTask = task->spawnArg2.pointer;
    model      = task->extra.tmd;
    root       = model->coords;

    if (!(parentTask->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    // The parent advances and clamps this frame before its attached searchlight runs.
    if (gGameSession->location.loc.view == SEQUENCE_VIEW) {
        root->param.rot.vx = D_actor_511000_80147AC4[parentTask->killCountdown].vx;
        root->param.rot.vy = D_actor_511000_80147AC4[parentTask->killCountdown].vy;
        root->param.rot.vz = D_actor_511000_80147AC4[parentTask->killCountdown].vz;
        RotMatrix(&root->param.rot, &root->coord);
        root->composeStamp = GRAPHICS_COORD_DIRTY;

        frameDistance = parentTask->killCountdown - GLOW_PEAK_FRAME;
        if (frameDistance < 0) {
            frameDistance = GLOW_PEAK_FRAME - parentTask->killCountdown;
        }
        if (frameDistance < ARRAY_SIZE(D_actor_511000_80149004)) {
            colorTable  = D_actor_511000_80149004;
            centerColor = &colorTable[frameDistance];
            rimRgb[0]   = centerColor->r - RIM_SHADE_DROP;
            rimRgb[1]   = centerColor->g - RIM_SHADE_DROP;
            rimRgb[2]   = centerColor->b - RIM_SHADE_DROP;
            _actor511000DrawHelicopterSearchlightGlow(task, centerColor, rimRgb);
        }
    }
}

/// Draws the helicopter searchlight's additive, 300-pixel-radius screen glow.
///
/// Borrows a live TMD task, one centre colour and three unsigned rim channels
/// for the call. Projects the model origin and emits 16 Gouraud triangles plus
/// a draw-mode packet. The caller must provide primitive space for all packets;
/// the current OT must contain the masked depth entry displaced by -30 tags.
/// Projection flags are deliberately not used to clip or suppress the glow.
static void _actor511000DrawHelicopterSearchlightGlow(Task* task, const CVECTOR* centerColor, const u8* rimRgb)
{
    enum { RIM_POINT_COUNT    = 16,
           RADIUS_PIXELS      = 300,
           UNIT_OFFSET_SCALE  = 4096,
           OT_DISPLACEMENT    = -30,
           ADDITIVE_DRAW_PAGE = getTPage(0, GPU_BLEND_ADD, 640, 0) };
    SVECTOR   worldOrigin;
    DVECTOR   screenRim[RIM_POINT_COUNT];
    MATRIX    worldTransform;
    long      screenCenter;
    long      projectionScale;
    long      projectionFlags;
    s32       sortDepth;
    POLY_G3*  triangle;
    DR_TPAGE* drawMode;
    u16       centerX;
    u16       centerY;
    s32       radiusPixels;
    u_long*   orderingTag;
    s32       pointIndex;
    DVECTOR*  rimPoint;
    DVECTOR*  unitOffset;

/// Emits one Gouraud triangle from the searchlight centre to two rim points.
///
/// `endPoint` is a live DVECTOR lvalue, evaluated twice without side effects.
/// Captures `triangle` (overwritten), `rimPoint`, `centerX`, `centerY`,
/// `centerColor`, `rimRgb`, `orderingTag` and `gGpuPrimCursor` (advanced by one
/// POLY_G3). Requires a live OT tag and packet space. Expands to standalone
/// statements inside a compound block and is undefined after the two uses.
#define ACTOR_511000_EMIT_SEARCHLIGHT_TRIANGLE(endPoint) \
    triangle       = gGpuPrimCursor;                     \
    gGpuPrimCursor = triangle + 1;                       \
    setPolyG3(triangle);                                 \
    setSemiTrans(triangle, 1);                           \
    triangle->x0 = centerX;                              \
    triangle->y0 = centerY;                              \
    triangle->x1 = rimPoint->vx;                         \
    triangle->y1 = rimPoint->vy;                         \
    triangle->x2 = (endPoint).vx;                        \
    triangle->y2 = (endPoint).vy;                        \
    triangle->r0 = centerColor->r;                       \
    triangle->g0 = centerColor->g;                       \
    triangle->b0 = centerColor->b;                       \
    triangle->r1 = rimRgb[0];                            \
    triangle->g1 = rimRgb[1];                            \
    triangle->b1 = rimRgb[2];                            \
    triangle->r2 = rimRgb[0];                            \
    triangle->g2 = rimRgb[1];                            \
    triangle->b2 = rimRgb[2];                            \
    addPrim(orderingTag, triangle);

    // Project the lamp origin; the halo radius stays fixed in screen pixels.
    gfxComposeNodeWorldTransform(task->extra.tmd->coords, &worldTransform, &worldOrigin);
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    sortDepth    = RotTransPers(&worldOrigin, &screenCenter, &projectionScale, &projectionFlags);
    centerX      = screenCenter;
    centerY      = screenCenter >> 16;
    unitOffset   = D_actor_511000_80149014;
    rimPoint     = screenRim;
    radiusPixels = RADIUS_PIXELS;
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(screenRim); pointIndex++) {
        rimPoint->vx = centerX + radiusPixels * unitOffset->vx / UNIT_OFFSET_SCALE;
        rimPoint->vy = centerY + radiusPixels * unitOffset->vy / UNIT_OFFSET_SCALE;
        rimPoint++;
        unitOffset++;
    }
    // Pack the fan first, then prepend its additive draw mode to the same OT chain.
    orderingTag = GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(sortDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK) + OT_DISPLACEMENT;
    rimPoint    = screenRim;
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(screenRim) - 1; pointIndex++, rimPoint++) {
        ACTOR_511000_EMIT_SEARCHLIGHT_TRIANGLE(rimPoint[1]);
    }
    ACTOR_511000_EMIT_SEARCHLIGHT_TRIANGLE(screenRim[0]);
#undef ACTOR_511000_EMIT_SEARCHLIGHT_TRIANGLE
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, 1, 0, ADDITIVE_DRAW_PAGE);
    addPrim(orderingTag, drawMode);
}

/// Blends one little-endian RGB555 palette colour through the GTE.
///
/// Both sources provide two readable bytes and the destination two writable bytes;
/// no halfword alignment is needed, and the destination may alias either source.
/// Weights use twelve fractional bits, each in 0..4096 and summing to 4096.
/// Bit 15 is discarded. Changes GTE interpolation state and retains no pointers.
static inline void _actor511000BlendPaletteColor(u8* destination, const u8* firstColor, const u8* secondColor, s32 firstWeight, s32 secondWeight)
{
    enum { ACTOR_511000_PALETTE_CHANNEL_MASK = 0x1F };
    CVECTOR channels[3];
    s32     packedColor;

    packedColor   = firstColor[0] | (firstColor[1] << 8);
    channels[0].r = ((u16)packedColor >> 10) & ACTOR_511000_PALETTE_CHANNEL_MASK;
    channels[0].g = ((u16)packedColor >> 5) & ACTOR_511000_PALETTE_CHANNEL_MASK;
    channels[0].b = packedColor & ACTOR_511000_PALETTE_CHANNEL_MASK;
    packedColor   = secondColor[0] | (secondColor[1] << 8);
    channels[1].r = ((u16)packedColor >> 10) & ACTOR_511000_PALETTE_CHANNEL_MASK;
    channels[1].g = ((u16)packedColor >> 5) & ACTOR_511000_PALETTE_CHANNEL_MASK;
    channels[1].b = packedColor & ACTOR_511000_PALETTE_CHANNEL_MASK;
    LoadAverageCol((u8*)&channels[0], (u8*)&channels[1], firstWeight, secondWeight, (u8*)&channels[2]);
    packedColor    = channels[2].b + ((channels[2].r << 10) + (channels[2].g << 5));
    destination[1] = (u32)packedColor >> 8;
    destination[0] = packedColor;
}

/// Advances and uploads the helicopter's sixteen-colour palette flash.
///
/// Requires initialized helicopter work with a 4.12 fade weight in 0..4096
/// and a nonnegative hold counter. The weight rises by 1365 per tick, reaching
/// the dark palette on its fourth increment; full-weight ticks consume the
/// hold, then the next tick snaps back to the lit palette and rearms it.
/// Reads and writes RGB555 colours as little-endian byte pairs, discarding bit
/// 15. The upload record must already borrow `work->palette`, whose storage must
/// remain live and unchanged until GPU transfer completes. Clobbers GTE state.
static void _actor511000TickHelicopterPaletteFlash(_Actor511000HelicopterWork* work)
{
    enum { ACTOR_511000_PALETTE_FADE_STEP = 0x555 };
    s32 byteIndex;
    s32 litWeight;
    s32 darkWeight;

    // Reach the dark palette, hold it, then snap back for the next flash.
    if (work->paletteHold != 0) {
        work->paletteFade += ACTOR_511000_PALETTE_FADE_STEP;
        if (work->paletteFade >= ACTOR_511000_PALETTE_FADE_FULL) {
            work->paletteFade = ACTOR_511000_PALETTE_FADE_FULL;
            if (--work->paletteHold < 0) {
                work->paletteHold = 0;
            }
        }
    } else {
        work->paletteFade -= ACTOR_511000_PALETTE_FADE_FULL;
        if (work->paletteFade <= 0) {
            work->paletteFade = 0;
            work->paletteHold = ACTOR_511000_PALETTE_HOLD_TICKS;
        }
    }
    litWeight  = ACTOR_511000_PALETTE_FADE_FULL - work->paletteFade;
    darkWeight = work->paletteFade;
    // Keep byte-pair accesses for the packed palette and publish the complete row.
    byteIndex = 0;
    do {
        _actor511000BlendPaletteColor(&work->palette[byteIndex], &D_actor_511000_80147E84[byteIndex], &D_actor_511000_80147EC4[byteIndex], litWeight, darkWeight);
        byteIndex += 2;
    } while (byteIndex < ARRAY_SIZE(work->palette));
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
    _actor511000PoseHelicopterSequenceFrame(task, D_actor_511000_80147344, D_actor_511000_80147704, 0);
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
        _actor511000TickHelicopterPaletteFlash(task->work);
    }
    if (gGameSession->location.loc.view == 0x18) {
        frame               = task->killCountdown + 1;
        task->killCountdown = frame;
        if (frame >= 0x78) {
            task->killCountdown = 0x77;
        }
        viewQueueCamera(&D_actor_511000_80147EE4[task->killCountdown]);
        _actor511000PoseHelicopterSequenceFrame(task, D_actor_511000_80147344, D_actor_511000_80147704, task->killCountdown);
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

/// Begins default teardown of the helicopter task and its attached children.
static void _actor511000KillHelicopter(Task* task)
{
    taskKill(task);
}

/// Attaches a helicopter rotor to the hull and inherits its lighting and visibility.
///
/// Both TMD tasks must be live. `spawnArg2.pointer` borrows the helicopter;
/// `spawnArg1.value` is 1 for the main rotor or 2 for the tail rotor. The
/// placement is local to the hull root. Joins the parent's teardown tree and
/// advances from state 0 to 1; borrowed coordinates and matrices must remain live.
static void _actor511000AttachHelicopterRotor(Task* task)
{
    Task*      parentTask;
    TmdObject* model;
    TmdObject* parentModel;
    GfxCoord*  root;
    GfxCoord*  parentRoot;

    parentTask  = task->spawnArg2.pointer;
    parentModel = parentTask->extra.tmd;
    model       = task->extra.tmd;
    parentRoot  = parentModel->coords;
    // Borrow the helicopter transform and lighting for this child task.
    model->lightMtx = parentModel->lightMtx;
    model->colorMtx = parentModel->colorMtx;
    model->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    root            = model->coords;
    if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        model->flags = 0;
    }
    _actor511000PlaceHelicopterPart(task);
    root->parent = parentRoot;
    taskReparent(parentTask, task);
    task->state += 1;
}

/// Spins a drawable helicopter's main or tail rotor and mirrors its visibility.
///
/// `spawnArg2.pointer` borrows the live helicopter TMD task. Part index 1
/// advances yaw by 660 units per tick; index 2 advances pitch by 1000, with
/// 4096 units per turn. Other indices only rebuild the matrix. A hidden parent
/// hides the child and freezes its angles. Automatic-buffer policy is retained.
static void _actor511000TickHelicopterRotor(Task* task)
{
    enum { MAIN_ROTOR      = 1,
           TAIL_ROTOR      = 2,
           MAIN_YAW_STEP   = 660,
           TAIL_PITCH_STEP = 1000 };
    Task*      parentTask;
    TmdObject* model;
    TmdObject* parentModel;
    GfxCoord*  root;

    model       = task->extra.tmd;
    root        = model->coords;
    parentTask  = task->spawnArg2.pointer;
    parentModel = parentTask->extra.tmd;

    if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;

        // Read the stored angle unsigned, then wrap in 4096 units per turn.
        switch (task->spawnArg1.value) {
            case MAIN_ROTOR:
                root->param.rot.vy = ((u16)root->param.rot.vy + MAIN_YAW_STEP) & ACTOR_TRANSFORM_ANGLE_MASK;
                break;
            case TAIL_ROTOR:
                root->param.rot.vx = ((u16)root->param.rot.vx + TAIL_PITCH_STEP) & ACTOR_TRANSFORM_ANGLE_MASK;
                break;
        }

        RotMatrix(&root->param.rot, &root->coord);
        root->composeStamp = GRAPHICS_COORD_DIRTY;
        return;
    }
    model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

/// Begins default teardown of an attached helicopter rotor.
static void _actor511000KillHelicopterRotor(Task* task)
{
    taskKill(task);
}

/// Attaches the searchlight beneath the helicopter with the sequence's initial aim.
///
/// Both TMD tasks must be live. `spawnArg2.pointer` borrows the helicopter;
/// `spawnArg1.value` is 3 for the searchlight's hull-relative translation.
/// Borrows its root coordinate and lighting, inherits visibility and joins its
/// teardown tree. Applies sequence frame 0 in 4096-unit Euler angles and
/// advances to state 1. The parent resources must outlive the attached model.
static void _actor511000AttachHelicopterSearchlight(Task* task)
{
    Task*      parentTask;
    TmdObject* model;
    TmdObject* parentModel;
    GfxCoord*  root;
    GfxCoord*  parentRoot;

    parentTask  = task->spawnArg2.pointer;
    parentModel = parentTask->extra.tmd;
    model       = task->extra.tmd;
    parentRoot  = parentModel->coords;
    // Borrow the helicopter transform and lighting for this child task.
    model->lightMtx = parentModel->lightMtx;
    model->colorMtx = parentModel->colorMtx;
    model->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    root            = model->coords;
    if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        model->flags = 0;
    }
    _actor511000PlaceHelicopterPart(task);
    root->parent = parentRoot;
    taskReparent(parentTask, task);
    root->param.rot.vx = D_actor_511000_80147AC4[0].vx;
    root->param.rot.vy = D_actor_511000_80147AC4[0].vy;
    root->param.rot.vz = D_actor_511000_80147AC4[0].vz;
    RotMatrix(&root->param.rot, &root->coord);
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state       += 1;
}

/// Begins default teardown of the helicopter's attached searchlight.
static void _actor511000KillHelicopterSearchlight(Task* task)
{
    taskKill(task);
}

/// Resets the helicopter's sequence cursor to frame zero.
///
/// Installed for `ACTOR_MESSAGE_PLAY_ANIMATION`, but reads neither payload nor
/// the message ID. The next view-24 update advances the cursor before applying
/// its hull pose, camera and searchlight frame. Returns 0.
static s32 _actor511000RestartHelicopterSequence(Task* task, s32 messageId, s32 unusedRequest, s32 unusedArg)
{
    task->killCountdown = 0;
    return 0;
}

/// Places the helicopter's root model and enables its active drawing.
///
/// Requires a live TMD body and a borrowed transform through dispatch. Position
/// uses parent-coordinate units and Euler angles use 4096 units per turn.
/// Copies all XYZ components, rebuilds rotation and invalidates composition.
/// Other draw and buffer flags are retained. The ID and second payload are
/// ignored; returns 0.
static s32 _actor511000PlaceHelicopter(Task* task, s32 messageId, const ActorTransform* transform, s32 unusedArg)
{
    GfxCoord*  root;
    TmdObject* model;

    model              = task->extra.tmd;
    root               = model->coords;
    root->coord.t[0]   = transform->pos.vx;
    root->coord.t[1]   = transform->pos.vy;
    root->coord.t[2]   = transform->pos.vz;
    root->param.rot.vx = transform->rot.vx;
    root->param.rot.vy = transform->rot.vy;
    root->param.rot.vz = transform->rot.vz;
    RotMatrix(&root->param.rot, &root->coord);
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    model->flags      &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
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

/// Applies a table frame's rotation and translation to the helicopter root.
///
/// Requires a live TMD body and readable rotation and translation entries at the
/// signed low sixteen bits of `frameIndex`. Callers use frames 0..119 of both
/// 120-entry tables. Euler angles use 4096 units per turn; translations are in
/// parent-coordinate units. Rebuilds local rotation and invalidates composition;
/// does not retain either table or change ancestry, lighting or drawing.
static void _actor511000PoseHelicopterSequenceFrame(Task* task, const SVECTOR* rotations, const SVECTOR* translations, s32 frameIndex)
{
    GfxCoord* root;
    s16       sequenceFrame;

    sequenceFrame      = frameIndex;
    root               = task->extra.tmd->coords;
    root->param.rot.vx = rotations[sequenceFrame].vx;
    root->param.rot.vy = rotations[sequenceFrame].vy;
    root->param.rot.vz = rotations[sequenceFrame].vz;
    root->coord.t[0]   = translations[sequenceFrame].vx;
    root->coord.t[1]   = translations[sequenceFrame].vy;
    root->coord.t[2]   = translations[sequenceFrame].vz;
    RotMatrix(&root->param.rot, &root->coord);
    root->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Sets a helicopter part's hull-relative translation and resets its Euler angles.
///
/// Requires a live TMD body and an unchecked `spawnArg1.value` in 0..3;
/// the attachment callers use 1 main rotor, 2 tail rotor or 3 searchlight.
/// Rebuilds the local rotation and invalidates the composed transform without
/// changing coordinate ancestry, task links, visibility or lighting.
static void _actor511000PlaceHelicopterPart(Task* task)
{
    GfxCoord* root;

    root               = task->extra.tmd->coords;
    root->coord.t[0]   = D_actor_511000_80148FE4[task->spawnArg1.value].vx;
    root->coord.t[1]   = D_actor_511000_80148FE4[task->spawnArg1.value].vy;
    root->coord.t[2]   = D_actor_511000_80148FE4[task->spawnArg1.value].vz;
    root->param.rot.vx = 0;
    root->param.rot.vy = 0;
    root->param.rot.vz = 0;
    RotMatrix(&root->param.rot, &root->coord);
    root->composeStamp = GRAPHICS_COORD_DIRTY;
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

/// Runs the helicopter's initialization, frame update or teardown state.
///
/// Requires its live descriptor-created TMD hull and state 0 initialize,
/// 1 update or 2 kill; state is not bounds-checked. Initialization creates the
/// work and binds its lighting and palette storage. Attached children borrow
/// the hull and work until teardown. Frame updates require that storage live.
static void _actor511000HelicopterTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_511000_80131E48;
    states.funcs[task->state](task);
}

/// Runs the helicopter rotor states: 0 attach, 1 spin and 2 kill.
///
/// The descriptor selects the main or tail rotor model. Attachment borrows
/// the helicopter task through `spawnArg2.pointer`; `spawnArg1.value` is
/// 1 main rotor or 2 tail rotor. The parent must outlive its attached child.
static void _actor511000HelicopterRotorTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_511000_80131E54;
    states.funcs[task->state](task);
}

/// Runs the helicopter searchlight states: 0 attach, 1 aim and glow, 2 kill.
///
/// Attachment requires a live helicopter TMD task in `spawnArg2.pointer`
/// and part index 3 in `spawnArg1.value`. Its pose follows sequence frames
/// 0..119 in view 24; the parent must outlive the attached model.
static void _actor511000HelicopterSearchlightTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_511000_80131E60;
    states.funcs[task->state](task);
}

/// Initializes the No. 9 golem's hidden body, playback storage and three children.
///
/// Requires its owning enemy and live nineteen-part TMD task in state 0.
/// The zeroed work is owned by the task; body and child models borrow its
/// lighting matrices. No clip is played until an animation message arrives.
/// Children attached at parts 8 and 3 receive the parent's area-placement
/// texture-page and CLUT-row offsets, rebuilding existing buffers; part 12
/// retains its own offsets. The parent's upper placement-key nibble must select
/// an existing placement in the synchronized location variant. Advances to
/// update state 1, or destroys the enemy if work allocation fails. The first
/// two child spawns are assumed to succeed, matching their unchecked use.
static void _actor511000SpawnNo9Golem(Enemy* enemy, Task* task)
{
    enum { ACTOR_511000_GOLEM_UPDATE_STATE = 1,
           ACTOR_511000_GOLEM_NO_CLIP      = 0,
           ACTOR_511000_GOLEM_PART8_CHILD  = 1,
           ACTOR_511000_GOLEM_PART3_CHILD  = 2,
           ACTOR_511000_GOLEM_PART12_CHILD = 3 };
    GameLocationKey           key;
    GameLocationKey*          sessionKey;
    u8                        view;
    u8                        stage;
    AreaVariant*              layout;
    TmdObject*                model;
    GfxCoord*                 bodyRoot;
    _Actor511000No9GolemWork* work;
    TaskDesc*                 childDescriptors;
    u32                       placeIndex;
    u32                       placeIndex2;
    AreaPlacement*            placements;
    Enemy*                    spawned;
    GameSession*              session;

    model    = task->extra.tmd;
    bodyRoot = model->coords;
    work     = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work      = work;
    model->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    animationInitContext(&work->rig.anim, D_actor_511000_801550C0, model, work->rig.poses, work->rig.slots);
    work->animId           = ACTOR_511000_GOLEM_NO_CLIP;
    task->msgTable         = D_actor_511000_801550A0;
    bodyRoot->composeStamp = GRAPHICS_COORD_DIRTY;

    childDescriptors = D_actor_511000_80155070;
    spawned          = enemySpawnFromTable(childDescriptors, ACTOR_511000_GOLEM_PART8_CHILD, 0, enemy);
    session          = gGameSession;
    // Child models inherit the texture relocation of the parent's placement.
    // The task address retains the spawn-result holder to preserve load order.
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
    layout                   = areaGetVariant(&key);
    placements               = layout->placements;
    model->texturePageOffset = placements[placeIndex].texturePageOffset;
    model->clutRowOffset     = placements[placeIndex].clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    spawned     = enemySpawnFromTable(childDescriptors, ACTOR_511000_GOLEM_PART3_CHILD, 0, enemy);
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
    layout                   = areaGetVariant(&key);
    placements               = layout->placements;
    model->texturePageOffset = placements[placeIndex2].texturePageOffset;
    model->clutRowOffset     = placements[placeIndex2].clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    enemySpawnFromTable(childDescriptors, ACTOR_511000_GOLEM_PART12_CHILD, 0, enemy);
    task->state = ACTOR_511000_GOLEM_UPDATE_STATE;
}

/// Advances the No. 9 golem's pose, lighting, ground shadow and exit fade.
///
/// Requires its owning enemy, live nineteen-part TMD body and initialized work.
/// Nonzero clip IDs tick slots 1..18. Clips below 3 refresh actor lighting and
/// draw a shadow at part 1; clips 3 and up instead fade the nine coefficients of
/// both borrowed matrices by 15/16 every third tick, retaining ambient terms.
/// An exit tick beginning above 96 enters destroy state 2. The signed halfword
/// tick counter advances on every call. Uses two SDK VECTOR slots (32 scratch
/// bytes), including across nested lighting and ground queries.
static void _actor511000TickNo9Golem(Enemy* enemy, Task* task)
{
    enum { ACTOR_511000_GOLEM_FIRST_EXIT_CLIP    = 3,
           ACTOR_511000_GOLEM_FADE_PERIOD_TICKS  = 3,
           ACTOR_511000_GOLEM_LAST_EXIT_TICK     = 96,
           ACTOR_511000_GOLEM_DESTROY_STATE      = 2,
           ACTOR_511000_GOLEM_SHADOW_HALF_SIZE   = 1024,
           ACTOR_511000_GOLEM_FADE_NUMERATOR     = 15,
           ACTOR_511000_GOLEM_FADE_FRACTION_BITS = 4 };
    TmdObject*                model;
    VECTOR*                   worldPosition;
    VECTOR*                   groundPoint;
    _Actor511000No9GolemWork* work;
    GfxCoord*                 modelCoords;
    GfxCoord*                 sampleCoord;
    s32                       loopIndex;
    s32                       column;
    s32                       animationId;

// Scale both borrowed matrices together; captures work, loopIndex and column.
// The counters traverse all nine coefficients and ambient translations are retained.
#define ACTOR_511000_FADE_GOLEM_LIGHTING()                                                                                                                          \
    do {                                                                                                                                                            \
        for (loopIndex = 0; loopIndex < ARRAY_SIZE(work->color.m); loopIndex++) {                                                                                   \
            for (column = 0; column < ARRAY_SIZE(work->color.m[loopIndex]); column++) {                                                                             \
                work->color.m[loopIndex][column] = (work->color.m[loopIndex][column] * ACTOR_511000_GOLEM_FADE_NUMERATOR) >> ACTOR_511000_GOLEM_FADE_FRACTION_BITS; \
                work->light.m[loopIndex][column] = (work->light.m[loopIndex][column] * ACTOR_511000_GOLEM_FADE_NUMERATOR) >> ACTOR_511000_GOLEM_FADE_FRACTION_BITS; \
            }                                                                                                                                                       \
        }                                                                                                                                                           \
    } while (0)

    model = task->extra.tmd;
    SCRATCH_STACK_RESERVE_BYTES(2 * sizeof(*worldPosition));
    work          = task->work;
    modelCoords   = model->coords;
    sampleCoord   = &modelCoords[1];
    animationId   = work->animId;
    worldPosition = SCRATCH_STACK_CURSOR(VECTOR);
    if (animationId != 0) {
        for (loopIndex = 1; loopIndex < ARRAY_SIZE(work->rig.slots); loopIndex++) {
            animationTickSlot(&work->rig.anim, loopIndex);
        }
    }
    // Refresh lighting at body part 1, then project its ground-shadow centre.
    if (work->animId < ACTOR_511000_GOLEM_FIRST_EXIT_CLIP) {
        sampleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(sampleCoord);
        worldPosition->vx = sampleCoord->workm.t[0];
        worldPosition->vy = sampleCoord->workm.t[1];
        worldPosition->vz = sampleCoord->workm.t[2];
        worldCoordUpdateActorColor(enemy, worldPosition, 0, 0);
        worldPosition->vx = sampleCoord->workm.t[0];
        worldPosition->vy = sampleCoord->workm.t[1];
        worldPosition->vz = sampleCoord->workm.t[2];
        groundPoint       = worldPosition + 1;
        if (worldCollisionProjectGroundPoint((VECTOR3*)worldPosition, (VECTOR3*)groundPoint) != 0) {
            effectDrawGroundShadow((VECTOR3*)groundPoint, ACTOR_511000_GOLEM_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    } else {
        if ((s16)(work->animTicks % ACTOR_511000_GOLEM_FADE_PERIOD_TICKS) == 0) {
            ACTOR_511000_FADE_GOLEM_LIGHTING();
#undef ACTOR_511000_FADE_GOLEM_LIGHTING
        }
        if (work->animTicks > ACTOR_511000_GOLEM_LAST_EXIT_TICK) {
            task->state = ACTOR_511000_GOLEM_DESTROY_STATE;
        }
    }
    work->animTicks++;
    modelCoords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(2 * sizeof(*worldPosition));
}

/// Runs the No. 9 golem's initialization, frame update or destroy state.
///
/// Requires its live nineteen-part TMD task and owning enemy in
/// `spawnArg2.pointer`. State is 0 initialize, 1 update or 2 destroy and is not
/// bounds-checked. Update requires initialized work; destroy releases the task
/// and its enemy. The enemy and borrowed body resources must remain live.
static void _actor511000No9GolemTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = D_actor_511000_80131E6C;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Restarts the No. 9 golem's body animation and exit timer.
///
/// Requires a live nineteen-part TMD body and initialized work. The borrowed,
/// read-only request selects a loaded clip 1..3 from the fixed set table;
/// source bank, blend choice and duration are ignored. Always resets slots
/// 1..18 and `animTicks`, including repeated clips, without immediately posing
/// the model. Slot 0 is retained. Bank data must outlive playback. The message
/// ID and second payload are ignored; returns 0.
static s32 _actor511000PlayNo9GolemAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor511000No9GolemWork* work;
    s32                       slotIndex;

    work         = task->work;
    work->animId = request->animationId;
    slotIndex    = 1;
    do {
        animationResetSlot(&work->rig.anim, slotIndex, work->animId);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
    work->animTicks = 0;
    return 0;
}

#include "../../shared/actor_messages_place_rot_matrix.inc.c"

/// Replaces the No. 9 golem body's drawing and automatic-buffer flags.
///
/// Requires a live TMD body. Bit 0 shows the model; clear hides it. Bit 1
/// disables automatic missing-buffer allocation. Other request bits are ignored
/// and all other model flags are cleared. No buffers are allocated or released,
/// and the actor state is retained. The message ID and second payload are
/// ignored; returns 0.
static s32 _actor511000SetNo9GolemModelDraw(Task* task, s32 messageId, s32 drawFlags, s32 unusedArg)
{
    enum { ACTOR_511000_GOLEM_DRAW_SHOW             = 1 << 0,
           ACTOR_511000_GOLEM_DRAW_SKIP_AUTO_BUFFER = 1 << 1 };
    TmdObject* model;

    model = task->extra.tmd;
    if (!(drawFlags & ACTOR_511000_GOLEM_DRAW_SHOW)) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags = 0;
    }
    if (drawFlags & ACTOR_511000_GOLEM_DRAW_SKIP_AUTO_BUFFER) {
        task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Runs the No. 9 golem's part-8 attachment states.
///
/// State 0 attaches and state 1 composes its root each tick; no other state is
/// valid. The descriptor supplies the child model and the spawner establishes
/// `task->parent` and the borrowed enemy in `spawnArg2.pointer`. The parent must
/// own a live nineteen-part body and work block that outlive the child.
static void _actor511000No9GolemPart8Task(Task* task)
{
    EnemyTaskFunc states[2] = { _actor511000AttachNo9GolemPart8Model, _actor511000TickNo9GolemPart8Model };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the No. 9 golem's part-8 model and shows it with the body's lighting.
///
/// The task's existing parent must own the live nineteen-part TMD body and
/// its `_Actor511000No9GolemWork`. Borrows coordinate 8 and both work matrices,
/// retains the child's local transform and advances from state 0 to 1.
/// Parent coordinate and work storage must outlive the child. The enemy callback
/// argument is unused; task parenting was established by the spawner.
static void _actor511000AttachNo9GolemPart8Model(Enemy* enemy, Task* task)
{
    enum { PARENT_PART  = 8,
           UPDATE_STATE = 1 };
    Task*                     parentTask;
    TmdObject*                model;
    _Actor511000No9GolemWork* parentWork;
    GfxCoord*                 root;
    GfxCoord*                 parentCoords;

    parentTask   = task->parent;
    model        = task->extra.tmd;
    parentCoords = parentTask->extra.tmd->coords;
    root         = model->coords;
    parentWork   = parentTask->work;

    root->parent    = &parentCoords[PARENT_PART];
    model->lightMtx = &parentWork->light;
    model->flags    = 0;
    model->colorMtx = &parentWork->color;
    task->state     = UPDATE_STATE;
}

/// Invalidates and composes the golem's part-8 attachment transform each tick.
///
/// Requires a live child TMD root and its borrowed parent-coordinate chain.
/// The enemy callback argument is unused; lighting and task state are retained.
static void _actor511000TickNo9GolemPart8Model(Enemy* enemy, Task* task)
{
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}

/// Runs the No. 9 golem's part-3 attachment states.
///
/// State 0 attaches and state 1 composes its root each tick; no other state is
/// valid. The descriptor supplies the child model and the spawner establishes
/// `task->parent` and the borrowed enemy in `spawnArg2.pointer`. The parent must
/// own a live nineteen-part body and work block that outlive the child.
static void _actor511000No9GolemPart3Task(Task* task)
{
    EnemyTaskFunc states[2] = { _actor511000AttachNo9GolemPart3Model, _actor511000TickNo9GolemPart3Model };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the No. 9 golem's part-3 model and shows it with the body's lighting.
///
/// The task's existing parent must own the live nineteen-part TMD body and
/// its `_Actor511000No9GolemWork`. Borrows coordinate 3 and both work matrices,
/// retains the child's local transform and advances from state 0 to 1.
/// Parent coordinate and work storage must outlive the child. The enemy callback
/// argument is unused; task parenting was established by the spawner.
static void _actor511000AttachNo9GolemPart3Model(Enemy* enemy, Task* task)
{
    enum { PARENT_PART  = 3,
           UPDATE_STATE = 1 };
    Task*                     parentTask;
    TmdObject*                model;
    _Actor511000No9GolemWork* parentWork;
    GfxCoord*                 root;
    GfxCoord*                 parentCoords;

    parentTask   = task->parent;
    model        = task->extra.tmd;
    parentCoords = parentTask->extra.tmd->coords;
    root         = model->coords;
    parentWork   = parentTask->work;

    root->parent    = &parentCoords[PARENT_PART];
    model->lightMtx = &parentWork->light;
    model->flags    = 0;
    model->colorMtx = &parentWork->color;
    task->state     = UPDATE_STATE;
}

/// Invalidates and composes the golem's part-3 attachment transform each tick.
///
/// Requires a live child TMD root and its borrowed parent-coordinate chain.
/// The enemy callback argument is unused; lighting and task state are retained.
static void _actor511000TickNo9GolemPart3Model(Enemy* enemy, Task* task)
{
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}

/// Runs the No. 9 golem's part-12 attachment states.
///
/// State 0 attaches and state 1 composes its root each tick; no other state is
/// valid. The descriptor supplies the child model and the spawner establishes
/// `task->parent` and the borrowed enemy in `spawnArg2.pointer`. The parent must
/// own a live nineteen-part body and work block that outlive the child.
static void _actor511000No9GolemPart12Task(Task* task)
{
    EnemyTaskFunc states[2] = { _actor511000AttachNo9GolemPart12Model, _actor511000TickNo9GolemPart12Model };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the No. 9 golem's part-12 model with the body's lighting.
///
/// The task's existing parent must own a live nineteen-part TMD body and its
/// `_Actor511000No9GolemWork`. Borrows coordinate 12 and both work matrices,
/// retains the child's local transform, clears its model flags and advances
/// from state 0 to 1. Parent coordinate and work storage must outlive the child.
/// The enemy callback argument is unused; the spawner established task parenting.
static void _actor511000AttachNo9GolemPart12Model(Enemy* enemy, Task* task)
{
    enum { ACTOR_511000_GOLEM_PARENT_PART  = 12,
           ACTOR_511000_GOLEM_UPDATE_STATE = 1 };
    Task*                     parentTask;
    TmdObject*                model;
    _Actor511000No9GolemWork* parentWork;
    GfxCoord*                 root;
    GfxCoord*                 parentCoords;

    parentTask   = task->parent;
    model        = task->extra.tmd;
    parentCoords = parentTask->extra.tmd->coords;
    root         = model->coords;
    parentWork   = parentTask->work;

    root->parent    = &parentCoords[ACTOR_511000_GOLEM_PARENT_PART];
    model->lightMtx = &parentWork->light;
    model->flags    = 0;
    model->colorMtx = &parentWork->color;
    task->state     = ACTOR_511000_GOLEM_UPDATE_STATE;
}

/// Invalidates and composes the golem's part-12 attachment transform each tick.
///
/// Requires a live child TMD root and its borrowed parent-coordinate chain.
/// The enemy callback argument is unused; lighting and task state are retained.
static void _actor511000TickNo9GolemPart12Model(Enemy* enemy, Task* task)
{
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}
