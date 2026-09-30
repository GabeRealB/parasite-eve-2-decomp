#include "actors/actor_420700.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Work block of the overlay's actor, allocated zeroed by its state-0 handler
/// and kept both in `D_actor_420700_8013EFE0` and at `Task::work`; the task
/// dispatcher republishes it every tick, and every other function in the
/// overlay reaches it through the global. `light` and `color` are the model's
/// matrices and `rig` and `st` its animation rig and state. The actor's own
/// state fields are a ramp: `st.field_6` is the mode message 0x7DB selects (1
/// and 3 rise, 2 falls, 0 leaves it alone), and `st.field_8` the value the
/// ramp walks by 0x80, clamped to 0..0x1000.
typedef struct Actor420700Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
    byte            pad_4EC[0xB4];
} Actor420700Work;
STATIC_ASSERT_SIZEOF(Actor420700Work, 0x5A0);

/// The work block above, published by the task dispatcher
/// `func_actor_420700_80132340` and by the state-0 handler.
extern Actor420700Work* D_actor_420700_8013EFE0;

/// The actor's own task, the `task` the state-0 handler
/// `func_actor_420700_80131E24` is entered with. Its `Task::extra` holds the
/// `TmdObject` whose trailing coordinate array `func_actor_420700_801323D8`
/// hangs the model task's own root off, at frame 4.
extern Task* D_actor_420700_8013EFE4;

/// The first task the state-0 handler spawns, the frame-4 model task
/// `func_actor_420700_801323D8`; the actor's exit callback kills it.
extern Task* D_actor_420700_8013EFE8;

/// The second task the state-0 handler spawns, the frame-8 model task
/// `func_actor_420700_801327EC`.
extern Task* D_actor_420700_8013EFEC;

static void func_actor_420700_8013239C(Task* task);
static void func_actor_420700_80132478(Task* task);
static void func_actor_420700_801324EC(void);
static void func_actor_420700_80132538(void);
static void func_actor_420700_801325C8(void);

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*);
        s32 (*call1)(Task*, s32, ActorCommand* request);
        s32 (*call2)(Task*, s32, s32);
    } handler;
} Actor420700MessageEntry;
STATIC_ASSERT_SIZEOF(Actor420700MessageEntry, 8);

extern Actor420700MessageEntry D_actor_420700_8013EF48[4];
extern TaskDesc                D_actor_420700_8013EF68[];
extern u8                      D_actor_420700_8013EF8C[];
extern s32                     D_actor_420700_8013EFF0;
extern s32                     D_actor_420700_8013EFF4;

extern TmdSource D_actor_420700_8013B68C;
extern TmdSource D_actor_420700_8013BB64;
extern TmdSource D_actor_420700_8013C028;
void             func_actor_420700_80132340(Task*);
void             func_actor_420700_801323D8(Task*);
void             func_actor_420700_801327EC(Task*);

s32 func_actor_420700_80132644(Task*, s32, AnimationPlayRequest*);
s32 func_actor_420700_801326F4(Task*, s32, s32);
s32 func_actor_420700_80132784(Task*, s32, ActorCommand* args);

AnimationPackedPose D_actor_420700_80132894[7] = {
#include "assets/actor_420700_animation_00DE0_bank1.inc"
};

AnimationPackedRotation D_actor_420700_801328E8[65] = {
#include "assets/actor_420700_animation_00DE0_bank4.inc"
};

AnimationRecord D_actor_420700_801329EC[123] = {
#include "assets/actor_420700_animation_00DE0_records.inc"
};

u16 D_actor_420700_80132BD8[20] = {
#include "assets/actor_420700_animation_00DE0_indices.inc"
};

AnimationSet D_actor_420700_80132C00 = {
    D_actor_420700_801329EC,
    D_actor_420700_80132BD8,
    { NULL, D_actor_420700_80132894, NULL, NULL, D_actor_420700_801328E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_80132C28[19] = {
#include "assets/actor_420700_animation_013A8_bank1.inc"
};

AnimationPackedRotation D_actor_420700_80132D0C[119] = {
#include "assets/actor_420700_animation_013A8_bank4.inc"
};

AnimationRecord D_actor_420700_80132EE8[174] = {
#include "assets/actor_420700_animation_013A8_records.inc"
};

u16 D_actor_420700_801331A0[20] = {
#include "assets/actor_420700_animation_013A8_indices.inc"
};

AnimationSet D_actor_420700_801331C8 = {
    D_actor_420700_80132EE8,
    D_actor_420700_801331A0,
    { NULL, D_actor_420700_80132C28, NULL, NULL, D_actor_420700_80132D0C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_801331F0[2] = {
#include "assets/actor_420700_animation_0164C_bank1.inc"
};

AnimationPackedRotation D_actor_420700_80133208[48] = {
#include "assets/actor_420700_animation_0164C_bank4.inc"
};

AnimationRecord D_actor_420700_801332C8[95] = {
#include "assets/actor_420700_animation_0164C_records.inc"
};

u16 D_actor_420700_80133444[20] = {
#include "assets/actor_420700_animation_0164C_indices.inc"
};

AnimationSet D_actor_420700_8013346C = {
    D_actor_420700_801332C8,
    D_actor_420700_80133444,
    { NULL, D_actor_420700_801331F0, NULL, NULL, D_actor_420700_80133208, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_80133494[2] = {
#include "assets/actor_420700_animation_01878_bank1.inc"
};

AnimationPackedRotation D_actor_420700_801334AC[34] = {
#include "assets/actor_420700_animation_01878_bank4.inc"
};

AnimationRecord D_actor_420700_80133534[79] = {
#include "assets/actor_420700_animation_01878_records.inc"
};

u16 D_actor_420700_80133670[20] = {
#include "assets/actor_420700_animation_01878_indices.inc"
};

AnimationSet D_actor_420700_80133698 = {
    D_actor_420700_80133534,
    D_actor_420700_80133670,
    { NULL, D_actor_420700_80133494, NULL, NULL, D_actor_420700_801334AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_801336C0[8] = {
#include "assets/actor_420700_animation_01C5C_bank1.inc"
};

AnimationPackedRotation D_actor_420700_80133720[77] = {
#include "assets/actor_420700_animation_01C5C_bank4.inc"
};

AnimationRecord D_actor_420700_80133854[128] = {
#include "assets/actor_420700_animation_01C5C_records.inc"
};

u16 D_actor_420700_80133A54[20] = {
#include "assets/actor_420700_animation_01C5C_indices.inc"
};

AnimationSet D_actor_420700_80133A7C = {
    D_actor_420700_80133854,
    D_actor_420700_80133A54,
    { NULL, D_actor_420700_801336C0, NULL, NULL, D_actor_420700_80133720, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_80133AA4[2] = {
#include "assets/actor_420700_animation_01EBC_bank1.inc"
};

AnimationPackedRotation D_actor_420700_80133ABC[37] = {
#include "assets/actor_420700_animation_01EBC_bank4.inc"
};

AnimationRecord D_actor_420700_80133B50[89] = {
#include "assets/actor_420700_animation_01EBC_records.inc"
};

u16 D_actor_420700_80133CB4[20] = {
#include "assets/actor_420700_animation_01EBC_indices.inc"
};

AnimationSet D_actor_420700_80133CDC = {
    D_actor_420700_80133B50,
    D_actor_420700_80133CB4,
    { NULL, D_actor_420700_80133AA4, NULL, NULL, D_actor_420700_80133ABC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_80133D04[7] = {
#include "assets/actor_420700_animation_02328_bank1.inc"
};

AnimationPackedRotation D_actor_420700_80133D58[96] = {
#include "assets/actor_420700_animation_02328_bank4.inc"
};

AnimationRecord D_actor_420700_80133ED8[146] = {
#include "assets/actor_420700_animation_02328_records.inc"
};

u16 D_actor_420700_80134120[20] = {
#include "assets/actor_420700_animation_02328_indices.inc"
};

AnimationSet D_actor_420700_80134148 = {
    D_actor_420700_80133ED8,
    D_actor_420700_80134120,
    { NULL, D_actor_420700_80133D04, NULL, NULL, D_actor_420700_80133D58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_80134170[28] = {
#include "assets/actor_420700_animation_02F18_bank1.inc"
};

AnimationPackedRotation D_actor_420700_801342C0[301] = {
#include "assets/actor_420700_animation_02F18_bank4.inc"
};

AnimationRecord D_actor_420700_80134774[359] = {
#include "assets/actor_420700_animation_02F18_records.inc"
};

u16 D_actor_420700_80134D10[20] = {
#include "assets/actor_420700_animation_02F18_indices.inc"
};

AnimationSet D_actor_420700_80134D38 = {
    D_actor_420700_80134774,
    D_actor_420700_80134D10,
    { NULL, D_actor_420700_80134170, NULL, NULL, D_actor_420700_801342C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_80134D60[3] = {
#include "assets/actor_420700_animation_0316C_bank1.inc"
};

AnimationPackedRotation D_actor_420700_80134D84[28] = {
#include "assets/actor_420700_animation_0316C_bank4.inc"
};

AnimationRecord D_actor_420700_80134DF4[92] = {
#include "assets/actor_420700_animation_0316C_records.inc"
};

u16 D_actor_420700_80134F64[20] = {
#include "assets/actor_420700_animation_0316C_indices.inc"
};

AnimationSet D_actor_420700_80134F8C = {
    D_actor_420700_80134DF4,
    D_actor_420700_80134F64,
    { NULL, D_actor_420700_80134D60, NULL, NULL, D_actor_420700_80134D84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_80134FB4[6] = {
#include "assets/actor_420700_animation_03404_bank1.inc"
};

AnimationPackedRotation D_actor_420700_80134FFC[38] = {
#include "assets/actor_420700_animation_03404_bank4.inc"
};

AnimationRecord D_actor_420700_80135094[90] = {
#include "assets/actor_420700_animation_03404_records.inc"
};

u16 D_actor_420700_801351FC[20] = {
#include "assets/actor_420700_animation_03404_indices.inc"
};

AnimationSet D_actor_420700_80135224 = {
    D_actor_420700_80135094,
    D_actor_420700_801351FC,
    { NULL, D_actor_420700_80134FB4, NULL, NULL, D_actor_420700_80134FFC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013524C[3] = {
#include "assets/actor_420700_animation_035D4_bank1.inc"
};

AnimationPackedRotation D_actor_420700_80135270[30] = {
#include "assets/actor_420700_animation_035D4_bank4.inc"
};

AnimationRecord D_actor_420700_801352E8[57] = {
#include "assets/actor_420700_animation_035D4_records.inc"
};

u16 D_actor_420700_801353CC[20] = {
#include "assets/actor_420700_animation_035D4_indices.inc"
};

AnimationSet D_actor_420700_801353F4 = {
    D_actor_420700_801352E8,
    D_actor_420700_801353CC,
    { NULL, D_actor_420700_8013524C, NULL, NULL, D_actor_420700_80135270, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013541C[11] = {
#include "assets/actor_420700_animation_03DD8_bank1.inc"
};

AnimationPackedRotation D_actor_420700_801354A0[186] = {
#include "assets/actor_420700_animation_03DD8_bank4.inc"
};

AnimationRecord D_actor_420700_80135788[274] = {
#include "assets/actor_420700_animation_03DD8_records.inc"
};

u16 D_actor_420700_80135BD0[20] = {
#include "assets/actor_420700_animation_03DD8_indices.inc"
};

AnimationSet D_actor_420700_80135BF8 = {
    D_actor_420700_80135788,
    D_actor_420700_80135BD0,
    { NULL, D_actor_420700_8013541C, NULL, NULL, D_actor_420700_801354A0, NULL, NULL, NULL },
};

TmdBone D_actor_420700_80135C20[20] = {
#include "assets/actor_420700_model_0986C_skeleton.inc"
};

u32 D_actor_420700_80135EF0[20] = {
#include "assets/actor_420700_model_0986C_partVerts.inc"
};

SVECTOR D_actor_420700_80135F40[364] = {
#include "assets/actor_420700_model_0986C_verts.inc"
};

SVECTOR D_actor_420700_80136AA0[354] = {
#include "assets/actor_420700_model_0986C_normals.inc"
};

u32 D_actor_420700_801375B0[4151] = {
#include "assets/actor_420700_model_0986C_stream.inc"
};

TmdSource D_actor_420700_8013B68C = {
    0,
    22008,
    6952,
    20,
    D_actor_420700_80135EF0,
    D_actor_420700_80135F40,
    D_actor_420700_80136AA0,
    D_actor_420700_80135C20,
    D_actor_420700_801375B0,
};

TmdBone D_actor_420700_8013B6B0[1] = {
#include "assets/actor_420700_model_09D44_skeleton.inc"
};

u32 D_actor_420700_8013B6D4[1] = {
#include "assets/actor_420700_model_09D44_partVerts.inc"
};

SVECTOR D_actor_420700_8013B6D8[21] = {
#include "assets/actor_420700_model_09D44_verts.inc"
};

SVECTOR D_actor_420700_8013B780[21] = {
#include "assets/actor_420700_model_09D44_normals.inc"
};

u32 D_actor_420700_8013B828[207] = {
#include "assets/actor_420700_model_09D44_stream.inc"
};

TmdSource D_actor_420700_8013BB64 = {
    0,
    1352,
    0,
    1,
    D_actor_420700_8013B6D4,
    D_actor_420700_8013B6D8,
    D_actor_420700_8013B780,
    D_actor_420700_8013B6B0,
    D_actor_420700_8013B828,
};

TmdBone D_actor_420700_8013BB88[1] = {
#include "assets/actor_420700_model_0A208_skeleton.inc"
};

u32 D_actor_420700_8013BBAC[1] = {
#include "assets/actor_420700_model_0A208_partVerts.inc"
};

SVECTOR D_actor_420700_8013BBB0[26] = {
#include "assets/actor_420700_model_0A208_verts.inc"
};

SVECTOR D_actor_420700_8013BC80[26] = {
#include "assets/actor_420700_model_0A208_normals.inc"
};

u32 D_actor_420700_8013BD50[182] = {
#include "assets/actor_420700_model_0A208_stream.inc"
};

TmdSource D_actor_420700_8013C028 = {
    0,
    1276,
    0,
    1,
    D_actor_420700_8013BBAC,
    D_actor_420700_8013BBB0,
    D_actor_420700_8013BC80,
    D_actor_420700_8013BB88,
    D_actor_420700_8013BD50,
};

AnimationPackedPose D_actor_420700_8013C04C[2] = {
#include "assets/actor_420700_animation_0A464_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013C064[30] = {
#include "assets/actor_420700_animation_0A464_bank4.inc"
};

AnimationRecord D_actor_420700_8013C0DC[96] = {
#include "assets/actor_420700_animation_0A464_records.inc"
};

u16 D_actor_420700_8013C25C[20] = {
#include "assets/actor_420700_animation_0A464_indices.inc"
};

AnimationSet D_actor_420700_8013C284 = {
    D_actor_420700_8013C0DC,
    D_actor_420700_8013C25C,
    { NULL, D_actor_420700_8013C04C, NULL, NULL, D_actor_420700_8013C064, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013C2AC[2] = {
#include "assets/actor_420700_animation_0A610_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013C2C4[20] = {
#include "assets/actor_420700_animation_0A610_bank4.inc"
};

AnimationRecord D_actor_420700_8013C314[61] = {
#include "assets/actor_420700_animation_0A610_records.inc"
};

u16 D_actor_420700_8013C408[20] = {
#include "assets/actor_420700_animation_0A610_indices.inc"
};

AnimationSet D_actor_420700_8013C430 = {
    D_actor_420700_8013C314,
    D_actor_420700_8013C408,
    { NULL, D_actor_420700_8013C2AC, NULL, NULL, D_actor_420700_8013C2C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013C458[2] = {
#include "assets/actor_420700_animation_0A7BC_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013C470[20] = {
#include "assets/actor_420700_animation_0A7BC_bank4.inc"
};

AnimationRecord D_actor_420700_8013C4C0[61] = {
#include "assets/actor_420700_animation_0A7BC_records.inc"
};

u16 D_actor_420700_8013C5B4[20] = {
#include "assets/actor_420700_animation_0A7BC_indices.inc"
};

AnimationSet D_actor_420700_8013C5DC = {
    D_actor_420700_8013C4C0,
    D_actor_420700_8013C5B4,
    { NULL, D_actor_420700_8013C458, NULL, NULL, D_actor_420700_8013C470, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013C604[2] = {
#include "assets/actor_420700_animation_0A96C_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013C61C[22] = {
#include "assets/actor_420700_animation_0A96C_bank4.inc"
};

AnimationRecord D_actor_420700_8013C674[60] = {
#include "assets/actor_420700_animation_0A96C_records.inc"
};

u16 D_actor_420700_8013C764[20] = {
#include "assets/actor_420700_animation_0A96C_indices.inc"
};

AnimationSet D_actor_420700_8013C78C = {
    D_actor_420700_8013C674,
    D_actor_420700_8013C764,
    { NULL, D_actor_420700_8013C604, NULL, NULL, D_actor_420700_8013C61C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013C7B4[2] = {
#include "assets/actor_420700_animation_0AD64_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013C7CC[79] = {
#include "assets/actor_420700_animation_0AD64_bank4.inc"
};

AnimationRecord D_actor_420700_8013C908[149] = {
#include "assets/actor_420700_animation_0AD64_records.inc"
};

u16 D_actor_420700_8013CB5C[20] = {
#include "assets/actor_420700_animation_0AD64_indices.inc"
};

AnimationSet D_actor_420700_8013CB84 = {
    D_actor_420700_8013C908,
    D_actor_420700_8013CB5C,
    { NULL, D_actor_420700_8013C7B4, NULL, NULL, D_actor_420700_8013C7CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013CBAC[2] = {
#include "assets/actor_420700_animation_0AF1C_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013CBC4[24] = {
#include "assets/actor_420700_animation_0AF1C_bank4.inc"
};

AnimationRecord D_actor_420700_8013CC24[60] = {
#include "assets/actor_420700_animation_0AF1C_records.inc"
};

u16 D_actor_420700_8013CD14[20] = {
#include "assets/actor_420700_animation_0AF1C_indices.inc"
};

AnimationSet D_actor_420700_8013CD3C = {
    D_actor_420700_8013CC24,
    D_actor_420700_8013CD14,
    { NULL, D_actor_420700_8013CBAC, NULL, NULL, D_actor_420700_8013CBC4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013CD64[2] = {
#include "assets/actor_420700_animation_0B0E8_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013CD7C[29] = {
#include "assets/actor_420700_animation_0B0E8_bank4.inc"
};

AnimationRecord D_actor_420700_8013CDF0[60] = {
#include "assets/actor_420700_animation_0B0E8_records.inc"
};

u16 D_actor_420700_8013CEE0[20] = {
#include "assets/actor_420700_animation_0B0E8_indices.inc"
};

AnimationSet D_actor_420700_8013CF08 = {
    D_actor_420700_8013CDF0,
    D_actor_420700_8013CEE0,
    { NULL, D_actor_420700_8013CD64, NULL, NULL, D_actor_420700_8013CD7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013CF30[2] = {
#include "assets/actor_420700_animation_0B3B0_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013CF48[40] = {
#include "assets/actor_420700_animation_0B3B0_bank4.inc"
};

AnimationRecord D_actor_420700_8013CFE8[112] = {
#include "assets/actor_420700_animation_0B3B0_records.inc"
};

u16 D_actor_420700_8013D1A8[20] = {
#include "assets/actor_420700_animation_0B3B0_indices.inc"
};

AnimationSet D_actor_420700_8013D1D0 = {
    D_actor_420700_8013CFE8,
    D_actor_420700_8013D1A8,
    { NULL, D_actor_420700_8013CF30, NULL, NULL, D_actor_420700_8013CF48, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013D1F8[2] = {
#include "assets/actor_420700_animation_0B57C_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013D210[29] = {
#include "assets/actor_420700_animation_0B57C_bank4.inc"
};

AnimationRecord D_actor_420700_8013D284[60] = {
#include "assets/actor_420700_animation_0B57C_records.inc"
};

u16 D_actor_420700_8013D374[20] = {
#include "assets/actor_420700_animation_0B57C_indices.inc"
};

AnimationSet D_actor_420700_8013D39C = {
    D_actor_420700_8013D284,
    D_actor_420700_8013D374,
    { NULL, D_actor_420700_8013D1F8, NULL, NULL, D_actor_420700_8013D210, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013D3C4[2] = {
#include "assets/actor_420700_animation_0B7DC_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013D3DC[30] = {
#include "assets/actor_420700_animation_0B7DC_bank4.inc"
};

AnimationRecord D_actor_420700_8013D454[96] = {
#include "assets/actor_420700_animation_0B7DC_records.inc"
};

u16 D_actor_420700_8013D5D4[20] = {
#include "assets/actor_420700_animation_0B7DC_indices.inc"
};

AnimationSet D_actor_420700_8013D5FC = {
    D_actor_420700_8013D454,
    D_actor_420700_8013D5D4,
    { NULL, D_actor_420700_8013D3C4, NULL, NULL, D_actor_420700_8013D3DC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013D624[2] = {
#include "assets/actor_420700_animation_0B9D0_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013D63C[34] = {
#include "assets/actor_420700_animation_0B9D0_bank4.inc"
};

AnimationRecord D_actor_420700_8013D6C4[65] = {
#include "assets/actor_420700_animation_0B9D0_records.inc"
};

u16 D_actor_420700_8013D7C8[20] = {
#include "assets/actor_420700_animation_0B9D0_indices.inc"
};

AnimationSet D_actor_420700_8013D7F0 = {
    D_actor_420700_8013D6C4,
    D_actor_420700_8013D7C8,
    { NULL, D_actor_420700_8013D624, NULL, NULL, D_actor_420700_8013D63C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013D818[2] = {
#include "assets/actor_420700_animation_0BC64_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013D830[39] = {
#include "assets/actor_420700_animation_0BC64_bank4.inc"
};

AnimationRecord D_actor_420700_8013D8CC[100] = {
#include "assets/actor_420700_animation_0BC64_records.inc"
};

u16 D_actor_420700_8013DA5C[20] = {
#include "assets/actor_420700_animation_0BC64_indices.inc"
};

AnimationSet D_actor_420700_8013DA84 = {
    D_actor_420700_8013D8CC,
    D_actor_420700_8013DA5C,
    { NULL, D_actor_420700_8013D818, NULL, NULL, D_actor_420700_8013D830, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013DAAC[2] = {
#include "assets/actor_420700_animation_0BE58_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013DAC4[34] = {
#include "assets/actor_420700_animation_0BE58_bank4.inc"
};

AnimationRecord D_actor_420700_8013DB4C[65] = {
#include "assets/actor_420700_animation_0BE58_records.inc"
};

u16 D_actor_420700_8013DC50[20] = {
#include "assets/actor_420700_animation_0BE58_indices.inc"
};

AnimationSet D_actor_420700_8013DC78 = {
    D_actor_420700_8013DB4C,
    D_actor_420700_8013DC50,
    { NULL, D_actor_420700_8013DAAC, NULL, NULL, D_actor_420700_8013DAC4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013DCA0[2] = {
#include "assets/actor_420700_animation_0C150_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013DCB8[60] = {
#include "assets/actor_420700_animation_0C150_bank4.inc"
};

AnimationRecord D_actor_420700_8013DDA8[104] = {
#include "assets/actor_420700_animation_0C150_records.inc"
};

u16 D_actor_420700_8013DF48[20] = {
#include "assets/actor_420700_animation_0C150_indices.inc"
};

AnimationSet D_actor_420700_8013DF70 = {
    D_actor_420700_8013DDA8,
    D_actor_420700_8013DF48,
    { NULL, D_actor_420700_8013DCA0, NULL, NULL, D_actor_420700_8013DCB8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013DF98[4] = {
#include "assets/actor_420700_animation_0C4D0_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013DFC8[67] = {
#include "assets/actor_420700_animation_0C4D0_bank4.inc"
};

AnimationRecord D_actor_420700_8013E0D4[125] = {
#include "assets/actor_420700_animation_0C4D0_records.inc"
};

u16 D_actor_420700_8013E2C8[20] = {
#include "assets/actor_420700_animation_0C4D0_indices.inc"
};

AnimationSet D_actor_420700_8013E2F0 = {
    D_actor_420700_8013E0D4,
    D_actor_420700_8013E2C8,
    { NULL, D_actor_420700_8013DF98, NULL, NULL, D_actor_420700_8013DFC8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013E318[2] = {
#include "assets/actor_420700_animation_0C764_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013E330[34] = {
#include "assets/actor_420700_animation_0C764_bank4.inc"
};

AnimationRecord D_actor_420700_8013E3B8[105] = {
#include "assets/actor_420700_animation_0C764_records.inc"
};

u16 D_actor_420700_8013E55C[20] = {
#include "assets/actor_420700_animation_0C764_indices.inc"
};

AnimationSet D_actor_420700_8013E584 = {
    D_actor_420700_8013E3B8,
    D_actor_420700_8013E55C,
    { NULL, D_actor_420700_8013E318, NULL, NULL, D_actor_420700_8013E330, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013E5AC[2] = {
#include "assets/actor_420700_animation_0C9CC_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013E5C4[43] = {
#include "assets/actor_420700_animation_0C9CC_bank4.inc"
};

AnimationRecord D_actor_420700_8013E670[85] = {
#include "assets/actor_420700_animation_0C9CC_records.inc"
};

u16 D_actor_420700_8013E7C4[20] = {
#include "assets/actor_420700_animation_0C9CC_indices.inc"
};

AnimationSet D_actor_420700_8013E7EC = {
    D_actor_420700_8013E670,
    D_actor_420700_8013E7C4,
    { NULL, D_actor_420700_8013E5AC, NULL, NULL, D_actor_420700_8013E5C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013E814[2] = {
#include "assets/actor_420700_animation_0CC08_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013E82C[25] = {
#include "assets/actor_420700_animation_0CC08_bank4.inc"
};

AnimationRecord D_actor_420700_8013E890[92] = {
#include "assets/actor_420700_animation_0CC08_records.inc"
};

u16 D_actor_420700_8013EA00[20] = {
#include "assets/actor_420700_animation_0CC08_indices.inc"
};

AnimationSet D_actor_420700_8013EA28 = {
    D_actor_420700_8013E890,
    D_actor_420700_8013EA00,
    { NULL, D_actor_420700_8013E814, NULL, NULL, D_actor_420700_8013E82C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013EA50[2] = {
#include "assets/actor_420700_animation_0CF0C_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013EA68[65] = {
#include "assets/actor_420700_animation_0CF0C_bank4.inc"
};

AnimationRecord D_actor_420700_8013EB6C[102] = {
#include "assets/actor_420700_animation_0CF0C_records.inc"
};

u16 D_actor_420700_8013ED04[20] = {
#include "assets/actor_420700_animation_0CF0C_indices.inc"
};

AnimationSet D_actor_420700_8013ED2C = {
    D_actor_420700_8013EB6C,
    D_actor_420700_8013ED04,
    { NULL, D_actor_420700_8013EA50, NULL, NULL, D_actor_420700_8013EA68, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_420700_8013ED54[2] = {
#include "assets/actor_420700_animation_0D100_bank1.inc"
};

AnimationPackedRotation D_actor_420700_8013ED6C[28] = {
#include "assets/actor_420700_animation_0D100_bank4.inc"
};

AnimationRecord D_actor_420700_8013EDDC[71] = {
#include "assets/actor_420700_animation_0D100_records.inc"
};

u16 D_actor_420700_8013EEF8[20] = {
#include "assets/actor_420700_animation_0D100_indices.inc"
};

AnimationSet D_actor_420700_8013EF20 = {
    D_actor_420700_8013EDDC,
    D_actor_420700_8013EEF8,
    { NULL, D_actor_420700_8013ED54, NULL, NULL, D_actor_420700_8013ED6C, NULL, NULL, NULL },
};

Actor420700MessageEntry D_actor_420700_8013EF48[4] = {
    { 2003, { .call0 = func_actor_420700_80132644 } },
    { 2005, { .call2 = func_actor_420700_801326F4 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_420700_80132784 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_420700_8013EF68[3] = {
    { TASK_BODY_TMD, 192, func_actor_420700_80132340, { .model = &D_actor_420700_8013B68C } },
    { TASK_BODY_TMD, 192, func_actor_420700_801323D8, { .model = &D_actor_420700_8013BB64 } },
    { TASK_BODY_TMD, 192, func_actor_420700_801327EC, { .model = &D_actor_420700_8013C028 } },
};

u8 D_actor_420700_8013EF8C[84] = {
    0,
    0,
    0,
    0,
    132,
    194,
    19,
    128,
    48,
    196,
    19,
    128,
    220,
    197,
    19,
    128,
    140,
    199,
    19,
    128,
    132,
    203,
    19,
    128,
    60,
    205,
    19,
    128,
    8,
    207,
    19,
    128,
    208,
    209,
    19,
    128,
    156,
    211,
    19,
    128,
    252,
    213,
    19,
    128,
    240,
    215,
    19,
    128,
    132,
    218,
    19,
    128,
    120,
    220,
    19,
    128,
    112,
    223,
    19,
    128,
    240,
    226,
    19,
    128,
    132,
    229,
    19,
    128,
    236,
    231,
    19,
    128,
    40,
    234,
    19,
    128,
    44,
    237,
    19,
    128,
    32,
    239,
    19,
    128,
};

Actor420700Work* D_actor_420700_8013EFE0;

Task* D_actor_420700_8013EFE4;

Task* D_actor_420700_8013EFE8;

Task* D_actor_420700_8013EFEC;

s32 D_actor_420700_8013EFF0;

s32 D_actor_420700_8013EFF4;

static void func_actor_420700_80131E24(GpEnemy* enemy, Task* task);
static void func_actor_420700_80132064(GpEnemy* enemy, Task* task);

/// Step 0 of the `func_actor_420700_80132340` dispatcher: allocate and publish the
/// work block, spawn the two model tasks, texture the first from the placement
/// the actor was spawned from, then seed the model's matrices and animation
/// context before running the first step body.
static void func_actor_420700_80131E24(GpEnemy* enemy, Task* task)
{
    VECTOR     vec;
    GfxCoord*  coord;
    TmdObject* obj;
    void*      work;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x5A0, 0);
    D_actor_420700_8013EFE0 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_420700_8013239C;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->flags                   = 0;
    D_actor_420700_8013EFE4      = task;
    D_actor_420700_8013EFE8      = Task_SpawnFromTable(D_actor_420700_8013EF68, 1, 0, 0);
    D_actor_420700_8013EFEC      = Task_SpawnFromTable(D_actor_420700_8013EF68, 2, 0, 0);
    actorTintTask(D_actor_420700_8013EFE8, enemy);
    obj->lightMtx           = &D_actor_420700_8013EFE0->light;
    obj->colorMtx           = &D_actor_420700_8013EFE0->color;
    D_actor_420700_8013EFF0 = 0;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_420700_8013EFF4 = 0x96;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_420700_8013EFE0->rig.anim, D_actor_420700_8013EF8C, obj,
                  D_actor_420700_8013EFE0->rig.poses, D_actor_420700_8013EFE0->rig.slots);
    D_actor_420700_8013EFE0->st.animId = 5;
    D_actor_420700_8013EFE0->st.state  = 2;
    task->msgTable                     = D_actor_420700_8013EF48;
    func_actor_420700_80132478(task);
    task->state++;
}

/// Step 1 of the `func_actor_420700_80132340` dispatcher: refresh the model's third
/// coordinate and colour the actor from its world translation, run
/// `func_actor_420700_80132478`, then step the `st.field_8` ramp by the mode in
/// `st.field_6` and pass it as the weight of `func_800B0928` aimed at the
/// slot-3 task (modes 0, 1 and 2) or of `func_800B0CF4` aimed at a fixed
/// world point (mode 3).
///
/// Mode 0 chooses its own step each frame: +0x40 while the actor lies behind
/// the slot-3 actor's `field_52` heading, -0x80 otherwise or while an event
/// is running. In that mode the animation slots after the first are held
/// (rate 0) once the ramp is off zero; otherwise they run at one frame per
/// tick.
static void func_actor_420700_80132064(GpEnemy* enemy, Task* task)
{
    VECTOR     pos;
    GfxCoord   target[2];
    GfxCoord*  coords;
    GfxCoord*  player;
    GfxCoord*  part;
    GameActor* actor;
    s32        dx;
    s32        dz;
    s32        c;
    s32        i;
    u8         rate;

    coords = task->extra.tmd->coords;
    part   = &coords[2];
    player = gameGetPtrSlot(3)->extra.tmd->coords;
    Gp_UpdateCoord(part);
    pos.vx = part->workm.t[0];
    pos.vy = part->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    func_actor_420700_80132478(task);
    rate = 0x10;
    if (D_actor_420700_8013EFE0->st.field_6 != 0) {
        if (D_actor_420700_8013EFE0->st.field_6 == 1 || D_actor_420700_8013EFE0->st.field_6 == 3) {
            D_actor_420700_8013EFE0->st.field_8 += 0x80;
            if (D_actor_420700_8013EFE0->st.field_8 > 0x1000) {
                D_actor_420700_8013EFE0->st.field_8 = 0x1000;
            }
        } else {
            D_actor_420700_8013EFE0->st.field_8 -= 0x80;
            if (D_actor_420700_8013EFE0->st.field_8 < 0) {
                D_actor_420700_8013EFE0->st.field_8 = 0;
            }
        }
        if (D_actor_420700_8013EFE0->st.field_6 == 3) {
            target[0].coord.t[0] = 0x1173;
            target[0].coord.t[1] = 0;
            target[0].coord.t[2] = -0x733;
            func_800B0CF4(task, target, 0x200, 0x100, D_actor_420700_8013EFE0->st.field_8);
        } else {
            func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, D_actor_420700_8013EFE0->st.field_8);
        }
    } else {
        if (gGameSession->eventState == 0) {
            dx    = coords->coord.t[0] - player->coord.t[0];
            dz    = coords->coord.t[2] - player->coord.t[2];
            actor = (GameActor*)gameGetPtrSlot(3)->work;
            c     = rcos(actor->field_52);
            if (dx * rsin(actor->field_52) + dz * c < 0) {
                D_actor_420700_8013EFF0 = 0x40;
            } else {
                D_actor_420700_8013EFF0 = -0x80;
            }
            if (D_actor_420700_8013EFE0->st.field_8 != 0) {
                rate = 0;
            }
        } else {
            D_actor_420700_8013EFF0 = -0x80;
        }
        D_actor_420700_8013EFE0->st.field_8 += D_actor_420700_8013EFF0;
        if (D_actor_420700_8013EFE0->st.field_8 > 0x1000) {
            D_actor_420700_8013EFE0->st.field_8 = 0x1000;
        }
        if (D_actor_420700_8013EFE0->st.field_8 < 0) {
            D_actor_420700_8013EFE0->st.field_8 = 0;
        }
        func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, D_actor_420700_8013EFE0->st.field_8);
    }
    for (i = 1; i < 0x14; i++) {
        D_actor_420700_8013EFE0->rig.slots[i].rate = rate;
    }
}

/// Task handler of the actor: republishes the task's work block in
/// `D_actor_420700_8013EFE0`, so the rest of the overlay can reach it without
/// the task, then runs the handler for the task's state from a two-entry table
/// built on the stack -- the spawn step `func_actor_420700_80131E24` or the
/// per-frame step `func_actor_420700_80132064`.
void func_actor_420700_80132340(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_420700_80131E24,
        func_actor_420700_80132064,
    };

    D_actor_420700_8013EFE0 = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Exit callback of the actor's task: kills the frame-4 model task and
/// destroys the enemy.
static void func_actor_420700_8013239C(Task* arg0)
{
    taskKill(D_actor_420700_8013EFE8);
    Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
}

/// State handler of the frame-4 model task: the spawn tick clears its root
/// coordinate's `composeStamp` and the model's flags, which leaves it visible, and hangs
/// the root off frame 4 of the actor's own model, stepping to state 1; every
/// later tick hands the actor model's root translation, dropped by 0x320 in y,
/// to `func_800D7A9C` for the model's colour matrix.
void func_actor_420700_801323D8(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = D_actor_420700_8013EFE4->extra.tmd->coords;
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

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. Steps 1 and 2 each return through their own copy of the
/// advance; the two are identical, so jump.c cross-jumps them and only the
/// second survives.
static void func_actor_420700_80132478(Task* task)
{
    if (D_actor_420700_8013EFE0->st.state == 1) {
        func_actor_420700_801325C8();
        D_actor_420700_8013EFE0->st.state = 3;
        return;
    }
    if (D_actor_420700_8013EFE0->st.state == 2) {
        func_actor_420700_80132538();
        D_actor_420700_8013EFE0->st.state = 3;
        return;
    }
    if (D_actor_420700_8013EFE0->st.state == 3) {
        func_actor_420700_801324EC();
    }
}

/// Advances animation slots 1..0x13 of the work block by one tick.
static void func_actor_420700_801324EC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_420700_8013EFE0->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Sets the rate of animation slots 1..0x13 to 1 and resets each of them to
/// the current animation id, then records that id as the one now playing.
static void func_actor_420700_80132538(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_420700_8013EFE0->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_420700_8013EFE0->rig.anim, i, D_actor_420700_8013EFE0->st.animId);
        i++;
    } while (i < 0x14);
    D_actor_420700_8013EFE0->st.appliedAnimId = D_actor_420700_8013EFE0->st.animId;
}

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
static void func_actor_420700_801325C8(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_420700_8013EFE0->rig.anim, i, D_actor_420700_8013EFE0->st.animId, 0, 8);
        i++;
    } while (i < 0x14);
    D_actor_420700_8013EFE0->st.appliedAnimId = D_actor_420700_8013EFE0->st.animId;
}

/// Starts the requested local clip, translating its bank selector to a clip offset.
///
/// Selectors 1 and 2 add 10 and 17 respectively; other selectors add zero.
/// Rejects clip ids 21 and above. A nonzero blend request selects an
/// eight-frame transition; the requested duration is unused.
s32 func_actor_420700_80132644(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    s32              offset;
    Actor420700Work* work;

    if (args->animationId < 0x15) {
        switch (args->source.index) {
            case 1:
                offset = 0xA;
                break;
            case 2:
                offset = 0x11;
                break;
            default:
                offset = 0;
                break;
        }
        work            = D_actor_420700_8013EFE0;
        work->st.animId = (u16)args->animationId + offset;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state = 1;
        } else {
            work->st.state = 2;
        }
        D_actor_420700_8013EFE0->st.field_A = 0;
        func_actor_420700_80132478(D_actor_420700_8013EFE4);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: rewrites the flags of the actor's three models -- its
/// own and those of the frame-4 and frame-8 model tasks. Bit 0 of the argument
/// shows all three (flags 0) when set and hides them (0x80) when clear; bit 1
/// then ORs 0x4 into all three. Always returns 0.
///
/// The argument is the handler table's third slot, not the second, so the three
/// objects it loads land in `$a3` / `$a0` / `$v1` rather than shifted one down.
s32 func_actor_420700_801326F4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* actor = D_actor_420700_8013EFE4->extra.tmd;
    TmdObject* model = D_actor_420700_8013EFE8->extra.tmd;
    TmdObject* twin  = D_actor_420700_8013EFEC->extra.tmd;

    if (arg2 & 1) {
        actor->flags = 0;
        model->flags = 0;
        twin->flags  = 0;
    } else {
        actor->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        twin->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        actor->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        twin->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Message 0x7DB handler: records the `st.field_6` mode the ramp
/// `func_actor_420700_80132064` runs and seeds `st.field_8` at the end that mode
/// walks away from -- 0 for the rising modes 1 and 3, 0x1000 for the falling
/// mode 2. Mode 0 is accepted as a no-op, and a block whose leading id is not
/// 0x1B02 is rejected with -1 without touching the work block.
///
/// The empty `case 0` is what the decision tree is built from: with the three
/// live cases alone GCC balances the list at the middle node and comes out one
/// test short, and adding the fourth node is what makes it split at the first
/// case instead. See DECOMPILATION_LEARNINGS.md, "An empty case node changes
/// the switch decision tree".
s32 func_actor_420700_80132784(Task* task, s32 arg1, ActorCommand* args)
{
    if (args->context.key != 0x1B02) {
        return -1;
    }
    D_actor_420700_8013EFE0->st.field_6 = args->command;
    switch (args->command) {
        case 0:
            break;
        case 1:
        case 3:
            D_actor_420700_8013EFE0->st.field_8 = 0;
            break;
        case 2:
            D_actor_420700_8013EFE0->st.field_8 = 0x1000;
            break;
    }
    return 0;
}

/// State handler of the frame-8 model task: the same as the frame-4 one,
/// `func_actor_420700_801323D8`, except that it hangs its root off frame 8 of
/// the actor's own model and its spawn tick also sets the model's `otOffset` to
/// -2.
void func_actor_420700_801327EC(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = D_actor_420700_8013EFE4->extra.tmd->coords;
    GfxCoord*  part  = parts + 8;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            extra->flags        = 0;
            extra->otOffset     = -2;
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
