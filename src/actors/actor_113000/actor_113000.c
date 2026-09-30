#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/item_pickup.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

extern GpImgRec D_actor_113000_8013AB6C[2];

/// Animation source table `func_actor_113000_80132208` indexes by the preset's
/// bank index and hands `func_800B3F84` as its data argument.
extern AnimationSet*  D_actor_113000_8013AB8C[9];
extern AnimationSet** D_actor_113000_8013ABB0[1];

/// Work block this actor allocates in its spawn handler and parks in
/// `Task::work`. It is fronted by a `GpAnimCtx`: the start-preset handler
/// passes the block itself, its `slots` array and the pose buffer after them
/// to `func_800B3F84`, and the per-frame tick walks slots 1..0x13. `light` /
/// `color` are the matrices the TMD object's `lightMtx` / `colorMtx` are
/// pointed at.
typedef struct Actor113000Work {
    /* 0x000 */ ActorAnimRig20 rig;
    /// Raised once a preset has started the slots; the per-frame tick only
    /// advances them while it is set.
    /* 0x474 */ s32    field_474;
    /* 0x478 */ s32    field_478; ///< -1 out of the spawn handler
    /* 0x47C */ s32    field_47C; ///< -1 out of the spawn handler
    /* 0x480 */ MATRIX light;
    /* 0x4A0 */ MATRIX color;
    /* 0x4C0 */ s16    field_4C0; ///< upload countdown reload; set to 1 alongside `field_4C4` by mode 3
    /* 0x4C2 */ u16    field_4C2; ///< upload countdown the per-frame state runs down, reloaded from `field_4C0` on underflow
    /* 0x4C4 */ s16    field_4C4; ///< upload step 1..3; set to 1 alongside `field_4C0` by mode 3
    /* 0x4C6 */ s16    field_4C6; ///< cleared by the spawn handler
    /* 0x4C8 */ s16    field_4C8; ///< countdown to `Tmd_FreeBuffers`, idle below 0; -1 out of the spawn handler, 2 from display mode 2
    /* 0x4CA */ byte   pad_4CA[0x2];
} Actor113000Work;
STATIC_ASSERT_SIZEOF(Actor113000Work, 0x4CC);

/// The actor's three texture records, one per mode of the message-0x7E0
/// handler. Each is a lone `GpImgRec` whose 0x20x0x10 source rect repeats the
/// size the upload code's scratch `RECT` carries and whose `data` points at
/// its pixel blob; the three sit 0x420 bytes apart in the overlay's data
/// segment.

/// Message dispatch table the spawn handler parks in `Task::msgTable`:
/// message id / handler pairs, terminated by 0x7FFFFFFF and a null word.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call1)(Task*, s32, GpXformArg*);
        s32 (*call2)(Task*, s32, s32);
        s32 (*call3)(Task*, s32, s32, s32);
    } handler;
} Actor113000MessageEntry;
STATIC_ASSERT_SIZEOF(Actor113000MessageEntry, 8);

extern Actor113000MessageEntry D_actor_113000_8013ABC0[5];

static void func_actor_113000_80131F90(Task* task);
static void func_actor_113000_80132070(Task* task);
static void func_actor_113000_801321A8(Task* task);

/// The actor's three task states, which `func_actor_113000_80131F38` runs by
/// `Task::state`: spawn, per-frame tick and exit.
static const TaskFuncTable3 D_actor_113000_80131E24 = { {
    func_actor_113000_80131F90,
    func_actor_113000_80132070,
    Gp_EnemyTaskExit,
} };

extern AnimationSet D_actor_113000_80137BB4;
extern AnimationSet D_actor_113000_80138014;
extern AnimationSet D_actor_113000_8013839C;
extern AnimationSet D_actor_113000_80138B30;
extern AnimationSet D_actor_113000_801391C8;
extern AnimationSet D_actor_113000_801396D8;
extern AnimationSet D_actor_113000_80139C8C;
extern AnimationSet D_actor_113000_80139F04;
extern TmdSource    D_actor_113000_801378E0;
s32                 func_actor_113000_80132208(Task*, s32, AnimationPlayRequest*, s32);
s32                 func_actor_113000_8013231C(Task*, s32, GpXformArg*);
s32                 func_actor_113000_80132398(Task*, s32, s32, s32);
s32                 func_actor_113000_80132474(Task*, s32, s32);
void                func_actor_113000_80131F38(Task*);

TmdBone D_actor_113000_80132534[20] = {
#include "assets/actor_113000_model_05AC0_skeleton.inc"
};

u32 D_actor_113000_80132804[20] = {
#include "assets/actor_113000_model_05AC0_partVerts.inc"
};

SVECTOR D_actor_113000_80132854[343] = {
#include "assets/actor_113000_model_05AC0_verts.inc"
};

SVECTOR D_actor_113000_8013330C[334] = {
#include "assets/actor_113000_model_05AC0_normals.inc"
};

u32 D_actor_113000_80133D7C[3801] = {
#include "assets/actor_113000_model_05AC0_stream.inc"
};

TmdSource D_actor_113000_801378E0 = {
    0,
    20988,
    5348,
    20,
    D_actor_113000_80132804,
    D_actor_113000_80132854,
    D_actor_113000_8013330C,
    D_actor_113000_80132534,
    D_actor_113000_80133D7C,
};

AnimationPackedPose D_actor_113000_80137904[3] = {
#include "assets/actor_113000_animation_05D94_bank1.inc"
};

AnimationPackedRotation D_actor_113000_80137928[27] = {
#include "assets/actor_113000_animation_05D94_bank4.inc"
};

AnimationRecord D_actor_113000_80137994[126] = {
#include "assets/actor_113000_animation_05D94_records.inc"
};

u16 D_actor_113000_80137B8C[20] = {
#include "assets/actor_113000_animation_05D94_indices.inc"
};

AnimationSet D_actor_113000_80137BB4 = {
    D_actor_113000_80137994,
    D_actor_113000_80137B8C,
    { NULL, D_actor_113000_80137904, NULL, NULL, D_actor_113000_80137928, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_113000_80137BDC[3] = {
#include "assets/actor_113000_animation_061F4_bank1.inc"
};

AnimationPackedRotation D_actor_113000_80137C00[27] = {
#include "assets/actor_113000_animation_061F4_bank4.inc"
};

AnimationRecord D_actor_113000_80137C6C[224] = {
#include "assets/actor_113000_animation_061F4_records.inc"
};

u16 D_actor_113000_80137FEC[20] = {
#include "assets/actor_113000_animation_061F4_indices.inc"
};

AnimationSet D_actor_113000_80138014 = {
    D_actor_113000_80137C6C,
    D_actor_113000_80137FEC,
    { NULL, D_actor_113000_80137BDC, NULL, NULL, D_actor_113000_80137C00, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_113000_8013803C[3] = {
#include "assets/actor_113000_animation_0657C_bank1.inc"
};

AnimationPackedRotation D_actor_113000_80138060[31] = {
#include "assets/actor_113000_animation_0657C_bank4.inc"
};

AnimationRecord D_actor_113000_801380DC[166] = {
#include "assets/actor_113000_animation_0657C_records.inc"
};

u16 D_actor_113000_80138374[20] = {
#include "assets/actor_113000_animation_0657C_indices.inc"
};

AnimationSet D_actor_113000_8013839C = {
    D_actor_113000_801380DC,
    D_actor_113000_80138374,
    { NULL, D_actor_113000_8013803C, NULL, NULL, D_actor_113000_80138060, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_113000_801383C4[7] = {
#include "assets/actor_113000_animation_06D10_bank1.inc"
};

AnimationPackedRotation D_actor_113000_80138418[186] = {
#include "assets/actor_113000_animation_06D10_bank4.inc"
};

AnimationRecord D_actor_113000_80138700[258] = {
#include "assets/actor_113000_animation_06D10_records.inc"
};

u16 D_actor_113000_80138B08[20] = {
#include "assets/actor_113000_animation_06D10_indices.inc"
};

AnimationSet D_actor_113000_80138B30 = {
    D_actor_113000_80138700,
    D_actor_113000_80138B08,
    { NULL, D_actor_113000_801383C4, NULL, NULL, D_actor_113000_80138418, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_113000_80138B58[24] = {
#include "assets/actor_113000_animation_073A8_bank1.inc"
};

AnimationPackedRotation D_actor_113000_80138C78[136] = {
#include "assets/actor_113000_animation_073A8_bank4.inc"
};

AnimationRecord D_actor_113000_80138E98[194] = {
#include "assets/actor_113000_animation_073A8_records.inc"
};

u16 D_actor_113000_801391A0[20] = {
#include "assets/actor_113000_animation_073A8_indices.inc"
};

AnimationSet D_actor_113000_801391C8 = {
    D_actor_113000_80138E98,
    D_actor_113000_801391A0,
    { NULL, D_actor_113000_80138B58, NULL, NULL, D_actor_113000_80138C78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_113000_801391F0[7] = {
#include "assets/actor_113000_animation_078B8_bank1.inc"
};

AnimationPackedRotation D_actor_113000_80139244[108] = {
#include "assets/actor_113000_animation_078B8_bank4.inc"
};

AnimationRecord D_actor_113000_801393F4[175] = {
#include "assets/actor_113000_animation_078B8_records.inc"
};

u16 D_actor_113000_801396B0[20] = {
#include "assets/actor_113000_animation_078B8_indices.inc"
};

AnimationSet D_actor_113000_801396D8 = {
    D_actor_113000_801393F4,
    D_actor_113000_801396B0,
    { NULL, D_actor_113000_801391F0, NULL, NULL, D_actor_113000_80139244, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_113000_80139700[15] = {
#include "assets/actor_113000_animation_07E6C_bank1.inc"
};

AnimationPackedRotation D_actor_113000_801397B4[120] = {
#include "assets/actor_113000_animation_07E6C_bank4.inc"
};

AnimationRecord D_actor_113000_80139994[180] = {
#include "assets/actor_113000_animation_07E6C_records.inc"
};

u16 D_actor_113000_80139C64[20] = {
#include "assets/actor_113000_animation_07E6C_indices.inc"
};

AnimationSet D_actor_113000_80139C8C = {
    D_actor_113000_80139994,
    D_actor_113000_80139C64,
    { NULL, D_actor_113000_80139700, NULL, NULL, D_actor_113000_801397B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_113000_80139CB4[3] = {
#include "assets/actor_113000_animation_080E4_bank1.inc"
};

AnimationPackedRotation D_actor_113000_80139CD8[31] = {
#include "assets/actor_113000_animation_080E4_bank4.inc"
};

AnimationRecord D_actor_113000_80139D54[98] = {
#include "assets/actor_113000_animation_080E4_records.inc"
};

u16 D_actor_113000_80139EDC[20] = {
#include "assets/actor_113000_animation_080E4_indices.inc"
};

AnimationSet D_actor_113000_80139F04 = {
    D_actor_113000_80139D54,
    D_actor_113000_80139EDC,
    { NULL, D_actor_113000_80139CB4, NULL, NULL, D_actor_113000_80139CD8, NULL, NULL, NULL },
};

u_long D_actor_113000_80139F2C[256] = {
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xC8E0D0EA,
    0x7F8E93AA,
    0x7A7A7A7F,
    0x7F7F7A7A,
    0xA6BF9C8A,
    0xA2BEA6BD,
    0x7F7F7F8E,
    0x8E7F7F7F,
    0xA9AC9C8A,
    0xE9D0C8A5,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xC7C6DEEB,
    0x7F8EA1AA,
    0x7A7A7A7A,
    0x7F7F7A7A,
    0xB6BFA28E,
    0x8ABFBDB8,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0xA991A18E,
    0xDBE0C5A5,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xC7C6E6FF,
    0x7F7FA1AA,
    0x7A7A7A7A,
    0x7F7F7A7A,
    0xA6BE9C8E,
    0x8AAFBDA6,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0xA8AFA18E,
    0xE9E0C7B6,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xD1D0E6FF,
    0x7F8EA1AA,
    0x7A7A7F7F,
    0x8E7F7F7A,
    0xBDA8B0A1,
    0xA2ACBABF,
    0x7F7F7F8E,
    0x8E7F7F7F,
    0xBDAEB08A,
    0xE9DEE1B8,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xDEDEE6FF,
    0xBDBCD6DB,
    0xA4A4B3B0,
    0xB0A1A4A4,
    0xC9CBBDAE,
    0xBDC9E1A6,
    0xB3A2A1AF,
    0xBFAFB3B3,
    0xC4DDD5BA,
    0xE9E9DBDD,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xEDE5E6F7,
    0x8FA7F5EA,
    0xCD8BD6BB,
    0xCBCBCDCE,
    0xD2BCD5D2,
    0xE1D0D5A6,
    0xBCCBB8C9,
    0xE4BBD4BC,
    0xEBEBECEC,
    0xE8EEECEA,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xEEE5DEEA,
    0xEDE4EEE5,
    0xA7A7ECEC,
    0xEDA7A78F,
    0xCBD3DBEE,
    0xDBE1C9B8,
    0xA7ECEDEE,
    0xEAEAA7F5,
    0xE4EDECEC,
    0xD9EEEEE4,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xDBDEDEDB,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xDEE9DE8F,
    0xC4DAE7E8,
    0xEBEAECED,
    0xD7F0EBEB,
    0xB8BCE5EB,
    0xDDCBBAA5,
    0xEAEBF0EA,
    0xE4EDEAEA,
    0xDAE7EFEF,
    0xE9E9E9C3,
    0xE9E9E9E9,
    0xDEDEE9E9,
    0xDBDEDEDE,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xD9E9DBE7,
    0xF0F0F0EA,
    0xF0D7D7F4,
    0xECECEAEB,
    0xB8E2D9EC,
    0xE4E1B6A8,
    0xECE4F1EC,
    0xF0F0EBEA,
    0xEC8FEAF0,
    0xE8E8E6F1,
    0xE9E9DBDB,
    0xE2E0DEE9,
    0xDBDED0E0,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xF4FFFFFF,
    0xD8CFD0E6,
    0xFBFDFD8F,
    0xA7D7FFFF,
    0xE5D8EDA7,
    0xBDCBCFD9,
    0xC3E1B6BF,
    0x8FD8DADE,
    0xFFFFF5F3,
    0x7DFEFDF8,
    0xCFCFD9EC,
    0xDBDBDED0,
    0xCAD3E2D0,
    0xCACACACA,
    0xDEE0D3CA,
    0xFFFFFFFF,
    0xF0FFFFFF,
    0xC6C6CFE9,
    0x427AC2BC,
    0xFEFAF6F4,
    0xD9D8BBFE,
    0xAAB8C6E3,
    0xD0C9A8AF,
    0x77E4E8DE,
    0xF6FFF3F2,
    0x56F2F2A7,
    0xB5C8C6CF,
    0xD0DEE0CA,
    0xBACBD5E1,
    0xAAA8AEA6,
    0xE1B8A8AA,
    0xFFFFFFFF,
    0xEAFFFFFF,
    0xCAC9C6E0,
    0xCDC2CAC6,
    0x6E7DA7ED,
    0xE3C4D4CE,
    0xACBDCACF,
    0xE1B6ACB0,
    0xD6DADBE0,
    0xEDA7F36E,
    0xCAC1C1BB,
    0xBFBFBDC8,
    0xD3E0E1BA,
    0xBEBACBCA,
    0xAFAFACBF,
    0xD3B8BEAC,
    0xFFFFFFFF,
    0xE4FFFFFF,
    0xBFC0CAC9,
    0xB5C8B5A8,
    0xBDBEAEAE,
    0xE1D5CACB,
    0xB0AABAE1,
    0xB8BEB09C,
    0xD5D5E1D3,
    0xAEA6CBD3,
    0xBFA6B5A6,
    0xA28E8AB0,
    0xCAD2D5BD,
    0xB2BFBDB8,
    0xAFAFAFAF,
    0xD0CABDAA,
    0xFFFFFFFF,
    0xCCFFFFFF,
    0xA2B2BAB5,
    0xA19C9CA1,
    0xA18A8E8A,
    0xBAAEBFAF,
    0xB0A9A6C8,
    0xBABFA2A1,
    0xBDBDB8C9,
    0xA1A2B0C0,
    0x8EA1A1A2,
    0xA28E8E8E,
    0xBAB8B8BD,
    0xAFAFBFAE,
    0xAEBEBFB2,
    0xDBC6CAB8,
    0xFFFFFFFF,
    0xBBFFFFFF,
    0xA2BEBAB6,
    0x7F7F8E8E,
    0x7F7F7A7A,
    0xBFB3A48E,
    0xA1ADBEBD,
    0xBDB28A8E,
    0xA2B0C0BD,
    0x7F7F7FA4,
    0x7F7F7F7F,
    0xBEA28E7F,
    0xAEBDBABA,
    0xB2AFB0BF,
    0xD2C8BDAE,
    0xDBDBD0E2,
    0xFFFFFFFF,
    0xCCFFFFFF,
    0xB0BDBDA6,
    0x7A7A7F8A,
    0x7F7A7A7A,
    0xB3A48E7F,
    0x8AB0BEC0,
    0xC0B08E7F,
    0x8EA2B2C0,
    0x7A7A7F7F,
    0x7F7F7F7A,
    0xBDC0A48E,
    0xBEAEBDBA,
    0xBDBEB2B2,
    0xD0E0D5B8,
    0xDCE8DBDB,
};

GpImgRec D_actor_113000_8013A32C[2] = {
    { 0, 0, { 0, 0, 32, 16 }, D_actor_113000_80139F2C },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_113000_8013A34C[256] = {
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xC8E0D0EA,
    0x7F8E93AA,
    0x7A7A7A7F,
    0x7F7F7A7A,
    0xA6BF9C8A,
    0xA2BEA6BD,
    0x7F7F7F8E,
    0x8E7F7F7F,
    0xA9AC9C8A,
    0xE9D0C8A5,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xC7C6DEEB,
    0x7F8EA1AA,
    0x7A7A7A7A,
    0x7F7F7A7A,
    0xB6BFA28E,
    0x8ABFBDB8,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0xA991A18E,
    0xDBE0C5A5,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xC7C6E6FF,
    0x7F7FA1AA,
    0x7A7A7A7A,
    0x7F7F7A7A,
    0xA6BE9C8E,
    0x8AAFBDA6,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0xA8AFA18E,
    0xE9E0C7B6,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xD1D0E6FF,
    0x7F8EA1AA,
    0x7A7A7F7F,
    0x8E7F7F7A,
    0xBDA8B0A1,
    0xA2ACBABF,
    0x7F7F7F8E,
    0x8E7F7F7F,
    0xBDAEB08A,
    0xE9DEE1B8,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xDEDEE6FF,
    0xBDBCD6DB,
    0xA4A4B3B0,
    0xB0A1A4A4,
    0xC9CBBDAE,
    0xBDC9E1A6,
    0xB3A2A1AF,
    0xBFAFB3B3,
    0xC4DDD5BA,
    0xE9E9DBDD,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xEDE5E6F7,
    0x8FA7F5EA,
    0xCD8BD6BB,
    0xCBCBCDCE,
    0xD2BCD5D2,
    0xE1D0D5A6,
    0xBCCBB8C9,
    0xE4BBD4BC,
    0xEBEBECEC,
    0xE8EEECEA,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xEEE5DEEA,
    0xEDE4EEE5,
    0xA7A7ECEC,
    0xEDA7A78F,
    0xCBD3DBEE,
    0xDBE1C9B8,
    0xA7ECEDEE,
    0xEAEAA7F5,
    0xE4EDECEC,
    0xD9EEEEE4,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xDBDEDEDB,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xE9E9DE8F,
    0xD8D8D8D9,
    0xA7EDD8D8,
    0xD7A7A7A7,
    0xB8BCE5EB,
    0xDDCBBAA5,
    0xEAEBF0EA,
    0xE4EDEAEA,
    0xDAE7EFEF,
    0xE9E9E9C3,
    0xE9E9E9E9,
    0xDEDEE9E9,
    0xDBDEDEDE,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xD9D9DBE7,
    0xEDD8D9D9,
    0xEDEDEDED,
    0xECECEAED,
    0xB8E2D9EC,
    0xE4E1B6A8,
    0xECE4F1EC,
    0xEDEDECEA,
    0xD8D8D8ED,
    0xE8E8E7E7,
    0xE9E9DBDB,
    0xE2E0DEE9,
    0xDBDED0E0,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xF4FFFFFF,
    0xD8CFD0E6,
    0xF0F0EDD8,
    0xF0F0F0F0,
    0xE5EDEDF0,
    0xBDCBCFD9,
    0xC3E1B6BF,
    0xF0D8DADE,
    0xF0F0F0F0,
    0xEDF0F0F0,
    0xCFCFD9D9,
    0xDBDBDED0,
    0xCAD3E2D0,
    0xCACACACA,
    0xDEE0D3CA,
    0xFFFFFFFF,
    0xF0FFFFFF,
    0xC6C6CFE9,
    0x42F3F0ED,
    0xFEFAF6F4,
    0xEDEDF3F3,
    0xAAB8C6E3,
    0xD0C9A8AF,
    0xEDEDE8DE,
    0xF6F6FEF3,
    0xF0F3FEF4,
    0xB5C8C6ED,
    0xD0DEE0CA,
    0xBACBD5E1,
    0xAAA8AEA6,
    0xE1B8A8AA,
    0xFFFFFFFF,
    0xEAFFFFFF,
    0xCAC9C6E0,
    0xC4C4CAC6,
    0xEDEDEDED,
    0xE3C4D4C4,
    0xACBDCACF,
    0xE1B6ACB0,
    0xC4DADBE0,
    0xEDEDEDC4,
    0xC4C4C4ED,
    0xBFBFBDC8,
    0xD3E0E1BA,
    0xBEBACBCA,
    0xAFAFACBF,
    0xD3B8BEAC,
    0xFFFFFFFF,
    0xE4FFFFFF,
    0xBFC0CAC9,
    0xB5B5B5A8,
    0xBDBEAEAE,
    0xE1D5CACB,
    0xB0AABAE1,
    0xB8BEB09C,
    0xD5D5E1D3,
    0xAEA6CBD3,
    0xBFA6B5A6,
    0xA28E8AB0,
    0xCAD2D5BD,
    0xB2BFBDB8,
    0xAFAFAFAF,
    0xD0CABDAA,
    0xFFFFFFFF,
    0xCCFFFFFF,
    0xA2B2BAB5,
    0xA19C9CA1,
    0xA18A8E8A,
    0xBAAEBFAF,
    0xB0A9A6C8,
    0xBABFA2A1,
    0xBDBDB8C9,
    0xA1A2B0C0,
    0x8EA1A1A2,
    0xA28E8E8E,
    0xBAB8B8BD,
    0xAFAFBFAE,
    0xAEBEBFB2,
    0xDBC6CAB8,
    0xFFFFFFFF,
    0xBBFFFFFF,
    0xA2BEBAB6,
    0x7F7F8E8E,
    0x7F7F7A7A,
    0xBFB3A48E,
    0xA1ADBEBD,
    0xBDB28A8E,
    0xA2B0C0BD,
    0x7F7F7FA4,
    0x7F7F7F7F,
    0xBEA28E7F,
    0xAEBDBABA,
    0xB2AFB0BF,
    0xD2C8BDAE,
    0xDBDBD0E2,
    0xFFFFFFFF,
    0xCCFFFFFF,
    0xB0BDBDA6,
    0x7A7A7F8A,
    0x7F7A7A7A,
    0xB3A48E7F,
    0x8AB0BEC0,
    0xC0B08E7F,
    0x8EA2B2C0,
    0x7A7A7F7F,
    0x7F7F7F7A,
    0xBDC0A48E,
    0xBEAEBDBA,
    0xBDBEB2B2,
    0xD0E0D5B8,
    0xDCE8DBDB,
};

GpImgRec D_actor_113000_8013A74C[2] = {
    { 0, 0, { 0, 0, 32, 16 }, D_actor_113000_8013A34C },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_113000_8013A76C[256] = {
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xC8E0D0EA,
    0x7F8E93AA,
    0x7A7A7A7F,
    0x7F7F7A7A,
    0xA6BF9C8A,
    0xA2BEA6BD,
    0x7F7F7F8E,
    0x8E7F7F7F,
    0xA9AC9C8A,
    0xE9D0C8A5,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xC7C6DEEB,
    0x7F8EA1AA,
    0x7A7A7A7A,
    0x7F7F7A7A,
    0xB6BFA28E,
    0x8ABFBDB8,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0xA991A18E,
    0xDBE0C5A5,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xC7C6E6FF,
    0x7F7FA1AA,
    0x7A7A7A7A,
    0x7F7F7A7A,
    0xA6BE9C8E,
    0x8AAFBDA6,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0xA8AFA18E,
    0xE9E0C7B6,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xD1D0E6FF,
    0x7F8EA1AA,
    0x7A7A7F7F,
    0x8E7F7F7A,
    0xBDA8B0A1,
    0xA2ACBABF,
    0x7F7F7F8E,
    0x8E7F7F7F,
    0xBDAEB08A,
    0xE9DEE1B8,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xDEDEE6FF,
    0xBDBCD6DB,
    0xA4A4B3B0,
    0xB0A1A4A4,
    0xC9CBBDAE,
    0xBDC9E1A6,
    0xB3A2A1AF,
    0xBFAFB3B3,
    0xC4DDD5BA,
    0xE9E9DBDD,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xEDE5E6F7,
    0x8FA7F5EA,
    0xCD8BD6BB,
    0xCBCBCDCE,
    0xD2BCD5D2,
    0xE1D0D5A6,
    0xBCCBB8C9,
    0xE4BBD4BC,
    0xEBEBECEC,
    0xE8EEECEA,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xEEE5DEEA,
    0xEDE4EEE5,
    0xA7A7ECEC,
    0xEDA7A78F,
    0xCBD3DBEE,
    0xDBE1C9B8,
    0xA7ECEDEE,
    0xEAEAA7F5,
    0xE4EDECEC,
    0xD9EEEEE4,
    0xE9E9E9E9,
    0xE9E9E9E9,
    0xDBDEDEDB,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xE9E9DE8F,
    0xD4D4E9E9,
    0xA7EDE9D4,
    0xD7A7A7A7,
    0xB8BCE5EB,
    0xDDCBBAA5,
    0xEAEBF0EA,
    0xD8D8EAEA,
    0xE9E9D8D8,
    0xE9E9E9C3,
    0xE9E9E9E9,
    0xDEDEE9E9,
    0xDBDEDEDE,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xE9D9DBE7,
    0xCBCBD4E9,
    0xD8D4D4CB,
    0xECECEAED,
    0xB8E2D9EC,
    0xE4E1B6A8,
    0xECECECEC,
    0xCBD4D8EC,
    0xE9D4D4CB,
    0xE8E8E9E9,
    0xE9E9DBDB,
    0xE2E0DEE9,
    0xDBDED0E0,
    0xE9E9E9E9,
    0xFFFFFFFF,
    0xF4FFFFFF,
    0xD8CFD0E6,
    0xF0F0EDD8,
    0xF0F0F0F0,
    0xE5E4ECF0,
    0xBDCBCFD9,
    0xC3E1B6BF,
    0xECD8E4DE,
    0xF0F0F0F0,
    0xEDF0F0F0,
    0xCFCFD9D8,
    0xDBDBDED0,
    0xCAD3E2D0,
    0xCACACACA,
    0xDEE0D3CA,
    0xFFFFFFFF,
    0xF0FFFFFF,
    0xC6C6CFE9,
    0xECF0F0ED,
    0xF0ECECEC,
    0xEDECF0F0,
    0xAAB8C6E3,
    0xD0C9A8AF,
    0xF0EDE8DE,
    0xECECECF0,
    0xF0F0F0EC,
    0xB5C8C6ED,
    0xD0DEE0CA,
    0xBACBD5E1,
    0xAAA8AEA6,
    0xE1B8A8AA,
    0xFFFFFFFF,
    0xEAFFFFFF,
    0xCAC9C6E0,
    0xC6C6C6C6,
    0xC6CBCBC6,
    0xE3C4D4D4,
    0xACBDCACF,
    0xE1B6ACB0,
    0xC4DADBE0,
    0xCBCBC6C4,
    0xC6C6C6C6,
    0xBFBFBDC8,
    0xD3E0E1BA,
    0xBEBACBCA,
    0xAFAFACBF,
    0xD3B8BEAC,
    0xFFFFFFFF,
    0xE4FFFFFF,
    0xBFC0CAC9,
    0xB5B5B5A8,
    0xBDBEAEAE,
    0xE1D5CACB,
    0xB0AABAE1,
    0xB8BEB09C,
    0xD5D5E1D3,
    0xAEA6CBD3,
    0xBFA6B5A6,
    0xA28E8AB0,
    0xCAD2D5BD,
    0xB2BFBDB8,
    0xAFAFAFAF,
    0xD0CABDAA,
    0xFFFFFFFF,
    0xCCFFFFFF,
    0xA2B2BAB5,
    0xA19C9CA1,
    0xA18A8E8A,
    0xBAAEBFAF,
    0xB0A9A6C8,
    0xBABFA2A1,
    0xBDBDB8C9,
    0xA1A2B0C0,
    0x8EA1A1A2,
    0xA28E8E8E,
    0xBAB8B8BD,
    0xAFAFBFAE,
    0xAEBEBFB2,
    0xDBC6CAB8,
    0xFFFFFFFF,
    0xBBFFFFFF,
    0xA2BEBAB6,
    0x7F7F8E8E,
    0x7F7F7A7A,
    0xBFB3A48E,
    0xA1ADBEBD,
    0xBDB28A8E,
    0xA2B0C0BD,
    0x7F7F7FA4,
    0x7F7F7F7F,
    0xBEA28E7F,
    0xAEBDBABA,
    0xB2AFB0BF,
    0xD2C8BDAE,
    0xDBDBD0E2,
    0xFFFFFFFF,
    0xCCFFFFFF,
    0xB0BDBDA6,
    0x7A7A7F8A,
    0x7F7A7A7A,
    0xB3A48E7F,
    0x8AB0BEC0,
    0xC0B08E7F,
    0x8EA2B2C0,
    0x7A7A7F7F,
    0x7F7F7F7A,
    0xBDC0A48E,
    0xBEAEBDBA,
    0xBDBEB2B2,
    0xD0E0D5B8,
    0xDCE8DBDB,
};

GpImgRec D_actor_113000_8013AB6C[2] = {
    { 0, 0, { 0, 0, 32, 16 }, D_actor_113000_8013A76C },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

AnimationSet* D_actor_113000_8013AB8C[9] = {
    NULL,
    &D_actor_113000_80137BB4,
    &D_actor_113000_80138014,
    &D_actor_113000_8013839C,
    &D_actor_113000_80138B30,
    &D_actor_113000_801391C8,
    &D_actor_113000_801396D8,
    &D_actor_113000_80139C8C,
    &D_actor_113000_80139F04,
};

AnimationSet** D_actor_113000_8013ABB0[1] = {
    D_actor_113000_8013AB8C,
};

TaskDesc D_actor_113000_8013ABB4 = { 1, 192, func_actor_113000_80131F38, { .model = &D_actor_113000_801378E0 } };

Actor113000MessageEntry D_actor_113000_8013ABC0[5] = {
    { 2003, { .call0 = func_actor_113000_80132208 } },
    { 2004, { .call1 = func_actor_113000_8013231C } },
    { 2005, { .call3 = func_actor_113000_80132398 } },
    { 2016, { .call2 = func_actor_113000_80132474 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
}; /// Texture-upload state: runs the countdown at `field_4C2` down one a frame

static void func_actor_113000_80131E30(Task* arg0);

/// while `field_4C4` names the upload step in progress, and on the frame it
/// underflows posts that step's image over the 0x20x0x10 rect at y 0x28 --
/// reloading the countdown from `field_4C0` and advancing `field_4C4` for
/// steps 1 and 2, or clearing it for step 3, which ends the sequence until
/// mode 3 of the message-0x7E0 handler restarts it. Step 0 does nothing.
static void func_actor_113000_80131E30(Task* arg0)
{
    Actor113000Work* work;
    RECT             rect;

    work   = (Actor113000Work*)((GameActor*)arg0->work);
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x20;
    rect.h = 0x10;

    switch (work->field_4C4) {
        case 1:
            work->field_4C2 = work->field_4C2 - 1;
            if ((s16)work->field_4C2 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_113000_8013AB6C[0], &rect);
                work->field_4C2 = work->field_4C0;
                work->field_4C4 = work->field_4C4 + 1;
            }
            break;
        case 2:
            work->field_4C2 = work->field_4C2 - 1;
            if ((s16)work->field_4C2 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_113000_8013A74C[0], &rect);
                work->field_4C2 = work->field_4C0;
                work->field_4C4 = work->field_4C4 + 1;
            }
            break;
        case 3:
            work->field_4C2 = work->field_4C2 - 1;
            if ((s16)work->field_4C2 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_113000_8013A32C[0], &rect);
                work->field_4C4 = 0;
            }
            break;
    }
}

/// Task callback of the actor: copies the three-handler table
/// `D_actor_113000_80131E24` (spawn `func_actor_113000_80131F90`, per-frame
/// tick `func_actor_113000_80132070`, exit `Gp_EnemyTaskExit`) onto the stack
/// and runs the entry `Task::state` selects.
void func_actor_113000_80131F38(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113000_80131E24;
    sp.funcs[task->state](task);
}

/// Spawn handler: allocates the work block, seeds its head, mirrors the
/// deferred-kill bit into the model, draws the ground shadow under the model's
/// second part, then hands the model's matrices to the light/color rebuilder.
static void func_actor_113000_80131F90(Task* task)
{
    Actor113000Work* work;
    TmdObject*       extra;
    VECTOR3          pos;
    u16              flags;

    extra = task->extra.tmd;
    work  = (Actor113000Work*)memCalloc(0x4CC, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = work;
    work->field_478 = -1;
    work->field_47C = -1;
    work->field_4C6 = 0;
    work->field_4C8 = -1;
    flags           = extra->flags | TMD_OBJECT_HIDDEN;
    extra->flags    = flags;
    if (!(flags & 0x80)) {
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShadowShade);
        }
    }
    func_actor_113000_801321A8(task);
    task->msgTable     = D_actor_113000_8013ABC0;
    task->exitCallback = Gp_EnemyTaskExit;
    task->state++;
}

/// Per-frame tick, run after the model has been published: ticks the animation
/// slots while the preset bank `field_474` marks live, draws the ground shadow
/// under model part 1 while the model is not deferred, rebuilds that part's
/// world matrix while the session's 0x4D is set, runs the texture-upload
/// state, and counts the buffer free at `field_4C8` down to zero.
static void func_actor_113000_80132070(Task* task)
{
    Actor113000Work* work;
    TmdObject*       extra;
    GfxCoord*        coords;
    VECTOR3          pos;
    s32              i;

    work  = (Actor113000Work*)task->work;
    extra = task->extra.tmd;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (!(extra->flags & TMD_OBJECT_HIDDEN)) {
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        coords                 = task->extra.tmd->coords;
        coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&coords[1]);
        func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
    }
    func_actor_113000_80131E30(task);
    if (work->field_4C8 >= 0) {
        if (work->field_4C8 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_4C8--;
    }
}

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
static void func_actor_113000_801321A8(Task* task)
{
    Actor113000Work* work;
    GfxCoord*        coords;
    TmdObject*       extra;

    work                   = (Actor113000Work*)task->work;
    extra                  = task->extra.tmd;
    coords                 = extra->coords;
    extra->lightMtx        = &work->light;
    extra->colorMtx        = &work->color;
    coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// Start-preset handler: a preset bank the work block is not already on
/// re-seeds it -- the animation id is reset to -1, the bank is stored and the
/// bank's animation source goes to `func_800B3F84` with the block's context,
/// its pose buffer and its slots. The preset's
/// animation id is then latched, every slot 1..0x13 restarted -- through
/// `func_800B4114` when the preset asks for it, through `Gp_AnimResetSlot`
/// otherwise -- ticked once, and `field_474` raised.
s32 func_actor_113000_80132208(Task* task, s32 msgId, AnimationPlayRequest* msg, s32 arg3)
{
    Actor113000Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor113000Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->field_47C) {
        work->field_47C = msg->source.index;
        work->field_478 = -1;
        func_800B3F84(&work->rig.anim, D_actor_113000_8013ABB0[work->field_47C], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->field_478 = msg->animationId;
    if (msg->blend != ANIMATION_BLEND_RESET) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->rig.anim, i, work->field_478, 0, 6);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->field_478);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->field_474 = 1;
    return 0;
}

/// Placement handler: writes the payload's position into the root
/// coordinate's translation and its Euler angles into the coordinate's `rot`
/// slot, rebuilds the rotation from them with `RotMatrix` and clears `composeStamp` so
/// the world matrix is recomputed. Returns 0.
s32 func_actor_113000_8013231C(Task* task, s32 arg1, GpXformArg* args)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Display handler: `mode` sets or clears bit 0x80 of `TmdObject::flags`
/// (hidden) and sets or clears bit 0x4:
///
///   mode 0  set 0x80, clear 0x4
///   mode 1  clear 0x80, `Tmd_AllocBuffers`, clear 0x4
///   mode 2  set 0x80, store 2 in the countdown `Actor113000Work::field_4C8`
///           that `func_actor_113000_80132070` ends in `Tmd_FreeBuffers`,
///           set 0x4
///   mode 3  clear 0x80, set 0x4
///
/// Any other mode returns 1; the four known ones return 0. `arg3` is unused.
s32 func_actor_113000_80132398(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*       obj;
    Actor113000Work* work;
    s32              ret;

    obj  = task->extra.tmd;
    work = (Actor113000Work*)task->work;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_HIDDEN;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_HIDDEN;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags     |= TMD_OBJECT_HIDDEN;
            work->field_4C8 = mode;
            obj->flags     |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_HIDDEN;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message-0x7E0 handler: uploads one of the actor's three texture records over
/// the 0x20x0x10 rect at y 0x28 -- `D_actor_113000_8013AB6C[0]` for mode 1,
/// `D_actor_113000_8013A32C[0]` for modes 0 and 2, and `D_actor_113000_8013A74C[0]`
/// for mode 3, which sets the work block's `field_4C4` / `field_4C0` to 1
/// first. Any other mode leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_113000_80132474(Task* arg0, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x20;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            img = &D_actor_113000_8013AB6C[0];
            break;
        case 0:
        case 2:
            img = &D_actor_113000_8013A32C[0];
            break;
        case 3:
            ((Actor113000Work*)((GameActor*)arg0->work))->field_4C4 = 1;
            ((Actor113000Work*)((GameActor*)arg0->work))->field_4C0 = 1;
            img                                                     = &D_actor_113000_8013A74C[0];
            break;
        default:
            img = NULL;
            break;
    }

    if (img != NULL) {
        ret = Gp_LoadActorImage(arg0, img, &rect);
    }
    return ret;
}
