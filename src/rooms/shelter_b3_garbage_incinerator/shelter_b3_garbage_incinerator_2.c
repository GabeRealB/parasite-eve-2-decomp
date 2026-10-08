#include "rooms/shelter_b3_garbage_incinerator.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>
#include <psyq/strings.h>

#include "common.h"

#include "shelter_b3_garbage_incinerator_private.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/cap.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/screen_wave.h"
#include "../../shared/actor_messages.h"
#include "../../shared/incinerator_blaze.h"

extern TaskDesc D_actor_342000_80164FF8[];

/// Lift task state selected by the actor event to begin the carried move.
enum { SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_CARRY_ACTOR = 5 };

/// The carried actor faces half a turn in the view coordinate system.
enum { SHELTER_B3_GARBAGE_INCINERATOR_LIFT_CARRIED_ACTOR_YAW = 0x800 };

/// Player animation-bank selectors and transition times used by the burn scene.
enum {
    SHELTER_B3_GARBAGE_INCINERATOR_PRIMARY_ANIMATION_BANK_BASE   = 1,
    SHELTER_B3_GARBAGE_INCINERATOR_ALTERNATE_ANIMATION_BANK_BASE = 0x22,
    SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_CHAIN_BLEND_FRAMES      = 10,
    SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_SCRIPT_BLEND_FRAMES     = 15,
};

/// Fade-task states requested by the burn scene's event script.
enum {
    SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_FADE_FAST_RED = 2,
    SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_FADE_SLOW_RED = 3,
    SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_FADE_WHITE    = 4,
};

/// Steps of the incinerator lift's second move, the one that carries an
/// actor, held in `_ShelterB3GarbageIncineratorLiftWork::carryState`.
///
/// Reaching the second rest pose advances two steps at once, so the jolt step
/// is never entered and the move reports done on the following frame.
enum {
    SHELTER_B3_GARBAGE_INCINERATOR_LIFT_CARRY_START  = 0, // Not begun; the next update starts the motion sound and records the carried actor's position
    SHELTER_B3_GARBAGE_INCINERATOR_LIFT_CARRY_MOVING = 1, // Lift travelling to its second rest pose, the carried actor keeping its own X and Z
    SHELTER_B3_GARBAGE_INCINERATOR_LIFT_CARRY_JOLT   = 2, // Carried actor pinned to its recorded X and Z, its height shaken about the lift's; skipped
    SHELTER_B3_GARBAGE_INCINERATOR_LIFT_CARRY_DONE   = 3, // Finished; the lift's task can end
};

/// Frames the jolt step shakes the carried actor for.
enum { SHELTER_B3_GARBAGE_INCINERATOR_LIFT_JOLT_FRAMES = 16 };

/// Work block of the incinerator lift, the room task that moves the lift's
/// model between its rest poses.
///
/// The task allocates the block zeroed on its first frame. The block is the
/// storage behind the model's lighting matrices, and holds what the lift's
/// sequence has to remember between frames: the view it waits to see change
/// after its first move, the room its lighting belongs to, and the progress of
/// the second move, which takes the area's placement-0 actor along by placing
/// it at the lift's height every frame.
typedef struct {
    MATRIX lightMtx;        // Storage for the model's light matrix
    MATRIX colorMtx;        // Storage for the model's colour matrix
    Task*  playerTask;      // Player task, recorded at set-up but never read
    Task*  carriedActor;    // Task of the area's placement-0 actor (key: area, stage, index 0), placed at the lift's height during the second move
    s32    carriedStartX;   // Carried actor's X as the second move starts; read only by the skipped jolt step
    s32    carriedStartY;   // Carried actor's Y as the second move starts; never read
    s32    carriedStartZ;   // Carried actor's Z as the second move starts; read only by the skipped jolt step
    byte   unknown_54[0xC]; // Never accessed; role unproven
    u16    carryState;      // Step of the second move (SHELTER_B3_GARBAGE_INCINERATOR_LIFT_CARRY_*)
    u16    joltFrames;      // Frames of the jolt step played, cleared as the lift reaches its second rest pose
    u16    arrivalView;     // Session view slot when the lift reached its first rest pose; the sequence resumes once the session shows another
    s16    litRoom;         // Session room the model's lighting was last rebuilt for (0 before the first rebuild)
} _ShelterB3GarbageIncineratorLiftWork;
STATIC_ASSERT_SIZEOF(_ShelterB3GarbageIncineratorLiftWork, 0x68);

extern ActorTransform D_shelter_b3_garbage_incinerator_80185B58[2];

extern TaskMessageEntry D_shelter_b3_garbage_incinerator_80185B40[3];
extern ActorTransform   D_shelter_b3_garbage_incinerator_80185B88;

/// Main-executable global with no module header yet: the remaining-enemy count.

extern s32 gScreenWaveRamp;

/* Shared in source with actors 342100 (the encounter's fade and spawn) and
   215100 (the caption drawing): their data here. */
extern TaskDesc D_shelter_b3_garbage_incinerator_80185BAC[];

static s16 _gCapCaptionTexturePageX;
static s16 _gCapCaptionTexturePageY;

static s32 _gCapCaptionCaretPulseLevel;
static s32 _gCapCaptionCaretPulseFalling;

// This carrier borrows the caption storage defined by its third translation unit.
#define CAP_CAPTION_COMMAND_REFS CapCaption_Data_8015E650
#define CAP_CAPTION_SELECTED_KEY CapCaption_Data_8015E666
/// Writable pointer lvalue borrowing readable glyph cells from the loaded CAP file.
///
/// Nonnegative text uses low-ten-bit cell indices; title selectors use eight bits.
#define CAP_CAPTION_GLYPH_CELLS CapCaption_Data_8015E654
/// Writable pointer lvalue borrowing the selected CAP sequence, or NULL.
///
/// Text records follow its command header and end at CAP_TEXT_REF_END.
#define CAP_CAPTION_SEQUENCE CapCaption_Data_8015E658
/// Writable s16 left X in biased screen pixels: (320 - widest closed line)/2 - 5.
#define CAP_CAPTION_BLOCK_LEFT_X CapCaption_Data_8015E65C
/// Writable s16 first baseline in screen pixels, derived from the bottom baseline.
#define CAP_CAPTION_FIRST_BASELINE_Y CapCaption_Data_8015E65E
/// Writable s16 final baseline in screen pixels, narrowed from the selector input.
#define CAP_CAPTION_BOTTOM_BASELINE_Y CapCaption_Data_8015E660
/// Writable s16 record slot in the selected sequence; text begins at slot one.
#define CAP_CAPTION_RECORD_INDEX CapCaption_Data_8015E662
/// Writable s16 closed-line block height in pixels.
///
/// Each closed line contributes its tallest glyph height plus the two-pixel gap.
#define CAP_CAPTION_BLOCK_HEIGHT CapCaption_Data_8015E664
/// Writable u16 continuation-triangle left X in draw pixels, with unsigned wrapping.
#define CAP_CAPTION_CARET_LEFT_X CapCaption_Data_8015E668
/// Writable u16 continuation-triangle tip Y in draw pixels, with unsigned wrapping.
#define CAP_CAPTION_CARET_TIP_Y CapCaption_Data_8015E66A
/// Writable u8 delay before the continuation caret, counted in eligible draw calls.
#define CAP_CAPTION_CARET_DRAWS_LEFT (CapCaption_Data_8015E66C[0])
#include "../../shared/cap_captions.h"

static void _capCaptionRunSchedule(Task* task);

/// Progress of the burn scene, held in `_ShelterB3GarbageIncineratorBlazeWork::sceneState`.
enum {
    SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_SCENE_START   = 0, // Not started; the next update installs the clips and the event script
    SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_SCENE_RUNNING = 1, // Event script playing; the scene ends when the session's event state clears
};

/// Work block of the room's blaze controller, the task that burns the player
/// once the scene clock has run out with the player still in the incinerator.
///
/// The controller allocates the block zeroed on its first frame and publishes
/// itself, so the event script's callbacks reach the block through the task.
/// It opens with the head the shared blaze tasks expect of their spawner.
/// `actor_342100` stages the same scene with a larger block that keeps one
/// more task and places the later members differently, so the two stay
/// separate types.
typedef struct {
    BlazeParentWork blaze;           // Head the fade task reaches through the controller: the heat-haze screen wave's ramp context
    Task*           playerTask;      // Player task, the receiver of the scene's animation messages
    Task*           bodyFireTask;    // Task spawning fire on the player's model; its spawn argument is set to 1 to spread the fire over the whole body
    Task*           fadeTask;        // Screen fade task, sent the state of each colour ramp by message
    s16             animationId;     // Bank index of the scene clip last played on the player (ANIMATION_BANK_BASE_SET_COUNT + clip)
    s16             sceneState;      // Burn scene progress (SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_SCENE_START or _RUNNING)
    byte            unknown_3C[0x4]; // Never accessed; role unproven
} _ShelterB3GarbageIncineratorBlazeWork;
STATIC_ASSERT_SIZEOF(_ShelterB3GarbageIncineratorBlazeWork, 0x40);

/// Null-terminated table counted and sent with message 0x3F7 on arming.
extern AnimationSet* D_shelter_b3_garbage_incinerator_80186F78[4];

/// Table indexed by `animationId - ANIMATION_BANK_BASE_SET_COUNT`: each entry
/// is the following clip less that base, and a negative entry means there is
/// none.
extern s16 D_shelter_b3_garbage_incinerator_80186F88[];

/// Event script started with `evsStartScript` on arming.
extern EvsCommand D_shelter_b3_garbage_incinerator_80186FB8[];

/// Effect record handed to `effectSpawnHit`: `coord` is the chosen part of the
/// model and `spawnArgLo` the scale that goes with it.
extern EffectSpawnArg gBlazeFireSpawn;

/// Model parts the effect record is aimed at, as indices into the
/// display object's coordinate array.
extern u16 gBlazePlayerParts[];

static s32 _shelterB3GarbageIncineratorAdvanceBlazeAnimation(Task* task);

/// Main-executable global with no module header yet: the base animation-set
/// id, whose alternate range `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` selects when it is 1.

/// Caption schedule scanned by `_capCaptionRunSchedule`.
static CapCaptionScheduleWindow CapCaption_Data_80154514[];

static TaskDesc CapCaption_Data_801544FC;

static TaskDesc CapCaption_Data_80154508;

static void _shelterB3GarbageIncineratorExitEncounterTask(Task* task);
static void _shelterB3GarbageIncineratorLiftTask(Task* task);
static s32  _shelterB3GarbageIncineratorHandleLiftActorEvent(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);
static void _shelterB3GarbageIncineratorBlazeControllerTask(Task* task);
static s32  _shelterB3GarbageIncineratorHandleBlazeFadeState(Task* task, s32 unusedMessageId, s32 nextState, s32 unusedSecondArg);
static void _shelterB3GarbageIncineratorPlayBlazeClip(s32 clipIndex);
static void _shelterB3GarbageIncineratorSetBlazeFadeState(s32 nextState);
static void _shelterB3GarbageIncineratorStartBlazeHeatHaze(void);
static void _shelterB3GarbageIncineratorSetBodyFirePhase(s32 spreadFire);
static void _shelterB3GarbageIncineratorKillPlayerInBlaze(void);

/// Burn-script commands: create the limited fire, then spread it over the body.
enum {
    SHELTER_B3_GARBAGE_INCINERATOR_BODY_FIRE_START  = 0,
    SHELTER_B3_GARBAGE_INCINERATOR_BODY_FIRE_SPREAD = 1,
};

TaskDesc D_shelter_b3_garbage_incinerator_801855E0 = { { { TASK_BODY_NONE, 192 } }, _shelterB3GarbageIncineratorExitEncounterTask, { .value = 0 } };

static TmdBone _gShelterB3GarbageIncineratorModel081E4Skeleton[1] = {
#include "assets/shelter_b3_garbage_incinerator_model_081E4_skeleton.inc"
};

static u32 _gShelterB3GarbageIncineratorModel081E4PartVerts[1] = {
#include "assets/shelter_b3_garbage_incinerator_model_081E4_partVerts.inc"
};

static SVECTOR _gShelterB3GarbageIncineratorModel081E4Verts[49] = {
#include "assets/shelter_b3_garbage_incinerator_model_081E4_verts.inc"
};

static SVECTOR _gShelterB3GarbageIncineratorModel081E4Normals[1] = {
#include "assets/shelter_b3_garbage_incinerator_model_081E4_normals.inc"
};

static u32 _gShelterB3GarbageIncineratorModel081E4Stream[222] = {
#include "assets/shelter_b3_garbage_incinerator_model_081E4_stream.inc"
};

static TmdSource _gShelterB3GarbageIncineratorModel081E4 = {
    0,
    1872,
    0,
    1,
    _gShelterB3GarbageIncineratorModel081E4PartVerts,
    _gShelterB3GarbageIncineratorModel081E4Verts,
    _gShelterB3GarbageIncineratorModel081E4Normals,
    _gShelterB3GarbageIncineratorModel081E4Skeleton,
    _gShelterB3GarbageIncineratorModel081E4Stream,
};

TaskMessageEntry D_shelter_b3_garbage_incinerator_80185B40[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceInView },
    { ROOM_MESSAGE_ACTOR_EVENT, _shelterB3GarbageIncineratorHandleLiftActorEvent },
};

ActorTransform D_shelter_b3_garbage_incinerator_80185B58[2] = {
    { { 0x36B0, 0, -0x4650, 0 }, { 0, 0, 0, 0 } },
    { { 0x36B0, 2000, -0x4650, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_shelter_b3_garbage_incinerator_80185B88 = { { 0x36B0, 3000, -0x4650, 0 }, { 0, 0, 0, 0 } };

TaskDesc D_shelter_b3_garbage_incinerator_80185BA0 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _shelterB3GarbageIncineratorLiftTask, { .model = &_gShelterB3GarbageIncineratorModel081E4 } };

TaskDesc D_shelter_b3_garbage_incinerator_80185BAC[2] = {
    { { { TASK_BODY_NONE, 192 } }, _screenWaveGridTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

static AnimationPackedPose _gShelterB3GarbageIncineratorAnimation088E4Bank1[6] = {
#include "assets/shelter_b3_garbage_incinerator_animation_088E4_bank1.inc"
};

static AnimationPackedRotation _gShelterB3GarbageIncineratorAnimation088E4Bank4[46] = {
#include "assets/shelter_b3_garbage_incinerator_animation_088E4_bank4.inc"
};

static AnimationRecord _gShelterB3GarbageIncineratorAnimation088E4Records[109] = {
#include "assets/shelter_b3_garbage_incinerator_animation_088E4_records.inc"
};

static u16 _gShelterB3GarbageIncineratorAnimation088E4Indices[20] = {
#include "assets/shelter_b3_garbage_incinerator_animation_088E4_indices.inc"
};

static AnimationSet _gShelterB3GarbageIncineratorAnimation088E4 = {
    _gShelterB3GarbageIncineratorAnimation088E4Records,
    _gShelterB3GarbageIncineratorAnimation088E4Indices,
    { NULL, _gShelterB3GarbageIncineratorAnimation088E4Bank1, NULL, NULL, _gShelterB3GarbageIncineratorAnimation088E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB3GarbageIncineratorAnimation09320Bank1[16] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09320_bank1.inc"
};

static AnimationPackedRotation _gShelterB3GarbageIncineratorAnimation09320Bank4[246] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09320_bank4.inc"
};

static AnimationRecord _gShelterB3GarbageIncineratorAnimation09320Records[341] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09320_records.inc"
};

static u16 _gShelterB3GarbageIncineratorAnimation09320Indices[20] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09320_indices.inc"
};

static AnimationSet _gShelterB3GarbageIncineratorAnimation09320 = {
    _gShelterB3GarbageIncineratorAnimation09320Records,
    _gShelterB3GarbageIncineratorAnimation09320Indices,
    { NULL, _gShelterB3GarbageIncineratorAnimation09320Bank1, NULL, NULL, _gShelterB3GarbageIncineratorAnimation09320Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB3GarbageIncineratorAnimation09988Bank1[14] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09988_bank1.inc"
};

static AnimationPackedRotation _gShelterB3GarbageIncineratorAnimation09988Bank4[148] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09988_bank4.inc"
};

static AnimationRecord _gShelterB3GarbageIncineratorAnimation09988Records[200] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09988_records.inc"
};

static u16 _gShelterB3GarbageIncineratorAnimation09988Indices[20] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09988_indices.inc"
};

static AnimationSet _gShelterB3GarbageIncineratorAnimation09988 = {
    _gShelterB3GarbageIncineratorAnimation09988Records,
    _gShelterB3GarbageIncineratorAnimation09988Indices,
    { NULL, _gShelterB3GarbageIncineratorAnimation09988Bank1, NULL, NULL, _gShelterB3GarbageIncineratorAnimation09988Bank4, NULL, NULL, NULL },
};

TaskMessageEntry gBlazeFadeMessages[1] = {
    { BLAZE_FADE_MESSAGE_SET_STATE, _shelterB3GarbageIncineratorHandleBlazeFadeState },
};

AnimationSet* D_shelter_b3_garbage_incinerator_80186F78[4] = {
    &_gShelterB3GarbageIncineratorAnimation088E4,
    &_gShelterB3GarbageIncineratorAnimation09988,
    &_gShelterB3GarbageIncineratorAnimation09320,
    NULL,
};

s16 D_shelter_b3_garbage_incinerator_80186F88[4] = {
    -1,
    -1,
    -1,
    0,
};

EffectSpawnArg gBlazeFireSpawn = { NULL, 0, 1 };

u16 gBlazePlayerParts[16] = {
    2,
    4,
    6,
    10,
    1,
    3,
    5,
    7,
    8,
    9,
    11,
    12,
    13,
    15,
    16,
    18,
};

EvsCommand D_shelter_b3_garbage_incinerator_80186FB8[17] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorPlayBlazeClip }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorSetBodyFirePhase }, { .value = SHELTER_B3_GARBAGE_INCINERATOR_BODY_FIRE_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorSetBlazeFadeState }, { .value = SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_FADE_FAST_RED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorPlayBlazeClip }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB3GarbageIncineratorStartBlazeHeatHaze }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorPlayBlazeClip }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorSetBodyFirePhase }, { .value = SHELTER_B3_GARBAGE_INCINERATOR_BODY_FIRE_SPREAD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorSetBlazeFadeState }, { .value = SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_FADE_SLOW_RED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorSetBlazeFadeState }, { .value = SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_FADE_WHITE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB3GarbageIncineratorKillPlayerInBlaze }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_shelter_b3_garbage_incinerator_80187150[4] = {
    { { { TASK_BODY_NONE, 192 } }, _shelterB3GarbageIncineratorBlazeControllerTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _blazeFadeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _blazeBodyFireTask, { .value = 0 } },
};

#include "../../shared/cap_captions_settings.inc.c"

static TaskDesc D_shelter_b3_garbage_incinerator_80187184[1] = {
    { { { TASK_BODY_NONE, 32 } }, _capCaptionRunSchedule, { .value = 0 } }
};

#include "../../shared/cap_captions_schedule.inc.c"

WorldCoordRoomLighting gShelterB3GarbageIncineratorRoomLighting[7] = {
    { D_shelter_b3_garbage_incinerator_8018DCF0, NULL },
    { D_shelter_b3_garbage_incinerator_8018DCF0, NULL },
    { D_shelter_b3_garbage_incinerator_8018DCF0, NULL },
    { D_shelter_b3_garbage_incinerator_8018E598, NULL },
    { D_shelter_b3_garbage_incinerator_8018E598, NULL },
    { D_shelter_b3_garbage_incinerator_8018E598, NULL },
    { D_shelter_b3_garbage_incinerator_8018E598, NULL },
};

WorldCollisionRoomResources D_shelter_b3_garbage_incinerator_801872B8[7] = {
    { D_shelter_b3_garbage_incinerator_80188388, D_shelter_b3_garbage_incinerator_8018E5B0, D_shelter_b3_garbage_incinerator_8018F734, D_shelter_b3_garbage_incinerator_8018FAD8 },
    { D_shelter_b3_garbage_incinerator_80188388, D_shelter_b3_garbage_incinerator_8018EC38, D_shelter_b3_garbage_incinerator_8018F734, D_shelter_b3_garbage_incinerator_8018FAD8 },
    { D_shelter_b3_garbage_incinerator_80188388, D_shelter_b3_garbage_incinerator_8018EC38, D_shelter_b3_garbage_incinerator_8018F734, D_shelter_b3_garbage_incinerator_8018FAD8 },
    { D_shelter_b3_garbage_incinerator_80188388, D_shelter_b3_garbage_incinerator_8018E5B0, D_shelter_b3_garbage_incinerator_8018F734, D_shelter_b3_garbage_incinerator_8018FAD8 },
    { D_shelter_b3_garbage_incinerator_80188388, D_shelter_b3_garbage_incinerator_8018EC38, D_shelter_b3_garbage_incinerator_8018F734, D_shelter_b3_garbage_incinerator_8018FAD8 },
    { D_shelter_b3_garbage_incinerator_80188388, D_shelter_b3_garbage_incinerator_8018EC38, D_shelter_b3_garbage_incinerator_8018F734, D_shelter_b3_garbage_incinerator_8018FAD8 },
    { D_shelter_b3_garbage_incinerator_80188388, D_shelter_b3_garbage_incinerator_8018F228, D_shelter_b3_garbage_incinerator_8018F734, D_shelter_b3_garbage_incinerator_8018FAD8 },
};

u8 gShelterB3GarbageIncineratorRoom1And3ViewMap[40] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
};

/// Forty logical-view mappings for incinerator room 2.
///
/// Indexed by logical view minus one; each byte is a one-based camera/image/
/// sprite index in 1..40. The room directory borrows this map while loaded.
static u8 _gShelterB3GarbageIncineratorRoom2ViewMap[40] = {
    1,
    2,
    3,
    4,
    5,
    16,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    6,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
};

/// Forty logical-view mappings for incinerator rooms 4 and 6.
///
/// Indexed by logical view minus one; each byte is a one-based camera/image/
/// sprite index in 1..40. The room directory borrows this map while loaded.
static u8 _gShelterB3GarbageIncineratorRoom4And6ViewMap[40] = {
    1,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    10,
    11,
    12,
    13,
    30,
    31,
    16,
    17,
    18,
    19,
    20,
    21,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    14,
    15,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
};

/// Forty logical-view mappings for incinerator room 5.
///
/// Indexed by logical view minus one; each byte is a one-based camera/image/
/// sprite index in 1..40. The room directory borrows this map while loaded.
static u8 _gShelterB3GarbageIncineratorRoom5ViewMap[40] = {
    1,
    22,
    23,
    24,
    25,
    32,
    27,
    28,
    29,
    10,
    11,
    12,
    13,
    30,
    31,
    16,
    17,
    18,
    19,
    20,
    21,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    14,
    15,
    26,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
};

/// Forty logical-view mappings for incinerator room 7.
///
/// Indexed by logical view minus one; each byte is a one-based camera/image/
/// sprite index in 1..40. The room directory borrows this map while loaded.
static u8 _gShelterB3GarbageIncineratorRoom7ViewMap[40] = {
    1,
    22,
    23,
    24,
    25,
    35,
    36,
    37,
    29,
    10,
    11,
    12,
    13,
    38,
    39,
    16,
    17,
    18,
    19,
    20,
    21,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    14,
    15,
    32,
    33,
    34,
    26,
    27,
    28,
    30,
    31,
    40,
};

u8* gShelterB3GarbageIncineratorViewMaps[7] = {
    gShelterB3GarbageIncineratorRoom1And3ViewMap,
    _gShelterB3GarbageIncineratorRoom2ViewMap,
    gShelterB3GarbageIncineratorRoom1And3ViewMap,
    _gShelterB3GarbageIncineratorRoom4And6ViewMap,
    _gShelterB3GarbageIncineratorRoom5ViewMap,
    _gShelterB3GarbageIncineratorRoom4And6ViewMap,
    _gShelterB3GarbageIncineratorRoom7ViewMap,
};

ViewCount D_shelter_b3_garbage_incinerator_8018740C[7] = { 40, 40, 40, 40, 40, 40, 40 };

DirectionWarpEntry D_shelter_b3_garbage_incinerator_8018741C[3] = {
    { { { .word = 1024 }, 640, 0, -2656 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 640, 0, -2656 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, 0x36D5, 0, -0x6466 }, { 0, 0, 0, 0 }, { { .word = 0 }, 0x36D5, 0, -0x6466 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, 0x54280006, DIRECTION_WARP_SOUND_NONE, 9, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, 522, 0, -714 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 522, 0, -714 }, { 0, 0, 0, 0 }, 0x54280002, 0x54280001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static s16 _shelterB3GarbageIncineratorMoveLiftWithActor(Task* task);

/// Loads and runs the exit encounter after the lift's carried move is complete.
///
/// Starts from state 0 with the room/player and CD queue live. `spawnArg1`
/// becomes the CD request slot (0..7), and `spawnArg2` the borrowed encounter
/// task polled until teardown. Unless the intro is skipped, the player plays
/// equipped-bank clip 1 and waits 31 updates while the actor bundle loads.
/// The Gunblade resets that clip; other weapons blend for ten frames. This
/// body owns no work block and releases itself when the encounter ends.
static void _shelterB3GarbageIncineratorExitEncounterTask(Task* task)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_TASK_LOAD       = 0,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_TASK_WAIT_INTRO = 1,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_TASK_WAIT_LOAD  = 2,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_TASK_WAIT_EVENT = 3,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_INTRO_TICKS     = 31,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_ANIMATION       = 1,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_BLEND_FRAMES    = 10,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_WEAPON_GUNBLADE = 23,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_FILE_GROUP      = 34,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_FILE_HUNDREDS   = 20,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_SOUND           = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 0x10),
    };
    u8                    fileKey[4];
    u8                    loadOptions[4];
    AnimationPlayRequest  request;
    s32                   encounterResult;
    s32                   bankIndex;
    s32                   weaponId;
    AnimationPlayRequest* requestPointer;

    /// Selects the equipped character bank and clip for the exit-intro request.
    ///
    /// Arguments are an AnimationPlayRequest lvalue, an AnimationPlayRequest*
    /// lvalue and two s32 lvalues. They are evaluated repeatedly and must have
    /// no side effects. Reads the current weapon and live-save character and
    /// uses this function's exit clip ID; blend/collision remain the caller's choice.
#define SHELTER_B3_GARBAGE_INCINERATOR_PREPARE_EXIT_INTRO(requestStorage, requestView, weaponIndex, selectedBank) \
    {                                                                                                             \
        (requestView) = &(requestStorage);                                                                        \
        (weaponIndex) = gPlayerStatus.weapon;                                                                     \
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {                                          \
            (selectedBank) = (weaponIndex) + SHELTER_B3_GARBAGE_INCINERATOR_PRIMARY_ANIMATION_BANK_BASE;          \
        } else {                                                                                                  \
            (selectedBank) = (weaponIndex) + SHELTER_B3_GARBAGE_INCINERATOR_ALTERNATE_ANIMATION_BANK_BASE;        \
        }                                                                                                         \
        (requestStorage).source.index = (selectedBank);                                                           \
        (requestView)->animationId    = SHELTER_B3_GARBAGE_INCINERATOR_EXIT_ANIMATION;                            \
    }

    switch (task->state) {
        case SHELTER_B3_GARBAGE_INCINERATOR_EXIT_TASK_LOAD:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                break;
            }
            sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_SWITCH_PRESS, 0, 0);
            sndEvtRequestScriptStart(SHELTER_B3_GARBAGE_INCINERATOR_EXIT_SOUND, 0, 0);
            // Stage-zero file 342000: key bytes 3/2/0 and four load-option bytes.
            fileKey[2]            = SHELTER_B3_GARBAGE_INCINERATOR_EXIT_FILE_GROUP;
            fileKey[3]            = 0;
            fileKey[0]            = 0;
            loadOptions[0]        = SHELTER_B3_GARBAGE_INCINERATOR_EXIT_FILE_HUNDREDS;
            loadOptions[1]        = CD_COMMAND_LOAD_DEFAULT;
            loadOptions[2]        = 0;
            loadOptions[3]        = 0;
            task->spawnArg1.value = (u16)cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadOptions);
            if (gGameSession->skipEventIntro == 0) {
                if (gPlayerStatus.weapon == SHELTER_B3_GARBAGE_INCINERATOR_EXIT_WEAPON_GUNBLADE) {
                    SHELTER_B3_GARBAGE_INCINERATOR_PREPARE_EXIT_INTRO(request, requestPointer, weaponId, bankIndex);
                    request.blend                = ANIMATION_BLEND_RESET;
                    request.blendFrames          = 0;
                    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
                } else {
                    SHELTER_B3_GARBAGE_INCINERATOR_PREPARE_EXIT_INTRO(request, requestPointer, weaponId, bankIndex);
                    requestPointer->blend        = ANIMATION_BLEND_INTERPOLATE;
                    requestPointer->blendFrames  = SHELTER_B3_GARBAGE_INCINERATOR_EXIT_BLEND_FRAMES;
                    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
                }
                task->killCountdown = 0;
                task->state++;
            } else {
                task->state = SHELTER_B3_GARBAGE_INCINERATOR_EXIT_TASK_WAIT_LOAD;
            }
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_EXIT_TASK_WAIT_INTRO:
            if (++task->killCountdown >= SHELTER_B3_GARBAGE_INCINERATOR_EXIT_INTRO_TICKS) {
                task->state++;
            }
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_EXIT_TASK_WAIT_LOAD:
            if (cdCmdIsSlotEmpty(task->spawnArg1.value)) {
                task->spawnArg2.pointer = taskSpawnFromTable(D_actor_342000_80164FF8, 0, 0, 0);
                task->state++;
            }
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_EXIT_TASK_WAIT_EVENT:
            if (taskPollKill(task->spawnArg2.pointer, &encounterResult) != 0) {
                taskKill(task);
            }
            break;
    }
}
#undef SHELTER_B3_GARBAGE_INCINERATOR_PREPARE_EXIT_INTRO

/// Advances the lift's second move while placing the carried actor at its height.
///
/// Requires the initialized lift work and both live models. Y increases by 15
/// view-coordinate units per update and clamps to the second rest pose;
/// skipping the intro in view 0x28 clamps immediately. The carried actor keeps
/// its current X/Z and faces half a turn. Arrival advances straight past the
/// jolt state, so no height shake runs on this path. Returns 0 while moving
/// (including the arrival update), then 1 on the following update.
static s16 _shelterB3GarbageIncineratorMoveLiftWithActor(Task* task)
{
    ActorTransform                        placement;
    _ShelterB3GarbageIncineratorLiftWork* work         = task->work;
    GfxCoord*                             liftCoord    = task->extra.tmd->coords;
    GfxCoord*                             carriedCoord = work->carriedActor->extra.tmd->coords;

    switch (work->carryState) {
        case SHELTER_B3_GARBAGE_INCINERATOR_LIFT_CARRY_START:
            sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_LIFT_MOVE_2, 0, 0);
            work->carriedStartX = carriedCoord->coord.t[0];
            work->carriedStartY = carriedCoord->coord.t[1];
            work->carriedStartZ = carriedCoord->coord.t[2];
            work->carryState++;
            /* fallthrough */
        case SHELTER_B3_GARBAGE_INCINERATOR_LIFT_CARRY_MOVING:
            liftCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            liftCoord->coord.t[1]  += 15;
            if (D_shelter_b3_garbage_incinerator_80185B58[1].pos.vy < liftCoord->coord.t[1] || (gGameSession->location.loc.view == 0x28 && gGameSession->skipEventIntro != 0)) {
                sndEvtRequestScriptStop(SOUND_SHELTER_B3_INCINERATOR_LIFT_MOVE_2, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_LIFT_JOLT, 0, 0);
                liftCoord->coord.t[1] = D_shelter_b3_garbage_incinerator_80185B58[1].pos.vy;
                // Arrival skips the jolt state; preserve both increments.
                work->carryState++;
                work->joltFrames = 0;
                work->carryState++;
            }
            placement.pos.vx = carriedCoord->coord.t[0];
            placement.pos.vy = liftCoord->coord.t[1];
            placement.pos.vz = carriedCoord->coord.t[2];
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_LIFT_CARRY_JOLT:
            // Retained jolt behavior for a state the normal arrival skips.
            placement.pos.vx = work->carriedStartX;
            placement.pos.vy = liftCoord->coord.t[1];
            placement.pos.vz = work->carriedStartZ;
            if (++work->joltFrames >= SHELTER_B3_GARBAGE_INCINERATOR_LIFT_JOLT_FRAMES) {
                work->carryState++;
            } else {
                placement.pos.vy += (gDisplayState.animFrame & 1) ? 10 : -10;
            }
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_LIFT_CARRY_DONE:
            return 1;
    }
    placement.rot.vz = 0;
    placement.rot.vx = 0;
    placement.rot.vy = SHELTER_B3_GARBAGE_INCINERATOR_LIFT_CARRIED_ACTOR_YAW;
    TASK_MESSAGE_DISPATCH_POINTER(work->carriedActor, ACTOR_MESSAGE_PLACE, &placement, 0);
    return 0;
}

/// Selects a room variant in the session and live save and requests its object rebuild.
///
/// `room` is a one-based variant in this area's 1..7 range; the event-room
/// index uses room minus one. Requires both session and live-save storage.
static inline void _shelterB3GarbageIncineratorSetRoom(s16 room)
{
    gGameSession->location.loc.room                            = room;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = room;
    gGameSession->eventRoomIndex                               = room - 1;
    gGameSession->roomObjsDirty                                = 1;
}

/// Lowers the lift in parent-coordinate units and clamps at its first rest pose.
///
/// Requires a live TMD root parented to the view and a positive descentStep
/// in integer coordinate units per task tick. Returns signed-halfword 1 only
/// on crossing below placement 0's rest height, otherwise 0; equality takes
/// another step next tick. Arrival stops the move sound and starts the stop sound.
static inline s16 _shelterB3GarbageIncineratorLowerLift(Task* task, s32 descentStep)
{
    GfxCoord* movingCoord;
    s16       arrived;

    movingCoord               = task->extra.tmd->coords;
    movingCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    movingCoord->coord.t[1]  -= descentStep;
    if (movingCoord->coord.t[1] < D_shelter_b3_garbage_incinerator_80185B58[0].pos.vy) {
        sndEvtRequestScriptStop(SOUND_SHELTER_B3_INCINERATOR_LIFT_MOVE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_LIFT_STOP, 0, 0);
        movingCoord->coord.t[1] = D_shelter_b3_garbage_incinerator_80185B58[0].pos.vy;
        arrived                 = 1;
    } else {
        arrived = 0;
    }
    return arrived;
}

/// Commits lift arrival and enters the state that waits for a logical view change.
///
/// Call in descending state 2 with the lift clamped at placement 0's rest height
/// and its owned work live. Updates the descent phase and collision walls,
/// records the current logical view and advances to state 3 without releasing work.
static inline void _shelterB3GarbageIncineratorFinishLiftDescent(Task* task)
{
    _ShelterB3GarbageIncineratorLiftWork* work;

    work                                  = task->work;
    gGameSession->incineratorDescentPhase = GAME_SESSION_INCINERATOR_DESCENT_LANDED;
    shelterB3GarbageIncineratorSetLiftArrivalCollisionWalls();
    work->arrivalView = gGameSession->location.loc.view;
    task->state++;
}

/// Runs the lift's descent, arrival wait and actor-carrying return move.
///
/// Requires a live TMD body and this area's placement-0 actor. State 0 owns
/// a primary-heap work block and supplies the model's lighting matrices.
/// The saved descent phase selects the starting pose. Room action 1 lowers
/// the lift three parent-coordinate units per tick, then a view change selects
/// room 3 or 6. `ROOM_MESSAGE_ACTOR_EVENT` starts the carried move from the
/// waiting state. Completion applies saved area updates and tears down the
/// task/work. Room changes rebuild the model's lighting at its cached XYZ.
/// Scene, menu, combat and attachment-wheel pauses suspend every phase.
static void _shelterB3GarbageIncineratorLiftTask(Task* task)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_INIT             = 0,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_WAIT_ACTION      = 1,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_DESCEND          = 2,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_WAIT_VIEW        = 3,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_WAIT_ACTOR_EVENT = 4,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_ACTION                = 1,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_DESCENT_STEP          = 3,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_OT_OFFSET             = 31,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_LIGHT_COUNT           = 3,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_SECOND_ROOM_FAMILY    = 4,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_FIRST_MOVING_ROOM     = 2,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_SECOND_MOVING_ROOM    = 5,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_FIRST_ARRIVAL_ROOM    = 3,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_SECOND_ARRIVAL_ROOM   = 6,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_FIRST_ROOM_GROUP      = 0,
        SHELTER_B3_GARBAGE_INCINERATOR_LIFT_SECOND_ROOM_GROUP     = 1,
    };
    VECTOR                                lightingPosition;
    u16                                   triggerControl;
    u8                                    actionId;
    u8                                    actionArgument;
    TmdObject*                            model;
    GfxCoord*                             rootCoord;
    _ShelterB3GarbageIncineratorLiftWork* work;
    s16                                   arrived;
    TmdObject*                            litModel;

    if (gGameSession->sceneUpdatesPaused != 0 || (s8)Gp_StateC08.menuOpen != ATTACHMENT_MENU_CLOSED || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING || Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
        return;
    }
    switch (task->state) {
        case SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_INIT:
            model      = task->extra.tmd;
            rootCoord  = model->coords;
            work       = memCalloc(sizeof(*work), false);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                rootCoord->parent                         = &gGfxViewCoord;
                model->flags                              = 0;
                model->otOffset                           = SHELTER_B3_GARBAGE_INCINERATOR_LIFT_OT_OFFSET;
                work->playerTask                          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                model->colorMtx                           = &work->colorMtx;
                D_shelter_b3_garbage_incinerator_8018FC34 = task;
                model->lightMtx                           = &work->lightMtx;
                task->msgTable                            = D_shelter_b3_garbage_incinerator_80185B40;
                work->carriedActor                        = sceneFindEnemyByPlaceKey(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8))->task;
            }
            if (gGameSession->incineratorExitPhase != GAME_SESSION_INCINERATOR_EXIT_NONE) {
                shelterB3GarbageIncineratorSetExitCollisionWalls();
                taskKill(task);
                return;
            }
            switch (gGameSession->incineratorDescentPhase) {
                case GAME_SESSION_INCINERATOR_DESCENT_WAITING:
                    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &D_shelter_b3_garbage_incinerator_80185B88, 0);
                    shelterB3GarbageIncineratorSetLiftCollisionWalls();
                    task->state = SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_WAIT_ACTION;
                    break;
                case GAME_SESSION_INCINERATOR_DESCENT_MOVING:
                case GAME_SESSION_INCINERATOR_DESCENT_LANDED:
                    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, D_shelter_b3_garbage_incinerator_80185B58, 0);
                    task->state = SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_WAIT_ACTOR_EVENT;
                    break;
                case GAME_SESSION_INCINERATOR_DESCENT_COMPLETE:
                    taskKill(task);
                    return;
            }
            taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_WAIT_ACTION:
            if (worldCollisionReadActionHit(&triggerControl, &actionId, &actionArgument) == 0) {
                break;
            }
            if ((triggerControl & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) != WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
                break;
            }
            if ((s8)actionId == SHELTER_B3_GARBAGE_INCINERATOR_LIFT_ACTION) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_SWITCH_PRESS, 0, 0);
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_LIFT_MOVE, 0, 0);
                if (gGameSession->location.loc.room < SHELTER_B3_GARBAGE_INCINERATOR_LIFT_SECOND_ROOM_FAMILY) {
                    _shelterB3GarbageIncineratorSetRoom(SHELTER_B3_GARBAGE_INCINERATOR_LIFT_FIRST_MOVING_ROOM);
                    gGameSession->eventRoomIndex       = gGameSession->location.loc.room - 1;
                    gGameSession->incineratorRoomGroup = SHELTER_B3_GARBAGE_INCINERATOR_LIFT_FIRST_ROOM_GROUP;
                } else {
                    _shelterB3GarbageIncineratorSetRoom(SHELTER_B3_GARBAGE_INCINERATOR_LIFT_SECOND_MOVING_ROOM);
                    gGameSession->eventRoomIndex       = gGameSession->location.loc.room - 1;
                    gGameSession->incineratorRoomGroup = SHELTER_B3_GARBAGE_INCINERATOR_LIFT_SECOND_ROOM_GROUP;
                }
                shelterB3GarbageIncineratorShowTimedCaption(SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_LIFT_ACTION, SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_DEFAULT_KEY, SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_NOTICE_TICKS);
                gGameSession->incineratorDescentPhase = GAME_SESSION_INCINERATOR_DESCENT_MOVING;
                task->state++;
            }
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_DESCEND:
            arrived = _shelterB3GarbageIncineratorLowerLift(task, SHELTER_B3_GARBAGE_INCINERATOR_LIFT_DESCENT_STEP);
            if (arrived) {
                _shelterB3GarbageIncineratorFinishLiftDescent(task);
            }
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_WAIT_VIEW: {
            _ShelterB3GarbageIncineratorLiftWork* waitingWork = task->work;

            if (waitingWork->arrivalView != gGameSession->location.loc.view) {
                if (gGameSession->location.loc.room < SHELTER_B3_GARBAGE_INCINERATOR_LIFT_SECOND_ROOM_FAMILY) {
                    _shelterB3GarbageIncineratorSetRoom(SHELTER_B3_GARBAGE_INCINERATOR_LIFT_FIRST_ARRIVAL_ROOM);
                } else {
                    _shelterB3GarbageIncineratorSetRoom(SHELTER_B3_GARBAGE_INCINERATOR_LIFT_SECOND_ARRIVAL_ROOM);
                }
                task->state++;
            }
            break;
        }
        case SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_CARRY_ACTOR:
            if (!_shelterB3GarbageIncineratorMoveLiftWithActor(task)) {
                break;
            }
            gGameSession->incineratorDescentPhase = GAME_SESSION_INCINERATOR_DESCENT_COMPLETE;
            areaApplySavedUpdates(D_shelter_b3_garbage_incinerator_8018FB6C);
            taskKill(task);
            return;
    }
    // Waiting for the actor event still permits relighting after a room change.
    work = task->work;
    if (gGameSession->location.loc.room != work->litRoom) {
        litModel            = task->extra.tmd;
        lightingPosition.vx = litModel->coords->workm.t[0];
        lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
        lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(litModel, &lightingPosition, 0, SHELTER_B3_GARBAGE_INCINERATOR_LIFT_LIGHT_COUNT);
        work->litRoom = gGameSession->location.loc.room;
    }
}

#include "../../shared/actor_messages_draw_mode.inc.c"

#include "../../shared/actor_messages_place_in_view.inc.c"

/// Restores the low lift collision walls and starts the actor-carrying move.
///
/// Receives `ROOM_MESSAGE_ACTOR_EVENT` on the live lift task. Both payloads
/// are ignored; the sender must discard the unspecified return value.
static s32 _shelterB3GarbageIncineratorHandleLiftActorEvent(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    shelterB3GarbageIncineratorSetLiftCollisionWalls();
    task->state = SHELTER_B3_GARBAGE_INCINERATOR_LIFT_TASK_CARRY_ACTOR;
    // The binary falls through without defining a return value.
}

#include "../../shared/screen_wave_grid.inc.c"

#include "../../shared/incinerator_blaze_fade.inc.c"

/// Sends a blended, collision-disabled animation to the burn controller's player.
///
/// Borrows live controller work and its player task through synchronous dispatch.
/// `animationId` is a signed bank clip ID; `blendFrames` is a nonnegative frame
/// duration (10 for successors, 15 for scripted clips). Selects the equipped
/// weapon's character bank and remembers the clip before dispatch, so successor
/// polling observes the new ID. The receiver copies the request; no stack
/// address is retained. Re-reads work after any preceding synchronous message.
static inline void _shelterB3GarbageIncineratorPlayBlazeAnimation(Task* controller, s16 animationId, s32 blendFrames)
{
    _ShelterB3GarbageIncineratorBlazeWork* work = controller->work;
    AnimationPlayRequest                   request;
    s32                                    weaponId  = gPlayerStatus.weapon;
    s32                                    bankIndex = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1)
                                                           ? weaponId + SHELTER_B3_GARBAGE_INCINERATOR_PRIMARY_ANIMATION_BANK_BASE
                                                           : weaponId + SHELTER_B3_GARBAGE_INCINERATOR_ALTERNATE_ANIMATION_BANK_BASE;

    request.source.index         = bankIndex;
    work->animationId            = animationId;
    request.animationId          = animationId;
    request.blend                = ANIMATION_BLEND_INTERPOLATE;
    request.blendFrames          = blendFrames;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Polls the burn scene's player animation and starts its configured successor.
///
/// Returns 0 while the current clip is playing, otherwise 1, including when
/// a successor has just been sent. A missing player, a base-bank animation or
/// a negative successor entry needs no playback request. The scene installs
/// three extension clips (indices 0..2), all with no successor in its table.
/// A nonnegative successor uses a ten-frame blend with world collision disabled.
/// The controller work, player and installed clip data must remain live.
static s32 _shelterB3GarbageIncineratorAdvanceBlazeAnimation(Task* task)
{
    _ShelterB3GarbageIncineratorBlazeWork* work = task->work;
    s16                                    nextAnimationId;

    if (work->playerTask == NULL) {
    idleReturn:
        return 1;
    }
    if (taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) != 0) {
        return 0;
    }
    if (work->animationId < ANIMATION_BANK_BASE_SET_COUNT) {
        goto idleReturn;
    }
    if (D_shelter_b3_garbage_incinerator_80186F88[work->animationId - ANIMATION_BANK_BASE_SET_COUNT] < 0) {
        goto idleReturn;
    }
    nextAnimationId = D_shelter_b3_garbage_incinerator_80186F88[work->animationId - ANIMATION_BANK_BASE_SET_COUNT] + ANIMATION_BANK_BASE_SET_COUNT;
    _shelterB3GarbageIncineratorPlayBlazeAnimation(task, nextAnimationId, SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_CHAIN_BLEND_FRAMES);
    goto idleReturn;
}

#include "../../shared/incinerator_blaze_body_fire.inc.c"

/// Copies the terminated scene-clip table into the player's writable extension bank.
///
/// Work and player must remain live through synchronous dispatch; clip pointers
/// and their data remain borrowed afterward. The table ends at NULL; only the
/// low sixteen bits of its scanned count are used as the element index/count.
/// The three playable entries occupy extension slots 0..2; the terminator is
/// excluded from the word copy and the installed bank outlives scene playback.
static inline void _shelterB3GarbageIncineratorInstallBlazeClips(Task* task)
{
    _ShelterB3GarbageIncineratorBlazeWork* clipWork = task->work;
    enum { SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_CLIP_COUNT_MASK = 0xFFFF };
    AnimationBankCopyRequest bankRequest;
    s32                      extensionCount;

    // The terminated table counts playable extension pointers, excluding NULL.
    extensionCount = 0;
    while (D_shelter_b3_garbage_incinerator_80186F78[extensionCount & SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_CLIP_COUNT_MASK] != NULL) {
        extensionCount += 1;
    }
    bankRequest.source.sets = D_shelter_b3_garbage_incinerator_80186F78;
    bankRequest.wordCount   = extensionCount & SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_CLIP_COUNT_MASK;
    TASK_MESSAGE_DISPATCH_POINTER(clipWork->playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &bankRequest, 0);
}

/// Starts the scripted burn scene and polls it through completion.
///
/// Requires initialized controller work, a live player and writable extension
/// animation bank. Installs the three scene clips, holds scripted player
/// control, locks attachments, hides the HUD and spawns the fade with a
/// borrowed controller pointer. The controller and clip data must remain
/// live through the fade. Returns 1 once the running script's event state
/// clears, otherwise advances animation successors and returns 0.
static s16 _shelterB3GarbageIncineratorRunBlazeScene(Task* task)
{
    enum { SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_FADE_TASK = 2 };
    _ShelterB3GarbageIncineratorBlazeWork* work = task->work;

    switch (work->sceneState) {
        case SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_SCENE_START:
            _shelterB3GarbageIncineratorInstallBlazeClips(task);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
            evsStartScript(D_shelter_b3_garbage_incinerator_80186FB8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            work->fadeTask   = taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_FADE_TASK, 0, task);
            work->sceneState = work->sceneState + 1;
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_SCENE_RUNNING:
            if (gGameSession->eventState != 0) {
                break;
            }
            return 1;
    }
    _shelterB3GarbageIncineratorAdvanceBlazeAnimation(task);
    return 0;
}

/// Watches the incinerator clock and runs the player's burn-death scene on expiry.
///
/// State 0 owns a zeroed primary-heap work block, publishes the controller for
/// script callbacks and starts the clock-driven caption task at baseline 208.
/// A nonzero `spawnArg1` suppresses the opening alert. The player must still
/// be alive on expiry; warp exit in logical view 33 suppresses ignition.
/// Scene/menu/combat pauses and a latched directional action suspend updates.
/// After the scene ends the controller remains live in state 3, retaining
/// work borrowed by its visual tasks until ordinary task teardown.
static void _shelterB3GarbageIncineratorBlazeControllerTask(Task* task)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_TASK_INIT       = 0,
        SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_TASK_WAIT_CLOCK = 1,
        SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_TASK_RUN_SCENE  = 2,
        SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_EXIT_VIEW       = 33,
        SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_BASELINE_Y    = 208,
    };
    GameSession*                           session = gGameSession;
    _ShelterB3GarbageIncineratorBlazeWork* work;
    s32                                    shouldBurn;
    PlayerStatus*                          playerStatus;

    if (session->sceneUpdatesPaused != 0 || (s8)Gp_StateC08.menuOpen != ATTACHMENT_MENU_CLOSED || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING || D_80114CF8 != 0) {
        return;
    }
    switch (task->state) {
        case SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_TASK_INIT:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            work       = memMalloc(sizeof(*work), false);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->playerTask                          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_shelter_b3_garbage_incinerator_8018FC3C = task;
            }
            taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80187184, 0, SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_BASELINE_Y, 0);
            if (task->spawnArg1.value == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
            }
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_TASK_WAIT_CLOCK:
            playerStatus = &gPlayerStatus;
            if (session->sceneClock > 0) {
                shouldBurn = 0;
            } else if (playerStatus->hp <= 0) {
                shouldBurn = 0;
            } else if (session->incineratorExitPhase != GAME_SESSION_INCINERATOR_EXIT_WARP || session->location.loc.view != SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_EXIT_VIEW) {
                shouldBurn = 1;
            } else {
                shouldBurn = 0;
            }
            if (!shouldBurn) {
                return;
            }
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_TASK_RUN_SCENE:
            if (_shelterB3GarbageIncineratorRunBlazeScene(task) == 0) {
                return;
            }
            break;
        default:
            return;
    }
    task->state++;
}

/// Sets the blaze fade task's full signed state word from the first payload.
///
/// Receives `BLAZE_FADE_MESSAGE_SET_STATE`; the message ID and second payload
/// are ignored. The event script requests fast red, slow red, then white.
/// Senders must discard the unspecified result.
static s32 _shelterB3GarbageIncineratorHandleBlazeFadeState(Task* task, s32 unusedMessageId, s32 nextState, s32 unusedSecondArg)
{
    task->state = nextState;
    // Senders discard the result; this callback leaves the return word unspecified.
}

/// Plays one of the burn scene's installed extension clips on the player.
///
/// `clipIndex` is 0..2 in event-script order. The active controller, player
/// and installed animation bank must remain live. The bank selector adds 1
/// to the equipped weapon for character 1, otherwise 34. The bank animation
/// ID is narrowed to s16 and remembered for successor polling; playback uses
/// a fifteen-frame blend and disables world collision.
static void _shelterB3GarbageIncineratorPlayBlazeClip(s32 clipIndex)
{
    s16 animationId = clipIndex + ANIMATION_BANK_BASE_SET_COUNT;

    _shelterB3GarbageIncineratorPlayBlazeAnimation(D_shelter_b3_garbage_incinerator_8018FC3C, animationId, SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_SCRIPT_BLEND_FRAMES);
}

/// Requests the next colour ramp on the active burn scene's fade task.
///
/// The controller and initialized fade task must remain live. `nextState`
/// is passed unchanged as a signed integer: the script selects fast red (2),
/// slow red (3), then white (4). No callback result is consumed.
static void _shelterB3GarbageIncineratorSetBlazeFadeState(s32 nextState)
{
    _ShelterB3GarbageIncineratorBlazeWork* work = D_shelter_b3_garbage_incinerator_8018FC3C->work;

    taskMessageDispatch(work->fadeTask, BLAZE_FADE_MESSAGE_SET_STATE, nextState, 0);
}

/// Starts the burn scene's heat-haze mesh with a 600-tick rise to eight-pixel displacement.
///
/// Requires the published controller's zeroed work and no other wave task in
/// this package. The mesh borrows the embedded ramp until its final draw;
/// the blaze fade finishes that ramp once the screen has washed white.
/// The mesh initializes the ramp frame/state, retaining the raw-texture tint.
static void _shelterB3GarbageIncineratorStartBlazeHeatHaze(void)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_HEAT_HAZE_RISE_TICKS    = 600,
        SHELTER_B3_GARBAGE_INCINERATOR_HEAT_HAZE_PEAK_STRENGTH = 256,
    };
    _ShelterB3GarbageIncineratorBlazeWork* work = D_shelter_b3_garbage_incinerator_8018FC3C->work;

    work->blaze.wave.span  = SHELTER_B3_GARBAGE_INCINERATOR_HEAT_HAZE_RISE_TICKS;
    work->blaze.wave.scale = SHELTER_B3_GARBAGE_INCINERATOR_HEAT_HAZE_PEAK_STRENGTH;
    taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80185BAC, 0, 0, &work->blaze.wave);
}

/// Starts limited player-body fire, or spreads the existing fire over the whole body.
///
/// The active controller/work must remain live. Zero starts the blaze sound,
/// cancels room effects, selects enemy-cull zone 16 and spawns the fire task.
/// Any nonzero value requires that task to exist and switches it from the
/// four-part, sixteen-tick emitter to the full-body, eight-tick emitter.
/// The event script orders start before spread; neither task owns the other.
static void _shelterB3GarbageIncineratorSetBodyFirePhase(s32 spreadFire)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_CULL_ZONE = 16,
        SHELTER_B3_GARBAGE_INCINERATOR_BODY_FIRE_TASK  = 3,
    };
    _ShelterB3GarbageIncineratorBlazeWork* work = D_shelter_b3_garbage_incinerator_8018FC3C->work;

    if (spreadFire == SHELTER_B3_GARBAGE_INCINERATOR_BODY_FIRE_START) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_BLAZE, 0, 0);
        roomEffectRequestCancelAll();
        gGameSession->enemyCullZone = SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_CULL_ZONE;
        work->bodyFireTask          = taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, SHELTER_B3_GARBAGE_INCINERATOR_BODY_FIRE_TASK, SHELTER_B3_GARBAGE_INCINERATOR_BODY_FIRE_START, 0);
        return;
    }
    work->bodyFireTask->spawnArg1.value = SHELTER_B3_GARBAGE_INCINERATOR_BODY_FIRE_SPREAD;
}

/// Ends the burn scene by killing the player and preserving the display for restart.
static void _shelterB3GarbageIncineratorKillPlayerInBlaze(void)
{
    gPlayerStatus.hp          = 0;
    gGameSession->restartMode = GAME_SESSION_RESTART_PRESERVE_DISPLAY;
}

#include "../../shared/cap_captions.inc.c"

void shelterB3GarbageIncineratorShowTimedCaption(s16 commandIndex, s16 key, s16 durationTicks)
{
    _capCaptionShowTimed(commandIndex, key, durationTicks);
}

#include "../../shared/cap_captions_resource.inc.c"

void shelterB3GarbageIncineratorSelectCaptionResource(s16 texturePageX, s16 texturePageY, s16 dataResourceIndex)
{
    _capCaptionLoadResource(texturePageX, texturePageY, dataResourceIndex);
}
