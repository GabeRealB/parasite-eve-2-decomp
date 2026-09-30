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
#include "gameplay/pairsrc.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_state.h"
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

/// Work block `func_actor_503500_80132778` allocates (`memCalloc(0xC)`) and
/// parks in `Task::work`. Each spawn packs `field_0 & 0xFFF` and
/// `field_4 & 0xF000` into the `Gp_SpawnEff` argument; `field_8` is a 16.16
/// period whose integer half is the `Task::killCountdown` limit between
/// spawns. Flag nibble 0x12A states 2..4 decay the first two and stretch the
/// period until it passes 0x10 and the task dies.
typedef struct Actor503500EffWork {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    union {
        /* 0x8 */ s32 w;
        struct {
            /* 0x8 */ s16 lo;
            /* 0xA */ s16 hi;
        } h;
    } field_8;
} Actor503500EffWork;
STATIC_ASSERT_SIZEOF(Actor503500EffWork, 0xC);

/// Spawn positions `func_actor_503500_80132778` indexes by `Task::spawnArg1`.
extern SVECTOR D_actor_503500_8014B97C[];

extern TaskDesc D_actor_503500_8014B964[];
/// Opaque script/table blobs in the overlay's `.data`, handed to
/// `func_800E8634` (which forwards them to `Task_Spawn`) as raw addresses.
extern GpEvsCmd D_actor_503500_8014CD98[];
extern GpEvsCmd D_actor_503500_8014D098[];

void func_actor_503500_80132778(Task*);
void func_actor_503500_80132990(Task*);
void func_actor_503500_80132D20(Task*);

extern AnimationSet D_actor_503500_80148BAC;
extern AnimationSet D_actor_503500_80148D38;
extern AnimationSet D_actor_503500_801496BC;
extern AnimationSet D_actor_503500_8014A00C;
extern AnimationSet D_actor_503500_8014A194;
extern AnimationSet D_actor_503500_8014A640;
extern AnimationSet D_actor_503500_8014A930;
extern AnimationSet D_actor_503500_8014ADEC;
extern AnimationSet D_actor_503500_8014B174;
extern AnimationSet D_actor_503500_8014B3C4;
extern AnimationSet D_actor_503500_8014B930;

extern AnimationPlayRequest D_actor_503500_8014B9E8;
extern AnimationPlayRequest D_actor_503500_8014B9FC[10];
extern AnimationPlayRequest D_actor_503500_8014BAC4;
extern ActorCommand         D_actor_503500_8014BC14;
extern ActorCommand         D_actor_503500_8014BC18[2];
extern ActorCommand         D_actor_503500_8014BC20;
extern ActorCommand         D_actor_503500_8014BC24;
extern ActorCommand         D_actor_503500_8014BC28;
extern GpCopyArg            D_actor_503500_8014B9CC;
extern GpScriptCmd          D_actor_503500_8014D2F0[2];
extern GpScriptCmd          D_actor_503500_8014D300[3];
extern GpScriptRec          D_actor_503500_8014D2F8[2];
extern GpScriptRec          D_actor_503500_8014D30C[3];
extern ActorTransform       D_actor_503500_8014BAD8;
extern ActorTransform       D_actor_503500_8014BAF0;
extern ActorTransform       D_actor_503500_8014BB08;
extern ActorTransform       D_actor_503500_8014BB20;
extern ActorTransform       D_actor_503500_8014BB38;
extern ActorTransform       D_actor_503500_8014BBB4[2];
extern ActorTransform       D_actor_503500_8014BBE4;
extern ActorTransform       D_actor_503500_8014BBFC;
void                        func_actor_503500_80132B78(void);
void                        func_actor_503500_80132B98(void);
void                        func_actor_503500_80132BB8(void);
void                        func_actor_503500_80132BD8(void);
void                        func_actor_503500_80132BF8(void);
void                        func_actor_503500_80132C40(s32);
void                        func_actor_503500_80132C70(s32);
void                        func_actor_503500_80132CA4(void);
void                        func_actor_503500_80132CC4(s8);
void                        func_actor_503500_80132D00(s32);
void                        func_actor_503500_80132D60(void);
void                        func_actor_503500_80132D7C(void);
void                        func_actor_503500_80132D90(s32);
void                        func_actor_503500_80132DB4(s32);
void                        func_actor_503500_80132DD4(void);
void                        func_actor_503500_80132DEC(void);
void                        func_actor_503500_80132E7C(void);
void                        func_actor_503500_80132EE8(u8);
void                        func_actor_503500_80132EF4(void);
void                        func_actor_503500_80132F28(void);

extern TmdSource D_actor_503500_80154624;
extern TmdSource D_actor_503500_80154C38;
extern TmdSource D_actor_503500_80156358;
extern TmdSource D_actor_503500_80157A78;
extern TmdSource D_actor_503500_80158A38;
extern TmdSource D_actor_503500_80159A08;
extern TmdSource D_actor_503500_8015A9D8;
extern TmdSource D_actor_503500_8015B998;
extern TmdSource D_actor_503500_8015D758;
extern TmdSource D_actor_503500_8015F3FC;

AnimationPackedPose D_actor_503500_801488D0[6] = {
#include "assets/actor_503500_animation_16D8C_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80148918[46] = {
#include "assets/actor_503500_animation_16D8C_bank4.inc"
};

AnimationRecord D_actor_503500_801489D0[109] = {
#include "assets/actor_503500_animation_16D8C_records.inc"
};

u16 D_actor_503500_80148B84[20] = {
#include "assets/actor_503500_animation_16D8C_indices.inc"
};

AnimationSet D_actor_503500_80148BAC = {
    D_actor_503500_801489D0,
    D_actor_503500_80148B84,
    { NULL, D_actor_503500_801488D0, NULL, NULL, D_actor_503500_80148918, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80148BD4[2] = {
#include "assets/actor_503500_animation_16F18_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80148BEC[16] = {
#include "assets/actor_503500_animation_16F18_bank4.inc"
};

AnimationRecord D_actor_503500_80148C2C[57] = {
#include "assets/actor_503500_animation_16F18_records.inc"
};

u16 D_actor_503500_80148D10[20] = {
#include "assets/actor_503500_animation_16F18_indices.inc"
};

AnimationSet D_actor_503500_80148D38 = {
    D_actor_503500_80148C2C,
    D_actor_503500_80148D10,
    { NULL, D_actor_503500_80148BD4, NULL, NULL, D_actor_503500_80148BEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80148D60[32] = {
#include "assets/actor_503500_animation_1789C_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80148EE0[172] = {
#include "assets/actor_503500_animation_1789C_bank4.inc"
};

AnimationRecord D_actor_503500_80149190[321] = {
#include "assets/actor_503500_animation_1789C_records.inc"
};

u16 D_actor_503500_80149694[20] = {
#include "assets/actor_503500_animation_1789C_indices.inc"
};

AnimationSet D_actor_503500_801496BC = {
    D_actor_503500_80149190,
    D_actor_503500_80149694,
    { NULL, D_actor_503500_80148D60, NULL, NULL, D_actor_503500_80148EE0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_801496E4[65] = {
#include "assets/actor_503500_animation_181EC_bank1.inc"
};

AnimationPackedRotation D_actor_503500_801499F0[111] = {
#include "assets/actor_503500_animation_181EC_bank4.inc"
};

AnimationRecord D_actor_503500_80149BAC[270] = {
#include "assets/actor_503500_animation_181EC_records.inc"
};

u16 D_actor_503500_80149FE4[20] = {
#include "assets/actor_503500_animation_181EC_indices.inc"
};

AnimationSet D_actor_503500_8014A00C = {
    D_actor_503500_80149BAC,
    D_actor_503500_80149FE4,
    { NULL, D_actor_503500_801496E4, NULL, NULL, D_actor_503500_801499F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_8014A034[2] = {
#include "assets/actor_503500_animation_18374_bank1.inc"
};

AnimationPackedRotation D_actor_503500_8014A04C[15] = {
#include "assets/actor_503500_animation_18374_bank4.inc"
};

AnimationRecord D_actor_503500_8014A088[57] = {
#include "assets/actor_503500_animation_18374_records.inc"
};

u16 D_actor_503500_8014A16C[20] = {
#include "assets/actor_503500_animation_18374_indices.inc"
};

AnimationSet D_actor_503500_8014A194 = {
    D_actor_503500_8014A088,
    D_actor_503500_8014A16C,
    { NULL, D_actor_503500_8014A034, NULL, NULL, D_actor_503500_8014A04C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_8014A1BC[13] = {
#include "assets/actor_503500_animation_18820_bank1.inc"
};

AnimationPackedRotation D_actor_503500_8014A258[92] = {
#include "assets/actor_503500_animation_18820_bank4.inc"
};

AnimationRecord D_actor_503500_8014A3C8[148] = {
#include "assets/actor_503500_animation_18820_records.inc"
};

u16 D_actor_503500_8014A618[20] = {
#include "assets/actor_503500_animation_18820_indices.inc"
};

AnimationSet D_actor_503500_8014A640 = {
    D_actor_503500_8014A3C8,
    D_actor_503500_8014A618,
    { NULL, D_actor_503500_8014A1BC, NULL, NULL, D_actor_503500_8014A258, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_8014A668[3] = {
#include "assets/actor_503500_animation_18B10_bank1.inc"
};

AnimationPackedRotation D_actor_503500_8014A68C[57] = {
#include "assets/actor_503500_animation_18B10_bank4.inc"
};

AnimationRecord D_actor_503500_8014A770[102] = {
#include "assets/actor_503500_animation_18B10_records.inc"
};

u16 D_actor_503500_8014A908[20] = {
#include "assets/actor_503500_animation_18B10_indices.inc"
};

AnimationSet D_actor_503500_8014A930 = {
    D_actor_503500_8014A770,
    D_actor_503500_8014A908,
    { NULL, D_actor_503500_8014A668, NULL, NULL, D_actor_503500_8014A68C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_8014A958[4] = {
#include "assets/actor_503500_animation_18FCC_bank1.inc"
};

AnimationPackedRotation D_actor_503500_8014A988[82] = {
#include "assets/actor_503500_animation_18FCC_bank4.inc"
};

AnimationRecord D_actor_503500_8014AAD0[189] = {
#include "assets/actor_503500_animation_18FCC_records.inc"
};

u16 D_actor_503500_8014ADC4[20] = {
#include "assets/actor_503500_animation_18FCC_indices.inc"
};

AnimationSet D_actor_503500_8014ADEC = {
    D_actor_503500_8014AAD0,
    D_actor_503500_8014ADC4,
    { NULL, D_actor_503500_8014A958, NULL, NULL, D_actor_503500_8014A988, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_8014AE14[3] = {
#include "assets/actor_503500_animation_19354_bank1.inc"
};

AnimationPackedRotation D_actor_503500_8014AE38[58] = {
#include "assets/actor_503500_animation_19354_bank4.inc"
};

AnimationRecord D_actor_503500_8014AF20[139] = {
#include "assets/actor_503500_animation_19354_records.inc"
};

u16 D_actor_503500_8014B14C[20] = {
#include "assets/actor_503500_animation_19354_indices.inc"
};

AnimationSet D_actor_503500_8014B174 = {
    D_actor_503500_8014AF20,
    D_actor_503500_8014B14C,
    { NULL, D_actor_503500_8014AE14, NULL, NULL, D_actor_503500_8014AE38, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_8014B19C[3] = {
#include "assets/actor_503500_animation_195A4_bank1.inc"
};

AnimationPackedRotation D_actor_503500_8014B1C0[29] = {
#include "assets/actor_503500_animation_195A4_bank4.inc"
};

AnimationRecord D_actor_503500_8014B234[90] = {
#include "assets/actor_503500_animation_195A4_records.inc"
};

u16 D_actor_503500_8014B39C[20] = {
#include "assets/actor_503500_animation_195A4_indices.inc"
};

AnimationSet D_actor_503500_8014B3C4 = {
    D_actor_503500_8014B234,
    D_actor_503500_8014B39C,
    { NULL, D_actor_503500_8014B19C, NULL, NULL, D_actor_503500_8014B1C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_8014B3EC[10] = {
#include "assets/actor_503500_animation_19B10_bank1.inc"
};

AnimationPackedRotation D_actor_503500_8014B464[126] = {
#include "assets/actor_503500_animation_19B10_bank4.inc"
};

AnimationRecord D_actor_503500_8014B65C[171] = {
#include "assets/actor_503500_animation_19B10_records.inc"
};

u16 D_actor_503500_8014B908[20] = {
#include "assets/actor_503500_animation_19B10_indices.inc"
};

AnimationSet D_actor_503500_8014B930 = {
    D_actor_503500_8014B65C,
    D_actor_503500_8014B908,
    { NULL, D_actor_503500_8014B3EC, NULL, NULL, D_actor_503500_8014B464, NULL, NULL, NULL },
};

TaskDesc D_actor_503500_8014B958 = { 0, 192, func_actor_503500_80132D20, { .model = NULL } };

TaskDesc D_actor_503500_8014B964[2] = {
    { TASK_BODY_COORD, 192, func_actor_503500_80132778, { .model = NULL } },
    { 0, 192, func_actor_503500_80132990, { .model = NULL } },
};

SVECTOR D_actor_503500_8014B97C[4] = {
    { 5000, 1000, 5000, 0 },
    { 8000, 1000, 7000, 0 },
    { 9000, 1000, 2000, 0 },
    { 6000, 1000, 9000, 0 },
};

AnimationSet* D_actor_503500_8014B99C[12] = {
    NULL,
    &D_actor_503500_80148BAC,
    &D_actor_503500_80148D38,
    &D_actor_503500_801496BC,
    &D_actor_503500_8014A00C,
    &D_actor_503500_8014A194,
    &D_actor_503500_8014A930,
    &D_actor_503500_8014ADEC,
    &D_actor_503500_8014B174,
    &D_actor_503500_8014B3C4,
    &D_actor_503500_8014B930,
    &D_actor_503500_8014A640,
};

GpCopyArg D_actor_503500_8014B9CC = { { .sets = D_actor_503500_8014B99C }, 12 };

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

GpOverlayIds D_actor_503500_8014BD30 = { 6, 10, 11 };

GpOverlayIds D_actor_503500_8014BD38 = { 6, 11, 11 };

GpOverlayIds D_actor_503500_8014BD40 = { 6, 80, 11 };

GpEvsCmd D_actor_503500_8014BD48[56] = {
    { 12, { .overlays = &D_actor_503500_8014BD30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132B78 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_503500_8014B9CC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_503500_8014BAD8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = D_actor_503500_8014BBB4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .storage = D_actor_503500_8014BC18 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132C40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132C40 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132C40 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132C70 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132DB4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132DD4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132B98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_503500_8014B9FC }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[9] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[8] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[1] }, { .value = 0 } },
    { 4, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[3] }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_503500_8014BB50[2] }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[2] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_503500_8014BB50[4] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132D00 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132D00 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132BB8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132BD8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_503500_8014BAF0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014BAC4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_503500_8014BBE4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC14 } }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_503500_8014C288[29] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_503500_8014BAF0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_503500_8014B9CC }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014BAC4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_503500_8014BBE4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC14 } }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132BD8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132DB4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132DD4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132D00 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132D00 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_503500_8014C540[61] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_503500_8014B9CC }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_actor_503500_8014BD38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132B78 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132D00 }, { .value = 128 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132D60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_503500_8014BB08 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_503500_8014BBB4[1] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_503500_8014BB50[3] }, { .value = 0 } },
    { 3, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132B98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[7] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .storage = &D_actor_503500_8014BC18[1] }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 85 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 17, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 17, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[4] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 19, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_503500_8014BB20 }, { .value = 0 } },
    { 4, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[5] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_503500_8014BC68 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_503500_8014BC40 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB4 } }, { .value = 0 } },
    { 14, { .padCommands = D_actor_503500_8014D2F0 }, { .padRecords = D_actor_503500_8014D2F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB0 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_503500_8014BC54 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB8 } }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[6] }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB0 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132BB8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS8 = func_actor_503500_80132CC4 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC20 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCBC } }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132DB4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 32 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 33 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132D90 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_503500_8014CAF8[28] = {
    { 17, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132D00 }, { .value = 128 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_503500_8014BB20 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_503500_8014B9CC }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { 13, { .callbackS8 = func_actor_503500_80132CC4 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC20 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCBC } }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132D60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132CA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132BD8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132D90 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_503500_80132DB4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 32 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 33 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_503500_8014CD98[32] = {
    { 12, { .overlays = &D_actor_503500_8014BD40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132B78 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_actor_503500_80132EE8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_503500_8014B9CC }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 32 }, { .value = 2004 }, { .storage = &D_actor_503500_8014BCC0 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 33 }, { .value = 2004 }, { .storage = &D_actor_503500_8014BCD8 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132D7C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132B98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 32 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD2C } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 32 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 33 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 32 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD24 } }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 33 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD28 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 32 }, { .value = 2004 }, { .storage = &D_actor_503500_8014BCF0 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 33 }, { .value = 2004 }, { .storage = &D_actor_503500_8014BD08 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 32 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD20 } }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 33 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD20 } }, { .value = 0 } },
    { 14, { .padCommands = D_actor_503500_8014D300 }, { .padRecords = D_actor_503500_8014D30C }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132BB8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132BF8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_503500_8014D098[8] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_503500_8014B9CC }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132BD8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132BF8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_503500_8014D158[17] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_503500_8014B9CC }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132EF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132D60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132DEC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_503500_8014BB38 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC24 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_503500_8014BBFC }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_503500_80132E7C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC28 } }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpScriptCmd D_actor_503500_8014D2F0[2] = {
    { 257, 1 },
    { 0, 0 },
};

GpScriptRec D_actor_503500_8014D2F8[2] = {
    { 255, 107, 20, 1 },
    { 0, 0, 8, 0 },
};

GpScriptCmd D_actor_503500_8014D300[3] = {
    { 1, 257 },
    { 0, 513 },
    { 0, 0 },
};

GpScriptRec D_actor_503500_8014D30C[3] = {
    { 0, 0, 8, 0 },
    { 254, 252, 8, 1 },
    { 255, 22, 11, 1 },
};

TmdBone D_actor_503500_8014D318[20] = {
#include "assets/actor_503500_model_22804_skeleton.inc"
};

u32 D_actor_503500_8014D5E8[20] = {
#include "assets/actor_503500_model_22804_partVerts.inc"
};

SVECTOR D_actor_503500_8014D638[441] = {
#include "assets/actor_503500_model_22804_verts.inc"
};

SVECTOR D_actor_503500_8014E400[523] = {
#include "assets/actor_503500_model_22804_normals.inc"
};

u32 D_actor_503500_8014F458[5235] = {
#include "assets/actor_503500_model_22804_stream.inc"
};

TmdSource D_actor_503500_80154624 = {
    0,
    27988,
    8696,
    20,
    D_actor_503500_8014D5E8,
    D_actor_503500_8014D638,
    D_actor_503500_8014E400,
    D_actor_503500_8014D318,
    D_actor_503500_8014F458,
};

TmdBone D_actor_503500_80154648[1] = {
#include "assets/actor_503500_model_22E18_skeleton.inc"
};

u32 D_actor_503500_8015466C[1] = {
#include "assets/actor_503500_model_22E18_partVerts.inc"
};

SVECTOR D_actor_503500_80154670[30] = {
#include "assets/actor_503500_model_22E18_verts.inc"
};

SVECTOR D_actor_503500_80154760[30] = {
#include "assets/actor_503500_model_22E18_normals.inc"
};

u32 D_actor_503500_80154850[250] = {
#include "assets/actor_503500_model_22E18_stream.inc"
};

TmdSource D_actor_503500_80154C38 = {
    0,
    1708,
    0,
    1,
    D_actor_503500_8015466C,
    D_actor_503500_80154670,
    D_actor_503500_80154760,
    D_actor_503500_80154648,
    D_actor_503500_80154850,
};

TmdBone D_actor_503500_80154C5C[9] = {
#include "assets/actor_503500_model_24538_skeleton.inc"
};

u32 D_actor_503500_80154DA0[9] = {
#include "assets/actor_503500_model_24538_partVerts.inc"
};

SVECTOR D_actor_503500_80154DC4[85] = {
#include "assets/actor_503500_model_24538_verts.inc"
};

SVECTOR D_actor_503500_8015506C[96] = {
#include "assets/actor_503500_model_24538_normals.inc"
};

u32 D_actor_503500_8015536C[1019] = {
#include "assets/actor_503500_model_24538_stream.inc"
};

TmdSource D_actor_503500_80156358 = {
    0,
    4624,
    2912,
    9,
    D_actor_503500_80154DA0,
    D_actor_503500_80154DC4,
    D_actor_503500_8015506C,
    D_actor_503500_80154C5C,
    D_actor_503500_8015536C,
};

TmdBone D_actor_503500_8015637C[9] = {
#include "assets/actor_503500_model_25C58_skeleton.inc"
};

u32 D_actor_503500_801564C0[9] = {
#include "assets/actor_503500_model_25C58_partVerts.inc"
};

SVECTOR D_actor_503500_801564E4[85] = {
#include "assets/actor_503500_model_25C58_verts.inc"
};

SVECTOR D_actor_503500_8015678C[96] = {
#include "assets/actor_503500_model_25C58_normals.inc"
};

u32 D_actor_503500_80156A8C[1019] = {
#include "assets/actor_503500_model_25C58_stream.inc"
};

TmdSource D_actor_503500_80157A78 = {
    0,
    4624,
    2912,
    9,
    D_actor_503500_801564C0,
    D_actor_503500_801564E4,
    D_actor_503500_8015678C,
    D_actor_503500_8015637C,
    D_actor_503500_80156A8C,
};

TmdBone D_actor_503500_80157A9C[9] = {
#include "assets/actor_503500_model_26C18_skeleton.inc"
};

u32 D_actor_503500_80157BE0[9] = {
#include "assets/actor_503500_model_26C18_partVerts.inc"
};

SVECTOR D_actor_503500_80157C04[54] = {
#include "assets/actor_503500_model_26C18_verts.inc"
};

SVECTOR D_actor_503500_80157DB4[70] = {
#include "assets/actor_503500_model_26C18_normals.inc"
};

u32 D_actor_503500_80157FE4[661] = {
#include "assets/actor_503500_model_26C18_stream.inc"
};

TmdSource D_actor_503500_80158A38 = {
    0,
    2928,
    1820,
    9,
    D_actor_503500_80157BE0,
    D_actor_503500_80157C04,
    D_actor_503500_80157DB4,
    D_actor_503500_80157A9C,
    D_actor_503500_80157FE4,
};

TmdBone D_actor_503500_80158A5C[9] = {
#include "assets/actor_503500_model_27BE8_skeleton.inc"
};

u32 D_actor_503500_80158BA0[9] = {
#include "assets/actor_503500_model_27BE8_partVerts.inc"
};

SVECTOR D_actor_503500_80158BC4[54] = {
#include "assets/actor_503500_model_27BE8_verts.inc"
};

SVECTOR D_actor_503500_80158D74[72] = {
#include "assets/actor_503500_model_27BE8_normals.inc"
};

u32 D_actor_503500_80158FB4[661] = {
#include "assets/actor_503500_model_27BE8_stream.inc"
};

TmdSource D_actor_503500_80159A08 = {
    0,
    2928,
    1820,
    9,
    D_actor_503500_80158BA0,
    D_actor_503500_80158BC4,
    D_actor_503500_80158D74,
    D_actor_503500_80158A5C,
    D_actor_503500_80158FB4,
};

TmdBone D_actor_503500_80159A2C[9] = {
#include "assets/actor_503500_model_28BB8_skeleton.inc"
};

u32 D_actor_503500_80159B70[9] = {
#include "assets/actor_503500_model_28BB8_partVerts.inc"
};

SVECTOR D_actor_503500_80159B94[54] = {
#include "assets/actor_503500_model_28BB8_verts.inc"
};

SVECTOR D_actor_503500_80159D44[72] = {
#include "assets/actor_503500_model_28BB8_normals.inc"
};

u32 D_actor_503500_80159F84[661] = {
#include "assets/actor_503500_model_28BB8_stream.inc"
};

TmdSource D_actor_503500_8015A9D8 = {
    0,
    2928,
    1820,
    9,
    D_actor_503500_80159B70,
    D_actor_503500_80159B94,
    D_actor_503500_80159D44,
    D_actor_503500_80159A2C,
    D_actor_503500_80159F84,
};

TmdBone D_actor_503500_8015A9FC[9] = {
#include "assets/actor_503500_model_29B78_skeleton.inc"
};

u32 D_actor_503500_8015AB40[9] = {
#include "assets/actor_503500_model_29B78_partVerts.inc"
};

SVECTOR D_actor_503500_8015AB64[54] = {
#include "assets/actor_503500_model_29B78_verts.inc"
};

SVECTOR D_actor_503500_8015AD14[70] = {
#include "assets/actor_503500_model_29B78_normals.inc"
};

u32 D_actor_503500_8015AF44[661] = {
#include "assets/actor_503500_model_29B78_stream.inc"
};

TmdSource D_actor_503500_8015B998 = {
    0,
    2928,
    1820,
    9,
    D_actor_503500_8015AB40,
    D_actor_503500_8015AB64,
    D_actor_503500_8015AD14,
    D_actor_503500_8015A9FC,
    D_actor_503500_8015AF44,
};

TmdBone D_actor_503500_8015B9BC[4] = {
#include "assets/actor_503500_model_2B938_skeleton.inc"
};

u32 D_actor_503500_8015BA4C[4] = {
#include "assets/actor_503500_model_2B938_partVerts.inc"
};

SVECTOR D_actor_503500_8015BA5C[129] = {
#include "assets/actor_503500_model_2B938_verts.inc"
};

SVECTOR D_actor_503500_8015BE64[129] = {
#include "assets/actor_503500_model_2B938_normals.inc"
};

u32 D_actor_503500_8015C26C[1339] = {
#include "assets/actor_503500_model_2B938_stream.inc"
};

TmdSource D_actor_503500_8015D758 = {
    0,
    8228,
    992,
    4,
    D_actor_503500_8015BA4C,
    D_actor_503500_8015BA5C,
    D_actor_503500_8015BE64,
    D_actor_503500_8015B9BC,
    D_actor_503500_8015C26C,
};

TmdBone D_actor_503500_8015D77C[4] = {
#include "assets/actor_503500_model_2D5DC_skeleton.inc"
};

u32 D_actor_503500_8015D80C[4] = {
#include "assets/actor_503500_model_2D5DC_partVerts.inc"
};

SVECTOR D_actor_503500_8015D81C[124] = {
#include "assets/actor_503500_model_2D5DC_verts.inc"
};

SVECTOR D_actor_503500_8015DBFC[124] = {
#include "assets/actor_503500_model_2D5DC_normals.inc"
};

u32 D_actor_503500_8015DFDC[1288] = {
#include "assets/actor_503500_model_2D5DC_stream.inc"
};

TmdSource D_actor_503500_8015F3FC = {
    0,
    7968,
    860,
    4,
    D_actor_503500_8015D80C,
    D_actor_503500_8015D81C,
    D_actor_503500_8015DBFC,
    D_actor_503500_8015D77C,
    D_actor_503500_8015DFDC,
};

AnimationPackedPose D_actor_503500_8015F420[3] = {
#include "assets/actor_503500_animation_2DB14_bank1.inc"
};

AnimationPackedRotation D_actor_503500_8015F444[99] = {
#include "assets/actor_503500_animation_2DB14_bank4.inc"
};

AnimationRecord D_actor_503500_8015F5D0[207] = {
#include "assets/actor_503500_animation_2DB14_records.inc"
};

u16 D_actor_503500_8015F90C[20] = {
#include "assets/actor_503500_animation_2DB14_indices.inc"
};

AnimationSet D_actor_503500_8015F934 = {
    D_actor_503500_8015F5D0,
    D_actor_503500_8015F90C,
    { NULL, D_actor_503500_8015F420, NULL, NULL, D_actor_503500_8015F444, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_8015F95C[15] = {
#include "assets/actor_503500_animation_2E4DC_bank1.inc"
};

AnimationPackedRotation D_actor_503500_8015FA10[230] = {
#include "assets/actor_503500_animation_2E4DC_bank4.inc"
};

AnimationRecord D_actor_503500_8015FDA8[331] = {
#include "assets/actor_503500_animation_2E4DC_records.inc"
};

u16 D_actor_503500_801602D4[20] = {
#include "assets/actor_503500_animation_2E4DC_indices.inc"
};

AnimationSet D_actor_503500_801602FC = {
    D_actor_503500_8015FDA8,
    D_actor_503500_801602D4,
    { NULL, D_actor_503500_8015F95C, NULL, NULL, D_actor_503500_8015FA10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80160324[15] = {
#include "assets/actor_503500_animation_2EF88_bank1.inc"
};

AnimationPackedRotation D_actor_503500_801603D8[247] = {
#include "assets/actor_503500_animation_2EF88_bank4.inc"
};

AnimationRecord D_actor_503500_801607B4[371] = {
#include "assets/actor_503500_animation_2EF88_records.inc"
};

u16 D_actor_503500_80160D80[20] = {
#include "assets/actor_503500_animation_2EF88_indices.inc"
};

AnimationSet D_actor_503500_80160DA8 = {
    D_actor_503500_801607B4,
    D_actor_503500_80160D80,
    { NULL, D_actor_503500_80160324, NULL, NULL, D_actor_503500_801603D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80160DD0[18] = {
#include "assets/actor_503500_animation_2FC70_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80160EA8[304] = {
#include "assets/actor_503500_animation_2FC70_bank4.inc"
};

AnimationRecord D_actor_503500_80161368[448] = {
#include "assets/actor_503500_animation_2FC70_records.inc"
};

u16 D_actor_503500_80161A68[20] = {
#include "assets/actor_503500_animation_2FC70_indices.inc"
};

AnimationSet D_actor_503500_80161A90 = {
    D_actor_503500_80161368,
    D_actor_503500_80161A68,
    { NULL, D_actor_503500_80160DD0, NULL, NULL, D_actor_503500_80160EA8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80161AB8[20] = {
#include "assets/actor_503500_animation_306E0_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80161BA8[181] = {
#include "assets/actor_503500_animation_306E0_bank4.inc"
};

AnimationRecord D_actor_503500_80161E7C[407] = {
#include "assets/actor_503500_animation_306E0_records.inc"
};

u16 D_actor_503500_801624D8[20] = {
#include "assets/actor_503500_animation_306E0_indices.inc"
};

AnimationSet D_actor_503500_80162500 = {
    D_actor_503500_80161E7C,
    D_actor_503500_801624D8,
    { NULL, D_actor_503500_80161AB8, NULL, NULL, D_actor_503500_80161BA8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80162528[16] = {
#include "assets/actor_503500_animation_30F1C_bank1.inc"
};

AnimationPackedRotation D_actor_503500_801625E8[166] = {
#include "assets/actor_503500_animation_30F1C_bank4.inc"
};

AnimationRecord D_actor_503500_80162880[293] = {
#include "assets/actor_503500_animation_30F1C_records.inc"
};

u16 D_actor_503500_80162D14[20] = {
#include "assets/actor_503500_animation_30F1C_indices.inc"
};

AnimationSet D_actor_503500_80162D3C = {
    D_actor_503500_80162880,
    D_actor_503500_80162D14,
    { NULL, D_actor_503500_80162528, NULL, NULL, D_actor_503500_801625E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80162D64[18] = {
#include "assets/actor_503500_animation_31788_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80162E3C[183] = {
#include "assets/actor_503500_animation_31788_bank4.inc"
};

AnimationRecord D_actor_503500_80163118[282] = {
#include "assets/actor_503500_animation_31788_records.inc"
};

u16 D_actor_503500_80163580[20] = {
#include "assets/actor_503500_animation_31788_indices.inc"
};

AnimationSet D_actor_503500_801635A8 = {
    D_actor_503500_80163118,
    D_actor_503500_80163580,
    { NULL, D_actor_503500_80162D64, NULL, NULL, D_actor_503500_80162E3C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_801635D0[12] = {
#include "assets/actor_503500_animation_31D8C_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80163660[131] = {
#include "assets/actor_503500_animation_31D8C_bank4.inc"
};

AnimationRecord D_actor_503500_8016386C[198] = {
#include "assets/actor_503500_animation_31D8C_records.inc"
};

u16 D_actor_503500_80163B84[20] = {
#include "assets/actor_503500_animation_31D8C_indices.inc"
};

AnimationSet D_actor_503500_80163BAC = {
    D_actor_503500_8016386C,
    D_actor_503500_80163B84,
    { NULL, D_actor_503500_801635D0, NULL, NULL, D_actor_503500_80163660, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80163BD4[43] = {
#include "assets/actor_503500_animation_32E24_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80163DD8[310] = {
#include "assets/actor_503500_animation_32E24_bank4.inc"
};

AnimationRecord D_actor_503500_801642B0[603] = {
#include "assets/actor_503500_animation_32E24_records.inc"
};

u16 D_actor_503500_80164C1C[20] = {
#include "assets/actor_503500_animation_32E24_indices.inc"
};

AnimationSet D_actor_503500_80164C44 = {
    D_actor_503500_801642B0,
    D_actor_503500_80164C1C,
    { NULL, D_actor_503500_80163BD4, NULL, NULL, D_actor_503500_80163DD8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80164C6C[14] = {
#include "assets/actor_503500_animation_333DC_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80164D14[101] = {
#include "assets/actor_503500_animation_333DC_bank4.inc"
};

AnimationRecord D_actor_503500_80164EA8[203] = {
#include "assets/actor_503500_animation_333DC_records.inc"
};

u16 D_actor_503500_801651D4[20] = {
#include "assets/actor_503500_animation_333DC_indices.inc"
};

AnimationSet D_actor_503500_801651FC = {
    D_actor_503500_80164EA8,
    D_actor_503500_801651D4,
    { NULL, D_actor_503500_80164C6C, NULL, NULL, D_actor_503500_80164D14, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80165224[21] = {
#include "assets/actor_503500_animation_33C14_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80165320[164] = {
#include "assets/actor_503500_animation_33C14_bank4.inc"
};

AnimationRecord D_actor_503500_801655B0[279] = {
#include "assets/actor_503500_animation_33C14_records.inc"
};

u16 D_actor_503500_80165A0C[20] = {
#include "assets/actor_503500_animation_33C14_indices.inc"
};

AnimationSet D_actor_503500_80165A34 = {
    D_actor_503500_801655B0,
    D_actor_503500_80165A0C,
    { NULL, D_actor_503500_80165224, NULL, NULL, D_actor_503500_80165320, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80165A5C[5] = {
#include "assets/actor_503500_animation_33EBC_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80165A98[30] = {
#include "assets/actor_503500_animation_33EBC_bank4.inc"
};

AnimationRecord D_actor_503500_80165B10[105] = {
#include "assets/actor_503500_animation_33EBC_records.inc"
};

u16 D_actor_503500_80165CB4[20] = {
#include "assets/actor_503500_animation_33EBC_indices.inc"
};

AnimationSet D_actor_503500_80165CDC = {
    D_actor_503500_80165B10,
    D_actor_503500_80165CB4,
    { NULL, D_actor_503500_80165A5C, NULL, NULL, D_actor_503500_80165A98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80165D04[5] = {
#include "assets/actor_503500_animation_341D8_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80165D40[37] = {
#include "assets/actor_503500_animation_341D8_bank4.inc"
};

AnimationRecord D_actor_503500_80165DD4[127] = {
#include "assets/actor_503500_animation_341D8_records.inc"
};

u16 D_actor_503500_80165FD0[20] = {
#include "assets/actor_503500_animation_341D8_indices.inc"
};

AnimationSet D_actor_503500_80165FF8 = {
    D_actor_503500_80165DD4,
    D_actor_503500_80165FD0,
    { NULL, D_actor_503500_80165D04, NULL, NULL, D_actor_503500_80165D40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80166020[48] = {
#include "assets/actor_503500_animation_350C8_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80166260[270] = {
#include "assets/actor_503500_animation_350C8_bank4.inc"
};

AnimationRecord D_actor_503500_80166698[522] = {
#include "assets/actor_503500_animation_350C8_records.inc"
};

u16 D_actor_503500_80166EC0[20] = {
#include "assets/actor_503500_animation_350C8_indices.inc"
};

AnimationSet D_actor_503500_80166EE8 = {
    D_actor_503500_80166698,
    D_actor_503500_80166EC0,
    { NULL, D_actor_503500_80166020, NULL, NULL, D_actor_503500_80166260, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80166F10[5] = {
#include "assets/actor_503500_animation_35390_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80166F4C[34] = {
#include "assets/actor_503500_animation_35390_bank4.inc"
};

AnimationRecord D_actor_503500_80166FD4[109] = {
#include "assets/actor_503500_animation_35390_records.inc"
};

u16 D_actor_503500_80167188[20] = {
#include "assets/actor_503500_animation_35390_indices.inc"
};

AnimationSet D_actor_503500_801671B0 = {
    D_actor_503500_80166FD4,
    D_actor_503500_80167188,
    { NULL, D_actor_503500_80166F10, NULL, NULL, D_actor_503500_80166F4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_801671D8[6] = {
#include "assets/actor_503500_animation_356DC_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80167220[43] = {
#include "assets/actor_503500_animation_356DC_bank4.inc"
};

AnimationRecord D_actor_503500_801672CC[130] = {
#include "assets/actor_503500_animation_356DC_records.inc"
};

u16 D_actor_503500_801674D4[20] = {
#include "assets/actor_503500_animation_356DC_indices.inc"
};

AnimationSet D_actor_503500_801674FC = {
    D_actor_503500_801672CC,
    D_actor_503500_801674D4,
    { NULL, D_actor_503500_801671D8, NULL, NULL, D_actor_503500_80167220, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_80167524[110] = {
#include "assets/actor_503500_animation_38AE0_bank1.inc"
};

AnimationPackedRotation D_actor_503500_80167A4C[1079] = {
#include "assets/actor_503500_animation_38AE0_bank4.inc"
};

AnimationRecord D_actor_503500_80168B28[1900] = {
#include "assets/actor_503500_animation_38AE0_records.inc"
};

u16 D_actor_503500_8016A8D8[20] = {
#include "assets/actor_503500_animation_38AE0_indices.inc"
};

AnimationSet D_actor_503500_8016A900 = {
    D_actor_503500_80168B28,
    D_actor_503500_8016A8D8,
    { NULL, D_actor_503500_80167524, NULL, NULL, D_actor_503500_80167A4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_8016A928[101] = {
#include "assets/actor_503500_animation_3A190_bank1.inc"
};

AnimationPackedRotation D_actor_503500_8016ADE4[476] = {
#include "assets/actor_503500_animation_3A190_bank4.inc"
};

AnimationRecord D_actor_503500_8016B554[653] = {
#include "assets/actor_503500_animation_3A190_records.inc"
};

u16 D_actor_503500_8016BF88[20] = {
#include "assets/actor_503500_animation_3A190_indices.inc"
};

AnimationSet D_actor_503500_8016BFB0 = {
    D_actor_503500_8016B554,
    D_actor_503500_8016BF88,
    { NULL, D_actor_503500_8016A928, NULL, NULL, D_actor_503500_8016ADE4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_503500_8016BFD8[151] = {
#include "assets/actor_503500_animation_3C968_bank1.inc"
};

AnimationPackedRotation D_actor_503500_8016C6EC[928] = {
#include "assets/actor_503500_animation_3C968_bank4.inc"
};

AnimationRecord D_actor_503500_8016D56C[1149] = {
#include "assets/actor_503500_animation_3C968_records.inc"
};

u16 D_actor_503500_8016E760[20] = {
#include "assets/actor_503500_animation_3C968_indices.inc"
};

AnimationSet D_actor_503500_8016E788 = {
    D_actor_503500_8016D56C,
    D_actor_503500_8016E760,
    { NULL, D_actor_503500_8016BFD8, NULL, NULL, D_actor_503500_8016C6EC, NULL, NULL, NULL },
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

GpPairSrcE D_actor_503500_8016E7EC[17] = {
    { D_actor_503500_8016E7E0, 3500, 300, 500, 200, 250, 3, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 1500, 100, 500, 0, 100, 0, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 500, 0, 0, 0, 100, 10, 100, 10, 0 },
    { D_actor_503500_8016E7E0, 500, 0, 0, 0, 100, 10, 100, 10, 0 },
    { D_actor_503500_8016E7E0, 700, 0, 0, 0, 100, 0, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 0, 0, 0, 100, 0, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 1500, 700, 2000, 0, 100, 0, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 800, 3000, 0, 100, 0, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 800, 3000, 0, 100, 0, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 1500, 200, 500, 0, 100, 0, 0, 0, 0 },
    { D_actor_503500_8016E7E8, 1000, 1000, 5000, 0, 100, 0, 0, 0, 0 },
    { D_actor_503500_8016E7E8, 1000, 1000, 5000, 0, 100, 0, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 100, 500, 0, 100, 0, 0, 0, 0 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10, 0 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10, 0 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10, 0 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10, 0 },
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
    { TASK_BODY_TMD, 96, func_actor_503500_80137238, { .model = &D_actor_503500_80154624 } },
    { TASK_BODY_TMD, 96, func_actor_503500_801384D4, { .model = &D_actor_503500_80154C38 } },
    { (TASK_BODY_TMD | 0x100), 96, func_actor_503500_8013AD0C, { .model = &D_actor_503500_80157A78 } },
    { (TASK_BODY_TMD | 0x100), 96, func_actor_503500_8013AD0C, { .model = &D_actor_503500_80156358 } },
    { TASK_BODY_COORD, 96, func_actor_503500_8013BE8C, { .model = NULL } },
    { TASK_BODY_COORD, 96, func_actor_503500_8013BE8C, { .model = NULL } },
    { TASK_BODY_COORD, 96, func_actor_503500_8013CA8C, { .model = NULL } },
    { TASK_BODY_COORD, 96, func_actor_503500_8013DBF4, { .model = NULL } },
    { TASK_BODY_COORD, 96, func_actor_503500_8013DBF4, { .model = NULL } },
    { TASK_BODY_COORD, 96, func_actor_503500_8013EC64, { .model = NULL } },
    { (TASK_BODY_TMD | 0x100), 96, func_actor_503500_801442A8, { .model = &D_actor_503500_8015D758 } },
    { (TASK_BODY_TMD | 0x100), 96, func_actor_503500_801442A8, { .model = &D_actor_503500_8015F3FC } },
    { TASK_BODY_COORD, 96, func_actor_503500_8013FA1C, { .model = NULL } },
    { (TASK_BODY_TMD | 0x100), 96, func_actor_503500_80142370, { .model = &D_actor_503500_80159A08 } },
    { (TASK_BODY_TMD | 0x100), 96, func_actor_503500_80142370, { .model = &D_actor_503500_80158A38 } },
    { (TASK_BODY_TMD | 0x100), 96, func_actor_503500_80142370, { .model = &D_actor_503500_8015B998 } },
    { (TASK_BODY_TMD | 0x100), 96, func_actor_503500_80142370, { .model = &D_actor_503500_8015A9D8 } },
};

TaskDesc D_actor_503500_8016E9F0[5] = {
    { TASK_BODY_COORD, 192, func_actor_503500_80144890, { .model = NULL } },
    { TASK_BODY_COORD, 192, func_actor_503500_80144E34, { .model = NULL } },
    { TASK_BODY_COORD, 192, func_actor_503500_8014554C, { .model = NULL } },
    { TASK_BODY_COORD, 192, func_actor_503500_801459D4, { .model = NULL } },
    { TASK_BODY_COORD, 192, func_actor_503500_80145F84, { .model = NULL } },
};

/// Player-facing flag byte in the main executable; no module header owns it yet.

static void func_actor_503500_80132F58(void);

void func_actor_503500_80132778(Task* task)
{
    GfxCoord*           coord;
    GpMtxWords*         rot;
    Actor503500EffWork* work;
    SVECTOR*            pos;
    u8                  done;

    coord = task->extra.coordBody->coord;
    if (task->state == 0) {
        pos                 = &D_actor_503500_8014B97C[task->spawnArg1.value];
        coord->coord.t[0]   = pos->vx;
        coord->coord.t[1]   = pos->vy;
        coord->coord.t[2]   = pos->vz;
        rot                 = (GpMtxWords*)&coord->coord;
        rot->m00_m01        = 0x1000;
        rot->m02_m10        = 0;
        rot->m11_m12        = 0x1000;
        rot->m20_m21        = 0;
        rot->m22            = 0x1000;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work                = memCalloc(0xC, false);
        if (work == NULL) {
            taskKill(task);
            return;
        }
        task->work      = work;
        work->field_0   = 0xC00;
        work->field_4   = 0x4000;
        work->field_8.w = 0x60000;
        task->state++;
    }
    work = (Actor503500EffWork*)task->work;
    if (Gp_StateF0.field_4 == 0) {
        if (work->field_8.h.hi < ++task->killCountdown) {
            task->killCountdown = 0;
            Gp_SpawnEff(0x6018C, coord,
                        (work->field_4 & 0xF000) | 0x03800000 | (work->field_0 & 0xFFF), NULL);
        }
    }
    switch (GameFlag_GetNibble(0x12A)) {
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
            work->field_4 -= 0x20;
            if (work->field_4 < 0x1000) {
                work->field_4 = 0x1000;
            }
            work->field_0 -= 0x10;
            if (work->field_0 < 0x100) {
                work->field_0 = 0x100;
            }
        case 4:
            work->field_8.w += 0x1000;
            done             = work->field_8.w > 0x100000;
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
    if (Gp_StateF0.field_4 == 0) {
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
    setDrawTPage(dr, 1, 0, getTPage(0, 2, 320, 0));
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
    Mc_SaveData[0].state.location.loc.area = 0x16;
    Mc_SaveData[0].state.location.loc.warp = 1;
    Mc_SaveData[0].state.location.loc.room = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

void func_actor_503500_80132C40(s32 arg0)
{
    Task_SpawnFromTable(D_actor_503500_8014B964, 0, arg0, 0);
}

void func_actor_503500_80132C70(s32 arg0)
{
    D_actor_503500_80176558 = Task_SpawnFromTable(D_actor_503500_8014B964, 1, arg0, 0);
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
    Gp_StateF0.prefix.bytes.field_1 = arg0;
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
    Gp_StateC08.field_6 |= 1;
}

void func_actor_503500_80132D7C(void)
{
    gGameSession->viewDirty = 1;
}

void func_actor_503500_80132D90(s32 arg0)
{
    GameFlag_SetNibble(0x100, arg0);
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

    slot3 = gameGetPtrSlot(3);
    coord = slot3->extra.tmd->coords;

    D_actor_503500_8017655C.pos.vx = coord->coord.t[0];
    D_actor_503500_8017655C.pos.vy = coord->coord.t[1];
    D_actor_503500_8017655C.pos.vz = coord->coord.t[2];

    /* Anchoring the rotation pointer *after* the three word stores is what
     * makes cse keep the plain symbol as the base address; taking it first
     * anchors the whole function on `D_actor_503500_8017655C + 0x10`. */
    rot = &D_actor_503500_8017655C.rot;

    rot->vx = ((GameActor*)slot3->work)->field_50;
    rot->vy = ((GameActor*)slot3->work)->field_52;
    rot->vz = ((GameActor*)slot3->work)->field_54;
}

void func_actor_503500_80132E7C(void)
{
    Task* slot3;

    slot3 = gameGetPtrSlot(3);
    if ((D_actor_503500_8017655C.pos.vx != 0) || (D_actor_503500_8017655C.pos.vy != 0) ||
        (D_actor_503500_8017655C.pos.vz != 0)) {
        Gp_DispatchMsgPtr(slot3, 0x3E9, &D_actor_503500_8017655C, 0);
    }
}

void func_actor_503500_80132EE8(u8 arg0)
{
    D_80115768 = arg0;
}

void func_actor_503500_80132EF4(void)
{
    func_80106350(gameGetPtrSlot(3), Player_Status.weapon, 0);
}

void func_actor_503500_80132F28(void)
{
    Gp_HaltPadScripts();
    gGameSession->padScriptFlags = 0;
}

static void func_actor_503500_80132F58(void)
{
    D_actor_503500_80176558 = NULL;
}
