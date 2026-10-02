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

extern TaskDesc D_80164FF8;

/// Work block of the task that moves its model while steering another task.
/// `lightMtx` and `colorMtx` are the model's own light and colour matrices,
/// `target` is the task that receives each frame's pose, `start*` its
/// translation captured on the first frame, `state` the step and `timer` the
/// frames spent jittering once the model has come to rest. `view` is the
/// session view recorded when the model lands, and `room` the session room the
/// model was last placed for.
typedef struct {
    MATRIX lightMtx;
    MATRIX colorMtx;
    Task*  field_40;
    Task*  target;
    s32    startX;
    s32    startY;
    s32    startZ;
    byte   unknown_54[0xC];
    u16    state;
    u16    timer;
    u16    view;
    s16    room;
} _DescentWork;

extern ActorTransform D_shelter_b3_garbage_incinerator_80185B58[2];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*);
        void (*call1)(Task*, s32, ActorTransform*);
        void (*call2)(Task*, s32, s32);
    } handler;
} ShelterB3GarbageIncinerator2ExtendedMessageEntry;
STATIC_ASSERT_SIZEOF(ShelterB3GarbageIncinerator2ExtendedMessageEntry, 8);

extern ShelterB3GarbageIncinerator2ExtendedMessageEntry D_shelter_b3_garbage_incinerator_80185B40[3];
extern ActorTransform                                   D_shelter_b3_garbage_incinerator_80185B88;

/// Main-executable global with no module header yet: the remaining-enemy count.

extern s32 gScreenWaveRamp;

/* Shared in source with actors 342100 (the encounter's fade and spawn) and
   215100 (the caption drawing): their data here. */
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, s32);
    } handler;
} ShelterB3GarbageIncinerator2MessageEntry;
STATIC_ASSERT_SIZEOF(ShelterB3GarbageIncinerator2MessageEntry, 8);

extern ShelterB3GarbageIncinerator2MessageEntry gBlazeFadeMessages[1];
extern TaskDesc                                 D_shelter_b3_garbage_incinerator_80185BAC[];

static s16 CapCaption_Data_801544EC;
static s16 CapCaption_Data_801544EE;

static s32 CapCaption_Data_801545E4;
static s32 CapCaption_Data_801545E8;

void func_shelter_b3_garbage_incinerator_8017F930(s32 arg0);
void func_shelter_b3_garbage_incinerator_8017F968(void);
#include "../../shared/cap_captions.h"

/// Work block of the task in `D_shelter_b3_garbage_incinerator_8018FC3C`.
/// `wave` is the ramp context handed to the screen-wave task, which the fade
/// task ends by raising its ramp state. `field_2C` is the task animation
/// messages are dispatched to, `child` the task spawned from the room's table,
/// `field_34` the task started from spawn entry 2 when the encounter is armed,
/// `field_38` the last animation set selected, and `field_3A` the arming state.
/// actor_342100 carries the same encounter with a larger block that holds
/// one more task and places the child task and the animation fields
/// differently, so the two stay separate types.
typedef struct {
    byte           pad_0[0x20];
    OverlayWaveCtx wave;
    Task*          field_2C;
    Task*          child;
    Task*          field_34;
    s16            field_38;
    s16            field_3A;
    byte           pad_3C[0x4];
} GarbageIncineratorWork;
STATIC_ASSERT_SIZEOF(GarbageIncineratorWork, 0x40);

/// Null-terminated table counted and sent with message 0x3F7 on arming.
extern AnimationSet* D_shelter_b3_garbage_incinerator_80186F78[4];

/// Table indexed by `field_38 - 0x2F`: each entry is the following animation
/// set less 0x2F, and a negative entry means there is none.
extern s16 D_shelter_b3_garbage_incinerator_80186F88[];

/// Model/animation set installed with `func_800E8614` on arming.
extern EvsCommand D_shelter_b3_garbage_incinerator_80186FB8[];

/// Effect record handed to `func_800FDB18`: `coord` is the chosen part of the
/// model and `spawnArgLo` the scale that goes with it.
extern EffectSpawnArg gBlazeFireSpawn;

/// Model parts the effect record is aimed at, as indices into the
/// display object's coordinate array.
extern u16 gBlazePlayerParts[];

static s32 func_shelter_b3_garbage_incinerator_8017F318(Task* arg0);

/// Main-executable global with no module header yet: the base animation-set
/// id, whose alternate range `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` selects when it is 1.

/// Caption schedule scanned by `func_shelter_b3_garbage_incinerator_8017FA58`.
static OverlayCapWindow CapCaption_Data_80154514[];

static TaskDesc CapCaption_Data_801544FC;

static TaskDesc CapCaption_Data_80154508;

void func_shelter_b3_garbage_incinerator_8017DCD4(Task*);
void func_shelter_b3_garbage_incinerator_8017E158(Task*);
void func_shelter_b3_garbage_incinerator_8017E7A4(Task*);
void func_shelter_b3_garbage_incinerator_8017F6D8(Task*);
void func_shelter_b3_garbage_incinerator_8017F8A4(Task*, s32, s32);
void func_shelter_b3_garbage_incinerator_8017F8AC(s32);
void func_shelter_b3_garbage_incinerator_8017F930(s32);
void func_shelter_b3_garbage_incinerator_8017F968(void);
void func_shelter_b3_garbage_incinerator_8017F9B4(s32);
void func_shelter_b3_garbage_incinerator_8017FA3C(void);

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

ShelterB3GarbageIncinerator2ExtendedMessageEntry D_shelter_b3_garbage_incinerator_80185B40[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, { .call2 = actorMsgSetDrawMode } },
    { ACTOR_MESSAGE_PLACE, { .call1 = actorMsgPlaceInView } },
    { ROOM_MESSAGE_ACTOR_EVENT, { .call0 = func_shelter_b3_garbage_incinerator_8017E7A4 } },
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

ShelterB3GarbageIncinerator2MessageEntry gBlazeFadeMessages[1] = {
    { 2011, { .call0 = func_shelter_b3_garbage_incinerator_8017F8A4 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_garbage_incinerator_8017F8AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_garbage_incinerator_8017F9B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_garbage_incinerator_8017F930 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_garbage_incinerator_8017F8AC }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_garbage_incinerator_8017F968 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_garbage_incinerator_8017F8AC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_garbage_incinerator_8017F9B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_garbage_incinerator_8017F930 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_garbage_incinerator_8017F930 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_garbage_incinerator_8017FA3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_shelter_b3_garbage_incinerator_80187150[4] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b3_garbage_incinerator_8017F6D8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, blazeFadeTask, { .value = 0 } },
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

GpRoomObjRec D_shelter_b3_garbage_incinerator_801872B8[7] = {
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

GpViewCountRec D_shelter_b3_garbage_incinerator_8018740C[7] = {
    { { .bytes = { 40, 0 } } },
    { { .bytes = { 40, 0 } } },
    { { .bytes = { 40, 0 } } },
    { { .bytes = { 40, 0 } } },
    { { .bytes = { 40, 0 } } },
    { { .bytes = { 40, 0 } } },
    { { .bytes = { 40, 0 } } },
};

GpWarpRec D_shelter_b3_garbage_incinerator_8018741C[3] = {
    { { .words = { 1024, 640, 0, -2656 } }, { 0, 0, 0, 0 }, { .words = { 1024, 640, 0, -2656 } }, { 0, 0, 0, 0 }, 0, 0, 0, 8, 0, 0 },
    { { .words = { 0, 0x36D5, 0, -0x6466 } }, { 0, 0, 0, 0 }, { .words = { 0, 0x36D5, 0, -0x6466 } }, { 0, 0, 0, 0 }, 0, 0x54280006, 0, 9, 0, 0 },
    { { .words = { 1024, 522, 0, -714 } }, { 0, 0, 0, 0 }, { .words = { 1024, 522, 0, -714 } }, { 0, 0, 0, 0 }, 0x54280002, 0x54280001, 0, 2, 0, 0 },
};

static s16 func_shelter_b3_garbage_incinerator_8017DF24(Task* arg0);
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
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                break;
            }
            SndEvt_EnqueueType6(0x5428000D, 0, 0);
            SndEvt_EnqueueType6(0x54280010, 0, 0);
            param1[2]             = 0x22;
            param1[3]             = 0;
            param1[0]             = 0;
            param2[0]             = 0x14;
            param2[1]             = 0;
            param2[2]             = 0;
            param2[3]             = 0;
            arg0->spawnArg1.value = (u16)CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
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
                arg0->spawnArg2.pointer = Task_SpawnFromTable(&D_80164FF8, 0, 0, 0);
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

/// Raises the Y translation of the task's model by 15 a frame until it passes
/// the second resting pose's (snapping to it at once when the session skips the
/// event intro in view 0x28), then jitters the sent height by 10 for 16 frames. Every frame it sends
/// the target task a pose built from its model's height, and returns 1 once
/// the sequence is over.
static s16 func_shelter_b3_garbage_incinerator_8017DF24(Task* arg0)
{
    ActorTransform msg;
    _DescentWork*  work  = arg0->work;
    GfxCoord*      coord = arg0->extra.tmd->coords;
    GfxCoord*      ref   = work->target->extra.tmd->coords;

    switch (work->state) {
        case 0:
            SndEvt_EnqueueType6(0x5428000E, 0, 0);
            work->startX = ref->coord.t[0];
            work->startY = ref->coord.t[1];
            work->startZ = ref->coord.t[2];
            work->state++;
            /* fallthrough */
        case 1:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  += 15;
            if (D_shelter_b3_garbage_incinerator_80185B58[1].pos.vy < coord->coord.t[1] || (gGameSession->location.loc.view == 0x28 && gGameSession->skipEventIntro != 0)) {
                SndEvt_EnqueueType7(0x5428000E, 1);
                SndEvt_EnqueueType6(0x5428000F, 0, 0);
                coord->coord.t[1] = D_shelter_b3_garbage_incinerator_80185B58[1].pos.vy;
                work->state++;
                work->timer = 0;
                work->state++;
            }
            msg.pos.vx = ref->coord.t[0];
            msg.pos.vy = coord->coord.t[1];
            msg.pos.vz = ref->coord.t[2];
            break;
        case 2:
            msg.pos.vx = work->startX;
            msg.pos.vy = coord->coord.t[1];
            msg.pos.vz = work->startZ;
            if (++work->timer >= 16) {
                work->state++;
            } else {
                msg.pos.vy += (gDisplayState.animFrame & 1) ? 10 : -10;
            }
            break;
        case 3:
            return 1;
    }
    msg.rot.vz = 0;
    msg.rot.vx = 0;
    msg.rot.vy = 0x800;
    TASK_MESSAGE_DISPATCH_POINTER(work->target, 0x7D4, &msg, 0);
    return 0;
}

/// Drives the room's moving model through the session's stage for it
/// (`incineratorDescentPhase`, 0 to 3). The first frame sets up the work block and places the
/// model at the pose for the recorded stage. In stage 0 it waits for pending
/// event 5 of kind 1, then sets the session room to 2 (5 when the room was 4 or
/// above); it then moves the model 3 units a frame until it reaches the first
/// rest pose's height, and once the view changes sets the room to 3 (or 6).
/// State 5 runs `func_shelter_b3_garbage_incinerator_8017DF24` until it
/// finishes, then applies the room's area records and ends the task. Every
/// frame the model is re-placed when the session room changes. Nothing runs
/// while any of the four flags tested on entry is set.
void func_shelter_b3_garbage_incinerator_8017E158(Task* task)
{
    VECTOR        pos;
    u16           id;
    s8            kind;
    u8            arg;
    TmdObject*    obj;
    GfxCoord*     coord;
    _DescentWork* work;
    s16           landed;
    TmdObject*    tail;
    GfxCoord*     lift;
    _DescentWork* done_work;
    s32           want;
    s32           t;

    if (gGameSession->sceneUpdatesPaused != 0 || (s8)Gp_StateC08.field_9 != 0 || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING || Gp_StateC08.field_A == 1) {
        return;
    }
    switch (task->state) {
        case 0:
            obj        = task->extra.tmd;
            coord      = obj->coords;
            work       = memCalloc(0x68, 0);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                coord->parent                             = &gGfxViewCoord;
                obj->flags                                = 0;
                obj->otOffset                             = 0x1F;
                work->field_40                            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                obj->colorMtx                             = &work->colorMtx;
                D_shelter_b3_garbage_incinerator_8018FC34 = task;
                obj->lightMtx                             = &work->lightMtx;
                task->msgTable                            = D_shelter_b3_garbage_incinerator_80185B40;
                work->target                              = Gp_FindWorkById(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8))->field_0;
            }
            if (gGameSession->incineratorExitPhase != GAME_SESSION_INCINERATOR_EXIT_NONE) {
                func_shelter_b3_garbage_incinerator_8018507C();
                goto kill;
            }
            switch (gGameSession->incineratorDescentPhase) {
                case GAME_SESSION_INCINERATOR_DESCENT_WAITING:
                    TASK_MESSAGE_DISPATCH_POINTER(task, 0x7D4, &D_shelter_b3_garbage_incinerator_80185B88, 0);
                    func_shelter_b3_garbage_incinerator_80185220();
                    task->state = 1;
                    break;
                case GAME_SESSION_INCINERATOR_DESCENT_MOVING:
                case GAME_SESSION_INCINERATOR_DESCENT_LANDED:
                    TASK_MESSAGE_DISPATCH_POINTER(task, 0x7D4, D_shelter_b3_garbage_incinerator_80185B58, 0);
                    task->state = 4;
                    break;
                case GAME_SESSION_INCINERATOR_DESCENT_COMPLETE:
                    goto kill;
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
                SndEvt_EnqueueType6(0x5428000D, 0, 0);
                SndEvt_EnqueueType6(0x54280003, 0, 0);
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
                SndEvt_EnqueueType7(0x54280003, 1);
                SndEvt_EnqueueType6(0x54280004, 0, 0);
                lift->coord.t[1] = D_shelter_b3_garbage_incinerator_80185B58[0].pos.vy;
                landed           = 1;
            } else {
                landed = 0;
            }
            if (landed) {
                done_work                             = task->work;
                gGameSession->incineratorDescentPhase = GAME_SESSION_INCINERATOR_DESCENT_LANDED;
                func_shelter_b3_garbage_incinerator_801853C4();
                done_work->view = gGameSession->location.loc.view;
                task->state++;
            }
            break;
        case 3:
            if (((_DescentWork*)task->work)->view != gGameSession->location.loc.view) {
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
        case 5:
            if (!func_shelter_b3_garbage_incinerator_8017DF24(task)) {
                break;
            }
            gGameSession->incineratorDescentPhase = GAME_SESSION_INCINERATOR_DESCENT_COMPLETE;
            Gp_ApplyAreaRecs(D_shelter_b3_garbage_incinerator_8018FB6C);
        kill:
            taskKill(task);
            return;
    }
    work = task->work;
    if (gGameSession->location.loc.room != work->room) {
        tail   = task->extra.tmd;
        pos.vx = tail->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        func_800D7A9C(tail, &pos, 0, 3);
        work->room = gGameSession->location.loc.room;
    }
}

#include "../../shared/actor_messages_draw_mode.inc.c"

#include "../../shared/actor_messages_place_in_view.inc.c"

void func_shelter_b3_garbage_incinerator_8017E7A4(Task* arg0)
{
    func_shelter_b3_garbage_incinerator_80185220();
    arg0->state = 5;
}

#include "../../shared/screen_wave_grid.inc.c"

#include "../../shared/incinerator_blaze_fade.inc.c"

/// Step `field_2C` to the next animation set in the table. Returns 0 when
/// message 0x3ED to it returns nonzero, and 1 otherwise: with no `field_2C`,
/// with `field_38` below 0x2F, or with a negative table entry nothing is sent;
/// else the entry plus 0x2F is recorded in `field_38` and sent with message
/// 0x3E8. The set's block is `gPlayerStatus.weapon + 1` when `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` is 1 and
/// `gPlayerStatus.weapon + 0x22` otherwise.
static s32 func_shelter_b3_garbage_incinerator_8017F318(Task* arg0)
{
    GarbageIncineratorWork* work = (GarbageIncineratorWork*)arg0->work;
    GarbageIncineratorWork* msgWork;
    AnimationPlayRequest    msg;
    s16                     anim;
    s32                     weaponId;
    s32                     setId;

    if (work->field_2C == NULL) {
    ret1:
        return 1;
    }
    if (taskMessageDispatch(work->field_2C, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) != 0) {
        return 0;
    }
    if (work->field_38 < 0x2F) {
        goto ret1;
    }
    if (D_shelter_b3_garbage_incinerator_80186F88[work->field_38 - 0x2F] < 0) {
        goto ret1;
    }
    anim                     = D_shelter_b3_garbage_incinerator_80186F88[work->field_38 - 0x2F] + 0x2F;
    msgWork                  = (GarbageIncineratorWork*)arg0->work;
    weaponId                 = gPlayerStatus.weapon;
    setId                    = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.source.index         = setId;
    msgWork->field_38        = anim;
    msg.animationId          = anim;
    msg.blend                = ANIMATION_BLEND_INTERPOLATE;
    msg.blendFrames          = 0xA;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(msgWork->field_2C, ANIMATION_MESSAGE_PLAY, &msg, 0);
    goto ret1;
}

#include "../../shared/incinerator_blaze_body_fire.inc.c"

/// Arms the encounter on state 0: sends `field_2C` message 0x3F7 with the
/// table and its live-entry count, raises `Gp_StateC08.field_6` bit 0,
/// installs the model set, hands slot 6 message 0xFA4, starts spawn entry 2
/// with the task itself and steps to state 1. State 1 returns 1 while
/// `gGameSession->eventState` is clear; every other path calls
/// `func_shelter_b3_garbage_incinerator_8017F318` with the task and returns 0.
static s32 func_shelter_b3_garbage_incinerator_8017F588(Task* arg0)
{
    GarbageIncineratorWork* work = (GarbageIncineratorWork*)arg0->work;
    GarbageIncineratorWork* msgWork;
    GpCopyArg               msg;
    s32                     n;

    switch (work->field_3A) {
        case 0:
            msgWork = work;
            n       = 0;
            while (D_shelter_b3_garbage_incinerator_80186F78[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.source.sets = &D_shelter_b3_garbage_incinerator_80186F78[0];
            msg.count       = n & 0xFFFF;
            TASK_MESSAGE_DISPATCH_POINTER(msgWork->field_2C, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &msg, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_StateC08.field_6 |= 1;
            func_800E8614(D_shelter_b3_garbage_incinerator_80186FB8, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            work->field_34 = Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 2, 0, arg0);
            work->field_3A = work->field_3A + 1;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                break;
            }
            return 1;
    }
    func_shelter_b3_garbage_incinerator_8017F318(arg0);
    return 0;
}

/// Does nothing while `gGameSession->sceneUpdatesPaused`, `Gp_StateC08.field_9`,
/// `gSceneCombatState.actorControl` or `D_80114CF8` is set. State 0 allocates and clears the work block (killing the task if that
/// fails), records `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` in `field_2C` and the task in
/// `D_shelter_b3_garbage_incinerator_8018FC3C`, spawns the table entry and,
/// with `spawnArg1` zero, queues sound event 0x54280005. State 1 advances once
/// the scene clock has run out while the player is alive, unless
/// `incineratorExitPhase` is 1 in view 0x21. State 2 advances when
/// `func_shelter_b3_garbage_incinerator_8017F588` returns nonzero.
void func_shelter_b3_garbage_incinerator_8017F6D8(Task* arg0)
{
    GameSession*            session = gGameSession;
    GarbageIncineratorWork* work;
    s32                     ok;
    PlayerStatus*           ps;

    if (session->sceneUpdatesPaused != 0 || (s8)Gp_StateC08.field_9 != 0 || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING || D_80114CF8 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            work       = Mem_Malloc(0x40, false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->field_2C                            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_shelter_b3_garbage_incinerator_8018FC3C = arg0;
            }
            Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187184, 0, 0xD0, 0);
            if (arg0->spawnArg1.value == 0) {
                SndEvt_EnqueueType6(0x54280005, 0, 0);
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

void func_shelter_b3_garbage_incinerator_8017F8A4(Task* arg0, s32 arg1, s32 arg2)
{
    arg0->state = arg2;
}

/// Select animation set `arg0 + 0x2F`, record it in the work block, and send
/// it to `field_2C` with message 0x3E8. The set's block is `gPlayerStatus.weapon + 1`
/// when `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` is 1 and `gPlayerStatus.weapon + 0x22` otherwise.
void func_shelter_b3_garbage_incinerator_8017F8AC(s32 arg0)
{
    GarbageIncineratorWork* work;
    AnimationPlayRequest    msg;
    s16                     anim;
    s32                     weaponId;
    s32                     setId;

    work                     = D_shelter_b3_garbage_incinerator_8018FC3C->work;
    anim                     = arg0 + 0x2F;
    weaponId                 = gPlayerStatus.weapon;
    setId                    = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.source.index         = setId;
    work->field_38           = anim;
    msg.animationId          = anim;
    msg.blend                = ANIMATION_BLEND_INTERPOLATE;
    msg.blendFrames          = 0xF;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(work->field_2C, ANIMATION_MESSAGE_PLAY, &msg, 0);
}

void func_shelter_b3_garbage_incinerator_8017F930(s32 arg0)
{
    GarbageIncineratorWork* work = (GarbageIncineratorWork*)D_shelter_b3_garbage_incinerator_8018FC3C->work;

    taskMessageDispatch(work->field_34, 0x7DB, arg0, 0);
}

/// Seed the spawn entry's two parameters and start the task that consumes
/// them, passing the block itself as `Task::spawnArg2`.
void func_shelter_b3_garbage_incinerator_8017F968(void)
{
    GarbageIncineratorWork* work = (GarbageIncineratorWork*)D_shelter_b3_garbage_incinerator_8018FC3C->work;

    work->wave.span  = 0x258;
    work->wave.scale = 0x100;
    Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80185BAC, 0, 0, &work->wave);
}

void func_shelter_b3_garbage_incinerator_8017F9B4(s32 arg0)
{
    GarbageIncineratorWork* work = D_shelter_b3_garbage_incinerator_8018FC3C->work;

    if (arg0 == 0) {
        SndEvt_EnqueueType6(0x54280008, 0, 0);
        Gp_PulseState1C();
        gGameSession->enemyCullZone = 0x10;
        work->child                 = Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 3, 0, 0);
        return;
    }
    work->child->spawnArg1.value = 1;
}

void func_shelter_b3_garbage_incinerator_8017FA3C(void)
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
