#include "rooms/acropolis_roof_garden.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/falling_leaves.h"
#include "../../shared/actor_contacts.h"
#include "../../shared/glow_draw.h"

static Task* _gAcropolisRoofGardenCutsceneSoundTask;

static s32 _gAcropolisRoofGardenRequestedAmbiencePercent;

/// Whole-unit X/Y/Z displacement left by `_actorContactApplyGridPushback`.
extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

/// Initializes a flare quad's command and colours, with vertex 2 lit and a black rim.
///
/// `quad` must be a stable writable POLY_G4 pointer without side effects; it is
/// evaluated repeatedly. `red` and `green` each evaluate once, after command and
/// rim writes, and narrow to bytes. No identifiers are captured or retained.
#define ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, red, green) \
    ((void)(setPolyG4((quad)),                                  \
            setRGB0((quad), 0, 0, 0),                           \
            setRGB1((quad), 0, 0, 0),                           \
            setRGB2((quad), (red), (green), 0),                 \
            setRGB3((quad), 0, 0, 0)))

static void _acropolisRoofGardenInitRoomTask(Task* task);
static void _acropolisRoofGardenTickRoom(Task* task);

/// Room-task lifecycle: register the receiver, start the arrival scene, release.
///
/// Copied by value for unchecked state 0..2 dispatch; callbacks require this overlay loaded.
static const TaskFuncTable3 _gAcropolisRoofGardenRoomTaskStates = {
    { _acropolisRoofGardenInitRoomTask, _acropolisRoofGardenTickRoom, taskKill },
};

static WorldCollisionGrid     _gAcropolisRoofGardenCollisionGrid;
static WorldCollisionOccluder _gAcropolisRoofGardenOccluders[2];
static WorldCollisionTrigger  _gAcropolisRoofGardenViewBoundaryTriggers[6];
static WorldCollisionTrigger  _gAcropolisRoofGardenActionTriggers[7];
static WorldCoordRoomLights   _gAcropolisRoofGardenRoomLights;

static void _acropolisRoofGardenReleaseMaggotCaterpillarEntrance(void);

/// Sound-task actions passed by the arrival scene and its skip script.
enum {
    ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_OPENING = 0,
    ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_CLOSING = 1,
    ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_CANCEL  = 2,
};
static void _acropolisRoofGardenControlCutsceneSound(s32 soundAction);

static AnimationSet _gAcropolisRoofGardenAnimation04164;
static AnimationSet _gAcropolisRoofGardenAnimation060FC;
static AnimationSet _gAcropolisRoofGardenAnimation065F4;

static s32  _acropolisRoofGardenResolveRoomTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _acropolisRoofGardenRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _acropolisRoofGardenHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32  _acropolisRoofGardenHandleSoundMessage(Task* task, s32 messageId, s32 soundCue, s32 unusedArg);
static s32  _acropolisRoofGardenHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandIndex, s32 unusedArg);
static void _acropolisRoofGardenAmbienceTask(Task* task);
static void _acropolisRoofGardenCutsceneOpeningSoundTask(Task* task);
static void _acropolisRoofGardenCutsceneClosingSoundTask(Task* task);

static AnimationPackedPose _gAcropolisRoofGardenAnimation04164Bank1[38] = {
#include "assets/acropolis_roof_garden_animation_04164_bank1.inc"
};

static AnimationPackedRotation _gAcropolisRoofGardenAnimation04164Bank4[571] = {
#include "assets/acropolis_roof_garden_animation_04164_bank4.inc"
};

static AnimationRecord _gAcropolisRoofGardenAnimation04164Records[666] = {
#include "assets/acropolis_roof_garden_animation_04164_records.inc"
};

static u16 _gAcropolisRoofGardenAnimation04164Indices[20] = {
#include "assets/acropolis_roof_garden_animation_04164_indices.inc"
};

static AnimationSet _gAcropolisRoofGardenAnimation04164 = {
    _gAcropolisRoofGardenAnimation04164Records,
    _gAcropolisRoofGardenAnimation04164Indices,
    { NULL, _gAcropolisRoofGardenAnimation04164Bank1, NULL, NULL, _gAcropolisRoofGardenAnimation04164Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisRoofGardenAnimation060FCBank1[40] = {
#include "assets/acropolis_roof_garden_animation_060FC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisRoofGardenAnimation060FCBank4[860] = {
#include "assets/acropolis_roof_garden_animation_060FC_bank4.inc"
};

static AnimationRecord _gAcropolisRoofGardenAnimation060FCRecords[1022] = {
#include "assets/acropolis_roof_garden_animation_060FC_records.inc"
};

static u16 _gAcropolisRoofGardenAnimation060FCIndices[20] = {
#include "assets/acropolis_roof_garden_animation_060FC_indices.inc"
};

static AnimationSet _gAcropolisRoofGardenAnimation060FC = {
    _gAcropolisRoofGardenAnimation060FCRecords,
    _gAcropolisRoofGardenAnimation060FCIndices,
    { NULL, _gAcropolisRoofGardenAnimation060FCBank1, NULL, NULL, _gAcropolisRoofGardenAnimation060FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisRoofGardenAnimation065F4Bank1[6] = {
#include "assets/acropolis_roof_garden_animation_065F4_bank1.inc"
};

static AnimationPackedRotation _gAcropolisRoofGardenAnimation065F4Bank4[87] = {
#include "assets/acropolis_roof_garden_animation_065F4_bank4.inc"
};

static AnimationRecord _gAcropolisRoofGardenAnimation065F4Records[193] = {
#include "assets/acropolis_roof_garden_animation_065F4_records.inc"
};

static u16 _gAcropolisRoofGardenAnimation065F4Indices[20] = {
#include "assets/acropolis_roof_garden_animation_065F4_indices.inc"
};

static AnimationSet _gAcropolisRoofGardenAnimation065F4 = {
    _gAcropolisRoofGardenAnimation065F4Records,
    _gAcropolisRoofGardenAnimation065F4Indices,
    { NULL, _gAcropolisRoofGardenAnimation065F4Bank1, NULL, NULL, _gAcropolisRoofGardenAnimation065F4Bank4, NULL, NULL, NULL },
};

enum { ACROPOLIS_ROOF_GARDEN_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Room-transition, action, command, key-item and sound-cue dispatch.
///
/// The room task borrows this six-record list while the overlay is loaded.
/// The final TASK_MESSAGE_TABLE_END record terminates lookup; payload types
/// are determined by each message, and unsupported IDs return zero.
static TaskMessageEntry _gAcropolisRoofGardenRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisRoofGardenResolveRoomTransition },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisRoofGardenHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _acropolisRoofGardenHandleCommandMessage },
    { ACROPOLIS_ROOF_GARDEN_MESSAGE_USE_KEY_ITEM, _acropolisRoofGardenRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _acropolisRoofGardenHandleSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Borrowed handle of the arrival scene's active opening or closing sound task.
///
/// NULL means no cancellable sequence. A final cue clears it before teardown;
/// cancellation kills the task before clearing it. Starts assume the previous
/// sequence has finished and replace the handle even when allocation fails.
static Task* _gAcropolisRoofGardenCutsceneSoundTask = NULL;

enum { ACROPOLIS_ROOF_GARDEN_SOUND_TASK_PRIORITY = 32 };

/// Bodyless sound-task recipes: 0 ambience, 1 arrival opening, 2 arrival closing.
///
/// All run at priority 32 with zero spawn arguments; TASK_DESC_END closes the
/// directory. The recipes and callbacks live with the loaded room overlay.
static TaskDesc _gAcropolisRoofGardenSoundTaskDescriptors[4] = {
    { { { TASK_BODY_NONE, ACROPOLIS_ROOF_GARDEN_SOUND_TASK_PRIORITY } }, _acropolisRoofGardenAmbienceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_ROOF_GARDEN_SOUND_TASK_PRIORITY } }, _acropolisRoofGardenCutsceneOpeningSoundTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_ROOF_GARDEN_SOUND_TASK_PRIORITY } }, _acropolisRoofGardenCutsceneClosingSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Unreferenced four-byte record retained as {1, 13, 0, 0}; its command role is unproven.
static ActorCommand D_acropolis_roof_garden_80183C40 = { { .loc = { 1, 13 } }, 0 };

/// Player clip 1 restarted at the arrival scene's end and by its skip path.
///
/// EVS replaces source.index with the equipped weapon bank in a copy. Playback
/// takes scripted control and disables grid participation until control resumes.
static AnimationPlayRequest _gAcropolisRoofGardenArrivalFinalPlayerAnimation = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Zero transform carried by the arrival scene's figure-placement message.
///
/// The selected view figure has no placement handler, so this payload is ignored.
static ActorTransform _gAcropolisRoofGardenArrivalFigurePlacement = {
    .pos = { 0, 0, 0, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Arrival opening placement of the player, in world-coordinate units.
///
/// Position is (0, 128, 0), with zero Euler rotation; dispatch copies XYZ and
/// all three angles into the player's model without retaining the record.
static ActorTransform _gAcropolisRoofGardenArrivalStartPlayerPlacement = {
    .pos = { 0, 128, 0, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Unreferenced transform-shaped record; its receiver and coordinate frame are unproven.
static ActorTransform D_acropolis_roof_garden_80183C88 = { { 0, 128, 0, 0 }, { 0, 407, 0, 0 } };

/// Player placement shared by the arrival scene's normal and skipped endings.
///
/// Position is (-7508, 0, -8572) in world-coordinate units; yaw is 407 in
/// 4096 units per turn. The player consumes the record synchronously.
static ActorTransform _gAcropolisRoofGardenArrivalEndPlayerPlacement = {
    .pos = { -7508, 0, -8572, 0 },
    .rot = { 0, 407, 0, 0 },
};

/// Borrowed player clips for arrival opening, closing and dialogue, in that order.
///
/// The bank-copy request installs all three at extension slots 47..49. The
/// table and clip assets belong to the room overlay and stay live during playback.
static AnimationSet* _gAcropolisRoofGardenArrivalPlayerAnimationSets[3] = {
    &_gAcropolisRoofGardenAnimation04164,
    &_gAcropolisRoofGardenAnimation060FC,
    &_gAcropolisRoofGardenAnimation065F4,
};

/// Installs the arrival scene's three player clips into the equipped bank extension.
///
/// Copies three pointer words synchronously without starting playback; the
/// copied clips remain borrowed from the room overlay through the scene.
static AnimationBankCopyRequest _gAcropolisRoofGardenArrivalPlayerBankCopy = {
    .source    = { .sets = _gAcropolisRoofGardenArrivalPlayerAnimationSets },
    .wordCount = ARRAY_SIZE(_gAcropolisRoofGardenArrivalPlayerAnimationSets),
};

enum {
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_OPENING_SET  = ANIMATION_BANK_BASE_SET_COUNT,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_CLOSING_SET  = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_DIALOGUE_SET = ANIMATION_BANK_BASE_SET_COUNT + 2,
};

/// Restarts the installed player clip for the arrival opening, disabling grid participation.
///
/// EVS resolves the equipped weapon bank in a copy; the room clip remains
/// borrowed until playback changes. The opening phase waits 154 frames.
static AnimationPlayRequest _gAcropolisRoofGardenArrivalOpeningPlayerAnimation = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_ROOF_GARDEN_ARRIVAL_OPENING_SET,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the installed player clip for the arrival closing, disabling grid participation.
///
/// EVS resolves the equipped weapon bank in a copy. The script waits 115 frames
/// before replacing this borrowed room clip with the final player animation.
static AnimationPlayRequest _gAcropolisRoofGardenArrivalClosingPlayerAnimation = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_ROOF_GARDEN_ARRIVAL_CLOSING_SET,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the installed player clip used through the arrival scene's CAP dialogue.
///
/// EVS resolves the equipped weapon bank in a copy. Grid participation stays
/// disabled; the closing request replaces this borrowed room clip after the cues.
static AnimationPlayRequest _gAcropolisRoofGardenArrivalDialoguePlayerAnimation = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_ROOF_GARDEN_ARRIVAL_DIALOGUE_SET,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Figure body clip 1 requested at arrival dialogue start and on scene skip.
///
/// The figure restarts tracks 1..19; only animationId is read. The request is
/// borrowed through dispatch and the figure owns the clip assets.
static AnimationPlayRequest _gAcropolisRoofGardenArrivalFigureAnimation1 = {
    .source               = { .index = 0 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Figure body clip 2 requested at the arrival dialogue's next CAP cue.
///
/// Only animationId is read; the figure restarts its body tracks synchronously.
static AnimationPlayRequest _gAcropolisRoofGardenArrivalFigureAnimation2 = {
    .source               = { .index = 0 },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Figure body clip 3 requested at a CAP cue before the arrival scene's 180-frame wait.
///
/// Only animationId is read; the figure restarts its body tracks synchronously.
static AnimationPlayRequest _gAcropolisRoofGardenArrivalFigureAnimation3 = {
    .source               = { .index = 0 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Figure body clip 4 requested after the arrival scene's 180-frame wait.
///
/// Only animationId is read; the figure restarts its body tracks synchronously.
static AnimationPlayRequest _gAcropolisRoofGardenArrivalFigureAnimation4 = {
    .source               = { .index = 0 },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Figure body clip 5 requested after clip 4's 79-frame wait, before the next CAP cue.
///
/// Only animationId is read; the figure restarts its body tracks synchronously.
static AnimationPlayRequest _gAcropolisRoofGardenArrivalFigureAnimation5 = {
    .source               = { .index = 0 },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced halfword triplet {1, 8, 11}; its scene-selection role is unproven.
static EvsSceneKey D_acropolis_roof_garden_80183D6C = { 1, 8, 11 };

// Arrival waits count interpreter ticks; sound cues use type-3 bank 0x108.
enum {
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_CAP_COMMAND               = 1,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_HIDE               = 0,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_SHOW               = 1,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_OPENING_WAIT_FRAMES       = 154,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_WAIT_FRAMES        = 180,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_CLIP4_FRAMES       = 79,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_CLOSING_WAIT_FRAMES       = 115,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_MUSIC_FADE_TICKS          = 120,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SKIP_FADE_FRAMES          = 8,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SKIP_VIEW                 = 3,
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_1               = SOUND_ID(3, 0x108, 1),
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_2               = SOUND_ID(3, 0x108, 2),
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_3               = SOUND_ID(3, 0x108, 3),
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_4               = SOUND_ID(3, 0x108, 4),
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_5               = SOUND_ID(3, 0x108, 5),
    ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_ALL_TYPE_3          = SOUND_ID(3, 0, 0),
    ACROPOLIS_ROOF_GARDEN_ENTRANCE_SECOND_CLIP_DELAY_FRAMES = 30,
    ACROPOLIS_ROOF_GARDEN_ENTRANCE_BATTLE_DELAY_FRAMES      = 40,
};

/// Arrival presentation: CAP dialogue, player/figure clips and synchronized sound cues.
///
/// Started once at warp 2 with HUD hide/restore. NULL scene selection leaves
/// the stream unchanged. END terminates the 44-command sequence; frame waits
/// count interpreter ticks. Commands and referenced actor resources stay
/// borrowed through playback, including the alternate skip completion.
static EvsCommand _gAcropolisRoofGardenArrivalScript[44] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gAcropolisRoofGardenArrivalPlayerBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisRoofGardenArrivalStartPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisRoofGardenArrivalOpeningPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisRoofGardenControlCutsceneSound }, { .value = ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_OPENING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_OPENING_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisRoofGardenArrivalDialoguePlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 61 }, { .value = 64 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisRoofGardenArrivalFigurePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisRoofGardenArrivalFigureAnimation1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisRoofGardenArrivalFigureAnimation2 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisRoofGardenArrivalFigureAnimation3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisRoofGardenArrivalFigureAnimation4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_CLIP4_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisRoofGardenArrivalFigureAnimation5 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_CUE_3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisRoofGardenArrivalClosingPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisRoofGardenControlCutsceneSound }, { .value = ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_CLOSING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_CLOSING_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_MUSIC_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisRoofGardenArrivalFinalPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_RESET_ANIMATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisRoofGardenArrivalEndPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip completion for the arrival, restoring player placement, view and ambience.
///
/// Stops type-3 sounds and the pending sound task, resets the figure, cleans
/// up CAP under the default fade and resumes player control. END terminates
/// all 17 commands; keep this script and its payloads loaded until completion.
static EvsCommand _gAcropolisRoofGardenArrivalSkipScript[17] = {
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_SOUND_ALL_TYPE_3 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisRoofGardenArrivalEndPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisRoofGardenArrivalFinalPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_FIGURE_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisRoofGardenArrivalFigureAnimation1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_MUSIC_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisRoofGardenControlCutsceneSound }, { .value = ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_SKIP_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACROPOLIS_ROOF_GARDEN_ARRIVAL_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Arrival-scene latch (0 unstarted, 1 attempted), reset by loading the overlay.
///
/// Set before starting the warp-2 script; a failed start is not retried.
static s32 _gAcropolisRoofGardenArrivalSceneStarted = 0;

static AnimationPackedPose _gAcropolisRoofGardenAnimation0704CBank1[6] = {
#include "assets/acropolis_roof_garden_animation_0704C_bank1.inc"
};

static AnimationPackedRotation _gAcropolisRoofGardenAnimation0704CBank4[46] = {
#include "assets/acropolis_roof_garden_animation_0704C_bank4.inc"
};

static AnimationRecord _gAcropolisRoofGardenAnimation0704CRecords[109] = {
#include "assets/acropolis_roof_garden_animation_0704C_records.inc"
};

static u16 _gAcropolisRoofGardenAnimation0704CIndices[20] = {
#include "assets/acropolis_roof_garden_animation_0704C_indices.inc"
};

static AnimationSet _gAcropolisRoofGardenAnimation0704C = {
    _gAcropolisRoofGardenAnimation0704CRecords,
    _gAcropolisRoofGardenAnimation0704CIndices,
    { NULL, _gAcropolisRoofGardenAnimation0704CBank1, NULL, NULL, _gAcropolisRoofGardenAnimation0704CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisRoofGardenAnimation074BCBank1[10] = {
#include "assets/acropolis_roof_garden_animation_074BC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisRoofGardenAnimation074BCBank4[95] = {
#include "assets/acropolis_roof_garden_animation_074BC_bank4.inc"
};

static AnimationRecord _gAcropolisRoofGardenAnimation074BCRecords[139] = {
#include "assets/acropolis_roof_garden_animation_074BC_records.inc"
};

static u16 _gAcropolisRoofGardenAnimation074BCIndices[20] = {
#include "assets/acropolis_roof_garden_animation_074BC_indices.inc"
};

static AnimationSet _gAcropolisRoofGardenAnimation074BC = {
    _gAcropolisRoofGardenAnimation074BCRecords,
    _gAcropolisRoofGardenAnimation074BCIndices,
    { NULL, _gAcropolisRoofGardenAnimation074BCBank1, NULL, NULL, _gAcropolisRoofGardenAnimation074BCBank4, NULL, NULL, NULL },
};

/// Player extension slots for the Maggot/Caterpillar entrance: empty, start, end.
///
/// Copying all three clears slot 47 and installs clips 48/49. Only those two
/// installed clips are played; their room-owned assets stay live through playback.
static AnimationSet* _gAcropolisRoofGardenEntrancePlayerAnimationSets[3] = {
    NULL,
    &_gAcropolisRoofGardenAnimation0704C,
    &_gAcropolisRoofGardenAnimation074BC,
};

/// Copies the Maggot/Caterpillar entrance's three words into the equipped player bank.
///
/// The first word deliberately clears extension slot 47. The following two
/// borrow the room's start/end clips; copying does not start playback.
static AnimationBankCopyRequest _gAcropolisRoofGardenEntrancePlayerBankCopy = {
    .source    = { .sets = _gAcropolisRoofGardenEntrancePlayerAnimationSets },
    .wordCount = ARRAY_SIZE(_gAcropolisRoofGardenEntrancePlayerAnimationSets),
};

/// Unused bank-1 clip-1 reset request retained with the entrance animation data.
///
/// Requests disabled world collision. Neither entrance nor arrival scripts use it.
static AnimationPlayRequest _gAcropolisRoofGardenUnusedBank1Animation1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    ACROPOLIS_ROOF_GARDEN_ENTRANCE_START_SET    = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACROPOLIS_ROOF_GARDEN_ENTRANCE_END_SET      = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACROPOLIS_ROOF_GARDEN_ENTRANCE_BLEND_FRAMES = 20,
};

/// Player clip blended in before releasing the Maggot/Caterpillar entrance.
///
/// EVS resolves the weapon bank in a copy. Blend duration is 20 normal-rate
/// frames; playback takes scripted control and disables grid participation.
static AnimationPlayRequest _gAcropolisRoofGardenEntranceStartPlayerAnimation = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_ROOF_GARDEN_ENTRANCE_START_SET,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACROPOLIS_ROOF_GARDEN_ENTRANCE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip blended in 30 frames after releasing the Maggot/Caterpillar entrance.
///
/// EVS resolves the weapon bank in a copy. Blend duration is 20 normal-rate
/// frames; grid participation stays disabled until scripted control ends.
static AnimationPlayRequest _gAcropolisRoofGardenEntranceEndPlayerAnimation = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_ROOF_GARDEN_ENTRANCE_END_SET,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACROPOLIS_ROOF_GARDEN_ENTRANCE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unused bank-1 clip-9 request retained with the entrance animation data.
///
/// Requests a ten-frame blend and disabled world collision; no script selects it.
static AnimationPlayRequest _gAcropolisRoofGardenUnusedBank1Animation9Request = {
    .source               = { .index = 1 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Releases the Maggot/Caterpillar entrance, stages the player and engages battle.
///
/// Triggered by room action 2 while entrance progress is armed. Copies the
/// player's animation extension, releases the enemy after the first clip,
/// waits 30 then 40 interpreter ticks and resumes with its root offset kept.
/// END terminates ten commands; script and actor resources remain borrowed.
static EvsCommand _gAcropolisRoofGardenMaggotCaterpillarEntranceScript[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gAcropolisRoofGardenEntrancePlayerBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisRoofGardenEntranceStartPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisRoofGardenReleaseMaggotCaterpillarEntrance }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACROPOLIS_ROOF_GARDEN_ENTRANCE_SECOND_CLIP_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisRoofGardenEntranceEndPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACROPOLIS_ROOF_GARDEN_ENTRANCE_BATTLE_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Ten light-glow spawn offsets in the ambient emitter's coordinate frame.
///
/// Signed halfword game units, transformed at spawning; indices 0/1 select
/// large glows, 2 the upper glow and 3..9 small glows. The same indices select
/// visibility masks. Spawning copies XYZ but retains the offset pointer, so
/// keep the room loaded through glow teardown. There is no terminator.
static SVECTOR _gAcropolisRoofGardenLightOffsets[10] = {
    { -6750, -1970, -0x2896, 0 },
    { -8220, -1970, -0x2896, 0 },
    { -2680, -6600, -1240, 0 },
    { -5840, -548, -9840, 0 },
    { -8740, -548, -9840, 0 },
    { -3430, -548, -7300, 0 },
    { -7620, -548, -7200, 0 },
    { -6300, -548, -6590, 0 },
    { -2890, -548, -2250, 0 },
    { -6470, -548, -2250, 0 },
};

/// Logical-view bits used by the roof garden's placed light glows.
enum {
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_2 = 1 << (2 - 1),
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_3 = 1 << (3 - 1),
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_4 = 1 << (4 - 1),
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_5 = 1 << (5 - 1),
};

/// Visible logical views for each of the ten placed light glows.
///
/// Index 0..9 is the light index in the spawn argument's low nibble; bit
/// `view - 1` permits drawing in logical views 1..7. The flare's first ray
/// also fetches entry 8 through its retained pre-array sine lookup.
static u16 _gAcropolisRoofGardenLightViewMasks[10] = {
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_2,
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_2,
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_3,
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_2,
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_2,
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_3,
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_3,
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_3 | ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_4,
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_5,
    ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_4 | ACROPOLIS_ROOF_GARDEN_LIGHT_VIEW_5,
};

/// Signed sine samples for the roof garden's circular flare geometry.
///
/// A turn has sixteen samples, beginning at -ONE; X reads four samples
/// ahead of Y. Samples have twelve fractional bits. Indices 0..22 are used;
/// the stored value 591 at index 23 has an unproven role. The first ray's
/// index -2 fetches the preceding light-view mask 8, rather than a sine sample.
static s16 _gAcropolisRoofGardenFlareSineSamples[24] = {
    -ONE,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    3784,
    ONE,
    3784,
    2896,
    1567,
    0,
    -1567,
    -2896,
    -3784,
    -ONE,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    591,
};

WorldCollisionRoomResources gAcropolisRoofGardenCollisionRoomResources[1] = {
    {
        .grid                 = &_gAcropolisRoofGardenCollisionGrid,
        .viewBoundaryTriggers = _gAcropolisRoofGardenViewBoundaryTriggers,
        .actionTriggers       = _gAcropolisRoofGardenActionTriggers,
        .occluders            = _gAcropolisRoofGardenOccluders,
    },
};

u8* gAcropolisRoofGardenViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gAcropolisRoofGardenViewCounts[1] = { ACROPOLIS_ROOF_GARDEN_VIEW_COUNT };

WorldCoordRoomLighting gAcropolisRoofGardenRoomLightingTable[1] = {
    { .lights = &_gAcropolisRoofGardenRoomLights, .ambientTable = NULL },
};

enum {
    ACROPOLIS_ROOF_GARDEN_WARP_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 2),
    ACROPOLIS_ROOF_GARDEN_WARP_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 1),
};

DirectionWarpEntry gAcropolisRoofGardenWarpEntries[2] = {
    {
        .player         = { { .word = 0 }, -7458, 0, -9712 },
        .companion      = { { .word = 0 }, -7458, 0, -9712 },
        .arrivalSound   = ACROPOLIS_ROOF_GARDEN_WARP_ARRIVAL_SOUND,
        .departureSound = ACROPOLIS_ROOF_GARDEN_WARP_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { { .word = 0 }, -7458, 0, -9712 },
        .companion      = { { .word = 0 }, -7458, 0, -9712 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 3,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Room-coordinate Q12 normals indexed by the roof garden's collision faces.
static SVECTOR _gAcropolisRoofGardenCollisionGridNormals[31] = {
#include "assets/acropolis_roof_garden_collision_07EE4_normals.inc"
};

/// Room-coordinate vertices, in whole game units, indexed by the collision faces.
static SVECTOR _gAcropolisRoofGardenCollisionGridVerts[92] = {
#include "assets/acropolis_roof_garden_collision_07EE4_verts.inc"
};

/// Roof-garden triangles/quads and their surface classes 0..2.
static WorldCollisionGridFace _gAcropolisRoofGardenCollisionGridFaces[53] = {
#include "assets/acropolis_roof_garden_collision_07EE4_faces.inc"
};

/// Packed face-index lists for the roof garden's nine XZ cells.
///
/// Each list ends at WORLD_COLLISION_GRID_CELL_END. The final stored zero
/// lies after the last terminator and is outside the traversed lists.
static s16 _gAcropolisRoofGardenCollisionGridCells[136] = {
#include "assets/acropolis_roof_garden_collision_07EE4_cells.inc"
};

enum {
    ACROPOLIS_ROOF_GARDEN_GRID_CELL_COUNT_X = 3,
    ACROPOLIS_ROOF_GARDEN_GRID_CELL_COUNT_Z = 3,
};

/// Resolves a generated face-list start in this grid's packed halfword pool.
///
/// halfwordOffset counts signed-halfword elements and is evaluated once.
/// It must select an in-bounds list start in the captured pool.
/// Captures `_gAcropolisRoofGardenCollisionGridCells`; returns a borrowed s16* without reading it.
/// The table include supplies list-start constants; empty cells use NULL.
/// Define only for that include and undefine immediately afterward.
#define ACROPOLIS_ROOF_GARDEN_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gAcropolisRoofGardenCollisionGridCells[(halfwordOffset)])
/// X-major 3-by-3 cell directory borrowing the packed signed face-index lists.
static s16* _gAcropolisRoofGardenCollisionGridTable[ACROPOLIS_ROOF_GARDEN_GRID_CELL_COUNT_X * ACROPOLIS_ROOF_GARDEN_GRID_CELL_COUNT_Z] = {
#include "assets/acropolis_roof_garden_collision_07EE4_table.inc"
};
#undef ACROPOLIS_ROOF_GARDEN_CELL_FACE_LIST_AT_OFFSET

/// Roof garden's collision mesh and 3-by-3 XZ index of 4000-game-unit cells.
///
/// The grid origin is (-10461, -10652) in room XZ. Setup binds its current
/// view transform; the mutable descriptor and all pools live with the overlay.
static WorldCollisionGrid _gAcropolisRoofGardenCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gAcropolisRoofGardenCollisionGridNormals,
    .vertices    = _gAcropolisRoofGardenCollisionGridVerts,
    .faces       = _gAcropolisRoofGardenCollisionGridFaces,
    .cellFaceIds = _gAcropolisRoofGardenCollisionGridTable,
    .xBias       = 10461,
    .zBias       = 10652,
    .cellCountX  = ACROPOLIS_ROOF_GARDEN_GRID_CELL_COUNT_X,
    .cellCountZ  = ACROPOLIS_ROOF_GARDEN_GRID_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gAcropolisRoofGardenCollisionGridFaces),
};

/// Directed view boundaries for both directions of 2/3, 3/4 and 4/5.
///
/// Positions/radii use room-coordinate units; normals use Q12. Setup binds,
/// links and enables all six writable records, including the LAST record.
static WorldCollisionTrigger _gAcropolisRoofGardenViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { -7264, -1440, -8768, 0 }, { { -1017, 5600, 185, 0 }, { 991, 5600, -215, 0 }, { -1017, -5600, 185, 0 }, { 991, -5600, -215, 0 } }, { 801, 0, 4025, 0 }, { 0, 0, 0, 0 }, 5678, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4544, -1536, -6880, 0 }, { { -1024, 5600, 0, 0 }, { 1024, 5600, 0, 0 }, { -1024, -5600, 0, 0 }, { 1024, -5600, 0, 0 } }, { 0, 0, 4105, 0 }, { 0, 0, 0, 0 }, 5678, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7296, -1696, -8448, 0 }, { { 982, 5600, -261, 0 }, { -1004, 5600, 237, 0 }, { 982, -5600, -261, 0 }, { -1004, -5600, 237, 0 } }, { -999, 0, -3982, 0 }, { 0, 0, 0, 0 }, 5678, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4480, -1856, -6560, 0 }, { { 1024, 5600, 0, 0 }, { -1024, 5600, 0, 0 }, { 1024, -5600, 0, 0 }, { -1024, -5600, 0, 0 } }, { 0, 0, -4106, 0 }, { 0, 0, 0, 0 }, 5678, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4736, -1568, -4000, 0 }, { { 2496, 5600, 0, 0 }, { -2496, 5600, 0, 0 }, { 2496, -5600, 0, 0 }, { -2496, -5600, 0, 0 } }, { 0, 0, -4103, 0 }, { 0, 0, 0, 0 }, 6122, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4608, -1824, -4640, 0 }, { { -2496, 5600, 0, 0 }, { 2496, 5600, 0, 0 }, { -2496, -5600, 0, 0 }, { 2496, -5600, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, 0, 0 }, 6122, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Trigger payloads selecting the sanctuary endpoint and the room's scripted actions.
enum {
    ACROPOLIS_ROOF_GARDEN_SANCTUARY_WARP_ENDPOINTS   = (1 << 4) | 2, // Departure endpoint 1, sanctuary arrival endpoint 2
    ACROPOLIS_ROOF_GARDEN_ROOM_ACTION_UNHANDLED      = 0,
    ACROPOLIS_ROOF_GARDEN_ROOM_ACTION_ARM_ENTRANCE   = 1,
    ACROPOLIS_ROOF_GARDEN_ROOM_ACTION_START_ENTRANCE = 2,
    ACROPOLIS_ROOF_GARDEN_ROOM_COMMAND_STORY         = 4,
};

/// Roof-garden interactions: sanctuary warp, three CAP actions and three automatic room actions.
///
/// Room actions 1/2 arm/start the Maggot/Caterpillar entrance; action 0 is ignored.
/// Positions/radii use room-coordinate units and normals Q12. Setup binds,
/// links and enables every writable record through the final LAST marker.
static WorldCollisionTrigger _gAcropolisRoofGardenActionTriggers[7] = {
    { NULL, NULL, NULL, { -7465, -32, -9928, 0 }, { { -752, 0, -416, 0 }, { 752, 0, -416, 0 }, { -752, 0, 416, 0 }, { 752, 0, 416, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, 4096, 0 }, 858, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_SANCTUARY, ACROPOLIS_ROOF_GARDEN_SANCTUARY_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4608, -128, -4816, 0 }, { { -1040, 0, -608, 0 }, { 1040, 0, -608, 0 }, { -1040, 0, 608, 0 }, { 1040, 0, 608, 0 } }, { 0, 4116, 0, 0 }, { -201, 0, -4091, 0 }, 1200, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4640, -96, -3776, 0 }, { { -752, 0, -368, 0 }, { 752, 0, -368, 0 }, { -752, 0, 368, 0 }, { 752, 0, 368, 0 } }, { 0, 4100, 0, 0 }, { -1, 0, 4095, 0 }, 836, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, ACROPOLIS_ROOF_GARDEN_ROOM_COMMAND_STORY, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4400, -96, -2080, 0 }, { { -640, 0, -560, 0 }, { 640, 0, -560, 0 }, { -640, 0, 560, 0 }, { 640, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 849, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7697, -160, -8384, 0 }, { { -736, 0, -1136, 0 }, { 1280, 0, -1136, 0 }, { -1279, 0, 1136, 0 }, { 737, 0, 1136, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1707, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, ACROPOLIS_ROOF_GARDEN_ROOM_ACTION_UNHANDLED, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -4640, -64, -6544, 0 }, { { -1296, 0, -208, 0 }, { 1296, 0, -208, 0 }, { -1296, 0, 208, 0 }, { 1296, 0, 208, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 1311, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, ACROPOLIS_ROOF_GARDEN_ROOM_ACTION_ARM_ENTRANCE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -4640, -64, -7104, 0 }, { { -1296, 0, -208, 0 }, { 1296, 0, -208, 0 }, { -1296, 0, 208, 0 }, { 1296, 0, 208, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 1311, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, ACROPOLIS_ROOF_GARDEN_ROOM_ACTION_START_ENTRANCE, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Placement-entry keys and decimal file numbers used by the resource lists.
///
/// Entry keys match the placement tables. File numbers select actor libraries
/// together with the base-10 or base-20 file-group selector in each resource.
enum {
    ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_VIEW_FIGURE = 110,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_MAGGOT      = 55,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_CATERPILLAR = 26,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_MOTH        = 8,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_VIEW_FIGURE  = 108,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_MAGGOT       = 55,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_CATERPILLAR  = 26,
    ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_MOTH         = 7,
};

/// Figure and room-bound Maggot resources for roof-garden placement variant 1.
///
/// Entry 110 loads actor file 110800 and spawns its view-parented figure;
/// entry 55 loads room-bound Maggot file 205500. `AREA_PLACEMENT_END` ends
/// the list. Placements and task recipes are borrowed while their overlays live.
static AreaResource _gAcropolisRoofGardenVariant1Resources[3] = {
    { ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_VIEW_FIGURE, ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_VIEW_FIGURE, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, VIEW_FIGURE_TASK_BODY, { 0, 0 }, gActor110800ViewFigureTasks },
    { ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_MAGGOT, ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_MAGGOT, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor205500MaggotCaterpillarTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Regular Maggot and Moth resources for roof-garden placement variant 2.
///
/// Entry 55 loads Maggot file 105500; entry 8 loads Moth file 200700.
/// `AREA_PLACEMENT_END` ends the list. Placement entry IDs select these
/// borrowed task recipes during loading and spawning.
static AreaResource _gAcropolisRoofGardenVariant2Resources[3] = {
    { ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_MAGGOT, ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_MAGGOT, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor105500MaggotCaterpillarTasks },
    { ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_MOTH, ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_MOTH, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor00700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Figure and room-bound Caterpillar resources for roof-garden placement variant 7.
///
/// Entry 110 loads actor file 110800 and spawns its view-parented figure;
/// entry 26 loads room-bound Caterpillar file 202600. `AREA_PLACEMENT_END`
/// ends the list. The room overlay owns the resource list and borrows task recipes.
static AreaResource _gAcropolisRoofGardenVariant7Resources[3] = {
    { ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_VIEW_FIGURE, ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_VIEW_FIGURE, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, VIEW_FIGURE_TASK_BODY, { 0, 0 }, gActor110800ViewFigureTasks },
    { ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_CATERPILLAR, ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_CATERPILLAR, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor202600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Regular Caterpillar and Moth resources for roof-garden placement variant 8.
///
/// Entry 26 loads Caterpillar file 102600; entry 8 loads Moth file 200700.
/// `AREA_PLACEMENT_END` ends the list. The room overlay owns this list and
/// borrows the loaded actor overlays' task recipes.
static AreaResource _gAcropolisRoofGardenVariant8Resources[3] = {
    { ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_CATERPILLAR, ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_CATERPILLAR, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { ACROPOLIS_ROOF_GARDEN_RESOURCE_ENTRY_MOTH, ACROPOLIS_ROOF_GARDEN_RESOURCE_FILE_MOTH, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor00700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gAcropolisRoofGardenAreaVariants[12] = {
    { NULL, NULL },
    { gMapAkropolisRoofGardenVariant1Placements, _gAcropolisRoofGardenVariant1Resources },
    { gMapAkropolisRoofGardenVariant2Placements, _gAcropolisRoofGardenVariant2Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { gMapAkropolisRoofGardenVariant7Placements, _gAcropolisRoofGardenVariant7Resources },
    { gMapAkropolisRoofGardenVariant8Placements, _gAcropolisRoofGardenVariant8Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

/// Texture bindings and colour-command values shared by the view sprite sources.
///
/// Both pages use 8-bit texels at VRAM Y zero; the CLUT is row 255, X zero.
/// Neutral RGB is 128, and zero code flags request opaque colour modulation.
enum {
    ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1 = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2 = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT           = getClut(0, 255),
    ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB    = 128,
    ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE         = 0,
};

/// Empty sprite list for mapped roof-garden view 1.
///
/// The zero first count keeps decoded image strips; the next record ends the
/// list. The view retains this address as its empty source slot. Overlay-owned.
static SpriteBatch _gAcropolisRoofGardenView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles and sorting depths for mapped roof-garden view 2.
///
/// 26 writable elements use pixel positions/sizes and unscaled sorting
/// depths. `_gAcropolisRoofGardenView2SpriteBatches` selects their ranges;
/// source storage belongs to the loaded room overlay.
static SpriteSource _gAcropolisRoofGardenView2SpriteSources[26] = {
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, -40, 40, 729, { .fields = { 96, 208 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, -32, 40, 765, { .fields = { 104, 40 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 40 } }, -160, 40, 701, { .fields = { 104, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 16 } }, -152, 40, 698, { .fields = { 88, 160 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 48 } }, -136, 40, 661, { .fields = { 104, 48 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 24 } }, -120, 40, 653, { .fields = { 80, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, 64, 658, { .fields = { 104, 232 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 32, 16 } }, -96, 40, 650, { .fields = { 72, 176 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 40 } }, -72, 56, 645, { .fields = { 96, 96 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 32 } }, -56, 56, 652, { .fields = { 104, 168 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, -48, 40, 686, { .fields = { 96, 48 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 48 } }, -24, 40, 751, { .fields = { 112, 96 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 40 } }, -16, 40, 839, { .fields = { 112, 144 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 32 } }, -8, 40, 883, { .fields = { 104, 200 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 32 } }, 0, 40, 950, { .fields = { 104, 136 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, 8, 48, 1072, { .fields = { 96, 72 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 32 } }, 88, 40, 1011, { .fields = { 112, 224 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 48 } }, 96, 40, 817, { .fields = { 112, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 56 } }, 104, 40, 667, { .fields = { 120, 200 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 64 } }, 112, 40, 551, { .fields = { 120, 80 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 80 } }, 120, 40, 473, { .fields = { 120, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, 128, 104, 453, { .fields = { 96, 192 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, 128, 40, 421, { .fields = { 96, 24 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 40 } }, 136, 48, 388, { .fields = { 112, 184 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 56 } }, 144, 64, 362, { .fields = { 120, 144 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, 152, 96, 352, { .fields = { 96, 136 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
};

/// Sprite ranges for mapped roof-garden view 2.
///
/// Ranges cover all 26 source elements. The zero first count keeps
/// decoded image strips; `SPRITE_BATCH_END` terminates the list.
/// These writable records belong to the loaded room overlay.
static SpriteBatch _gAcropolisRoofGardenView2SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisRoofGardenView2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles and sorting depths for mapped roof-garden view 3.
///
/// 57 writable elements use pixel positions/sizes and unscaled sorting
/// depths. `_gAcropolisRoofGardenView3SpriteBatches` selects their ranges;
/// source storage belongs to the loaded room overlay.
static SpriteSource _gAcropolisRoofGardenView3SpriteSources[57] = {
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 40 } }, 32, 56, 563, { .fields = { 96, 56 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, 40, 48, 551, { .fields = { 104, 96 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 32 } }, 40, 80, 563, { .fields = { 56, 80 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 56 } }, 48, 40, 518, { .fields = { 24, 56 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, 48, 499, { .fields = { 120, 232 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, 64, 48, 479, { .fields = { 0, 96 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 40 } }, 72, 48, 456, { .fields = { 104, 56 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 40 } }, 80, 80, 424, { .fields = { 64, 216 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, 104, 471, { .fields = { 32, 240 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 40 } }, 96, 80, 417, { .fields = { 72, 216 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, 80, 48, 418, { .fields = { 16, 104 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, 48, 403, { .fields = { 24, 240 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, 96, 56, 412, { .fields = { 80, 120 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 40 } }, 104, 56, 401, { .fields = { 96, 96 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, 64, 392, { .fields = { 120, 96 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, 120, 64, 377, { .fields = { 112, 96 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 32 } }, 128, 64, 346, { .fields = { 64, 80 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 8 } }, 136, 64, 326, { .fields = { 48, 112 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 32 } }, 136, 88, 349, { .fields = { 48, 192 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 104, 480, { .fields = { 24, 112 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, 32, 72, 561, { .fields = { 80, 232 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, 40, 56, 558, { .fields = { 64, 120 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 24 } }, 48, 40, 599, { .fields = { 0, 232 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, 72, 608, { .fields = { 16, 232 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 72, 64 } }, 64, 40, 557, { .fields = { 104, 120 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 88 } }, 136, 16, 560, { .fields = { 104, 144 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 40, 24 } }, 48, -120, 855, { .fields = { 88, 232 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 56, 40 } }, 32, -96, 920, { .fields = { 40, 40 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 56, 40 } }, 32, -56, 1272, { .fields = { 40, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 72, 40 } }, 88, -120, 1111, { .fields = { 8, 120 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 72, 32 } }, 88, -40, 1225, { .fields = { 0, 224 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 40, 8 } }, 112, -8, 1250, { .fields = { 8, 112 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 8 } }, 72, -16, 1275, { .fields = { 48, 184 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 40 } }, 88, -80, 1111, { .fields = { 96, 184 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 32 } }, 112, -72, 1109, { .fields = { 96, 224 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 40 } }, 136, -80, 1112, { .fields = { 112, 56 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 64, 56 } }, -16, 16, 1104, { .fields = { 96, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 56, 144 } }, -112, -64, 1064, { .fields = { 72, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 40, 120 } }, -56, -48, 1149, { .fields = { 32, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 32, 48 } }, 80, 16, 1285, { .fields = { 120, 184 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 32, 24 } }, 112, 40, 1204, { .fields = { 32, 160 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 64 } }, -8, -8, 1563, { .fields = { 48, 192 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 88 } }, 8, -32, 1580, { .fields = { 80, 144 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 56 } }, 32, 0, 1639, { .fields = { 24, 184 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 72 } }, -160, 48, 600, { .fields = { 64, 144 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 72 } }, -144, 40, 650, { .fields = { 48, 120 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 32 } }, -128, 48, 700, { .fields = { 64, 160 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 16 } }, -112, 40, 750, { .fields = { 40, 80 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 40 } }, -96, 40, 800, { .fields = { 80, 176 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 16 } }, -80, 40, 850, { .fields = { 40, 96 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 48 } }, -64, 32, 900, { .fields = { 8, 56 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 40 } }, -48, 32, 950, { .fields = { 72, 216 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 40 } }, -24, 32, 1050, { .fields = { 72, 80 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, 32, 1100, { .fields = { 80, 136 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, 24, 32, 1100, { .fields = { 8, 104 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, 32, 40, 1100, { .fields = { 88, 120 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, 40, 48, 1100, { .fields = { 40, 240 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
};

/// Sprite ranges for mapped roof-garden view 3.
///
/// Ranges cover all 57 source elements. The zero first count keeps
/// decoded image strips; `SPRITE_BATCH_END` terminates the list.
/// These writable records belong to the loaded room overlay.
static SpriteBatch _gAcropolisRoofGardenView3SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 7, 0, 0, { 4, 0 } },
    { 26, 10, 0, 0, { 1, 0 } },
    { 36, 3, 0, 0, { 5, 0 } },
    { 39, 2, 0, 0, { 0, 0 } },
    { 41, 3, 0, 0, { 6, 0 } },
    { 44, ARRAY_SIZE(_gAcropolisRoofGardenView3SpriteSources) - 44, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles and sorting depths for mapped roof-garden view 4.
///
/// 21 writable elements use pixel positions/sizes and unscaled sorting
/// depths. `_gAcropolisRoofGardenView4SpriteBatches` selects their ranges;
/// source storage belongs to the loaded room overlay.
static SpriteSource _gAcropolisRoofGardenView4SpriteSources[21] = {
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 32, 104 } }, -160, -96, 462, { .fields = { 56, 112 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, -56, 462, { .fields = { 48, 240 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 40, 104 } }, -160, 16, 472, { .fields = { 88, 152 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 32, 112 } }, -40, 8, 525, { .fields = { 56, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 24 } }, -8, 88, 500, { .fields = { 56, 216 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 40, 88 } }, -120, 32, 522, { .fields = { 104, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 40, 88 } }, -80, 32, 522, { .fields = { 104, 88 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, 64, 664, { .fields = { 32, 240 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 40, 96 } }, 96, 0, 624, { .fields = { 16, 96 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, 136, 8, 663, { .fields = { 40, 240 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_1, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 48 } }, 136, 48, 599, { .fields = { 112, 176 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, -24, 966, { .fields = { 24, 192 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 96 } }, -40, -48, 1008, { .fields = { 16, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 80 } }, -48, -48, 1105, { .fields = { 8, 176 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 40, 152 } }, -32, -96, 974, { .fields = { 88, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 32, 96 } }, 8, -48, 963, { .fields = { 24, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 40 } }, -96, -16, 1550, { .fields = { 64, 216 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 48 } }, -104, -8, 1500, { .fields = { 40, 192 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 48 } }, -112, -8, 1450, { .fields = { 32, 192 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 48 } }, -120, -8, 1400, { .fields = { 48, 192 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, 24, 1350, { .fields = { 56, 240 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
};

/// Sprite ranges for mapped roof-garden view 4.
///
/// Ranges cover all 21 source elements. The zero first count keeps
/// decoded image strips; `SPRITE_BATCH_END` terminates the list.
/// These writable records belong to the loaded room overlay.
static SpriteBatch _gAcropolisRoofGardenView4SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 3, 0 } },
    { 7, 4, 0, 0, { 0, 0 } },
    { 11, 5, 0, 0, { 2, 0 } },
    { 16, ARRAY_SIZE(_gAcropolisRoofGardenView4SpriteSources) - 16, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles and sorting depths for mapped roof-garden view 5.
///
/// 47 writable elements use pixel positions/sizes and unscaled sorting
/// depths. `_gAcropolisRoofGardenView5SpriteBatches` selects their ranges;
/// source storage belongs to the loaded room overlay.
static SpriteSource _gAcropolisRoofGardenView5SpriteSources[47] = {
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 40, 16 } }, -48, -32, 950, { .fields = { 32, 112 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 32, 32 } }, -8, -40, 1019, { .fields = { 48, 192 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 56, 48 } }, -64, -80, 875, { .fields = { 72, 72 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 40, 40 } }, -8, -80, 909, { .fields = { 88, 160 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 40, 24 } }, 0, -104, 923, { .fields = { 32, 72 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 56, 40 } }, -56, -120, 667, { .fields = { 72, 120 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 72, 72 } }, -160, 48, 153, { .fields = { 56, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 16, 32 } }, -88, 88, 296, { .fields = { 72, 160 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 56, 24 } }, -56, 96, 294, { .fields = { 40, 232 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 32 } }, 0, 88, 298, { .fields = { 80, 200 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 32, 24 } }, 24, 96, 298, { .fields = { 96, 232 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 32 } }, 56, 88, 302, { .fields = { 104, 200 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 24, 16 } }, 80, 104, 299, { .fields = { 48, 96 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, -120, 1294, { .fields = { 48, 24 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 48, -120, 1265, { .fields = { 48, 16 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 56, -120, 1302, { .fields = { 48, 40 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 64, -120, 1240, { .fields = { 48, 32 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, -120, 1256, { .fields = { 56, 224 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, -120, 1184, { .fields = { 56, 184 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, -120, 1118, { .fields = { 48, 8 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, -120, 1109, { .fields = { 48, 0 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, -112, 1316, { .fields = { 48, 144 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 48, -112, 1284, { .fields = { 48, 136 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 56, -112, 1281, { .fields = { 48, 160 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 64, -112, 1275, { .fields = { 48, 152 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, -112, 1229, { .fields = { 48, 56 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, -112, 1209, { .fields = { 48, 48 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, -112, 1147, { .fields = { 48, 128 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, -112, 1048, { .fields = { 48, 64 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 48, -104, 1307, { .fields = { 64, 160 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 56, -104, 1300, { .fields = { 64, 152 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 64, -104, 1276, { .fields = { 64, 176 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, -104, 1258, { .fields = { 64, 168 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, -104, 1258, { .fields = { 64, 144 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, -104, 1189, { .fields = { 72, 224 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, -104, 1085, { .fields = { 80, 192 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 64, -96, 1287, { .fields = { 64, 136 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, -96, 1268, { .fields = { 64, 128 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, -96, 1240, { .fields = { 56, 160 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, -96, 1196, { .fields = { 56, 152 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, -96, 1081, { .fields = { 56, 176 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, -96, 1069, { .fields = { 56, 168 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, -88, 1267, { .fields = { 56, 144 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, -88, 1199, { .fields = { 64, 224 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, -88, 1174, { .fields = { 64, 184 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, -88, 1070, { .fields = { 56, 136 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
    { ACROPOLIS_ROOF_GARDEN_SPRITE_TEXTURE_PAGE_2, ACROPOLIS_ROOF_GARDEN_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, -88, 1054, { .fields = { 56, 128 } }, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_NEUTRAL_RGB, ACROPOLIS_ROOF_GARDEN_SPRITE_OPAQUE },
};

/// Sprite ranges for mapped roof-garden view 5.
///
/// Ranges cover all 47 source elements. The zero first count keeps
/// decoded image strips; `SPRITE_BATCH_END` terminates the list.
/// These writable records belong to the loaded room overlay.
static SpriteBatch _gAcropolisRoofGardenView5SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 7, 0, 0, { 2, 0 } },
    { 13, ARRAY_SIZE(_gAcropolisRoofGardenView5SpriteSources) - 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped roof-garden view 6.
///
/// The zero first count keeps decoded image strips; the next record ends
/// the list. Its address also occupies the view's empty source slot.
/// Storage belongs to the loaded room overlay.
static SpriteBatch _gAcropolisRoofGardenView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped roof-garden view 7.
///
/// The zero first count keeps decoded image strips; the next record ends
/// the list. Its address also occupies the view's empty source slot.
/// Storage belongs to the loaded room overlay.
static SpriteBatch _gAcropolisRoofGardenView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gAcropolisRoofGardenSpriteViews[ACROPOLIS_ROOF_GARDEN_VIEW_COUNT] = {
    { { .empty = _gAcropolisRoofGardenView1SpriteBatches }, _gAcropolisRoofGardenView1SpriteBatches, NULL },
    { { .elements = _gAcropolisRoofGardenView2SpriteSources }, _gAcropolisRoofGardenView2SpriteBatches, NULL },
    { { .elements = _gAcropolisRoofGardenView3SpriteSources }, _gAcropolisRoofGardenView3SpriteBatches, NULL },
    { { .elements = _gAcropolisRoofGardenView4SpriteSources }, _gAcropolisRoofGardenView4SpriteBatches, NULL },
    { { .elements = _gAcropolisRoofGardenView5SpriteSources }, _gAcropolisRoofGardenView5SpriteBatches, NULL },
    { { .empty = _gAcropolisRoofGardenView6SpriteBatches }, _gAcropolisRoofGardenView6SpriteBatches, NULL },
    { { .empty = _gAcropolisRoofGardenView7SpriteBatches }, _gAcropolisRoofGardenView7SpriteBatches, NULL },
};

/// Authored point lights for model shading throughout the roof garden.
///
/// Positions and inner/outer radii use integer world units; RGB intensities
/// have 12 fractional bits (ONE is full intensity). Every light accepts every view.
/// Gameplay updates the transform caches and attenuation, so these records stay
/// writable and must remain live while the room overlay is in use.
static WorldCoordPointLight _gAcropolisRoofGardenPointLights[] = {
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -5884, -700, -9680 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -6210, -700, -2566 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4984, -100, -5134 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2457, 3276, ONE },
        },
        .inner = 100,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4153, -100, -5134 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2457, 3276, ONE },
        },
        .inner = 100,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4153, -1500, -4622 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2457, 3276, ONE },
        },
        .inner = 100,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -5007, -1500, -4622 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2457, 3276, ONE },
        },
        .inner = 100,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4654, -200, -3856 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 2457, 2867 },
        },
        .inner = 100,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4654, -2000, -2126 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 819, 1638 },
        },
        .inner = 100,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -8188, -1800, -0x2864 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2867, 3686, ONE },
        },
        .inner = 200,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -6846, -1800, -0x2864 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2867, 3686, ONE },
        },
        .inner = 200,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -8931, -700, -9680 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -6248, -700, -6705 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -3472, -700, -7111 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -3202, -700, -2566 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
};

/// Fourteen point lights used to shade models throughout roof-garden room 1.
///
/// No directional or cone lights are supplied. The collection borrows the
/// overlay's mutable point-light array, whose coordinates lighting composes.
static WorldCoordRoomLights _gAcropolisRoofGardenRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gAcropolisRoofGardenPointLights),
    .pointLights           = _gAcropolisRoofGardenPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

ViewCamera gAcropolisRoofGardenViewCameras[8] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 5600, 0x7530, 5960 } }, 589 },
    { { { { -3911, 0, -1216 }, { 204, 4037, -656 }, { 1199, -687, -3855 } }, { 7887, 505, 6331 } }, 230 },
    { { { { 3788, 0, -1557 }, { 186, 4066, 454 }, { 1545, -491, 3761 } }, { 7879, 656, 0x29A1 } }, 230 },
    { { { { 3851, 0, -1394 }, { -208, 4049, -576 }, { 1379, 612, 3807 } }, { 5932, 1317, 8463 } }, 230 },
    { { { { -4010, 0, -830 }, { -785, 1329, 3793 }, { 269, 3874, -1302 } }, { 5201, 4187, 2281 } }, 230 },
    { { { { -2305, 0, -3385 }, { -1984, 3319, 1350 }, { 2743, 2400, -1867 } }, { 5865, 2196, 2875 } }, 230 },
    { { { { -3852, 0, -1389 }, { -827, 3291, 2292 }, { 1116, 2437, -3096 } }, { 4797, 459, 3558 } }, 230 },
    { { { { -3750, 0, 1646 }, { 1161, 2905, 2643 }, { -1168, 2887, -2659 } }, { 3260, 3610, 2030 } }, 230 },
};

/// Two roof-garden quads that block enemy line-of-sight tests.
///
/// Origins/corner offsets use room-coordinate units; normals use Q12. Setup
/// links and enables both writable records through LAST. Low flag bit 0 and
/// the final zero byte are retained authored values with unproven roles.
static WorldCollisionOccluder _gAcropolisRoofGardenOccluders[2] = {
    { NULL, NULL, { -5888, -4064, -8128, 0 }, { { -4256, 0, -2848, 0 }, { 4256, 0, -2848, 0 }, { -4256, 0, 2848, 0 }, { 4256, 0, 2848, 0 } }, { 0, 4105, 0, 0 }, 5120, 1, 0 },
    { NULL, NULL, { -4576, -496, -4400, 0 }, { { -576, 928, 0, 0 }, { 576, 928, 0, 0 }, { -576, -928, 0, 0 }, { 576, -928, 0, 0 } }, { 0, 0, 4111, 0 }, 1086, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Cue-2 script bases in the currently loaded type-1 sound bank.
enum {
    ACROPOLIS_ROOF_GARDEN_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1D,
    ACROPOLIS_ROOF_GARDEN_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1F,
};

/// Shared walking, running and scripted-jump cues for surface classes 1 and 2.
///
/// Cue 1 adds one to each base; companion playback adds 100. Scripted jumps
/// use the walking pair. The surface records borrow this overlay-owned record.
static WorldCollisionFootstepSounds _gAcropolisRoofGardenFootstepSounds = {
    .walk         = ACROPOLIS_ROOF_GARDEN_FOOTSTEP_WALK,
    .run          = ACROPOLIS_ROOF_GARDEN_FOOTSTEP_RUN,
    .scriptedJump = ACROPOLIS_ROOF_GARDEN_FOOTSTEP_WALK,
};

/// Blocking, impact-enabled, pushback surfaces without footstep cues (classes 0, 3..7).
static WorldCollisionSurfaceProperties _gAcropolisRoofGardenSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking, impact-enabled, pushback surface class 1 with the room's footstep cues.
static WorldCollisionSurfaceProperties _gAcropolisRoofGardenSurfaceClass1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisRoofGardenFootstepSounds,
};

/// Blocking, impact-enabled, pushback surface class 2 with the room's footstep cues.
static WorldCollisionSurfaceProperties _gAcropolisRoofGardenSurfaceClass2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisRoofGardenFootstepSounds,
};

WorldCollisionSurfaceProperties* gAcropolisRoofGardenSurfacePropertiesByClass[8] = {
    &_gAcropolisRoofGardenSilentSurfaceProperties,
    &_gAcropolisRoofGardenSurfaceClass1Properties,
    &_gAcropolisRoofGardenSurfaceClass2Properties,
    &_gAcropolisRoofGardenSilentSurfaceProperties,
    &_gAcropolisRoofGardenSilentSurfaceProperties,
    &_gAcropolisRoofGardenSilentSurfaceProperties,
    &_gAcropolisRoofGardenSilentSurfaceProperties,
    &_gAcropolisRoofGardenSilentSurfaceProperties,
};

static TmdBone _gAcropolisRoofGardenModel09868Skeleton[1] = {
#include "assets/acropolis_roof_garden_model_09868_skeleton.inc"
};

static u32 _gAcropolisRoofGardenModel09868PartVerts[1] = {
#include "assets/acropolis_roof_garden_model_09868_partVerts.inc"
};

static SVECTOR _gAcropolisRoofGardenModel09868Verts[4] = {
#include "assets/acropolis_roof_garden_model_09868_verts.inc"
};

static SVECTOR _gAcropolisRoofGardenModel09868Normals[2] = {
#include "assets/acropolis_roof_garden_model_09868_normals.inc"
};

static u32 _gAcropolisRoofGardenModel09868Stream[18] = {
#include "assets/acropolis_roof_garden_model_09868_stream.inc"
};

TmdSource gAcropolisRoofGardenModel09868 = {
    0,
    104,
    0,
    1,
    _gAcropolisRoofGardenModel09868PartVerts,
    _gAcropolisRoofGardenModel09868Verts,
    _gAcropolisRoofGardenModel09868Normals,
    _gAcropolisRoofGardenModel09868Skeleton,
    _gAcropolisRoofGardenModel09868Stream,
};

/// Last ambience volume percentage requested by the view controller (0, 30 or 100).
///
/// Reset to zero on its first tick. Requests update this latch even when the
/// sound queue rejects them; it records no confirmation of playback or fading.
static s32 _gAcropolisRoofGardenRequestedAmbiencePercent = 0;

SVECTOR ActorContact_ScratchPosition;

/// Adjusts the roof-garden ambience to the current camera view.
///
/// The first tick resets the requested-volume latch. Subsequent ticks request
/// 30 percent in view 5, 100 percent in view 7 and silence elsewhere, sending
/// sound commands only when that percentage changes. Leaving an audible view
/// requests a 30-update fade. The latch tracks requests, not driver completion.
static void _acropolisRoofGardenAmbienceTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_INITIALIZE      = 0,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_UPDATE          = 1,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_SILENT          = 0,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_VIEW5_PERCENT   = 30,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT    = 100,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_MAX_ATTENUATION = 127,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_FADE_UPDATES    = 30,
    };
    s32 volumePercent;
    u8  view;
    s32 previousPercent;

    switch (task->state) {
        case ACROPOLIS_ROOF_GARDEN_AMBIENCE_INITIALIZE:
            _gAcropolisRoofGardenRequestedAmbiencePercent = ACROPOLIS_ROOF_GARDEN_AMBIENCE_SILENT;
            task->state                                   = task->state + 1;
            return;
        case ACROPOLIS_ROOF_GARDEN_AMBIENCE_UPDATE:
            break;
        default:
            return;
    }

    view = gGameSession->location.loc.view;
    if (view != 5) {
        volumePercent = ACROPOLIS_ROOF_GARDEN_AMBIENCE_SILENT;
        if (view == 7) {
            volumePercent = ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT;
        }
    } else {
        volumePercent = ACROPOLIS_ROOF_GARDEN_AMBIENCE_VIEW5_PERCENT;
    }

    previousPercent = _gAcropolisRoofGardenRequestedAmbiencePercent;
    if (volumePercent == previousPercent) {
        return;
    }
    // Convert the requested percentage to the driver's signed attenuation byte.
    if (previousPercent == ACROPOLIS_ROOF_GARDEN_AMBIENCE_SILENT) {
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_ROOF_GARDEN_AMBIENCE, 0, (s8)(((ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT - volumePercent) * ACROPOLIS_ROOF_GARDEN_AMBIENCE_MAX_ATTENUATION) / ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT));
    } else if (volumePercent == ACROPOLIS_ROOF_GARDEN_AMBIENCE_SILENT) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_ROOF_GARDEN_AMBIENCE, ACROPOLIS_ROOF_GARDEN_AMBIENCE_FADE_UPDATES);
    } else {
        sndEvtRequestScriptMix(SOUND_ACROPOLIS_ROOF_GARDEN_AMBIENCE, 0, (s8)(((ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT - volumePercent) * ACROPOLIS_ROOF_GARDEN_AMBIENCE_MAX_ATTENUATION) / ACROPOLIS_ROOF_GARDEN_AMBIENCE_FULL_PERCENT));
    }
    _gAcropolisRoofGardenRequestedAmbiencePercent = volumePercent;
}

/// Allows a room transition and suppresses the untriggered sanctuary event on departure.
///
/// Borrows a complete request and writable reply, which may alias, through
/// synchronous dispatch. The reply preserves all eight bytes. Executing a
/// sanctuary-bound transition with a clear event latch sets it to 2 and hides
/// saved object 0x13; queries change neither. Always returns 1 (allowed).
static s32 _acropolisRoofGardenResolveRoomTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_SANCTUARY_EVENT_UNSET      = 0,
        ACROPOLIS_ROOF_GARDEN_SANCTUARY_EVENT_SUPPRESSED = 2,
        ACROPOLIS_ROOF_GARDEN_PICKUP_OBJECT_ID           = 0x13,
        ACROPOLIS_ROOF_GARDEN_PICKUP_HIDDEN              = 2,
        ACROPOLIS_ROOF_GARDEN_TRANSITION_ALLOWED         = 1,
    };
    *reply = *request;
    if (request->areaId == GAME_AREA_ACROPOLIS_SANCTUARY && request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH) == ACROPOLIS_ROOF_GARDEN_SANCTUARY_EVENT_UNSET) {
        gameFlagSetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH, ACROPOLIS_ROOF_GARDEN_SANCTUARY_EVENT_SUPPRESSED);
        areaSetCurrentObjectState(ACROPOLIS_ROOF_GARDEN_PICKUP_OBJECT_ID, ACROPOLIS_ROOF_GARDEN_PICKUP_HIDDEN);
    }
    return ACROPOLIS_ROOF_GARDEN_TRANSITION_ALLOWED;
}

/// Refuses every key-item use request in the roof garden.
///
/// All arguments are ignored. Returning zero selects the inventory's
/// "No use now" notice without consuming the selected item.
static s32 _acropolisRoofGardenRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ACROPOLIS_ROOF_GARDEN_KEY_ITEM_USE_REFUSED = 0 };

    return ACROPOLIS_ROOF_GARDEN_KEY_ITEM_USE_REFUSED;
}

/// Arms and starts the Maggot/Caterpillar entrance at the room's action triggers.
///
/// Borrows a four-byte `DirectionActionRequest` through synchronous dispatch;
/// only `actionId` is read and no pointer is retained. In variants 1/7, action
/// 1 arms clear progress and action 2 starts the script only while armed.
/// Other actions do nothing. The zero second word is ignored; returns 1.
static s32 _acropolisRoofGardenHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_ACTION_ARM_ENTRANCE       = 1,
        ACROPOLIS_ROOF_GARDEN_ACTION_START_ENTRANCE     = 2,
        ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_CLEAR   = 0,
        ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_ARMED   = 1,
        ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_STARTED = 2,
        ACROPOLIS_ROOF_GARDEN_ACTION_REPLY              = 1,
    };
    switch (request->actionId) {
        case ACROPOLIS_ROOF_GARDEN_ACTION_ARM_ENTRANCE:
            if (((gGameSession->location.loc.variant == 1) || (gGameSession->location.loc.variant == 7)) && (gameFlagGetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS) == ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_CLEAR)) {
                gameFlagSetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS, ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_ARMED);
            }
            break;
        case ACROPOLIS_ROOF_GARDEN_ACTION_START_ENTRANCE:
            if (((gGameSession->location.loc.variant == 1) || (gGameSession->location.loc.variant == 7)) && (gameFlagGetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS) == ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_ARMED)) {
                evsStartScript(_gAcropolisRoofGardenMaggotCaterpillarEntranceScript, EVENT_SCRIPT_HUD_KEEP);
                gameFlagSetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS, ACROPOLIS_ROOF_GARDEN_ENTRANCE_PROGRESS_STARTED);
            }
            break;
    }
    return ACROPOLIS_ROOF_GARDEN_ACTION_REPLY;
}

/// Plays roof-garden sound entry 3 for room sound cue 3; other cues do nothing.
///
/// Integer payloads are consumed synchronously; the second is ignored.
/// Always returns zero. The empty switch arms preserve the compiled dispatch
/// tree; the last arm's original cue value is unproven.
static s32 _acropolisRoofGardenHandleSoundMessage(Task* task, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum { ACROPOLIS_ROOF_GARDEN_SOUND_CUE_ENTRY3 = 3 };
    switch (soundCue) {
        case ACROPOLIS_ROOF_GARDEN_SOUND_CUE_ENTRY3:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, ACROPOLIS_ROOF_GARDEN_SOUND_CUE_ENTRY3), 0, 0);
            break;
        case 5:
            break;
        case 9:
            break;
    }
    return 0;
}

/// Selects the pickup dialogue or starts the roof-garden story dialogue.
///
/// `ROOM_MESSAGE_COMMAND` carries the CAP command index as its first integer
/// payload. Command 2 chooses pickup command 5 for saved object states 0/1,
/// otherwise sequence slot 2, variant 0. Command 4 advances dialogue progress
/// to at least 6, pauses actor updates, plays CAP 4 in place and sets objective 7.
/// Other commands do nothing; all paths return zero. Requires live flags and
/// relocated CAP data; the receiver, message ID and second payload are unused.
static s32 _acropolisRoofGardenHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandIndex, s32 unusedArg)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_COMMAND_PICKUP           = 2,
        ACROPOLIS_ROOF_GARDEN_COMMAND_STORY            = 4,
        ACROPOLIS_ROOF_GARDEN_COMMAND_PICKUP_AVAILABLE = 5,
        ACROPOLIS_ROOF_GARDEN_PICKUP_OBJECT_ID         = 0x13,
        ACROPOLIS_ROOF_GARDEN_PICKUP_STATE_UNSET       = 0,
        ACROPOLIS_ROOF_GARDEN_PICKUP_STATE_PRESENT     = 1,
        ACROPOLIS_ROOF_GARDEN_PICKUP_CAP_SLOT          = 2,
        ACROPOLIS_ROOF_GARDEN_PICKUP_CAP_VARIANT       = 0,
        ACROPOLIS_ROOF_GARDEN_DIALOGUE_PROGRESS        = 6,
        ACROPOLIS_ROOF_GARDEN_FOLLOW_UP_UNSET          = 0,
        ACROPOLIS_ROOF_GARDEN_STORY_OBJECTIVE          = 7,
    };
    if (commandIndex == ACROPOLIS_ROOF_GARDEN_COMMAND_PICKUP) {
        // Retain the separate reads of the saved two-bit object state.
        if ((areaGetCurrentObjectState(ACROPOLIS_ROOF_GARDEN_PICKUP_OBJECT_ID) == ACROPOLIS_ROOF_GARDEN_PICKUP_STATE_UNSET) || (areaGetCurrentObjectState(ACROPOLIS_ROOF_GARDEN_PICKUP_OBJECT_ID) == ACROPOLIS_ROOF_GARDEN_PICKUP_STATE_PRESENT)) {
            capRunCommandWithTransition(ACROPOLIS_ROOF_GARDEN_COMMAND_PICKUP_AVAILABLE);
        } else {
            capStartSequenceSlot(ACROPOLIS_ROOF_GARDEN_PICKUP_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, ACROPOLIS_ROOF_GARDEN_PICKUP_CAP_VARIANT);
        }
    }
    if (commandIndex == ACROPOLIS_ROOF_GARDEN_COMMAND_STORY) {
        if (gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) < ACROPOLIS_ROOF_GARDEN_DIALOGUE_PROGRESS) {
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, ACROPOLIS_ROOF_GARDEN_FOLLOW_UP_UNSET);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, ACROPOLIS_ROOF_GARDEN_DIALOGUE_PROGRESS);
        }
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        capRunCommand(ACROPOLIS_ROOF_GARDEN_COMMAND_STORY, CAP_PLAYBACK_IN_PLACE);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACROPOLIS_ROOF_GARDEN_STORY_OBJECTIVE);
    }
    return 0;
}

/// Plays the arrival cutscene's opening sound sequence and retires its task.
///
/// `state` counts callback ticks from zero. Entries 6..10 play at ticks
/// 20, 39, 57, 99 and 114. The last cue clears the script's task handle before
/// teardown; the final counter increment is retained after that call.
static void _acropolisRoofGardenCutsceneOpeningSoundTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_OPENING_CUE6_TICK  = 20,
        ACROPOLIS_ROOF_GARDEN_OPENING_CUE7_TICK  = 39,
        ACROPOLIS_ROOF_GARDEN_OPENING_CUE8_TICK  = 57,
        ACROPOLIS_ROOF_GARDEN_OPENING_CUE9_TICK  = 99,
        ACROPOLIS_ROOF_GARDEN_OPENING_FINAL_TICK = 114,
    };
    switch (task->state) {
        case ACROPOLIS_ROOF_GARDEN_OPENING_CUE6_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 6), 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_OPENING_CUE7_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 7), 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_OPENING_CUE8_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 8), 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_OPENING_CUE9_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 9), 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_OPENING_FINAL_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0A), 0, 0);
            _gAcropolisRoofGardenCutsceneSoundTask = NULL;
            taskKill(task);
            break;
    }
    task->state += 1;
}

/// Plays the arrival cutscene's closing sound sequence and retires its task.
///
/// `state` counts callback ticks from zero. Entries 15/16 play at ticks 76/100.
/// The final cue clears the script's task handle before teardown, retaining
/// the counter increment after that call.
static void _acropolisRoofGardenCutsceneClosingSoundTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_CLOSING_CUE15_TICK = 76,
        ACROPOLIS_ROOF_GARDEN_CLOSING_FINAL_TICK = 100,
    };
    switch (task->state) {
        case ACROPOLIS_ROOF_GARDEN_CLOSING_CUE15_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0F), 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_CLOSING_FINAL_TICK:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x10), 0, 0);
            _gAcropolisRoofGardenCutsceneSoundTask = NULL;
            taskKill(task);
            break;
    }
    task->state += 1;
}

/// Starts or cancels the arrival cutscene's singleton timed-sound task.
///
/// `soundAction` is 0 opening, 1 closing, or 2 cancel; other values do nothing.
/// Starts replace the saved handle even on allocation failure and assume the
/// previous sequence has finished. Cancel kills a live task before clearing
/// its handle. Room sound data must remain loaded until the sequence ends.
static void _acropolisRoofGardenControlCutsceneSound(s32 soundAction)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_OPENING_SOUND_TASK_INDEX = 1,
        ACROPOLIS_ROOF_GARDEN_CLOSING_SOUND_TASK_INDEX = 2,
    };
    switch (soundAction) {
        case ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_OPENING:
            _gAcropolisRoofGardenCutsceneSoundTask = taskSpawnFromTable(_gAcropolisRoofGardenSoundTaskDescriptors, ACROPOLIS_ROOF_GARDEN_OPENING_SOUND_TASK_INDEX, 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_CLOSING:
            _gAcropolisRoofGardenCutsceneSoundTask = taskSpawnFromTable(_gAcropolisRoofGardenSoundTaskDescriptors, ACROPOLIS_ROOF_GARDEN_CLOSING_SOUND_TASK_INDEX, 0, 0);
            break;
        case ACROPOLIS_ROOF_GARDEN_CUTSCENE_SOUND_CANCEL:
            if (_gAcropolisRoofGardenCutsceneSoundTask != NULL) {
                taskKill(_gAcropolisRoofGardenCutsceneSoundTask);
                _gAcropolisRoofGardenCutsceneSoundTask = NULL;
            }
            break;
    }
}

/// Registers the roof-garden room task and starts its ambient-sound controller.
///
/// Called in state 0 with the room resources loaded. Installs the message table
/// and room slot, changes music event 6 to 5 and attempts the ambience spawn.
/// Advances to state 1 even if that spawn fails; it retains no child handle.
static void _acropolisRoofGardenInitRoomTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_ARRIVAL_MUSIC_EVENT = 6,
        ACROPOLIS_ROOF_GARDEN_ROOM_MUSIC_EVENT    = 5,
        ACROPOLIS_ROOF_GARDEN_AMBIENCE_TASK_INDEX = 0,
    };
    task->msgTable = _gAcropolisRoofGardenRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == ACROPOLIS_ROOF_GARDEN_ARRIVAL_MUSIC_EVENT) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_ROOF_GARDEN_ROOM_MUSIC_EVENT;
    }
    taskSpawnFromTable(_gAcropolisRoofGardenSoundTaskDescriptors, ACROPOLIS_ROOF_GARDEN_AMBIENCE_TASK_INDEX, 0, 0);
    task->state += 1;
}

/// Starts the skippable arrival scene once per overlay load when arrival warp is 2.
///
/// Marks the sanctuary blocker cleared and immediately selects its placement
/// variant 3, discarding saved poses. The event latch is set before starting
/// the scene; only stage and area of the temporary location key are consumed.
static void _acropolisRoofGardenTickRoom(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_CUTSCENE_WARP             = 2,
        ACROPOLIS_ROOF_GARDEN_CUTSCENE_NOT_STARTED      = 0,
        ACROPOLIS_ROOF_GARDEN_CUTSCENE_STARTED          = 1,
        ACROPOLIS_ROOF_GARDEN_SANCTUARY_CLEARED_VARIANT = 3,
        ACROPOLIS_ROOF_GARDEN_SANCTUARY_BLOCKER_CLEARED = 1,
    };
    GameLocationKey sanctuary;

    if ((gGameSession->location.loc.warp == ACROPOLIS_ROOF_GARDEN_CUTSCENE_WARP) && (_gAcropolisRoofGardenArrivalSceneStarted == ACROPOLIS_ROOF_GARDEN_CUTSCENE_NOT_STARTED)) {
        _gAcropolisRoofGardenArrivalSceneStarted = ACROPOLIS_ROOF_GARDEN_CUTSCENE_STARTED;
        evsStartScriptWithSkip(_gAcropolisRoofGardenArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisRoofGardenArrivalSkipScript);
        gameFlagSetNibble(GAME_FLAG_SANCTUARY_BLOCKER_CLEARED, ACROPOLIS_ROOF_GARDEN_SANCTUARY_BLOCKER_CLEARED);
        sanctuary.stage = GAME_STAGE_ACROPOLIS;
        sanctuary.area  = GAME_AREA_ACROPOLIS_SANCTUARY;
        areaSetPlacementVariant(&sanctuary, ACROPOLIS_ROOF_GARDEN_SANCTUARY_CLEARED_VARIANT, AREA_VARIANT_RESET_ALWAYS);
    }
}

void acropolisRoofGardenRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gAcropolisRoofGardenRoomTaskStates;
    states.funcs[task->state](task);
}

/// Releases the Maggot/Caterpillar scripted entrance for the roof garden encounter.
static void _acropolisRoofGardenReleaseMaggotCaterpillarEntrance(void)
{
    enum { ACROPOLIS_ROOF_GARDEN_ENTRANCE_RELEASED = 1 };

    gSceneCombatState.maggotCaterpillarEntranceReady = ACROPOLIS_ROOF_GARDEN_ENTRANCE_RELEASED;
}

/// Spawns a one-frame flare at (-4770, -220, -3865) in the emitter's local space.
///
/// Overwrites the emitter work's signed-halfword XYZ move vector. Spawning
/// transforms XYZ into a new coordinate under the view root. Both emitter
/// records are borrowed through that call; the flare never reads its retained
/// offset or emitter pointers. A refused spawn still overwrites the move vector.
/// `packedFlareOptions` bits 0..7 give pulse phase steps per animation frame,
/// 8..15 the radius scale, bit 16 selects green instead of red, bit 28 adds
/// streaks to a diamond, and bit 31 selects a disc instead of a diamond.
static inline void _acropolisRoofGardenEmitFlare(EffectWork* emitterWork, GfxCoord* emitterCoord, s32 packedFlareOptions)
{
    emitterWork->move.vx = -4770;
    emitterWork->move.vy = -220;
    emitterWork->move.vz = -3865;
    effectSpawn(EFFECT_ACROPOLIS_ROOF_GARDEN_FLARE, emitterCoord, packedFlareOptions, &emitterWork->move);
}

void acropolisRoofGardenAmbientEffectsTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_EFFECT_INITIALIZE   = 0,
        ACROPOLIS_ROOF_GARDEN_GLOW_SCALE_SHIFT    = 16,
        ACROPOLIS_ROOF_GARDEN_GLOW_CELL_SHIFT     = 8,
        ACROPOLIS_ROOF_GARDEN_LARGE_GLOW_COUNT    = 2,
        ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_INDEX    = 2,
        ACROPOLIS_ROOF_GARDEN_SMALL_GLOW_FIRST    = 3,
        ACROPOLIS_ROOF_GARDEN_LARGE_GLOW_SCALE    = 512,
        ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_SCALE    = 1024,
        ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_CELL     = 1,
        ACROPOLIS_ROOF_GARDEN_SMALL_GLOW_CELL     = 2,
        ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_VIEWS = (1 << (5 - 1)) | (1 << (6 - 1)),
        ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_STEPS   = 14,
        ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT  = 8,
        ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_SCALE = 6,
        ACROPOLIS_ROOF_GARDEN_FLARE_DISC_SCALE    = 3,
        ACROPOLIS_ROOF_GARDEN_FLARE_DISC_SHAPE    = 0x80000000,
    };
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    lightOffsets;
    s32         lightIndex;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (task->state == ACROPOLIS_ROOF_GARDEN_EFFECT_INITIALIZE) {
        // Place ten persistent glows, keeping one index across both table walks.
        for (lightIndex = 0; lightIndex < ACROPOLIS_ROOF_GARDEN_LARGE_GLOW_COUNT; lightIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, lightIndex + (ACROPOLIS_ROOF_GARDEN_LARGE_GLOW_SCALE << ACROPOLIS_ROOF_GARDEN_GLOW_SCALE_SHIFT), &_gAcropolisRoofGardenLightOffsets[lightIndex]);
        }
        lightOffsets = _gAcropolisRoofGardenLightOffsets;
        effectSpawn(EFFECT_ACROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, (ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_SCALE << ACROPOLIS_ROOF_GARDEN_GLOW_SCALE_SHIFT) | (ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_CELL << ACROPOLIS_ROOF_GARDEN_GLOW_CELL_SHIFT) | ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_INDEX, &lightOffsets[ACROPOLIS_ROOF_GARDEN_UPPER_GLOW_INDEX]);
        for (lightIndex = ACROPOLIS_ROOF_GARDEN_SMALL_GLOW_FIRST; lightIndex < ARRAY_SIZE(_gAcropolisRoofGardenLightOffsets); lightIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, lightIndex + (ACROPOLIS_ROOF_GARDEN_SMALL_GLOW_CELL << ACROPOLIS_ROOF_GARDEN_GLOW_CELL_SHIFT), &lightOffsets[lightIndex]);
        }
        task->state = task->state + 1;
    }
    // Emit one-frame flares only while room effects accept new spawns.
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if ((ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_VIEWS >> (gGameSession->location.loc.view - 1)) & 1) {
            _acropolisRoofGardenEmitFlare(work, coord, (ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_SCALE << ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT) | ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_STEPS);
        }
        if (gGameSession->location.loc.view == 7) {
            _acropolisRoofGardenEmitFlare(work, coord, ACROPOLIS_ROOF_GARDEN_FLARE_DISC_SHAPE | (ACROPOLIS_ROOF_GARDEN_FLARE_DISC_SCALE << ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT) | ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_STEPS);
        }
    }
}

/// Positions a light glow quad as a square around its projected screen centre.
///
/// Borrows a writable `quad` and a separate readable `projection`; only
/// `screenPos` and the nonnegative `halfExtent` need initialization, in pixels.
/// Centre plus/minus half-extent must fit signed 32 bits. Each edge narrows to
/// signed 16 bits without clipping, before its paired corner stores. Vertex
/// indices 0..3 are top-left, top-right, bottom-left and bottom-right.
/// Writes only the eight coordinate halfwords; no GPU packets are queued.
static inline void _acropolisRoofGardenSetLightGlowBounds(POLY_FT4* quad, const RoomGlowSpriteScratch* projection)
{
    s16 left;
    s16 right;
    s16 top;
    s16 bottom;

    left     = projection->screenPos.vx - projection->halfExtent;
    quad->x2 = left;
    quad->x0 = left;
    right    = projection->screenPos.vx + projection->halfExtent;
    quad->x3 = right;
    quad->x1 = right;
    top      = projection->screenPos.vy - projection->halfExtent;
    quad->y1 = top;
    quad->y0 = top;
    bottom   = projection->screenPos.vy + projection->halfExtent;
    quad->y3 = bottom;
    quad->y2 = bottom;
}

void acropolisRoofGardenLightGlowTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_LIGHT_INITIALIZE    = 0,
        ACROPOLIS_ROOF_GARDEN_LIGHT_INDEX_MASK    = 0xF,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_SHIFT    = 8,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_MASK     = 3,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_COUNT    = 3,
        ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_SHIFT   = 16,
        ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_MASK    = 0xFFF,
        ACROPOLIS_ROOF_GARDEN_LIGHT_DEFAULT_SCALE = 640,
        ACROPOLIS_ROOF_GARDEN_LIGHT_FLICKER_STEP  = 16,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_X_STRIDE = 16,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_Y        = 270,
    };
    EffectWork*            work;
    GfxCoord*              coord;
    RoomGlowSpriteScratch* projection;
    POLY_FT4*              quad;
    s32                    greyLevel;
    s32                    flickerLevel;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        if ((_gAcropolisRoofGardenLightViewMasks[task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_LIGHT_INDEX_MASK] >> (gGameSession->location.loc.view - 1)) & 1) {
            actorRenderComposeCoord(coord);
            projection = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
            if (task->state == ACROPOLIS_ROOF_GARDEN_LIGHT_INITIALIZE) {
                // Resting grey of each sheet cell; the room only spawns cells 0..2.
                u8 restingLevels[ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_COUNT] = { 0x40, 0x60, 0x10 };

                // Decode once, retaining only the view-mask index in the spawn argument.
                if (task->spawnArg1.value & (ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_MASK << ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_SHIFT)) {
                    work->scale = (task->spawnArg1.value >> ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_SHIFT) & ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_MASK;
                } else {
                    work->scale = ACROPOLIS_ROOF_GARDEN_LIGHT_DEFAULT_SCALE;
                }
                work->angle           = (task->spawnArg1.value >> ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_SHIFT) & ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_MASK;
                task->spawnArg1.value = task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_LIGHT_INDEX_MASK;
                work->period          = restingLevels[work->angle];
                task->state++;
            }
            // Rejected projections still consume one frame-arena quad.
            projection->worldPos.vx = coord->workm.t[0];
            projection->worldPos.vy = coord->workm.t[1];
            projection->worldPos.vz = coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&projection->worldPos);
            gte_rtps();
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            gte_stsxy(&projection->screenPos);
            gte_stszotz(&projection->otz);
            if (projection->otz >= GLOW_MIN_DEPTH) {
                flickerLevel = ((u8)gDisplayState.animFrame & 1) * ACROPOLIS_ROOF_GARDEN_LIGHT_FLICKER_STEP;
                greyLevel    = (u8)work->period + flickerLevel;
                quad->tpage  = GLOW_FLARE_TEXTURE_PAGE;
                setRGB0(quad, greyLevel, greyLevel, greyLevel);
                setSemiTrans(quad, true);
                quad->clut = getClut(work->angle * ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_X_STRIDE, ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_Y);
                quad->u0   = work->angle * GLOW_FLARE_CELL_STRIDE;
                quad->v0   = 0;
                quad->u1   = work->angle * GLOW_FLARE_CELL_STRIDE + GLOW_FLARE_CELL_LAST_TEXEL;
                quad->v1   = 0;
                quad->u2   = work->angle * GLOW_FLARE_CELL_STRIDE;
                quad->v2   = GLOW_FLARE_CELL_LAST_TEXEL;
                quad->u3   = work->angle * GLOW_FLARE_CELL_STRIDE + GLOW_FLARE_CELL_LAST_TEXEL;
                quad->v3   = GLOW_FLARE_CELL_LAST_TEXEL;

                projection->halfExtent = (work->scale * GLOW_FLARE_CELL_LAST_TEXEL) / projection->otz;
                _acropolisRoofGardenSetLightGlowBounds(quad, projection);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        quad);
            }
            SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
        }
    }
}

/// Initializes a two-segment Gouraud flare streak fading from its centre to black.
///
/// Borrows one word-aligned writable `LINE_G3`, setting its opaque command,
/// packet length and polyline terminator. Vertex 1 receives `centreRed` and
/// `centreGreen` (0..255); blue and both endpoints are zero. The caller supplies all three
/// vertex coordinates, DMA linkage and semitransparency before drawing.
static inline void _acropolisRoofGardenInitFlareStreak(LINE_G3* streak, u8 centreRed, u8 centreGreen)
{
    setLineG3(streak);
    setRGB0(streak, 0, 0, 0);
    setRGB1(streak, centreRed, centreGreen, 0);
    setRGB2(streak, 0, 0, 0);
}

/// Queues a flare packet and prepends its additive draw mode at the same depth.
///
/// Borrows a word-aligned, initialized untextured `POLY_G4` or `LINE_G3` packet
/// to link, and a separate readable `s32` `depthWord` for this call. The depth
/// is projected SZ3 / 4, before `otDepthShift`; the caller rejects values below
/// `GLOW_MIN_DEPTH`. Keep the depth word and display depth shift stable while
/// linking. Unsigned scaling wraps to tag 0..1023 in the current ordering table.
/// Enables packet semitransparency and consumes `sizeof(DR_TPAGE)` bytes of
/// word-aligned frame-arena space without checking capacity. The table must
/// contain the selected tag; both packets must live until GPU completion.
static inline void _acropolisRoofGardenQueueFlare(void* primitive, const s32* depthWord)
{
    enum { ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_BYTE_OFFSET_SHIFT = 2 };

    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                ((((u32)*depthWord << gDisplayState.otDepthShift) >> ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_BYTE_OFFSET_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            primitive);
    // Prepending the draw mode after the primitive makes the GPU apply it first.
    gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, *depthWord);
}

void acropolisRoofGardenFlareTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_RATE_MASK      = 0xFF,
        ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT         = 8,
        ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_MASK          = 0xFF,
        ACROPOLIS_ROOF_GARDEN_FLARE_GREEN_SHIFT          = 16,
        ACROPOLIS_ROOF_GARDEN_FLARE_STREAKS              = 0x10000000,
        ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_FALLING        = 0x80,
        ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_LEVEL_MASK     = 0x7F,
        ACROPOLIS_ROOF_GARDEN_FLARE_OUTER_RADIUS_SHIFT   = 10,
        ACROPOLIS_ROOF_GARDEN_FLARE_INNER_RADIUS_SHIFT   = 7,
        ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_RADIUS_SHIFT = 9,
        ACROPOLIS_ROOF_GARDEN_FLARE_CIRCLE_STEPS         = 16,
        ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_TAG_SHIFT   = 4,
        ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TAG_MASK       = GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt),
    };
    GfxCoord*             coord;
    EffectWork*           work;
    RoomGlowRadiiScratch* projection;
    POLY_G4*              quad;
    LINE_G3*              streak;
    s32                   partIndex;
    s32                   pulsePhase;
    s32                   pulseOrOptions; // Folded pulse level, then packed spawn options.
    s32                   radiusScale;
    s16                   intensity;
    s16                   green;
    s16                   halfIntensity;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    // Draw one projected frame before retiring the counted work and task.
    actorRenderComposeCoord(coord);
    projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
    projection->worldPos.vx = coord->workm.t[0];
    projection->worldPos.vy = coord->workm.t[1];
    projection->worldPos.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPos);
    gte_rtps();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    if (projection->otz >= GLOW_MIN_DEPTH) {
        // Fold the phase into a 0..127 triangle wave, then select red or green.
        pulsePhase  = gDisplayState.animFrame;
        pulsePhase *= task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_RATE_MASK;
        green       = (task->spawnArg1.value >> ACROPOLIS_ROOF_GARDEN_FLARE_GREEN_SHIFT) & 1;
        if (pulsePhase & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_FALLING) {
            pulseOrOptions = ~pulsePhase & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_LEVEL_MASK;
        } else {
            pulseOrOptions = pulsePhase & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_LEVEL_MASK;
        }
        intensity      = pulseOrOptions * 2;
        pulseOrOptions = task->spawnArg1.value;
        if (pulseOrOptions < 0) {
            // Layer two concentric wedge fans and four half-bright rays.
            radiusScale             = (pulseOrOptions >> ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT) & ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_MASK;
            projection->outerRadius = (radiusScale << ACROPOLIS_ROOF_GARDEN_FLARE_OUTER_RADIUS_SHIFT) / projection->otz;
            projection->innerRadius = (radiusScale << ACROPOLIS_ROOF_GARDEN_FLARE_INNER_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < ACROPOLIS_ROOF_GARDEN_FLARE_CIRCLE_STEPS; partIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, (intensity * (green ^ 1)) >> 1, (green * intensity) >> 1);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 5]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 1]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 6]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 2]) >> GLOW_TRIG_SHIFT);
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, intensity * (green ^ 1), green * intensity);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 5]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 1]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 6]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 2]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);
            }
            // The first ray fetches light-view mask 8 (16) through sine index -2.
            halfIntensity = intensity >> 1;
            for (partIndex = 2; partIndex < ACROPOLIS_ROOF_GARDEN_FLARE_CIRCLE_STEPS; partIndex += 8) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, halfIntensity * (green ^ 1), green * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex - 4]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT - 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex]) >> (GLOW_TRIG_SHIFT - 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, halfIntensity * (green ^ 1), green * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 12]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAcropolisRoofGardenFlareSineSamples[partIndex + 8]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);
            }
        } else {
            projection->outerRadius = (((pulseOrOptions >> ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT) & ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_MASK) << ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < 2; partIndex++) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, intensity * (green ^ 1), green * intensity);
                quad->x0 = projection->screenPos.vx - projection->outerRadius;
                quad->x1 = quad->x2 = projection->screenPos.vx;
                quad->x3            = projection->screenPos.vx + projection->outerRadius;
                quad->y0 = quad->y2 = quad->y3 = projection->screenPos.vy;
                quad->y1                       = (projection->screenPos.vy - projection->outerRadius) + projection->outerRadius * (partIndex + partIndex);
                addPrim((&gGpuCurrentOt[((u32)projection->otz << gDisplayState.otDepthShift) >> ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_TAG_SHIFT & ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TAG_MASK]),
                        quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz);
            }
            // Optional streaks cross at the diamond centre.
            if (task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_FLARE_STREAKS) {
                for (partIndex = 0; partIndex < 2; partIndex++) {
                    streak         = gGpuPrimCursor;
                    gGpuPrimCursor = streak + 1;
                    _acropolisRoofGardenInitFlareStreak(streak, intensity * (green ^ 1), green * intensity);
                    streak->x0 = projection->screenPos.vx + projection->outerRadius * (partIndex * 3 - 1);
                    streak->y0 = projection->screenPos.vy - projection->outerRadius * (partIndex + 1);
                    streak->x1 = projection->screenPos.vx;
                    streak->y1 = projection->screenPos.vy;
                    streak->x2 = projection->screenPos.vx - projection->outerRadius * (partIndex * 3 - 1);
                    streak->y2 = projection->screenPos.vy + projection->outerRadius * (partIndex + 1);
                    _acropolisRoofGardenQueueFlare(streak, &projection->otz);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    effectKillTask(work, task);
}

#undef ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD

#include "../../shared/falling_leaves_task.inc.c"

void acropolisRoofGardenLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../shared/falling_leaves_draw.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

void acropolisRoofGardenPickupModelTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_PICKUP_FIRST_VIEW = 5,
        ACROPOLIS_ROOF_GARDEN_PICKUP_VIEW_LIMIT = 8,
        ACROPOLIS_ROOF_GARDEN_PICKUP_HIDDEN     = 2,
    };
    Enemy*     enemy;
    TmdObject* tmd;
    s32        objectState;
    s32        mappedView;

    enemy = task->spawnArg2.pointer;
    tmd   = task->extra.tmd;
    // Only the low byte is the saved object ID; mapped views can differ from logical views.
    objectState = areaGetCurrentObjectState((u8)enemy->placeKey);
    mappedView  = viewGetMappedIndex();
    // Separate view tests preserve the two comparisons in the compiled draw gate.
    if (mappedView >= ACROPOLIS_ROOF_GARDEN_PICKUP_VIEW_LIMIT) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else if (mappedView < ACROPOLIS_ROOF_GARDEN_PICKUP_FIRST_VIEW) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else if (objectState == ACROPOLIS_ROOF_GARDEN_PICKUP_HIDDEN) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
    }
}
