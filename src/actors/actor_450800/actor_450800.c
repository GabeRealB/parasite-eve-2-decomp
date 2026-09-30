#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/shelter_b6_nursery.h"

// The engine copies words across the exported animation bank and its
// following argument records. Both views cover the complete backing object.
typedef union {
    struct {
        AnimationSet*        sets[17];
        AnimationPlayRequest arguments[3];
    } data;
    s32 words[32];
} Actor450800AnimCopy9310;
STATIC_ASSERT_SIZEOF(Actor450800AnimCopy9310, 128);

extern Actor450800AnimCopy9310 D_actor_450800_80139310;

typedef union {
    struct {
        AnimationSet*        sets[11];
        AnimationPlayRequest arguments[5];
    } data;
    s32 words[36];
} Actor450800AnimCopy94BC;
STATIC_ASSERT_SIZEOF(Actor450800AnimCopy94BC, 144);

extern Actor450800AnimCopy94BC D_actor_450800_801394BC;

/// Work block of the overlay's own actor, allocated zeroed by its spawn
/// routine and kept at `Task::work`; the overlay's enemy uses
/// `Actor150400Work` instead, and the two dispatchers keep the blocks apart.
/// `light` and `color` are the matrices the actor's model is lit with, `rig`
/// and `st` its animation rig and state, and `turnFrames` the frames of
/// turning left while animation 3 plays. `field_4F0` .. `field_4F8` are the
/// helper tasks the spawn routine starts and the exit callback kills.
/// `animArg` is the argument the blended reseed passes on, and `field_4FE`
/// the approach mode the last approach command selected.
typedef struct Actor450800Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
    s16             turnFrames;
    byte            pad_4EE[0x2];
    Task*           field_4F0;
    Task*           field_4F4;
    Task*           field_4F8;
    s16             animArg;
    s16             field_4FE;
    u8              field_500; // 0x7DB mode 1 latches the copied flags here, 2 the 0x84 state
    byte            pad_501[0x3];
} Actor450800Work;
STATIC_ASSERT_SIZEOF(Actor450800Work, 0x504);

/// Spawn offset `func_actor_450800_80132108` copies into a local and hands to
/// `Gp_SpawnEff` as the effect's position.
static const SVECTOR D_actor_450800_80131E24 = { 0x19C8, -0x578, 0x3C0, 0 };

/// Message table `func_actor_450800_80132160` hangs off `Task::msgTable`, and
/// the `TaskDesc` table its three helper tasks come from.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*);
        s32 (*call2)(Task*, s32, ActorCommand* request, s32);
        s32 (*call3)(Task*, s32, ActorTransform*);
        s32 (*call4)(Task*, s32, VECTOR*);
        s32 (*call5)(Task*, s32, VECTOR*, s32);
        s32 (*call6)(Task*, s32, s32);
    } handler;
} Actor450800MsgEntry;
STATIC_ASSERT_SIZEOF(Actor450800MsgEntry, 8);

extern Actor450800MsgEntry D_actor_450800_8014AC58[];
extern TaskDesc            D_actor_450800_8014AC88[];

/// Animation data `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_450800_8014ACC4[];

/// The enemy's message table, the `TaskDesc` table its model tasks come from,
/// and the animation data its work block's slots are seeded from - the same
/// three roles as the actor's tables above.
extern Actor450800MsgEntry D_actor_450800_801539AC[];
extern TaskDesc            D_actor_450800_801539DC[];
extern u8                  D_actor_450800_801539F4[];

extern s32                  D_actor_450800_8013930C;
extern AnimationPlayRequest D_actor_450800_801397A4;
extern ActorTransform       D_actor_450800_801398EC;
extern GpEvsCmd             D_actor_450800_8013A564[];
extern GpEvsCmd             D_actor_450800_8013A684[];
extern GpEvsCmd             D_actor_450800_8013A774[];
extern GpEvsCmd             D_actor_450800_8013A984[];
extern GpEvsCmd             D_actor_450800_8013AB7C[];
extern GpEvsCmd             D_actor_450800_8013ACFC[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void func_actor_450800_80132448(Task* task);
static void func_actor_450800_801327E4(GpEnemy* enemy, Task* task);
static void func_actor_450800_80132868(Task* task);
static void func_actor_450800_801328BC(Task* task);
static void func_actor_450800_80132A1C(Task* task);
static void func_actor_450800_80132A68(Task* task);
static void func_actor_450800_80132AE0(Task* task);
static void func_actor_450800_801330AC(Task* task);
static void func_actor_450800_801332B8(GpEnemy* enemy, Task* task);
static void func_actor_450800_8013333C(Task* task);
static void func_actor_450800_80133364(Task* task);
static void func_actor_450800_80133400(Task* task);
static void func_actor_450800_8013344C(Task* task);
static void func_actor_450800_801334C4(Task* task);

extern TmdSource D_actor_450800_80140604;
extern TmdSource D_actor_450800_80145148;
extern TmdSource D_actor_450800_8014A048;
extern TmdSource D_actor_450800_8014A538;
extern TmdSource D_actor_450800_8014AC34;
void             func_actor_450800_80132790(Task*);
void             func_actor_450800_80132958(Task*);

s32 func_actor_450800_80132B44(Task*, s32, AnimationPlayRequest*);
s32 func_actor_450800_80132BB0(Task*, s32, s32);
s32 func_actor_450800_80132C68(Task*, s32, ActorTransform* placement);
s32 func_actor_450800_80132CE0(Task*, s32, ActorCommand* msg, s32);
s32 func_actor_450800_80132D74(Task*, s32, VECTOR*, s32);

extern TmdSource D_actor_450800_80150024;
extern TmdSource D_actor_450800_80150568;
s32              func_actor_450800_80133528(Task*, s32, AnimationPlayRequest*);
s32              func_actor_450800_80133594(Task*, s32, s32);
s32              func_actor_450800_801335F8(Task*, s32, ActorTransform* placement);
s32              func_actor_450800_80133670(void);
s32              func_actor_450800_80133678(Task*, s32, VECTOR*);
void             func_actor_450800_80133264(Task*);
void             func_actor_450800_80133740(Task*);

extern AnimationPlayRequest D_actor_450800_80139560;
extern AnimationPlayRequest D_actor_450800_80139628;
extern AnimationPlayRequest D_actor_450800_80139894;
extern ActorTransform       D_actor_450800_8013994C;
void                        func_actor_450800_80131F28(s32);
void                        func_actor_450800_80132080(void);

extern AnimationPlayRequest D_actor_450800_80139458;
extern AnimationPlayRequest D_actor_450800_8013946C;
extern AnimationPlayRequest D_actor_450800_80139480;
extern AnimationPlayRequest D_actor_450800_80139494;
extern AnimationPlayRequest D_actor_450800_801394A8;
extern AnimationPlayRequest D_actor_450800_801395C4;
extern AnimationPlayRequest D_actor_450800_801395D8;
extern AnimationPlayRequest D_actor_450800_801397CC;
extern AnimationPlayRequest D_actor_450800_801397E0;
extern AnimationPlayRequest D_actor_450800_801397F4;
extern AnimationPlayRequest D_actor_450800_80139808;
extern AnimationPlayRequest D_actor_450800_8013981C;
extern AnimationPlayRequest D_actor_450800_80139830;
extern AnimationPlayRequest D_actor_450800_801398A8;
extern AnimationPlayRequest D_actor_450800_8013ADEC;
extern AnimationPlayRequest D_actor_450800_8013AE00;
extern AnimationPlayRequest D_actor_450800_8013AE14;
extern ActorCommand         D_actor_450800_801398E0;
extern ActorCommand         D_actor_450800_801398E4;
extern GpCopyArg            D_actor_450800_801398D0;
extern GpCopyArg            D_actor_450800_801398D8;
extern ActorTransform       D_actor_450800_8013AE30;
extern ActorTransform       D_actor_450800_8013AE48;
extern ActorTransform       D_actor_450800_8013AE60;
void                        func_actor_450800_80131F28(s32);
void                        func_actor_450800_80132080(void);
void                        func_actor_450800_801320E8(s32);
void                        func_actor_450800_80132108(void);

extern AnimationPlayRequest D_actor_450800_80139390;
extern AnimationPlayRequest D_actor_450800_801393A4;
extern AnimationPlayRequest D_actor_450800_801393CC;
extern AnimationPlayRequest D_actor_450800_801393E0;
extern AnimationPlayRequest D_actor_450800_801393F4;
extern AnimationPlayRequest D_actor_450800_80139408;
extern AnimationPlayRequest D_actor_450800_8013941C;
extern AnimationPlayRequest D_actor_450800_80139430;
extern AnimationPlayRequest D_actor_450800_8013954C;
extern AnimationPlayRequest D_actor_450800_80139574;
extern AnimationPlayRequest D_actor_450800_80139588;
extern AnimationPlayRequest D_actor_450800_80139650;
extern AnimationPlayRequest D_actor_450800_80139664;
extern AnimationPlayRequest D_actor_450800_80139678;
extern AnimationPlayRequest D_actor_450800_8013968C;
extern AnimationPlayRequest D_actor_450800_801396A0;
extern AnimationPlayRequest D_actor_450800_801396C8;
extern AnimationPlayRequest D_actor_450800_801396DC;
extern AnimationPlayRequest D_actor_450800_801396F0;
extern AnimationPlayRequest D_actor_450800_80139704;
extern AnimationPlayRequest D_actor_450800_80139718;
extern AnimationPlayRequest D_actor_450800_8013972C;
extern AnimationPlayRequest D_actor_450800_80139740;
extern AnimationPlayRequest D_actor_450800_80139754;
extern AnimationPlayRequest D_actor_450800_80139768;
extern AnimationPlayRequest D_actor_450800_8013977C;
extern AnimationPlayRequest D_actor_450800_80139790;
void                        func_actor_450800_80131F28(s32);
void                        func_actor_450800_80131F70(u32);
void                        func_actor_450800_80131F98(s32);

AnimationPackedPose D_actor_450800_801337B0[2] = {
#include "assets/actor_450800_animation_01B20_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801337C8[22] = {
#include "assets/actor_450800_animation_01B20_bank4.inc"
};

AnimationRecord D_actor_450800_80133820[62] = {
#include "assets/actor_450800_animation_01B20_records.inc"
};

u16 D_actor_450800_80133918[20] = {
#include "assets/actor_450800_animation_01B20_indices.inc"
};

AnimationSet D_actor_450800_80133940 = {
    D_actor_450800_80133820,
    D_actor_450800_80133918,
    { NULL, D_actor_450800_801337B0, NULL, NULL, D_actor_450800_801337C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80133968[2] = {
#include "assets/actor_450800_animation_01DFC_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80133980[41] = {
#include "assets/actor_450800_animation_01DFC_bank4.inc"
};

AnimationRecord D_actor_450800_80133A24[116] = {
#include "assets/actor_450800_animation_01DFC_records.inc"
};

u16 D_actor_450800_80133BF4[20] = {
#include "assets/actor_450800_animation_01DFC_indices.inc"
};

AnimationSet D_actor_450800_80133C1C = {
    D_actor_450800_80133A24,
    D_actor_450800_80133BF4,
    { NULL, D_actor_450800_80133968, NULL, NULL, D_actor_450800_80133980, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80133C44[2] = {
#include "assets/actor_450800_animation_02070_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80133C5C[34] = {
#include "assets/actor_450800_animation_02070_bank4.inc"
};

AnimationRecord D_actor_450800_80133CE4[97] = {
#include "assets/actor_450800_animation_02070_records.inc"
};

u16 D_actor_450800_80133E68[20] = {
#include "assets/actor_450800_animation_02070_indices.inc"
};

AnimationSet D_actor_450800_80133E90 = {
    D_actor_450800_80133CE4,
    D_actor_450800_80133E68,
    { NULL, D_actor_450800_80133C44, NULL, NULL, D_actor_450800_80133C5C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80133EB8[2] = {
#include "assets/actor_450800_animation_022D8_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80133ED0[31] = {
#include "assets/actor_450800_animation_022D8_bank4.inc"
};

AnimationRecord D_actor_450800_80133F4C[97] = {
#include "assets/actor_450800_animation_022D8_records.inc"
};

u16 D_actor_450800_801340D0[20] = {
#include "assets/actor_450800_animation_022D8_indices.inc"
};

AnimationSet D_actor_450800_801340F8 = {
    D_actor_450800_80133F4C,
    D_actor_450800_801340D0,
    { NULL, D_actor_450800_80133EB8, NULL, NULL, D_actor_450800_80133ED0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80134120[2] = {
#include "assets/actor_450800_animation_02538_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80134138[30] = {
#include "assets/actor_450800_animation_02538_bank4.inc"
};

AnimationRecord D_actor_450800_801341B0[96] = {
#include "assets/actor_450800_animation_02538_records.inc"
};

u16 D_actor_450800_80134330[20] = {
#include "assets/actor_450800_animation_02538_indices.inc"
};

AnimationSet D_actor_450800_80134358 = {
    D_actor_450800_801341B0,
    D_actor_450800_80134330,
    { NULL, D_actor_450800_80134120, NULL, NULL, D_actor_450800_80134138, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80134380[2] = {
#include "assets/actor_450800_animation_02840_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80134398[44] = {
#include "assets/actor_450800_animation_02840_bank4.inc"
};

AnimationRecord D_actor_450800_80134448[124] = {
#include "assets/actor_450800_animation_02840_records.inc"
};

u16 D_actor_450800_80134638[20] = {
#include "assets/actor_450800_animation_02840_indices.inc"
};

AnimationSet D_actor_450800_80134660 = {
    D_actor_450800_80134448,
    D_actor_450800_80134638,
    { NULL, D_actor_450800_80134380, NULL, NULL, D_actor_450800_80134398, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80134688[2] = {
#include "assets/actor_450800_animation_02A04_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801346A0[24] = {
#include "assets/actor_450800_animation_02A04_bank4.inc"
};

AnimationRecord D_actor_450800_80134700[63] = {
#include "assets/actor_450800_animation_02A04_records.inc"
};

u16 D_actor_450800_801347FC[20] = {
#include "assets/actor_450800_animation_02A04_indices.inc"
};

AnimationSet D_actor_450800_80134824 = {
    D_actor_450800_80134700,
    D_actor_450800_801347FC,
    { NULL, D_actor_450800_80134688, NULL, NULL, D_actor_450800_801346A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_8013484C[2] = {
#include "assets/actor_450800_animation_02C30_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80134864[33] = {
#include "assets/actor_450800_animation_02C30_bank4.inc"
};

AnimationRecord D_actor_450800_801348E8[80] = {
#include "assets/actor_450800_animation_02C30_records.inc"
};

u16 D_actor_450800_80134A28[20] = {
#include "assets/actor_450800_animation_02C30_indices.inc"
};

AnimationSet D_actor_450800_80134A50 = {
    D_actor_450800_801348E8,
    D_actor_450800_80134A28,
    { NULL, D_actor_450800_8013484C, NULL, NULL, D_actor_450800_80134864, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80134A78[2] = {
#include "assets/actor_450800_animation_02DE4_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80134A90[26] = {
#include "assets/actor_450800_animation_02DE4_bank4.inc"
};

AnimationRecord D_actor_450800_80134AF8[57] = {
#include "assets/actor_450800_animation_02DE4_records.inc"
};

u16 D_actor_450800_80134BDC[20] = {
#include "assets/actor_450800_animation_02DE4_indices.inc"
};

AnimationSet D_actor_450800_80134C04 = {
    D_actor_450800_80134AF8,
    D_actor_450800_80134BDC,
    { NULL, D_actor_450800_80134A78, NULL, NULL, D_actor_450800_80134A90, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80134C2C[8] = {
#include "assets/actor_450800_animation_03348_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80134C8C[97] = {
#include "assets/actor_450800_animation_03348_bank4.inc"
};

AnimationRecord D_actor_450800_80134E10[204] = {
#include "assets/actor_450800_animation_03348_records.inc"
};

u16 D_actor_450800_80135140[20] = {
#include "assets/actor_450800_animation_03348_indices.inc"
};

AnimationSet D_actor_450800_80135168 = {
    D_actor_450800_80134E10,
    D_actor_450800_80135140,
    { NULL, D_actor_450800_80134C2C, NULL, NULL, D_actor_450800_80134C8C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80135190[3] = {
#include "assets/actor_450800_animation_0359C_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801351B4[30] = {
#include "assets/actor_450800_animation_0359C_bank4.inc"
};

AnimationRecord D_actor_450800_8013522C[90] = {
#include "assets/actor_450800_animation_0359C_records.inc"
};

u16 D_actor_450800_80135394[20] = {
#include "assets/actor_450800_animation_0359C_indices.inc"
};

AnimationSet D_actor_450800_801353BC = {
    D_actor_450800_8013522C,
    D_actor_450800_80135394,
    { NULL, D_actor_450800_80135190, NULL, NULL, D_actor_450800_801351B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_801353E4[3] = {
#include "assets/actor_450800_animation_03954_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80135408[81] = {
#include "assets/actor_450800_animation_03954_bank4.inc"
};

AnimationRecord D_actor_450800_8013554C[128] = {
#include "assets/actor_450800_animation_03954_records.inc"
};

u16 D_actor_450800_8013574C[20] = {
#include "assets/actor_450800_animation_03954_indices.inc"
};

AnimationSet D_actor_450800_80135774 = {
    D_actor_450800_8013554C,
    D_actor_450800_8013574C,
    { NULL, D_actor_450800_801353E4, NULL, NULL, D_actor_450800_80135408, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_8013579C[5] = {
#include "assets/actor_450800_animation_03D3C_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801357D8[67] = {
#include "assets/actor_450800_animation_03D3C_bank4.inc"
};

AnimationRecord D_actor_450800_801358E4[148] = {
#include "assets/actor_450800_animation_03D3C_records.inc"
};

u16 D_actor_450800_80135B34[20] = {
#include "assets/actor_450800_animation_03D3C_indices.inc"
};

AnimationSet D_actor_450800_80135B5C = {
    D_actor_450800_801358E4,
    D_actor_450800_80135B34,
    { NULL, D_actor_450800_8013579C, NULL, NULL, D_actor_450800_801357D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80135B84[3] = {
#include "assets/actor_450800_animation_04078_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80135BA8[53] = {
#include "assets/actor_450800_animation_04078_bank4.inc"
};

AnimationRecord D_actor_450800_80135C7C[125] = {
#include "assets/actor_450800_animation_04078_records.inc"
};

u16 D_actor_450800_80135E70[20] = {
#include "assets/actor_450800_animation_04078_indices.inc"
};

AnimationSet D_actor_450800_80135E98 = {
    D_actor_450800_80135C7C,
    D_actor_450800_80135E70,
    { NULL, D_actor_450800_80135B84, NULL, NULL, D_actor_450800_80135BA8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80135EC0[2] = {
#include "assets/actor_450800_animation_04240_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80135ED8[23] = {
#include "assets/actor_450800_animation_04240_bank4.inc"
};

AnimationRecord D_actor_450800_80135F34[65] = {
#include "assets/actor_450800_animation_04240_records.inc"
};

u16 D_actor_450800_80136038[20] = {
#include "assets/actor_450800_animation_04240_indices.inc"
};

AnimationSet D_actor_450800_80136060 = {
    D_actor_450800_80135F34,
    D_actor_450800_80136038,
    { NULL, D_actor_450800_80135EC0, NULL, NULL, D_actor_450800_80135ED8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80136088[6] = {
#include "assets/actor_450800_animation_04680_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801360D0[89] = {
#include "assets/actor_450800_animation_04680_bank4.inc"
};

AnimationRecord D_actor_450800_80136234[145] = {
#include "assets/actor_450800_animation_04680_records.inc"
};

u16 D_actor_450800_80136478[20] = {
#include "assets/actor_450800_animation_04680_indices.inc"
};

AnimationSet D_actor_450800_801364A0 = {
    D_actor_450800_80136234,
    D_actor_450800_80136478,
    { NULL, D_actor_450800_80136088, NULL, NULL, D_actor_450800_801360D0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_801364C8[2] = {
#include "assets/actor_450800_animation_048B8_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801364E0[26] = {
#include "assets/actor_450800_animation_048B8_bank4.inc"
};

AnimationRecord D_actor_450800_80136548[90] = {
#include "assets/actor_450800_animation_048B8_records.inc"
};

u16 D_actor_450800_801366B0[20] = {
#include "assets/actor_450800_animation_048B8_indices.inc"
};

AnimationSet D_actor_450800_801366D8 = {
    D_actor_450800_80136548,
    D_actor_450800_801366B0,
    { NULL, D_actor_450800_801364C8, NULL, NULL, D_actor_450800_801364E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80136700[2] = {
#include "assets/actor_450800_animation_04C24_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80136718[81] = {
#include "assets/actor_450800_animation_04C24_bank4.inc"
};

AnimationRecord D_actor_450800_8013685C[112] = {
#include "assets/actor_450800_animation_04C24_records.inc"
};

u16 D_actor_450800_80136A1C[20] = {
#include "assets/actor_450800_animation_04C24_indices.inc"
};

AnimationSet D_actor_450800_80136A44 = {
    D_actor_450800_8013685C,
    D_actor_450800_80136A1C,
    { NULL, D_actor_450800_80136700, NULL, NULL, D_actor_450800_80136718, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80136A6C[2] = {
#include "assets/actor_450800_animation_04E48_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80136A84[23] = {
#include "assets/actor_450800_animation_04E48_bank4.inc"
};

AnimationRecord D_actor_450800_80136AE0[88] = {
#include "assets/actor_450800_animation_04E48_records.inc"
};

u16 D_actor_450800_80136C40[20] = {
#include "assets/actor_450800_animation_04E48_indices.inc"
};

AnimationSet D_actor_450800_80136C68 = {
    D_actor_450800_80136AE0,
    D_actor_450800_80136C40,
    { NULL, D_actor_450800_80136A6C, NULL, NULL, D_actor_450800_80136A84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80136C90[2] = {
#include "assets/actor_450800_animation_04FD8_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80136CA8[17] = {
#include "assets/actor_450800_animation_04FD8_bank4.inc"
};

AnimationRecord D_actor_450800_80136CEC[57] = {
#include "assets/actor_450800_animation_04FD8_records.inc"
};

u16 D_actor_450800_80136DD0[20] = {
#include "assets/actor_450800_animation_04FD8_indices.inc"
};

AnimationSet D_actor_450800_80136DF8 = {
    D_actor_450800_80136CEC,
    D_actor_450800_80136DD0,
    { NULL, D_actor_450800_80136C90, NULL, NULL, D_actor_450800_80136CA8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80136E20[24] = {
#include "assets/actor_450800_animation_05CB4_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80136F40[302] = {
#include "assets/actor_450800_animation_05CB4_bank4.inc"
};

AnimationRecord D_actor_450800_801373F8[429] = {
#include "assets/actor_450800_animation_05CB4_records.inc"
};

u16 D_actor_450800_80137AAC[20] = {
#include "assets/actor_450800_animation_05CB4_indices.inc"
};

AnimationSet D_actor_450800_80137AD4 = {
    D_actor_450800_801373F8,
    D_actor_450800_80137AAC,
    { NULL, D_actor_450800_80136E20, NULL, NULL, D_actor_450800_80136F40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80137AFC[3] = {
#include "assets/actor_450800_animation_05EDC_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80137B20[24] = {
#include "assets/actor_450800_animation_05EDC_bank4.inc"
};

AnimationRecord D_actor_450800_80137B80[85] = {
#include "assets/actor_450800_animation_05EDC_records.inc"
};

u16 D_actor_450800_80137CD4[20] = {
#include "assets/actor_450800_animation_05EDC_indices.inc"
};

AnimationSet D_actor_450800_80137CFC = {
    D_actor_450800_80137B80,
    D_actor_450800_80137CD4,
    { NULL, D_actor_450800_80137AFC, NULL, NULL, D_actor_450800_80137B20, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80137D24[4] = {
#include "assets/actor_450800_animation_061D0_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80137D54[35] = {
#include "assets/actor_450800_animation_061D0_bank4.inc"
};

AnimationRecord D_actor_450800_80137DE0[122] = {
#include "assets/actor_450800_animation_061D0_records.inc"
};

u16 D_actor_450800_80137FC8[20] = {
#include "assets/actor_450800_animation_061D0_indices.inc"
};

AnimationSet D_actor_450800_80137FF0 = {
    D_actor_450800_80137DE0,
    D_actor_450800_80137FC8,
    { NULL, D_actor_450800_80137D24, NULL, NULL, D_actor_450800_80137D54, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80138018[2] = {
#include "assets/actor_450800_animation_063E8_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80138030[31] = {
#include "assets/actor_450800_animation_063E8_bank4.inc"
};

AnimationRecord D_actor_450800_801380AC[77] = {
#include "assets/actor_450800_animation_063E8_records.inc"
};

u16 D_actor_450800_801381E0[20] = {
#include "assets/actor_450800_animation_063E8_indices.inc"
};

AnimationSet D_actor_450800_80138208 = {
    D_actor_450800_801380AC,
    D_actor_450800_801381E0,
    { NULL, D_actor_450800_80138018, NULL, NULL, D_actor_450800_80138030, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80138230[5] = {
#include "assets/actor_450800_animation_0690C_bank1.inc"
};

AnimationPackedRotation D_actor_450800_8013826C[101] = {
#include "assets/actor_450800_animation_0690C_bank4.inc"
};

AnimationRecord D_actor_450800_80138400[193] = {
#include "assets/actor_450800_animation_0690C_records.inc"
};

u16 D_actor_450800_80138704[20] = {
#include "assets/actor_450800_animation_0690C_indices.inc"
};

AnimationSet D_actor_450800_8013872C = {
    D_actor_450800_80138400,
    D_actor_450800_80138704,
    { NULL, D_actor_450800_80138230, NULL, NULL, D_actor_450800_8013826C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80138754[4] = {
#include "assets/actor_450800_animation_06C64_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80138784[41] = {
#include "assets/actor_450800_animation_06C64_bank4.inc"
};

AnimationRecord D_actor_450800_80138828[141] = {
#include "assets/actor_450800_animation_06C64_records.inc"
};

u16 D_actor_450800_80138A5C[20] = {
#include "assets/actor_450800_animation_06C64_indices.inc"
};

AnimationSet D_actor_450800_80138A84 = {
    D_actor_450800_80138828,
    D_actor_450800_80138A5C,
    { NULL, D_actor_450800_80138754, NULL, NULL, D_actor_450800_80138784, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80138AAC[8] = {
#include "assets/actor_450800_animation_070F8_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80138B0C[90] = {
#include "assets/actor_450800_animation_070F8_bank4.inc"
};

AnimationRecord D_actor_450800_80138C74[159] = {
#include "assets/actor_450800_animation_070F8_records.inc"
};

u16 D_actor_450800_80138EF0[20] = {
#include "assets/actor_450800_animation_070F8_indices.inc"
};

AnimationSet D_actor_450800_80138F18 = {
    D_actor_450800_80138C74,
    D_actor_450800_80138EF0,
    { NULL, D_actor_450800_80138AAC, NULL, NULL, D_actor_450800_80138B0C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80138F40[6] = {
#include "assets/actor_450800_animation_074C4_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80138F88[64] = {
#include "assets/actor_450800_animation_074C4_bank4.inc"
};

AnimationRecord D_actor_450800_80139088[141] = {
#include "assets/actor_450800_animation_074C4_records.inc"
};

u16 D_actor_450800_801392BC[20] = {
#include "assets/actor_450800_animation_074C4_indices.inc"
};

AnimationSet D_actor_450800_801392E4 = {
    D_actor_450800_80139088,
    D_actor_450800_801392BC,
    { NULL, D_actor_450800_80138F40, NULL, NULL, D_actor_450800_80138F88, NULL, NULL, NULL },
};

s32 D_actor_450800_8013930C = 0;

Actor450800AnimCopy9310 D_actor_450800_80139310 = { .data = { { &D_actor_450800_80133940, &D_actor_450800_80133C1C, &D_actor_450800_80133E90, &D_actor_450800_801340F8, &D_actor_450800_80134358, &D_actor_450800_80134660, &D_actor_450800_80134824, &D_actor_450800_80134A50, &D_actor_450800_80134C04, &D_actor_450800_80135168, &D_actor_450800_801353BC, &D_actor_450800_80135774, &D_actor_450800_80135B5C, &D_actor_450800_80135E98, &D_actor_450800_80136060, &D_actor_450800_801364A0, &D_actor_450800_801392E4 }, { { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 48, 0, 0, 0 } } } };

AnimationPlayRequest D_actor_450800_80139390 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801393A4 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801393B8 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801393CC = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801393E0 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801393F4 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139408 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013941C = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139430 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139444 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139458 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013946C = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139480 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139494 = { { .index = 1 }, 62, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801394A8 = { { .index = 1 }, 63, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

Actor450800AnimCopy94BC D_actor_450800_801394BC = { .data = { { &D_actor_450800_801366D8, &D_actor_450800_80136A44, &D_actor_450800_80136C68, &D_actor_450800_80136DF8, &D_actor_450800_80137AD4, &D_actor_450800_80137CFC, &D_actor_450800_80137FF0, &D_actor_450800_80138208, &D_actor_450800_8013872C, &D_actor_450800_80138A84, &D_actor_450800_80138F18 }, { { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 48, 0, 0, 0 }, { { .index = 1 }, 49, 0, 0, 0 }, { { .index = 1 }, 50, 0, 0, 0 } } } };

AnimationPlayRequest D_actor_450800_8013954C = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139560 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139574 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139588 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013959C[2] = {
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_450800_801395C4 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801395D8 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801395EC[3] = {
    { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_450800_80139628 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013963C = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139650 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139664 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139678 = { { .index = 1 }, 5, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013968C = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801396A0 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801396B4 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801396C8 = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801396DC = { { .index = 1 }, 10, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801396F0 = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139704 = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139718 = { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013972C = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139740 = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139754 = { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139768 = { { .index = 1 }, 17, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013977C = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139790 = { { .index = 1 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801397A4 = { { .index = 1 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801397B8 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801397CC = { { .index = 1 }, 21, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801397E0 = { { .index = 1 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801397F4 = { { .index = 1 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139808 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013981C = { { .index = 1 }, 25, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139830 = { { .index = 1 }, 26, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139844[2] = {
    { { .index = 1 }, 27, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 28, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_450800_8013986C = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139880 = { { .index = 1 }, 58, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139894 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801398A8 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801398BC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpCopyArg D_actor_450800_801398D0 = { { .words = D_actor_450800_801394BC.words }, 32 };

GpCopyArg D_actor_450800_801398D8 = { { .words = D_actor_450800_80139310.words }, 32 };

ActorCommand D_actor_450800_801398E0 = { { .loc = { 5, 22 } }, 0 };

ActorCommand D_actor_450800_801398E4 = { { .loc = { 5, 22 } }, 1 };

ActorCommand D_actor_450800_801398E8 = { { .loc = { 5, 22 } }, 2 };

ActorTransform D_actor_450800_801398EC = { { 5650, 0, 2400, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450800_80139904 = { { 6080, 0, 900, 0 }, { 0, 853, 0, 0 } };

ActorTransform D_actor_450800_8013991C = { { 5951, 0, 900, 0 }, { 0, 853, 0, 0 } };

ActorTransform D_actor_450800_80139934 = { { 6770, 0, 900, 0 }, { 0, -1137, 0, 0 } };

ActorTransform D_actor_450800_8013994C = { { 6650, 0, 401, 0 }, { 0, -1024, 0, 0 } };

GpEvsCmd D_actor_450800_80139964[105] = {
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 47, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450800_801398D8 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450800_801398D0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139628 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E8 } }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139894 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398BC }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_80139904 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_80139934 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139650 }, { .value = 0 } },
    { 4, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139678 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139768 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139310.data.arguments[1] }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394BC.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139310.data.arguments[2] }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139390 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801393A4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_8013977C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801393CC }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394BC.data.arguments[2] }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394BC.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139664 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_8013972C }, { .value = 0 } },
    { 4, { .value = 61 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139628 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139740 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139650 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_8013968C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801396A0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801393E0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801393F4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU32 = func_actor_450800_80131F70 }, { .value = 0x10000 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b6_nursery_8017FFF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139408 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139588 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394BC.data.arguments[4] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139754 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_8013941C }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_8013954C }, { .value = 0 } },
    { 4, { .value = 78 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55160006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013991C }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013994C }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139560 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139894 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 20, { .value = 25 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450800_8013A33C[23] = {
    { 16, { .value = 0x55160006 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU32 = func_actor_450800_80131F70 }, { .value = 0x10000 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013994C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013991C }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 20, { .value = 25 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b6_nursery_8017FFF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139560 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139894 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139628 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450800_8013A564[12] = {
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450800_801398D0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139574 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_8013986C }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450800_8013A684[10] = {
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F98 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801396C8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801396DC }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450800_8013A774[9] = {
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F98 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139790 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450800_8013A84C[6] = {
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450800_801398D0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013994C }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139560 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139628 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450800_8013A8DC[7] = {
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450800_801398D0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013994C }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139560 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801397A4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_450800_801398EC }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450800_8013A984[21] = {
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450800_801398D8 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801396F0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139704 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139880 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801396F0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139704 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139430 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139718 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801397A4 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450800_8013AB7C[16] = {
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450800_801398D8 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801396F0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139704 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139880 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801396F0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139704 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450800_8013ACFC[10] = {
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F98 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801396F0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139704 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

AnimationPlayRequest D_actor_450800_8013ADEC = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013AE00 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013AE14 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpOverrideArg D_actor_450800_8013AE28 = { 19, 1 };

ActorTransform D_actor_450800_8013AE30 = { { 6280, 0, 960, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450800_8013AE48 = { { 6400, 0, 960, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450800_8013AE60 = { { 5350, 0, -20, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450800_8013AE78 = { { 5320, 0, 2900, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_450800_8013AE90 = { { 5320, 0, 1180, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_450800_8013AEA8 = { { 5320, 0, 540, 0 }, { 0, -1877, 0, 0 } };

ActorTransform D_actor_450800_8013AEC0 = { { 5280, 0, -290, 0 }, { 0, 455, 0, 0 } };

ActorTransform D_actor_450800_8013AED8 = { { 6500, 0, 1000, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450800_8013AEF0 = { { 5710, 0, -290, 0 }, { 0, -455, 0, 0 } };

ActorTransform D_actor_450800_8013AF08 = { { 7000, 0, 1200, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450800_8013AF20 = { { -1200, 0, -220, 0 }, { 0, 807, 0, 0 } };

ActorTransform D_actor_450800_8013AF38 = { { 800, 0, 450, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450800_8013AF50 = { { 0, 0, 0, 0 }, { 0, 853, 0, 0 } };

GpOverlayIds D_actor_450800_8013AF68 = { 5, 9, 11 };

GpScriptCmd D_actor_450800_8013AF70[5] = {
    { 257, 1 },
    { 258, 0 },
    { 771, 0 },
    { 4, 0 },
    { 0, 0 },
};

GpScriptRec D_actor_450800_8013AF84[2] = {
    { 210, 63, 9, 1 },
    { 0, 0, 2, 0 },
};

GpEvsCmd D_actor_450800_8013AF8C[117] = {
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_actor_450800_8013AF68 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450800_801398D8 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450800_801398D0 }, { .value = 0 } },
    { 15, { .value = 0x55160007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 150 }, { .value = 40 }, { .value = 40 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E4 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E4 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013AE90 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013AEA8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139458 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_8013946C }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 1 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139830 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_450800_8013AE30 }, { .value = 0 } },
    { 14, { .padCommands = D_actor_450800_8013AF70 }, { .padRecords = D_actor_450800_8013AF84 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_8013981C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E0 } }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_450800_80132108 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_801320E8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 14, { .padCommands = D_actor_450800_8013AF70 }, { .padRecords = D_actor_450800_8013AF84 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_8013981C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E0 } }, { .value = 0 } },
    { 14, { .padCommands = D_actor_450800_8013AF70 }, { .padRecords = D_actor_450800_8013AF84 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_8013981C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E0 } }, { .value = 0 } },
    { 14, { .padCommands = D_actor_450800_8013AF70 }, { .padRecords = D_actor_450800_8013AF84 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801397CC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013AEC0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013AEF0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139480 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801395C4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_801397E0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_450800_8013AE48 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 32, { .value = 50 }, { .value = 10 }, { .value = 10 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_450800_8013AE14 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_actor_450800_8013AE14 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_450800_8013AF20 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_450800_8013ADEC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2013 }, { .storage = &D_actor_450800_8013AF38 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2004 }, { .storage = &D_actor_450800_8013AF50 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_actor_450800_8013AE00 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 150 }, { .value = 40 }, { .value = 40 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013AF08 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013AED8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139494 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801395D8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2003 }, { .storage = &D_actor_450800_801397F4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2004 }, { .storage = &D_actor_450800_8013AE60 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2003 }, { .storage = &D_actor_450800_80139808 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_450800_80132080 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 37, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_450800_8013BA84[20] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_450800_80132080 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450800_8013994C }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139560 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139894 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_450800_80139628 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_450800_8013BC64[20] = {
#include "assets/actor_450800_model_0E7E4_skeleton.inc"
};

u32 D_actor_450800_8013BF34[20] = {
#include "assets/actor_450800_model_0E7E4_partVerts.inc"
};

SVECTOR D_actor_450800_8013BF84[287] = {
#include "assets/actor_450800_model_0E7E4_verts.inc"
};

SVECTOR D_actor_450800_8013C87C[285] = {
#include "assets/actor_450800_model_0E7E4_normals.inc"
};

u32 D_actor_450800_8013D164[3368] = {
#include "assets/actor_450800_model_0E7E4_stream.inc"
};

TmdSource D_actor_450800_80140604 = {
    0,
    17772,
    5724,
    20,
    D_actor_450800_8013BF34,
    D_actor_450800_8013BF84,
    D_actor_450800_8013C87C,
    D_actor_450800_8013BC64,
    D_actor_450800_8013D164,
};

TmdBone D_actor_450800_80140628[20] = {
#include "assets/actor_450800_model_13328_skeleton.inc"
};

u32 D_actor_450800_801408F8[20] = {
#include "assets/actor_450800_model_13328_partVerts.inc"
};

SVECTOR D_actor_450800_80140948[300] = {
#include "assets/actor_450800_model_13328_verts.inc"
};

SVECTOR D_actor_450800_801412A8[298] = {
#include "assets/actor_450800_model_13328_normals.inc"
};

u32 D_actor_450800_80141BF8[3412] = {
#include "assets/actor_450800_model_13328_stream.inc"
};

TmdSource D_actor_450800_80145148 = {
    0,
    18224,
    5696,
    20,
    D_actor_450800_801408F8,
    D_actor_450800_80140948,
    D_actor_450800_801412A8,
    D_actor_450800_80140628,
    D_actor_450800_80141BF8,
};

AnimationPackedPose D_actor_450800_8014516C[2] = {
#include "assets/actor_450800_animation_13564_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80145184[25] = {
#include "assets/actor_450800_animation_13564_bank4.inc"
};

AnimationRecord D_actor_450800_801451E8[93] = {
#include "assets/actor_450800_animation_13564_records.inc"
};

u16 D_actor_450800_8014535C[20] = {
#include "assets/actor_450800_animation_13564_indices.inc"
};

AnimationSet D_actor_450800_80145384 = {
    D_actor_450800_801451E8,
    D_actor_450800_8014535C,
    { NULL, D_actor_450800_8014516C, NULL, NULL, D_actor_450800_80145184, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_801453AC[2] = {
#include "assets/actor_450800_animation_13870_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801453C4[53] = {
#include "assets/actor_450800_animation_13870_bank4.inc"
};

AnimationRecord D_actor_450800_80145498[116] = {
#include "assets/actor_450800_animation_13870_records.inc"
};

u16 D_actor_450800_80145668[20] = {
#include "assets/actor_450800_animation_13870_indices.inc"
};

AnimationSet D_actor_450800_80145690 = {
    D_actor_450800_80145498,
    D_actor_450800_80145668,
    { NULL, D_actor_450800_801453AC, NULL, NULL, D_actor_450800_801453C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_801456B8[2] = {
#include "assets/actor_450800_animation_13B28_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801456D0[37] = {
#include "assets/actor_450800_animation_13B28_bank4.inc"
};

AnimationRecord D_actor_450800_80145764[111] = {
#include "assets/actor_450800_animation_13B28_records.inc"
};

u16 D_actor_450800_80145920[20] = {
#include "assets/actor_450800_animation_13B28_indices.inc"
};

AnimationSet D_actor_450800_80145948 = {
    D_actor_450800_80145764,
    D_actor_450800_80145920,
    { NULL, D_actor_450800_801456B8, NULL, NULL, D_actor_450800_801456D0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80145970[2] = {
#include "assets/actor_450800_animation_13E14_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80145988[42] = {
#include "assets/actor_450800_animation_13E14_bank4.inc"
};

AnimationRecord D_actor_450800_80145A30[119] = {
#include "assets/actor_450800_animation_13E14_records.inc"
};

u16 D_actor_450800_80145C0C[20] = {
#include "assets/actor_450800_animation_13E14_indices.inc"
};

AnimationSet D_actor_450800_80145C34 = {
    D_actor_450800_80145A30,
    D_actor_450800_80145C0C,
    { NULL, D_actor_450800_80145970, NULL, NULL, D_actor_450800_80145988, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80145C5C[2] = {
#include "assets/actor_450800_animation_14030_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80145C74[38] = {
#include "assets/actor_450800_animation_14030_bank4.inc"
};

AnimationRecord D_actor_450800_80145D0C[71] = {
#include "assets/actor_450800_animation_14030_records.inc"
};

u16 D_actor_450800_80145E28[20] = {
#include "assets/actor_450800_animation_14030_indices.inc"
};

AnimationSet D_actor_450800_80145E50 = {
    D_actor_450800_80145D0C,
    D_actor_450800_80145E28,
    { NULL, D_actor_450800_80145C5C, NULL, NULL, D_actor_450800_80145C74, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80145E78[2] = {
#include "assets/actor_450800_animation_14304_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80145E90[42] = {
#include "assets/actor_450800_animation_14304_bank4.inc"
};

AnimationRecord D_actor_450800_80145F38[113] = {
#include "assets/actor_450800_animation_14304_records.inc"
};

u16 D_actor_450800_801460FC[20] = {
#include "assets/actor_450800_animation_14304_indices.inc"
};

AnimationSet D_actor_450800_80146124 = {
    D_actor_450800_80145F38,
    D_actor_450800_801460FC,
    { NULL, D_actor_450800_80145E78, NULL, NULL, D_actor_450800_80145E90, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_8014614C[2] = {
#include "assets/actor_450800_animation_14544_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80146164[25] = {
#include "assets/actor_450800_animation_14544_bank4.inc"
};

AnimationRecord D_actor_450800_801461C8[93] = {
#include "assets/actor_450800_animation_14544_records.inc"
};

u16 D_actor_450800_8014633C[20] = {
#include "assets/actor_450800_animation_14544_indices.inc"
};

AnimationSet D_actor_450800_80146364 = {
    D_actor_450800_801461C8,
    D_actor_450800_8014633C,
    { NULL, D_actor_450800_8014614C, NULL, NULL, D_actor_450800_80146164, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_8014638C[2] = {
#include "assets/actor_450800_animation_1478C_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801463A4[26] = {
#include "assets/actor_450800_animation_1478C_bank4.inc"
};

AnimationRecord D_actor_450800_8014640C[94] = {
#include "assets/actor_450800_animation_1478C_records.inc"
};

u16 D_actor_450800_80146584[20] = {
#include "assets/actor_450800_animation_1478C_indices.inc"
};

AnimationSet D_actor_450800_801465AC = {
    D_actor_450800_8014640C,
    D_actor_450800_80146584,
    { NULL, D_actor_450800_8014638C, NULL, NULL, D_actor_450800_801463A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_801465D4[2] = {
#include "assets/actor_450800_animation_14A70_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801465EC[42] = {
#include "assets/actor_450800_animation_14A70_bank4.inc"
};

AnimationRecord D_actor_450800_80146694[117] = {
#include "assets/actor_450800_animation_14A70_records.inc"
};

u16 D_actor_450800_80146868[20] = {
#include "assets/actor_450800_animation_14A70_indices.inc"
};

AnimationSet D_actor_450800_80146890 = {
    D_actor_450800_80146694,
    D_actor_450800_80146868,
    { NULL, D_actor_450800_801465D4, NULL, NULL, D_actor_450800_801465EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_801468B8[2] = {
#include "assets/actor_450800_animation_14C98_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801468D0[39] = {
#include "assets/actor_450800_animation_14C98_bank4.inc"
};

AnimationRecord D_actor_450800_8014696C[73] = {
#include "assets/actor_450800_animation_14C98_records.inc"
};

u16 D_actor_450800_80146A90[20] = {
#include "assets/actor_450800_animation_14C98_indices.inc"
};

AnimationSet D_actor_450800_80146AB8 = {
    D_actor_450800_8014696C,
    D_actor_450800_80146A90,
    { NULL, D_actor_450800_801468B8, NULL, NULL, D_actor_450800_801468D0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80146AE0[2] = {
#include "assets/actor_450800_animation_14F8C_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80146AF8[43] = {
#include "assets/actor_450800_animation_14F8C_bank4.inc"
};

AnimationRecord D_actor_450800_80146BA4[120] = {
#include "assets/actor_450800_animation_14F8C_records.inc"
};

u16 D_actor_450800_80146D84[20] = {
#include "assets/actor_450800_animation_14F8C_indices.inc"
};

AnimationSet D_actor_450800_80146DAC = {
    D_actor_450800_80146BA4,
    D_actor_450800_80146D84,
    { NULL, D_actor_450800_80146AE0, NULL, NULL, D_actor_450800_80146AF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80146DD4[2] = {
#include "assets/actor_450800_animation_1517C_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80146DEC[30] = {
#include "assets/actor_450800_animation_1517C_bank4.inc"
};

AnimationRecord D_actor_450800_80146E64[68] = {
#include "assets/actor_450800_animation_1517C_records.inc"
};

u16 D_actor_450800_80146F74[20] = {
#include "assets/actor_450800_animation_1517C_indices.inc"
};

AnimationSet D_actor_450800_80146F9C = {
    D_actor_450800_80146E64,
    D_actor_450800_80146F74,
    { NULL, D_actor_450800_80146DD4, NULL, NULL, D_actor_450800_80146DEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80146FC4[2] = {
#include "assets/actor_450800_animation_15494_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80146FDC[56] = {
#include "assets/actor_450800_animation_15494_bank4.inc"
};

AnimationRecord D_actor_450800_801470BC[116] = {
#include "assets/actor_450800_animation_15494_records.inc"
};

u16 D_actor_450800_8014728C[20] = {
#include "assets/actor_450800_animation_15494_indices.inc"
};

AnimationSet D_actor_450800_801472B4 = {
    D_actor_450800_801470BC,
    D_actor_450800_8014728C,
    { NULL, D_actor_450800_80146FC4, NULL, NULL, D_actor_450800_80146FDC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_801472DC[2] = {
#include "assets/actor_450800_animation_157F0_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801472F4[70] = {
#include "assets/actor_450800_animation_157F0_bank4.inc"
};

AnimationRecord D_actor_450800_8014740C[119] = {
#include "assets/actor_450800_animation_157F0_records.inc"
};

u16 D_actor_450800_801475E8[20] = {
#include "assets/actor_450800_animation_157F0_indices.inc"
};

AnimationSet D_actor_450800_80147610 = {
    D_actor_450800_8014740C,
    D_actor_450800_801475E8,
    { NULL, D_actor_450800_801472DC, NULL, NULL, D_actor_450800_801472F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80147638[2] = {
#include "assets/actor_450800_animation_15A78_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80147650[44] = {
#include "assets/actor_450800_animation_15A78_bank4.inc"
};

AnimationRecord D_actor_450800_80147700[92] = {
#include "assets/actor_450800_animation_15A78_records.inc"
};

u16 D_actor_450800_80147870[20] = {
#include "assets/actor_450800_animation_15A78_indices.inc"
};

AnimationSet D_actor_450800_80147898 = {
    D_actor_450800_80147700,
    D_actor_450800_80147870,
    { NULL, D_actor_450800_80147638, NULL, NULL, D_actor_450800_80147650, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_801478C0[2] = {
#include "assets/actor_450800_animation_15C18_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801478D8[18] = {
#include "assets/actor_450800_animation_15C18_bank4.inc"
};

AnimationRecord D_actor_450800_80147920[60] = {
#include "assets/actor_450800_animation_15C18_records.inc"
};

u16 D_actor_450800_80147A10[20] = {
#include "assets/actor_450800_animation_15C18_indices.inc"
};

AnimationSet D_actor_450800_80147A38 = {
    D_actor_450800_80147920,
    D_actor_450800_80147A10,
    { NULL, D_actor_450800_801478C0, NULL, NULL, D_actor_450800_801478D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80147A60[2] = {
#include "assets/actor_450800_animation_15E00_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80147A78[24] = {
#include "assets/actor_450800_animation_15E00_bank4.inc"
};

AnimationRecord D_actor_450800_80147AD8[72] = {
#include "assets/actor_450800_animation_15E00_records.inc"
};

u16 D_actor_450800_80147BF8[20] = {
#include "assets/actor_450800_animation_15E00_indices.inc"
};

AnimationSet D_actor_450800_80147C20 = {
    D_actor_450800_80147AD8,
    D_actor_450800_80147BF8,
    { NULL, D_actor_450800_80147A60, NULL, NULL, D_actor_450800_80147A78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80147C48[2] = {
#include "assets/actor_450800_animation_15FDC_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80147C60[23] = {
#include "assets/actor_450800_animation_15FDC_bank4.inc"
};

AnimationRecord D_actor_450800_80147CBC[70] = {
#include "assets/actor_450800_animation_15FDC_records.inc"
};

u16 D_actor_450800_80147DD4[20] = {
#include "assets/actor_450800_animation_15FDC_indices.inc"
};

AnimationSet D_actor_450800_80147DFC = {
    D_actor_450800_80147CBC,
    D_actor_450800_80147DD4,
    { NULL, D_actor_450800_80147C48, NULL, NULL, D_actor_450800_80147C60, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80147E24[2] = {
#include "assets/actor_450800_animation_16320_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80147E3C[55] = {
#include "assets/actor_450800_animation_16320_bank4.inc"
};

AnimationRecord D_actor_450800_80147F18[128] = {
#include "assets/actor_450800_animation_16320_records.inc"
};

u16 D_actor_450800_80148118[20] = {
#include "assets/actor_450800_animation_16320_indices.inc"
};

AnimationSet D_actor_450800_80148140 = {
    D_actor_450800_80147F18,
    D_actor_450800_80148118,
    { NULL, D_actor_450800_80147E24, NULL, NULL, D_actor_450800_80147E3C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80148168[3] = {
#include "assets/actor_450800_animation_16658_bank1.inc"
};

AnimationPackedRotation D_actor_450800_8014818C[59] = {
#include "assets/actor_450800_animation_16658_bank4.inc"
};

AnimationRecord D_actor_450800_80148278[118] = {
#include "assets/actor_450800_animation_16658_records.inc"
};

u16 D_actor_450800_80148450[20] = {
#include "assets/actor_450800_animation_16658_indices.inc"
};

AnimationSet D_actor_450800_80148478 = {
    D_actor_450800_80148278,
    D_actor_450800_80148450,
    { NULL, D_actor_450800_80148168, NULL, NULL, D_actor_450800_8014818C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_801484A0[2] = {
#include "assets/actor_450800_animation_167F8_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801484B8[18] = {
#include "assets/actor_450800_animation_167F8_bank4.inc"
};

AnimationRecord D_actor_450800_80148500[60] = {
#include "assets/actor_450800_animation_167F8_records.inc"
};

u16 D_actor_450800_801485F0[20] = {
#include "assets/actor_450800_animation_167F8_indices.inc"
};

AnimationSet D_actor_450800_80148618 = {
    D_actor_450800_80148500,
    D_actor_450800_801485F0,
    { NULL, D_actor_450800_801484A0, NULL, NULL, D_actor_450800_801484B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80148640[2] = {
#include "assets/actor_450800_animation_169B8_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80148658[26] = {
#include "assets/actor_450800_animation_169B8_bank4.inc"
};

AnimationRecord D_actor_450800_801486C0[60] = {
#include "assets/actor_450800_animation_169B8_records.inc"
};

u16 D_actor_450800_801487B0[20] = {
#include "assets/actor_450800_animation_169B8_indices.inc"
};

AnimationSet D_actor_450800_801487D8 = {
    D_actor_450800_801486C0,
    D_actor_450800_801487B0,
    { NULL, D_actor_450800_80148640, NULL, NULL, D_actor_450800_80148658, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80148800[2] = {
#include "assets/actor_450800_animation_16C14_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80148818[42] = {
#include "assets/actor_450800_animation_16C14_bank4.inc"
};

AnimationRecord D_actor_450800_801488C0[83] = {
#include "assets/actor_450800_animation_16C14_records.inc"
};

u16 D_actor_450800_80148A0C[20] = {
#include "assets/actor_450800_animation_16C14_indices.inc"
};

AnimationSet D_actor_450800_80148A34 = {
    D_actor_450800_801488C0,
    D_actor_450800_80148A0C,
    { NULL, D_actor_450800_80148800, NULL, NULL, D_actor_450800_80148818, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80148A5C[3] = {
#include "assets/actor_450800_animation_16EA8_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80148A80[31] = {
#include "assets/actor_450800_animation_16EA8_bank4.inc"
};

AnimationRecord D_actor_450800_80148AFC[105] = {
#include "assets/actor_450800_animation_16EA8_records.inc"
};

u16 D_actor_450800_80148CA0[20] = {
#include "assets/actor_450800_animation_16EA8_indices.inc"
};

AnimationSet D_actor_450800_80148CC8 = {
    D_actor_450800_80148AFC,
    D_actor_450800_80148CA0,
    { NULL, D_actor_450800_80148A5C, NULL, NULL, D_actor_450800_80148A80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80148CF0[3] = {
#include "assets/actor_450800_animation_17080_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80148D14[23] = {
#include "assets/actor_450800_animation_17080_bank4.inc"
};

AnimationRecord D_actor_450800_80148D70[66] = {
#include "assets/actor_450800_animation_17080_records.inc"
};

u16 D_actor_450800_80148E78[20] = {
#include "assets/actor_450800_animation_17080_indices.inc"
};

AnimationSet D_actor_450800_80148EA0 = {
    D_actor_450800_80148D70,
    D_actor_450800_80148E78,
    { NULL, D_actor_450800_80148CF0, NULL, NULL, D_actor_450800_80148D14, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80148EC8[10] = {
#include "assets/actor_450800_animation_17488_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80148F40[85] = {
#include "assets/actor_450800_animation_17488_bank4.inc"
};

AnimationRecord D_actor_450800_80149094[123] = {
#include "assets/actor_450800_animation_17488_records.inc"
};

u16 D_actor_450800_80149280[20] = {
#include "assets/actor_450800_animation_17488_indices.inc"
};

AnimationSet D_actor_450800_801492A8 = {
    D_actor_450800_80149094,
    D_actor_450800_80149280,
    { NULL, D_actor_450800_80148EC8, NULL, NULL, D_actor_450800_80148F40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_801492D0[2] = {
#include "assets/actor_450800_animation_17700_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801492E8[50] = {
#include "assets/actor_450800_animation_17700_bank4.inc"
};

AnimationRecord D_actor_450800_801493B0[82] = {
#include "assets/actor_450800_animation_17700_records.inc"
};

u16 D_actor_450800_801494F8[20] = {
#include "assets/actor_450800_animation_17700_indices.inc"
};

AnimationSet D_actor_450800_80149520 = {
    D_actor_450800_801493B0,
    D_actor_450800_801494F8,
    { NULL, D_actor_450800_801492D0, NULL, NULL, D_actor_450800_801492E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80149548[2] = {
#include "assets/actor_450800_animation_178F4_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80149560[19] = {
#include "assets/actor_450800_animation_178F4_bank4.inc"
};

AnimationRecord D_actor_450800_801495AC[80] = {
#include "assets/actor_450800_animation_178F4_records.inc"
};

u16 D_actor_450800_801496EC[20] = {
#include "assets/actor_450800_animation_178F4_indices.inc"
};

AnimationSet D_actor_450800_80149714 = {
    D_actor_450800_801495AC,
    D_actor_450800_801496EC,
    { NULL, D_actor_450800_80149548, NULL, NULL, D_actor_450800_80149560, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_8014973C[8] = {
#include "assets/actor_450800_animation_17DD0_bank1.inc"
};

AnimationPackedRotation D_actor_450800_8014979C[114] = {
#include "assets/actor_450800_animation_17DD0_bank4.inc"
};

AnimationRecord D_actor_450800_80149964[153] = {
#include "assets/actor_450800_animation_17DD0_records.inc"
};

u16 D_actor_450800_80149BC8[20] = {
#include "assets/actor_450800_animation_17DD0_indices.inc"
};

AnimationSet D_actor_450800_80149BF0 = {
    D_actor_450800_80149964,
    D_actor_450800_80149BC8,
    { NULL, D_actor_450800_8014973C, NULL, NULL, D_actor_450800_8014979C, NULL, NULL, NULL },
};

TmdBone D_actor_450800_80149C18[1] = {
#include "assets/actor_450800_model_18228_skeleton.inc"
};

u32 D_actor_450800_80149C3C[1] = {
#include "assets/actor_450800_model_18228_partVerts.inc"
};

SVECTOR D_actor_450800_80149C40[23] = {
#include "assets/actor_450800_model_18228_verts.inc"
};

SVECTOR D_actor_450800_80149CF8[23] = {
#include "assets/actor_450800_model_18228_normals.inc"
};

u32 D_actor_450800_80149DB0[166] = {
#include "assets/actor_450800_model_18228_stream.inc"
};

TmdSource D_actor_450800_8014A048 = {
    0,
    1148,
    0,
    1,
    D_actor_450800_80149C3C,
    D_actor_450800_80149C40,
    D_actor_450800_80149CF8,
    D_actor_450800_80149C18,
    D_actor_450800_80149DB0,
};

TmdBone D_actor_450800_8014A06C[1] = {
#include "assets/actor_450800_model_18718_skeleton.inc"
};

u32 D_actor_450800_8014A090[1] = {
#include "assets/actor_450800_model_18718_partVerts.inc"
};

SVECTOR D_actor_450800_8014A094[27] = {
#include "assets/actor_450800_model_18718_verts.inc"
};

SVECTOR D_actor_450800_8014A16C[27] = {
#include "assets/actor_450800_model_18718_normals.inc"
};

u32 D_actor_450800_8014A244[189] = {
#include "assets/actor_450800_model_18718_stream.inc"
};

TmdSource D_actor_450800_8014A538 = {
    0,
    1328,
    0,
    1,
    D_actor_450800_8014A090,
    D_actor_450800_8014A094,
    D_actor_450800_8014A16C,
    D_actor_450800_8014A06C,
    D_actor_450800_8014A244,
};

AnimationPackedPose D_actor_450800_8014A55C[3] = {
#include "assets/actor_450800_animation_189CC_bank1.inc"
};

AnimationPackedRotation D_actor_450800_8014A580[29] = {
#include "assets/actor_450800_animation_189CC_bank4.inc"
};

AnimationRecord D_actor_450800_8014A5F4[116] = {
#include "assets/actor_450800_animation_189CC_records.inc"
};

u16 D_actor_450800_8014A7C4[20] = {
#include "assets/actor_450800_animation_189CC_indices.inc"
};

AnimationSet D_actor_450800_8014A7EC = {
    D_actor_450800_8014A5F4,
    D_actor_450800_8014A7C4,
    { NULL, D_actor_450800_8014A55C, NULL, NULL, D_actor_450800_8014A580, NULL, NULL, NULL },
};

TmdBone D_actor_450800_8014A814[1] = {
#include "assets/actor_450800_model_18E14_skeleton.inc"
};

u32 D_actor_450800_8014A838[1] = {
#include "assets/actor_450800_model_18E14_partVerts.inc"
};

SVECTOR D_actor_450800_8014A83C[22] = {
#include "assets/actor_450800_model_18E14_verts.inc"
};

SVECTOR D_actor_450800_8014A8EC[24] = {
#include "assets/actor_450800_model_18E14_normals.inc"
};

u32 D_actor_450800_8014A9AC[162] = {
#include "assets/actor_450800_model_18E14_stream.inc"
};

TmdSource D_actor_450800_8014AC34 = {
    0,
    1108,
    0,
    1,
    D_actor_450800_8014A838,
    D_actor_450800_8014A83C,
    D_actor_450800_8014A8EC,
    D_actor_450800_8014A814,
    D_actor_450800_8014A9AC,
};

Actor450800MsgEntry D_actor_450800_8014AC58[6] = {
    { 2003, { .call1 = func_actor_450800_80132B44 } },
    { 2005, { .call6 = func_actor_450800_80132BB0 } },
    { 2004, { .call3 = func_actor_450800_80132C68 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call2 = func_actor_450800_80132CE0 } },
    { 2013, { .call5 = func_actor_450800_80132D74 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_450800_8014AC88[5] = {
    { TASK_BODY_TMD, 192, func_actor_450800_80132790, { .model = &D_actor_450800_80145148 } },
    { TASK_BODY_TMD, 192, func_actor_450800_80132958, { .model = &D_actor_450800_8014A538 } },
    { TASK_BODY_TMD, 192, func_actor_450800_80132958, { .model = &D_actor_450800_8014A048 } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_450800_80132790, { .model = &D_actor_450800_80140604 } },
    { TASK_BODY_TMD, 192, func_actor_450800_80132958, { .model = &D_actor_450800_8014AC34 } },
};

u8 D_actor_450800_8014ACC4[124] = {
    0,
    0,
    0,
    0,
    132,
    83,
    20,
    128,
    144,
    86,
    20,
    128,
    72,
    89,
    20,
    128,
    52,
    92,
    20,
    128,
    80,
    94,
    20,
    128,
    36,
    97,
    20,
    128,
    100,
    99,
    20,
    128,
    172,
    101,
    20,
    128,
    144,
    104,
    20,
    128,
    184,
    106,
    20,
    128,
    172,
    109,
    20,
    128,
    156,
    111,
    20,
    128,
    180,
    114,
    20,
    128,
    16,
    118,
    20,
    128,
    152,
    120,
    20,
    128,
    56,
    122,
    20,
    128,
    32,
    124,
    20,
    128,
    252,
    125,
    20,
    128,
    64,
    129,
    20,
    128,
    236,
    167,
    20,
    128,
    120,
    132,
    20,
    128,
    24,
    134,
    20,
    128,
    216,
    135,
    20,
    128,
    52,
    138,
    20,
    128,
    200,
    140,
    20,
    128,
    160,
    142,
    20,
    128,
    168,
    146,
    20,
    128,
    32,
    149,
    20,
    128,
    20,
    151,
    20,
    128,
    240,
    155,
    20,
    128,
};

TmdBone D_actor_450800_8014AD40[19] = {
#include "assets/actor_450800_model_1E204_skeleton.inc"
};

u32 D_actor_450800_8014AFEC[19] = {
#include "assets/actor_450800_model_1E204_partVerts.inc"
};

SVECTOR D_actor_450800_8014B038[339] = {
#include "assets/actor_450800_model_1E204_verts.inc"
};

SVECTOR D_actor_450800_8014BAD0[346] = {
#include "assets/actor_450800_model_1E204_normals.inc"
};

u32 D_actor_450800_8014C5A0[3745] = {
#include "assets/actor_450800_model_1E204_stream.inc"
};

TmdSource D_actor_450800_80150024 = {
    0,
    20476,
    5672,
    19,
    D_actor_450800_8014AFEC,
    D_actor_450800_8014B038,
    D_actor_450800_8014BAD0,
    D_actor_450800_8014AD40,
    D_actor_450800_8014C5A0,
};

TmdBone D_actor_450800_80150048[1] = {
#include "assets/actor_450800_model_1E748_skeleton.inc"
};

u32 D_actor_450800_8015006C[1] = {
#include "assets/actor_450800_model_1E748_partVerts.inc"
};

SVECTOR D_actor_450800_80150070[29] = {
#include "assets/actor_450800_model_1E748_verts.inc"
};

SVECTOR D_actor_450800_80150158[24] = {
#include "assets/actor_450800_model_1E748_normals.inc"
};

u32 D_actor_450800_80150218[212] = {
#include "assets/actor_450800_model_1E748_stream.inc"
};

TmdSource D_actor_450800_80150568 = {
    0,
    1436,
    0,
    1,
    D_actor_450800_8015006C,
    D_actor_450800_80150070,
    D_actor_450800_80150158,
    D_actor_450800_80150048,
    D_actor_450800_80150218,
};

AnimationPackedPose D_actor_450800_8015058C[21] = {
#include "assets/actor_450800_animation_1F37C_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80150688[317] = {
#include "assets/actor_450800_animation_1F37C_bank4.inc"
};

AnimationRecord D_actor_450800_80150B7C[382] = {
#include "assets/actor_450800_animation_1F37C_records.inc"
};

u16 D_actor_450800_80151174[20] = {
#include "assets/actor_450800_animation_1F37C_indices.inc"
};

AnimationSet D_actor_450800_8015119C = {
    D_actor_450800_80150B7C,
    D_actor_450800_80151174,
    { NULL, D_actor_450800_8015058C, NULL, NULL, D_actor_450800_80150688, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_801511C4[12] = {
#include "assets/actor_450800_animation_1FAE4_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80151254[187] = {
#include "assets/actor_450800_animation_1FAE4_bank4.inc"
};

AnimationRecord D_actor_450800_80151540[231] = {
#include "assets/actor_450800_animation_1FAE4_records.inc"
};

u16 D_actor_450800_801518DC[20] = {
#include "assets/actor_450800_animation_1FAE4_indices.inc"
};

AnimationSet D_actor_450800_80151904 = {
    D_actor_450800_80151540,
    D_actor_450800_801518DC,
    { NULL, D_actor_450800_801511C4, NULL, NULL, D_actor_450800_80151254, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_8015192C[20] = {
#include "assets/actor_450800_animation_206D4_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80151A1C[321] = {
#include "assets/actor_450800_animation_206D4_bank4.inc"
};

AnimationRecord D_actor_450800_80151F20[363] = {
#include "assets/actor_450800_animation_206D4_records.inc"
};

u16 D_actor_450800_801524CC[20] = {
#include "assets/actor_450800_animation_206D4_indices.inc"
};

AnimationSet D_actor_450800_801524F4 = {
    D_actor_450800_80151F20,
    D_actor_450800_801524CC,
    { NULL, D_actor_450800_8015192C, NULL, NULL, D_actor_450800_80151A1C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_8015251C[16] = {
#include "assets/actor_450800_animation_2103C_bank1.inc"
};

AnimationPackedRotation D_actor_450800_801525DC[237] = {
#include "assets/actor_450800_animation_2103C_bank4.inc"
};

AnimationRecord D_actor_450800_80152990[297] = {
#include "assets/actor_450800_animation_2103C_records.inc"
};

u16 D_actor_450800_80152E34[20] = {
#include "assets/actor_450800_animation_2103C_indices.inc"
};

AnimationSet D_actor_450800_80152E5C = {
    D_actor_450800_80152990,
    D_actor_450800_80152E34,
    { NULL, D_actor_450800_8015251C, NULL, NULL, D_actor_450800_801525DC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450800_80152E84[26] = {
#include "assets/actor_450800_animation_21B64_bank1.inc"
};

AnimationPackedRotation D_actor_450800_80152FBC[224] = {
#include "assets/actor_450800_animation_21B64_bank4.inc"
};

AnimationRecord D_actor_450800_8015333C[392] = {
#include "assets/actor_450800_animation_21B64_records.inc"
};

u16 D_actor_450800_8015395C[20] = {
#include "assets/actor_450800_animation_21B64_indices.inc"
};

AnimationSet D_actor_450800_80153984 = {
    D_actor_450800_8015333C,
    D_actor_450800_8015395C,
    { NULL, D_actor_450800_80152E84, NULL, NULL, D_actor_450800_80152FBC, NULL, NULL, NULL },
};

Actor450800MsgEntry D_actor_450800_801539AC[6] = {
    { 2003, { .call1 = func_actor_450800_80133528 } },
    { 2005, { .call6 = func_actor_450800_80133594 } },
    { 2004, { .call3 = func_actor_450800_801335F8 } },
    { 2011, { .call0 = func_actor_450800_80133670 } },
    { 2013, { .call4 = func_actor_450800_80133678 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_450800_801539DC[2] = {
    { (TASK_BODY_TMD | 0x100), 96, func_actor_450800_80133264, { .model = &D_actor_450800_80150024 } },
    { (TASK_BODY_TMD | 0x100), 96, func_actor_450800_80133740, { .model = &D_actor_450800_80150568 } },
};

u8 D_actor_450800_801539F4[24] = {
    0,
    0,
    0,
    0,
    156,
    17,
    21,
    128,
    4,
    25,
    21,
    128,
    244,
    36,
    21,
    128,
    92,
    46,
    21,
    128,
    132,
    57,
    21,
    128,
};

static void        func_actor_450800_80131E2C(void);
static void        func_actor_450800_80132000(void);
static void        func_actor_450800_80132028(void);
static inline void _actor450800TintModel(Task* spawned, Task* actor);
static void        func_actor_450800_80132160(GpEnemy* enemy, Task* task);
static void        func_actor_450800_80132E9C(GpEnemy* enemy, Task* task);

static void func_actor_450800_80131E2C(void)
{
    s32 temp_v0;
    s32 n;

    if (gGameSession->at4.loc.view == 4) {
        if (GameFlag_GetNibble(0xC7) == 1) {
            temp_v0                 = D_actor_450800_8013930C + 1;
            D_actor_450800_8013930C = temp_v0;
            if (temp_v0 >= 3) {
                D_actor_450800_8013930C = 3;
                func_800E8614(D_actor_450800_8013A774, 0);
            } else {
                func_800E8614(D_actor_450800_8013A684, 0);
            }
        } else {
            n = GameFlag_GetNibble(0xC8) + 1;
            if (n >= 4) {
                n = 3;
            }
            GameFlag_SetNibble(0xC8, n);
            if (n == 1) {
                if (GameFlag_GetNibble(0x83) == n) {
                    func_800E8614(D_actor_450800_8013A984, 0);
                } else {
                    func_800E8614(D_actor_450800_8013AB7C, 0);
                }
                func_800E3FAC(0xA2, 0x32);
            } else {
                func_800E8614(D_actor_450800_8013ACFC, 0);
            }
        }
    }
}

/// Callback the overlay's event scripts name: a non-zero `arg0` clears
/// `Gp_CapFile`, loads capture file 2 and hands 0x340 to `func_800E6D4C`; zero
/// resets the capture state instead.
void func_actor_450800_80131F28(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_450800_80131F70(u32 arg0)
{
    func_shelter_b6_nursery_80182D14(arg0 >> 16, arg0 & 0xFFFF);
}

/// Two call sites, not one: `Gp_StartCapSlot` is written out in both arms of
/// the outer test. The tail-call cross-jump in `jump.c` merges them only from
/// the `jal` onward, because sched2 hoists the `a1`/`a2` setup away from the
/// call in the first arm before that pass runs - which is why the object sets
/// `$a1`/`$a2` twice and shares one `jal`.
///
/// The global is an `s32` (see `func_actor_450800_80131E2C`, which increments
/// it whole), but this arm only wants its low half, which is the `lhu`.
void func_actor_450800_80131F98(s32 arg0)
{
    s16 var_a0;

    if (arg0 == 1) {
        var_a0 = (u16)D_actor_450800_8013930C + 2;
        Gp_StartCapSlot(var_a0, 0, 0);
    } else {
        if (GameFlag_GetNibble(0xC8) == 2) {
            var_a0 = 8;
        } else {
            var_a0 = 9;
        }
        Gp_StartCapSlot(var_a0, 0, 0);
    }
}

static void func_actor_450800_80132000(void)
{
    func_800E8614(D_actor_450800_8013A564, 0);
}

static void func_actor_450800_80132028(void)
{
    Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D3, &D_actor_450800_801397A4, 0);
    Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D4, &D_actor_450800_801398EC, 0);
}

void func_actor_450800_80132080(void)
{
    if (Mc_SaveData[0].state.demoScene != 9) {
        Mc_SaveData[0].state.at4.loc.stage = 5;
        Mc_SaveData[0].state.at4.loc.area  = 0x17;
        Mc_SaveData[0].state.at4.loc.warp  = 1;
        Mc_SaveData[0].state.at4.loc.room  = 1;
        gDisplayState.spriteVariant        = 1;
        Task_Spawn(0, 0x11, 0, 0);
    }
}

void func_actor_450800_801320E8(s32 arg0)
{
    func_shelter_b6_nursery_80180038(arg0 & 0xFF);
}

void func_actor_450800_80132108(void)
{
    SVECTOR pos;

    pos = D_actor_450800_80131E24;
    Gp_SpawnEff(0x6003B, NULL, 0x200, &pos);
}

/// Gives a freshly spawned helper model the texture page and palette of the
/// actor's placement in the current area, then streams it twice.
static inline void _actor450800TintModel(Task* spawned, Task* actor)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    AreaPlacement*   entry;
    TmdObject*       model;
    u32              idx;

    sessionKey = &gGameSession->at4.loc;
    idx        = ((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 12;
    model      = spawned->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    key.view   = gGameSession->at4.loc.view;
    areaSyncLocationVariant(&key);
    entry                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->field_0, idx);
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

/// Spawn handler of the actor's own task, state 0 of the `fns` table
/// `func_actor_450800_80132790` dispatches through. Builds the actor's
/// `Actor450800Work` block, hangs its leading matrices off the model's
/// `lightMtx` / `colorMtx`, and starts the animation.
///
/// The three helper tasks come out of `D_actor_450800_8014AC88`: 1 and 2 are
/// the actor's own model parts, textured from the placement the actor's
/// `Task::spawnArg2` enemy selects. Task 4 is spawned but not textured.
static void func_actor_450800_80132160(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GfxCoord*        coord;
    TmdObject*       obj;
    Actor450800Work* work;
    Task*            spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(0x504, 0);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_450800_80132868;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    if ((s16)(task->spawnArg1.value >> 16) == 1) {
        obj->flags = 0;
    }
    obj->otOffset = 1;
    obj->lightMtx = &work->light;
    obj->colorMtx = &work->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_450800_8014ACC4, obj, work->rig.poses,
                  work->rig.slots);
    work->st.animId = 1;
    work->st.state  = 2;

    spawned = Task_SpawnFromTable(D_actor_450800_8014AC88, 1, 8, 0);
    if (spawned != NULL) {
        work->field_4F0 = spawned;
        spawned->parent = task;
        _actor450800TintModel(spawned, task);
    }

    spawned = Task_SpawnFromTable(D_actor_450800_8014AC88, 2, 0xC, 0);
    if (spawned != NULL) {
        work->field_4F4 = spawned;
        spawned->parent = task;
        _actor450800TintModel(spawned, task);
    }

    spawned = Task_SpawnFromTable(D_actor_450800_8014AC88, 4, 8, 0);
    if (spawned != NULL) {
        spawned->parent = task;
        work->field_4F8 = spawned;
    }

    work->animArg    = 8;
    work->st.travel  = 0;
    work->turnFrames = 0;
    work->field_500  = 0;
    task->msgTable   = D_actor_450800_8014AC58;
    func_actor_450800_80132448(task);
    task->state++;
}

static void func_actor_450800_80132448(Task* task)
{
    GfxCoord*        coord = task->extra.tmd->coords;
    Actor450800Work* work  = (Actor450800Work*)task->work;

    if (work->st.state == 1) {
        func_actor_450800_80132AE0(task);
        work->st.state = 3;
    } else if (work->st.state == 2) {
        func_actor_450800_80132A68(task);
        work->st.state = 3;
    } else if (work->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (work->field_4FE) {
                    case 0:
                        actorMoveModelForward(task, 0x3C);
                        break;
                    case 1:
                        actorMoveModelForward(task, -0xF);
                        break;
                    case 2:
                        actorMoveModelForward(task, 0x19);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state  = 1;
                    work->animArg   = 0xA;
                    work->st.animId = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->turnFrames--;
        }
        func_actor_450800_80132A1C(task);
    }
}

void func_actor_450800_80132790(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_450800_80132160, func_actor_450800_801327E4 };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Per-frame handler of the actor's own task, state 1 of the `fns` table
/// `func_actor_450800_80132790` dispatches through: refreshes the model root's
/// world matrix, relights the model from a point 0x320 above its translation,
/// then runs the animation state machine and draws the ground shadow.
static void func_actor_450800_801327E4(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_450800_80132448(task);
    func_actor_450800_801328BC(task);
}

static void func_actor_450800_80132868(Task* task)
{
    Actor450800Work* work = (Actor450800Work*)task->work;

    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
    taskKill(work->field_4F0);
    taskKill(work->field_4F4);
    taskKill(work->field_4F8);
}

/// Draws the actor's ground shadow quad under the model root, skipped while
/// the model's `flags` has 0x80 set or it has no buffer yet. The world
/// position is the translation of the root part's `workm`, staged in a
/// scratchpad VECTOR3 rather than on the stack, and the quad's shade is the
/// room's current `Gp_State1C` level.
static void func_actor_450800_801328BC(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShadowShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// State handler of one of the actor's model tasks: the spawn tick hangs this
/// task's own coordinate frame off part `spawnArg1` of the actor's model and
/// every later tick hands that part's world translation, dropped by 0x320 in y,
/// to `func_800D7A9C` for the part colour matrix. The parts come from
/// `task->parent`, the actor task that spawned this one
/// (`func_actor_450800_80132160`, which also tests the same halfword on itself).
///
/// The model flags are cleared only for spawn variant 1: the high half of the
/// parent's `spawnArg1`.
void func_actor_450800_80132958(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = task->parent->extra.tmd->coords;
    GfxCoord*  part  = parts + task->spawnArg1.value;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            if ((s16)(task->parent->spawnArg1.value >> 16) == 1) {
                extra->flags = 0;
            }
            coord->parent = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// Ticks the actor's animation slots 1..0x13.
static void func_actor_450800_80132A1C(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets the actor's animation slots 1..0x13 to clip `st.animId` at rate 1,
/// without a reset argument, and latches the clip into `st.appliedAnimId`.
static void func_actor_450800_80132A68(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->work;
    i    = 1;
    do {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

static void func_actor_450800_80132AE0(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 0x1F and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 func_actor_450800_80132B44(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    Actor450800Work* work;

    work = (Actor450800Work*)task->work;
    if (args->animationId < 0x1F) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state = 1;
            work->animArg  = args->blendFrames;
        } else {
            work->st.state = 2;
        }
        work->st.field_6 = 0;
        func_actor_450800_80132448(task);
        return 0;
    }
    return -1;
}

/// Message handler 0x7D5 of `D_actor_450800_8014AC58`: sets `TmdObject::flags`
/// on this actor's own model and on the three helper tasks' ones at once.
///
/// `arg2` bit 0 selects 0 rather than 0x80, and bit 1 ORs 4 in.
/// `Actor450800Work::field_500` overrides the last of them: while it is 0 the
/// helper at `field_4F8` keeps the 0x84 handler 0x7DB's mode 2 gave it,
/// instead of the flags just computed.
s32 func_actor_450800_80132BB0(Task* task, s32 arg1, s32 arg2)
{
    Actor450800Work* work;
    TmdObject*       self;
    TmdObject*       first;
    TmdObject*       second;
    TmdObject*       third;

    work   = (Actor450800Work*)task->work;
    self   = task->extra.tmd;
    first  = work->field_4F0->extra.tmd;
    second = work->field_4F4->extra.tmd;
    third  = work->field_4F8->extra.tmd;

    if (arg2 & 1) {
        self->flags   = 0;
        first->flags  = 0;
        second->flags = 0;
        third->flags  = 0;
    } else {
        self->flags   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        first->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        second->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        third->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        self->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        first->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        second->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        third->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    if (work->field_500 == 0) {
        third->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    }
    return 0;
}

/// Message handler 0x7D4 of `D_actor_450800_8014AC58`, the placement opcode:
/// yaws the actor's root coordinate to `placement->rot.vy`, caching that yaw in
/// `Actor450800Work::yaw`, then drops the placement translation into the matrix
/// and marks it dirty.
s32 func_actor_450800_80132C68(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord*        coord;
    Actor450800Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor450800Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Message handler 0x7DB of `D_actor_450800_8014AC58`: recolour this actor's
/// body (or spawn its 0x6002B burst) according to the message's selector.
///
/// The model is the actor's own -- `task->extra.tmd`, the `TmdObject` a bodyKind-1
/// task carries -- and the one it is driven through is that of the helper task
/// in `Actor450800Work::field_4F8`. Both pointers, and `field_8` of the helper's
/// model, are resolved before the switch: the ROM reads them there, and a
/// scheduler pass cannot lift the loads into the entry block on its own.
s32 func_actor_450800_80132CE0(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    Actor450800Work* work  = (Actor450800Work*)task->work;
    TmdObject*       obj   = work->field_4F8->extra.tmd;
    GfxCoord*        coord = obj->coords;
    TmdObject*       self  = task->extra.tmd;
    s32              mode  = msg->command;

    switch (mode) {
        case 0:
            Gp_SpawnEff(0x6002B, coord, 0x21, 0);
            break;
        case 1:
            work->field_500 = mode;
            obj->flags      = self->flags;
            break;
        case 2:
            work->field_500 = 0;
            obj->flags      = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
    }
    return 0;
}

/// Message handler 0x7DD of `D_actor_450800_8014AC58`, the payload's first two
/// words being the target position: turns the actor's model to face it -- away
/// from it in mode 1 -- and latches the per-step distance over the step count
/// the mode selects, 60 in mode 0, 15 in mode 1 and 25 otherwise. The mode and
/// both results are kept on the work block.
///
/// The mode store sits after the two differences on purpose. Its place in the
/// source sets its RTL uid, and the uid is what the scheduler's ready-list
/// tie-break compares once `-O2` has CSE'd the constant 1 into a register and
/// every candidate carries the same priority; from before them the whole entry
/// block comes out in a different order and on different registers.
s32 func_actor_450800_80132D74(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    Actor450800Work* work;
    GfxCoord*        coord;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord           = task->extra.tmd->coords;
    work            = (Actor450800Work*)task->work;
    dx              = target->vx - coord->coord.t[0];
    dz              = target->vz - coord->coord.t[2];
    work->field_4FE = mode;
    angle           = ratan2(dx, dz);
    work->st.yaw    = angle;
    if (work->field_4FE == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (work->field_4FE) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}

/// Spawn handler of the enemy this actor's model task carries: state 0 of
/// `func_actor_450800_80133264`'s `fns` table. Builds the enemy's `Actor150400Work` block,
/// spawns its own model task out of the same `D_actor_450800_801539DC` table,
/// faces it at the placed spawn point, starts the animation and hands the state
/// machine to `func_actor_450800_801330AC`.
static void func_actor_450800_80132E9C(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor150400Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;
    GpEnemy*         spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (work = (Actor150400Work*)memCalloc(sizeof(Actor150400Work), false));
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_450800_8013333C;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    work->enemy                  = enemy;
    spawned                      = Gp_SpawnEnemyFromTable(D_actor_450800_801539DC, 1, 0, enemy);
    actorTintModel(spawned->task->extra.tmd, enemy);
    Task_Reparent(task, spawned->task);
    work->pairTask = spawned->task;
    obj->lightMtx  = &work->light;
    obj->colorMtx  = &work->color;
    vec.vx         = coord->workm.t[0];
    vec.vy         = coord->workm.t[1] - 0x320;
    vec.vz         = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_450800_801539F4, obj,
                  &work->rig.poses, work->rig.slots);
    work->st.animId = 1;
    work->st.state  = 2;
    task->msgTable  = D_actor_450800_801539AC;
    func_actor_450800_801330AC(task);
    task->state++;
}

/// The enemy's animation state machine, run by its spawn and per-frame
/// handlers. States 1 and 2 start the clip in `animId` through
/// `func_actor_450800_801334C4` or `func_actor_450800_8013344C` and advance to
/// state 3. State 3 walks the model 12 units a frame while the walk clip (4)
/// has `travel` left, dropping back to clip 1 with reset argument 0xA when it
/// runs out, then ticks the slots.
static void func_actor_450800_801330AC(Task* task)
{
    Actor150400Work* work;
    s16              animId;

    work = (Actor150400Work*)task->work;
    if (work->st.state == 1) {
        func_actor_450800_801334C4(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_450800_8013344C(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveModelForward(task, 0xC);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_450800_80133400(task);
        return;
    }
}

void func_actor_450800_80133264(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_450800_80132E9C, func_actor_450800_801332B8 };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Per-frame handler of the enemy this actor's model task carries: state 1 of
/// `func_actor_450800_80133264`'s `fns` table.
///
/// Refreshes the enemy model root's coordinate, feeds its world translation
/// (lowered by 800 on y, to sit on the ground) to `func_800D7A9C`, then ticks
/// the enemy's animation state through `func_actor_450800_801330AC` and draws its
/// ground shadow.
static void func_actor_450800_801332B8(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_450800_801330AC(task);
    func_actor_450800_80133364(task);
}

/// Exit callback of the enemy's task, set by its spawn handler
/// `func_actor_450800_80132E9C`: releases the enemy slot the task was spawned
/// for.
static void func_actor_450800_8013333C(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Draws the enemy's ground shadow quad under its model root, skipped while
/// the model's `flags` has 0x80 set or it has no buffer yet. The world
/// position is the translation of the root part's `workm`, staged in a
/// scratchpad VECTOR3 rather than on the stack, and the quad's shade is the
/// room's current `Gp_State1C` level.
static void func_actor_450800_80133364(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShadowShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks the enemy's animation slots 1..0x12.
static void func_actor_450800_80133400(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Resets the enemy's animation slots 1..0x12 to clip `animId` at rate 1,
/// without a reset argument, and latches the clip into `appliedAnimId`.
static void func_actor_450800_8013344C(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
    }
    work->st.appliedAnimId = work->st.animId;
}

/// Starts the enemy's animation slots 1..0x12 on clip `animId`, forwarding
/// `animArg` as the reset argument, and latches the clip into `appliedAnimId`.
static void func_actor_450800_801334C4(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    for (i = 1; i < 0x13; i++) {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
    }
    work->st.appliedAnimId = work->st.animId;
}

/// Message handler 0x7D3 of `D_actor_450800_801539AC`, the enemy's "start
/// animation" opcode: `withArg` selects between the two start paths
/// `func_actor_450800_801330AC` dispatches on, and only the first carries
/// `animArg`. Returns -1, without touching the work block, when the clip id is
/// out of range.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one. Without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`);
/// the ROM has the *else* arm's `state = 2` there, which the pass only reaches
/// once an `asm` at the head of the fall-through stops it searching that
/// thread. See DECOMPILATION_LEARNINGS.md, "An empty asm at the head of the
/// then-arm moves the delay slot to the else arm".
s32 func_actor_450800_80133528(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    Actor150400Work* work;

    work = (Actor150400Work*)task->work;
    if (args->animationId < 6) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state = 1;
            work->animArg  = args->blendFrames;
        } else {
            work->st.state = 2;
        }
        work->st.field_6 = 0;
        func_actor_450800_801330AC(task);
        return 0;
    }
    return -1;
}

/// Message handler 0x7D5 of `D_actor_450800_801539AC`: sets `TmdObject::flags`
/// on the enemy's own model and on the sub-model task's in `pairTask` at
/// once. `flags` bit 0 selects 0 rather than 0x80, and bit 1 ORs 4 in.
s32 func_actor_450800_80133594(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = task->extra.tmd;
    other = ((Actor150400Work*)task->work)->pairTask->extra.tmd;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        other->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (flags & 2) {
        self->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        other->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Message handler 0x7D4 of `D_actor_450800_801539AC`, the enemy's placement
/// opcode: yaws its root coordinate to `placement->rot.vy`, caching that yaw
/// in `Actor150400Work::yaw`, then drops the placement translation into
/// the matrix and marks it dirty.
s32 func_actor_450800_801335F8(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord*        coord;
    Actor150400Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

s32 func_actor_450800_80133670(void)
{
    return 0;
}

/// Message handler 0x7DD of `D_actor_450800_801539AC`, the enemy's "walk to"
/// opcode: turns its root coordinate to face `target`, caching the yaw in
/// `Actor150400Work::yaw`, and leaves the horizontal distance to it, in
/// twelfths, in `travel` for the walk state to count down.
s32 func_actor_450800_80133678(Task* task, s32 arg1, VECTOR* target)
{
    GfxCoord*        coord;
    Actor150400Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    dx           = target->vx - coord->coord.t[0];
    dz           = target->vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}

/// Handler of the enemy's sub-model task, the second entry of
/// `D_actor_450800_801539DC`, which the enemy's spawn handler reparents under
/// the enemy's own task. On its first tick it lights the sub-model with the
/// enemy's `Actor150400Work` matrices and hangs its root coordinate off
/// part 7 of the enemy's model; after that it only marks the coordinate dirty
/// each frame so it follows that part.
void func_actor_450800_80133740(Task* task)
{
    char             pad[0x10];
    Task*            parent = task->parent;
    TmdObject*       obj    = task->extra.tmd;
    GfxCoord*        coord  = obj->coords;
    GfxCoord*        sub    = &parent->extra.tmd->coords[7];
    Actor150400Work* work   = (Actor150400Work*)parent->work;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            obj->lightMtx       = &work->light;
            obj->colorMtx       = &work->color;
            coord->parent       = sub;
            task->state++;
            break;
        case 1:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
