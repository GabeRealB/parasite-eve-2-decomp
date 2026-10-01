#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/scripted_walk.h"
#include "../../shared/actor_messages.h"

// The engine copies words across the exported animation bank and its
// following argument records. Both views cover the complete backing object.
typedef union {
    struct {
        AnimationSet*        sets[18];
        AnimationPlayRequest arguments[3];
    } data;
    s32 words[33];
} Actor146300AnimCopy7898;
STATIC_ASSERT_SIZEOF(Actor146300AnimCopy7898, 132);

extern Actor146300AnimCopy7898 D_actor_146300_80137898;

/// Work block of the overlay's actor, allocated zeroed by its spawn routine
/// and kept both in `gScriptedWalkWork` and at `Task::work`; every other
/// function in the overlay reaches it through the global. `light` and `color`
/// are the matrices the spawn routine hands the model, and `rig` and `st` its
/// animation rig and state.
typedef struct Actor146300Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
} Actor146300Work;
STATIC_ASSERT_SIZEOF(Actor146300Work, 0x4EC);

/// The work block above, published by the task handler
/// `func_actor_146300_801326CC` and by the spawn routine.
extern Actor146300Work* gScriptedWalkWork;

/// The actor's own task, published by the spawn routine: the 0x7D3 handler
/// runs the per-frame update on it, and the 0x7D5 handler and the companion's
/// handler `func_actor_146300_80132B1C` reach the actor's model through its
/// `extra`.
extern Task* gActorSelfTask;

/// Reset argument `scriptedWalkBlendAnim` forwards: the 0x7D3 handler
/// latches the preset's `field_C` here.
extern s16 gScriptedWalkBlendFrames;

/// The companion task the spawn routine starts from
/// `D_actor_146300_801427C8`; its `extra` is the model whose texture page and
/// CLUT row come out of the area record, and the actor's own task is reparented
/// under it.
extern Task* gActorHelperTask;

/// Spawn table of the actor's two tasks: index 0 runs
/// `func_actor_146300_801326CC`, index 1 the companion's
/// `func_actor_146300_80132B1C`, which the spawn routine starts.
extern TaskDesc D_actor_146300_801427C8[];

/// Animation stream the spawn routine binds into the work block's animation
/// context with `func_800B3F84`.
extern u8 D_actor_146300_801427E0[];

/// Message handler table the spawn routine publishes as `Task::msgTable`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*);
        s32 (*call2)(Task*, s32, ActorTransform*);
        s32 (*call3)(Task*, s32, s32);
    } handler;
} Actor146300MsgEntry;
STATIC_ASSERT_SIZEOF(Actor146300MsgEntry, 8);

extern Actor146300MsgEntry D_actor_146300_801427A0[];

extern AnimationPlayRequest D_actor_146300_80137AAC;
extern AnimationPlayRequest D_actor_146300_80137B10;
extern AnimationPlayRequest D_actor_146300_80137B38;
extern AnimationPlayRequest D_actor_146300_80137B60;
extern ActorTransform       D_actor_146300_80137C10;
extern EvsCommand           D_actor_146300_801386C0[];
extern EvsCommand           D_actor_146300_80138810[];
extern EvsCommand           D_actor_146300_801388D0[];
extern EvsCommand           D_actor_146300_80138A38[];
extern EvsCommand           D_actor_146300_80138AC8[];
extern s32                  D_actor_146300_80142824;

static void func_actor_146300_80132728(Enemy* enemy, Task* task);
static void func_actor_146300_801327A4(Task* task);
static void func_actor_146300_801327CC(Task* task);

extern TmdSource D_actor_146300_8013ED68;
extern TmdSource D_actor_146300_8013EF64;
void             func_actor_146300_801326CC(Task*);
void             func_actor_146300_80132B1C(Task*);

s32 func_actor_146300_8013299C(Task*, s32, AnimationPlayRequest*);
s32 func_actor_146300_80132B14(void);

extern AnimationPlayRequest D_actor_146300_80137A20;
extern AnimationPlayRequest D_actor_146300_80137A34;
extern AnimationPlayRequest D_actor_146300_80137A98;
extern AnimationPlayRequest D_actor_146300_80137B74;
extern AnimationPlayRequest D_actor_146300_80137B88;
extern AnimationPlayRequest D_actor_146300_80137BC4;
extern GpCopyArg            D_actor_146300_80137BD8;
void                        func_actor_146300_80132418(s32);

extern AnimationPlayRequest D_actor_146300_8013791C;
extern AnimationPlayRequest D_actor_146300_80137930;
extern AnimationPlayRequest D_actor_146300_80137944;
extern AnimationPlayRequest D_actor_146300_80137958;
extern AnimationPlayRequest D_actor_146300_8013796C;
extern AnimationPlayRequest D_actor_146300_80137980;
extern AnimationPlayRequest D_actor_146300_80137994;
extern AnimationPlayRequest D_actor_146300_801379A8;
extern AnimationPlayRequest D_actor_146300_801379BC;
extern AnimationPlayRequest D_actor_146300_801379D0;
extern AnimationPlayRequest D_actor_146300_801379E4;
extern AnimationPlayRequest D_actor_146300_801379F8;
extern AnimationPlayRequest D_actor_146300_80137A0C;
extern AnimationPlayRequest D_actor_146300_80137A5C;
extern AnimationPlayRequest D_actor_146300_80137A70;
extern AnimationPlayRequest D_actor_146300_80137A84;
extern AnimationPlayRequest D_actor_146300_80137AC0;
extern AnimationPlayRequest D_actor_146300_80137AD4;
extern AnimationPlayRequest D_actor_146300_80137AE8;
extern AnimationPlayRequest D_actor_146300_80137AFC;
extern AnimationPlayRequest D_actor_146300_80137B24;
extern AnimationPlayRequest D_actor_146300_80137B4C;
extern AnimationPlayRequest D_actor_146300_80137B9C;
extern AnimationPlayRequest D_actor_146300_80137BB0;
extern ActorTransform       D_actor_146300_80137BE0;
extern ActorTransform       D_actor_146300_80137BF8;
void                        func_actor_146300_801323E0(void);

void func_actor_146300_80131ECC(Task*);

AnimationPackedPose D_actor_146300_80132BBC[12] = {
#include "assets/actor_146300_animation_01330_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80132C4C[135] = {
#include "assets/actor_146300_animation_01330_bank4.inc"
};

AnimationRecord D_actor_146300_80132E68[176] = {
#include "assets/actor_146300_animation_01330_records.inc"
};

u16 D_actor_146300_80133128[20] = {
#include "assets/actor_146300_animation_01330_indices.inc"
};

AnimationSet D_actor_146300_80133150 = {
    D_actor_146300_80132E68,
    D_actor_146300_80133128,
    { NULL, D_actor_146300_80132BBC, NULL, NULL, D_actor_146300_80132C4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80133178[5] = {
#include "assets/actor_146300_animation_016C8_bank1.inc"
};

AnimationPackedRotation D_actor_146300_801331B4[57] = {
#include "assets/actor_146300_animation_016C8_bank4.inc"
};

AnimationRecord D_actor_146300_80133298[138] = {
#include "assets/actor_146300_animation_016C8_records.inc"
};

u16 D_actor_146300_801334C0[20] = {
#include "assets/actor_146300_animation_016C8_indices.inc"
};

AnimationSet D_actor_146300_801334E8 = {
    D_actor_146300_80133298,
    D_actor_146300_801334C0,
    { NULL, D_actor_146300_80133178, NULL, NULL, D_actor_146300_801331B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80133510[23] = {
#include "assets/actor_146300_animation_01E30_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80133624[166] = {
#include "assets/actor_146300_animation_01E30_bank4.inc"
};

AnimationRecord D_actor_146300_801338BC[219] = {
#include "assets/actor_146300_animation_01E30_records.inc"
};

u16 D_actor_146300_80133C28[20] = {
#include "assets/actor_146300_animation_01E30_indices.inc"
};

AnimationSet D_actor_146300_80133C50 = {
    D_actor_146300_801338BC,
    D_actor_146300_80133C28,
    { NULL, D_actor_146300_80133510, NULL, NULL, D_actor_146300_80133624, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80133C78[2] = {
#include "assets/actor_146300_animation_021A8_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80133C90[80] = {
#include "assets/actor_146300_animation_021A8_bank4.inc"
};

AnimationRecord D_actor_146300_80133DD0[116] = {
#include "assets/actor_146300_animation_021A8_records.inc"
};

u16 D_actor_146300_80133FA0[20] = {
#include "assets/actor_146300_animation_021A8_indices.inc"
};

AnimationSet D_actor_146300_80133FC8 = {
    D_actor_146300_80133DD0,
    D_actor_146300_80133FA0,
    { NULL, D_actor_146300_80133C78, NULL, NULL, D_actor_146300_80133C90, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80133FF0[19] = {
#include "assets/actor_146300_animation_028E4_bank1.inc"
};

AnimationPackedRotation D_actor_146300_801340D4[170] = {
#include "assets/actor_146300_animation_028E4_bank4.inc"
};

AnimationRecord D_actor_146300_8013437C[216] = {
#include "assets/actor_146300_animation_028E4_records.inc"
};

u16 D_actor_146300_801346DC[20] = {
#include "assets/actor_146300_animation_028E4_indices.inc"
};

AnimationSet D_actor_146300_80134704 = {
    D_actor_146300_8013437C,
    D_actor_146300_801346DC,
    { NULL, D_actor_146300_80133FF0, NULL, NULL, D_actor_146300_801340D4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_8013472C[2] = {
#include "assets/actor_146300_animation_02B6C_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80134744[51] = {
#include "assets/actor_146300_animation_02B6C_bank4.inc"
};

AnimationRecord D_actor_146300_80134810[85] = {
#include "assets/actor_146300_animation_02B6C_records.inc"
};

u16 D_actor_146300_80134964[20] = {
#include "assets/actor_146300_animation_02B6C_indices.inc"
};

AnimationSet D_actor_146300_8013498C = {
    D_actor_146300_80134810,
    D_actor_146300_80134964,
    { NULL, D_actor_146300_8013472C, NULL, NULL, D_actor_146300_80134744, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_801349B4[10] = {
#include "assets/actor_146300_animation_0300C_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80134A2C[96] = {
#include "assets/actor_146300_animation_0300C_bank4.inc"
};

AnimationRecord D_actor_146300_80134BAC[150] = {
#include "assets/actor_146300_animation_0300C_records.inc"
};

u16 D_actor_146300_80134E04[20] = {
#include "assets/actor_146300_animation_0300C_indices.inc"
};

AnimationSet D_actor_146300_80134E2C = {
    D_actor_146300_80134BAC,
    D_actor_146300_80134E04,
    { NULL, D_actor_146300_801349B4, NULL, NULL, D_actor_146300_80134A2C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80134E54[6] = {
#include "assets/actor_146300_animation_0352C_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80134E9C[124] = {
#include "assets/actor_146300_animation_0352C_bank4.inc"
};

AnimationRecord D_actor_146300_8013508C[166] = {
#include "assets/actor_146300_animation_0352C_records.inc"
};

u16 D_actor_146300_80135324[20] = {
#include "assets/actor_146300_animation_0352C_indices.inc"
};

AnimationSet D_actor_146300_8013534C = {
    D_actor_146300_8013508C,
    D_actor_146300_80135324,
    { NULL, D_actor_146300_80134E54, NULL, NULL, D_actor_146300_80134E9C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80135374[2] = {
#include "assets/actor_146300_animation_03804_bank1.inc"
};

AnimationPackedRotation D_actor_146300_8013538C[43] = {
#include "assets/actor_146300_animation_03804_bank4.inc"
};

AnimationRecord D_actor_146300_80135438[113] = {
#include "assets/actor_146300_animation_03804_records.inc"
};

u16 D_actor_146300_801355FC[20] = {
#include "assets/actor_146300_animation_03804_indices.inc"
};

AnimationSet D_actor_146300_80135624 = {
    D_actor_146300_80135438,
    D_actor_146300_801355FC,
    { NULL, D_actor_146300_80135374, NULL, NULL, D_actor_146300_8013538C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_8013564C[3] = {
#include "assets/actor_146300_animation_03ACC_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80135670[57] = {
#include "assets/actor_146300_animation_03ACC_bank4.inc"
};

AnimationRecord D_actor_146300_80135754[92] = {
#include "assets/actor_146300_animation_03ACC_records.inc"
};

u16 D_actor_146300_801358C4[20] = {
#include "assets/actor_146300_animation_03ACC_indices.inc"
};

AnimationSet D_actor_146300_801358EC = {
    D_actor_146300_80135754,
    D_actor_146300_801358C4,
    { NULL, D_actor_146300_8013564C, NULL, NULL, D_actor_146300_80135670, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80135914[6] = {
#include "assets/actor_146300_animation_03FEC_bank1.inc"
};

AnimationPackedRotation D_actor_146300_8013595C[124] = {
#include "assets/actor_146300_animation_03FEC_bank4.inc"
};

AnimationRecord D_actor_146300_80135B4C[166] = {
#include "assets/actor_146300_animation_03FEC_records.inc"
};

u16 D_actor_146300_80135DE4[20] = {
#include "assets/actor_146300_animation_03FEC_indices.inc"
};

AnimationSet D_actor_146300_80135E0C = {
    D_actor_146300_80135B4C,
    D_actor_146300_80135DE4,
    { NULL, D_actor_146300_80135914, NULL, NULL, D_actor_146300_8013595C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80135E34[7] = {
#include "assets/actor_146300_animation_04394_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80135E88[81] = {
#include "assets/actor_146300_animation_04394_bank4.inc"
};

AnimationRecord D_actor_146300_80135FCC[112] = {
#include "assets/actor_146300_animation_04394_records.inc"
};

u16 D_actor_146300_8013618C[20] = {
#include "assets/actor_146300_animation_04394_indices.inc"
};

AnimationSet D_actor_146300_801361B4 = {
    D_actor_146300_80135FCC,
    D_actor_146300_8013618C,
    { NULL, D_actor_146300_80135E34, NULL, NULL, D_actor_146300_80135E88, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_801361DC[7] = {
#include "assets/actor_146300_animation_04758_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80136230[81] = {
#include "assets/actor_146300_animation_04758_bank4.inc"
};

AnimationRecord D_actor_146300_80136374[119] = {
#include "assets/actor_146300_animation_04758_records.inc"
};

u16 D_actor_146300_80136550[20] = {
#include "assets/actor_146300_animation_04758_indices.inc"
};

AnimationSet D_actor_146300_80136578 = {
    D_actor_146300_80136374,
    D_actor_146300_80136550,
    { NULL, D_actor_146300_801361DC, NULL, NULL, D_actor_146300_80136230, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_801365A0[2] = {
#include "assets/actor_146300_animation_049F0_bank1.inc"
};

AnimationPackedRotation D_actor_146300_801365B8[52] = {
#include "assets/actor_146300_animation_049F0_bank4.inc"
};

AnimationRecord D_actor_146300_80136688[88] = {
#include "assets/actor_146300_animation_049F0_records.inc"
};

u16 D_actor_146300_801367E8[20] = {
#include "assets/actor_146300_animation_049F0_indices.inc"
};

AnimationSet D_actor_146300_80136810 = {
    D_actor_146300_80136688,
    D_actor_146300_801367E8,
    { NULL, D_actor_146300_801365A0, NULL, NULL, D_actor_146300_801365B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80136838[2] = {
#include "assets/actor_146300_animation_04C7C_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80136850[49] = {
#include "assets/actor_146300_animation_04C7C_bank4.inc"
};

AnimationRecord D_actor_146300_80136914[88] = {
#include "assets/actor_146300_animation_04C7C_records.inc"
};

u16 D_actor_146300_80136A74[20] = {
#include "assets/actor_146300_animation_04C7C_indices.inc"
};

AnimationSet D_actor_146300_80136A9C = {
    D_actor_146300_80136914,
    D_actor_146300_80136A74,
    { NULL, D_actor_146300_80136838, NULL, NULL, D_actor_146300_80136850, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80136AC4[14] = {
#include "assets/actor_146300_animation_05354_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80136B6C[155] = {
#include "assets/actor_146300_animation_05354_bank4.inc"
};

AnimationRecord D_actor_146300_80136DD8[221] = {
#include "assets/actor_146300_animation_05354_records.inc"
};

u16 D_actor_146300_8013714C[20] = {
#include "assets/actor_146300_animation_05354_indices.inc"
};

AnimationSet D_actor_146300_80137174 = {
    D_actor_146300_80136DD8,
    D_actor_146300_8013714C,
    { NULL, D_actor_146300_80136AC4, NULL, NULL, D_actor_146300_80136B6C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_8013719C[5] = {
#include "assets/actor_146300_animation_0568C_bank1.inc"
};

AnimationPackedRotation D_actor_146300_801371D8[65] = {
#include "assets/actor_146300_animation_0568C_bank4.inc"
};

AnimationRecord D_actor_146300_801372DC[106] = {
#include "assets/actor_146300_animation_0568C_records.inc"
};

u16 D_actor_146300_80137484[20] = {
#include "assets/actor_146300_animation_0568C_indices.inc"
};

AnimationSet D_actor_146300_801374AC = {
    D_actor_146300_801372DC,
    D_actor_146300_80137484,
    { NULL, D_actor_146300_8013719C, NULL, NULL, D_actor_146300_801371D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_801374D4[3] = {
#include "assets/actor_146300_animation_05A44_bank1.inc"
};

AnimationPackedRotation D_actor_146300_801374F8[81] = {
#include "assets/actor_146300_animation_05A44_bank4.inc"
};

AnimationRecord D_actor_146300_8013763C[128] = {
#include "assets/actor_146300_animation_05A44_records.inc"
};

u16 D_actor_146300_8013783C[20] = {
#include "assets/actor_146300_animation_05A44_indices.inc"
};

AnimationSet D_actor_146300_80137864 = {
    D_actor_146300_8013763C,
    D_actor_146300_8013783C,
    { NULL, D_actor_146300_801374D4, NULL, NULL, D_actor_146300_801374F8, NULL, NULL, NULL },
};

TaskDesc D_actor_146300_8013788C = { { { TASK_BODY_NONE, 192 } }, func_actor_146300_80131ECC, { .value = 0 } };

Actor146300AnimCopy7898 D_actor_146300_80137898 = { .data = { { &D_actor_146300_80133150, &D_actor_146300_801334E8, &D_actor_146300_80133C50, &D_actor_146300_80133FC8, &D_actor_146300_80134704, &D_actor_146300_8013498C, &D_actor_146300_80134E2C, &D_actor_146300_8013534C, &D_actor_146300_80135624, &D_actor_146300_801358EC, &D_actor_146300_80135E0C, &D_actor_146300_801361B4, &D_actor_146300_80136578, &D_actor_146300_80136810, &D_actor_146300_80136A9C, &D_actor_146300_80137174, &D_actor_146300_801374AC, &D_actor_146300_80137864 }, { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_146300_8013791C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137930 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137944 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137958 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_8013796C = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137980 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137994 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_801379A8 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_801379BC = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_801379D0 = { { .index = 1 }, 58, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_801379E4 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_801379F8 = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A0C = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A20 = { { .index = 1 }, 62, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A34 = { { .index = 1 }, 63, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A48 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A5C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A70 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A84 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A98 = { { .index = 1 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137AAC = { { .index = 1 }, 5, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137AC0 = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137AD4 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137AE8 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137AFC = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B10 = { { .index = 1 }, 10, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B24 = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146300_80137B38 = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B4C = { { .index = 1 }, 13, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B60 = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B74 = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B88 = { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B9C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137BB0 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137BC4 = { { .index = 1 }, 64, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpCopyArg D_actor_146300_80137BD8 = { { .words = D_actor_146300_80137898.words }, 32 };

ActorTransform D_actor_146300_80137BE0 = { { 1110, -0x2EE0, -2500, 0 }, { 0, 1820, 0, 0 } };

ActorTransform D_actor_146300_80137BF8 = { { 1500, -0x2EE0, -1744, 0 }, { 0, 1820, 0, 0 } };

ActorTransform D_actor_146300_80137C10 = { { 1110, -0x2EE0, -2000, 0 }, { 0, 682, 0, 0 } };

EvsCommand D_actor_146300_80137C28[99] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_146300_801323E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 16 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_146300_80137BD8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137BB0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 13 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137B24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_146300_80137BE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137898.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5315000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137898.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A5C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137898.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_8013791C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A70 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137930 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A84 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137944 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137AAC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137958 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137980 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137AAC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379BC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137B4C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137AAC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_8013796C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137AC0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137994 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137AD4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137AE8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5315000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379F8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137A0C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137AFC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137B10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5315000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_146300_80137BF8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137B9C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137AAC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_146300_80138570[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137B9C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137B38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_146300_80137BF8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 17 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_146300_801386C0[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_146300_80137BD8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137BC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137A20 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137A34 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_146300_80138810[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137A98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_146300_80132418 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_146300_80132418 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_146300_801388D0[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 21 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_146300_80137BD8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137B88 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137BC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137B74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137BC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137B88 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_146300_80138A38[6] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137B88 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137B60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_146300_80138AC8[7] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 22 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137B88 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_146300_80137B60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_146300_80138B70[20] = {
#include "assets/actor_146300_model_0CF48_skeleton.inc"
};

u32 D_actor_146300_80138E40[20] = {
#include "assets/actor_146300_model_0CF48_partVerts.inc"
};

SVECTOR D_actor_146300_80138E90[390] = {
#include "assets/actor_146300_model_0CF48_verts.inc"
};

SVECTOR D_actor_146300_80139AC0[407] = {
#include "assets/actor_146300_model_0CF48_normals.inc"
};

u32 D_actor_146300_8013A778[4476] = {
#include "assets/actor_146300_model_0CF48_stream.inc"
};

TmdSource D_actor_146300_8013ED68 = {
    0,
    24444,
    6776,
    20,
    D_actor_146300_80138E40,
    D_actor_146300_80138E90,
    D_actor_146300_80139AC0,
    D_actor_146300_80138B70,
    D_actor_146300_8013A778,
};

TmdBone D_actor_146300_8013ED8C[1] = {
#include "assets/actor_146300_model_0D144_skeleton.inc"
};

u32 D_actor_146300_8013EDB0[1] = {
#include "assets/actor_146300_model_0D144_partVerts.inc"
};

SVECTOR D_actor_146300_8013EDB4[14] = {
#include "assets/actor_146300_model_0D144_verts.inc"
};

SVECTOR D_actor_146300_8013EE24[12] = {
#include "assets/actor_146300_model_0D144_normals.inc"
};

u32 D_actor_146300_8013EE84[56] = {
#include "assets/actor_146300_model_0D144_stream.inc"
};

TmdSource D_actor_146300_8013EF64 = {
    0,
    340,
    0,
    1,
    D_actor_146300_8013EDB0,
    D_actor_146300_8013EDB4,
    D_actor_146300_8013EE24,
    D_actor_146300_8013ED8C,
    D_actor_146300_8013EE84,
};

AnimationPackedPose D_actor_146300_8013EF88[6] = {
#include "assets/actor_146300_animation_0D4CC_bank1.inc"
};

AnimationPackedRotation D_actor_146300_8013EFD0[59] = {
#include "assets/actor_146300_animation_0D4CC_bank4.inc"
};

AnimationRecord D_actor_146300_8013F0BC[130] = {
#include "assets/actor_146300_animation_0D4CC_records.inc"
};

u16 D_actor_146300_8013F2C4[20] = {
#include "assets/actor_146300_animation_0D4CC_indices.inc"
};

AnimationSet D_actor_146300_8013F2EC = {
    D_actor_146300_8013F0BC,
    D_actor_146300_8013F2C4,
    { NULL, D_actor_146300_8013EF88, NULL, NULL, D_actor_146300_8013EFD0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_8013F314[9] = {
#include "assets/actor_146300_animation_0DB80_bank1.inc"
};

AnimationPackedRotation D_actor_146300_8013F380[163] = {
#include "assets/actor_146300_animation_0DB80_bank4.inc"
};

AnimationRecord D_actor_146300_8013F60C[219] = {
#include "assets/actor_146300_animation_0DB80_records.inc"
};

u16 D_actor_146300_8013F978[20] = {
#include "assets/actor_146300_animation_0DB80_indices.inc"
};

AnimationSet D_actor_146300_8013F9A0 = {
    D_actor_146300_8013F60C,
    D_actor_146300_8013F978,
    { NULL, D_actor_146300_8013F314, NULL, NULL, D_actor_146300_8013F380, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_8013F9C8[9] = {
#include "assets/actor_146300_animation_0E208_bank1.inc"
};

AnimationPackedRotation D_actor_146300_8013FA34[149] = {
#include "assets/actor_146300_animation_0E208_bank4.inc"
};

AnimationRecord D_actor_146300_8013FC88[222] = {
#include "assets/actor_146300_animation_0E208_records.inc"
};

u16 D_actor_146300_80140000[20] = {
#include "assets/actor_146300_animation_0E208_indices.inc"
};

AnimationSet D_actor_146300_80140028 = {
    D_actor_146300_8013FC88,
    D_actor_146300_80140000,
    { NULL, D_actor_146300_8013F9C8, NULL, NULL, D_actor_146300_8013FA34, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80140050[2] = {
#include "assets/actor_146300_animation_0E5C8_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80140068[78] = {
#include "assets/actor_146300_animation_0E5C8_bank4.inc"
};

AnimationRecord D_actor_146300_801401A0[136] = {
#include "assets/actor_146300_animation_0E5C8_records.inc"
};

u16 D_actor_146300_801403C0[20] = {
#include "assets/actor_146300_animation_0E5C8_indices.inc"
};

AnimationSet D_actor_146300_801403E8 = {
    D_actor_146300_801401A0,
    D_actor_146300_801403C0,
    { NULL, D_actor_146300_80140050, NULL, NULL, D_actor_146300_80140068, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80140410[2] = {
#include "assets/actor_146300_animation_0E7D4_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80140428[21] = {
#include "assets/actor_146300_animation_0E7D4_bank4.inc"
};

AnimationRecord D_actor_146300_8014047C[84] = {
#include "assets/actor_146300_animation_0E7D4_records.inc"
};

u16 D_actor_146300_801405CC[20] = {
#include "assets/actor_146300_animation_0E7D4_indices.inc"
};

AnimationSet D_actor_146300_801405F4 = {
    D_actor_146300_8014047C,
    D_actor_146300_801405CC,
    { NULL, D_actor_146300_80140410, NULL, NULL, D_actor_146300_80140428, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_8014061C[2] = {
#include "assets/actor_146300_animation_0EA84_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80140634[56] = {
#include "assets/actor_146300_animation_0EA84_bank4.inc"
};

AnimationRecord D_actor_146300_80140714[90] = {
#include "assets/actor_146300_animation_0EA84_records.inc"
};

u16 D_actor_146300_8014087C[20] = {
#include "assets/actor_146300_animation_0EA84_indices.inc"
};

AnimationSet D_actor_146300_801408A4 = {
    D_actor_146300_80140714,
    D_actor_146300_8014087C,
    { NULL, D_actor_146300_8014061C, NULL, NULL, D_actor_146300_80140634, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_801408CC[2] = {
#include "assets/actor_146300_animation_0ECC4_bank1.inc"
};

AnimationPackedRotation D_actor_146300_801408E4[42] = {
#include "assets/actor_146300_animation_0ECC4_bank4.inc"
};

AnimationRecord D_actor_146300_8014098C[76] = {
#include "assets/actor_146300_animation_0ECC4_records.inc"
};

u16 D_actor_146300_80140ABC[20] = {
#include "assets/actor_146300_animation_0ECC4_indices.inc"
};

AnimationSet D_actor_146300_80140AE4 = {
    D_actor_146300_8014098C,
    D_actor_146300_80140ABC,
    { NULL, D_actor_146300_801408CC, NULL, NULL, D_actor_146300_801408E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80140B0C[2] = {
#include "assets/actor_146300_animation_0F18C_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80140B24[115] = {
#include "assets/actor_146300_animation_0F18C_bank4.inc"
};

AnimationRecord D_actor_146300_80140CF0[165] = {
#include "assets/actor_146300_animation_0F18C_records.inc"
};

u16 D_actor_146300_80140F84[20] = {
#include "assets/actor_146300_animation_0F18C_indices.inc"
};

AnimationSet D_actor_146300_80140FAC = {
    D_actor_146300_80140CF0,
    D_actor_146300_80140F84,
    { NULL, D_actor_146300_80140B0C, NULL, NULL, D_actor_146300_80140B24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80140FD4[2] = {
#include "assets/actor_146300_animation_0F414_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80140FEC[48] = {
#include "assets/actor_146300_animation_0F414_bank4.inc"
};

AnimationRecord D_actor_146300_801410AC[88] = {
#include "assets/actor_146300_animation_0F414_records.inc"
};

u16 D_actor_146300_8014120C[20] = {
#include "assets/actor_146300_animation_0F414_indices.inc"
};

AnimationSet D_actor_146300_80141234 = {
    D_actor_146300_801410AC,
    D_actor_146300_8014120C,
    { NULL, D_actor_146300_80140FD4, NULL, NULL, D_actor_146300_80140FEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_8014125C[2] = {
#include "assets/actor_146300_animation_0F888_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80141274[93] = {
#include "assets/actor_146300_animation_0F888_bank4.inc"
};

AnimationRecord D_actor_146300_801413E8[166] = {
#include "assets/actor_146300_animation_0F888_records.inc"
};

u16 D_actor_146300_80141680[20] = {
#include "assets/actor_146300_animation_0F888_indices.inc"
};

AnimationSet D_actor_146300_801416A8 = {
    D_actor_146300_801413E8,
    D_actor_146300_80141680,
    { NULL, D_actor_146300_8014125C, NULL, NULL, D_actor_146300_80141274, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_801416D0[2] = {
#include "assets/actor_146300_animation_0FA24_bank1.inc"
};

AnimationPackedRotation D_actor_146300_801416E8[17] = {
#include "assets/actor_146300_animation_0FA24_bank4.inc"
};

AnimationRecord D_actor_146300_8014172C[60] = {
#include "assets/actor_146300_animation_0FA24_records.inc"
};

u16 D_actor_146300_8014181C[20] = {
#include "assets/actor_146300_animation_0FA24_indices.inc"
};

AnimationSet D_actor_146300_80141844 = {
    D_actor_146300_8014172C,
    D_actor_146300_8014181C,
    { NULL, D_actor_146300_801416D0, NULL, NULL, D_actor_146300_801416E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_8014186C[2] = {
#include "assets/actor_146300_animation_0FCC4_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80141884[37] = {
#include "assets/actor_146300_animation_0FCC4_bank4.inc"
};

AnimationRecord D_actor_146300_80141918[105] = {
#include "assets/actor_146300_animation_0FCC4_records.inc"
};

u16 D_actor_146300_80141ABC[20] = {
#include "assets/actor_146300_animation_0FCC4_indices.inc"
};

AnimationSet D_actor_146300_80141AE4 = {
    D_actor_146300_80141918,
    D_actor_146300_80141ABC,
    { NULL, D_actor_146300_8014186C, NULL, NULL, D_actor_146300_80141884, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80141B0C[2] = {
#include "assets/actor_146300_animation_0FED0_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80141B24[31] = {
#include "assets/actor_146300_animation_0FED0_bank4.inc"
};

AnimationRecord D_actor_146300_80141BA0[74] = {
#include "assets/actor_146300_animation_0FED0_records.inc"
};

u16 D_actor_146300_80141CC8[20] = {
#include "assets/actor_146300_animation_0FED0_indices.inc"
};

AnimationSet D_actor_146300_80141CF0 = {
    D_actor_146300_80141BA0,
    D_actor_146300_80141CC8,
    { NULL, D_actor_146300_80141B0C, NULL, NULL, D_actor_146300_80141B24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80141D18[2] = {
#include "assets/actor_146300_animation_1014C_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80141D30[32] = {
#include "assets/actor_146300_animation_1014C_bank4.inc"
};

AnimationRecord D_actor_146300_80141DB0[101] = {
#include "assets/actor_146300_animation_1014C_records.inc"
};

u16 D_actor_146300_80141F44[20] = {
#include "assets/actor_146300_animation_1014C_indices.inc"
};

AnimationSet D_actor_146300_80141F6C = {
    D_actor_146300_80141DB0,
    D_actor_146300_80141F44,
    { NULL, D_actor_146300_80141D18, NULL, NULL, D_actor_146300_80141D30, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80141F94[5] = {
#include "assets/actor_146300_animation_103E0_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80141FD0[36] = {
#include "assets/actor_146300_animation_103E0_bank4.inc"
};

AnimationRecord D_actor_146300_80142060[94] = {
#include "assets/actor_146300_animation_103E0_records.inc"
};

u16 D_actor_146300_801421D8[20] = {
#include "assets/actor_146300_animation_103E0_indices.inc"
};

AnimationSet D_actor_146300_80142200 = {
    D_actor_146300_80142060,
    D_actor_146300_801421D8,
    { NULL, D_actor_146300_80141F94, NULL, NULL, D_actor_146300_80141FD0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146300_80142228[2] = {
#include "assets/actor_146300_animation_10954_bank1.inc"
};

AnimationPackedRotation D_actor_146300_80142240[135] = {
#include "assets/actor_146300_animation_10954_bank4.inc"
};

AnimationRecord D_actor_146300_8014245C[188] = {
#include "assets/actor_146300_animation_10954_records.inc"
};

u16 D_actor_146300_8014274C[20] = {
#include "assets/actor_146300_animation_10954_indices.inc"
};

AnimationSet D_actor_146300_80142774 = {
    D_actor_146300_8014245C,
    D_actor_146300_8014274C,
    { NULL, D_actor_146300_80142228, NULL, NULL, D_actor_146300_80142240, NULL, NULL, NULL },
};

s16 gScriptedWalkBlendFrames = 8;

Actor146300MsgEntry D_actor_146300_801427A0[5] = {
    { 2003, { .call1 = func_actor_146300_8013299C } },
    { 2005, { .call3 = actorMsgSetPairVisibility } },
    { 2004, { .call2 = scriptedWalkPlace } },
    { 2011, { .call0 = func_actor_146300_80132B14 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_146300_801427C8[2] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_146300_801326CC, { .model = &D_actor_146300_8013ED68 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_146300_80132B1C, { .model = &D_actor_146300_8013EF64 } },
};

u8 D_actor_146300_801427E0[68] = {
    0,
    0,
    0,
    0,
    236,
    242,
    19,
    128,
    160,
    249,
    19,
    128,
    40,
    0,
    20,
    128,
    232,
    3,
    20,
    128,
    244,
    5,
    20,
    128,
    164,
    8,
    20,
    128,
    228,
    10,
    20,
    128,
    172,
    15,
    20,
    128,
    52,
    18,
    20,
    128,
    168,
    22,
    20,
    128,
    68,
    24,
    20,
    128,
    228,
    26,
    20,
    128,
    240,
    28,
    20,
    128,
    108,
    31,
    20,
    128,
    0,
    34,
    20,
    128,
    116,
    39,
    20,
    128,
};

s32 D_actor_146300_80142824 = 0;

Actor146300Work* gScriptedWalkWork;

Task* gActorSelfTask;

Task* gActorHelperTask;

static void func_actor_146300_8013224C(void);
static void func_actor_146300_801324AC(Enemy* enemy, Task* task);

void func_actor_146300_80131ECC(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_AgeFlag119();
            switch (GameFlag_GetNibble(0x7B)) {
                case 2:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        Gp_RunCapCmd1(0x12);
                        task->state++;
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        D_actor_146300_80142824 = 0x13;
                        GameFlag_SetNibble(0x7B, 3);
                        task->state = 0xA;
                    }
                    break;
                case 3:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        if (Gp_GetCurBit2Flag(0x1F) == 1) {
                            Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA4, 0, 0);
                            D_actor_146300_80142824 = 0x13;
                            task->state             = 0x14;
                        } else {
                            Gp_RunCapCmd1(0x12);
                            task->state++;
                        }
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        D_actor_146300_80142824 = 0x14;
                        GameFlag_SetNibble(0x7B, 4);
                        task->state = 0xA;
                    }
                    break;
                case 4:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        if (Gp_GetCurBit2Flag(0x20) == 1) {
                            Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA4, 0, 0);
                            D_actor_146300_80142824 = 0x14;
                            task->state             = 0x14;
                        } else {
                            Gp_RunCapCmd1(0x12);
                            task->state++;
                        }
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        GameFlag_SetNibble(0x7B, 5);
                        task->state = 0x1E;
                    }
                    break;
                case 5:
                    if (Gp_GetCurBit2Flag(0x21) == 1) {
                        Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA4, 0, 0);
                        task->state = 0x28;
                    } else {
                        func_800E8614(D_actor_146300_80138AC8, 0);
                        taskKill(task);
                    }
                    break;
                default:
                    task->state++;
                    break;
            }
            break;
        case 1:
            Gp_MsgPlayerWeapon(1);
            taskKill(task);
            break;
        case 10:
            Gp_StartCapSlot((s16)D_actor_146300_80142824, 0, 0);
            func_800E8614(D_actor_146300_801386C0, 1);
            task->state++;
            break;
        case 11:
            if (gGameSession->eventState == 0) {
                task->state = 0x14;
            }
            break;
        case 20:
            Gp_StartCapSlot((s16)D_actor_146300_80142824, 0, 1);
            func_800E8614(D_actor_146300_80138810, 1);
            task->state++;
            break;
        case 30:
            func_800E8614(D_actor_146300_801388D0, 1);
            task->state++;
            break;
        case 31:
            if (gGameSession->eventState == 0) {
                task->state = 0x28;
            }
            break;
        case 40:
            Gp_StartCapSlot(0x15, 0, 1);
            func_800E8614(D_actor_146300_80138A38, 1);
            task->state++;
            break;
        case 21:
        case 41:
            if (gGameSession->eventState == 0) {
                task->state = 1;
            }
            break;
    }
}

static void func_actor_146300_8013224C(void)
{
    switch (GameFlag_GetNibble(0x7B)) {
        case 2:
            Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D3, &D_actor_146300_80137B38, 0);
            break;
        case 3:
            if (Gp_HasCollectedBit(0x119) == 0) {
                if (Gp_GetCurBit2Flag(0x1F) == 1) {
                    Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D3, &D_actor_146300_80137AAC, 0);
                } else {
                    Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D3, &D_actor_146300_80137B38, 0);
                }
            } else {
                Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D3, &D_actor_146300_80137B38, 0);
            }
            break;
        case 4:
            if (Gp_HasCollectedBit(0x119) == 0) {
                if (Gp_GetCurBit2Flag(0x20) == 1) {
                    Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D3, &D_actor_146300_80137AAC, 0);
                } else {
                    Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D3, &D_actor_146300_80137B38, 0);
                }
                break;
            }
            /* fallthrough */
        case 5:
            Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D4, &D_actor_146300_80137C10, 0);
            Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D3, &D_actor_146300_80137B60, 0);
            break;
    }
}

void func_actor_146300_801323E0(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

void func_actor_146300_80132418(s32 arg0)
{
    switch (arg0) {
        case 0:
            if (Gp_GetCapEventKey() == 1) {
                Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D3, &D_actor_146300_80137B10, 0);
            }
            break;
        case 1:
            if (Gp_GetCapEventKey() == 2) {
                Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D3, &D_actor_146300_80137AAC, 0);
            }
            break;
    }
}

/// Spawn routine, state 0 of the task handler `func_actor_146300_801326CC`:
/// allocates the 0x4EC work block and publishes it in `gScriptedWalkWork`
/// and the task's `work` slot (destroying the enemy if the allocation fails),
/// installs the exit callback, binds the model's coordinate frame to the view
/// and publishes the task in `gActorSelfTask`.
///
/// The companion task from `D_actor_146300_801427C8` carries the model whose
/// texture page and CLUT row come out of the current area record - the session
/// location key is copied onto the stack, `areaSyncLocationVariant` fills in its
/// nested index and the enemy's `placeKey >> ENEMY_PLACE_INDEX_SHIFT` selects the 0x10-byte record.
/// The actor's task is then reparented under that companion, the model gets the
/// block's light and colour matrices and is relit from a point 0x320 above its
/// root translation, the animation stream is bound, the animation state is
/// seeded with mode 2 / id 0xB, the message table is published and the
/// per-frame update runs once before the state advances.
static void func_actor_146300_801324AC(Enemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor146300Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    Task*            helper;

    obj               = task->extra.tmd;
    coord             = obj->coords;
    work              = memCalloc(0x4EC, 0);
    gScriptedWalkWork = work;
    task->work        = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_146300_801327A4;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->flags                       = 0;
    gActorSelfTask                   = task;
    helper                           = Task_SpawnFromTable(D_actor_146300_801427C8, 1, 0, 0);
    gActorHelperTask                 = helper;
    actorTintTask(helper, enemy);
    Task_Reparent(task, gActorHelperTask);
    obj->lightMtx = &gScriptedWalkWork->light;
    obj->colorMtx = &gScriptedWalkWork->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&gScriptedWalkWork->rig.anim, D_actor_146300_801427E0, obj,
                  &gScriptedWalkWork->rig.poses[0], gScriptedWalkWork->rig.slots);
    gScriptedWalkWork->st.animId = 0xB;
    gScriptedWalkWork->st.state  = 2;
    task->msgTable               = D_actor_146300_801427A0;
    func_actor_146300_801327CC(task);
    task->state++;
}

/// The actor's task handler: publishes the task's work block in
/// `gScriptedWalkWork` on the way through, then runs the handler its
/// state selects from a table built on the stack - the spawn routine for state
/// 0, the per-frame update after it.
void func_actor_146300_801326CC(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_146300_801324AC,
        func_actor_146300_80132728,
    };

    gScriptedWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// State 1 of the task handler `func_actor_146300_801326CC`: refreshes the model
/// root's world matrix, relights the model from a point 0x320 above its
/// translation, then runs the per-frame update.
static void func_actor_146300_80132728(Enemy* enemy, Task* task)
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
    func_actor_146300_801327CC(task);
}

/// `Task::exitCallback` the spawn routine installs: hands the task's `Enemy`
/// (parked in `Task::spawnArg2`) back to `Gp_DestroyEnemy`.
static void func_actor_146300_801327A4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Per-frame update: reset mode 1 runs the reseed with the latched reset
/// argument and mode 2 the plain reseed, each then switching to mode 3; mode 3
/// ticks the animation. Steps 1 and 2 each return through their own copy of the
/// switch to mode 3; the two are identical, so jump.c cross-jumps them and only
/// the second survives.
static void func_actor_146300_801327CC(Task* task)
{
    if (gScriptedWalkWork->st.state == 1) {
        scriptedWalkBlendAnim();
        gScriptedWalkWork->st.state = 3;
        return;
    }
    if (gScriptedWalkWork->st.state == 2) {
        scriptedWalkResetAnim();
        gScriptedWalkWork->st.state = 3;
        return;
    }
    if (gScriptedWalkWork->st.state == 3) {
        scriptedWalkTickAnim();
    }
}

#include "../../shared/scripted_walk_tick_anim.inc.c"

#include "../../shared/scripted_walk_reset_anim.inc.c"

#include "../../shared/scripted_walk_blend_anim.inc.c"

/// Message 0x7D3 handler: adopts `preset`'s animation id when it is
/// one of the first 0x11, latching the reset mode and the reset argument the
/// reseed forwards, then hands the published task to the per-frame update. Ids
/// past the range are rejected with -1 and leave the work block untouched.
s32 func_actor_146300_8013299C(Task* task, s32 arg1, AnimationPlayRequest* preset)
{
    if (preset->animationId < 0x11) {
        gScriptedWalkWork->st.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET) {
            gScriptedWalkWork->st.state = 1;
            gScriptedWalkBlendFrames    = preset->blendFrames;
        } else {
            gScriptedWalkWork->st.state = 2;
        }
        gScriptedWalkWork->st.field_6 = 0;
        func_actor_146300_801327CC(gActorSelfTask);
        return 0;
    }
    return -1;
}

#include "../../shared/actor_messages_pair_visibility.inc.c"

#include "../../shared/scripted_walk_place.inc.c"

/// Message 0x7DB handler: accepts the message and does nothing.
s32 func_actor_146300_80132B14(void)
{
    return 0;
}

/// Task handler of the companion task: the first tick hangs the companion
/// model's coordinate frame under part 4 of the actor's model, shows the model
/// and steps to state 1; every later tick relights the companion model from a
/// point 0x320 above the actor model's root translation.
void func_actor_146300_80132B1C(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = gActorSelfTask->extra.tmd->coords;
    GfxCoord*  part  = parts + 4;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            extra->flags        = 0;
            coord->parent       = part;
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
