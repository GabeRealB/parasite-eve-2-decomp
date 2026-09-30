#include "actors/actor_800200.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
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

/// 8-byte fixed-point X/Z entry of a path table (`D_actor_800200_8016A128`
/// and its neighbours). `GpActorD4.pathStep` selects the entry; the Y
/// component of a destination comes from the actor's own `GfxCoord`.
typedef struct {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} GpActorPathStep;

/// 0x18-byte scratch stack block `func_actor_800200_801622B0` takes for the
/// ground-quad heading it copies into the three `GameActor.field_88` records.
typedef struct {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR vec;
} Actor800200VecScratch;

/// View of `GameActor.field_973` as the unsigned byte its rotation
/// multiply sign-extends.
typedef struct {
    byte pad[0x973];
    u8   field_973;
} ActorDirByte;

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*);
        s32 (*call2)(Task*, s32, GpCopyArg*);
        s32 (*call3)(Task*, s32, GpCountArg*);
        s32 (*call4)(Task*, s32, ActorTransform*);
        s32 (*transform)(Task*, s32, ActorTransform*, s32);
        s32 (*call5)(Task*, s32, ActorTransform*, GpOverrideArg*);
        s32 (*call6)(Task*, s32, s32);
        s32 (*call7)(Task*, s32, s32, s32);
        s32 (*call8)(Task*, s32, GfxCoord*);
    } handler;
} Actor800200MessageEntry;
STATIC_ASSERT_SIZEOF(Actor800200MessageEntry, 8);

extern Actor800200MessageEntry D_actor_800200_80169EF0[20];
extern u8*                     D_actor_800200_80169FD0[4];
extern GpActorPathStep         D_actor_800200_80169FE0[];
extern GpActorPathStep         D_actor_800200_80169FF8[];
extern GpActorPathStep         D_actor_800200_8016A018[];
extern GpActorPathStep         D_actor_800200_8016A020[];
extern GpActorPathStep         D_actor_800200_8016A040[];
extern GpActorPathStep         D_actor_800200_8016A048[];
extern GpActorPathStep         D_actor_800200_8016A058[];
extern GpActorPathStep         D_actor_800200_8016A068[];
extern GpActorPathStep         D_actor_800200_8016A080[];
extern GpActorPathStep         D_actor_800200_8016A090[];
extern GpActorPathStep         D_actor_800200_8016A098[];
extern GpActorPathStep         D_actor_800200_8016A0B0[];
extern GpActorPathStep         D_actor_800200_8016A0C8[];
extern GpActorPathStep         D_actor_800200_8016A0E0[];
extern GpActorPathStep         D_actor_800200_8016A108[];
extern GpActorPathStep         D_actor_800200_8016A128[];
extern GpActorPathStep         D_actor_800200_8016A130[];

static void func_actor_800200_801626A0(Task* task);
static void func_actor_800200_801652EC(Task* arg0);
static void func_actor_800200_80165380(Task* arg0);
static void func_actor_800200_801653A0(Task* arg0);
static void func_actor_800200_801653C0(Task* arg0);
static void func_actor_800200_80165408(Task* arg0, s32 arg1);
static void func_actor_800200_80165434(Task* arg0, s16 arg1);
static void func_actor_800200_8016545C(Task* arg0, s8 arg1);
static void func_actor_800200_801654EC(Task* arg0, s32 arg1);
static void func_actor_800200_80165534(Task* arg0);
static void func_actor_800200_80165580(Task* arg0);
static void func_actor_800200_80165644(Task* arg0);
static void func_actor_800200_80165708(Task* arg0);
static void func_actor_800200_80165814(Task* arg0);
static void func_actor_800200_801658E0(Task* arg0);
static void func_actor_800200_8016599C(Task* arg0);
static void func_actor_800200_801659CC(Task* arg0);
static void func_actor_800200_80165ACC(Task* arg0);
static void func_actor_800200_80165B84(Task* arg0);
static void func_actor_800200_80165CB4(Task* arg0);
static void func_actor_800200_80165D44(Task* arg0);
static void func_actor_800200_80165E50(Task* arg0);
static void func_actor_800200_80165E90(Task* arg0);
static void func_actor_800200_80165F28(Task* arg0);
static void func_actor_800200_80165F48(Task* arg0);
static void func_actor_800200_80165F50(Task* arg0);
static void func_actor_800200_80165FF0(Task* arg0);
static s32  _actor800200GetContactDistance(GfxCoord* coord, WorldCollisionContact* contact, u16* contactZY);

extern AnimationSet D_actor_800200_8016A464;
extern AnimationSet D_actor_800200_8016AAE0;
extern AnimationSet D_actor_800200_8016B324;
extern AnimationSet D_actor_800200_8016B8D8;
extern AnimationSet D_actor_800200_8016BB68;
extern AnimationSet D_actor_800200_8016BEE4;
extern AnimationSet D_actor_800200_8016C174;
extern AnimationSet D_actor_800200_8016C554;
extern AnimationSet D_actor_800200_8016CA44;
extern AnimationSet D_actor_800200_8016CEA0;
extern AnimationSet D_actor_800200_8016D148;
extern AnimationSet D_actor_800200_8016D72C;
extern AnimationSet D_actor_800200_8016DD44;
extern AnimationSet D_actor_800200_8016E350;
extern AnimationSet D_actor_800200_8016EA90;
extern AnimationSet D_actor_800200_8016EC20;
extern AnimationSet D_actor_800200_8016EDC8;
extern AnimationSet D_actor_800200_8016EF88;
extern AnimationSet D_actor_800200_8016F1E0;

TmdBone D_actor_800200_80166174[19] = {
#include "assets/actor_800200_model_080AC_skeleton.inc"
};

u32 D_actor_800200_80166420[19] = {
#include "assets/actor_800200_model_080AC_partVerts.inc"
};

SVECTOR D_actor_800200_8016646C[238] = {
#include "assets/actor_800200_model_080AC_verts.inc"
};

SVECTOR D_actor_800200_80166BDC[238] = {
#include "assets/actor_800200_model_080AC_normals.inc"
};

u32 D_actor_800200_8016734C[2784] = {
#include "assets/actor_800200_model_080AC_stream.inc"
};

TmdSource D_actor_800200_80169ECC = {
    0,
    13636,
    6016,
    19,
    D_actor_800200_80166420,
    D_actor_800200_8016646C,
    D_actor_800200_80166BDC,
    D_actor_800200_80166174,
    D_actor_800200_8016734C,
};

Actor800200MessageEntry D_actor_800200_80169EF0[20] = {
    { 1000, { .call1 = func_8010C4F0 } },
    { 1002, { .call1 = func_8010C4F0 } },
    { 1003, { .call1 = func_8010C4F0 } },
    { 1004, { .call1 = func_8010C4F0 } },
    { 1001, { .call4 = func_80104D68 } },
    { 1005, { .call7 = func_8010583C } },
    { 1006, { .transform = func_8010C688 } },
    { 1007, { .call1 = func_8010C4F0 } },
    { 1008, { .call0 = func_80105828 } },
    { 1009, { .call0 = func_8010C30C } },
    { 1010, { .call5 = func_8010C6C8 } },
    { 1011, { .call6 = func_80104684 } },
    { 1012, { .call1 = func_8010C648 } },
    { 1013, { .call8 = func_80105A60 } },
    { 1014, { .call3 = func_801052B8 } },
    { 1015, { .call2 = Gp_CopyAllyAnim } },
    { 1016, { .call1 = func_8010C4F0 } },
    { 1017, { .call1 = func_8010C4F0 } },
    { 1018, { .call1 = func_8010C4F0 } },
    { 1019, { .call5 = func_8010C708 } },
};

u8 D_actor_800200_80169F90[16] = {
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800200_80169FA0[16] = {
    3,
    3,
    3,
    3,
    3,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800200_80169FB0[16] = {
    3,
    3,
    3,
    3,
    3,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800200_80169FC0[16] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    3,
    3,
    2,
    2,
    2,
};

u8* D_actor_800200_80169FD0[4] = {
    D_actor_800200_80169F90,
    D_actor_800200_80169FA0,
    D_actor_800200_80169FB0,
    D_actor_800200_80169FC0,
};

GpActorPathStep D_actor_800200_80169FE0[3] = {
    { 0x3C28, 3850 },
    { 0x52DA, 3390 },
    { 0x5488, 3000 },
};

GpActorPathStep D_actor_800200_80169FF8[4] = {
    { 0x32C8, 3740 },
    { 7530, 2720 },
    { 1720, 1480 },
    { 5100, 1250 },
};

GpActorPathStep D_actor_800200_8016A018[1] = {
    { 940, 2320 },
};

GpActorPathStep D_actor_800200_8016A020[4] = {
    { 5000, 3760 },
    { 2970, 3020 },
    { 3400, 1640 },
    { 4530, 1500 },
};

GpActorPathStep D_actor_800200_8016A040[1] = {
    { 200, 1650 },
};

GpActorPathStep D_actor_800200_8016A048[2] = {
    { 1530, 2560 },
    { 1100, 7060 },
};

GpActorPathStep D_actor_800200_8016A058[2] = {
    { 0x2C4C, 1450 },
    { 7940, 1590 },
};

GpActorPathStep D_actor_800200_8016A068[3] = {
    { 4100, -350 },
    { 200, -1050 },
    { -1900, 600 },
};

GpActorPathStep D_actor_800200_8016A080[2] = {
    { 8512, 2880 },
    { 0x2792, 2880 },
};

GpActorPathStep D_actor_800200_8016A090[1] = {
    { 3130, 0 },
};

GpActorPathStep D_actor_800200_8016A098[3] = {
    { -9350, -1248 },
    { -5070, 230 },
    { -4960, 1380 },
};

GpActorPathStep D_actor_800200_8016A0B0[3] = {
    { -4740, 4740 },
    { 2170, 4660 },
    { 2100, 3390 },
};

GpActorPathStep D_actor_800200_8016A0C8[3] = {
    { 4600, -320 },
    { 5200, -1540 },
    { 9100, -1560 },
};

GpActorPathStep D_actor_800200_8016A0E0[5] = {
    { -480, -3700 },
    { 1940, -3640 },
    { 3000, -320 },
    { 1860, -4720 },
    { 1980, -6660 },
};

GpActorPathStep D_actor_800200_8016A108[4] = {
    { 1780, 7950 },
    { 9850, 7980 },
    { 9710, 6210 },
    { 0x288C, 6240 },
};

GpActorPathStep D_actor_800200_8016A128[1] = {
    { 5570, 18 },
};

GpActorPathStep D_actor_800200_8016A130[5] = {
    { -800, -2050 },
    { 10, -2950 },
    { 10, -6900 },
    { 10, -1900 },
    { 10, -390 },
};

AnimationPackedPose D_actor_800200_8016A158[5] = {
#include "assets/actor_800200_animation_08644_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016A194[46] = {
#include "assets/actor_800200_animation_08644_bank4.inc"
};

AnimationRecord D_actor_800200_8016A24C[124] = {
#include "assets/actor_800200_animation_08644_records.inc"
};

u16 D_actor_800200_8016A43C[20] = {
#include "assets/actor_800200_animation_08644_indices.inc"
};

AnimationSet D_actor_800200_8016A464 = {
    D_actor_800200_8016A24C,
    D_actor_800200_8016A43C,
    { NULL, D_actor_800200_8016A158, NULL, NULL, D_actor_800200_8016A194, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016A48C[9] = {
#include "assets/actor_800200_animation_08CC0_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016A4F8[139] = {
#include "assets/actor_800200_animation_08CC0_bank4.inc"
};

AnimationRecord D_actor_800200_8016A724[229] = {
#include "assets/actor_800200_animation_08CC0_records.inc"
};

u16 D_actor_800200_8016AAB8[20] = {
#include "assets/actor_800200_animation_08CC0_indices.inc"
};

AnimationSet D_actor_800200_8016AAE0 = {
    D_actor_800200_8016A724,
    D_actor_800200_8016AAB8,
    { NULL, D_actor_800200_8016A48C, NULL, NULL, D_actor_800200_8016A4F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016AB08[16] = {
#include "assets/actor_800200_animation_09504_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016ABC8[182] = {
#include "assets/actor_800200_animation_09504_bank4.inc"
};

AnimationRecord D_actor_800200_8016AEA0[279] = {
#include "assets/actor_800200_animation_09504_records.inc"
};

u16 D_actor_800200_8016B2FC[20] = {
#include "assets/actor_800200_animation_09504_indices.inc"
};

AnimationSet D_actor_800200_8016B324 = {
    D_actor_800200_8016AEA0,
    D_actor_800200_8016B2FC,
    { NULL, D_actor_800200_8016AB08, NULL, NULL, D_actor_800200_8016ABC8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016B34C[11] = {
#include "assets/actor_800200_animation_09AB8_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016B3D0[134] = {
#include "assets/actor_800200_animation_09AB8_bank4.inc"
};

AnimationRecord D_actor_800200_8016B5E8[178] = {
#include "assets/actor_800200_animation_09AB8_records.inc"
};

u16 D_actor_800200_8016B8B0[20] = {
#include "assets/actor_800200_animation_09AB8_indices.inc"
};

AnimationSet D_actor_800200_8016B8D8 = {
    D_actor_800200_8016B5E8,
    D_actor_800200_8016B8B0,
    { NULL, D_actor_800200_8016B34C, NULL, NULL, D_actor_800200_8016B3D0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016B900[2] = {
#include "assets/actor_800200_animation_09D48_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016B918[36] = {
#include "assets/actor_800200_animation_09D48_bank4.inc"
};

AnimationRecord D_actor_800200_8016B9A8[102] = {
#include "assets/actor_800200_animation_09D48_records.inc"
};

u16 D_actor_800200_8016BB40[20] = {
#include "assets/actor_800200_animation_09D48_indices.inc"
};

AnimationSet D_actor_800200_8016BB68 = {
    D_actor_800200_8016B9A8,
    D_actor_800200_8016BB40,
    { NULL, D_actor_800200_8016B900, NULL, NULL, D_actor_800200_8016B918, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016BB90[6] = {
#include "assets/actor_800200_animation_0A0C4_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016BBD8[74] = {
#include "assets/actor_800200_animation_0A0C4_bank4.inc"
};

AnimationRecord D_actor_800200_8016BD00[111] = {
#include "assets/actor_800200_animation_0A0C4_records.inc"
};

u16 D_actor_800200_8016BEBC[20] = {
#include "assets/actor_800200_animation_0A0C4_indices.inc"
};

AnimationSet D_actor_800200_8016BEE4 = {
    D_actor_800200_8016BD00,
    D_actor_800200_8016BEBC,
    { NULL, D_actor_800200_8016BB90, NULL, NULL, D_actor_800200_8016BBD8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016BF0C[2] = {
#include "assets/actor_800200_animation_0A354_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016BF24[39] = {
#include "assets/actor_800200_animation_0A354_bank4.inc"
};

AnimationRecord D_actor_800200_8016BFC0[99] = {
#include "assets/actor_800200_animation_0A354_records.inc"
};

u16 D_actor_800200_8016C14C[20] = {
#include "assets/actor_800200_animation_0A354_indices.inc"
};

AnimationSet D_actor_800200_8016C174 = {
    D_actor_800200_8016BFC0,
    D_actor_800200_8016C14C,
    { NULL, D_actor_800200_8016BF0C, NULL, NULL, D_actor_800200_8016BF24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016C19C[7] = {
#include "assets/actor_800200_animation_0A734_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016C1F0[89] = {
#include "assets/actor_800200_animation_0A734_bank4.inc"
};

AnimationRecord D_actor_800200_8016C354[118] = {
#include "assets/actor_800200_animation_0A734_records.inc"
};

u16 D_actor_800200_8016C52C[20] = {
#include "assets/actor_800200_animation_0A734_indices.inc"
};

AnimationSet D_actor_800200_8016C554 = {
    D_actor_800200_8016C354,
    D_actor_800200_8016C52C,
    { NULL, D_actor_800200_8016C19C, NULL, NULL, D_actor_800200_8016C1F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016C57C[5] = {
#include "assets/actor_800200_animation_0AC24_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016C5B8[112] = {
#include "assets/actor_800200_animation_0AC24_bank4.inc"
};

AnimationRecord D_actor_800200_8016C778[169] = {
#include "assets/actor_800200_animation_0AC24_records.inc"
};

u16 D_actor_800200_8016CA1C[20] = {
#include "assets/actor_800200_animation_0AC24_indices.inc"
};

AnimationSet D_actor_800200_8016CA44 = {
    D_actor_800200_8016C778,
    D_actor_800200_8016CA1C,
    { NULL, D_actor_800200_8016C57C, NULL, NULL, D_actor_800200_8016C5B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016CA6C[7] = {
#include "assets/actor_800200_animation_0B080_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016CAC0[92] = {
#include "assets/actor_800200_animation_0B080_bank4.inc"
};

AnimationRecord D_actor_800200_8016CC30[146] = {
#include "assets/actor_800200_animation_0B080_records.inc"
};

u16 D_actor_800200_8016CE78[20] = {
#include "assets/actor_800200_animation_0B080_indices.inc"
};

AnimationSet D_actor_800200_8016CEA0 = {
    D_actor_800200_8016CC30,
    D_actor_800200_8016CE78,
    { NULL, D_actor_800200_8016CA6C, NULL, NULL, D_actor_800200_8016CAC0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016CEC8[4] = {
#include "assets/actor_800200_animation_0B328_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016CEF8[45] = {
#include "assets/actor_800200_animation_0B328_bank4.inc"
};

AnimationRecord D_actor_800200_8016CFAC[93] = {
#include "assets/actor_800200_animation_0B328_records.inc"
};

u16 D_actor_800200_8016D120[20] = {
#include "assets/actor_800200_animation_0B328_indices.inc"
};

AnimationSet D_actor_800200_8016D148 = {
    D_actor_800200_8016CFAC,
    D_actor_800200_8016D120,
    { NULL, D_actor_800200_8016CEC8, NULL, NULL, D_actor_800200_8016CEF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016D170[7] = {
#include "assets/actor_800200_animation_0B90C_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016D1C4[128] = {
#include "assets/actor_800200_animation_0B90C_bank4.inc"
};

AnimationRecord D_actor_800200_8016D3C4[208] = {
#include "assets/actor_800200_animation_0B90C_records.inc"
};

u16 D_actor_800200_8016D704[20] = {
#include "assets/actor_800200_animation_0B90C_indices.inc"
};

AnimationSet D_actor_800200_8016D72C = {
    D_actor_800200_8016D3C4,
    D_actor_800200_8016D704,
    { NULL, D_actor_800200_8016D170, NULL, NULL, D_actor_800200_8016D1C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016D754[3] = {
#include "assets/actor_800200_animation_0BF24_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016D778[144] = {
#include "assets/actor_800200_animation_0BF24_bank4.inc"
};

AnimationRecord D_actor_800200_8016D9B8[217] = {
#include "assets/actor_800200_animation_0BF24_records.inc"
};

u16 D_actor_800200_8016DD1C[20] = {
#include "assets/actor_800200_animation_0BF24_indices.inc"
};

AnimationSet D_actor_800200_8016DD44 = {
    D_actor_800200_8016D9B8,
    D_actor_800200_8016DD1C,
    { NULL, D_actor_800200_8016D754, NULL, NULL, D_actor_800200_8016D778, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016DD6C[3] = {
#include "assets/actor_800200_animation_0C530_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016DD90[141] = {
#include "assets/actor_800200_animation_0C530_bank4.inc"
};

AnimationRecord D_actor_800200_8016DFC4[217] = {
#include "assets/actor_800200_animation_0C530_records.inc"
};

u16 D_actor_800200_8016E328[20] = {
#include "assets/actor_800200_animation_0C530_indices.inc"
};

AnimationSet D_actor_800200_8016E350 = {
    D_actor_800200_8016DFC4,
    D_actor_800200_8016E328,
    { NULL, D_actor_800200_8016DD6C, NULL, NULL, D_actor_800200_8016DD90, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016E378[8] = {
#include "assets/actor_800200_animation_0CC70_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016E3D8[158] = {
#include "assets/actor_800200_animation_0CC70_bank4.inc"
};

AnimationRecord D_actor_800200_8016E650[262] = {
#include "assets/actor_800200_animation_0CC70_records.inc"
};

u16 D_actor_800200_8016EA68[20] = {
#include "assets/actor_800200_animation_0CC70_indices.inc"
};

AnimationSet D_actor_800200_8016EA90 = {
    D_actor_800200_8016E650,
    D_actor_800200_8016EA68,
    { NULL, D_actor_800200_8016E378, NULL, NULL, D_actor_800200_8016E3D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016EAB8[2] = {
#include "assets/actor_800200_animation_0CE00_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016EAD0[17] = {
#include "assets/actor_800200_animation_0CE00_bank4.inc"
};

AnimationRecord D_actor_800200_8016EB14[57] = {
#include "assets/actor_800200_animation_0CE00_records.inc"
};

u16 D_actor_800200_8016EBF8[20] = {
#include "assets/actor_800200_animation_0CE00_indices.inc"
};

AnimationSet D_actor_800200_8016EC20 = {
    D_actor_800200_8016EB14,
    D_actor_800200_8016EBF8,
    { NULL, D_actor_800200_8016EAB8, NULL, NULL, D_actor_800200_8016EAD0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016EC48[2] = {
#include "assets/actor_800200_animation_0CFA8_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016EC60[23] = {
#include "assets/actor_800200_animation_0CFA8_bank4.inc"
};

AnimationRecord D_actor_800200_8016ECBC[57] = {
#include "assets/actor_800200_animation_0CFA8_records.inc"
};

u16 D_actor_800200_8016EDA0[20] = {
#include "assets/actor_800200_animation_0CFA8_indices.inc"
};

AnimationSet D_actor_800200_8016EDC8 = {
    D_actor_800200_8016ECBC,
    D_actor_800200_8016EDA0,
    { NULL, D_actor_800200_8016EC48, NULL, NULL, D_actor_800200_8016EC60, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016EDF0[2] = {
#include "assets/actor_800200_animation_0D168_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016EE08[26] = {
#include "assets/actor_800200_animation_0D168_bank4.inc"
};

AnimationRecord D_actor_800200_8016EE70[60] = {
#include "assets/actor_800200_animation_0D168_records.inc"
};

u16 D_actor_800200_8016EF60[20] = {
#include "assets/actor_800200_animation_0D168_indices.inc"
};

AnimationSet D_actor_800200_8016EF88 = {
    D_actor_800200_8016EE70,
    D_actor_800200_8016EF60,
    { NULL, D_actor_800200_8016EDF0, NULL, NULL, D_actor_800200_8016EE08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800200_8016EFB0[2] = {
#include "assets/actor_800200_animation_0D3C0_bank1.inc"
};

AnimationPackedRotation D_actor_800200_8016EFC8[40] = {
#include "assets/actor_800200_animation_0D3C0_bank4.inc"
};

AnimationRecord D_actor_800200_8016F068[84] = {
#include "assets/actor_800200_animation_0D3C0_records.inc"
};

u16 D_actor_800200_8016F1B8[20] = {
#include "assets/actor_800200_animation_0D3C0_indices.inc"
};

AnimationSet D_actor_800200_8016F1E0 = {
    D_actor_800200_8016F068,
    D_actor_800200_8016F1B8,
    { NULL, D_actor_800200_8016EFB0, NULL, NULL, D_actor_800200_8016EFC8, NULL, NULL, NULL },
};

AnimationSet* D_actor_800200_8016F208[79] = {
    NULL,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016AAE0,
    &D_actor_800200_8016AAE0,
    &D_actor_800200_8016B324,
    &D_actor_800200_8016E350,
    &D_actor_800200_8016DD44,
    &D_actor_800200_8016B8D8,
    &D_actor_800200_8016BEE4,
    &D_actor_800200_8016BB68,
    &D_actor_800200_8016D148,
    &D_actor_800200_8016D72C,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016EA90,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016CA44,
    &D_actor_800200_8016CEA0,
    &D_actor_800200_8016EC20,
    &D_actor_800200_8016AAE0,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016A464,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016C174,
    &D_actor_800200_8016C554,
    &D_actor_800200_8016EDC8,
    &D_actor_800200_8016EF88,
    &D_actor_800200_8016F1E0,
    &D_actor_800200_8016A464,
    &D_actor_800200_8016A464,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static void func_actor_800200_80162088(Task* arg0);
static void func_actor_800200_801622B0(Task* arg0);
static void func_actor_800200_80162694(Task* arg0);
static void func_actor_800200_80162750(Task* arg0);
static void func_actor_800200_80162990(Task* arg0);
static void func_actor_800200_80162BFC(Task* arg0);
static void func_actor_800200_80162E0C(Task* arg0);
static void func_actor_800200_80163044(Task* arg0);
static void func_actor_800200_80163180(Task* arg0);
static void func_actor_800200_8016337C(Task* arg0);
static void func_actor_800200_80163584(Task* arg0);
static void func_actor_800200_801637B4(Task* arg0);
static void func_actor_800200_8016390C(Task* arg0);
static void func_actor_800200_80163A54(Task* arg0);
static void func_actor_800200_80163B90(Task* arg0);
static void func_actor_800200_80163CCC(Task* arg0);
static void func_actor_800200_80163E14(Task* arg0);
static void func_actor_800200_80163F5C(Task* arg0);
static void func_actor_800200_80164180(Task* arg0);
static void func_actor_800200_8016436C(Task* arg0);
static void func_actor_800200_80164598(Task* arg0);
static void func_actor_800200_801647A8(Task* arg0);
static void func_actor_800200_801649D8(Task* arg0);
static void func_actor_800200_80164C54(Task* arg0);
static void func_actor_800200_80164EBC(Task* arg0);
static s32  func_actor_800200_80165104(Task* arg0);

static void func_actor_800200_80162088(Task* arg0)
{
    GameActor*             actor;
    TmdObject*             extra;
    GfxCoord*              coord;
    GfxCoord*              next;
    GfxCoord**             addr;
    WorldCollisionBody*    obj;
    WorldCollisionContact* recs;
    McSaveData*            save;
    SVECTOR3*              scratch;
    void*                  head;
    s32                    packed;

    actor                      = arg0->work;
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = head - 8;
    scratch                    = (SVECTOR3*)(head - 8);
    extra                      = arg0->extra.tmd;
    addr                       = &extra->coords;
    coord                      = *addr;
    arg0->state++;
    arg0->msgTable      = D_actor_800200_80169EF0;
    arg0->exitCallback  = &func_actor_800200_801626A0;
    actor->field_938    = 0x13;
    Gp_ActorSlots[1]    = arg0;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags        = 0;
    RotMatrix((SVECTOR*)&actor->field_50, &coord->coord);
    func_8010BFCC(arg0);
    actor->field_985 = 0x10;
    Gp_AnimResetChildSlots(arg0, actor->field_93C);
    Gp_AnimTickChildSlots(arg0);
    recs                        = actor->field_17C;
    obj                         = (WorldCollisionBody*)actor->field_AC;
    actor->field_10             = coord->coord.t[0];
    actor->field_14             = coord->coord.t[1];
    actor->field_18             = coord->coord.t[2];
    obj->context.motion         = &actor->field_88[0];
    obj->coord                  = coord;
    actor->field_88[0].contacts = recs;
    save                        = &Mc_SaveData[0];
    obj->pos.vx                 = 0;
    obj->pos.vy                 = -0xFA;
    obj->pos.vz                 = 0;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0xFA;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        packed      = 0x10000;
        obj->key    = temp | packed;
        Gp_LinkObj(0, obj);
    }
    Gp_InitRec18Table(actor->field_88[0].contacts, ARRAY_SIZE(actor->field_17C), 0);
    obj->flags                 |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    obj                         = (WorldCollisionBody*)actor->field_CC;
    next                        = arg0->extra.tmd->coords;
    obj->context.motion         = &actor->field_88[1];
    obj->coord                  = next + 4;
    actor->field_88[1].contacts = recs;
    obj->pos.vx                 = 0;
    obj->pos.vy                 = 0;
    obj->pos.vz                 = 0;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0xC8;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        obj->key    = temp | packed;
        Gp_LinkObj(0, obj);
    }
    obj->flags                 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    actor->field_984            = 7;
    ((SVECTOR3*)(head - 8))->vx = 0;
    scratch->vy                 = -0x100;
    scratch->vz                 = 0x200;
    Gp_BindActorD4(arg0, scratch, 0x600);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_actor_800200_801622B0(Task* arg0)
{
    void**                 scratch;
    u8*                    head;
    Actor800200VecScratch* sc;
    GameActor*             actor;
    TmdObject*             obj;
    TmdObject*             extra;
    GfxCoord*              coord;
    GpActorD4*             d4;
    WorldCollisionBody*    objs[2];
    s32                    dy;
    s32                    i;
    s8                     bits;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    obj                            = arg0->extra.tmd;
    SCRATCH_HEAD_AT(scratch, void) = head - 0x18;
    extra                          = obj;
    sc                             = (Actor800200VecScratch*)(head - 0x18);
    coord                          = extra->coords;
    actor                          = arg0->work;
    d4                             = actor->field_910;
    if (actor->field_954 != 2 &&
        (dy = coord->coord.t[1], dy = dy - actor->field_14, dy = ABS(dy), dy >= 0x200)) {
        coord->coord.t[0] = actor->field_10;
        coord->coord.t[1] = actor->field_14;
        coord->coord.t[2] = actor->field_18;
    } else {
        if (actor->field_984 & 1) {
            actor->field_992 = func_801011D0(coord, actor->field_88[0].contacts, ARRAY_SIZE(actor->field_17C), &actor->field_930);
            if ((s8)actor->field_992 == 2) {
                coord->coord.t[0] = actor->field_10;
                coord->coord.t[1] = actor->field_14;
                coord->coord.t[2] = actor->field_18;
            }
        } else {
            actor->field_992 = 0;
        }
        actor->field_10 = coord->coord.t[0];
        actor->field_14 = coord->coord.t[1];
        actor->field_18 = coord->coord.t[2];
    }
    d4->coord = *arg0->extra.tmd->coords;
    objs[0]   = (WorldCollisionBody*)actor->field_AC;
    objs[1]   = (WorldCollisionBody*)actor->field_CC;
    for (i = 0; i < 2; i++) {
        bits = actor->field_983;
        if ((bits >> i) & 1) {
            actor->field_984 |= 1 << i;
            objs[i]->flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        } else if (bits & (8 << i)) {
            actor->field_984 &= ~(1 << i);
            objs[i]->flags   &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    actor->field_983 = 0;
    if (D_80115768 == 0 && Gp_StateF0.field_4 == 0) {
        func_actor_800200_801652EC(arg0);
    }
    Gp_ClearRec18Occupied(actor->field_17C);
    Gp_ClearRec18Occupied(&actor->field_910->contact);
    if (actor->field_984 & 1) {
        coord->coord.t[1] = actor->field_14 + 8;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    if ((s8)actor->field_986 != 0) {
        sc->vec.vx = (u16)actor->field_30.vx;
        sc->vec.vy = (u16)actor->field_30.vy;
        sc->vec.vz = (u16)actor->field_30.vz;
    } else {
        sc->vec.vx = (u16)coord->workm.m[0][2] * (s8)((volatile ActorDirByte*)actor)->field_973;
        sc->vec.vy = (u16)coord->workm.m[1][2] * (s8)((volatile ActorDirByte*)actor)->field_973;
        sc->vec.vz = (u16)coord->workm.m[2][2] * (s8)((volatile ActorDirByte*)actor)->field_973;
    }
    actor->field_88[0].motionDirection.vx = sc->vec.vx;
    actor->field_88[0].motionDirection.vy = sc->vec.vy;
    actor->field_88[0].motionDirection.vz = sc->vec.vz;
    actor->field_88[1].motionDirection.vx = sc->vec.vx;
    actor->field_88[1].motionDirection.vy = sc->vec.vy;
    actor->field_88[1].motionDirection.vz = sc->vec.vz;
    actor->field_88[2].motionDirection.vx = sc->vec.vx;
    actor->field_88[2].motionDirection.vy = sc->vec.vy;
    actor->field_88[2].motionDirection.vz = sc->vec.vz;
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        Gp_DrawEffGroundQuad(MATRIX_TRANS(&coord->workm), 0x200, Gp_State1C->groundShadowShade);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

static void func_actor_800200_80162694(Task* arg0)
{
    arg0->state = 3;
}

/// Teardown of the actor's main task, run both as its exit callback and as
/// the last entry of its state table: clears the second `Gp_ActorSlots` slot,
/// unlinks the two collision objects the set-up state linked, and kills the
/// task.
static void func_actor_800200_801626A0(Task* task)
{
    GameActor* actor;

    actor            = (GameActor*)task->work;
    Gp_ActorSlots[1] = NULL;
    Gp_UnlinkObj((WorldCollisionBody*)actor->field_AC);
    Gp_UnlinkObj((WorldCollisionBody*)actor->field_CC);
    taskKill(task);
}

/// State handlers of the actor's main task, indexed by its state: set-up, the
/// per-frame update, a step that only advances to the last state, and the
/// teardown.
static const TaskFuncTable4 D_actor_800200_80161E24 = { {
    func_actor_800200_80162088,
    func_actor_800200_801622B0,
    func_actor_800200_80162694,
    func_actor_800200_801626A0,
} };

/// Handlers `func_actor_800200_801652EC` runs, indexed by `field_954`.
static const TaskFuncTable3 D_actor_800200_80161E34 = { {
    func_actor_800200_80165B84,
    func_actor_800200_80165E90,
    func_actor_800200_80165F50,
} };

/// Per-frame entry point of the actor's main task: runs the handler its state
/// selects. The table is a local, so it is copied from `.rodata` onto the
/// stack on every call.
void func_actor_800200_801626EC(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800200_80161E24;
    states.funcs[task->state](task);
}

static void func_actor_800200_80162750(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    GfxCoord*  target;
    u8*        head;
    VECTOR3*   vec;
    void*      lock;
    u32        state;
    u8*        tbl;
    s32        dist;
    s32        diff;

    coord                    = arg0->extra.tmd->coords;
    target                   = (gameGetPtrSlot(3))->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    vec                      = (VECTOR3*)(head - 0x10);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;
    actor                    = arg0->work;
    actor->field_93E        += 1;
    d4                       = actor->field_910;
    if (Gp_StateF0.prefix.bytes.field_0 == 1) {
        state            = 0;
        lock             = Gp_FindLockNode(arg0);
        actor->field_90C = lock;
        if (lock != NULL) {
            Gp_GetLockPos(lock, vec);
            func_80103C74(coord, vec, vec);
            dist  = func_80103D8C(((VECTOR3*)(head - 0x10))->vx, vec->vz);
            dist /= 1024;
            if (dist >= 4) {
                dist = 3;
            }
            tbl   = D_actor_800200_80169FD0[dist];
            state = *(tbl + (rand() & 0xF));
        }
        switch (state) {
            case 0:
                break;
            case 1:
                Gp_GetLockPos(actor->field_90C, (VECTOR3*)&actor->field_20);
                func_actor_800200_80165408(arg0, 6);
                break;
            case 2:
                d4->repeatCount = (rand() & 3) + 1;
                func_actor_800200_80165434(arg0, 1);
                break;
            case 3:
                goto do_65380;
        }
    } else {
        if (func_8010BC70(coord) >= 0x600) {
        do_65380:
            func_actor_800200_80165380(arg0);
        } else {
            diff = func_8010BCF4(arg0, MATRIX_TRANS(&target->coord));
            if (diff < 0) {
                diff = -diff;
            }
            if (diff >= 0x200) {
                actor->field_90C = NULL;
                func_actor_800200_801653A0(arg0);
            } else if (actor->field_93E >= ((rand() & 0x7F) + 0x96)) {
                func_actor_800200_801653C0(arg0);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_800200_80162990(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u16        state;
    s32        mode;
    s32        delay;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_960 = 1;
            actor->field_20  = D_actor_800200_80169FF8[3].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_80169FF8[3].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_80169FF8[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_80169FF8[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 3) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xE00 || (d4->pathStep == 2 && Gp_HasCollectedBit(0x114) == 0)) {
                    actor->field_960 = 2;
                    actor->field_934 = 0;
                    actor->field_90C = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                d4->pathStep++;
                return;
            }
            mode = 6;
            if (d4->pathStep == 3) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 2:
            if ((func_8010BC70(coord) < 0xC01 && d4->pathStep < 2) || (d4->pathStep == state && Gp_HasCollectedBit(0x114) != 0)) {
                d4->pathStep++;
                actor->field_960 = 1;
                return;
            }
            delay            = actor->field_934 - 1;
            actor->field_934 = delay;
            if (delay <= 0) {
                actor->field_934 = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

static void func_actor_800200_80162BFC(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u16        state;
    s32        mode;
    s32        delay;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_960 = 1;
            actor->field_20  = D_actor_800200_8016A020[3].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A020[3].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A020[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A020[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 3) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xC00) {
                    actor->field_960 = 2;
                    actor->field_934 = 0;
                    actor->field_90C = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                d4->pathStep++;
                return;
            }
            mode = 6;
            if (d4->pathStep == 3) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 2:
            if (func_8010BC70(coord) < 0xB01 && d4->pathStep > 0) {
                d4->pathStep++;
                actor->field_960 = 1;
                return;
            }
            delay            = actor->field_934 - 1;
            actor->field_934 = delay;
            if (delay <= 0) {
                actor->field_934 = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

static void func_actor_800200_80162E0C(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    GfxCoord*  target;
    s32        delay;

    coord  = arg0->extra.tmd->coords;
    target = (gameGetPtrSlot(3))->extra.tmd->coords;
    actor  = arg0->work;
    d4     = actor->field_910;
    switch (actor->field_960) {
        case 0:
            actor->field_20 = D_actor_800200_80169FE0[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_80169FE0[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 2) {
                    actor->field_960 = 3;
                    actor->field_95E = 0;
                    actor->field_95C = 7;
                    Gp_AnimPlayChildSlotsEx(arg0, 7, 0, 3);
                    func_actor_800200_80165408(arg0, 6);
                    return;
                }
                if (func_8010BC70(coord) >= 0xE00) {
                    actor->field_960 = 1;
                    actor->field_934 = 0;
                    actor->field_90C = 0;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                d4->pathStep++;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        case 1:
            if ((func_8010BC70(coord) < 0xA01) || (coord->coord.t[0] < target->coord.t[0])) {
                d4->pathStep++;
                actor->field_960 = 0;
                return;
            }
            delay            = actor->field_934 - 1;
            actor->field_934 = delay;
            if (delay <= 0) {
                d4->repeatCount  = 1;
                actor->field_934 = rand() & 0x7F;
                func_actor_800200_80165434(arg0, 0);
            }
            return;
        case 3:
            if (actor->field_95E != 0) {
                actor->field_960++;
            }
            return;
        case 4:
            actor->field_95C = 0;
            actor->field_960++;
            Gp_AnimResetChildSlots(arg0, 9);
            return;
        default:
        case 2:
        case 5:
            return;
    }
}

static void func_actor_800200_80163044(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u16        state;
    s32        flag;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A048[1].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A048[1].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A048[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A048[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 1) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->pathStep++;
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

static void func_actor_800200_80163180(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u16        state;
    s32        delay;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_960 = 1;
            actor->field_20  = D_actor_800200_8016A058[1].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A058[1].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A058[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A058[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 1) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xC00) {
                    actor->field_960 = 2;
                    actor->field_934 = 0;
                    actor->field_90C = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                d4->pathStep++;
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        case 2:
            if (func_8010BC70(coord) < 0x801) {
                d4->pathStep++;
                actor->field_960 = 1;
                return;
            }
            delay            = actor->field_934 - 1;
            actor->field_934 = delay;
            if (delay <= 0) {
                actor->field_934 = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

static void func_actor_800200_8016337C(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u16        state;
    s32        mode;
    s32        delay;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_960 = 1;
            actor->field_20  = D_actor_800200_8016A068[2].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A068[2].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A068[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A068[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 2) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xC00) {
                    actor->field_960 = 2;
                    actor->field_934 = 0;
                    actor->field_90C = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                d4->pathStep++;
                return;
            }
            mode = 6;
            if (d4->pathStep == 2) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 2:
            if (func_8010BC70(coord) < 0x901) {
                d4->pathStep++;
                actor->field_960 = 1;
                return;
            }
            delay            = actor->field_934 - 1;
            actor->field_934 = delay;
            if (delay <= 0) {
                actor->field_934 = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

static void func_actor_800200_80163584(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u16        state;
    s32        mode;
    s32        delay;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_960 = 1;
            actor->field_20  = D_actor_800200_8016A080[1].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A080[1].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_960++;
            func_actor_800200_80165534(arg0);
            return;
        case 2:
            actor->field_20 = D_actor_800200_8016A080[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A080[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 1) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xC00) {
                    actor->field_960 = 3;
                    actor->field_934 = 0;
                    actor->field_90C = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                d4->pathStep++;
                return;
            }
            mode = 6;
            if (d4->pathStep == 1) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 3:
            if (func_8010BC70(coord) < 0x901) {
                d4->pathStep++;
                actor->field_960 = 1;
                return;
            }
            delay            = actor->field_934 - 1;
            actor->field_934 = delay;
            if (delay <= 0) {
                actor->field_934 = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

static void func_actor_800200_801637B4(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u16        state;
    s32        flag;
    s32        mode;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A098[2].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A098[2].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A098[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A098[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 2) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->pathStep++;
                func_actor_800200_80165534(arg0);
                return;
            }
            mode = 6;
            if (d4->pathStep == 1) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        default:
            return;
    }
}

static void func_actor_800200_8016390C(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u16        state;
    s32        flag;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A0B0[2].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A0B0[2].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A0B0[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A0B0[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 2) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->pathStep++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

static void func_actor_800200_80163A54(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    s32        flag;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    d4    = actor->field_910;
    switch (actor->field_960) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A0C8[2].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A0C8[2].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
            func_actor_800200_80165534(arg0);
            break;
        case 1:
            actor->field_20 = D_actor_800200_8016A0C8[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A0C8[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 2) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    break;
                }
                d4->pathStep = d4->pathStep + 1;
                func_actor_800200_80165534(arg0);
                break;
            }
            func_actor_800200_80165408(arg0, 6);
            break;
    }
}

static void func_actor_800200_80163B90(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u16        state;
    s32        flag;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A0E0[4].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A0E0[4].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
            func_actor_800200_80165534(arg0);
            return;
        case 1:
            actor->field_20 = D_actor_800200_8016A0E0[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A0E0[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 4) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->pathStep++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

static void func_actor_800200_80163CCC(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u16        state;
    s32        flag;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A108[3].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A108[3].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A108[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A108[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 3) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->pathStep++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

static void func_actor_800200_80163E14(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u16        state;
    s32        flag;
    s32        mode;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A130[4].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A130[4].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A130[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A130[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 4) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_80165534(arg0);
                    return;
                }
                d4->pathStep++;
                func_actor_800200_80165534(arg0);
                return;
            }
            mode = 6;
            if (d4->pathStep == 1) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        default:
            return;
    }
}

static void func_actor_800200_80163F5C(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    GfxCoord*  target;
    VECTOR3*   vec;
    GameActor* hit;
    s32        mode;
    s32        dist;
    s32        angle;

    coord  = arg0->extra.tmd->coords;
    target = (gameGetPtrSlot(3))->extra.tmd->coords;
    actor  = arg0->work;
    dist   = _actor800200GetContactDistance(coord, &actor->field_910->contact, NULL);
    if (dist != 0 && dist < 0x301) {
        hit            = arg0->work;
        d4             = hit->field_910;
        hit->field_956 = 0xA;
        hit->field_95A = 2;
        hit->field_954 = 0;
        hit->field_95C = 0;
        hit->field_95E = 0;
        hit->field_973 = 0;
        hit->field_975 = 0;
        d4->scanDist   = -1;
        d4->scanAngle  = 0;
        Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 3);
        return;
    }
    switch (actor->field_95E) {
        case 0:
            actor->field_934 = 0;
            if (func_8010BC70(coord) >= 0xE00) {
                mode             = 4;
                actor->field_95E = 2;
                actor->field_958 = 6;
            } else {
            resume:
                if (actor->field_95E != 3) {
                    actor->field_95E = 1;
                }
                actor->field_958 = 5;
                mode             = 2;
            }
            actor->field_973 = 1;
            Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
        case 1:
        case 2:
        case 3:
            dist = func_8010BC70(coord);
            if (dist < 0x301) {
                Gp_ResetActorMove(arg0, 0);
                break;
            }
            if (actor->field_95E == 3) {
                break;
            }
            actor->field_934++;
            if (actor->field_934 == 0xF0) {
                actor->field_95E = 3;
                goto resume;
            }
            angle = rand() & 0x3FF;
            if ((0x800 - angle) < dist) {
                goto in_range;
            }
            if (actor->field_95E == 2) {
                goto reset;
            }
        in_range:
            if (dist < angle + 0xC00) {
                break;
            }
            if (actor->field_95E != 1) {
                break;
            }
        reset:
            actor->field_95E = 0;
            break;
        default:
            break;
    }
    vec = (VECTOR3*)&target->coord.t[0];
    func_8010BD88(arg0, vec);
    func_8010BE5C(arg0, vec);
}

static void func_actor_800200_80164180(Task* arg0)
{
    GameActor*       actor;
    GpActorD4*       d4;
    GfxCoord*        target;
    WorldTargetNode* node;
    u8*              head;
    u8*              tmp;
    VECTOR3*         vec;
    GameActor*       actor2;
    s32              dist;
    s32              anim;
    u16              flag;

    actor                    = arg0->work;
    d4                       = actor->field_910;
    target                   = (gameGetPtrSlot(3))->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    tmp                      = head - 0x10;
    SCRATCH_STACK_CURSOR(u8) = tmp;
    vec                      = (VECTOR3*)tmp;
    node                     = actor->field_90C;
    if (node != NULL) {
        if (!(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            Gp_GetLockPos(node, vec);
        } else {
            actor->field_95E = 2;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = target->coord.t[0];
        vec->vy                       = target->coord.t[1];
        vec->vz                       = target->coord.t[2];
    }
    switch (actor->field_95E) {
        case 0:
            actor->field_95E = 1;
            actor->field_958 = 5;
            actor->field_973 = 1;
            if (func_8010BCF4(arg0, vec) < 0) {
                actor->field_975 = -1;
                anim             = 5;
            } else {
                actor->field_975 = 1;
                anim             = 6;
            }
            Gp_AnimPlayChildSlotsEx(arg0, anim, 1, 5);
        case 1:
        case 2:
            dist = func_8010BCF4(arg0, vec);
            if (dist < 0) {
                dist = -dist;
            }
            if ((dist < 0x101) || (actor->field_95E == 2)) {
                if ((s8)d4->repeatCount > 0) {
                    flag              = actor->field_90C != 0;
                    actor2            = arg0->work;
                    actor2->field_954 = 0;
                    actor2->field_956 = 4;
                    actor2->field_958 = 0;
                    actor2->field_95A = 0;
                    actor2->field_95C = 0;
                    actor2->field_95E = 0;
                    actor2->field_940 = flag;
                } else {
                    Gp_ResetActorMove(arg0, 0);
                }
            }
            break;
        default:
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)&target->coord.t[0]);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_800200_8016436C(Task* arg0)
{
    GameActor*       actor;
    GpActorD4*       d4;
    GfxCoord*        target;
    WorldTargetNode* node;
    u8*              tmp;
    GfxCoord*        coord;
    VECTOR3*         vec;
    u8*              head;
    void**           scratch;
    s8               count;
    s32              pan;
    s32              dist;
    u16              state;
    s32              next = 1;
    GameActor*       actor2;

    actor                    = arg0->work;
    d4                       = actor->field_910;
    target                   = (gameGetPtrSlot(3))->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    tmp                      = head - 0x10;
    SCRATCH_STACK_CURSOR(u8) = tmp;
    vec                      = (VECTOR3*)tmp;
    coord                    = arg0->extra.tmd->coords;
    if (actor->field_90C != NULL) {
        node             = Gp_FindLockNode(arg0);
        actor->field_90C = node;
        if ((node != NULL) && !(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            Gp_GetLockPos(node, vec);
        } else {
            d4->repeatCount = 1;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = target->coord.t[0];
        vec->vy                       = target->coord.t[1];
        vec->vz                       = target->coord.t[2];
    }
    state = actor->field_95E;
    if (state != 0) {
        if (state != 1) {
            scratch = SCRATCH_HEAD_ADDR;
        } else {
            goto tick;
        }
    } else {
        actor->field_95E = next;
        Gp_AnimPlayChildSlotsEx(arg0, actor->field_940 + 0xA, 0, 4);
        pan = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(actor->field_940 + 0x40720009, pan, (s8)gpGetObjDepth(coord));
    tick:
        if (func_80105894(arg0, 1, 0, 0) == 0) {
            dist = func_8010BCF4(arg0, vec);
            if (dist < 0) {
                dist = -dist;
            }
            if ((dist >= 0x281) && (func_80103DD4(MATRIX_TRANS(&coord->coord), vec) >= 0x201)) {
                actor2            = arg0->work;
                actor2->field_954 = 0;
                actor2->field_956 = 2;
                actor2->field_95A = 2;
                actor2->field_95C = 0;
                actor2->field_95E = 0;
            } else {
                count           = d4->repeatCount - 1;
                d4->repeatCount = count;
                if (count <= 0) {
                    Gp_ResetActorMove(arg0, 0);
                } else {
                    actor->field_95E = 0;
                }
            }
        }
        scratch = SCRATCH_HEAD_ADDR;
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

static void func_actor_800200_80164598(Task* arg0)
{
    GpApproachScratch* block;
    GfxCoord*          coord;
    GameActor*         actor;
    s32                val;
    s32                mode;
    s32                flag;

    actor           = arg0->work;
    coord           = arg0->extra.tmd->coords;
    block           = SCRATCH_STACK_RESERVE_BLOCK(GpApproachScratch);
    block->vec.vx   = actor->field_20 - coord->coord.t[0];
    block->vec.vy   = actor->field_24 - coord->coord.t[1];
    block->vec.vz   = actor->field_28 - coord->coord.t[2];
    actor->field_82 = ratan2(block->vec.vx, block->vec.vz);
    val             = func_80103E7C(actor->field_52, actor->field_82);
    block->field_0  = val;
    if (val > 0x30) {
        block->field_0 = 0x30;
    } else if (val < -0x30) {
        block->field_0 = -0x30;
    } else if (actor->field_95E == 0) {
        actor->field_95E = 1;
    }
    actor->field_52 = (actor->field_52 + block->field_0) & 0xFFF;
    switch (actor->field_95E) {
        case 0:
            flag             = 1;
            actor->field_95E = flag;
            actor->field_973 = flag;
            mode             = 6;
            if (block->field_0 < 0) {
                mode = 5;
            }
            Gp_AnimPlayChildSlots(arg0, mode, 1);
        case 1:
            if (block->field_0 == 0) {
                actor->field_958 = actor->field_934;
                actor->field_95E++;
                mode = 4;
                if ((u16)actor->field_934 == 5) {
                    mode = 2;
                }
                Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
            }
            break;
        case 2:
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0xC1 ||
                func_801041B4(arg0) != 0) {
                Gp_ResetActorMove(arg0, 0);
            } else {
                actor->field_973 = 1;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpApproachScratch);
}

static void func_actor_800200_801647A8(Task* arg0)
{
    GameActor*       actor;
    GameActor*       actor2;
    GameActor*       actor3;
    GfxCoord*        coord;
    GfxCoord*        target;
    WorldTargetNode* node;
    VECTOR3*         vec;
    u8*              head;
    u8*              tmp;
    s32              dist;
    s32              value;
    u16              flag;
    u16              state;
    s32              next;
    s32              initialState;

    target                   = (gameGetPtrSlot(3))->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    tmp                      = head - 0x10;
    SCRATCH_STACK_CURSOR(u8) = tmp;
    vec                      = (VECTOR3*)tmp;
    actor                    = arg0->work;
    coord                    = arg0->extra.tmd->coords;
    state                    = actor->field_95E;
    switch (state) {
        case 0:
            initialState     = 1;
            actor->field_95E = initialState;
            if (Gp_StateF0.prefix.bytes.field_0 == 1) {
                node             = Gp_FindLockNode(arg0);
                actor->field_90C = node;
                if ((node != NULL) && !(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
                    Gp_GetLockPos(node, vec);
                    dist = func_80103DD4(MATRIX_TRANS(&coord->coord), vec);
                    dist = dist / 640;
                    if (dist >= 8) {
                        dist = 7;
                    }
                    value = (rand() & 0x7F) + (dist << 5);
                    goto store;
                }
            }
            dist = func_8010BC70(coord);
            dist = dist / 640;
            if (dist >= 8) {
                dist = 7;
            }
            value            = (rand() & 0x1FF) - (dist * 0x30);
            actor->field_934 = value;
            if (value < 0x60) {
                value = 0x60;
            store:
                actor->field_934 = value;
            }
        case 1:
            value            = actor->field_934 - 1;
            actor->field_934 = value;
            if (value <= 0) {
                actor2                         = arg0->work;
                next                           = 1;
                actor2->field_910->repeatCount = next;
                if (Gp_StateF0.prefix.bytes.field_0 == next) {
                    actor2->field_90C = Gp_FindLockNode(arg0);
                } else {
                    actor2->field_90C = NULL;
                }
                flag              = actor2->field_90C != 0;
                actor3            = arg0->work;
                actor3->field_954 = 0;
                actor3->field_956 = 4;
                actor3->field_958 = 0;
                actor3->field_95A = 0;
                actor3->field_95C = 0;
                actor3->field_95E = 0;
                actor3->field_940 = flag;
            }
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)&target->coord.t[0]);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_800200_801649D8(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    s32        distance;
    s32        one;
    s32        turnAnim;
    s32        turn;
    s32        idleAnim;
    s32        delta;
    s32        value;
    s32        tick;
    s32        heading;
    u32        random;
    u16        target;

    actor    = arg0->work;
    d4       = actor->field_910;
    distance = _actor800200GetContactDistance(arg0->extra.tmd->coords, &d4->contact, 0);
    value    = actor->field_95E;
    one      = 1;
    switch (value) {
        case 0:
            if (d4->scanAngle < 0x1000) {
                if ((d4->scanDist != 0) && ((d4->scanDist < distance) || (distance == 0))) {
                    d4->scanDist      = (s16)distance;
                    d4->targetHeading = (u16)d4->scanAngle;
                }
                d4->scanAngle = (u16)d4->scanAngle + 0x80;
                return;
            }
            actor->field_95E  = one;
            target            = ((u16)d4->targetHeading + actor->field_52) & 0xFFF;
            d4->targetHeading = target;
            turn              = func_80103E7C((s16)actor->field_52, (s16)target) << 0x10;
            turnAnim          = 5;
            if (turn > 0) {
                turnAnim    = 6;
                d4->turnDir = 1;
            } else {
                d4->turnDir = -1;
            }
            Gp_AnimPlayChildSlotsEx(arg0, turnAnim, 0, 3);
            return;

        case 1:
            actor->field_975 = d4->turnDir;
            do {
                heading = (s16)actor->field_52;
                value   = (s16)d4->targetHeading;
                delta   = heading - value;
            } while (0);
            if (delta < 0) {
                delta = -delta;
            }
            if (delta < 0x40) {
                actor->field_95E += 1;
                actor->field_52   = (u16)d4->targetHeading;
                actor->field_975  = 0;
                if (Gp_StateF0.prefix.bytes.field_0 == 1) {
                    idleAnim         = 4;
                    actor->field_958 = 6;
                    random           = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState      = random;
                    random           = ((random >> 0x10) & 0x1F) + 0x14;
                    actor->field_934 = random;
                    Gp_AnimPlayChildSlotsEx(arg0, idleAnim, 0, 3);
                } else {
                    idleAnim         = 2;
                    actor->field_958 = 5;
                    random           = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState      = random;
                    random           = ((random >> 0x10) & 0x7F) + 0x3C;
                    actor->field_934 = random;
                    Gp_AnimPlayChildSlotsEx(arg0, idleAnim, 0, 3);
                }
                return;
            }
            return;

        case 2:
            if ((distance >= 0x341) || (distance == 0)) {
                tick             = actor->field_934 - 1;
                actor->field_934 = tick;
                if (tick <= 0) {
                reset:
                    Gp_ResetActorMove(arg0, 0);
                    break;
                }
                actor->field_973 = 1;
                break;
            }
            goto reset;
    }
}

static void func_actor_800200_80164C54(Task* arg0)
{
    GpApproachScratch* block;
    GfxCoord*          coord;
    GameActor*         actor;
    s32                val;
    s32                mode;

    actor           = arg0->work;
    coord           = arg0->extra.tmd->coords;
    block           = SCRATCH_STACK_RESERVE_BLOCK(GpApproachScratch);
    block->vec.vx   = actor->field_20 - coord->coord.t[0];
    block->vec.vy   = actor->field_24 - coord->coord.t[1];
    block->vec.vz   = actor->field_28 - coord->coord.t[2];
    actor->field_82 = ratan2(block->vec.vx, block->vec.vz);
    val             = func_80103E7C(actor->field_52, actor->field_82);
    block->field_0  = val;
    if (val > 0x40) {
        block->field_0 = 0x40;
    } else if (val < -0x40) {
        block->field_0 = -0x40;
    } else if (actor->field_95E == 0) {
        actor->field_95E = 1;
    }
    actor->field_52 = (actor->field_52 + block->field_0) & 0xFFF;
    switch (actor->field_95E) {
        case 0:
            actor->field_95E = 1;
            mode             = 6;
            if (block->field_0 < 0) {
                mode = 5;
            }
            Gp_AnimPlayChildSlots(arg0, mode, 1);
        case 1:
            if (block->field_0 == 0) {
                actor->field_958 = 5;
                actor->field_95E++;
                if (actor->field_93C == 0) {
                    mode = 2;
                    if (actor->field_91C == NULL) {
                        mode = 0x13;
                    }
                } else {
                    mode = actor->field_93C;
                }
                Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
            }
            break;
        case 2:
            if (abs(coord->coord.t[0] - actor->field_20) < 0x69) {
                if (abs(coord->coord.t[2] - actor->field_28) < 0x69) {
                    actor->field_982 = 0;
                    actor->field_956 = 1;
                    mode             = 1;
                    if (actor->field_93E != 0) {
                        mode = actor->field_93E;
                    }
                    Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
                    break;
                }
            }
            actor->field_973 = 1;
            Gp_StepPlayerMove(arg0);
            func_80105ED4(arg0);
            break;
    }
    Gp_AnimTickChildSlots(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(GpApproachScratch);
}

static void func_actor_800200_80164EBC(Task* arg0)
{
    GpApproachScratch* block;
    GfxCoord*          coord;
    GameActor*         actor;
    s32                val;
    s32                mode;

    actor           = arg0->work;
    coord           = arg0->extra.tmd->coords;
    block           = SCRATCH_STACK_RESERVE_BLOCK(GpApproachScratch);
    block->vec.vx   = actor->field_20 - coord->coord.t[0];
    block->vec.vy   = actor->field_24 - coord->coord.t[1];
    block->vec.vz   = actor->field_28 - coord->coord.t[2];
    actor->field_82 = ratan2(block->vec.vx, block->vec.vz);
    val             = func_80103E7C(actor->field_52, actor->field_82);
    block->field_0  = val;
    if (val > 0x40) {
        block->field_0 = 0x40;
    } else if (val < -0x40) {
        block->field_0 = -0x40;
    } else if (actor->field_95E == 0) {
        actor->field_95E = 1;
    }
    actor->field_52 = (actor->field_52 + block->field_0) & 0xFFF;
    switch (actor->field_95E) {
        case 0:
            actor->field_95E = 1;
            mode             = 6;
            if (block->field_0 < 0) {
                mode = 5;
            }
            Gp_AnimPlayChildSlots(arg0, mode, 1);
        case 1:
            if (block->field_0 == 0) {
                actor->field_958 = 6;
                actor->field_95E++;
                mode = 4;
                if (actor->field_93C != 0) {
                    mode = actor->field_93C;
                }
                Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
            }
            break;
        case 2:
            if (abs(coord->coord.t[0] - actor->field_20) < 0x69) {
                if (abs(coord->coord.t[2] - actor->field_28) < 0x69) {
                    actor->field_982 = 0;
                    actor->field_956 = 1;
                    mode             = 1;
                    if (actor->field_93E != 0) {
                        mode = actor->field_93E;
                    }
                    Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
                    break;
                }
            }
            actor->field_973 = 1;
            Gp_StepPlayerMove(arg0);
            break;
    }
    Gp_AnimTickChildSlots(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(GpApproachScratch);
}

/// Handlers `func_actor_800200_80165B84` runs, indexed by `field_956`.
static const TaskFuncTable12 D_actor_800200_80161E5C = { {
    func_actor_800200_80165CB4,
    func_actor_800200_80163F5C,
    func_actor_800200_80164180,
    func_actor_800200_80165CB4,
    func_actor_800200_8016436C,
    func_actor_800200_80165CB4,
    func_actor_800200_80165CB4,
    func_actor_800200_80165D44,
    func_actor_800200_80164598,
    func_actor_800200_801647A8,
    func_actor_800200_801649D8,
    func_actor_800200_80165E50,
} };

/// Handlers `func_actor_800200_80165CB4` runs, indexed by the low nibble of
/// the task's `spawnArg1`.
static const TaskFuncTable11 D_actor_800200_80161E8C = { {
    func_actor_800200_80162750,
    func_actor_800200_80165580,
    func_actor_800200_80162750,
    func_actor_800200_80162E0C,
    func_actor_800200_80162750,
    func_actor_800200_80162750,
    func_actor_800200_80162750,
    func_actor_800200_80165644,
    func_actor_800200_80165708,
    func_actor_800200_80165708,
    func_actor_800200_80165708,
} };

/// Handlers `func_actor_800200_80165E90` runs, indexed by `field_96C`.
static const TaskFuncTable4 D_actor_800200_80161EB8 = { {
    func_actor_800200_80165F28,
    func_actor_800200_80165F28,
    func_actor_800200_80165F28,
    func_actor_800200_80165F48,
} };

/// Handlers `func_actor_800200_80165F50` runs, indexed by `field_956`; the
/// gameplay entries are the player's own mode-2 state handlers.
static const TaskFuncTable9 D_actor_800200_80161EC8 = { {
    Gp_PlayerMode2State0,
    Gp_PlayerMode2State1,
    func_actor_800200_80165FF0,
    Gp_PlayerMode2State1,
    func_actor_800200_80164C54,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State1,
    func_actor_800200_80164EBC,
} };

static s32 func_actor_800200_80165104(Task* arg0)
{
    GameActor*             actor;
    const AnimationRecord* rec;
    GfxCoord*              obj;
    GpRoomParamRec*        param;
    s32*                   sounds;
    s32                    ret;
    s32                    sound;
    s8                     cueBits;
    s32                    pan;

    ret   = 0;
    sound = 0;
    actor = arg0->work;
    obj   = arg0->extra.tmd->coords;
    rec   = Gp_AnimGetRec((AnimationContext*)actor->field_424, actor->field_438 + 1);
    if (rec != NULL && rec != actor->field_92C) {
        actor->field_92C = rec;
        switch (cueBits = rec->flags & ANIMATION_RECORD_CUE_MASK) {
            case ANIMATION_RECORD_CUE_1:
            case ANIMATION_RECORD_CUE_2:
                param  = Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][actor->field_930];
                sounds = param->field_4;
                if (sounds != NULL) {
                    if ((u16)actor->field_958 - 5 < 2U) {
                        switch (sounds[0]) {
                            case 0x10000015:
                                sound = 0x40720007;
                                break;
                            case 0x1000002D:
                                sound = 0x40720003;
                                break;
                            case 0x1000001D:
                            case 0x10000049:
                                sound = 0x40720001;
                                break;
                            case 0x1000003D:
                            case 0x10000041:
                            case 0x10000051:
                            case 0x10000059:
                            case 0x1000005D:
                                sound = 0x40720005;
                                break;
                        }
                        if (cueBits == ANIMATION_RECORD_CUE_1) {
                            sound++;
                        }
                        if ((u16)actor->field_958 == 6) {
                            Gp_SetStateF0Bit(5);
                        }
                    }
                    if (sound != 0) {
                        pan = (s8)Gp_GetObjPan(obj);
                        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(obj));
                        func_800EA3A0(cueBits != ANIMATION_RECORD_CUE_2);
                    }
                }
                ret = 1;
                break;
        }
    }
    return ret;
}

static void func_actor_800200_801652EC(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable3 sp;

    sp    = D_actor_800200_80161E34;
    actor = arg0->work;
    if ((s8)actor->field_97A > 0) {
        actor->field_97A--;
    }
    sp.funcs[actor->field_954](arg0);
    func_actor_800200_80165104(arg0);
    actor->field_986 = 0;
}

static void func_actor_800200_80165380(Task* arg0)
{
    GameActor* actor = arg0->work;

    actor->field_954 = 0;
    actor->field_956 = 1;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
}

static void func_actor_800200_801653A0(Task* arg0)
{
    GameActor* actor = arg0->work;

    actor->field_954 = 0;
    actor->field_956 = 2;
    actor->field_95A = 2;
    actor->field_95C = 0;
    actor->field_95E = 0;
}

static void func_actor_800200_801653C0(Task* arg0)
{
    GameActor* actor = arg0->work;

    actor->field_954 = 0;
    actor->field_956 = 7;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 7;
    actor->field_95E = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 7, 0, 3);
}

static void func_actor_800200_80165408(Task* arg0, s32 arg1)
{
    GameActor* actor = arg0->work;

    actor->field_956 = 8;
    actor->field_954 = 0;
    actor->field_958 = 5;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_934 = arg1;
}

static void func_actor_800200_80165434(Task* arg0, s16 arg1)
{
    GameActor* actor = arg0->work;

    actor->field_954 = 0;
    actor->field_956 = 4;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_940 = arg1;
}

static void func_actor_800200_8016545C(Task* arg0, s8 arg1)
{
    GameActor* actor = arg0->work;
    GameActor* actor2;
    u16        flag;

    actor->field_910->repeatCount = arg1;
    if (Gp_StateF0.prefix.bytes.field_0 == 1) {
        actor->field_90C = Gp_FindLockNode(arg0);
    } else {
        actor->field_90C = 0;
    }
    flag              = actor->field_90C != 0;
    actor2            = arg0->work;
    actor2->field_954 = 0;
    actor2->field_956 = 4;
    actor2->field_958 = 0;
    actor2->field_95A = 0;
    actor2->field_95C = 0;
    actor2->field_95E = 0;
    actor2->field_940 = flag;
}

static void func_actor_800200_801654EC(Task* arg0, s32 arg1)
{
    GameActor* actor = arg0->work;

    actor->field_954 = 0;
    actor->field_956 = 9;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 3);
}

static void func_actor_800200_80165534(Task* arg0)
{
    GameActor* actor = arg0->work;

    actor->field_956 = 0xB;
    actor->field_954 = 0;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 7;
    actor->field_95E = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 0xE, 0, 3);
}

static void func_actor_800200_80165580(Task* arg0)
{
    u8 temp_v1;

    if (((GameActor*)arg0->work)->field_910->pathDone == 1) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v1 = gGameSession->location.loc.area;
    switch (temp_v1) {
        case 26:
            func_actor_800200_80162990(arg0);
            return;
        case 24:
            func_actor_800200_80165814(arg0);
            return;
        case 23:
            func_actor_800200_80162BFC(arg0);
            return;
        case 25:
            func_actor_800200_801658E0(arg0);
            return;
    }
}

static void func_actor_800200_80165644(Task* arg0)
{
    u8 temp_v1;

    if (((GameActor*)arg0->work)->field_910->pathDone == 1) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v1 = gGameSession->location.loc.area;
    switch (temp_v1) {
        case 25:
            func_actor_800200_8016599C(arg0);
            return;
        case 23:
            func_actor_800200_80163044(arg0);
            return;
        case 22:
            func_actor_800200_80163180(arg0);
            return;
        case 20:
            func_actor_800200_8016337C(arg0);
            return;
    }
}

static void func_actor_800200_80165708(Task* arg0)
{
    u8 temp_v0;

    if (((GameActor*)arg0->work)->field_910->pathDone == 1) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v0 = gGameSession->location.loc.area;
    switch (temp_v0) {
        case 1:
            func_actor_800200_80163A54(arg0);
            return;
        case 2:
            func_actor_800200_801637B4(arg0);
            return;
        case 3:
            func_actor_800200_801659CC(arg0);
            return;
        case 4:
            func_actor_800200_80163584(arg0);
            return;
        case 5:
            func_actor_800200_8016390C(arg0);
            return;
        case 15:
            func_actor_800200_80163E14(arg0);
            return;
        case 19:
            func_actor_800200_80163CCC(arg0);
            return;
        case 20:
            func_actor_800200_80163B90(arg0);
            return;
        case 24:
            func_actor_800200_80165ACC(arg0);
            return;
    }
}

static void func_actor_800200_80165814(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    s32        arg;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    d4    = actor->field_910;
    if (actor->field_960 == 0) {
        actor->field_20 = D_actor_800200_8016A018[d4->pathStep].field_0;
        actor->field_24 = coord->coord.t[1];
        actor->field_28 = D_actor_800200_8016A018[d4->pathStep].field_4;
        if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
            d4->pathDone = 1;
            func_actor_800200_801654EC(arg0, 0);
            return;
        }
        arg = 6;
        if (d4->pathStep == 2) {
            arg = 5;
        }
        func_actor_800200_80165408(arg0, arg);
    }
}

static void func_actor_800200_801658E0(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    d4    = actor->field_910;
    if (actor->field_960 == 0) {
        actor->field_20 = D_actor_800200_8016A040[d4->pathStep].field_0;
        actor->field_24 = coord->coord.t[1];
        actor->field_28 = D_actor_800200_8016A040[d4->pathStep].field_4;
        if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
            d4->pathDone = 1;
            func_actor_800200_801654EC(arg0, 0);
            return;
        }
        func_actor_800200_80165408(arg0, 6);
    }
}

static void func_actor_800200_8016599C(Task* arg0)
{
    ((GameActor*)arg0->work)->field_910->pathDone = 1;
    func_actor_800200_801654EC(arg0, 0);
}

static void func_actor_800200_801659CC(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;
    u32        state;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_20 = D_actor_800200_8016A090[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A090[d4->pathStep].field_4;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
                actor->field_960++;
                if (d4->pathDone != 1) {
                    func_actor_800200_80165534(arg0);
                }
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        case 1:
            d4->pathDone = state;
            func_actor_800200_801654EC(arg0, 0);
            break;
    }
}

static void func_actor_800200_80165ACC(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GfxCoord*  coord;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    d4    = actor->field_910;
    if (actor->field_960 == 0) {
        actor->field_20 = D_actor_800200_8016A128[d4->pathStep].field_0;
        actor->field_24 = coord->coord.t[1];
        actor->field_28 = D_actor_800200_8016A128[d4->pathStep].field_4;
        if (func_80103DD4(MATRIX_TRANS(&coord->coord), (VECTOR3*)&actor->field_20) < 0x401) {
            d4->pathDone = 1;
            func_actor_800200_80165534(arg0);
            return;
        }
        func_actor_800200_80165408(arg0, 6);
    }
}

static void func_actor_800200_80165B84(Task* arg0)
{
    GameActor*      actor;
    GpActorD4*      d4;
    GfxCoord*       coord;
    TaskFuncTable12 sp;
    s32             pan;

    sp    = D_actor_800200_80161E5C;
    actor = arg0->work;
    d4    = actor->field_910;
    coord = arg0->extra.tmd->coords;
    if (d4->decisionTimer > 0) {
        d4->decisionTimer--;
    }
    sp.funcs[actor->field_956](arg0);
    if ((s8)actor->field_97A == 0) {
        func_80109BB4(arg0, actor->field_17C);
        if ((u16)actor->field_96C != 0) {
            func_8010B9A4(arg0);
            pan = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(0x4072000A, pan, (s8)gpGetObjDepth(coord));
        }
    }
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

static void func_actor_800200_80165CB4(Task* arg0)
{
    TaskFuncTable11 sp;

    sp = D_actor_800200_80161E8C;
    sp.funcs[arg0->spawnArg1.value & 0xF](arg0);
}

static void func_actor_800200_80165D44(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  target;

    coord  = arg0->extra.tmd->coords;
    target = (gameGetPtrSlot(3))->extra.tmd->coords;
    actor  = arg0->work;
    switch (actor->field_95E) {
        case 1:
            actor->field_95C  = 0;
            actor->field_95E += 1;
            Gp_AnimResetChildSlots(arg0, 9);
        case 2:
            if ((func_8010BC70(coord) >= 0x500) || (Gp_StateF0.prefix.bytes.field_0 == 1)) {
                actor->field_95C  = 7;
                actor->field_95E += 1;
                Gp_AnimPlayChildSlotsEx(arg0, 8, 0, 3);
            }
            break;
        case 4:
            Gp_ResetActorMove(arg0, 0);
            break;
        default:
        case 0:
        case 3:
            break;
    }
    func_8010BE5C(arg0, MATRIX_TRANS(&target->coord));
}

static void func_actor_800200_80165E50(Task* arg0)
{
    u16 state = ((GameActor*)arg0->work)->field_95E;

    if (state != 0) {
        if (state == 1) {
            Gp_ResetActorMove(arg0, 0);
        }
    }
}

static void func_actor_800200_80165E90(Task* arg0)
{
    TaskFuncTable4 sp;
    GameActor*     actor;

    sp    = D_actor_800200_80161EB8;
    actor = arg0->work;
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    sp.funcs[(u16)actor->field_96C](arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

static void func_actor_800200_80165F28(Task* arg0)
{
    func_8010ABD4(arg0);
}

static void func_actor_800200_80165F48(Task* arg0)
{
}

static void func_actor_800200_80165F50(Task* arg0)
{
    TaskFuncTable9 sp;
    GameActor*     actor;
    GfxCoord*      coord;

    sp    = D_actor_800200_80161EC8;
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    sp.funcs[actor->field_956](arg0);
    RotMatrix((SVECTOR*)&actor->field_50, &coord->coord);
}

static void func_actor_800200_80165FF0(Task* arg0)
{
    GameActor* actor;
    s16        cur;
    s16        tgt;
    u16        raw;
    s32        temp;
    s32        wrap;
    s32        delta;
    s32        flag;

    actor = arg0->work;
    cur   = actor->field_52;
    tgt   = actor->field_82;
    raw   = actor->field_82;
    temp  = cur - tgt;
    if (temp < 0) {
        temp = -temp;
    }
    if (temp < 0x31 || (wrap = tgt - 0x1000, temp = cur - wrap, temp = ABS(temp), temp < 0x31)) {
        flag             = 1;
        actor->field_52  = raw;
        actor->field_982 = 0;
        actor->field_956 = flag;
        Gp_AnimPlayChildSlotsEx(arg0, flag, 0, 5);
    } else {
        delta = func_80103E7C(cur, tgt);
        if (delta > 0x30) {
            delta = 0x30;
        } else if (delta < -0x30) {
            delta = -0x30;
        }
        actor->field_958 = 5;
        actor->field_973 = 1;
        actor->field_52  = ((u16)actor->field_52 + delta) & 0xFFF;
    }
    Gp_AnimTickChildSlots(arg0);
}

/// Planar distance from the coordinate origin to a nonempty contact, or 0.
///
/// Distances use world units. Optional `contactZY` needs two halfwords and
/// receives the signed coordinate bits (Z, Y). The original X, Y, Z stores
/// deliberately retain their order, with Z overwriting X.
static s32 _actor800200GetContactDistance(GfxCoord* coord, WorldCollisionContact* contact, u16* contactZY)
{
    s32 distance;

    if (contact->key.value != 0) {
        distance = func_80103D8C(coord->workm.t[0] - contact->point.vx, coord->workm.t[2] - contact->point.vz);
        if (contactZY != NULL) {
            // Retain the original repeated first-halfword write.
            contactZY[0] = contact->point.vx;
            contactZY[1] = contact->point.vy;
            contactZY[0] = contact->point.vz;
        }
    } else {
        distance = 0;
    }
    return distance;
}
