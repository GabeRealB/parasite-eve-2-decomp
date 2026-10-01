#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
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
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/dryfield_garage.h"
#include "../../shared/screen_fade.h"
#include "../../shared/actor_messages.h"

/// Work block this overlay hangs off `Task::work`; each pair at
/// 0x4C0 and 0x4C8 is a request code plus its phase counter, reset together.
/// `field_4DE` is a 0/1 latch: `func_actor_120300_80133E94` calls
/// `Gp_SpawnWeaponEff` while it is set, clears it, then hands
/// `Gp_MsgPlayerWeapon` the zero that follows.
///
/// 0x4B8/0x4BC are `taskMessageDispatch` targets, not state:
/// `func_actor_120300_80133D04` sends message 0x7D5 to the actor, to 0x4B8 and
/// to 0x4BC in turn.
///
/// The block opens with a 0x14-byte animation context and its twenty 0x28-byte
/// animation slots. `Mem_Malloc` is
/// asked for 0x4E4 bytes -- the whole block -- by
/// `func_actor_120300_80132004` and `func_actor_120300_801321C8`, while
/// `func_actor_120300_80133330` walks slots 1..19 through `animationResetSlot`
/// after parking 8 in `field_4D4`, then lifts the scale at `field_4E0` to
/// 0x1000 once the actor is up.
typedef struct Actor120300Work {
    /* 0x000 */ ActorAnimRig20 rig;
    /* 0x474 */ MATRIX         field_474; // light matrix, into TmdObject::lightMtx
    /* 0x494 */ MATRIX         field_494; // colour matrix, into TmdObject::colorMtx
    /* 0x4B4 */ Task*          field_4B4; // taskMessageDispatch target for msgs 0x3E8/0x3E9
    /* 0x4B8 */ Task*          field_4B8;
    /* 0x4BC */ Task*          field_4BC;
    /* 0x4C0 */ s16            field_4C0;
    /* 0x4C2 */ s16            field_4C2;
    /* 0x4C4 */ u16            field_4C4; // phase countdown; request 9 advances once it reaches 0x10
    /* 0x4C6 */ byte           pad_4C6[0x2];
    /* 0x4C8 */ s16            field_4C8;
    /* 0x4CA */ s16            field_4CA;
    /* 0x4CC */ byte           pad_4CC[0x6];
    /* 0x4D2 */ u16            field_4D2; // animation index sent with message 0x3F4
    /* 0x4D4 */ u16            field_4D4; // animation id, indexed into the -1-terminated table below; written by func_actor_120300_80133330
    /* 0x4D6 */ u16            field_4D6;
    /* 0x4D8 */ u16            field_4D8;
    /* 0x4DA */ u16            field_4DA;
    /* 0x4DC */ s16            field_4DC; // facing, copied to and from the player's aim yaw
    /* 0x4DE */ s16            field_4DE; // player-eff flag: Gp_SpawnWeaponEff
    /* 0x4E0 */ s16            field_4E0; // uniform scale: broadcast to all three axes of a ScaleMatrix vector, so 0x1000 is 1.0
    /* 0x4E2 */ byte           pad_4E2[0x2];
} Actor120300Work;
STATIC_ASSERT_SIZEOF(Actor120300Work, 0x4E4);

extern Task* D_actor_120300_80141BA8;

/// The actor's five-entry task table, spawned from by index. Entries 2 and 3
/// are the two tasks kept in `field_4B8`/`field_4BC`; entry 4 is the fade to
/// black.
extern TaskDesc D_actor_120300_80141B6C[];

extern AnimationSet* D_actor_120300_801408CC[];
extern AnimationSet* D_actor_120300_80140910[19];
extern s16           D_actor_120300_8014095C[];

/// Animation id per `Actor120300Work::field_4D4`; -1 skips the restart.
extern s16 D_actor_120300_80140980[];

extern s32 D_actor_120300_801409A8[6];
extern s32 D_actor_120300_801409C0[24];
extern s32 D_actor_120300_80140A20[9];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, ActorTransform*);
        void (*call1)(Task*, s32, s32);
    } handler;
} Actor120300MessageEntry;
STATIC_ASSERT_SIZEOF(Actor120300MessageEntry, 8);

extern Actor120300MessageEntry D_actor_120300_80140A44[2];
extern ActorTransform          D_actor_120300_80140A54[13];
extern EvsCommand              D_actor_120300_80140B94[];
extern EvsCommand              D_actor_120300_80141524[];
extern EvsCommand              D_actor_120300_801416D4[];
extern EvsCommand              D_actor_120300_801417AC[];
extern EvsCommand              D_actor_120300_80141884[];
extern EvsCommand              D_actor_120300_8014195C[];
extern EvsCommand              D_actor_120300_80141A34[];

extern TmdSource D_actor_120300_80139A04;
extern TmdSource D_actor_120300_80139EDC;
extern TmdSource D_actor_120300_8013A4D0;
void             func_actor_120300_80132004(Task*);
void             func_actor_120300_801321C8(Task*);
void             func_actor_120300_80133330(s32);
void             func_actor_120300_801337C4(Task*);
void             func_actor_120300_80133C38(Task*, s32, s32);
void             func_actor_120300_80133D04(s32);
void             func_actor_120300_80133DA4(void);
void             func_actor_120300_80133DD4(void);
void             func_actor_120300_80133DF4(void);
void             func_actor_120300_80133E14(s16);
void             func_actor_120300_80133E34(s16);
void             func_actor_120300_80133E54(void);
void             func_actor_120300_80133E94(void);
void             func_actor_120300_80133EE4(void);
void             func_actor_120300_80133F14(Task*);

TmdBone D_actor_120300_80133F98[20] = {
#include "assets/gary_douglas_body_skeleton.inc"
};

u32 D_actor_120300_80134268[20] = {
#include "assets/gary_douglas_body_partVerts.inc"
};

SVECTOR D_actor_120300_801342B8[364] = {
#include "assets/gary_douglas_body_verts.inc"
};

SVECTOR D_actor_120300_80134E18[354] = {
#include "assets/gary_douglas_body_normals.inc"
};

u32 D_actor_120300_80135928[4151] = {
#include "assets/gary_douglas_body_stream.inc"
};

TmdSource D_actor_120300_80139A04 = {
    0,
    22008,
    6952,
    20,
    D_actor_120300_80134268,
    D_actor_120300_801342B8,
    D_actor_120300_80134E18,
    D_actor_120300_80133F98,
    D_actor_120300_80135928,
};

TmdBone D_actor_120300_80139A28[1] = {
#include "assets/gary_douglas_head_hat_skeleton.inc"
};

u32 D_actor_120300_80139A4C[1] = {
#include "assets/gary_douglas_head_hat_partVerts.inc"
};

SVECTOR D_actor_120300_80139A50[21] = {
#include "assets/gary_douglas_head_hat_verts.inc"
};

SVECTOR D_actor_120300_80139AF8[21] = {
#include "assets/gary_douglas_head_hat_normals.inc"
};

u32 D_actor_120300_80139BA0[207] = {
#include "assets/gary_douglas_head_hat_stream.inc"
};

TmdSource D_actor_120300_80139EDC = {
    0,
    1352,
    0,
    1,
    D_actor_120300_80139A4C,
    D_actor_120300_80139A50,
    D_actor_120300_80139AF8,
    D_actor_120300_80139A28,
    D_actor_120300_80139BA0,
};

TmdBone D_actor_120300_80139F00[1] = {
#include "assets/actor_120300_model_082F8_skeleton.inc"
};

u32 D_actor_120300_80139F24[1] = {
#include "assets/actor_120300_model_082F8_partVerts.inc"
};

SVECTOR D_actor_120300_80139F28[34] = {
#include "assets/actor_120300_model_082F8_verts.inc"
};

SVECTOR D_actor_120300_8013A038[28] = {
#include "assets/actor_120300_model_082F8_normals.inc"
};

u32 D_actor_120300_8013A118[238] = {
#include "assets/actor_120300_model_082F8_stream.inc"
};

TmdSource D_actor_120300_8013A4D0 = {
    0,
    1692,
    0,
    1,
    D_actor_120300_80139F24,
    D_actor_120300_80139F28,
    D_actor_120300_8013A038,
    D_actor_120300_80139F00,
    D_actor_120300_8013A118,
};

AnimationPackedPose D_actor_120300_8013A4F4[3] = {
#include "assets/actor_120300_animation_08A6C_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013A518[74] = {
#include "assets/actor_120300_animation_08A6C_bank4.inc"
};

AnimationRecord D_actor_120300_8013A640[137] = {
#include "assets/actor_120300_animation_08A6C_records.inc"
};

u16 D_actor_120300_8013A864[20] = {
#include "assets/actor_120300_animation_08A6C_indices.inc"
};

AnimationSet D_actor_120300_8013A88C = {
    D_actor_120300_8013A640,
    D_actor_120300_8013A864,
    { NULL, D_actor_120300_8013A4F4, NULL, NULL, D_actor_120300_8013A518, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013A8B4[2] = {
#include "assets/actor_120300_animation_08C54_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013A8CC[28] = {
#include "assets/actor_120300_animation_08C54_bank4.inc"
};

AnimationRecord D_actor_120300_8013A93C[68] = {
#include "assets/actor_120300_animation_08C54_records.inc"
};

u16 D_actor_120300_8013AA4C[20] = {
#include "assets/actor_120300_animation_08C54_indices.inc"
};

AnimationSet D_actor_120300_8013AA74 = {
    D_actor_120300_8013A93C,
    D_actor_120300_8013AA4C,
    { NULL, D_actor_120300_8013A8B4, NULL, NULL, D_actor_120300_8013A8CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013AA9C[2] = {
#include "assets/actor_120300_animation_08E18_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013AAB4[23] = {
#include "assets/actor_120300_animation_08E18_bank4.inc"
};

AnimationRecord D_actor_120300_8013AB10[64] = {
#include "assets/actor_120300_animation_08E18_records.inc"
};

u16 D_actor_120300_8013AC10[20] = {
#include "assets/actor_120300_animation_08E18_indices.inc"
};

AnimationSet D_actor_120300_8013AC38 = {
    D_actor_120300_8013AB10,
    D_actor_120300_8013AC10,
    { NULL, D_actor_120300_8013AA9C, NULL, NULL, D_actor_120300_8013AAB4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013AC60[2] = {
#include "assets/actor_120300_animation_09058_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013AC78[41] = {
#include "assets/actor_120300_animation_09058_bank4.inc"
};

AnimationRecord D_actor_120300_8013AD1C[77] = {
#include "assets/actor_120300_animation_09058_records.inc"
};

u16 D_actor_120300_8013AE50[20] = {
#include "assets/actor_120300_animation_09058_indices.inc"
};

AnimationSet D_actor_120300_8013AE78 = {
    D_actor_120300_8013AD1C,
    D_actor_120300_8013AE50,
    { NULL, D_actor_120300_8013AC60, NULL, NULL, D_actor_120300_8013AC78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013AEA0[2] = {
#include "assets/actor_120300_animation_0928C_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013AEB8[40] = {
#include "assets/actor_120300_animation_0928C_bank4.inc"
};

AnimationRecord D_actor_120300_8013AF58[75] = {
#include "assets/actor_120300_animation_0928C_records.inc"
};

u16 D_actor_120300_8013B084[20] = {
#include "assets/actor_120300_animation_0928C_indices.inc"
};

AnimationSet D_actor_120300_8013B0AC = {
    D_actor_120300_8013AF58,
    D_actor_120300_8013B084,
    { NULL, D_actor_120300_8013AEA0, NULL, NULL, D_actor_120300_8013AEB8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013B0D4[2] = {
#include "assets/actor_120300_animation_094DC_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013B0EC[29] = {
#include "assets/actor_120300_animation_094DC_bank4.inc"
};

AnimationRecord D_actor_120300_8013B160[93] = {
#include "assets/actor_120300_animation_094DC_records.inc"
};

u16 D_actor_120300_8013B2D4[20] = {
#include "assets/actor_120300_animation_094DC_indices.inc"
};

AnimationSet D_actor_120300_8013B2FC = {
    D_actor_120300_8013B160,
    D_actor_120300_8013B2D4,
    { NULL, D_actor_120300_8013B0D4, NULL, NULL, D_actor_120300_8013B0EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013B324[3] = {
#include "assets/actor_120300_animation_09770_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013B348[41] = {
#include "assets/actor_120300_animation_09770_bank4.inc"
};

AnimationRecord D_actor_120300_8013B3EC[95] = {
#include "assets/actor_120300_animation_09770_records.inc"
};

u16 D_actor_120300_8013B568[20] = {
#include "assets/actor_120300_animation_09770_indices.inc"
};

AnimationSet D_actor_120300_8013B590 = {
    D_actor_120300_8013B3EC,
    D_actor_120300_8013B568,
    { NULL, D_actor_120300_8013B324, NULL, NULL, D_actor_120300_8013B348, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013B5B8[8] = {
#include "assets/actor_120300_animation_09B34_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013B618[75] = {
#include "assets/actor_120300_animation_09B34_bank4.inc"
};

AnimationRecord D_actor_120300_8013B744[122] = {
#include "assets/actor_120300_animation_09B34_records.inc"
};

u16 D_actor_120300_8013B92C[20] = {
#include "assets/actor_120300_animation_09B34_indices.inc"
};

AnimationSet D_actor_120300_8013B954 = {
    D_actor_120300_8013B744,
    D_actor_120300_8013B92C,
    { NULL, D_actor_120300_8013B5B8, NULL, NULL, D_actor_120300_8013B618, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013B97C[8] = {
#include "assets/actor_120300_animation_09F24_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013B9DC[80] = {
#include "assets/actor_120300_animation_09F24_bank4.inc"
};

AnimationRecord D_actor_120300_8013BB1C[128] = {
#include "assets/actor_120300_animation_09F24_records.inc"
};

u16 D_actor_120300_8013BD1C[20] = {
#include "assets/actor_120300_animation_09F24_indices.inc"
};

AnimationSet D_actor_120300_8013BD44 = {
    D_actor_120300_8013BB1C,
    D_actor_120300_8013BD1C,
    { NULL, D_actor_120300_8013B97C, NULL, NULL, D_actor_120300_8013B9DC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013BD6C[2] = {
#include "assets/actor_120300_animation_0A178_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013BD84[38] = {
#include "assets/actor_120300_animation_0A178_bank4.inc"
};

AnimationRecord D_actor_120300_8013BE1C[85] = {
#include "assets/actor_120300_animation_0A178_records.inc"
};

u16 D_actor_120300_8013BF70[20] = {
#include "assets/actor_120300_animation_0A178_indices.inc"
};

AnimationSet D_actor_120300_8013BF98 = {
    D_actor_120300_8013BE1C,
    D_actor_120300_8013BF70,
    { NULL, D_actor_120300_8013BD6C, NULL, NULL, D_actor_120300_8013BD84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013BFC0[8] = {
#include "assets/actor_120300_animation_0A5B8_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013C020[88] = {
#include "assets/actor_120300_animation_0A5B8_bank4.inc"
};

AnimationRecord D_actor_120300_8013C180[140] = {
#include "assets/actor_120300_animation_0A5B8_records.inc"
};

u16 D_actor_120300_8013C3B0[20] = {
#include "assets/actor_120300_animation_0A5B8_indices.inc"
};

AnimationSet D_actor_120300_8013C3D8 = {
    D_actor_120300_8013C180,
    D_actor_120300_8013C3B0,
    { NULL, D_actor_120300_8013BFC0, NULL, NULL, D_actor_120300_8013C020, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013C400[2] = {
#include "assets/actor_120300_animation_0A854_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013C418[26] = {
#include "assets/actor_120300_animation_0A854_bank4.inc"
};

AnimationRecord D_actor_120300_8013C480[115] = {
#include "assets/actor_120300_animation_0A854_records.inc"
};

u16 D_actor_120300_8013C64C[20] = {
#include "assets/actor_120300_animation_0A854_indices.inc"
};

AnimationSet D_actor_120300_8013C674 = {
    D_actor_120300_8013C480,
    D_actor_120300_8013C64C,
    { NULL, D_actor_120300_8013C400, NULL, NULL, D_actor_120300_8013C418, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013C69C[2] = {
#include "assets/actor_120300_animation_0AAA4_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013C6B4[26] = {
#include "assets/actor_120300_animation_0AAA4_bank4.inc"
};

AnimationRecord D_actor_120300_8013C71C[96] = {
#include "assets/actor_120300_animation_0AAA4_records.inc"
};

u16 D_actor_120300_8013C89C[20] = {
#include "assets/actor_120300_animation_0AAA4_indices.inc"
};

AnimationSet D_actor_120300_8013C8C4 = {
    D_actor_120300_8013C71C,
    D_actor_120300_8013C89C,
    { NULL, D_actor_120300_8013C69C, NULL, NULL, D_actor_120300_8013C6B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013C8EC[2] = {
#include "assets/actor_120300_animation_0ACAC_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013C904[32] = {
#include "assets/actor_120300_animation_0ACAC_bank4.inc"
};

AnimationRecord D_actor_120300_8013C984[72] = {
#include "assets/actor_120300_animation_0ACAC_records.inc"
};

u16 D_actor_120300_8013CAA4[20] = {
#include "assets/actor_120300_animation_0ACAC_indices.inc"
};

AnimationSet D_actor_120300_8013CACC = {
    D_actor_120300_8013C984,
    D_actor_120300_8013CAA4,
    { NULL, D_actor_120300_8013C8EC, NULL, NULL, D_actor_120300_8013C904, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013CAF4[2] = {
#include "assets/actor_120300_animation_0B2E4_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013CB0C[160] = {
#include "assets/actor_120300_animation_0B2E4_bank4.inc"
};

AnimationRecord D_actor_120300_8013CD8C[212] = {
#include "assets/actor_120300_animation_0B2E4_records.inc"
};

u16 D_actor_120300_8013D0DC[20] = {
#include "assets/actor_120300_animation_0B2E4_indices.inc"
};

AnimationSet D_actor_120300_8013D104 = {
    D_actor_120300_8013CD8C,
    D_actor_120300_8013D0DC,
    { NULL, D_actor_120300_8013CAF4, NULL, NULL, D_actor_120300_8013CB0C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013D12C[7] = {
#include "assets/actor_120300_animation_0B83C_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013D180[104] = {
#include "assets/actor_120300_animation_0B83C_bank4.inc"
};

AnimationRecord D_actor_120300_8013D320[197] = {
#include "assets/actor_120300_animation_0B83C_records.inc"
};

u16 D_actor_120300_8013D634[20] = {
#include "assets/actor_120300_animation_0B83C_indices.inc"
};

AnimationSet D_actor_120300_8013D65C = {
    D_actor_120300_8013D320,
    D_actor_120300_8013D634,
    { NULL, D_actor_120300_8013D12C, NULL, NULL, D_actor_120300_8013D180, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013D684[6] = {
#include "assets/actor_120300_animation_0BBD0_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013D6CC[79] = {
#include "assets/actor_120300_animation_0BBD0_bank4.inc"
};

AnimationRecord D_actor_120300_8013D808[112] = {
#include "assets/actor_120300_animation_0BBD0_records.inc"
};

u16 D_actor_120300_8013D9C8[20] = {
#include "assets/actor_120300_animation_0BBD0_indices.inc"
};

AnimationSet D_actor_120300_8013D9F0 = {
    D_actor_120300_8013D808,
    D_actor_120300_8013D9C8,
    { NULL, D_actor_120300_8013D684, NULL, NULL, D_actor_120300_8013D6CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013DA18[6] = {
#include "assets/actor_120300_animation_0BED4_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013DA60[46] = {
#include "assets/actor_120300_animation_0BED4_bank4.inc"
};

AnimationRecord D_actor_120300_8013DB18[109] = {
#include "assets/actor_120300_animation_0BED4_records.inc"
};

u16 D_actor_120300_8013DCCC[20] = {
#include "assets/actor_120300_animation_0BED4_indices.inc"
};

AnimationSet D_actor_120300_8013DCF4 = {
    D_actor_120300_8013DB18,
    D_actor_120300_8013DCCC,
    { NULL, D_actor_120300_8013DA18, NULL, NULL, D_actor_120300_8013DA60, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013DD1C[2] = {
#include "assets/actor_120300_animation_0C154_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013DD34[52] = {
#include "assets/actor_120300_animation_0C154_bank4.inc"
};

AnimationRecord D_actor_120300_8013DE04[82] = {
#include "assets/actor_120300_animation_0C154_records.inc"
};

u16 D_actor_120300_8013DF4C[20] = {
#include "assets/actor_120300_animation_0C154_indices.inc"
};

AnimationSet D_actor_120300_8013DF74 = {
    D_actor_120300_8013DE04,
    D_actor_120300_8013DF4C,
    { NULL, D_actor_120300_8013DD1C, NULL, NULL, D_actor_120300_8013DD34, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013DF9C[4] = {
#include "assets/actor_120300_animation_0C4F8_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013DFCC[81] = {
#include "assets/actor_120300_animation_0C4F8_bank4.inc"
};

AnimationRecord D_actor_120300_8013E110[120] = {
#include "assets/actor_120300_animation_0C4F8_records.inc"
};

u16 D_actor_120300_8013E2F0[20] = {
#include "assets/actor_120300_animation_0C4F8_indices.inc"
};

AnimationSet D_actor_120300_8013E318 = {
    D_actor_120300_8013E110,
    D_actor_120300_8013E2F0,
    { NULL, D_actor_120300_8013DF9C, NULL, NULL, D_actor_120300_8013DFCC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013E340[3] = {
#include "assets/actor_120300_animation_0C7BC_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013E364[61] = {
#include "assets/actor_120300_animation_0C7BC_bank4.inc"
};

AnimationRecord D_actor_120300_8013E458[87] = {
#include "assets/actor_120300_animation_0C7BC_records.inc"
};

u16 D_actor_120300_8013E5B4[20] = {
#include "assets/actor_120300_animation_0C7BC_indices.inc"
};

AnimationSet D_actor_120300_8013E5DC = {
    D_actor_120300_8013E458,
    D_actor_120300_8013E5B4,
    { NULL, D_actor_120300_8013E340, NULL, NULL, D_actor_120300_8013E364, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013E604[2] = {
#include "assets/actor_120300_animation_0C948_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013E61C[16] = {
#include "assets/actor_120300_animation_0C948_bank4.inc"
};

AnimationRecord D_actor_120300_8013E65C[57] = {
#include "assets/actor_120300_animation_0C948_records.inc"
};

u16 D_actor_120300_8013E740[20] = {
#include "assets/actor_120300_animation_0C948_indices.inc"
};

AnimationSet D_actor_120300_8013E768 = {
    D_actor_120300_8013E65C,
    D_actor_120300_8013E740,
    { NULL, D_actor_120300_8013E604, NULL, NULL, D_actor_120300_8013E61C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013E790[2] = {
#include "assets/actor_120300_animation_0CBD4_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013E7A8[45] = {
#include "assets/actor_120300_animation_0CBD4_bank4.inc"
};

AnimationRecord D_actor_120300_8013E85C[92] = {
#include "assets/actor_120300_animation_0CBD4_records.inc"
};

u16 D_actor_120300_8013E9CC[20] = {
#include "assets/actor_120300_animation_0CBD4_indices.inc"
};

AnimationSet D_actor_120300_8013E9F4 = {
    D_actor_120300_8013E85C,
    D_actor_120300_8013E9CC,
    { NULL, D_actor_120300_8013E790, NULL, NULL, D_actor_120300_8013E7A8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013EA1C[2] = {
#include "assets/actor_120300_animation_0CE80_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013EA34[49] = {
#include "assets/actor_120300_animation_0CE80_bank4.inc"
};

AnimationRecord D_actor_120300_8013EAF8[96] = {
#include "assets/actor_120300_animation_0CE80_records.inc"
};

u16 D_actor_120300_8013EC78[20] = {
#include "assets/actor_120300_animation_0CE80_indices.inc"
};

AnimationSet D_actor_120300_8013ECA0 = {
    D_actor_120300_8013EAF8,
    D_actor_120300_8013EC78,
    { NULL, D_actor_120300_8013EA1C, NULL, NULL, D_actor_120300_8013EA34, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013ECC8[2] = {
#include "assets/actor_120300_animation_0D09C_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013ECE0[35] = {
#include "assets/actor_120300_animation_0D09C_bank4.inc"
};

AnimationRecord D_actor_120300_8013ED6C[74] = {
#include "assets/actor_120300_animation_0D09C_records.inc"
};

u16 D_actor_120300_8013EE94[20] = {
#include "assets/actor_120300_animation_0D09C_indices.inc"
};

AnimationSet D_actor_120300_8013EEBC = {
    D_actor_120300_8013ED6C,
    D_actor_120300_8013EE94,
    { NULL, D_actor_120300_8013ECC8, NULL, NULL, D_actor_120300_8013ECE0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013EEE4[3] = {
#include "assets/actor_120300_animation_0D42C_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013EF08[87] = {
#include "assets/actor_120300_animation_0D42C_bank4.inc"
};

AnimationRecord D_actor_120300_8013F064[112] = {
#include "assets/actor_120300_animation_0D42C_records.inc"
};

u16 D_actor_120300_8013F224[20] = {
#include "assets/actor_120300_animation_0D42C_indices.inc"
};

AnimationSet D_actor_120300_8013F24C = {
    D_actor_120300_8013F064,
    D_actor_120300_8013F224,
    { NULL, D_actor_120300_8013EEE4, NULL, NULL, D_actor_120300_8013EF08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013F274[2] = {
#include "assets/actor_120300_animation_0D674_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013F28C[35] = {
#include "assets/actor_120300_animation_0D674_bank4.inc"
};

AnimationRecord D_actor_120300_8013F318[85] = {
#include "assets/actor_120300_animation_0D674_records.inc"
};

u16 D_actor_120300_8013F46C[20] = {
#include "assets/actor_120300_animation_0D674_indices.inc"
};

AnimationSet D_actor_120300_8013F494 = {
    D_actor_120300_8013F318,
    D_actor_120300_8013F46C,
    { NULL, D_actor_120300_8013F274, NULL, NULL, D_actor_120300_8013F28C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013F4BC[2] = {
#include "assets/actor_120300_animation_0D99C_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013F4D4[35] = {
#include "assets/actor_120300_animation_0D99C_bank4.inc"
};

AnimationRecord D_actor_120300_8013F560[141] = {
#include "assets/actor_120300_animation_0D99C_records.inc"
};

u16 D_actor_120300_8013F794[20] = {
#include "assets/actor_120300_animation_0D99C_indices.inc"
};

AnimationSet D_actor_120300_8013F7BC = {
    D_actor_120300_8013F560,
    D_actor_120300_8013F794,
    { NULL, D_actor_120300_8013F4BC, NULL, NULL, D_actor_120300_8013F4D4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013F7E4[2] = {
#include "assets/actor_120300_animation_0DB94_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013F7FC[25] = {
#include "assets/actor_120300_animation_0DB94_bank4.inc"
};

AnimationRecord D_actor_120300_8013F860[75] = {
#include "assets/actor_120300_animation_0DB94_records.inc"
};

u16 D_actor_120300_8013F98C[20] = {
#include "assets/actor_120300_animation_0DB94_indices.inc"
};

AnimationSet D_actor_120300_8013F9B4 = {
    D_actor_120300_8013F860,
    D_actor_120300_8013F98C,
    { NULL, D_actor_120300_8013F7E4, NULL, NULL, D_actor_120300_8013F7FC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013F9DC[3] = {
#include "assets/actor_120300_animation_0DD8C_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013FA00[34] = {
#include "assets/actor_120300_animation_0DD8C_bank4.inc"
};

AnimationRecord D_actor_120300_8013FA88[63] = {
#include "assets/actor_120300_animation_0DD8C_records.inc"
};

u16 D_actor_120300_8013FB84[20] = {
#include "assets/actor_120300_animation_0DD8C_indices.inc"
};

AnimationSet D_actor_120300_8013FBAC = {
    D_actor_120300_8013FA88,
    D_actor_120300_8013FB84,
    { NULL, D_actor_120300_8013F9DC, NULL, NULL, D_actor_120300_8013FA00, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013FBD4[5] = {
#include "assets/actor_120300_animation_0E0E0_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013FC10[58] = {
#include "assets/actor_120300_animation_0E0E0_bank4.inc"
};

AnimationRecord D_actor_120300_8013FCF8[120] = {
#include "assets/actor_120300_animation_0E0E0_records.inc"
};

u16 D_actor_120300_8013FED8[20] = {
#include "assets/actor_120300_animation_0E0E0_indices.inc"
};

AnimationSet D_actor_120300_8013FF00 = {
    D_actor_120300_8013FCF8,
    D_actor_120300_8013FED8,
    { NULL, D_actor_120300_8013FBD4, NULL, NULL, D_actor_120300_8013FC10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_8013FF28[3] = {
#include "assets/actor_120300_animation_0E2D8_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8013FF4C[34] = {
#include "assets/actor_120300_animation_0E2D8_bank4.inc"
};

AnimationRecord D_actor_120300_8013FFD4[63] = {
#include "assets/actor_120300_animation_0E2D8_records.inc"
};

u16 D_actor_120300_801400D0[20] = {
#include "assets/actor_120300_animation_0E2D8_indices.inc"
};

AnimationSet D_actor_120300_801400F8 = {
    D_actor_120300_8013FFD4,
    D_actor_120300_801400D0,
    { NULL, D_actor_120300_8013FF28, NULL, NULL, D_actor_120300_8013FF4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_80140120[5] = {
#include "assets/actor_120300_animation_0E700_bank1.inc"
};

AnimationPackedRotation D_actor_120300_8014015C[90] = {
#include "assets/actor_120300_animation_0E700_bank4.inc"
};

AnimationRecord D_actor_120300_801402C4[141] = {
#include "assets/actor_120300_animation_0E700_records.inc"
};

u16 D_actor_120300_801404F8[20] = {
#include "assets/actor_120300_animation_0E700_indices.inc"
};

AnimationSet D_actor_120300_80140520 = {
    D_actor_120300_801402C4,
    D_actor_120300_801404F8,
    { NULL, D_actor_120300_80140120, NULL, NULL, D_actor_120300_8014015C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120300_80140548[6] = {
#include "assets/actor_120300_animation_0EA84_bank1.inc"
};

AnimationPackedRotation D_actor_120300_80140590[69] = {
#include "assets/actor_120300_animation_0EA84_bank4.inc"
};

AnimationRecord D_actor_120300_801406A4[118] = {
#include "assets/actor_120300_animation_0EA84_records.inc"
};

u16 D_actor_120300_8014087C[20] = {
#include "assets/actor_120300_animation_0EA84_indices.inc"
};

AnimationSet D_actor_120300_801408A4 = {
    D_actor_120300_801406A4,
    D_actor_120300_8014087C,
    { NULL, D_actor_120300_80140548, NULL, NULL, D_actor_120300_80140590, NULL, NULL, NULL },
};

AnimationSet* D_actor_120300_801408CC[17] = {
    &D_actor_120300_8013DCF4,
    &D_actor_120300_8013DF74,
    &D_actor_120300_8013E318,
    &D_actor_120300_8013E5DC,
    &D_actor_120300_8013E768,
    &D_actor_120300_8013E9F4,
    &D_actor_120300_8013ECA0,
    &D_actor_120300_8013EEBC,
    &D_actor_120300_8013F24C,
    &D_actor_120300_8013F494,
    &D_actor_120300_8013F7BC,
    &D_actor_120300_8013FBAC,
    &D_actor_120300_8013FF00,
    &D_actor_120300_801400F8,
    &D_actor_120300_80140520,
    &D_actor_120300_801408A4,
    &D_actor_120300_8013F9B4,
};

AnimationSet* D_actor_120300_80140910[19] = {
    NULL,
    &D_actor_120300_8013A88C,
    NULL,
    NULL,
    &D_actor_120300_8013AA74,
    &D_actor_120300_8013AC38,
    &D_actor_120300_8013AE78,
    &D_actor_120300_8013B0AC,
    &D_actor_120300_8013B2FC,
    &D_actor_120300_8013B590,
    &D_actor_120300_8013B954,
    &D_actor_120300_8013BD44,
    &D_actor_120300_8013BF98,
    &D_actor_120300_8013C3D8,
    &D_actor_120300_8013C674,
    &D_actor_120300_8013D65C,
    &D_actor_120300_8013D9F0,
    &D_actor_120300_8013C8C4,
    &D_actor_120300_8013CACC,
};

s16 D_actor_120300_8014095C[18] = {
    -1,
    -1,
    -1,
    10,
    10,
    10,
    10,
    10,
    0,
    0,
    -1,
    12,
    -1,
    0,
    0,
    0,
    10,
    0,
};

s16 D_actor_120300_80140980[20] = {
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    17,
    -1,
    -1,
    17,
    17,
    -1,
    14,
    0,
};

s32 D_actor_120300_801409A8[6] = { 0xF000, 0, 0, 0xF000, 4096, 0 };

s32 D_actor_120300_801409C0[24] = { -0x3E7F380, 2460, -0x3E7F380, 2000, 3200, 2460, 3200, 2000, -0x3E7F380, 2000, -0x3E7F0C4, 2000, 3200, 2000, 3900, 2000, -0x3E7F0C4, 2000, -0x3E7F0C4, 2460, 3900, 2000, 3900, 2460 };

s32 D_actor_120300_80140A20[9] = { 0x10000, 0x30002, 0, 0x50004, 0x70006, 1, 0x90008, 0xB000A, 2 };

Actor120300MessageEntry D_actor_120300_80140A44[2] = {
    { 2005, { .call1 = func_actor_120300_80133C38 } },
    { 2004, { .call0 = actorMsgPlaceInView } },
};

ActorTransform D_actor_120300_80140A54[13] = {
    { { 2414, 0, 2529, 0 }, { 0, 0, 0, 0 } },
    { { 2553, 0, 2396, 0 }, { 0, 1024, 0, 0 } },
    { { 2553, 0, 1700, 0 }, { 0, 1024, 0, 0 } },
    { { 2589, 0, 2068, 0 }, { 0, 1024, 0, 0 } },
    { { 1559, 0, 3583, 0 }, { 0, 1536, 0, 0 } },
    { { 2250, 0, 2200, 0 }, { 0, 1024, 0, 0 } },
    { { 800, 0, 3360, 0 }, { 0, 1536, 0, 0 } },
    { { 5000, 0, 1531, 0 }, { 0, 3072, 0, 0 } },
    { { 3792, 0, 1531, 0 }, { 0, 3072, 0, 0 } },
    { { 3475, 0, 2165, 0 }, { 0, 0, 0, 0 } },
    { { 3475, 0, 2165, 0 }, { 0, 3072, 0, 0 } },
    { { 3900, -300, 2070, 0 }, { 1638, 0, 0, 0 } },
    { { 8500, 0, 1531, 0 }, { 0, 3072, 0, 0 } },
};

EvsSceneKey D_actor_120300_80140B8C = { 2, 3, 11 };

EvsCommand D_actor_120300_80140B94[102] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_120300_80140B8C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133DA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133DD4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133E54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133EE4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133E94 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133DF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_80141524[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133E94 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133330 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_801416D4[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_801417AC[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_80141884[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_8014195C[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_80141A34[13] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_120300_80141B6C[5] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_120300_801337C4, { .model = &D_actor_120300_80139A04 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_120300_80133F14, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_120300_80132004, { .model = &D_actor_120300_80139EDC } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_120300_801321C8, { .model = &D_actor_120300_8013A4D0 } },
    { { { TASK_BODY_NONE, 192 } }, screenFadeOutTask, { .value = 0 } },
};

Task* D_actor_120300_80141BA8;

static inline void func_actor_120300_FillLight(Task* arg0, TmdObject* tmd, VECTOR* vec);
static s32         func_actor_120300_80131EE0(Task* arg0);
static inline s16  _actor120300InitChild(Task* arg0, s32 part);
static inline void _actor120300PlayAnim(Task* task, u16 anim);
static inline void _actor120300SetAnim(Task* task, u16 anim);
static void        func_actor_120300_80132338(Task* arg0);
static inline void _actor120300BlendAll(Task* task, u16 anim);
static inline void _actor120300ResetAll(Task* task, u16 anim);
static void        func_actor_120300_80132C60(Task* arg0);
static s32         func_actor_120300_801334A4(Task* arg0);
static void        func_actor_120300_801335D8(Task* task);

/// Fill part-1 translation and hand it to `func_800D7A9C`. `vec` is a
/// parameter rather than a local so its address stays out of the CSE class of
/// the `ScaleMatrix` argument that follows.
static inline void func_actor_120300_FillLight(Task* arg0, TmdObject* tmd, VECTOR* vec)
{
    vec->vx = tmd->coords[1].workm.t[0];
    vec->vy = arg0->extra.tmd->coords[1].workm.t[1];
    vec->vz = arg0->extra.tmd->coords[1].workm.t[2];
    func_800D7A9C(tmd, vec, 0, 3);
}

/// Ticks slots 1..19 of a task's animation context and, if every one of them
/// then has `ANIMATION_SLOT_SETTLED` set, re-reads the work block and
/// restarts all twenty slots on the id `D_actor_120300_80140980` selects for
/// `field_4D4`, returning 1; a negative entry or an unset slot returns 0. The
/// gotos reproduce retail's block layout.
static s32 func_actor_120300_80131EE0(Task* arg0)
{
    Actor120300Work* work;
    Actor120300Work* animWork;
    u16              anim;
    u16              i;
    u16              done;

    work = (Actor120300Work*)arg0->work;
    for (i = 1; i < 0x14; i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < 0x14; i++) {
        if (!(work->rig.slots[i].flags & ANIMATION_SLOT_SETTLED)) {
            goto fail;
        }
    }
check:
    if (done) {
        if (D_actor_120300_80140980[work->field_4D4] >= 0) {
            anim                = D_actor_120300_80140980[work->field_4D4];
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = anim;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&animWork->rig.anim, i, anim, 0, 10);
            }
        }
        return 1;
    }
    return 0;
}

/// Allocates and wires up a child's work block, anchoring its root
/// coordinate under part `part` of the spawning task's model. Returns
/// nonzero when the allocation failed.
static inline s16 _actor120300InitChild(Task* arg0, s32 part)
{
    TmdObject*       tmd   = arg0->extra.tmd;
    GfxCoord*        coord = tmd->coords;
    Actor120300Work* work;

    work       = Mem_Malloc(0x4E4, 0);
    arg0->work = work;
    if (work == NULL) {
        return 1;
    }
    Mem_Set(work, 0, 0x4E4);
    coord->parent          = ((Task*)arg0->spawnArg2.pointer)->extra.tmd->coords + part;
    arg0->extra.tmd->flags = 0;
    Tmd_AllocBuffers(tmd);
    tmd->lightMtx  = &work->field_474;
    tmd->colorMtx  = &work->field_494;
    arg0->msgTable = D_actor_120300_80140A44;
    return 0;
}

/// Spawn tick of a child actor that keeps the model facing the player: state 0
/// allocates the 0x4E4-byte `Actor120300Work` block, parks it in
/// `Task::work`, points the model's light and colour matrices at the block's
/// `field_474` / `field_494`, clears `TmdObject::flags` and anchors the root
/// coordinate `parent` under part 4 of the spawning task's model
/// (`Task::spawnArg2->extra`); a failed allocation kills the task instead of
/// stepping to state 1. The texture page / CLUT row then come from the
/// placement record at the nested area table's `field_0` list with resource-entry ID 0x6A (or the end record if that ID is absent). Every tick after that reads
/// the parent work block's `field_4E0` and primes the colour matrix with the
/// root coordinate's own translation through `func_800D7A9C`, then replaces
/// that translation with the parent scale broadcast over all three axes and
/// folds it in with `ScaleMatrix`.
void func_actor_120300_80132004(Task* task)
{
    VECTOR         vec;
    AreaPlacement* place;
    s32            scale;
    u16            scaleRaw;
    TmdObject*     tmd2;
    u8             id;

    if (task->state == 0) {
        if (_actor120300InitChild(task, 4) != 0) {
            taskKill(task);
            return;
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
    tmd2     = task->extra.tmd;
    scaleRaw = ((Actor120300Work*)((Task*)task->spawnArg2.pointer)->work)->field_4E0;
    vec.vx   = tmd2->coords->workm.t[0];
    vec.vy   = task->extra.tmd->coords->workm.t[1];
    vec.vz   = task->extra.tmd->coords->workm.t[2];
    func_800D7A9C(tmd2, &vec, 0, 3);
    scale  = scaleRaw & 0xFFFF;
    vec.vz = scale;
    vec.vy = scale;
    vec.vx = scale;
    ScaleMatrix(tmd2->colorMtx, &vec);
}

/// Spawn tick of a child actor. State 0 allocates the 0x4E4-byte
/// `Actor120300Work` block, parks it in `Task::work`, points the model's
/// light and colour matrices at the block's `field_474` / `field_494`, clears
/// `TmdObject::flags` and anchors the root coordinate `parent` under part 8 of
/// the spawning task's model (`Task::spawnArg2->extra`). A failed allocation
/// kills the task rather than stepping to state 1.
/// Every later tick reads the parent work block's `field_4E0` and primes the
/// colour matrix with the root coordinate's own translation through
/// `func_800D7A9C`, then replaces that translation with the parent scale
/// broadcast over all three axes and folds it in with `ScaleMatrix`.
void func_actor_120300_801321C8(Task* arg0)
{
    VECTOR     vec;
    s32        scale;
    u16        scaleRaw;
    TmdObject* tmd2;

    if (arg0->state == 0) {
        if (_actor120300InitChild(arg0, 8) != 0) {
            taskKill(arg0);
            return;
        }
        arg0->state += 1;
    }
    tmd2     = arg0->extra.tmd;
    scaleRaw = ((Actor120300Work*)((Task*)arg0->spawnArg2.pointer)->work)->field_4E0;
    vec.vx   = tmd2->coords->workm.t[0];
    vec.vy   = arg0->extra.tmd->coords->workm.t[1];
    vec.vz   = arg0->extra.tmd->coords->workm.t[2];
    func_800D7A9C(tmd2, &vec, 0, 3);
    scale  = scaleRaw & 0xFFFF;
    vec.vz = scale;
    vec.vy = scale;
    vec.vx = scale;
    ScaleMatrix(tmd2->colorMtx, &vec);
}

/// Records `anim` in `field_4D2` and sends the task at `field_4B4` message
/// 0x3F4 to blend into animation `anim` of `D_actor_120300_801408CC`. Does
/// nothing while that task is unset.
static inline void _actor120300PlayAnim(Task* task, u16 anim)
{
    Actor120300Work*     work = (Actor120300Work*)task->work;
    AnimationPlayRequest msg;

    if (work->field_4B4 != NULL) {
        msg.source.sets          = D_actor_120300_801408CC;
        work->field_4D2          = anim;
        msg.animationId          = anim;
        msg.blend                = ANIMATION_BLEND_INTERPOLATE;
        msg.blendFrames          = 0xA;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
    }
}

/// As `_actor120300PlayAnim`, but with `field_8` zero, so the receiver resets
/// its animation slots to `anim` instead of blending.
static inline void _actor120300SetAnim(Task* task, u16 anim)
{
    Actor120300Work*     work = (Actor120300Work*)task->work;
    AnimationPlayRequest msg;

    if (work->field_4B4 != NULL) {
        msg.source.sets          = D_actor_120300_801408CC;
        work->field_4D2          = anim;
        msg.animationId          = anim;
        msg.blend                = ANIMATION_BLEND_RESET;
        msg.blendFrames          = 0;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
    }
}

/// Sends an equipped-weapon playback request, with world collision disabled.
///
/// All arguments are evaluated once. The bank selector depends on the current
/// weapon and save character; `frames` is a whole-frame blend duration.
/// Dispatch consumes the block-local request synchronously.
#define ACTOR_120300_PLAY_PLAYER_WEAPON_ANIMATION(target, blendChoice, frames)                                                       \
    {                                                                                                                                \
        AnimationPlayRequest request;                                                                                                \
        s32                  weaponId;                                                                                               \
                                                                                                                                     \
        weaponId                     = gPlayerStatus.weapon;                                                                         \
        request.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22; \
        request.animationId          = 1;                                                                                            \
        request.blend                = (blendChoice);                                                                                \
        request.blendFrames          = (frames);                                                                                     \
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                                            \
        TASK_MESSAGE_DISPATCH_POINTER((target), ANIMATION_MESSAGE_PLAY, &request, 0);                                                \
    }

/// Request handler for the code latched in `field_4C0`. While the session
/// event state is set, a finished 0x3ED query advances `field_4D2` from
/// `D_actor_120300_8014095C` and restarts that animation. The switch sends
/// 0x3F4 animation payloads, 0x3E9 placement records and the 0x3E8 weapon
/// record, then clears the request. Request 9 waits 0x10 ticks on
/// `field_4C4` first; requests 20 and 21 step the facing in `field_4DC` by
/// 0x30 towards the player's bearing or towards zero, copying it onto the
/// player's aim yaw each tick.
static void func_actor_120300_80132338(Task* arg0)
{
    Actor120300Work* work;
    GameActor*       player;
    GfxCoord*        actorCoord;
    GfxCoord*        playerCoord;
    s32              dx;
    s32              dz;
    s32              diff;
    s16              target;
    s16              cur;
    ActorTransform*  rec;
    Task*            playerTask;

    work = (Actor120300Work*)arg0->work;
    if (gGameSession->eventState != 0) {
        if ((work->field_4B4 != NULL) && (taskMessageDispatch(work->field_4B4, 0x3ED, 0, 0) == 0)) {
            if (D_actor_120300_8014095C[work->field_4D2] >= 0) {
                _actor120300PlayAnim(arg0, D_actor_120300_8014095C[work->field_4D2]);
            }
        }
    }
    switch ((u16)work->field_4C0) {
        case 0:
            break;
        case 1:
            _actor120300SetAnim(arg0, 0);
            break;
        case 2:
            rec = &D_actor_120300_80140A54[6];
            TASK_MESSAGE_DISPATCH_POINTER(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, rec, 0);
            /* Both views belong to the same placement table. */
            TASK_MESSAGE_DISPATCH_POINTER(((Actor120300Work*)arg0->work)->field_4B4, 0x3F2, rec - 5, 0);
            break;
        case 3:
            TASK_MESSAGE_DISPATCH_POINTER(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, &D_actor_120300_80140A54[1], 0);
            _actor120300SetAnim(arg0, 1);
            break;
        case 4:
            _actor120300PlayAnim(arg0, 2);
            break;
        case 5:
            taskMessageDispatch(work->field_4B4, 0x3F3, 1, 0);
            TASK_MESSAGE_DISPATCH_POINTER(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, &D_actor_120300_80140A54[2], 0);
            _actor120300PlayAnim(arg0, 3);
            break;
        case 7:
            _actor120300PlayAnim(arg0, 5);
            break;
        case 8:
            _actor120300PlayAnim(arg0, 6);
            break;
        case 9:
            switch ((u16)work->field_4C2) {
                case 0:
                    work->field_4C4 = 0;
                    work->field_4C2++;
                    break;
                case 1:
                    if (++work->field_4C4 >= 0x10) {
                        _actor120300PlayAnim(arg0, 7);
                        work->field_4C0 = 0;
                    }
                    break;
            }
            return;
        case 10:
            _actor120300PlayAnim(arg0, 8);
            taskMessageDispatch(work->field_4B4, 0x3FD, 8, 0);
            break;
        case 11:
            TASK_MESSAGE_DISPATCH_POINTER(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, &D_actor_120300_80140A54[3], 0);
            _actor120300PlayAnim(arg0, 0xB);
            taskMessageDispatch(work->field_4B4, 0x3FD, 8, 0);
            break;
        case 12:
            _actor120300PlayAnim(arg0, 9);
            taskMessageDispatch(work->field_4B4, 0x3FD, 0x18, 0);
            break;
        case 13:
            TASK_MESSAGE_DISPATCH_POINTER(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, &D_actor_120300_80140A54[5], 0);
            ACTOR_120300_PLAY_PLAYER_WEAPON_ANIMATION(work->field_4B4, 0, 0);
            break;
        case 14:
            _actor120300PlayAnim(arg0, 0xF);
            taskMessageDispatch(work->field_4B4, 0x3FD, 8, 0);
            break;
        case 15:
            _actor120300PlayAnim(arg0, 0xE);
            taskMessageDispatch(work->field_4B4, 0x3FD, 8, 0);
            break;
        case 16:
            _actor120300PlayAnim(arg0, 0xD);
            taskMessageDispatch(work->field_4B4, 0x3FD, 8, 0);
            break;
        case 17:
            _actor120300PlayAnim(arg0, 0xE);
            taskMessageDispatch(work->field_4B4, 0x3FD, 8, 0);
            break;
        case 18:
            ACTOR_120300_PLAY_PLAYER_WEAPON_ANIMATION(work->field_4B4, 1, 0xA);
            break;
        case 19:
            _actor120300PlayAnim(arg0, 0x10);
            break;
        case 20:
            playerTask  = work->field_4B4;
            actorCoord  = arg0->extra.tmd->coords;
            playerCoord = playerTask->extra.tmd->coords;
            player      = (GameActor*)playerTask->work;
            switch ((u16)work->field_4C2) {
                case 0:
                    work->field_4DC = player->aimYaw;
                    work->field_4C2++;
                    /* fallthrough */
                case 1:
                    if (actorCoord->coord.t[0] > playerCoord->coord.t[0]) {
                        dx = (u16)actorCoord->coord.t[0] - (u16)playerCoord->coord.t[0];
                        dz = (u16)playerCoord->coord.t[2] - (u16)actorCoord->coord.t[2];
                    } else {
                        dx = (u16)playerCoord->coord.t[0] - (u16)actorCoord->coord.t[0];
                        dz = (u16)actorCoord->coord.t[2] - (u16)playerCoord->coord.t[2];
                    }
                    target = ratan2((s16)dz, (s16)dx);
                    cur    = work->field_4DC;
                    diff   = target - cur;
                    if (diff < 0) {
                        diff = -diff;
                    }
                    if (diff < 0x31) {
                        work->field_4C2++;
                    } else if (cur < target) {
                        work->field_4DC = cur + 0x30;
                    } else {
                        work->field_4DC = cur - 0x30;
                    }
                    /* fallthrough */
                case 2:
                    player->aimYaw = work->field_4DC;
                    return;
            }
            return;
        case 21: {
            GameActor* aim;

            aim = (GameActor*)work->field_4B4->work;
            if (ABS(work->field_4DC) < 0x31) {
                work->field_4DC = 0;
                work->field_4C0 = 0;
            } else if (work->field_4DC < 0) {
                work->field_4DC += 0x30;
            } else {
                work->field_4DC -= 0x30;
            }
            aim->aimYaw = work->field_4DC;
            return;
        }
    }
    work->field_4C0 = 0;
}

/// Cross-fades body slots 1..19 of `work`'s animation context to animation
/// `id` over `frames` frames.
#define _ACTOR120300_BLEND_SLOTS(work, id, frames)                   \
    do {                                                             \
        u16 _i;                                                      \
        for (_i = 1; _i < 0x14; _i++) {                              \
            func_800B4114(&(work)->rig.anim, _i, (id), 0, (frames)); \
        }                                                            \
    } while (0)

/// Parks `anim` in `field_4D4` and cross-fades every body slot to it over ten
/// frames.
static inline void _actor120300BlendAll(Task* task, u16 anim)
{
    Actor120300Work* work = (Actor120300Work*)task->work;

    work->field_4D4 = anim;
    _ACTOR120300_BLEND_SLOTS(work, anim, 10);
}

/// Parks `anim` in `field_4D4` and restarts every body slot on it at rate 0x10.
static inline void _actor120300ResetAll(Task* task, u16 anim)
{
    Actor120300Work* work = (Actor120300Work*)task->work;
    u16              i;

    work->field_4D4 = anim;
    for (i = 1; i < 0x14; i++) {
        work->rig.slots[i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, i, anim);
    }
}

/// After `func_actor_120300_80131EE0`, runs the request at `field_4C8` (0..19):
/// most codes park an animation id in `field_4D4` and walk slots 1..19 through
/// `func_800B4114` or `animationResetSlot`; a few also send message 0x7D4 or
/// change `field_4E0`. Code 1 is two-phase, stepped by `field_4CA`: phase 1
/// slides the model on X until `coord.t[0] < 0xF3D`. Every other code, and
/// code 1 once the slide ends, clears `field_4C8`.
static void func_actor_120300_80132C60(Task* arg0)
{
    TmdObject*       tmd;
    GfxCoord*        coord;
    Actor120300Work* work;
    s32              x;
    ActorTransform*  msg;

    tmd   = arg0->extra.tmd;
    work  = (Actor120300Work*)arg0->work;
    coord = tmd->coords;
    func_actor_120300_80131EE0(arg0);
    switch ((u16)work->field_4C8) {
        case 1:
            switch ((u16)work->field_4CA) {
                case 0:
                    TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_120300_80140A54[7], 0);
                    _actor120300ResetAll(arg0, 1);
                    work->field_4CA++;
                    return;
                case 1:
                    x                   = coord->coord.t[0];
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                    x                  -= 0x14;
                    coord->coord.t[0]   = x;
                    if (x < 0xF3D) {
                        _actor120300BlendAll(arg0, 0xE);
                        work->field_4C8 = 0;
                    }
                    return;
            }
            return;
        case 2:
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_120300_80140A54[8], 0);
            break;
        case 3:
            _actor120300BlendAll(arg0, 4);
            break;
        case 4:
            _actor120300BlendAll(arg0, 0x12);
            break;
        case 5:
            _actor120300BlendAll(arg0, 6);
            break;
        case 6:
            _actor120300BlendAll(arg0, 7);
            break;
        case 7:
            _actor120300BlendAll(arg0, 0xD);
            break;
        case 8:
            msg = &D_actor_120300_80140A54[9];
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, msg, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->field_4BC, 0x7D4, msg + 2, 0);
            _actor120300ResetAll(arg0, 8);
            break;
        case 9:
            _actor120300BlendAll(arg0, 0xB);
            break;
        case 10:
            _actor120300BlendAll(arg0, 9);
            break;
        case 11:
            _actor120300BlendAll(arg0, 0xA);
            break;
        case 12:
            _actor120300BlendAll(arg0, 0xC);
            break;
        case 13:
            work->field_4E0 = 0x400;
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_120300_80140A54[12], 0);
            _actor120300ResetAll(arg0, 0xE);
            break;
        case 14:
            work->field_4E0 = 0x1000;
            break;
        case 15:
            _actor120300BlendAll(arg0, 0xF);
            break;
        case 16:
            _actor120300BlendAll(arg0, 0x10);
            break;
        case 17:
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_120300_80140A54[10], 0);
            _actor120300ResetAll(arg0, 0x11);
            break;
        case 18:
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_120300_80140A54[9], 0);
            _actor120300ResetAll(arg0, 8);
            break;
        case 19:
            _actor120300BlendAll(arg0, 5);
            break;
        case 0:
        default:
            break;
    }
    work->field_4C8 = 0;
}

/// Sets the actor up for play: clears the model's `field_C`, sends message
/// 0x7D5 to the actor and to the two slots at 0x4B8/0x4BC, and resets animation
/// slots 1..19 to the 8 it first parks in `field_4D4`.
/// `func_actor_120300_801337C4` calls it with 1 once flag nibble 0x2D is set; a
/// zero argument additionally hands the task at 0x4B4 the player-weapon record
/// (`AnimationPlayRequest`, built from the equip-slot addend `gPlayerStatus.weapon`), lifts
/// `field_4E0` to 0x1000 and drops the pending overlay replacement.  The
/// request codes at 0x4C0 and 0x4C8 are cleared either way, so any phase
/// counter armed alongside them restarts from the top.
void func_actor_120300_80133330(s32 arg0)
{
    Task*                task;
    Actor120300Work*     work;
    Actor120300Work*     animWork;
    SVECTOR              unused;
    AnimationPlayRequest rec;
    s32                  i;
    s32                  weaponId;
    s32                  id;

    task                   = D_actor_120300_80141BA8;
    work                   = (Actor120300Work*)task->work;
    task->extra.tmd->flags = 0;
    TASK_MESSAGE_DISPATCH_POINTER(task, 0x7D4, &D_actor_120300_80140A54[9], 0);

    animWork            = (Actor120300Work*)task->work;
    animWork->field_4D4 = 8;
    i                   = 1;
    do {
        animWork->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&animWork->rig.anim, (u16)i, 8);
        i++;
    } while ((u16)i < 0x14U);

    taskMessageDispatch(work->field_4B8, 0x7D5, 1, 0);
    taskMessageDispatch(work->field_4BC, 0x7D5, 1, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->field_4BC, 0x7D4, &D_actor_120300_80140A54[11], 0);
    if (arg0 == 0) {
        weaponId                 = gPlayerStatus.weapon;
        id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
        rec.source.index         = id;
        rec.animationId          = 1;
        rec.blend                = ANIMATION_BLEND_RESET;
        rec.blendFrames          = 0;
        rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, ANIMATION_MESSAGE_PLAY, &rec, 0);
        TASK_MESSAGE_DISPATCH_POINTER(work->field_4B4, 0x3E9, &D_actor_120300_80140A54[5], 0);
        work->field_4E0 = 0x1000;
        CdCmd_CancelReplaceAndActivate();
    }
    work->field_4C0 = 0;
    work->field_4C8 = 0;
}

/// Tick for the two overlay-load phases the work block arms at 0x4DA: phase 0
/// walks the three-entry request list through 0x4D6 (0x416D4, then 0x417AC,
/// then 0x41884) before leaving through 0x4D8, while phase 1 issues the last
/// record 0x41A34 once and then only counts 0x4D8.  Each of the two phases
/// returns 1 while the session at `gGameSession->eventState` is still 0, so the
/// task that calls this keeps the actor alive until play starts.
static s32 func_actor_120300_801334A4(Task* arg0)
{
    Actor120300Work* work;

    work = arg0->work;
    switch (work->field_4DA) {
        case 0:
            switch (work->field_4D8) {
                case 0:
                    switch (work->field_4D6) {
                        case 0:
                            func_800E8614(D_actor_120300_801416D4, 0);
                            work->field_4D6++;
                            break;
                        case 1:
                            func_800E8614(D_actor_120300_801417AC, 0);
                            work->field_4D6++;
                            break;
                        default:
                            func_800E8614(D_actor_120300_80141884, 0);
                            break;
                    }
                    work->field_4D8++;
                    break;
                case 1:
                    if (gGameSession->eventState == 0) {
                        return 1;
                    }
                    break;
            }
            break;
        case 1:
            switch (work->field_4D8) {
                case 0:
                    func_800E8614(D_actor_120300_80141A34, 0);
                    work->field_4D8++;
                    break;
                case 1:
                    if (gGameSession->eventState == 0) {
                        return 1;
                    }
                    break;
            }
            break;
    }
    return 0;
}

/// Initialize the cutscene actor's model, animations and child tasks.
///
/// Uses the area placement for resource-entry 0x6A, or the end record when
/// that entry is absent. Allocation failure kills `task`.
static void func_actor_120300_801335D8(Task* task)
{
    enum { TEXTURE_RESOURCE_ENTRY_ID = 0x6A };

    Actor120300Work* work;
    Actor120300Work* allocatedWork;
    Actor120300Work* animWork;
    TmdObject*       tmd;
    GfxCoord*        coord;
    AreaPlacement*   place;
    u8               entryId;
    s32              slotIndex;

    tmd           = task->extra.tmd;
    coord         = tmd->coords;
    allocatedWork = Mem_Malloc(sizeof(Actor120300Work), 0);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    work = allocatedWork;
    Mem_Set(work, 0, sizeof(*work));
    work->field_4B4         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    D_actor_120300_80141BA8 = task;
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
    func_800B3F84(&work->rig.anim, D_actor_120300_80140910, tmd, work->rig.poses, work->rig.slots);
    animWork            = (Actor120300Work*)task->work;
    animWork->field_4D4 = 0xE;
    slotIndex           = 1;
    do {
        animWork->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&animWork->rig.anim, (u16)slotIndex, 0xE);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(animWork->rig.slots));
    work->field_4B8 = Task_SpawnFromTable(D_actor_120300_80141B6C, 2, 0, task);
    work->field_4BC = Task_SpawnFromTable(D_actor_120300_80141B6C, 3, 0, task);
    task->msgTable  = D_actor_120300_80140A44;
    work->field_4E0 = 0x1000;
    taskReparent(task, work->field_4B8);
    taskReparent(task, work->field_4BC);
}

/// Main tick of the cutscene actor. State 0 waits until no other cutscene is
/// up (`Gp_StateC08.field_A` / `gDisplayState.pendingMode`), builds the work block, then either arms
/// play (`func_actor_120300_80133330`) once flag nibble 0x2D is set or sends
/// the slot-3 weapon record and starts the script. States 1-4 step the area
/// records, the pending `Gp_TakePendingObj4C` cue, and the overlay-load
/// phases. Every path but the cutscene-busy early-out then ticks the two
/// animation helpers, draws the floor quad, and scales the model.
void func_actor_120300_801337C4(Task* arg0)
{
    union {
        struct {
            SVECTOR rot;
            VECTOR  vec;
        } draw;
        AnimationPlayRequest rec;
    } scratch;
    Actor120300Work* work;
    Actor120300Work* temp;
    TmdObject*       tmd;
    s32              state;
    s32              weaponId;
    s32              scale;
    s16              ready;
    s32              take;
    u16              scaleRaw;
    u16              evtId;
    u8               evtKind;
    u8               evtSub;

    state = arg0->state;
    work  = (Actor120300Work*)arg0->work;
    switch (state) {
        case 0:
            if ((Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                func_actor_120300_801335D8(arg0);
                work = (Actor120300Work*)arg0->work;
                if (GameFlag_GetNibble(0x2D) != 0) {
                    func_actor_120300_80133330(1);
                    if (GameFlag_GetNibble(0x2E) != 0) {
                        work->field_4D6 = 1;
                    }
                    arg0->state = 4;
                } else {
                    weaponId = gPlayerStatus.weapon;
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                        weaponId = weaponId + 1;
                    } else {
                        weaponId = weaponId + 0x22;
                    }
                    scratch.rec.source.index         = weaponId;
                    scratch.rec.animationId          = 1;
                    scratch.rec.blend                = ANIMATION_BLEND_RESET;
                    scratch.rec.blendFrames          = 0;
                    scratch.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &scratch.rec, 0);
                    GameFlag_SetNibble(0x2C, 1);
                    GameFlag_SetNibble(0x2D, 1);
                    func_800E3FAC(0xA2, 0xB);
                    func_800E8634(D_actor_120300_80140B94, 0, D_actor_120300_80141524);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 2;
                    arg0->state                                        += 1;
                }
                Mem_CopyUnaligned(&D_actor_120300_801409A8, D_dryfield_garage_8017DD6C, 0x18);
                Mem_CopyUnaligned(&D_actor_120300_80140A20, D_dryfield_garage_8017E1F4, sizeof(D_actor_120300_80140A20));
                Mem_CopyUnaligned(&D_actor_120300_801409C0, D_dryfield_garage_8017DEA4, 0x60);
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                Gp_ApplyAreaRecs(D_dryfield_garage_80180204);
                arg0->state += 1;
            }
            break;
        case 2:
            ready = 0;
            temp  = (Actor120300Work*)arg0->work;
            if ((s16)Gp_TakePendingObj4C(&evtId, &evtKind, &evtSub) != 0) {
                if (!((s16)evtId & WORLD_COLLISION_TRIGGER_AUTOMATIC)) {
                    if ((evtId & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
                        ready = gPlayerStatus.interactionPressed != 0;
                    }
                }
            }
            if (ready != 0) {
                if ((s8)evtKind == 1) {
                    temp->field_4DA = 0;
                }
                if ((s8)evtKind == 2) {
                    temp->field_4DA = 1;
                }
                temp->field_4D8 = 0;
                take            = 1;
            } else {
                take = 0;
            }
            if (take != 0) {
                arg0->state += 1;
            }
            break;
        case 3:
            if ((s16)func_actor_120300_801334A4(arg0) != 0) {
                arg0->state -= 1;
            }
            break;
        case 4:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_4BC, 0x7D4, &D_actor_120300_80140A54[11], 0);
            arg0->state = 2;
            break;
    }

    func_actor_120300_80132338(arg0);
    func_actor_120300_80132C60(arg0);
    scratch.draw.rot.vx = 0;
    scratch.draw.rot.vy = 0x380;
    scratch.draw.rot.vz = 0;
    Gp_DrawFloorQuad(&arg0->extra.tmd->coords[1], 0x300, &scratch.draw.rot);
    tmd      = arg0->extra.tmd;
    scaleRaw = work->field_4E0;
    func_actor_120300_FillLight(arg0, tmd, &scratch.draw.vec);
    scale               = scaleRaw & 0xFFFF;
    scratch.draw.vec.vz = scale;
    scratch.draw.vec.vy = scale;
    scratch.draw.vec.vx = scale;
    ScaleMatrix(tmd->colorMtx, &scratch.draw.vec);
}

#include "../../shared/screen_fade_out.inc.c"

/// Message 0x7D5 handler: a nonzero `arg2` shows the task's model (clears
/// `TmdObject` flag 0x80), zero hides it. `arg1` is the message id.
void func_actor_120300_80133C38(Task* task, s32 arg1, s32 arg2)
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

/// Broadcasts message 0x7D5 -- the visibility control the actor's display task
/// handles -- to the actor itself and to the two task slots on its work block.
/// Sending it is the whole body: `arg0` is the message's payload and only 0/1
/// are accepted.
void func_actor_120300_80133D04(s32 arg0)
{
    Actor120300Work* work = D_actor_120300_80141BA8->work;

    if (arg0 == 0) {
        taskMessageDispatch(D_actor_120300_80141BA8, 0x7D5, 0, 0);
        taskMessageDispatch(work->field_4B8, 0x7D5, 0, 0);
        taskMessageDispatch(work->field_4BC, 0x7D5, 0, 0);
    } else if (arg0 == 1) {
        taskMessageDispatch(D_actor_120300_80141BA8, 0x7D5, 1, 0);
        taskMessageDispatch(work->field_4B8, 0x7D5, 1, 0);
        taskMessageDispatch(work->field_4BC, 0x7D5, 1, 0);
    }
}

void func_actor_120300_80133DA4(void)
{
    CdCmd_EnqueueReplaceOverlay82();
    gGameSession->viewDirty = 1;
}

void func_actor_120300_80133DD4(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_actor_120300_80133DF4(void)
{
    Gp_RestoreStreamRng();
}

void func_actor_120300_80133E14(s16 arg0)
{
    Actor120300Work* work = D_actor_120300_80141BA8->work;

    work->field_4C0 = arg0;
    work->field_4C2 = 0;
}

void func_actor_120300_80133E34(s16 arg0)
{
    Actor120300Work* work = D_actor_120300_80141BA8->work;

    work->field_4C8 = arg0;
    work->field_4CA = 0;
}

/// Requests the player-weapon effect be killed: latches `field_4DE` so the
/// call happens once, and `func_actor_120300_80133E94` consumes the latch.
void func_actor_120300_80133E54(void)
{
    Actor120300Work* work = D_actor_120300_80141BA8->work;

    if (work->field_4DE == 0) {
        work->field_4DE = 1;
        Gp_KillPlayerEffs();
    }
}

/// Runs the pending player-weapon effect and reports it: the latch at
/// `field_4DE` keeps it one-shot, and the `Gp_MsgPlayerWeapon` argument beside
/// the clear is the same zero.
void func_actor_120300_80133E94(void)
{
    Actor120300Work* work = D_actor_120300_80141BA8->work;

    if (work->field_4DE != 0) {
        Gp_SpawnWeaponEff();
        work->field_4DE = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

/// Spawns the fade task (entry 4 of the actor's task table) at rate 9.
void func_actor_120300_80133EE4(void)
{
    Task_SpawnFromTable(D_actor_120300_80141B6C, 4, 9, 0);
}

void func_actor_120300_80133F14(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            func_800E8614(D_actor_120300_8014195C, 0);
            arg0->state += 1;
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                taskKill(arg0);
            }
            break;
    }
}
