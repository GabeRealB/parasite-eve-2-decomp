#include "actors/actor_521100.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actor_521100_private.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/pairsrc.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

typedef struct {
    s32 id;
    union {
        s16 (*call0)(Task*);
        s32 (*call1)(Task*);
        s32 (*call2)(Task*, s32, AnimationPlayRequest*);
        s32 (*call3)(Task*, s32, ActorCommand* request);
        s32 (*call4)(Task*, s32, ActorTransform*);
        s32 (*call5)(Task*, s32, s32);
    } handler;
} Actor521100MessageEntry;
STATIC_ASSERT_SIZEOF(Actor521100MessageEntry, 8);

extern Actor521100MessageEntry D_actor_521100_8015F6FC[8];

s32 func_actor_521100_80135BEC(Task*);

s32 func_actor_521100_80135C14(Task*, s32, AnimationPlayRequest*);

s32 func_actor_521100_80135CAC(Task*, s32, ActorTransform* args);

typedef struct Actor521100FireScratch {
    /* 0x00 */ VECTOR               pos;
    /* 0x10 */ VECTOR               delta;
    /* 0x20 */ SVECTOR              vec;
    /* 0x28 */ AnimationPlayRequest msg;
    /* 0x3C */ ActorTransform       aim;
} Actor521100FireScratch;
STATIC_ASSERT_SIZEOF(Actor521100FireScratch, 0x54);

extern s16 D_actor_521100_8015F570[];

extern s16 D_actor_521100_8015F684[];

/// The three waypoints the state-6 body `func_actor_521100_80134774` walks the
/// actor to, one per phase `field_6A0` it switches on: `(-4000, 0, -2000)` for
/// phases 0 and 2, and `(-5250, 0, -1200)` for phase 1. Only `vx` and `vz` are
/// read, and only when the actor is too far from the player for that phase to
/// aim at it; the y of all three is zero, as the positions are on the floor.
extern VECTOR D_actor_521100_8015F654[];

/// Sixteen frames of the burn-out effect the state bodies at `field_6A0 == 1`
/// pick between on their last frame, indexed by the 4 bits under the top half
/// of an LCG draw (`(rng >> 16) & 0xF`). The sibling state body
/// `func_actor_521100_801357F0` reads the table one slot down at 0x8015F5F4.
extern u16 D_actor_521100_8015F634[];

/// The other sixteen-frame burn-out table, read by the state-5 body
/// `func_actor_521100_801357F0` off the same LCG draw bits the state-3 body
/// `func_actor_521100_8013570C` indexes `D_actor_521100_8015F634` with.
extern u16 D_actor_521100_8015F5F4[];

/// The burn-out effect table the sequence resets read, one 0x20-byte table
/// below `D_actor_521100_8015F5F4`: the state-1 body
/// `func_actor_521100_801335B4` draws from it both on the frame the actor
/// catches alight and on the frame the reset latch `field_6AA` has run out.
extern u16 D_actor_521100_8015F5D4[];

/// The 4-byte pair `func_actor_521100_801335B4` packs a type-2 record into and
/// copies onto both `obj57C` / `obj59C` at `field_18`, where the sibling
/// overlays' burn-out bodies put the same pair. Same shape as
/// `D_actor_510900_80167968` and `D_actor_400100_*`.
extern DamageAttack D_actor_521100_8015F550[4];

/// One signed halfword choice in a three-row, two-choice transition table.
/// The selector combines the row and random-column byte offsets before
/// accessing this member. The member access also keeps GCC's structure-memory
/// annotation, allowing the independent RNG write to retain its schedule.
typedef struct Actor521100StateChoice {
    s16 state;
} Actor521100StateChoice;
STATIC_ASSERT_SIZEOF(Actor521100StateChoice, 2);

extern s16                    D_actor_521100_8015F57C[16];
extern Actor521100StateChoice D_actor_521100_8015F59C[6];
extern s16                    D_actor_521100_8015F5A8[16];
extern Actor521100StateChoice D_actor_521100_8015F5C8[6];

extern u16 D_actor_521100_8015F614[];

/// Main-executable global with no module header yet: the remaining-enemy count.

extern GpPairSrcE D_actor_521100_8015F560;
extern TaskDesc   D_actor_521100_8015F6E4[];

static void func_actor_521100_801322F8(Task* arg0, TmdObject* arg1, s32 arg2);
static void func_actor_521100_80132958(Task* arg0);
static s32  func_actor_521100_80132C70(Task* arg0);
static void func_actor_521100_80132DE8(Task* arg0);
static void func_actor_521100_80133104(Task* arg0);
static void func_actor_521100_8013334C(Task* arg0);
static void func_actor_521100_801335B4(Task* arg0);
static void func_actor_521100_801339B0(Task* arg0);
static void func_actor_521100_80134658(Task* arg0);
static void func_actor_521100_80134774(Task* arg0);
static void func_actor_521100_80134C38(Task* arg0);
static void func_actor_521100_80134D88(Task* arg0);
static void func_actor_521100_80134EDC(Task* arg0);
static void func_actor_521100_80135024(Task* arg0);
static void func_actor_521100_80135230(Task* arg0);
static void func_actor_521100_801353CC(GpEnemy* arg0, Task* arg1);
static void func_actor_521100_80135414(GpEnemy* arg0, Task* arg1);
static void func_actor_521100_80135478(GpEnemy* arg0, Task* arg1);
static void func_actor_521100_801355C8(Task* arg0);
static void func_actor_521100_80135680(Task* arg0);
static void func_actor_521100_8013570C(Task* arg0);
static void func_actor_521100_801357F0(Task* arg0);
static void func_actor_521100_801358D4(Task* arg0);
static void func_actor_521100_80135964(Task* arg0);
static void func_actor_521100_80135A34(Task* arg0);
static void func_actor_521100_80135A90(Task* arg0);
static void func_actor_521100_80135B40(GpEnemy* enemy, Task* task);
static void func_actor_521100_80135B80(GpEnemy* arg0, Task* task);

extern TmdSource D_actor_521100_80141894;
extern TmdSource D_actor_521100_80142098;
void             func_actor_521100_80135378(Task*);
void             func_actor_521100_80135AE4(Task*);

AnimationPackedPose D_actor_521100_80136CB0[6] = {
#include "assets/actor_521100_animation_05474_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80136CF8[148] = {
#include "assets/actor_521100_animation_05474_bank4.inc"
};

AnimationRecord D_actor_521100_80136F48[201] = {
#include "assets/actor_521100_animation_05474_records.inc"
};

u16 D_actor_521100_8013726C[20] = {
#include "assets/actor_521100_animation_05474_indices.inc"
};

AnimationSet D_actor_521100_80137294 = {
    D_actor_521100_80136F48,
    D_actor_521100_8013726C,
    { NULL, D_actor_521100_80136CB0, NULL, NULL, D_actor_521100_80136CF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_801372BC[9] = {
#include "assets/actor_521100_animation_059C4_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80137328[131] = {
#include "assets/actor_521100_animation_059C4_bank4.inc"
};

AnimationRecord D_actor_521100_80137534[162] = {
#include "assets/actor_521100_animation_059C4_records.inc"
};

u16 D_actor_521100_801377BC[20] = {
#include "assets/actor_521100_animation_059C4_indices.inc"
};

AnimationSet D_actor_521100_801377E4 = {
    D_actor_521100_80137534,
    D_actor_521100_801377BC,
    { NULL, D_actor_521100_801372BC, NULL, NULL, D_actor_521100_80137328, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8013780C[8] = {
#include "assets/actor_521100_animation_05F98_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8013786C[117] = {
#include "assets/actor_521100_animation_05F98_bank4.inc"
};

AnimationRecord D_actor_521100_80137A40[212] = {
#include "assets/actor_521100_animation_05F98_records.inc"
};

u16 D_actor_521100_80137D90[20] = {
#include "assets/actor_521100_animation_05F98_indices.inc"
};

AnimationSet D_actor_521100_80137DB8 = {
    D_actor_521100_80137A40,
    D_actor_521100_80137D90,
    { NULL, D_actor_521100_8013780C, NULL, NULL, D_actor_521100_8013786C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80137DE0[5] = {
#include "assets/actor_521100_animation_0623C_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80137E1C[32] = {
#include "assets/actor_521100_animation_0623C_bank4.inc"
};

AnimationRecord D_actor_521100_80137E9C[102] = {
#include "assets/actor_521100_animation_0623C_records.inc"
};

u16 D_actor_521100_80138034[20] = {
#include "assets/actor_521100_animation_0623C_indices.inc"
};

AnimationSet D_actor_521100_8013805C = {
    D_actor_521100_80137E9C,
    D_actor_521100_80138034,
    { NULL, D_actor_521100_80137DE0, NULL, NULL, D_actor_521100_80137E1C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80138084[47] = {
#include "assets/actor_521100_animation_07168_bank1.inc"
};

AnimationPackedRotation D_actor_521100_801382B8[243] = {
#include "assets/actor_521100_animation_07168_bank4.inc"
};

AnimationRecord D_actor_521100_80138684[567] = {
#include "assets/actor_521100_animation_07168_records.inc"
};

u16 D_actor_521100_80138F60[20] = {
#include "assets/actor_521100_animation_07168_indices.inc"
};

AnimationSet D_actor_521100_80138F88 = {
    D_actor_521100_80138684,
    D_actor_521100_80138F60,
    { NULL, D_actor_521100_80138084, NULL, NULL, D_actor_521100_801382B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80138FB0[16] = {
#include "assets/actor_521100_animation_07BA4_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80139070[246] = {
#include "assets/actor_521100_animation_07BA4_bank4.inc"
};

AnimationRecord D_actor_521100_80139448[341] = {
#include "assets/actor_521100_animation_07BA4_records.inc"
};

u16 D_actor_521100_8013999C[20] = {
#include "assets/actor_521100_animation_07BA4_indices.inc"
};

AnimationSet D_actor_521100_801399C4 = {
    D_actor_521100_80139448,
    D_actor_521100_8013999C,
    { NULL, D_actor_521100_80138FB0, NULL, NULL, D_actor_521100_80139070, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_801399EC[14] = {
#include "assets/actor_521100_animation_0820C_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80139A94[148] = {
#include "assets/actor_521100_animation_0820C_bank4.inc"
};

AnimationRecord D_actor_521100_80139CE4[200] = {
#include "assets/actor_521100_animation_0820C_records.inc"
};

u16 D_actor_521100_8013A004[20] = {
#include "assets/actor_521100_animation_0820C_indices.inc"
};

AnimationSet D_actor_521100_8013A02C = {
    D_actor_521100_80139CE4,
    D_actor_521100_8013A004,
    { NULL, D_actor_521100_801399EC, NULL, NULL, D_actor_521100_80139A94, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8013A054[25] = {
#include "assets/actor_521100_animation_09030_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8013A180[369] = {
#include "assets/actor_521100_animation_09030_bank4.inc"
};

AnimationRecord D_actor_521100_8013A744[441] = {
#include "assets/actor_521100_animation_09030_records.inc"
};

u16 D_actor_521100_8013AE28[20] = {
#include "assets/actor_521100_animation_09030_indices.inc"
};

AnimationSet D_actor_521100_8013AE50 = {
    D_actor_521100_8013A744,
    D_actor_521100_8013AE28,
    { NULL, D_actor_521100_8013A054, NULL, NULL, D_actor_521100_8013A180, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8013AE78[2] = {
#include "assets/actor_521100_animation_091C0_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8013AE90[17] = {
#include "assets/actor_521100_animation_091C0_bank4.inc"
};

AnimationRecord D_actor_521100_8013AED4[57] = {
#include "assets/actor_521100_animation_091C0_records.inc"
};

u16 D_actor_521100_8013AFB8[20] = {
#include "assets/actor_521100_animation_091C0_indices.inc"
};

AnimationSet D_actor_521100_8013AFE0 = {
    D_actor_521100_8013AED4,
    D_actor_521100_8013AFB8,
    { NULL, D_actor_521100_8013AE78, NULL, NULL, D_actor_521100_8013AE90, NULL, NULL, NULL },
};

TmdBone D_actor_521100_8013B008[19] = {
#include "assets/actor_521100_model_0FA74_skeleton.inc"
};

u32 D_actor_521100_8013B2B4[19] = {
#include "assets/actor_521100_model_0FA74_partVerts.inc"
};

SVECTOR D_actor_521100_8013B300[432] = {
#include "assets/actor_521100_model_0FA74_verts.inc"
};

SVECTOR D_actor_521100_8013C080[444] = {
#include "assets/actor_521100_model_0FA74_normals.inc"
};

u32 D_actor_521100_8013CE60[4749] = {
#include "assets/actor_521100_model_0FA74_stream.inc"
};

TmdSource D_actor_521100_80141894 = {
    0,
    26564,
    6624,
    19,
    D_actor_521100_8013B2B4,
    D_actor_521100_8013B300,
    D_actor_521100_8013C080,
    D_actor_521100_8013B008,
    D_actor_521100_8013CE60,
};

TmdBone D_actor_521100_801418B8[1] = {
#include "assets/actor_521100_model_10278_skeleton.inc"
};

u32 D_actor_521100_801418DC[1] = {
#include "assets/actor_521100_model_10278_partVerts.inc"
};

SVECTOR D_actor_521100_801418E0[43] = {
#include "assets/actor_521100_model_10278_verts.inc"
};

SVECTOR D_actor_521100_80141A38[41] = {
#include "assets/actor_521100_model_10278_normals.inc"
};

u32 D_actor_521100_80141B80[326] = {
#include "assets/actor_521100_model_10278_stream.inc"
};

TmdSource D_actor_521100_80142098 = {
    0,
    2300,
    0,
    1,
    D_actor_521100_801418DC,
    D_actor_521100_801418E0,
    D_actor_521100_80141A38,
    D_actor_521100_801418B8,
    D_actor_521100_80141B80,
};

AnimationPackedPose D_actor_521100_801420BC[21] = {
#include "assets/actor_521100_animation_10EAC_bank1.inc"
};

AnimationPackedRotation D_actor_521100_801421B8[317] = {
#include "assets/actor_521100_animation_10EAC_bank4.inc"
};

AnimationRecord D_actor_521100_801426AC[382] = {
#include "assets/actor_521100_animation_10EAC_records.inc"
};

u16 D_actor_521100_80142CA4[20] = {
#include "assets/actor_521100_animation_10EAC_indices.inc"
};

AnimationSet D_actor_521100_80142CCC = {
    D_actor_521100_801426AC,
    D_actor_521100_80142CA4,
    { NULL, D_actor_521100_801420BC, NULL, NULL, D_actor_521100_801421B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80142CF4[12] = {
#include "assets/actor_521100_animation_11614_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80142D84[187] = {
#include "assets/actor_521100_animation_11614_bank4.inc"
};

AnimationRecord D_actor_521100_80143070[231] = {
#include "assets/actor_521100_animation_11614_records.inc"
};

u16 D_actor_521100_8014340C[20] = {
#include "assets/actor_521100_animation_11614_indices.inc"
};

AnimationSet D_actor_521100_80143434 = {
    D_actor_521100_80143070,
    D_actor_521100_8014340C,
    { NULL, D_actor_521100_80142CF4, NULL, NULL, D_actor_521100_80142D84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8014345C[20] = {
#include "assets/actor_521100_animation_12204_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8014354C[321] = {
#include "assets/actor_521100_animation_12204_bank4.inc"
};

AnimationRecord D_actor_521100_80143A50[363] = {
#include "assets/actor_521100_animation_12204_records.inc"
};

u16 D_actor_521100_80143FFC[20] = {
#include "assets/actor_521100_animation_12204_indices.inc"
};

AnimationSet D_actor_521100_80144024 = {
    D_actor_521100_80143A50,
    D_actor_521100_80143FFC,
    { NULL, D_actor_521100_8014345C, NULL, NULL, D_actor_521100_8014354C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8014404C[5] = {
#include "assets/actor_521100_animation_1271C_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80144088[99] = {
#include "assets/actor_521100_animation_1271C_bank4.inc"
};

AnimationRecord D_actor_521100_80144214[192] = {
#include "assets/actor_521100_animation_1271C_records.inc"
};

u16 D_actor_521100_80144514[20] = {
#include "assets/actor_521100_animation_1271C_indices.inc"
};

AnimationSet D_actor_521100_8014453C = {
    D_actor_521100_80144214,
    D_actor_521100_80144514,
    { NULL, D_actor_521100_8014404C, NULL, NULL, D_actor_521100_80144088, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80144564[21] = {
#include "assets/actor_521100_animation_13454_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80144660[354] = {
#include "assets/actor_521100_animation_13454_bank4.inc"
};

AnimationRecord D_actor_521100_80144BE8[409] = {
#include "assets/actor_521100_animation_13454_records.inc"
};

u16 D_actor_521100_8014524C[20] = {
#include "assets/actor_521100_animation_13454_indices.inc"
};

AnimationSet D_actor_521100_80145274 = {
    D_actor_521100_80144BE8,
    D_actor_521100_8014524C,
    { NULL, D_actor_521100_80144564, NULL, NULL, D_actor_521100_80144660, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8014529C[16] = {
#include "assets/actor_521100_animation_13B8C_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8014535C[177] = {
#include "assets/actor_521100_animation_13B8C_bank4.inc"
};

AnimationRecord D_actor_521100_80145620[217] = {
#include "assets/actor_521100_animation_13B8C_records.inc"
};

u16 D_actor_521100_80145984[20] = {
#include "assets/actor_521100_animation_13B8C_indices.inc"
};

AnimationSet D_actor_521100_801459AC = {
    D_actor_521100_80145620,
    D_actor_521100_80145984,
    { NULL, D_actor_521100_8014529C, NULL, NULL, D_actor_521100_8014535C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_801459D4[14] = {
#include "assets/actor_521100_animation_143F0_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80145A7C[220] = {
#include "assets/actor_521100_animation_143F0_bank4.inc"
};

AnimationRecord D_actor_521100_80145DEC[255] = {
#include "assets/actor_521100_animation_143F0_records.inc"
};

u16 D_actor_521100_801461E8[20] = {
#include "assets/actor_521100_animation_143F0_indices.inc"
};

AnimationSet D_actor_521100_80146210 = {
    D_actor_521100_80145DEC,
    D_actor_521100_801461E8,
    { NULL, D_actor_521100_801459D4, NULL, NULL, D_actor_521100_80145A7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80146238[9] = {
#include "assets/actor_521100_animation_14918_bank1.inc"
};

AnimationPackedRotation D_actor_521100_801462A4[124] = {
#include "assets/actor_521100_animation_14918_bank4.inc"
};

AnimationRecord D_actor_521100_80146494[159] = {
#include "assets/actor_521100_animation_14918_records.inc"
};

u16 D_actor_521100_80146710[20] = {
#include "assets/actor_521100_animation_14918_indices.inc"
};

AnimationSet D_actor_521100_80146738 = {
    D_actor_521100_80146494,
    D_actor_521100_80146710,
    { NULL, D_actor_521100_80146238, NULL, NULL, D_actor_521100_801462A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80146760[5] = {
#include "assets/actor_521100_animation_14C38_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8014679C[55] = {
#include "assets/actor_521100_animation_14C38_bank4.inc"
};

AnimationRecord D_actor_521100_80146878[110] = {
#include "assets/actor_521100_animation_14C38_records.inc"
};

u16 D_actor_521100_80146A30[20] = {
#include "assets/actor_521100_animation_14C38_indices.inc"
};

AnimationSet D_actor_521100_80146A58 = {
    D_actor_521100_80146878,
    D_actor_521100_80146A30,
    { NULL, D_actor_521100_80146760, NULL, NULL, D_actor_521100_8014679C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80146A80[10] = {
#include "assets/actor_521100_animation_15168_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80146AF8[121] = {
#include "assets/actor_521100_animation_15168_bank4.inc"
};

AnimationRecord D_actor_521100_80146CDC[161] = {
#include "assets/actor_521100_animation_15168_records.inc"
};

u16 D_actor_521100_80146F60[20] = {
#include "assets/actor_521100_animation_15168_indices.inc"
};

AnimationSet D_actor_521100_80146F88 = {
    D_actor_521100_80146CDC,
    D_actor_521100_80146F60,
    { NULL, D_actor_521100_80146A80, NULL, NULL, D_actor_521100_80146AF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80146FB0[2] = {
#include "assets/actor_521100_animation_152F8_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80146FC8[17] = {
#include "assets/actor_521100_animation_152F8_bank4.inc"
};

AnimationRecord D_actor_521100_8014700C[57] = {
#include "assets/actor_521100_animation_152F8_records.inc"
};

u16 D_actor_521100_801470F0[20] = {
#include "assets/actor_521100_animation_152F8_indices.inc"
};

AnimationSet D_actor_521100_80147118 = {
    D_actor_521100_8014700C,
    D_actor_521100_801470F0,
    { NULL, D_actor_521100_80146FB0, NULL, NULL, D_actor_521100_80146FC8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80147140[9] = {
#include "assets/actor_521100_animation_15904_bank1.inc"
};

AnimationPackedRotation D_actor_521100_801471AC[151] = {
#include "assets/actor_521100_animation_15904_bank4.inc"
};

AnimationRecord D_actor_521100_80147408[189] = {
#include "assets/actor_521100_animation_15904_records.inc"
};

u16 D_actor_521100_801476FC[20] = {
#include "assets/actor_521100_animation_15904_indices.inc"
};

AnimationSet D_actor_521100_80147724 = {
    D_actor_521100_80147408,
    D_actor_521100_801476FC,
    { NULL, D_actor_521100_80147140, NULL, NULL, D_actor_521100_801471AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8014774C[8] = {
#include "assets/actor_521100_animation_15E28_bank1.inc"
};

AnimationPackedRotation D_actor_521100_801477AC[116] = {
#include "assets/actor_521100_animation_15E28_bank4.inc"
};

AnimationRecord D_actor_521100_8014797C[169] = {
#include "assets/actor_521100_animation_15E28_records.inc"
};

u16 D_actor_521100_80147C20[20] = {
#include "assets/actor_521100_animation_15E28_indices.inc"
};

AnimationSet D_actor_521100_80147C48 = {
    D_actor_521100_8014797C,
    D_actor_521100_80147C20,
    { NULL, D_actor_521100_8014774C, NULL, NULL, D_actor_521100_801477AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80147C70[10] = {
#include "assets/actor_521100_animation_16628_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80147CE8[195] = {
#include "assets/actor_521100_animation_16628_bank4.inc"
};

AnimationRecord D_actor_521100_80147FF4[267] = {
#include "assets/actor_521100_animation_16628_records.inc"
};

u16 D_actor_521100_80148420[20] = {
#include "assets/actor_521100_animation_16628_indices.inc"
};

AnimationSet D_actor_521100_80148448 = {
    D_actor_521100_80147FF4,
    D_actor_521100_80148420,
    { NULL, D_actor_521100_80147C70, NULL, NULL, D_actor_521100_80147CE8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80148470[16] = {
#include "assets/actor_521100_animation_16F90_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80148530[237] = {
#include "assets/actor_521100_animation_16F90_bank4.inc"
};

AnimationRecord D_actor_521100_801488E4[297] = {
#include "assets/actor_521100_animation_16F90_records.inc"
};

u16 D_actor_521100_80148D88[20] = {
#include "assets/actor_521100_animation_16F90_indices.inc"
};

AnimationSet D_actor_521100_80148DB0 = {
    D_actor_521100_801488E4,
    D_actor_521100_80148D88,
    { NULL, D_actor_521100_80148470, NULL, NULL, D_actor_521100_80148530, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80148DD8[53] = {
#include "assets/actor_521100_animation_18B20_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80149054[738] = {
#include "assets/actor_521100_animation_18B20_bank4.inc"
};

AnimationRecord D_actor_521100_80149BDC[847] = {
#include "assets/actor_521100_animation_18B20_records.inc"
};

u16 D_actor_521100_8014A918[20] = {
#include "assets/actor_521100_animation_18B20_indices.inc"
};

AnimationSet D_actor_521100_8014A940 = {
    D_actor_521100_80149BDC,
    D_actor_521100_8014A918,
    { NULL, D_actor_521100_80148DD8, NULL, NULL, D_actor_521100_80149054, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8014A968[50] = {
#include "assets/actor_521100_animation_1A8E4_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8014ABC0[812] = {
#include "assets/actor_521100_animation_1A8E4_bank4.inc"
};

AnimationRecord D_actor_521100_8014B870[923] = {
#include "assets/actor_521100_animation_1A8E4_records.inc"
};

u16 D_actor_521100_8014C6DC[20] = {
#include "assets/actor_521100_animation_1A8E4_indices.inc"
};

AnimationSet D_actor_521100_8014C704 = {
    D_actor_521100_8014B870,
    D_actor_521100_8014C6DC,
    { NULL, D_actor_521100_8014A968, NULL, NULL, D_actor_521100_8014ABC0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8014C72C[26] = {
#include "assets/actor_521100_animation_1B8F8_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8014C864[434] = {
#include "assets/actor_521100_animation_1B8F8_bank4.inc"
};

AnimationRecord D_actor_521100_8014CF2C[497] = {
#include "assets/actor_521100_animation_1B8F8_records.inc"
};

u16 D_actor_521100_8014D6F0[20] = {
#include "assets/actor_521100_animation_1B8F8_indices.inc"
};

AnimationSet D_actor_521100_8014D718 = {
    D_actor_521100_8014CF2C,
    D_actor_521100_8014D6F0,
    { NULL, D_actor_521100_8014C72C, NULL, NULL, D_actor_521100_8014C864, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8014D740[15] = {
#include "assets/actor_521100_animation_1C104_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8014D7F4[197] = {
#include "assets/actor_521100_animation_1C104_bank4.inc"
};

AnimationRecord D_actor_521100_8014DB08[253] = {
#include "assets/actor_521100_animation_1C104_records.inc"
};

u16 D_actor_521100_8014DEFC[20] = {
#include "assets/actor_521100_animation_1C104_indices.inc"
};

AnimationSet D_actor_521100_8014DF24 = {
    D_actor_521100_8014DB08,
    D_actor_521100_8014DEFC,
    { NULL, D_actor_521100_8014D740, NULL, NULL, D_actor_521100_8014D7F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8014DF4C[5] = {
#include "assets/actor_521100_animation_1C58C_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8014DF88[87] = {
#include "assets/actor_521100_animation_1C58C_bank4.inc"
};

AnimationRecord D_actor_521100_8014E0E4[168] = {
#include "assets/actor_521100_animation_1C58C_records.inc"
};

u16 D_actor_521100_8014E384[20] = {
#include "assets/actor_521100_animation_1C58C_indices.inc"
};

AnimationSet D_actor_521100_8014E3AC = {
    D_actor_521100_8014E0E4,
    D_actor_521100_8014E384,
    { NULL, D_actor_521100_8014DF4C, NULL, NULL, D_actor_521100_8014DF88, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8014E3D4[147] = {
#include "assets/actor_521100_animation_20880_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8014EAB8[1395] = {
#include "assets/actor_521100_animation_20880_bank4.inc"
};

AnimationRecord D_actor_521100_80150084[2429] = {
#include "assets/actor_521100_animation_20880_records.inc"
};

u16 D_actor_521100_80152678[20] = {
#include "assets/actor_521100_animation_20880_indices.inc"
};

AnimationSet D_actor_521100_801526A0 = {
    D_actor_521100_80150084,
    D_actor_521100_80152678,
    { NULL, D_actor_521100_8014E3D4, NULL, NULL, D_actor_521100_8014EAB8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_801526C8[11] = {
#include "assets/actor_521100_animation_20F94_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8015274C[162] = {
#include "assets/actor_521100_animation_20F94_bank4.inc"
};

AnimationRecord D_actor_521100_801529D4[238] = {
#include "assets/actor_521100_animation_20F94_records.inc"
};

u16 D_actor_521100_80152D8C[20] = {
#include "assets/actor_521100_animation_20F94_indices.inc"
};

AnimationSet D_actor_521100_80152DB4 = {
    D_actor_521100_801529D4,
    D_actor_521100_80152D8C,
    { NULL, D_actor_521100_801526C8, NULL, NULL, D_actor_521100_8015274C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80152DDC[14] = {
#include "assets/actor_521100_animation_2177C_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80152E84[170] = {
#include "assets/actor_521100_animation_2177C_bank4.inc"
};

AnimationRecord D_actor_521100_8015312C[274] = {
#include "assets/actor_521100_animation_2177C_records.inc"
};

u16 D_actor_521100_80153574[20] = {
#include "assets/actor_521100_animation_2177C_indices.inc"
};

AnimationSet D_actor_521100_8015359C = {
    D_actor_521100_8015312C,
    D_actor_521100_80153574,
    { NULL, D_actor_521100_80152DDC, NULL, NULL, D_actor_521100_80152E84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_801535C4[13] = {
#include "assets/actor_521100_animation_220B8_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80153660[206] = {
#include "assets/actor_521100_animation_220B8_bank4.inc"
};

AnimationRecord D_actor_521100_80153998[326] = {
#include "assets/actor_521100_animation_220B8_records.inc"
};

u16 D_actor_521100_80153EB0[20] = {
#include "assets/actor_521100_animation_220B8_indices.inc"
};

AnimationSet D_actor_521100_80153ED8 = {
    D_actor_521100_80153998,
    D_actor_521100_80153EB0,
    { NULL, D_actor_521100_801535C4, NULL, NULL, D_actor_521100_80153660, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80153F00[12] = {
#include "assets/actor_521100_animation_229A0_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80153F90[194] = {
#include "assets/actor_521100_animation_229A0_bank4.inc"
};

AnimationRecord D_actor_521100_80154298[320] = {
#include "assets/actor_521100_animation_229A0_records.inc"
};

u16 D_actor_521100_80154798[20] = {
#include "assets/actor_521100_animation_229A0_indices.inc"
};

AnimationSet D_actor_521100_801547C0 = {
    D_actor_521100_80154298,
    D_actor_521100_80154798,
    { NULL, D_actor_521100_80153F00, NULL, NULL, D_actor_521100_80153F90, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_801547E8[10] = {
#include "assets/actor_521100_animation_2318C_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80154860[175] = {
#include "assets/actor_521100_animation_2318C_bank4.inc"
};

AnimationRecord D_actor_521100_80154B1C[282] = {
#include "assets/actor_521100_animation_2318C_records.inc"
};

u16 D_actor_521100_80154F84[20] = {
#include "assets/actor_521100_animation_2318C_indices.inc"
};

AnimationSet D_actor_521100_80154FAC = {
    D_actor_521100_80154B1C,
    D_actor_521100_80154F84,
    { NULL, D_actor_521100_801547E8, NULL, NULL, D_actor_521100_80154860, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80154FD4[12] = {
#include "assets/actor_521100_animation_239C0_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80155064[179] = {
#include "assets/actor_521100_animation_239C0_bank4.inc"
};

AnimationRecord D_actor_521100_80155330[290] = {
#include "assets/actor_521100_animation_239C0_records.inc"
};

u16 D_actor_521100_801557B8[20] = {
#include "assets/actor_521100_animation_239C0_indices.inc"
};

AnimationSet D_actor_521100_801557E0 = {
    D_actor_521100_80155330,
    D_actor_521100_801557B8,
    { NULL, D_actor_521100_80154FD4, NULL, NULL, D_actor_521100_80155064, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80155808[48] = {
#include "assets/actor_521100_animation_24F5C_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80155A48[554] = {
#include "assets/actor_521100_animation_24F5C_bank4.inc"
};

AnimationRecord D_actor_521100_801562F0[665] = {
#include "assets/actor_521100_animation_24F5C_records.inc"
};

u16 D_actor_521100_80156D54[20] = {
#include "assets/actor_521100_animation_24F5C_indices.inc"
};

AnimationSet D_actor_521100_80156D7C = {
    D_actor_521100_801562F0,
    D_actor_521100_80156D54,
    { NULL, D_actor_521100_80155808, NULL, NULL, D_actor_521100_80155A48, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80156DA4[13] = {
#include "assets/actor_521100_animation_25ACC_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80156E40[259] = {
#include "assets/actor_521100_animation_25ACC_bank4.inc"
};

AnimationRecord D_actor_521100_8015724C[414] = {
#include "assets/actor_521100_animation_25ACC_records.inc"
};

u16 D_actor_521100_801578C4[20] = {
#include "assets/actor_521100_animation_25ACC_indices.inc"
};

AnimationSet D_actor_521100_801578EC = {
    D_actor_521100_8015724C,
    D_actor_521100_801578C4,
    { NULL, D_actor_521100_80156DA4, NULL, NULL, D_actor_521100_80156E40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80157914[8] = {
#include "assets/actor_521100_animation_26024_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80157974[117] = {
#include "assets/actor_521100_animation_26024_bank4.inc"
};

AnimationRecord D_actor_521100_80157B48[181] = {
#include "assets/actor_521100_animation_26024_records.inc"
};

u16 D_actor_521100_80157E1C[20] = {
#include "assets/actor_521100_animation_26024_indices.inc"
};

AnimationSet D_actor_521100_80157E44 = {
    D_actor_521100_80157B48,
    D_actor_521100_80157E1C,
    { NULL, D_actor_521100_80157914, NULL, NULL, D_actor_521100_80157974, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80157E6C[25] = {
#include "assets/actor_521100_animation_26E3C_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80157F98[372] = {
#include "assets/actor_521100_animation_26E3C_bank4.inc"
};

AnimationRecord D_actor_521100_80158568[435] = {
#include "assets/actor_521100_animation_26E3C_records.inc"
};

u16 D_actor_521100_80158C34[20] = {
#include "assets/actor_521100_animation_26E3C_indices.inc"
};

AnimationSet D_actor_521100_80158C5C = {
    D_actor_521100_80158568,
    D_actor_521100_80158C34,
    { NULL, D_actor_521100_80157E6C, NULL, NULL, D_actor_521100_80157F98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80158C84[2] = {
#include "assets/actor_521100_animation_26FCC_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80158C9C[17] = {
#include "assets/actor_521100_animation_26FCC_bank4.inc"
};

AnimationRecord D_actor_521100_80158CE0[57] = {
#include "assets/actor_521100_animation_26FCC_records.inc"
};

u16 D_actor_521100_80158DC4[20] = {
#include "assets/actor_521100_animation_26FCC_indices.inc"
};

AnimationSet D_actor_521100_80158DEC = {
    D_actor_521100_80158CE0,
    D_actor_521100_80158DC4,
    { NULL, D_actor_521100_80158C84, NULL, NULL, D_actor_521100_80158C9C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80158E14[2] = {
#include "assets/actor_521100_animation_271A8_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80158E2C[17] = {
#include "assets/actor_521100_animation_271A8_bank4.inc"
};

AnimationRecord D_actor_521100_80158E70[76] = {
#include "assets/actor_521100_animation_271A8_records.inc"
};

u16 D_actor_521100_80158FA0[20] = {
#include "assets/actor_521100_animation_271A8_indices.inc"
};

AnimationSet D_actor_521100_80158FC8 = {
    D_actor_521100_80158E70,
    D_actor_521100_80158FA0,
    { NULL, D_actor_521100_80158E14, NULL, NULL, D_actor_521100_80158E2C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80158FF0[19] = {
#include "assets/actor_521100_animation_27A78_bank1.inc"
};

AnimationPackedRotation D_actor_521100_801590D4[211] = {
#include "assets/actor_521100_animation_27A78_bank4.inc"
};

AnimationRecord D_actor_521100_80159420[276] = {
#include "assets/actor_521100_animation_27A78_records.inc"
};

u16 D_actor_521100_80159870[20] = {
#include "assets/actor_521100_animation_27A78_indices.inc"
};

AnimationSet D_actor_521100_80159898 = {
    D_actor_521100_80159420,
    D_actor_521100_80159870,
    { NULL, D_actor_521100_80158FF0, NULL, NULL, D_actor_521100_801590D4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_801598C0[6] = {
#include "assets/actor_521100_animation_27DB0_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80159908[63] = {
#include "assets/actor_521100_animation_27DB0_bank4.inc"
};

AnimationRecord D_actor_521100_80159A04[105] = {
#include "assets/actor_521100_animation_27DB0_records.inc"
};

u16 D_actor_521100_80159BA8[20] = {
#include "assets/actor_521100_animation_27DB0_indices.inc"
};

AnimationSet D_actor_521100_80159BD0 = {
    D_actor_521100_80159A04,
    D_actor_521100_80159BA8,
    { NULL, D_actor_521100_801598C0, NULL, NULL, D_actor_521100_80159908, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_80159BF8[11] = {
#include "assets/actor_521100_animation_28380_bank1.inc"
};

AnimationPackedRotation D_actor_521100_80159C7C[131] = {
#include "assets/actor_521100_animation_28380_bank4.inc"
};

AnimationRecord D_actor_521100_80159E88[188] = {
#include "assets/actor_521100_animation_28380_records.inc"
};

u16 D_actor_521100_8015A178[20] = {
#include "assets/actor_521100_animation_28380_indices.inc"
};

AnimationSet D_actor_521100_8015A1A0 = {
    D_actor_521100_80159E88,
    D_actor_521100_8015A178,
    { NULL, D_actor_521100_80159BF8, NULL, NULL, D_actor_521100_80159C7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8015A1C8[17] = {
#include "assets/actor_521100_animation_28DD8_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8015A294[212] = {
#include "assets/actor_521100_animation_28DD8_bank4.inc"
};

AnimationRecord D_actor_521100_8015A5E4[379] = {
#include "assets/actor_521100_animation_28DD8_records.inc"
};

u16 D_actor_521100_8015ABD0[20] = {
#include "assets/actor_521100_animation_28DD8_indices.inc"
};

AnimationSet D_actor_521100_8015ABF8 = {
    D_actor_521100_8015A5E4,
    D_actor_521100_8015ABD0,
    { NULL, D_actor_521100_8015A1C8, NULL, NULL, D_actor_521100_8015A294, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8015AC20[8] = {
#include "assets/actor_521100_animation_291F8_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8015AC80[93] = {
#include "assets/actor_521100_animation_291F8_bank4.inc"
};

AnimationRecord D_actor_521100_8015ADF4[127] = {
#include "assets/actor_521100_animation_291F8_records.inc"
};

u16 D_actor_521100_8015AFF0[20] = {
#include "assets/actor_521100_animation_291F8_indices.inc"
};

AnimationSet D_actor_521100_8015B018 = {
    D_actor_521100_8015ADF4,
    D_actor_521100_8015AFF0,
    { NULL, D_actor_521100_8015AC20, NULL, NULL, D_actor_521100_8015AC80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8015B040[6] = {
#include "assets/actor_521100_animation_29534_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8015B088[64] = {
#include "assets/actor_521100_animation_29534_bank4.inc"
};

AnimationRecord D_actor_521100_8015B188[105] = {
#include "assets/actor_521100_animation_29534_records.inc"
};

u16 D_actor_521100_8015B32C[20] = {
#include "assets/actor_521100_animation_29534_indices.inc"
};

AnimationSet D_actor_521100_8015B354 = {
    D_actor_521100_8015B188,
    D_actor_521100_8015B32C,
    { NULL, D_actor_521100_8015B040, NULL, NULL, D_actor_521100_8015B088, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8015B37C[10] = {
#include "assets/actor_521100_animation_29A88_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8015B3F4[117] = {
#include "assets/actor_521100_animation_29A88_bank4.inc"
};

AnimationRecord D_actor_521100_8015B5C8[174] = {
#include "assets/actor_521100_animation_29A88_records.inc"
};

u16 D_actor_521100_8015B880[20] = {
#include "assets/actor_521100_animation_29A88_indices.inc"
};

AnimationSet D_actor_521100_8015B8A8 = {
    D_actor_521100_8015B5C8,
    D_actor_521100_8015B880,
    { NULL, D_actor_521100_8015B37C, NULL, NULL, D_actor_521100_8015B3F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8015B8D0[28] = {
#include "assets/actor_521100_animation_2A998_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8015BA20[400] = {
#include "assets/actor_521100_animation_2A998_bank4.inc"
};

AnimationRecord D_actor_521100_8015C060[460] = {
#include "assets/actor_521100_animation_2A998_records.inc"
};

u16 D_actor_521100_8015C790[20] = {
#include "assets/actor_521100_animation_2A998_indices.inc"
};

AnimationSet D_actor_521100_8015C7B8 = {
    D_actor_521100_8015C060,
    D_actor_521100_8015C790,
    { NULL, D_actor_521100_8015B8D0, NULL, NULL, D_actor_521100_8015BA20, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8015C7E0[33] = {
#include "assets/actor_521100_animation_2B89C_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8015C96C[385] = {
#include "assets/actor_521100_animation_2B89C_bank4.inc"
};

AnimationRecord D_actor_521100_8015CF70[457] = {
#include "assets/actor_521100_animation_2B89C_records.inc"
};

u16 D_actor_521100_8015D694[20] = {
#include "assets/actor_521100_animation_2B89C_indices.inc"
};

AnimationSet D_actor_521100_8015D6BC = {
    D_actor_521100_8015CF70,
    D_actor_521100_8015D694,
    { NULL, D_actor_521100_8015C7E0, NULL, NULL, D_actor_521100_8015C96C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8015D6E4[19] = {
#include "assets/actor_521100_animation_2C378_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8015D7C8[289] = {
#include "assets/actor_521100_animation_2C378_bank4.inc"
};

AnimationRecord D_actor_521100_8015DC4C[329] = {
#include "assets/actor_521100_animation_2C378_records.inc"
};

u16 D_actor_521100_8015E170[20] = {
#include "assets/actor_521100_animation_2C378_indices.inc"
};

AnimationSet D_actor_521100_8015E198 = {
    D_actor_521100_8015DC4C,
    D_actor_521100_8015E170,
    { NULL, D_actor_521100_8015D6E4, NULL, NULL, D_actor_521100_8015D7C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8015E1C0[17] = {
#include "assets/actor_521100_animation_2CCA8_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8015E28C[234] = {
#include "assets/actor_521100_animation_2CCA8_bank4.inc"
};

AnimationRecord D_actor_521100_8015E634[283] = {
#include "assets/actor_521100_animation_2CCA8_records.inc"
};

u16 D_actor_521100_8015EAA0[20] = {
#include "assets/actor_521100_animation_2CCA8_indices.inc"
};

AnimationSet D_actor_521100_8015EAC8 = {
    D_actor_521100_8015E634,
    D_actor_521100_8015EAA0,
    { NULL, D_actor_521100_8015E1C0, NULL, NULL, D_actor_521100_8015E28C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_521100_8015EAF0[21] = {
#include "assets/actor_521100_animation_2D708_bank1.inc"
};

AnimationPackedRotation D_actor_521100_8015EBEC[261] = {
#include "assets/actor_521100_animation_2D708_bank4.inc"
};

AnimationRecord D_actor_521100_8015F000[320] = {
#include "assets/actor_521100_animation_2D708_records.inc"
};

u16 D_actor_521100_8015F500[20] = {
#include "assets/actor_521100_animation_2D708_indices.inc"
};

AnimationSet D_actor_521100_8015F528 = {
    D_actor_521100_8015F000,
    D_actor_521100_8015F500,
    { NULL, D_actor_521100_8015EAF0, NULL, NULL, D_actor_521100_8015EBEC, NULL, NULL, NULL },
};

DamageAttack D_actor_521100_8015F550[4] = {
    { 15, 7 },
    { 25, 7 },
    { 40, 7 },
    { 10, 0 },
};

GpPairSrcE D_actor_521100_8015F560 = { D_actor_521100_8015F550, 1100, 800, 300, 50, 0, 0, 0, 0, 0 };

s16 D_actor_521100_8015F570[6] = {
    6,
    11,
    16,
    16,
    4,
    0,
};

s16 D_actor_521100_8015F57C[16] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    2,
    2,
    2,
    2,
    2,
};

Actor521100StateChoice D_actor_521100_8015F59C[6] = {
    { 2 },
    { 2 },
    { 0 },
    { 2 },
    { 0 },
    { 0 },
};

s16 D_actor_521100_8015F5A8[16] = {
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
};

Actor521100StateChoice D_actor_521100_8015F5C8[6] = {
    { 1 },
    { 2 },
    { 0 },
    { 2 },
    { 0 },
    { 1 },
};

u16 D_actor_521100_8015F5D4[16] = { 0 };

u16 D_actor_521100_8015F5F4[16] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    15,
    15,
    15,
    15,
    30,
    30,
    30,
    30,
};

u16 D_actor_521100_8015F614[16] = {
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
};

u16 D_actor_521100_8015F634[16] = { 0 };

VECTOR D_actor_521100_8015F654[3] = {
    { -4000, 0, -2000, 0 },
    { -5250, 0, -1200, 0 },
    { -4000, 0, -2000, 0 },
};

s16 D_actor_521100_8015F684[48] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

TaskDesc D_actor_521100_8015F6E4[2] = {
    { TASK_BODY_TMD, 96, func_actor_521100_80135378, { .model = &D_actor_521100_80141894 } },
    { TASK_BODY_TMD, 96, func_actor_521100_80135AE4, { .model = &D_actor_521100_80142098 } },
};

Actor521100MessageEntry D_actor_521100_8015F6FC[8] = {
    { 2014, { .call1 = func_actor_521100_80135BEC } },
    { 2003, { .call2 = func_actor_521100_80135C14 } },
    { 2004, { .call4 = func_actor_521100_80135CAC } },
    { 2005, { .call5 = func_actor_521100_80135D10 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call3 = func_actor_521100_80135D58 } },
    { 2007, { .call1 = func_actor_521100_80135D9C } },
    { 2006, { .call0 = func_actor_521100_80135DC8 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

static void           func_actor_521100_80131E8C(GpEnemy* enemy, Task* task);
static __inline__ s32 Actor521100_GetHitType(s32 key);

/// Spawn state of the actor: allocates its 0x6C0 work block, registers the
/// enemy on the lock-on list with its parameter record, contact table and body
/// coordinate (the model's fourth part), and starts the animation on clip 0x15.
/// It then links the actor's collision bodies, spawns a second enemy from
/// `D_actor_521100_8015F6E4` with this one as its parent, dresses that enemy's
/// model with the texture page and CLUT of this enemy's placement, and links
/// two more pairs of bodies, one of them placed on the second enemy's model.
///
/// Every body's coordinate is assigned first in its block: the model pointer is
/// reloaded from the task each time, and that load has to precede the stores
/// into the work block, which it cannot be scheduled across.
static void func_actor_521100_80131E8C(GpEnemy* enemy, Task* task)
{
    GameLocationKey  key;
    TmdObject*       obj;
    GfxCoord*        coord;
    Actor521100Work* work;
    GpEnemy*         spawned;
    TmdObject*       model;
    GameLocationKey* sessionKey;
    AreaPlacement*   place;
    s32              idx;
    u32              raw;
    s32              i;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x6C0, false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->light;
    obj->colorMtx       = &work->color;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord         = &task->extra.tmd->coords[3];
    enemy->bodyPos.vx    = 0;
    enemy->bodyPos.vy    = 0;
    enemy->bodyPos.vz    = 0;
    enemy->param         = &D_actor_521100_8015F560;
    enemy->recs          = work->rec534;
    enemy->hp            = D_actor_521100_8015F560.hpMax;
    work->eff.coord      = &task->extra.tmd->coords[3];
    work->eff.spawnArgLo = 0x400;
    work->eff.spawnArgHi = 3;
    func_800B3F84(&work->rig.anim, D_actor_521100_8015F73C, obj, work->rig.poses, work->rig.slots);
    work->field_686 = 0x15;
    work->field_688 = 0x15;
    i               = 1;
    do {
        Gp_AnimResetSlot(&work->rig.anim, i, work->field_686);
        i++;
    } while (i < 0x13);
    work->field_6B2 = 1;
    work->field_6B6 = -1;
    work->field_6B8 = -1;

    work->obj47C.coord            = task->extra.tmd->coords;
    work->obj47C.context.contacts = work->rec49C;
    work->obj47C.pos.vx           = 0;
    work->obj47C.pos.vy           = -0x190;
    work->obj47C.pos.vz           = 0;
    work->obj47C.key              = 0x30022;
    work->obj47C.radius           = 0x190;
    work->obj47C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj47C);
    Gp_InitRec18Table(work->rec49C, 5, 0);
    work->obj47C.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

    work->obj514.coord            = &task->extra.tmd->coords[3];
    work->obj514.context.contacts = work->rec534;
    work->obj514.pos.vx           = 0;
    work->obj514.pos.vy           = 0;
    work->obj514.pos.vz           = 0;
    work->obj514.key              = 0x30022;
    work->obj514.radius           = 0x190;
    work->obj514.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj514);
    Gp_InitRec18Table(work->rec534, 3, 0);
    work->obj514.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    spawned    = Gp_SpawnEnemyFromTable(D_actor_521100_8015F6E4, 1, 0, enemy);
    model      = spawned->task->extra.tmd;
    raw        = enemy->placeKey;
    sessionKey = &gGameSession->location.loc;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    idx        = raw >> 12;
    key.view   = sessionKey->view;
    areaSyncLocationVariant(&key);
    /* offset + base, as in the sibling spawn bodies: the ROM adds the scaled
       index onto the table. */
    place                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->field_0, idx);
    model->texturePageOffset = place->texturePageOffset;
    model->clutRowOffset     = place->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    work->field_654 = spawned->task;

    work->obj57C.coord            = spawned->task->extra.tmd->coords;
    work->obj57C.pos.vx           = -0x226;
    work->obj57C.context.contacts = work->rec5BC;
    work->obj57C.pos.vy           = 0x64;
    work->obj57C.pos.vz           = 0;
    work->obj57C.key              = 0;
    work->obj57C.radius           = 0x1C2;
    work->obj57C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj57C);
    Gp_InitRec18Table(work->rec5BC, 1, 0);
    work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->obj59C.coord            = &task->extra.tmd->coords[7];
    work->obj59C.context.contacts = work->rec5BC;
    work->obj59C.pos.vx           = 0;
    work->obj59C.pos.vy           = 0;
    work->obj59C.pos.vz           = 0;
    work->obj59C.key              = 0;
    work->obj59C.radius           = 0x1C2;
    work->obj59C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj59C);

    work->shape.end0.vz    = 0x5DC;
    work->shape.end0.vx    = 0;
    work->shape.end0.vy    = 0;
    work->shape.end1.vx    = 0;
    work->shape.end1.vy    = 0;
    work->shape.end1.vz    = 0;
    work->shape.end0Radius = 1;
    work->shape.end1Radius = 1;
    work->shape.recs       = work->rec62C;
    work->obj59C.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->obj5D4.coord           = task->extra.tmd->coords;
    work->obj5D4.context.capsule = &work->shape;
    work->obj5D4.pos.vy          = -0x1F4;
    work->obj5D4.pos.vx          = 0;
    work->obj5D4.pos.vz          = 0;
    work->obj5D4.key             = 0;
    work->obj5D4.radius          = 0;
    work->obj5D4.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(3, &work->obj5D4);
    Gp_InitRec18Table(work->rec62C, 1, 0);
    work->obj5D4.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;

    work->obj5F4.coord            = task->extra.tmd->coords;
    work->obj5F4.pos.vy           = -0x320;
    work->obj5F4.context.contacts = work->rec62C;
    work->obj5F4.pos.vx           = 0;
    work->obj5F4.pos.vz           = 0x4E2;
    work->obj5F4.key              = 0;
    work->obj5F4.radius           = 0x1C2;
    work->obj5F4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj5F4);
    work->obj5F4.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;

    task->msgTable = D_actor_521100_8015F6FC;
    task->state    = 1;
}

static __inline__ s32 Actor521100_GetHitType(s32 key)
{
    if (key & 0x8000) {
        return 1;
    }
    return D_actor_521100_8015F684[key & 0x3F];
}

static void func_actor_521100_801322F8(Task* arg0, TmdObject* arg1, s32 arg2)
{
    ActorDeltaFrame48* scratch;
    Actor521100Work*   work;
    GpEnemy*           enemy;
    GfxCoord*          coord;
    u32                lastId;
    u32                sound;
    u32                kind;
    u32                damage;
    u32                rng;
    u32                rng2;
    u32                r;
    s32                result;
    s32                dx;
    s32                coordX;
    s32                dz;
    s32                absDiff;
    s32                r2;
    s32                angle;
    s32                angle2;
    s32                hitType;
    s32                i;
    s16                diff;
    s16                wrap;
    s16                cooldown;
    s32                pan;
    s32                pan1;
    s32                pan2;
    s32                depth;
    s16                wait;

    lastId  = 0;
    work    = arg0->work;
    scratch = (ActorDeltaFrame48*)SCRATCH_PUSH_BYTES(0x48);
    coord   = arg0->extra.tmd->coords;
    enemy   = arg0->spawnArg2.pointer;
    result  = func_800E0C10(work->rec49C, &scratch->delta, 5, NULL);
    switch (result) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += scratch->delta.vx.h.hi;
            coord->coord.t[1] += scratch->delta.vy.h.hi;
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_64C;
            coord->coord.t[1] = work->field_64E;
            coord->coord.t[2] = work->field_650;
            break;
    }
    Gp_ClearRec18Occupied(work->rec49C);
    if (work->field_684 != 0) {
        cooldown        = (u16)work->field_684 - 1;
        work->field_684 = cooldown;
        if ((cooldown << 0x10) <= 0) {
            work->field_684 = 0;
        }
    }
    if (work->field_6AC != 0) {
        work->field_6AC = (u16)work->field_6AC - 1;
    }
    for (i = 0; i < 3; i++) {
        kind = (u16)(work->rec534[i].key.value >> 0x10);
        if (kind < 2) {
            continue;
        }
        if (kind != 2) {
            continue;
        }
        if (work->field_684 != 0) {
            continue;
        }
        coordX              = coord->coord.t[0];
        dx                  = Player_Status.coordMtx->t[0] - coordX;
        scratch->delta.vx.w = dx;
        scratch->delta.vy.w = 0;
        dz                  = Player_Status.coordMtx->t[2] - coord->coord.t[2];
        scratch->delta.vz.w = dz;
        damage              = Gp_ComputeDamage(work->rec534[i].key.value, SquareRoot0(dx * dx + dz * dz), 0, 0);
        hitType             = Actor521100_GetHitType(work->rec534[i].key.value);
        if (hitType == 1) {
            if (work->field_69E == 2) {
                hitType = 0;
            } else {
                diff    = work->field_696 - (ratan2((s16)scratch->delta.vx.w, (s16)scratch->delta.vz.w) & 0xFFF);
                absDiff = abs(diff);
                if (absDiff < 0x800) {
                    wrap = absDiff;
                } else if (diff > 0) {
                    wrap = 0x1000 - diff;
                } else {
                    wrap = diff + 0x1000;
                }
                if ((wrap >= 0x301) || (work->field_6AE == 1)) {
                    hitType = 2;
                }
            }
        }
        switch (hitType) {
            case 0:
                rng         = Gp_LcgState * 5 + 0x71357911;
                r           = rng >> 0x10;
                angle       = (r & 0x7F) + 0x40;
                Gp_LcgState = rng;
                if (!(r & 1)) {
                    angle = -angle;
                }
                work->field_678.vx = angle;
                r2                 = (s16)r >> 8;
                angle2             = (r2 & 0x7F) + 0x40;
                if (!(r2 & 1)) {
                    angle2 = -angle2;
                }
                work->field_678.vy = angle2;
                work->field_680    = 1;
                damage           >>= 1;
                if ((Gp_GetIdParam0(work->rec534[i].key.value) & 0xFFFF) == 5) {
                    damage *= 2;
                    Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 2, NULL);
                }
                if ((work->field_69E == 0) && (work->field_6AC <= 0)) {
                    work->field_69E = 5;
                    work->field_6A0 = 0;
                    rng2            = Gp_LcgState * 5 + 0x71357911;
                    work->field_6AC = ((rng2 >> 0x10) & 0xFF) + 0x96;
                    Gp_LcgState     = rng2;
                    sound           = (((u16)enemy->placeKey >> 12) << 8) | 0x401C0006;
                    pan             = (s8)Gp_GetObjPan(coord);
                    depth           = (s8)gpGetObjDepth(coord);
                    SndEvt_EnqueueType6((s32)sound, pan, depth);
                    goto damage_done;
                }
                goto damage_done;
            case 1:
                work->field_69E = 4;
                work->field_6A0 = 0;
                if (work->rec534[i].key.value & 0x8000) {
                    damage >>= 2;
                } else {
                    damage >>= 3;
                }
                sound = (((u16)enemy->placeKey >> 12) << 8) | 0x401C0003;
                pan1  = (s8)Gp_GetObjPan(coord);
                depth = (s8)gpGetObjDepth(coord);
                SndEvt_EnqueueType6((s32)sound, pan1, depth);
                goto damage_done;
            case 2:
                work->field_69E = 3;
                work->field_6A0 = 0;
                work->field_6AE = 0;
                if (work->rec534[i].key.value & 0x8000) {
                    damage *= 2;
                } else {
                    damage >>= 1;
                }
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((Gp_LcgState >> 16) & 1) {
                    sound = (((u16)enemy->placeKey >> 12) << 8) | 0x401C0004;
                } else {
                    sound = (((u16)enemy->placeKey >> 12) << 8) | 0x401C0005;
                }
                pan2  = (s8)Gp_GetObjPan(coord);
                depth = (s8)gpGetObjDepth(coord);
                SndEvt_EnqueueType6((s32)sound, pan2, depth);
        }
    damage_done:
        func_800E2C78(enemy, (s32)work->rec534[i].key.value, (s32)damage, 0);
        func_800DA6E8(&enemy->node, (s32)damage, 0);
        enemy->hp = (u16)enemy->hp - damage;
        if (lastId != work->rec534[i].key.value) {
            lastId = work->rec534[i].key.value;
            func_800FDB18(Gp_GetIdParam1(work->rec534[i].key.value) & 0xFFFF, &arg0->extra.tmd->coords[3], NULL, &work->eff);
        }
        wait = Gp_GetIdParam2(work->rec534[i].key.value);
        if (wait > 0) {
            work->field_684 = wait;
        }
    }
    Gp_ClearRec18Occupied(work->rec534);
    if (work->rec5BC[0].flags & 1) {
        work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(work->rec5BC);
        work->field_6A6 = 1;
    }
    work->field_6BE = 0;
    if (work->rec62C[0].flags & 1) {
        work->field_6BE = 1;
        Gp_ClearRec18Occupied(work->rec62C);
    }
    if (enemy->hp <= 0) {
        if (Player_Status.hp > 0) {
            work->field_6B2 = 0;
        } else {
            enemy->hp = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x48);
}

static void func_actor_521100_80132958(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    VECTOR*          scratchEnd;
    VECTOR*          vec;
    u16*             tbl;
    u16*             tbl1;
    u16*             tbl2;
    u32              rng;
    u32              rng1;
    u32              rng2;
    s16              state;
    s16              delta;
    s16              angle;
    s16              timer;
    s16              wrapped;
    s32              magnitude;

    coord                        = arg0->extra.tmd->coords;
    work                         = arg0->work;
    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    vec                          = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = vec;
    scratchEnd[-1].vx            = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    vec->vy                      = 0;
    vec->vz                      = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    work->field_6AA              = SquareRoot0(scratchEnd[-1].vx * scratchEnd[-1].vx + vec->vz * vec->vz);
    angle                        = ratan2((s16)scratchEnd[-1].vx, (s16)vec->vz) & 0xFFF;
    work->field_698              = angle;
    if (gGameSession->location.loc.view == 2) {
        work->field_69E = 6;
        work->field_6A0 = 0;
    } else {
        state = work->field_6A0;
        switch (state) {
            case 0:
                work->field_69A = 0;
                work->field_69C = 0;
                timer           = (u16)work->field_68E - 1;
                work->field_68E = timer;
                if (timer < 0) {
                    work->field_6A0 = 1;
                    work->field_686 = 0x12;
                    tbl             = D_actor_521100_8015F614;
                    rng             = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState     = rng;
                    work->field_68E = tbl[(rng >> 16) & 0xF];
                } else if (func_actor_521100_80132C70(arg0) == 0) {
                    func_actor_521100_80135680(arg0);
                }
                break;
            case 1:
                if ((work->field_6BE == state) && (work->field_6AA < 0x7D0)) {
                    delta     = (u16)angle - (u16)work->field_696;
                    magnitude = abs(delta);
                    if (magnitude < 0x800) {
                        wrapped = magnitude;
                    } else {
                        if (delta > 0) {
                            wrapped = 0x1000 - delta;
                        } else {
                            wrapped = delta + 0x1000;
                        }
                    }
                    if (wrapped < 0x100) {
                        work->field_68E = 0;
                    }
                }
                timer           = (u16)work->field_68E - 1;
                work->field_68E = timer;
                if (timer <= 0) {
                    work->field_69C = 0x78;
                    work->field_69A = 0;
                    tbl1            = D_actor_521100_8015F5F4;
                    rng1            = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState     = rng1;
                    timer           = tbl1[(rng1 >> 16) & 0xF];
                    work->field_68E = timer;
                    if (timer == 0) {
                        func_actor_521100_80135680(arg0);
                        if (work->field_69E == 0) {
                            tbl2            = D_actor_521100_8015F614;
                            rng2            = Gp_LcgState * 5 + 0x71357911;
                            Gp_LcgState     = rng2;
                            work->field_68E = tbl2[(rng2 >> 16) & 0xF];
                        }
                    } else {
                        work->field_6A0 = 0;
                        work->field_686 = 1;
                    }
                } else {
                    work->field_69A = 0x14;
                    work->field_69C = 0x78;
                    func_actor_521100_80132C70(arg0);
                }
                break;
        }
    }
    work->field_6AE = 0;
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Asks the player for the hold (message 0x3F8, range 0x19) once the actor has
/// swung its heading to within 0x20 of the slot-3 task's own and is lined up
/// to latch on. The heading error is the 12-bit difference between the work
/// block's `field_698` and `field_696`, wrapped into [-0x800, 0x800] and then
/// narrowed by `field_69C` being armed with 0x50; the request goes out only
/// while fewer than 0x4E2 units of the actor's health are left, the latch
/// `field_6BE` is clear, `Player_Status.hp` (the remaining-enemy count) is positive
/// and the player's own `GameActor::field_954` is not its mode 2. On acceptance
/// the body rearms the motion state (2 into `field_69E`, 0xA frames of blend
/// into `field_686`, the 0xA/0xFF/0x80 pad lerp) and returns 1; the 0x3F8
/// query buffer is the 0x18 bytes pushed on the scratch-pad stack.
static s32 func_actor_521100_80132C70(Task* arg0)
{
    Actor521100Work* work;
    Task*            player;
    GpDelayArg*      msg;
    s16              diff;
    s32              adiff;
    s16              wrap;
    s32              ret;

    work   = arg0->work;
    player = gameGetPtrSlot(3);
    msg    = (GpDelayArg*)SCRATCH_PUSH_BYTES(0x18);

    diff  = work->field_698 - work->field_696;
    adiff = diff >= 0 ? diff : -diff;
    ret   = 0;
    if (adiff < 0x800) {
        wrap = adiff;
    } else if (diff > 0) {
        wrap = 0x1000 - diff;
    } else {
        wrap = diff + 0x1000;
    }
    if ((wrap < 0x400) && (work->field_6AA < 0x4E2) && (work->field_6BE == 0) && (Player_Status.hp > 0) && (work->field_69C = 0x50, (wrap < 0x20)) && (((GameActor*)player->work)->field_954 != 2)) {
        msg->field_14 = 0x19;
        if (Gp_DispatchMsgPtr(player, 0x3F8, msg, 0) == 0) {
            ret             = 1;
            work->field_6A8 = 0;
            work->field_69E = 2;
            work->field_6A0 = 0;
            work->field_6A2 = 0;
            work->field_686 = 0xA;
            work->field_69A = 0;
            work->field_69C = 0;
            Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
    return ret;
}
static void func_actor_521100_80132DE8(Task* arg0)
{
    Actor521100Work*        work;
    GfxCoord*               coord;
    VECTOR*                 head;
    VECTOR*                 vec;
    Actor521100StateChoice* pairNear;
    Actor521100StateChoice* pairFar;
    s16*                    flatNear;
    s16*                    flatFar;
    u16                     prev;
    u32                     rngPN;
    u32                     rngPF;
    u32                     rngFN;
    u32                     rngFF;
    s32                     packed;
    s32                     next;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    head  = SCRATCH_STACK_CURSOR(VECTOR);
    vec   = head - 1;

    SCRATCH_STACK_CURSOR(VECTOR) = vec;
    head[-1].vx                  = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    vec->vy                      = 0;
    vec->vz                      = Player_Status.coordMtx->t[2] - coord->coord.t[2];

    work->field_6AA = SquareRoot0(head[-1].vx * head[-1].vx + vec->vz * vec->vz);
    work->field_698 = ratan2((s16)head[-1].vx, (s16)vec->vz) & 0xFFF;

    switch (work->field_6A0) {
        case 0:
            if (((u32)((u8)Gp_StateC08.field_A - 2) >= 2U) && (gGameSession->location.loc.view != 2)) {
                if (work->field_6AA < 0x8FC) {
                    if (work->field_6B8 == work->field_6B6) {
                        pairNear    = D_actor_521100_8015F59C;
                        rngPN       = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rngPN;
                        next        = ((Actor521100StateChoice*)((u8*)pairNear + (work->field_6B6 * 4 + ((rngPN >> 16) & 1) * 2)))->state;
                    } else {
                        flatNear    = D_actor_521100_8015F57C;
                        rngFN       = Gp_LcgState * 5 + 0x71357911;
                        next        = flatNear[(rngFN >> 16) & 0xF];
                        Gp_LcgState = rngFN;
                    }
                } else {
                    if (work->field_6B8 == work->field_6B6) {
                        pairFar     = D_actor_521100_8015F5C8;
                        rngPF       = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rngPF;
                        next        = ((Actor521100StateChoice*)((u8*)pairFar + (work->field_6B6 * 4 + ((rngPF >> 16) & 1) * 2)))->state;
                    } else {
                        flatFar     = D_actor_521100_8015F5A8;
                        rngFF       = Gp_LcgState * 5 + 0x71357911;
                        next        = flatFar[(rngFF >> 16) & 0xF];
                        Gp_LcgState = rngFF;
                    }
                }
            } else {
                next = 2;
            }

            prev            = (u16)work->field_6B6;
            work->field_6B6 = next;
            work->field_6B8 = prev;

            switch (next) {
                case 0:
                    work->field_6A0  = 1;
                    work->field_686  = 5;
                    packed           = Gp_PackPair(D_actor_521100_8015F550, 0);
                    work->obj57C.key = packed;
                    work->obj59C.key = packed;
                    break;
                case 1:
                    work->field_6A0  = 2;
                    work->field_686  = 6;
                    packed           = Gp_PackPair(D_actor_521100_8015F550, 1);
                    work->obj57C.key = packed;
                    work->obj59C.key = packed;
                    break;
                case 2:
                    work->field_6A0  = 3;
                    work->field_6A2  = 0;
                    work->field_686  = 3;
                    packed           = Gp_PackPair(D_actor_521100_8015F550, 2);
                    work->obj57C.key = packed;
                    work->obj59C.key = packed;
                    break;
            }
            break;
        case 1:
            func_actor_521100_80133104(arg0);
            break;
        case 2:
            func_actor_521100_8013334C(arg0);
            break;
        case 3:
            func_actor_521100_801335B4(arg0);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// The scratch head is taken through `ActorScratchStack` rather than as
/// `SCRATCH_STACK_CURSOR`, which does not compile the same.
static void func_actor_521100_80133104(Task* arg0)
{
    GfxCoord*        coord;
    SVECTOR*         vec;
    SVECTOR*         head;
    s16*             clipPtr;
    s16              frame2;
    s16              frame3;
    s16              frame;
    s16              clip;
    s16              speed;
    s32              snd;
    s32              pan;
    GfxCoord*        effectCoord;
    s32              effect;
    s32              kind;
    SVECTOR*         offset;
    u16*             tbl;
    u16              clipId;
    u16              part;
    u32              rng;
    Actor521100Work* work;

    head                                                  = ((ActorScratchStack*)SCRATCH_STACK_CURSOR_SLOT)->head;
    vec                                                   = head - 1;
    ((ActorScratchStack*)SCRATCH_STACK_CURSOR_SLOT)->head = vec;
    work                                                  = arg0->work;
    frame                                                 = (s16)work->field_68A;
    clipPtr                                               = &D_actor_521100_8015F894[work->field_686];
    clip                                                  = *clipPtr;
    clipId                                                = (u16)*clipPtr;
    coord                                                 = arg0->extra.tmd->coords;
    if (frame == (clip + 0x1A)) {
        effect      = 0x60188;
        kind        = 0xC;
        effectCoord = coord;
        SOFT_TOUCH_REG(effectCoord);
        offset = NULL;
        SOFT_TOUCH_REG4(effect, kind, effectCoord, offset);
        Gp_SpawnEff(effect, &effectCoord[8], kind, offset);
        Gp_SpawnPadLerp(0xA, 0x40, 0xFF);
    } else if (frame == (clip + 0x1E)) {
        vec->vx = -0x320;
        vec->vy = 0x64;
        vec->vz = 0;
        Gp_SpawnEff(0x6009C, work->field_654->extra.tmd->coords, 0, vec);
    }
    frame2 = (s16)work->field_68A;
    if (frame2 == ((s16)clipId + 0x1C)) {
        work->field_6AE    = 1;
        work->obj57C.flags = (u16)(work->obj57C.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj59C.flags = (u16)(work->obj59C.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
        snd                = (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x401C0008;
        pan                = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
        speed = 0;
    } else {
        speed = 0;
        if (frame2 == ((s16)clipId + 0x28)) {
            work->field_6A6    = 0;
            work->obj57C.flags = (u16)(work->obj57C.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
            work->obj59C.flags = (u16)(work->obj59C.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        }
    }
    frame3 = (s16)work->field_68A;
    if (frame3 >= ((s16)clipId + 0x1C)) {
        if (((s16)clipId + 0x1E) >= frame3) {
            speed = 0x64;
        }
    }
    work->field_69A = speed;
    if ((s16)work->field_68A >= ((s16)clipId + 0x7A)) {
        work->field_686 = 1;
        tbl             = D_actor_521100_8015F5F4;
        rng             = (Gp_LcgState * 5) + 0x71357911;
        work->field_69E = 0;
        work->field_6A0 = 0;
        part            = tbl[(rng >> 16) & 0xF];
        Gp_LcgState     = rng;
        work->field_6AE = 0;
        work->field_68E = part;
    }
    ((ActorScratchStack*)SCRATCH_STACK_CURSOR_SLOT)->head = (SVECTOR*)((ActorScratchStack*)SCRATCH_STACK_CURSOR_SLOT)->head + 1;
}

/// Runs one frame of the burn-out sequence timed off the clip the slots are
/// playing: `D_actor_521100_8015F894[field_686]` is the clip's own length, read
/// signed and again unsigned because the cue frames below need it both ways,
/// and `field_68A` is the frame counter the blend in
/// `func_actor_521100_80135964` ticks. The counter is re-read at each cue
/// rather than carried, so the effects spawned in between cannot leave a stale
/// copy behind.
///
/// The cues, all offsets from that length: under +0x28 the turn limit
/// `field_69C` is held at 0x50; at +0x23 effect 0x60188 drops onto the attach
/// coordinate eight slots along and the 0xA/0x40/0xFF pad lerp starts; at
/// +0x27 the 8-byte scratch `SVECTOR` is thrown to (-0x320, 0x64, 0) and handed
/// to effect 0x6009C on the coordinate `field_654`'s own display object
/// carries; and +0x23 again, this time against the unsigned length, arms
/// `field_6AE` and raises the two record flags at 0x59A / 0x5BA together, then
/// cues `SndEvt_EnqueueType6` with the actor's pan and depth narrowed to bytes.
/// +0x2D hands the flags back down and clears the parked animation `field_6A6`.
///
/// The two ends are the motion: `field_69A` is held at 0x88 of forward speed
/// while the counter is between +0x20 and +0x2A of the length, and is zero
/// everywhere else, and past +0x90 the sequence starts over - clip 1, a fresh
/// effect id out of `D_actor_521100_8015F5F4` (the top four bits of an LCG
/// draw) into `field_68E`, and the state latch `field_69E`, its phase
/// `field_6A0` and the armed flag all cleared.
static void func_actor_521100_8013334C(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    SVECTOR*         head;
    SVECTOR*         vec;
    u16*             tbl;
    u32              rng;
    s16              clip;
    u16              clipId;
    s16              turn;
    s16              speed;
    s16              frame;
    s16              frame2;
    s32              frame3;
    s32              snd;
    s32              pan;

    work                          = arg0->work;
    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    vec                           = head - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = vec;
    clip                          = D_actor_521100_8015F894[work->field_686];
    clipId                        = D_actor_521100_8015F894[work->field_686];
    coord                         = arg0->extra.tmd->coords;

    turn = 0;
    if ((s16)work->field_68A < clip + 0x28) {
        turn = 0x50;
    }
    work->field_69C = turn;

    frame = (s16)work->field_68A;
    if (frame == clip + 0x23) {
        Gp_SpawnEff(0x60188, arg0->extra.tmd->coords + 8, 0xC, NULL);
        Gp_SpawnPadLerp(0xA, 0x40, 0xFF);
    } else if (frame == clip + 0x27) {
        vec->vx = -0x320;
        vec->vy = 0x64;
        vec->vz = 0;
        Gp_SpawnEff(0x6009C, work->field_654->extra.tmd->coords, 0, vec);
    }

    frame2 = (s16)work->field_68A;
    if (frame2 == (s16)clipId + 0x23) {
        work->field_6AE     = 1;
        work->obj57C.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj59C.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        snd                 = (((u32)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x401C0008;
        pan                 = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
        speed = 0;
    } else {
        speed = 0;
        if (frame2 == (s16)clipId + 0x2D) {
            work->field_6A6     = 0;
            work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }

    frame3 = (s16)work->field_68A;
    if ((s16)clipId + 0x20 < frame3) {
        if ((s16)clipId + 0x2A >= frame3) {
            speed = 0x88;
        }
    }
    work->field_69A = speed;
    if ((s16)work->field_68A >= (s16)clipId + 0x90) {
        work->field_686 = 1;
        work->field_69E = 0;
        work->field_6A0 = 0;
        tbl             = D_actor_521100_8015F5F4;
        rng             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState     = rng;
        work->field_68E = tbl[(rng >> 16) & 0xF];
        work->field_6AE = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
/// Step-0 body of the burn-out sequence: the transition into it and the two
/// respawn draws. `field_6A2` is a four-phase latch. Phase 0 waits out the clip
/// long enough for the actor to commit (`D_actor_521100_8015F894[field_686]`
/// plus 0x38) and then latches clip 4 and hands phase 1 a fresh effect id out
/// of `D_actor_521100_8015F5D4`, cueing the 0x401C0007 sound with the actor's
/// own pan and depth. Phase 1 counts the effect id down (unsigned `field_68E`,
/// tested as a halfword) and, when it lands, either arms the two collision
/// nodes with the type-2 pair and asks for clip 8, or - once `field_6AA` has
/// run out - drops the actor back to idle with a draw out of
/// `D_actor_521100_8015F5F4`.
///
/// Phase 2 walks the turn limit `field_69C` 0x3C up while the clip is young,
/// fires the 0x401C0009 cue, the effect 0x60188 on the eighth coordinate and
/// the 0xA/0x40/0xFF pad lerp together on the clip's 0x20th frame, holds the
/// forward speed at 0x64 across the 0x22..0x26 window, and at 0x27 latches
/// phase 3 and hands both record flags back. Phase 3 waits 0x5E frames and then
/// picks the finish off `coord->coord.t[0]`: under -0xFA0 the actor stays
/// burning (phase 2 of the latch, or 1 in the session's mode 2), otherwise it
/// resets to idle with the clip-1 draw.
static void func_actor_521100_801335B4(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    u32              rng;
    u16              timer;
    s16              turn;
    s32              snd;
    s32              pair;

    SCRATCH_PUSH_BYTES(0x18);
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;

    switch (work->field_6A2) {
        case 0:
            if ((s16)work->field_68A >= D_actor_521100_8015F894[work->field_686] + 0x38) {
                u16* tbl        = D_actor_521100_8015F5D4;
                work->field_686 = 4;
                work->field_6A2 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_68E = tbl[(Gp_LcgState >> 16) & 0xF];
                snd             = (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x401C0007;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            }
            break;
        case 1:
            timer           = work->field_68E - 1;
            work->field_68E = timer;
            if ((s16)timer > 0) {
                break;
            }
            if (work->field_6AA >= 0xDAC) {
                u16* tbl        = D_actor_521100_8015F5F4;
                work->field_69E = 0;
                work->field_6A0 = 0;
                work->field_6A2 = 0;
                work->field_686 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_68E = tbl[(Gp_LcgState >> 16) & 0xF];
            } else {
                work->field_6A2  = 2;
                work->field_686  = 8;
                pair             = Gp_PackPair(D_actor_521100_8015F550, 2);
                work->obj57C.key = pair;
                work->obj59C.key = pair;
            }
            break;
        case 2:
            turn = 0;
            if ((s16)work->field_68A < 0x20) {
                turn = 0x3C;
            }
            work->field_69C = turn;
            if ((s16)work->field_68A == 0x20) {
                work->field_6AE     = 1;
                work->obj57C.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                snd                 = (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x401C0009;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
                Gp_SpawnEff(0x60188, arg0->extra.tmd->coords + 8, 8, NULL);
                Gp_SpawnPadLerp(0xA, 0x40, 0xFF);
            }
            if ((u32)(work->field_68A - 0x22) < 5) {
                work->field_69A = 0x64;
            } else {
                work->field_69A = 0;
            }
            if ((s16)work->field_68A >= 0x27) {
                work->field_6A2     = 3;
                work->field_686     = 7;
                work->field_6A6     = 0;
                work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case 3:
            if ((s16)work->field_68A < 0x5E) {
                break;
            }
            if (gGameSession->location.loc.view == 2) {
                work->field_69E = 6;
                if (coord->coord.t[0] < -0xFA0) {
                    work->field_6A0 = 1;
                } else {
                    work->field_6A0 = 0;
                }
            } else if (coord->coord.t[0] < -0xFA0) {
                work->field_69E = 6;
                work->field_6A0 = 2;
            } else {
                u16* tbl        = D_actor_521100_8015F5F4;
                work->field_69E = 0;
                work->field_6A0 = 0;
                work->field_686 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_68E = tbl[(Gp_LcgState >> 16) & 0xF];
            }
            work->field_6AE = 0;
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

static void func_actor_521100_801339B0(Task* arg0)
{
    Actor521100Work*        work;
    GfxCoord*               coord;
    GfxCoord*               pcoord;
    Task*                   player;
    Actor521100FireScratch* sc;
    s32                     flag;
    s32                     i;
    s32                     snd;
    s32                     absDiff;
    s32                     angle;
    s16                     state;
    s16                     turn;
    u16                     timer;
    u32                     rng;
    u16*                    tbl;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    player = gameGetPtrSlot(3);
    SCRATCH_PUSH_BYTES(0x54);
    sc = SCRATCH_STACK_CURSOR(Actor521100FireScratch);

    switch (work->field_6A0) {
        case 0:
            if ((s16)work->field_68A == 0xA) {
                snd = (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 6;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            } else if ((s16)work->field_68A == 0xC) {
                flag                         = (Player_Status.coordMtx->m[0][2] * coord->coord.m[0][2] + Player_Status.coordMtx->m[1][2] * coord->coord.m[1][2] + Player_Status.coordMtx->m[2][2] * coord->coord.m[2][2]);
                work->field_6A4              = (u32)flag >> 31;
                sc->msg.source.sets          = D_actor_521100_8015F7CC;
                sc->msg.animationId          = work->field_6A4 ? 2 : 6;
                sc->msg.blend                = ANIMATION_BLEND_RESET;
                sc->msg.blendFrames          = 0;
                sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                Gp_DispatchMsgPtr(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
            } else if ((s16)work->field_68A >= 0x2B) {
                work->field_6A0              = 1;
                work->field_686              = 0xB;
                work->field_68E              = 0;
                work->field_690              = 0;
                sc->msg.source.sets          = D_actor_521100_8015F7CC;
                sc->msg.animationId          = work->field_6A4 ? 3 : 7;
                sc->msg.blend                = ANIMATION_BLEND_RESET;
                sc->msg.blendFrames          = 0;
                sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                Gp_DispatchMsgPtr(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
            }
            if ((u16)(work->field_68A - 4) < 9) {
                sc->vec.vz = 0x4E2;
                sc->vec.vx = 0;
                sc->vec.vy = 0;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&sc->vec);
                gte_rtv0();
                gte_stlvnl(&sc->pos);
                sc->pos.vx   = coord->coord.t[0] + sc->pos.vx;
                sc->pos.vy   = coord->coord.t[1] + sc->pos.vy;
                sc->pos.vz   = coord->coord.t[2] + sc->pos.vz;
                pcoord       = player->extra.tmd->coords;
                sc->delta.vx = sc->pos.vx - pcoord->coord.t[0];
                sc->delta.vy = 0;
                sc->delta.vz = sc->pos.vz - pcoord->coord.t[2];
                if ((SquareRoot0((sc->delta.vx * sc->delta.vx) + (sc->delta.vz * sc->delta.vz)) < 0x32) || ((s16)work->field_68A == 0xC)) {
                    sc->aim.pos.vx = sc->pos.vx;
                    sc->aim.pos.vy = sc->pos.vy;
                    sc->aim.pos.vz = sc->pos.vz;
                } else {
                    VectorNormal(&sc->delta, &sc->pos);
                    sc->aim.pos.vx = pcoord->coord.t[0] + ((sc->pos.vx * 0x32) >> 12);
                    sc->aim.pos.vy = pcoord->coord.t[1] + ((sc->pos.vy * 0x32) >> 12);
                    sc->aim.pos.vz = pcoord->coord.t[2] + ((sc->pos.vz * 0x32) >> 12);
                }
                angle          = ratan2(pcoord->coord.m[0][2], pcoord->coord.m[2][2]) & 0xFFF;
                flag           = (s16)work->field_696 - angle;
                sc->aim.rot.vx = 0;
                sc->aim.rot.vz = 0;
                if ((s16)work->field_68A == 0xC) {
                    sc->aim.rot.vy = work->field_696;
                } else {
                    absDiff = flag >= 0 ? flag : -flag;
                    if ((u32)(absDiff - 0x400) >= 0x801U) {
                        if (absDiff < 0x65) {
                            sc->aim.rot.vy = work->field_696;
                        } else if (flag > 0) {
                            sc->aim.rot.vy = angle + 0x64;
                        } else {
                            sc->aim.rot.vy = angle - 0x64;
                        }
                    } else {
                        if (absDiff < 0x65) {
                            sc->aim.rot.vy = (work->field_696 + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                        } else if (flag > 0) {
                            sc->aim.rot.vy = angle - 0x64;
                        } else {
                            sc->aim.rot.vy = angle + 0x64;
                        }
                    }
                }
                Gp_DispatchMsgPtr(player, 0x3E9, &sc->aim, 0);
            }
            break;
        case 1:
            flag = 0;
            if (work->field_68E == 2) {
                Gp_SpawnPadLerp(5, 0xC0, 0x80);
            }
            timer           = work->field_68E - 1;
            work->field_68E = timer;
            if ((s16)timer <= 0) {
                if (Player_Status.hp <= D_actor_521100_8015F570[Gp_StateF0.field_2B]) {
                    work->field_686              = 0x14;
                    work->field_6A0              = 5;
                    sc->msg.source.sets          = D_actor_521100_8015F7CC;
                    sc->msg.animationId          = work->field_6A4 ? 0xC : 0xD;
                    sc->msg.blend                = ANIMATION_BLEND_RESET;
                    sc->msg.blendFrames          = 0;
                    sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    Gp_DispatchMsgPtr(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
                    flag = 1;
                } else {
                    work->field_68E = 0x20;
                    Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(D_actor_521100_8015F550, 3), 0);
                }
            }
            if (flag != 1) {
                flag = 0;
                if ((work->field_6A8 == 1) && (Player_Status.hp < 0x3D) && (((D_actor_521100_8015F560.hpMax / 3) & 0xFFFF) >= ((GpEnemy*)arg0->spawnArg2.pointer)->hp)) {
                    flag = work->field_6A4 == 1;
                }
                if (flag != 0) {
                    work->field_6A0              = 3;
                    work->field_6A8              = 0;
                    work->field_68A              = 0;
                    sc->msg.source.sets          = D_actor_521100_8015F7CC;
                    sc->msg.animationId          = 5;
                    sc->msg.blend                = ANIMATION_BLEND_RESET;
                    sc->msg.blendFrames          = 0;
                    sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    Gp_DispatchMsgPtr(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
                } else {
                    if (work->field_6A8 == 0) {
                        timer           = work->field_690 + 1;
                        work->field_690 = timer;
                        if ((s16)timer < 0x97) {
                            break;
                        }
                    }
                    work->field_6A0              = 2;
                    work->field_686              = 0x13;
                    work->field_6A8              = 0;
                    sc->msg.source.sets          = D_actor_521100_8015F7CC;
                    sc->msg.animationId          = work->field_6A4 ? 9 : 0xA;
                    sc->msg.blend                = ANIMATION_BLEND_RESET;
                    sc->msg.blendFrames          = 0;
                    sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    Gp_DispatchMsgPtr(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
                }
            }
            break;
        case 2:
            if ((s16)work->field_68A == 0x22) {
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
            }
            if ((s16)work->field_68A == 0x25) {
                snd = (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x401C000F;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_68A < 0x45) {
                for (i = 0; i < 0x11; i++) {
                    if ((s16)work->field_68A < D_actor_521100_8015F80C[work->field_6A4][i].field_0) {
                        sc->vec.vx = D_actor_521100_8015F80C[work->field_6A4][i].field_2;
                        break;
                    }
                }
                sc->vec.vy = 0;
                sc->vec.vz = 0;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&sc->vec);
                gte_rtv0();
                gte_stlvnl(&sc->pos);
                pcoord         = player->extra.tmd->coords;
                sc->aim.pos.vx = pcoord->coord.t[0] + sc->pos.vx;
                sc->aim.pos.vy = pcoord->coord.t[1] + sc->pos.vy;
                sc->aim.pos.vz = pcoord->coord.t[2] + sc->pos.vz;
                sc->aim.rot.vx = 0;
                sc->aim.rot.vy = work->field_696;
                sc->aim.rot.vz = 0;
                Gp_DispatchMsgPtr(player, 0x3E9, &sc->aim, 0);
            }
            if ((s16)work->field_68A == 0x23) {
                Gp_SpawnEff(0x60054, player->extra.tmd->coords + 3, 0x80003400, NULL);
                Gp_SpawnEff(0x60054, player->extra.tmd->coords + 3, 0x80003400, NULL);
                Gp_SpawnEff(0x60054, player->extra.tmd->coords + 3, 0x80003400, NULL);
            }
            if ((s16)work->field_68A == 0x45) {
                sc->msg.source.sets          = D_actor_521100_8015F7CC;
                sc->msg.animationId          = 0xB;
                sc->msg.blend                = ANIMATION_BLEND_RESET;
                sc->msg.blendFrames          = 0;
                sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                Gp_DispatchMsgPtr(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
            }
            turn = 0;
            if ((s16)work->field_68A < 0x5F) {
                turn = -0x10;
            }
            work->field_69A = turn;
            if ((s16)work->field_68A == 0x6F) {
                coord          = player->extra.tmd->coords;
                sc->aim.pos.vx = coord->coord.t[0];
                sc->aim.pos.vy = coord->coord.t[1];
                sc->aim.pos.vz = coord->coord.t[2];
                sc->aim.rot.vx = 0;
                sc->aim.rot.vy = (work->field_696 + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                sc->aim.rot.vz = 0;
                Gp_DispatchMsgPtr(player, 0x3E9, &sc->aim, 0);
            }
            if (((s16)work->field_68A >= 0x6F) && (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0)) {
                Gp_DispatchMsg(player, 0x3F1, 0, 0);
            }
            if ((s16)work->field_68A >= 0xA4) {
                tbl             = D_actor_521100_8015F5F4;
                work->field_686 = 1;
                work->field_69E = 0;
                work->field_6A0 = 0;
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            break;
        case 3:
            if ((s16)work->field_68A == 0x20) {
                Gp_SpawnEff(0x60273, gameGetPtrSlot(3)->extra.tmd->coords + 0xC, 0, NULL);
                snd = (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x401C000E;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
                work->field_6A0              = 4;
                work->field_686              = 0xD;
                sc->msg.source.sets          = D_actor_521100_8015F7CC;
                sc->msg.animationId          = 4;
                sc->msg.blend                = ANIMATION_BLEND_RESET;
                sc->msg.blendFrames          = 0;
                sc->msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                Gp_DispatchMsgPtr(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->msg, 0);
            }
            break;
        case 4:
            if ((s16)work->field_68A == 0xF) {
                snd = (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x401C000C;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_68A == 0x64) {
                ((GpEnemy*)arg0->spawnArg2.pointer)->hp = 0;
            }
            break;
        case 5:
            if ((s16)work->field_68A == 0x1A) {
                ((GameActor*)player->work)->field_956 = 0xA;
                work->field_6A0                       = 6;
                work->field_68E                       = 0;
                gGameSession->deathRestartDelay       = 0x5A;
                gGameSession->deathSoundCountdown     = GAME_SESSION_DEATH_SOUND_HOLD;
                sc->vec.vx                            = 0;
                sc->vec.vy                            = -0x96;
                sc->vec.vz                            = 0xC8;
                func_800FDB18(1, gameGetPtrSlot(3)->extra.tmd->coords + 4, &sc->vec,
                              &D_actor_521100_8015F804);
                Gp_SpawnPadLerp(0xA, 0xFF, 8);
                Gp_DispatchMsg(player, 0x400, 0, 0);
                Player_Status.hp = 0;
            }
            break;
        case 6:
            state = (s16)work->field_68E;
            if (state != 1) {
                if (state < 2) {
                    if (state == 0) {
                        CdCmd_EnqueueLoadFile(9, 0x1E, 3);
                        work->field_68E = 1;
                    }
                }
            } else if ((CdCmd_IsIdle() & 0xFFFF) == state) {
                coord = gameGetPtrSlot(3)->extra.tmd->coords;
                SndEvt_EnqueueType6(0x70010001, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
                work->field_68E = 2;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x54);
}

/// Step-4 body of the burn-out sequence, the fourth of the ones the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is a
/// three-phase latch: phase 0 hands the record flags at 0x59A / 0x5BA back and
/// asks the slot blend for clip 0xE, phase 1 waits out 5 blended frames and
/// asks for clip 0xF, and phase 2 waits out 0x26 of them and then either drops
/// the actor to the idle state or, when `field_6BA` asks for it, on to state 6
/// at sub-state `field_6BC`. Phase 2 latches clip 1 for the blend and picks this
/// frame's effect out of `D_actor_521100_8015F634`, the same 4-bit draw
/// `func_actor_521100_8013570C` makes.
static void func_actor_521100_80134658(Task* arg0)
{
    Actor521100Work* work;
    u16*             tbl;
    u32              rng;

    work = arg0->work;
    switch (work->field_6A0) {
        case 0:
            work->field_686     = 0xE;
            work->field_6A0     = 1;
            work->field_69A     = 0;
            work->field_69C     = 0;
            work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case 1:
            if ((s16)work->field_68A >= 5) {
                work->field_686 = 0xF;
                work->field_6A0 = 2;
            }
            return;
        case 2:
            if ((s16)work->field_68A >= 0x26) {
                if (work->field_6BA == 0) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else {
                    work->field_69E = 6;
                    work->field_6A0 = work->field_6BC;
                }
                work->field_686 = 1;
                tbl             = D_actor_521100_8015F634;
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            return;
    }
}
/// State-6 body of the burn-out sequence, the last one the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is a
/// three-phase latch again and `D_actor_521100_8015F654` holds one waypoint per
/// phase; every phase seeds the slot blend the same way (`field_686` 0x12,
/// forward speed `field_69A` 0x14, turn limit `field_69C` 0x78) and builds the
/// vector from the attach coordinate's translation to its target into the
/// 0x18-byte scratch, of which only the `vec` half is written.
///
/// Phase 0 aims at the player (`Player_Status.coordMtx->t`) and hands the actor
/// back to state 1, speed zeroed, once it is within 0x7D0 of it and the
/// player's own Z is past -0x5DC; otherwise it aims at waypoint 0 and steps the
/// phase to 1 on arrival within 0x3C. Those two paths leave the switch
/// directly, while the ones that reach neither clear `field_69E` and
/// `field_6A0` - or raise `field_6BA` only, in a session whose `field_4` is 2 -
/// and then clear `field_6BC`. Phase 1 aims at the player and falls back to
/// waypoint 1 past 0x7D0, re-aiming at the player from 0x3C of that waypoint;
/// the within-0x7D0 path and the re-aim one share the epilogue that stops the
/// actor (`field_698` re-aimed, `field_69A` 0, `field_69C` 0x78, `field_69E` 1,
/// `field_6A0` 0), while the far one goes to `game`, where the phase steps to 2
/// and both `field_6BA` / `field_6BC` are cleared, or both raised when
/// `field_4` is 2. Phase 2 aims at waypoint 2 and, from the coordinate's X past
/// -0xFA0, clears `field_69E` and `field_6A0` (the session check there only
/// clears `field_6A0`), then drops both flags.
///
/// `sc2` is a second view of the same scratch that only phase 0's else branch
/// reads: CSE folds its initialisation into a copy of `sc`, and the
/// `do { ... } while (0)` around phase 1's `ratan2` is what keeps `head` ahead
/// of that copy in the register allocator's order - see
/// `DECOMPILATION_LEARNINGS.md`, "loop_depth as an allocation weight".
static void func_actor_521100_80134774(Task* arg0)
{
    Actor521100Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    ActorFaceScratch* sc2;
    u8*               head;
    s16               state;

    head                     = SCRATCH_STACK_CURSOR(u8);
    sc                       = (ActorFaceScratch*)(head - 0x18);
    sc2                      = sc;
    SCRATCH_STACK_CURSOR(u8) = (u8*)sc;
    work                     = arg0->work;
    coord                    = arg0->extra.tmd->coords;
    state                    = work->field_6A0;
    switch (state) {
        case 0:
            work->field_686 = 0x12;
            work->field_69A = 0x14;
            work->field_69C = 0x78;
            sc->delta.vx    = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->delta.vy    = 0;
            sc->delta.vz    = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            if ((SquareRoot0((sc->delta.vx * sc->delta.vx) + (sc->delta.vz * sc->delta.vz)) < 0x7D0) && (Player_Status.coordMtx->t[2] < -0x5DC)) {
                work->field_698 = (u16)(ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF);
                work->field_69A = 0;
                work->field_69C = 0x78;
                work->field_69E = 1;
                work->field_6A0 = 0;
            } else {
                sc2->delta.vx   = D_actor_521100_8015F654[0].vx - coord->coord.t[0];
                sc2->delta.vy   = 0;
                sc2->delta.vz   = D_actor_521100_8015F654[0].vz - coord->coord.t[2];
                work->field_698 = (u16)(ratan2((s16)sc2->delta.vx, (s16)sc2->delta.vz) & 0xFFF);
                if (SquareRoot0((sc2->delta.vx * sc2->delta.vx) + (sc2->delta.vz * sc2->delta.vz)) < 0x3C) {
                    work->field_6A0 = 1;
                } else {
                    if (gGameSession->location.loc.view != 2) {
                        work->field_69E = 0;
                        work->field_6A0 = 0;
                        work->field_6BA = 0;
                    } else {
                        work->field_6BA = 1;
                    }
                    work->field_6BC = 0;
                }
            }
            break;
        case 1:
            work->field_686 = 0x12;
            work->field_69A = 0x14;
            work->field_69C = 0x78;
            sc->delta.vx    = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->delta.vy    = 0;
            sc->delta.vz    = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            if (SquareRoot0((sc->delta.vx * sc->delta.vx) + (sc->delta.vz * sc->delta.vz)) >= 0x7D0) {
                sc->delta.vx = D_actor_521100_8015F654[1].vx - coord->coord.t[0];
                sc->delta.vy = 0;
                sc->delta.vz = D_actor_521100_8015F654[1].vz - coord->coord.t[2];
                do {
                    work->field_698 = (u16)(ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF);
                    if (SquareRoot0((sc->delta.vx * sc->delta.vx) + (sc->delta.vz * sc->delta.vz)) >= 0x3C) {
                        goto game;
                    }
                } while (0);
                sc->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy = 0;
                sc->delta.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            }
            work->field_698 = (u16)(ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF);
            work->field_69A = 0;
            work->field_69C = 0x78;
            work->field_69E = 1;
            work->field_6A0 = 0;
            break;
        game:
            if (gGameSession->location.loc.view != 2) {
                work->field_6A0 = 2;
                work->field_6BA = 0;
                work->field_6BC = 0;
            } else {
                work->field_6BA = 1;
                work->field_6BC = 1;
            }
            break;
        case 2:
            work->field_686 = 0x12;
            work->field_69A = 0x14;
            work->field_69C = 0x78;
            sc->delta.vx    = D_actor_521100_8015F654[2].vx - coord->coord.t[0];
            sc->delta.vy    = 0;
            sc->delta.vz    = D_actor_521100_8015F654[2].vz - coord->coord.t[2];
            work->field_698 = (u16)(ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF);
            if (coord->coord.t[0] < -0xFA0) {
                if (SquareRoot0((sc->delta.vx * sc->delta.vx) + (sc->delta.vz * sc->delta.vz)) < 0x3C) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else if (gGameSession->location.loc.view == state) {
                    work->field_6A0 = 0;
                }
            } else {
                if (gGameSession->location.loc.view != state) {
                    work->field_69E = 0;
                }
                work->field_6A0 = 0;
            }
            work->field_6BA = 0;
            work->field_6BC = 0;
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}
/// Steers the actor's heading towards the work block's `field_698` at up to
/// `field_69C` of turn per frame, then builds the result into the attach
/// coordinate as a pure-yaw rotation. The heading error is `field_698` minus
/// the coordinate's own Z-axis yaw (`ratan2` of `m[0][2]` over `m[2][2]`,
/// masked to the 12 bits the rotation is measured in), taken signed; the new
/// `field_696` is the target when the error is within the turn limit, and the
/// current yaw stepped by that limit otherwise. Errors past half a turn take
/// the short way round the wrap: the limit only has to beat `0x1000` minus the
/// error (or the error plus `0x1000`) to snap, so the turn never crosses into
/// the far half. `field_696` is read back as a signed half, the form the
/// sibling overlays' work blocks declare their yaw in; this body is the same
/// one `Actor02500_Fn016FC` and `func_actor_300700_80164794` carry.
static void func_actor_521100_80134C38(Task* arg0)
{
    Actor521100Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s32               ang;
    u16               want;
    s16               diff;
    s32               adiff;
    s32               step;
    s32               cur;
    s32               next;
    s32               wrapStep;

    sc    = (ActorFaceScratch*)SCRATCH_PUSH_BYTES(0x18);
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_698;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_696 = ang;
    if (adiff < 0x800) {
        step = work->field_69C;
        if (step >= adiff) {
            work->field_696 = want;
        } else {
            next = (s16)work->field_696;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_696 = next;
        }
    } else {
        step = work->field_69C;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_696 = work->field_698;
        goto done;
    turn:
        wrapStep = work->field_69C;
        cur      = (s16)work->field_696;
        if (diff > 0) {
            work->field_696 = cur - wrapStep;
        } else {
            work->field_696 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_696;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}
/// Plays the actor's footstep cues: while the animation record the cue body
/// reads carries `flags` bit 0x20 (or 0x10), a sound is queued on the frame
/// that bit has just dropped from `Actor521100Work::field_6B4`, panned and
/// depth-attenuated from the actor's display coordinate. The record is the one
/// `Gp_AnimGetRec` returns for the slot at 0x3C - the second of the 0x28-byte
/// slots the actor work blocks lay out from 0x14, the same one the other actor
/// overlays' cue bodies play from. The cue id is the `GpEnemy` work id's bits
/// 12+ placed in bits 8-11 with the overlay's 0x401C tag, 1 for the 0x20 foot
/// and 2 for the 0x10 one, and the record's two bits are latched for the next
/// frame at the end.
static void func_actor_521100_80134D88(Task* arg0)
{
    s32                    snd;
    s32                    pan;
    s32                    pan2;
    Actor521100Work*       work;
    GfxCoord*              coord;
    const AnimationRecord* rec;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    rec   = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec != NULL) {
        if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->field_6B4 & ANIMATION_RECORD_CUE_2)) {
            snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x401C0001;
            pan = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
        }
        if (!(rec->flags & ANIMATION_RECORD_CUE_1) && (work->field_6B4 & ANIMATION_RECORD_CUE_1)) {
            snd  = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x401C0002;
            pan2 = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(coord));
        }
        work->field_6B4 = (u16)(rec->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

/// Aims the actor's head coordinate (`field_8[4]`) at the player. Takes that
/// coordinate's `workm` into view space, offsets the player position by 0x600
/// in Y, rotates the delta into the body's frame, clamps it to +/-0x400 yaw,
/// +/-0x300 pitch and a minimum 0x200 forward, then builds the head rotation
/// from it. Same body as `func_actor_510900_80138BF0`.
///
/// `head` is kept as its own pointer rather than indexing `coord` twice: CSE
/// folds `head->workm` back onto `coord + 0x164` while `head` stays live, which
/// is what puts the `coord += 0x140` in the clamp's branch delay slot. The
/// `+ 0x600` likewise needs the temporary, or it is sunk into the subtrahend as
/// `- 0x600` on the player coordinate.
static void func_actor_521100_80134EDC(Task* arg0)
{
    ActorAimScratch* scratch;
    GfxCoord*        coord;
    GfxCoord*        head;
    s32              offsetY;

    coord = arg0->extra.tmd->coords;
    head  = &coord[4];
    SCRATCH_PUSH_BYTES(sizeof(ActorAimScratch));
    scratch = SCRATCH_STACK_CURSOR(ActorAimScratch);

    Gp_WorldToLocal(&gGfxViewCoord.workm, &head->workm, &scratch->view);
    scratch->delta.vx = Player_Status.coordMtx->t[0] - scratch->view.t[0];
    offsetY           = scratch->view.t[1] + 0x600;
    scratch->delta.vy = Player_Status.coordMtx->t[1] - offsetY;
    scratch->delta.vz = Player_Status.coordMtx->t[2] - scratch->view.t[2];
    ApplyTransposeMatrixLV(&coord->coord, &scratch->delta, &scratch->local);

    if (scratch->local.vx < -0x400) {
        scratch->local.vx = -0x400;
    } else if (scratch->local.vx > 0x400) {
        scratch->local.vx = 0x400;
    }
    if (scratch->local.vy < -0x300) {
        scratch->local.vy = -0x300;
    } else if (scratch->local.vy > 0x300) {
        scratch->local.vy = 0x300;
    }
    if (scratch->local.vz < 0x200) {
        scratch->local.vz = 0x200;
    }
    Gp_OrientAlong(&scratch->local, &head->coord, 0);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorAimScratch));
}
/// Untwists the coordinate at `field_8[3]`, which `func_actor_521100_801322F8`
/// left rotated by the random residual in `Actor521100Work::field_678` on the
/// frame the actor took a hit. The residual is turned into a matrix and
/// multiplied into that coordinate's own by `Gp_MulMatrix0`'s three `rtir`
/// passes - `rtir` multiplies the GTE rotation matrix by the vector in
/// `IR1..IR3`, so the body loads the coordinate's matrix, then each row of the
/// scratch matrix in turn, storing each result back over the coordinate. The
/// two angles are then stepped 0x20 towards zero; `field_680`, the flag the hit
/// body armed, survives while either is still moving and is cleared on the
/// frame both arrive, which is what the update body tests before calling this.
///
/// Same body as `Actor02000_Fn01698` and `func_actor_510900_80138D38`.
static void func_actor_521100_80135024(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    matrix = SCRATCH_STACK_CURSOR(MATRIX);
    active = 0;
    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    RotMatrix(&work->field_678, matrix);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(matrix);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&matrix->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&matrix->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);
    angleX = work->field_678.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->field_678.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->field_678.vx = nextX;
            active             = 1;
        }
    }
    angleY = work->field_678.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->field_678.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->field_678.vy = nextY;
            active             = 1;
        }
    }
    if (active == 0) {
        work->field_680 = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}
/// The burn-out tick `func_actor_521100_80135414` runs while the sequence state
/// `field_68C` is non-zero. `field_68E` counts the frames since the last effect
/// and fires one once it reaches `D_actor_521100_8015F8CC[field_68C]` — every 7
/// frames while the body is alight in state 1, then 0xE and 0x1C as it burns
/// down. Every effect splashes part 3 of the model's coordinate array; in state
/// 1 a second one lands on a random other part, picked out of
/// `D_actor_521100_8015F8BC` by the top three bits of an LCG draw. `field_690`
/// is the sequence's own clock, walking the state 1 -> 2 at 0xF0 frames, 2 -> 3
/// at 0x14A and 3 -> 0 at 0x1A4, where the tick stops.
static void func_actor_521100_80135230(Task* arg0)
{
    Actor521100Work* work;
    u16              timer;
    s16*             tbl;
    s16              part;

    work            = arg0->work;
    timer           = work->field_68E + 1;
    work->field_68E = timer;
    if ((s16)timer >= D_actor_521100_8015F8CC[work->field_68C]) {
        work->field_68E = 0U;
        func_800FDB18(3, &arg0->extra.tmd->coords[3], NULL, &work->eff);
        if (work->field_68C == 1) {
            tbl         = D_actor_521100_8015F8BC;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            part        = tbl[(Gp_LcgState >> 16) & 7];
            func_800FDB18(3, &arg0->extra.tmd->coords[part], NULL, &work->eff);
        }
    }
    timer           = work->field_690 + 1;
    work->field_690 = timer;
    if ((s16)timer == 0xF0) {
        work->field_68C = 2;
    }
    if ((s16)work->field_690 == 0x14A) {
        work->field_68C = 3;
    }
    if ((s16)work->field_690 >= 0x1A4) {
        work->field_68C = 0;
    }
}

/// State handlers of the actor's second part, which `func_actor_521100_80135AE4`
/// dispatches through: the setup `func_actor_521100_80135B40`, the per-frame
/// tick `func_actor_521100_80135B80` and `Gp_DestroyEnemy`.
static const GpEnemyTaskFuncTable3 D_actor_521100_80131E40 = { {
    func_actor_521100_80135B40,
    func_actor_521100_80135B80,
    Gp_DestroyEnemy,
} };

/// The actor's task body: runs the handler for `Task::state` out of a two-entry
/// table built on the stack - the spawn state `func_actor_521100_80131E8C`,
/// then the per-frame state `func_actor_521100_801353CC` - passing the task's
/// `GpEnemy` along with the task.
void func_actor_521100_80135378(Task* task)
{
    GpEnemyTaskFunc fns[2] = {
        func_actor_521100_80131E8C,
        func_actor_521100_801353CC,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_521100_801353CC(GpEnemy* arg0, Task* arg1)
{
    Actor521100Work* work;

    work = arg1->work;
    if (gGameSession->eventState != 0) {
        work->field_682 = 1;
        func_actor_521100_80135414(arg0, arg1);
        return;
    }
    work->field_682 = 0;
    func_actor_521100_80135478(arg0, arg1);
}

static void func_actor_521100_80135414(GpEnemy* arg0, Task* arg1)
{
    Actor521100Work* temp_s0;

    temp_s0                      = arg1->work;
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    func_actor_521100_80135964(arg1);
    func_actor_521100_80135A34(arg1);
    func_actor_521100_80135A90(arg1);
    if (temp_s0->field_68C != 0) {
        func_actor_521100_80135230(arg1);
    }
}

static void func_actor_521100_80135478(GpEnemy* arg0, Task* arg1)
{
    GfxCoord*        temp_s2;
    TmdObject*       temp_a1;
    Actor521100Work* temp_s1;
    s32              state;
    s32              one;

    temp_a1 = arg1->extra.tmd;
    state   = Gp_StateF0.field_4;
    temp_s1 = arg1->work;
    temp_s2 = temp_a1->coords;
    one     = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    temp_a1->flags                       = 0;
    temp_s1->field_654->extra.tmd->flags = 0;
    arg0->node.state.parts.flags         = WORLD_TARGET_HIDE_HP;
    goto default_body;
case2:
    temp_a1->flags                       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    temp_s1->field_654->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags         = one;
    return;
default_body:
    if (temp_s1->field_6B0 == 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    func_actor_521100_801322F8(arg1, temp_a1, one);
    func_actor_521100_801355C8(arg1);
    func_actor_521100_80134C38(arg1);
    func_actor_521100_801358D4(arg1);
    func_actor_521100_80134D88(arg1);
    func_actor_521100_80135964(arg1);
    func_actor_521100_80134EDC(arg1);
    if (temp_s1->field_680 != 0) {
        func_actor_521100_80135024(arg1);
    }
    temp_s2->composeStamp                   = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(temp_s2);
case1:
    func_actor_521100_80135A34(arg1);
    func_actor_521100_80135A90(arg1);
}

static void func_actor_521100_801355C8(Task* arg0)
{
    s16 temp_v1;

    temp_v1 = ((Actor521100Work*)arg0->work)->field_69E;
    switch (temp_v1) {
        case 0:
            func_actor_521100_80132958(arg0);
            return;
        case 1:
            func_actor_521100_80132DE8(arg0);
            return;
        case 2:
            func_actor_521100_801339B0(arg0);
            return;
        case 3:
            func_actor_521100_8013570C(arg0);
            return;
        case 4:
            func_actor_521100_80134658(arg0);
            return;
        case 5:
            func_actor_521100_801357F0(arg0);
            return;
        case 6:
            func_actor_521100_80134774(arg0);
        default:
            return;
    }
}

/// Steps the actor into state 1 once its facing has come within 45 degrees of
/// the angle at `field_696`, then stops it: both speeds are zeroed.
static void func_actor_521100_80135680(Task* arg0)
{
    Actor521100Work* work;
    s16              delta;
    s16              angle;
    s16              wrapped;
    s32              magnitude;

    work      = arg0->work;
    delta     = work->field_698 - work->field_696;
    magnitude = abs(delta);
    if (magnitude < 0x800) {
        angle = magnitude;
    } else {
        if (delta > 0) {
            wrapped = 0x1000 - delta;
        } else {
            wrapped = delta + 0x1000;
        }
        angle = wrapped;
    }
    if ((angle < 0x200) && (work->field_6AA < 0xDAC)) {
        work->field_69E = 1;
        work->field_6A0 = 0;
        work->field_69A = 0;
        work->field_69C = 0;
    }
}

/// Step-3 body of the burn-out sequence, the third of the three the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is its own
/// two-phase latch: phase 0 hands the record flags at 0x59A / 0x5BA back and
/// asks the slot blend for clip 0x10, phase 1 waits out 0x37 blended frames and
/// then either drops the actor to the idle state or, when `field_6BA` asks for
/// it, on to state 6 at sub-state `field_6BC`. Either way it latches clip 1 for
/// the blend and picks this frame's effect out of `D_actor_521100_8015F634`.
static void func_actor_521100_8013570C(Task* arg0)
{
    Actor521100Work* work;
    u16*             tbl;
    u32              rng;

    work = arg0->work;
    switch (work->field_6A0) {
        case 0:
            work->field_686     = 0x10;
            work->field_6A0     = 1;
            work->field_69A     = 0;
            work->field_69C     = 0;
            work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case 1:
            if ((s16)work->field_68A >= 0x37) {
                if (work->field_6BA == 0) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else {
                    work->field_69E = 6;
                    work->field_6A0 = work->field_6BC;
                }
                work->field_686 = 1;
                tbl             = D_actor_521100_8015F634;
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            return;
    }
}
/// Step-5 body of the burn-out sequence, the same two-phase `field_6A0` latch
/// `func_actor_521100_8013570C` runs with the longer timing: phase 0 hands the
/// record flags at 0x59A / 0x5BA back and asks the slot blend for clip 0x11,
/// phase 1 waits out 0x48 blended frames and then either drops the actor to the
/// idle state or, when `field_6BA` asks for it, on to state 6 at sub-state
/// `field_6BC`. Either way it latches clip 1 for the blend and picks this
/// frame's effect out of `D_actor_521100_8015F5F4`.
static void func_actor_521100_801357F0(Task* arg0)
{
    Actor521100Work* work;
    u16*             tbl;
    u32              rng;

    work = arg0->work;
    switch (work->field_6A0) {
        case 0:
            work->field_686     = 0x11;
            work->field_6A0     = 1;
            work->field_69A     = 0;
            work->field_69C     = 0;
            work->obj57C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj59C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case 1:
            if ((s16)work->field_68A >= 0x48) {
                if (work->field_6BA == 0) {
                    work->field_69E = 0;
                    work->field_6A0 = 0;
                } else {
                    work->field_69E = 6;
                    work->field_6A0 = work->field_6BC;
                }
                work->field_686 = 1;
                tbl             = D_actor_521100_8015F5F4;
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_68E = tbl[(rng >> 16) & 0xF];
            }
            return;
    }
}
/// Snapshots the attach coordinate's translation into the work block, then
/// walks the coordinate forward: 0x80 up, and along its own facing axis
/// (`m[0][2]` / `m[2][2]`) scaled by the work block's speed in 12-bit fixed
/// point.
static void func_actor_521100_801358D4(Task* arg0)
{
    GfxCoord*        coord;
    Actor521100Work* work;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    work->field_64C    = coord->coord.t[0];
    work->field_64E    = coord->coord.t[1];
    work->field_650    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_69A) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_69A) >> 12;
}

/// Blends every animation slot towards the clip latched in `field_686` while
/// it differs from the clip the slots carry, then ticks them once they agree:
/// the blend runs the nineteen slots through `func_800B4114` with the length
/// `D_actor_521100_8015F894` gives the incoming clip, and the tick counts the
/// agreeing frames in `field_68A`.
static void func_actor_521100_80135964(Task* arg0)
{
    Actor521100Work* work;
    s32              i;
    s32              val;

    work = arg0->work;
    val  = 0;
    if (work->field_686 != work->field_688) {
        work->field_688 = work->field_686;
        work->field_68A = 0;
        if (work->field_686 < 0x15) {
            val = D_actor_521100_8015F894[work->field_686];
        }
        i = 1;
        do {
            func_800B4114(&work->rig.anim, i, work->field_686, 0, val);
            i++;
        } while (i < 0x13);
        return;
    }
    i                = 1;
    work->field_68A += i;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Colours the actor from the world position of its second model coordinate,
/// handing it to `Gp_UpdateActorColor` with no blend parameters.
static void func_actor_521100_80135A34(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = &arg0->extra.tmd->coords[1];
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Draws the actor's ground shadow: a quad at the second model coordinate's
/// x/z and the first's y, so it lies on the ground under the actor.
static void func_actor_521100_80135A90(Task* arg0)
{
    GfxCoord* coord;
    GfxCoord* sub;
    VECTOR3   vec;

    coord  = &arg0->extra.tmd->coords[0];
    sub    = &arg0->extra.tmd->coords[1];
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, 0x80);
}

/// Task body of the actor's second part: copies `D_actor_521100_80131E40`
/// onto the stack and runs the handler for `Task::state` on the task's
/// `GpEnemy`.
void func_actor_521100_80135AE4(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_521100_80131E40;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Setup state of the actor's second part: hangs its model coordinate under
/// the parent model's ninth coordinate, draws it under the parent work block's
/// light and colour matrices, shows it and moves the task on to its tick.
static void func_actor_521100_80135B40(GpEnemy* enemy, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor521100Work* work;
    GfxCoord*        coord;
    GfxCoord*        parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = (Actor521100Work*)parent->work;

    coord->parent = &parentCoords[8];
    obj->lightMtx = &work->light;
    obj->flags    = 0;
    obj->colorMtx = &work->color;
    task->state   = 1;
}

static void func_actor_521100_80135B80(GpEnemy* arg0, Task* task)
{
    TmdObject*       obj;
    Actor521100Work* work;
    s16              mode;

    work = (Actor521100Work*)task->parent->work;
    obj  = task->extra.tmd;
    if (work->field_682 != 0) {
        mode       = ((work->field_692 & 1) == 0) << 7;
        obj->flags = mode;
        if (work->field_692 & 2) {
            obj->flags = mode | 4;
        }
        if (work->field_694 != 0) {
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

s32 func_actor_521100_80135BEC(Task* arg0)
{
    if (Player_Status.hp > 0) {
        ((Actor521100Work*)arg0->work)->field_6A8 = 1;
    }
    return 0;
}

s32 func_actor_521100_80135C14(Task* arg0, s32 arg1, AnimationPlayRequest* args)
{
    Actor521100Work* work;
    s32              i;
    s32              frames;
    s16              clip;
    s16              base;

    frames = 0;
    work   = arg0->work;
    base   = 0x1D;
    if (args->source.index == 0) {
        base = 0x14;
    }
    clip            = args->animationId + base;
    work->field_686 = clip;
    work->field_688 = clip;
    if (args->blend != ANIMATION_BLEND_RESET) {
        frames = args->blendFrames;
    }
    for (i = 1; i < 0x13; i++) {
        func_800B4114(&work->rig.anim, i, work->field_686, 0, frames);
    }
    return 0;
}

/// Message 0x7D4 handler in `D_actor_521100_8015F6FC`, placing the actor: builds the root coordinate's
/// matrix from the argument block's angles, stores its translation and clears
/// `composeStamp` so the world matrix is recomputed.
s32 func_actor_521100_80135CAC(Task* task, s32 arg1, ActorTransform* args)
{
    TmdObject* ext   = task->extra.tmd;
    GfxCoord*  coord = ext->coords;

    RotMatrix(&args->rot, &coord->coord);
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
