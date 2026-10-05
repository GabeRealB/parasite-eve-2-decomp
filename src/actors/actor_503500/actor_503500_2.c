#include "actor_503500_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/shelter_r48.h"

/// Work block of a drift-sprite emitter, a coordinate task that sheds Shelter
/// R48's drift sprites from one fixed point of the room.
///
/// Each sprite is spawned on the emitter's own coordinate, drifting narrowly
/// upward, with `spriteSize` and `cellPeriod` packed into its spawn argument.
/// While the room's scene state winds the emitter down the sprites shrink,
/// run through their cells faster and come less often, and the emitter ends
/// once `spawnInterval` passes sixteen frames.
///
/// The emitter's task allocates the block zeroed and keeps it in `Task::work`.
typedef struct {
    s32     spriteSize;    // Size the next sprite is spawned with, a perspective numerator; only bits 0..11 reach the sprite. Shrinks to a floor while winding down
    s32     cellPeriod;    // Frames the next sprite shows each animation cell, in 1/4096 of a frame; only the whole frames reach the sprite. Shrinks to one frame while winding down
    Fixed16 spawnInterval; // Running frames between sprites: one is spawned when the task's frame count passes the integer half. Grows while winding down
} _Actor503500DriftSpriteEmitterWork;
STATIC_ASSERT_SIZEOF(_Actor503500DriftSpriteEmitterWork, 0xC);

/// Spawn positions `func_actor_503500_80132778` indexes by `Task::spawnArg1`.
extern SVECTOR D_actor_503500_8014B97C[];

extern TaskDesc D_actor_503500_8014B964[];
/// Event scripts in the overlay's `.data`, handed to `func_800E8634` (which
/// forwards the first to `Task_Spawn`).
extern EvsCommand D_actor_503500_8014CD98[];
extern EvsCommand D_actor_503500_8014D098[];

void func_actor_503500_80132778(Task*);
void func_actor_503500_80132990(Task*);
void func_actor_503500_80132D20(Task*);

static AnimationSet _gActor503500Animation16D8C;
static AnimationSet _gActor503500Animation16F18;
static AnimationSet _gActor503500Animation1789C;
static AnimationSet _gActor503500Animation181EC;
static AnimationSet _gActor503500Animation18374;
static AnimationSet _gActor503500Animation18820;
static AnimationSet _gActor503500Animation18B10;
static AnimationSet _gActor503500Animation18FCC;
static AnimationSet _gActor503500Animation19354;
static AnimationSet _gActor503500Animation195A4;
static AnimationSet _gActor503500Animation19B10;

extern AnimationPlayRequest      D_actor_503500_8014B9E8;
extern AnimationPlayRequest      D_actor_503500_8014B9FC[10];
extern AnimationPlayRequest      D_actor_503500_8014BAC4;
extern ActorCommand              D_actor_503500_8014BC14;
extern ActorCommand              D_actor_503500_8014BC18[2];
extern ActorCommand              D_actor_503500_8014BC20;
extern ActorCommand              D_actor_503500_8014BC24;
extern ActorCommand              D_actor_503500_8014BC28;
extern AnimationBankCopyRequest  D_actor_503500_8014B9CC;
extern PadScriptCmd              D_actor_503500_8014D2F0[2];
extern PadScriptCmd              D_actor_503500_8014D300[3];
extern PadScriptVibrationSegment D_actor_503500_8014D2F8[2];
extern PadScriptVibrationSegment D_actor_503500_8014D30C[3];
extern ActorTransform            D_actor_503500_8014BAD8;
extern ActorTransform            D_actor_503500_8014BAF0;
extern ActorTransform            D_actor_503500_8014BB08;
extern ActorTransform            D_actor_503500_8014BB20;
extern ActorTransform            D_actor_503500_8014BB38;
extern ActorTransform            D_actor_503500_8014BBB4[2];
extern ActorTransform            D_actor_503500_8014BBE4;
extern ActorTransform            D_actor_503500_8014BBFC;
void                             func_actor_503500_80132B78(void);
void                             func_actor_503500_80132B98(void);
void                             func_actor_503500_80132BB8(void);
void                             func_actor_503500_80132BD8(void);
void                             func_actor_503500_80132BF8(void);
void                             func_actor_503500_80132C40(s32);
void                             func_actor_503500_80132C70(s32);
void                             func_actor_503500_80132CA4(void);
void                             func_actor_503500_80132CC4(s8);
void                             func_actor_503500_80132D00(s32);
void                             func_actor_503500_80132D60(void);
void                             func_actor_503500_80132D7C(void);
void                             func_actor_503500_80132D90(s32);
void                             func_actor_503500_80132DB4(s32);
void                             func_actor_503500_80132DD4(void);
void                             func_actor_503500_80132DEC(void);
void                             func_actor_503500_80132E7C(void);
void                             func_actor_503500_80132EE8(u8);
void                             func_actor_503500_80132EF4(void);
void                             func_actor_503500_80132F28(void);

static TmdSource _gActor503500BrahmanTorso;
static TmdSource _gActor503500Model22A30;
static TmdSource _gActor503500Model23550;
static TmdSource _gActor503500Model24C70;
static TmdSource _gActor503500Model261C8;
static TmdSource _gActor503500Model27198;
static TmdSource _gActor503500Model28168;
static TmdSource _gActor503500Model29128;
static TmdSource _gActor503500Model2A450;
static TmdSource _gActor503500Model2C1C0;

static AnimationPackedPose _gActor503500Animation16D8CBank1[6] = {
#include "assets/actor_503500_animation_16D8C_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation16D8CBank4[46] = {
#include "assets/actor_503500_animation_16D8C_bank4.inc"
};

static AnimationRecord _gActor503500Animation16D8CRecords[109] = {
#include "assets/actor_503500_animation_16D8C_records.inc"
};

static u16 _gActor503500Animation16D8CIndices[20] = {
#include "assets/actor_503500_animation_16D8C_indices.inc"
};

static AnimationSet _gActor503500Animation16D8C = {
    _gActor503500Animation16D8CRecords,
    _gActor503500Animation16D8CIndices,
    { NULL, _gActor503500Animation16D8CBank1, NULL, NULL, _gActor503500Animation16D8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation16F18Bank1[2] = {
#include "assets/actor_503500_animation_16F18_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation16F18Bank4[16] = {
#include "assets/actor_503500_animation_16F18_bank4.inc"
};

static AnimationRecord _gActor503500Animation16F18Records[57] = {
#include "assets/actor_503500_animation_16F18_records.inc"
};

static u16 _gActor503500Animation16F18Indices[20] = {
#include "assets/actor_503500_animation_16F18_indices.inc"
};

static AnimationSet _gActor503500Animation16F18 = {
    _gActor503500Animation16F18Records,
    _gActor503500Animation16F18Indices,
    { NULL, _gActor503500Animation16F18Bank1, NULL, NULL, _gActor503500Animation16F18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation1789CBank1[32] = {
#include "assets/actor_503500_animation_1789C_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation1789CBank4[172] = {
#include "assets/actor_503500_animation_1789C_bank4.inc"
};

static AnimationRecord _gActor503500Animation1789CRecords[321] = {
#include "assets/actor_503500_animation_1789C_records.inc"
};

static u16 _gActor503500Animation1789CIndices[20] = {
#include "assets/actor_503500_animation_1789C_indices.inc"
};

static AnimationSet _gActor503500Animation1789C = {
    _gActor503500Animation1789CRecords,
    _gActor503500Animation1789CIndices,
    { NULL, _gActor503500Animation1789CBank1, NULL, NULL, _gActor503500Animation1789CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation181ECBank1[65] = {
#include "assets/actor_503500_animation_181EC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation181ECBank4[111] = {
#include "assets/actor_503500_animation_181EC_bank4.inc"
};

static AnimationRecord _gActor503500Animation181ECRecords[270] = {
#include "assets/actor_503500_animation_181EC_records.inc"
};

static u16 _gActor503500Animation181ECIndices[20] = {
#include "assets/actor_503500_animation_181EC_indices.inc"
};

static AnimationSet _gActor503500Animation181EC = {
    _gActor503500Animation181ECRecords,
    _gActor503500Animation181ECIndices,
    { NULL, _gActor503500Animation181ECBank1, NULL, NULL, _gActor503500Animation181ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation18374Bank1[2] = {
#include "assets/actor_503500_animation_18374_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation18374Bank4[15] = {
#include "assets/actor_503500_animation_18374_bank4.inc"
};

static AnimationRecord _gActor503500Animation18374Records[57] = {
#include "assets/actor_503500_animation_18374_records.inc"
};

static u16 _gActor503500Animation18374Indices[20] = {
#include "assets/actor_503500_animation_18374_indices.inc"
};

static AnimationSet _gActor503500Animation18374 = {
    _gActor503500Animation18374Records,
    _gActor503500Animation18374Indices,
    { NULL, _gActor503500Animation18374Bank1, NULL, NULL, _gActor503500Animation18374Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation18820Bank1[13] = {
#include "assets/actor_503500_animation_18820_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation18820Bank4[92] = {
#include "assets/actor_503500_animation_18820_bank4.inc"
};

static AnimationRecord _gActor503500Animation18820Records[148] = {
#include "assets/actor_503500_animation_18820_records.inc"
};

static u16 _gActor503500Animation18820Indices[20] = {
#include "assets/actor_503500_animation_18820_indices.inc"
};

static AnimationSet _gActor503500Animation18820 = {
    _gActor503500Animation18820Records,
    _gActor503500Animation18820Indices,
    { NULL, _gActor503500Animation18820Bank1, NULL, NULL, _gActor503500Animation18820Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation18B10Bank1[3] = {
#include "assets/actor_503500_animation_18B10_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation18B10Bank4[57] = {
#include "assets/actor_503500_animation_18B10_bank4.inc"
};

static AnimationRecord _gActor503500Animation18B10Records[102] = {
#include "assets/actor_503500_animation_18B10_records.inc"
};

static u16 _gActor503500Animation18B10Indices[20] = {
#include "assets/actor_503500_animation_18B10_indices.inc"
};

static AnimationSet _gActor503500Animation18B10 = {
    _gActor503500Animation18B10Records,
    _gActor503500Animation18B10Indices,
    { NULL, _gActor503500Animation18B10Bank1, NULL, NULL, _gActor503500Animation18B10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation18FCCBank1[4] = {
#include "assets/actor_503500_animation_18FCC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation18FCCBank4[82] = {
#include "assets/actor_503500_animation_18FCC_bank4.inc"
};

static AnimationRecord _gActor503500Animation18FCCRecords[189] = {
#include "assets/actor_503500_animation_18FCC_records.inc"
};

static u16 _gActor503500Animation18FCCIndices[20] = {
#include "assets/actor_503500_animation_18FCC_indices.inc"
};

static AnimationSet _gActor503500Animation18FCC = {
    _gActor503500Animation18FCCRecords,
    _gActor503500Animation18FCCIndices,
    { NULL, _gActor503500Animation18FCCBank1, NULL, NULL, _gActor503500Animation18FCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation19354Bank1[3] = {
#include "assets/actor_503500_animation_19354_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation19354Bank4[58] = {
#include "assets/actor_503500_animation_19354_bank4.inc"
};

static AnimationRecord _gActor503500Animation19354Records[139] = {
#include "assets/actor_503500_animation_19354_records.inc"
};

static u16 _gActor503500Animation19354Indices[20] = {
#include "assets/actor_503500_animation_19354_indices.inc"
};

static AnimationSet _gActor503500Animation19354 = {
    _gActor503500Animation19354Records,
    _gActor503500Animation19354Indices,
    { NULL, _gActor503500Animation19354Bank1, NULL, NULL, _gActor503500Animation19354Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation195A4Bank1[3] = {
#include "assets/actor_503500_animation_195A4_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation195A4Bank4[29] = {
#include "assets/actor_503500_animation_195A4_bank4.inc"
};

static AnimationRecord _gActor503500Animation195A4Records[90] = {
#include "assets/actor_503500_animation_195A4_records.inc"
};

static u16 _gActor503500Animation195A4Indices[20] = {
#include "assets/actor_503500_animation_195A4_indices.inc"
};

static AnimationSet _gActor503500Animation195A4 = {
    _gActor503500Animation195A4Records,
    _gActor503500Animation195A4Indices,
    { NULL, _gActor503500Animation195A4Bank1, NULL, NULL, _gActor503500Animation195A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation19B10Bank1[10] = {
#include "assets/actor_503500_animation_19B10_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation19B10Bank4[126] = {
#include "assets/actor_503500_animation_19B10_bank4.inc"
};

static AnimationRecord _gActor503500Animation19B10Records[171] = {
#include "assets/actor_503500_animation_19B10_records.inc"
};

static u16 _gActor503500Animation19B10Indices[20] = {
#include "assets/actor_503500_animation_19B10_indices.inc"
};

static AnimationSet _gActor503500Animation19B10 = {
    _gActor503500Animation19B10Records,
    _gActor503500Animation19B10Indices,
    { NULL, _gActor503500Animation19B10Bank1, NULL, NULL, _gActor503500Animation19B10Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_503500_8014B958 = { { { TASK_BODY_NONE, 192 } }, func_actor_503500_80132D20, { .value = 0 } };

TaskDesc D_actor_503500_8014B964[2] = {
    { { { TASK_BODY_COORD, 192 } }, func_actor_503500_80132778, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_503500_80132990, { .value = 0 } },
};

SVECTOR D_actor_503500_8014B97C[4] = {
    { 5000, 1000, 5000, 0 },
    { 8000, 1000, 7000, 0 },
    { 9000, 1000, 2000, 0 },
    { 6000, 1000, 9000, 0 },
};

AnimationSet* D_actor_503500_8014B99C[12] = {
    NULL,
    &_gActor503500Animation16D8C,
    &_gActor503500Animation16F18,
    &_gActor503500Animation1789C,
    &_gActor503500Animation181EC,
    &_gActor503500Animation18374,
    &_gActor503500Animation18B10,
    &_gActor503500Animation18FCC,
    &_gActor503500Animation19354,
    &_gActor503500Animation195A4,
    &_gActor503500Animation19B10,
    &_gActor503500Animation18820,
};

AnimationBankCopyRequest D_actor_503500_8014B9CC = { { .sets = D_actor_503500_8014B99C }, ARRAY_SIZE(D_actor_503500_8014B99C) };

AnimationPlayRequest D_actor_503500_8014B9D4 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_503500_8014B9E8 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_503500_8014B9FC[10] = {
    { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_503500_8014BAC4 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_503500_8014BAD8 = { { 2500, -2000, 6900, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_503500_8014BAF0 = { { 2000, -2000, 6900, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_503500_8014BB08 = { { 1990, -2000, 6580, 0 }, { 0, 910, 0, 0 } };

ActorTransform D_actor_503500_8014BB20 = { { 2240, -2000, 6770, 0 }, { 0, 910, 0, 0 } };

ActorTransform D_actor_503500_8014BB38 = { { 2000, 0, 1000, 0 }, { 0, 0, 0, 0 } };

AnimationPlayRequest D_actor_503500_8014BB50[5] = {
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 4, ANIMATION_BLEND_INTERPOLATE, 40, ANIMATION_WORLD_COLLISION_DISABLE },
};

ActorTransform D_actor_503500_8014BBB4[2] = {
    { { 5200, 5200, 6500, 0 }, { 0, -1024, 0, 0 } },
    { { 8490, 1000, 6210, 0 }, { 0, -682, 0, 0 } },
};

ActorTransform D_actor_503500_8014BBE4 = { { 7000, 1000, 7000, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_503500_8014BBFC = { { 7300, 500, 7600, 0 }, { 0, 1536, 0, 0 } };

ActorCommand D_actor_503500_8014BC14 = { { .loc = { 4, 48 } }, 0 };

ActorCommand D_actor_503500_8014BC18[2] = {
    { { .loc = { 4, 48 } }, 1 },
    { { .loc = { 4, 48 } }, 2 },
};

ActorCommand D_actor_503500_8014BC20 = { { .loc = { 4, 48 } }, 3 };

ActorCommand D_actor_503500_8014BC24 = { { .loc = { 4, 48 } }, 4 };

ActorCommand D_actor_503500_8014BC28 = { { .loc = { 4, 48 } }, 5 };

AnimationPlayRequest D_actor_503500_8014BC2C = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_503500_8014BC40 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_503500_8014BC54 = { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_503500_8014BC68 = { { 5190, 3920, 7820, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_503500_8014BC80[2] = {
    { { 0x2B5C, 560, 7970, 0 }, { 0, 1024, 0, 0 } },
    { { 0x3232, 1610, 7970, 0 }, { 0, 1024, 0, 0 } },
};

ActorCommand D_actor_503500_8014BCB0 = { { .loc = { 4, 48 } }, 0 };

ActorCommand D_actor_503500_8014BCB4 = { { .loc = { 4, 48 } }, 1 };

ActorCommand D_actor_503500_8014BCB8 = { { .loc = { 4, 48 } }, 2 };

ActorCommand D_actor_503500_8014BCBC = { { .loc = { 4, 48 } }, 3 };

ActorTransform D_actor_503500_8014BCC0 = { { 3350, -3360, 0x2D82, 0 }, { 0, -512, 0, 0 } };

ActorTransform D_actor_503500_8014BCD8 = { { 0x311A, -3360, 2430, 0 }, { 0, 1535, 0, 0 } };

ActorTransform D_actor_503500_8014BCF0 = { { 7960, -3360, 7040, 0 }, { 0, -512, 0, 0 } };

ActorTransform D_actor_503500_8014BD08 = { { 7960, -3360, 7040, 0 }, { 0, 1535, 0, 0 } };

ActorCommand D_actor_503500_8014BD20 = { { .loc = { 4, 48 } }, 0 };

ActorCommand D_actor_503500_8014BD24 = { { .loc = { 4, 48 } }, 1 };

ActorCommand D_actor_503500_8014BD28 = { { .loc = { 4, 48 } }, 2 };

ActorCommand D_actor_503500_8014BD2C = { { .loc = { 4, 48 } }, 3 };

EvsSceneKey D_actor_503500_8014BD30 = { 6, 10, 11 };

EvsSceneKey D_actor_503500_8014BD38 = { 6, 11, 11 };

EvsSceneKey D_actor_503500_8014BD40 = { 6, 80, 11 };

EvsCommand D_actor_503500_8014BD48[56] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_503500_8014BD30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132B78 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BAD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = D_actor_503500_8014BBB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = D_actor_503500_8014BC18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132C40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132C40 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132C40 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132C70 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132DB4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132DD4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132B98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_503500_8014B9FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[9] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[8] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_503500_8014BB50[2] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_503500_8014BB50[4] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132D00 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132D00 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132BB8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132BD8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BAF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014BAC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BBE4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014C288[29] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BAF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014BAC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BBE4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132BD8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132DB4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132DD4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132D00 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132D00 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014C540[61] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_503500_8014BD38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132B78 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132D00 }, { .value = 128 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132D60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BB08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BBB4[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_503500_8014BB50[3] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132B98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[7] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = &D_actor_503500_8014BC18[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 85 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BB20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BC68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_503500_8014BC40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_actor_503500_8014D2F0 }, { .vibrationSegments = D_actor_503500_8014D2F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_503500_8014BC54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132BB8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_503500_80132CC4 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCBC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132DB4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132D90 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014CAF8[28] = {
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132D00 }, { .value = 128 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BB20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_503500_80132CC4 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCBC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132D60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132BD8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132D90 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_503500_80132DB4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014CD98[32] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_503500_8014BD40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132B78 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_503500_80132EE8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BCC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BCD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132D7C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132B98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BCF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BD08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_actor_503500_8014D300 }, { .vibrationSegments = D_actor_503500_8014D30C }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132BB8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132BF8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014D098[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132BD8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132BF8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014D158[17] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132EF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132D60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132DEC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BB38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BBFC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_503500_80132E7C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

PadScriptCmd D_actor_503500_8014D2F0[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_503500_8014D2F8[2] = {
    { 255, 107, 20, 1 },
    { 0, 0, 8, 0 },
};

PadScriptCmd D_actor_503500_8014D300[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_503500_8014D30C[3] = {
    { 0, 0, 8, 0 },
    { 254, 252, 8, 1 },
    { 255, 22, 11, 1 },
};

static TmdBone _gActor503500BrahmanTorsoSkeleton[20] = {
#include "assets/brahman_torso_skeleton.inc"
};

static u32 _gActor503500BrahmanTorsoPartVerts[20] = {
#include "assets/brahman_torso_partVerts.inc"
};

static SVECTOR _gActor503500BrahmanTorsoVerts[441] = {
#include "assets/brahman_torso_verts.inc"
};

static SVECTOR _gActor503500BrahmanTorsoNormals[523] = {
#include "assets/brahman_torso_normals.inc"
};

static u32 _gActor503500BrahmanTorsoStream[5235] = {
#include "assets/brahman_torso_stream.inc"
};

static TmdSource _gActor503500BrahmanTorso = {
    0,
    27988,
    8696,
    20,
    _gActor503500BrahmanTorsoPartVerts,
    _gActor503500BrahmanTorsoVerts,
    _gActor503500BrahmanTorsoNormals,
    _gActor503500BrahmanTorsoSkeleton,
    _gActor503500BrahmanTorsoStream,
};

static TmdBone _gActor503500Model22A30Skeleton[1] = {
#include "assets/actor_503500_model_22A30_skeleton.inc"
};

static u32 _gActor503500Model22A30PartVerts[1] = {
#include "assets/actor_503500_model_22A30_partVerts.inc"
};

static SVECTOR _gActor503500Model22A30Verts[30] = {
#include "assets/actor_503500_model_22A30_verts.inc"
};

static SVECTOR _gActor503500Model22A30Normals[30] = {
#include "assets/actor_503500_model_22A30_normals.inc"
};

static u32 _gActor503500Model22A30Stream[250] = {
#include "assets/actor_503500_model_22A30_stream.inc"
};

static TmdSource _gActor503500Model22A30 = {
    0,
    1708,
    0,
    1,
    _gActor503500Model22A30PartVerts,
    _gActor503500Model22A30Verts,
    _gActor503500Model22A30Normals,
    _gActor503500Model22A30Skeleton,
    _gActor503500Model22A30Stream,
};

static TmdBone _gActor503500Model23550Skeleton[9] = {
#include "assets/actor_503500_model_23550_skeleton.inc"
};

static u32 _gActor503500Model23550PartVerts[9] = {
#include "assets/actor_503500_model_23550_partVerts.inc"
};

static SVECTOR _gActor503500Model23550Verts[85] = {
#include "assets/actor_503500_model_23550_verts.inc"
};

static SVECTOR _gActor503500Model23550Normals[96] = {
#include "assets/actor_503500_model_23550_normals.inc"
};

static u32 _gActor503500Model23550Stream[1019] = {
#include "assets/actor_503500_model_23550_stream.inc"
};

static TmdSource _gActor503500Model23550 = {
    0,
    4624,
    2912,
    9,
    _gActor503500Model23550PartVerts,
    _gActor503500Model23550Verts,
    _gActor503500Model23550Normals,
    _gActor503500Model23550Skeleton,
    _gActor503500Model23550Stream,
};

static TmdBone _gActor503500Model24C70Skeleton[9] = {
#include "assets/actor_503500_model_24C70_skeleton.inc"
};

static u32 _gActor503500Model24C70PartVerts[9] = {
#include "assets/actor_503500_model_24C70_partVerts.inc"
};

static SVECTOR _gActor503500Model24C70Verts[85] = {
#include "assets/actor_503500_model_24C70_verts.inc"
};

static SVECTOR _gActor503500Model24C70Normals[96] = {
#include "assets/actor_503500_model_24C70_normals.inc"
};

static u32 _gActor503500Model24C70Stream[1019] = {
#include "assets/actor_503500_model_24C70_stream.inc"
};

static TmdSource _gActor503500Model24C70 = {
    0,
    4624,
    2912,
    9,
    _gActor503500Model24C70PartVerts,
    _gActor503500Model24C70Verts,
    _gActor503500Model24C70Normals,
    _gActor503500Model24C70Skeleton,
    _gActor503500Model24C70Stream,
};

static TmdBone _gActor503500Model261C8Skeleton[9] = {
#include "assets/actor_503500_model_261C8_skeleton.inc"
};

static u32 _gActor503500Model261C8PartVerts[9] = {
#include "assets/actor_503500_model_261C8_partVerts.inc"
};

static SVECTOR _gActor503500Model261C8Verts[54] = {
#include "assets/actor_503500_model_261C8_verts.inc"
};

static SVECTOR _gActor503500Model261C8Normals[70] = {
#include "assets/actor_503500_model_261C8_normals.inc"
};

static u32 _gActor503500Model261C8Stream[661] = {
#include "assets/actor_503500_model_261C8_stream.inc"
};

static TmdSource _gActor503500Model261C8 = {
    0,
    2928,
    1820,
    9,
    _gActor503500Model261C8PartVerts,
    _gActor503500Model261C8Verts,
    _gActor503500Model261C8Normals,
    _gActor503500Model261C8Skeleton,
    _gActor503500Model261C8Stream,
};

static TmdBone _gActor503500Model27198Skeleton[9] = {
#include "assets/actor_503500_model_27198_skeleton.inc"
};

static u32 _gActor503500Model27198PartVerts[9] = {
#include "assets/actor_503500_model_27198_partVerts.inc"
};

static SVECTOR _gActor503500Model27198Verts[54] = {
#include "assets/actor_503500_model_27198_verts.inc"
};

static SVECTOR _gActor503500Model27198Normals[72] = {
#include "assets/actor_503500_model_27198_normals.inc"
};

static u32 _gActor503500Model27198Stream[661] = {
#include "assets/actor_503500_model_27198_stream.inc"
};

static TmdSource _gActor503500Model27198 = {
    0,
    2928,
    1820,
    9,
    _gActor503500Model27198PartVerts,
    _gActor503500Model27198Verts,
    _gActor503500Model27198Normals,
    _gActor503500Model27198Skeleton,
    _gActor503500Model27198Stream,
};

static TmdBone _gActor503500Model28168Skeleton[9] = {
#include "assets/actor_503500_model_28168_skeleton.inc"
};

static u32 _gActor503500Model28168PartVerts[9] = {
#include "assets/actor_503500_model_28168_partVerts.inc"
};

static SVECTOR _gActor503500Model28168Verts[54] = {
#include "assets/actor_503500_model_28168_verts.inc"
};

static SVECTOR _gActor503500Model28168Normals[72] = {
#include "assets/actor_503500_model_28168_normals.inc"
};

static u32 _gActor503500Model28168Stream[661] = {
#include "assets/actor_503500_model_28168_stream.inc"
};

static TmdSource _gActor503500Model28168 = {
    0,
    2928,
    1820,
    9,
    _gActor503500Model28168PartVerts,
    _gActor503500Model28168Verts,
    _gActor503500Model28168Normals,
    _gActor503500Model28168Skeleton,
    _gActor503500Model28168Stream,
};

static TmdBone _gActor503500Model29128Skeleton[9] = {
#include "assets/actor_503500_model_29128_skeleton.inc"
};

static u32 _gActor503500Model29128PartVerts[9] = {
#include "assets/actor_503500_model_29128_partVerts.inc"
};

static SVECTOR _gActor503500Model29128Verts[54] = {
#include "assets/actor_503500_model_29128_verts.inc"
};

static SVECTOR _gActor503500Model29128Normals[70] = {
#include "assets/actor_503500_model_29128_normals.inc"
};

static u32 _gActor503500Model29128Stream[661] = {
#include "assets/actor_503500_model_29128_stream.inc"
};

static TmdSource _gActor503500Model29128 = {
    0,
    2928,
    1820,
    9,
    _gActor503500Model29128PartVerts,
    _gActor503500Model29128Verts,
    _gActor503500Model29128Normals,
    _gActor503500Model29128Skeleton,
    _gActor503500Model29128Stream,
};

static TmdBone _gActor503500Model2A450Skeleton[4] = {
#include "assets/actor_503500_model_2A450_skeleton.inc"
};

static u32 _gActor503500Model2A450PartVerts[4] = {
#include "assets/actor_503500_model_2A450_partVerts.inc"
};

static SVECTOR _gActor503500Model2A450Verts[129] = {
#include "assets/actor_503500_model_2A450_verts.inc"
};

static SVECTOR _gActor503500Model2A450Normals[129] = {
#include "assets/actor_503500_model_2A450_normals.inc"
};

static u32 _gActor503500Model2A450Stream[1339] = {
#include "assets/actor_503500_model_2A450_stream.inc"
};

static TmdSource _gActor503500Model2A450 = {
    0,
    8228,
    992,
    4,
    _gActor503500Model2A450PartVerts,
    _gActor503500Model2A450Verts,
    _gActor503500Model2A450Normals,
    _gActor503500Model2A450Skeleton,
    _gActor503500Model2A450Stream,
};

static TmdBone _gActor503500Model2C1C0Skeleton[4] = {
#include "assets/actor_503500_model_2C1C0_skeleton.inc"
};

static u32 _gActor503500Model2C1C0PartVerts[4] = {
#include "assets/actor_503500_model_2C1C0_partVerts.inc"
};

static SVECTOR _gActor503500Model2C1C0Verts[124] = {
#include "assets/actor_503500_model_2C1C0_verts.inc"
};

static SVECTOR _gActor503500Model2C1C0Normals[124] = {
#include "assets/actor_503500_model_2C1C0_normals.inc"
};

static u32 _gActor503500Model2C1C0Stream[1288] = {
#include "assets/actor_503500_model_2C1C0_stream.inc"
};

static TmdSource _gActor503500Model2C1C0 = {
    0,
    7968,
    860,
    4,
    _gActor503500Model2C1C0PartVerts,
    _gActor503500Model2C1C0Verts,
    _gActor503500Model2C1C0Normals,
    _gActor503500Model2C1C0Skeleton,
    _gActor503500Model2C1C0Stream,
};

static AnimationPackedPose _gActor503500Animation2DB14Bank1[3] = {
#include "assets/actor_503500_animation_2DB14_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation2DB14Bank4[99] = {
#include "assets/actor_503500_animation_2DB14_bank4.inc"
};

static AnimationRecord _gActor503500Animation2DB14Records[207] = {
#include "assets/actor_503500_animation_2DB14_records.inc"
};

static u16 _gActor503500Animation2DB14Indices[20] = {
#include "assets/actor_503500_animation_2DB14_indices.inc"
};

AnimationSet gActor503500Animation2DB14 = {
    _gActor503500Animation2DB14Records,
    _gActor503500Animation2DB14Indices,
    { NULL, _gActor503500Animation2DB14Bank1, NULL, NULL, _gActor503500Animation2DB14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation2E4DCBank1[15] = {
#include "assets/actor_503500_animation_2E4DC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation2E4DCBank4[230] = {
#include "assets/actor_503500_animation_2E4DC_bank4.inc"
};

static AnimationRecord _gActor503500Animation2E4DCRecords[331] = {
#include "assets/actor_503500_animation_2E4DC_records.inc"
};

static u16 _gActor503500Animation2E4DCIndices[20] = {
#include "assets/actor_503500_animation_2E4DC_indices.inc"
};

AnimationSet gActor503500Animation2E4DC = {
    _gActor503500Animation2E4DCRecords,
    _gActor503500Animation2E4DCIndices,
    { NULL, _gActor503500Animation2E4DCBank1, NULL, NULL, _gActor503500Animation2E4DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation2EF88Bank1[15] = {
#include "assets/actor_503500_animation_2EF88_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation2EF88Bank4[247] = {
#include "assets/actor_503500_animation_2EF88_bank4.inc"
};

static AnimationRecord _gActor503500Animation2EF88Records[371] = {
#include "assets/actor_503500_animation_2EF88_records.inc"
};

static u16 _gActor503500Animation2EF88Indices[20] = {
#include "assets/actor_503500_animation_2EF88_indices.inc"
};

AnimationSet gActor503500Animation2EF88 = {
    _gActor503500Animation2EF88Records,
    _gActor503500Animation2EF88Indices,
    { NULL, _gActor503500Animation2EF88Bank1, NULL, NULL, _gActor503500Animation2EF88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation2FC70Bank1[18] = {
#include "assets/actor_503500_animation_2FC70_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation2FC70Bank4[304] = {
#include "assets/actor_503500_animation_2FC70_bank4.inc"
};

static AnimationRecord _gActor503500Animation2FC70Records[448] = {
#include "assets/actor_503500_animation_2FC70_records.inc"
};

static u16 _gActor503500Animation2FC70Indices[20] = {
#include "assets/actor_503500_animation_2FC70_indices.inc"
};

AnimationSet gActor503500Animation2FC70 = {
    _gActor503500Animation2FC70Records,
    _gActor503500Animation2FC70Indices,
    { NULL, _gActor503500Animation2FC70Bank1, NULL, NULL, _gActor503500Animation2FC70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation306E0Bank1[20] = {
#include "assets/actor_503500_animation_306E0_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation306E0Bank4[181] = {
#include "assets/actor_503500_animation_306E0_bank4.inc"
};

static AnimationRecord _gActor503500Animation306E0Records[407] = {
#include "assets/actor_503500_animation_306E0_records.inc"
};

static u16 _gActor503500Animation306E0Indices[20] = {
#include "assets/actor_503500_animation_306E0_indices.inc"
};

AnimationSet gActor503500Animation306E0 = {
    _gActor503500Animation306E0Records,
    _gActor503500Animation306E0Indices,
    { NULL, _gActor503500Animation306E0Bank1, NULL, NULL, _gActor503500Animation306E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation30F1CBank1[16] = {
#include "assets/actor_503500_animation_30F1C_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation30F1CBank4[166] = {
#include "assets/actor_503500_animation_30F1C_bank4.inc"
};

static AnimationRecord _gActor503500Animation30F1CRecords[293] = {
#include "assets/actor_503500_animation_30F1C_records.inc"
};

static u16 _gActor503500Animation30F1CIndices[20] = {
#include "assets/actor_503500_animation_30F1C_indices.inc"
};

AnimationSet gActor503500Animation30F1C = {
    _gActor503500Animation30F1CRecords,
    _gActor503500Animation30F1CIndices,
    { NULL, _gActor503500Animation30F1CBank1, NULL, NULL, _gActor503500Animation30F1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation31788Bank1[18] = {
#include "assets/actor_503500_animation_31788_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation31788Bank4[183] = {
#include "assets/actor_503500_animation_31788_bank4.inc"
};

static AnimationRecord _gActor503500Animation31788Records[282] = {
#include "assets/actor_503500_animation_31788_records.inc"
};

static u16 _gActor503500Animation31788Indices[20] = {
#include "assets/actor_503500_animation_31788_indices.inc"
};

AnimationSet gActor503500Animation31788 = {
    _gActor503500Animation31788Records,
    _gActor503500Animation31788Indices,
    { NULL, _gActor503500Animation31788Bank1, NULL, NULL, _gActor503500Animation31788Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation31D8CBank1[12] = {
#include "assets/actor_503500_animation_31D8C_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation31D8CBank4[131] = {
#include "assets/actor_503500_animation_31D8C_bank4.inc"
};

static AnimationRecord _gActor503500Animation31D8CRecords[198] = {
#include "assets/actor_503500_animation_31D8C_records.inc"
};

static u16 _gActor503500Animation31D8CIndices[20] = {
#include "assets/actor_503500_animation_31D8C_indices.inc"
};

AnimationSet gActor503500Animation31D8C = {
    _gActor503500Animation31D8CRecords,
    _gActor503500Animation31D8CIndices,
    { NULL, _gActor503500Animation31D8CBank1, NULL, NULL, _gActor503500Animation31D8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation32E24Bank1[43] = {
#include "assets/actor_503500_animation_32E24_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation32E24Bank4[310] = {
#include "assets/actor_503500_animation_32E24_bank4.inc"
};

static AnimationRecord _gActor503500Animation32E24Records[603] = {
#include "assets/actor_503500_animation_32E24_records.inc"
};

static u16 _gActor503500Animation32E24Indices[20] = {
#include "assets/actor_503500_animation_32E24_indices.inc"
};

AnimationSet gActor503500Animation32E24 = {
    _gActor503500Animation32E24Records,
    _gActor503500Animation32E24Indices,
    { NULL, _gActor503500Animation32E24Bank1, NULL, NULL, _gActor503500Animation32E24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation333DCBank1[14] = {
#include "assets/actor_503500_animation_333DC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation333DCBank4[101] = {
#include "assets/actor_503500_animation_333DC_bank4.inc"
};

static AnimationRecord _gActor503500Animation333DCRecords[203] = {
#include "assets/actor_503500_animation_333DC_records.inc"
};

static u16 _gActor503500Animation333DCIndices[20] = {
#include "assets/actor_503500_animation_333DC_indices.inc"
};

AnimationSet gActor503500Animation333DC = {
    _gActor503500Animation333DCRecords,
    _gActor503500Animation333DCIndices,
    { NULL, _gActor503500Animation333DCBank1, NULL, NULL, _gActor503500Animation333DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation33C14Bank1[21] = {
#include "assets/actor_503500_animation_33C14_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation33C14Bank4[164] = {
#include "assets/actor_503500_animation_33C14_bank4.inc"
};

static AnimationRecord _gActor503500Animation33C14Records[279] = {
#include "assets/actor_503500_animation_33C14_records.inc"
};

static u16 _gActor503500Animation33C14Indices[20] = {
#include "assets/actor_503500_animation_33C14_indices.inc"
};

AnimationSet gActor503500Animation33C14 = {
    _gActor503500Animation33C14Records,
    _gActor503500Animation33C14Indices,
    { NULL, _gActor503500Animation33C14Bank1, NULL, NULL, _gActor503500Animation33C14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation33EBCBank1[5] = {
#include "assets/actor_503500_animation_33EBC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation33EBCBank4[30] = {
#include "assets/actor_503500_animation_33EBC_bank4.inc"
};

static AnimationRecord _gActor503500Animation33EBCRecords[105] = {
#include "assets/actor_503500_animation_33EBC_records.inc"
};

static u16 _gActor503500Animation33EBCIndices[20] = {
#include "assets/actor_503500_animation_33EBC_indices.inc"
};

AnimationSet gActor503500Animation33EBC = {
    _gActor503500Animation33EBCRecords,
    _gActor503500Animation33EBCIndices,
    { NULL, _gActor503500Animation33EBCBank1, NULL, NULL, _gActor503500Animation33EBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation341D8Bank1[5] = {
#include "assets/actor_503500_animation_341D8_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation341D8Bank4[37] = {
#include "assets/actor_503500_animation_341D8_bank4.inc"
};

static AnimationRecord _gActor503500Animation341D8Records[127] = {
#include "assets/actor_503500_animation_341D8_records.inc"
};

static u16 _gActor503500Animation341D8Indices[20] = {
#include "assets/actor_503500_animation_341D8_indices.inc"
};

AnimationSet gActor503500Animation341D8 = {
    _gActor503500Animation341D8Records,
    _gActor503500Animation341D8Indices,
    { NULL, _gActor503500Animation341D8Bank1, NULL, NULL, _gActor503500Animation341D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation350C8Bank1[48] = {
#include "assets/actor_503500_animation_350C8_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation350C8Bank4[270] = {
#include "assets/actor_503500_animation_350C8_bank4.inc"
};

static AnimationRecord _gActor503500Animation350C8Records[522] = {
#include "assets/actor_503500_animation_350C8_records.inc"
};

static u16 _gActor503500Animation350C8Indices[20] = {
#include "assets/actor_503500_animation_350C8_indices.inc"
};

AnimationSet gActor503500Animation350C8 = {
    _gActor503500Animation350C8Records,
    _gActor503500Animation350C8Indices,
    { NULL, _gActor503500Animation350C8Bank1, NULL, NULL, _gActor503500Animation350C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation35390Bank1[5] = {
#include "assets/actor_503500_animation_35390_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation35390Bank4[34] = {
#include "assets/actor_503500_animation_35390_bank4.inc"
};

static AnimationRecord _gActor503500Animation35390Records[109] = {
#include "assets/actor_503500_animation_35390_records.inc"
};

static u16 _gActor503500Animation35390Indices[20] = {
#include "assets/actor_503500_animation_35390_indices.inc"
};

AnimationSet gActor503500Animation35390 = {
    _gActor503500Animation35390Records,
    _gActor503500Animation35390Indices,
    { NULL, _gActor503500Animation35390Bank1, NULL, NULL, _gActor503500Animation35390Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation356DCBank1[6] = {
#include "assets/actor_503500_animation_356DC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation356DCBank4[43] = {
#include "assets/actor_503500_animation_356DC_bank4.inc"
};

static AnimationRecord _gActor503500Animation356DCRecords[130] = {
#include "assets/actor_503500_animation_356DC_records.inc"
};

static u16 _gActor503500Animation356DCIndices[20] = {
#include "assets/actor_503500_animation_356DC_indices.inc"
};

AnimationSet gActor503500Animation356DC = {
    _gActor503500Animation356DCRecords,
    _gActor503500Animation356DCIndices,
    { NULL, _gActor503500Animation356DCBank1, NULL, NULL, _gActor503500Animation356DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation38AE0Bank1[110] = {
#include "assets/actor_503500_animation_38AE0_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation38AE0Bank4[1079] = {
#include "assets/actor_503500_animation_38AE0_bank4.inc"
};

static AnimationRecord _gActor503500Animation38AE0Records[1900] = {
#include "assets/actor_503500_animation_38AE0_records.inc"
};

static u16 _gActor503500Animation38AE0Indices[20] = {
#include "assets/actor_503500_animation_38AE0_indices.inc"
};

AnimationSet gActor503500Animation38AE0 = {
    _gActor503500Animation38AE0Records,
    _gActor503500Animation38AE0Indices,
    { NULL, _gActor503500Animation38AE0Bank1, NULL, NULL, _gActor503500Animation38AE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation3A190Bank1[101] = {
#include "assets/actor_503500_animation_3A190_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3A190Bank4[476] = {
#include "assets/actor_503500_animation_3A190_bank4.inc"
};

static AnimationRecord _gActor503500Animation3A190Records[653] = {
#include "assets/actor_503500_animation_3A190_records.inc"
};

static u16 _gActor503500Animation3A190Indices[20] = {
#include "assets/actor_503500_animation_3A190_indices.inc"
};

AnimationSet gActor503500Animation3A190 = {
    _gActor503500Animation3A190Records,
    _gActor503500Animation3A190Indices,
    { NULL, _gActor503500Animation3A190Bank1, NULL, NULL, _gActor503500Animation3A190Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation3C968Bank1[151] = {
#include "assets/actor_503500_animation_3C968_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3C968Bank4[928] = {
#include "assets/actor_503500_animation_3C968_bank4.inc"
};

static AnimationRecord _gActor503500Animation3C968Records[1149] = {
#include "assets/actor_503500_animation_3C968_records.inc"
};

static u16 _gActor503500Animation3C968Indices[20] = {
#include "assets/actor_503500_animation_3C968_indices.inc"
};

AnimationSet gActor503500Animation3C968 = {
    _gActor503500Animation3C968Records,
    _gActor503500Animation3C968Indices,
    { NULL, _gActor503500Animation3C968Bank1, NULL, NULL, _gActor503500Animation3C968Bank4, NULL, NULL, NULL },
};

DamageAttack D_actor_503500_8016E7B0[2] = {
    { 20, 2 },
    { 25, 2 },
};

DamageAttack D_actor_503500_8016E7B8[2] = {
    { 15, 3 },
    { 15, 2 },
};

DamageAttack D_actor_503500_8016E7C0[1] = {
    { 40, 6 },
};

DamageAttack D_actor_503500_8016E7C4[1] = {
    { 30, 1 },
};

DamageAttack D_actor_503500_8016E7C8[1] = {
    { 120, 6 },
};

DamageAttack* D_actor_503500_8016E7CC[1] = {
    D_actor_503500_8016E7B0,
};

DamageAttack* D_actor_503500_8016E7D0[1] = {
    D_actor_503500_8016E7B8,
};

DamageAttack* D_actor_503500_8016E7D4[2] = {
    D_actor_503500_8016E7C0,
    D_actor_503500_8016E7C4,
};

DamageAttack* D_actor_503500_8016E7DC[1] = {
    D_actor_503500_8016E7C8,
};

DamageAttack D_actor_503500_8016E7E0[1] = { 0 };

DamageAttack D_actor_503500_8016E7E4[1] = {
    { 25, 7 },
};

DamageAttack D_actor_503500_8016E7E8[1] = {
    { 45, 7 },
};

EnemyParams D_actor_503500_8016E7EC[17] = {
    { D_actor_503500_8016E7E0, 3500, 300, 500, 200, 250, 3, 0, 0 },
    { D_actor_503500_8016E7E0, 1500, 100, 500, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 500, 0, 0, 0, 100, 10, 100, 10 },
    { D_actor_503500_8016E7E0, 500, 0, 0, 0, 100, 10, 100, 10 },
    { D_actor_503500_8016E7E0, 700, 0, 0, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 0, 0, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 1500, 700, 2000, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 800, 3000, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 800, 3000, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 1500, 200, 500, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E8, 1000, 1000, 5000, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E8, 1000, 1000, 5000, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 100, 500, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10 },
};

s8 D_actor_503500_8016E8FC[20] = {
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    2,
    2,
    2,
    2,
    0,
    1,
    1,
    2,
    2,
    2,
    1,
    2,
    0,
};

u8 D_actor_503500_8016E910[20] = {
    15,
    107,
    55,
    87,
    59,
    91,
    118,
    55,
    87,
    108,
    59,
    91,
    11,
    55,
    55,
    87,
    87,
    0,
    0,
    0,
};

TaskDesc D_actor_503500_8016E924[17] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_503500_80137238, { .model = &_gActor503500BrahmanTorso } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_503500_801384D4, { .model = &_gActor503500Model22A30 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_503500_8013AD0C, { .model = &_gActor503500Model24C70 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_503500_8013AD0C, { .model = &_gActor503500Model23550 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_503500_8013BE8C, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_503500_8013BE8C, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_503500_8013CA8C, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_503500_8013DBF4, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_503500_8013DBF4, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_503500_8013EC64, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_503500_801442A8, { .model = &_gActor503500Model2A450 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_503500_801442A8, { .model = &_gActor503500Model2C1C0 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_503500_8013FA1C, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_503500_80142370, { .model = &_gActor503500Model27198 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_503500_80142370, { .model = &_gActor503500Model261C8 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_503500_80142370, { .model = &_gActor503500Model29128 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_503500_80142370, { .model = &_gActor503500Model28168 } },
};

TaskDesc D_actor_503500_8016E9F0[5] = {
    { { { TASK_BODY_COORD, 192 } }, func_actor_503500_80144890, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, func_actor_503500_80144E34, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, func_actor_503500_8014554C, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, func_actor_503500_801459D4, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, func_actor_503500_80145F84, { .value = 0 } },
};

/// Player-facing flag byte in the main executable; no module header owns it yet.

void func_actor_503500_80132F58(s32 unused);

void func_actor_503500_80132778(Task* task)
{
    enum {
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE         = 0xC00,      // Sprite size until the emitter winds down
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_STEP    = 0x10,       // Size lost per winding-down frame
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_MIN     = 0x100,      // Floor of the shrinking size
        ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD         = 0x4000,     // Four frames per cell until the emitter winds down
        ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_STEP    = 0x20,       // 1/128 of a frame lost per winding-down frame
        ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_MIN     = 0x1000,     // Floor of the shrinking period, one frame per cell
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL      = 0x60000,    // Six frames between sprites until the emitter winds down
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_STEP = 0x1000,     // 1/16 of a frame gained per winding-down frame
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_MAX  = 0x100000,   // Sixteen frames; the emitter ends once its interval is longer
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_SIZE_MASK     = 0xFFF,      // Size bits of the drift sprite's spawn argument
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_PERIOD_MASK   = 0xF000,     // Frames-per-cell nibble of the drift sprite's spawn argument
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_NARROW_UPWARD = 0x03800000, // Movement kind 3 (random, narrowly upward) at speed 0x80
    };
    GfxCoord*                           coord;
    GfxRotationWords*                   rot;
    _Actor503500DriftSpriteEmitterWork* work;
    SVECTOR*                            pos;
    u8                                  done;

    coord = task->extra.coordBody->coord;
    if (task->state == 0) {
        pos                 = &D_actor_503500_8014B97C[task->spawnArg1.value];
        coord->coord.t[0]   = pos->vx;
        coord->coord.t[1]   = pos->vy;
        coord->coord.t[2]   = pos->vz;
        rot                 = (GfxRotationWords*)&coord->coord;
        rot->m00M01         = ONE;
        rot->m02M10         = 0;
        rot->m11M12         = ONE;
        rot->m20M21         = 0;
        rot->m22            = ONE;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work                = memCalloc(sizeof(*work), false);
        if (work == NULL) {
            taskKill(task);
            return;
        }
        task->work               = work;
        work->spriteSize         = ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE;
        work->cellPeriod         = ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD;
        work->spawnInterval.word = ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL;
        task->state++;
    }
    work = task->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        if (work->spawnInterval.halves.integer < ++task->killCountdown) {
            task->killCountdown = 0;
            Gp_SpawnEff(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord,
                        (work->cellPeriod & ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_PERIOD_MASK) | ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_NARROW_UPWARD | (work->spriteSize & ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_SIZE_MASK), NULL);
        }
    }
    switch (gameFlagGetNibble(GAME_FLAG_SHELTER_R48_SCENE_STATE)) {
        case 0:
        case 1:
            if (gGameSession->eventState == 0) {
                taskKill(task);
                return;
            }
            done = gGameSession->evtSkipped;
            break;
        case 2:
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            // Winding down: later sprites are smaller and shorter-lived.
            work->cellPeriod -= ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_STEP;
            if (work->cellPeriod < ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_MIN) {
                work->cellPeriod = ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_MIN;
            }
            work->spriteSize -= ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_STEP;
            if (work->spriteSize < ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_MIN) {
                work->spriteSize = ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_MIN;
            }
        case 4:
            work->spawnInterval.word += ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_STEP;
            done                      = work->spawnInterval.word > ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_MAX;
            break;
        default:
            taskKill(task);
            return;
    }
    if (done) {
        taskKill(task);
    }
}

void func_actor_503500_80132990(Task* task)
{
    TILE*     tile;
    DR_TPAGE* dr;
    u8        r, g, b;

    r = g = b = task->killCountdown;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        switch (task->state) {
            case 0:
                task->killCountdown = 0xFF;
                task->state++;
                break;
            case 1:
                if (--task->spawnArg1.value < 0 || gGameSession->evtSkipped != 0) {
                    task->state++;
                }
                break;
            case 2:
                task->killCountdown -= 8;
                if (task->killCountdown < 0) {
                    taskKill(task);
                }
                break;
            default:
                taskKill(task);
                break;
        }
    }
    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    SetSemiTrans(tile, 1);
    tile->x0 = -160;
    tile->y0 = -120;
    tile->w  = 320;
    tile->h  = 240;
    setRGB0(tile, r, g, b);
    addPrim(gGpuCurrentOt + 3, tile);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 1, 0, getTPage(0, GPU_BLEND_SUBTRACT, 320, 0));
    addPrim(gGpuCurrentOt + 3, dr);
}

/// Record handler (opcode 0x0D) of the actor's script data: queues the
/// replacing load of overlay 0x82.
void func_actor_503500_80132B78(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Record handler (opcode 0x0D) of the actor's script data: queues the load
/// of overlay 0x81.
void func_actor_503500_80132B98(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Record handler (opcode 0x0D) of the actor's script data: restores the
/// stream random-number state.
void func_actor_503500_80132BB8(void)
{
    Gp_RestoreStreamRng();
}

/// Record handler (opcode 0x0D) of the actor's script data: cancels the queued
/// CD command and restarts the CD queue.
void func_actor_503500_80132BD8(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_actor_503500_80132BF8(void)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = 0x16;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

void func_actor_503500_80132C40(s32 arg0)
{
    taskSpawnFromTable(D_actor_503500_8014B964, 0, arg0, 0);
}

void func_actor_503500_80132C70(s32 arg0)
{
    D_actor_503500_80176558 = taskSpawnFromTable(D_actor_503500_8014B964, 1, arg0, 0);
}

/// Record handler (opcode 0x0D) of the actor's script data: calls
/// `Gp_PulseState1C`.
void func_actor_503500_80132CA4(void)
{
    Gp_PulseState1C();
}

void func_actor_503500_80132CC4(s8 arg0)
{
    Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x23);
    gSceneCombatState.signals.bytes.endDelayFrames = arg0;
}

/// Record handler (opcode 0x0D) of the actor's script data, taking the
/// record's argument word: ORs it into `GameSession::flowFlags` (the script
/// passes 1 and 2).
void func_actor_503500_80132D00(s32 bits)
{
    gGameSession->flowFlags |= bits;
}

void func_actor_503500_80132D20(Task* arg0)
{
    func_800E8634(D_actor_503500_8014CD98, 0, D_actor_503500_8014D098);
    taskKill(arg0);
}

void func_actor_503500_80132D60(void)
{
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

void func_actor_503500_80132D7C(void)
{
    gGameSession->viewDirty = 1;
}

void func_actor_503500_80132D90(s32 arg0)
{
    gameFlagSetNibble(GAME_FLAG_100, arg0);
}

void func_actor_503500_80132DB4(s32 arg0)
{
    func_shelter_r48_8017E27C(arg0 & 0xFF);
}

void func_actor_503500_80132DD4(void)
{
    D_actor_503500_8017655C.pos.vx = 0;
    D_actor_503500_8017655C.pos.vy = 0;
    D_actor_503500_8017655C.pos.vz = 0;
}

void func_actor_503500_80132DEC(void)
{
    Task*     slot3;
    GfxCoord* coord;
    SVECTOR*  rot;

    slot3 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    coord = slot3->extra.tmd->coords;

    D_actor_503500_8017655C.pos.vx = coord->coord.t[0];
    D_actor_503500_8017655C.pos.vy = coord->coord.t[1];
    D_actor_503500_8017655C.pos.vz = coord->coord.t[2];

    /* Anchoring the rotation pointer *after* the three word stores is what
     * makes cse keep the plain symbol as the base address; taking it first
     * anchors the whole function on `D_actor_503500_8017655C + 0x10`. */
    rot = &D_actor_503500_8017655C.rot;

    rot->vx = ((GameActor*)slot3->work)->rotation.vx;
    rot->vy = ((GameActor*)slot3->work)->rotation.vy;
    rot->vz = ((GameActor*)slot3->work)->rotation.vz;
}

void func_actor_503500_80132E7C(void)
{
    Task* slot3;

    slot3 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if ((D_actor_503500_8017655C.pos.vx != 0) || (D_actor_503500_8017655C.pos.vy != 0) ||
        (D_actor_503500_8017655C.pos.vz != 0)) {
        TASK_MESSAGE_DISPATCH_POINTER(slot3, 0x3E9, &D_actor_503500_8017655C, 0);
    }
}

void func_actor_503500_80132EE8(u8 arg0)
{
    D_80115768 = arg0;
}

void func_actor_503500_80132EF4(void)
{
    func_80106350(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), gPlayerStatus.weapon, 0);
}

void func_actor_503500_80132F28(void)
{
    Gp_HaltPadScripts();
    gGameSession->padScriptFlags = 0;
}

void func_actor_503500_80132F58(s32 unused)
{
    D_actor_503500_80176558 = NULL;
}
