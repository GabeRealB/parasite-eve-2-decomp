#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/dryfield_night_main_street.h"
#include "../../shared/screen_fade.h"
#include "../../shared/actor_messages.h"

extern ActorTransform D_actor_136100_8013F334[2];

extern ActorTransform D_actor_136100_8013F304[2];

/// Work block for the `actor_136100` overlay's cutscene actor.
///
/// `func_actor_136100_80133A88` allocates it with `Mem_Malloc(0x4F0, 0)`,
/// zeroes it with `Mem_Set` and parks the pointer in the task's `Task::work`
/// slot (0x1C) -- that slot is not a `TaskIdMap` here, so reach the block with
/// `(Actor136100Work*)task->work`.  The same function publishes the task
/// itself in `D_actor_136100_8014078C` and stores the `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)`
/// task in `field_4B4`.
///
/// The block opens with the model's rig, whose slots 1..19 the animation
/// start handler reseeds. The pairs from `field_4C4`, `field_4CC` and
/// `field_4D4` on are value/countdown pairs the overlay's small setters write
/// together.
typedef struct Actor136100Work {
    /* 0x000 */ ActorAnimRig20 rig;
    /* 0x474 */ MATRIX         field_474; // light matrix, into TmdObject::lightMtx
    /* 0x494 */ MATRIX         field_494; // colour matrix, into TmdObject::colorMtx
    /* 0x4B4 */ Task*          field_4B4; // gameGetTaskSlot(GAME_TASK_SLOT_PLAYER) task
    /* 0x4B8 */ Task*          field_4B8; // task spawned from entry 2 of D_actor_136100_80140744
    /* 0x4BC */ Task*          field_4BC; // task spawned from entry 3 of D_actor_136100_80140744
    /* 0x4C0 */ Task*          field_4C0; // second dispatch task (NULL-checked senders)
    /* 0x4C4 */ s16            field_4C4; // set by func_actor_136100_80134838
    /* 0x4C6 */ s16            field_4C6; // cleared alongside field_4C4
    /* 0x4C8 */ s16            field_4C8; // tick counter: func_actor_136100_801323F8
    /* 0x4CA */ byte           pad_4CA[0x2];
    /* 0x4CC */ s16            field_4CC; // set by func_actor_136100_80134858
    /* 0x4CE */ s16            field_4CE; // cleared alongside field_4CC
    /* 0x4D0 */ s16            field_4D0; // request 5 tick counter
    /* 0x4D2 */ byte           pad_4D2[0x2];
    /* 0x4D4 */ s16            field_4D4; // set by func_actor_136100_80134878
    /* 0x4D6 */ s16            field_4D6; // cleared alongside field_4D4
    /* 0x4D8 */ s16            field_4D8; // shot count: func_actor_136100_80132BC0
    /* 0x4DA */ s16            field_4DA; // countdown: func_actor_136100_80132BC0
    /* 0x4DC */ u16            field_4DC; // cue step: func_actor_136100_80133904
    /* 0x4DE */ s16            field_4DE; // set by func_actor_136100_80133690
    /* 0x4E0 */ s16            field_4E0; // animation slot count reset by func_actor_136100_801347B8
    /* 0x4E2 */ u16            field_4E2; // index into the D_actor_136100_8013F218 animation chain
    /* 0x4E4 */ u16            field_4E4; // cue phase: func_actor_136100_80133904
    /* 0x4E6 */ byte           pad_4E6[0x4];
    /* 0x4EA */ s16            field_4EA; // fourth model part's Y rotation
    /* 0x4EC */ s16            field_4EC; // player-eff flag: Gp_KillPlayerEffs / Gp_SpawnWeaponEff
    /* 0x4EE */ byte           pad_4EE[0x2];
} Actor136100Work;
STATIC_ASSERT_SIZEOF(Actor136100Work, 0x4F0);

/// Set by `func_actor_136100_801348F8` when the cutscene wants the display
/// back on; while it is non-zero the fade task kills itself instead of fading.
extern u16 D_actor_136100_8013F17C;

/// Next-animation table indexed by field_4DE - 0x2F; negative entries end
/// the chain, and live entries are sent as animation ids with 0x2F added.
extern s16 D_actor_136100_8013F1EC[];

/// Next animation indexed by field_4E0; negative entries skip the restart.
extern s16 D_actor_136100_8013F1FC[];

/// The actor's six-entry task table, spawned from by index: 0 is the actor
/// itself, 2 and 3 the tasks kept in `field_4B8`/`field_4BC`, 4 the fade-in and
/// 5 the fade-out.
extern TaskDesc D_actor_136100_80140744[];

extern ActorTransform D_actor_136100_8013F3C4;
extern ActorTransform D_actor_136100_8013F3DC;

extern AnimationSet* D_actor_136100_8013F180[8];
extern AnimationSet* D_actor_136100_8013F1A0[13];
extern AnimationSet* D_actor_136100_8013F1D4[6];
extern s16           D_actor_136100_8013F218[];
extern s32           D_actor_136100_8013F224[8];
extern s32           D_actor_136100_8013F244[32];
extern s32           D_actor_136100_8013F2C4[12];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, ActorTransform*);
        void (*call1)(Task*, s32, s32);
    } handler;
} Actor136100MessageEntry;
STATIC_ASSERT_SIZEOF(Actor136100MessageEntry, 8);

extern Actor136100MessageEntry D_actor_136100_8013F2F4[2];
extern ActorTransform          D_actor_136100_8013F364;
extern ActorTransform          D_actor_136100_8013F37C;
extern ActorTransform          D_actor_136100_8013F394;
extern ActorTransform          D_actor_136100_8013F3AC;
extern ActorTransform          D_actor_136100_8013F3F4;
extern ActorTransform          D_actor_136100_8013F40C;
extern ActorTransform          D_actor_136100_8013F424;
extern ActorTransform          D_actor_136100_8013F43C;
extern ActorTransform          D_actor_136100_8013F454;
extern EvsCommand              D_actor_136100_8013F46C[];
extern EvsCommand              D_actor_136100_8013F784[];
extern EvsCommand              D_actor_136100_8013F94C[];
extern EvsCommand              D_actor_136100_8013FAE4[];
extern EvsCommand              D_actor_136100_8013FC64[];
extern EvsCommand              D_actor_136100_8013FD84[];
extern EvsCommand              D_actor_136100_80140114[];
extern EvsCommand              D_actor_136100_801402C4[];
extern EvsCommand              D_actor_136100_801404EC[];
extern EvsCommand              D_actor_136100_8014063C[];
extern Task*                   D_actor_136100_8014078C;

static void func_actor_136100_80132748(Task* arg0);
static void func_actor_136100_80133238(Task* arg0);
static void func_actor_136100_80134A18(Task* task);

extern AnimationSet D_actor_136100_8013B234;
extern AnimationSet D_actor_136100_8013B574;
extern AnimationSet D_actor_136100_8013B85C;
extern AnimationSet D_actor_136100_8013BB18;
extern AnimationSet D_actor_136100_8013BCCC;
extern AnimationSet D_actor_136100_8013BF20;
extern AnimationSet D_actor_136100_8013C130;
extern AnimationSet D_actor_136100_8013C3C4;
extern AnimationSet D_actor_136100_8013C6D4;
extern AnimationSet D_actor_136100_8013C8C8;
extern AnimationSet D_actor_136100_8013CB5C;
extern AnimationSet D_actor_136100_8013CD50;
extern AnimationSet D_actor_136100_8013E774;
extern AnimationSet D_actor_136100_8013EA04;
extern AnimationSet D_actor_136100_8013EB94;
extern AnimationSet D_actor_136100_8013ED3C;
extern AnimationSet D_actor_136100_8013EEFC;
extern AnimationSet D_actor_136100_8013F154;

extern TmdSource D_actor_136100_8013A500;
extern TmdSource D_actor_136100_8013A9D8;
extern TmdSource D_actor_136100_8013AFCC;
void             func_actor_136100_801320E0(Task*);
void             func_actor_136100_80132284(Task*);
void             func_actor_136100_80133690(void);
void             func_actor_136100_8013379C(s32);
void             func_actor_136100_80133BC8(Task*);
void             func_actor_136100_80134588(Task*);
void             func_actor_136100_8013467C(void);
void             func_actor_136100_801346EC(Task*, s32, s32);
void             func_actor_136100_801347B8(void);
void             func_actor_136100_80134838(s16);
void             func_actor_136100_80134858(s16);
void             func_actor_136100_80134878(s16);
void             func_actor_136100_80134898(void);
void             func_actor_136100_801348C8(void);
void             func_actor_136100_801348F8(void);
void             func_actor_136100_80134924(void);
void             func_actor_136100_80134964(void);
void             func_actor_136100_801349B4(s32);

TmdBone D_actor_136100_80134A94[20] = {
#include "assets/gary_douglas_body_skeleton.inc"
};

u32 D_actor_136100_80134D64[20] = {
#include "assets/gary_douglas_body_partVerts.inc"
};

SVECTOR D_actor_136100_80134DB4[364] = {
#include "assets/gary_douglas_body_verts.inc"
};

SVECTOR D_actor_136100_80135914[354] = {
#include "assets/gary_douglas_body_normals.inc"
};

u32 D_actor_136100_80136424[4151] = {
#include "assets/gary_douglas_body_stream.inc"
};

TmdSource D_actor_136100_8013A500 = {
    0,
    22008,
    6952,
    20,
    D_actor_136100_80134D64,
    D_actor_136100_80134DB4,
    D_actor_136100_80135914,
    D_actor_136100_80134A94,
    D_actor_136100_80136424,
};

TmdBone D_actor_136100_8013A524[1] = {
#include "assets/gary_douglas_head_hat_skeleton.inc"
};

u32 D_actor_136100_8013A548[1] = {
#include "assets/gary_douglas_head_hat_partVerts.inc"
};

SVECTOR D_actor_136100_8013A54C[21] = {
#include "assets/gary_douglas_head_hat_verts.inc"
};

SVECTOR D_actor_136100_8013A5F4[21] = {
#include "assets/gary_douglas_head_hat_normals.inc"
};

u32 D_actor_136100_8013A69C[207] = {
#include "assets/gary_douglas_head_hat_stream.inc"
};

TmdSource D_actor_136100_8013A9D8 = {
    0,
    1352,
    0,
    1,
    D_actor_136100_8013A548,
    D_actor_136100_8013A54C,
    D_actor_136100_8013A5F4,
    D_actor_136100_8013A524,
    D_actor_136100_8013A69C,
};

TmdBone D_actor_136100_8013A9FC[1] = {
#include "assets/actor_120300_model_082F8_skeleton.inc"
};

u32 D_actor_136100_8013AA20[1] = {
#include "assets/actor_120300_model_082F8_partVerts.inc"
};

SVECTOR D_actor_136100_8013AA24[34] = {
#include "assets/actor_120300_model_082F8_verts.inc"
};

SVECTOR D_actor_136100_8013AB34[28] = {
#include "assets/actor_120300_model_082F8_normals.inc"
};

u32 D_actor_136100_8013AC14[238] = {
#include "assets/actor_120300_model_082F8_stream.inc"
};

TmdSource D_actor_136100_8013AFCC = {
    0,
    1692,
    0,
    1,
    D_actor_136100_8013AA20,
    D_actor_136100_8013AA24,
    D_actor_136100_8013AB34,
    D_actor_136100_8013A9FC,
    D_actor_136100_8013AC14,
};

AnimationPackedPose D_actor_136100_8013AFF0[2] = {
#include "assets/actor_136100_animation_09414_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013B008[24] = {
#include "assets/actor_136100_animation_09414_bank4.inc"
};

AnimationRecord D_actor_136100_8013B068[105] = {
#include "assets/actor_136100_animation_09414_records.inc"
};

u16 D_actor_136100_8013B20C[20] = {
#include "assets/actor_136100_animation_09414_indices.inc"
};

AnimationSet D_actor_136100_8013B234 = {
    D_actor_136100_8013B068,
    D_actor_136100_8013B20C,
    { NULL, D_actor_136100_8013AFF0, NULL, NULL, D_actor_136100_8013B008, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013B25C[2] = {
#include "assets/actor_136100_animation_09754_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013B274[54] = {
#include "assets/actor_136100_animation_09754_bank4.inc"
};

AnimationRecord D_actor_136100_8013B34C[128] = {
#include "assets/actor_136100_animation_09754_records.inc"
};

u16 D_actor_136100_8013B54C[20] = {
#include "assets/actor_136100_animation_09754_indices.inc"
};

AnimationSet D_actor_136100_8013B574 = {
    D_actor_136100_8013B34C,
    D_actor_136100_8013B54C,
    { NULL, D_actor_136100_8013B25C, NULL, NULL, D_actor_136100_8013B274, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013B59C[2] = {
#include "assets/actor_136100_animation_09A3C_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013B5B4[64] = {
#include "assets/actor_136100_animation_09A3C_bank4.inc"
};

AnimationRecord D_actor_136100_8013B6B4[96] = {
#include "assets/actor_136100_animation_09A3C_records.inc"
};

u16 D_actor_136100_8013B834[20] = {
#include "assets/actor_136100_animation_09A3C_indices.inc"
};

AnimationSet D_actor_136100_8013B85C = {
    D_actor_136100_8013B6B4,
    D_actor_136100_8013B834,
    { NULL, D_actor_136100_8013B59C, NULL, NULL, D_actor_136100_8013B5B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013B884[2] = {
#include "assets/actor_136100_animation_09CF8_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013B89C[41] = {
#include "assets/actor_136100_animation_09CF8_bank4.inc"
};

AnimationRecord D_actor_136100_8013B940[108] = {
#include "assets/actor_136100_animation_09CF8_records.inc"
};

u16 D_actor_136100_8013BAF0[20] = {
#include "assets/actor_136100_animation_09CF8_indices.inc"
};

AnimationSet D_actor_136100_8013BB18 = {
    D_actor_136100_8013B940,
    D_actor_136100_8013BAF0,
    { NULL, D_actor_136100_8013B884, NULL, NULL, D_actor_136100_8013B89C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013BB40[2] = {
#include "assets/actor_136100_animation_09EAC_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013BB58[23] = {
#include "assets/actor_136100_animation_09EAC_bank4.inc"
};

AnimationRecord D_actor_136100_8013BBB4[60] = {
#include "assets/actor_136100_animation_09EAC_records.inc"
};

u16 D_actor_136100_8013BCA4[20] = {
#include "assets/actor_136100_animation_09EAC_indices.inc"
};

AnimationSet D_actor_136100_8013BCCC = {
    D_actor_136100_8013BBB4,
    D_actor_136100_8013BCA4,
    { NULL, D_actor_136100_8013BB40, NULL, NULL, D_actor_136100_8013BB58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013BCF4[2] = {
#include "assets/actor_136100_animation_0A100_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013BD0C[35] = {
#include "assets/actor_136100_animation_0A100_bank4.inc"
};

AnimationRecord D_actor_136100_8013BD98[88] = {
#include "assets/actor_136100_animation_0A100_records.inc"
};

u16 D_actor_136100_8013BEF8[20] = {
#include "assets/actor_136100_animation_0A100_indices.inc"
};

AnimationSet D_actor_136100_8013BF20 = {
    D_actor_136100_8013BD98,
    D_actor_136100_8013BEF8,
    { NULL, D_actor_136100_8013BCF4, NULL, NULL, D_actor_136100_8013BD0C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013BF48[2] = {
#include "assets/actor_136100_animation_0A310_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013BF60[20] = {
#include "assets/actor_136100_animation_0A310_bank4.inc"
};

AnimationRecord D_actor_136100_8013BFB0[86] = {
#include "assets/actor_136100_animation_0A310_records.inc"
};

u16 D_actor_136100_8013C108[20] = {
#include "assets/actor_136100_animation_0A310_indices.inc"
};

AnimationSet D_actor_136100_8013C130 = {
    D_actor_136100_8013BFB0,
    D_actor_136100_8013C108,
    { NULL, D_actor_136100_8013BF48, NULL, NULL, D_actor_136100_8013BF60, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013C158[2] = {
#include "assets/actor_136100_animation_0A5A4_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013C170[34] = {
#include "assets/actor_136100_animation_0A5A4_bank4.inc"
};

AnimationRecord D_actor_136100_8013C1F8[105] = {
#include "assets/actor_136100_animation_0A5A4_records.inc"
};

u16 D_actor_136100_8013C39C[20] = {
#include "assets/actor_136100_animation_0A5A4_indices.inc"
};

AnimationSet D_actor_136100_8013C3C4 = {
    D_actor_136100_8013C1F8,
    D_actor_136100_8013C39C,
    { NULL, D_actor_136100_8013C158, NULL, NULL, D_actor_136100_8013C170, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013C3EC[4] = {
#include "assets/actor_136100_animation_0A8B4_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013C41C[59] = {
#include "assets/actor_136100_animation_0A8B4_bank4.inc"
};

AnimationRecord D_actor_136100_8013C508[105] = {
#include "assets/actor_136100_animation_0A8B4_records.inc"
};

u16 D_actor_136100_8013C6AC[20] = {
#include "assets/actor_136100_animation_0A8B4_indices.inc"
};

AnimationSet D_actor_136100_8013C6D4 = {
    D_actor_136100_8013C508,
    D_actor_136100_8013C6AC,
    { NULL, D_actor_136100_8013C3EC, NULL, NULL, D_actor_136100_8013C41C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013C6FC[2] = {
#include "assets/actor_136100_animation_0AAA8_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013C714[34] = {
#include "assets/actor_136100_animation_0AAA8_bank4.inc"
};

AnimationRecord D_actor_136100_8013C79C[65] = {
#include "assets/actor_136100_animation_0AAA8_records.inc"
};

u16 D_actor_136100_8013C8A0[20] = {
#include "assets/actor_136100_animation_0AAA8_indices.inc"
};

AnimationSet D_actor_136100_8013C8C8 = {
    D_actor_136100_8013C79C,
    D_actor_136100_8013C8A0,
    { NULL, D_actor_136100_8013C6FC, NULL, NULL, D_actor_136100_8013C714, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013C8F0[2] = {
#include "assets/actor_136100_animation_0AD3C_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013C908[39] = {
#include "assets/actor_136100_animation_0AD3C_bank4.inc"
};

AnimationRecord D_actor_136100_8013C9A4[100] = {
#include "assets/actor_136100_animation_0AD3C_records.inc"
};

u16 D_actor_136100_8013CB34[20] = {
#include "assets/actor_136100_animation_0AD3C_indices.inc"
};

AnimationSet D_actor_136100_8013CB5C = {
    D_actor_136100_8013C9A4,
    D_actor_136100_8013CB34,
    { NULL, D_actor_136100_8013C8F0, NULL, NULL, D_actor_136100_8013C908, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013CB84[2] = {
#include "assets/actor_136100_animation_0AF30_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013CB9C[34] = {
#include "assets/actor_136100_animation_0AF30_bank4.inc"
};

AnimationRecord D_actor_136100_8013CC24[65] = {
#include "assets/actor_136100_animation_0AF30_records.inc"
};

u16 D_actor_136100_8013CD28[20] = {
#include "assets/actor_136100_animation_0AF30_indices.inc"
};

AnimationSet D_actor_136100_8013CD50 = {
    D_actor_136100_8013CC24,
    D_actor_136100_8013CD28,
    { NULL, D_actor_136100_8013CB84, NULL, NULL, D_actor_136100_8013CB9C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013CD78[6] = {
#include "assets/actor_136100_animation_0B234_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013CDC0[46] = {
#include "assets/actor_136100_animation_0B234_bank4.inc"
};

AnimationRecord D_actor_136100_8013CE78[109] = {
#include "assets/actor_136100_animation_0B234_records.inc"
};

u16 D_actor_136100_8013D02C[20] = {
#include "assets/actor_136100_animation_0B234_indices.inc"
};

AnimationSet D_actor_136100_8013D054 = {
    D_actor_136100_8013CE78,
    D_actor_136100_8013D02C,
    { NULL, D_actor_136100_8013CD78, NULL, NULL, D_actor_136100_8013CDC0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013D07C[2] = {
#include "assets/actor_136100_animation_0B590_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013D094[67] = {
#include "assets/actor_136100_animation_0B590_bank4.inc"
};

AnimationRecord D_actor_136100_8013D1A0[122] = {
#include "assets/actor_136100_animation_0B590_records.inc"
};

u16 D_actor_136100_8013D388[20] = {
#include "assets/actor_136100_animation_0B590_indices.inc"
};

AnimationSet D_actor_136100_8013D3B0 = {
    D_actor_136100_8013D1A0,
    D_actor_136100_8013D388,
    { NULL, D_actor_136100_8013D07C, NULL, NULL, D_actor_136100_8013D094, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013D3D8[3] = {
#include "assets/actor_136100_animation_0B7EC_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013D3FC[35] = {
#include "assets/actor_136100_animation_0B7EC_bank4.inc"
};

AnimationRecord D_actor_136100_8013D488[87] = {
#include "assets/actor_136100_animation_0B7EC_records.inc"
};

u16 D_actor_136100_8013D5E4[20] = {
#include "assets/actor_136100_animation_0B7EC_indices.inc"
};

AnimationSet D_actor_136100_8013D60C = {
    D_actor_136100_8013D488,
    D_actor_136100_8013D5E4,
    { NULL, D_actor_136100_8013D3D8, NULL, NULL, D_actor_136100_8013D3FC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013D634[8] = {
#include "assets/actor_136100_animation_0BBC0_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013D694[84] = {
#include "assets/actor_136100_animation_0BBC0_bank4.inc"
};

AnimationRecord D_actor_136100_8013D7E4[117] = {
#include "assets/actor_136100_animation_0BBC0_records.inc"
};

u16 D_actor_136100_8013D9B8[20] = {
#include "assets/actor_136100_animation_0BBC0_indices.inc"
};

AnimationSet D_actor_136100_8013D9E0 = {
    D_actor_136100_8013D7E4,
    D_actor_136100_8013D9B8,
    { NULL, D_actor_136100_8013D634, NULL, NULL, D_actor_136100_8013D694, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013DA08[7] = {
#include "assets/actor_136100_animation_0BFB0_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013DA5C[62] = {
#include "assets/actor_136100_animation_0BFB0_bank4.inc"
};

AnimationRecord D_actor_136100_8013DB54[149] = {
#include "assets/actor_136100_animation_0BFB0_records.inc"
};

u16 D_actor_136100_8013DDA8[20] = {
#include "assets/actor_136100_animation_0BFB0_indices.inc"
};

AnimationSet D_actor_136100_8013DDD0 = {
    D_actor_136100_8013DB54,
    D_actor_136100_8013DDA8,
    { NULL, D_actor_136100_8013DA08, NULL, NULL, D_actor_136100_8013DA5C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013DDF8[7] = {
#include "assets/actor_136100_animation_0C31C_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013DE4C[74] = {
#include "assets/actor_136100_animation_0C31C_bank4.inc"
};

AnimationRecord D_actor_136100_8013DF74[104] = {
#include "assets/actor_136100_animation_0C31C_records.inc"
};

u16 D_actor_136100_8013E114[20] = {
#include "assets/actor_136100_animation_0C31C_indices.inc"
};

AnimationSet D_actor_136100_8013E13C = {
    D_actor_136100_8013DF74,
    D_actor_136100_8013E114,
    { NULL, D_actor_136100_8013DDF8, NULL, NULL, D_actor_136100_8013DE4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013E164[5] = {
#include "assets/actor_136100_animation_0C620_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013E1A0[55] = {
#include "assets/actor_136100_animation_0C620_bank4.inc"
};

AnimationRecord D_actor_136100_8013E27C[103] = {
#include "assets/actor_136100_animation_0C620_records.inc"
};

u16 D_actor_136100_8013E418[20] = {
#include "assets/actor_136100_animation_0C620_indices.inc"
};

AnimationSet D_actor_136100_8013E440 = {
    D_actor_136100_8013E27C,
    D_actor_136100_8013E418,
    { NULL, D_actor_136100_8013E164, NULL, NULL, D_actor_136100_8013E1A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013E468[5] = {
#include "assets/actor_136100_animation_0C954_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013E4A4[46] = {
#include "assets/actor_136100_animation_0C954_bank4.inc"
};

AnimationRecord D_actor_136100_8013E55C[124] = {
#include "assets/actor_136100_animation_0C954_records.inc"
};

u16 D_actor_136100_8013E74C[20] = {
#include "assets/actor_136100_animation_0C954_indices.inc"
};

AnimationSet D_actor_136100_8013E774 = {
    D_actor_136100_8013E55C,
    D_actor_136100_8013E74C,
    { NULL, D_actor_136100_8013E468, NULL, NULL, D_actor_136100_8013E4A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013E79C[2] = {
#include "assets/actor_136100_animation_0CBE4_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013E7B4[36] = {
#include "assets/actor_136100_animation_0CBE4_bank4.inc"
};

AnimationRecord D_actor_136100_8013E844[102] = {
#include "assets/actor_136100_animation_0CBE4_records.inc"
};

u16 D_actor_136100_8013E9DC[20] = {
#include "assets/actor_136100_animation_0CBE4_indices.inc"
};

AnimationSet D_actor_136100_8013EA04 = {
    D_actor_136100_8013E844,
    D_actor_136100_8013E9DC,
    { NULL, D_actor_136100_8013E79C, NULL, NULL, D_actor_136100_8013E7B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013EA2C[2] = {
#include "assets/actor_136100_animation_0CD74_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013EA44[17] = {
#include "assets/actor_136100_animation_0CD74_bank4.inc"
};

AnimationRecord D_actor_136100_8013EA88[57] = {
#include "assets/actor_136100_animation_0CD74_records.inc"
};

u16 D_actor_136100_8013EB6C[20] = {
#include "assets/actor_136100_animation_0CD74_indices.inc"
};

AnimationSet D_actor_136100_8013EB94 = {
    D_actor_136100_8013EA88,
    D_actor_136100_8013EB6C,
    { NULL, D_actor_136100_8013EA2C, NULL, NULL, D_actor_136100_8013EA44, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013EBBC[2] = {
#include "assets/actor_136100_animation_0CF1C_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013EBD4[23] = {
#include "assets/actor_136100_animation_0CF1C_bank4.inc"
};

AnimationRecord D_actor_136100_8013EC30[57] = {
#include "assets/actor_136100_animation_0CF1C_records.inc"
};

u16 D_actor_136100_8013ED14[20] = {
#include "assets/actor_136100_animation_0CF1C_indices.inc"
};

AnimationSet D_actor_136100_8013ED3C = {
    D_actor_136100_8013EC30,
    D_actor_136100_8013ED14,
    { NULL, D_actor_136100_8013EBBC, NULL, NULL, D_actor_136100_8013EBD4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013ED64[2] = {
#include "assets/actor_136100_animation_0D0DC_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013ED7C[26] = {
#include "assets/actor_136100_animation_0D0DC_bank4.inc"
};

AnimationRecord D_actor_136100_8013EDE4[60] = {
#include "assets/actor_136100_animation_0D0DC_records.inc"
};

u16 D_actor_136100_8013EED4[20] = {
#include "assets/actor_136100_animation_0D0DC_indices.inc"
};

AnimationSet D_actor_136100_8013EEFC = {
    D_actor_136100_8013EDE4,
    D_actor_136100_8013EED4,
    { NULL, D_actor_136100_8013ED64, NULL, NULL, D_actor_136100_8013ED7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136100_8013EF24[2] = {
#include "assets/actor_136100_animation_0D334_bank1.inc"
};

AnimationPackedRotation D_actor_136100_8013EF3C[40] = {
#include "assets/actor_136100_animation_0D334_bank4.inc"
};

AnimationRecord D_actor_136100_8013EFDC[84] = {
#include "assets/actor_136100_animation_0D334_records.inc"
};

u16 D_actor_136100_8013F12C[20] = {
#include "assets/actor_136100_animation_0D334_indices.inc"
};

AnimationSet D_actor_136100_8013F154 = {
    D_actor_136100_8013EFDC,
    D_actor_136100_8013F12C,
    { NULL, D_actor_136100_8013EF24, NULL, NULL, D_actor_136100_8013EF3C, NULL, NULL, NULL },
};

u16 D_actor_136100_8013F17C = 0;

AnimationSet* D_actor_136100_8013F180[8] = {
    &D_actor_136100_8013D054,
    &D_actor_136100_8013D3B0,
    &D_actor_136100_8013D60C,
    &D_actor_136100_8013D9E0,
    &D_actor_136100_8013DDD0,
    &D_actor_136100_8013E13C,
    &D_actor_136100_8013E440,
    NULL,
};

AnimationSet* D_actor_136100_8013F1A0[13] = {
    NULL,
    &D_actor_136100_8013B234,
    &D_actor_136100_8013B574,
    &D_actor_136100_8013C130,
    &D_actor_136100_8013C3C4,
    &D_actor_136100_8013B85C,
    &D_actor_136100_8013C6D4,
    &D_actor_136100_8013C8C8,
    &D_actor_136100_8013CB5C,
    &D_actor_136100_8013CD50,
    &D_actor_136100_8013BB18,
    &D_actor_136100_8013BCCC,
    &D_actor_136100_8013BF20,
};

AnimationSet* D_actor_136100_8013F1D4[6] = {
    &D_actor_136100_8013E774,
    &D_actor_136100_8013EA04,
    &D_actor_136100_8013F154,
    &D_actor_136100_8013ED3C,
    &D_actor_136100_8013EEFC,
    &D_actor_136100_8013EB94,
};

s16 D_actor_136100_8013F1EC[8] = {
    -1,
    -1,
    -1,
    4,
    -1,
    0,
    0,
    0,
};

s16 D_actor_136100_8013F1FC[14] = {
    -1,
    -1,
    -1,
    -1,
    -1,
    1,
    -1,
    8,
    -1,
    1,
    -1,
    1,
    1,
    0,
};

s16 D_actor_136100_8013F218[6] = {
    -1,
    -1,
    -1,
    -1,
    1,
    -1,
};

s32 D_actor_136100_8013F224[8] = { 0xF000, 0, 0, 0xF000, 4096, 0, 0, 4096 };

s32 D_actor_136100_8013F244[32] = { -0x12B0BB8, 8400, -0x12B0BB8, 7000, 0xF448, 8400, 0xF448, 7000, -0x12B0BB8, 7000, -0x12B0708, 7000, 0xF448, 7000, 0xF8F8, 7000, -0x12B0708, 7000, -0x12B0708, 8400, 0xF8F8, 7000, 0xF8F8, 8400, -0x12B0708, 8400, -0x12B0BB8, 8400, 0xF8F8, 8400, 0xF448, 8400 };

s32 D_actor_136100_8013F2C4[12] = { 0x10000, 0x30002, 0, 0x50004, 0x70006, 1, 0x90008, 0xB000A, 2, 0xD000C, 0xF000E, 3 };

Actor136100MessageEntry D_actor_136100_8013F2F4[2] = {
    { 2005, { .call1 = func_actor_136100_801346EC } },
    { 2004, { .call0 = actorMsgPlaceInView } },
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
ActorTransform D_actor_136100_8013F304[2] = {
    { { -3500, 0, -3700, 0 }, { 0, -1024, 0, 0 } },
    { { -4800, 0, -4100, 0 }, { 0, -1024, 0, 0 } },
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
ActorTransform D_actor_136100_8013F334[2] = {
    { { -3200, 0, 6500, 0 }, { 0, 910, 0, 0 } },
    { { -2520, 0, 7690, 0 }, { 0, 910, 0, 0 } },
};

ActorTransform D_actor_136100_8013F364 = { { -3300, 0, 6400, 0 }, { 0, 910, 0, 0 } };

ActorTransform D_actor_136100_8013F37C = { { -5400, 0, -4100, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_136100_8013F394 = { { -6400, 0, -4000, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_136100_8013F3AC = { { -2600, 0, 7400, 0 }, { 0, 420, 0, 0 } };

ActorTransform D_actor_136100_8013F3C4 = { { -1300, 0, 7000, 0 }, { 0, 420, 0, 0 } };

ActorTransform D_actor_136100_8013F3DC = { { -1540, 0, 7600, 0 }, { 0, 420, 0, 0 } };

ActorTransform D_actor_136100_8013F3F4 = { { -5800, 0, -3200, 0 }, { 0, 1365, 0, 0 } };

ActorTransform D_actor_136100_8013F40C = { { -2210, 0, 8000, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_136100_8013F424 = { { -1000, 0, 7725, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_136100_8013F43C = { { -910, 0, 8130, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_136100_8013F454 = { { -5800, -400, -4400, 0 }, { 1843, 0, 0, 0 } };

EvsCommand D_actor_136100_8013F46C[33] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_8013467C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134924 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134898 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348C8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_801349B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013F784[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80133690 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_801349B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 18 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013F94C[17] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_8013467C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134924 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013FAE4[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136100_8013F37C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_8013467C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801347B8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013FC64[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013FD84[38] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 15 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_8013467C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134924 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134898 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348C8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_801349B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_80140114[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_8013379C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_801349B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 18 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_801402C4[23] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 16 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_8013467C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136100_8013F334[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134924 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_801404EC[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_8013379C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8014063C[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 17 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_136100_80140744[6] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_136100_80133BC8, { .model = &D_actor_136100_8013A500 } },
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_136100_801320E0, { .model = &D_actor_136100_8013A9D8 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_136100_80132284, { .model = &D_actor_136100_8013AFCC } },
    { { { TASK_BODY_NONE, 192 } }, screenFadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_136100_80134588, { .value = 0 } },
};

Task* D_actor_136100_8014078C = NULL;

static s32         func_actor_136100_80131EC4(Task* arg0);
static s32         func_actor_136100_80131FBC(Task* arg0);
static void        func_actor_136100_801323F8(Task* arg0);
static inline void func_actor_136100_SetAnim(Task* task, s16 anim);
static inline void func_actor_136100_ResetSlots(Task* task, s32 count);
static inline void func_actor_136100_PlayAnim(Task* task, u16 anim, s32 blend, s32 speed);
static void        func_actor_136100_80132BC0(Task* arg0);
static void        func_actor_136100_80132E78(Task* arg0);
static void        func_actor_136100_80133558(Task* arg0);
static s32         func_actor_136100_80133904(Task* task);
static void        func_actor_136100_80133A88(Task* task);
static inline s16  func_actor_136100_TakeStartCue(u16* evtId, u8* evtKind, u8* evtSub);
static inline void func_actor_136100_UpdateShadow(Task* arg0, VECTOR* vec);

static s32 func_actor_136100_80131EC4(Task* arg0)
{
    Actor136100Work*     work;
    Actor136100Work*     msgWork;
    AnimationPlayRequest rec;
    s16*                 sel;
    s32                  i;
    u16                  idx;
    s32                  weaponId;
    s32                  id;

    work = (Actor136100Work*)arg0->work;
    if (work->field_4B4 == NULL) {
    ret1:
        return 1;
    }
    if (taskMessageDispatch(work->field_4B4, 0x3ED, 0, 0) != 0) {
        return 0;
    }
    if ((u16)work->field_4DE < 0x2FU) {
        goto ret1;
    }

    i   = (u16)work->field_4DE - 0x2FU;
    sel = &D_actor_136100_8013F1EC[i];
    id  = *sel;
    if (id < 0) {
        goto ret1;
    }
    idx = (u16)*sel + 0x2FU;

    msgWork                  = (Actor136100Work*)arg0->work;
    weaponId                 = gPlayerStatus.weapon;
    id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.source.index         = id;
    msgWork->field_4DE       = idx;
    rec.animationId          = idx;
    rec.blend                = ANIMATION_BLEND_INTERPOLATE;
    rec.blendFrames          = 0xA;
    rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(msgWork->field_4B4, ANIMATION_MESSAGE_PLAY, &rec, 0);
    goto ret1;
}

static s32 func_actor_136100_80131FBC(Task* arg0)
{
    Actor136100Work* work;
    Actor136100Work* animWork;
    u16              i;
    u16              done;
    u16              anim;
    u16              id;

    work = (Actor136100Work*)arg0->work;
    for (i = 1; i < 20; i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    for (done = i = 1; i < 20; i++) {
        if (!(work->rig.slots[i].flags & ANIMATION_SLOT_SETTLED)) {
            goto fail;
        }
    }
check:
    if (done) {
        anim = D_actor_136100_8013F1FC[(u16)work->field_4E0];
        if (D_actor_136100_8013F1FC[(u16)work->field_4E0] >= 0) {
            id                  = anim;
            animWork            = (Actor136100Work*)arg0->work;
            animWork->field_4E0 = id;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = 1; i < 20; i++) {
                func_800B4114(&animWork->rig.anim, i, id, 0, 0xA);
            }
        }
        return 1;
    }
    return 0;
}

/// Spawn tick of the cutscene actor's second phase: allocates the 0x4F0-byte
/// `Actor136100Work` block, zeroes it and parks it in `Task::work`, then wires
/// the model object up -- `Tmd_AllocBuffers`, `TmdObject::flags` cleared, the
/// work block's light/colour matrices into `TmdObject::lightMtx` / `colorMtx`
/// and the animation-context task reparented under `D_actor_136100_8014078C`.
/// The texture page / CLUT row come from the placement record at the nested
/// area table's `field_0` list with resource-entry ID 0x6A (or the end record if that ID is absent), and this all runs even on the `Mem_Malloc` failure path,
/// which still advances the state after killing the task.
///
/// The dead `VECTOR` is read back through `task->extra` rather than the local
/// `obj`, which is what makes the original reload `Task::extra` for each of the
/// three coordinate reads (see `func_actor_136100_80132284`).
void func_actor_136100_801320E0(Task* task)
{
    Actor136100Work* work;
    VECTOR           vec;
    AreaPlacement*   place;
    u8               id;

    if (task->state == 0) {
        TmdObject* tmd   = task->extra.tmd;
        GfxCoord*  coord = tmd->coords;

        work       = Mem_Malloc(sizeof(Actor136100Work), 0);
        task->work = work;
        if (work == NULL) {
            taskKill(task);
        } else {
            Mem_Set(work, 0, sizeof(*work));
            coord->parent          = task->spawnArg2.pointer;
            task->extra.tmd->flags = 0;
            Tmd_AllocBuffers(tmd);
            tmd->lightMtx  = &work->field_474;
            tmd->colorMtx  = &work->field_494;
            task->msgTable = D_actor_136100_8013F2F4;
            taskReparent(D_actor_136100_8014078C, task);
        }
        place = Gp_GetNestedAreaRec(&gGameSession->location.loc)->field_0;
        id    = place->entryId;
        while (id != AREA_PLACEMENT_END) {
            if (id == 0x6A) {
                break;
            }
            place++;
            id = place->entryId;
        }
        Gp_SetTmdBytes(task->extra.tmd, place->texturePageOffset, place->clutRowOffset);
        task->state += 1;
    }
    {
        TmdObject* obj = task->extra.tmd;

        Gp_UpdateCoord(obj->coords);
        vec.vx = task->extra.tmd->coords->workm.t[0];
        vec.vy = task->extra.tmd->coords->workm.t[1];
        vec.vz = task->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &vec, 0, 3);
    }
}

void func_actor_136100_80132284(Task* arg0)
{
    Actor136100Work* work;
    VECTOR           vec;

    if (arg0->state == 0) {
        TmdObject* tmd   = arg0->extra.tmd;
        GfxCoord*  coord = tmd->coords;

        work       = Mem_Malloc(0x4F0, 0);
        arg0->work = work;
        if (work == NULL) {
            taskKill(arg0);
        } else {
            Mem_Set(work, 0, 0x4F0);
            coord->parent          = arg0->spawnArg2.pointer;
            arg0->extra.tmd->flags = 0;
            Tmd_AllocBuffers(tmd);
            tmd->lightMtx  = &work->field_474;
            tmd->colorMtx  = &work->field_494;
            arg0->msgTable = D_actor_136100_8013F2F4;
            taskReparent(D_actor_136100_8014078C, arg0);
        }
        arg0->state += 1;
        if (arg0->spawnArg1.value != 0) {
            GfxCoord* reset = arg0->extra.tmd->coords;

            Gfx_RotMatrixX(&reset->coord, 0x400, 1);
            reset->coord.t[1]   = 0xC8;
            reset->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    {
        TmdObject* obj = arg0->extra.tmd;

        Gp_UpdateCoord(obj->coords);
        vec.vx = arg0->extra.tmd->coords->workm.t[0];
        vec.vy = arg0->extra.tmd->coords->workm.t[1];
        vec.vz = arg0->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &vec, 0, 3);
    }
}

/// Builds and sends the player's equipped-weapon animation request.
///
/// `record` is accessed repeatedly and must be a side-effect-free request
/// lvalue. `anim` is evaluated twice and must be side-effect-free; `task` is
/// evaluated once. Dispatch consumes the request synchronously.
/// The bank selector depends on the current weapon and save character.
/// Per-expansion work pointers preserve the original call scheduling.
#define ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, anim, blendChoice, frames, record)                                            \
    {                                                                                                                                 \
        Actor136100Work* msgWork;                                                                                                     \
        s32              weaponId;                                                                                                    \
        s32              id;                                                                                                          \
                                                                                                                                      \
        msgWork                       = (Actor136100Work*)(task)->work;                                                               \
        weaponId                      = gPlayerStatus.weapon;                                                                         \
        id                            = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22; \
        (record).source.index         = id;                                                                                           \
        msgWork->field_4DE            = anim;                                                                                         \
        (record).animationId          = anim;                                                                                         \
        (record).blend                = (blendChoice);                                                                                \
        (record).blendFrames          = (frames);                                                                                     \
        (record).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                                            \
        TASK_MESSAGE_DISPATCH_POINTER(msgWork->field_4B4, ANIMATION_MESSAGE_PLAY, &(record), 0);                                      \
    }

/// Step the cutscene actor's `field_4C4` request.  Request 1 runs a three-step
/// sequence on `field_4C6` (send the 0x3E9 / 0x3F2 placement, wait for 0x3F0,
/// then wait six ticks on `field_4C8`) before sending the weapon record; 2..6
/// send it straight away with their own animation.  A finished request is
/// cleared.
static void func_actor_136100_801323F8(Task* arg0)
{
    Actor136100Work*     work = (Actor136100Work*)arg0->work;
    AnimationPlayRequest rec;

    if (gGameSession->eventState != 0) {
        func_actor_136100_80131EC4(arg0);
    }
    switch ((u16)work->field_4C4) {
        case 0:
            break;
        case 1:
            switch ((u16)work->field_4C6) {
                case 0:
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, 0x3E9, &D_actor_136100_8013F304[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, 0x3F2, &D_actor_136100_8013F304[1], 0);
                    work->field_4C8 = 0;
                    work->field_4C6++;
                    return;
                case 1:
                    if (taskMessageDispatch(work->field_4B4, 0x3F0, 0, 0) == 0) {
                        work->field_4C6++;
                    }
                    return;
                case 2:
                    if (++work->field_4C8 < 6) {
                        return;
                    }
                    ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x2F, 1, 5, rec);
                    break;
                default:
                    return;
            }
            break;
        case 2:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x32, 1, 0xA, rec);
            break;
        case 3:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x34, 1, 0xA, rec);
            break;
        case 4:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, 0x3E9, &D_actor_136100_8013F37C, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x2F, 0, 0, rec);
            break;
        case 5:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 1, 0xA, rec);
            break;
        case 6:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 0, 0, rec);
            break;
    }
    work->field_4C4 = 0;
}

/// Installs the linked actor's animation table and requests a blended clip.
///
/// `record` is accessed repeatedly and must be a side-effect-free request
/// lvalue. `anim` is evaluated twice and must be side-effect-free; `task` is
/// evaluated once. Dispatch consumes the request synchronously.
/// A null linked task suppresses evaluation of the clip and record arguments.
/// Per-expansion work pointers preserve the original call scheduling.
#define ACTOR_136100_PLAY_LINKED_ANIMATION(task, anim, blendChoice, frames, record)                               \
    {                                                                                                             \
        Actor136100Work* animWork = (Actor136100Work*)(task)->work;                                               \
                                                                                                                  \
        if (animWork->field_4C0 != NULL) {                                                                        \
            (record).source.sets          = D_actor_136100_8013F1D4;                                              \
            animWork->field_4E2           = anim;                                                                 \
            (record).animationId          = anim;                                                                 \
            (record).blend                = (blendChoice);                                                        \
            (record).blendFrames          = (frames);                                                             \
            (record).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                    \
            TASK_MESSAGE_DISPATCH_POINTER(animWork->field_4C0, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(record), 0); \
        }                                                                                                         \
    }

/// Record `id` as the work block's current animation (`field_4E0`).
#define _actor136100SetAnimId(work, id) \
    do {                                \
        (work)->field_4E0 = (id);       \
    } while (0)

/// Record `anim` as the current animation and start it on rig slots 1..19.
static inline void func_actor_136100_SetAnim(Task* task, s16 anim)
{
    Actor136100Work* work = (Actor136100Work*)task->work;
    s32              i;

    _actor136100SetAnimId(work, anim);
    for (i = 1; (u16)i < 0x14U; i++) {
        func_800B4114(&work->rig.anim, i & 0xFFFF, anim, 0, 0xA);
    }
}

/// Re-arm animation slots 1..19 with slot count `count`
/// (`func_actor_136100_801347B8`'s loop, reaching the work block through `task`).
static inline void func_actor_136100_ResetSlots(Task* task, s32 count)
{
    Actor136100Work* work = (Actor136100Work*)task->work;
    s32              i;

    work->field_4E0 = count;
    i               = 1;
    do {
        work->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)i, count);
        i++;
    } while ((u16)i < 0x14U);
}

static void func_actor_136100_80132748(Task* arg0)
{
    Actor136100Work*     work = (Actor136100Work*)arg0->work;
    AnimationPlayRequest rec;

    func_actor_136100_80131FBC(arg0);
    switch ((u16)work->field_4CC) {
        case 0:
            break;
        case 1:
            switch ((u16)work->field_4CE) {
                case 0:
                    work->field_4D0 = 0;
                    func_actor_136100_SetAnim(arg0, 2);
                    work->field_4CE++;
                    return;
                case 1:
                    if (++work->field_4D0 < 0x3D) {
                        return;
                    }
                    ACTOR_136100_PLAY_LINKED_ANIMATION(arg0, 3, 1, 0xA, rec);
                    break;
                default:
                    return;
            }
            break;
        case 2:
            switch ((u16)work->field_4CE) {
                case 0:
                    work->field_4D0 = 0;
                    func_actor_136100_SetAnim(arg0, 5);
                    work->field_4CE++;
                    return;
                case 1:
                    if (++work->field_4D0 == 0x11) {
                        SndEvt_EnqueueType6(0x5302000E, 0, 0);
                    }
                    if (work->field_4D0 < 0x15) {
                        return;
                    }
                    {
                        Actor136100Work* msgWork;
                        s32              weaponId;
                        s32              id;

                        msgWork                  = arg0->work;
                        weaponId                 = gPlayerStatus.weapon;
                        id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                        rec.source.index         = id;
                        msgWork->field_4DE       = 0x30;
                        rec.animationId          = 0x30;
                        rec.blend                = ANIMATION_BLEND_INTERPOLATE;
                        rec.blendFrames          = 0xA;
                        rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;

                        TASK_MESSAGE_DISPATCH_POINTER(msgWork->field_4B4, ANIMATION_MESSAGE_PLAY, &rec, 0);
                    }
                    ACTOR_136100_PLAY_LINKED_ANIMATION(arg0, 4, 1, 0xA, rec);
                    break;
                default:
                    return;
            }
            break;
        case 3:
            func_actor_136100_SetAnim(arg0, 7);
            break;
        case 4:
            func_actor_136100_SetAnim(arg0, 9);
            break;
        case 5:
            switch ((u16)work->field_4CE) {
                case 0:
                    work->field_4D0 = 0;
                    func_actor_136100_SetAnim(arg0, 0xA);
                    work->field_4CE++;
                    return;
                case 1:
                    if (++work->field_4D0 < 0x50) {
                        return;
                    }
                    func_actor_136100_SetAnim(arg0, 0xB);
                    break;
                default:
                    return;
            }
            break;
        case 6: {
            Actor136100Work* animWork = (Actor136100Work*)arg0->work;
            s32              i;

            animWork->field_4E0 = 1;
            for (i = 1; (u16)i < 0x14U; i++) {
                animWork->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&animWork->rig.anim, (u16)i, 1);
            }
        } break;
    }
    work->field_4CC = 0;
}

/// Play `anim` on the `field_4C0` task (message 0x3F4) and record it as the
/// current `field_4E2` chain entry; does nothing while that task is unset.
static inline void func_actor_136100_PlayAnim(Task* task, u16 anim, s32 blend, s32 speed)
{
    Actor136100Work*     work = (Actor136100Work*)task->work;
    AnimationPlayRequest msg;

    if (work->field_4C0 != NULL) {
        msg.source.sets          = D_actor_136100_8013F1D4;
        work->field_4E2          = anim;
        msg.animationId          = anim;
        msg.blend                = blend;
        msg.blendFrames          = speed;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->field_4C0, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
    }
}

/// Advance the animation chain like `func_actor_136100_80133558`, then run the
/// `field_4D4` request: 1 and 3 play a fixed animation, 2 steps the
/// `field_4D6` sequence -- three sound-and-animation shots every 15 ticks
/// (`field_4DA` countdown, `field_4D8` shot count) before a final animation.
/// Every request that finishes clears `field_4D4`.
static void func_actor_136100_80132BC0(Task* arg0)
{
    Actor136100Work* work;
    s16              anim;

    work = (Actor136100Work*)arg0->work;
    if (work->field_4C0 != NULL && taskMessageDispatch(work->field_4C0, 0x3ED, 0, 0) == 0) {
        anim = D_actor_136100_8013F218[work->field_4E2];
        if (anim >= 0) {
            func_actor_136100_PlayAnim(arg0, anim, 1, 0xA);
        }
    }
    switch ((u16)work->field_4D4) {
        case 0:
            break;
        case 1:
            func_actor_136100_PlayAnim(arg0, 1, 0, 0);
            break;
        case 2:
            switch ((u16)work->field_4D6) {
                case 0:
                    work->field_4D8 = 0;
                    work->field_4DA = 0;
                    work->field_4D6++;
                    return;
                case 1:
                    if (--work->field_4DA > 0) {
                        return;
                    }
                    if (work->field_4D8 >= 3) {
                        func_actor_136100_PlayAnim(arg0, 1, 1, 0xA);
                        break;
                    }
                    SndEvt_EnqueueType6(0x40720009, 0, 0);
                    func_actor_136100_PlayAnim(arg0, 2, 1, 0xA);
                    work->field_4DA = 0xF;
                    work->field_4D8++;
                    return;
                default:
                    return;
            }
            break;
        case 3:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_4C0, 0x3E9, &D_actor_136100_8013F3F4, 0);
            func_actor_136100_PlayAnim(arg0, 0, 0, 0);
            break;
    }
    work->field_4D4 = 0;
}

/// Step the cutscene actor's second-phase `field_4C4` request, the sibling of
/// `func_actor_136100_801323F8`: request 2 runs the three-step `field_4C6`
/// sequence (0x3F3 / 0x3E9 / 0x3F2 placement, wait for 0x3F0, six ticks on
/// `field_4C8`), the others send the weapon record straight away.  A finished
/// request is cleared.
static void func_actor_136100_80132E78(Task* arg0)
{
    Actor136100Work*     work = (Actor136100Work*)arg0->work;
    AnimationPlayRequest rec;

    if (gGameSession->eventState != 0) {
        func_actor_136100_80131EC4(arg0);
    }
    switch ((u16)work->field_4C4) {
        case 0:
            break;
        case 1:
            taskMessageDispatch(work->field_4B4, 0x3F3, 0, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 0, 0, rec);
            break;
        case 2:
            switch ((u16)work->field_4C6) {
                case 0:
                    taskMessageDispatch(work->field_4B4, 0x3F3, 1, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, 0x3E9, &D_actor_136100_8013F334[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, 0x3F2, &D_actor_136100_8013F334[1], 0);
                    work->field_4C8 = 0;
                    work->field_4C6++;
                    return;
                case 1:
                    if (taskMessageDispatch(work->field_4B4, 0x3F0, 0, 0) == 0) {
                        work->field_4C6++;
                    }
                    return;
                case 2:
                    if (++work->field_4C8 < 6) {
                        return;
                    }
                    ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x2F, 1, 5, rec);
                    break;
                default:
                    return;
            }
            break;
        case 3:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x31, 1, 0xA, rec);
            break;
        case 4:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x35, 1, 0xA, rec);
            break;
        case 5:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, 0x3E9, &D_actor_136100_8013F364, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 0, 0, rec);
            break;
        case 6:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x2F, 0, 0, rec);
            break;
        case 7:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 1, 0xA, rec);
            break;
    }
    work->field_4C4 = 0;
}

static void func_actor_136100_80133238(Task* arg0)
{
    Actor136100Work* work;
    GfxCoord*        coords;

    work = (Actor136100Work*)arg0->work;
    func_actor_136100_80131FBC(arg0);
    switch ((u16)work->field_4CC) {
        case 0:
            break;
        case 1:
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F3C4, 0);
            break;
        case 2:
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F3DC, 0);
            break;
        case 3:
            func_actor_136100_SetAnim(arg0, 4);
            break;
        case 4:
            func_actor_136100_ResetSlots(arg0, 3);
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F3AC, 0);
            break;
        case 5:
            switch ((u16)work->field_4CE) {
                case 0:
                    func_actor_136100_SetAnim(arg0, 6);
                    work->field_4D0 = 0;
                    work->field_4CE++;
                    return;
                case 1:
                    if (++work->field_4D0 == 0xF) {
                        SndEvt_EnqueueType6(0x5302000F, 0, 0);
                        work->field_4CC = 0;
                    }
                    return;
            }
            return;
        case 6:
            switch ((u16)work->field_4C6) {
                case 0:
                    TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F3DC, 0);
                    work->field_4EA = 0x1000;
                    work->field_4C6++;
                    return;
                case 1:
                    gfxRotMatrixY(&arg0->extra.tmd->coords[4].coord, work->field_4EA, 1);
                    if (work->field_4EA >= 0xD56) {
                        work->field_4EA -= 0x40;
                    }
                    return;
            }
            return;
        case 7:
            coords = arg0->extra.tmd->coords;
            if ((work->field_4EA += 0x40) > 0x1000) {
                work->field_4EA = 0;
                work->field_4CC = 0;
            }
            gfxRotMatrixY(&coords[4].coord, work->field_4EA, 1);
            return;
        case 8:
            func_actor_136100_ResetSlots(arg0, 3);
            break;
    }
    work->field_4CC = 0;
}

/// Advance the cutscene actor's animation chain and send its pending placement.
///
/// Once the `field_4C0` task reports its current animation done (message
/// 0x3ED), steps `field_4E2` to the next entry of the `D_actor_136100_8013F218`
/// chain (negative ends it) and plays it with 0x3F4.  Then sends the 0x3E9
/// placement selected by `field_4D4` (1..3) and clears the request.
static void func_actor_136100_80133558(Task* arg0)
{
    Actor136100Work*     work;
    Actor136100Work*     msgWork;
    AnimationPlayRequest msg;
    u16                  anim;

    work = (Actor136100Work*)arg0->work;
    if (work->field_4C0 != NULL && taskMessageDispatch(work->field_4C0, 0x3ED, 0, 0) == 0) {
        anim = D_actor_136100_8013F218[work->field_4E2];
        if (D_actor_136100_8013F218[work->field_4E2] >= 0) {
            msgWork = (Actor136100Work*)arg0->work;
            if (msgWork->field_4C0 != NULL) {
                msg.source.sets          = D_actor_136100_8013F1D4;
                msgWork->field_4E2       = anim;
                msg.animationId          = anim;
                msg.blend                = ANIMATION_BLEND_INTERPOLATE;
                msg.blendFrames          = 0xA;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(msgWork->field_4C0, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
        }
    }
    switch ((u16)work->field_4D4) {
        case 0:
            break;
        case 1:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_4C0, 0x3E9, &D_actor_136100_8013F424, 0);
            break;
        case 2:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_4C0, 0x3E9, &D_actor_136100_8013F43C, 0);
            break;
        case 3:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_4C0, 0x3E9, &D_actor_136100_8013F40C, 0);
            break;
    }
    work->field_4D4 = 0;
}

/// Reset the cutscene actor's animation state and re-send the weapon record.
///
/// Clears the first two value/countdown pairs, re-arms all nineteen animation
/// slots through `animationResetSlot` with the work block's slot count at 1, then
/// sends slot 3 the 0x3E9 placement and the 0x3E8 weapon record
/// (`AnimationPlayRequest`) built from the equip-slot addend (`gPlayerStatus.weapon`), the pair
/// `func_actor_136100_8013467C` sends on its own.  `field_4DE` is armed on the
/// way past.
///
/// Three separate `task->work` loads are what the original reaches the block
/// with -- the stores to `field_4C4` / `field_4CC` invalidate the first in cse,
/// and the first is still live for the 0x3E9 send after the loop.  The dead
/// `SVECTOR` is not read; it reserves the 8-byte local the frame has between
/// the outgoing-arg area and `rec` (see `func_actor_136100_801347B8`).
void func_actor_136100_80133690(void)
{
    Task*                task;
    Actor136100Work*     work;
    Actor136100Work*     animWork;
    Actor136100Work*     msgWork;
    SVECTOR              unused;
    AnimationPlayRequest rec;
    s32                  i;
    s32                  weaponId;
    s32                  id;

    task            = D_actor_136100_8014078C;
    work            = (Actor136100Work*)task->work;
    work->field_4C4 = 0;
    work->field_4CC = 0;

    animWork            = (Actor136100Work*)task->work;
    animWork->field_4E0 = 1;
    i                   = 1;
    do {
        animWork->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&animWork->rig.anim, (u16)i, 1);
        i++;
    } while ((u16)i < 0x14U);

    TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, 0x3E9, &D_actor_136100_8013F304[1], 0);

    msgWork                  = (Actor136100Work*)task->work;
    weaponId                 = gPlayerStatus.weapon;
    id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.source.index         = id;
    msgWork->field_4DE       = 1;
    rec.animationId          = 1;
    rec.blend                = ANIMATION_BLEND_RESET;
    rec.blendFrames          = 0;
    rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(msgWork->field_4B4, ANIMATION_MESSAGE_PLAY, &rec, 0);
}

/// Second half of the cutscene actor's re-arm: clears the first two
/// value/countdown pairs, sends the 0x7D4 cue to the host task, re-arms all
/// nineteen animation slots with the work block's slot count at 3, then re-sends
/// the two placement cues and the 0x3E8 weapon record (`AnimationPlayRequest`) built from the
/// equip-slot addend (`gPlayerStatus.weapon`).  `arg0 == 1` additionally resets the
/// fourth bone's rotation to zero.
///
/// Two `task->work` loads reach the block: the stores to `field_4C4` /
/// `field_4CC` invalidate the first in cse, and it is still live for the 0x3E9
/// and 0x3E9/`field_4C0` sends after the loop.  The dead `SVECTOR` is not read;
/// it reserves the 8-byte local the frame has between the outgoing-arg area and
/// `rec` (see `func_actor_136100_80133690`).
void func_actor_136100_8013379C(s32 arg0)
{
    Task*                task;
    Actor136100Work*     work;
    Actor136100Work*     animWork;
    Actor136100Work*     msgWork;
    SVECTOR              unused;
    AnimationPlayRequest rec;
    s32                  i;
    s32                  weaponId;
    s32                  id;

    task            = D_actor_136100_8014078C;
    work            = (Actor136100Work*)task->work;
    work->field_4C4 = 0;
    work->field_4CC = 0;

    TASK_MESSAGE_DISPATCH_POINTER(task, 0x7D4, &D_actor_136100_8013F3AC, 0);

    animWork            = (Actor136100Work*)task->work;
    animWork->field_4E0 = 3;
    i                   = 1;
    do {
        animWork->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&animWork->rig.anim, (u16)i, 3);
        i++;
    } while ((u16)i < 0x14U);

    TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, 0x3E9, D_actor_136100_8013F334, 0);

    msgWork                  = (Actor136100Work*)task->work;
    weaponId                 = gPlayerStatus.weapon;
    id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.source.index         = id;
    msgWork->field_4DE       = 1;
    rec.animationId          = 1;
    rec.blend                = ANIMATION_BLEND_RESET;
    rec.blendFrames          = 0;
    rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(msgWork->field_4B4, ANIMATION_MESSAGE_PLAY, &rec, 0);

    TASK_MESSAGE_DISPATCH_POINTER(work->field_4C0, 0x3E9, &D_actor_136100_8013F40C, 0);

    if (arg0 == 1) {
        gfxRotMatrixY(&task->extra.tmd->coords[4].coord, 0, 1);
    }
}

/// Cue handler: when the pending `Gp_TakePendingObj4C` event is a positive
/// id 5 (and `gPlayerStatus.interactionPressed` is set), kind 0x12 in phase 0 or kind 0x13 in phase 1
/// notifies via `func_actor_136100_80134A18` and plays the phase's first cue on
/// the first hit (`func_800E8634`, advancing `field_4DC`) or its repeat cue after.
/// `ready` must be `s16`: as `s32` the `!= 0` store fuses into the callee-saved
/// home and the join copy into `$v0` disappears.
static s32 func_actor_136100_80133904(Task* task)
{
    Actor136100Work* work = (Actor136100Work*)task->work;
    u16              evtId;
    u8               evtKind;
    u8               evtSub;
    s16              ready;

    ready = 0;
    if (Gp_TakePendingObj4C(&evtId, &evtKind, &evtSub) != 0) {
        if (!((s16)evtId & WORLD_COLLISION_TRIGGER_AUTOMATIC)) {
            if ((evtId & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
                ready = gPlayerStatus.interactionPressed != 0;
            }
        }
    }
    if (ready == 0 || Gp_StateC08.field_A == 1) {
        return 0;
    }
    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
        return 0;
    }
    if ((s8)evtKind == 0x12 && work->field_4E4 == 0) {
        func_actor_136100_80134A18(task);
        if (work->field_4DC == 0) {
            func_800E8634(D_actor_136100_8013F94C, 0, D_actor_136100_8013FAE4);
            work->field_4DC++;
        } else {
            func_800E8614(D_actor_136100_8013FC64, 0);
        }
        return 1;
    }
    if ((s8)evtKind == 0x13 && work->field_4E4 == 1) {
        func_actor_136100_80134A18(task);
        if (work->field_4DC == 0) {
            func_800E8634(D_actor_136100_801402C4, 0, D_actor_136100_801404EC);
            work->field_4DC++;
        } else {
            func_800E8614(D_actor_136100_8014063C, 0);
        }
        return 1;
    }
    return 0;
}

/// Initialize the cutscene actor's model and animations.
///
/// Uses the area placement for resource-entry 0x6A, or the end record when
/// that entry is absent. Allocation failure kills `task`.
static void func_actor_136100_80133A88(Task* task)
{
    enum { TEXTURE_RESOURCE_ENTRY_ID = 0x6A };

    Actor136100Work* work;
    Actor136100Work* allocatedWork;
    TmdObject*       tmd;
    GfxCoord*        coord;
    AreaPlacement*   place;
    u8               entryId;

    tmd           = task->extra.tmd;
    coord         = tmd->coords;
    allocatedWork = Mem_Malloc(sizeof(Actor136100Work), 0);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    work = allocatedWork;
    Mem_Set(work, 0, sizeof(*work));
    work->field_4B4         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    D_actor_136100_8014078C = task;
    coord->parent           = &gGfxViewCoord;
    Tmd_AllocBuffers(tmd);
    tmd->lightMtx = &work->field_474;
    tmd->colorMtx = &work->field_494;
    tmd->flags   &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
    place         = Gp_GetNestedAreaRec(&gGameSession->location.loc)->field_0;
    entryId       = place->entryId;
    while (entryId != AREA_PLACEMENT_END) {
        if (entryId == TEXTURE_RESOURCE_ENTRY_ID) {
            break;
        }
        place++;
        entryId = place->entryId;
    }
    Gp_SetTmdBytes(tmd, place->texturePageOffset, place->clutRowOffset);
    func_800B3F84(&work->rig.anim, D_actor_136100_8013F1A0, tmd, work->rig.poses, work->rig.slots);
    task->msgTable = D_actor_136100_8013F2F4;
}

/// Classify the pending `Gp_TakePendingObj4C` event for the cutscene's start
/// cue: id 5 with kind 0x10 is 1 (phase 0), kind 0x11 is 2 (phase 1), anything
/// else 0.  The `s16` return is what keeps the result in its own pseudo, copied
/// into the caller's compare register after the join.
static inline s16 func_actor_136100_TakeStartCue(u16* evtId, u8* evtKind, u8* evtSub)
{
    if (Gp_TakePendingObj4C(evtId, evtKind, evtSub) != 0) {
        if ((*evtId & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
            if ((s8)*evtKind == 0x10) {
                return 1;
            }
            if ((s8)*evtKind == 0x11) {
                return 2;
            }
        }
    }
    return 0;
}

/// Installs the linked actor's animation table and resets to a clip.
///
/// `record` is accessed repeatedly and must be a side-effect-free request
/// lvalue. `anim` is evaluated twice and must be side-effect-free; `task` is
/// evaluated once. Dispatch consumes the request synchronously.
/// A null linked task suppresses evaluation of the clip and record arguments.
/// Per-expansion work pointers preserve the original call scheduling.
#define ACTOR_136100_RESET_LINKED_ANIMATION(task, anim, record)                                                   \
    {                                                                                                             \
        Actor136100Work* animWork = (Actor136100Work*)(task)->work;                                               \
                                                                                                                  \
        if (animWork->field_4C0 != NULL) {                                                                        \
            (record).source.sets          = D_actor_136100_8013F1D4;                                              \
            animWork->field_4E2           = anim;                                                                 \
            (record).animationId          = anim;                                                                 \
            (record).blend                = ANIMATION_BLEND_RESET;                                                \
            (record).blendFrames          = 0;                                                                    \
            (record).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                    \
            TASK_MESSAGE_DISPATCH_POINTER(animWork->field_4C0, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(record), 0); \
        }                                                                                                         \
    }

/// Copies seven set addresses into the player's writable animation-bank extension.
///
/// `record` must be a side-effect-free `GpCopyArg` lvalue. The table is
/// null-terminated; the receiver copies only the preceding entries.
/// Per-expansion work pointers preserve the original call scheduling.
#define ACTOR_136100_COPY_PLAYER_ANIMATION_SETS(task, record)                                                   \
    {                                                                                                           \
        Actor136100Work* msgWork = (Actor136100Work*)(task)->work;                                              \
        s32              n;                                                                                     \
                                                                                                                \
        n = 0;                                                                                                  \
        while (D_actor_136100_8013F180[n & 0xFFFF] != 0) {                                                      \
            n += 1;                                                                                             \
        }                                                                                                       \
        (record).source.sets = &D_actor_136100_8013F180[0];                                                     \
        (record).count       = n & 0xFFFF;                                                                      \
        TASK_MESSAGE_DISPATCH_POINTER(msgWork->field_4B4, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &(record), 0); \
    }

/// Refresh the shadow coordinate and hand its translation to `func_800D7A9C`.
/// `vec` is a parameter rather than a local so the caller's buffer address
/// stays out of the CSE class of the `Gp_DrawFloorQuad` argument that follows.
static inline void func_actor_136100_UpdateShadow(Task* arg0, VECTOR* vec)
{
    TmdObject* obj = arg0->extra.tmd;

    Gp_UpdateCoord(&obj->coords[1]);
    vec->vx = arg0->extra.tmd->coords[1].workm.t[0];
    vec->vy = arg0->extra.tmd->coords[1].workm.t[1];
    vec->vz = arg0->extra.tmd->coords[1].workm.t[2];
    func_800D7A9C(obj, vec, 0, 3);
}

/// Main tick of the cutscene actor.  State 0 allocates the work block, picks
/// the phase (`field_4E4`, from game flag 0x73) and spawns the two helper tasks;
/// state 1 sends the phase's opening cues; state 2 waits for the matching start
/// cue, sends the weapon record and table and sets game flag 0x7C; states 3..5
/// wait on `gGameSession->eventState` and `func_actor_136100_80133904`.  Every
/// state then runs the phase's three per-frame handlers and redraws the shadow.
///
/// Animation, table-copy and geometry values use separate views of the same
/// temporary storage; each is consumed before the next view is written.
void func_actor_136100_80133BC8(Task* arg0)
{
    Actor136100Work* work = (Actor136100Work*)arg0->work;
    SVECTOR          unused;
    union {
        AnimationPlayRequest animation;
        GpCopyArg            copy;
        VECTOR               shadowPosition;
        SVECTOR              floorOffset;
    } message;
    s32 cue;
    u16 evtId;
    u8  evtKind;
    u8  evtSub;
    u16 evtId2;
    u8  evtKind2;
    u8  evtSub2;

    switch (arg0->state) {
        case 0:
            if (GameFlag_GetNibble(0x7C) != 0) {
                taskKill(arg0);
                return;
            }
            func_actor_136100_80133A88(arg0);
            work            = (Actor136100Work*)arg0->work;
            work->field_4E4 = GameFlag_GetNibble(0x73) == 0;
            work->field_4B8 = Task_SpawnFromTable(D_actor_136100_80140744, 2, 0,
                                                  arg0->extra.tmd->coords + 4);
            if (work->field_4E4 == 0) {
                func_actor_136100_ResetSlots(arg0, 1);
                work->field_4BC = Task_SpawnFromTable(D_actor_136100_80140744, 3, 0, &gGfxViewCoord);
            } else {
                func_actor_136100_ResetSlots(arg0, 3);
                work->field_4BC = Task_SpawnFromTable(D_actor_136100_80140744, 3, 1,
                                                      arg0->extra.tmd->coords + 8);
                Mem_CopyUnaligned(&D_actor_136100_8013F224, D_dryfield_night_main_street_801833F4, 0x20);
                Mem_CopyUnaligned(&D_actor_136100_8013F2C4, D_dryfield_night_main_street_80183ACC, sizeof(D_actor_136100_8013F2C4));
                Mem_CopyUnaligned(&D_actor_136100_8013F244, D_dryfield_night_main_street_801834AC, 0x80);
            }
            work->field_4C0 = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
            arg0->state++;
            break;
        case 1:
            if (work->field_4E4 == 0) {
                func_800E3FAC(0xA2, 0x19);
                taskMessageDispatch(arg0, 0x7D5, 1, 0);
                taskMessageDispatch(work->field_4B8, 0x7D5, 1, 0);
                taskMessageDispatch(work->field_4BC, 0x7D5, 1, 0);
                TASK_MESSAGE_DISPATCH_POINTER(work->field_4BC, 0x7D4, &D_actor_136100_8013F454, 0);
                if (work->field_4C0 != NULL) {
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_4C0, 0x3E9, &D_actor_136100_8013F3F4, 0);
                }
                TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F394, 0);
                func_actor_136100_ResetSlots(arg0, 1);
                ACTOR_136100_RESET_LINKED_ANIMATION(arg0, 1, message.animation);
            } else {
                func_800E3FAC(0xA2, 0x1A);
                taskMessageDispatch(arg0, 0x7D5, 1, 0);
                taskMessageDispatch(work->field_4B8, 0x7D5, 1, 0);
                taskMessageDispatch(work->field_4BC, 0x7D5, 1, 0);
                if (work->field_4C0 != NULL) {
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_4C0, 0x3E9, &D_actor_136100_8013F40C, 0);
                }
                TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F3AC, 0);
                func_actor_136100_ResetSlots(arg0, 3);
                ACTOR_136100_RESET_LINKED_ANIMATION(arg0, 5, message.animation);
            }
            arg0->state++;
            break;
        case 2:
            cue = func_actor_136100_TakeStartCue(&evtId, &evtKind, &evtSub);
            if (cue == 1 && work->field_4E4 == 0) {
                if (Gp_StateC08.field_A == 1) {
                    return;
                }
                if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                    return;
                }
                ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 1, 0xA, message.animation);
                func_800E8634(D_actor_136100_8013F46C, 0, D_actor_136100_8013F784);
                Gp_UnlinkObj4A(0, &D_dryfield_night_main_street_8018824C[8]);
                ACTOR_136100_COPY_PLAYER_ANIMATION_SETS(arg0, message.copy);
                GameFlag_SetNibble(0x7C, 1);
                arg0->state++;
                break;
            }
            if (func_actor_136100_TakeStartCue(&evtId2, &evtKind2, &evtSub2) == 2 && work->field_4E4 == 1) {
                if (Gp_StateC08.field_A == 1) {
                    return;
                }
                if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                    return;
                }
                ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 1, 0xA, message.animation);
                func_800E8634(D_actor_136100_8013FD84, 0, D_actor_136100_80140114);
                Gp_UnlinkObj4A(0, &D_dryfield_night_main_street_8018824C[9]);
                ACTOR_136100_COPY_PLAYER_ANIMATION_SETS(arg0, message.copy);
                GameFlag_SetNibble(0x7C, 1);
                arg0->state++;
            }
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                arg0->state++;
            }
            break;
        case 4:
            if ((s16)func_actor_136100_80133904(arg0) != 0) {
                arg0->state++;
            }
            break;
        case 5:
            if (gGameSession->eventState == 0) {
                arg0->state--;
            }
            break;
    }
    if (work->field_4E4 == 0) {
        func_actor_136100_801323F8(arg0);
        func_actor_136100_80132748(arg0);
        func_actor_136100_80132BC0(arg0);
    } else {
        func_actor_136100_80132E78(arg0);
        func_actor_136100_80133238(arg0);
        func_actor_136100_80133558(arg0);
    }
    func_actor_136100_UpdateShadow(arg0, &message.shadowPosition);
    message.floorOffset.vx = 0;
    message.floorOffset.vy = 0x380;
    message.floorOffset.vz = 0;
    Gp_DrawFloorQuad(&arg0->extra.tmd->coords[1], 0x300, &message.floorOffset);
}

#include "../../shared/screen_fade_in.inc.c"

/// Display-fade task: on its first tick it allocates the 8-byte `r`/`g`/`b`
/// block, then every frame draws the fade overlay and steps all three channels
/// up by `spawnArg1`.  The fade ends once `r` reaches 0x100, at which point the
/// task blanks the display and kills itself; `D_actor_136100_8013F17C` makes it
/// kill itself immediately instead (the cutscene wants the display back).
void func_actor_136100_80134588(Task* arg0)
{
    OverlayFadeWork* fade;
    OverlayFadeWork* alloc;

    fade = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r + (u16)arg0->spawnArg1.value);
            fade->g = (s16)((u16)fade->g + (u16)arg0->spawnArg1.value);
            fade->b = (s16)((u16)fade->b + (u16)arg0->spawnArg1.value);
            if (D_actor_136100_8013F17C != 0) {
                taskKill(arg0);
                return;
            }
            if (fade->r < 0x100) {
                return;
            }
            SetDispMask(0);
            taskKill(arg0);
            break;
    }
}

void func_actor_136100_8013467C(void)
{
    AnimationPlayRequest rec;
    s32                  weaponId;
    s32                  id;

    weaponId                 = gPlayerStatus.weapon;
    id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.source.index         = id;
    rec.animationId          = 1;
    rec.blend                = ANIMATION_BLEND_RESET;
    rec.blendFrames          = 0;
    rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &rec, 0);
}

/// Shows the task's model when `arg2` is non-zero and hides it (bit 0x80 of
/// its `TmdObject` flags) otherwise; `arg1` is unused.
void func_actor_136100_801346EC(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (arg2 != 0) {
        obj->flags = obj->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    obj->flags = obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

#include "../../shared/actor_messages_place_in_view.inc.c"

void func_actor_136100_801347B8(void)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->work;
    SVECTOR          unused;
    s32              i;

    work->field_4E0 = 1;
    i               = 1;
    do {
        work->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)i, 1);
        i++;
    } while ((u16)i < 0x14U);
}

void func_actor_136100_80134838(s16 arg0)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->work;

    work->field_4C4 = arg0;
    work->field_4C6 = 0;
}

void func_actor_136100_80134858(s16 arg0)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->work;

    work->field_4CC = arg0;
    work->field_4CE = 0;
}

void func_actor_136100_80134878(s16 arg0)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->work;

    work->field_4D4 = arg0;
    work->field_4D6 = 0;
}

/// Starts the fade-in (entry 4 of the actor's task table).
void func_actor_136100_80134898(void)
{
    Task_SpawnFromTable(D_actor_136100_80140744, 4, 9, 0);
}

void func_actor_136100_801348C8(void)
{
    Task_SpawnFromTable(D_actor_136100_80140744, 5, 9, 0);
}

void func_actor_136100_801348F8(void)
{
    D_actor_136100_8013F17C = 1;
    SetDispMask(1);
}

void func_actor_136100_80134924(void)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->work;

    if (work->field_4EC == 0) {
        work->field_4EC = 1;
        Gp_KillPlayerEffs();
    }
}

void func_actor_136100_80134964(void)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->work;

    if (work->field_4EC != 0) {
        Gp_SpawnWeaponEff();
        work->field_4EC = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

void func_actor_136100_801349B4(s32 arg0)
{
    GameFlag_SetNibble(0x46, 0);
    GameFlag_SetNibble(0x4C, 3);
    if (arg0 == 0) {
        GameFlag_SetNibble(0x4B, 6);
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 8;
        GameFlag_SetNibble(0x4B, 0);
    }
}

/// Reports the live entries of the actor's pointer table to the slot-3 task:
/// counts the leading non-null words of `D_actor_136100_8013F180` and hands
/// the table and that count to message 0x3F7.
static void func_actor_136100_80134A18(Task* arg0)
{
    Actor136100Work* work = (Actor136100Work*)arg0->work;
    GpCopyArg        msg;
    s32              n;

    n = 0;
    while (D_actor_136100_8013F180[n & 0xFFFF] != 0) {
        n += 1;
    }
    msg.source.sets = &D_actor_136100_8013F180[0];
    msg.count       = n & 0xFFFF;
    TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &msg, 0);
}
