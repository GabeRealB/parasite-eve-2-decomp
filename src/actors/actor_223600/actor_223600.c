#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/coord_math.h"
#include "../../shared/actor_messages.h"

/// The actor's per-instance work block, allocated by the spawn state and
/// reached through `Task::work`. Only the fields the actor's code touches are
/// modelled: the state word and its change latch, the animation context and
/// slots, the motion fields `animDriverTick` drives, the bytes a
/// message handler copies out of its event packet, the spawn and target
/// positions, the model's light and colour matrices and the park latch.
typedef struct Actor223600Work {
    /* 0x000 */ s16              field_0; ///< state
    /* 0x002 */ s16              field_2; ///< state at the previous dispatch
    /* 0x004 */ s16              field_4; ///< set when `field_0` moved away from `field_2`
    /* 0x006 */ s16              field_6; ///< frames spent in the approach state
    /* 0x008 */ s16              field_8;
    /* 0x00A */ byte             pad_A[0x2];
    /* 0x00C */ AnimationContext anim;     ///< `func_800B3F84` arg0
    /* 0x020 */ AnimationSlot    slots[1]; ///< slots 1.. continue past here, overlapping the fields below
    /* 0x048 */ byte             pad_48[0x2];
    /* 0x04A */ u16              field_4A; ///< low ten bits: current animation id
    /* 0x04C */ byte             pad_4C[0xC];
    /* 0x058 */ u16              field_58;
    /* 0x05A */ byte             pad_5A[0xB6];
    /* 0x110 */ byte             poses[0x60]; ///< `func_800B3F84` arg3
    /* 0x170 */ s16              field_170;
    /* 0x172 */ s16              field_172;
    /* 0x174 */ s16              field_174; ///< motion state
    /* 0x176 */ u16              field_176;
    /* 0x178 */ s16              field_178;
    /* 0x17A */ s16              field_17A; ///< frames since the motion last restarted
    /* 0x17C */ s16              field_17C; ///< frames since then with `field_58` bit 1 set
    /* 0x17E */ s16              field_17E;
    /* 0x180 */ u8               field_180;
    /* 0x181 */ u8               field_181;
    /* 0x182 */ u8               field_182;
    /* 0x183 */ byte             pad_183[0x1];
    /* 0x184 */ u16              field_184;
    /* 0x186 */ u16              field_186;
    /* 0x188 */ byte             pad_188[0xC];
    /// World X/Y/Z of the model's coordinate, narrowed to 16 bits as the spawn
    /// handler samples the low 16 bits of each local translation component.
    /* 0x194 */ u16  field_194;
    /* 0x196 */ u16  field_196;
    /* 0x198 */ u16  field_198;
    /* 0x19A */ byte pad_19A[0x2];
    /// Target the approach state steers towards: world X in `field_19C` and
    /// world Z in `field_1A0`, both seeded from the spawn point.
    /* 0x19C */ u16    field_19C;
    /* 0x19E */ s16    field_19E;
    /* 0x1A0 */ u16    field_1A0;
    /* 0x1A2 */ byte   pad_1A2[0x6];
    /* 0x1A8 */ MATRIX field_1A8; ///< installed at `TmdObject.lightMtx`
    /* 0x1C8 */ MATRIX field_1C8; ///< installed at `TmdObject.colorMtx`
    /* 0x1E8 */ byte   pad_1E8[0x20];
    /* 0x208 */ u16    field_208; ///< animation id that last raised the reaction
    /* 0x20A */ byte   pad_20A[0x2];
    /* 0x20C */ s8     field_20C; ///< 1 while the model's coordinate is zeroed
    /* 0x20D */ byte   pad_20D[0x5];
    /// Per-frame height step the parked state adds to the model's world Y,
    /// seeded by the motion the tick enters and retuned as it advances.
    /* 0x212 */ s16 field_212;
} Actor223600Work;
STATIC_ASSERT_SIZEOF(Actor223600Work, 0x214);

/// 0xC-byte scratch taken from `0x1F8003FC` by the approach state: the XZ
/// offset from the model to its target, and the yaw step derived from it.
typedef struct Actor223600Turn {
    /* 0x0 */ s16  dx;
    /* 0x2 */ s16  dy;
    /* 0x4 */ s16  dz;
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s16  yaw;
    /* 0xA */ byte pad_A[0x2];
} Actor223600Turn;
STATIC_ASSERT_SIZEOF(Actor223600Turn, 0xC);

/// Effect record the spawn handler fills with the instance's own coordinate
/// and the 0x100 / 1 argument pair.
extern EffectSpawnArg D_actor_223600_80150B5C;

/// Enemy parameters the spawn handler installs at `Enemy::param`.
extern EnemyParams D_actor_223600_8014CFCC;

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern AnimationSet* D_actor_223600_801509C0[26];

/// Event packet handed to this actor's message handlers. Its first three bytes
/// are copied into the work block, and its first four are then re-read as two
/// little-endian `u16` words: a command word and a sub-command.
typedef union Actor223600Event {
    /* 0x0 */ u8  bytes[4];
    /* 0x0 */ u16 words[2];
} Actor223600Event;
STATIC_ASSERT_SIZEOF(Actor223600Event, 0x4);

/// Message table the spawn handler publishes as `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, Actor223600Event*);
        s32 (*call1)(Task*, s32, ActorTransform*);
        s32 (*call2)(Task*, s32, s32);
    } handler;
} Actor223600MessageEntry;
STATIC_ASSERT_SIZEOF(Actor223600MessageEntry, 8);

extern Actor223600MessageEntry D_actor_223600_80150B28[4];

/// Integer part of the last movement step `func_actor_223600_8014AA04`
/// applied.
static SVECTOR ActorContact_ScratchPosition;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static void func_actor_223600_8014CF3C(Enemy* arg0, Task* arg1);

extern TmdSource D_actor_223600_8014E660;
void             func_actor_223600_8014CF6C(Task*);

s32 func_actor_223600_8014CC04(Task*, s32, s32);
s32 func_actor_223600_8014CCD4(Task*, s32, Actor223600Event*);

#include "../../shared/actor_contacts.h"
#include "../../shared/anim_driver.h"

DamageAttack D_actor_223600_8014CFC8[1] = {
    { 24, 7 },
};

EnemyParams D_actor_223600_8014CFCC = { D_actor_223600_8014CFC8, 1, 6, 20, 3, 100, 0, 100, 0 };

PadScriptCmd D_actor_223600_8014CFDC[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_223600_8014CFE8[3] = {
    { 0, 0, 9, 0 },
    { 255, 255, 12, 1 },
    { 100, 50, 6, 1 },
};

TmdBone D_actor_223600_8014CFF4[6] = {
#include "assets/actor_223600_model_04840_skeleton.inc"
};

u32 D_actor_223600_8014D0CC[6] = {
#include "assets/actor_223600_model_04840_partVerts.inc"
};

SVECTOR D_actor_223600_8014D0E4[94] = {
#include "assets/actor_223600_model_04840_verts.inc"
};

SVECTOR D_actor_223600_8014D3D4[94] = {
#include "assets/actor_223600_model_04840_normals.inc"
};

u32 D_actor_223600_8014D6C4[999] = {
#include "assets/actor_223600_model_04840_stream.inc"
};

TmdSource D_actor_223600_8014E660 = {
    0,
    5624,
    1228,
    6,
    D_actor_223600_8014D0CC,
    D_actor_223600_8014D0E4,
    D_actor_223600_8014D3D4,
    D_actor_223600_8014CFF4,
    D_actor_223600_8014D6C4,
};

AnimationPackedPose D_actor_223600_8014E684[4] = {
#include "assets/actor_223600_animation_049A0_bank1.inc"
};

AnimationPackedRotation D_actor_223600_8014E6B4[21] = {
#include "assets/actor_223600_animation_049A0_bank4.inc"
};

AnimationRecord D_actor_223600_8014E708[43] = {
#include "assets/actor_223600_animation_049A0_records.inc"
};

u16 D_actor_223600_8014E7B4[6] = {
#include "assets/actor_223600_animation_049A0_indices.inc"
};

AnimationSet D_actor_223600_8014E7C0 = {
    D_actor_223600_8014E708,
    D_actor_223600_8014E7B4,
    { NULL, D_actor_223600_8014E684, NULL, NULL, D_actor_223600_8014E6B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_8014E7E8[19] = {
#include "assets/actor_223600_animation_04C64_bank1.inc"
};

AnimationPackedRotation D_actor_223600_8014E8CC[36] = {
#include "assets/actor_223600_animation_04C64_bank4.inc"
};

AnimationRecord D_actor_223600_8014E95C[71] = {
#include "assets/actor_223600_animation_04C64_records.inc"
};

u16 D_actor_223600_8014EA78[6] = {
#include "assets/actor_223600_animation_04C64_indices.inc"
};

AnimationSet D_actor_223600_8014EA84 = {
    D_actor_223600_8014E95C,
    D_actor_223600_8014EA78,
    { NULL, D_actor_223600_8014E7E8, NULL, NULL, D_actor_223600_8014E8CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_8014EAAC[20] = {
#include "assets/actor_223600_animation_04F08_bank1.inc"
};

AnimationPackedRotation D_actor_223600_8014EB9C[31] = {
#include "assets/actor_223600_animation_04F08_bank4.inc"
};

AnimationRecord D_actor_223600_8014EC18[65] = {
#include "assets/actor_223600_animation_04F08_records.inc"
};

u16 D_actor_223600_8014ED1C[6] = {
#include "assets/actor_223600_animation_04F08_indices.inc"
};

AnimationSet D_actor_223600_8014ED28 = {
    D_actor_223600_8014EC18,
    D_actor_223600_8014ED1C,
    { NULL, D_actor_223600_8014EAAC, NULL, NULL, D_actor_223600_8014EB9C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_8014ED50[6] = {
#include "assets/actor_223600_animation_050E8_bank1.inc"
};

AnimationPackedRotation D_actor_223600_8014ED98[38] = {
#include "assets/actor_223600_animation_050E8_bank4.inc"
};

AnimationRecord D_actor_223600_8014EE30[51] = {
#include "assets/actor_223600_animation_050E8_records.inc"
};

u16 D_actor_223600_8014EEFC[6] = {
#include "assets/actor_223600_animation_050E8_indices.inc"
};

AnimationSet D_actor_223600_8014EF08 = {
    D_actor_223600_8014EE30,
    D_actor_223600_8014EEFC,
    { NULL, D_actor_223600_8014ED50, NULL, NULL, D_actor_223600_8014ED98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_8014EF30[4] = {
#include "assets/actor_223600_animation_05220_bank1.inc"
};

AnimationPackedRotation D_actor_223600_8014EF60[13] = {
#include "assets/actor_223600_animation_05220_bank4.inc"
};

AnimationRecord D_actor_223600_8014EF94[40] = {
#include "assets/actor_223600_animation_05220_records.inc"
};

u16 D_actor_223600_8014F034[6] = {
#include "assets/actor_223600_animation_05220_indices.inc"
};

AnimationSet D_actor_223600_8014F040 = {
    D_actor_223600_8014EF94,
    D_actor_223600_8014F034,
    { NULL, D_actor_223600_8014EF30, NULL, NULL, D_actor_223600_8014EF60, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_8014F068[7] = {
#include "assets/actor_223600_animation_053C0_bank1.inc"
};

AnimationPackedRotation D_actor_223600_8014F0BC[28] = {
#include "assets/actor_223600_animation_053C0_bank4.inc"
};

AnimationRecord D_actor_223600_8014F12C[42] = {
#include "assets/actor_223600_animation_053C0_records.inc"
};

u16 D_actor_223600_8014F1D4[6] = {
#include "assets/actor_223600_animation_053C0_indices.inc"
};

AnimationSet D_actor_223600_8014F1E0 = {
    D_actor_223600_8014F12C,
    D_actor_223600_8014F1D4,
    { NULL, D_actor_223600_8014F068, NULL, NULL, D_actor_223600_8014F0BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_8014F208[6] = {
#include "assets/actor_223600_animation_055D4_bank1.inc"
};

AnimationPackedRotation D_actor_223600_8014F250[42] = {
#include "assets/actor_223600_animation_055D4_bank4.inc"
};

AnimationRecord D_actor_223600_8014F2F8[60] = {
#include "assets/actor_223600_animation_055D4_records.inc"
};

u16 D_actor_223600_8014F3E8[6] = {
#include "assets/actor_223600_animation_055D4_indices.inc"
};

AnimationSet D_actor_223600_8014F3F4 = {
    D_actor_223600_8014F2F8,
    D_actor_223600_8014F3E8,
    { NULL, D_actor_223600_8014F208, NULL, NULL, D_actor_223600_8014F250, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_8014F41C[17] = {
#include "assets/actor_223600_animation_059E4_bank1.inc"
};

AnimationPackedRotation D_actor_223600_8014F4E8[85] = {
#include "assets/actor_223600_animation_059E4_bank4.inc"
};

AnimationRecord D_actor_223600_8014F63C[111] = {
#include "assets/actor_223600_animation_059E4_records.inc"
};

u16 D_actor_223600_8014F7F8[6] = {
#include "assets/actor_223600_animation_059E4_indices.inc"
};

AnimationSet D_actor_223600_8014F804 = {
    D_actor_223600_8014F63C,
    D_actor_223600_8014F7F8,
    { NULL, D_actor_223600_8014F41C, NULL, NULL, D_actor_223600_8014F4E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_8014F82C[12] = {
#include "assets/actor_223600_animation_05CA4_bank1.inc"
};

AnimationPackedRotation D_actor_223600_8014F8BC[52] = {
#include "assets/actor_223600_animation_05CA4_bank4.inc"
};

AnimationRecord D_actor_223600_8014F98C[75] = {
#include "assets/actor_223600_animation_05CA4_records.inc"
};

u16 D_actor_223600_8014FAB8[6] = {
#include "assets/actor_223600_animation_05CA4_indices.inc"
};

AnimationSet D_actor_223600_8014FAC4 = {
    D_actor_223600_8014F98C,
    D_actor_223600_8014FAB8,
    { NULL, D_actor_223600_8014F82C, NULL, NULL, D_actor_223600_8014F8BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_8014FAEC[5] = {
#include "assets/actor_223600_animation_05E7C_bank1.inc"
};

AnimationPackedRotation D_actor_223600_8014FB28[19] = {
#include "assets/actor_223600_animation_05E7C_bank4.inc"
};

AnimationRecord D_actor_223600_8014FB74[71] = {
#include "assets/actor_223600_animation_05E7C_records.inc"
};

u16 D_actor_223600_8014FC90[6] = {
#include "assets/actor_223600_animation_05E7C_indices.inc"
};

AnimationSet D_actor_223600_8014FC9C = {
    D_actor_223600_8014FB74,
    D_actor_223600_8014FC90,
    { NULL, D_actor_223600_8014FAEC, NULL, NULL, D_actor_223600_8014FB28, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_8014FCC4[15] = {
#include "assets/actor_223600_animation_061F8_bank1.inc"
};

AnimationPackedRotation D_actor_223600_8014FD78[70] = {
#include "assets/actor_223600_animation_061F8_bank4.inc"
};

AnimationRecord D_actor_223600_8014FE90[95] = {
#include "assets/actor_223600_animation_061F8_records.inc"
};

u16 D_actor_223600_8015000C[6] = {
#include "assets/actor_223600_animation_061F8_indices.inc"
};

AnimationSet D_actor_223600_80150018 = {
    D_actor_223600_8014FE90,
    D_actor_223600_8015000C,
    { NULL, D_actor_223600_8014FCC4, NULL, NULL, D_actor_223600_8014FD78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_80150040[14] = {
#include "assets/actor_223600_animation_065A8_bank1.inc"
};

AnimationPackedRotation D_actor_223600_801500E8[64] = {
#include "assets/actor_223600_animation_065A8_bank4.inc"
};

AnimationRecord D_actor_223600_801501E8[117] = {
#include "assets/actor_223600_animation_065A8_records.inc"
};

u16 D_actor_223600_801503BC[6] = {
#include "assets/actor_223600_animation_065A8_indices.inc"
};

AnimationSet D_actor_223600_801503C8 = {
    D_actor_223600_801501E8,
    D_actor_223600_801503BC,
    { NULL, D_actor_223600_80150040, NULL, NULL, D_actor_223600_801500E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_801503F0[14] = {
#include "assets/actor_223600_animation_067E4_bank1.inc"
};

AnimationPackedRotation D_actor_223600_80150498[28] = {
#include "assets/actor_223600_animation_067E4_bank4.inc"
};

AnimationRecord D_actor_223600_80150508[60] = {
#include "assets/actor_223600_animation_067E4_records.inc"
};

u16 D_actor_223600_801505F8[6] = {
#include "assets/actor_223600_animation_067E4_indices.inc"
};

AnimationSet D_actor_223600_80150604 = {
    D_actor_223600_80150508,
    D_actor_223600_801505F8,
    { NULL, D_actor_223600_801503F0, NULL, NULL, D_actor_223600_80150498, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_8015062C[10] = {
#include "assets/actor_223600_animation_06A28_bank1.inc"
};

AnimationPackedRotation D_actor_223600_801506A4[40] = {
#include "assets/actor_223600_animation_06A28_bank4.inc"
};

AnimationRecord D_actor_223600_80150744[62] = {
#include "assets/actor_223600_animation_06A28_records.inc"
};

u16 D_actor_223600_8015083C[6] = {
#include "assets/actor_223600_animation_06A28_indices.inc"
};

AnimationSet D_actor_223600_80150848 = {
    D_actor_223600_80150744,
    D_actor_223600_8015083C,
    { NULL, D_actor_223600_8015062C, NULL, NULL, D_actor_223600_801506A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_223600_80150870[6] = {
#include "assets/actor_223600_animation_06B78_bank1.inc"
};

AnimationPackedRotation D_actor_223600_801508B8[20] = {
#include "assets/actor_223600_animation_06B78_bank4.inc"
};

AnimationRecord D_actor_223600_80150908[33] = {
#include "assets/actor_223600_animation_06B78_records.inc"
};

u16 D_actor_223600_8015098C[6] = {
#include "assets/actor_223600_animation_06B78_indices.inc"
};

AnimationSet D_actor_223600_80150998 = {
    D_actor_223600_80150908,
    D_actor_223600_8015098C,
    { NULL, D_actor_223600_80150870, NULL, NULL, D_actor_223600_801508B8, NULL, NULL, NULL },
};

AnimationSet* D_actor_223600_801509C0[26] = {
    NULL,
    &D_actor_223600_8014E7C0,
    &D_actor_223600_8014EA84,
    &D_actor_223600_8014ED28,
    &D_actor_223600_8014EF08,
    &D_actor_223600_8014F040,
    &D_actor_223600_8014F1E0,
    &D_actor_223600_8014F3F4,
    &D_actor_223600_8014F804,
    NULL,
    &D_actor_223600_8014FC9C,
    &D_actor_223600_80150018,
    &D_actor_223600_801503C8,
    &D_actor_223600_80150604,
    &D_actor_223600_80150848,
    &D_actor_223600_80150998,
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

u8 D_actor_223600_80150A28[256] = {
    0,
    0,
    6,
    0,
    0,
    0,
    0,
    0,
    9,
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
    6,
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
    6,
    6,
    6,
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

Actor223600MessageEntry D_actor_223600_80150B28[4] = {
    { 2005, { .call2 = func_actor_223600_8014CC04 } },
    { 2011, { .call0 = func_actor_223600_8014CCD4 } },
    { 2004, { .call1 = actorMsgPlace } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_223600_80150B48 = { TASK_BODY_TMD, 96, func_actor_223600_8014CF6C, { .model = &D_actor_223600_8014E660 } };

static SVECTOR ActorContact_ScratchPosition = { 0 };

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

EffectSpawnArg D_actor_223600_80150B5C = { 0 };

static s32             func_actor_223600_8014B464(Actor223600Work* arg0);
static __inline__ void Actor223600_ScaleForward(SVECTOR* dir, s16 amount);
static __inline__ void Actor223600_MoveForward(GfxCoord* coord, s16 amount);
static void            func_actor_223600_8014B540(Enemy* enemy, Task* task);
static void            func_actor_223600_8014B840(Enemy* enemy, Task* task);
static void            func_actor_223600_8014BBF4(Enemy* enemy, Task* task);
static void            func_actor_223600_8014CA00(Enemy* enemy, Task* task);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/anim_driver_tick.inc.c"

/// In motion states 2 and 3, reports 0x400C0001 the first time the animation id
/// in `field_4A` reaches one of that state's trigger ids (latched in
/// `field_208`); in state 5, 0x400C0005 while bit 2 of `field_58` is set.
/// Returns 0 otherwise.
static s32 func_actor_223600_8014B464(Actor223600Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_208 == v) {
                goto same;
            }
            arg0->field_208 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_208 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_208 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

/// Normalises `dir` in place and scales it to `amount`/0x1000 of unit length on
/// the GTE. The pointer stays in one register across `VectorNormalSS` because
/// the GTE loads read it back afterwards.
static __inline__ void Actor223600_ScaleForward(SVECTOR* dir, s16 amount)
{
    VectorNormalSS(dir, dir);
    gte_lddp(amount);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);
}

/// Steps the model `amount` units along its facing -- the coordinate matrix's z
/// column, normalised and GTE-scaled in a scratch-pad vector -- and invalidates
/// the coordinate. Skipped entirely while `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen` is 1.
static __inline__ void Actor223600_MoveForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        Actor223600_ScaleForward(vec, amount);
        coord->coord.t[0]  += head[-1].vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Spawn state of this enemy: allocates the 0x214 work block, publishes it as
/// `Task::work`, reparents the model to `gGfxViewCoord`, seeds its animation
/// slots from `D_actor_223600_801509C0` and hangs the enemy's display node off
/// part 2 of the model's coordinate array. HP and max HP both come from
/// `D_actor_223600_8014CFCC`, which also picks the opening motion through
/// `animDriverTick`. The placement index in `Enemy::placeKey` biases the
/// initial values in `field_176`, `field_184` and `field_186`: odd indices
/// add the index, even indices subtract half of it. The model's world
/// position is sampled into `field_194`..`field_198` and its facing is
/// normalised and scaled on the GTE, and the instance is published as the
/// overlay's anchor `D_actor_223600_80150B5C`.
static void func_actor_223600_8014B540(Enemy* enemy, Task* task)
{
    SVECTOR          dir;
    Actor223600Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    u32              placementIndex;
    u32              placementParity;
    s32              hp;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(sizeof(Actor223600Work), false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    coord->parent  = &gGfxViewCoord;
    task->msgTable = D_actor_223600_80150B28;
    obj->flags     = 0;
    func_800B3F84(&work->anim, D_actor_223600_801509C0, obj, work->poses, work->slots);

    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->hpMax                  = 1;
    enemy->hp                     = 1;
    enemy->reactionFlags          = 0;
    hp                            = D_actor_223600_8014CFCC.hpMax;
    enemy->param                  = &D_actor_223600_8014CFCC;
    enemy->recs                   = 0;
    enemy->hpMax                  = hp;
    enemy->hp                     = hp;

    work->field_174 = 1;
    work->field_170 = 2;
    work->field_176 = 0x10;
    work->field_178 = 0;
    animDriverTick(task);
    work->field_17E     = 0;
    work->field_8       = 0;
    obj->lightMtx       = &work->field_1A8;
    obj->colorMtx       = &work->field_1C8;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_184     = 5;
    work->field_186     = 0x14;

    placementIndex  = (u16)(enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT);
    placementParity = placementIndex & 1;
    if (placementParity == 1) {
        work->field_176 += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_186 += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_184 += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    } else {
        work->field_176 -= placementIndex >> 1;
        work->field_186 -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_184 -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
    }

    work->field_194 = (u16)task->extra.tmd->coords->coord.t[0];
    work->field_196 = (u16)task->extra.tmd->coords->coord.t[1];
    work->field_198 = (u16)task->extra.tmd->coords->coord.t[2];

    Gfx_MatrixCol2(&task->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    Actor223600_ScaleForward(&dir, 0x3E8);

    work->field_0 = 0;
    work->field_2 = -1;

    D_actor_223600_80150B5C.coord      = task->extra.tmd->coords;
    D_actor_223600_80150B5C.spawnArgLo = 0x100;
    D_actor_223600_80150B5C.spawnArgHi = 1;
    task->state++;
}

/// Approach state of this enemy. On the frame it is entered (`field_4` set) it
/// allocates the model's draw buffers, seeds the target position in
/// `field_19C`/`field_1A0`, writes the starting world position for this
/// context's top `field_8` nibble -- two spawn points, a third leaving the
/// coordinate alone -- faces the model down +Z and restarts its motion. On
/// every later frame it counts the frame in `field_6`, turns the model by up to
/// 0x10 towards the target (the clamped yaw kept in the scratch block) and
/// walks it 5 units forward.
static void func_actor_223600_8014B840(Enemy* enemy, Task* task)
{
    Actor223600Work* work;
    Actor223600Turn* head;
    Actor223600Turn* turn;
    GfxCoord*        coord;
    TmdObject*       obj;
    u32              mode;

    work = (Actor223600Work*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_19C = 0x115D;
        work->field_19E = 1;
        work->field_1A0 = 0x12D5;

        mode = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (mode) {
            case 0:
                task->extra.tmd->coords->coord.t[0] = 0xA8C;
                task->extra.tmd->coords->coord.t[1] = 1;
                task->extra.tmd->coords->coord.t[2] = 0xA28;
                break;
            case 1:
                task->extra.tmd->coords->coord.t[0] = 0x384;
                task->extra.tmd->coords->coord.t[1] = mode;
                task->extra.tmd->coords->coord.t[2] = 0x960;
                break;
        }
        gfxRotMatrixY(&task->extra.tmd->coords->coord, 0, 1);
        work->field_174 = 2;
        work->field_170 = 2;
        animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_6                         = 0;
        return;
    }

    work->field_6++;
    head                                  = SCRATCH_STACK_CURSOR(Actor223600Turn);
    head[-1].dx                           = work->field_19C - (u16)task->extra.tmd->coords->coord.t[0];
    SCRATCH_STACK_CURSOR(Actor223600Turn) = head - 1;
    turn                                  = head - 1;
    turn->dy                              = 0;
    turn->dz                              = work->field_1A0 - (u16)task->extra.tmd->coords->coord.t[2];

    coord     = task->extra.tmd->coords;
    turn->yaw = actorNormalizeYaw(ratan2(head[-1].dx, turn->dz) -
                                  ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    if (turn->yaw > 0x10) {
        turn->yaw = 0x10;
    }
    if (turn->yaw < -0x10) {
        turn->yaw = -0x10;
    }
    turn->yaw += ratan2(-task->extra.tmd->coords->coord.m[2][0],
                        task->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, turn->yaw, 1);
    Actor223600_MoveForward(task->extra.tmd->coords, 5);
    animDriverTick(task);
    SCRATCH_STACK_RELEASE_BLOCK(Actor223600Turn);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Parked state of this enemy. On the frame it is entered (`field_4` set) it
/// allocates the model's draw buffers and drops the model at the spawn point
/// the context's top `field_8` nibble selects -- two of them just face the
/// model and hand it to motion state 2, while the third steps it forward and
/// spins up motion state 0xE, waiting out the restart until `field_58` bit 0
/// comes back. Every later frame it runs one step of the motion the work
/// block's `field_174` names: 0xE raises the model by `field_212` a frame,
/// stepping it forward while the frame counter is inside the walk window, and
/// hands over to 0xF once the model's world Y goes positive; 0xF walks the
/// coordinate along its own axes on the GTE and swings part 1 through the
/// flourish, in a longer form for the nibble-0 context than for the others;
/// state 2 only widens `field_176`. The scratch block comes off
/// the scratch stack under three names -- `head`, whose negative index the
/// world-X step reads, `vec`, which the column and normalise calls take, and
/// `gte`, which the GTE round trip reads back -- and the two the scratch stack
/// pointers are the carve and the release, each materialised where it is used.
static void func_actor_223600_8014BBF4(Enemy* enemy, Task* task)
{
    Actor223600Work*  work;
    Actor223600Turn** push;
    Actor223600Turn** pop;
    Actor223600Turn*  head;
    SVECTOR*          vec;
    SVECTOR*          gte;
    TmdObject*        obj;
    s32               mode;
    s32               state;
    s16               frame;

    work = (Actor223600Work*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        mode = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (mode) {
            case 0:
                task->extra.tmd->coords->coord.t[0] = 0xA1E;
                task->extra.tmd->coords->coord.t[1] = -0x384;
                task->extra.tmd->coords->coord.t[2] = 0x1590;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x7D0, 1);
                work->field_6   = -0xA;
                work->field_212 = 0xB4;
                work->field_174 = 2;
                work->field_170 = 2;
                break;
            case 1:
                task->extra.tmd->coords->coord.t[0] = 0x12C;
                task->extra.tmd->coords->coord.t[1] = -0x4C4;
                task->extra.tmd->coords->coord.t[2] = 0x1194;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x3E8, 1);
                work->field_212 = 0xBE;
                work->field_6   = 0;
                work->field_174 = 2;
                work->field_170 = 2;
                break;
            case 2:
                task->extra.tmd->coords->coord.t[0] = 0x104A;
                task->extra.tmd->coords->coord.t[1] = -0x384;
                task->extra.tmd->coords->coord.t[2] = 0xFE6;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, -0x400, 1);
                Actor223600_MoveForward(task->extra.tmd->coords, 0x15E);
                work->field_6   = 0x3C;
                work->field_212 = 0x50;
                work->field_176 = 0x40;
                work->field_174 = 0xE;
                work->field_170 = mode;
                do {
                    animDriverTick(task);
                } while ((work->field_58 & 1) == 0);
                work->field_176 = 0x10;
                work->field_212 = 0x46;
                break;
        }
        animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        return;
    }

    if (work->field_6 == 0) {
        state = work->field_174;
        if (state == 2) {
            work->field_174 = 0xE;
            work->field_170 = state;
        }
    }
    animDriverTick(task);

    push                                   = (Actor223600Turn**)SCRATCH_HEAD_ADDR;
    head                                   = SCRATCH_HEAD_AT(push, Actor223600Turn);
    vec                                    = (SVECTOR*)(head - 1);
    gte                                    = (SVECTOR*)(head - 1);
    SCRATCH_HEAD_AT(push, Actor223600Turn) = head - 1;

    switch (work->field_174) {
        case 0xE:
            work->field_176 = 0x10;
            if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) != 2) {
                if (work->field_6 < 0x32) {
                    if (work->field_6 >= 0x28) {
                        Actor223600_MoveForward(task->extra.tmd->coords, 0x16);
                    } else if (work->field_6 >= 0x23) {
                        Actor223600_MoveForward(task->extra.tmd->coords, 0xA);
                    } else if (work->field_6 >= 0x13) {
                        Actor223600_MoveForward(task->extra.tmd->coords, 0xA);
                    }
                }
            }
            frame = (u16)work->field_6;
            if (frame >= 0x28) {
                if ((u16)(frame % 5) < 2) {
                    task->extra.tmd->coords->coord.t[1] += work->field_212 - (frame - 0x28) / 4;
                } else {
                    task->extra.tmd->coords->coord.t[1] += work->field_212 + (frame - 0x28) / 2;
                }
                if (task->extra.tmd->coords->coord.t[1] >= 2) {
                    task->extra.tmd->coords->coord.t[1] = 1;
                }
            }
            if (task->extra.tmd->coords->coord.t[1] > 0) {
                work->field_174 = 0xF;
                work->field_170 = 2;
                work->field_6   = 0;
            }
            break;
        case 0xF:
            work->field_176 = 0x10;
            if (work->field_6 < 0xB) {
                Gfx_MatrixCol2(&task->extra.tmd->coords->coord, vec);
                VectorNormalSS(vec, vec);
                gte_lddp(0x23);
                gte_ldsv(gte);
                gte_gpf12();
                gte_stsv(gte);
                task->extra.tmd->coords->coord.t[0] += head[-1].dx;
                task->extra.tmd->coords->coord.t[1] += vec->vy;
                task->extra.tmd->coords->coord.t[2] += vec->vz;
            }
            if ((u32)((u16)work->field_6 - 5) < 9) {
                Gfx_MatrixCol1(&task->extra.tmd->coords->coord, vec);
                VectorNormalSS(vec, vec);
                gte_lddp(-0x14);
                gte_ldsv(gte);
                gte_gpf12();
                gte_stsv(gte);
                task->extra.tmd->coords->coord.t[0] += head[-1].dx;
                task->extra.tmd->coords->coord.t[1] += vec->vy;
                task->extra.tmd->coords->coord.t[2] += vec->vz;
                Gfx_RotMatrixX(&task->extra.tmd->coords[1].coord,
                               -((work->field_6 - 4) * 0xCC), 0);
            }
            if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
                if ((u32)((u16)work->field_6 - 0xE) < 0x17) {
                    Gfx_MatrixCol1(&task->extra.tmd->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(0xB);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    task->extra.tmd->coords->coord.t[0] += head[-1].dx;
                    task->extra.tmd->coords->coord.t[1] += vec->vy;
                    task->extra.tmd->coords->coord.t[2] += vec->vz;
                    Gfx_MatrixCol0(&task->extra.tmd->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(-0x1D);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    switch ((s16)((u16)work->field_6 - 0xF)) {
                        case 0:
                        case 1:
                        case 3:
                        case 4:
                        case 5:
                        case 7:
                        case 9:
                            task->extra.tmd->coords->coord.t[0] -= gte->vx;
                            task->extra.tmd->coords->coord.t[1] -= gte->vy;
                            task->extra.tmd->coords->coord.t[2] -= gte->vz;
                            Gfx_RotMatrixX(&task->extra.tmd->coords[1].coord, -0x800, 0);
                            Gfx_RotMatrixZ(&task->extra.tmd->coords[1].coord,
                                           (work->field_6 - 0xD) * 0x55 - 0x6E, 0);
                            break;
                        default:
                            task->extra.tmd->coords->coord.t[0] += gte->vx;
                            task->extra.tmd->coords->coord.t[1] += gte->vy;
                            task->extra.tmd->coords->coord.t[2] += gte->vz;
                            Gfx_RotMatrixX(&task->extra.tmd->coords[1].coord, -0x800, 0);
                            Gfx_RotMatrixZ(&task->extra.tmd->coords[1].coord,
                                           (work->field_6 - 0xD) * 0x55, 0);
                            break;
                    }
                }
                if (work->field_6 >= 0x25) {
                    Gfx_RotMatrixX(&task->extra.tmd->coords[1].coord, -0x800, 0);
                    Gfx_RotMatrixZ(&task->extra.tmd->coords[1].coord, 0x800, 0);
                }
            } else {
                if ((u32)((u16)work->field_6 - 0xE) < 0x10) {
                    Gfx_MatrixCol1(&task->extra.tmd->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(0xB);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    task->extra.tmd->coords->coord.t[0] += head[-1].dx;
                    task->extra.tmd->coords->coord.t[1] += vec->vy;
                    task->extra.tmd->coords->coord.t[2] += vec->vz;
                    Gfx_MatrixCol0(&task->extra.tmd->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(-0x1D);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    task->extra.tmd->coords->coord.t[0] += head[-1].dx;
                    task->extra.tmd->coords->coord.t[1] += vec->vy;
                    task->extra.tmd->coords->coord.t[2] += vec->vz;
                    Gfx_RotMatrixX(&task->extra.tmd->coords[1].coord, -0x800, 0);
                    Gfx_RotMatrixZ(&task->extra.tmd->coords[1].coord,
                                   (work->field_6 - 0xD) * 0x78, 0);
                }
                if (work->field_6 >= 0x1E) {
                    Gfx_RotMatrixX(&task->extra.tmd->coords[1].coord, -0x800, 0);
                    Gfx_RotMatrixZ(&task->extra.tmd->coords[1].coord, 0x800, 0);
                }
            }
            break;
        case 2:
            work->field_176 = 0x20;
            break;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    pop                                   = (Actor223600Turn**)SCRATCH_HEAD_ADDR;
    SCRATCH_POP_AT(pop, Actor223600Turn);
    work->field_6++;
}

/// The three state handlers the tick below picks between by the work block's
/// state word, copied onto the stack before the call. The copy is a three-word
/// block move out of the unit's `.rodata`, which is why the table is a rodata
/// object rather than a local initialiser.
static const GpEnemyTaskFuncTable3 D_actor_223600_80149E4C = {
    {
        func_actor_223600_8014CF3C,
        func_actor_223600_8014B840,
        func_actor_223600_8014BBF4,
    },
};

/// Per-frame tick of this enemy, entry 1 of `D_actor_223600_80149E58`. The
/// game mode word selects a one-shot arm first: mode 0 clears the model's
/// `field_C` when the work block's state is nonzero and then carries on, mode 1
/// does the same and returns, and mode 2 forces `field_C` to 0x80 and returns.
/// The common path records the state change in `field_4` and the dispatched
/// state in `field_2`, runs the state handler from `D_actor_223600_80149E4C`,
/// turns the animation latch `func_actor_223600_8014B464` raises into a
/// `SndEvt_EnqueueType6` cue -- the top nibble of the enemy's `placeKey` in
/// bits 8-11, with the model's pan and depth -- and finally re-parks the model
/// through `func_800D7A9C` while `field_20C` is set, latching `field_20C` once
/// the session's `viewReady` or a dirty coordinate arrives.
static void func_actor_223600_8014CA00(Enemy* enemy, Task* task)
{
    Actor223600Work*      work;
    GpEnemyTaskFuncTable3 fns;
    s32                   reaction;
    s32                   cue;
    s32                   pan;

    work = (Actor223600Work*)task->work;
    fns  = D_actor_223600_80149E4C;

    switch (Gp_StateF0.field_4) {
        case 0:
            if (work->field_0 != 0) {
                task->extra.tmd->flags = 0;
            }
            break;
        case 1:
            if (work->field_0 != 0) {
                task->extra.tmd->flags = 0;
            }
            return;
        case 2:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    fns.funcs[work->field_0](enemy, task);

    reaction = func_actor_223600_8014B464(work);
    if (reaction != 0) {
        cue = reaction | (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan = (s8)Gp_GetObjPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(
            cue, pan,
            (s8)gpGetObjDepth(task->extra.tmd->coords));
    }
    if (work->field_20C != 0) {
        func_800D7A9C(task->extra.tmd,
                      (VECTOR*)task->extra.tmd->coords->workm.t, 0, 3);
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (task->extra.tmd->coords->composeStamp == GRAPHICS_COORD_DIRTY) {
        work->field_20C = 1;
        return;
    }
    work->field_20C = 0;
}

/// The enemy's three task states -- spawn, per-frame tick and teardown -- which
/// `func_actor_223600_8014CF6C` runs by `Task::state`.
static const GpEnemyTaskFuncTable3 D_actor_223600_80149E58 = {
    {
        func_actor_223600_8014B540,
        func_actor_223600_8014CA00,
        Gp_DestroyEnemy,
    },
};

/// Message handler (id 0x7D5 in `D_actor_223600_80150B28`). Drives the model's
/// `field_C` flag word and the work block's state word from `arg2`: 0 sets 0x80
/// and rewrites the buffers, 1 clears it and rewrites the buffers, 2 sets bit
/// 2, and 3 clears then sets bit 2. Only case 1 keeps `arg2` as the state.
s32 func_actor_223600_8014CC04(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj  = task->extra.tmd;
    Actor223600Work* work = (Actor223600Work*)task->work;

    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = arg2;
            break;
        case 2:
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Message handler (id 0x7DB in `D_actor_223600_80150B28`). Copies the first
/// three bytes of the event packet into the work block, then, for command word
/// 0x302, drives the work block's state word from the packet's sub-command: 1
/// selects 2, 2 and 9 select 0, and 0 is a no-op.
s32 func_actor_223600_8014CCD4(Task* task, s32 arg1, Actor223600Event* event)
{
    Actor223600Work* work;

    work            = (Actor223600Work*)task->work;
    work->field_180 = event->bytes[0];
    work->field_181 = event->bytes[1];
    work->field_182 = event->bytes[2];
    if (event->words[0] == 0x302) {
        switch (event->words[1]) {
            case 9:
                work->field_0 = 0;
                break;
            case 1:
                work->field_0 = 2;
                break;
            case 2:
                work->field_0 = 0;
                break;
            case 0:
                break;
        }
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

#include "../../shared/coord_math_yaw_scale.inc.c"

/// Idle state of this enemy (entry 0 of `D_actor_223600_80149E4C`). On the
/// frame the state is entered (`field_4` set) it marks the enemy not lockable
/// and sets the model's flags to 0x80; it does nothing on later frames.
static void func_actor_223600_8014CF3C(Enemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor223600Work*)arg1->work)->field_4 != 0) {
        model                        = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Runs the handler of `D_actor_223600_80149E58` that `Task::state` selects --
/// spawn, per-frame tick or teardown -- on the enemy in `Task::spawnArg2`,
/// copying the table onto the stack before the call.
void func_actor_223600_8014CF6C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_223600_80149E58;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
