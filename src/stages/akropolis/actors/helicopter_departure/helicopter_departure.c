#include "stages/akropolis/actors/helicopter_departure.h"

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
#include "../../../../shared/actor/model_placement.h"
#include "../../../../shared/actor/actor_messages.h"

/// Work block of the No. 9 golem, the package's enemy.
typedef struct {
    ActorAnimRig19 rig;            // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    MATRIX         color;          // Light-colour matrix lent to the golem's model and its attached models
    MATRIX         light;          // Light-direction matrix lent to the same models
    s32            animId;         // Clip the last play request seeded the slots with (0 none yet: the slots are not ticked; 3 and up fade the model out)
    s16            animTicks;      // Ticks since the last play request; paces the fade and ends the enemy
    byte           unknown_482[6]; // Allocated but never accessed; role unproven
} _No9Work;
STATIC_ASSERT_SIZEOF(_No9Work, 0x488);

/// RGB555/STP words in the helicopter's flashing palette row and VRAM upload.
#define PALETTE_COLORS 16

/// Byte extent of the helicopter's flashing palette row.
#define PALETTE_BYTES (PALETTE_COLORS * sizeof(u16))

/// Dark-palette weight at full fade, in the GTE's twelve-fractional-bit scale.
#define PALETTE_FADE_FULL ONE

/// Update ticks spent at the dark palette before snapping back to the lit palette.
#define PALETTE_HOLD_TICKS 30

/// Work block of the helicopter.
typedef struct {
    byte   unknown_0[8];           // Allocated but never accessed; role unproven
    s32    freeCountdown;          // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    u8     palette[PALETTE_BYTES]; // Blended palette row as uploaded: `PALETTE_COLORS` colours, low byte first
    s16    paletteFade;            // Share of the second stored palette in the blend, 4.12 fixed point (0 first palette only, `PALETTE_FADE_FULL` second only)
    s8     paletteHold;            // Ticks the full fade still holds; counted down only once the fade is full, and at 0 the blend snaps back
    s8     partsSpawned;           // Set once the first show command has spawned the three attached models, so a later one does not repeat it
    MATRIX light;                  // Light-direction matrix lent to the model object
    MATRIX color;                  // Light-colour matrix lent to the model object
} _HelicopterWork;
STATIC_ASSERT_SIZEOF(_HelicopterWork, 0x70);

/// `_RupertWork::blinkStep`: the eye image the blink posts next.
enum {
    BLINK_NONE   = 0, // No blink in progress
    BLINK_CLOSED = 1, // The closed eyes are posted next
    BLINK_HALF   = 2, // The half-open eyes are posted next
    BLINK_OPEN   = 3, // The open eyes are posted next, which ends the blink
};

/// Work block of Rupert Broderick's body.
typedef struct {
    ActorAnimRig20 rig;             // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    s32            ticking;         // Set once a clip has been applied, never cleared: the slots are ticked each frame
    s32            animId;          // Clip the slots were last seeded with, within `bank`
    s32            bank;            // Index, in the package's animation bank table, of the bank the rig is bound to
    s32            freeCountdown;   // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    MATRIX         light;           // Light-direction matrix lent to the model object
    MATRIX         color;           // Light-colour matrix lent to the model object
    Task*          gunTask;         // Task of the Mongoose revolver chained under his right hand; the shot's sound and muzzle flash are placed at its root
    Task*          radioTask;       // Task of his radio, chained under his left hand; an actor command shows it as it hides the revolver
    s16            blinkFrameDelay; // Value `blinkCountdown` restarts from after the closed and the half-open eyes: each is shown for this many ticks plus one
    s16            blinkCountdown;  // Ticks left before the blink posts its next eye image, which the tick taking it below 0 does; not reset as a blink starts or ends
    s16            blinkStep;       // Eye image the blink posts next (0 `BLINK_NONE`, else `_CLOSED`, `_HALF` or `_OPEN`)
    s16            shotTicks;       // Ticks spent playing clip 1; the sixteenth fires the revolver's sound and muzzle flash. Never reset
} _RupertWork;
STATIC_ASSERT_SIZEOF(_RupertWork, 0x4D4);

static void _modelPlacementAttachPartTask(Task* childTask);
static void _modelPlacementMirrorParentDrawFlags(Task* childTask);
static void _helicopterDepartureTickRupert(Task* task);
static void _helicopterDepartureTickRupertBlink(Task* task);
static void _helicopterDepartureIdleRupertRevolver(Task* task);
static void _helicopterDepartureInitRupert(Task* task);
static void _helicopterDepartureBindRupertLighting(Task* task);
static void _helicopterDepartureTickHelicopterSearchlight(Task* task);
static void _helicopterDepartureDrawHelicopterSearchlightGlow(Task* task, const CVECTOR* centerColor, const u8* rimRgb);
static void _helicopterDepartureInitHelicopter(Task* task);
static void _helicopterDepartureTickHelicopter(Task* task);
static void _helicopterDepartureKillHelicopter(Task* task);
static void _helicopterDepartureAttachHelicopterRotor(Task* task);
static void _helicopterDepartureTickHelicopterRotor(Task* task);
static void _helicopterDepartureKillHelicopterRotor(Task* task);
static void _helicopterDepartureAttachHelicopterSearchlight(Task* task);
static void _helicopterDepartureKillHelicopterSearchlight(Task* task);
static void _helicopterDeparturePoseHelicopterSequenceFrame(Task* task, const SVECTOR* rotations, const SVECTOR* translations, s32 frameIndex);
static void _helicopterDeparturePlaceHelicopterPart(Task* task);
static void _helicopterDepartureBindHelicopterLighting(Task* task);
static void _helicopterDepartureSpawnNo9Golem(Enemy* enemy, Task* task);
static void _helicopterDepartureTickNo9Golem(Enemy* enemy, Task* task);
static void _helicopterDepartureAttachNo9Sword(Enemy* enemy, Task* task);
static void _helicopterDepartureTickNo9Sword(Enemy* enemy, Task* task);
static void _helicopterDepartureAttachNo9ChestPiece(Enemy* enemy, Task* task);
static void _helicopterDepartureTickNo9ChestPiece(Enemy* enemy, Task* task);
static void _helicopterDepartureAttachNo9Prop(Enemy* enemy, Task* task);
static void _helicopterDepartureTickNo9Prop(Enemy* enemy, Task* task);

/// Rupert's revolver states: 0 attach to his right hand, 1 idle, 2 kill.
static const TaskFuncTable3 _gHelicopterDepartureRupertRevolverStates = {
    _modelPlacementAttachPartTask,
    _helicopterDepartureIdleRupertRevolver,
    taskKill,
};

/// Rupert's radio states: 0 attach to his left hand, 1 mirror his drawing, 2 kill.
static const TaskFuncTable3 _gHelicopterDepartureRupertRadioStates = {
    _modelPlacementAttachChild,
    _modelPlacementMirrorParentDrawFlags,
    taskKill,
};

/// Rupert's body states: 0 initialize work and children, 1 update, 2 enemy exit.
static const TaskFuncTable3 _gHelicopterDepartureRupertBodyStates = {
    _helicopterDepartureInitRupert,
    _helicopterDepartureTickRupert,
    enemyTaskExit,
};

/// Helicopter hull states: 0 initialize work and pose, 1 update, 2 kill.
static const TaskFuncTable3 _gHelicopterDepartureHelicopterStates = {
    _helicopterDepartureInitHelicopter,
    _helicopterDepartureTickHelicopter,
    _helicopterDepartureKillHelicopter,
};

/// Main and tail rotor states: 0 attach to the hull, 1 spin, 2 kill.
static const TaskFuncTable3 _gHelicopterDepartureHelicopterRotorStates = {
    _helicopterDepartureAttachHelicopterRotor,
    _helicopterDepartureTickHelicopterRotor,
    _helicopterDepartureKillHelicopterRotor,
};

/// Helicopter searchlight states: 0 attach, 1 aim and draw the glow, 2 kill.
static const TaskFuncTable3 _gHelicopterDepartureHelicopterSearchlightStates = {
    _helicopterDepartureAttachHelicopterSearchlight,
    _helicopterDepartureTickHelicopterSearchlight,
    _helicopterDepartureKillHelicopterSearchlight,
};

/// No. 9 GOLEM body states: 0 initialize work and children, 1 update, 2 destroy.
static const EnemyTaskFuncTable3 _gHelicopterDepartureNo9GolemStates = {
    _helicopterDepartureSpawnNo9Golem,
    _helicopterDepartureTickNo9Golem,
    enemyDestroy,
};

static TmdSource _gRupertBody2;
static TmdSource _gRupertMongoose;
static TmdSource _gRupertRadio;
static s32       _helicopterDeparturePlayRupertAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32       _helicopterDepartureSetRupertModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32       _helicopterDepartureApplyRupertCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static s32       _helicopterDepartureSetRupertEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedArg);
static void      _helicopterDepartureRupertRevolverTask(Task* task);
static void      _helicopterDepartureRupertRadioTask(Task* task);
static void      _helicopterDepartureRupertBodyTask(Task* task);

static AnimationSet _gAkropolisHelicopterLandingPadRupertShootClip;
static AnimationSet _gAkropolisHelicopterLandingPadRupertArriveClip;
static AnimationSet _gAkropolisHelicopterLandingPadRupertDetonatorTalkClip;

static s32  _helicopterDepartureRestartHelicopterSequence(Task* task, s32 messageId, s32 unusedRequest, s32 unusedArg);
static s32  _helicopterDeparturePlaceHelicopter(Task* task, s32 messageId, const ActorTransform* transform, s32 unusedArg);
static s32  _helicopterDepartureSetHelicopterModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArgument);
static s32  _helicopterDeparturePlayNo9GolemAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _helicopterDepartureSetNo9GolemModelDraw(Task* task, s32 messageId, s32 drawFlags, s32 unusedArg);
static void _helicopterDepartureNo9GolemTask(Task* task);
static void _helicopterDepartureNo9SwordTask(Task* task);
static void _helicopterDepartureNo9ChestPieceTask(Task* task);
static void _helicopterDepartureNo9PropTask(Task* task);

static TmdSource _gHelicopterDepartureHelicopterHullModel;
static TmdSource _gAkropolisHelicopterLandingPadMainRotor;
static TmdSource _gAkropolisHelicopterLandingPadTailRotor;
static TmdSource _gAkropolisHelicopterLandingPadSearchlight;
static void      _helicopterDepartureHelicopterTask(Task* task);
static void      _helicopterDepartureHelicopterRotorTask(Task* task);
static void      _helicopterDepartureHelicopterSearchlightTask(Task* task);

#include "assets/rooms/akropolis/helicopter_landing_pad/helicopter_departure_clips_1.inc"

TaskDesc gHelicopterDepartureHelicopterTaskDescriptors[4] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _helicopterDepartureHelicopterTask, { .model = &_gHelicopterDepartureHelicopterHullModel } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _helicopterDepartureHelicopterRotorTask, { .model = &_gAkropolisHelicopterLandingPadMainRotor } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _helicopterDepartureHelicopterRotorTask, { .model = &_gAkropolisHelicopterLandingPadTailRotor } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _helicopterDepartureHelicopterSearchlightTask, { .model = &_gAkropolisHelicopterLandingPadSearchlight } },
};

#include "assets/rooms/akropolis/helicopter_landing_pad/helicopter_hull.inc"

#include "assets/rooms/akropolis/helicopter_landing_pad/main_rotor.inc"

#include "assets/rooms/akropolis/helicopter_landing_pad/tail_rotor.inc"

#include "assets/rooms/akropolis/helicopter_landing_pad/searchlight.inc"

#include "assets/characters/rupert/body_2.inc"

#include "assets/characters/rupert/mongoose.inc"

#include "assets/characters/rupert/radio.inc"

#include "assets/rooms/akropolis/helicopter_landing_pad/helicopter_departure_clips_2.inc"

/// Rupert's eye band in his face texture: its first row, its width in 16-bit VRAM words and its height in rows.
enum {
    EYE_TEXTURE_Y_ROWS      = 40,
    EYE_TEXTURE_WIDTH_WORDS = 24,
    EYE_TEXTURE_HEIGHT_ROWS = 16,
};

/// The parts of Rupert's body his shadow and lighting follow and his revolver and radio ride.
enum {
    RUPERT_PELVIS_PART     = 1,
    RUPERT_RIGHT_HAND_PART = 8,  // The Mongoose revolver
    RUPERT_LEFT_HAND_PART  = 12, // The radio
};

/// Packed open-eye pixels for Rupert's face texture.
static u_long _gHelicopterDepartureRupertEyesOpenPixels[EYE_TEXTURE_WIDTH_WORDS * EYE_TEXTURE_HEIGHT_ROWS / 2] = {
#include "assets/characters/rupert/eyes_open.inc"
};

/// Writable COPY/END list restoring Rupert's open-eye texture patch.
static GpuImageUpload _gHelicopterDepartureRupertEyesOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, EYE_TEXTURE_WIDTH_WORDS, EYE_TEXTURE_HEIGHT_ROWS }, _gHelicopterDepartureRupertEyesOpenPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Packed half-open-eye pixels for Rupert's face texture.
static u_long _gHelicopterDepartureRupertEyesHalfOpenPixels[EYE_TEXTURE_WIDTH_WORDS * EYE_TEXTURE_HEIGHT_ROWS / 2] = {
#include "assets/characters/rupert/eyes_half_open.inc"
};

/// Writable COPY/END list for Rupert's half-open blink image.
static GpuImageUpload _gHelicopterDepartureRupertEyesHalfOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, EYE_TEXTURE_WIDTH_WORDS, EYE_TEXTURE_HEIGHT_ROWS }, _gHelicopterDepartureRupertEyesHalfOpenPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Packed closed-eye pixels for Rupert's face texture.
static u_long _gHelicopterDepartureRupertEyesClosedPixels[EYE_TEXTURE_WIDTH_WORDS * EYE_TEXTURE_HEIGHT_ROWS / 2] = {
#include "assets/characters/rupert/eyes_closed.inc"
};

/// Writable COPY/END list for Rupert's closed-eye image.
static GpuImageUpload _gHelicopterDepartureRupertEyesClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, EYE_TEXTURE_WIDTH_WORDS, EYE_TEXTURE_HEIGHT_ROWS }, _gHelicopterDepartureRupertEyesClosedPixels },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Rupert's bank-0 clip sources, with NULL at 0 and loaded sets at IDs 1..3.
#include "assets/rooms/akropolis/helicopter_landing_pad/rupert_bank.inc"

/// Animation-bank selector for Rupert's body; only bank index 0 is valid.
static AnimationSet** _gHelicopterDepartureRupertAnimationBanks[1] = {
    _gHelicopterDepartureRupertAnimationSets,
};

TaskDesc gHelicopterDepartureRupertTaskDescriptors[3] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _helicopterDepartureRupertBodyTask, { .model = &_gRupertBody2 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _helicopterDepartureRupertRevolverTask, { .model = &_gRupertMongoose } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _helicopterDepartureRupertRadioTask, { .model = &_gRupertRadio } },
};

/// Terminated message table for Rupert's animation, placement, drawing and props.
static TaskMessageEntry _gHelicopterDepartureRupertMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _helicopterDeparturePlayRupertAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _helicopterDepartureSetRupertModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _helicopterDepartureApplyRupertCommand },
    { ACTOR_MESSAGE_SET_TEXTURE_MODE, _helicopterDepartureSetRupertEyes },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Muzzle-flash position relative to Rupert's revolver root, in game units.
static SVECTOR _gHelicopterDepartureRupertMuzzleFlashOffset = { -30, 440, 140, 0 };

/// Synchronized hull, camera and searchlight frames of the view-24 sequence.
enum { HELICOPTER_SEQUENCE_FRAME_COUNT = 120 };

/// Helicopter hull Euler rotations for sequence frames 0..119.
static SVECTOR _gHelicopterDepartureHelicopterSequenceRotations[HELICOPTER_SEQUENCE_FRAME_COUNT] = {
#include "assets/rooms/akropolis/helicopter_landing_pad/helicopter_rotation_track.inc"
};

/// Helicopter hull positions for sequence frames 0..119, in parent game units.
static SVECTOR _gHelicopterDepartureHelicopterSequenceTranslations[HELICOPTER_SEQUENCE_FRAME_COUNT] = {
#include "assets/rooms/akropolis/helicopter_landing_pad/helicopter_translation_track.inc"
};

/// Searchlight Euler aim relative to the hull for sequence frames 0..119.
static SVECTOR _gHelicopterDepartureHelicopterSearchlightRotations[HELICOPTER_SEQUENCE_FRAME_COUNT] = {
#include "assets/rooms/akropolis/helicopter_landing_pad/searchlight_rotation_track.inc"
};

/// Lit red-ramp endpoint of the helicopter's sixteen-colour flashing palette.
static u8 _gHelicopterDepartureHelicopterFlashLitPalette[PALETTE_BYTES] = {
#include "assets/rooms/akropolis/helicopter_landing_pad/helicopter_palette_lit.inc"
};

/// Absolute VRAM row rewritten by the helicopter's flashing palette.
enum { HELICOPTER_FLASH_CLUT_ROW = 264 };

/// Writable COPY/END list for the helicopter's flashing VRAM palette row.
static GpuImageUpload _gHelicopterDepartureHelicopterFlashPaletteUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, HELICOPTER_FLASH_CLUT_ROW, PALETTE_COLORS, 1 }, (u_long*)_gHelicopterDepartureHelicopterFlashLitPalette },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Dark endpoint of the helicopter's sixteen-colour flashing palette.
static u8 _gHelicopterDepartureHelicopterFlashDarkPalette[PALETTE_BYTES] = {
#include "assets/rooms/akropolis/helicopter_landing_pad/helicopter_palette_dark.inc"
};

/// Camera frames synchronized with the helicopter's view-24 hull and lamp poses.
static ViewCamera _gHelicopterDepartureHelicopterSequenceCameras[HELICOPTER_SEQUENCE_FRAME_COUNT] = {
#include "assets/rooms/akropolis/helicopter_landing_pad/helicopter_camera_track.inc"
};

/// Terminated helicopter messages for sequence restart, placement and drawing.
static TaskMessageEntry _gHelicopterDepartureHelicopterMessages[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _helicopterDepartureRestartHelicopterSequence },
    { ACTOR_MESSAGE_PLACE, _helicopterDeparturePlaceHelicopter },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _helicopterDepartureSetHelicopterModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Hull-relative translations of helicopter parts, in game units, by their spawn argument.
static SVECTOR _gHelicopterDepartureHelicopterPartOffsets[4] = {
    { 0, -0x3A98, 0, 0 },    // No part spawns with argument 0
    { 0, -1350, 1360, 0 },   // The main rotor
    { 340, -230, -5790, 0 }, // The tail rotor
    { 0, 1520, 50, 0 },      // The searchlight
};

/// Searchlight glow centre colours at frame distances 0..3 from peak frame 89.
static CVECTOR _gHelicopterDepartureHelicopterSearchlightGlowColors[4] = {
    { 255, 255, 255, 0 },
    { 200, 200, 200, 0 },
    { 150, 150, 150, 0 },
    { 60, 60, 60, 0 },
};

/// One unit of a searchlight rim direction in signed 4.12 fixed point.
enum { SEARCHLIGHT_DIRECTION_ONE = 4096 };

/// Sixteen equally spaced screen-XY unit directions for the searchlight glow rim.
static DVECTOR _gHelicopterDepartureHelicopterSearchlightRimDirections[16] = {
    { SEARCHLIGHT_DIRECTION_ONE, 0 },
    { 3784, 1567 },
    { 2896, 2896 },
    { 1567, 3784 },
    { 0, SEARCHLIGHT_DIRECTION_ONE },
    { -1567, 3784 },
    { -2896, 2896 },
    { -3784, 1567 },
    { -SEARCHLIGHT_DIRECTION_ONE, 0 },
    { -3784, -1567 },
    { -2896, -2896 },
    { -1567, -3784 },
    { 0, -SEARCHLIGHT_DIRECTION_ONE },
    { 1567, -3784 },
    { 2896, -2896 },
    { 3784, -1567 },
};

/// Child entries in `gHelicopterDepartureHelicopterTaskDescriptors`, excluding hull 0.
enum {
    HELICOPTER_MAIN_ROTOR_DESCRIPTOR  = 1,
    HELICOPTER_TAIL_ROTOR_DESCRIPTOR  = 2,
    HELICOPTER_SEARCHLIGHT_DESCRIPTOR = 3,
};

/// Descriptor indices spawned on the helicopter hull's first show request.
static s32 _gHelicopterDepartureHelicopterChildDescriptorIndices[3] = {
    HELICOPTER_MAIN_ROTOR_DESCRIPTOR,
    HELICOPTER_TAIL_ROTOR_DESCRIPTOR,
    HELICOPTER_SEARCHLIGHT_DESCRIPTOR,
};

#include "assets/enemies/golem/no_9/body_akropolis.inc"

#include "assets/enemies/golem/no_9/akropolis_sword.inc"

#include "assets/enemies/golem/no_9/akropolis_prop.inc"

#include "assets/enemies/golem/no_9/akropolis_chest_piece.inc"

#include "assets/rooms/akropolis/helicopter_landing_pad/helicopter_departure_clips_3.inc"

TaskDesc gHelicopterDepartureNo9GolemTaskDescriptors[4] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _helicopterDepartureNo9GolemTask, { .model = &gGolemNo9BodyAkropolis } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _helicopterDepartureNo9SwordTask, { .model = &gGolemNo9AkropolisSword } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _helicopterDepartureNo9ChestPieceTask, { .model = &gGolemNo9AkropolisChestPiece } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _helicopterDepartureNo9PropTask, { .model = &gGolemNo9AkropolisProp } },
};

/// No. 9 GOLEM playback, matrix-placement and draw messages, with an end marker.
static TaskMessageEntry _gHelicopterDepartureNo9GolemMessages[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _helicopterDeparturePlayNo9GolemAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _helicopterDepartureSetNo9GolemModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// No. 9 GOLEM's fixed clip bank: zero is absent, clips 1..3 drive slots 1..18.
#include "assets/rooms/akropolis/helicopter_landing_pad/no_9_bank.inc"

static void _helicopterDepartureTickHelicopterPaletteFlash(_HelicopterWork* work);

/// Advances Rupert's animation, shot cues, lighting, blink and buffer-release delay.
static void _helicopterDepartureTickRupert(Task* task)
{
    enum {
        RUPERT_SHOT_CLIP = 1,
        RUPERT_SHOT_TICK = 16,
        // A bank-type 3 entry of bank 0x13A, which no bank on the disc is: the only type-3 bank is the roof garden's
        // (0x3108), so this request finds nothing to play.
        SOUND_RUPERT_SHOT = 0x313A0003,
        SHADOW_HALF_SIZE  = 0x300,
        LIGHT_COUNT       = 3,
    };

    _RupertWork* work;
    TmdObject*   model;
    GfxCoord*    coord;
    GfxCoord*    gunCoord;
    VECTOR3      groundPoint;
    s32          slotIndex;
    s32          audioPan;

    model = task->extra.tmd;
    work  = task->work;
    coord = &model->coords[RUPERT_PELVIS_PART];
    if (work->ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        if (work->animId == RUPERT_SHOT_CLIP) {
            if (++work->shotTicks == RUPERT_SHOT_TICK) {
                gunCoord = work->gunTask->extra.tmd->coords;
                audioPan = (s8)worldCoordGetOriginAudioPan(gunCoord);
                sndEvtRequestScriptStart(SOUND_RUPERT_SHOT, audioPan, (s8)worldCoordGetOriginAudioDepth(gunCoord));
                effectSpawn(EFFECT_ACTOR_MUZZLE_FLASH, gunCoord, 0, &_gHelicopterDepartureRupertMuzzleFlashOffset);
            }
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[RUPERT_PELVIS_PART].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        worldCoordSetModelLighting(model, coord->workm.t, 0, LIGHT_COUNT);
    }
    _helicopterDepartureTickRupertBlink(task);
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Posts the next timed blink image and restarts its display interval.
static inline void _helicopterDepartureAdvanceRupertBlinkImage(Task* task, _RupertWork* work,
                                                               GpuImageUpload* uploadList, const RECT* eyeRect)
{
    actorRenderUploadTexture(task, uploadList, eyeRect);
    work->blinkCountdown = work->blinkFrameDelay;
    work->blinkStep      = work->blinkStep + 1;
}

/// Advances Rupert's closed, half-open and open eye sequence by one tick.
static void _helicopterDepartureTickRupertBlink(Task* task)
{
    _RupertWork* work;
    RECT         eyeRect;

    work      = task->work;
    eyeRect.x = 0;
    eyeRect.y = EYE_TEXTURE_Y_ROWS;
    eyeRect.w = EYE_TEXTURE_WIDTH_WORDS;
    eyeRect.h = EYE_TEXTURE_HEIGHT_ROWS;

    switch (work->blinkStep) {
        case BLINK_CLOSED:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                _helicopterDepartureAdvanceRupertBlinkImage(task, work, &_gHelicopterDepartureRupertEyesClosedUploads[0], &eyeRect);
            }
            break;
        case BLINK_HALF:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                _helicopterDepartureAdvanceRupertBlinkImage(task, work, &_gHelicopterDepartureRupertEyesHalfOpenUploads[0], &eyeRect);
            }
            break;
        case BLINK_OPEN:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(task, &_gHelicopterDepartureRupertEyesOpenUploads[0], &eyeRect);
                work->blinkStep = BLINK_NONE;
            }
            break;
    }
}

/// Runs Rupert Broderick's revolver attachment states.
static void _helicopterDepartureRupertRevolverTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gHelicopterDepartureRupertRevolverStates;
    states.funcs[task->state](task);
}

#include "../../../../shared/actor/model_placement_attach_part.inc.c"

/// Leaves the revolver attachment unchanged between attachment and teardown.
static void _helicopterDepartureIdleRupertRevolver(Task* task)
{
}

/// Runs the states of Rupert's radio.
static void _helicopterDepartureRupertRadioTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gHelicopterDepartureRupertRadioStates;
    states.funcs[task->state](task);
}

#include "../../../../shared/actor/model_placement_attach.inc.c"

#include "../../../../shared/actor/model_placement_mirror_parent.inc.c"

/// Runs Rupert Broderick's body initialization, frame update or teardown.
static void _helicopterDepartureRupertBodyTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gHelicopterDepartureRupertBodyStates;
    states.funcs[task->state](task);
}

/// Initializes Rupert's hidden body and his revolver and radio.
static void _helicopterDepartureInitRupert(Task* task)
{
    enum {
        NO_BUFFER_RELEASE        = -1,
        GUN_DESCRIPTOR           = 1,
        RADIO_DESCRIPTOR         = 2,
        INITIAL_SHADOW_HALF_SIZE = 0x200,
    };

    _RupertWork* work;
    TmdObject*   model;
    VECTOR3      groundPoint;
    u16          flags;

    model = task->extra.tmd;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work          = work;
    work->animId        = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->bank          = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->shotTicks     = 0;
    work->freeCountdown = NO_BUFFER_RELEASE;
    flags               = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    model->flags        = flags;
    // The target retains this shadow branch after setting the hidden flag.
    if (!(flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[RUPERT_PELVIS_PART].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, INITIAL_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    work->gunTask   = taskSpawnFromTable(gHelicopterDepartureRupertTaskDescriptors, GUN_DESCRIPTOR, RUPERT_RIGHT_HAND_PART, task);
    work->radioTask = taskSpawnFromTable(gHelicopterDepartureRupertTaskDescriptors, RADIO_DESCRIPTOR, RUPERT_LEFT_HAND_PART, task);
    _helicopterDepartureBindRupertLighting(task);
    task->msgTable     = _gHelicopterDepartureRupertMessages;
    task->exitCallback = enemyTaskExit;
    task->state++;
}

/// Binds Rupert's body lighting matrices and relights him at the pelvis.
static void _helicopterDepartureBindRupertLighting(Task* task)
{
    enum { COUNT = 3 };
    _RupertWork* work;
    GfxCoord*    partCoords;
    TmdObject*   model;

    work            = task->work;
    model           = task->extra.tmd;
    partCoords      = model->coords;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;

    partCoords[RUPERT_PELVIS_PART].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&partCoords[RUPERT_PELVIS_PART]);
    worldCoordSetModelLighting(model, partCoords[RUPERT_PELVIS_PART].workm.t, 0, COUNT);
}

/// Reseeds Rupert's driven slots, updates their poses and enables ticking.
static inline void _helicopterDepartureReseedRupertAnimation(_RupertWork* work, const AnimationPlayRequest* request)
{
    enum { RUPERT_BLEND_FRAMES = 6 };
    s32 slotIndex;

    work->animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, RUPERT_BLEND_FRAMES);
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
static s32 _helicopterDeparturePlayRupertAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _RupertWork* work;
    TmdObject*   model;

    work  = task->work;
    model = task->extra.tmd;
    // A new bank invalidates the selected clip before its pose is seeded.
    if (request->source.index != work->bank) {
        work->bank   = request->source.index;
        work->animId = ACTOR_MOTION_ANIMATION_INDEX_NONE;
        animationInitContext(&work->rig.anim, _gHelicopterDepartureRupertAnimationBanks[work->bank], model, work->rig.poses,
                             work->rig.slots);
    }
    if (request->animationId != work->animId) {
        _helicopterDepartureReseedRupertAnimation(work, request);
    }
    return 0;
}

#include "../../../../shared/actor/actor_messages_place_euler.inc.c"

/// Changes Rupert's drawing and primitive-buffer policy.
static s32 _helicopterDepartureSetRupertModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    TmdObject*   model;
    _RupertWork* work;
    s32          result;

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
        case ACTOR_MESSAGE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Shows Rupert's revolver, or hides it and shows his radio, for an actor command.
static s32 _helicopterDepartureApplyRupertCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum { REVOLVER = 0,
           RADIO    = 1 };
    _RupertWork* work;
    Task*        childTask;
    u16          commandId;

    commandId = command->command;
    work      = task->work;

    switch (commandId) {
        case REVOLVER:
            childTask = work->gunTask;
            if (childTask != NULL) {
                childTask->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case RADIO:
            childTask = work->gunTask;
            if (childTask != NULL) {
                childTask->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            childTask = work->radioTask;
            if (childTask != NULL) {
                childTask->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
    }
    return 0;
}

/// Sets Rupert's eye image or starts a timed blink for `ACTOR_MESSAGE_SET_TEXTURE_MODE`.
static s32 _helicopterDepartureSetRupertEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedArg)
{
    enum { EYES_OPEN           = 0,
           EYES_CLOSED         = 1,
           EYES_OPEN_ALTERNATE = 2,
           EYES_BLINK          = 3,
           BLINK_FRAME_DELAY   = 1 };
    RECT            eyeRect;
    GpuImageUpload* uploadList;
    s32             result;

    result    = 0;
    eyeRect.x = 0;
    eyeRect.y = EYE_TEXTURE_Y_ROWS;
    eyeRect.w = EYE_TEXTURE_WIDTH_WORDS;
    eyeRect.h = EYE_TEXTURE_HEIGHT_ROWS;

    switch (eyeMode) {
        case EYES_CLOSED:
            uploadList = &_gHelicopterDepartureRupertEyesClosedUploads[0];
            break;
        case EYES_OPEN:
        case EYES_OPEN_ALTERNATE:
            uploadList = &_gHelicopterDepartureRupertEyesOpenUploads[0];
            break;
        case EYES_BLINK: {
            _RupertWork* stepWork;
            _RupertWork* delayWork;

            stepWork                   = task->work;
            stepWork->blinkStep        = BLINK_CLOSED;
            delayWork                  = task->work;
            delayWork->blinkFrameDelay = BLINK_FRAME_DELAY;
            uploadList                 = &_gHelicopterDepartureRupertEyesHalfOpenUploads[0];
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
static void _helicopterDepartureTickHelicopterSearchlight(Task* task)
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
    if (gGameSession->location.view == SEQUENCE_VIEW) {
        root->param.rot.vx = _gHelicopterDepartureHelicopterSearchlightRotations[parentTask->killCountdown].vx;
        root->param.rot.vy = _gHelicopterDepartureHelicopterSearchlightRotations[parentTask->killCountdown].vy;
        root->param.rot.vz = _gHelicopterDepartureHelicopterSearchlightRotations[parentTask->killCountdown].vz;
        RotMatrix(&root->param.rot, &root->coord);
        root->composeStamp = GRAPHICS_COORD_DIRTY;

        frameDistance = parentTask->killCountdown - GLOW_PEAK_FRAME;
        if (frameDistance < 0) {
            frameDistance = GLOW_PEAK_FRAME - parentTask->killCountdown;
        }
        if (frameDistance < ARRAY_SIZE(_gHelicopterDepartureHelicopterSearchlightGlowColors)) {
            colorTable  = _gHelicopterDepartureHelicopterSearchlightGlowColors;
            centerColor = &colorTable[frameDistance];
            rimRgb[0]   = centerColor->r - RIM_SHADE_DROP;
            rimRgb[1]   = centerColor->g - RIM_SHADE_DROP;
            rimRgb[2]   = centerColor->b - RIM_SHADE_DROP;
            _helicopterDepartureDrawHelicopterSearchlightGlow(task, centerColor, rimRgb);
        }
    }
}

/// Draws the helicopter searchlight's additive, 300-pixel-radius screen glow.
static void _helicopterDepartureDrawHelicopterSearchlightGlow(Task* task, const CVECTOR* centerColor, const u8* rimRgb)
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

/// Allocates and links one semitransparent Gouraud triangle in the searchlight fan.
#define EMIT_SEARCHLIGHT_TRIANGLE(endPoint) \
    ({                                      \
        triangle       = gGpuPrimCursor;    \
        gGpuPrimCursor = triangle + 1;      \
        setPolyG3(triangle);                \
        setSemiTrans(triangle, 1);          \
        triangle->x0 = centerX;             \
        triangle->y0 = centerY;             \
        triangle->x1 = rimPoint->vx;        \
        triangle->y1 = rimPoint->vy;        \
        triangle->x2 = (endPoint).vx;       \
        triangle->y2 = (endPoint).vy;       \
        triangle->r0 = centerColor->r;      \
        triangle->g0 = centerColor->g;      \
        triangle->b0 = centerColor->b;      \
        triangle->r1 = rimRgb[0];           \
        triangle->g1 = rimRgb[1];           \
        triangle->b1 = rimRgb[2];           \
        triangle->r2 = rimRgb[0];           \
        triangle->g2 = rimRgb[1];           \
        triangle->b2 = rimRgb[2];           \
        addPrim(orderingTag, triangle);     \
    })

    // Project the lamp origin; the halo radius stays fixed in screen pixels.
    gfxComposeNodeWorldTransform(task->extra.tmd->coords, &worldTransform, &worldOrigin);
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    sortDepth    = RotTransPers(&worldOrigin, &screenCenter, &projectionScale, &projectionFlags);
    centerX      = screenCenter;
    centerY      = screenCenter >> 16;
    unitOffset   = _gHelicopterDepartureHelicopterSearchlightRimDirections;
    rimPoint     = screenRim;
    radiusPixels = RADIUS_PIXELS;
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(screenRim); pointIndex++) {
        rimPoint->vx = centerX + radiusPixels * unitOffset->vx / UNIT_OFFSET_SCALE;
        rimPoint->vy = centerY + radiusPixels * unitOffset->vy / UNIT_OFFSET_SCALE;
        rimPoint++;
        unitOffset++;
    }
    // Pack the fan first, then prepend its additive draw mode to the same OT chain.
    orderingTag = GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(sortDepth) + OT_DISPLACEMENT;
    rimPoint    = screenRim;
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(screenRim) - 1; pointIndex++, rimPoint++) {
        EMIT_SEARCHLIGHT_TRIANGLE(rimPoint[1]);
    }
    EMIT_SEARCHLIGHT_TRIANGLE(screenRim[0]);
#undef EMIT_SEARCHLIGHT_TRIANGLE
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, 1, 0, ADDITIVE_DRAW_PAGE);
    addPrim(orderingTag, drawMode);
}

/// Blends one little-endian RGB555 palette colour through the GTE.
static inline void _helicopterDepartureBlendPaletteColor(u8* destination, const u8* firstColor, const u8* secondColor, s32 firstWeight, s32 secondWeight)
{
    enum { PALETTE_CHANNEL_MASK = 0x1F };
    CVECTOR channels[3];
    s32     packedColor;

    packedColor   = firstColor[0] | (firstColor[1] << 8);
    channels[0].r = ((u16)packedColor >> 10) & PALETTE_CHANNEL_MASK;
    channels[0].g = ((u16)packedColor >> 5) & PALETTE_CHANNEL_MASK;
    channels[0].b = packedColor & PALETTE_CHANNEL_MASK;
    packedColor   = secondColor[0] | (secondColor[1] << 8);
    channels[1].r = ((u16)packedColor >> 10) & PALETTE_CHANNEL_MASK;
    channels[1].g = ((u16)packedColor >> 5) & PALETTE_CHANNEL_MASK;
    channels[1].b = packedColor & PALETTE_CHANNEL_MASK;
    LoadAverageCol((u8*)&channels[0], (u8*)&channels[1], firstWeight, secondWeight, (u8*)&channels[2]);
    packedColor    = channels[2].b + ((channels[2].r << 10) + (channels[2].g << 5));
    destination[1] = (u32)packedColor >> 8;
    destination[0] = packedColor;
}

/// Advances and uploads the helicopter's sixteen-colour palette flash.
static void _helicopterDepartureTickHelicopterPaletteFlash(_HelicopterWork* work)
{
    enum { PALETTE_FADE_STEP = 0x555 };
    s32 byteIndex;
    s32 litWeight;
    s32 darkWeight;

    // Reach the dark palette, hold it, then snap back for the next flash.
    if (work->paletteHold != 0) {
        work->paletteFade += PALETTE_FADE_STEP;
        if (work->paletteFade >= PALETTE_FADE_FULL) {
            work->paletteFade = PALETTE_FADE_FULL;
            if (--work->paletteHold < 0) {
                work->paletteHold = 0;
            }
        }
    } else {
        work->paletteFade -= PALETTE_FADE_FULL;
        if (work->paletteFade <= 0) {
            work->paletteFade = 0;
            work->paletteHold = PALETTE_HOLD_TICKS;
        }
    }
    litWeight  = PALETTE_FADE_FULL - work->paletteFade;
    darkWeight = work->paletteFade;
    // Keep byte-pair accesses for the packed palette and publish the complete row.
    byteIndex = 0;
    do {
        _helicopterDepartureBlendPaletteColor(&work->palette[byteIndex], &_gHelicopterDepartureHelicopterFlashLitPalette[byteIndex], &_gHelicopterDepartureHelicopterFlashDarkPalette[byteIndex], litWeight, darkWeight);
        byteIndex += 2;
    } while (byteIndex < ARRAY_SIZE(work->palette));
    gpuUploadImages(&_gHelicopterDepartureHelicopterFlashPaletteUploads[0]);
}

/// Initializes the helicopter's owned work, hidden hull and first sequence pose.
static void _helicopterDepartureInitHelicopter(Task* task)
{
    enum { HELICOPTER_FREE_NONE = -1 };
    _HelicopterWork* work;
    TmdObject*       model;

    model = task->extra.tmd;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work          = work;
    work->freeCountdown = HELICOPTER_FREE_NONE;
    model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    _helicopterDeparturePoseHelicopterSequenceFrame(task, _gHelicopterDepartureHelicopterSequenceRotations, _gHelicopterDepartureHelicopterSequenceTranslations, 0);
    _helicopterDepartureBindHelicopterLighting(task);
    // The GPU API consumes words; the fade writes little-endian byte pairs.
    task->msgTable                                               = _gHelicopterDepartureHelicopterMessages;
    _gHelicopterDepartureHelicopterFlashPaletteUploads[0].pixels = (u_long*)work->palette;
    task->state                                                 += 1;
}

/// Advances the helicopter's synchronized camera and hull-pose playback by one frame.
static inline void _helicopterDepartureAdvanceHelicopterSequence(Task* task, GfxCoord* rootCoord)
{
    s16 nextFrame;

    nextFrame           = task->killCountdown + 1;
    task->killCountdown = nextFrame;
    if (nextFrame >= (s32)ARRAY_SIZE(_gHelicopterDepartureHelicopterSequenceCameras)) {
        task->killCountdown = ARRAY_SIZE(_gHelicopterDepartureHelicopterSequenceCameras) - 1;
    }
    viewQueueCamera(&_gHelicopterDepartureHelicopterSequenceCameras[task->killCountdown]);
    _helicopterDeparturePoseHelicopterSequenceFrame(task, _gHelicopterDepartureHelicopterSequenceRotations, _gHelicopterDepartureHelicopterSequenceTranslations, task->killCountdown);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Updates the drawable helicopter's lighting and flash, and advances its scene pose.
static void _helicopterDepartureTickHelicopter(Task* task)
{
    enum { HELICOPTER_SEQUENCE_VIEW = 24 };
    _HelicopterWork* work;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    s32              freeCountdown;

    model     = task->extra.tmd;
    work      = task->work;
    rootCoord = model->coords;

    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        actorRenderComposeCoord(rootCoord);
        worldCoordSetModelLighting(model, rootCoord->workm.t, 0, ARRAY_SIZE(model->colorMtx->m[0]));
        _helicopterDepartureTickHelicopterPaletteFlash(task->work);
    }
    // Scene motion and camera playback continue even when the hull is hidden.
    if (gGameSession->location.view == HELICOPTER_SEQUENCE_VIEW) {
        _helicopterDepartureAdvanceHelicopterSequence(task, rootCoord);
    }
    // Reload after freeing, preserving the callback-visible countdown value.
    freeCountdown = work->freeCountdown;
    if (freeCountdown >= 0) {
        if (freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
            freeCountdown = work->freeCountdown;
        }
        work->freeCountdown = freeCountdown - 1;
    }
}

/// Begins default teardown of the helicopter task and its attached children.
static void _helicopterDepartureKillHelicopter(Task* task)
{
    taskKill(task);
}

/// Attaches a helicopter rotor to the hull and inherits its lighting and visibility.
static void _helicopterDepartureAttachHelicopterRotor(Task* task)
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
    _helicopterDeparturePlaceHelicopterPart(task);
    root->parent = parentRoot;
    taskReparent(parentTask, task);
    task->state += 1;
}

/// Spins a drawable helicopter's main or tail rotor and mirrors its visibility.
static void _helicopterDepartureTickHelicopterRotor(Task* task)
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
static void _helicopterDepartureKillHelicopterRotor(Task* task)
{
    taskKill(task);
}

/// Attaches the searchlight beneath the helicopter with the sequence's initial aim.
static void _helicopterDepartureAttachHelicopterSearchlight(Task* task)
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
    _helicopterDeparturePlaceHelicopterPart(task);
    root->parent = parentRoot;
    taskReparent(parentTask, task);
    root->param.rot.vx = _gHelicopterDepartureHelicopterSearchlightRotations[0].vx;
    root->param.rot.vy = _gHelicopterDepartureHelicopterSearchlightRotations[0].vy;
    root->param.rot.vz = _gHelicopterDepartureHelicopterSearchlightRotations[0].vz;
    RotMatrix(&root->param.rot, &root->coord);
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state       += 1;
}

/// Begins default teardown of the helicopter's attached searchlight.
static void _helicopterDepartureKillHelicopterSearchlight(Task* task)
{
    taskKill(task);
}

/// Resets the helicopter's sequence cursor to frame zero.
static s32 _helicopterDepartureRestartHelicopterSequence(Task* task, s32 messageId, s32 unusedRequest, s32 unusedArg)
{
    task->killCountdown = 0;
    return 0;
}

/// Places the helicopter's root model and enables its active drawing.
static s32 _helicopterDeparturePlaceHelicopter(Task* task, s32 messageId, const ActorTransform* transform, s32 unusedArg)
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

/// Selects hull drawing and buffer policy, spawning its three parts on the first show.
static s32 _helicopterDepartureSetHelicopterModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArgument)
{
    TmdObject*       model;
    _HelicopterWork* work;
    Task*            child;
    s32              result;
    s32              partIndex;

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
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = drawMode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    if (drawMode == ACTOR_MESSAGE_DRAW_SHOW && work->partsSpawned == 0) {
        for (partIndex = 1; partIndex < (s32)ARRAY_SIZE(_gHelicopterDepartureHelicopterChildDescriptorIndices) + 1; partIndex++) {
            child = taskSpawnFromTable(gHelicopterDepartureHelicopterTaskDescriptors, _gHelicopterDepartureHelicopterChildDescriptorIndices[partIndex - 1], partIndex, task);
            if (child != NULL) {
                child->extra.tmd->flags &= ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            }
        }
        work->partsSpawned = 1;
    }
    return result;
}

/// Applies a table frame's rotation and translation to the helicopter root.
static void _helicopterDeparturePoseHelicopterSequenceFrame(Task* task, const SVECTOR* rotations, const SVECTOR* translations, s32 frameIndex)
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
static void _helicopterDeparturePlaceHelicopterPart(Task* task)
{
    GfxCoord* root;

    root               = task->extra.tmd->coords;
    root->coord.t[0]   = _gHelicopterDepartureHelicopterPartOffsets[task->spawnArg1.value].vx;
    root->coord.t[1]   = _gHelicopterDepartureHelicopterPartOffsets[task->spawnArg1.value].vy;
    root->coord.t[2]   = _gHelicopterDepartureHelicopterPartOffsets[task->spawnArg1.value].vz;
    root->param.rot.vx = 0;
    root->param.rot.vy = 0;
    root->param.rot.vz = 0;
    RotMatrix(&root->param.rot, &root->coord);
    root->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Binds the helicopter hull's lighting matrices and relights it at the root.
static void _helicopterDepartureBindHelicopterLighting(Task* task)
{
    enum {
        HELICOPTER_LIGHT_COUNT = 3,
    };
    GfxCoord*        rootCoord;
    _HelicopterWork* work;
    TmdObject*       model;

    work                    = task->work;
    model                   = task->extra.tmd;
    rootCoord               = model->coords;
    model->lightMtx         = &work->light;
    model->colorMtx         = &work->color;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    worldCoordSetModelLighting(model, rootCoord->workm.t, 0, HELICOPTER_LIGHT_COUNT);
}

/// Runs the helicopter's initialization, frame update or teardown state.
static void _helicopterDepartureHelicopterTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gHelicopterDepartureHelicopterStates;
    states.funcs[task->state](task);
}

/// Runs the helicopter rotor states: 0 attach, 1 spin and 2 kill.
static void _helicopterDepartureHelicopterRotorTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gHelicopterDepartureHelicopterRotorStates;
    states.funcs[task->state](task);
}

/// Runs the helicopter searchlight states: 0 attach, 1 aim and glow, 2 kill.
static void _helicopterDepartureHelicopterSearchlightTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gHelicopterDepartureHelicopterSearchlightStates;
    states.funcs[task->state](task);
}

/// Initializes the No. 9 golem's hidden body, playback storage and three children.
static void _helicopterDepartureSpawnNo9Golem(Enemy* enemy, Task* task)
{
    enum { GOLEM_UPDATE_STATE = 1,
           NO_CLIP            = 0,
           PART8_CHILD        = 1,
           PART3_CHILD        = 2,
           PART12_CHILD       = 3 };
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    u8               view;
    u8               stage;
    AreaVariant*     layout;
    TmdObject*       model;
    GfxCoord*        bodyRoot;
    _No9Work*        work;
    TaskDesc*        childDescriptors;
    u32              placeIndex;
    u32              placeIndex2;
    AreaPlacement*   placements;
    Enemy*           spawned;
    GameSession*     session;

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
    animationInitContext(&work->rig.anim, _gHelicopterDepartureNo9GolemAnimationSets, model, work->rig.poses, work->rig.slots);
    work->animId           = NO_CLIP;
    task->msgTable         = _gHelicopterDepartureNo9GolemMessages;
    bodyRoot->composeStamp = GRAPHICS_COORD_DIRTY;

    childDescriptors = gHelicopterDepartureNo9GolemTaskDescriptors;
    spawned          = enemySpawnFromTable(childDescriptors, PART8_CHILD, 0, enemy);
    session          = gGameSession;
    // Child models inherit the texture relocation of the parent's placement.
    // The task address retains the spawn-result holder to preserve load order.
    spawned       = (Enemy*)spawned->task;
    sessionKey    = &session->location;
    placeIndex    = enemy->placeKey;
    stage         = sessionKey->stage;
    model         = ((Task*)spawned)->extra.tmd;
    key.stage     = stage;
    key.area      = sessionKey->area;
    key.roomState = sessionKey->roomState;
    view          = session->location.view;
    placeIndex    = placeIndex >> ENEMY_PLACE_INDEX_SHIFT;
    key.view      = view;
    areaSyncLocationVariant(&key);
    layout                   = areaGetVariant(&key);
    placements               = layout->placements;
    model->texturePageOffset = placements[placeIndex].texturePageOffset;
    model->clutRowOffset     = placements[placeIndex].clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    spawned       = enemySpawnFromTable(childDescriptors, PART3_CHILD, 0, enemy);
    session       = gGameSession;
    spawned       = (Enemy*)spawned->task;
    sessionKey    = &session->location;
    placeIndex2   = enemy->placeKey;
    stage         = sessionKey->stage;
    model         = ((Task*)spawned)->extra.tmd;
    key.stage     = stage;
    key.area      = sessionKey->area;
    key.roomState = sessionKey->roomState;
    view          = session->location.view;
    placeIndex2   = placeIndex2 >> ENEMY_PLACE_INDEX_SHIFT;
    key.view      = view;
    areaSyncLocationVariant(&key);
    layout                   = areaGetVariant(&key);
    placements               = layout->placements;
    model->texturePageOffset = placements[placeIndex2].texturePageOffset;
    model->clutRowOffset     = placements[placeIndex2].clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    enemySpawnFromTable(childDescriptors, PART12_CHILD, 0, enemy);
    task->state = GOLEM_UPDATE_STATE;
}

/// Advances the No. 9 golem's pose, lighting, ground shadow and exit fade.
static void _helicopterDepartureTickNo9Golem(Enemy* enemy, Task* task)
{
    enum { FIRST_EXIT_CLIP    = 3,
           FADE_PERIOD_TICKS  = 3,
           LAST_EXIT_TICK     = 96,
           DESTROY_STATE      = 2,
           SHADOW_HALF_SIZE   = 1024,
           FADE_NUMERATOR     = 15,
           FADE_FRACTION_BITS = 4 };
    TmdObject* model;
    VECTOR*    worldPosition;
    VECTOR*    groundPoint;
    _No9Work*  work;
    GfxCoord*  modelCoords;
    GfxCoord*  sampleCoord;
    s32        loopIndex;
    s32        column;
    s32        animationId;

/// Scales the golem's shared light and colour bases by 15/16 for one fade step.
#define FADE_GOLEM_LIGHTING()                                                                                                 \
    do {                                                                                                                      \
        for (loopIndex = 0; loopIndex < ARRAY_SIZE(work->color.m); loopIndex++) {                                             \
            for (column = 0; column < ARRAY_SIZE(work->color.m[loopIndex]); column++) {                                       \
                work->color.m[loopIndex][column] = (work->color.m[loopIndex][column] * FADE_NUMERATOR) >> FADE_FRACTION_BITS; \
                work->light.m[loopIndex][column] = (work->light.m[loopIndex][column] * FADE_NUMERATOR) >> FADE_FRACTION_BITS; \
            }                                                                                                                 \
        }                                                                                                                     \
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
    if (work->animId < FIRST_EXIT_CLIP) {
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
            effectDrawGroundShadow((VECTOR3*)groundPoint, SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    } else {
        if ((s16)(work->animTicks % FADE_PERIOD_TICKS) == 0) {
            FADE_GOLEM_LIGHTING();
#undef FADE_GOLEM_LIGHTING
        }
        if (work->animTicks > LAST_EXIT_TICK) {
            task->state = DESTROY_STATE;
        }
    }
    work->animTicks++;
    modelCoords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(2 * sizeof(*worldPosition));
}

/// Runs the No. 9 golem's initialization, frame update or destroy state.
static void _helicopterDepartureNo9GolemTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = _gHelicopterDepartureNo9GolemStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Restarts the No. 9 golem's body animation and exit timer.
static s32 _helicopterDeparturePlayNo9GolemAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _No9Work* work;
    s32       slotIndex;

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

#include "../../../../shared/actor/actor_messages_place_rot_matrix.inc.c"

/// Replaces the No. 9 golem body's drawing and automatic-buffer flags.
static s32 _helicopterDepartureSetNo9GolemModelDraw(Task* task, s32 messageId, s32 drawFlags, s32 unusedArg)
{
    enum { SKIP_AUTO_BUFFER = 1 << 1 };
    TmdObject* model;

    model = task->extra.tmd;
    if (!(drawFlags & ACTOR_MESSAGE_DRAW_SHOW)) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags = 0;
    }
    if (drawFlags & SKIP_AUTO_BUFFER) {
        task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Runs the states of the No. 9 golem's sword.
static void _helicopterDepartureNo9SwordTask(Task* task)
{
    EnemyTaskFunc states[2] = { _helicopterDepartureAttachNo9Sword, _helicopterDepartureTickNo9Sword };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the No. 9 golem's sword to its right hand and shows it with the body's lighting.
static void _helicopterDepartureAttachNo9Sword(Enemy* enemy, Task* task)
{
    enum { RIGHT_HAND_PART = 8,
           UPDATE_STATE    = 1 };
    Task*      parentTask;
    TmdObject* model;
    _No9Work*  parentWork;
    GfxCoord*  root;
    GfxCoord*  parentCoords;

    parentTask   = task->parent;
    model        = task->extra.tmd;
    parentCoords = parentTask->extra.tmd->coords;
    root         = model->coords;
    parentWork   = parentTask->work;

    root->parent    = &parentCoords[RIGHT_HAND_PART];
    model->lightMtx = &parentWork->light;
    model->flags    = 0;
    model->colorMtx = &parentWork->color;
    task->state     = UPDATE_STATE;
}

/// Composes the sword's attached transform each tick.
static void _helicopterDepartureTickNo9Sword(Enemy* enemy, Task* task)
{
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}

/// Runs the states of the No. 9 golem's chest piece.
static void _helicopterDepartureNo9ChestPieceTask(Task* task)
{
    EnemyTaskFunc states[2] = { _helicopterDepartureAttachNo9ChestPiece, _helicopterDepartureTickNo9ChestPiece };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the No. 9 golem's chest piece to its chest and shows it with the body's lighting.
static void _helicopterDepartureAttachNo9ChestPiece(Enemy* enemy, Task* task)
{
    enum { CHEST_PART   = 3,
           UPDATE_STATE = 1 };
    Task*      parentTask;
    TmdObject* model;
    _No9Work*  parentWork;
    GfxCoord*  root;
    GfxCoord*  parentCoords;

    parentTask   = task->parent;
    model        = task->extra.tmd;
    parentCoords = parentTask->extra.tmd->coords;
    root         = model->coords;
    parentWork   = parentTask->work;

    root->parent    = &parentCoords[CHEST_PART];
    model->lightMtx = &parentWork->light;
    model->flags    = 0;
    model->colorMtx = &parentWork->color;
    task->state     = UPDATE_STATE;
}

/// Composes the chest piece's attached transform each tick.
static void _helicopterDepartureTickNo9ChestPiece(Enemy* enemy, Task* task)
{
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}

/// Runs the states of the No. 9 golem's prop.
static void _helicopterDepartureNo9PropTask(Task* task)
{
    EnemyTaskFunc states[2] = { _helicopterDepartureAttachNo9Prop, _helicopterDepartureTickNo9Prop };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the No. 9 golem's prop to its left hand with the body's lighting.
static void _helicopterDepartureAttachNo9Prop(Enemy* enemy, Task* task)
{
    enum { LEFT_HAND_PART = 12,
           UPDATE_STATE   = 1 };
    Task*      parentTask;
    TmdObject* model;
    _No9Work*  parentWork;
    GfxCoord*  root;
    GfxCoord*  parentCoords;

    parentTask   = task->parent;
    model        = task->extra.tmd;
    parentCoords = parentTask->extra.tmd->coords;
    root         = model->coords;
    parentWork   = parentTask->work;

    root->parent    = &parentCoords[LEFT_HAND_PART];
    model->lightMtx = &parentWork->light;
    model->flags    = 0;
    model->colorMtx = &parentWork->color;
    task->state     = UPDATE_STATE;
}

/// Composes the prop's attached transform each tick.
static void _helicopterDepartureTickNo9Prop(Enemy* enemy, Task* task)
{
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}
