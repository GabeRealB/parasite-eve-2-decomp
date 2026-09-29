#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

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
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

/// 0x20-byte block `func_actor_160900_80133F90` allocates with
/// `memCalloc(0x20, 0)` for each of the two child tasks it spawns from index 7
/// of `D_actor_160900_8013FB50`, and parks in that child's `Task::work` slot
/// (0x1C) -- a third work block in this overlay, not a `TaskIdMap`. The size
/// below is the allocation: the function zeroes all 0x20 bytes with `Mem_Set`.
///
/// The four vectors are the corners of an axis-aligned rectangle in the Y/Z
/// plane, written as differences from the child's own origin. Child 0 (spawn
/// arg 1) takes the `+Z` side -- `(0,-0x5DC,0x3E8)`, `(0,-0x5DC,0)`,
/// `(0,0,0x3E8)`, `(0,0,0)` -- and child 1 the `-Z` side, the same rectangle
/// reflected through Z. No vector's `pad` halfword is touched, and `vx` is
/// zero in every one of them.
typedef struct Actor160900ChildWork {
    /* 0x00 */ SVECTOR field_0;
    /* 0x08 */ SVECTOR field_8;
    /* 0x10 */ SVECTOR field_10;
    /* 0x18 */ SVECTOR field_18;
} Actor160900ChildWork;
STATIC_ASSERT_SIZEOF(Actor160900ChildWork, 0x20);

/// Work block this overlay hangs off the task's `Task::work` slot (0x1C),
/// which is not a `TaskIdMap` here. Reach it with
/// `(Actor160900Work*)task->work`.
///
/// `func_actor_160900_8013418C` allocates it with `Mem_Malloc(0x68, 0)` and
/// zeroes all 0x68 bytes, so the size below is the allocation. That function
/// fills `field_34` with `gameGetPtrSlot(3)` -- the task every `Gp_DispatchMsg`
/// in this overlay targets -- and 0x38/0x3C/0x40 with the tasks it spawns from
/// `D_actor_160900_8013FB50` indices 3, 5 and 6.
///
/// `field_64` indexes `D_actor_160900_8013F1CC` and `field_66` counts frames
/// against the step's `hold`.
///
/// `wave` is the context of the screen-wave task `func_actor_160900_80131EB0`,
/// which the `field_4C == 4` request spawns with it as the argument after
/// setting an eight-frame ramp; the `field_4C == 5` request ends the wave.
/// The block is zeroed at allocation and this overlay never sets the tint.
typedef struct Actor160900Work {
    /* 0x00 */ OverlayWaveCtx wave;
    /* 0x0C */ Task*          field_C[10]; // child tasks, killed on death
    /* 0x34 */ Task*          field_34;    // gameGetPtrSlot(3), Gp_DispatchMsg target
    /* 0x38 */ Task*          field_38;    // D_actor_160900_8013FB50[3]
    /* 0x3C */ Task*          field_3C;    // D_actor_160900_8013FB50[5]
    /* 0x40 */ Task*          field_40;    // D_actor_160900_8013FB50[6]
    /* 0x44 */ Task*          field_44;    // optional, notified with 0x7D5 alongside 0x3C/0x40
    /* 0x48 */ byte           pad_48[4];
    /* 0x4C */ s16            field_4C;
    /* 0x4E */ s16            field_4E;
    /* 0x50 */ byte           pad_50[4];
    /* 0x54 */ s16            field_54;
    /* 0x56 */ s16            field_56;
    /* 0x58 */ byte           pad_58[4];
    /* 0x5C */ s16            field_5C;
    /* 0x5E */ s16            field_5E;
    /* 0x60 */ byte           pad_60[4];
    /* 0x64 */ u16            field_64;
    /* 0x66 */ u16            field_66;
} Actor160900Work;
STATIC_ASSERT_SIZEOF(Actor160900Work, 0x68);

/// Work block of the `D_actor_160900_8013FB50[3]` child (`field_38`), as far
/// as `func_actor_160900_8013358C` reaches into it: the model's rig, the
/// matrices it is lit with, and the animation script it walks.
typedef struct Actor160900Child3Work {
    /* 0x000 */ ActorAnimRig20 rig;
    /* 0x474 */ MATRIX         light; // `TmdObject::lightMtx`
    /* 0x494 */ MATRIX         color; // `TmdObject::colorMtx`
    /* 0x4B4 */ void*          field_4B4;
    /* 0x4B8 */ s16            field_4B8;
    /* 0x4BA */ s16            field_4BA;
} Actor160900Child3Work;
STATIC_ASSERT_SIZEOF(Actor160900Child3Work, 0x4BC);

/// The overlay's task table. Entry 0 is `func_actor_160900_8013418C`, which
/// spawns entries 3, 5 and 6 into `Actor160900Work`; entries 1 and 2 are
/// spawned by `func_actor_160900_801346B0` / `func_actor_160900_801346E0`.
extern TaskDesc D_actor_160900_8013FB50[];

/// Animation script `func_actor_160900_801326EC` walks and the animation-set
/// table it hands the player task as message 0x3F4's `field_0`.
extern ActorAnimStep D_actor_160900_8013F1CC[];
extern u8            D_actor_160900_8013F198[];

extern u8       D_actor_160900_8013F210[];
extern u8       D_actor_160900_8013F228[];
extern TaskDesc D_actor_160900_8013F17C[];

/// Point lists `func_actor_160900_8013418C` hands `func_actor_160900_80133758`
/// for `Actor160900Work::field_5C` values 1-5.
extern SVECTOR D_actor_160900_8013F258[];
extern SVECTOR D_actor_160900_8013F2E0[];
extern SVECTOR D_actor_160900_8013F3B0[];
extern SVECTOR D_actor_160900_8013F400[];
extern SVECTOR D_actor_160900_8013F458[];

/// Pair of blocks `func_actor_160900_8013418C` passes to `func_800E8634`.
extern GpEvsCmd D_actor_160900_8013F538[];
extern GpEvsCmd D_actor_160900_8013FAA8[];

/// Distortion amplitude of the screen wave, `frame * scale / span` of the
/// running context, recomputed every frame.
extern s32 D_actor_160900_8013F194;

/// The context the running wave task was spawned with, parked at spawn so
/// the tick reads the ramp through it.
extern OverlayWaveCtx* D_actor_160900_8013FBB0;

extern Task* D_actor_160900_8013FBB4;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern OverlayWaveRec6 D_actor_160900_8013FBB8[13];

extern OverlayWaveRec6 D_actor_160900_8013FC08[30];

extern TmdSource D_actor_160900_801393B8;
extern TmdSource D_actor_160900_801397FC;
extern TmdSource D_actor_160900_80139C50;
extern TmdSource D_actor_160900_8013A0A4;
void             func_actor_160900_80132A14(Task*);
void             func_actor_160900_80132C08(Task*);
void             func_actor_160900_80132E80(Task*);
void             func_actor_160900_80133880(void);
void             func_actor_160900_80133A84(void);
void             func_actor_160900_80133F90(void);
void             func_actor_160900_8013418C(Task*);
void             func_actor_160900_801343E4(Task*);
void             func_actor_160900_801344D8(Task*);
void             func_actor_160900_801346B0(s32);
void             func_actor_160900_801346E0(s32);
void             func_actor_160900_80134710(void);
void             func_actor_160900_80134790(s16);
void             func_actor_160900_801347B0(s16);
void             func_actor_160900_801347D0(s16);
void             func_actor_160900_801347F0(void);
void             func_actor_160900_80134830(void);
void             func_actor_160900_80134850(void);
void             func_actor_160900_80134870(void);

void func_actor_160900_80131EB0(Task*);
void func_actor_160900_801345D0(Task*, s32, s32);
void func_actor_160900_80134624(Task*, s32, GpXformArg*);

TmdBone D_actor_160900_80134898[20] = {
#include "assets/actor_160900_model_07598_skeleton.inc"
};

u32 D_actor_160900_80134B68[20] = {
#include "assets/actor_160900_model_07598_partVerts.inc"
};

SVECTOR D_actor_160900_80134BB8[300] = {
#include "assets/actor_160900_model_07598_verts.inc"
};

SVECTOR D_actor_160900_80135518[298] = {
#include "assets/actor_160900_model_07598_normals.inc"
};

u32 D_actor_160900_80135E68[3412] = {
#include "assets/actor_160900_model_07598_stream.inc"
};

TmdSource D_actor_160900_801393B8 = {
    0,
    18224,
    5696,
    20,
    D_actor_160900_80134B68,
    D_actor_160900_80134BB8,
    D_actor_160900_80135518,
    D_actor_160900_80134898,
    D_actor_160900_80135E68,
};

TmdBone D_actor_160900_801393DC[1] = {
#include "assets/actor_160900_model_079DC_skeleton.inc"
};

u32 D_actor_160900_80139400[1] = {
#include "assets/actor_160900_model_079DC_partVerts.inc"
};

SVECTOR D_actor_160900_80139404[22] = {
#include "assets/actor_160900_model_079DC_verts.inc"
};

SVECTOR D_actor_160900_801394B4[24] = {
#include "assets/actor_160900_model_079DC_normals.inc"
};

u32 D_actor_160900_80139574[162] = {
#include "assets/actor_160900_model_079DC_stream.inc"
};

TmdSource D_actor_160900_801397FC = {
    0,
    1108,
    0,
    1,
    D_actor_160900_80139400,
    D_actor_160900_80139404,
    D_actor_160900_801394B4,
    D_actor_160900_801393DC,
    D_actor_160900_80139574,
};

TmdBone D_actor_160900_80139820[1] = {
#include "assets/actor_160900_model_07E30_skeleton.inc"
};

u32 D_actor_160900_80139844[1] = {
#include "assets/actor_160900_model_07E30_partVerts.inc"
};

SVECTOR D_actor_160900_80139848[23] = {
#include "assets/actor_160900_model_07E30_verts.inc"
};

SVECTOR D_actor_160900_80139900[23] = {
#include "assets/actor_160900_model_07E30_normals.inc"
};

u32 D_actor_160900_801399B8[166] = {
#include "assets/actor_160900_model_07E30_stream.inc"
};

TmdSource D_actor_160900_80139C50 = {
    0,
    1148,
    0,
    1,
    D_actor_160900_80139844,
    D_actor_160900_80139848,
    D_actor_160900_80139900,
    D_actor_160900_80139820,
    D_actor_160900_801399B8,
};

TmdBone D_actor_160900_80139C74[1] = {
#include "assets/actor_160900_model_08284_skeleton.inc"
};

u32 D_actor_160900_80139C98[1] = {
#include "assets/actor_160900_model_08284_partVerts.inc"
};

SVECTOR D_actor_160900_80139C9C[23] = {
#include "assets/actor_160900_model_08284_verts.inc"
};

SVECTOR D_actor_160900_80139D54[23] = {
#include "assets/actor_160900_model_08284_normals.inc"
};

u32 D_actor_160900_80139E0C[166] = {
#include "assets/actor_160900_model_08284_stream.inc"
};

TmdSource D_actor_160900_8013A0A4 = {
    0,
    1148,
    0,
    1,
    D_actor_160900_80139C98,
    D_actor_160900_80139C9C,
    D_actor_160900_80139D54,
    D_actor_160900_80139C74,
    D_actor_160900_80139E0C,
};

AnimationPackedPose D_actor_160900_8013A0C8[39] = {
#include "assets/actor_160900_animation_09138_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013A29C[364] = {
#include "assets/actor_160900_animation_09138_bank4.inc"
};

GpAnimRec D_actor_160900_8013A84C[441] = {
#include "assets/actor_160900_animation_09138_records.inc"
};

u16 D_actor_160900_8013AF30[20] = {
#include "assets/actor_160900_animation_09138_indices.inc"
};

GpAnimSet D_actor_160900_8013AF58 = {
    D_actor_160900_8013A84C,
    D_actor_160900_8013AF30,
    { NULL, D_actor_160900_8013A0C8, NULL, NULL, D_actor_160900_8013A29C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013AF80[60] = {
#include "assets/actor_160900_animation_09D3C_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013B250[241] = {
#include "assets/actor_160900_animation_09D3C_bank4.inc"
};

GpAnimRec D_actor_160900_8013B614[328] = {
#include "assets/actor_160900_animation_09D3C_records.inc"
};

u16 D_actor_160900_8013BB34[20] = {
#include "assets/actor_160900_animation_09D3C_indices.inc"
};

GpAnimSet D_actor_160900_8013BB5C = {
    D_actor_160900_8013B614,
    D_actor_160900_8013BB34,
    { NULL, D_actor_160900_8013AF80, NULL, NULL, D_actor_160900_8013B250, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013BB84[18] = {
#include "assets/actor_160900_animation_0A35C_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013BC5C[86] = {
#include "assets/actor_160900_animation_0A35C_bank4.inc"
};

GpAnimRec D_actor_160900_8013BDB4[232] = {
#include "assets/actor_160900_animation_0A35C_records.inc"
};

u16 D_actor_160900_8013C154[20] = {
#include "assets/actor_160900_animation_0A35C_indices.inc"
};

GpAnimSet D_actor_160900_8013C17C = {
    D_actor_160900_8013BDB4,
    D_actor_160900_8013C154,
    { NULL, D_actor_160900_8013BB84, NULL, NULL, D_actor_160900_8013BC5C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013C1A4[10] = {
#include "assets/actor_160900_animation_0A7F0_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013C21C[71] = {
#include "assets/actor_160900_animation_0A7F0_bank4.inc"
};

GpAnimRec D_actor_160900_8013C338[172] = {
#include "assets/actor_160900_animation_0A7F0_records.inc"
};

u16 D_actor_160900_8013C5E8[20] = {
#include "assets/actor_160900_animation_0A7F0_indices.inc"
};

GpAnimSet D_actor_160900_8013C610 = {
    D_actor_160900_8013C338,
    D_actor_160900_8013C5E8,
    { NULL, D_actor_160900_8013C1A4, NULL, NULL, D_actor_160900_8013C21C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013C638[3] = {
#include "assets/actor_160900_animation_0A9A8_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013C65C[24] = {
#include "assets/actor_160900_animation_0A9A8_bank4.inc"
};

GpAnimRec D_actor_160900_8013C6BC[57] = {
#include "assets/actor_160900_animation_0A9A8_records.inc"
};

u16 D_actor_160900_8013C7A0[20] = {
#include "assets/actor_160900_animation_0A9A8_indices.inc"
};

GpAnimSet D_actor_160900_8013C7C8 = {
    D_actor_160900_8013C6BC,
    D_actor_160900_8013C7A0,
    { NULL, D_actor_160900_8013C638, NULL, NULL, D_actor_160900_8013C65C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013C7F0[2] = {
#include "assets/actor_160900_animation_0AB80_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013C808[16] = {
#include "assets/actor_160900_animation_0AB80_bank4.inc"
};

GpAnimRec D_actor_160900_8013C848[76] = {
#include "assets/actor_160900_animation_0AB80_records.inc"
};

u16 D_actor_160900_8013C978[20] = {
#include "assets/actor_160900_animation_0AB80_indices.inc"
};

GpAnimSet D_actor_160900_8013C9A0 = {
    D_actor_160900_8013C848,
    D_actor_160900_8013C978,
    { NULL, D_actor_160900_8013C7F0, NULL, NULL, D_actor_160900_8013C808, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013C9C8[37] = {
#include "assets/actor_160900_animation_0BA0C_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013CB84[358] = {
#include "assets/actor_160900_animation_0BA0C_bank4.inc"
};

GpAnimRec D_actor_160900_8013D11C[442] = {
#include "assets/actor_160900_animation_0BA0C_records.inc"
};

u16 D_actor_160900_8013D804[20] = {
#include "assets/actor_160900_animation_0BA0C_indices.inc"
};

GpAnimSet D_actor_160900_8013D82C = {
    D_actor_160900_8013D11C,
    D_actor_160900_8013D804,
    { NULL, D_actor_160900_8013C9C8, NULL, NULL, D_actor_160900_8013CB84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013D854[6] = {
#include "assets/actor_160900_animation_0BD10_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013D89C[46] = {
#include "assets/actor_160900_animation_0BD10_bank4.inc"
};

GpAnimRec D_actor_160900_8013D954[109] = {
#include "assets/actor_160900_animation_0BD10_records.inc"
};

u16 D_actor_160900_8013DB08[20] = {
#include "assets/actor_160900_animation_0BD10_indices.inc"
};

GpAnimSet D_actor_160900_8013DB30 = {
    D_actor_160900_8013D954,
    D_actor_160900_8013DB08,
    { NULL, D_actor_160900_8013D854, NULL, NULL, D_actor_160900_8013D89C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013DB58[7] = {
#include "assets/actor_160900_animation_0C028_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013DBAC[54] = {
#include "assets/actor_160900_animation_0C028_bank4.inc"
};

GpAnimRec D_actor_160900_8013DC84[103] = {
#include "assets/actor_160900_animation_0C028_records.inc"
};

u16 D_actor_160900_8013DE20[20] = {
#include "assets/actor_160900_animation_0C028_indices.inc"
};

GpAnimSet D_actor_160900_8013DE48 = {
    D_actor_160900_8013DC84,
    D_actor_160900_8013DE20,
    { NULL, D_actor_160900_8013DB58, NULL, NULL, D_actor_160900_8013DBAC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013DE70[6] = {
#include "assets/actor_160900_animation_0C774_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013DEB8[187] = {
#include "assets/actor_160900_animation_0C774_bank4.inc"
};

GpAnimRec D_actor_160900_8013E1A4[242] = {
#include "assets/actor_160900_animation_0C774_records.inc"
};

u16 D_actor_160900_8013E56C[20] = {
#include "assets/actor_160900_animation_0C774_indices.inc"
};

GpAnimSet D_actor_160900_8013E594 = {
    D_actor_160900_8013E1A4,
    D_actor_160900_8013E56C,
    { NULL, D_actor_160900_8013DE70, NULL, NULL, D_actor_160900_8013DEB8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013E5BC[2] = {
#include "assets/actor_160900_animation_0CA18_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013E5D4[49] = {
#include "assets/actor_160900_animation_0CA18_bank4.inc"
};

GpAnimRec D_actor_160900_8013E698[94] = {
#include "assets/actor_160900_animation_0CA18_records.inc"
};

u16 D_actor_160900_8013E810[20] = {
#include "assets/actor_160900_animation_0CA18_indices.inc"
};

GpAnimSet D_actor_160900_8013E838 = {
    D_actor_160900_8013E698,
    D_actor_160900_8013E810,
    { NULL, D_actor_160900_8013E5BC, NULL, NULL, D_actor_160900_8013E5D4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013E860[4] = {
#include "assets/actor_160900_animation_0CDF4_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013E890[56] = {
#include "assets/actor_160900_animation_0CDF4_bank4.inc"
};

GpAnimRec D_actor_160900_8013E970[159] = {
#include "assets/actor_160900_animation_0CDF4_records.inc"
};

u16 D_actor_160900_8013EBEC[20] = {
#include "assets/actor_160900_animation_0CDF4_indices.inc"
};

GpAnimSet D_actor_160900_8013EC14 = {
    D_actor_160900_8013E970,
    D_actor_160900_8013EBEC,
    { NULL, D_actor_160900_8013E860, NULL, NULL, D_actor_160900_8013E890, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160900_8013EC3C[9] = {
#include "assets/actor_160900_animation_0D334_bank1.inc"
};

AnimationPackedRotation D_actor_160900_8013ECA8[124] = {
#include "assets/actor_160900_animation_0D334_bank4.inc"
};

GpAnimRec D_actor_160900_8013EE98[165] = {
#include "assets/actor_160900_animation_0D334_records.inc"
};

u16 D_actor_160900_8013F12C[20] = {
#include "assets/actor_160900_animation_0D334_indices.inc"
};

GpAnimSet D_actor_160900_8013F154 = {
    D_actor_160900_8013EE98,
    D_actor_160900_8013F12C,
    { NULL, D_actor_160900_8013EC3C, NULL, NULL, D_actor_160900_8013ECA8, NULL, NULL, NULL },
};

TaskDesc D_actor_160900_8013F17C[2] = {
    { 0, 192, func_actor_160900_80131EB0, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 D_actor_160900_8013F194 = 256;

u8 D_actor_160900_8013F198[44] = {
    48,
    219,
    19,
    128,
    88,
    175,
    19,
    128,
    92,
    187,
    19,
    128,
    124,
    193,
    19,
    128,
    16,
    198,
    19,
    128,
    200,
    199,
    19,
    128,
    44,
    216,
    19,
    128,
    56,
    232,
    19,
    128,
    148,
    229,
    19,
    128,
    72,
    222,
    19,
    128,
    160,
    201,
    19,
    128,
};

u8 D_actor_160900_8013F1C4[8] = {
    20,
    236,
    19,
    128,
    84,
    241,
    19,
    128,
};

ActorAnimStep D_actor_160900_8013F1CC[11] = {
    { 0, -1 },
    { 0, -1 },
    { 0, 3 },
    { 0, 4 },
    { 0, 5 },
    { 0, -1 },
    { 0, -1 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, -1 },
};

u8 D_actor_160900_8013F1F8[8] = {
    0,
    0,
    255,
    255,
    0,
    0,
    255,
    255,
};

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, GpXformArg*);
        void (*call1)(Task*, s32, s32);
    } handler;
} Actor160900MessageEntry;
STATIC_ASSERT_SIZEOF(Actor160900MessageEntry, 8);

Actor160900MessageEntry D_actor_160900_8013F200[2] = {
    { 2005, { .call1 = func_actor_160900_801345D0 } },
    { 2004, { .call0 = func_actor_160900_80134624 } },
};

u8 D_actor_160900_8013F210[24] = {
    148,
    17,
    0,
    0,
    0,
    0,
    0,
    0,
    128,
    12,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
};

u8 D_actor_160900_8013F228[24] = {
    148,
    17,
    0,
    0,
    0,
    0,
    0,
    0,
    128,
    12,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    12,
    0,
    0,
    0,
    0,
};

u8 D_actor_160900_8013F240[24] = {
    16,
    39,
    0,
    0,
    0,
    0,
    0,
    0,
    128,
    12,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    252,
    0,
    0,
    0,
    0,
};

SVECTOR D_actor_160900_8013F258[17] = {
    { 6000, 1000, 2000, 0 },
    { 6000, 500, 2000, 0 },
    { 6000, 0, 2000, 0 },
    { 6000, 500, 2500, 0 },
    { 6000, 0, 2500, 0 },
    { 6000, -500, 2500, 0 },
    { 6000, 1000, 3000, 0 },
    { 6000, 500, 3000, 0 },
    { 6000, 0, 3000, 0 },
    { 6000, -500, 3000, 0 },
    { 6000, 500, 3500, 0 },
    { 6000, 0, 3500, 0 },
    { 6000, -500, 3500, 0 },
    { 6000, 1000, 4000, 0 },
    { 6000, 500, 4000, 0 },
    { 6000, 0, 4000, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_actor_160900_8013F2E0[26] = {
    { 6000, 500, 2000, 0 },
    { 6000, 0, 2000, 0 },
    { 6000, -500, 2000, 0 },
    { 6000, -1000, 2000, 0 },
    { 6000, -1500, 2000, 0 },
    { 6000, 500, 2500, 0 },
    { 6000, 0, 2500, 0 },
    { 6000, -500, 2500, 0 },
    { 6000, -1000, 2500, 0 },
    { 6000, -1500, 2500, 0 },
    { 6000, 500, 3000, 0 },
    { 6000, 0, 3000, 0 },
    { 6000, -500, 3000, 0 },
    { 6000, -1000, 3000, 0 },
    { 6000, -1500, 3000, 0 },
    { 6000, 500, 3500, 0 },
    { 6000, 0, 3500, 0 },
    { 6000, -500, 3500, 0 },
    { 6000, -1000, 3500, 0 },
    { 6000, -1500, 3500, 0 },
    { 6000, 500, 4000, 0 },
    { 6000, 0, 4000, 0 },
    { 6000, -500, 4000, 0 },
    { 6000, -1000, 4000, 0 },
    { 6000, -1500, 4000, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_actor_160900_8013F3B0[10] = {
    { 6000, -1000, 2500, 0 },
    { 6000, -500, 2500, 0 },
    { 6000, 0, 2500, 0 },
    { 6000, -1000, 3000, 0 },
    { 6000, -500, 3000, 0 },
    { 6000, 0, 3000, 0 },
    { 6000, -1000, 3500, 0 },
    { 6000, -500, 3500, 0 },
    { 6000, 0, 3500, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_actor_160900_8013F400[11] = {
    { 8000, 0, 3000, 0 },
    { 8000, -500, 3000, 0 },
    { 8000, 0, 2900, 0 },
    { 8000, -500, 2900, 0 },
    { 8000, 0, 2850, 0 },
    { 8000, -500, 2850, 0 },
    { 8000, 0, 2800, 0 },
    { 8000, -500, 2800, 0 },
    { 8000, 0, 2750, 0 },
    { 8000, -500, 2750, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_actor_160900_8013F458[27] = {
    { 7000, 3000, 3500, 0 },
    { 7000, 3000, 3000, 0 },
    { 7000, 3000, 2500, 0 },
    { 7000, 2000, 3500, 0 },
    { 7000, 2000, 3000, 0 },
    { 7000, 2000, 2500, 0 },
    { 7000, 1000, 3500, 0 },
    { 7000, 1000, 3000, 0 },
    { 7000, 1000, 2500, 0 },
    { 7000, 0, 4000, 0 },
    { 7000, 0, 3500, 0 },
    { 7000, 0, 3000, 0 },
    { 7000, 0, 2500, 0 },
    { 7000, 0, 2000, 0 },
    { 7000, -1500, 4000, 0 },
    { 7000, -1500, 3500, 0 },
    { 7000, -1500, 3000, 0 },
    { 7000, -1500, 2500, 0 },
    { 7000, -1500, 2000, 0 },
    { 7000, -3000, 4500, 0 },
    { 7000, -3000, 4000, 0 },
    { 7000, -3000, 3500, 0 },
    { 7000, -3000, 3000, 0 },
    { 7000, -3000, 2500, 0 },
    { 7000, -3000, 2000, 0 },
    { 7000, -3000, 1500, 0 },
    { 0, 0, 0, -1 },
};

GpOverlayIds D_actor_160900_8013F530 = { 6, 9, 11 };

GpEvsCmd D_actor_160900_8013F538[58] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 13, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_80134790 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_801347B0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_801347D0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_actor_160900_8013F530 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_160900_80134830 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_160900_80133880 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_160900_80134850 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_160900_801346B0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_80134790 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_801347D0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_160900_80134710 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_160900_80133A84 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_801347B0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_80134790 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_80134790 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_80134790 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_80134790 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_80134790 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_801347D0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_160900_80134710 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_160900_80133F90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_80134790 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_80134790 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_801347D0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_160900_80134710 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_80134790 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_801347B0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_801347D0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = Gp_PulseState1C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_160900_801346E0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_160900_801347B0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_160900_80134870 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_160900_8013FAA8[7] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_160900_801347F0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 43 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_actor_160900_8013FB50[8] = {
    { 0, 192, func_actor_160900_8013418C, { .model = NULL } },
    { 0, 192, func_actor_160900_801344D8, { .model = NULL } },
    { 0, 192, func_actor_160900_801343E4, { .model = NULL } },
    { 257, 192, func_actor_160900_80132C08, { .model = &D_actor_160900_801393B8 } },
    { 257, 192, func_actor_160900_80132A14, { .model = &D_actor_160900_801397FC } },
    { 257, 192, func_actor_160900_80132A14, { .model = &D_actor_160900_80139C50 } },
    { 257, 192, func_actor_160900_80132A14, { .model = &D_actor_160900_8013A0A4 } },
    { 2, 192, func_actor_160900_80132E80, { .model = NULL } },
};

OverlayWaveCtx* D_actor_160900_8013FBB0;

Task* D_actor_160900_8013FBB4;

OverlayWaveRec6 D_actor_160900_8013FBB8[13];

OverlayWaveRec6 D_actor_160900_8013FC08[30];

extern u8 D_actor_160900_8013F240[];

/// Animation source `func_800B3F84` seeds the child's slots from, the table
/// published as `Actor160900Child3Work::field_4B4`, and the message table
/// published as `Task::msgTable`.
extern u8 D_actor_160900_8013F1C4[];

extern u8 D_actor_160900_8013F1F8[];

extern Actor160900MessageEntry D_actor_160900_8013F200[2];

static s32         func_actor_160900_801326EC(Task* arg0);
static inline void func_actor_160900_Reseed(Task* arg0, u16 anim);
static s32         func_actor_160900_80132844(Task* arg0);
static inline void func_actor_160900_InitAnim(Task* task, TmdObject* obj);
static inline void func_actor_160900_SetAnim(Task* task, u16 anim);
static inline void func_actor_160900_SetAnimZ(Task* task, u16 anim);
static void        func_actor_160900_80133238(Task* arg0);
static void        func_actor_160900_8013358C(Task* arg0);
static void        func_actor_160900_80133758(SVECTOR* pts);

/// Screen-wave task spawned from `D_actor_160900_8013F17C` with the overlay's
/// work block as its argument. State 0 seeds the column and row phases, parks
/// the argument and clears its ramp; state 1 ramps the frame up to the span
/// (ramp state 0) or back down to zero (ramp state 1, then 2, which kills the
/// task and restores the display field), and redraws the frame buffer as a
/// 10 by 30 mesh of textured quads displaced by sine waves of that amplitude,
/// tinted when the block's tint flag is set.
///
/// `Task::state` is read as a scalar through a cast: that keeps the load
/// behind the `CdCmd_Queue.field_22A` store, which a member read lets GCC hoist above it.
void func_actor_160900_80131EB0(Task* arg0)
{
    OverlayWaveCtx* ctx;
    POLY_FT4*       p;
    DR_STP*         stp;
    s32             i, j, k;
    s32             drawY;
    s32             tpage0, tpage1;
    s32             u0, u1, v0, v1;
    s32             waveX0, waveY0, waveX1, waveY1;
    s32             waveX2, waveY2, waveX3, waveY3;

    CdCmd_Queue.field_22A = 2;
    /* `Task::state` read as a scalar through a cast: that keeps the load
       behind the `CdCmd_Queue.field_22A` store, which a member read lets GCC hoist
       above it. */
    switch (*(s32*)((u8*)arg0 + OFFSET_OF(Task, state))) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_actor_160900_8013FBB8[i].phase  = 0;
                D_actor_160900_8013FBB8[i].offset = (u32)rand() >> 3;
                D_actor_160900_8013FBB8[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_actor_160900_8013FC08[i].phase  = 0;
                D_actor_160900_8013FC08[i].offset = (u32)rand() >> 3;
                D_actor_160900_8013FC08[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_actor_160900_8013F194        = 0;
            D_actor_160900_8013FBB0        = arg0->spawnArg2.pointer;
            D_actor_160900_8013FBB0->frame = 0;
            D_actor_160900_8013FBB0->state = 0;
            Display_ClampField126(-8);
            arg0->state++;
            break;
        case 1:
            ctx = D_actor_160900_8013FBB0;
            switch (ctx->state) {
                case 0:
                    if (ctx->frame < ctx->span) {
                        ctx->frame++;
                    }
                    break;
                case 1:
                    if (ctx->frame > 0) {
                        ctx->frame--;
                    } else {
                        ctx->state = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_actor_160900_8013F194 = D_actor_160900_8013FBB0->frame * D_actor_160900_8013FBB0->scale / D_actor_160900_8013FBB0->span;
            for (i = 0; i < 11; i++) {
                D_actor_160900_8013FBB8[i].phase += D_actor_160900_8013FBB8[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_actor_160900_8013FC08[i].phase += D_actor_160900_8013FC08[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (D_actor_160900_8013FBB0->blend == 0) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_actor_160900_8013FBB0->r;
                        p->g0 = D_actor_160900_8013FBB0->g;
                        p->b0 = D_actor_160900_8013FBB0->b;
                    }
                    u0 = k * 32;
                    u1 = (k + 1) * 32;
                    if (u1 == 320)
                        u1 = 319;
                    if (u0 < 128) {
                        p->tpage = tpage0;
                    } else {
                        p->tpage = tpage1;
                        u0      -= 128;
                        u1      -= 128;
                    }
                    if (j != -1) {
                        v1     = (j + 1) * 8 + gDisplayState.drawBuffer * 16;
                        v0     = j * 8 + gDisplayState.drawBuffer * 16;
                        waveX0 = D_actor_160900_8013F194 * (rsin((j << 9) + D_actor_160900_8013FBB8[k].phase + D_actor_160900_8013FBB8[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_actor_160900_8013F194 * (rsin((k << 10) + D_actor_160900_8013FC08[j].phase + D_actor_160900_8013FC08[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_actor_160900_8013F194 * (rsin((j << 9) + D_actor_160900_8013FBB8[k + 1].phase + D_actor_160900_8013FBB8[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_actor_160900_8013F194 * (rsin(((k + 1) << 10) + D_actor_160900_8013FC08[j].phase + D_actor_160900_8013FC08[j].offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        drawY = gDisplayState.drawBuffer * 16;
                        p->x0 = k * 32 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 32 - 160;
                        p->y1 = -112;
                        v0    = drawY + 8;
                        v1    = drawY;
                    }
                    {

                        waveX2 = D_actor_160900_8013F194 * (rsin(((j + 1) << 9) + D_actor_160900_8013FBB8[k].phase + D_actor_160900_8013FBB8[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_actor_160900_8013F194 * (rsin((k << 10) + D_actor_160900_8013FC08[j + 1].phase + D_actor_160900_8013FC08[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_actor_160900_8013F194 * (rsin(((j + 1) << 9) + D_actor_160900_8013FBB8[k + 1].phase + D_actor_160900_8013FBB8[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_actor_160900_8013F194 * (rsin(((k + 1) << 10) + D_actor_160900_8013FC08[j + 1].phase + D_actor_160900_8013FC08[j + 1].offset) << 3);
                        p->y3  = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    p->u0 = u0;
                    p->v0 = v0;
                    p->u1 = u1;
                    p->v1 = v0;
                    p->u2 = u0;
                    p->v2 = v1;
                    p->u3 = u1;
                    p->v3 = v1;
                    addPrim(&gGpuCurrentOt[3], p);
                }
            }
            break;
    }
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
}

static s32 func_actor_160900_801326EC(Task* arg0)
{
    Actor160900Work* work;
    ActorAnimStep*   table;
    ActorAnimStep*   entry;
    ActorAnimStep*   entry2;
    GpAnimArg        msg;
    u16              anim;
    u16              anim2;

    work = (Actor160900Work*)arg0->work;
    if (work->field_34 == NULL) {
        return 1;
    }
    table = D_actor_160900_8013F1CC;
    entry = &table[work->field_64];
    if (entry->hold != 0) {
        if ((s16)work->field_66 >= entry->hold) {
            if (entry->animId < 0) {
                return 1;
            }
            anim              = entry->animId;
            msg.animBlock.ptr = D_actor_160900_8013F198;
            work->field_64    = anim;
            msg.field_4       = anim;
            msg.field_8       = 1;
            msg.field_C       = 0xA;
            msg.field_10      = 1;
            Gp_DispatchMsgPtr(work->field_34, 0x3F4, &msg, 0);
            work->field_66 = 0;
        } else {
            work->field_66 += 1;
        }
    } else {
        if (Gp_DispatchMsg(work->field_34, 0x3ED, 0, 0) != 0) {
            return 0;
        }
        entry2 = &D_actor_160900_8013F1CC[work->field_64];
        if (entry2->animId < 0) {
            return 1;
        }
        work = (Actor160900Work*)arg0->work;
        if (work->field_34 != NULL) {
            anim2             = entry2->animId;
            msg.animBlock.ptr = D_actor_160900_8013F198;
            work->field_64    = anim2;
            msg.field_4       = anim2;
            msg.field_8       = 1;
            msg.field_C       = 0xA;
            msg.field_10      = 1;
            Gp_DispatchMsgPtr(work->field_34, 0x3F4, &msg, 0);
            work->field_66 = 0;
        }
    }
    return 0;
}
static inline void func_actor_160900_Reseed(Task* arg0, u16 anim)
{
    Actor160900Child3Work* work;
    u16                    i;
    u16                    id;

    i               = 1;
    work            = (Actor160900Child3Work*)arg0->work;
    work->field_4B8 = anim;
    work->field_4BA = 0;
    id              = anim;
    TOUCH_REG_USE2(id, work, work);
    for (; i < 0x14; i++) {
        func_800B4114(&work->rig.anim, i, id, 0, 0xA);
    }
}

static s32 func_actor_160900_80132844(Task* arg0)
{
    Actor160900Child3Work* work;
    ActorAnimStep*         table;
    u16                    i;
    u16                    done;

    work = (Actor160900Child3Work*)arg0->work;
    if (arg0->extra.tmd->flags & 0x80) {
        return 0;
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < 0x14; i++) {
        if (!(work->rig.slots[i].flags & 0x100)) {
            done = 0;
            break;
        }
    }
    table = (ActorAnimStep*)work->field_4B4;
    if (table[(u16)work->field_4B8].hold != 0) {
        if (work->field_4BA >= table[(u16)work->field_4B8].hold) {
            if (table[(u16)work->field_4B8].animId >= 0) {
                func_actor_160900_Reseed(arg0, table[(u16)work->field_4B8].animId);
            } else {
                return 1;
            }
        } else {
            work->field_4BA++;
        }
    } else if (done) {
        if (table[(u16)work->field_4B8].animId >= 0) {
            func_actor_160900_Reseed(arg0, table[(u16)work->field_4B8].animId);
        } else {
            return 1;
        }
    }
    return 0;
}

void func_actor_160900_80132A14(Task* arg0)
{
    VECTOR pos;

    if (arg0->state == 0) {
        TmdObject*             tmd    = arg0->extra.tmd;
        Task*                  parent = arg0->spawnArg2.pointer;
        GfxCoord*              coord  = tmd->coords;
        Actor160900Child3Work* work;
        Actor160900Child3Work* block;
        GpAreaPlace*           place;
        u8                     id;

        block      = Mem_Malloc(0x4BC, 0);
        arg0->work = (TaskIdMap*)block;
        if (block == NULL) {
            taskKill(arg0);
            return;
        }
        work = block;
        switch (arg0->spawnArg1.value) {
            case 0:
                coord->parent = &parent->extra.tmd->coords[12];
                break;
            case 1:
            case 2:
                coord->parent = &parent->extra.tmd->coords[8];
                break;
        }
        Mem_Set(arg0->work, 0, 0x4BC);
        tmd->lightMtx = &work->light;
        tmd->colorMtx = &work->color;
        if (arg0->spawnArg1.value < 2) {
            place = Gp_GetNestedAreaRec(&gGameSession->at4.loc)->field_0;
            id    = place->entryId;
            while (id != 0xFF) {
                if (id == 0x65) {
                    break;
                }
                place++;
                id = place->entryId;
            }
            Gp_SetTmdBytes(arg0->extra.tmd, (s8)place->tpage, (s8)place->clut);
        } else if (arg0->spawnArg1.value == 2) {
            Gp_SetTmdBytes(arg0->extra.tmd, 0, 0);
        }
        Task_Reparent(parent, arg0);
        arg0->msgTable = D_actor_160900_8013F200;
        arg0->state   += 1;
        return;
    } else {
        TmdObject* obj = arg0->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = arg0->extra.tmd->coords->workm.t[1];
        pos.vz = arg0->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
}

/// Binds the child's animation context and resets slots 1-19. Taking the model
/// as a parameter is what schedules its load after the work-block load.
static inline void func_actor_160900_InitAnim(Task* task, TmdObject* obj)
{
    Actor160900Child3Work* work;
    s32                    i;

    work = (Actor160900Child3Work*)task->work;
    func_800B3F84(&work->rig.anim, D_actor_160900_8013F1C4, obj, work->rig.poses, work->rig.slots);
    work->field_4B4 = D_actor_160900_8013F1F8;
    work            = (Actor160900Child3Work*)task->work;
    i               = 1;
    work->field_4B8 = 0;
    work->field_4BA = 0;
    do {
        work->rig.slots[(u16)i].rate = 0x10;
        Gp_AnimResetSlot(&work->rig.anim, (u16)i, 0);
        i++;
    } while ((u16)i < 0x14U);
}

void func_actor_160900_80132C08(Task* task)
{
    TmdObject*             obj;
    TmdObject*             obj2;
    GfxCoord*              coord;
    Actor160900Child3Work* work;
    GpAreaPlace*           place;
    VECTOR                 pos;
    s32                    failed;

    if (task->state == 0) {
        obj        = task->extra.tmd;
        coord      = obj->coords;
        work       = (Actor160900Child3Work*)Mem_Malloc(0x4BC, false);
        task->work = (TaskIdMap*)work;
        if (work == NULL) {
            failed = 1;
        } else {
            coord->parent = &gGfxViewCoord;
            Mem_Set(task->work, 0, 0x4BC);
            obj->lightMtx  = &work->light;
            obj->colorMtx  = &work->color;
            obj->flags    |= 0x84;
            task->msgTable = D_actor_160900_8013F200;
            place          = Gp_GetNestedAreaRec(&gGameSession->at4.loc)->field_0;
            while (place->entryId != 0xFF && place->entryId != 0x65) {
                place++;
            }
            Gp_SetTmdBytes(task->extra.tmd, (s8)place->tpage, (s8)place->clut);
            Task_Reparent(D_actor_160900_8013FBB4, task);
            failed = 0;
        }
        if ((u16)failed) {
            taskKill(task);
            return;
        }
        func_actor_160900_InitAnim(task, task->extra.tmd);
        task->state++;
    }
    func_actor_160900_80132844(task);
    if (gGameSession->at4.loc.view == 0x2E) {
        Gfx_RotMatrixZ(&task->extra.tmd->coords[18].coord, 0x800, 1);
    } else {
        Gfx_RotMatrixX(&task->extra.tmd->coords[18].coord, 0x79C, 1);
    }
    obj2   = task->extra.tmd;
    pos.vx = obj2->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    func_800D7A9C(obj2, &pos, 0, 3);
}

void func_actor_160900_80132E80(Task* task)
{
    s16       xs[4];
    s16       ys[4];
    SVECTOR   origin;
    s32       sxy;
    s32       otz;
    GfxCoord* coord;
    SVECTOR*  verts;
    POLY_G4*  poly;
    DR_TPAGE* tp;
    s16       i;

    coord = task->extra.tmd->coords;
    verts = &((Actor160900ChildWork*)task->work)->field_0;
    Gp_UpdateCoord(coord);
    gte_SetTransMatrix(&coord->workm);
    gte_SetRotMatrix(&coord->workm);
    origin.vz = 0;
    origin.vy = 0;
    origin.vx = 0;
    gte_ldv0(&origin);
    gte_rtps();
    gte_stsxy(&sxy);
    gte_stszotz(&otz);
    for (i = 0; i < 4; i++) {
        gte_ldv0(&verts[i]);
        gte_rtps();
        gte_stsxy(&sxy);
        xs[i] = sxy;
        ys[i] = sxy >> 16;
    }

    poly           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = poly + 1;
    setlen(poly, 8);
    setcode(poly, 0x3A);
    poly->r0 = 0;
    poly->g0 = 0;
    poly->b0 = 0;
    poly->r1 = 0;
    poly->g1 = 0;
    poly->b1 = 0;
    switch (task->spawnArg1.value) {
        case 0:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r2 = 0;
            poly->g2 = 0;
            poly->b2 = 0;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
        case 1:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r3 = 0;
            poly->g3 = 0;
            poly->b3 = 0;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            break;
        case 2:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0xFF;
            poly->g1 = 0xFF;
            poly->b1 = 0xFF;
            poly->r2 = 0;
            poly->g2 = 0;
            poly->b2 = 0;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
        case 3:
            poly->r0 = 0xFF;
            poly->g0 = 0xFF;
            poly->b0 = 0xFF;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            poly->r3 = 0;
            poly->g3 = 0;
            poly->b3 = 0;
            break;
        case 4:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
        case 5:
            poly->r0 = 0xFF;
            poly->g0 = 0xFF;
            poly->b0 = 0xFF;
            poly->r1 = 0xFF;
            poly->g1 = 0xFF;
            poly->b1 = 0xFF;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
    }
    poly->x0 = xs[0];
    poly->y0 = ys[0];
    poly->x1 = xs[1];
    poly->y1 = ys[1];
    poly->x2 = xs[2];
    poly->y2 = ys[2];
    poly->x3 = xs[3];
    poly->y3 = ys[3];
    addPrim(&gGpuCurrentOt[otz >> 4], poly);
    tp             = gGpuPrimCursor;
    gGpuPrimCursor = tp + 1;
    setlen(tp, 1);
    tp->code[0] = 0xE1000220;
    addPrim(&gGpuCurrentOt[otz >> 4], tp);
}

static inline void func_actor_160900_SetAnim(Task* task, u16 anim)
{
    Actor160900Work* work;
    GpAnimArg        msg;
    GpAnimArg*       p;

    work = (Actor160900Work*)task->work;
    p    = &msg;
    if (work->field_34 != NULL) {
        p->animBlock.ptr = D_actor_160900_8013F198;
        work->field_64   = anim;
        p->field_4       = anim;
        p->field_8       = 1;
        p->field_C       = 10;
        p->field_10      = 1;
        Gp_DispatchMsgPtr(work->field_34, 0x3F4, p, 0);
        work->field_66 = 0;
    }
}

static inline void func_actor_160900_SetAnimZ(Task* task, u16 anim)
{
    Actor160900Work* work;
    GpAnimArg        msg;
    GpAnimArg*       p;

    work = (Actor160900Work*)task->work;
    p    = &msg;
    if (work->field_34 != NULL) {
        p->animBlock.ptr = D_actor_160900_8013F198;
        work->field_64   = anim;
        p->field_4       = anim;
        p->field_8       = 0;
        p->field_C       = 0;
        p->field_10      = 1;
        Gp_DispatchMsgPtr(work->field_34, 0x3F4, p, 0);
        work->field_66 = 0;
    }
}

/// Runs the one-shot request in `Actor160900Work::field_4C` (animation
/// changes on the player task, a spawn, a flag) and clears it.
static void func_actor_160900_80133238(Task* arg0)
{
    Actor160900Work* work;
    GpAnimArg        msg;
    Actor160900Work* w;
    s32              v;
    s32              x;

    work = (Actor160900Work*)arg0->work;
    func_actor_160900_801326EC(arg0);
    switch ((u16)work->field_4C) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsgPtr(work->field_34, 0x3E9, D_actor_160900_8013F210, 0);
            w = (Actor160900Work*)arg0->work;
            if (w->field_34 != NULL) {
                msg.animBlock.ptr = D_actor_160900_8013F198;
                w->field_64       = 10;
                msg.field_4       = 10;
                msg.field_8       = 0;
                msg.field_C       = 0;
                msg.field_10      = 1;
                Gp_DispatchMsgPtr(w->field_34, 0x3F4, &msg, 0);
                w->field_66 = 0;
            }
            break;
        case 2:
            w = (Actor160900Work*)arg0->work;
            if (w->field_34 != NULL) {
                msg.animBlock.ptr = D_actor_160900_8013F198;
                w->field_64       = 1;
                msg.field_4       = 1;
                msg.field_8       = 0;
                msg.field_C       = 0;
                msg.field_10      = 1;
                Gp_DispatchMsgPtr(w->field_34, 0x3F4, &msg, 0);
                w->field_66 = 0;
            }
            break;
        case 3:
            if ((u16)work->field_4E == 0) {
                x = Player_Status.weapon;
                if (Mc_SaveData[0].state.characterId == 1) {
                    v = x + 1;
                } else {
                    v = x + 0x22;
                }
                msg.animBlock.index = v;
                msg.field_4         = 1;
                msg.field_8         = 0;
                msg.field_C         = 0;
                msg.field_10        = 0;
                /* The message ABI carries this object address in one 32-bit word. */
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&msg, 0);
                Gp_DispatchMsgPtr(work->field_34, 0x3E9, D_actor_160900_8013F228, 0);
                msg.animBlock.index = -0x7D0;
                msg.field_4         = 0;
                msg.field_8         = 0xC80;
                Gp_DispatchMsgPtr(work->field_34, 0x3FB, &msg, 0);
                work->field_4E++;
            }
            return;
        case 4:
            work->wave.span  = 8;
            work->wave.scale = 0x80;
            Task_SpawnFromTable(D_actor_160900_8013F17C, 0, 0, &work->wave);
            work->field_4C = 0;
            return;
        case 5:
            work->wave.state = 2;
            break;
        case 6:
            func_actor_160900_SetAnimZ(arg0, 2);
            break;
        case 7:
            func_actor_160900_SetAnim(arg0, 6);
            break;
        case 8:
            func_actor_160900_SetAnim(arg0, 7);
            break;
        case 9:
            func_actor_160900_SetAnim(arg0, 8);
            break;
        case 10:
            func_actor_160900_SetAnim(arg0, 9);
            break;
    }
    work->field_4C = 0;
}

static void func_actor_160900_8013358C(Task* arg0)
{
    Actor160900Work*       work;
    Actor160900Child3Work* child;
    SVECTOR                ofs;
    SVECTOR                ofs2;
    s32                    i;

    work = (Actor160900Work*)arg0->work;
    switch ((u16)work->field_54) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsg(work->field_38, 0x7D5, 2, 0);
            Gp_DispatchMsgPtr(work->field_38, 0x7D4, D_actor_160900_8013F240, 0);
            break;
        case 2:
            Gp_DispatchMsg(work->field_3C, 0x7D5, 1, 0);
            Gp_DispatchMsg(work->field_40, 0x7D5, 1, 0);
            if (work->field_44 != NULL) {
                Gp_DispatchMsg(work->field_44, 0x7D5, 1, 0);
            }
            Gp_DispatchMsg(work->field_38, 0x7D5, 1, 0);
            ofs.vx = -100;
            ofs.vy = 100;
            ofs.vz = -1200;
            Gp_SpawnEff(0x60046, work->field_38->extra.tmd->coords, 0x20000100, &ofs);
            break;
        case 3:
            child            = (Actor160900Child3Work*)work->field_38->work;
            child->field_4B8 = 1;
            child->field_4BA = 0;
            do {
            } while (0);
            for (i = 1; (u16)i < 20; i++) {
                func_800B4114(&child->rig.anim, (u16)i, 1, 0, 10);
            }
            break;
        case 4:
            ofs2.vx = -200;
            ofs2.vy = 100;
            ofs2.vz = -400;
            Gp_SpawnEff(0x60046, work->field_38->extra.tmd->coords, 0x20000100, &ofs2);
            break;
    }
    work->field_54 = 0;
}

/// Spawn effect 0x601B4 at each point of a `pad == -1` terminated list, x
/// jittered by up to +-700; runs one frame in eight.
static void func_actor_160900_80133758(SVECTOR* pts)
{
    SVECTOR pos;
    s32     x;
    u32     seed;
    s32     flags;

    if (!(gDisplayState.animFrame & 7) && pts->pad != -1) {
        flags = 0x81203400;
        do {
            seed        = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = seed;
            x           = pts->vx + (((seed >> 16) & 1) ? ((Gp_LcgState = seed * 5 + 0x71357911) >> 16) & 7
                                                        : -(((Gp_LcgState = seed * 5 + 0x71357911) >> 16) & 7)) *
                              100;
            pos.vx = x;
            pos.vy = pts->vy;
            pos.vz = pts->vz;
            Gp_SpawnEff(0x601B4, NULL, flags, &pos);
            pts++;
        } while (pts->pad != -1);
    }
}

void func_actor_160900_80133880(void)
{
    Actor160900Work*      data;
    Actor160900ChildWork* alloc;
    Actor160900ChildWork* work;
    Task*                 task;

    data             = (Actor160900Work*)D_actor_160900_8013FBB4->work;
    task             = Task_SpawnFromTable(D_actor_160900_8013FB50, 7, 0, 0);
    data->field_C[0] = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0x3E8;
    task->extra.tmd->coords->coord.t[2] = 0xBB8;
    work->field_0.vx                    = 0;
    work->field_0.vy                    = -0x5DC;
    work->field_0.vz                    = 0x3E8;
    work->field_8.vx                    = 0;
    work->field_8.vy                    = -0x5DC;
    work->field_8.vz                    = 0;
    work->field_10.vx                   = 0;
    work->field_10.vy                   = 0;
    work->field_10.vz                   = 0x3E8;
    work->field_18.vx                   = 0;
    work->field_18.vy                   = 0;
    work->field_18.vz                   = 0;
    task                                = Task_SpawnFromTable(D_actor_160900_8013FB50, 7, 1, 0);
    data->field_C[1]                    = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0x3E8;
    task->extra.tmd->coords->coord.t[2] = 0xBB8;
    work->field_0.vx                    = 0;
    work->field_0.vy                    = -0x5DC;
    work->field_0.vz                    = 0;
    work->field_8.vx                    = 0;
    work->field_8.vy                    = -0x5DC;
    work->field_8.vz                    = -0x3E8;
    work->field_10.vx                   = 0;
    work->field_10.vy                   = 0;
    work->field_10.vz                   = 0;
    work->field_18.vx                   = 0;
    work->field_18.vy                   = 0;
    work->field_18.vz                   = -0x3E8;
}
void func_actor_160900_80133A84(void)
{
    Actor160900Work*      data;
    Actor160900ChildWork* alloc;
    Actor160900ChildWork* work;
    Task*                 task;

    data             = (Actor160900Work*)D_actor_160900_8013FBB4->work;
    task             = Task_SpawnFromTable(D_actor_160900_8013FB50, 7, 5, 0);
    data->field_C[0] = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                    = 0;
    work->field_0.vy                    = -0x3E8;
    work->field_0.vz                    = 0x1F4;
    work->field_8.vx                    = 0;
    work->field_8.vy                    = -0x3E8;
    work->field_8.vz                    = -0x1F4;
    work->field_10.vx                   = 0;
    work->field_10.vy                   = 0;
    work->field_10.vz                   = 0x1F4;
    work->field_18.vx                   = 0;
    work->field_18.vy                   = 0;
    work->field_18.vz                   = -0x1F4;
    task                                = Task_SpawnFromTable(D_actor_160900_8013FB50, 7, 2, 0);
    data->field_C[1]                    = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                    = 0;
    work->field_0.vy                    = -0x3E8;
    work->field_0.vz                    = 0x3E8;
    work->field_8.vx                    = 0;
    work->field_8.vy                    = -0x3E8;
    work->field_8.vz                    = 0x1F4;
    work->field_10.vx                   = 0;
    work->field_10.vy                   = 0;
    work->field_10.vz                   = 0x3E8;
    work->field_18.vx                   = 0;
    work->field_18.vy                   = 0;
    work->field_18.vz                   = 0x1F4;
    task                                = Task_SpawnFromTable(D_actor_160900_8013FB50, 7, 3, 0);
    data->field_C[2]                    = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                    = 0;
    work->field_0.vy                    = -0x3E8;
    work->field_0.vz                    = -0x1F4;
    work->field_8.vx                    = 0;
    work->field_8.vy                    = -0x3E8;
    work->field_8.vz                    = -0x3E8;
    work->field_10.vx                   = 0;
    work->field_10.vy                   = 0;
    work->field_10.vz                   = -0x1F4;
    work->field_18.vx                   = 0;
    work->field_18.vy                   = 0;
    work->field_18.vz                   = -0x3E8;
    task                                = Task_SpawnFromTable(D_actor_160900_8013FB50, 7, 4, 0);
    data->field_C[3]                    = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = -0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                    = 0;
    work->field_0.vy                    = -0x3E8;
    work->field_0.vz                    = 0x1F4;
    work->field_8.vx                    = 0;
    work->field_8.vy                    = -0x3E8;
    work->field_8.vz                    = -0x1F4;
    work->field_10.vx                   = 0;
    work->field_10.vy                   = 0;
    work->field_10.vz                   = 0x1F4;
    work->field_18.vx                   = 0;
    work->field_18.vy                   = 0;
    work->field_18.vz                   = -0x1F4;
    task                                = Task_SpawnFromTable(D_actor_160900_8013FB50, 7, 0, 0);
    data->field_C[4]                    = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = -0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                    = 0;
    work->field_0.vy                    = -0x3E8;
    work->field_0.vz                    = 0x3E8;
    work->field_8.vx                    = 0;
    work->field_8.vy                    = -0x3E8;
    work->field_8.vz                    = 0x1F4;
    work->field_10.vx                   = 0;
    work->field_10.vy                   = 0;
    work->field_10.vz                   = 0x3E8;
    work->field_18.vx                   = 0;
    work->field_18.vy                   = 0;
    work->field_18.vz                   = 0x1F4;
    task                                = Task_SpawnFromTable(D_actor_160900_8013FB50, 7, 1, 0);
    data->field_C[5]                    = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = -0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->field_0.vx                    = 0;
    work->field_0.vy                    = -0x3E8;
    work->field_0.vz                    = -0x1F4;
    work->field_8.vx                    = 0;
    work->field_8.vy                    = -0x3E8;
    work->field_8.vz                    = -0x3E8;
    work->field_10.vx                   = 0;
    work->field_10.vy                   = 0;
    work->field_10.vz                   = -0x1F4;
    work->field_18.vx                   = 0;
    work->field_18.vy                   = 0;
    work->field_18.vz                   = -0x3E8;
}
void func_actor_160900_80133F90(void)
{
    Actor160900Work*      data;
    Actor160900ChildWork* alloc;
    Actor160900ChildWork* work;
    Task*                 task;

    data             = (Actor160900Work*)D_actor_160900_8013FBB4->work;
    task             = Task_SpawnFromTable(D_actor_160900_8013FB50, 7, 0, 0);
    data->field_C[0] = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0;
    task->extra.tmd->coords->coord.t[2] = 0xBB8;
    work->field_0.vx                    = 0;
    work->field_0.vy                    = -0x5DC;
    work->field_0.vz                    = 0x3E8;
    work->field_8.vx                    = 0;
    work->field_8.vy                    = -0x5DC;
    work->field_8.vz                    = 0;
    work->field_10.vx                   = 0;
    work->field_10.vy                   = 0;
    work->field_10.vz                   = 0x3E8;
    work->field_18.vx                   = 0;
    work->field_18.vy                   = 0;
    work->field_18.vz                   = 0;
    task                                = Task_SpawnFromTable(D_actor_160900_8013FB50, 7, 1, 0);
    data->field_C[1]                    = task;
    if (task == NULL) {
        return;
    }
    alloc      = (Actor160900ChildWork*)memCalloc(0x20, 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    Mem_Set(work, 0, 0x20);
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0;
    task->extra.tmd->coords->coord.t[2] = 0xBB8;
    work->field_0.vx                    = 0;
    work->field_0.vy                    = -0x5DC;
    work->field_0.vz                    = 0;
    work->field_8.vx                    = 0;
    work->field_8.vy                    = -0x5DC;
    work->field_8.vz                    = -0x3E8;
    work->field_10.vx                   = 0;
    work->field_10.vy                   = 0;
    work->field_10.vz                   = 0;
    work->field_18.vx                   = 0;
    work->field_18.vy                   = 0;
    work->field_18.vz                   = -0x3E8;
}
void func_actor_160900_8013418C(Task* arg0)
{
    Actor160900Work* work;
    Actor160900Work* data;

    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != 0) {
                return;
            }
            work       = (Actor160900Work*)Mem_Malloc(0x68, 0);
            arg0->work = (TaskIdMap*)work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x68);
                work->field_34          = gameGetPtrSlot(3);
                D_actor_160900_8013FBB4 = arg0;
                work->field_38          = Task_SpawnFromTable(D_actor_160900_8013FB50, 3, 0, arg0);
                work->field_3C          = Task_SpawnFromTable(D_actor_160900_8013FB50, 5, 1, work->field_38);
                work->field_40          = Task_SpawnFromTable(D_actor_160900_8013FB50, 6, 0, work->field_38);
            }
            Gp_CapFile = 0;
            Gp_LoadCapFile(3);
            func_800E6D4C(0x180, 0);
            arg0->state += 1;
            return;
        case 1:
            func_800E8634(D_actor_160900_8013F538, 0, D_actor_160900_8013FAA8);
            arg0->state += 1;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                Mc_SaveData[0].state.sceneEvent = 0x1E;
                Task_RequestKill(arg0, 0);
            }
            break;
    }
    func_actor_160900_80133238(arg0);
    func_actor_160900_8013358C(arg0);
    data = (Actor160900Work*)arg0->work;
    switch ((u16)data->field_5C) {
        case 1:
            func_actor_160900_80133758(D_actor_160900_8013F258);
            break;
        case 2:
            func_actor_160900_80133758(D_actor_160900_8013F2E0);
            break;
        case 3:
            func_actor_160900_80133758(D_actor_160900_8013F3B0);
            break;
        case 4:
            func_actor_160900_80133758(D_actor_160900_8013F400);
            break;
        case 5:
            func_actor_160900_80133758(D_actor_160900_8013F458);
            break;
        case 0:
        default:
            data->field_5C = 0;
            break;
    }
}

void func_actor_160900_801343E4(Task* arg0)
{
    OverlayFadeWork* work;
    OverlayFadeWork* alloc;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r += (u16)arg0->spawnArg1.value;
            work->g += (u16)arg0->spawnArg1.value;
            work->b += (u16)arg0->spawnArg1.value;
            if (work->r >= 0x100) {
                work->b = 0xFF;
                work->g = 0xFF;
                work->r = 0xFF;
            }
            break;
    }
}
void func_actor_160900_801344D8(Task* arg0)
{
    OverlayFadeWork* work;
    OverlayFadeWork* alloc;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work    = alloc;
            work->b = 0xFF;
            work->g = 0xFF;
            work->r = 0xFF;
            goto state_inc;
        case 3:
            SetDispMask(1);
        case 1:
        case 2:
        state_inc:
            arg0->state += 1;
            /* fallthrough */
        case 4:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r -= (u16)arg0->spawnArg1.value;
            work->g -= (u16)arg0->spawnArg1.value;
            work->b -= (u16)arg0->spawnArg1.value;
            if (work->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}
/// Message 0x7D5 handler of `D_actor_160900_8013F200`: argument 1 clears the
/// 0x84 bits of the task's `TmdObject` flags, argument 2 sets them, and any
/// other value does nothing.
void func_actor_160900_801345D0(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = task->extra.tmd;
    switch (arg2) {
        case 0:
            break;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
        case 2:
            extra->flags = extra->flags | 0x84;
            return;
    }
}

/// Message 0x7D4 handler of `D_actor_160900_8013F200`: copies `placement` onto
/// the task's `TmdObject` coordinate frame. The three longs become the
/// translation, then yaw / pitch / roll are applied with `Gfx_RotMatrixY` /
/// `X` / `Z` and the coordinate is marked dirty.
void func_actor_160900_80134624(Task* task, s32 arg1, GpXformArg* placement)
{
    GfxCoord* coord;
    MATRIX*   mtx;

    coord             = task->extra.tmd->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Spawns `func_actor_160900_801344D8`, entry 1 of `D_actor_160900_8013FB50`,
/// with `arg0` as its first spawn argument.
void func_actor_160900_801346B0(s32 arg0)
{
    Task_SpawnFromTable(D_actor_160900_8013FB50, 1, arg0, 0);
}

/// Spawns the fade task `func_actor_160900_801343E4`, entry 2 of
/// `D_actor_160900_8013FB50`, with `arg0` as its first spawn argument.
void func_actor_160900_801346E0(s32 arg0)
{
    Task_SpawnFromTable(D_actor_160900_8013FB50, 2, arg0, 0);
}

void func_actor_160900_80134710(void)
{
    Actor160900Work* work;
    Task*            task;
    s16              i;

    work = (Actor160900Work*)D_actor_160900_8013FBB4->work;
    for (i = 0; i < 10; i++) {
        task = work->field_C[i];
        if (task != NULL) {
            taskKill(task);
            work->field_C[i] = NULL;
        }
    }
}
void func_actor_160900_80134790(s16 arg0)
{
    Actor160900Work* work;

    work           = (Actor160900Work*)D_actor_160900_8013FBB4->work;
    work->field_4C = arg0;
    work->field_4E = 0;
}
void func_actor_160900_801347B0(s16 arg0)
{
    Actor160900Work* work;

    work           = (Actor160900Work*)D_actor_160900_8013FBB4->work;
    work->field_54 = arg0;
    work->field_56 = 0;
}

void func_actor_160900_801347D0(s16 arg0)
{
    Actor160900Work* work;

    work           = (Actor160900Work*)D_actor_160900_8013FBB4->work;
    work->field_5C = arg0;
    work->field_5E = 0;
}
void func_actor_160900_801347F0(void)
{
    Actor160900Work* work;

    work           = (Actor160900Work*)D_actor_160900_8013FBB4->work;
    work->field_4C = 0;
    work->field_54 = 0;
    work->field_5C = 0;
    CdCmd_CancelReplaceAndActivate();
    SetDispMask(1);
}

void func_actor_160900_80134830(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_actor_160900_80134850(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_actor_160900_80134870(void)
{
    CdCmd_CancelReplaceAndActivate();
    Gp_RestoreStreamRng();
}
