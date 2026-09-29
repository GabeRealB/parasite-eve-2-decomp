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
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

extern GpXformArg D_shelter_b3_garbage_incinerator_80185B58[2];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*);
        void (*call1)(Task*, s32, GpXformArg*);
        void (*call2)(Task*, s32, s32);
    } handler;
} ShelterB3GarbageIncinerator2ExtendedMessageEntry;
STATIC_ASSERT_SIZEOF(ShelterB3GarbageIncinerator2ExtendedMessageEntry, 8);

extern ShelterB3GarbageIncinerator2ExtendedMessageEntry D_shelter_b3_garbage_incinerator_80185B40[3];
extern GpXformArg                                       D_shelter_b3_garbage_incinerator_80185B88;

/// Main-executable global with no module header yet: the remaining-enemy count.

extern s32 D_shelter_b3_garbage_incinerator_80185BC4;

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

extern ShelterB3GarbageIncinerator2MessageEntry D_shelter_b3_garbage_incinerator_80186F70[1];
extern TaskDesc                                 D_shelter_b3_garbage_incinerator_80185BAC[];

static s16 CapCaption_Data_801544EC;
static s16 CapCaption_Data_801544EE;

static s32 CapCaption_Data_801545E4;
static s32 CapCaption_Data_801545E8;

void func_shelter_b3_garbage_incinerator_8017F0A8(Task* arg0);
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
extern GpAnimSet* D_shelter_b3_garbage_incinerator_80186F78[4];

/// Table indexed by `field_38 - 0x2F`: each entry is the following animation
/// set less 0x2F, and a negative entry means there is none.
extern s16 D_shelter_b3_garbage_incinerator_80186F88[];

/// Model/animation set installed with `func_800E8614` on arming.
extern GpEvsCmd D_shelter_b3_garbage_incinerator_80186FB8[];

/// Effect record handed to `func_800FDB18`: `coord` is the chosen part of the
/// model and `spawnArgLo` the scale that goes with it.
extern GpEffArg D_shelter_b3_garbage_incinerator_80186F90;

/// Model parts the effect record is aimed at, as indices into the
/// display object's coordinate array.
extern u16 D_shelter_b3_garbage_incinerator_80186F98[];

static s32 func_shelter_b3_garbage_incinerator_8017F318(Task* arg0);

/// Main-executable global with no module header yet: the base animation-set
/// id, whose alternate range `Mc_SaveData[0].state.characterId` selects when it is 1.

/// Caption schedule scanned by `func_shelter_b3_garbage_incinerator_8017FA58`.
static OverlayCapWindow CapCaption_Data_80154514[];

static TaskDesc CapCaption_Data_801544FC;

static TaskDesc CapCaption_Data_80154508;

void func_shelter_b3_garbage_incinerator_8017DCD4(Task*);
void func_shelter_b3_garbage_incinerator_8017E158(Task*);
void func_shelter_b3_garbage_incinerator_8017E690(Task*, s32, s32);
void func_shelter_b3_garbage_incinerator_8017E70C(Task*, s32, GpXformArg*);
void func_shelter_b3_garbage_incinerator_8017E7A4(Task*);
void func_shelter_b3_garbage_incinerator_8017E7D0(Task*);
void func_shelter_b3_garbage_incinerator_8017F0A8(Task*);
void func_shelter_b3_garbage_incinerator_8017F410(Task*);
void func_shelter_b3_garbage_incinerator_8017F6D8(Task*);
void func_shelter_b3_garbage_incinerator_8017F8A4(Task*, s32, s32);
void func_shelter_b3_garbage_incinerator_8017F8AC(s32);
void func_shelter_b3_garbage_incinerator_8017F930(s32);
void func_shelter_b3_garbage_incinerator_8017F968(void);
void func_shelter_b3_garbage_incinerator_8017F9B4(s32);
void func_shelter_b3_garbage_incinerator_8017FA3C(void);

TaskDesc D_shelter_b3_garbage_incinerator_801855E0 = { 0, 192, func_shelter_b3_garbage_incinerator_8017DCD4, { .model = NULL } };

TmdBone D_shelter_b3_garbage_incinerator_801855EC[1] = {
#include "assets/shelter_b3_garbage_incinerator_model_0855C_skeleton.inc"
};

u32 D_shelter_b3_garbage_incinerator_80185610[1] = {
#include "assets/shelter_b3_garbage_incinerator_model_0855C_partVerts.inc"
};

SVECTOR D_shelter_b3_garbage_incinerator_80185614[49] = {
#include "assets/shelter_b3_garbage_incinerator_model_0855C_verts.inc"
};

SVECTOR D_shelter_b3_garbage_incinerator_8018579C[1] = {
#include "assets/shelter_b3_garbage_incinerator_model_0855C_normals.inc"
};

u32 D_shelter_b3_garbage_incinerator_801857A4[222] = {
#include "assets/shelter_b3_garbage_incinerator_model_0855C_stream.inc"
};

TmdSource D_shelter_b3_garbage_incinerator_80185B1C = {
    0,
    1872,
    0,
    1,
    D_shelter_b3_garbage_incinerator_80185610,
    D_shelter_b3_garbage_incinerator_80185614,
    D_shelter_b3_garbage_incinerator_8018579C,
    D_shelter_b3_garbage_incinerator_801855EC,
    D_shelter_b3_garbage_incinerator_801857A4,
};

ShelterB3GarbageIncinerator2ExtendedMessageEntry D_shelter_b3_garbage_incinerator_80185B40[3] = {
    { 2005, { .call2 = func_shelter_b3_garbage_incinerator_8017E690 } },
    { 2004, { .call1 = func_shelter_b3_garbage_incinerator_8017E70C } },
    { 5108, { .call0 = func_shelter_b3_garbage_incinerator_8017E7A4 } },
};

GpXformArg D_shelter_b3_garbage_incinerator_80185B58[2] = {
    { { 0x36B0, 0, -0x4650, 0 }, { 0, 0, 0, 0 } },
    { { 0x36B0, 2000, -0x4650, 0 }, { 0, 0, 0, 0 } },
};

GpXformArg D_shelter_b3_garbage_incinerator_80185B88 = { { 0x36B0, 3000, -0x4650, 0 }, { 0, 0, 0, 0 } };

TaskDesc D_shelter_b3_garbage_incinerator_80185BA0 = { 257, 192, func_shelter_b3_garbage_incinerator_8017E158, { .model = &D_shelter_b3_garbage_incinerator_80185B1C } };

TaskDesc D_shelter_b3_garbage_incinerator_80185BAC[2] = {
    { 0, 192, func_shelter_b3_garbage_incinerator_8017E7D0, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 D_shelter_b3_garbage_incinerator_80185BC4 = 256;

AnimationPackedPose D_shelter_b3_garbage_incinerator_80185BC8[6] = {
#include "assets/shelter_b3_garbage_incinerator_animation_088E4_bank1.inc"
};

AnimationPackedRotation D_shelter_b3_garbage_incinerator_80185C10[46] = {
#include "assets/shelter_b3_garbage_incinerator_animation_088E4_bank4.inc"
};

AnimationRecord D_shelter_b3_garbage_incinerator_80185CC8[109] = {
#include "assets/shelter_b3_garbage_incinerator_animation_088E4_records.inc"
};

u16 D_shelter_b3_garbage_incinerator_80185E7C[20] = {
#include "assets/shelter_b3_garbage_incinerator_animation_088E4_indices.inc"
};

GpAnimSet D_shelter_b3_garbage_incinerator_80185EA4 = {
    D_shelter_b3_garbage_incinerator_80185CC8,
    D_shelter_b3_garbage_incinerator_80185E7C,
    { NULL, D_shelter_b3_garbage_incinerator_80185BC8, NULL, NULL, D_shelter_b3_garbage_incinerator_80185C10, NULL, NULL, NULL },
};

AnimationPackedPose D_shelter_b3_garbage_incinerator_80185ECC[16] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09320_bank1.inc"
};

AnimationPackedRotation D_shelter_b3_garbage_incinerator_80185F8C[246] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09320_bank4.inc"
};

AnimationRecord D_shelter_b3_garbage_incinerator_80186364[341] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09320_records.inc"
};

u16 D_shelter_b3_garbage_incinerator_801868B8[20] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09320_indices.inc"
};

GpAnimSet D_shelter_b3_garbage_incinerator_801868E0 = {
    D_shelter_b3_garbage_incinerator_80186364,
    D_shelter_b3_garbage_incinerator_801868B8,
    { NULL, D_shelter_b3_garbage_incinerator_80185ECC, NULL, NULL, D_shelter_b3_garbage_incinerator_80185F8C, NULL, NULL, NULL },
};

AnimationPackedPose D_shelter_b3_garbage_incinerator_80186908[14] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09988_bank1.inc"
};

AnimationPackedRotation D_shelter_b3_garbage_incinerator_801869B0[148] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09988_bank4.inc"
};

AnimationRecord D_shelter_b3_garbage_incinerator_80186C00[200] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09988_records.inc"
};

u16 D_shelter_b3_garbage_incinerator_80186F20[20] = {
#include "assets/shelter_b3_garbage_incinerator_animation_09988_indices.inc"
};

GpAnimSet D_shelter_b3_garbage_incinerator_80186F48 = {
    D_shelter_b3_garbage_incinerator_80186C00,
    D_shelter_b3_garbage_incinerator_80186F20,
    { NULL, D_shelter_b3_garbage_incinerator_80186908, NULL, NULL, D_shelter_b3_garbage_incinerator_801869B0, NULL, NULL, NULL },
};

ShelterB3GarbageIncinerator2MessageEntry D_shelter_b3_garbage_incinerator_80186F70[1] = {
    { 2011, { .call0 = func_shelter_b3_garbage_incinerator_8017F8A4 } },
};

GpAnimSet* D_shelter_b3_garbage_incinerator_80186F78[4] = {
    &D_shelter_b3_garbage_incinerator_80185EA4,
    &D_shelter_b3_garbage_incinerator_80186F48,
    &D_shelter_b3_garbage_incinerator_801868E0,
    NULL,
};

s16 D_shelter_b3_garbage_incinerator_80186F88[4] = {
    -1,
    -1,
    -1,
    0,
};

GpEffArg D_shelter_b3_garbage_incinerator_80186F90 = { NULL, 0, 1 };

u16 D_shelter_b3_garbage_incinerator_80186F98[16] = {
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

GpEvsCmd D_shelter_b3_garbage_incinerator_80186FB8[17] = {
    { 13, { .callback = func_shelter_b3_garbage_incinerator_8017F8AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_garbage_incinerator_8017F9B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_garbage_incinerator_8017F930 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_garbage_incinerator_8017F8AC }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_garbage_incinerator_8017F968 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_garbage_incinerator_8017F8AC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_garbage_incinerator_8017F9B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_garbage_incinerator_8017F930 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_garbage_incinerator_8017F930 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_garbage_incinerator_8017FA3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_shelter_b3_garbage_incinerator_80187150[4] = {
    { 0, 192, func_shelter_b3_garbage_incinerator_8017F6D8, { .model = NULL } },
    { 0, 192, taskKill, { .model = NULL } },
    { 0, 192, func_shelter_b3_garbage_incinerator_8017F0A8, { .model = NULL } },
    { 0, 192, func_shelter_b3_garbage_incinerator_8017F410, { .model = NULL } },
};

#include "../../shared/cap_captions_settings.inc.c"

static void CapCaption_RunSchedule(Task* task, s32 arg1);

static CapCaptionTaskTable D_shelter_b3_garbage_incinerator_80187184 = {
    .native = { { 0, 32, { .withArg = CapCaption_RunSchedule }, { .value = 0 } } }
};

#include "../../shared/cap_captions_schedule.inc.c"

GpRoomCoordRec D_shelter_b3_garbage_incinerator_80187280[7] = {
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
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != 0) {
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
            arg0->spawnArg1.value = (u16)CdCmd_Enqueue(0x21, param1, param2);
            if ((u8)gGameSession->skipEventIntro == 0) {
                if (Player_Status.weapon == 0x17) {
                    p = msg;
                    w = Player_Status.weapon;
                    if (Mc_SaveData[0].state.characterId == 1) {
                        v = w + 1;
                    } else {
                        v = w + 0x22;
                    }
                    msg[0] = v;
                    p[1]   = 1;
                    msg[2] = 0;
                    msg[3] = 0;
                    msg[4] = 0;
                    Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, msg, 0);
                } else {
                    p = msg;
                    w = Player_Status.weapon;
                    if (Mc_SaveData[0].state.characterId == 1) {
                        v = w + 1;
                    } else {
                        v = w + 0x22;
                    }
                    msg[0] = v;
                    p[1]   = 1;
                    p[2]   = 1;
                    p[3]   = 10;
                    msg[4] = 0;
                    Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, msg, 0);
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
    GpXformArg    msg;
    _DescentWork* work  = arg0->work;
    GfxCoord*     coord = arg0->extra.tmd->coords;
    GfxCoord*     ref   = work->target->extra.tmd->coords;

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
            if (D_shelter_b3_garbage_incinerator_80185B58[1].pos.vy < coord->coord.t[1] || (gGameSession->at4.loc.view == 0x28 && (u8)gGameSession->skipEventIntro != 0)) {
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
    Gp_DispatchMsgPtr(work->target, 0x7D4, &msg, 0);
    return 0;
}

/// Drives the room's moving model through the session's stage for it
/// (`field_132`, 0 to 3). The first frame sets up the work block and places the
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

    if (gGameSession->field_65 != 0 || (s8)Gp_StateC08.field_9 != 0 || Gp_StateF0.field_4 != 0 || Gp_StateC08.field_A == 1) {
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
                Mem_Set(work, 0, 0x68);
                coord->parent                             = &gGfxViewCoord;
                obj->flags                                = 0;
                obj->otOffset                             = 0x1F;
                work->field_40                            = gameGetPtrSlot(3);
                obj->colorMtx                             = &work->colorMtx;
                D_shelter_b3_garbage_incinerator_8018FC34 = task;
                obj->lightMtx                             = &work->lightMtx;
                task->msgTable                            = D_shelter_b3_garbage_incinerator_80185B40;
                work->target                              = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8))->field_0;
            }
            if (gGameSession->field_135 != 0) {
                func_shelter_b3_garbage_incinerator_8018507C();
                goto kill;
            }
            switch (gGameSession->field_132) {
                case 0:
                    Gp_DispatchMsgPtr(task, 0x7D4, &D_shelter_b3_garbage_incinerator_80185B88, 0);
                    func_shelter_b3_garbage_incinerator_80185220();
                    task->state = 1;
                    break;
                case 1:
                case 2:
                    Gp_DispatchMsgPtr(task, 0x7D4, D_shelter_b3_garbage_incinerator_80185B58, 0);
                    task->state = 4;
                    break;
                case 3:
                    goto kill;
            }
            Gp_DispatchMsg(task, 0x7D5, 1, 0);
            break;
        case 1:
            if (Gp_TakePendingObj4C(&id, (u8*)&kind, &arg) == 0) {
                break;
            }
            t    = id & 0x7FFF;
            want = 5;
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
                if (gGameSession->at4.loc.room < 4) {
                    gGameSession->at4.loc.room        = 2;
                    Mc_SaveData[0].state.at4.loc.room = 2;
                    gGameSession->eventRoomIndex      = 1;
                    gGameSession->roomObjsDirty       = 1;
                    gGameSession->eventRoomIndex      = gGameSession->at4.loc.room - 1;
                    gGameSession->field_133           = 0;
                } else {
                    gGameSession->at4.loc.room        = 5;
                    Mc_SaveData[0].state.at4.loc.room = 5;
                    gGameSession->eventRoomIndex      = 4;
                    gGameSession->roomObjsDirty       = 1;
                    gGameSession->eventRoomIndex      = gGameSession->at4.loc.room - 1;
                    gGameSession->field_133           = 1;
                }
                func_shelter_b3_garbage_incinerator_80180FE4(5, 0, 0x3C);
                gGameSession->field_132 = 1;
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
                done_work               = task->work;
                gGameSession->field_132 = 2;
                func_shelter_b3_garbage_incinerator_801853C4();
                done_work->view = gGameSession->at4.loc.view;
                task->state++;
            }
            break;
        case 3:
            if (((_DescentWork*)task->work)->view != gGameSession->at4.loc.view) {
                if (gGameSession->at4.loc.room < 4) {
                    gGameSession->at4.loc.room        = 3;
                    Mc_SaveData[0].state.at4.loc.room = 3;
                    gGameSession->eventRoomIndex      = 2;
                    gGameSession->roomObjsDirty       = 1;
                } else {
                    gGameSession->at4.loc.room        = 6;
                    Mc_SaveData[0].state.at4.loc.room = 6;
                    gGameSession->eventRoomIndex      = 5;
                    gGameSession->roomObjsDirty       = 1;
                }
                task->state++;
            }
            break;
        case 5:
            if (!func_shelter_b3_garbage_incinerator_8017DF24(task)) {
                break;
            }
            gGameSession->field_132 = 3;
            Gp_ApplyAreaRecs(D_shelter_b3_garbage_incinerator_8018FB6C);
        kill:
            taskKill(task);
            return;
    }
    work = task->work;
    if (gGameSession->at4.loc.room != work->room) {
        tail   = task->extra.tmd;
        pos.vx = tail->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        func_800D7A9C(tail, &pos, 0, 3);
        work->room = gGameSession->at4.loc.room;
    }
}

/// Sets how the task's model is treated from `arg2`: 0 hides it and leaves its
/// primitive buffer to be allocated on demand, 1 shows it with the same
/// allocation, 2 hides it and exempts it from that allocation.
void func_shelter_b3_garbage_incinerator_8017E690(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = task->extra.tmd;
    switch (arg2) {
        case 0:
            extra->flags = (extra->flags | 0x80) & 0xFFFB;
            return;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
        case 2:
            extra->flags = extra->flags | 0x84;
            return;
    }
}

/// Places the task's model: re-parents its coordinate to the world frame, takes
/// the three longs of `placement` as the translation and applies the three
/// shorts as yaw, pitch and roll.
void func_shelter_b3_garbage_incinerator_8017E70C(Task* task, s32 arg1, GpXformArg* placement)
{
    GfxCoord* coord;
    MATRIX*   mtx;

    coord             = task->extra.tmd->coords;
    coord->parent     = &gGfxViewCoord;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

void func_shelter_b3_garbage_incinerator_8017E7A4(Task* arg0)
{
    func_shelter_b3_garbage_incinerator_80185220();
    arg0->state = 5;
}

/// Screen-wave effect task, driven by the context passed as its spawn
/// argument. Its first run seeds a random phase offset and speed for each of
/// the 9 column and 30 row waves and builds two 8x30 grids of `POLY_FT4`s that
/// sample the two frame-buffer halves. Every later frame it ramps the
/// context's strength up to its target, or back down once the context asks
/// and then kills itself, advances the waves while a gameplay state flag is
/// clear, and draws the current buffer's grid with each vertex displaced by
/// sine waves scaled by that strength.
void func_shelter_b3_garbage_incinerator_8017E7D0(Task* arg0)
{
    OverlayWaveScratch* scratch;
    OverlayWaveScratch* head;
    OverlayWaveCtx*     ctx;
    OverlayWaveRec*     cols;
    POLY_FT4*           p;
    DR_STP*             stp;
    s32                 i;
    s32                 j;
    s32                 k;
    s32                 rowIndex;
    s32                 rowBack;
    s32                 u0;
    s32                 u1;
    s32                 v0;
    s32                 v1;
    s32                 waveX0;
    s32                 waveY0;
    s32                 waveX1;
    s32                 waveY1;
    s32                 waveX2;
    s32                 waveY2;
    s32                 waveX3;
    s32                 waveY3;
    OverlayWaveRec*     row;
    POLY_FT4(*grid)
    [8];
    s32 tpage0;
    s32 tpage1;

    head                             = SCRATCH_HEAD(OverlayWaveScratch);
    CdCmd_Queue.field_22A            = 2;
    SCRATCH_HEAD(OverlayWaveScratch) = head - 1;
    cols                             = head[-1].cols;
    scratch                          = head - 1;
    switch (arg0->state) {
        case 0:
            for (i = 0; i < 9; i++) {
                D_shelter_b3_garbage_incinerator_8018FC60[i].phase  = 0;
                D_shelter_b3_garbage_incinerator_8018FC60[i].offset = (u32)rand() >> 3;
                D_shelter_b3_garbage_incinerator_8018FC60[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            for (i = 0; i < 30; i++) {
                D_shelter_b3_garbage_incinerator_8018FCB0[i].phase  = 0;
                D_shelter_b3_garbage_incinerator_8018FCB0[i].offset = (u32)rand() >> 3;
                D_shelter_b3_garbage_incinerator_8018FCB0[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            D_shelter_b3_garbage_incinerator_80185BC4        = 0;
            D_shelter_b3_garbage_incinerator_8018FC38        = arg0->spawnArg2.pointer;
            D_shelter_b3_garbage_incinerator_8018FC38->frame = 0;
            D_shelter_b3_garbage_incinerator_8018FC38->state = 0;
            Display_ClampField126(-8);
            for (i = 0; i < 2; i++) {
                tpage0 = getTPage(2, 0, 0, i << 8);
                tpage1 = getTPage(2, 0, 128, i << 8);
                grid   = &D_shelter_b3_garbage_incinerator_8018FDA0[i][1];
                for (j = -1; j < 29; j++) {
                    p = grid[j];
                    for (k = 0; k < 8; p++, k++) {
                        setPolyFT4(p);
                        if (D_shelter_b3_garbage_incinerator_8018FC38->blend == 0) {
                            setShadeTex(p, 1);
                        } else {
                            setShadeTex(p, 0);
                            p->r0 = D_shelter_b3_garbage_incinerator_8018FC38->r;
                            p->g0 = D_shelter_b3_garbage_incinerator_8018FC38->g;
                            p->b0 = D_shelter_b3_garbage_incinerator_8018FC38->b;
                        }
                        u0 = k * 40;
                        u1 = (k + 1) * 40;
                        if (u1 == 320) {
                            u1 = 319;
                        }
                        if (u0 < 128) {
                            p->tpage = tpage0;
                        } else {
                            p->tpage = tpage1;
                            u0      -= 128;
                            u1      -= 128;
                        }
                        v1 = (j + 1) * 8 + i * 16;
                        if (j != -1) {
                            v0 = j * 8 + i * 16;
                        } else {
                            v0 = i * 16 + 8;
                            v1 = i * 16;
                        }
                        p->u0 = u0;
                        p->v0 = v0;
                        p->u1 = u1;
                        p->v1 = v0;
                        do {
                            p->u2 = u0;
                            p->v2 = v1;
                            p->u3 = u1;
                        } while (0);
                        p->v3 = v1;
                    }
                }
            }
            arg0->state++;
            break;
        case 1:
            ctx = D_shelter_b3_garbage_incinerator_8018FC38;
            switch (ctx->state) {
                case 0:
                    if (ctx->frame < ctx->span) {
                        ctx->frame++;
                    }
                    break;
                case 1:
                    if (ctx->frame > 0) {
                        if (Gp_StateF0.field_4 == 0) {
                            ctx->frame--;
                        }
                    } else {
                        ctx->state = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_shelter_b3_garbage_incinerator_80185BC4 = D_shelter_b3_garbage_incinerator_8018FC38->frame * D_shelter_b3_garbage_incinerator_8018FC38->scale / D_shelter_b3_garbage_incinerator_8018FC38->span;
            for (i = 0; i < 9; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_shelter_b3_garbage_incinerator_8018FC60[i].phase += D_shelter_b3_garbage_incinerator_8018FC60[i].speed;
                }
                *(s32*)&cols[i] = *(s32*)&D_shelter_b3_garbage_incinerator_8018FC60[i];
            }
            for (i = 0; i < 30; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_shelter_b3_garbage_incinerator_8018FCB0[i].phase += D_shelter_b3_garbage_incinerator_8018FCB0[i].speed;
                }
                *(s32*)&scratch->rows[i] = *(s32*)&D_shelter_b3_garbage_incinerator_8018FCB0[i];
            }
            rowIndex = -1;
            for (j = -1; j < 29; rowIndex += 2, j++, rowIndex--) {
                rowBack = -rowIndex;
                row     = scratch->rows - rowBack;
                grid    = &D_shelter_b3_garbage_incinerator_8018FDA0[gDisplayState.drawBuffer][1];
                p       = grid[j];
                for (k = 0; k < 8; k++, p++) {
                    if (j != -1) {
                        waveX0 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((j << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x0  = k * 40 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((k << 10) + row->phase + row->offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((j << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 40 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((k + 1) << 10) + row->phase + row->offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        p->x0 = k * 40 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 40 - 160;
                        p->y1 = -112;
                    }
                    {
                        OverlayWaveRec* next = row + 1;
                        waveX2               = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((j + 1) << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x2                = k * 40 + (s16)((waveX2 >> 20) - 160);
                        waveY2               = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((k << 10) + row[1].phase + next->offset) << 3);
                        p->y2                = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3               = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((j + 1) << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x3                = (k + 1) * 40 + (s16)((waveX3 >> 20) - 160);
                        waveY3               = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((k + 1) << 10) + row[1].phase + next->offset) << 3);
                        p->y3                = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    addPrim(&gGpuCurrentOt[3], p);
                }
                SOFT_USE_REG(p);
            }
            break;
    }
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
    SCRATCH_POP(OverlayWaveScratch);
}

/// Fade-to-white driver of the encounter, six states over the eight-byte
/// channel block it allocates into its own `Task::work` and hands the parent
/// work block through `Task::spawnArg2`.
///
/// State 0 allocates the ramp, zeroes the three channels and parks the
/// message record `D_shelter_b3_garbage_incinerator_80186F70` in `Task::msgTable`. States 2 and
/// 3 step `r` -- the first by 0xA up to 0x50, the second by 1 up to
/// 0xFF -- and each hands the state machine back to 1 when it clamps, so the
/// two ramps run back to back. State 4 steps `g` / `b` by 8; once
/// `g` passes 0xFF the display mode is switched, `Fs_ImgBuffers` is
/// filled white, the parent work block's wave ramp is ended, and state 5
/// draws the full-screen white `TILE` + `DR_TPAGE` packed into
/// `gGpuPrimCursor` before returning without the fade call. Every other state
/// -- 1, 6 and up -- only draws the fade.
void func_shelter_b3_garbage_incinerator_8017F0A8(Task* arg0)
{
    OverlayFadeWork*        work;
    OverlayFadeWork*        alloc;
    GarbageIncineratorWork* parent;
    TILE*                   tile;
    DR_TPAGE*               dr;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work           = alloc;
            work->b        = 0;
            work->g        = 0;
            work->r        = 0;
            arg0->msgTable = D_shelter_b3_garbage_incinerator_80186F70;
            arg0->state   += 1;
            break;
        case 2:
            work->r += 0xA;
            if ((s16)work->r >= 0x51) {
                work->r     = 0x50;
                arg0->state = 1;
            }
            break;
        case 3:
            work->r += 1;
            if ((s16)work->r >= 0x100) {
                work->r     = 0xFF;
                arg0->state = 1;
            }
            break;
        case 4:
            work->g += 8;
            work->b += 8;
            if ((s16)work->g >= 0x100) {
                parent             = (GarbageIncineratorWork*)((Task*)arg0->spawnArg2.pointer)->work;
                parent->wave.state = 2;
                Display_SetMode(0xD010);
                Mem_Set(Fs_ImgBuffers, 0xFF, 0x25800);
                work->b     = 0xFF;
                work->g     = 0xFF;
                arg0->state = 5;
            }
            break;
        case 5:
            tile           = (TILE*)gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x60);
            tile->r0 = 0xFF;
            tile->g0 = 0xFF;
            tile->b0 = 0xFF;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(gGpuCurrentOt - 16, tile);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000200;
            addPrim(gGpuCurrentOt - 16, dr);
            return;
    }
    Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->b, 1);
}

/// Step `field_2C` to the next animation set in the table. Returns 0 when
/// message 0x3ED to it returns nonzero, and 1 otherwise: with no `field_2C`,
/// with `field_38` below 0x2F, or with a negative table entry nothing is sent;
/// else the entry plus 0x2F is recorded in `field_38` and sent with message
/// 0x3E8. The set's block is `Player_Status.weapon + 1` when `Mc_SaveData[0].state.characterId` is 1 and
/// `Player_Status.weapon + 0x22` otherwise.
static s32 func_shelter_b3_garbage_incinerator_8017F318(Task* arg0)
{
    GarbageIncineratorWork* work = (GarbageIncineratorWork*)arg0->work;
    GarbageIncineratorWork* msgWork;
    GpAnimArg               msg;
    s16                     anim;
    s32                     weaponId;
    s32                     setId;

    if (work->field_2C == NULL) {
    ret1:
        return 1;
    }
    if (Gp_DispatchMsg(work->field_2C, 0x3ED, 0, 0) != 0) {
        return 0;
    }
    if (work->field_38 < 0x2F) {
        goto ret1;
    }
    if (D_shelter_b3_garbage_incinerator_80186F88[work->field_38 - 0x2F] < 0) {
        goto ret1;
    }
    anim                = D_shelter_b3_garbage_incinerator_80186F88[work->field_38 - 0x2F] + 0x2F;
    msgWork             = (GarbageIncineratorWork*)arg0->work;
    weaponId            = Player_Status.weapon;
    setId               = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.animBlock.index = setId;
    msgWork->field_38   = anim;
    msg.field_4         = anim;
    msg.field_8         = 1;
    msg.field_C         = 0xA;
    msg.field_10        = 0;
    Gp_DispatchMsgPtr(msgWork->field_2C, 0x3E8, &msg, 0);
    goto ret1;
}

/// Each tick rolls the LCG and aims the effect record at one part of the
/// model owned by `gameGetPtrSlot(3)`. State 0 fires with one of the first
/// four parts at scale 0x100 and steps to state 1. State 1 fires only on some
/// frames: with `spawnArg1` zero, one of the first four parts at scale 0x10
/// every sixteenth frame; otherwise one of the first sixteen at scale 0x100
/// every eighth frame.
void func_shelter_b3_garbage_incinerator_8017F410(Task* arg0)
{
    Task* slot;
    s32   idx;

    slot        = gameGetPtrSlot(3);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    idx         = Gp_LcgState >> 16;

    switch (arg0->state) {
        case 0:
            idx                                                 &= 3;
            D_shelter_b3_garbage_incinerator_80186F90.spawnArgLo = 0x100;
            D_shelter_b3_garbage_incinerator_80186F90.coord      = &slot->extra.tmd->coords[D_shelter_b3_garbage_incinerator_80186F98[idx]];
            func_800FDB18(3, slot->extra.tmd->coords, NULL, &D_shelter_b3_garbage_incinerator_80186F90);
            arg0->state++;
            return;
        case 1:
            if (arg0->spawnArg1.value == 0) {
                if (gDisplayState.animFrame & 0xF) {
                    return;
                }
                idx                                                 &= 3;
                D_shelter_b3_garbage_incinerator_80186F90.spawnArgLo = 0x10;
                D_shelter_b3_garbage_incinerator_80186F90.coord      = &slot->extra.tmd->coords[D_shelter_b3_garbage_incinerator_80186F98[idx]];
                func_800FDB18(3, slot->extra.tmd->coords, NULL, &D_shelter_b3_garbage_incinerator_80186F90);
                return;
            }
            if (gDisplayState.animFrame & 7) {
                return;
            }
            idx                                                 &= 0xF;
            D_shelter_b3_garbage_incinerator_80186F90.spawnArgLo = 0x100;
            D_shelter_b3_garbage_incinerator_80186F90.coord      = &slot->extra.tmd->coords[D_shelter_b3_garbage_incinerator_80186F98[idx]];
            func_800FDB18(3, slot->extra.tmd->coords, NULL, &D_shelter_b3_garbage_incinerator_80186F90);
            return;
    }
}

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
            Gp_DispatchMsgPtr(msgWork->field_2C, 0x3F7, &msg, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_StateC08.field_6 |= 1;
            func_800E8614(D_shelter_b3_garbage_incinerator_80186FB8, 0);
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
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

/// Does nothing while `gGameSession->field_65`, `Gp_StateC08.field_9`,
/// `Gp_StateF0.field_4` or `D_80114CF8` is set. State 0 allocates and clears the work block (killing the task if that
/// fails), records `gameGetPtrSlot(3)` in `field_2C` and the task in
/// `D_shelter_b3_garbage_incinerator_8018FC3C`, spawns the table entry and,
/// with `spawnArg1` zero, queues sound event 0x54280005. State 1 advances once
/// the scene clock has run out while the player is alive, unless
/// `field_135` is 1 in view 0x21. State 2 advances when
/// `func_shelter_b3_garbage_incinerator_8017F588` returns nonzero.
void func_shelter_b3_garbage_incinerator_8017F6D8(Task* arg0)
{
    GameSession*            session = gGameSession;
    GarbageIncineratorWork* work;
    s32                     ok;
    PlayerStatus*           ps;

    if (session->field_65 != 0 || (s8)Gp_StateC08.field_9 != 0 || Gp_StateF0.field_4 != 0 || D_80114CF8 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != 0) {
                return;
            }
            work       = Mem_Malloc(0x40, false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x40);
                work->field_2C                            = gameGetPtrSlot(3);
                D_shelter_b3_garbage_incinerator_8018FC3C = arg0;
            }
            Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187184.tasks, 0, 0xD0, 0);
            if (arg0->spawnArg1.value == 0) {
                SndEvt_EnqueueType6(0x54280005, 0, 0);
            }
            break;
        case 1:
            ps = &Player_Status;
            if (session->sceneClock > 0) {
                ok = 0;
            } else if (ps->hp <= 0) {
                ok = 0;
            } else if (session->field_135 != 1 || session->at4.loc.view != 0x21) {
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
/// it to `field_2C` with message 0x3E8. The set's block is `Player_Status.weapon + 1`
/// when `Mc_SaveData[0].state.characterId` is 1 and `Player_Status.weapon + 0x22` otherwise.
void func_shelter_b3_garbage_incinerator_8017F8AC(s32 arg0)
{
    GarbageIncineratorWork* work;
    GpAnimArg               msg;
    s16                     anim;
    s32                     weaponId;
    s32                     setId;

    work                = D_shelter_b3_garbage_incinerator_8018FC3C->work;
    anim                = arg0 + 0x2F;
    weaponId            = Player_Status.weapon;
    setId               = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.animBlock.index = setId;
    work->field_38      = anim;
    msg.field_4         = anim;
    msg.field_8         = 1;
    msg.field_C         = 0xF;
    msg.field_10        = 0;
    Gp_DispatchMsgPtr(work->field_2C, 0x3E8, &msg, 0);
}

void func_shelter_b3_garbage_incinerator_8017F930(s32 arg0)
{
    GarbageIncineratorWork* work = (GarbageIncineratorWork*)D_shelter_b3_garbage_incinerator_8018FC3C->work;

    Gp_DispatchMsg(work->field_34, 0x7DB, arg0, 0);
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
    Player_Status.hp          = 0;
    gGameSession->restartMode = 3;
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
