#include "actors/actor_210700.h"

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

extern GpImgRec D_actor_210700_8015858C[2];

/// The actor's work block. The spawn handler `func_actor_210700_80149F90`
/// allocates it zeroed with `memCalloc(0x540, 0)` and keeps it in
/// `Task::work`. The front is the animation state the 0x7D3 message handler
/// drives - the context, its slots and the pose buffer handed to
/// `func_800B3F84` - followed by the light / colour matrices
/// `func_actor_210700_8014A208` points the model at, and the texture-upload
/// state the upload handler runs.
typedef struct Actor210700Work {
    /* 0x000 */ ActorAnimRig20 rig;
    /// Non-zero once an animation has been started; gates the per-frame tick
    /// of slots 1..0x13.
    /* 0x474 */ s32 field_474;
    /// Animation id the slots were last reset to; -1 out of the spawn handler
    /// and whenever a new animation source is loaded.
    /* 0x478 */ s32 field_478;
    /// Index of the animation source last loaded from
    /// `D_actor_210700_801585C8`; -1 out of the spawn handler.
    /* 0x47C */ s32    field_47C;
    /* 0x480 */ MATRIX light;
    /* 0x4A0 */ MATRIX color;
    /* 0x4C0 */ byte   pad_4C0[0x78];
    /// Countdown reload value for the texture upload; set to 1 by the 0x7E0
    /// handler's mode 3.
    /* 0x538 */ s16 field_538;
    /// Frames left before the next texture-upload step.
    /* 0x53A */ u16 field_53A;
    /// Texture-upload step in progress, 0 when idle; set to 1 by the 0x7E0
    /// handler's mode 3.
    /* 0x53C */ s16 field_53C;
    /// Frames until the model buffers are freed; -1 when idle, latched to 2 by
    /// the 0x7D5 handler's mode 2.
    /* 0x53E */ s16 field_53E;
} Actor210700Work;
STATIC_ASSERT_SIZEOF(Actor210700Work, 0x540);

/// Payload of the 0x7D3 animation message. `field_0` indexes the animation
/// source table `D_actor_210700_801585C8`, `field_4` is the animation id every
/// slot 1..0x13 is reset to, and a non-zero `field_8` resets the slots through
/// `animationSeekSlotWithBlend` instead of `animationResetSlot`. Only the first three words
/// are read; the spawn handler's frame spaces its locals as if the block were
/// 0x18 bytes.
typedef struct _Actor210700Anim {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[0xC];
} Actor210700Anim;
STATIC_ASSERT_SIZEOF(Actor210700Anim, 0x18);

/// Animation sources the 0x7D3 handler loads, indexed by its payload's
/// `field_0`.
extern AnimationSet*  D_actor_210700_801585AC[7];
extern AnimationSet** D_actor_210700_801585C8[1];

/// The actor's message table, parked in `Task::msgTable`: 0x7D3
/// `func_actor_210700_8014A224`, 0x7D4 `func_actor_210700_8014A344`, 0x7D5
/// `func_actor_210700_8014A3D4`, 0x7E0 `func_actor_210700_8014A4B0`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, Actor210700Anim*, s32);
        s32 (*call1)(Task*, s32, ActorTransform*, s32);
        s32 (*call2)(Task*, s32, s32);
    } handler;
} Actor210700MsgEntry;
STATIC_ASSERT_SIZEOF(Actor210700MsgEntry, 8);

extern Actor210700MsgEntry D_actor_210700_801585D8[];

/// Images the texture-upload state and the 0x7E0 handler post over the
/// model's texture.

static void func_actor_210700_80149F90(Task* task);
static void func_actor_210700_8014A0AC(Task* task);
static void func_actor_210700_8014A1E8(Task* task);
static void func_actor_210700_8014A208(Task* arg0);
s32         func_actor_210700_8014A224(Task* task, s32 arg1, Actor210700Anim* msg, s32 arg3);
s32         func_actor_210700_8014A344(Task* task, s32 arg1, ActorTransform* args, s32 arg3);

/// The actor's three task states - spawn, tick and teardown - which
/// `func_actor_210700_80149F38` runs by `Task::state`.
static const TaskFuncTable3 D_actor_210700_80149E24 = { {
    func_actor_210700_80149F90,
    func_actor_210700_8014A0AC,
    func_actor_210700_8014A1E8,
} };

extern AnimationSet D_actor_210700_80156A40;
extern AnimationSet D_actor_210700_80156E00;
extern AnimationSet D_actor_210700_80157060;
extern AnimationSet D_actor_210700_8015754C;
extern AnimationSet D_actor_210700_8015799C;
extern AnimationSet D_actor_210700_80157C24;
extern TmdSource    D_actor_210700_80156504;
s32                 func_actor_210700_8014A224(Task*, s32, Actor210700Anim*, s32);
s32                 func_actor_210700_8014A344(Task*, s32, ActorTransform* args, s32);
s32                 func_actor_210700_8014A3D4(Task*, s32, s32);
s32                 func_actor_210700_8014A4B0(Task*, s32, s32);
void                func_actor_210700_80149F38(Task*);

AnimationPackedPose D_actor_210700_8014A570[49] = {
#include "assets/actor_210700_animation_01E2C_bank1.inc"
};

AnimationPackedRotation D_actor_210700_8014A7BC[592] = {
#include "assets/actor_210700_animation_01E2C_bank4.inc"
};

AnimationRecord D_actor_210700_8014B0FC[714] = {
#include "assets/actor_210700_animation_01E2C_records.inc"
};

u16 D_actor_210700_8014BC24[20] = {
#include "assets/actor_210700_animation_01E2C_indices.inc"
};

AnimationSet D_actor_210700_8014BC4C = {
    D_actor_210700_8014B0FC,
    D_actor_210700_8014BC24,
    { NULL, D_actor_210700_8014A570, NULL, NULL, D_actor_210700_8014A7BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_8014BC74[18] = {
#include "assets/actor_210700_animation_027F8_bank1.inc"
};

AnimationPackedRotation D_actor_210700_8014BD4C[226] = {
#include "assets/actor_210700_animation_027F8_bank4.inc"
};

AnimationRecord D_actor_210700_8014C0D4[327] = {
#include "assets/actor_210700_animation_027F8_records.inc"
};

u16 D_actor_210700_8014C5F0[20] = {
#include "assets/actor_210700_animation_027F8_indices.inc"
};

AnimationSet D_actor_210700_8014C618 = {
    D_actor_210700_8014C0D4,
    D_actor_210700_8014C5F0,
    { NULL, D_actor_210700_8014BC74, NULL, NULL, D_actor_210700_8014BD4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_8014C640[6] = {
#include "assets/actor_210700_animation_02B54_bank1.inc"
};

AnimationPackedRotation D_actor_210700_8014C688[51] = {
#include "assets/actor_210700_animation_02B54_bank4.inc"
};

AnimationRecord D_actor_210700_8014C754[126] = {
#include "assets/actor_210700_animation_02B54_records.inc"
};

u16 D_actor_210700_8014C94C[20] = {
#include "assets/actor_210700_animation_02B54_indices.inc"
};

AnimationSet D_actor_210700_8014C974 = {
    D_actor_210700_8014C754,
    D_actor_210700_8014C94C,
    { NULL, D_actor_210700_8014C640, NULL, NULL, D_actor_210700_8014C688, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_8014C99C[5] = {
#include "assets/actor_210700_animation_02E0C_bank1.inc"
};

AnimationPackedRotation D_actor_210700_8014C9D8[50] = {
#include "assets/actor_210700_animation_02E0C_bank4.inc"
};

AnimationRecord D_actor_210700_8014CAA0[89] = {
#include "assets/actor_210700_animation_02E0C_records.inc"
};

u16 D_actor_210700_8014CC04[20] = {
#include "assets/actor_210700_animation_02E0C_indices.inc"
};

AnimationSet D_actor_210700_8014CC2C = {
    D_actor_210700_8014CAA0,
    D_actor_210700_8014CC04,
    { NULL, D_actor_210700_8014C99C, NULL, NULL, D_actor_210700_8014C9D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_8014CC54[8] = {
#include "assets/actor_210700_animation_0318C_bank1.inc"
};

AnimationPackedRotation D_actor_210700_8014CCB4[67] = {
#include "assets/actor_210700_animation_0318C_bank4.inc"
};

AnimationRecord D_actor_210700_8014CDC0[113] = {
#include "assets/actor_210700_animation_0318C_records.inc"
};

u16 D_actor_210700_8014CF84[20] = {
#include "assets/actor_210700_animation_0318C_indices.inc"
};

AnimationSet D_actor_210700_8014CFAC = {
    D_actor_210700_8014CDC0,
    D_actor_210700_8014CF84,
    { NULL, D_actor_210700_8014CC54, NULL, NULL, D_actor_210700_8014CCB4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_8014CFD4[5] = {
#include "assets/actor_210700_animation_03424_bank1.inc"
};

AnimationPackedRotation D_actor_210700_8014D010[37] = {
#include "assets/actor_210700_animation_03424_bank4.inc"
};

AnimationRecord D_actor_210700_8014D0A4[94] = {
#include "assets/actor_210700_animation_03424_records.inc"
};

u16 D_actor_210700_8014D21C[20] = {
#include "assets/actor_210700_animation_03424_indices.inc"
};

AnimationSet D_actor_210700_8014D244 = {
    D_actor_210700_8014D0A4,
    D_actor_210700_8014D21C,
    { NULL, D_actor_210700_8014CFD4, NULL, NULL, D_actor_210700_8014D010, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_8014D26C[15] = {
#include "assets/actor_210700_animation_03BD0_bank1.inc"
};

AnimationPackedRotation D_actor_210700_8014D320[170] = {
#include "assets/actor_210700_animation_03BD0_bank4.inc"
};

AnimationRecord D_actor_210700_8014D5C8[256] = {
#include "assets/actor_210700_animation_03BD0_records.inc"
};

u16 D_actor_210700_8014D9C8[20] = {
#include "assets/actor_210700_animation_03BD0_indices.inc"
};

AnimationSet D_actor_210700_8014D9F0 = {
    D_actor_210700_8014D5C8,
    D_actor_210700_8014D9C8,
    { NULL, D_actor_210700_8014D26C, NULL, NULL, D_actor_210700_8014D320, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_8014DA18[116] = {
#include "assets/actor_210700_animation_067E8_bank1.inc"
};

AnimationPackedRotation D_actor_210700_8014DF88[1028] = {
#include "assets/actor_210700_animation_067E8_bank4.inc"
};

AnimationRecord D_actor_210700_8014EF98[1426] = {
#include "assets/actor_210700_animation_067E8_records.inc"
};

u16 D_actor_210700_801505E0[20] = {
#include "assets/actor_210700_animation_067E8_indices.inc"
};

AnimationSet D_actor_210700_80150608 = {
    D_actor_210700_8014EF98,
    D_actor_210700_801505E0,
    { NULL, D_actor_210700_8014DA18, NULL, NULL, D_actor_210700_8014DF88, NULL, NULL, NULL },
};

TmdBone D_actor_210700_80150630[20] = {
#include "assets/rupert_broderick_body_1_skeleton.inc"
};

u32 D_actor_210700_80150900[20] = {
#include "assets/rupert_broderick_body_1_partVerts.inc"
};

SVECTOR D_actor_210700_80150950[386] = {
#include "assets/rupert_broderick_body_1_verts.inc"
};

SVECTOR D_actor_210700_80151560[385] = {
#include "assets/rupert_broderick_body_1_normals.inc"
};

u32 D_actor_210700_80152168[4327] = {
#include "assets/rupert_broderick_body_1_stream.inc"
};

TmdSource D_actor_210700_80156504 = {
    0,
    23980,
    6012,
    20,
    D_actor_210700_80150900,
    D_actor_210700_80150950,
    D_actor_210700_80151560,
    D_actor_210700_80150630,
    D_actor_210700_80152168,
};

AnimationPackedPose D_actor_210700_80156528[9] = {
#include "assets/actor_210700_animation_0CC20_bank1.inc"
};

AnimationPackedRotation D_actor_210700_80156594[112] = {
#include "assets/actor_210700_animation_0CC20_bank4.inc"
};

AnimationRecord D_actor_210700_80156754[177] = {
#include "assets/actor_210700_animation_0CC20_records.inc"
};

u16 D_actor_210700_80156A18[20] = {
#include "assets/actor_210700_animation_0CC20_indices.inc"
};

AnimationSet D_actor_210700_80156A40 = {
    D_actor_210700_80156754,
    D_actor_210700_80156A18,
    { NULL, D_actor_210700_80156528, NULL, NULL, D_actor_210700_80156594, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_80156A68[7] = {
#include "assets/actor_210700_animation_0CFE0_bank1.inc"
};

AnimationPackedRotation D_actor_210700_80156ABC[62] = {
#include "assets/actor_210700_animation_0CFE0_bank4.inc"
};

AnimationRecord D_actor_210700_80156BB4[137] = {
#include "assets/actor_210700_animation_0CFE0_records.inc"
};

u16 D_actor_210700_80156DD8[20] = {
#include "assets/actor_210700_animation_0CFE0_indices.inc"
};

AnimationSet D_actor_210700_80156E00 = {
    D_actor_210700_80156BB4,
    D_actor_210700_80156DD8,
    { NULL, D_actor_210700_80156A68, NULL, NULL, D_actor_210700_80156ABC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_80156E28[2] = {
#include "assets/actor_210700_animation_0D240_bank1.inc"
};

AnimationPackedRotation D_actor_210700_80156E40[32] = {
#include "assets/actor_210700_animation_0D240_bank4.inc"
};

AnimationRecord D_actor_210700_80156EC0[94] = {
#include "assets/actor_210700_animation_0D240_records.inc"
};

u16 D_actor_210700_80157038[20] = {
#include "assets/actor_210700_animation_0D240_indices.inc"
};

AnimationSet D_actor_210700_80157060 = {
    D_actor_210700_80156EC0,
    D_actor_210700_80157038,
    { NULL, D_actor_210700_80156E28, NULL, NULL, D_actor_210700_80156E40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_80157088[6] = {
#include "assets/actor_210700_animation_0D72C_bank1.inc"
};

AnimationPackedRotation D_actor_210700_801570D0[104] = {
#include "assets/actor_210700_animation_0D72C_bank4.inc"
};

AnimationRecord D_actor_210700_80157270[173] = {
#include "assets/actor_210700_animation_0D72C_records.inc"
};

u16 D_actor_210700_80157524[20] = {
#include "assets/actor_210700_animation_0D72C_indices.inc"
};

AnimationSet D_actor_210700_8015754C = {
    D_actor_210700_80157270,
    D_actor_210700_80157524,
    { NULL, D_actor_210700_80157088, NULL, NULL, D_actor_210700_801570D0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_80157574[6] = {
#include "assets/actor_210700_animation_0DB7C_bank1.inc"
};

AnimationPackedRotation D_actor_210700_801575BC[84] = {
#include "assets/actor_210700_animation_0DB7C_bank4.inc"
};

AnimationRecord D_actor_210700_8015770C[154] = {
#include "assets/actor_210700_animation_0DB7C_records.inc"
};

u16 D_actor_210700_80157974[20] = {
#include "assets/actor_210700_animation_0DB7C_indices.inc"
};

AnimationSet D_actor_210700_8015799C = {
    D_actor_210700_8015770C,
    D_actor_210700_80157974,
    { NULL, D_actor_210700_80157574, NULL, NULL, D_actor_210700_801575BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210700_801579C4[4] = {
#include "assets/actor_210700_animation_0DE04_bank1.inc"
};

AnimationPackedRotation D_actor_210700_801579F4[52] = {
#include "assets/actor_210700_animation_0DE04_bank4.inc"
};

AnimationRecord D_actor_210700_80157AC4[78] = {
#include "assets/actor_210700_animation_0DE04_records.inc"
};

u16 D_actor_210700_80157BFC[20] = {
#include "assets/actor_210700_animation_0DE04_indices.inc"
};

AnimationSet D_actor_210700_80157C24 = {
    D_actor_210700_80157AC4,
    D_actor_210700_80157BFC,
    { NULL, D_actor_210700_801579C4, NULL, NULL, D_actor_210700_801579F4, NULL, NULL, NULL },
};

u_long D_actor_210700_80157C4C[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA6A2A28D,
    0xADA5ADA5,
    0xB1B1B1AE,
    0xAEAEADAE,
    0x65929AA9,
    0xADA68766,
    0xADA5A5AD,
    0xADADADAD,
    0x8B969EA5,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0xA5958C8D,
    0xFFB9C29C,
    0xAE61B5FF,
    0xA9A9A9A5,
    0x7C788F95,
    0xA7958785,
    0x9FA5A68B,
    0xFFFFB561,
    0x9890C5BF,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0x9B8C828D,
    0xFFB21716,
    0xCECCFFFF,
    0x9595A890,
    0x7B65864E,
    0x998D8665,
    0xCEACA997,
    0xB7FFFFC4,
    0x9BCCD5D0,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9894028A,
    0x1CCB0B1,
    0x81958B9B,
    0x72677882,
    0x998E8568,
    0xCC9B95A2,
    0x98AE7EC6,
    0x87891601,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x87879A9B,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpImgRec D_actor_210700_80157F4C[2] = {
    { 0, 0, { 0, 0, 24, 16 }, D_actor_210700_80157C4C },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_210700_80157F6C[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA2A2A28D,
    0x8C8C8C8C,
    0xA9A9A68C,
    0xAEAEA9A9,
    0x65929AA9,
    0xADA68766,
    0xA9A9A5AD,
    0x9595A9A9,
    0x95959595,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0x95958C8D,
    0xB0C2A995,
    0xC2C2B0B0,
    0xA9A9A9A9,
    0x7C788F95,
    0xA7958785,
    0xA9A9A68B,
    0xC2B0B0C2,
    0x959595C2,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0xC28C828D,
    0xB0B0B0B0,
    0xB0B0B0B0,
    0x9595A8B0,
    0x7B65864E,
    0x998D8665,
    0xB0B0A997,
    0xB0B0B0B0,
    0x9BC2B0B0,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9894028A,
    0x1CCB0B1,
    0x81958B9B,
    0x72677882,
    0x998E8568,
    0xCC9B95A2,
    0x98AE7EC6,
    0x87891601,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x879A9A9A,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpImgRec D_actor_210700_8015826C[2] = {
    { 0, 0, { 0, 0, 24, 16 }, D_actor_210700_80157F6C },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_210700_8015828C[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA2A2A28D,
    0xA2A2A2A2,
    0xB19898A2,
    0xAEAEADAE,
    0x65929AA9,
    0xADA68766,
    0xADA5A5AD,
    0x8B8B9898,
    0xA2A2A2A2,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0xA2958C8D,
    0x788FA2A2,
    0xA2A28F8F,
    0xA9A9A9A6,
    0x7C788F95,
    0xA7958785,
    0xA2A2A7A7,
    0x8F788F8F,
    0x959595A2,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0x958C828D,
    0x61ADADA9,
    0xAD616161,
    0xA9A99898,
    0x7B65864E,
    0x998D8665,
    0x61AD9897,
    0x61616161,
    0x95A9ADAD,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9898ADAD,
    0x98A9A9A9,
    0x818198AD,
    0x72677882,
    0x998E8568,
    0x98AD9898,
    0x98A9A9A9,
    0x87ADADAD,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x87879A9B,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpImgRec D_actor_210700_8015858C[2] = {
    { 0, 0, { 0, 0, 24, 16 }, D_actor_210700_8015828C },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

AnimationSet* D_actor_210700_801585AC[7] = {
    NULL,
    &D_actor_210700_80156A40,
    &D_actor_210700_80156E00,
    &D_actor_210700_80157060,
    &D_actor_210700_8015754C,
    &D_actor_210700_8015799C,
    &D_actor_210700_80157C24,
};

AnimationSet** D_actor_210700_801585C8[1] = {
    D_actor_210700_801585AC,
};

TaskDesc D_actor_210700_801585CC = { { { TASK_BODY_TMD, 192 } }, func_actor_210700_80149F38, { .model = &D_actor_210700_80156504 } };

Actor210700MsgEntry D_actor_210700_801585D8[5] = {
    { 2003, { .call0 = func_actor_210700_8014A224 } },
    { 2004, { .call1 = func_actor_210700_8014A344 } },
    { 2005, { .call2 = func_actor_210700_8014A3D4 } },
    { 2016, { .call2 = func_actor_210700_8014A4B0 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
}; /// Texture-upload step, run by the tick state: while `field_53C` names an

static void func_actor_210700_80149E30(Task* arg0);

/// upload in progress, counts `field_53A` down one a frame, and on the frame
/// it underflows posts that step's image over the 0x18x0x10 rect at y 0x28.
/// Steps 1 and 2 then reload the countdown from `field_538` and advance to the
/// next step; step 3 returns to idle.
static void func_actor_210700_80149E30(Task* arg0)
{
    Actor210700Work* work;
    RECT             rect;

    work   = (Actor210700Work*)arg0->work;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (work->field_53C) {
        case 1:
            work->field_53A = work->field_53A - 1;
            if ((s16)work->field_53A < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_8015858C[0], &rect);
                work->field_53A = work->field_538;
                work->field_53C = work->field_53C + 1;
            }
            break;
        case 2:
            work->field_53A = work->field_53A - 1;
            if ((s16)work->field_53A < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_8015826C[0], &rect);
                work->field_53A = work->field_538;
                work->field_53C = work->field_53C + 1;
            }
            break;
        case 3:
            work->field_53A = work->field_53A - 1;
            if ((s16)work->field_53A < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_80157F4C[0], &rect);
                work->field_53C = 0;
            }
            break;
    }
}

/// The actor's task entry: runs the handler for the task's current state out
/// of `D_actor_210700_80149E24` - spawn, per-frame tick or teardown - copying
/// the table onto the stack before the call.
void func_actor_210700_80149F38(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_210700_80149E24;
    sp.funcs[task->state](task);
}

/// Spawn state: allocates the zeroed work block into `Task::work` (handing
/// the task to `Gp_EnemyTaskExit` if that fails), marks no animation loaded,
/// hides the model with `TmdObject::flags` bit 0x80, then runs its own 0x7D4
/// and 0x7D3 message handlers directly to place the actor at the origin -
/// which shows it again - and start animation 1 of source 0. It draws the
/// ground shadow under the model's second part, points the model at the work
/// block's light / colour matrices, installs the message table and the exit
/// callback, and advances to the tick state.
static void func_actor_210700_80149F90(Task* task)
{
    Actor210700Work* work;
    TmdObject*       extra;
    ActorTransform   args;
    Actor210700Anim  anim;
    VECTOR3          pos;

    extra = task->extra.tmd;
    work  = memCalloc(0x540, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = work;
    work->field_478 = -1;
    work->field_47C = -1;
    work->field_53E = -1;
    extra->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    args.pos.vx     = 0;
    args.pos.vy     = 0;
    args.pos.vz     = 0;
    args.rot.vx     = 0;
    args.rot.vy     = 0;
    args.rot.vz     = 0;
    func_actor_210700_8014A344(task, 0x7D4, &args, 0);
    anim.field_0 = 0;
    anim.field_4 = 1;
    anim.field_8 = 0;
    func_actor_210700_8014A224(task, 0x7D3, &anim, 0);
    if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x400, gRoomEffectState->groundShadowShade);
    }
    func_actor_210700_8014A208(task);
    task->msgTable     = D_actor_210700_801585D8;
    task->exitCallback = func_actor_210700_8014A1E8;
    task->state++;
}

/// Tick state: while an animation is running ticks slots 1..0x13 and draws
/// the ground shadow under the model's second part. While the game session's
/// view is ready it invalidates and rebuilds that part's coordinate and hands
/// it to `func_800D7A9C`. It then runs the texture-upload step and counts
/// `field_53E` down, freeing the model buffers on the frame it reaches 0.
static void func_actor_210700_8014A0AC(Task* task)
{
    Actor210700Work* work;
    TmdObject*       ext;
    VECTOR3          pos;
    s16              count;
    s32              i;

    work = (Actor210700Work*)task->work;
    ext  = task->extra.tmd;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x400, gRoomEffectState->groundShadowShade);
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    func_actor_210700_80149E30(task);
    count = work->field_53E;
    if (count >= 0) {
        if (count == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_53E = (s16)((u16)work->field_53E - 1);
    }
}

/// The actor's teardown state and `Task::exitCallback`: hands the task to
/// `Gp_EnemyTaskExit`.
static void func_actor_210700_8014A1E8(Task* task)
{
    Gp_EnemyTaskExit(task);
}

/// Points the model at the work block's light and colour matrices, so the
/// actor is lit from its own block rather than the defaults.
static void func_actor_210700_8014A208(Task* arg0)
{
    TmdObject*       ext;
    Actor210700Work* work;

    work          = (Actor210700Work*)arg0->work;
    ext           = arg0->extra.tmd;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Message-0x7D3 handler: starts an animation. A source index different from
/// the one last loaded seeds the animation context from that entry of
/// `D_actor_210700_801585C8` and forgets the current animation id; a new
/// animation id then resets slots 1..0x13 to it - through `animationSeekSlotWithBlend`
/// when the payload's `field_8` is set, `animationResetSlot` otherwise - ticks
/// them once and enables the per-frame tick. Always returns 0.
s32 func_actor_210700_8014A224(Task* task, s32 arg1, Actor210700Anim* msg, s32 arg3)
{
    Actor210700Work* work;
    s32              i;
    TmdObject*       ext;

    work = (Actor210700Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->field_0 != work->field_47C) {
        work->field_47C = msg->field_0;
        work->field_478 = -1;
        func_800B3F84(&work->rig.anim, D_actor_210700_801585C8[work->field_47C], ext, work->rig.poses,
                      work->rig.slots);
    }
    if (msg->field_4 != work->field_478) {
        work->field_478 = msg->field_4;
        if (msg->field_8 != 0) {
            for (i = 1; i < 0x14; i++) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->field_478, 0, 6);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                animationResetSlot(&work->rig.anim, i, work->field_478);
            }
        }
        for (i = 1; i < 0x14; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        work->field_474 = 1;
    }
    return 0;
}

/// Message-0x7D4 handler: places the actor. Writes the payload's translation
/// into the root coordinate's local matrix and its Euler angles into the
/// coordinate's `rot` slot, rebuilds the rotation from them, clears `composeStamp` so
/// the world matrix is recomputed, and clears `TmdObject::flags` bit 0x80 to
/// show the model. Always returns 0.
s32 func_actor_210700_8014A344(Task* task, s32 arg1, ActorTransform* args, s32 arg3)
{
    GfxCoord*  coord;
    TmdObject* extra;

    extra               = task->extra.tmd;
    coord               = extra->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return 0;
}

/// Message-0x7D5 handler: sets the model's visibility and mode bit from the
/// message's mode word. `TmdObject::flags` bit 0x80 hides the model and bit
/// 0x4 is the one modes 2 and 3 raise. Mode 0 hides the model and drops 0x4,
/// 1 shows it, reallocates its buffers through `Tmd_AllocBuffers` and drops
/// 0x4, 2 hides it, raises 0x4 and starts the work block's `field_53E`
/// countdown to freeing the buffers, and 3 shows it and raises 0x4. Handled
/// modes return 0; anything else returns 1 and changes nothing.
s32 func_actor_210700_8014A3D4(Task* arg0, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    Actor210700Work* work;
    s32              ret;

    obj  = arg0->extra.tmd;
    work = (Actor210700Work*)arg0->work;
    ret  = 0;

    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_53E = mode;
            obj->flags     |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message-0x7E0 handler: uploads one of the actor's three texture records over
/// the 0x18x0x10 rect at y 0x28 -- `D_actor_210700_8015858C[0]` for mode 1,
/// `D_actor_210700_80157F4C[0]` for modes 0 and 2, and `D_actor_210700_8015826C[0]`
/// for mode 3, which sets the work block's `field_53C` / `field_538` to 1
/// first. Any other mode leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_210700_8014A4B0(Task* arg0, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            img = &D_actor_210700_8015858C[0];
            break;
        case 0:
        case 2:
            img = &D_actor_210700_80157F4C[0];
            break;
        case 3:
            ((Actor210700Work*)arg0->work)->field_53C = 1;
            ((Actor210700Work*)arg0->work)->field_538 = 1;
            img                                       = &D_actor_210700_8015826C[0];
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
