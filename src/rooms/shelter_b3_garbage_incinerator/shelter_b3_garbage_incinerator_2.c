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

static s16 CapCaption_Data_801544EC;
static s16 CapCaption_Data_801544EE;

static s32 CapCaption_Data_801545E4;
static s32 CapCaption_Data_801545E8;

void func_shelter_b3_garbage_incinerator_8017F968(void);
#include "../../shared/cap_captions.h"

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

/// Model/animation set installed with `func_800E8614` on arming.
extern EvsCommand D_shelter_b3_garbage_incinerator_80186FB8[];

/// Effect record handed to `func_800FDB18`: `coord` is the chosen part of the
/// model and `spawnArgLo` the scale that goes with it.
extern EffectSpawnArg gBlazeFireSpawn;

/// Model parts the effect record is aimed at, as indices into the
/// display object's coordinate array.
extern u16 gBlazePlayerParts[];

static s32 _shelterB3GarbageIncineratorAdvanceBlazeAnimation(Task* task);

/// Main-executable global with no module header yet: the base animation-set
/// id, whose alternate range `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` selects when it is 1.

/// Caption schedule scanned by `func_shelter_b3_garbage_incinerator_8017FA58`.
static CapCaptionScheduleWindow CapCaption_Data_80154514[];

static TaskDesc CapCaption_Data_801544FC;

static TaskDesc CapCaption_Data_80154508;

void        func_shelter_b3_garbage_incinerator_8017DCD4(Task*);
void        func_shelter_b3_garbage_incinerator_8017E158(Task*);
static s32  _shelterB3GarbageIncineratorHandleLiftActorEvent(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);
void        func_shelter_b3_garbage_incinerator_8017F6D8(Task*);
static s32  _shelterB3GarbageIncineratorHandleBlazeFadeState(Task* task, s32 unusedMessageId, s32 nextState, s32 unusedSecondArg);
static void _shelterB3GarbageIncineratorPlayBlazeClip(s32 clipIndex);
static void _shelterB3GarbageIncineratorSetBlazeFadeState(s32 nextState);
void        func_shelter_b3_garbage_incinerator_8017F968(void);
void        func_shelter_b3_garbage_incinerator_8017F9B4(s32);
static void _shelterB3GarbageIncineratorKillPlayerInBlaze(void);

TaskDesc D_shelter_b3_garbage_incinerator_801855E0 = { { { TASK_BODY_NONE, 192 } }, func_shelter_b3_garbage_incinerator_8017DCD4, { .value = 0 } };

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

TaskDesc D_shelter_b3_garbage_incinerator_80185BA0 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_shelter_b3_garbage_incinerator_8017E158, { .model = &_gShelterB3GarbageIncineratorModel081E4 } };

TaskDesc D_shelter_b3_garbage_incinerator_80185BAC[2] = {
    { { { TASK_BODY_NONE, 192 } }, screenWaveGridTask, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_garbage_incinerator_8017F9B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorSetBlazeFadeState }, { .value = SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_FADE_FAST_RED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorPlayBlazeClip }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_garbage_incinerator_8017F968 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorPlayBlazeClip }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_garbage_incinerator_8017F9B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorSetBlazeFadeState }, { .value = SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_FADE_SLOW_RED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3GarbageIncineratorSetBlazeFadeState }, { .value = SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_FADE_WHITE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB3GarbageIncineratorKillPlayerInBlaze }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_shelter_b3_garbage_incinerator_80187150[4] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b3_garbage_incinerator_8017F6D8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _blazeFadeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, blazeBodyFireTask, { .value = 0 } },
};

#include "../../shared/cap_captions_settings.inc.c"

static void CapCaption_RunSchedule(Task* task);

static TaskDesc D_shelter_b3_garbage_incinerator_80187184[1] = {
    { { { TASK_BODY_NONE, 32 } }, CapCaption_RunSchedule, { .value = 0 } }
};

#include "../../shared/cap_captions_schedule.inc.c"

WorldCoordRoomLighting D_shelter_b3_garbage_incinerator_80187280[7] = {
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

u8 D_shelter_b3_garbage_incinerator_80187328[40] = {
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

u8 D_shelter_b3_garbage_incinerator_80187350[40] = {
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

u8 D_shelter_b3_garbage_incinerator_80187378[40] = {
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

u8 D_shelter_b3_garbage_incinerator_801873A0[40] = {
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

u8 D_shelter_b3_garbage_incinerator_801873C8[40] = {
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

u8* D_shelter_b3_garbage_incinerator_801873F0[7] = {
    D_shelter_b3_garbage_incinerator_80187328,
    D_shelter_b3_garbage_incinerator_80187350,
    D_shelter_b3_garbage_incinerator_80187328,
    D_shelter_b3_garbage_incinerator_80187378,
    D_shelter_b3_garbage_incinerator_801873A0,
    D_shelter_b3_garbage_incinerator_80187378,
    D_shelter_b3_garbage_incinerator_801873C8,
};

ViewCount D_shelter_b3_garbage_incinerator_8018740C[7] = { 40, 40, 40, 40, 40, 40, 40 };

DirectionWarpEntry D_shelter_b3_garbage_incinerator_8018741C[3] = {
    { { { .word = 1024 }, 640, 0, -2656 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 640, 0, -2656 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, 0x36D5, 0, -0x6466 }, { 0, 0, 0, 0 }, { { .word = 0 }, 0x36D5, 0, -0x6466 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, 0x54280006, DIRECTION_WARP_SOUND_NONE, 9, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, 522, 0, -714 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 522, 0, -714 }, { 0, 0, 0, 0 }, 0x54280002, 0x54280001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static s16 _shelterB3GarbageIncineratorMoveLiftWithActor(Task* task);
static s32 func_shelter_b3_garbage_incinerator_8017F588(Task* arg0);

void func_shelter_b3_garbage_incinerator_8017DCD4(Task* arg0)
{
    u8   param1[8];
    u8   param2[8];
    s32  msg[5];
    s32  out;
    s32  v;
    s32  w;
    s32* p;

    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                break;
            }
            sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_SWITCH_PRESS, 0, 0);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 0x10), 0, 0);
            param1[2]             = 0x22;
            param1[3]             = 0;
            param1[0]             = 0;
            param2[0]             = 0x14;
            param2[1]             = 0;
            param2[2]             = 0;
            param2[3]             = 0;
            arg0->spawnArg1.value = (u16)cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            if (gGameSession->skipEventIntro == 0) {
                if (gPlayerStatus.weapon == 0x17) {
                    p = msg;
                    w = gPlayerStatus.weapon;
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                        v = w + 1;
                    } else {
                        v = w + 0x22;
                    }
                    msg[0] = v;
                    p[1]   = 1;
                    msg[2] = 0;
                    msg[3] = 0;
                    msg[4] = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, msg, 0);
                } else {
                    p = msg;
                    w = gPlayerStatus.weapon;
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                        v = w + 1;
                    } else {
                        v = w + 0x22;
                    }
                    msg[0] = v;
                    p[1]   = 1;
                    p[2]   = 1;
                    p[3]   = 10;
                    msg[4] = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, msg, 0);
                }
                arg0->killCountdown = 0;
                arg0->state++;
            } else {
                arg0->state = 2;
            }
            break;
        case 1:
            if (++arg0->killCountdown >= 31) {
                arg0->state++;
            }
            break;
        case 2:
            if (CdCmd_IsSlotEmpty(arg0->spawnArg1.value)) {
                arg0->spawnArg2.pointer = taskSpawnFromTable(D_actor_342000_80164FF8, 0, 0, 0);
                arg0->state++;
            }
            break;
        case 3:
            if (Task_PollKill(arg0->spawnArg2.pointer, &out) != 0) {
                taskKill(arg0);
            }
            break;
    }
}

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

/// Drives the room's moving model through the session's stage for it
/// (`incineratorDescentPhase`, 0 to 3). The first frame sets up the work block and places the
/// model at the pose for the recorded stage. In stage 0 it waits for pending
/// event 5 of kind 1, then sets the session room to 2 (5 when the room was 4 or
/// above); it then moves the model 3 units a frame until it reaches the first
/// rest pose's height, and once the view changes sets the room to 3 (or 6).
/// State 5 runs `_shelterB3GarbageIncineratorMoveLiftWithActor` until it
/// finishes, then applies the room's area records and ends the task. Every
/// frame the model is relit when the session room changes. Nothing runs
/// while any of the four flags tested on entry is set.
void func_shelter_b3_garbage_incinerator_8017E158(Task* task)
{
    VECTOR                                pos;
    u16                                   id;
    s8                                    kind;
    u8                                    arg;
    TmdObject*                            obj;
    GfxCoord*                             coord;
    _ShelterB3GarbageIncineratorLiftWork* work;
    s16                                   landed;
    TmdObject*                            tail;
    GfxCoord*                             lift;
    _ShelterB3GarbageIncineratorLiftWork* done_work;
    s32                                   want;
    s32                                   t;

    if (gGameSession->sceneUpdatesPaused != 0 || (s8)Gp_StateC08.menuOpen != ATTACHMENT_MENU_CLOSED || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING || Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
        return;
    }
    switch (task->state) {
        case 0:
            obj        = task->extra.tmd;
            coord      = obj->coords;
            work       = memCalloc(sizeof(*work), 0);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                coord->parent                             = &gGfxViewCoord;
                obj->flags                                = 0;
                obj->otOffset                             = 0x1F;
                work->playerTask                          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                obj->colorMtx                             = &work->colorMtx;
                D_shelter_b3_garbage_incinerator_8018FC34 = task;
                obj->lightMtx                             = &work->lightMtx;
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
                    TASK_MESSAGE_DISPATCH_POINTER(task, 0x7D4, &D_shelter_b3_garbage_incinerator_80185B88, 0);
                    shelterB3GarbageIncineratorSetLiftCollisionWalls();
                    task->state = 1;
                    break;
                case GAME_SESSION_INCINERATOR_DESCENT_MOVING:
                case GAME_SESSION_INCINERATOR_DESCENT_LANDED:
                    TASK_MESSAGE_DISPATCH_POINTER(task, 0x7D4, D_shelter_b3_garbage_incinerator_80185B58, 0);
                    task->state = 4;
                    break;
                case GAME_SESSION_INCINERATOR_DESCENT_COMPLETE:
                    taskKill(task);
                    return;
            }
            taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            break;
        case 1:
            if (Gp_TakePendingObj4C(&id, (u8*)&kind, &arg) == 0) {
                break;
            }
            t    = id & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC);
            want = WORLD_COLLISION_TRIGGER_ACTION_ROOM;
            if (t != want) {
                break;
            }
            /* MATCHING CARRIER: ends both compare operands here. Otherwise cse
             * carries `t == 5` into the else arm below and stores its 5 from
             * that register, keeping it live across the calls. */
            DEF_REG(t);
            DEF_REG(want);
            if (kind == 1) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_SWITCH_PRESS, 0, 0);
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_LIFT_MOVE, 0, 0);
                if (gGameSession->location.loc.room < 4) {
                    gGameSession->location.loc.room                            = 2;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2;
                    gGameSession->eventRoomIndex                               = 1;
                    gGameSession->roomObjsDirty                                = 1;
                    gGameSession->eventRoomIndex                               = gGameSession->location.loc.room - 1;
                    gGameSession->incineratorRoomGroup                         = 0;
                } else {
                    gGameSession->location.loc.room                            = 5;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 5;
                    gGameSession->eventRoomIndex                               = 4;
                    gGameSession->roomObjsDirty                                = 1;
                    gGameSession->eventRoomIndex                               = gGameSession->location.loc.room - 1;
                    gGameSession->incineratorRoomGroup                         = 1;
                }
                func_shelter_b3_garbage_incinerator_80180FE4(5, 0, 0x3C);
                gGameSession->incineratorDescentPhase = GAME_SESSION_INCINERATOR_DESCENT_MOVING;
                task->state++;
            }
            break;
        case 2:
            lift               = task->extra.tmd->coords;
            lift->composeStamp = GRAPHICS_COORD_DIRTY;
            lift->coord.t[1]  -= 3;
            if (lift->coord.t[1] < D_shelter_b3_garbage_incinerator_80185B58[0].pos.vy) {
                sndEvtRequestScriptStop(SOUND_SHELTER_B3_INCINERATOR_LIFT_MOVE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_LIFT_STOP, 0, 0);
                lift->coord.t[1] = D_shelter_b3_garbage_incinerator_80185B58[0].pos.vy;
                landed           = 1;
            } else {
                landed = 0;
            }
            if (landed) {
                done_work                             = task->work;
                gGameSession->incineratorDescentPhase = GAME_SESSION_INCINERATOR_DESCENT_LANDED;
                shelterB3GarbageIncineratorSetLiftArrivalCollisionWalls();
                done_work->arrivalView = gGameSession->location.loc.view;
                task->state++;
            }
            break;
        case 3: {
            _ShelterB3GarbageIncineratorLiftWork* liftWork = task->work;

            if (liftWork->arrivalView != gGameSession->location.loc.view) {
                if (gGameSession->location.loc.room < 4) {
                    gGameSession->location.loc.room                            = 3;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 3;
                    gGameSession->eventRoomIndex                               = 2;
                    gGameSession->roomObjsDirty                                = 1;
                } else {
                    gGameSession->location.loc.room                            = 6;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 6;
                    gGameSession->eventRoomIndex                               = 5;
                    gGameSession->roomObjsDirty                                = 1;
                }
                task->state++;
            }
            break;
        }
        case 5:
            if (!_shelterB3GarbageIncineratorMoveLiftWithActor(task)) {
                break;
            }
            gGameSession->incineratorDescentPhase = GAME_SESSION_INCINERATOR_DESCENT_COMPLETE;
            Gp_ApplyAreaRecs(D_shelter_b3_garbage_incinerator_8018FB6C);
            taskKill(task);
            return;
    }
    work = task->work;
    if (gGameSession->location.loc.room != work->litRoom) {
        tail   = task->extra.tmd;
        pos.vx = tail->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(tail, &pos, 0, 3);
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
/// Re-reads the controller work after any preceding synchronous message.
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

/// Arms the encounter on state 0: sends `playerTask` message 0x3F7 with the
/// table and its live-entry count, raises `Gp_StateC08.flags` bit 0,
/// installs the model set, hands slot 6 message 0xFA4, starts spawn entry 2
/// with the task itself and steps to state 1. State 1 returns 1 while
/// `gGameSession->eventState` is clear; every other path calls
/// `_shelterB3GarbageIncineratorAdvanceBlazeAnimation` with the task and returns 0.
static s32 func_shelter_b3_garbage_incinerator_8017F588(Task* arg0)
{
    _ShelterB3GarbageIncineratorBlazeWork* work = arg0->work;
    _ShelterB3GarbageIncineratorBlazeWork* msgWork;
    AnimationBankCopyRequest               msg;
    s32                                    n;

    switch (work->sceneState) {
        case SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_SCENE_START:
            msgWork = work;
            n       = 0;
            while (D_shelter_b3_garbage_incinerator_80186F78[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.source.sets = &D_shelter_b3_garbage_incinerator_80186F78[0];
            msg.wordCount   = n & 0xFFFF;
            TASK_MESSAGE_DISPATCH_POINTER(msgWork->playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &msg, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
            func_800E8614(D_shelter_b3_garbage_incinerator_80186FB8, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            work->fadeTask   = taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 2, 0, arg0);
            work->sceneState = work->sceneState + 1;
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_SCENE_RUNNING:
            if (gGameSession->eventState != 0) {
                break;
            }
            return 1;
    }
    _shelterB3GarbageIncineratorAdvanceBlazeAnimation(arg0);
    return 0;
}

/// Does nothing while `gGameSession->sceneUpdatesPaused`, `Gp_StateC08.menuOpen`,
/// `gSceneCombatState.actorControl` or `D_80114CF8` is set. State 0 allocates and clears the work block (killing the task if that
/// fails), records `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` in `playerTask` and the task in
/// `D_shelter_b3_garbage_incinerator_8018FC3C`, spawns the table entry and,
/// with `spawnArg1` zero, queues sound event 0x54280005. State 1 advances once
/// the scene clock has run out while the player is alive, unless
/// `incineratorExitPhase` is 1 in view 0x21. State 2 advances when
/// `func_shelter_b3_garbage_incinerator_8017F588` returns nonzero.
void func_shelter_b3_garbage_incinerator_8017F6D8(Task* arg0)
{
    GameSession*                           session = gGameSession;
    _ShelterB3GarbageIncineratorBlazeWork* work;
    s32                                    ok;
    PlayerStatus*                          ps;

    if (session->sceneUpdatesPaused != 0 || (s8)Gp_StateC08.menuOpen != ATTACHMENT_MENU_CLOSED || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING || D_80114CF8 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            work       = memMalloc(sizeof(*work), false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->playerTask                          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_shelter_b3_garbage_incinerator_8018FC3C = arg0;
            }
            taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80187184, 0, 0xD0, 0);
            if (arg0->spawnArg1.value == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
            }
            break;
        case 1:
            ps = &gPlayerStatus;
            if (session->sceneClock > 0) {
                ok = 0;
            } else if (ps->hp <= 0) {
                ok = 0;
            } else if (session->incineratorExitPhase != GAME_SESSION_INCINERATOR_EXIT_WARP || session->location.loc.view != 0x21) {
                ok = 1;
            } else {
                ok = 0;
            }
            if (!ok) {
                return;
            }
            break;
        case 2:
            if ((s16)func_shelter_b3_garbage_incinerator_8017F588(arg0) == 0) {
                return;
            }
            break;
        default:
            return;
    }
    arg0->state++;
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

/// Seed the spawn entry's two parameters and start the task that consumes
/// them, passing the block itself as `Task::spawnArg2`.
void func_shelter_b3_garbage_incinerator_8017F968(void)
{
    _ShelterB3GarbageIncineratorBlazeWork* work = D_shelter_b3_garbage_incinerator_8018FC3C->work;

    work->blaze.wave.span  = 0x258;
    work->blaze.wave.scale = 0x100;
    taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80185BAC, 0, 0, &work->blaze.wave);
}

void func_shelter_b3_garbage_incinerator_8017F9B4(s32 arg0)
{
    _ShelterB3GarbageIncineratorBlazeWork* work = D_shelter_b3_garbage_incinerator_8018FC3C->work;

    if (arg0 == 0) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_BLAZE, 0, 0);
        Gp_PulseState1C();
        gGameSession->enemyCullZone = 0x10;
        work->bodyFireTask          = taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 3, 0, 0);
        return;
    }
    work->bodyFireTask->spawnArg1.value = 1;
}

/// Ends the burn scene by killing the player and preserving the display for restart.
static void _shelterB3GarbageIncineratorKillPlayerInBlaze(void)
{
    gPlayerStatus.hp          = 0;
    gGameSession->restartMode = GAME_SESSION_RESTART_PRESERVE_DISPLAY;
}

#include "../../shared/cap_captions.inc.c"

void func_shelter_b3_garbage_incinerator_80180FE4(s16 arg0, s16 arg1, s16 arg2)
{
    CapCaption_ShowTimed(arg0, arg1, arg2);
}

#include "../../shared/cap_captions_resource.inc.c"

void func_shelter_b3_garbage_incinerator_8018108C(s16 arg0, s16 arg1, s16 arg2)
{
    CapCaption_LoadResource(arg0, arg1, arg2);
}
