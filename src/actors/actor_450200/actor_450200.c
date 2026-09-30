#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/neo_ark_observatory.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[4];
        AnimationPlayRequest arguments[6];
    } data;
    s32 words[34];
} Actor450200AnimStorageFB4C;
STATIC_ASSERT_SIZEOF(Actor450200AnimStorageFB4C, 136);

extern Actor450200AnimStorageFB4C D_actor_450200_8013FB4C;

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[5];
        AnimationPlayRequest arguments[6];
    } data;
    s32 words[35];
} Actor450200AnimStorageC66C;
STATIC_ASSERT_SIZEOF(Actor450200AnimStorageC66C, 140);

extern Actor450200AnimStorageC66C D_actor_450200_8013C66C;

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[8];
        GpCopyArg            copy;
        AnimationPlayRequest arguments[5];
    } data;
    s32 words[35];
} Actor450200AnimStorage7A84;
STATIC_ASSERT_SIZEOF(Actor450200AnimStorage7A84, 140);

extern Actor450200AnimStorage7A84 D_actor_450200_80137A84;

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[8];
        GpCopyArg            copy;
        AnimationPlayRequest arguments[5];
    } data;
    s32 words[35];
} Actor450200AnimStorage7BD8;
STATIC_ASSERT_SIZEOF(Actor450200AnimStorage7BD8, 140);

extern Actor450200AnimStorage7BD8 D_actor_450200_80137BD8;

extern u8       D_actor_450200_8013885C[];
extern SVECTOR  D_actor_450200_80138868;
extern GpEvsCmd D_actor_450200_80138870[];
extern GpEvsCmd D_actor_450200_80138A68[];
extern GpEvsCmd D_actor_450200_80138C60[];
extern GpEvsCmd D_actor_450200_80138E88[];
extern GpEvsCmd D_actor_450200_80139098[];
extern TaskDesc D_actor_450200_80137A60[];
extern TaskDesc D_actor_450200_8013FB40;

/// Placement payload that four of the overlay's data records pair with message
/// 0x3EE. Only its yaw changes, set by `func_actor_450200_8013219C`.
extern ActorTransform D_actor_450200_80137DC4;
extern Task*          D_actor_450200_801401E0;
extern Task*          D_actor_450200_801401E4;
extern u16            D_actor_450200_801401E8[256];
extern u16            D_actor_450200_801403E8[256];
extern u16            D_actor_450200_801405E8[256];
extern u16            D_actor_450200_801407E8[256];

void func_actor_450200_80132848(s32);
void func_actor_450200_80132880(s32);
void func_actor_450200_801328A0(u8);

extern Actor450200AnimStorage7BD8 D_actor_450200_80137BD8;
extern AnimationPlayRequest       D_actor_450200_80137B38;
extern AnimationPlayRequest       D_actor_450200_80137B4C;
extern AnimationPlayRequest       D_actor_450200_80137B60;
extern AnimationPlayRequest       D_actor_450200_80137B74;
extern AnimationSet               D_actor_450200_8013389C;
extern AnimationSet               D_actor_450200_80134BEC;
extern AnimationSet               D_actor_450200_80134FC8;
extern AnimationSet               D_actor_450200_801351FC;
extern AnimationSet               D_actor_450200_80135638;
extern AnimationSet               D_actor_450200_801357D8;
extern AnimationSet               D_actor_450200_80135A50;
extern AnimationSet               D_actor_450200_80135C24;
void                              func_actor_450200_801320D4(s32);
void                              func_actor_450200_8013215C(void);
void                              func_actor_450200_8013217C(s32);
void                              func_actor_450200_8013219C(void);
void                              func_actor_450200_80132538(Task*);

extern Actor450200AnimStorage7A84 D_actor_450200_80137A84;
void                              func_actor_450200_80131E24(Task*);
void                              func_actor_450200_80131FA8(Task*);

AnimationPackedPose D_actor_450200_801328BC[62] = {
#include "assets/actor_450200_animation_01A7C_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80132BA4[225] = {
#include "assets/actor_450200_animation_01A7C_bank4.inc"
};

AnimationRecord D_actor_450200_80132F28[595] = {
#include "assets/actor_450200_animation_01A7C_records.inc"
};

u16 D_actor_450200_80133874[20] = {
#include "assets/actor_450200_animation_01A7C_indices.inc"
};

AnimationSet D_actor_450200_8013389C = {
    D_actor_450200_80132F28,
    D_actor_450200_80133874,
    { NULL, D_actor_450200_801328BC, NULL, NULL, D_actor_450200_80132BA4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_801338C4[43] = {
#include "assets/actor_450200_animation_02DCC_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80133AC8[468] = {
#include "assets/actor_450200_animation_02DCC_bank4.inc"
};

AnimationRecord D_actor_450200_80134218[619] = {
#include "assets/actor_450200_animation_02DCC_records.inc"
};

u16 D_actor_450200_80134BC4[20] = {
#include "assets/actor_450200_animation_02DCC_indices.inc"
};

AnimationSet D_actor_450200_80134BEC = {
    D_actor_450200_80134218,
    D_actor_450200_80134BC4,
    { NULL, D_actor_450200_801338C4, NULL, NULL, D_actor_450200_80133AC8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80134C14[2] = {
#include "assets/actor_450200_animation_031A8_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80134C2C[91] = {
#include "assets/actor_450200_animation_031A8_bank4.inc"
};

AnimationRecord D_actor_450200_80134D98[130] = {
#include "assets/actor_450200_animation_031A8_records.inc"
};

u16 D_actor_450200_80134FA0[20] = {
#include "assets/actor_450200_animation_031A8_indices.inc"
};

AnimationSet D_actor_450200_80134FC8 = {
    D_actor_450200_80134D98,
    D_actor_450200_80134FA0,
    { NULL, D_actor_450200_80134C14, NULL, NULL, D_actor_450200_80134C2C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80134FF0[2] = {
#include "assets/actor_450200_animation_033DC_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80135008[33] = {
#include "assets/actor_450200_animation_033DC_bank4.inc"
};

AnimationRecord D_actor_450200_8013508C[82] = {
#include "assets/actor_450200_animation_033DC_records.inc"
};

u16 D_actor_450200_801351D4[20] = {
#include "assets/actor_450200_animation_033DC_indices.inc"
};

AnimationSet D_actor_450200_801351FC = {
    D_actor_450200_8013508C,
    D_actor_450200_801351D4,
    { NULL, D_actor_450200_80134FF0, NULL, NULL, D_actor_450200_80135008, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80135224[2] = {
#include "assets/actor_450200_animation_03818_bank1.inc"
};

AnimationPackedRotation D_actor_450200_8013523C[104] = {
#include "assets/actor_450200_animation_03818_bank4.inc"
};

AnimationRecord D_actor_450200_801353DC[141] = {
#include "assets/actor_450200_animation_03818_records.inc"
};

u16 D_actor_450200_80135610[20] = {
#include "assets/actor_450200_animation_03818_indices.inc"
};

AnimationSet D_actor_450200_80135638 = {
    D_actor_450200_801353DC,
    D_actor_450200_80135610,
    { NULL, D_actor_450200_80135224, NULL, NULL, D_actor_450200_8013523C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80135660[2] = {
#include "assets/actor_450200_animation_039B8_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80135678[18] = {
#include "assets/actor_450200_animation_039B8_bank4.inc"
};

AnimationRecord D_actor_450200_801356C0[60] = {
#include "assets/actor_450200_animation_039B8_records.inc"
};

u16 D_actor_450200_801357B0[20] = {
#include "assets/actor_450200_animation_039B8_indices.inc"
};

AnimationSet D_actor_450200_801357D8 = {
    D_actor_450200_801356C0,
    D_actor_450200_801357B0,
    { NULL, D_actor_450200_80135660, NULL, NULL, D_actor_450200_80135678, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80135800[2] = {
#include "assets/actor_450200_animation_03C30_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80135818[28] = {
#include "assets/actor_450200_animation_03C30_bank4.inc"
};

AnimationRecord D_actor_450200_80135888[104] = {
#include "assets/actor_450200_animation_03C30_records.inc"
};

u16 D_actor_450200_80135A28[20] = {
#include "assets/actor_450200_animation_03C30_indices.inc"
};

AnimationSet D_actor_450200_80135A50 = {
    D_actor_450200_80135888,
    D_actor_450200_80135A28,
    { NULL, D_actor_450200_80135800, NULL, NULL, D_actor_450200_80135818, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80135A78[2] = {
#include "assets/actor_450200_animation_03E04_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80135A90[23] = {
#include "assets/actor_450200_animation_03E04_bank4.inc"
};

AnimationRecord D_actor_450200_80135AEC[68] = {
#include "assets/actor_450200_animation_03E04_records.inc"
};

u16 D_actor_450200_80135BFC[20] = {
#include "assets/actor_450200_animation_03E04_indices.inc"
};

AnimationSet D_actor_450200_80135C24 = {
    D_actor_450200_80135AEC,
    D_actor_450200_80135BFC,
    { NULL, D_actor_450200_80135A78, NULL, NULL, D_actor_450200_80135A90, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80135C4C[8] = {
#include "assets/actor_450200_animation_04168_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80135CAC[68] = {
#include "assets/actor_450200_animation_04168_bank4.inc"
};

AnimationRecord D_actor_450200_80135DBC[105] = {
#include "assets/actor_450200_animation_04168_records.inc"
};

u16 D_actor_450200_80135F60[20] = {
#include "assets/actor_450200_animation_04168_indices.inc"
};

AnimationSet D_actor_450200_80135F88 = {
    D_actor_450200_80135DBC,
    D_actor_450200_80135F60,
    { NULL, D_actor_450200_80135C4C, NULL, NULL, D_actor_450200_80135CAC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80135FB0[2] = {
#include "assets/actor_450200_animation_043F0_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80135FC8[51] = {
#include "assets/actor_450200_animation_043F0_bank4.inc"
};

AnimationRecord D_actor_450200_80136094[85] = {
#include "assets/actor_450200_animation_043F0_records.inc"
};

u16 D_actor_450200_801361E8[20] = {
#include "assets/actor_450200_animation_043F0_indices.inc"
};

AnimationSet D_actor_450200_80136210 = {
    D_actor_450200_80136094,
    D_actor_450200_801361E8,
    { NULL, D_actor_450200_80135FB0, NULL, NULL, D_actor_450200_80135FC8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80136238[10] = {
#include "assets/actor_450200_animation_04890_bank1.inc"
};

AnimationPackedRotation D_actor_450200_801362B0[96] = {
#include "assets/actor_450200_animation_04890_bank4.inc"
};

AnimationRecord D_actor_450200_80136430[150] = {
#include "assets/actor_450200_animation_04890_records.inc"
};

u16 D_actor_450200_80136688[20] = {
#include "assets/actor_450200_animation_04890_indices.inc"
};

AnimationSet D_actor_450200_801366B0 = {
    D_actor_450200_80136430,
    D_actor_450200_80136688,
    { NULL, D_actor_450200_80136238, NULL, NULL, D_actor_450200_801362B0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_801366D8[6] = {
#include "assets/actor_450200_animation_04DB0_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80136720[124] = {
#include "assets/actor_450200_animation_04DB0_bank4.inc"
};

AnimationRecord D_actor_450200_80136910[166] = {
#include "assets/actor_450200_animation_04DB0_records.inc"
};

u16 D_actor_450200_80136BA8[20] = {
#include "assets/actor_450200_animation_04DB0_indices.inc"
};

AnimationSet D_actor_450200_80136BD0 = {
    D_actor_450200_80136910,
    D_actor_450200_80136BA8,
    { NULL, D_actor_450200_801366D8, NULL, NULL, D_actor_450200_80136720, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80136BF8[2] = {
#include "assets/actor_450200_animation_05088_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80136C10[43] = {
#include "assets/actor_450200_animation_05088_bank4.inc"
};

AnimationRecord D_actor_450200_80136CBC[113] = {
#include "assets/actor_450200_animation_05088_records.inc"
};

u16 D_actor_450200_80136E80[20] = {
#include "assets/actor_450200_animation_05088_indices.inc"
};

AnimationSet D_actor_450200_80136EA8 = {
    D_actor_450200_80136CBC,
    D_actor_450200_80136E80,
    { NULL, D_actor_450200_80136BF8, NULL, NULL, D_actor_450200_80136C10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80136ED0[3] = {
#include "assets/actor_450200_animation_05350_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80136EF4[57] = {
#include "assets/actor_450200_animation_05350_bank4.inc"
};

AnimationRecord D_actor_450200_80136FD8[92] = {
#include "assets/actor_450200_animation_05350_records.inc"
};

u16 D_actor_450200_80137148[20] = {
#include "assets/actor_450200_animation_05350_indices.inc"
};

AnimationSet D_actor_450200_80137170 = {
    D_actor_450200_80136FD8,
    D_actor_450200_80137148,
    { NULL, D_actor_450200_80136ED0, NULL, NULL, D_actor_450200_80136EF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_80137198[6] = {
#include "assets/actor_450200_animation_05870_bank1.inc"
};

AnimationPackedRotation D_actor_450200_801371E0[124] = {
#include "assets/actor_450200_animation_05870_bank4.inc"
};

AnimationRecord D_actor_450200_801373D0[166] = {
#include "assets/actor_450200_animation_05870_records.inc"
};

u16 D_actor_450200_80137668[20] = {
#include "assets/actor_450200_animation_05870_indices.inc"
};

AnimationSet D_actor_450200_80137690 = {
    D_actor_450200_801373D0,
    D_actor_450200_80137668,
    { NULL, D_actor_450200_80137198, NULL, NULL, D_actor_450200_801371E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_801376B8[7] = {
#include "assets/actor_450200_animation_05C18_bank1.inc"
};

AnimationPackedRotation D_actor_450200_8013770C[81] = {
#include "assets/actor_450200_animation_05C18_bank4.inc"
};

AnimationRecord D_actor_450200_80137850[112] = {
#include "assets/actor_450200_animation_05C18_records.inc"
};

u16 D_actor_450200_80137A10[20] = {
#include "assets/actor_450200_animation_05C18_indices.inc"
};

AnimationSet D_actor_450200_80137A38 = {
    D_actor_450200_80137850,
    D_actor_450200_80137A10,
    { NULL, D_actor_450200_801376B8, NULL, NULL, D_actor_450200_8013770C, NULL, NULL, NULL },
};

TaskDesc D_actor_450200_80137A60[3] = {
    { 0, 192, taskKill, { .model = NULL } },
    { 0, 32, func_actor_450200_80131E24, { .model = NULL } },
    { 0, 97, func_actor_450200_80131FA8, { .model = NULL } },
};

Actor450200AnimStorage7A84 D_actor_450200_80137A84 = { .data = { { &D_actor_450200_80135F88, &D_actor_450200_80136210, &D_actor_450200_801366B0, &D_actor_450200_80136BD0, &D_actor_450200_80136EA8, &D_actor_450200_80137170, &D_actor_450200_80137690, &D_actor_450200_80137A38 }, { { .words = D_actor_450200_80137A84.words }, 32 }, { { { .index = 1 }, 47, 1, 5, 1 }, { { .index = 1 }, 47, 0, 0, 1 }, { { .index = 1 }, 47, 0, 0, 1 }, { { .index = 1 }, 47, 0, 0, 1 }, { { .index = 1 }, 47, 0, 0, 1 } } } };

AnimationPlayRequest D_actor_450200_80137B10[2] = {
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_actor_450200_80137B38 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137B4C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137B60 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137B74 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137B88 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137B9C = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137BB0 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137BC4 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

Actor450200AnimStorage7BD8 D_actor_450200_80137BD8 = { .data = { { &D_actor_450200_8013389C, &D_actor_450200_80134BEC, &D_actor_450200_80134FC8, &D_actor_450200_801351FC, &D_actor_450200_80135638, &D_actor_450200_801357D8, &D_actor_450200_80135A50, &D_actor_450200_80135C24 }, { { .words = D_actor_450200_80137BD8.words }, 32 }, { { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 48, 1, 8, 0 }, { { .index = 1 }, 49, 0, 0, 0 }, { { .index = 1 }, 50, 0, 0, 0 }, { { .index = 1 }, 51, 0, 0, 0 } } } };

AnimationPlayRequest D_actor_450200_80137C64 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450200_80137C78 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450200_80137C8C = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450200_80137CA0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137CB4 = { { .index = 1 }, 32, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137CC8 = { { .index = 1 }, 33, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

GpScriptCmd D_actor_450200_80137CDC[2] = {
    { 257, 1 },
    { 0, 0 },
};

GpScriptRec D_actor_450200_80137CE4[2] = {
    { 253, 67, 12, 1 },
    { 0, 0, 13, 0 },
};

ActorTransform D_actor_450200_80137CEC = { { 0x36B0, 0, 0x2D50, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D04 = { { 8000, 0, 0x2D50, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D1C = { { 0x2904, 0, 0x2FA8, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D34 = { { 8000, 0, 0x2FA8, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D4C = { { 8000, 0, 9000, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D64 = { { 0x2AF8, 0, 0x3070, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D7C = { { 0x2AF8, 0, 0x2C24, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_450200_80137D94 = { { 9750, 0, 0x2C24, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_450200_80137DAC = { { 8000, 0, 9700, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_actor_450200_80137DC4 = { { 0, 0, 0, 0 }, { 0, -1024, 0, 0 } };

GpEvsCmd D_actor_450200_80137DDC[4] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BC4 }, { .value = 0 } },
    { 4, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137B9C }, { .value = 0 } },
    { 45, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450200_80137E3C[7] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137B38 }, { .value = 0 } },
    { 4, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137B4C }, { .value = 0 } },
    { 4, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1021 }, { .value = 30 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137B60 }, { .value = 0 } },
    { 45, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450200_80137EE4[82] = {
    { 35, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450200_80137A84.data.copy }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450200_80137BD8.data.copy }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CA0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CA0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_80137CEC }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_80137D64 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_450200_80137BD8.data.arguments }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_450200_80137D04 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_801320D4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.arguments[1] }, { .value = 0 } },
    { 15, { .value = 0x55070008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x4066000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 14, { .padCommands = D_actor_450200_80137CDC }, { .padRecords = D_actor_450200_80137CE4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_450200_80137A84.data.arguments }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55070007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_450200_8013215C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.arguments[2] }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_80137D1C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137B74 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_450200_8013215C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_801320D4 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_450200_80137DDC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_450200_80137E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_450200_80137DDC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_450200_80137E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_450200_80137DDC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_450200_80137E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BB0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CA0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C64 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_80137D7C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_450200_80137D34 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_450200_80137D4C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.arguments[4] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_801320D4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_450200_8013215C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_80137D94 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_80137DAC }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CA0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_neo_ark_observatory_8017FA98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450200_80138694[19] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_801320D4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_450200_8013215C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_80137D94 }, { .value = 0 } },
    { 39, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_80137DAC }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CA0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_neo_ark_observatory_8017FA98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

u8 D_actor_450200_8013885C[12] = {
    1,
    3,
    5,
    6,
    9,
    14,
    15,
    16,
    17,
    18,
    19,
    0,
};

SVECTOR D_actor_450200_80138868 = { 0, -128, 0, 0 };

GpEvsCmd D_actor_450200_80138870[21] = {
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450200_80137BD8.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_450200_8013219C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_450200_80137DC4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_8013217C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CB4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CC8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_8013217C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450200_80138A68[21] = {
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450200_80137BD8.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_450200_8013219C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_450200_80137DC4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_8013217C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CB4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CC8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_8013217C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450200_80138C60[23] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_450200_8013219C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_450200_80137DC4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_8013217C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CB4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CC8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_8013217C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450200_80138E88[22] = {
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450200_80137BD8.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_450200_8013219C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_450200_80137DC4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_8013217C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CB4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CC8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_8013217C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450200_80139098[8] = {
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_80137D94 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450200_80137BD8.data.copy }, { .value = 0 } },
    { 39, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_neo_ark_observatory_8017FA98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

AnimationPackedPose D_actor_450200_80139158[61] = {
#include "assets/actor_450200_animation_0865C_bank1.inc"
};

AnimationPackedRotation D_actor_450200_80139434[363] = {
#include "assets/actor_450200_animation_0865C_bank4.inc"
};

AnimationRecord D_actor_450200_801399E0[669] = {
#include "assets/actor_450200_animation_0865C_records.inc"
};

u16 D_actor_450200_8013A454[20] = {
#include "assets/actor_450200_animation_0865C_indices.inc"
};

AnimationSet D_actor_450200_8013A47C = {
    D_actor_450200_801399E0,
    D_actor_450200_8013A454,
    { NULL, D_actor_450200_80139158, NULL, NULL, D_actor_450200_80139434, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_8013A4A4[12] = {
#include "assets/actor_450200_animation_08B88_bank1.inc"
};

AnimationPackedRotation D_actor_450200_8013A534[116] = {
#include "assets/actor_450200_animation_08B88_bank4.inc"
};

AnimationRecord D_actor_450200_8013A704[159] = {
#include "assets/actor_450200_animation_08B88_records.inc"
};

u16 D_actor_450200_8013A980[20] = {
#include "assets/actor_450200_animation_08B88_indices.inc"
};

AnimationSet D_actor_450200_8013A9A8 = {
    D_actor_450200_8013A704,
    D_actor_450200_8013A980,
    { NULL, D_actor_450200_8013A4A4, NULL, NULL, D_actor_450200_8013A534, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_8013A9D0[2] = {
#include "assets/actor_450200_animation_08E94_bank1.inc"
};

AnimationPackedRotation D_actor_450200_8013A9E8[46] = {
#include "assets/actor_450200_animation_08E94_bank4.inc"
};

AnimationRecord D_actor_450200_8013AAA0[123] = {
#include "assets/actor_450200_animation_08E94_records.inc"
};

u16 D_actor_450200_8013AC8C[20] = {
#include "assets/actor_450200_animation_08E94_indices.inc"
};

AnimationSet D_actor_450200_8013ACB4 = {
    D_actor_450200_8013AAA0,
    D_actor_450200_8013AC8C,
    { NULL, D_actor_450200_8013A9D0, NULL, NULL, D_actor_450200_8013A9E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_8013ACDC[34] = {
#include "assets/actor_450200_animation_09E70_bank1.inc"
};

AnimationPackedRotation D_actor_450200_8013AE74[398] = {
#include "assets/actor_450200_animation_09E70_bank4.inc"
};

AnimationRecord D_actor_450200_8013B4AC[495] = {
#include "assets/actor_450200_animation_09E70_records.inc"
};

u16 D_actor_450200_8013BC68[20] = {
#include "assets/actor_450200_animation_09E70_indices.inc"
};

AnimationSet D_actor_450200_8013BC90 = {
    D_actor_450200_8013B4AC,
    D_actor_450200_8013BC68,
    { NULL, D_actor_450200_8013ACDC, NULL, NULL, D_actor_450200_8013AE74, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_8013BCB8[21] = {
#include "assets/actor_450200_animation_0A824_bank1.inc"
};

AnimationPackedRotation D_actor_450200_8013BDB4[243] = {
#include "assets/actor_450200_animation_0A824_bank4.inc"
};

AnimationRecord D_actor_450200_8013C180[295] = {
#include "assets/actor_450200_animation_0A824_records.inc"
};

u16 D_actor_450200_8013C61C[20] = {
#include "assets/actor_450200_animation_0A824_indices.inc"
};

AnimationSet D_actor_450200_8013C644 = {
    D_actor_450200_8013C180,
    D_actor_450200_8013C61C,
    { NULL, D_actor_450200_8013BCB8, NULL, NULL, D_actor_450200_8013BDB4, NULL, NULL, NULL },
};

Actor450200AnimStorageC66C D_actor_450200_8013C66C = { .data = { { &D_actor_450200_8013A47C, &D_actor_450200_8013A9A8, &D_actor_450200_8013ACB4, &D_actor_450200_8013BC90, &D_actor_450200_8013C644 }, { { { .index = 1 }, 1, 0, 0, 0 }, { { .index = 1 }, 1, 1, 15, 0 }, { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 48, 0, 0, 0 }, { { .index = 1 }, 49, 1, 10, 0 }, { { .index = 1 }, 50, 1, 10, 0 } } } };

AnimationPlayRequest D_actor_450200_8013C6F8 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

GpCopyArg D_actor_450200_8013C70C = { { .words = D_actor_450200_8013C66C.words }, 32 };

ActorTransform D_actor_450200_8013C714 = { { 0x2710, 0, 0x2EE0, 0 }, { 0, -1024, 0, 0 } };

GpEvsCmd D_actor_450200_8013C72C[40] = {
    { 35, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450200_8013C70C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_8013C714 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55070001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.arguments[2] }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.arguments[3] }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.arguments[4] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.arguments[5] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C6F8 }, { .value = 0 } },
    { 4, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.arguments[4] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55070002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_8013C714 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_450200_8013C66C.data.arguments }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450200_8013CAEC[12] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_8013C714 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_450200_8013C66C.data.arguments }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

AnimationPackedPose D_actor_450200_8013CC0C[6] = {
#include "assets/actor_450200_animation_0B190_bank1.inc"
};

AnimationPackedRotation D_actor_450200_8013CC54[64] = {
#include "assets/actor_450200_animation_0B190_bank4.inc"
};

AnimationRecord D_actor_450200_8013CD54[141] = {
#include "assets/actor_450200_animation_0B190_records.inc"
};

u16 D_actor_450200_8013CF88[20] = {
#include "assets/actor_450200_animation_0B190_indices.inc"
};

AnimationSet D_actor_450200_8013CFB0 = {
    D_actor_450200_8013CD54,
    D_actor_450200_8013CF88,
    { NULL, D_actor_450200_8013CC0C, NULL, NULL, D_actor_450200_8013CC54, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_8013CFD8[84] = {
#include "assets/actor_450200_animation_0D678_bank1.inc"
};

AnimationPackedRotation D_actor_450200_8013D3C8[907] = {
#include "assets/actor_450200_animation_0D678_bank4.inc"
};

AnimationRecord D_actor_450200_8013E1F4[1183] = {
#include "assets/actor_450200_animation_0D678_records.inc"
};

u16 D_actor_450200_8013F470[20] = {
#include "assets/actor_450200_animation_0D678_indices.inc"
};

AnimationSet D_actor_450200_8013F498 = {
    D_actor_450200_8013E1F4,
    D_actor_450200_8013F470,
    { NULL, D_actor_450200_8013CFD8, NULL, NULL, D_actor_450200_8013D3C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_8013F4C0[5] = {
#include "assets/actor_450200_animation_0D9E0_bank1.inc"
};

AnimationPackedRotation D_actor_450200_8013F4FC[74] = {
#include "assets/actor_450200_animation_0D9E0_bank4.inc"
};

AnimationRecord D_actor_450200_8013F624[109] = {
#include "assets/actor_450200_animation_0D9E0_records.inc"
};

u16 D_actor_450200_8013F7D8[20] = {
#include "assets/actor_450200_animation_0D9E0_indices.inc"
};

AnimationSet D_actor_450200_8013F800 = {
    D_actor_450200_8013F624,
    D_actor_450200_8013F7D8,
    { NULL, D_actor_450200_8013F4C0, NULL, NULL, D_actor_450200_8013F4FC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450200_8013F828[5] = {
#include "assets/actor_450200_animation_0DCF8_bank1.inc"
};

AnimationPackedRotation D_actor_450200_8013F864[66] = {
#include "assets/actor_450200_animation_0DCF8_bank4.inc"
};

AnimationRecord D_actor_450200_8013F96C[97] = {
#include "assets/actor_450200_animation_0DCF8_records.inc"
};

u16 D_actor_450200_8013FAF0[20] = {
#include "assets/actor_450200_animation_0DCF8_indices.inc"
};

AnimationSet D_actor_450200_8013FB18 = {
    D_actor_450200_8013F96C,
    D_actor_450200_8013FAF0,
    { NULL, D_actor_450200_8013F828, NULL, NULL, D_actor_450200_8013F864, NULL, NULL, NULL },
};

TaskDesc D_actor_450200_8013FB40 = { 0, 32, func_actor_450200_80132538, { .model = NULL } };

Actor450200AnimStorageFB4C D_actor_450200_8013FB4C = { .data = { { &D_actor_450200_8013CFB0, &D_actor_450200_8013F498, &D_actor_450200_8013F800, &D_actor_450200_8013FB18 }, { { { .index = 1 }, 1, 0, 0, 0 }, { { .index = 1 }, 1, 1, 15, 0 }, { { .index = 1 }, 1, 1, 30, 0 }, { { .index = 1 }, 47, 1, 15, 0 }, { { .index = 1 }, 48, 0, 0, 0 }, { { .index = 1 }, 49, 1, 8, 0 } } } };

AnimationPlayRequest D_actor_450200_8013FBD4 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

GpCopyArg D_actor_450200_8013FBE8 = { { .words = D_actor_450200_8013FB4C.words }, 32 };

GpOverrideArg D_actor_450200_8013FBF0 = { 19, 1 };

ActorTransform D_actor_450200_8013FBF8 = { { 6271, 0, 3306, 0 }, { 0, -682, 0, 0 } };

ActorTransform D_actor_450200_8013FC10 = { { 3096, 0, 4774, 0 }, { 0, -1080, 0, 0 } };

ActorTransform D_actor_450200_8013FC28 = { { 880, 0, 6890, 0 }, { 0, 3015, 0, 0 } };

ActorTransform D_actor_450200_8013FC40 = { { 880, 0, 6890, 0 }, { 0, 967, 0, 0 } };

GpEvsCmd D_actor_450200_8013FC58[44] = {
    { 35, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450200_8013FBE8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_8013FBF8 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_450200_8013FC10 }, { .storage = &D_actor_450200_8013FBF0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55070004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.arguments[3] }, { .value = 0 } },
    { 4, { .value = 68 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.arguments[1] }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_450200_8013FC28 }, { .storage = &D_actor_450200_8013FBF0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_450200_8013FC28 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.arguments[5] }, { .value = 0 } },
    { 4, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FBD4 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55070003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_80132848 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.arguments[4] }, { .value = 0 } },
    { 4, { .value = 160 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_450200_8013FC40 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_actor_450200_801328A0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450200_80140078[15] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_actor_450200_801328A0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450200_80132880 }, { .value = 160 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450200_8013FC40 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.arguments[0] }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

Task* D_actor_450200_801401E0;

Task* D_actor_450200_801401E4;

u16 D_actor_450200_801401E8[256];

u16 D_actor_450200_801403E8[256];

u16 D_actor_450200_801405E8[256];

u16 D_actor_450200_801407E8[256];

static void        func_actor_450200_80132220(void);
static void        func_actor_450200_801322F8(void);
static void        func_actor_450200_80132368(s32 x, s32 tpageX, s32 clutY, s32 semiTrans, s32 rgb, s32 shadeTex);
static inline void _actor450200LoadScaledClut(u16* src, u16* dst, s32 scale, s32 y);

/// Effect state machine of this actor's first sub-task: state 0 arms the
/// self-destruct countdown at 0x64 and state 2 re-arms it at 0x80, both then
/// stepping the state on; state 1 throws effect 0x60080 on every other frame,
/// state 3 splits into an odd branch that bursts 0x60080 with the countdown
/// scaled into the spawn argument while it is still positive and an even
/// branch that spawns a 0x60070 only every eighth frame -- the other two bits
/// of the odd/even split the two effects see. The part the effects hang off is
/// picked at random from the model's coordinate array: the 11-entry byte table
/// holds indices into it, which is why the load is unsigned and the stride is
/// `GfxCoord`.
void func_actor_450200_80131E24(Task* task)
{
    Task*     slot;
    GfxCoord* coord;
    s16       countdown;

    slot  = gameGetPtrSlot(0xA);
    coord = &slot->extra.tmd->coords[D_actor_450200_8013885C[(rand() * 11) >> 15]];
    switch (task->state) {
        case 0:
            task->killCountdown = 0x64;
            task->state++;
            return;
        case 1:
            countdown           = (u16)task->killCountdown - 1;
            task->killCountdown = countdown;
            if ((countdown & 1) == 0) {
                Gp_SpawnEff(0x60080, coord, 0x80000300, NULL);
            }
            return;
        case 2:
            task->killCountdown = 0x80;
            task->state++;
            return;
        case 3:
            countdown           = (u16)task->killCountdown - 1;
            task->killCountdown = countdown;
            if (countdown & 1) {
                if (countdown > 0) {
                    Gp_SpawnEff(0x60080, coord, countdown * 2 + 0x80000080,
                                &D_actor_450200_80138868);
                }
            } else if (countdown >= -0x1F && (countdown & 7) == 0) {
                Gp_SpawnEff(0x60070, coord, 0xF0010100, &D_actor_450200_80138868);
            }
            return;
    }
}

/// Head-aim state of this actor's second sub-task: state 0 allocates the
/// `GpHeadAim` record into `Task::work` and seeds both clamps to
/// 0x100, state 1 ramps its `rate` up toward 0x1000 while `Task::spawnArg1` is
/// set and back down toward 0 while it is not, then hands the record to
/// `func_800B17D4` between the slot-3 task whose head turns and the
/// `gameGetPtrSlot(0xA)` task it turns toward. A failed allocation, and every
/// state past 1, kill the task; only the latter clears
/// `D_actor_450200_801401E0`, which is why the two `taskKill` calls are
/// distinct.
void func_actor_450200_80131FA8(Task* arg0)
{
    Task*      looker;
    GpHeadAim* aim;
    u16        rate;

    looker = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            aim = memCalloc(sizeof(GpHeadAim), false);
            if (aim == NULL) {
                taskKill(arg0);
                return;
            }
            arg0->work      = aim;
            aim->yawLimit   = 0x100;
            aim->pitchLimit = 0x100;
            arg0->state++;
            /* fallthrough */
        case 1:
            aim = (GpHeadAim*)arg0->work;
            if (arg0->spawnArg1.value != 0) {
                rate      = aim->rate + 0x200;
                aim->rate = rate;
                if ((s16)rate >= 0x1001) {
                    aim->rate = 0x1000;
                }
            } else {
                rate      = aim->rate - 0x100;
                aim->rate = rate;
                if ((s16)rate < 0) {
                    aim->rate = 0;
                }
            }
            func_800B17D4(looker, gameGetPtrSlot(0xA), aim);
            return;
        default:
            taskKill(arg0);
            D_actor_450200_801401E0 = NULL;
            return;
    }
}

/// Three-way control for the second spawned sub-task: 0 tears the live one
/// down, 1 spawns it fresh, anything else is a state write the sub-task sees.
/// Spawning is skipped when the sub-task is already running.
void func_actor_450200_801320D4(s32 arg0)
{
    if (arg0 == 0) {
        if (D_actor_450200_801401E4 != NULL) {
            taskKill(D_actor_450200_801401E4);
            D_actor_450200_801401E4 = NULL;
        }
    } else if (arg0 == 1) {
        D_actor_450200_801401E4 = Task_SpawnFromTable(D_actor_450200_80137A60, 1, 0, 0);
    } else if (D_actor_450200_801401E4 != NULL) {
        D_actor_450200_801401E4->state = arg0;
    }
}

void func_actor_450200_8013215C(void)
{
    Gp_PulseState1C();
}

void func_actor_450200_8013217C(s32 arg0)
{
    if (D_actor_450200_801401E0 != NULL) {
        D_actor_450200_801401E0->spawnArg1.value = arg0;
    }
}

/// Stores in the yaw of `D_actor_450200_80137DC4` the heading, as a 12-bit
/// angle, from the slot-3 task's root coordinate to the `gameGetPtrSlot(0xA)`
/// task's, refreshing both coordinates first so the X/Z offset is current.
void func_actor_450200_8013219C(void)
{
    GfxCoord* target;
    GfxCoord* looker;

    target = (gameGetPtrSlot(0xA))->extra.tmd->coords;
    looker = (gameGetPtrSlot(3))->extra.tmd->coords;
    Gp_UpdateCoord(target);
    Gp_UpdateCoord(looker);
    D_actor_450200_80137DC4.rot.vy =
        ratan2(target->coord.t[0] - looker->coord.t[0], target->coord.t[2] - looker->coord.t[2]) & 0xFFF;
}

static void func_actor_450200_80132220(void)
{
    switch (GameFlag_GetNibble(0x101)) {
        case 0:
            func_800E8614(D_actor_450200_80138870, 0);
            GameFlag_SetNibble(0x101, 1);
            break;
        case 1:
            func_800E8614(D_actor_450200_80138A68, 0);
            GameFlag_SetNibble(0x101, 2);
            break;
        case 2:
            func_800E8614(D_actor_450200_80138C60, 0);
            GameFlag_SetNibble(0x101, 3);
            break;
        case 3:
            func_800E8614(D_actor_450200_80138E88, 0);
            break;
    }
}

static void func_actor_450200_801322F8(void)
{
    if (GameFlag_GetNibble(0xD7) != 0) {
        func_800E8614(D_actor_450200_80139098, 1);
    } else {
        func_neo_ark_observatory_8017FA98(0);
    }
    if (gameGetPtrSlot(0xA) != NULL) {
        D_actor_450200_801401E0 = Task_SpawnFromTable(D_actor_450200_80137A60, 2, 0, 0);
    }
}

static void func_actor_450200_80132368(s32 x, s32 tpageX, s32 clutY, s32 semiTrans, s32 rgb, s32 shadeTex)
{
    SPRT*    p;
    DR_MODE* dr;
    s32      i;

    for (i = 0; i < 2; i++) {
        p              = gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        setSprt(p);
        setShadeTex(p, shadeTex);
        setSemiTrans(p, semiTrans);
        p->x0   = x - 0xA0;
        p->y0   = -0x78;
        p->w    = 0x100;
        p->u0   = 0;
        p->v0   = 0;
        p->h    = 0xF0;
        p->r0   = rgb;
        p->g0   = rgb;
        p->b0   = rgb;
        p->clut = GetClut(0, clutY);
        addPrim(&gGpuCurrentOt[0x3FE], p);

        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, getTPage(1, 1, tpageX, 0x100));
        addPrim(&gGpuCurrentOt[0x3FE], dr);

        tpageX += 0x80;
        x      += 0x100;
    }
}

/// Scales the 256-entry 15-bit CLUT `src` by `scale`/0x80 per channel into
/// `dst`, setting the semi-transparency bit on every entry, and uploads it to
/// VRAM row `y`.
static inline void _actor450200LoadScaledClut(u16* src, u16* dst, s32 scale, s32 y)
{
    RECT rect;
    s32  i;
    u32  r;
    u32  g;
    u32  b;
    u32  col;

    for (i = 0; i < 0x100; i++) {
        r      = ((src[i] >> 10) & 0x1F) * scale;
        g      = ((src[i] >> 5) & 0x1F) * scale;
        b      = ((u8)src[i] & 0x1F) * scale;
        col    = r >> 7;
        g    >>= 7;
        col   &= 0xFF;
        col  <<= 10;
        col   |= ~0x7FFF;
        g     &= 0xFF;
        g    <<= 5;
        col   |= g;
        r      = b >> 7;
        r     &= 0xFF;
        r     |= col;
        dst[i] = r;
    }
    setRECT(&rect, 0, y, 0x100, 1);
    LoadImage(&rect, (u_long*)dst);
}

void func_actor_450200_80132538(Task* task)
{
    RECT rect;
    s32  state;
    s32  level;

    if (gGameSession->at4.loc.view == 8) {
        taskKill(task);
        return;
    }

    state = task->state;
    switch (state) {
        case 0:
            task->killCountdown = 0x80;
            task->state        += 1;
            setRECT(&rect, 0, 0xF7, 0x100, 1);
            StoreImage(&rect, (u_long*)D_actor_450200_801401E8);
            rect.y = 0xF8;
            StoreImage(&rect, (u_long*)D_actor_450200_801403E8);
            break;

        case 1:
            if (task->killCountdown >= 0) {
                func_actor_450200_80132368(-0x40, 0x1C0, 0xFA, 1, 0x80, state);
                func_actor_450200_80132368(0, 0x140, 0xF9, 0, 0x80, state);
                _actor450200LoadScaledClut(D_actor_450200_801401E8, D_actor_450200_801405E8, task->killCountdown, 0xF9);
                _actor450200LoadScaledClut(D_actor_450200_801403E8, D_actor_450200_801407E8, 0x80 - task->killCountdown, 0xFA);
            } else {
                func_actor_450200_80132368(-0x40, 0x1C0, 0xFA, 0, 0x80, state);
            }

            level = 0xA0 - (u16)task->killCountdown;
            if ((u32)(level & 0xFFFF) >= 0xA0U) {
                level = 0xA0;
            }
            func_neo_ark_observatory_80180DAC(level & 0xFFFF);
            task->killCountdown = (u16)task->killCountdown - 4;
            break;
    }
}

void func_actor_450200_80132848(s32 arg0)
{
    if (arg0 == 1) {
        Task_SpawnFromTable(&D_actor_450200_8013FB40, 0, 0, 0);
    }
}

void func_actor_450200_80132880(s32 arg0)
{
    func_neo_ark_observatory_80180DAC(arg0 & 0xFFFF);
}

void func_actor_450200_801328A0(u8 arg0)
{
    gGameSession->at4.loc.room        = arg0;
    Mc_SaveData[0].state.at4.loc.room = arg0;
}
