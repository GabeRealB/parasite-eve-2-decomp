#include "rooms/dryfield_dilapidated_house.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_521100.h"

#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/screen/screen_wave.h"
#include "../../shared/actor/bezier_curve.h"
#include "../../shared/screen/screen_negative.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/effects/sprite_quad.h"
#include "../../shared/actor/model_morph.h"

/// Work block of the morphing model a script command attaches to one of the
/// player's coordinates, which its child beam and cone effects read through
/// their parent task.
///
/// The three levels all hold the task's morph progress, 0..`ONE` in 4.12 fixed
/// point, rewritten every frame. `attachMtx` is the composed matrix of the
/// attachment point. The allocation is 0x6C bytes; nothing accesses the bytes
/// after the matrix.
typedef struct {
    s32    morphLevel;     // Morph progress; written every frame but never read
    s32    beamLevel;      // Brightness and length of the two ring beams
    s32    coneLevel;      // Inner-edge grey of the two cones, saturating at 0x400
    MATRIX attachMtx;      // Parent model's coordinate chain composed up to the attachment coordinate
    byte   field_2C[0x40]; // Allocated but never accessed; role unproven
} _DryfieldDilapidatedHouseMorphWork;
STATIC_ASSERT_SIZEOF(_DryfieldDilapidatedHouseMorphWork, 0x6C);

/// Work block of the curve debug task: a model task whose model stays hidden
/// while it marks a fixed Bezier path's control points and samples on screen
/// with small coloured crosses.
///
/// Nothing in the room spawns that task. The block is filled in once as the
/// task starts and never read back: the markers are placed with the parent
/// task's `_DryfieldDilapidatedHouseMorphWork::attachMtx` instead.
typedef struct {
    MATRIX initialMtx; // The model's local coordinate matrix as the task started
    s32    field_20;   // Set to 0x1000 at start and never read; role unproven
} _DryfieldDilapidatedHouseCurveDebugWork;
STATIC_ASSERT_SIZEOF(_DryfieldDilapidatedHouseCurveDebugWork, 0x24);

/// One full cone-rim ripple turn, in 16384-unit phase coordinates.
///
/// Entries of `_DryfieldDilapidatedHouseConeWork::rimPhase` wrap with
/// period - 1 as a mask and shift right by two before `rsin` (4096 units/turn).
#define DRYFIELD_DILAPIDATED_HOUSE_CONE_RIM_PHASE_PERIOD 0x4000

/// Work block of one of the two cones the morphing model spawns as children:
/// the ripple that waves the cone's outer rim back and forth along its axis.
///
/// A cone is two 16-vertex rings joined by quads, rebuilt every frame. The
/// inner ring is rigid; each outer vertex is displaced along the axis by the
/// sine of its own phase, and every phase advances at its own fixed rate, so
/// the rim never settles into a repeating outline. A cone starts with each
/// phase already advanced by the frame count in its spawn argument, which is
/// what keeps the two cones out of step with each other.
typedef struct {
    s32 rimPhase[16]; // Ripple phase of each outer-ring vertex, wrapped to one period
} _DryfieldDilapidatedHouseConeWork;
STATIC_ASSERT_SIZEOF(_DryfieldDilapidatedHouseConeWork, 0x40);

extern void func_80724608(void* owner, s32 arg1, s32 arg2, void* name);

static s32 _gScreenWaveAmplitude;

static ScreenWaveCtx* _gScreenWaveContext;

static ScreenWaveOscillator _gScreenWaveColumnWaves[13];
static ScreenWaveOscillator _gScreenWaveRowWaves[32];

/// Encounter progression stored as an s32 for the lifetime of this room load.
enum {
    DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_NOT_STARTED      = 0,
    DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_INTRO_LATCHED    = 1,
    DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_WAIT_RELEASE     = 2,
    DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_FINISH_REQUESTED = 3,
};

/// Execution priorities of the room's script effects and encounter helpers.
enum {
    DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_NEGATIVE_CAPTURE = 32,
    DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_HEAD_TRACK       = 97,
    DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_EFFECT           = 192,
};

enum {
    DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT = 6,
    DRYFIELD_DILAPIDATED_HOUSE_BEAM_POINT_COUNT     = 4 * DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT,
};

static void _dryfieldDilapidatedHouseSetNegativeCapture(s32 durationFrames);
static void _dryfieldDilapidatedHouseMorphExit(Task* task);
static s32  _dryfieldDilapidatedHouseAdvanceMorph(Task* task);
static void _dryfieldDilapidatedHouseUpdateAttachmentTransform(Task* task);
static void _dryfieldDilapidatedHouseCopyModelVisibility(TmdObject* model, const TmdObject* parentModel);

static void _dryfieldDilapidatedHouseRoomInit(Task* task);
static void _dryfieldDilapidatedHouseRoomUpdate(Task* task);
static void _dryfieldDilapidatedHouseMorphAttachmentInit(Task* task);
static void _dryfieldDilapidatedHouseMorphAttachmentUpdate(Task* task);
static void _dryfieldDilapidatedHouseCurveDebugInit(Task* task);
static void _dryfieldDilapidatedHouseCurveDebugUpdate(Task* task);
static void _dryfieldDilapidatedHouseRingBeamInit(Task* task);
static void _dryfieldDilapidatedHouseCappedBeamUpdate(Task* task);
static void _dryfieldDilapidatedHouseRingBeamExit(Task* task);
static void _dryfieldDilapidatedHouseMorphConeInit(Task* task);
static void _dryfieldDilapidatedHouseMorphConeUpdate(Task* task);
static void _dryfieldDilapidatedHouseMorphConeExit(Task* task);
static void _dryfieldDilapidatedHouseDrawLightPrism(GfxCoord* coord, s16 firstVertex);
static void _dryfieldDilapidatedHouseDrawMorphCone(Task* task, const SVECTOR* ringVertices);
static void _dryfieldDilapidatedHouseDrawTwinTrail(s16 newestSlot, s16 colorMultipliers);

enum {
    DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT = 16,
    DRYFIELD_DILAPIDATED_HOUSE_PRISM_RING_CORNERS     = 4,
    DRYFIELD_DILAPIDATED_HOUSE_PRISM_VERTEX_COUNT     = 8,
};

/// Nonpositive script requests accepted by the room's screen-wave command.
enum {
    DRYFIELD_DILAPIDATED_HOUSE_WAVE_PREPARE_DECODE = -2,
    DRYFIELD_DILAPIDATED_HOUSE_WAVE_FAST_RISE      = -1,
    DRYFIELD_DILAPIDATED_HOUSE_WAVE_SLOW_RISE      = 0,
};

/// States accepted by the room's script-controlled player head tracker.
enum {
    DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_INIT   = 0,
    DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_IDLE   = 1,
    DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_ACTIVE = 2,
};

static WorldCollisionGrid               _gDryfieldDilapidatedHouseCollisionGrid;
static WorldCollisionOccluder           _gDryfieldDilapidatedHouseSightOccluders[1];
static WorldCollisionTrigger            _gDryfieldDilapidatedHouseViewBoundaryTriggers[9];
static WorldCollisionTrigger            _gDryfieldDilapidatedHouseActionTriggers[9];
static const WorldCoordRoomAmbientEntry _gDryfieldDilapidatedHouseAmbientByView[22];
static WorldCoordRoomLights             _gDryfieldDilapidatedHouseRoomLights;
static PadScriptCmd                     _gDryfieldDilapidatedHouseIntroVibrationCommands[2];
static PadScriptCmd                     _gDryfieldDilapidatedHouseClosingWaveVibrationCommands[4];
static PadScriptCmd                     _gDryfieldDilapidatedHouseClosingClip60VibrationCommands[2];
static PadScriptVibrationSegment        _gDryfieldDilapidatedHouseIntroVibrationSegments[2];
static PadScriptVibrationSegment        _gDryfieldDilapidatedHouseClosingWaveVibrationSegments[3];
static PadScriptVibrationSegment        _gDryfieldDilapidatedHouseClosingClip60VibrationSegments[2];
static SVECTOR                          _gDryfieldDilapidatedHouseAttachmentMorphSavedVertices[40];
static s32                              _dryfieldDilapidatedHouseRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32                              _dryfieldDilapidatedHouseResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32                              _dryfieldDilapidatedHouseIgnoreRoomCommand(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
static s32                              _dryfieldDilapidatedHouseStartEncounterMsg(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg);
static void                             _dryfieldDilapidatedHouseNegativeCaptureTask(Task* task);
static void                             _dryfieldDilapidatedHouseBlackoutTask(Task* task);
static void                             _dryfieldDilapidatedHouseEncounterFinishTask(Task* task);
static void                             _dryfieldDilapidatedHouseHeadTrackTask(Task* task);
static void                             _dryfieldDilapidatedHouseShakeYTask(Task* task);
static void                             _dryfieldDilapidatedHouseModeExitCountdownTask(Task* task);
static void                             _dryfieldDilapidatedHouseSetHeadTrackState(s32 state);
static void                             _dryfieldDilapidatedHouseCancelEffects(void);
static void                             _dryfieldDilapidatedHouseControlScreenWave(s32 request);
static void                             _dryfieldDilapidatedHouseSetBlackoutDelay(s32 delayFrames);
static void                             _dryfieldDilapidatedHouseSetMorphAttachment(s32 enabled);
static void                             _dryfieldDilapidatedHouseLockAttachmentsForEvent(void);
static void                             _dryfieldDilapidatedHouseMorphAttachmentTask(Task* task);
static void                             _dryfieldDilapidatedHouseCurveDebugTask(Task* task);
static void                             _dryfieldDilapidatedHouseCappedBeamTask(Task* task);
static void                             _dryfieldDilapidatedHouseMorphConeTask(Task* task);

/// Screen-wave spawn recipe at index 0, followed by a non-spawnable terminator.
///
/// Argument 2 borrows the room's writable ramp context through the final draw.
/// Only one wave may use the package's shared ramp and oscillators at a time.
static TaskDesc _gDryfieldDilapidatedHouseScreenWaveTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_EFFECT } }, _screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Current sine-displacement strength, recomputed as frame * scale / span.
///
/// Strength 32 corresponds to one pixel at the sine peak. The seed tick resets
/// the initial 256 to zero; the context must have a positive span while drawing.
static s32 _gScreenWaveAmplitude = 256;

/// Negative-frame capture recipe at index 0, followed by a non-spawnable terminator.
///
/// Argument 2 borrows the room's writable duration/completion block. Capture
/// runs before the priority-192 blackout helper; only one capture may be active.
static TaskDesc _gDryfieldDilapidatedHouseNegativeCaptureTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_NEGATIVE_CAPTURE } }, _dryfieldDilapidatedHouseNegativeCaptureTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Fixed VRAM rectangle read as one contiguous RGB555 frame in negative debug mode.
static RECT _gScreenNegativeFrameRect = { 0, 0, FILE_SYSTEM_IMAGE_WIDTH, FILE_SYSTEM_IMAGE_HEIGHT };

/// Mutable VRAM read rectangle for one displayed RGB555 frame strip.
///
/// Capture selects the displayed buffer's Y origin and advances X by strip
/// width. The capture task owns these coordinates through the transfer.
static RECT _gScreenNegativeStripRect = { 0, 0, FILE_SYSTEM_IMAGE_STRIP_WIDTH, FILE_SYSTEM_IMAGE_HEIGHT };

/// Room handlers for transition resolution, key-item refusal, encounter actions and ignored CAP commands.
///
/// Four message IDs followed by `TASK_MESSAGE_TABLE_END`; payloads are borrowed
/// through synchronous dispatch. The transition reply may alias its request.
static const TaskMessageEntry _gDryfieldDilapidatedHouseRoomMessageTable[] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldDilapidatedHouseResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldDilapidatedHouseRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldDilapidatedHouseStartEncounterMsg },
    { ROOM_MESSAGE_COMMAND, _dryfieldDilapidatedHouseIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless encounter helpers: 0 head tracker, 1 Y shake, 2 blackout, 3 finisher.
///
/// This indexed table has no terminator. Shake argument 1 is a positive frame
/// count; the other entries start with zero payloads and use room-owned state.
static TaskDesc _gDryfieldDilapidatedHouseEncounterTaskDescriptors[4] = {
    { { { TASK_BODY_NONE, DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_HEAD_TRACK } }, _dryfieldDilapidatedHouseHeadTrackTask, { .value = 0 } },
    { { { TASK_BODY_NONE, DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_EFFECT } }, _dryfieldDilapidatedHouseShakeYTask, { .value = 0 } },
    { { { TASK_BODY_NONE, DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_EFFECT } }, _dryfieldDilapidatedHouseBlackoutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_EFFECT } }, _dryfieldDilapidatedHouseEncounterFinishTask, { .value = 0 } },
};

/// Retained stage-exit countdown recipe at index 0, followed by a non-spawnable terminator.
///
/// No room code spawns it. Argument 1 is a signed tick count; a nonnegative
/// count exits after count + 2 updates. Argument 2 is unused.
static TaskDesc _gDryfieldDilapidatedHouseModeExitCountdownTaskDescriptors[] = {
    { { { TASK_BODY_NONE, DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_EFFECT } }, _dryfieldDilapidatedHouseModeExitCountdownTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Encounter phase: 0 untriggered, 1 intro latched, 2 waiting, 3 finisher requested.
///
/// Reset only by loading the room overlay. The action receiver latches phase 1;
/// room updates advance it after the event and actor/display/attachment gates.
static s32 _gDryfieldDilapidatedHouseEncounterPhase = DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_NOT_STARTED;

/// Sixteen borrowed player-bank entries for encounter clip IDs 47..62, with holes.
///
/// The copy request overwrites this complete extent in the equipped bank.
/// Clip data belongs to the loaded actor-521100 package and must outlive playback;
/// null slots are retained but cannot be played. Keep the authored data placement.
static AnimationSet* const _gDryfieldDilapidatedHousePlayerAnimationExtension[16] SECTION(".data") = {
    NULL,
    &gGolemNo9PlayerEvent48Clip,
    NULL,
    NULL,
    &gGolemNo9PlayerEvent51Clip,
    NULL,
    &gGolemNo9PlayerEvent53Clip,
    NULL,
    &gGolemNo9PlayerEvent55Clip,
    NULL,
    &gGolemNo9PlayerEvent57Clip,
    &gGolemNo9PlayerEvent58Clip,
    &gGolemNo9PlayerEvent59Clip,
    &gGolemNo9PlayerEvent60Clip,
    &gGolemNo9PlayerEvent61Clip,
    NULL,
};

/// Installs the encounter's sixteen clip pointers into the equipped player bank.
///
/// Both encounter scripts borrow this request synchronously before playback.
/// No adjacent request bytes are copied; later bank entries stay untouched.
static const AnimationBankCopyRequest _gDryfieldDilapidatedHousePlayerAnimationExtensionRequest SECTION(".data") = {
    .sets  = _gDryfieldDilapidatedHousePlayerAnimationExtension,
    .count = ARRAY_SIZE(_gDryfieldDilapidatedHousePlayerAnimationExtension),
};
STATIC_ASSERT(ARRAY_SIZE(_gDryfieldDilapidatedHousePlayerAnimationExtension) <= ANIMATION_BANK_EXTENSION_CAPACITY, dryfieldDilapidatedHousePlayerExtensionFitsBank);

/// Retained player-bank requests for encounter clips 47..50.
///
/// No script references these templates. Extension slots 0, 2 and 3 are NULL;
/// only clip 48 has loaded data. All four reset playback without blending;
/// clips 47..49 enable world collision, while clip 50 disables it.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClips47To50Requests[] SECTION(".data") = {
    { .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 1, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 2, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 3, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Restarts encounter clip 51 on the player and restores world-collision participation.
///
/// The script interpreter copies this template and resolves its weapon bank.
/// Clip 51 comes from extension slot 4; the reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClip51Request SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Retained reset request for player encounter clip 52 with collision enabled.
///
/// No script references it, and extension slot 5 has no loaded animation.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClip52Request SECTION(".data") = { .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 5, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE };

/// Restarts encounter clip 53 on the player and restores world-collision participation.
///
/// The script interpreter resolves the weapon bank in a copy of this template.
/// Clip 53 comes from extension slot 6; the reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClip53Request SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Retained reset request for player encounter clip 54 with collision disabled.
///
/// No script references it, and extension slot 7 has no loaded animation.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClip54Request SECTION(".data") = { .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 7, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the player into encounter clip 55 over ten frames with world collision disabled.
///
/// The script interpreter resolves the weapon bank in a copy of this template.
/// Clip 55 comes from extension slot 8; playback takes scripted control.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClip55Request SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 8,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained player-bank requests for encounter clips 56 and 57 with collision disabled.
///
/// No script references them. Clip 56 resets extension slot 9, which is NULL;
/// clip 57 has loaded data at slot 10 and requests an eight-frame blend.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClips56To57Requests[] SECTION(".data") = {
    { .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 9, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 10, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = 8, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Restarts encounter clip 48 on the player with world collision disabled.
///
/// The script interpreter resolves the weapon bank in a copy of this template.
/// Clip 48 comes from extension slot 1; the reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClip48Request SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts encounter clip 58 on the player with world collision disabled.
///
/// The closing encounter script resolves the weapon bank in a template copy.
/// Clip 58 comes from extension slot 11; the reset ignores blendFrames.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClip58Request SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 11,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts encounter clip 59 on the player with world collision disabled.
///
/// The closing script resolves the equipped weapon bank in a template copy.
/// Clip 59 is extension slot 12; reset ignores the blend duration.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClip59Request SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 12,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts encounter clip 60 on the player with world collision disabled.
///
/// The closing script resolves the equipped weapon bank in a template copy.
/// Clip 60 is extension slot 13; reset ignores the blend duration.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClip60Request SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 13,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained reset request for player encounter clip 61 with collision disabled.
///
/// Extension slot 14 has loaded data, but no script references this template.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClip61Request SECTION(".data") = { .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 14, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the ANMC woman's local clip 1 during the encounter intro.
///
/// Placed actor 1 translates request ID 0 to local set 1. Only
/// `animationId` is read; the authored bank, blend and collision values
/// are ignored. The script borrows this template through dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseAnmcWomanClip1Request SECTION(".data") = {
    .source               = { .index = 0 },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained request for the ANMC woman's local animation set 2.
///
/// No script references it. Her handler maps request 1 to set 2 and reads only
/// `animationId`; the authored bank, blend and collision fields have no effect.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseAnmcWomanClip2Request SECTION(".data") = { .source = { .index = 0 }, .animationId = 1, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts the ANMC woman's local clip 3 during the encounter intro.
///
/// Placed actor 1 translates request ID 2 to local set 3. Only
/// `animationId` is read; the authored bank, blend and collision values
/// are ignored. The script borrows this template through dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseAnmcWomanClip3Request SECTION(".data") = {
    .source               = { .index = 0 },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the ANMC woman's local clip 4 during the encounter intro.
///
/// Placed actor 1 translates request ID 3 to local set 4. Only
/// `animationId` is read; the authored bank, blend and collision values
/// are ignored. The script borrows this template through dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseAnmcWomanClip4Request SECTION(".data") = {
    .source               = { .index = 0 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained requests for the ANMC woman's local animation sets 5..9.
///
/// No script references them. Her handler maps IDs 4..8 to sets 5..9 and reads
/// only `animationId`, ignoring the authored bank, blend and collision fields.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseAnmcWomanClips5To9Requests[] SECTION(".data") = {
    { .source = { .index = 0 }, .animationId = 4, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 0 }, .animationId = 5, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 0 }, .animationId = 6, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 0 }, .animationId = 7, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 0 }, .animationId = 8, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Restarts the ANMC woman's local clip 10 during the encounter intro.
///
/// Placed actor 1 translates request ID 9 to local set 10. Only
/// `animationId` is read; the authored bank, blend and collision values
/// are ignored. The script borrows this template through dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseAnmcWomanClip10Request SECTION(".data") = {
    .source               = { .index = 0 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Bank selectors for No. 9's introductory and closing body-animation requests.
enum {
    DRYFIELD_DILAPIDATED_HOUSE_NO9_INTRO_BANK   = 0,
    DRYFIELD_DILAPIDATED_HOUSE_NO9_CLOSING_BANK = 1,
};

/// Restarts No. 9's intro clip 21 on the body tracks.
///
/// Placed actor 0 translates bank 0/request 1 to local set 21.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip21Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_INTRO_BANK },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9's intro clip 22 on the body tracks.
///
/// Placed actor 0 translates bank 0/request 2 to local set 22.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip22Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_INTRO_BANK },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9's intro clip 23 on the body tracks.
///
/// Placed actor 0 translates bank 0/request 3 to local set 23.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip23Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_INTRO_BANK },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9's intro clip 24 on the body tracks.
///
/// Placed actor 0 translates bank 0/request 4 to local set 24.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip24Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_INTRO_BANK },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9's intro clip 25 on the body tracks.
///
/// Placed actor 0 translates bank 0/request 5 to local set 25.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip25Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_INTRO_BANK },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9's intro clip 26 on the body tracks.
///
/// Placed actor 0 translates bank 0/request 6 to local set 26.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip26Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_INTRO_BANK },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9's intro clip 27 on the body tracks.
///
/// Placed actor 0 translates bank 0/request 7 to local set 27.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip27Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_INTRO_BANK },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9's intro clip 28 on the body tracks.
///
/// Placed actor 0 translates bank 0/request 8 to local set 28.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip28Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_INTRO_BANK },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9's closing clip 29 on the body tracks.
///
/// Placed actor 0 translates bank 1/request 0 to local set 29.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip29Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_CLOSING_BANK },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends No. 9 into closing clip 30 over eight normal-rate frames.
///
/// Placed actor 0 translates bank 1/request 1 to local set 30.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip30Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_CLOSING_BANK },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained reset requests for No. 9's closing body clips 31 and 32.
///
/// No script references them. Closing-bank IDs 2 and 3 map to loaded sets 31
/// and 32; the body handler leaves world collision unchanged.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clips31To32Requests[] SECTION(".data") = {
    { .source = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_CLOSING_BANK }, .animationId = 2, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_CLOSING_BANK }, .animationId = 3, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Restarts No. 9's closing clip 33 on the body tracks.
///
/// Placed actor 0 translates bank 1/request 4 to local set 33.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip33Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_CLOSING_BANK },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9's closing clip 34 on the body tracks.
///
/// Placed actor 0 translates bank 1/request 5 to local set 34.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip34Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_CLOSING_BANK },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9's closing clip 35 on the body tracks.
///
/// Placed actor 0 translates bank 1/request 6 to local set 35.
/// The receiver leaves world collision unchanged and borrows the
/// complete request only through synchronous dispatch.
static const AnimationPlayRequest _gDryfieldDilapidatedHouseNo9Clip35Request SECTION(".data") = {
    .source               = { .index = DRYFIELD_DILAPIDATED_HOUSE_NO9_CLOSING_BANK },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum { DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_WEAPON_CLIP = 9 };

/// Resets the player's equipped-weapon stationary aim pose during encounter scripts.
///
/// The event interpreter copies the request and replaces `source.index` with
/// the equipped weapon's bank before dispatch. Playback takes scripted control
/// and disables world collision; the reset ignores the zero blend duration.
/// Keep the authored data-section placement for this read-only template.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerAimHoldRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_WEAPON_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player into equipped-bank clip 1 over eight frames with collision enabled.
///
/// Both encounter scripts copy this template before resolving its weapon bank.
/// Clip 1 belongs to the bank's base clips rather than the room's extension.
static const AnimationPlayRequest _gDryfieldDilapidatedHousePlayerClip1Request SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Command 0 in this room's actor namespace, shared by two different receivers.
///
/// Placed actor 1 resumes the ANMC woman's normal update; placed actor 0 starts
/// No. 9's full-burning event phase. Scripts borrow the record through dispatch.
static ActorCommand _gDryfieldDilapidatedHouseActorCommand0 = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE } },
    .command = 0,
};

/// Command 1 in this room's actor namespace, shared by two different receivers.
///
/// Placed actor 1 starts the ANMC woman's own emitter; placed actor 0 hides
/// No. 9's weapon. Scripts borrow the complete record through dispatch.
static ActorCommand _gDryfieldDilapidatedHouseActorCommand1 = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE } },
    .command = 1,
};

/// Commands sent only to the encounter's placed ANMC woman.
enum {
    DRYFIELD_DILAPIDATED_HOUSE_ANMC_WOMAN_COMMAND_FLATTEN         = 2,
    DRYFIELD_DILAPIDATED_HOUSE_ANMC_WOMAN_COMMAND_STOP_SPLATTER   = 3,
    DRYFIELD_DILAPIDATED_HOUSE_ANMC_WOMAN_COMMAND_PLAYER_SPLATTER = 4,
};

/// Restarts the ANMC woman's flattening phase during the encounter intro.
///
/// Placed actor 1 resets flattening progress and selects its flatten task state.
/// The receiver ignores the authored room context and borrows this record only
/// through synchronous dispatch.
static const ActorCommand _gDryfieldDilapidatedHouseAnmcWomanFlattenCommand SECTION(".data") = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE } },
    .command = DRYFIELD_DILAPIDATED_HOUSE_ANMC_WOMAN_COMMAND_FLATTEN,
};

/// Stops both ANMC-woman splatter emitters when the encounter intro is skipped.
///
/// Placed actor 1 kills the woman/player emitters, then resumes normal update.
/// The receiver ignores the authored room context and borrows this record only
/// through synchronous dispatch.
static const ActorCommand _gDryfieldDilapidatedHouseAnmcWomanStopSplatterCommand SECTION(".data") = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE } },
    .command = DRYFIELD_DILAPIDATED_HOUSE_ANMC_WOMAN_COMMAND_STOP_SPLATTER,
};

/// Starts the ANMC woman's periodic splatter emitter at the player.
///
/// Placed actor 1 creates the player-anchored emitter during the encounter intro.
/// The receiver ignores the authored room context and borrows this record only
/// through synchronous dispatch.
static const ActorCommand _gDryfieldDilapidatedHouseAnmcWomanPlayerSplatterCommand SECTION(".data") = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE } },
    .command = DRYFIELD_DILAPIDATED_HOUSE_ANMC_WOMAN_COMMAND_PLAYER_SPLATTER,
};

/// Player placement at the opening of the encounter intro.
///
/// XYZ uses room game units and yaw uses 4096 units per turn; pitch and
/// roll are zero. The placement is borrowed only through message dispatch.
static const ActorTransform _gDryfieldDilapidatedHousePlayerIntroArrivalPlacement SECTION(".data") = {
    .pos = { -5304, 0, -1940, 0 },
    .rot = { 0, 796, 0, 0 },
};

/// Player placement for equipped-bank clip 48 during the encounter intro.
///
/// XYZ uses room game units and yaw uses 4096 units per turn; pitch and
/// roll are zero. The placement is borrowed only through message dispatch.
static const ActorTransform _gDryfieldDilapidatedHousePlayerIntroClip48Placement SECTION(".data") = {
    .pos = { -2630, 0, -1740, 0 },
    .rot = { 0, 910, 0, 0 },
};

/// Player placement used during the intro and by its skip-to-battle sequence.
///
/// XYZ is in room game units; yaw is a positive quarter turn with no pitch or
/// roll. The placement message borrows this template synchronously.
static const ActorTransform _gDryfieldDilapidatedHousePlayerBattlePlacement SECTION(".data") = {
    .pos = { -2630, 0, -1900, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Player placement for closing clip 58 after the screen distortion.
///
/// XYZ uses room game units and yaw uses 4096 units per turn; pitch and
/// roll are zero. The placement is borrowed only through message dispatch.
static const ActorTransform _gDryfieldDilapidatedHousePlayerClosingClip58Placement SECTION(".data") = {
    .pos = { 2900, 0, -120, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Player placement for the equipped-weapon aim pose in the closing script.
///
/// XYZ uses room game units and yaw uses 4096 units per turn; pitch and
/// roll are zero. The placement is borrowed only through message dispatch.
static const ActorTransform _gDryfieldDilapidatedHousePlayerClosingAimPlacement SECTION(".data") = {
    .pos = { 2900, 0, -408, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// ANMC woman's opening placement during the encounter intro.
///
/// XYZ uses room game units and yaw uses 4096 units per turn; pitch and
/// roll are zero. The placement is borrowed only through message dispatch.
static const ActorTransform _gDryfieldDilapidatedHouseAnmcWomanIntroOpeningPlacement SECTION(".data") = {
    .pos = { 0, 0, -860, 0 },
    .rot = { 0, -1137, 0, 0 },
};

/// ANMC woman's placement when the intro starts local clip 1.
///
/// XYZ uses room game units and yaw uses 4096 units per turn; pitch and
/// roll are zero. The placement is borrowed only through message dispatch.
static const ActorTransform _gDryfieldDilapidatedHouseAnmcWomanIntroClip1Placement SECTION(".data") = {
    .pos = { -1300, 0, -1300, 0 },
    .rot = { 0, -1137, 0, 0 },
};

/// ANMC woman's placement before the intro starts local clip 3.
///
/// XYZ uses room game units and yaw uses 4096 units per turn; pitch and
/// roll are zero. The placement is borrowed only through message dispatch.
static const ActorTransform _gDryfieldDilapidatedHouseAnmcWomanIntroClip3Placement SECTION(".data") = {
    .pos = { -1300, 0, -1800, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// ANMC woman's placement before the intro starts her flattening phase.
///
/// XYZ uses room game units and yaw uses 4096 units per turn; pitch and
/// roll are zero. The placement is borrowed only through message dispatch.
static const ActorTransform _gDryfieldDilapidatedHouseAnmcWomanFlattenPlacement SECTION(".data") = {
    .pos = { -1880, 0, -1460, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// No. 9's placement during the intro's opening fade.
///
/// Position is in room game units; yaw is a negative quarter turn. The actor
/// copies the full position and composes all three angles during dispatch.
static const ActorTransform _gDryfieldDilapidatedHouseNo9IntroOpeningPlacement SECTION(".data") = {
    .pos = { 130, 0, -930, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// No. 9's intro placement while the player holds the weapon aim pose.
///
/// Position is in room game units; yaw is a negative quarter turn. The actor
/// borrows the complete transform only through synchronous dispatch.
static const ActorTransform _gDryfieldDilapidatedHouseNo9IntroAimPlacement SECTION(".data") = {
    .pos = { -370, 0, -1100, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// No. 9's battle placement shared by the intro's normal ending and skip sequence.
///
/// XYZ is in room game units; yaw is a negative quarter turn with no pitch or
/// roll. Placed actor 0 borrows this template during synchronous placement.
static const ActorTransform _gDryfieldDilapidatedHouseNo9BattlePlacement SECTION(".data") = {
    .pos = { 500, 0, 300, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// No. 9's placement at the start of the closing encounter scene.
///
/// Position is in room game units; yaw is a positive quarter turn. The actor
/// borrows the complete transform only through synchronous dispatch.
static const ActorTransform _gDryfieldDilapidatedHouseNo9ClosingOpeningPlacement SECTION(".data") = {
    .pos = { 1000, 0, 0, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Unreferenced words 19 and 1; the movement-animation payload interpretation is unproven.
static const GameActorMoveAnim D_dryfield_dilapidated_house_801843F0 SECTION(".data") = { 19, 1 };

/// Scene/audio stream key for the No. 9 encounter introduction.
///
/// Group 2, stream 11, qualifier 11; retained until the event ends or replaces it.
static const EvsSceneKey _gDryfieldDilapidatedHouseEncounterIntroSceneKey SECTION(".data") = { .group = 2, .streamId = 11, .subId = 11 };

/// Scene/audio stream key for the No. 9 encounter's closing scene.
///
/// Group 2, stream 12, qualifier 11; retained until the event ends or replaces it.
static const EvsSceneKey _gDryfieldDilapidatedHouseEncounterClosingSceneKey SECTION(".data") = { .group = 2, .streamId = 12, .subId = 11 };

/// Message and draw requests used by the two placed encounter actors.
enum {
    DRYFIELD_DILAPIDATED_HOUSE_MESSAGE_ACTIVATE_NO9        = 2007,
    DRYFIELD_DILAPIDATED_HOUSE_ACTOR_HIDE                  = 0,
    DRYFIELD_DILAPIDATED_HOUSE_ACTOR_SHOW                  = 1,
    DRYFIELD_DILAPIDATED_HOUSE_ACTOR_HIDE_SKIP_AUTO_BUFFER = 2,
};

/// CAP sequence selectors and the closing script's finisher-release signal.
enum {
    DRYFIELD_DILAPIDATED_HOUSE_CAP_INTRO       = 9,
    DRYFIELD_DILAPIDATED_HOUSE_CAP_CLOSING     = 10,
    DRYFIELD_DILAPIDATED_HOUSE_CAP_SEQUENCE_13 = 13,
    DRYFIELD_DILAPIDATED_HOUSE_EVENT_RELEASED  = 2,
};

/// Skippable No. 9 encounter introduction, ending with battle engaged.
///
/// Installs the player clips, synchronizes CAP sequence 9 with scene audio,
/// stages the ANMC woman and No. 9, then restores weapons and player control.
/// The interpreter borrows this END-terminated stream and its payloads; keep
/// the room and actor resources loaded through script and vibration completion.
static EvsCommand _gDryfieldDilapidatedHouseEncounterIntroScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = DRYFIELD_DILAPIDATED_HOUSE_MESSAGE_ACTIVATE_NO9 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldDilapidatedHousePlayerAnimationExtensionRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_DILAPIDATED_HOUSE_CAP_INTRO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gDryfieldDilapidatedHouseEncounterIntroSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_HIDE_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHousePlayerIntroArrivalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHouseAnmcWomanIntroOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9IntroOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldDilapidatedHouseActorCommand0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHousePlayerIntroClip48Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldDilapidatedHouseActorCommand0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseAnmcWomanClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHouseAnmcWomanIntroClip1Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHousePlayerBattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHouseAnmcWomanIntroClip3Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetHeadTrackState }, { .value = DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_ACTIVE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseAnmcWomanClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldDilapidatedHouseActorCommand1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseAnmcWomanClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = &_gDryfieldDilapidatedHouseAnmcWomanPlayerSplatterCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetHeadTrackState }, { .value = DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_INIT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip22Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gDryfieldDilapidatedHouseIntroVibrationCommands }, { .vibrationSegments = _gDryfieldDilapidatedHouseIntroVibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerAimHoldRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9IntroAimPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip23Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip27Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_HIDE_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip28Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_HIDE_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHouseAnmcWomanFlattenPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseAnmcWomanClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = &_gDryfieldDilapidatedHouseAnmcWomanFlattenCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip21Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_HIDE_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip24Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip25Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip26Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9BattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Intro skip path that places the player and No. 9 for battle.
///
/// Ends scene streaming and splatter, resets head tracking, restores view 3
/// and weapons, then engages battle and resumes player control. The interpreter
/// borrows this END-terminated stream and its room/actor payloads through completion.
static EvsCommand _gDryfieldDilapidatedHouseEncounterIntroSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = &_gDryfieldDilapidatedHouseAnmcWomanStopSplatterCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetHeadTrackState }, { .value = DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_INIT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_HIDE_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9BattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHousePlayerBattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerAimHoldRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldDilapidatedHouseCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skippable closing scene that releases the encounter for saved-session progression.
///
/// Synchronizes CAP sequence 10 with scene audio, waves, negative captures and
/// the player morph attachment. Event state 2 releases the finisher before the
/// last waits and player-control restoration. This END-terminated stream and
/// its room/actor payloads remain borrowed through script and effect completion.
static EvsCommand _gDryfieldDilapidatedHouseEncounterClosingScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldDilapidatedHouseLockAttachmentsForEvent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldDilapidatedHousePlayerAnimationExtensionRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_DILAPIDATED_HOUSE_CAP_CLOSING }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gDryfieldDilapidatedHouseEncounterClosingSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip34Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHousePlayerClosingAimPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerAimHoldRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_DILAPIDATED_HOUSE_ACTOR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9ClosingOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip29Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip30Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = DRYFIELD_DILAPIDATED_HOUSE_WAVE_PREPARE_DECODE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = DRYFIELD_DILAPIDATED_HOUSE_WAVE_FAST_RISE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gDryfieldDilapidatedHouseClosingWaveVibrationCommands }, { .vibrationSegments = _gDryfieldDilapidatedHouseClosingWaveVibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = SCREEN_WAVE_RAMP_FALLING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = DRYFIELD_DILAPIDATED_HOUSE_WAVE_SLOW_RISE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gDryfieldDilapidatedHousePlayerClosingClip58Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = SCREEN_WAVE_RAMP_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerClip58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerClip59Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetMorphAttachment }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldDilapidatedHouseActorCommand0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetMorphAttachment }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip35Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldDilapidatedHouseNo9Clip33Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldDilapidatedHousePlayerClip60Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gDryfieldDilapidatedHouseClosingClip60VibrationCommands }, { .vibrationSegments = _gDryfieldDilapidatedHouseClosingClip60VibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldDilapidatedHouseActorCommand1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = DRYFIELD_DILAPIDATED_HOUSE_EVENT_RELEASED }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Closing-scene skip path that cancels room effects and releases the encounter.
///
/// Stops the wave, blackout capture and morph attachment before setting event
/// state 2, then restores weapons, view 3 and player control. The interpreter
/// borrows this END-terminated stream and its callbacks through completion.
static EvsCommand _gDryfieldDilapidatedHouseEncounterClosingSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = SCREEN_WAVE_RAMP_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetMorphAttachment }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = DRYFIELD_DILAPIDATED_HOUSE_EVENT_RELEASED }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Retained event stream that starts CAP sequence 13 and waits for its cue.
///
/// No room code starts it. After the cue, waits three script frames and ends;
/// the narrative purpose of CAP sequence 13 is unproven.
static EvsCommand _gDryfieldDilapidatedHouseCapSequence13Script[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_DILAPIDATED_HOUSE_CAP_SEQUENCE_13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Tracked morph-model attachment, or NULL before spawn and after explicit removal.
///
/// The script starts one instance on player coordinate 3, then kills and clears
/// it. The player owns the task subtree; this borrowed handle is not a reference
/// count and is not cleared by independent parent teardown. Do not start twice.
static Task* _gDryfieldDilapidatedHouseMorphAttachmentTask = NULL;

static TmdBone _gDryfieldDilapidatedHouseModel08794Skeleton[1] = {
#include "assets/dryfield_dilapidated_house_model_08794_skeleton.inc"
};

static u32 _gDryfieldDilapidatedHouseModel08794PartVerts[1] = {
#include "assets/dryfield_dilapidated_house_model_08794_partVerts.inc"
};

static SVECTOR _gDryfieldDilapidatedHouseModel08794Verts[40] = {
#include "assets/dryfield_dilapidated_house_model_08794_verts.inc"
};

static SVECTOR _gDryfieldDilapidatedHouseModel08794Normals[128] = {
#include "assets/dryfield_dilapidated_house_model_08794_normals.inc"
};

static u32 _gDryfieldDilapidatedHouseModel08794Stream[171] = {
#include "assets/dryfield_dilapidated_house_model_08794_stream.inc"
};

static TmdSource _gDryfieldDilapidatedHouseModel08794 = {
    0,
    1160,
    0,
    1,
    _gDryfieldDilapidatedHouseModel08794PartVerts,
    _gDryfieldDilapidatedHouseModel08794Verts,
    _gDryfieldDilapidatedHouseModel08794Normals,
    _gDryfieldDilapidatedHouseModel08794Skeleton,
    _gDryfieldDilapidatedHouseModel08794Stream,
};

static TmdBone _gDryfieldDilapidatedHouseModel08D0CSkeleton[1] = {
#include "assets/dryfield_dilapidated_house_model_08D0C_skeleton.inc"
};

static u32 _gDryfieldDilapidatedHouseModel08D0CPartVerts[1] = {
#include "assets/dryfield_dilapidated_house_model_08D0C_partVerts.inc"
};

static SVECTOR _gDryfieldDilapidatedHouseModel08D0CVerts[40] = {
#include "assets/dryfield_dilapidated_house_model_08D0C_verts.inc"
};

static SVECTOR _gDryfieldDilapidatedHouseModel08D0CNormals[40] = {
#include "assets/dryfield_dilapidated_house_model_08D0C_normals.inc"
};

static u32 _gDryfieldDilapidatedHouseModel08D0CStream[171] = {
#include "assets/dryfield_dilapidated_house_model_08D0C_stream.inc"
};

static TmdSource _gDryfieldDilapidatedHouseModel08D0C = {
    0,
    1160,
    0,
    1,
    _gDryfieldDilapidatedHouseModel08D0CPartVerts,
    _gDryfieldDilapidatedHouseModel08D0CVerts,
    _gDryfieldDilapidatedHouseModel08D0CNormals,
    _gDryfieldDilapidatedHouseModel08D0CSkeleton,
    _gDryfieldDilapidatedHouseModel08D0CStream,
};

/// Full-weight local XYZ displacements for attachment model vertices 0..31.
///
/// The morph applies these signed game-unit deltas with a Q12 weight. Fourth
/// halfwords are unused; the model's final eight vertices receive no deltas.
static SVECTOR _gDryfieldDilapidatedHouseAttachmentMorphVertexDeltas[32] = {
#include "assets/dryfield_dilapidated_house_morph_08FDC.inc"
};

/// Shared vertex-only deformation of the player's scripted model attachment.
///
/// Setup snapshots all 40 rest vertices; blends affect the first 32. A Q12
/// weight of ONE applies the full delta and zero restores rest XYZ. The task's
/// increasing progress is complemented before blending. Normals stay unchanged.
/// Only one attachment may own the shared snapshot while this overlay is loaded.
static const ModelMorph _gDryfieldDilapidatedHouseAttachmentMorph SECTION(".data") = {
    .vertexDeltas     = _gDryfieldDilapidatedHouseAttachmentMorphVertexDeltas,
    .targetNormals    = NULL,
    .savedVertices    = _gDryfieldDilapidatedHouseAttachmentMorphSavedVertices,
    .savedNormals     = NULL,
    .savedVertexCount = ARRAY_SIZE(_gDryfieldDilapidatedHouseAttachmentMorphSavedVertices),
    .normalCount      = 0,
    .firstVertex      = 0,
    .deltaCount       = ARRAY_SIZE(_gDryfieldDilapidatedHouseAttachmentMorphVertexDeltas),
};
STATIC_ASSERT(ARRAY_SIZE(_gDryfieldDilapidatedHouseAttachmentMorphSavedVertices) == ARRAY_SIZE(_gDryfieldDilapidatedHouseModel08794Verts), dryfieldDilapidatedHouseMorphSnapshotFitsModel);
STATIC_ASSERT(ARRAY_SIZE(_gDryfieldDilapidatedHouseAttachmentMorphVertexDeltas) <= ARRAY_SIZE(_gDryfieldDilapidatedHouseAttachmentMorphSavedVertices), dryfieldDilapidatedHouseMorphDeltasFitSnapshot);

/// Attachment-local control points and markers of the debug Bezier curve.
///
/// Segments use entries 0..3 and 3..6, sharing endpoint 3; marker 7 repeats
/// entry 0. XYZ uses signed game-coordinate units. The room never spawns the
/// debug task; an authored purpose beyond its curve display is unproven.
static SVECTOR _gDryfieldDilapidatedHouseDebugCurveControlPoints[] = {
    { -81, -162, -82, 0 },
    { -150, -232, -355, 0 },
    { -150, -162, -720, 0 },
    { 351, -622, -720, 0 },
    { 351, -348, -605, 0 },
    { 351, 6, -113, 0 },
    { -351, 6, -113, 0 },
    { -81, -162, -82, 0 },
};

/// Capped-beam topology: six lit-core quads followed by ten fading halo quads.
///
/// Each signed-byte row selects four screen points and colours in 0..23.
/// Points 12 and 18 are unused halo centres. Order also selects the blend mode.
static const s8 _gGlowCappedBeamQuadIndices[16][4] SECTION(".data") = {
    { 0, 6, 1, 11 },
    { 0, 6, 5, 7 },
    { 0, 3, 1, 2 },
    { 0, 3, 5, 4 },
    { 6, 9, 7, 8 },
    { 6, 9, 11, 10 },
    { 2, 1, 14, 13 },
    { 3, 2, 15, 14 },
    { 3, 4, 15, 16 },
    { 4, 5, 16, 17 },
    { 5, 7, 17, 19 },
    { 8, 7, 20, 19 },
    { 9, 8, 21, 20 },
    { 9, 10, 21, 22 },
    { 10, 11, 22, 23 },
    { 1, 11, 13, 23 },
};

/// White and yellow capped-beam RGB colours for four six-point screen contours.
///
/// Points 0..11 light the core; 12..23 are black halo edges. The drawer scales
/// RGB by signed Q12 brightness. The fourth byte is zero and unused.
static CVECTOR _gGlowCappedBeamColors[24] = {
    { 255, 255, 255, 0 },
    { 255, 255, 255, 0 },
    { 255, 255, 120, 0 },
    { 255, 255, 120, 0 },
    { 255, 255, 120, 0 },
    { 255, 255, 255, 0 },
    { 255, 255, 255, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

/// Start and full-length end of the capped beam in attachment-local game units.
///
/// The paired beam mirrors X; morph progress shortens the transformed end
/// toward the start before projection.
static SVECTOR _gDryfieldDilapidatedHouseCappedBeamEndpoints[] = {
    { -81, -162, -82, 0 },
    { -150, -232, -355, 0 },
};

/// Start-cap contour shared by the beam's core and enlarged halo.
///
/// Entry 0 is the centre, followed by five rim offsets. XY is scaled by the
/// projection distance divided by the end depth, then rotated in screen space;
/// the stored zero Z components are not read.
static SVECTOR _gDryfieldDilapidatedHouseCappedBeamStartCapOffsets[DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT] = {
    { 0, 0, 0, 0 },
    { 12, 0, 0, 0 },
    { 6, 5, 0, 0 },
    { 0, 5, 0, 0 },
    { -6, 5, 0, 0 },
    { -12, 0, 0, 0 },
};

/// End-cap contour shared by the beam's core and enlarged halo.
///
/// Entry 0 is the centre, followed by five rim offsets. XY uses the same
/// perspective scale and screen rotation as the start cap; Z is not read.
static SVECTOR _gDryfieldDilapidatedHouseCappedBeamEndCapOffsets[DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT] = {
    { 0, 0, 0, 0 },
    { -5, 0, 0, 0 },
    { -3, -2, 0, 0 },
    { 0, -4, 0, 0 },
    { 3, -2, 0, 0 },
    { 5, 0, 0, 0 },
};

/// Per-callback phase advance of each morph cone outer-ring vertex.
///
/// One turn is `DRYFIELD_DILAPIDATED_HOUSE_CONE_RIM_PHASE_PERIOD` units. The
/// initial phases are these rates times the spawn argument's frame advance.
static s32 _gDryfieldDilapidatedHouseConeRimPhaseRates[] = {
    128,
    256,
    324,
    238,
    455,
    224,
    348,
    380,
    155,
    370,
    445,
    317,
    288,
    200,
    426,
    222,
};
STATIC_ASSERT(ARRAY_SIZE(_gDryfieldDilapidatedHouseConeRimPhaseRates) == ARRAY_SIZE(((_DryfieldDilapidatedHouseConeWork*)0)->rimPhase), dryfieldDilapidatedHouseConePhaseRateCount);

/// Inner then outer morph-cone ring centre, in attachment-local game units.
static SVECTOR _gDryfieldDilapidatedHouseConeRingCenters[] = {
    { 0, -164, -83, 0 },
    { 0, -164, -280, 0 },
};

/// Morph family recipes: 0 attachment, 1 curve debug, 2 capped beam, 3 cone.
///
/// No terminator. The attachment borrows the player task and coordinate index
/// in arguments 2 and 1. Its children borrow the attachment in argument 2;
/// beams take a mirror selector and cones an initial phase advance in frames.
/// Entry 1 is retained but never spawned by this room. Each model has 40 vertices.
static TaskDesc _gDryfieldDilapidatedHouseMorphTaskDescriptors[4] = {
    { { { TASK_BODY_TMD, DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_EFFECT } }, _dryfieldDilapidatedHouseMorphAttachmentTask, { .model = &_gDryfieldDilapidatedHouseModel08794 } },
    { { { TASK_BODY_TMD, DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_EFFECT } }, _dryfieldDilapidatedHouseCurveDebugTask, { .model = &_gDryfieldDilapidatedHouseModel08D0C } },
    { { { TASK_BODY_COORD, DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_EFFECT } }, _dryfieldDilapidatedHouseCappedBeamTask, { .value = 0 } },
    { { { TASK_BODY_COORD, DRYFIELD_DILAPIDATED_HOUSE_PRIORITY_EFFECT } }, _dryfieldDilapidatedHouseMorphConeTask, { .value = 0 } },
};

/// Corners of the left, middle and right room light prisms, in local game units.
///
/// Each consecutive group of eight stores the four lit-ring corners followed
/// by the four dark-ring corners. The drawer selects bases 0, 8 and 16.
static SVECTOR _gDryfieldDilapidatedHouseLightPrismVertices[] = {
    { -5500, -2250, -3030, 0 },
    { -4500, -2250, -3030, 0 },
    { -4500, -1030, -3030, 0 },
    { -5500, -1030, -3030, 0 },
    { -5780, 0, -180, 0 },
    { -5000, 0, -180, 0 },
    { -4770, 0, -1660, 0 },
    { -5660, 0, -1660, 0 },
    { -3000, -2250, -3030, 0 },
    { -2000, -2250, -3030, 0 },
    { -2000, -1030, -3030, 0 },
    { -3000, -1030, -3030, 0 },
    { -3370, 0, -180, 0 },
    { -2500, 0, -180, 0 },
    { -2770, 0, -1660, 0 },
    { -3140, 0, -1660, 0 },
    { -500, -2250, -3030, 0 },
    { 500, -2250, -3030, 0 },
    { 500, -1030, -3030, 0 },
    { -500, -1030, -3030, 0 },
    { -870, 0, -180, 0 },
    { 0, 0, -180, 0 },
    { 230, 0, -1660, 0 },
    { -650, 0, -1660, 0 },
};

/// First and second ribbon endpoints in No. 9's model-part-8 local game units.
static SVECTOR _gDryfieldDilapidatedHouseTwinTrailEndpointOffsets[] = {
    { -128, 96, 0, 0 },
    { -896, 96, 0, 0 },
};

WorldCollisionRoomResources gDryfieldDilapidatedHouseRoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldDilapidatedHouseCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldDilapidatedHouseViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldDilapidatedHouseActionTriggers,
        .occluders            = _gDryfieldDilapidatedHouseSightOccluders,
    },
};

u8* gDryfieldDilapidatedHouseRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldDilapidatedHouseRoomViewCounts[1] = { ARRAY_SIZE(gDryfieldDilapidatedHouseViewCameras) };

WorldCoordRoomLighting gDryfieldDilapidatedHouseRoomLighting[1] = {
    { .lights = &_gDryfieldDilapidatedHouseRoomLights, .ambientTable = _gDryfieldDilapidatedHouseAmbientByView },
};

enum {
    DRYFIELD_DILAPIDATED_HOUSE_WARP_1_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE, 2),
    DRYFIELD_DILAPIDATED_HOUSE_WARP_1_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE, 1),
    DRYFIELD_DILAPIDATED_HOUSE_WARP_1_MAP_FLAG        = 467,
};

DirectionWarpEntry gDryfieldDilapidatedHouseWarpEntries[2] = {
    { { { .word = 0 }, 1596, 0, -2540 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2296, 0, -1578 }, { 0, 0, 0, 0 }, DRYFIELD_DILAPIDATED_HOUSE_WARP_1_ARRIVAL_SOUND, DRYFIELD_DILAPIDATED_HOUSE_WARP_1_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, DRYFIELD_DILAPIDATED_HOUSE_WARP_1_MAP_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -5403, 2, -400 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -5184, 2, 200 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room collision face normals, with 4096 representing unit length.
static SVECTOR _gDryfieldDilapidatedHouseCollisionGridNormals[] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_normals.inc"
};

/// Room collision vertices in signed room-local game coordinates.
static SVECTOR _gDryfieldDilapidatedHouseCollisionGridVerts[] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_verts.inc"
};

/// Indexed room collision faces, using surface classes 0..2.
static WorldCollisionGridFace _gDryfieldDilapidatedHouseCollisionGridFaces[] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_faces.inc"
};

/// Complete serialized pool of signed face-index lists, each ending in -1.
static s16 _gDryfieldDilapidatedHouseCollisionGridCells[] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldDilapidatedHouseCollisionGridCells` (208 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_DILAPIDATED_HOUSE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldDilapidatedHouseCollisionGridCells[(halfwordOffset)])

/// X-major pointers to the six face lists of the room's 3-by-2 collision grid.
static s16* _gDryfieldDilapidatedHouseCollisionGridTable[] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_table.inc"
};
#undef DRYFIELD_DILAPIDATED_HOUSE_CELL_FACE_LIST_AT_OFFSET

/// Room 1's collision mesh and 4000-unit XZ cell index.
///
/// The origin is (-6000, -3200), with three X cells and two Z cells. Activation
/// binds the writable descriptor to the view; it and all borrowed pools must
/// remain loaded until the active grid is retired.
static WorldCollisionGrid _gDryfieldDilapidatedHouseCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldDilapidatedHouseCollisionGridNormals,
    .vertices    = _gDryfieldDilapidatedHouseCollisionGridVerts,
    .faces       = _gDryfieldDilapidatedHouseCollisionGridFaces,
    .cellFaceIds = _gDryfieldDilapidatedHouseCollisionGridTable,
    .xBias       = 6000,
    .zBias       = 3200,
    .cellCountX  = 3,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldDilapidatedHouseCollisionGridFaces),
};

ViewCamera gDryfieldDilapidatedHouseViewCameras[21] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 900, 0x4E20, 0 } }, 490 },
    { { { { 3872, 0, -1335 }, { -1200, 1793, -3481 }, { 584, 3682, 1695 } }, { 5380, 4300, 2840 } }, 207 },
    { { { { -2538, 0, 3214 }, { 1329, 3729, 1049 }, { -2926, 1694, -2310 } }, { 200, 2400, -200 } }, 257 },
    { { { { -1264, 0, 3896 }, { 486, 4063, 158 }, { -3865, 511, -1254 } }, { -3950, 2000, -1850 } }, 230 },
    { { { { -1264, 0, -3896 }, { -486, 4063, 158 }, { 3865, 511, -1254 } }, { 3950, 2000, -1850 } }, 230 },
    { { { { -3842, 0, -1419 }, { -415, 3916, 1123 }, { 1357, 1198, -3674 } }, { -600, 2000, -600 } }, 257 },
    { { { { -2926, 0, 2866 }, { 1386, 3585, 1415 }, { -2508, 1980, -2561 } }, { -412, 2960, -716 } }, 230 },
    { { { { -2291, 0, -3395 }, { -66, 4095, 45 }, { 3394, 80, -2291 } }, { 3808, 1373, 933 } }, 329 },
    { { { { -60, 0, 4095 }, { -794, 4018, -11 }, { -4017, -794, -59 } }, { 1221, 1047, 1902 } }, 329 },
    { { { { -138, 0, -4093 }, { 165, 4092, -5 }, { 4090, -165, -137 } }, { 1372, 1030, 834 } }, 257 },
    { { { { 2330, 0, 3368 }, { 49, 4095, -34 }, { -3367, 60, 2330 } }, { -638, 1112, 2964 } }, 289 },
    { { { { 2266, 0, -3411 }, { 1314, 3780, 872 }, { 3148, -1577, 2091 } }, { 2575, 366, 1974 } }, 289 },
    { { { { 4092, 0, -172 }, { -3, 4095, -76 }, { 172, 76, 4091 } }, { 2270, 220, 2900 } }, 289 },
    { { { { -1589, 0, 3775 }, { -499, 4060, -210 }, { -3741, -541, -1575 } }, { -3420, 920, -690 } }, 257 },
    { { { { -1116, 0, -3940 }, { 314, 4082, -89 }, { 3928, -327, -1113 } }, { -1680, 1200, -170 } }, 329 },
    { { { { -3467, 0, -2179 }, { 653, 3907, -1040 }, { 2079, -1228, -3308 } }, { -2000, 700, -1330 } }, 257 },
    { { { { -3364, 0, 2336 }, { 2068, 1903, 2978 }, { -1085, 3626, -1563 } }, { -1220, 2760, -480 } }, 257 },
    { { { { 3587, 0, -1975 }, { 248, 4063, 451 }, { 1960, -515, 3559 } }, { -2080, 920, 1450 } }, 289 },
    { { { { -2069, 0, 3534 }, { -442, 4063, -259 }, { -3507, -513, -2052 } }, { -3190, 1000, -1140 } }, 257 },
    { { { { 4087, 0, -269 }, { 38, 4054, 580 }, { 266, -581, 4045 } }, { -3270, 670, 2170 } }, 257 },
    { { { { 4095, 0, -58 }, { -41, 2903, -2888 }, { 41, 2889, 2903 } }, { -2480, 2960, 2650 } }, 257 },
};

/// View 1's empty sprite list, retaining decoded image strips and its end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 67 textured background rectangles, with pixel positions and sorting depths.
static SpriteSource _gDryfieldDilapidatedHouseView2SpriteSources[] = {
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -48, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 64, -56, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 80, -40, 625, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 96, -16, 625, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 112, 8, 625, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -32, -120, 1125, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -24, -120, 1125, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -120, 1125, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -120, 1000, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 937, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 8, -120, 875, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 8, -64, 937, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 16, -8, 956, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -120, 750, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -64, 875, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -8, 1136, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 40, -120, 625, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, -8, 1250, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 40, 8, 1147, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 56, -120, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -120, 250, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -120, 250, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 104, -120, 250, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -120, 250, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 136, -120, 250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -120, 250, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -64, 500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 0, 550, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, -64, 425, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -64, 425, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -64, 375, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 136, -64, 350, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, -64, 329, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 0, 522, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 0, 463, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, 0, 416, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 136, 0, 378, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 152, 0, 353, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, 24, 1125, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 24, 250, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 24, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 24, 250, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 24, 250, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 144, 24, 250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 24, 250, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, 48, 750, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 56, 32, 750, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 72, 24, 750, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, 24, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 104, 24, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -40, 1000, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -64, 687, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -24, 1204, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, -64, 687, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 250, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 80, 350, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, 72, 425, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, 64, 425, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -16, 64, 425, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 0, 56, 400, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 16, 56, 425, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 32, 48, 425, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 48, 40, 250, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, 32, 250, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, 24, 250, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, 24, 250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, 32, 250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
};

/// View 2's three sprite ranges after the image-strip selector, then the end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 49, 0, 0, { 2, 0 } },
    { 54, ARRAY_SIZE(_gDryfieldDilapidatedHouseView2SpriteSources) - 54, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 63 textured background rectangles, with pixel positions and sorting depths.
static SpriteSource _gDryfieldDilapidatedHouseView3SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 0, 1125, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -48, 1125, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -48, 1125, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -16, -120, 1125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -120, 1125, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 1125, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, -72, 1125, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 8, -120, 1000, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 24, -120, 1000, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -120, 1000, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -120, 1000, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, -120, 1000, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -72, 1062, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -16, 1125, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -120, 875, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -64, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -8, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -120, 875, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -64, 875, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -8, 875, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -120, 750, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 120, -120, 750, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -120, 750, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 152, -120, 750, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -8, 1392, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -24, 1187, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -8, 1250, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 8, 1250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -8, -24, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -8, 1250, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 8, 1325, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 72, 750, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 56, 750, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, 16, 750, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, 56, 750, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 8, 750, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 56, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -128, 8, 750, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 56, 750, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 8, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 64, 750, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 64, 750, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -160, 16, 750, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 937, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 48, 937, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, 8, 937, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -48, 875, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 80, 8, 937, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -88, 812, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -32, 875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, 24, 937, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -88, 812, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -32, 875, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 104, 24, 937, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 120, 24, 937, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -32, 812, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -88, 750, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 136, -88, 750, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 136, -32, 750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 136, 24, 937, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 24, 937, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -32, 750, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -88, 750, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
};

/// View 3's two sprite ranges after the image-strip selector, then the end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 31, 0, 0, { 1, 0 } },
    { 31, ARRAY_SIZE(_gDryfieldDilapidatedHouseView3SpriteSources) - 31, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 17 textured background rectangles, with pixel positions and sorting depths.
static SpriteSource _gDryfieldDilapidatedHouseView4SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -56, 2125, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, 8, 1625, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 1625, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -104, 0, 1625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 8, 1625, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -8, 2125, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -56, -56, 2125, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -56, 2125, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -24, -56, 2125, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -8, -56, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 8, -56, 1915, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 24, -56, 2000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 40, -56, 2125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 56, -56, 2125, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 72, -56, 2125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 88, -56, 2125, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 104, -56, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

/// View 4's complete sprite range after the image-strip selector, then the end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldDilapidatedHouseView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's six textured background rectangles, with pixel positions and sorting depths.
static SpriteSource _gDryfieldDilapidatedHouseView5SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 16, 1425, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 0, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, 0, 1475, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 8, 1375, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 8, 1312, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, 8, 1250, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
};

/// View 5's complete sprite range after the image-strip selector, then the end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldDilapidatedHouseView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's ten textured background rectangles, with pixel positions and sorting depths.
static SpriteSource _gDryfieldDilapidatedHouseView6SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -8, 750, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 48, 750, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, 8, 750, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, 48, 750, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 104, -16, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 48, 750, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, -8, 875, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 48, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 0, 875, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 48, 875, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
};

/// View 6's complete sprite range after the image-strip selector, then the end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldDilapidatedHouseView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's 15 textured background rectangles, with pixel positions and sorting depths.
static SpriteSource _gDryfieldDilapidatedHouseView7SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 112, -120, 1100, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 96, -120, 1100, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 80, -120, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -32, 1300, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 64, -120, 1300, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, -120, 1250, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 32, -56, 1450, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 32, -120, 1250, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -120, 1350, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -56, 1375, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 0, 1375, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 8, -64, 1475, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -32, 1100, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -32, 1100, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 80, -32, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

/// View 7's complete sprite range after the image-strip selector, then the end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldDilapidatedHouseView7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's seven textured background rectangles, with pixel positions and sorting depths.
static SpriteSource _gDryfieldDilapidatedHouseView8SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -120, 375, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -120, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, -120, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -120, 375, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -16, -88, 375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 0, -88, 750, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -88, 750, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
};

/// View 8's complete sprite range after the image-strip selector, then the end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldDilapidatedHouseView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's 23 textured background rectangles, with pixel positions and sorting depths.
static SpriteSource _gDryfieldDilapidatedHouseView9SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -160, -48, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -48, 625, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -48, 625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, -48, 625, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -96, -48, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -80, -48, 625, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -64, -48, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, 40, 625, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -144, 40, 625, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, 40, 625, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, 40, 625, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, 40, 625, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, 40, 625, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 40, 625, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 88, 625, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 72, 625, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, 72, 625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, 72, 625, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 72, 625, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 72, 625, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 72, 625, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 72, 625, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, 72, 625, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
};

/// View 9's complete sprite range after the image-strip selector, then the end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldDilapidatedHouseView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10's empty sprite list, retaining decoded image strips and its end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView10SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 11's 18 textured background rectangles, with pixel positions and sorting depths.
static SpriteSource _gDryfieldDilapidatedHouseView11SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 112, 175, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 40, 312, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -152, 32, 337, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -160, 24, 350, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, 48, 287, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 56, 300, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 64, 312, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, 80, 175, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -152, 80, 175, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 80, 175, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 80, 175, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 80, 175, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 80, 175, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 80, 175, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 88, 175, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 96, 175, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 96, 175, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 104, 175, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
};

/// View 11's complete sprite range after the image-strip selector, then the end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView11SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldDilapidatedHouseView11SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 12's empty sprite list, retaining decoded image strips and its end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView12SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 13's texture rectangles, pixel positions and unscaled sorting depths.
///
/// The view's complete sprite range borrows this writable source array.
static SpriteSource _gDryfieldDilapidatedHouseView13SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 125, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 72, 125, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -128, 56, 125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, 40, 125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, 40, 125, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, 48, 125, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, 56, 125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -48, 56, 125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, 56, 125, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, 56, 125, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 0, 48, 125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 16, 40, 125, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 32, 48, 125, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 48, 56, 125, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, 56, 125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 80, 48, 125, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, 40, 125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 104, 125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, 40, 125, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 48, 125, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 48, 125, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
};

/// View 13's complete sprite range after the image-strip selector, then the end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView13SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldDilapidatedHouseView13SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 14's texture rectangles, pixel positions and unscaled sorting depths.
///
/// The view's complete sprite range borrows this writable source array.
static SpriteSource _gDryfieldDilapidatedHouseView14SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -32, 2000, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -32, 2125, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, 24, 1875, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -8, -32, 2050, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, 24, 2000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, -32, 2000, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 56, 2000, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -32, 1875, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, 24, 1875, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 40, -32, 1750, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 24, 1750, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 24, 1750, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -32, 1750, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
};

/// View 14's complete sprite range after the image-strip selector, then the end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView14SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldDilapidatedHouseView14SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 15's empty sprite list, retaining decoded image strips and its end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView15SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 16's empty sprite list, retaining decoded image strips and its end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView16SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 17's empty sprite list, retaining decoded image strips and its end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView17SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 18's empty sprite list, retaining decoded image strips and its end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView18SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 19's empty sprite list, retaining decoded image strips and its end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView19SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 20's empty sprite list, retaining decoded image strips and its end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView20SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 21's empty sprite list, retaining decoded image strips and its end marker.
static SpriteBatch _gDryfieldDilapidatedHouseView21SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldDilapidatedHouseSpriteViews[21] = {
    { { .empty = _gDryfieldDilapidatedHouseView1SpriteBatches }, _gDryfieldDilapidatedHouseView1SpriteBatches, NULL },
    { { .elements = _gDryfieldDilapidatedHouseView2SpriteSources }, _gDryfieldDilapidatedHouseView2SpriteBatches, NULL },
    { { .elements = _gDryfieldDilapidatedHouseView3SpriteSources }, _gDryfieldDilapidatedHouseView3SpriteBatches, NULL },
    { { .elements = _gDryfieldDilapidatedHouseView4SpriteSources }, _gDryfieldDilapidatedHouseView4SpriteBatches, NULL },
    { { .elements = _gDryfieldDilapidatedHouseView5SpriteSources }, _gDryfieldDilapidatedHouseView5SpriteBatches, NULL },
    { { .elements = _gDryfieldDilapidatedHouseView6SpriteSources }, _gDryfieldDilapidatedHouseView6SpriteBatches, NULL },
    { { .elements = _gDryfieldDilapidatedHouseView7SpriteSources }, _gDryfieldDilapidatedHouseView7SpriteBatches, NULL },
    { { .elements = _gDryfieldDilapidatedHouseView8SpriteSources }, _gDryfieldDilapidatedHouseView8SpriteBatches, NULL },
    { { .elements = _gDryfieldDilapidatedHouseView9SpriteSources }, _gDryfieldDilapidatedHouseView9SpriteBatches, NULL },
    { { .empty = _gDryfieldDilapidatedHouseView10SpriteBatches }, _gDryfieldDilapidatedHouseView10SpriteBatches, NULL },
    { { .elements = _gDryfieldDilapidatedHouseView11SpriteSources }, _gDryfieldDilapidatedHouseView11SpriteBatches, NULL },
    { { .empty = _gDryfieldDilapidatedHouseView12SpriteBatches }, _gDryfieldDilapidatedHouseView12SpriteBatches, NULL },
    { { .elements = _gDryfieldDilapidatedHouseView13SpriteSources }, _gDryfieldDilapidatedHouseView13SpriteBatches, NULL },
    { { .elements = _gDryfieldDilapidatedHouseView14SpriteSources }, _gDryfieldDilapidatedHouseView14SpriteBatches, NULL },
    { { .empty = _gDryfieldDilapidatedHouseView15SpriteBatches }, _gDryfieldDilapidatedHouseView15SpriteBatches, NULL },
    { { .empty = _gDryfieldDilapidatedHouseView16SpriteBatches }, _gDryfieldDilapidatedHouseView16SpriteBatches, NULL },
    { { .empty = _gDryfieldDilapidatedHouseView17SpriteBatches }, _gDryfieldDilapidatedHouseView17SpriteBatches, NULL },
    { { .empty = _gDryfieldDilapidatedHouseView18SpriteBatches }, _gDryfieldDilapidatedHouseView18SpriteBatches, NULL },
    { { .empty = _gDryfieldDilapidatedHouseView19SpriteBatches }, _gDryfieldDilapidatedHouseView19SpriteBatches, NULL },
    { { .empty = _gDryfieldDilapidatedHouseView20SpriteBatches }, _gDryfieldDilapidatedHouseView20SpriteBatches, NULL },
    { { .empty = _gDryfieldDilapidatedHouseView21SpriteBatches }, _gDryfieldDilapidatedHouseView21SpriteBatches, NULL },
};

/// Directed view-change quads for room 1, including the final LAST record.
///
/// Source/destination pairs are 3->2, 2->3, 3->4, 4->3, 5->4, 6->5, 4->5,
/// 5->6 and another 3->2 boundary. Activation binds and links the writable
/// records; their room-local geometry uses game units. Keep the overlay
/// loaded until they are unlinked.
static WorldCollisionTrigger _gDryfieldDilapidatedHouseViewBoundaryTriggers[9] = {
    { NULL, NULL, NULL, { -4256, -2000, -2432, 0 }, { { 0, -3056, -2464, 0 }, { 0, -3056, 2432, 0 }, { 0, 3056, -2432, 0 }, { 0, 3056, 2464, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3924, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4064, -2080, -2368, 0 }, { { 0, -3104, 2272, 0 }, { 0, -3104, -2272, 0 }, { 0, 3104, 2272, 0 }, { 0, 3104, -2272, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3840, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1937, -1376, -1937, 0 }, { { -2182, -2400, 2208, 0 }, { 2183, -2400, -2207, 0 }, { -2182, 2400, 2208, 0 }, { 2183, 2400, -2207, 0 } }, { -2920, 0, -2887, 0 }, { 0, 0, 4096, 0 }, 3916, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2017, -1360, -2017, 0 }, { { 2183, -2384, -2207, 0 }, { -2182, -2384, 2208, 0 }, { 2183, 2384, -2207, 0 }, { -2182, 2384, 2208, 0 } }, { 2913, 0, 2880, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 220, -1392, 318, 0 }, { { -30, -2416, -3113, 0 }, { 4, -2416, 3091, 0 }, { -30, 2416, -3113, 0 }, { 4, 2416, 3091, 0 } }, { 4098, 0, -23, 0 }, { 0, 0, 4096, 0 }, 3932, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2974, -1312, -1986, 0 }, { { -2235, -2336, -137, 0 }, { 2226, -2336, 127, 0 }, { -2235, 2336, -137, 0 }, { 2226, 2336, 127, 0 } }, { 241, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 3228, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 318, -1360, 352, 0 }, { { 13, -2384, 3100, 0 }, { -23, -2384, -3109, 0 }, { 13, 2384, 3100, 0 }, { -23, 2384, -3109, 0 } }, { -4098, 0, 23, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2975, -1472, -2048, 0 }, { { 2226, -2496, 127, 0 }, { -2235, -2496, -137, 0 }, { 2226, 2496, 127, 0 }, { -2235, 2496, -137, 0 } }, { -243, 0, 4090, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4961, -2048, -2225, 0 }, { { -670, -3056, -1440, 0 }, { 660, -3056, 1411, 0 }, { -659, 3056, -1410, 0 }, { 671, 3056, 1441, 0 } }, { 3712, 1, -1733, 0 }, { 0, 0, 4096, 0 }, 3444, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room 1's interaction quads: two warps, six CAP commands and the encounter gate.
///
/// CAP commands 2, 3, 5, 18, 17 and 16 run outside battle; the automatic gate
/// sends room action 1. The final LAST record remains an active CAP trigger.
/// Geometry uses room-local game units; activation binds and links these
/// writable records, which stay loaded until unlinked.
static WorldCollisionTrigger _gDryfieldDilapidatedHouseActionTriggers[9] = {
    { NULL, NULL, NULL, { 1504, -52, -2768, 0 }, { { -768, 0, -400, 0 }, { 768, 0, -400, 0 }, { -768, 0, 400, 0 }, { 768, 0, 400, 0 } }, { 0, 4116, 0, 0 }, { 0, 0, 4096, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5600, -52, 192, 0 }, { { 224, 0, -624, 0 }, { 224, 0, 624, 0 }, { -224, 0, -624, 0 }, { -224, 0, 624, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 662, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4688, -64, 352, 0 }, { { -512, 0, -608, 0 }, { 512, 0, -608, 0 }, { -512, 0, 608, 0 }, { 512, 0, 608, 0 } }, { 0, 4101, 0, 0 }, { -4091, 0, 201, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4704, -64, -944, 0 }, { { -512, 0, -560, 0 }, { 512, 0, -560, 0 }, { -512, 0, 560, 0 }, { 512, 0, 560, 0 } }, { 0, 4110, 0, 0 }, { -4077, 0, -402, 0 }, 757, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3472, -64, -496, 0 }, { { -336, 0, -928, 0 }, { 1456, 0, -928, 0 }, { -336, 0, 928, 0 }, { 1136, 0, 928, 0 } }, { 0, 4107, 0, 0 }, { -4091, 0, 201, 0 }, 1722, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1328, -64, -2288, 0 }, { { -912, 0, -640, 0 }, { 912, 0, -640, 0 }, { -912, 0, 640, 0 }, { 912, 0, 640, 0 } }, { 0, 4103, 0, 0 }, { -4091, 0, 201, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 18, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3969, -64, -2273, 0 }, { { -502, 0, -1200, 0 }, { 521, 0, -1249, 0 }, { -520, 0, 1249, 0 }, { 502, 0, 1199, 0 } }, { 0, 4099, 0, 0 }, { -4091, 0, 201, 0 }, 1348, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -5168, -64, 768, 0 }, { { -800, 0, -480, 0 }, { 800, 0, -480, 0 }, { -800, 0, 480, 0 }, { 800, 0, 480, 0 } }, { 0, 4100, 0, 0 }, { -201, 0, -4091, 0 }, 931, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5232, -64, -2816, 0 }, { { -720, 0, -352, 0 }, { 720, 0, -352, 0 }, { -720, 0, 352, 0 }, { 720, 0, 352, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, 4091, 0 }, 799, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// One room-local sight-blocking quad spanning the X=-4096 plane.
///
/// Linked and enabled by room binding; the inclusive LAST bit ends the array.
/// XYZ and radius are game units. The authored normal is (4097,0,0), on the
/// 4096-per-unit scale; retain low flag value 1, whose role is unproven.
static WorldCollisionOccluder _gDryfieldDilapidatedHouseSightOccluders[1] = {
    { NULL, NULL, { -4096, -2000, 1232, 0 }, { { 0, 2576, -2544, 0 }, { 0, -2576, -2544, 0 }, { 0, 2576, 2544, 0 }, { 0, -2576, 2544, 0 } }, { 4097, 0, 0, 0 }, 3620, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Authored point lights for model shading in every view of the Dryfield dilapidated house.
///
/// Positions and falloff radii use integer world units; RGB intensities have 12
/// fractional bits (`ONE` is full strength). The room light collection borrows
/// this writable table while the overlay is loaded: coordinate updates set its
/// parents and composed matrices, and lighting queries overwrite attenuation.
static WorldCoordPointLight _gDryfieldDilapidatedHousePointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -1500, 500 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1847, 1847, 1847 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2000,
        .outer = 3549,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2500, -1500, -2500 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2666, 2666, 2666 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2256,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -1500, -2500 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2666, 2666, 2666 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -1500, -2500 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1028, 1028, 1028 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2256,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1500, -2500 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2666, 2666, 2666 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2256,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2500, -1500, 2000 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1028, 1028, 1028 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2000,
        .outer = 6400,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1500, 2000 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1028, 1028, 1028 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2000,
        .outer = 3003,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2500, -1500, 2000 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1028, 1028, 1028 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2000,
        .outer = 3000,
    },
};

/// Authored cone-light table for every view of the Dryfield dilapidated house.
///
/// The single entry has zero RGB intensity. Position and falloff radii use
/// integer world units; the axis has 12 fractional bits and the opening uses
/// 0x1000 units per turn. The room light collection borrows this writable table
/// while the overlay is loaded: coordinate updates rebuild its orientation and
/// composed transform, and lighting queries overwrite attenuation.
static WorldCoordSpotLight _gDryfieldDilapidatedHouseConeLights[1] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { -ONE, 0, 0 }, { 0, -2902, 2901 }, { 0, 2901, 2901 } }, { -5000, -2500, -4000 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 0, 0, 0 },
            .unknown_56 = { 0, 0 },
        },
        .axis  = { 0, 2896, 2896, 0 },
        .inner = 100,
        .outer = 3000,
        .angle = 625,
    },
};

/// Unreferenced bytes following the room's cone light in the overlay image.
///
/// Their original purpose is unproven. Preserve the representation, including
/// embedded address values, without exposing it as additional lights.
static u8 _gDryfieldDilapidatedHouseUnreferencedData[] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0xA4,
    0x50,
    0x19,
    0x80,
    0x01,
    0x00,
    0x00,
    0x00,
    0xA4,
    0x53,
    0x00,
    0x00,
    0x2D,
    0xFC,
    0x80,
    0x10,
    0xD8,
    0xFF,
    0x00,
    0x00,
    0x55,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xBE,
    0xE7,
    0x30,
    0xF3,
    0xDE,
    0x15,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x07,
    0x00,
    0x00,
    0x00,
    0xD4,
    0x03,
    0x00,
    0x00,
    0xF9,
    0xFF,
    0x00,
    0x00,
    0x2D,
    0xFC,
    0x00,
    0x00,
    0x07,
    0x00,
    0x00,
    0x00,
    0xD4,
    0x03,
    0xD0,
    0x10,
    0xF9,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xEB,
    0xEF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x34,
    0x11,
    0x00,
    0x00,
    0x06,
    0x05,
    0x00,
    0x00,
    0xE0,
    0x54,
    0x19,
    0x80,
    0x48,
    0x54,
    0x19,
    0x80,
    0xF8,
    0x30,
    0x07,
    0x80,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xC0,
    0x03,
    0xE0,
    0x11,
    0x3A,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x51,
    0xF0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x38,
    0x12,
    0x00,
    0x00,
    0x07,
    0x06,
    0x61,
    0x00,
    0x2C,
    0x55,
    0x19,
    0x80,
    0x94,
    0x54,
    0x19,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x5D,
    0xE8,
    0x30,
    0xF2,
    0x3C,
    0xFE,
    0x00,
    0x00,
    0xB5,
    0x03,
    0x30,
    0xEE,
    0x0C,
    0xFF,
    0x00,
    0x00,
    0x4B,
    0xFC,
    0x00,
    0x00,
    0xF5,
    0x00,
    0x00,
    0x00,
    0xB5,
    0x03,
    0xD0,
    0x11,
    0x0C,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x78,
    0x55,
    0x19,
    0x80,
    0xE0,
    0x54,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x40,
    0xEB,
    0x60,
    0xF2,
    0x6F,
    0xF4,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0x80,
    0xEF,
    0x7C,
    0xF7,
    0x00,
    0x00,
    0x08,
    0x00,
    0x80,
    0xEF,
    0x84,
    0x08,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0x80,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00,
    0x80,
    0x10,
    0x84,
    0x08,
    0x00,
    0x00,
    0x02,
    0x10,
    0x00,
    0x00,
    0xF0,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x8C,
    0x12,
    0x00,
    0x00,
    0x02,
    0x07,
    0x61,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x0C,
    0xF7,
    0x00,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0x80,
    0x10,
    0x0C,
    0xF7,
    0x00,
    0x00,
    0xFA,
    0xEF,
    0x00,
    0x00,
    0x0E,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0xC2,
    0x12,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x56,
    0x19,
    0x80,
    0x78,
    0x55,
    0x19,
    0x80,
    0xF8,
    0x30,
    0x07,
    0x80,
    0x40,
    0xEB,
    0xE0,
    0xF4,
    0xE0,
    0x28,
    0x00,
    0x00,
    0xEB,
    0x00,
    0x80,
    0xEF,
    0xCE,
    0xFA,
    0x00,
    0x00,
    0x10,
    0xFF,
    0x80,
    0xEF,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x52,
    0x11,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x5C,
    0x56,
    0x19,
    0x80,
    0xC4,
    0x55,
    0x00,
    0x00,
    0xF8,
    0x30,
    0x07,
    0x80,
    0xA0,
    0xEB,
    0x00,
    0x00,
    0x40,
    0x29,
    0x00,
    0x00,
    0x11,
    0xFF,
    0x80,
    0xEF,
    0x2E,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xCF,
    0xFA,
    0x00,
    0x00,
    0x11,
    0xFF,
    0x80,
    0x10,
    0x2E,
    0x05,
    0x00,
    0x00,
    0xEC,
    0x00,
    0x80,
    0x10,
    0xCF,
    0xFA,
    0x00,
    0x00,
    0x3F,
    0xF0,
    0x00,
    0x00,
    0x2E,
    0xFD,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x22,
    0x05,
    0x60,
    0xEF,
    0xA3,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x5D,
    0xFD,
    0x00,
    0x00,
    0x22,
    0x05,
    0x00,
    0x00,
    0xA3,
    0x02,
    0x00,
    0x00,
    0xDF,
    0xFA,
    0x00,
    0x00,
    0x5D,
    0xFD,
    0x00,
    0x00,
    0xAA,
    0xF8,
    0x00,
    0x00,
    0x43,
    0x0E,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x8C,
    0x11,
    0x00,
    0x00,
    0x04,
    0x03,
    0x61,
    0x00,
    0xF4,
    0x56,
    0x19,
    0x80,
    0x5C,
    0x56,
    0x00,
    0x00,
    0xF8,
    0x30,
    0x07,
    0x80,
    0xCC,
    0xFB,
    0x40,
    0xF3,
    0x6D,
    0x14,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x78,
    0x07,
    0x00,
    0x00,
    0xD0,
    0xF1,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x6F,
    0x11,
    0x00,
    0x00,
    0x03,
    0x04,
    0x61,
    0x00,
    0x40,
    0x57,
    0x00,
    0x00,
    0xA8,
    0x56,
    0x19,
    0x80,
    0xF8,
    0x30,
    0x07,
    0x80,
    0x40,
    0xEC,
    0x60,
    0xF3,
};

/// The room's eight point lights and one cone light for model shading.
///
/// No directional lights are authored. The light arrays stay writable for
/// coordinate composition and query attenuation while this overlay is loaded.
static WorldCoordRoomLights _gDryfieldDilapidatedHouseRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldDilapidatedHousePointLights),
    .pointLights           = _gDryfieldDilapidatedHousePointLights,
    .coneLightCount        = ARRAY_SIZE(_gDryfieldDilapidatedHouseConeLights),
    .coneLights            = _gDryfieldDilapidatedHouseConeLights,
};

/// Placement-entry keys and file numbers for the two encounter actors.
enum {
    DRYFIELD_DILAPIDATED_HOUSE_NO9_RESOURCE_ENTRY        = 34,
    DRYFIELD_DILAPIDATED_HOUSE_ANMC_WOMAN_RESOURCE_ENTRY = 29,
    DRYFIELD_DILAPIDATED_HOUSE_NO9_FILE_NUMBER           = 211,
    DRYFIELD_DILAPIDATED_HOUSE_ANMC_WOMAN_FILE_NUMBER    = 212,
    DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_VARIANT         = 1,
};

/// Variant 1's No. 9 and ANMC-woman file selections and spawn recipes.
///
/// Matches the two Dryfield-map placement keys and spawns descriptor 0 from
/// each actor table. File-group base 50 selects base files 521100 and 521200.
/// The third entry ends the table; loaders and spawners borrow all records
/// and task tables while the room and actor overlays remain loaded.
static AreaResource _gDryfieldDilapidatedHouseVariant1Resources[] = {
    {
        .entryId        = DRYFIELD_DILAPIDATED_HOUSE_NO9_RESOURCE_ENTRY,
        .fileNumber     = DRYFIELD_DILAPIDATED_HOUSE_NO9_FILE_NUMBER,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_50,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = gActor521100No9GolemTaskDescs,
    },
    {
        .entryId        = DRYFIELD_DILAPIDATED_HOUSE_ANMC_WOMAN_RESOURCE_ENTRY,
        .fileNumber     = DRYFIELD_DILAPIDATED_HOUSE_ANMC_WOMAN_FILE_NUMBER,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_50,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = gActor521100AnmcWomanTaskDescs,
    },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldDilapidatedHouseAreaVariants[13] = {
    [DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_VARIANT] = {
        .placements = gMapDryfieldDilapidatedHouseVariant1Placements,
        .resources  = _gDryfieldDilapidatedHouseVariant1Resources,
    },
};

/// Minimum ambient RGB for logical views 1..21, preceded by their count.
///
/// Colours use 16 units per 8-bit channel level. Lighting reads these minima
/// while the room overlay is loaded. Keep the authored data-section placement.
static const WorldCoordRoomAmbientEntry _gDryfieldDilapidatedHouseAmbientByView[22] SECTION(".data") = {
    { .viewCount = ARRAY_SIZE(_gDryfieldDilapidatedHouseAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1000, 1000, 1000, 1000 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 0, 0, 0, 0 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 3000, 3000, 600, 2700 } },
    { .color = { 2000, 750, 550, 1193 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 750, 750, 750, 750 } },
};

enum {
    DRYFIELD_DILAPIDATED_HOUSE_CLASS_1_WALK_ENTRY = 0x45,
    DRYFIELD_DILAPIDATED_HOUSE_CLASS_1_RUN_ENTRY  = 0x47,
    DRYFIELD_DILAPIDATED_HOUSE_CLASS_2_WALK_ENTRY = 0x4D,
    DRYFIELD_DILAPIDATED_HOUSE_CLASS_2_RUN_ENTRY  = 0x4F,
};

/// Surface class 1's walk/run cue bases in the loaded type-1 sound bank.
///
/// Scripted jumps use the walk base; cue 1 adds one and companion cues add
/// 100. Surface properties borrow this read-only record while the room is loaded.
/// Data-section placement preserves the authored layout.
static const WorldCollisionFootstepSounds _gDryfieldDilapidatedHouseClass1FootstepSounds SECTION(".data") = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_DILAPIDATED_HOUSE_CLASS_1_WALK_ENTRY,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_DILAPIDATED_HOUSE_CLASS_1_RUN_ENTRY,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_DILAPIDATED_HOUSE_CLASS_1_WALK_ENTRY,
};

/// Surface class 2's distinct walk/run cue bases in the loaded type-1 bank.
///
/// Scripted jumps use the walk base; cue 1 adds one and companion cues add
/// 100. Surface properties borrow this read-only record while the room is loaded.
/// Data-section placement preserves the authored layout.
static const WorldCollisionFootstepSounds _gDryfieldDilapidatedHouseClass2FootstepSounds SECTION(".data") = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_DILAPIDATED_HOUSE_CLASS_2_WALK_ENTRY,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_DILAPIDATED_HOUSE_CLASS_2_RUN_ENTRY,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_DILAPIDATED_HOUSE_CLASS_2_WALK_ENTRY,
};

/// Blocking, impact-enabled surface for classes 0 and 3..7, without footsteps.
static WorldCollisionSurfaceProperties _gDryfieldDilapidatedHouseSilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's blocking, impact-enabled surface with pushback and footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldDilapidatedHouseClass1Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldDilapidatedHouseClass1FootstepSounds,
};

/// Class 2's blocking, impact-enabled surface with pushback and distinct footsteps.
static WorldCollisionSurfaceProperties _gDryfieldDilapidatedHouseClass2Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldDilapidatedHouseClass2FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldDilapidatedHouseSurfacePropertiesByClass[8] = {
    &_gDryfieldDilapidatedHouseSilentSurface,
    &_gDryfieldDilapidatedHouseClass1Surface,
    &_gDryfieldDilapidatedHouseClass2Surface,
    &_gDryfieldDilapidatedHouseSilentSurface,
    &_gDryfieldDilapidatedHouseSilentSurface,
    &_gDryfieldDilapidatedHouseSilentSurface,
    &_gDryfieldDilapidatedHouseSilentSurface,
    &_gDryfieldDilapidatedHouseSilentSurface,
};

/// Nonzero low policy nibble that marks an updated saved area on the map.
enum { DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK = 1 };

/// Saved night-Dryfield area updates applied after the No. 9 encounter.
///
/// Thirty-two updates followed by `AREA_APPLY_END`; layouts and map marks
/// prepare the next session and discard saved enemy poses. Main street and
/// back street each carry separate replay/Scavenger and Bounty/Nightmare policies.
/// The consumer borrows this complete list only during synchronous application.
static const AreaApplyRec _gDryfieldDilapidatedHousePostEncounterAreaUpdates[] SECTION(".data") = {
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 1, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, 1, AREA_APPLY_MODE_REPLAY_SCAVENGER | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 1, AREA_APPLY_MODE_REPLAY_SCAVENGER | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE, 1, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2, 3, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, 3, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TOILET, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, 1, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TANK, 1, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_FACTORY, 1, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GARAGE, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 1, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 1, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6, 1, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, 1, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_CELLAR, 1, AREA_APPLY_MODE_ALWAYS | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { .stage = AREA_APPLY_END },
};

/// Additional night-underpass updates after the encounter when Gray Stalker is defeated.
///
/// Two mode-tagged layout/map-mark records followed by `AREA_APPLY_END`.
/// Applied after the main post-encounter list; borrowed only during application.
static const AreaApplyRec _gDryfieldDilapidatedHousePostEncounterGrayStalkerAreaUpdates[] SECTION(".data") = {
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 2, AREA_APPLY_MODE_REPLAY_SCAVENGER | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | DRYFIELD_DILAPIDATED_HOUSE_AREA_UPDATE_MAP_MARK },
    { .stage = AREA_APPLY_END },
};

/// Intro vibration: five frames of the on/off motor and an eight-frame variable ramp.
///
/// Both lanes start at step 0, select segments 0/1, then stop at step 1.
/// The script task borrows both entries and the paired segments until teardown.
static PadScriptCmd _gDryfieldDilapidatedHouseIntroVibrationCommands[] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Intro motor segments: index 0 holds for five frames; index 1 ramps 200 to 255 over eight.
///
/// Intensities are bytes before script attenuation. The on/off lane reads only
/// duration; the fourth byte is retained and unread by either lane.
static PadScriptVibrationSegment _gDryfieldDilapidatedHouseIntroVibrationSegments[] = {
    { 0, 0, 5, 0 },
    { 200, 255, 8, 1 },
};

/// Variable-motor vibration during the closing scene's fast screen-wave rise.
///
/// The on/off lane stops at step 0. The variable lane plays segment 0, waits
/// two frames, plays segment 1 and stops at step 3; segment 2 and the later
/// on/off commands are unreachable. The task borrows all four authored entries.
static PadScriptCmd _gDryfieldDilapidatedHouseClosingWaveVibrationCommands[] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Closing-wave variable ramps: 200 to 255 over seven frames, then 200 to 90 over five.
///
/// Entries 0/1 are played; entry 2 is retained but unreachable from either lane.
/// Intensities are bytes before attenuation; each fourth byte is retained and unread.
static PadScriptVibrationSegment _gDryfieldDilapidatedHouseClosingWaveVibrationSegments[] = {
    { 200, 255, 7, 1 },
    { 200, 90, 5, 1 },
    { 180, 60, 1, 0 },
};

/// Closing clip-60 vibration: five on/off frames and an eight-frame variable ramp.
///
/// Both lanes play segments 0/1 at step 0 and stop at step 1. The variable
/// STOP operand 2 is ignored. Both entries remain borrowed until task teardown.
static PadScriptCmd _gDryfieldDilapidatedHouseClosingClip60VibrationCommands[] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 2) }
};

/// Closing clip-60 segments: five on/off frames and a 200-to-255 ramp over eight.
///
/// These are separate authored records from the identical intro segments.
/// Intensities are bytes before attenuation; the fourth byte is retained and unread.
static PadScriptVibrationSegment _gDryfieldDilapidatedHouseClosingClip60VibrationSegments[] = {
    { 0, 0, 5, 0 },
    { 200, 255, 8, 1 },
};

/// Shared remaining-frame countdown of the vertical screen-shake task.
///
/// Setup copies its positive duration; every running tick decrements it once.
/// Parity selects the displacement's sign, so concurrent shakes cannot share it.
static s32 _gDryfieldDilapidatedHouseShakeFramesLeft = 0;

/// Signed tick countdown from the initial blackout through negative capture and final blackout.
///
/// Arming copies argument 1; unpaused countdown ticks decrement it once. Zero
/// starts the negative capture, and values below -15 draw black again. It keeps
/// decreasing until rearmed; reset leaves it intact, and overlay loading sets zero.
static s32 _gDryfieldDilapidatedHouseBlackoutFramesLeft = 0;

/// Borrowed context of this renderer's active wave task.
///
/// Set on the seed tick and left unchanged at teardown. Only one wave may use
/// this storage; the context must stay live through its final drawing tick.
static ScreenWaveCtx* _gScreenWaveContext = NULL;

/// Optional persistent task that aims the player's head at placed actor 1.
///
/// Room setup publishes it only when that actor exists. Script commands borrow
/// the handle to switch its state; it starts NULL on overlay load and is not
/// cleared by teardown. Keep its task live while those commands can run.
static Task* _gDryfieldDilapidatedHouseHeadTrackTask = NULL;

/// Persistent blackout timer armed and reset by the encounter scripts.
///
/// Room setup publishes the spawned task; commands require successful setup.
/// This borrowed handle starts NULL on overlay load and is not cleared by
/// teardown. The timer must stay live while scripts change its state/payload.
static Task* _gDryfieldDilapidatedHouseBlackoutTask = NULL;

/// Room-owned duration and completion signal borrowed by one negative capture.
///
/// The starter writes the unsigned frame duration before spawn; capture setup
/// clears done. Expiry or a later nonzero done releases the frozen frame.
/// Ordinary durations are 1..32767 frames after signed countdown narrowing.
/// Keep this overlay loaded and do not overlap captures using this block.
static ScreenNegativeCaptureArgs _gDryfieldDilapidatedHouseNegativeCaptureArgs = {
    .duration = 0,
    .done     = false,
};

/// Column sine oscillators for horizontal screen displacement.
///
/// The per-frame renderer uses entries 0..10; the prebuilt-grid renderer uses
/// 0..8. Any remaining declared capacity is unaccessed; its role is unproven.
static ScreenWaveOscillator _gScreenWaveColumnWaves[13] = { 0 };

/// Thirty active row sine oscillators for vertical screen displacement.
///
/// Seeded at wave startup and advanced on drawing updates. Any declared
/// entries beyond 29 are unaccessed; their role is unproven.
static ScreenWaveOscillator _gScreenWaveRowWaves[32] = { 0 };

/// Room-owned ramp context for its untinted screen-wave mesh.
///
/// Commands seed a 5- or 100-frame rise to strength 256 (eight pixels) before
/// spawn. Setup resets frame/phase; later commands request falling or finished.
/// The initial zero span is not usable by a running task. Room setup marks it
/// finished. Only one task may borrow the context, through its final draw tick.
static ScreenWaveCtx _gDryfieldDilapidatedHouseScreenWaveSpawnContext = {
    .modulateTexture = SCREEN_WAVE_TEXTURE_RAW,
};

/// Shared rest-XYZ snapshot for every vertex of the morph attachment model.
///
/// Setup fills XYZ before blending. Each vector's fourth halfword is retained
/// and unused. The single active attachment borrows the snapshot through its
/// final blend; concurrent setup would overwrite the same storage.
static SVECTOR _gDryfieldDilapidatedHouseAttachmentMorphSavedVertices[ARRAY_SIZE(_gDryfieldDilapidatedHouseModel08794Verts)];

/// Eight-frame circular history of the first twin-trail endpoint.
///
/// Local matrices retain world-space snapshots parented to `gGfxViewCoord`;
/// composed matrices supply view-space translations to the ribbon drawer.
/// All trail tasks share this storage, reseeding every slot on initialization.
static GfxCoord _gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory[8];

/// Circular history of the second endpoint, synchronized with the first history.
///
/// Uses the same world-space snapshots, view parent and age-masked slot index.
/// All trail tasks share the array; it has no per-task allocation or release.
static GfxCoord _gDryfieldDilapidatedHouseTwinTrailSecondEndpointHistory[ARRAY_SIZE(_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory)];
STATIC_ASSERT((ARRAY_SIZE(_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory) & (ARRAY_SIZE(_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory) - 1)) == 0, dryfieldDilapidatedHouseTrailHistoryPowerOfTwo);

#include "../../shared/screen/screen_wave.inc.c"

#include "../../shared/screen/screen_negative_capture.inc.c"

/// Captures the room's frame as a grayscale negative and holds scene drawing.
///
/// Argument 2 borrows writable `ScreenNegativeCaptureArgs` through completion.
/// Setup clears `done`; duration expiry or a later nonzero `done` releases it.
/// Use 0..32767 frames for an ordinary signed task countdown. The script pause
/// gate suspends every phase. Requires the loaded room overlay and exclusive
/// use of the resident image workspace until filtering finishes.
static void _dryfieldDilapidatedHouseNegativeCaptureTask(Task* task)
{
    _screenNegativeCaptureTask(task);
}

/// Room task states 0 initialize, 1 advance the encounter, 2 release the task.
///
/// Dispatch requires state 0..2 and copies all three callbacks before calling.
/// The room and callback code must remain loaded; preserve the rodata position.
static const TaskFuncTable3 _gDryfieldDilapidatedHouseRoomStateHandlers = {
    { _dryfieldDilapidatedHouseRoomInit, _dryfieldDilapidatedHouseRoomUpdate, taskKill },
};

/// Advances the room encounter after its introductory script releases control.
///
/// With event state zero, latch phase 1 becomes waiting phase 2. The later
/// encounter task starts once placed actor 0 reports absent, the attachment
/// wheel is closed and no display mode is pending. The receiver is unused.
/// Nonzero display debug mode also forwards actor and player names to a
/// development hook, independently of progression. Its numeric arguments'
/// meanings are unproven.
static void _dryfieldDilapidatedHouseRoomUpdate(Task* task)
{
    enum {
        EVENT_SCRIPT_IDLE     = 0,
        ENCOUNTER_FINISH_TASK = 3,
        ENCOUNTER_ACTOR_SLOT  = 0,
        HEAD_TRACK_ACTOR_SLOT = 1,
    };

    if (gGameSession->eventState == EVENT_SCRIPT_IDLE) {
        if (_gDryfieldDilapidatedHouseEncounterPhase == DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_INTRO_LATCHED) {
            _gDryfieldDilapidatedHouseEncounterPhase = DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_WAIT_RELEASE;
        } else if ((_gDryfieldDilapidatedHouseEncounterPhase == DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_WAIT_RELEASE) &&
                   (taskMessageDispatch(sceneFindPlacedActor(ENCOUNTER_ACTOR_SLOT), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0)) {
            if (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    _gDryfieldDilapidatedHouseEncounterPhase += 1;
                    taskSpawnFromTable(_gDryfieldDilapidatedHouseEncounterTaskDescriptors, ENCOUNTER_FINISH_TASK, 0, 0);
                }
            }
        }
    }
    if ((gDisplayState.debugMode != 0) && (sceneFindPlacedActor(HEAD_TRACK_ACTOR_SLOT) != 0)) {
        func_80724608(sceneFindPlacedActor(HEAD_TRACK_ACTOR_SLOT), -0x8C, 0xA, "AUNT");
        func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), -0x8C, 0x14, "Player");
    }
}

/// Queues an opaque 320-by-256 black tile at the current ordering-table depth.
///
/// Requires room drawing coordinates centred on the screen and one free TILE
/// in the current frame's packet arena. Advances the packet cursor; the tile
/// stays borrowed by the GPU until that frame finishes.
static inline void _dryfieldDilapidatedHouseDrawBlackout(void)
{
    enum { COVER_LEFT_PIXELS   = -160,
           COVER_TOP_PIXELS    = -128,
           COVER_WIDTH_PIXELS  = 320,
           COVER_HEIGHT_PIXELS = 256 };
    TILE* tile;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    SetTile(tile);
    tile->x0 = COVER_LEFT_PIXELS;
    tile->y0 = COVER_TOP_PIXELS;
    tile->w  = COVER_WIDTH_PIXELS;
    tile->h  = COVER_HEIGHT_PIXELS;
    tile->r0 = 0;
    tile->g0 = 0;
    tile->b0 = 0;
    addPrim(gGpuCurrentOt, tile);
}

/// Times a negative freeze-frame between the room's initial and final blackouts.
///
/// States are 0 reset, 1 idle, 2 arm from argument 1's signed frame count,
/// 3 initial blackout and 4 countdown. Arming also draws the initial blackout.
/// Countdown zero starts a 15-frame negative capture; counts below -15 draw
/// black again. The shared countdown keeps decreasing. The event-script pause
/// gate suspends both state changes and drawing; other states do nothing.
static void _dryfieldDilapidatedHouseBlackoutTask(Task* task)
{
    enum { BLACKOUT_RESET          = 0,
           BLACKOUT_IDLE           = 1,
           BLACKOUT_ARM            = 2,
           BLACKOUT_INITIAL_COVER  = 3,
           BLACKOUT_COUNTDOWN      = 4,
           NEGATIVE_CAPTURE_FRAMES = 15 };
    s32 paintBlack;
    s32 remainingFrames;

    paintBlack = false;
    if (gEvsPlaybackPaused == 0) {
        switch (task->state) {
            case BLACKOUT_RESET:
                task->state += 1;
                break;
            case BLACKOUT_IDLE:
                break;
            case BLACKOUT_ARM:
                _gDryfieldDilapidatedHouseBlackoutFramesLeft = task->spawnArg1.value;
                task->state                                  = task->state + 1;
                /* fallthrough */
            case BLACKOUT_INITIAL_COVER:
                paintBlack   = true;
                task->state += 1;
                break;
            case BLACKOUT_COUNTDOWN:
                remainingFrames                              = _gDryfieldDilapidatedHouseBlackoutFramesLeft - 1;
                _gDryfieldDilapidatedHouseBlackoutFramesLeft = remainingFrames;
                if (remainingFrames == 0) {
                    _dryfieldDilapidatedHouseSetNegativeCapture(NEGATIVE_CAPTURE_FRAMES);
                }
                if (_gDryfieldDilapidatedHouseBlackoutFramesLeft < -NEGATIVE_CAPTURE_FRAMES) {
                    paintBlack = true;
                }
                break;
        }
        if (paintBlack != false) {
            _dryfieldDilapidatedHouseDrawBlackout();
        }
    }
}

/// Commits the encounter's saved progression and queues a captured-frame reload.
///
/// Restores player HP/MP and companion HP, selects Dryfield R08 room/warp 1
/// with scene event 1, and restores sprite variant 1. Gray Stalker's defeat
/// enables the second area-update list. Requires the live save, party state,
/// room update lists and gameplay reload resources; the spawn result is ignored.
static inline void _dryfieldDilapidatedHousePrepareNextSession(void)
{
    enum {
        COMPANION_2_POST_ENCOUNTER_SCHEDULE = 6,
        COMPANION_1_POST_ENCOUNTER_SCHEDULE = 1,
        POST_ENCOUNTER_TALK_PROGRESS        = 2,
        NEXT_SCENE_EVENT                    = 1,
        NEXT_WARP                           = 1,
        NEXT_ROOM                           = 1,
        DEFAULT_SPRITE_VARIANT              = 1,
    };
    // Commit the encounter's saved progression before reloading resources.
    areaApplySavedUpdates(_gDryfieldDilapidatedHousePostEncounterAreaUpdates);
    if (gameFlagGetNibble(GAME_FLAG_GRAY_STALKER_DEFEATED) != 0) {
        areaApplySavedUpdates(_gDryfieldDilapidatedHousePostEncounterGrayStalkerAreaUpdates);
    }
    gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, COMPANION_2_POST_ENCOUNTER_SCHEDULE);
    gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, COMPANION_1_POST_ENCOUNTER_SCHEDULE);
    gameFlagSetNibble(GAME_FLAG_GAS_STATION_MAIN_STREET_BLOCKED, true);
    gameFlagSetNibble(GAME_FLAG_GENERAL_STORE_UNDERPASS_BLOCKED, true);
    gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_CUTSCENE_SEEN, true);
    gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS, POST_ENCOUNTER_TALK_PROGRESS);
    gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
    gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, 0);
    playerStateRestoreFullHpMp();
    companionRestoreFullHp();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = NEXT_SCENE_EVENT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_DRYFIELD;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = NEXT_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = NEXT_ROOM;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_DRYFIELD_R08;
    gDisplayState.spriteVariant                                 = DEFAULT_SPRITE_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
}

/// Runs the encounter's closing scene and prepares the next Dryfield session.
///
/// Starts the skippable event, waits for event state 2, credits placed actor 0's
/// battle rewards, allows four update ticks, then waits for battle reset. The
/// final state applies area and story progression, restores party HP/MP and
/// requests a captured-frame session reload. Demo scene 9 skips those final
/// changes. Every path through the final state releases this bodyless task.
static void _dryfieldDilapidatedHouseEncounterFinishTask(Task* task)
{
    enum {
        ENCOUNTER_START_SCENE       = 0,
        ENCOUNTER_WAIT_SCENE        = 1,
        ENCOUNTER_SETTLE_TICK_1     = 2,
        ENCOUNTER_SETTLE_TICK_2     = 3,
        ENCOUNTER_SETTLE_TICK_3     = 4,
        ENCOUNTER_SETTLE_TICK_4     = 5,
        ENCOUNTER_WAIT_BATTLE_RESET = 6,
        ENCOUNTER_APPLY_PROGRESSION = 7,
        EVENT_SCENE_RELEASED        = 2,
        ENCOUNTER_ACTOR_SLOT        = 0,
        UNUSED_BATTLE_RELEASE_ARG   = 0x1B,
        BATTLE_END_DELAY_FRAMES     = 3,
        DEMO_SCENE_SKIP_PROGRESSION = 9,
    };

    switch (task->state) {
        case ENCOUNTER_START_SCENE:
            evsStartScriptWithSkip(_gDryfieldDilapidatedHouseEncounterClosingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldDilapidatedHouseEncounterClosingSkipScript);
            task->state += 1;
            return;
        case ENCOUNTER_WAIT_SCENE:
            if (gGameSession->eventState == EVENT_SCENE_RELEASED) {
                sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(ENCOUNTER_ACTOR_SLOT), UNUSED_BATTLE_RELEASE_ARG);
                gSceneCombatState.signals.bytes.endDelayFrames = BATTLE_END_DELAY_FRAMES;
                task->state                                   += 1;
            }
            return;
        case ENCOUNTER_SETTLE_TICK_1:
        case ENCOUNTER_SETTLE_TICK_2:
        case ENCOUNTER_SETTLE_TICK_3:
        case ENCOUNTER_SETTLE_TICK_4:
            task->state += 1;
            return;
        case ENCOUNTER_WAIT_BATTLE_RESET:
            if (gGameSession->battleResetPending != 0) {
                task->state += 1;
            }
            return;
        case ENCOUNTER_APPLY_PROGRESSION:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != DEMO_SCENE_SKIP_PROGRESSION) {
                _dryfieldDilapidatedHousePrepareNextSession();
            }
            taskKill(task);
            return;
    }
}

#include "../../shared/screen/screen_negative_filter.inc.c"

/// Refuses key-item use in this room by returning zero without changing anything.
static s32 _dryfieldDilapidatedHouseRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return 0;
}

/// Resolves the warehouse variant and blocks departures requiring room dialogue.
///
/// Borrows a complete request and writable reply, which may alias. Executed
/// Dryfield warehouse requests select room 1 before its event or room 2 after it.
/// Warehouse departures during battle and all back-street departures return 0;
/// execution starts their CAP events, while queries suppress them. Other areas
/// return 1 to permit the transition. The receiver and message ID are unused.
static s32 _dryfieldDilapidatedHouseResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        WAREHOUSE_ROOM_BEFORE_EVENT = 1,
        WAREHOUSE_ROOM_AFTER_EVENT  = 2,
        CAP_EVENT_BACK_STREET       = 0x13,
        CAP_EVENT_WAREHOUSE_BATTLE  = 0x14,
        TRANSITION_BLOCKED          = 0,
        TRANSITION_ALLOWED          = 1,
    };

    u8 stage;

    *reply = *request;
    stage  = gGameSession->location.stage;
    if (stage == GAME_STAGE_DRYFIELD) {
        if (request->areaId == GAME_AREA_DRYFIELD_WAREHOUSE) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_WAREHOUSE_EVENT_SEEN) == 0) {
                    reply->roomState = WAREHOUSE_ROOM_BEFORE_EVENT;
                } else {
                    reply->roomState = WAREHOUSE_ROOM_AFTER_EVENT;
                }
            }
        }
    }
    if ((request->areaId == GAME_AREA_DRYFIELD_WAREHOUSE) && (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED)) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capSpawnEventIfIdle(CAP_EVENT_WAREHOUSE_BATTLE, CAP_EVENT_NO_FLAGS);
        }
        return TRANSITION_BLOCKED;
    }
    if (request->areaId == GAME_AREA_DRYFIELD_BACK_STREET) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capSpawnEventIfIdle(CAP_EVENT_BACK_STREET, CAP_EVENT_NO_FLAGS);
        }
        return TRANSITION_BLOCKED;
    }
    return TRANSITION_ALLOWED;
}

/// Ignores room-command messages and returns zero; both payload words are unused.
static s32 _dryfieldDilapidatedHouseIgnoreRoomCommand(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return 0;
}

/// Starts the encounter's introductory script once for room action 1.
///
/// Borrows a four-byte action request during synchronous dispatch; only its
/// action ID is read. Latches the encounter before starting its script with
/// the skip sequence and HUD restoration. Always returns 0; other arguments
/// are unused, and later action-1 requests leave the encounter unchanged.
static s32 _dryfieldDilapidatedHouseStartEncounterMsg(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg)
{
    enum { ACTION_START_ENCOUNTER = 1 };

    u8 actionId;

    actionId = request->actionId;
    if ((actionId == ACTION_START_ENCOUNTER) && (_gDryfieldDilapidatedHouseEncounterPhase == DRYFIELD_DILAPIDATED_HOUSE_ENCOUNTER_NOT_STARTED)) {
        _gDryfieldDilapidatedHouseEncounterPhase = actionId;
        evsStartScriptWithSkip(_gDryfieldDilapidatedHouseEncounterIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldDilapidatedHouseEncounterIntroSkipScript);
    }
    return 0;
}

/// Aims the player's head at placed actor 1 while the cutscene enables tracking.
///
/// State 0 clears the first payload and enters idle state 1; state 2 applies
/// full-weight head aiming every frame. Both actors must have live head rigs.
/// This task borrows their models and remains alive when tracking is disabled.
static void _dryfieldDilapidatedHouseHeadTrackTask(Task* task)
{
    enum { HEAD_MAX_YAW   = 0x200,
           HEAD_MAX_PITCH = 0x180 }; // 4096 angle units per turn

    Task* playerTask;
    Task* targetTask;
    s32   state;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    targetTask = sceneFindPlacedActor(1);
    state      = task->state;
    switch (state) {
        case DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_INIT:
            task->spawnArg1.value = 0;
            task->state          += 1;
            return;
        case DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_ACTIVE:
            animationAimHeadAtTask(playerTask, targetTask, HEAD_MAX_YAW, HEAD_MAX_PITCH, ONE);
            /* fallthrough */
        case DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_IDLE:
            return;
    }
}

/// Alternates a decaying three-pixel vertical screen shake for a positive frame count.
///
/// `spawnArg1.value` is the duration, unchanged during playback. Initialization
/// seeds a shared countdown and returns without shaking. Only one instance may
/// run at a time; the task kills itself on the final shake frame.
static void _dryfieldDilapidatedHouseShakeYTask(Task* task)
{
    enum { SHAKE_INIT             = 0,
           SHAKE_RUNNING          = 1,
           SHAKE_AMPLITUDE_PIXELS = 3 };

    s32 framesLeft;
    s32 state;
    s32 shakePixels;

    state = task->state;
    switch (state) {
        case SHAKE_INIT:
            _gDryfieldDilapidatedHouseShakeFramesLeft = task->spawnArg1.value;
            task->state                              += 1;
            return;
        case SHAKE_RUNNING:
            shakePixels = (_gDryfieldDilapidatedHouseShakeFramesLeft * SHAKE_AMPLITUDE_PIXELS) / task->spawnArg1.value;
            if (_gDryfieldDilapidatedHouseShakeFramesLeft & 1) {
                shakePixels = -shakePixels;
            }
            displaySetShakeY(shakePixels);
            framesLeft                                = _gDryfieldDilapidatedHouseShakeFramesLeft - 1;
            _gDryfieldDilapidatedHouseShakeFramesLeft = framesLeft;
            if (framesLeft == 0) {
                taskKill(task);
            }
            return;
    }
}

/// Requests stage-mode exit after the spawn-argument countdown becomes negative.
///
/// A nonnegative `spawnArg1.value` takes that many ticks plus two to exit.
/// The retained countdown is reloaded and decremented even after task teardown;
/// this callback requires deferred task collection.
static void _dryfieldDilapidatedHouseModeExitCountdownTask(Task* task)
{
    s32 framesLeft;

    framesLeft = task->spawnArg1.value;
    if (framesLeft < 0) {
        stageRequestModeTaskExit();
        taskKill(task);
        framesLeft = task->spawnArg1.value;
    }
    framesLeft            = framesLeft - 1;
    task->spawnArg1.value = framesLeft;
}

/// Curve-debug states 0 initialize, 1 draw control points/samples, 2 kill.
///
/// Dispatch copies all three pointers and requires a state in 0..2. The room
/// never spawns this retained debug task; keep the table at its authored position.
static const TaskFuncTable3 _gDryfieldDilapidatedHouseCurveDebugStateHandlers = {
    { _dryfieldDilapidatedHouseCurveDebugInit, _dryfieldDilapidatedHouseCurveDebugUpdate, taskKill },
};

/// Capped-beam states 0 attach, 1 build/draw, 2 detach and kill.
///
/// Dispatch copies all three pointers and requires a state in 0..2. The child
/// borrows the morph task's model/work through teardown. Preserve table placement.
static const TaskFuncTable3 _gDryfieldDilapidatedHouseCappedBeamStateHandlers = {
    { _dryfieldDilapidatedHouseRingBeamInit, _dryfieldDilapidatedHouseCappedBeamUpdate,
      _dryfieldDilapidatedHouseRingBeamExit },
};

/// Morph-cone states 0 attach/seed, 1 build/draw, 2 detach and kill.
///
/// Dispatch copies all three pointers and requires a state in 0..2. The child
/// owns rim phases and borrows the morph task's model/work. Preserve placement.
static const TaskFuncTable3 _gDryfieldDilapidatedHouseMorphConeStateHandlers = {
    { _dryfieldDilapidatedHouseMorphConeInit, _dryfieldDilapidatedHouseMorphConeUpdate,
      _dryfieldDilapidatedHouseMorphConeExit },
};

/// Morph-attachment states 0 attach and seed, 1 morph and draw, 2 release the task.
///
/// Dispatch requires state 0..2 and copies all three callbacks. Explicit removal
/// kills the task through its installed exit callback; preserve rodata placement.
static const TaskFuncTable3 _gDryfieldDilapidatedHouseMorphAttachmentStateHandlers = {
    { _dryfieldDilapidatedHouseMorphAttachmentInit, _dryfieldDilapidatedHouseMorphAttachmentUpdate, taskKill },
};

/// Sets the cutscene's head tracking state, if its task exists.
///
/// Scripts pass 2 to enable tracking or 0 to reset it to idle on the next tick.
/// State 1 is already idle. Other values are stored unchanged and do no aiming.
static void _dryfieldDilapidatedHouseSetHeadTrackState(s32 state)
{
    if (_gDryfieldDilapidatedHouseHeadTrackTask != NULL) {
        _gDryfieldDilapidatedHouseHeadTrackTask->state = state;
    }
}

/// Requests ordinary and parasite-energy effect cancellation at the next update.
///
/// Used as the encounter script's cleanup callback with a live room-effect state.
static void _dryfieldDilapidatedHouseCancelEffects(void)
{
    roomEffectRequestCancelAll();
}

/// Prepares mask-bit image decoding, starts a screen wave, or changes its phase.
///
/// Request -2 only prepares RGB16 decoding with pixel bit 15 set. Zero starts
/// a 100-frame rise; other negative requests start a five-frame rise. Both
/// peak at strength 256 (eight pixels of sine displacement). Positive requests
/// narrow to the context's signed-halfword phase: 1 falling, 2 finished.
/// Starting requires no other active wave and loaded room/wave resources;
/// the room's shared context remains borrowed until the wave ends.
static void _dryfieldDilapidatedHouseControlScreenWave(s32 request)
{
    enum { WAVE_SLOW_RISE_FRAMES = 100,
           WAVE_FAST_RISE_FRAMES = 5,
           WAVE_PEAK_STRENGTH    = 256 };
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (request <= 0) {
        queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
        if (request != DRYFIELD_DILAPIDATED_HOUSE_WAVE_PREPARE_DECODE) {
            // Retain each branch's complete span/scale pair for this compiler.
            if (request == DRYFIELD_DILAPIDATED_HOUSE_WAVE_SLOW_RISE) {
                _gDryfieldDilapidatedHouseScreenWaveSpawnContext.span  = WAVE_SLOW_RISE_FRAMES;
                _gDryfieldDilapidatedHouseScreenWaveSpawnContext.scale = WAVE_PEAK_STRENGTH;
            } else {
                _gDryfieldDilapidatedHouseScreenWaveSpawnContext.span  = WAVE_FAST_RISE_FRAMES;
                _gDryfieldDilapidatedHouseScreenWaveSpawnContext.scale = WAVE_PEAK_STRENGTH;
            }
            taskSpawnFromTable(_gDryfieldDilapidatedHouseScreenWaveTaskDescriptors, 0, 0, &_gDryfieldDilapidatedHouseScreenWaveSpawnContext);
        }
    } else {
        _gDryfieldDilapidatedHouseScreenWaveSpawnContext.state = request;
    }
}

/// Arms the room's blackout countdown, or ends the frozen negative with zero.
///
/// A nonzero `delayFrames` enters timer state 2 with that signed frame count;
/// scripts supply 40 or 180. Zero resets the timer to state 0 and requests
/// capture completion. The room must have successfully spawned its timer task.
static void _dryfieldDilapidatedHouseSetBlackoutDelay(s32 delayFrames)
{
    enum { BLACKOUT_RESET = 0,
           BLACKOUT_ARM   = 2 };

    if (delayFrames == 0) {
        _gDryfieldDilapidatedHouseBlackoutTask->state      = BLACKOUT_RESET;
        _gDryfieldDilapidatedHouseNegativeCaptureArgs.done = true;
        return;
    }
    _gDryfieldDilapidatedHouseBlackoutTask->state           = BLACKOUT_ARM;
    _gDryfieldDilapidatedHouseBlackoutTask->spawnArg1.value = delayFrames;
}

/// Starts a negative freeze-frame with vibration, or requests its early release.
///
/// Nonzero durations store their low unsigned halfword before spawning the
/// capture; use 1..32767 frames for a positive signed task countdown. Zero
/// sets the shared completion flag. Starting requires no active capture;
/// the room's argument block and overlay must outlive the capture task.
static void _dryfieldDilapidatedHouseSetNegativeCapture(s32 durationFrames)
{
    if (durationFrames != 0) {
        padScriptSpawn(gPadScriptTransitionCommands, gPadScriptTransitionSegments);
        _gDryfieldDilapidatedHouseNegativeCaptureArgs.duration = durationFrames;
        taskSpawnFromTable(_gDryfieldDilapidatedHouseNegativeCaptureTaskDescriptors, 0, 0, &_gDryfieldDilapidatedHouseNegativeCaptureArgs);
        return;
    }
    _gDryfieldDilapidatedHouseNegativeCaptureArgs.done = true;
}

/// Starts or removes the player's morph attachment and its child light effects.
///
/// Nonzero starts a new model task on player coordinate 3; zero kills and
/// clears the tracked task, if any. Callers start once before removing it:
/// repeated starts overwrite the handle without releasing the earlier task.
/// The live player's model needs coordinate 3 and must outlive the attachment.
static void _dryfieldDilapidatedHouseSetMorphAttachment(s32 enabled)
{
    enum { MORPH_ATTACHMENT_TASK   = 0,
           PLAYER_ATTACHMENT_COORD = 3 };
    if (enabled != 0) {
        _gDryfieldDilapidatedHouseMorphAttachmentTask =
            taskSpawnFromTable(_gDryfieldDilapidatedHouseMorphTaskDescriptors, MORPH_ATTACHMENT_TASK, PLAYER_ATTACHMENT_COORD, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
        return;
    }
    if (_gDryfieldDilapidatedHouseMorphAttachmentTask != NULL) {
        taskKill(_gDryfieldDilapidatedHouseMorphAttachmentTask);
        _gDryfieldDilapidatedHouseMorphAttachmentTask = NULL;
    }
}

/// Cancels effects and locks Parasite Energy selection for the scripted event.
///
/// Requires live room-effect and attachment state. The cancellation takes effect
/// on the next update; the event lock remains set for later code to release.
static void _dryfieldDilapidatedHouseLockAttachmentsForEvent(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Registers the room controller and starts its persistent encounter helpers.
///
/// The bodyless task becomes the room message receiver. Spawn head tracking
/// only when placed actor 1 exists; always attempt the blackout timer and mark
/// the wave context finished. Music-suppression and weapon-reequip flow flags
/// replace the previous flags. Enter the running state even if a spawn fails.
static void _dryfieldDilapidatedHouseRoomInit(Task* task)
{
    enum { HEAD_TRACK_ACTOR_SLOT = 1,
           HEAD_TRACK_TASK       = 0,
           BLACKOUT_TASK         = 2 };
    task->msgTable = _gDryfieldDilapidatedHouseRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (sceneFindPlacedActor(HEAD_TRACK_ACTOR_SLOT) != 0) {
        _gDryfieldDilapidatedHouseHeadTrackTask =
            taskSpawnFromTable(_gDryfieldDilapidatedHouseEncounterTaskDescriptors, HEAD_TRACK_TASK, 0, 0);
    }
    _gDryfieldDilapidatedHouseScreenWaveSpawnContext.state = SCREEN_WAVE_RAMP_FINISHED;
    _gDryfieldDilapidatedHouseBlackoutTask =
        taskSpawnFromTable(_gDryfieldDilapidatedHouseEncounterTaskDescriptors, BLACKOUT_TASK, 0, 0);
    gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_REEQUIP_WEAPON);
    task->state            += 1;
}

void dryfieldDilapidatedHouseRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldDilapidatedHouseRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Draws red crosses at the debug path's eight local control-point entries.
///
/// Borrows the spawning morph task's attachment transform, then view-projects
/// signed-halfword world points. Queues two lines per entry at OT tag 10,
/// including the repeated closing point, without depth or GTE-flag rejection.
static void _dryfieldDilapidatedHouseDrawCurveControlPoints(Task* task)
{
    enum { CONTROL_POINT_HALF_SIZE_PIXELS = 5,
           DEBUG_OT_OFFSET                = 10 };

    struct {
        SVECTOR point;
        s32     screenWord;
        s32     depthCue;
        s32     projectionFlags;
        s32     depth;
    } projection;
    MATRIX*                             attachmentMtx;
    Task*                               parentTask;
    _DryfieldDilapidatedHouseMorphWork* parentWork;
    LINE_F2*                            line;
    u16                                 screenX;
    s32                                 screenY;
    s16                                 leftX;
    s16                                 topY;
    s16                                 rightX;
    s16                                 bottomY;
    s32                                 pointIndex;

    pointIndex    = 0;
    parentTask    = task->spawnArg2.pointer;
    parentWork    = parentTask->work;
    attachmentMtx = &parentWork->attachMtx;
    // Place markers in the attachment's frame, narrowing world XYZ to halfwords.
    do {
        projection.point.vx = _gDryfieldDilapidatedHouseDebugCurveControlPoints[pointIndex].vx;
        projection.point.vy = _gDryfieldDilapidatedHouseDebugCurveControlPoints[pointIndex].vy;
        projection.point.vz = _gDryfieldDilapidatedHouseDebugCurveControlPoints[pointIndex].vz;
        gte_SetRotMatrix(attachmentMtx);
        gte_ldv0(&projection.point);
        gte_rtv0();
        gte_stsv(&projection.point);
        projection.point.vx = (u16)projection.point.vx + (u16)attachmentMtx->t[0];
        projection.point.vy = (u16)projection.point.vy + (u16)attachmentMtx->t[1];
        projection.point.vz = (u16)projection.point.vz + (u16)attachmentMtx->t[2];
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&projection.point);
        gte_rtps();
        gte_stsxy(&projection.screenWord);
        gte_stdp(&projection.depthCue);
        gte_stflg(&projection.projectionFlags);
        gte_stszotz(&projection.depth);
        line           = gGpuPrimCursor;
        screenX        = projection.screenWord;
        screenY        = projection.screenWord >> 16;
        gGpuPrimCursor = line + 1;
        leftX          = screenX - CONTROL_POINT_HALF_SIZE_PIXELS;
        topY           = screenY - CONTROL_POINT_HALF_SIZE_PIXELS;
        rightX         = screenX + CONTROL_POINT_HALF_SIZE_PIXELS;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        bottomY  = screenY + CONTROL_POINT_HALF_SIZE_PIXELS;
        line->x0 = leftX;
        line->y0 = topY;
        line->x1 = rightX;
        line->y1 = bottomY;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        pointIndex++;
        line->x0 = rightX;
        line->y0 = topY;
        line->x1 = leftX;
        line->y1 = bottomY;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);
    } while (pointIndex < ARRAY_SIZE(_gDryfieldDilapidatedHouseDebugCurveControlPoints));
}

/// Marks each debug Bezier segment with 21 projected crosses, green then blue.
///
/// The segments share control point 3 and end at point 6. Samples run from
/// t = 0 to just below t = 1 using the reverse sampler. The spawning morph task
/// must stay live; world XYZ narrows to signed halfwords before view projection.
/// Queues two lines per sample at OT tag 10 without projection rejection.
static void _dryfieldDilapidatedHouseDrawCurveSamples(Task* task)
{
    enum { CURVE_SUBDIVISIONS = 20,
           DEBUG_OT_OFFSET    = 10 };

    SVECTOR                             point;
    DVECTOR                             screenX; // Only vx is used as a signed pixel coordinate.
    DVECTOR                             screenY; // Only vx is used; vy is never accessed.
    long                                sampleXyz[3];
    s32                                 screenWord;
    s32                                 depthCue;
    s32                                 projectionFlags;
    s32                                 depth;
    MATRIX*                             attachmentMtx;
    Task*                               parentTask;
    _DryfieldDilapidatedHouseMorphWork* parentWork;
    LINE_F2*                            line;
    s32                                 sampleIndex;

    /// Transforms and projects this function's local-space sample in place.
    ///
    /// Captures `attachmentMtx` and the writable `point`, `screenWord`,
    /// `depthCue`, `projectionFlags` and `depth` locals. XYZ narrows to signed
    /// halfwords before view projection; depth is camera Z / 4. Leaves the view
    /// transform in the GTE and does not reject flags. No arguments or hidden
    /// evaluation; use as a standalone statement. Undefined at the body's end.
#define DRYFIELD_DILAPIDATED_HOUSE_PROJECT_CURVE_SAMPLE()    \
    {                                                        \
        gte_SetRotMatrix(attachmentMtx);                     \
        gte_ldv0(&point);                                    \
        gte_rtv0();                                          \
        gte_stsv(&point);                                    \
        point.vx = (u16)point.vx + (u16)attachmentMtx->t[0]; \
        point.vy = (u16)point.vy + (u16)attachmentMtx->t[1]; \
        point.vz = (u16)point.vz + (u16)attachmentMtx->t[2]; \
        gte_SetRotMatrix(&gGfxViewCoord.workm);              \
        gte_SetTransMatrix(&gGfxViewCoord.workm);            \
        gte_ldv0(&point);                                    \
        gte_rtps();                                          \
        gte_stsxy(&screenWord);                              \
        gte_stdp(&depthCue);                                 \
        gte_stflg(&projectionFlags);                         \
        gte_stszotz(&depth);                                 \
    }

    parentTask    = task->spawnArg2.pointer;
    parentWork    = parentTask->work;
    attachmentMtx = &parentWork->attachMtx;
    // Keep the two segments distinct so their markers carry different colours.
    for (sampleIndex = CURVE_SUBDIVISIONS; sampleIndex >= 0; sampleIndex--) {
        _bezierCurveEvaluate(_gDryfieldDilapidatedHouseDebugCurveControlPoints, _gDryfieldDilapidatedHouseDebugCurveControlPoints + 3, CURVE_SUBDIVISIONS, sampleIndex, sampleXyz);
        point.vx = sampleXyz[0];
        point.vy = sampleXyz[1];
        point.vz = sampleXyz[2];
        DRYFIELD_DILAPIDATED_HOUSE_PROJECT_CURVE_SAMPLE();
        screenX.vx = screenWord;
        screenY.vx = screenWord >> 16;

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = screenX.vx - 1;
        line->y0 = screenY.vx - 1;
        line->x1 = screenX.vx + 1;
        line->y1 = screenY.vx + 1;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = screenX.vx + 1;
        line->y0 = screenY.vx - 1;
        line->x1 = screenX.vx - 1;
        line->y1 = screenY.vx + 1;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);
    }
    for (sampleIndex = CURVE_SUBDIVISIONS; sampleIndex >= 0; sampleIndex--) {
        _bezierCurveEvaluate(_gDryfieldDilapidatedHouseDebugCurveControlPoints + 3, _gDryfieldDilapidatedHouseDebugCurveControlPoints + 6, CURVE_SUBDIVISIONS, sampleIndex, sampleXyz);
        point.vx = sampleXyz[0];
        point.vy = sampleXyz[1];
        point.vz = sampleXyz[2];
        DRYFIELD_DILAPIDATED_HOUSE_PROJECT_CURVE_SAMPLE();
        screenX.vx = screenWord;
        screenY.vx = screenWord >> 16;

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = screenX.vx - 1;
        line->y0 = screenY.vx - 1;
        line->x1 = screenX.vx + 1;
        line->y1 = screenY.vx + 1;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = screenX.vx + 1;
        line->y0 = screenY.vx - 1;
        line->x1 = screenX.vx - 1;
        line->y1 = screenY.vx + 1;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);
    }
#undef DRYFIELD_DILAPIDATED_HOUSE_PROJECT_CURVE_SAMPLE
}

#include "../../shared/actor/bezier_curve_evaluate.inc.c"

/// Morph controller's Q12 brightness binding for the capped-beam drawer.
///
/// `beamTask` is a live Task* expression, evaluated once; no locals are captured.
/// Its `spawnArg2.pointer` borrows a live Task with initialized
/// `_DryfieldDilapidatedHouseMorphWork` at `work`; both must outlive drawing.
/// Returns the stored s32 beam level, the controller's morph progress (0..4096).
/// The drawer interprets its signed low halfword as Q12 (4096 is unit brightness),
/// then narrows RGB without saturation. Define before including
/// `glow_draw_ring_beam.inc.c`, which undefines it.
#define GLOW_CAPPED_BEAM_BRIGHTNESS(beamTask) \
    (((const _DryfieldDilapidatedHouseMorphWork*)((const Task*)(beamTask)->spawnArg2.pointer)->work)->beamLevel)
/// Halo angle increment for the morph's capped beam, in 4096 units/turn per draw.
///
/// Optional integer configuration for `glow_draw_ring_beam.inc.c`; only this
/// carrier supplies it. The drawer adds it to signed-halfword killCountdown
/// after geometry is built, then resets angles at or above a half turn.
/// The fragment undefines the binding; an absent binding leaves phase unchanged
/// before that same reset (as in actor_141000).
#define GLOW_CAPPED_BEAM_PHASE_STEP 0x40
/// Ordering-table tag bias for the morph attachment's capped-beam core and halo.
///
/// Required signed integer constant configuration for `glow_draw_ring_beam.inc.c`.
/// Adds three tags to the quantized depth entry for every quad and draw-mode
/// packet. Applied after byte-offset conversion to `u_long*`, so each unit is
/// one four-byte DMA tag. The selected table must contain the resulting entry;
/// the drawer neither clamps nor wraps the biased index. Has no arguments,
/// captured identifiers or side effects. Define before the fragment, which
/// undefines it after the draw function.
#define GLOW_CAPPED_BEAM_ORDERING_TABLE_TAG_BIAS 3
/// Complete quarter-additive GPU draw-mode word for the capped beam's halo.
///
/// Required 32-bit constant configuration for `glow_draw_ring_beam.inc.c`:
/// 0xE1000465, stored in `DR_TPAGE::code[0]` before each semitransparent halo
/// quad. Enables drawing in the display area, disables dithering and selects
/// B + F/4 blending. The encoded 4-bit page at VRAM word X=320, Y=0 is unused
/// by these untextured quads. Requires Psy-Q `libgpu.h` and
/// `main/display_types.h`; define before the fragment, which undefines it.
/// Has no arguments, captured locals or side effects.
#define GLOW_CAPPED_BEAM_HALO_DRAW_MODE _get_mode(true, false, getTPage(0, GPU_BLEND_ADD_QUARTER, 320, 0))
#include "../../shared/effects/glow_draw_ring_beam.inc.c"

/// Builds the morph attachment's projected core and halo beam caps.
///
/// Writes only X/Y of 24 caller-owned points: six-point core caps at each
/// endpoint, then the corresponding halo caps. Each cap contains its centre
/// followed by five rim points. Borrows the live spawning morph task's matrix
/// and signed low-halfword Q12 beam level. Spawn argument 1 equal to 1 mirrors
/// the local endpoints in X; the beam grows from quarter length to full length.
///
/// Both caps use the last endpoint's camera Z / 4 as their size divisor and
/// return it in `endDepth`; it must be nonzero. `projectionFlags` receives only
/// that endpoint's final FLAG, with no rejection. `killCountdown` supplies a
/// 4096-unit-per-turn halo pulse angle; this builder does not advance it.
static void _dryfieldDilapidatedHouseBuildCappedBeamPoints(Task* task, SVECTOR screenPoints[DRYFIELD_DILAPIDATED_HOUSE_BEAM_POINT_COUNT], s32* endDepth, s32* projectionFlags)
{
    SVECTOR                             startPoint;
    SVECTOR                             endPoint;
    MATRIX                              screenRotation;
    s32                                 startScreenWord;
    s32                                 depthCue;
    s32                                 endScreenWord;
    _DryfieldDilapidatedHouseMorphWork* work;
    MATRIX*                             attachmentMtx;
    const SVECTOR*                      endTemplate;
    s16                                 lengthQ12;
    s16                                 haloScaleQ12;
    s32                                 projectionDistance;
    s32                                 pointIndex;
    u16                                 brightnessQ12;
    s16                                 startX;
    s32                                 startY;
    s16                                 endX;
    s32                                 endY;
    s32                                 deltaX;
    s32                                 deltaY;
    s32                                 mirrorX;
    s32                                 screenAngle;
    Task*                               parentTask;

    mirrorX       = task->spawnArg1.value;
    parentTask    = task->spawnArg2.pointer;
    work          = parentTask->work;
    attachmentMtx = &work->attachMtx;
    brightnessQ12 = work->beamLevel;
    startPoint.vx = _gDryfieldDilapidatedHouseCappedBeamEndpoints[0].vx;
    startPoint.vy = _gDryfieldDilapidatedHouseCappedBeamEndpoints[0].vy;
    startPoint.vz = _gDryfieldDilapidatedHouseCappedBeamEndpoints[0].vz;
    endTemplate   = &_gDryfieldDilapidatedHouseCappedBeamEndpoints[1];
    endPoint.vx   = endTemplate->vx;
    endPoint.vy   = endTemplate->vy;
    endPoint.vz   = endTemplate->vz;
    if (mirrorX == 1) {
        startPoint.vx *= -1;
        endPoint.vx   *= -1;
    }
    gte_SetRotMatrix(attachmentMtx);
    gte_ldv0(&startPoint);
    gte_rtv0();
    gte_stsv(&startPoint);
    gte_ldv0(&endPoint);
    gte_rtv0();
    gte_stsv(&endPoint);
    lengthQ12      = (s16)brightnessQ12 * 0.75 + ONE / 4;
    endPoint.vx    = startPoint.vx + (endPoint.vx - startPoint.vx) * lengthQ12 / ONE;
    endPoint.vy    = startPoint.vy + (endPoint.vy - startPoint.vy) * lengthQ12 / ONE;
    endPoint.vz    = startPoint.vz + (endPoint.vz - startPoint.vz) * lengthQ12 / ONE;
    startPoint.vx += attachmentMtx->t[0];
    startPoint.vy += attachmentMtx->t[1];
    startPoint.vz += attachmentMtx->t[2];
    endPoint.vx   += attachmentMtx->t[0];
    endPoint.vy   += attachmentMtx->t[1];
    endPoint.vz   += attachmentMtx->t[2];
    // Project both ends. Each screen point comes back packed, x in the low
    // half and y in the high; the depth-cue coefficient is not used.
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_RotTransPers(&startPoint, &startScreenWord, &depthCue, projectionFlags, endDepth);
    gte_RotTransPers(&endPoint, &endScreenWord, &depthCue, projectionFlags, endDepth);
    deltaY             = (startScreenWord >> 16) - (endScreenWord >> 16);
    endX               = endScreenWord;
    startX             = startScreenWord;
    deltaX             = endX - startX;
    startY             = startScreenWord >> 16;
    endY               = endScreenWord >> 16;
    screenAngle        = ratan2(deltaX, deltaY);
    projectionDistance = gDisplayState.screenDistance;
    gfxSetRotIdentity(&screenRotation);
    RotMatrixZ(screenAngle, &screenRotation);
    gte_SetRotMatrix(&screenRotation);
    // Reuse the endpoint vectors for cap offsets, retaining the start point's world Z.
    // Rotate their XY contours along the projected segment.
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(_gDryfieldDilapidatedHouseCappedBeamStartCapOffsets); pointIndex++) {
        startPoint.vx = _gDryfieldDilapidatedHouseCappedBeamStartCapOffsets[pointIndex].vx * projectionDistance / *endDepth;
        startPoint.vy = _gDryfieldDilapidatedHouseCappedBeamStartCapOffsets[pointIndex].vy * projectionDistance / *endDepth;
        gte_ldv0(&startPoint);
        gte_rtv0();
        gte_stsv(&endPoint);
        screenPoints[pointIndex].vx = endPoint.vx + startX;
        screenPoints[pointIndex].vy = endPoint.vy + startY;
    }
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(_gDryfieldDilapidatedHouseCappedBeamEndCapOffsets); pointIndex++) {
        startPoint.vx = _gDryfieldDilapidatedHouseCappedBeamEndCapOffsets[pointIndex].vx * projectionDistance / *endDepth;
        startPoint.vy = _gDryfieldDilapidatedHouseCappedBeamEndCapOffsets[pointIndex].vy * projectionDistance / *endDepth;
        gte_ldv0(&startPoint);
        gte_rtv0();
        gte_stsv(&endPoint);
        screenPoints[pointIndex + DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vx = endPoint.vx + endX;
        screenPoints[pointIndex + DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vy = endPoint.vy + endY;
    }
    // Pulse enlarged halo caps around the same endpoints and depth as the core.
    haloScaleQ12 = ONE - rsin(task->killCountdown) * 0.5 + ONE;
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(_gDryfieldDilapidatedHouseCappedBeamStartCapOffsets); pointIndex++) {
        startPoint.vx = ((_gDryfieldDilapidatedHouseCappedBeamStartCapOffsets[pointIndex].vx * haloScaleQ12) >> GLOW_TRIG_SHIFT) * projectionDistance / *endDepth;
        startPoint.vy = ((_gDryfieldDilapidatedHouseCappedBeamStartCapOffsets[pointIndex].vy * haloScaleQ12) >> GLOW_TRIG_SHIFT) * projectionDistance / *endDepth;
        gte_ldv0(&startPoint);
        gte_rtv0();
        gte_stsv(&endPoint);
        screenPoints[pointIndex + 2 * DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vx = endPoint.vx + startX;
        screenPoints[pointIndex + 2 * DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vy = endPoint.vy + startY;
    }
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(_gDryfieldDilapidatedHouseCappedBeamEndCapOffsets); pointIndex++) {
        startPoint.vx = ((_gDryfieldDilapidatedHouseCappedBeamEndCapOffsets[pointIndex].vx * haloScaleQ12) >> GLOW_TRIG_SHIFT) * projectionDistance / *endDepth;
        startPoint.vy = ((_gDryfieldDilapidatedHouseCappedBeamEndCapOffsets[pointIndex].vy * haloScaleQ12) >> GLOW_TRIG_SHIFT) * projectionDistance / *endDepth;
        gte_ldv0(&startPoint);
        gte_rtv0();
        gte_stsv(&endPoint);
        screenPoints[pointIndex + 3 * DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vx = endPoint.vx + endX;
        screenPoints[pointIndex + 3 * DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vy = endPoint.vy + endY;
    }
}

/// Projects two consecutive cone vertices and the vertex one ring later.
///
/// The GTE must hold the view transform. Store packed screen words and the
/// final outer corner's SZ3 / 4. `vertex` and `screen` each provide indices
/// 0, 1 and 16 from the current cursor; `depth` provides one writable word.
/// At the last inner vertex, index 1 is the first outer vertex rather than the
/// closing inner vertex. The drawer closes that edge with its saved first point.
static inline void _dryfieldDilapidatedHouseProjectConeSegment(const SVECTOR* vertex, s32* screen, s32* depth)
{
    gte_ldv3(vertex, vertex + 1, vertex + DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT);
    gte_rtpt();
    gte_stsxy3(screen, screen + 1, screen + DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT);
    gte_stszotz(depth);
}

/// Draws the morph attachment's yellow-to-black cone with quarter-additive quads.
///
/// `ringVertices` supplies 32 world-space vertices: 16 inner, then 16 outer.
/// The borrowed parent task supplies `coneLevel`; values up to 0x400 map to
/// byte intensity, with larger values clamped. Each segment sorts four OT
/// entries beyond its outer vertex's SZ3 / 4, without projection rejection.
static void _dryfieldDilapidatedHouseDrawMorphCone(Task* task, const SVECTOR* ringVertices)
{
    enum {
        CONE_RING_VERTEX_COUNT = DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT,
        CONE_BRIGHTNESS_ONE    = 0x400,
        CONE_MAX_INTENSITY     = 0xFF,
        CONE_OT_OFFSET         = 4,
        CONE_DRAW_MODE_COMMAND = 0xE1000400,
    };

    s32                                 screenPositions[2 * CONE_RING_VERTEX_COUNT];
    s32                                 depths[CONE_RING_VERTEX_COUNT];
    CVECTOR                             innerColor;
    CVECTOR                             outerColor;
    POLY_G4*                            quad;
    DR_TPAGE*                           drawMode;
    s16                                 brightness;
    s32                                 segmentIndex;
    s32*                                screenWord;
    const SVECTOR*                      vertexCursor;
    s32*                                screenCursor;
    s32*                                depthCursor;
    Task*                               parentTask;
    _DryfieldDilapidatedHouseMorphWork* parentWork;

    vertexCursor = ringVertices;
    screenCursor = screenPositions;
    depthCursor  = depths;
    parentTask   = task->spawnArg2.pointer;
    parentWork   = parentTask->work;
    brightness   = parentWork->coneLevel;
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    // Share projected ring corners between adjacent segments.
    for (segmentIndex = 0; segmentIndex < CONE_RING_VERTEX_COUNT; segmentIndex++) {
        _dryfieldDilapidatedHouseProjectConeSegment(vertexCursor, screenCursor, depthCursor);
        screenCursor++;
        depthCursor++;
        vertexCursor++;
    }
    if (brightness > CONE_BRIGHTNESS_ONE) {
        brightness = CONE_BRIGHTNESS_ONE;
    }
    innerColor.r = brightness * CONE_MAX_INTENSITY / CONE_BRIGHTNESS_ONE;
    innerColor.g = brightness * CONE_MAX_INTENSITY / CONE_BRIGHTNESS_ONE;
    innerColor.b = 0;
    outerColor.r = 0;
    outerColor.g = 0;
    outerColor.b = 0;
    for (segmentIndex = 0; segmentIndex < CONE_RING_VERTEX_COUNT; segmentIndex++) {
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyG4(quad);
        setSemiTrans(quad, 1);
        quad->r0 = innerColor.r;
        quad->g0 = innerColor.g;
        quad->b0 = innerColor.b;
        quad->r1 = innerColor.r;
        quad->g1 = innerColor.g;
        quad->b1 = innerColor.b;
        quad->r2 = outerColor.r;
        quad->g2 = outerColor.g;
        quad->b2 = outerColor.b;
        quad->r3 = outerColor.r;
        quad->g3 = outerColor.g;
        quad->b3 = outerColor.b;
        // Packed screen words, one per vertex; each `screenWord` word is two words past
        // the previous one because a colour word sits between them.
        screenWord = (s32*)&quad->x0;
        if (segmentIndex < CONE_RING_VERTEX_COUNT - 1) {
            screenWord[0] = screenPositions[segmentIndex];
            screenWord[2] = screenPositions[segmentIndex + 1];
            screenWord[4] = screenPositions[segmentIndex + CONE_RING_VERTEX_COUNT];
            screenWord[6] = screenPositions[segmentIndex + CONE_RING_VERTEX_COUNT + 1];
        } else {
            screenWord[0] = screenPositions[CONE_RING_VERTEX_COUNT - 1];
            screenWord[2] = screenPositions[0];
            screenWord[4] = screenPositions[2 * CONE_RING_VERTEX_COUNT - 1];
            screenWord[6] = screenPositions[CONE_RING_VERTEX_COUNT];
        }
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depths[segmentIndex] << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + CONE_OT_OFFSET, quad);
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, 1);
        drawMode->code[0] = CONE_DRAW_MODE_COMMAND | getTPage(0, GPU_BLEND_ADD_QUARTER, 320, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depths[segmentIndex] << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + CONE_OT_OFFSET, drawMode);
    }
}

/// Rebuilds two sixteen-vertex world-space ellipses for the morph cone.
///
/// Writes XYZ of 32 borrowed output vectors: inner ring first, then outer.
/// The spawning morph task supplies the attachment frame and must stay live.
/// Each outer Z ripples by its per-vertex phase, advanced once per call in
/// 16384 units per turn. Rotation, centre offsets and translation narrow to
/// signed halfwords; each output's fourth halfword remains untouched.
static void _dryfieldDilapidatedHouseBuildMorphConeRings(Task* task, SVECTOR* ringVertices)
{
    enum {
        CONE_RING_VERTEX_COUNT = DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT,
        INNER_RADIUS_X         = 150,
        INNER_RADIUS_Y         = 75,
        OUTER_RADIUS_X         = 250,
        OUTER_RADIUS_Y         = 125,
        RIM_RIPPLE_AMPLITUDE   = 100,
    }; // Radii and ripple amplitude are attachment-local coordinate units.

    _DryfieldDilapidatedHouseConeWork*  coneWork;
    _DryfieldDilapidatedHouseMorphWork* parentWork;
    MATRIX*                             attachmentMtx;
    const SVECTOR*                      innerCentre;
    const SVECTOR*                      outerCentre;
    SVECTOR                             ringCentres[2];
    SVECTOR*                            innerVertex;
    SVECTOR*                            outerVertex;
    s16                                 translationX;
    s16                                 translationY;
    s16                                 translationZ;
    s32                                 vertexIndex;
    s32                                 angle;
    s32                                 cosineQ12;
    s32                                 sineQ12;
    Task*                               parentTask;

    innerVertex = ringVertices;
    outerVertex = &ringVertices[CONE_RING_VERTEX_COUNT];

    coneWork   = task->work;
    parentTask = task->spawnArg2.pointer;
    parentWork = parentTask->work;

    innerCentre       = _gDryfieldDilapidatedHouseConeRingCenters;
    outerCentre       = _gDryfieldDilapidatedHouseConeRingCenters + 1;
    ringCentres[0].vx = innerCentre->vx;
    ringCentres[0].vy = innerCentre->vy;
    ringCentres[0].vz = innerCentre->vz;
    ringCentres[1].vx = outerCentre->vx;
    ringCentres[1].vy = outerCentre->vy;
    ringCentres[1].vz = outerCentre->vz;

    attachmentMtx = &parentWork->attachMtx;

    translationX = attachmentMtx->t[0];
    translationY = attachmentMtx->t[1];
    translationZ = attachmentMtx->t[2];

    gte_SetRotMatrix(attachmentMtx);

    // Walk both rings together; only the outer ring's axial coordinate ripples.
    for (vertexIndex = 0; vertexIndex < CONE_RING_VERTEX_COUNT; vertexIndex++) {
        angle     = (vertexIndex << GLOW_TRIG_SHIFT) >> 4;
        cosineQ12 = rcos(angle);
        sineQ12   = rsin(angle);

        innerVertex->vx = ringCentres[0].vx + ((cosineQ12 * INNER_RADIUS_X) >> GLOW_TRIG_SHIFT);
        innerVertex->vy = ringCentres[0].vy + ((sineQ12 * INNER_RADIUS_Y) >> GLOW_TRIG_SHIFT);
        innerVertex->vz = ringCentres[0].vz;

        gte_ldv0(innerVertex);
        gte_rtv0();
        gte_stsv(innerVertex);

        innerVertex->vx += translationX;
        innerVertex->vy += translationY;
        innerVertex->vz += translationZ;
        innerVertex++;

        outerVertex->vx = ringCentres[1].vx + ((cosineQ12 * OUTER_RADIUS_X) >> GLOW_TRIG_SHIFT);
        outerVertex->vy = ringCentres[1].vy + ((sineQ12 * OUTER_RADIUS_Y) >> GLOW_TRIG_SHIFT);
        // Ripple the outer ring along its axis, then advance that vertex's phase.
        outerVertex->vz = ringCentres[1].vz + ((rsin(coneWork->rimPhase[vertexIndex] >> 2) * RIM_RIPPLE_AMPLITUDE) >> GLOW_TRIG_SHIFT);

        coneWork->rimPhase[vertexIndex] = (coneWork->rimPhase[vertexIndex] + _gDryfieldDilapidatedHouseConeRimPhaseRates[vertexIndex]) &
                                          (DRYFIELD_DILAPIDATED_HOUSE_CONE_RIM_PHASE_PERIOD - 1);

        gte_ldv0(outerVertex);
        gte_rtv0();
        gte_stsv(outerVertex);

        outerVertex->vx += translationX;
        outerVertex->vy += translationY;
        outerVertex->vz += translationZ;
        outerVertex++;
    }
}

#include "../../shared/actor/model_morph_blend.inc.c"

#include "../../shared/actor/model_morph_save_rest_shape.inc.c"

/// Initializes the player's morphing model and its two beams and two cones.
///
/// Argument 2 borrows a live TMD parent; argument 1 selects one of its model
/// coordinates (the script uses 3). Owns a 0x6C-byte primary-heap work block and
/// joins the parent's task tree. Allocation failure kills the task. The shared
/// morph snapshots 40 XYZ vertices, leaves normals alone, then advances once.
/// Children borrow this task/work until teardown; failed child spawns are skipped.
/// Their initial matrix copies retain the original uninitialized work contents;
/// the attachment matrix and remaining levels are first filled by the update.
static void _dryfieldDilapidatedHouseMorphAttachmentInit(Task* task)
{
    enum { ATTACHMENT_OT_OFFSET     = 4,
           CONE_TASK                = 3,
           BEAM_TASK                = 2,
           FIRST_CONE_PHASE_FRAMES  = 9,
           SECOND_CONE_PHASE_FRAMES = 17,
           BEAM_UNMIRRORED          = 0,
           BEAM_MIRRORED            = 1 };
    Task*                               parentTask;
    TmdObject*                          model;
    TmdObject*                          parentModel;
    GfxCoord*                           modelCoord;
    GfxCoord*                           attachmentCoord;
    _DryfieldDilapidatedHouseMorphWork* work;
    TaskDesc*                           effectTasks;
    Task*                               childTask;
    GfxCoord*                           childCoord;
    u16                                 modelFlags;

    parentTask      = task->spawnArg2.pointer;
    model           = task->extra.tmd;
    parentModel     = parentTask->extra.tmd;
    modelCoord      = model->coords;
    attachmentCoord = parentModel->coords;
    work            = memMalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    // Borrow the selected player model part and its lighting, owning our work.
    task->work       = work;
    work->morphLevel = 0;
    modelFlags       = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    model->flags     = modelFlags;
    if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        model->flags = modelFlags & ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    model->otOffset          = ATTACHMENT_OT_OFFSET;
    model->flags            |= TMD_OBJECT_SEMI_TRANS;
    attachmentCoord         += task->spawnArg1.value;
    modelCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    modelCoord->parent       = attachmentCoord;
    model->lightMtx          = parentModel->lightMtx;
    model->colorMtx          = parentModel->colorMtx;
    taskReparent(parentTask, task);

    // Preserve the shared rest geometry before applying the initial deformation.
    _modelMorphSaveRestShape(task, &_gDryfieldDilapidatedHouseAttachmentMorph);

    _dryfieldDilapidatedHouseAdvanceMorph(task);

    // The child descriptors allocate coordinate bodies, not model bodies.
    effectTasks = _gDryfieldDilapidatedHouseMorphTaskDescriptors;
    childTask   = taskSpawnFromTable(effectTasks, CONE_TASK, FIRST_CONE_PHASE_FRAMES, task);
    if (childTask != NULL) {
        childCoord        = childTask->extra.coordBody->coord;
        childCoord->coord = work->attachMtx;
    }
    childTask = taskSpawnFromTable(effectTasks, CONE_TASK, SECOND_CONE_PHASE_FRAMES, task);
    if (childTask != NULL) {
        childCoord        = childTask->extra.coordBody->coord;
        childCoord->coord = work->attachMtx;
    }
    childTask = taskSpawnFromTable(effectTasks, BEAM_TASK, BEAM_UNMIRRORED, task);
    if (childTask != NULL) {
        childCoord        = childTask->extra.coordBody->coord;
        childCoord->coord = work->attachMtx;
    }
    childTask = taskSpawnFromTable(effectTasks, BEAM_TASK, BEAM_MIRRORED, task);
    if (childTask != NULL) {
        childCoord        = childTask->extra.coordBody->coord;
        childCoord->coord = work->attachMtx;
    }

    task->exitCallback = _dryfieldDilapidatedHouseMorphExit;
    task->state       += 1;
}

/// Runs the morphing model attachment and its beam and cone children.
///
/// `spawnArg2.pointer` borrows a live parent model task; `spawnArg1.value`
/// selects an existing coordinate in that model. States are 0 setup, 1 update,
/// and 2 teardown; other indices are invalid. The parent and shared morph record
/// must outlive updates. Teardown releases the children and owned work block.
static void _dryfieldDilapidatedHouseMorphAttachmentTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldDilapidatedHouseMorphAttachmentStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Updates the attachment's visibility, transform and shared Q12 morph progress.
///
/// Requires setup-owned work, saved rest vertices and a live spawning model.
/// The parent coordinate chain is composed into the attachment matrix; progress
/// advances by 68 toward `ONE` and drives both beam and cone children.
static void _dryfieldDilapidatedHouseMorphAttachmentUpdate(Task* task)
{
    _DryfieldDilapidatedHouseMorphWork* work;
    Task*                               parentTask;
    s32                                 morphProgress;

    work       = task->work;
    parentTask = task->spawnArg2.pointer;
    _dryfieldDilapidatedHouseCopyModelVisibility(task->extra.tmd,
                                                 parentTask->extra.tmd);
    _dryfieldDilapidatedHouseUpdateAttachmentTransform(task);
    morphProgress    = _dryfieldDilapidatedHouseAdvanceMorph(task);
    work->morphLevel = morphProgress;
    work->coneLevel  = morphProgress;
    work->beamLevel  = morphProgress;
}

/// Releases the morph attachment task through ordinary task teardown.
static void _dryfieldDilapidatedHouseMorphExit(Task* task)
{
    taskKill(task);
}

/// Advances morph progress toward `ONE` and applies the complementary deformation.
///
/// The task owns a live morph model with its rest vertices already saved.
/// `killCountdown` holds signed Q12 progress, normally 0..`ONE`; each call
/// adds 68 and saturates above `ONE`, without clamping negative input.
/// Returns the full-width progress after storing its low signed halfword.
static s32 _dryfieldDilapidatedHouseAdvanceMorph(Task* task)
{
    enum { MORPH_PROGRESS_STEP_Q12 = 68 };

    s32 ramp;

    ramp = task->killCountdown + MORPH_PROGRESS_STEP_Q12;
    if (ramp >= ONE + 1) {
        ramp = ONE;
    }
    task->killCountdown = ramp;
    _modelMorphBlend(task, &_gDryfieldDilapidatedHouseAttachmentMorph, ONE - ramp);
    return ramp;
}

/// Rebuilds the transform of the parent model part carrying the morph attachment.
///
/// The attachment's coordinate parent must be in its spawning model's contiguous
/// part array. Compose local transforms from the first part through that part,
/// inclusively, into the work block's `attachMtx` (12 fractional rotation bits
/// and whole-coordinate translation). This walk follows array order, not links.
static void _dryfieldDilapidatedHouseUpdateAttachmentTransform(Task* task)
{
    VECTOR                              rotatedTranslation;
    GfxCoord*                           modelCoord;
    _DryfieldDilapidatedHouseMorphWork* work;
    GfxCoord*                           parentCoord;
    MATRIX*                             attachmentMtx;
    Task*                               parentTask;

    modelCoord    = task->extra.tmd->coords;
    work          = task->work;
    parentTask    = task->spawnArg2.pointer;
    parentCoord   = parentTask->extra.tmd->coords;
    attachmentMtx = &work->attachMtx;
    gfxSetRotIdentity(&work->attachMtx);
    attachmentMtx->t[0] = 0;
    attachmentMtx->t[1] = 0;
    attachmentMtx->t[2] = 0;
    do {
        // The SDK vector call reads the three translation words, not a fourth word.
        ApplyMatrixLV(attachmentMtx, (VECTOR*)parentCoord->coord.t, &rotatedTranslation);
        attachmentMtx->t[0] += rotatedTranslation.vx;
        attachmentMtx->t[1] += rotatedTranslation.vy;
        attachmentMtx->t[2] += rotatedTranslation.vz;
        MulMatrix0(attachmentMtx, &parentCoord->coord, attachmentMtx);
    } while (parentCoord++ != modelCoord->parent);
}

/// Copies the parent's active-draw exclusion bit while preserving other model flags.
static void _dryfieldDilapidatedHouseCopyModelVisibility(TmdObject* model, const TmdObject* parentModel)
{
    if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

/// Dispatches the hidden-model task that marks the attachment's debug Bezier path.
///
/// States 0, 1 and 2 initialize, draw markers and kill the task, respectively.
/// The spawn payload borrows the live morph task. The room retains this
/// descriptor but never spawns it; dispatch preserves the three-handler copy.
static void _dryfieldDilapidatedHouseCurveDebugTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldDilapidatedHouseCurveDebugStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Initializes the hidden model task that marks the attachment's Bezier curve.
///
/// Snapshots its initial local matrix and seeds an otherwise unread word to
/// `ONE`; the word's purpose is unproven. The task owns the allocation and
/// becomes a child of the borrowed spawning task. Allocation failure kills it.
static void _dryfieldDilapidatedHouseCurveDebugInit(Task* task)
{
    TmdObject*                               model;
    GfxCoord*                                coord;
    _DryfieldDilapidatedHouseCurveDebugWork* work;

    model = task->extra.tmd;
    coord = model->coords;
    work  = memMalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work       = work;
    work->field_20   = ONE;
    work->initialMtx = coord->coord;
    model->flags    |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    taskReparent(task->spawnArg2.pointer, task);
    task->state += 1;
}

/// Draws the debug path's control-point crosses and both sampled segments.
static void _dryfieldDilapidatedHouseCurveDebugUpdate(Task* task)
{
    _dryfieldDilapidatedHouseDrawCurveControlPoints(task);
    _dryfieldDilapidatedHouseDrawCurveSamples(task);
}

#include "../../shared/actor/bezier_curve_coefficients.inc.c"

/// Dispatches a capped beam attached to the spawning morph model.
///
/// Requires a coordinate body and a live morph task in `spawnArg2.pointer`;
/// argument 1 equal to 1 mirrors the endpoints. States 0, 1 and 2 initialize,
/// build/draw and detach/kill. Retains the stack copy of the three handlers.
static void _dryfieldDilapidatedHouseCappedBeamTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldDilapidatedHouseCappedBeamStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Attaches a ring-beam coordinate task beneath the spawning morph model.
///
/// Retains a four-byte primary-heap work allocation whose contents are never
/// accessed. Teardown owns that allocation. Failure kills the task; success
/// installs coordinate detachment as its exit callback and enters the draw state.
static void _dryfieldDilapidatedHouseRingBeamInit(Task* task)
{
    enum { RING_BEAM_UNUSED_WORK_BYTES = 4 };

    GfxCoord* coord;
    void*     unusedWork;
    Task*     parentTask;

    coord      = task->extra.coordBody->coord;
    unusedWork = memMalloc(RING_BEAM_UNUSED_WORK_BYTES, false);
    if (unusedWork == NULL) {
        taskKill(task);
        return;
    }
    task->work    = unusedWork;
    parentTask    = task->spawnArg2.pointer;
    coord->parent = parentTask->extra.tmd->coords;
    taskReparent(task->spawnArg2.pointer, task);
    task->exitCallback = _dryfieldDilapidatedHouseRingBeamExit;
    task->state       += 1;
}

/// Builds one capped beam and queues two draws of the same projected geometry.
///
/// The final endpoint depth sorts both draws; projection flags are unused.
/// Each draw advances the next geometry phase by 64, wrapping at half a turn.
static void _dryfieldDilapidatedHouseCappedBeamUpdate(Task* task)
{
    SVECTOR screenPoints[DRYFIELD_DILAPIDATED_HOUSE_BEAM_POINT_COUNT];
    s32     endDepth;
    s32     projectionFlags;

    _dryfieldDilapidatedHouseBuildCappedBeamPoints(task, screenPoints, &endDepth, &projectionFlags);
    // The repeated draw also repeats the pulse-phase advance for the next frame.
    _glowDrawCappedBeam(task, screenPoints, endDepth);
    _glowDrawCappedBeam(task, screenPoints, endDepth);
}

/// Detaches the ring beam from its parent model before releasing the task.
static void _dryfieldDilapidatedHouseRingBeamExit(Task* task)
{
    GfxCoord* coord;

    coord         = task->extra.coordBody->coord;
    coord->parent = &gGfxViewCoord;
    taskKill(task);
}

/// Runs the morph cone's initialization, build/draw or detachment state.
///
/// `task->state` must be 0, 1 or 2; dispatch is unchecked. Start with a live
/// coordinate body, argument 1's phase advance in frames and argument 2's
/// spawning morph-model task. Initialization owns the rim-phase work block
/// and links the cone to that parent's teardown tree; keep both tasks and
/// this overlay live until the coordinate is detached and the cone released.
static void _dryfieldDilapidatedHouseMorphConeTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldDilapidatedHouseMorphConeStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Attaches the morph cone's coordinate and seeds its sixteen rim phases.
///
/// Requires a coordinate body and the live spawning morph task in argument 2.
/// Argument 1 is the initial phase advance in frames (the two instances use
/// 9 and 17). Owns a primary-heap phase block; allocation failure kills the
/// task, while success attaches it to the parent's teardown tree and enters
/// the per-frame build/draw state. Phase arithmetic wraps to 16384 units/turn.
static void _dryfieldDilapidatedHouseMorphConeInit(Task* task)
{
    _DryfieldDilapidatedHouseConeWork* work;
    GfxCoord*                          coord;
    s32                                vertexIndex;
    Task*                              parentTask;

    coord = task->extra.coordBody->coord;
    work  = memMalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work = work;
    for (vertexIndex = 0; vertexIndex < ARRAY_SIZE(work->rimPhase); vertexIndex++) {
        work->rimPhase[vertexIndex] = (_gDryfieldDilapidatedHouseConeRimPhaseRates[vertexIndex] * task->spawnArg1.value) &
                                      (DRYFIELD_DILAPIDATED_HOUSE_CONE_RIM_PHASE_PERIOD - 1);
    }
    parentTask    = task->spawnArg2.pointer;
    coord->parent = parentTask->extra.tmd->coords;
    taskReparent(task->spawnArg2.pointer, task);
    task->state += 1;
}

/// Rebuilds the morph cone's two rings, advancing its ripple, then draws the cone.
static void _dryfieldDilapidatedHouseMorphConeUpdate(Task* task)
{
    SVECTOR ringVertices[2 * DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT];

    _dryfieldDilapidatedHouseBuildMorphConeRings(task, ringVertices);
    _dryfieldDilapidatedHouseDrawMorphCone(task, ringVertices);
}

/// Detaches the morph cone's coordinate before releasing the task and rim phases.
static void _dryfieldDilapidatedHouseMorphConeExit(Task* task)
{
    GfxCoord* coord;

    coord         = task->extra.coordBody->coord;
    coord->parent = &gGfxViewCoord;
    taskKill(task);
}

/// Draws one pulsing additive light prism, with four sides and a lit end cap.
///
/// `firstVertex` is 0, 8 or 16 in the room's corner table. Its first four
/// vertices are lit; its last four are black. `coord->workm` must be composed.
/// Corners narrow to 16-bit world coordinates before view projection; there is
/// no near-plane or GTE-flag rejection. Scratch storage is released on return.
static void _dryfieldDilapidatedHouseDrawLightPrism(GfxCoord* coord, s16 firstVertex)
{
    /// Transforms a local prism corner into the scratch quad's world coordinates.
    ///
    /// Captures `coord` (composed GfxCoord*) and `scratch` (EffectQuadCornersScratch*).
    /// GTE rotation must already be `coord->workm`. Arguments have no side effects:
    /// worldIndex is 0..3 and evaluated repeatedly; vertexIndex is 0..23 and
    /// evaluated once. Each axis narrows to s16. Expands several statements, so
    /// use only as a standalone sequence here, never as an unbraced branch body.
#define DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(vertexIndex, worldIndex)                         \
    gte_ldv0(&_gDryfieldDilapidatedHouseLightPrismVertices[(vertexIndex)]);                                \
    gte_rtv0();                                                                                            \
    gte_stsv(&scratch->vertices[(worldIndex)]);                                                            \
    scratch->vertices[(worldIndex)].vx = (u16)scratch->vertices[(worldIndex)].vx + (u16)coord->workm.t[0]; \
    scratch->vertices[(worldIndex)].vy = (u16)scratch->vertices[(worldIndex)].vy + (u16)coord->workm.t[1]; \
    scratch->vertices[(worldIndex)].vz = (u16)scratch->vertices[(worldIndex)].vz + (u16)coord->workm.t[2]

    enum {
        PRISM_PULSE_ANGLE_SHIFT     = 10,
        PRISM_PULSE_INTENSITY_SHIFT = 11,
        PRISM_BASE_INTENSITY        = 20,
    };

    EffectQuadCornersScratch* scratch;
    POLY_G4*                  quad;
    s32                       cornerIndex;
    s32                       nextCorner;
    s32                       farCorner;
    s32                       farNextCorner;
    u8                        intensity;

    SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    scratch = SCRATCH_STACK_CURSOR(EffectQuadCornersScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    intensity = (rsin(gDisplayState.animFrame << PRISM_PULSE_ANGLE_SHIFT) >> PRISM_PULSE_INTENSITY_SHIFT) + PRISM_BASE_INTENSITY;
    // Each side joins adjacent lit corners to the corresponding dark corners.
    for (cornerIndex = 0; cornerIndex < DRYFIELD_DILAPIDATED_HOUSE_PRISM_RING_CORNERS; cornerIndex++) {
        gte_SetRotMatrix(&coord->workm);
        DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + cornerIndex, 0);
        gte_SetRotMatrix(&coord->workm);
        nextCorner = (cornerIndex + 1) & (DRYFIELD_DILAPIDATED_HOUSE_PRISM_RING_CORNERS - 1);
        DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + nextCorner, 1);
        gte_SetRotMatrix(&coord->workm);
        farCorner = cornerIndex + DRYFIELD_DILAPIDATED_HOUSE_PRISM_RING_CORNERS;
        DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + farCorner, 2);
        gte_SetRotMatrix(&coord->workm);
        farNextCorner = nextCorner + DRYFIELD_DILAPIDATED_HOUSE_PRISM_RING_CORNERS;
        DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + farNextCorner, 3);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->vertices[0]);
        gte_rtps();
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyG4(quad);
        gte_stsxy(&quad->x0);
        gte_ldv3(&scratch->vertices[1], &scratch->vertices[2], &scratch->vertices[3]);
        gte_rtpt();
        gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
        gte_stszotz(&scratch->depth);
        setRGB0(quad, intensity, intensity, intensity);
        setRGB1(quad, intensity, intensity, intensity);
        setRGB2(quad, 0, 0, 0);
        setRGB3(quad, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->depth);
    }
    // Close the lit end in the GPU quad's strip order.
    gte_SetRotMatrix(&coord->workm);
    DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex, 0);
    gte_SetRotMatrix(&coord->workm);
    DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + 1, 1);
    gte_SetRotMatrix(&coord->workm);
    DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + 3, 2);
    gte_SetRotMatrix(&coord->workm);
    DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + 2, 3);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->vertices[0]);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyG4(quad);
    gte_stsxy(&quad->x0);
    gte_ldv3(&scratch->vertices[1], &scratch->vertices[2], &scratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&scratch->depth);
    setRGB0(quad, intensity, intensity, intensity);
    setRGB1(quad, intensity, intensity, intensity);
    setRGB2(quad, intensity, intensity, intensity);
    setRGB3(quad, intensity, intensity, intensity);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->depth);
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
#undef DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER
}

/// Stores a trail endpoint in world space, independent of later anchor motion.
///
/// `endpointCoord->workm.t` must be a current view-space endpoint position;
/// `gGfxViewCoord.workm` must be current with an orthonormal view rotation.
/// Copies the complete endpoint cache into `historyFrame->workm`, then removes
/// the view transform to form its world-space `coord`, parented to
/// `gGfxViewCoord`. Rotation coefficients are copied and rebased too, although
/// the ribbon consumes only translation. Rotations use 12 fractional bits;
/// translations are signed 32-bit game coordinates.
///
/// The word-aligned coordinates must be disjoint from each other, the view
/// nodes and the scratch reservation. The frame remains caller-owned;
/// no endpoint pointer is retained. Leaves `composeStamp` and `param` untouched:
/// the caller must mark the frame dirty before composing it again.
/// Loads endpoint rotation and translation into the GTE before rebasing and
/// changes GTE arithmetic state. Requires 48 free bytes on the initialized
/// scratch stack, released before return.
static inline void _dryfieldDilapidatedHouseStoreTrailFrame(GfxCoord* historyFrame, const GfxCoord* endpointCoord)
{
    historyFrame->parent = &gGfxViewCoord;
    historyFrame->workm  = endpointCoord->workm;
    gte_SetRotMatrix(&endpointCoord->workm);
    gte_SetTransMatrix(&endpointCoord->workm);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &historyFrame->workm, &historyFrame->coord);
}

void dryfieldDilapidatedHouseTwinTrailTask(Task* task)
{
    enum {
        TRAIL_INITIALIZE,
        TRAIL_RECORD,
        TRAIL_SLOT_MASK         = ARRAY_SIZE(_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory) - 1,
        TRAIL_COLOR_MULTIPLIERS = (2 << 8) | (1 << 4),
    };

    GfxCoord    secondEndpointCoord; // Only its translation is initialized and consumed by the ribbon
    GfxCoord*   firstEndpointCoord;
    GfxCoord*   historyFrame;
    EffectWork* work;
    SVECTOR*    initialOffset;
    s32         slotIndex;

    work               = task->spawnArg2.pointer;
    firstEndpointCoord = task->extra.coordBody->coord;

    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        switch (task->state) {
            case TRAIL_INITIALIZE:
                // Seed both histories so the first ribbon starts collapsed.
                firstEndpointCoord->parent       = work->parent;
                firstEndpointCoord->coord.t[0]   = _gDryfieldDilapidatedHouseTwinTrailEndpointOffsets[0].vx;
                firstEndpointCoord->coord.t[1]   = _gDryfieldDilapidatedHouseTwinTrailEndpointOffsets[0].vy;
                firstEndpointCoord->coord.t[2]   = _gDryfieldDilapidatedHouseTwinTrailEndpointOffsets[0].vz;
                firstEndpointCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(firstEndpointCoord);
                task->state                      = TRAIL_RECORD;
                secondEndpointCoord.parent       = work->parent;
                initialOffset                    = &_gDryfieldDilapidatedHouseTwinTrailEndpointOffsets[1];
                secondEndpointCoord.coord.t[0]   = initialOffset->vx;
                secondEndpointCoord.coord.t[1]   = initialOffset->vy;
                secondEndpointCoord.coord.t[2]   = initialOffset->vz;
                secondEndpointCoord.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&secondEndpointCoord);
                for (slotIndex = 0; slotIndex < ARRAY_SIZE(_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory); slotIndex++) {
                    historyFrame = &_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory[slotIndex];
                    _dryfieldDilapidatedHouseStoreTrailFrame(historyFrame, firstEndpointCoord);
                    historyFrame = &_gDryfieldDilapidatedHouseTwinTrailSecondEndpointHistory[slotIndex];
                    _dryfieldDilapidatedHouseStoreTrailFrame(historyFrame, &secondEndpointCoord);
                }
                return;

            case TRAIL_RECORD:
                // Snapshots are rebased under the view, independent of later sword motion.
                firstEndpointCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(firstEndpointCoord);
                secondEndpointCoord.parent = work->parent;
                {
                    SVECTOR* updateOffset          = &_gDryfieldDilapidatedHouseTwinTrailEndpointOffsets[1];
                    secondEndpointCoord.coord.t[0] = updateOffset->vx;
                    secondEndpointCoord.coord.t[1] = updateOffset->vy;
                    secondEndpointCoord.coord.t[2] = updateOffset->vz;
                }
                secondEndpointCoord.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&secondEndpointCoord);
                historyFrame = &_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory[work->age & TRAIL_SLOT_MASK];
                _dryfieldDilapidatedHouseStoreTrailFrame(historyFrame, firstEndpointCoord);
                historyFrame = &_gDryfieldDilapidatedHouseTwinTrailSecondEndpointHistory[work->age & TRAIL_SLOT_MASK];
                _dryfieldDilapidatedHouseStoreTrailFrame(historyFrame, &secondEndpointCoord);
                for (slotIndex = 0; slotIndex < ARRAY_SIZE(_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory); slotIndex++) {
                    historyFrame               = &_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory[slotIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                    historyFrame               = &_gDryfieldDilapidatedHouseTwinTrailSecondEndpointHistory[slotIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                }
                _dryfieldDilapidatedHouseDrawTwinTrail(work->age & TRAIL_SLOT_MASK, TRAIL_COLOR_MULTIPLIERS);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    effectKillTask(work, task);
                }
                break;
        }
    }
}

/// Draws an additive ribbon joining the two eight-frame endpoint histories.
///
/// `newestSlot` is 0..7. Seven quads walk backwards around the histories;
/// their translations must already be in view space. `colorMultipliers` stores
/// red in the signed high byte and two-bit green/blue factors at bits 4/0;
/// the caller passes 2:1:0. Intensity falls by nine per edge from 64. Quads
/// with final-corner SZ3 / 4 below 17 are skipped after reserving their packet.
static void _dryfieldDilapidatedHouseDrawTwinTrail(s16 newestSlot, s16 colorMultipliers)
{
    enum {
        TRAIL_SLOT_MASK          = ARRAY_SIZE(_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory) - 1,
        TRAIL_INITIAL_INTENSITY  = 64,
        TRAIL_INTENSITY_STEP     = 9,
        TRAIL_MIN_DEPTH          = 17,
        TRAIL_COLOR_CHANNEL_MASK = 3,
    };

    OverlayFlaggedQuadScratch* scratch;
    GfxCoord*                  firstEndpointFrame;
    GfxCoord*                  secondEndpointFrame;
    POLY_G4*                   quad;
    s32                        segmentIndex;
    s32                        frameIndex;
    s32                        newerSlot;
    s32                        olderSlot;
    s32                        newerIntensity;
    s32                        olderIntensity;
    s32                        segmentIntensity;

    SCRATCH_STACK_RESERVE_BLOCK(OverlayFlaggedQuadScratch);
    scratch = SCRATCH_STACK_CURSOR(OverlayFlaggedQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    // Newer endpoint pairs form the bright edge; older pairs form the dim edge.
    for (segmentIndex = 0; segmentIndex < ARRAY_SIZE(_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory) - 1; segmentIndex++) {
        frameIndex             = newestSlot - segmentIndex;
        newerSlot              = frameIndex & TRAIL_SLOT_MASK;
        olderSlot              = (frameIndex - 1) & TRAIL_SLOT_MASK;
        firstEndpointFrame     = &_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory[newerSlot];
        scratch->corners[0].vx = (u16)firstEndpointFrame->workm.t[0];
        scratch->corners[0].vy = (u16)firstEndpointFrame->workm.t[1];
        secondEndpointFrame    = &_gDryfieldDilapidatedHouseTwinTrailSecondEndpointHistory[newerSlot];
        scratch->corners[0].vz = (u16)firstEndpointFrame->workm.t[2];
        scratch->corners[1].vx = (u16)secondEndpointFrame->workm.t[0];
        scratch->corners[1].vy = (u16)secondEndpointFrame->workm.t[1];
        firstEndpointFrame     = &_gDryfieldDilapidatedHouseTwinTrailFirstEndpointHistory[olderSlot];
        scratch->corners[1].vz = (u16)secondEndpointFrame->workm.t[2];
        scratch->corners[2].vx = (u16)firstEndpointFrame->workm.t[0];
        scratch->corners[2].vy = (u16)firstEndpointFrame->workm.t[1];
        secondEndpointFrame    = &_gDryfieldDilapidatedHouseTwinTrailSecondEndpointHistory[olderSlot];
        scratch->corners[2].vz = (u16)firstEndpointFrame->workm.t[2];
        scratch->corners[3].vx = (u16)secondEndpointFrame->workm.t[0];
        scratch->corners[3].vy = (u16)secondEndpointFrame->workm.t[1];
        scratch->corners[3].vz = (u16)secondEndpointFrame->workm.t[2];
        gte_ldv0(&scratch->corners[0]);
        gte_rtps();
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyG4(quad);
        gte_stsxy(&quad->x0);
        gte_ldv3(&scratch->corners[1], &scratch->corners[2], &scratch->corners[3]);
        gte_rtpt();
        gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
        gte_stszotz(&scratch->otz);
        if (scratch->otz >= TRAIL_MIN_DEPTH) {
            segmentIntensity = TRAIL_INITIAL_INTENSITY - segmentIndex * TRAIL_INTENSITY_STEP;
            newerIntensity   = segmentIntensity & 0xFF;
            olderIntensity   = (segmentIntensity - TRAIL_INTENSITY_STEP) & 0xFF;
            setRGB0(quad, newerIntensity * (colorMultipliers >> 8), newerIntensity * ((colorMultipliers >> 4) & TRAIL_COLOR_CHANNEL_MASK), newerIntensity * (colorMultipliers & TRAIL_COLOR_CHANNEL_MASK));
            setRGB1(quad, newerIntensity * (colorMultipliers >> 8), newerIntensity * ((colorMultipliers >> 4) & TRAIL_COLOR_CHANNEL_MASK), newerIntensity * (colorMultipliers & TRAIL_COLOR_CHANNEL_MASK));
            setRGB2(quad, olderIntensity * (colorMultipliers >> 8), olderIntensity * ((colorMultipliers >> 4) & TRAIL_COLOR_CHANNEL_MASK), olderIntensity * (colorMultipliers & TRAIL_COLOR_CHANNEL_MASK));
            setRGB3(quad, olderIntensity * (colorMultipliers >> 8), olderIntensity * ((colorMultipliers >> 4) & TRAIL_COLOR_CHANNEL_MASK), olderIntensity * (colorMultipliers & TRAIL_COLOR_CHANNEL_MASK));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayFlaggedQuadScratch);
}

void dryfieldDilapidatedHouseFireBlastTask(Task* task)
{
    enum {
        FIRE_BLAST_INITIALIZE         = 0,
        FIRE_BLAST_EXPAND             = 1,
        FIRE_BLAST_INTENSITY          = 192,
        FIRE_BLAST_FLASH_RADIUS       = 1280,
        FIRE_BLAST_EXPANSION_RADIUS   = 896,
        FIRE_BLAST_RADIUS_STEP        = 64,
        FIRE_BLAST_LAST_RADIUS        = 1408,
        FIRE_BLAST_LIGHT_FRAMES       = 4,
        FIRE_BLAST_LIGHT_INNER_RADIUS = 512,
        FIRE_BLAST_LIGHT_OUTER_RADIUS = 8192,
        FIRE_BLAST_LIGHT_BASE_Q12     = ONE / 2,
        FIRE_BLAST_LIGHT_RANDOM_MASK  = 0x700,
        FIRE_BLAST_RING_ANGLE_STEP    = 0x2AA,
        FIRE_BLAST_RING_ANGLE_LIMIT   = 0x556,
    };

    EffectWork*                    work;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          pointLight;
    EffectWork*                    ringEffect;
    s16                            previousAge;
    s16                            age;
    s32                            ringAngle;
    u8                             flashRgb[3];

    work        = task->spawnArg2.pointer;
    coord       = task->extra.coordBody->coord;
    previousAge = work->age;
    age         = previousAge + 1;
    work->age   = age;
    lightSlot   = &gWorldCoordTransientPointLights[0];
    pointLight  = &lightSlot->light;

/// Seeds transient point-light slot 0 from the blast's world-space origin.
///
/// Captures `coord`, `lightSlot`, `pointLight` and this function's light constants.
/// The aliases must designate slot 0 and its `light` member. Advances the shared
/// random generator once for Q12 red intensity 2048..3840, with half/quarter
/// green/blue. Expiration is four gameplay frames; the slot remains borrowed.
/// No arguments are evaluated; use as a standalone statement. Undefined below.
#define DRYFIELD_DILAPIDATED_HOUSE_SEED_FIRE_BLAST_LIGHT()                                                                                         \
    {                                                                                                                                              \
        s16 lightIntensityQ12;                                                                                                                     \
        gWorldCoordTransientPointLights[0].framesLeft      = FIRE_BLAST_LIGHT_FRAMES;                                                              \
        pointLight->inner                                  = FIRE_BLAST_LIGHT_INNER_RADIUS;                                                        \
        pointLight->outer                                  = FIRE_BLAST_LIGHT_OUTER_RADIUS;                                                        \
        gRandomLcgState                                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                       \
        lightIntensityQ12                                  = ((gRandomLcgState >> 16) & FIRE_BLAST_LIGHT_RANDOM_MASK) + FIRE_BLAST_LIGHT_BASE_Q12; \
        pointLight->head.color.r                           = lightIntensityQ12;                                                                    \
        pointLight->head.color.g                           = lightIntensityQ12 >> 1;                                                               \
        pointLight->head.color.b                           = lightIntensityQ12 >> 2;                                                               \
        pointLight->head.transform.coord.coord.t[0]        = coord->coord.t[0];                                                                    \
        pointLight->head.transform.coord.coord.t[1]        = coord->coord.t[1];                                                                    \
        pointLight->head.transform.coord.coord.t[2]        = coord->coord.t[2];                                                                    \
        lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;                                                                 \
    }

    switch (task->state) {
        case FIRE_BLAST_INITIALIZE:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age = previousAge;
                if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                    effectKillTask(work, task);
                }
                return;
            }
            // Seed the flash before resetting the radius for the expanding phase.
            work->scale     = FIRE_BLAST_INTENSITY;
            work->angle     = FIRE_BLAST_FLASH_RADIUS;
            work->index     = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
            effectSpawn(EFFECT_DRYFIELD_DILAPIDATED_HOUSE_FLAME_CONE, coord, 0, NULL);
            flashRgb[0] = 0xFF;
            flashRgb[1] = 0x7F;
            flashRgb[2] = 0x3F;
            effectDrawScreenTint(flashRgb, GPU_BLEND_ADD);
            // The light uses Q12 colour and the blast origin's world translation.
            DRYFIELD_DILAPIDATED_HOUSE_SEED_FIRE_BLAST_LIGHT();
            _spriteQuadDrawFlicker(coord, work->age, work->angle, work->period);
            glowDrawFlameDisc(coord, work->angle, work->scale >> 1);
            work->angle = FIRE_BLAST_EXPANSION_RADIUS;
            // Successful rings become children so cancellation tears them down too.
            for (ringAngle = 0; ringAngle < FIRE_BLAST_RING_ANGLE_LIMIT; ringAngle += FIRE_BLAST_RING_ANGLE_STEP) {
                ringEffect = effectSpawn(EFFECT_DILAPIDATED_HOUSE_FLAME_RING, coord, ringAngle, NULL);
                if (ringEffect != NULL) {
                    taskReparent(task, ringEffect->task);
                }
            }
            task->state = FIRE_BLAST_EXPAND;
            return;
        case FIRE_BLAST_EXPAND:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age = previousAge;
                if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                    effectKillTask(work, task);
                }
                return;
            }
            _spriteQuadDrawFlicker(coord, age, work->angle, work->period);
            glowDrawFlameDisc(coord, work->angle, work->scale >> 1);
            glowDrawFlameDisc(coord, (u16)work->angle * 2, work->scale >> 1);
            work->angle += FIRE_BLAST_RADIUS_STEP;
            if (work->angle >= FIRE_BLAST_LAST_RADIUS + 1) {
                effectKillTask(work, task);
            }
            break;
    }
}

#undef DRYFIELD_DILAPIDATED_HOUSE_SEED_FIRE_BLAST_LIGHT

#include "../../shared/effects/glow_draw_flame.inc.c"

/// Perspective-sizing multiplier for the dilapidated house's fire-blast billboard.
///
/// Uses the endpoint span of each fixed 56-texel flicker cell.
/// `sizeFactor * SPRITE_QUAD_SCALE / (SZ3 / 4 + 1)` gives the signed pixel
/// half-diagonal, truncated before Q12 rotation. The next flicker inclusion
/// requires a positive signed integer constant and undefines this binding.
#define SPRITE_QUAD_SCALE (56 - 1)
/// Reserves a flicker packet even when the GTE rejects the quad's projection.
#define SPRITE_QUAD_RESERVE_BEFORE_PROJECTION_CHECK 1
/// Selects the house fire blast's additive warm tint on odd frames.
///
/// `quad` points to a writable, complete `POLY_FT4` initialized by `setPolyFT4`,
/// with texture modulation enabled. Colour bytes use 128 as unity: red scales
/// by three halves, green by three quarters and blue by one half.
/// `SPRITE_QUAD_SET_FLICKER_ODD_CELL` supplies the fixed cell, palette and
/// additive texture page; this binding enables semitransparency.
/// The argument is evaluated repeatedly: supply a stable expression without
/// side effects. Captures no locals; invoke inside a braced caller block.
/// Bind before the next flicker-drawer inclusion, which undefines the binding.
#define SPRITE_QUAD_CONFIGURE_FLICKER_ODD_PACKET(quad)                                                                                                          \
    {                                                                                                                                                           \
        enum { DRYFIELD_DILAPIDATED_HOUSE_FLICKER_ODD_RED   = 0xC0,                                                                                             \
               DRYFIELD_DILAPIDATED_HOUSE_FLICKER_ODD_GREEN = 0x60,                                                                                             \
               DRYFIELD_DILAPIDATED_HOUSE_FLICKER_ODD_BLUE  = 0x40 };                                                                                            \
        setRGB0((quad), DRYFIELD_DILAPIDATED_HOUSE_FLICKER_ODD_RED, DRYFIELD_DILAPIDATED_HOUSE_FLICKER_ODD_GREEN, DRYFIELD_DILAPIDATED_HOUSE_FLICKER_ODD_BLUE); \
        SPRITE_QUAD_SET_FLICKER_ODD_CELL((quad));                                                                                                               \
        setSemiTrans((quad), 1);                                                                                                                                \
    }
/// Selects the house fire blast's additive raw texture on even frames.
///
/// `quad` points to a writable, complete `POLY_FT4` initialized by `setPolyFT4`.
/// `SPRITE_QUAD_SET_FLICKER_EVEN_CELL` supplies the fixed cell and palette;
/// semitransparency uses that cell's additive texture page. Raw texture mode
/// ignores the packet's RGB bytes, which this binding leaves untouched.
/// The argument is evaluated repeatedly: supply a stable expression without
/// side effects. Captures no locals; invoke inside a braced caller block.
/// Bind before the next flicker-drawer inclusion, which undefines the binding.
#define SPRITE_QUAD_CONFIGURE_FLICKER_EVEN_PACKET(quad) \
    {                                                   \
        SPRITE_QUAD_SET_FLICKER_EVEN_CELL((quad));      \
        setSemiTrans((quad), 1);                        \
        setShadeTex((quad), 1);                         \
    }
#include "../../shared/effects/sprite_quad_draw_flicker.inc.c"

#include "../../shared/effects/glow_draw_flame_ring.inc.c"

void dryfieldDilapidatedHouseLightPrismTask(Task* task)
{
    enum {
        PRISM_LEFT_VIEWS   = 0x84A9C,
        PRISM_MIDDLE_VIEWS = 0x104B98,
        PRISM_RIGHT_VIEWS  = 0xA55F8,
    };

    GfxCoord* coord;
    s32       viewMask;

    viewMask = 1 << gGameSession->location.view;
    coord    = task->extra.coordBody->coord;
    if (viewMask & PRISM_LEFT_VIEWS) {
        _dryfieldDilapidatedHouseDrawLightPrism(coord, 0);
    }
    if (viewMask & PRISM_MIDDLE_VIEWS) {
        _dryfieldDilapidatedHouseDrawLightPrism(coord, DRYFIELD_DILAPIDATED_HOUSE_PRISM_VERTEX_COUNT);
    }
    if (viewMask & PRISM_RIGHT_VIEWS) {
        _dryfieldDilapidatedHouseDrawLightPrism(coord, 2 * DRYFIELD_DILAPIDATED_HOUSE_PRISM_VERTEX_COUNT);
    }
}

void dryfieldDilapidatedHouseFlameConeTask(Task* task)
{
    enum {
        FLAME_CONE_INITIALIZE,
        FLAME_CONE_ACTIVE,
        FLAME_CONE_INITIAL_INTENSITY = 192,
        FLAME_CONE_INITIAL_RADIUS    = 256,
        FLAME_CONE_RADIUS_STEP       = 64,
        FLAME_CONE_INTENSITY_STEP    = 16,
    };

    EffectWork* work;
    s16         effectControl;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    work->age++;
    if (task->state == FLAME_CONE_INITIALIZE) {
        work->scale = FLAME_CONE_INITIAL_INTENSITY;
        work->angle = FLAME_CONE_INITIAL_RADIUS;
        task->state = FLAME_CONE_ACTIVE;
    }
    glowDrawFlameCone(task->extra.coordBody->coord, work->angle, work->scale);
    work->angle += FLAME_CONE_RADIUS_STEP;
    work->scale -= FLAME_CONE_INTENSITY_STEP;
    if (work->scale < FLAME_CONE_INTENSITY_STEP) {
        effectKillTask(work, task);
    }
}

void dryfieldDilapidatedHouseFlameRingTask(Task* task)
{
    enum {
        FLAME_RING_INITIALIZE,
        FLAME_RING_ACTIVE,
        FLAME_RING_INITIAL_INTENSITY = 128,
        FLAME_RING_INITIAL_RADIUS    = 256,
        FLAME_RING_WIDTH             = 256,
        FLAME_RING_RADIUS_STEP       = 128,
        FLAME_RING_INTENSITY_STEP    = 8,
        FLAME_RING_MIN_INTENSITY     = 9,
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    if (task->state == FLAME_RING_INITIALIZE) {
        // The spawn angle tilts the local XZ ring once; only radius and intensity ramp.
        gfxRotMatrixZ(&coord->coord, task->spawnArg1.value, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->scale = FLAME_RING_INITIAL_INTENSITY;
        work->angle = FLAME_RING_INITIAL_RADIUS;
        task->state = FLAME_RING_ACTIVE;
    }

    glowDrawFlameRing(coord, work->angle, FLAME_RING_WIDTH, work->scale);
    work->angle += FLAME_RING_RADIUS_STEP;
    work->scale -= FLAME_RING_INTENSITY_STEP;
    if (work->scale < FLAME_RING_MIN_INTENSITY) {
        effectKillTask(work, task);
    }
}
