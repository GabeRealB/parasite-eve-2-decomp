#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
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

#include "rooms/shelter_r47.h"
#include "../../shared/model_placement.h"
#include "../../shared/actor_messages.h"

/// Work block `func_actor_443500_80132078` `memCalloc`s (0x4C4) and parks in
/// the task's `Task::work` slot, which holds no `TaskIdMap` here. The spawn
/// handler seeds the two `sb` bytes at 0x475/0x476 and the word at 0x4BC to
/// -1 and copies the parent TmdObject's flags halfword to 0x4C0; the
/// light/colour matrix pair at 0x478/0x498 is the one
/// `func_actor_443500_801327C4` republishes onto the model.
///
/// The size is the allocation; the fields below are the ones this overlay's
/// decompiled bodies touch.
typedef struct Actor443500Work {
    ActorAnimRig20   rig;
    ActorModelState  model;
    /* 0x4B8 */ byte pad_4B8[0x2];
    /// Cleared by `func_actor_443500_801327E0` after the slot passes, beside
    /// the `model.ticking` latch it raises.
    /* 0x4BA */ s16 field_4BA;
    /* 0x4BC */ s32 field_4BC;
    /* 0x4C0 */ s32 field_4C0;
} Actor443500Work;
STATIC_ASSERT_SIZEOF(Actor443500Work, 0x4C4);

static void func_actor_443500_80132078(Task* task);
static void func_actor_443500_801321F0(Task* task);
static void func_actor_443500_801326A0(Task* task);
static void func_actor_443500_801327A4(Task* arg0);
static void func_actor_443500_801327C4(Task* task);
s32         func_actor_443500_801327E0(Task* task, s32 anim, AnimationPlayRequest* params, s32 arg3);
s32         func_actor_443500_8013297C(Task* task, s32 anim, s32 mode, s32 arg3);
static void func_actor_443500_80132A68(s32 arg0);

/// State table of the actor's child task (`TaskDesc` entry 1): setup, the
/// per-frame flag mirror and `taskKill`.
static const TaskFuncTable3 D_actor_443500_80131E24 = {
    { modelPlacementAttachChild, func_actor_443500_801326A0, taskKill }
};

/// State table of the actor's main task (`TaskDesc` entry 0): the spawn
/// handler, the per-frame tick and the exit callback.
static const TaskFuncTable3 D_actor_443500_80131E30 = {
    { func_actor_443500_80132078, func_actor_443500_801321F0, func_actor_443500_801327A4 }
};

extern TaskDesc D_actor_443500_80140E38;

/// Default animation arguments, 0x14 bytes: `{ NULL, 0x1C, 1, 4, 0 }`.
extern AnimationPlayRequest D_actor_443500_80158728;

/// The actor's two-entry `TaskDesc` table; the spawn handler starts entry 1.
extern TaskDesc D_actor_443500_8015873C[];

/// The actor's animation table: `(anim id, handler)` pairs for 0x7D3 / 0x7D4 /
/// 0x7D5, ended by `0x7FFFFFFF`. The spawn handler parks its address in
/// `Task::msgTable` (0x24).
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call1)(Task*, s32, ActorTransform*);
        s32 (*call2)(Task*, s32, s32, s32);
    } handler;
} Actor443500MessageEntry;
STATIC_ASSERT_SIZEOF(Actor443500MessageEntry, 8);

extern Actor443500MessageEntry D_actor_443500_80158754[4];

/// The bank table `func_actor_443500_801327E0` re-seeds the work block's slot
/// array off: one entry, the animation bank the default preset's `field_0` of
/// zero selects.
extern AnimationSet*  D_actor_443500_80158694[36];
extern AnimationSet** D_actor_443500_80158724[1];

extern GpGridParams D_actor_443500_801587D8;

extern TmdSource D_actor_443500_8014977C;
void             func_actor_443500_80132738(Task*);

extern GpGridFace D_actor_443500_801587B4[2];
extern SVECTOR    D_actor_443500_80158774[2];
extern SVECTOR    D_actor_443500_80158784[6];

extern AnimationPlayRequest D_actor_443500_80140E8C;
extern AnimationPlayRequest D_actor_443500_80140EA0;
extern AnimationPlayRequest D_actor_443500_80140EB4;
extern AnimationPlayRequest D_actor_443500_80140EC8;
extern AnimationPlayRequest D_actor_443500_80140EDC;
extern AnimationPlayRequest D_actor_443500_80140EF0;
extern AnimationPlayRequest D_actor_443500_80140F04;
extern AnimationPlayRequest D_actor_443500_80140F18;
extern AnimationPlayRequest D_actor_443500_80140F2C;
extern AnimationPlayRequest D_actor_443500_80140F40;
extern AnimationPlayRequest D_actor_443500_80141004;
extern AnimationPlayRequest D_actor_443500_80141018;
extern AnimationPlayRequest D_actor_443500_8014102C;
extern AnimationPlayRequest D_actor_443500_80141040;
extern AnimationPlayRequest D_actor_443500_80141054;
extern AnimationPlayRequest D_actor_443500_80141068;
extern AnimationPlayRequest D_actor_443500_80141090;
extern AnimationPlayRequest D_actor_443500_801410A4;
extern AnimationPlayRequest D_actor_443500_801410B8;
extern AnimationPlayRequest D_actor_443500_801410CC;
extern AnimationPlayRequest D_actor_443500_801410E0;
extern AnimationPlayRequest D_actor_443500_801410F4;
extern AnimationPlayRequest D_actor_443500_80141108;
extern AnimationPlayRequest D_actor_443500_8014111C;
extern AnimationPlayRequest D_actor_443500_80141130;
extern AnimationPlayRequest D_actor_443500_80141144;
extern AnimationPlayRequest D_actor_443500_80141158;
extern AnimationPlayRequest D_actor_443500_8014116C;
extern AnimationPlayRequest D_actor_443500_80141180;
extern AnimationPlayRequest D_actor_443500_801411A8;
extern AnimationPlayRequest D_actor_443500_801411BC;
extern AnimationPlayRequest D_actor_443500_801411D0;
extern GpCopyArg            D_actor_443500_80140E70;
extern GpCopyArg            D_actor_443500_80140FE8;
extern ActorTransform       D_actor_443500_80140F54;
extern ActorTransform       D_actor_443500_80140F6C;
extern ActorTransform       D_actor_443500_801411E4;
extern ActorTransform       D_actor_443500_801411FC;
void                        func_actor_443500_80131E3C(s32);
void                        func_actor_443500_80131E84(s32);
void                        func_actor_443500_80131EE4(void);
void                        func_actor_443500_80131F18(void);
void                        func_actor_443500_80131F58(void);
void                        func_actor_443500_8013201C(s16);
void                        func_actor_443500_80132048(void);
void                        func_actor_443500_8013206C(s8);

extern AnimationSet D_actor_443500_80132EF0;
extern AnimationSet D_actor_443500_8013451C;
extern AnimationSet D_actor_443500_8013503C;
extern AnimationSet D_actor_443500_80135B54;
extern AnimationSet D_actor_443500_80135D74;
extern AnimationSet D_actor_443500_80136818;
extern AnimationSet D_actor_443500_80136A68;
extern AnimationSet D_actor_443500_80136E94;
extern AnimationSet D_actor_443500_80137390;
extern AnimationSet D_actor_443500_80137E54;
extern AnimationSet D_actor_443500_801380DC;
extern AnimationSet D_actor_443500_801383F8;
extern AnimationSet D_actor_443500_8013953C;
extern AnimationSet D_actor_443500_8013A208;
extern AnimationSet D_actor_443500_8013A4DC;
extern AnimationSet D_actor_443500_8013AC28;
extern AnimationSet D_actor_443500_8013B170;
extern AnimationSet D_actor_443500_8013B820;
extern AnimationSet D_actor_443500_8013F980;
extern AnimationSet D_actor_443500_8013FD54;
extern AnimationSet D_actor_443500_80140144;
extern AnimationSet D_actor_443500_801404B0;
extern AnimationSet D_actor_443500_80140A30;
extern AnimationSet D_actor_443500_80140E10;

void func_actor_443500_80131F88(Task*);

AnimationPackedPose D_actor_443500_80132C14[6] = {
#include "assets/actor_443500_animation_010D0_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80132C5C[46] = {
#include "assets/actor_443500_animation_010D0_bank4.inc"
};

AnimationRecord D_actor_443500_80132D14[109] = {
#include "assets/actor_443500_animation_010D0_records.inc"
};

u16 D_actor_443500_80132EC8[20] = {
#include "assets/actor_443500_animation_010D0_indices.inc"
};

AnimationSet D_actor_443500_80132EF0 = {
    D_actor_443500_80132D14,
    D_actor_443500_80132EC8,
    { NULL, D_actor_443500_80132C14, NULL, NULL, D_actor_443500_80132C5C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80132F18[53] = {
#include "assets/actor_443500_animation_026FC_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80133194[561] = {
#include "assets/actor_443500_animation_026FC_bank4.inc"
};

AnimationRecord D_actor_443500_80133A58[679] = {
#include "assets/actor_443500_animation_026FC_records.inc"
};

u16 D_actor_443500_801344F4[20] = {
#include "assets/actor_443500_animation_026FC_indices.inc"
};

AnimationSet D_actor_443500_8013451C = {
    D_actor_443500_80133A58,
    D_actor_443500_801344F4,
    { NULL, D_actor_443500_80132F18, NULL, NULL, D_actor_443500_80133194, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80134544[10] = {
#include "assets/actor_443500_animation_0321C_bank1.inc"
};

AnimationPackedRotation D_actor_443500_801345BC[295] = {
#include "assets/actor_443500_animation_0321C_bank4.inc"
};

AnimationRecord D_actor_443500_80134A58[367] = {
#include "assets/actor_443500_animation_0321C_records.inc"
};

u16 D_actor_443500_80135014[20] = {
#include "assets/actor_443500_animation_0321C_indices.inc"
};

AnimationSet D_actor_443500_8013503C = {
    D_actor_443500_80134A58,
    D_actor_443500_80135014,
    { NULL, D_actor_443500_80134544, NULL, NULL, D_actor_443500_801345BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80135064[5] = {
#include "assets/actor_443500_animation_03D34_bank1.inc"
};

AnimationPackedRotation D_actor_443500_801350A0[300] = {
#include "assets/actor_443500_animation_03D34_bank4.inc"
};

AnimationRecord D_actor_443500_80135550[375] = {
#include "assets/actor_443500_animation_03D34_records.inc"
};

u16 D_actor_443500_80135B2C[20] = {
#include "assets/actor_443500_animation_03D34_indices.inc"
};

AnimationSet D_actor_443500_80135B54 = {
    D_actor_443500_80135550,
    D_actor_443500_80135B2C,
    { NULL, D_actor_443500_80135064, NULL, NULL, D_actor_443500_801350A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80135B7C[4] = {
#include "assets/actor_443500_animation_03F54_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80135BAC[20] = {
#include "assets/actor_443500_animation_03F54_bank4.inc"
};

AnimationRecord D_actor_443500_80135BFC[84] = {
#include "assets/actor_443500_animation_03F54_records.inc"
};

u16 D_actor_443500_80135D4C[20] = {
#include "assets/actor_443500_animation_03F54_indices.inc"
};

AnimationSet D_actor_443500_80135D74 = {
    D_actor_443500_80135BFC,
    D_actor_443500_80135D4C,
    { NULL, D_actor_443500_80135B7C, NULL, NULL, D_actor_443500_80135BAC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80135D9C[4] = {
#include "assets/actor_443500_animation_049F8_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80135DCC[273] = {
#include "assets/actor_443500_animation_049F8_bank4.inc"
};

AnimationRecord D_actor_443500_80136210[376] = {
#include "assets/actor_443500_animation_049F8_records.inc"
};

u16 D_actor_443500_801367F0[20] = {
#include "assets/actor_443500_animation_049F8_indices.inc"
};

AnimationSet D_actor_443500_80136818 = {
    D_actor_443500_80136210,
    D_actor_443500_801367F0,
    { NULL, D_actor_443500_80135D9C, NULL, NULL, D_actor_443500_80135DCC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80136840[2] = {
#include "assets/actor_443500_animation_04C48_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80136858[27] = {
#include "assets/actor_443500_animation_04C48_bank4.inc"
};

AnimationRecord D_actor_443500_801368C4[95] = {
#include "assets/actor_443500_animation_04C48_records.inc"
};

u16 D_actor_443500_80136A40[20] = {
#include "assets/actor_443500_animation_04C48_indices.inc"
};

AnimationSet D_actor_443500_80136A68 = {
    D_actor_443500_801368C4,
    D_actor_443500_80136A40,
    { NULL, D_actor_443500_80136840, NULL, NULL, D_actor_443500_80136858, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80136A90[2] = {
#include "assets/actor_443500_animation_05074_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80136AA8[99] = {
#include "assets/actor_443500_animation_05074_bank4.inc"
};

AnimationRecord D_actor_443500_80136C34[142] = {
#include "assets/actor_443500_animation_05074_records.inc"
};

u16 D_actor_443500_80136E6C[20] = {
#include "assets/actor_443500_animation_05074_indices.inc"
};

AnimationSet D_actor_443500_80136E94 = {
    D_actor_443500_80136C34,
    D_actor_443500_80136E6C,
    { NULL, D_actor_443500_80136A90, NULL, NULL, D_actor_443500_80136AA8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80136EBC[4] = {
#include "assets/actor_443500_animation_05570_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80136EEC[128] = {
#include "assets/actor_443500_animation_05570_bank4.inc"
};

AnimationRecord D_actor_443500_801370EC[159] = {
#include "assets/actor_443500_animation_05570_records.inc"
};

u16 D_actor_443500_80137368[20] = {
#include "assets/actor_443500_animation_05570_indices.inc"
};

AnimationSet D_actor_443500_80137390 = {
    D_actor_443500_801370EC,
    D_actor_443500_80137368,
    { NULL, D_actor_443500_80136EBC, NULL, NULL, D_actor_443500_80136EEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_801373B8[23] = {
#include "assets/actor_443500_animation_06034_bank1.inc"
};

AnimationPackedRotation D_actor_443500_801374CC[267] = {
#include "assets/actor_443500_animation_06034_bank4.inc"
};

AnimationRecord D_actor_443500_801378F8[333] = {
#include "assets/actor_443500_animation_06034_records.inc"
};

u16 D_actor_443500_80137E2C[20] = {
#include "assets/actor_443500_animation_06034_indices.inc"
};

AnimationSet D_actor_443500_80137E54 = {
    D_actor_443500_801378F8,
    D_actor_443500_80137E2C,
    { NULL, D_actor_443500_801373B8, NULL, NULL, D_actor_443500_801374CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80137E7C[2] = {
#include "assets/actor_443500_animation_062BC_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80137E94[33] = {
#include "assets/actor_443500_animation_062BC_bank4.inc"
};

AnimationRecord D_actor_443500_80137F18[103] = {
#include "assets/actor_443500_animation_062BC_records.inc"
};

u16 D_actor_443500_801380B4[20] = {
#include "assets/actor_443500_animation_062BC_indices.inc"
};

AnimationSet D_actor_443500_801380DC = {
    D_actor_443500_80137F18,
    D_actor_443500_801380B4,
    { NULL, D_actor_443500_80137E7C, NULL, NULL, D_actor_443500_80137E94, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80138104[2] = {
#include "assets/actor_443500_animation_065D8_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013811C[60] = {
#include "assets/actor_443500_animation_065D8_bank4.inc"
};

AnimationRecord D_actor_443500_8013820C[113] = {
#include "assets/actor_443500_animation_065D8_records.inc"
};

u16 D_actor_443500_801383D0[20] = {
#include "assets/actor_443500_animation_065D8_indices.inc"
};

AnimationSet D_actor_443500_801383F8 = {
    D_actor_443500_8013820C,
    D_actor_443500_801383D0,
    { NULL, D_actor_443500_80138104, NULL, NULL, D_actor_443500_8013811C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80138420[20] = {
#include "assets/actor_443500_animation_0771C_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80138510[447] = {
#include "assets/actor_443500_animation_0771C_bank4.inc"
};

AnimationRecord D_actor_443500_80138C0C[578] = {
#include "assets/actor_443500_animation_0771C_records.inc"
};

u16 D_actor_443500_80139514[20] = {
#include "assets/actor_443500_animation_0771C_indices.inc"
};

AnimationSet D_actor_443500_8013953C = {
    D_actor_443500_80138C0C,
    D_actor_443500_80139514,
    { NULL, D_actor_443500_80138420, NULL, NULL, D_actor_443500_80138510, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80139564[10] = {
#include "assets/actor_443500_animation_083E8_bank1.inc"
};

AnimationPackedRotation D_actor_443500_801395DC[331] = {
#include "assets/actor_443500_animation_083E8_bank4.inc"
};

AnimationRecord D_actor_443500_80139B08[438] = {
#include "assets/actor_443500_animation_083E8_records.inc"
};

u16 D_actor_443500_8013A1E0[20] = {
#include "assets/actor_443500_animation_083E8_indices.inc"
};

AnimationSet D_actor_443500_8013A208 = {
    D_actor_443500_80139B08,
    D_actor_443500_8013A1E0,
    { NULL, D_actor_443500_80139564, NULL, NULL, D_actor_443500_801395DC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013A230[2] = {
#include "assets/actor_443500_animation_086BC_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013A248[40] = {
#include "assets/actor_443500_animation_086BC_bank4.inc"
};

AnimationRecord D_actor_443500_8013A2E8[115] = {
#include "assets/actor_443500_animation_086BC_records.inc"
};

u16 D_actor_443500_8013A4B4[20] = {
#include "assets/actor_443500_animation_086BC_indices.inc"
};

AnimationSet D_actor_443500_8013A4DC = {
    D_actor_443500_8013A2E8,
    D_actor_443500_8013A4B4,
    { NULL, D_actor_443500_8013A230, NULL, NULL, D_actor_443500_8013A248, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013A504[7] = {
#include "assets/actor_443500_animation_08E08_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013A558[171] = {
#include "assets/actor_443500_animation_08E08_bank4.inc"
};

AnimationRecord D_actor_443500_8013A804[255] = {
#include "assets/actor_443500_animation_08E08_records.inc"
};

u16 D_actor_443500_8013AC00[20] = {
#include "assets/actor_443500_animation_08E08_indices.inc"
};

AnimationSet D_actor_443500_8013AC28 = {
    D_actor_443500_8013A804,
    D_actor_443500_8013AC00,
    { NULL, D_actor_443500_8013A504, NULL, NULL, D_actor_443500_8013A558, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013AC50[2] = {
#include "assets/actor_443500_animation_09350_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013AC68[120] = {
#include "assets/actor_443500_animation_09350_bank4.inc"
};

AnimationRecord D_actor_443500_8013AE48[192] = {
#include "assets/actor_443500_animation_09350_records.inc"
};

u16 D_actor_443500_8013B148[20] = {
#include "assets/actor_443500_animation_09350_indices.inc"
};

AnimationSet D_actor_443500_8013B170 = {
    D_actor_443500_8013AE48,
    D_actor_443500_8013B148,
    { NULL, D_actor_443500_8013AC50, NULL, NULL, D_actor_443500_8013AC68, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013B198[29] = {
#include "assets/actor_443500_animation_09A00_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013B2F4[127] = {
#include "assets/actor_443500_animation_09A00_bank4.inc"
};

AnimationRecord D_actor_443500_8013B4F0[194] = {
#include "assets/actor_443500_animation_09A00_records.inc"
};

u16 D_actor_443500_8013B7F8[20] = {
#include "assets/actor_443500_animation_09A00_indices.inc"
};

AnimationSet D_actor_443500_8013B820 = {
    D_actor_443500_8013B4F0,
    D_actor_443500_8013B7F8,
    { NULL, D_actor_443500_8013B198, NULL, NULL, D_actor_443500_8013B2F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013B848[25] = {
#include "assets/actor_443500_animation_0A4E4_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013B974[250] = {
#include "assets/actor_443500_animation_0A4E4_bank4.inc"
};

AnimationRecord D_actor_443500_8013BD5C[352] = {
#include "assets/actor_443500_animation_0A4E4_records.inc"
};

u16 D_actor_443500_8013C2DC[20] = {
#include "assets/actor_443500_animation_0A4E4_indices.inc"
};

AnimationSet D_actor_443500_8013C304 = {
    D_actor_443500_8013BD5C,
    D_actor_443500_8013C2DC,
    { NULL, D_actor_443500_8013B848, NULL, NULL, D_actor_443500_8013B974, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013C32C[14] = {
#include "assets/actor_443500_animation_0ACA0_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013C3D4[196] = {
#include "assets/actor_443500_animation_0ACA0_bank4.inc"
};

AnimationRecord D_actor_443500_8013C6E4[237] = {
#include "assets/actor_443500_animation_0ACA0_records.inc"
};

u16 D_actor_443500_8013CA98[20] = {
#include "assets/actor_443500_animation_0ACA0_indices.inc"
};

AnimationSet D_actor_443500_8013CAC0 = {
    D_actor_443500_8013C6E4,
    D_actor_443500_8013CA98,
    { NULL, D_actor_443500_8013C32C, NULL, NULL, D_actor_443500_8013C3D4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013CAE8[6] = {
#include "assets/actor_443500_animation_0AFB8_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013CB30[52] = {
#include "assets/actor_443500_animation_0AFB8_bank4.inc"
};

AnimationRecord D_actor_443500_8013CC00[108] = {
#include "assets/actor_443500_animation_0AFB8_records.inc"
};

u16 D_actor_443500_8013CDB0[20] = {
#include "assets/actor_443500_animation_0AFB8_indices.inc"
};

AnimationSet D_actor_443500_8013CDD8 = {
    D_actor_443500_8013CC00,
    D_actor_443500_8013CDB0,
    { NULL, D_actor_443500_8013CAE8, NULL, NULL, D_actor_443500_8013CB30, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013CE00[24] = {
#include "assets/actor_443500_animation_0BC74_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013CF20[309] = {
#include "assets/actor_443500_animation_0BC74_bank4.inc"
};

AnimationRecord D_actor_443500_8013D3F4[414] = {
#include "assets/actor_443500_animation_0BC74_records.inc"
};

u16 D_actor_443500_8013DA6C[20] = {
#include "assets/actor_443500_animation_0BC74_indices.inc"
};

AnimationSet D_actor_443500_8013DA94 = {
    D_actor_443500_8013D3F4,
    D_actor_443500_8013DA6C,
    { NULL, D_actor_443500_8013CE00, NULL, NULL, D_actor_443500_8013CF20, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013DABC[4] = {
#include "assets/actor_443500_animation_0BEC0_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013DAEC[24] = {
#include "assets/actor_443500_animation_0BEC0_bank4.inc"
};

AnimationRecord D_actor_443500_8013DB4C[91] = {
#include "assets/actor_443500_animation_0BEC0_records.inc"
};

u16 D_actor_443500_8013DCB8[20] = {
#include "assets/actor_443500_animation_0BEC0_indices.inc"
};

AnimationSet D_actor_443500_8013DCE0 = {
    D_actor_443500_8013DB4C,
    D_actor_443500_8013DCB8,
    { NULL, D_actor_443500_8013DABC, NULL, NULL, D_actor_443500_8013DAEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013DD08[8] = {
#include "assets/actor_443500_animation_0C5DC_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013DD68[180] = {
#include "assets/actor_443500_animation_0C5DC_bank4.inc"
};

AnimationRecord D_actor_443500_8013E038[231] = {
#include "assets/actor_443500_animation_0C5DC_records.inc"
};

u16 D_actor_443500_8013E3D4[20] = {
#include "assets/actor_443500_animation_0C5DC_indices.inc"
};

AnimationSet D_actor_443500_8013E3FC = {
    D_actor_443500_8013E038,
    D_actor_443500_8013E3D4,
    { NULL, D_actor_443500_8013DD08, NULL, NULL, D_actor_443500_8013DD68, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013E424[10] = {
#include "assets/actor_443500_animation_0CCD8_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013E49C[147] = {
#include "assets/actor_443500_animation_0CCD8_bank4.inc"
};

AnimationRecord D_actor_443500_8013E6E8[250] = {
#include "assets/actor_443500_animation_0CCD8_records.inc"
};

u16 D_actor_443500_8013EAD0[20] = {
#include "assets/actor_443500_animation_0CCD8_indices.inc"
};

AnimationSet D_actor_443500_8013EAF8 = {
    D_actor_443500_8013E6E8,
    D_actor_443500_8013EAD0,
    { NULL, D_actor_443500_8013E424, NULL, NULL, D_actor_443500_8013E49C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013EB20[5] = {
#include "assets/actor_443500_animation_0D12C_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013EB5C[80] = {
#include "assets/actor_443500_animation_0D12C_bank4.inc"
};

AnimationRecord D_actor_443500_8013EC9C[162] = {
#include "assets/actor_443500_animation_0D12C_records.inc"
};

u16 D_actor_443500_8013EF24[20] = {
#include "assets/actor_443500_animation_0D12C_indices.inc"
};

AnimationSet D_actor_443500_8013EF4C = {
    D_actor_443500_8013EC9C,
    D_actor_443500_8013EF24,
    { NULL, D_actor_443500_8013EB20, NULL, NULL, D_actor_443500_8013EB5C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013EF74[4] = {
#include "assets/actor_443500_animation_0D3C0_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013EFA4[51] = {
#include "assets/actor_443500_animation_0D3C0_bank4.inc"
};

AnimationRecord D_actor_443500_8013F070[82] = {
#include "assets/actor_443500_animation_0D3C0_records.inc"
};

u16 D_actor_443500_8013F1B8[20] = {
#include "assets/actor_443500_animation_0D3C0_indices.inc"
};

AnimationSet D_actor_443500_8013F1E0 = {
    D_actor_443500_8013F070,
    D_actor_443500_8013F1B8,
    { NULL, D_actor_443500_8013EF74, NULL, NULL, D_actor_443500_8013EFA4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013F208[13] = {
#include "assets/actor_443500_animation_0DB60_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013F2A4[179] = {
#include "assets/actor_443500_animation_0DB60_bank4.inc"
};

AnimationRecord D_actor_443500_8013F570[250] = {
#include "assets/actor_443500_animation_0DB60_records.inc"
};

u16 D_actor_443500_8013F958[20] = {
#include "assets/actor_443500_animation_0DB60_indices.inc"
};

AnimationSet D_actor_443500_8013F980 = {
    D_actor_443500_8013F570,
    D_actor_443500_8013F958,
    { NULL, D_actor_443500_8013F208, NULL, NULL, D_actor_443500_8013F2A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013F9A8[8] = {
#include "assets/actor_443500_animation_0DF34_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013FA08[84] = {
#include "assets/actor_443500_animation_0DF34_bank4.inc"
};

AnimationRecord D_actor_443500_8013FB58[117] = {
#include "assets/actor_443500_animation_0DF34_records.inc"
};

u16 D_actor_443500_8013FD2C[20] = {
#include "assets/actor_443500_animation_0DF34_indices.inc"
};

AnimationSet D_actor_443500_8013FD54 = {
    D_actor_443500_8013FB58,
    D_actor_443500_8013FD2C,
    { NULL, D_actor_443500_8013F9A8, NULL, NULL, D_actor_443500_8013FA08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8013FD7C[7] = {
#include "assets/actor_443500_animation_0E324_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8013FDD0[62] = {
#include "assets/actor_443500_animation_0E324_bank4.inc"
};

AnimationRecord D_actor_443500_8013FEC8[149] = {
#include "assets/actor_443500_animation_0E324_records.inc"
};

u16 D_actor_443500_8014011C[20] = {
#include "assets/actor_443500_animation_0E324_indices.inc"
};

AnimationSet D_actor_443500_80140144 = {
    D_actor_443500_8013FEC8,
    D_actor_443500_8014011C,
    { NULL, D_actor_443500_8013FD7C, NULL, NULL, D_actor_443500_8013FDD0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014016C[7] = {
#include "assets/actor_443500_animation_0E690_bank1.inc"
};

AnimationPackedRotation D_actor_443500_801401C0[74] = {
#include "assets/actor_443500_animation_0E690_bank4.inc"
};

AnimationRecord D_actor_443500_801402E8[104] = {
#include "assets/actor_443500_animation_0E690_records.inc"
};

u16 D_actor_443500_80140488[20] = {
#include "assets/actor_443500_animation_0E690_indices.inc"
};

AnimationSet D_actor_443500_801404B0 = {
    D_actor_443500_801402E8,
    D_actor_443500_80140488,
    { NULL, D_actor_443500_8014016C, NULL, NULL, D_actor_443500_801401C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_801404D8[10] = {
#include "assets/actor_443500_animation_0EC10_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80140550[104] = {
#include "assets/actor_443500_animation_0EC10_bank4.inc"
};

AnimationRecord D_actor_443500_801406F0[198] = {
#include "assets/actor_443500_animation_0EC10_records.inc"
};

u16 D_actor_443500_80140A08[20] = {
#include "assets/actor_443500_animation_0EC10_indices.inc"
};

AnimationSet D_actor_443500_80140A30 = {
    D_actor_443500_801406F0,
    D_actor_443500_80140A08,
    { NULL, D_actor_443500_801404D8, NULL, NULL, D_actor_443500_80140550, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80140A58[8] = {
#include "assets/actor_443500_animation_0EFF0_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80140AB8[85] = {
#include "assets/actor_443500_animation_0EFF0_bank4.inc"
};

AnimationRecord D_actor_443500_80140C0C[119] = {
#include "assets/actor_443500_animation_0EFF0_records.inc"
};

u16 D_actor_443500_80140DE8[20] = {
#include "assets/actor_443500_animation_0EFF0_indices.inc"
};

AnimationSet D_actor_443500_80140E10 = {
    D_actor_443500_80140C0C,
    D_actor_443500_80140DE8,
    { NULL, D_actor_443500_80140A58, NULL, NULL, D_actor_443500_80140AB8, NULL, NULL, NULL },
};

TaskDesc D_actor_443500_80140E38 = { 0, 192, func_actor_443500_80131F88, { .model = NULL } };

AnimationSet* D_actor_443500_80140E44[11] = {
    NULL,
    &D_actor_443500_80132EF0,
    &D_actor_443500_8013C304,
    &D_actor_443500_8013CAC0,
    &D_actor_443500_8013CDD8,
    &D_actor_443500_8013DA94,
    &D_actor_443500_8013DCE0,
    &D_actor_443500_8013E3FC,
    &D_actor_443500_8013EAF8,
    &D_actor_443500_8013EF4C,
    &D_actor_443500_8013F1E0,
};

GpCopyArg D_actor_443500_80140E70 = { { .sets = D_actor_443500_80140E44 }, 11 };

AnimationPlayRequest D_actor_443500_80140E78 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140E8C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140EA0 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140EB4 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140EC8 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140EDC = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140EF0 = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140F04 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140F18 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140F2C = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140F40 = { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_443500_80140F54 = { { 0x3A98, -1000, 7600, 0 }, { 0, 512, 0, 0 } };

ActorTransform D_actor_443500_80140F6C = { { 0x3CF0, -1000, 9000, 0 }, { 0, 512, 0, 0 } };

AnimationSet* D_actor_443500_80140F84[25] = {
    NULL,
    &D_actor_443500_80132EF0,
    &D_actor_443500_8013451C,
    &D_actor_443500_8013503C,
    &D_actor_443500_80135B54,
    &D_actor_443500_80135D74,
    &D_actor_443500_80136818,
    &D_actor_443500_80136A68,
    &D_actor_443500_80136E94,
    &D_actor_443500_80137390,
    &D_actor_443500_80137E54,
    &D_actor_443500_801380DC,
    &D_actor_443500_801383F8,
    &D_actor_443500_8013953C,
    &D_actor_443500_8013A208,
    &D_actor_443500_8013A4DC,
    &D_actor_443500_8013AC28,
    &D_actor_443500_8013B170,
    &D_actor_443500_8013B820,
    &D_actor_443500_8013FD54,
    &D_actor_443500_80140144,
    &D_actor_443500_801404B0,
    &D_actor_443500_80140A30,
    &D_actor_443500_80140E10,
    &D_actor_443500_8013F980,
};

GpCopyArg D_actor_443500_80140FE8 = { { .sets = D_actor_443500_80140F84 }, 25 };

AnimationPlayRequest D_actor_443500_80140FF0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141004 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141018 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014102C = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141040 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141054 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141068 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014107C = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141090 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801410A4 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801410B8 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801410CC = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801410E0 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801410F4 = { { .index = 1 }, 60, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141108 = { { .index = 1 }, 61, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014111C = { { .index = 1 }, 62, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141130 = { { .index = 1 }, 63, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141144 = { { .index = 1 }, 64, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141158 = { { .index = 1 }, 65, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014116C = { { .index = 1 }, 66, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141180 = { { .index = 1 }, 67, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141194 = { { .index = 1 }, 68, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801411A8 = { { .index = 1 }, 69, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801411BC = { { .index = 1 }, 70, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801411D0 = { { .index = 1 }, 71, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_443500_801411E4 = { { 0x3F48, -1000, 8000, 0 }, { 0, 2047, 0, 0 } };

ActorTransform D_actor_443500_801411FC = { { 0x3FAC, -1000, 6950, 0 }, { 0, 1365, 0, 0 } };

AnimationPlayRequest D_actor_443500_80141214 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141228 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014123C = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141250 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141264 = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141278 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014128C = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801412A0 = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801412B4 = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801412C8 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801412DC = { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801412F0 = { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141304 = { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141318 = { { .index = 0 }, 13, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014132C = { { .index = 0 }, 14, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141340 = { { .index = 0 }, 15, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141354 = { { .index = 0 }, 16, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141368 = { { .index = 0 }, 17, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014137C = { { .index = 0 }, 18, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141390 = { { .index = 0 }, 19, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801413A4 = { { .index = 0 }, 20, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801413B8 = { { .index = 0 }, 21, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801413CC = { { .index = 0 }, 22, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801413E0 = { { .index = 0 }, 23, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801413F4 = { { .index = 0 }, 24, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141408 = { { .index = 0 }, 25, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014141C = { { .index = 0 }, 26, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141430 = { { .index = 0 }, 27, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141444 = { { .index = 0 }, 28, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141458 = { { .index = 0 }, 29, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014146C = { { .index = 0 }, 30, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141480 = { { .index = 0 }, 31, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141494 = { { .index = 0 }, 32, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801414A8 = { { .index = 0 }, 33, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801414BC = { { .index = 0 }, 34, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801414D0 = { { .index = 0 }, 35, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_443500_801414E4 = { { 0x3E80, -1000, 9000, 0 }, { 0, 2560, 0, 0 } };

ActorTransform D_actor_443500_801414FC = { { 0x3F48, -1000, 6200, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_443500_80141514 = { { 0x3E80, -1000, 5910, 0 }, { 0, 0, 0, 0 } };

GpEvsCmd D_actor_443500_8014152C[74] = {
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_443500_80140E70 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 13, { .callback = func_actor_443500_80131E3C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 16, { .value = 0x542F0001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x542F0006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_443500_80140F54 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_443500_801414E4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140E8C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_8014123C }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140EA0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141250 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141264 }, { .value = 0 } },
    { 4, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801412F0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140F18 }, { .value = 0 } },
    { 4, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140EB4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140EC8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801412DC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140EDC }, { .value = 0 } },
    { 4, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141304 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140EF0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801412B4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801412C8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141278 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140F04 }, { .value = 0 } },
    { 4, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141318 }, { .value = 0 } },
    { 4, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140E8C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140F2C }, { .value = 0 } },
    { 4, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140F40 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140E8C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140E8C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_8014128C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801412A0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_443500_80140F6C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_443500_80141C1C[16] = {
    { 16, { .value = 0x542F0001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_443500_80140F6C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_443500_801414E4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_443500_80141D9C[137] = {
    { 13, { .callbackNoArg = func_actor_443500_80131F58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 16, { .value = 0x542F0001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_443500_80140FE8 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS8 = func_actor_443500_8013206C }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_443500_80131E3C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_443500_801411E4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_443500_801414FC }, { .value = 0 } },
    { 3, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141018 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_8014132C }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014102C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141340 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014116C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141228 }, { .value = 0 } },
    { 4, { .value = 46 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141180 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141318 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801414A8 }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801414BC }, { .value = 0 } },
    { 4, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801412C8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141040 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141228 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801414A8 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801414BC }, { .value = 0 } },
    { 4, { .value = 88 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141004 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141228 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801412C8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801412C8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801411A8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141228 }, { .value = 0 } },
    { 4, { .value = 189 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801411BC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141004 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141480 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141494 }, { .value = 0 } },
    { 4, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141318 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801414BC }, { .value = 0 } },
    { 4, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801414D0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801412C8 }, { .value = 0 } },
    { 4, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141228 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014102C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141228 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141004 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141430 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801411D0 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_443500_801411FC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_8014132C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_443500_80131F18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_443500_80140FE8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141054 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_443500_801411FC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_443500_80141514 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141354 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141368 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141390 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801413A4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141068 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_8014137C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141090 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801413B8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801410A4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801413CC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141054 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141408 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141390 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801413A4 }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_8014137C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801410B8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801413E0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141354 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_443500_80142A74[18] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 16, { .value = 0x542F0001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS8 = func_actor_443500_8013206C }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_443500_801411FC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_443500_80141514 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_443500_8013201C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_443500_80142C24[73] = {
    { 13, { .callbackNoArg = func_actor_443500_80131F58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_443500_80140FE8 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_443500_80131E3C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_443500_801411FC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_443500_80141514 }, { .value = 0 } },
    { 3, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801410CC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141354 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801410E0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141368 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801410F4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141004 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141390 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801413A4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141108 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_8014137C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014111C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141390 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801413A4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141130 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_8014137C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801413F4 }, { .value = 0 } },
    { 4, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014111C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_8014137C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141408 }, { .value = 0 } },
    { 4, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141390 }, { .value = 0 } },
    { 4, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801413A4 }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_801413E0 }, { .value = 0 } },
    { 4, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141354 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141144 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014111C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141368 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_8014141C }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141158 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141444 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_443500_80132048 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_443500_801432FC[17] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_443500_801411FC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_443500_80141514 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141444 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_443500_80132048 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_443500_80143494[10] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_443500_80140FE8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141004 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_80141458 }, { .value = 0 } },
    { 13, { .callback = func_actor_443500_80131E84 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_443500_80131EE4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_443500_8014146C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_443500_80143584[20] = {
#include "assets/actor_443500_model_1795C_skeleton.inc"
};

u32 D_actor_443500_80143854[20] = {
#include "assets/actor_443500_model_1795C_partVerts.inc"
};

SVECTOR D_actor_443500_801438A4[390] = {
#include "assets/actor_443500_model_1795C_verts.inc"
};

SVECTOR D_actor_443500_801444D4[407] = {
#include "assets/actor_443500_model_1795C_normals.inc"
};

u32 D_actor_443500_8014518C[4476] = {
#include "assets/actor_443500_model_1795C_stream.inc"
};

TmdSource D_actor_443500_8014977C = {
    0,
    24444,
    6776,
    20,
    D_actor_443500_80143854,
    D_actor_443500_801438A4,
    D_actor_443500_801444D4,
    D_actor_443500_80143584,
    D_actor_443500_8014518C,
};

TmdBone D_actor_443500_801497A0[1] = {
#include "assets/actor_443500_model_17B58_skeleton.inc"
};

u32 D_actor_443500_801497C4[1] = {
#include "assets/actor_443500_model_17B58_partVerts.inc"
};

SVECTOR D_actor_443500_801497C8[14] = {
#include "assets/actor_443500_model_17B58_verts.inc"
};

SVECTOR D_actor_443500_80149838[12] = {
#include "assets/actor_443500_model_17B58_normals.inc"
};

u32 D_actor_443500_80149898[56] = {
#include "assets/actor_443500_model_17B58_stream.inc"
};

TmdSource D_actor_443500_80149978 = {
    0,
    340,
    0,
    1,
    D_actor_443500_801497C4,
    D_actor_443500_801497C8,
    D_actor_443500_80149838,
    D_actor_443500_801497A0,
    D_actor_443500_80149898,
};

AnimationPackedPose D_actor_443500_8014999C[2] = {
#include "assets/actor_443500_animation_17DD0_bank1.inc"
};

AnimationPackedRotation D_actor_443500_801499B4[32] = {
#include "assets/actor_443500_animation_17DD0_bank4.inc"
};

AnimationRecord D_actor_443500_80149A34[101] = {
#include "assets/actor_443500_animation_17DD0_records.inc"
};

u16 D_actor_443500_80149BC8[20] = {
#include "assets/actor_443500_animation_17DD0_indices.inc"
};

AnimationSet D_actor_443500_80149BF0 = {
    D_actor_443500_80149A34,
    D_actor_443500_80149BC8,
    { NULL, D_actor_443500_8014999C, NULL, NULL, D_actor_443500_801499B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80149C18[4] = {
#include "assets/actor_443500_animation_18308_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80149C48[112] = {
#include "assets/actor_443500_animation_18308_bank4.inc"
};

AnimationRecord D_actor_443500_80149E08[190] = {
#include "assets/actor_443500_animation_18308_records.inc"
};

u16 D_actor_443500_8014A100[20] = {
#include "assets/actor_443500_animation_18308_indices.inc"
};

AnimationSet D_actor_443500_8014A128 = {
    D_actor_443500_80149E08,
    D_actor_443500_8014A100,
    { NULL, D_actor_443500_80149C18, NULL, NULL, D_actor_443500_80149C48, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014A150[10] = {
#include "assets/actor_443500_animation_18914_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014A1C8[134] = {
#include "assets/actor_443500_animation_18914_bank4.inc"
};

AnimationRecord D_actor_443500_8014A3E0[203] = {
#include "assets/actor_443500_animation_18914_records.inc"
};

u16 D_actor_443500_8014A70C[20] = {
#include "assets/actor_443500_animation_18914_indices.inc"
};

AnimationSet D_actor_443500_8014A734 = {
    D_actor_443500_8014A3E0,
    D_actor_443500_8014A70C,
    { NULL, D_actor_443500_8014A150, NULL, NULL, D_actor_443500_8014A1C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014A75C[33] = {
#include "assets/actor_443500_animation_197CC_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014A8E8[322] = {
#include "assets/actor_443500_animation_197CC_bank4.inc"
};

AnimationRecord D_actor_443500_8014ADF0[501] = {
#include "assets/actor_443500_animation_197CC_records.inc"
};

u16 D_actor_443500_8014B5C4[20] = {
#include "assets/actor_443500_animation_197CC_indices.inc"
};

AnimationSet D_actor_443500_8014B5EC = {
    D_actor_443500_8014ADF0,
    D_actor_443500_8014B5C4,
    { NULL, D_actor_443500_8014A75C, NULL, NULL, D_actor_443500_8014A8E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014B614[10] = {
#include "assets/actor_443500_animation_1A180_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014B68C[231] = {
#include "assets/actor_443500_animation_1A180_bank4.inc"
};

AnimationRecord D_actor_443500_8014BA28[340] = {
#include "assets/actor_443500_animation_1A180_records.inc"
};

u16 D_actor_443500_8014BF78[20] = {
#include "assets/actor_443500_animation_1A180_indices.inc"
};

AnimationSet D_actor_443500_8014BFA0 = {
    D_actor_443500_8014BA28,
    D_actor_443500_8014BF78,
    { NULL, D_actor_443500_8014B614, NULL, NULL, D_actor_443500_8014B68C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014BFC8[22] = {
#include "assets/actor_443500_animation_1AA30_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014C0D0[193] = {
#include "assets/actor_443500_animation_1AA30_bank4.inc"
};

AnimationRecord D_actor_443500_8014C3D4[277] = {
#include "assets/actor_443500_animation_1AA30_records.inc"
};

u16 D_actor_443500_8014C828[20] = {
#include "assets/actor_443500_animation_1AA30_indices.inc"
};

AnimationSet D_actor_443500_8014C850 = {
    D_actor_443500_8014C3D4,
    D_actor_443500_8014C828,
    { NULL, D_actor_443500_8014BFC8, NULL, NULL, D_actor_443500_8014C0D0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014C878[41] = {
#include "assets/actor_443500_animation_1BA70_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014CA64[384] = {
#include "assets/actor_443500_animation_1BA70_bank4.inc"
};

AnimationRecord D_actor_443500_8014D064[513] = {
#include "assets/actor_443500_animation_1BA70_records.inc"
};

u16 D_actor_443500_8014D868[20] = {
#include "assets/actor_443500_animation_1BA70_indices.inc"
};

AnimationSet D_actor_443500_8014D890 = {
    D_actor_443500_8014D064,
    D_actor_443500_8014D868,
    { NULL, D_actor_443500_8014C878, NULL, NULL, D_actor_443500_8014CA64, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014D8B8[9] = {
#include "assets/actor_443500_animation_1C41C_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014D924[245] = {
#include "assets/actor_443500_animation_1C41C_bank4.inc"
};

AnimationRecord D_actor_443500_8014DCF8[327] = {
#include "assets/actor_443500_animation_1C41C_records.inc"
};

u16 D_actor_443500_8014E214[20] = {
#include "assets/actor_443500_animation_1C41C_indices.inc"
};

AnimationSet D_actor_443500_8014E23C = {
    D_actor_443500_8014DCF8,
    D_actor_443500_8014E214,
    { NULL, D_actor_443500_8014D8B8, NULL, NULL, D_actor_443500_8014D924, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014E264[2] = {
#include "assets/actor_443500_animation_1C990_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014E27C[135] = {
#include "assets/actor_443500_animation_1C990_bank4.inc"
};

AnimationRecord D_actor_443500_8014E498[188] = {
#include "assets/actor_443500_animation_1C990_records.inc"
};

u16 D_actor_443500_8014E788[20] = {
#include "assets/actor_443500_animation_1C990_indices.inc"
};

AnimationSet D_actor_443500_8014E7B0 = {
    D_actor_443500_8014E498,
    D_actor_443500_8014E788,
    { NULL, D_actor_443500_8014E264, NULL, NULL, D_actor_443500_8014E27C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014E7D8[15] = {
#include "assets/actor_443500_animation_1CFD4_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014E88C[123] = {
#include "assets/actor_443500_animation_1CFD4_bank4.inc"
};

AnimationRecord D_actor_443500_8014EA78[213] = {
#include "assets/actor_443500_animation_1CFD4_records.inc"
};

u16 D_actor_443500_8014EDCC[20] = {
#include "assets/actor_443500_animation_1CFD4_indices.inc"
};

AnimationSet D_actor_443500_8014EDF4 = {
    D_actor_443500_8014EA78,
    D_actor_443500_8014EDCC,
    { NULL, D_actor_443500_8014E7D8, NULL, NULL, D_actor_443500_8014E88C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014EE1C[6] = {
#include "assets/actor_443500_animation_1D394_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014EE64[84] = {
#include "assets/actor_443500_animation_1D394_bank4.inc"
};

AnimationRecord D_actor_443500_8014EFB4[118] = {
#include "assets/actor_443500_animation_1D394_records.inc"
};

u16 D_actor_443500_8014F18C[20] = {
#include "assets/actor_443500_animation_1D394_indices.inc"
};

AnimationSet D_actor_443500_8014F1B4 = {
    D_actor_443500_8014EFB4,
    D_actor_443500_8014F18C,
    { NULL, D_actor_443500_8014EE1C, NULL, NULL, D_actor_443500_8014EE64, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014F1DC[7] = {
#include "assets/actor_443500_animation_1D758_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014F230[82] = {
#include "assets/actor_443500_animation_1D758_bank4.inc"
};

AnimationRecord D_actor_443500_8014F378[118] = {
#include "assets/actor_443500_animation_1D758_records.inc"
};

u16 D_actor_443500_8014F550[20] = {
#include "assets/actor_443500_animation_1D758_indices.inc"
};

AnimationSet D_actor_443500_8014F578 = {
    D_actor_443500_8014F378,
    D_actor_443500_8014F550,
    { NULL, D_actor_443500_8014F1DC, NULL, NULL, D_actor_443500_8014F230, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014F5A0[10] = {
#include "assets/actor_443500_animation_1DDD4_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014F618[142] = {
#include "assets/actor_443500_animation_1DDD4_bank4.inc"
};

AnimationRecord D_actor_443500_8014F850[223] = {
#include "assets/actor_443500_animation_1DDD4_records.inc"
};

u16 D_actor_443500_8014FBCC[20] = {
#include "assets/actor_443500_animation_1DDD4_indices.inc"
};

AnimationSet D_actor_443500_8014FBF4 = {
    D_actor_443500_8014F850,
    D_actor_443500_8014FBCC,
    { NULL, D_actor_443500_8014F5A0, NULL, NULL, D_actor_443500_8014F618, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8014FC1C[4] = {
#include "assets/actor_443500_animation_1E65C_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8014FC4C[205] = {
#include "assets/actor_443500_animation_1E65C_bank4.inc"
};

AnimationRecord D_actor_443500_8014FF80[309] = {
#include "assets/actor_443500_animation_1E65C_records.inc"
};

u16 D_actor_443500_80150454[20] = {
#include "assets/actor_443500_animation_1E65C_indices.inc"
};

AnimationSet D_actor_443500_8015047C = {
    D_actor_443500_8014FF80,
    D_actor_443500_80150454,
    { NULL, D_actor_443500_8014FC1C, NULL, NULL, D_actor_443500_8014FC4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_801504A4[74] = {
#include "assets/actor_443500_animation_1FF38_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8015081C[574] = {
#include "assets/actor_443500_animation_1FF38_bank4.inc"
};

AnimationRecord D_actor_443500_80151114[775] = {
#include "assets/actor_443500_animation_1FF38_records.inc"
};

u16 D_actor_443500_80151D30[20] = {
#include "assets/actor_443500_animation_1FF38_indices.inc"
};

AnimationSet D_actor_443500_80151D58 = {
    D_actor_443500_80151114,
    D_actor_443500_80151D30,
    { NULL, D_actor_443500_801504A4, NULL, NULL, D_actor_443500_8015081C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80151D80[2] = {
#include "assets/actor_443500_animation_201E0_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80151D98[32] = {
#include "assets/actor_443500_animation_201E0_bank4.inc"
};

AnimationRecord D_actor_443500_80151E18[112] = {
#include "assets/actor_443500_animation_201E0_records.inc"
};

u16 D_actor_443500_80151FD8[20] = {
#include "assets/actor_443500_animation_201E0_indices.inc"
};

AnimationSet D_actor_443500_80152000 = {
    D_actor_443500_80151E18,
    D_actor_443500_80151FD8,
    { NULL, D_actor_443500_80151D80, NULL, NULL, D_actor_443500_80151D98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80152028[7] = {
#include "assets/actor_443500_animation_204F0_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8015207C[53] = {
#include "assets/actor_443500_animation_204F0_bank4.inc"
};

AnimationRecord D_actor_443500_80152150[102] = {
#include "assets/actor_443500_animation_204F0_records.inc"
};

u16 D_actor_443500_801522E8[20] = {
#include "assets/actor_443500_animation_204F0_indices.inc"
};

AnimationSet D_actor_443500_80152310 = {
    D_actor_443500_80152150,
    D_actor_443500_801522E8,
    { NULL, D_actor_443500_80152028, NULL, NULL, D_actor_443500_8015207C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80152338[2] = {
#include "assets/actor_443500_animation_2071C_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80152350[21] = {
#include "assets/actor_443500_animation_2071C_bank4.inc"
};

AnimationRecord D_actor_443500_801523A4[92] = {
#include "assets/actor_443500_animation_2071C_records.inc"
};

u16 D_actor_443500_80152514[20] = {
#include "assets/actor_443500_animation_2071C_indices.inc"
};

AnimationSet D_actor_443500_8015253C = {
    D_actor_443500_801523A4,
    D_actor_443500_80152514,
    { NULL, D_actor_443500_80152338, NULL, NULL, D_actor_443500_80152350, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80152564[3] = {
#include "assets/actor_443500_animation_20CB4_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80152588[113] = {
#include "assets/actor_443500_animation_20CB4_bank4.inc"
};

AnimationRecord D_actor_443500_8015274C[216] = {
#include "assets/actor_443500_animation_20CB4_records.inc"
};

u16 D_actor_443500_80152AAC[20] = {
#include "assets/actor_443500_animation_20CB4_indices.inc"
};

AnimationSet D_actor_443500_80152AD4 = {
    D_actor_443500_8015274C,
    D_actor_443500_80152AAC,
    { NULL, D_actor_443500_80152564, NULL, NULL, D_actor_443500_80152588, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80152AFC[2] = {
#include "assets/actor_443500_animation_20FA8_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80152B14[62] = {
#include "assets/actor_443500_animation_20FA8_bank4.inc"
};

AnimationRecord D_actor_443500_80152C0C[101] = {
#include "assets/actor_443500_animation_20FA8_records.inc"
};

u16 D_actor_443500_80152DA0[20] = {
#include "assets/actor_443500_animation_20FA8_indices.inc"
};

AnimationSet D_actor_443500_80152DC8 = {
    D_actor_443500_80152C0C,
    D_actor_443500_80152DA0,
    { NULL, D_actor_443500_80152AFC, NULL, NULL, D_actor_443500_80152B14, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80152DF0[16] = {
#include "assets/actor_443500_animation_21738_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80152EB0[182] = {
#include "assets/actor_443500_animation_21738_bank4.inc"
};

AnimationRecord D_actor_443500_80153188[234] = {
#include "assets/actor_443500_animation_21738_records.inc"
};

u16 D_actor_443500_80153530[20] = {
#include "assets/actor_443500_animation_21738_indices.inc"
};

AnimationSet D_actor_443500_80153558 = {
    D_actor_443500_80153188,
    D_actor_443500_80153530,
    { NULL, D_actor_443500_80152DF0, NULL, NULL, D_actor_443500_80152EB0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80153580[13] = {
#include "assets/actor_443500_animation_21E4C_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8015361C[173] = {
#include "assets/actor_443500_animation_21E4C_bank4.inc"
};

AnimationRecord D_actor_443500_801538D0[221] = {
#include "assets/actor_443500_animation_21E4C_records.inc"
};

u16 D_actor_443500_80153C44[20] = {
#include "assets/actor_443500_animation_21E4C_indices.inc"
};

AnimationSet D_actor_443500_80153C6C = {
    D_actor_443500_801538D0,
    D_actor_443500_80153C44,
    { NULL, D_actor_443500_80153580, NULL, NULL, D_actor_443500_8015361C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80153C94[8] = {
#include "assets/actor_443500_animation_22120_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80153CF4[45] = {
#include "assets/actor_443500_animation_22120_bank4.inc"
};

AnimationRecord D_actor_443500_80153DA8[92] = {
#include "assets/actor_443500_animation_22120_records.inc"
};

u16 D_actor_443500_80153F18[20] = {
#include "assets/actor_443500_animation_22120_indices.inc"
};

AnimationSet D_actor_443500_80153F40 = {
    D_actor_443500_80153DA8,
    D_actor_443500_80153F18,
    { NULL, D_actor_443500_80153C94, NULL, NULL, D_actor_443500_80153CF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80153F68[4] = {
#include "assets/actor_443500_animation_22A1C_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80153F98[225] = {
#include "assets/actor_443500_animation_22A1C_bank4.inc"
};

AnimationRecord D_actor_443500_8015431C[318] = {
#include "assets/actor_443500_animation_22A1C_records.inc"
};

u16 D_actor_443500_80154814[20] = {
#include "assets/actor_443500_animation_22A1C_indices.inc"
};

AnimationSet D_actor_443500_8015483C = {
    D_actor_443500_8015431C,
    D_actor_443500_80154814,
    { NULL, D_actor_443500_80153F68, NULL, NULL, D_actor_443500_80153F98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80154864[2] = {
#include "assets/actor_443500_animation_22F94_bank1.inc"
};

AnimationPackedRotation D_actor_443500_8015487C[135] = {
#include "assets/actor_443500_animation_22F94_bank4.inc"
};

AnimationRecord D_actor_443500_80154A98[189] = {
#include "assets/actor_443500_animation_22F94_records.inc"
};

u16 D_actor_443500_80154D8C[20] = {
#include "assets/actor_443500_animation_22F94_indices.inc"
};

AnimationSet D_actor_443500_80154DB4 = {
    D_actor_443500_80154A98,
    D_actor_443500_80154D8C,
    { NULL, D_actor_443500_80154864, NULL, NULL, D_actor_443500_8015487C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80154DDC[2] = {
#include "assets/actor_443500_animation_23814_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80154DF4[229] = {
#include "assets/actor_443500_animation_23814_bank4.inc"
};

AnimationRecord D_actor_443500_80155188[289] = {
#include "assets/actor_443500_animation_23814_records.inc"
};

u16 D_actor_443500_8015560C[20] = {
#include "assets/actor_443500_animation_23814_indices.inc"
};

AnimationSet D_actor_443500_80155634 = {
    D_actor_443500_80155188,
    D_actor_443500_8015560C,
    { NULL, D_actor_443500_80154DDC, NULL, NULL, D_actor_443500_80154DF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8015565C[30] = {
#include "assets/actor_443500_animation_24544_bank1.inc"
};

AnimationPackedRotation D_actor_443500_801557C4[312] = {
#include "assets/actor_443500_animation_24544_bank4.inc"
};

AnimationRecord D_actor_443500_80155CA4[422] = {
#include "assets/actor_443500_animation_24544_records.inc"
};

u16 D_actor_443500_8015633C[20] = {
#include "assets/actor_443500_animation_24544_indices.inc"
};

AnimationSet D_actor_443500_80156364 = {
    D_actor_443500_80155CA4,
    D_actor_443500_8015633C,
    { NULL, D_actor_443500_8015565C, NULL, NULL, D_actor_443500_801557C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8015638C[3] = {
#include "assets/actor_443500_animation_24C1C_bank1.inc"
};

AnimationPackedRotation D_actor_443500_801563B0[83] = {
#include "assets/actor_443500_animation_24C1C_bank4.inc"
};

AnimationRecord D_actor_443500_801564FC[326] = {
#include "assets/actor_443500_animation_24C1C_records.inc"
};

u16 D_actor_443500_80156A14[20] = {
#include "assets/actor_443500_animation_24C1C_indices.inc"
};

AnimationSet D_actor_443500_80156A3C = {
    D_actor_443500_801564FC,
    D_actor_443500_80156A14,
    { NULL, D_actor_443500_8015638C, NULL, NULL, D_actor_443500_801563B0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80156A64[11] = {
#include "assets/actor_443500_animation_25234_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80156AE8[120] = {
#include "assets/actor_443500_animation_25234_bank4.inc"
};

AnimationRecord D_actor_443500_80156CC8[217] = {
#include "assets/actor_443500_animation_25234_records.inc"
};

u16 D_actor_443500_8015702C[20] = {
#include "assets/actor_443500_animation_25234_indices.inc"
};

AnimationSet D_actor_443500_80157054 = {
    D_actor_443500_80156CC8,
    D_actor_443500_8015702C,
    { NULL, D_actor_443500_80156A64, NULL, NULL, D_actor_443500_80156AE8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_8015707C[11] = {
#include "assets/actor_443500_animation_25700_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80157100[106] = {
#include "assets/actor_443500_animation_25700_bank4.inc"
};

AnimationRecord D_actor_443500_801572A8[148] = {
#include "assets/actor_443500_animation_25700_records.inc"
};

u16 D_actor_443500_801574F8[20] = {
#include "assets/actor_443500_animation_25700_indices.inc"
};

AnimationSet D_actor_443500_80157520 = {
    D_actor_443500_801572A8,
    D_actor_443500_801574F8,
    { NULL, D_actor_443500_8015707C, NULL, NULL, D_actor_443500_80157100, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80157548[9] = {
#include "assets/actor_443500_animation_25D38_bank1.inc"
};

AnimationPackedRotation D_actor_443500_801575B4[132] = {
#include "assets/actor_443500_animation_25D38_bank4.inc"
};

AnimationRecord D_actor_443500_801577C4[219] = {
#include "assets/actor_443500_animation_25D38_records.inc"
};

u16 D_actor_443500_80157B30[20] = {
#include "assets/actor_443500_animation_25D38_indices.inc"
};

AnimationSet D_actor_443500_80157B58 = {
    D_actor_443500_801577C4,
    D_actor_443500_80157B30,
    { NULL, D_actor_443500_80157548, NULL, NULL, D_actor_443500_801575B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80157B80[6] = {
#include "assets/actor_443500_animation_2609C_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80157BC8[72] = {
#include "assets/actor_443500_animation_2609C_bank4.inc"
};

AnimationRecord D_actor_443500_80157CE8[107] = {
#include "assets/actor_443500_animation_2609C_records.inc"
};

u16 D_actor_443500_80157E94[20] = {
#include "assets/actor_443500_animation_2609C_indices.inc"
};

AnimationSet D_actor_443500_80157EBC = {
    D_actor_443500_80157CE8,
    D_actor_443500_80157E94,
    { NULL, D_actor_443500_80157B80, NULL, NULL, D_actor_443500_80157BC8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_80157EE4[3] = {
#include "assets/actor_443500_animation_26274_bank1.inc"
};

AnimationPackedRotation D_actor_443500_80157F08[29] = {
#include "assets/actor_443500_animation_26274_bank4.inc"
};

AnimationRecord D_actor_443500_80157F7C[60] = {
#include "assets/actor_443500_animation_26274_records.inc"
};

u16 D_actor_443500_8015806C[20] = {
#include "assets/actor_443500_animation_26274_indices.inc"
};

AnimationSet D_actor_443500_80158094 = {
    D_actor_443500_80157F7C,
    D_actor_443500_8015806C,
    { NULL, D_actor_443500_80157EE4, NULL, NULL, D_actor_443500_80157F08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_801580BC[3] = {
#include "assets/actor_443500_animation_26690_bank1.inc"
};

AnimationPackedRotation D_actor_443500_801580E0[80] = {
#include "assets/actor_443500_animation_26690_bank4.inc"
};

AnimationRecord D_actor_443500_80158220[154] = {
#include "assets/actor_443500_animation_26690_records.inc"
};

u16 D_actor_443500_80158488[20] = {
#include "assets/actor_443500_animation_26690_indices.inc"
};

AnimationSet D_actor_443500_801584B0 = {
    D_actor_443500_80158220,
    D_actor_443500_80158488,
    { NULL, D_actor_443500_801580BC, NULL, NULL, D_actor_443500_801580E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_443500_801584D8[2] = {
#include "assets/actor_443500_animation_2684C_bank1.inc"
};

AnimationPackedRotation D_actor_443500_801584F0[25] = {
#include "assets/actor_443500_animation_2684C_bank4.inc"
};

AnimationRecord D_actor_443500_80158554[60] = {
#include "assets/actor_443500_animation_2684C_records.inc"
};

u16 D_actor_443500_80158644[20] = {
#include "assets/actor_443500_animation_2684C_indices.inc"
};

AnimationSet D_actor_443500_8015866C = {
    D_actor_443500_80158554,
    D_actor_443500_80158644,
    { NULL, D_actor_443500_801584D8, NULL, NULL, D_actor_443500_801584F0, NULL, NULL, NULL },
};

AnimationSet* D_actor_443500_80158694[36] = {
    NULL,
    &D_actor_443500_80149BF0,
    &D_actor_443500_8014A128,
    &D_actor_443500_8014A734,
    &D_actor_443500_8014B5EC,
    &D_actor_443500_8014BFA0,
    &D_actor_443500_8014C850,
    &D_actor_443500_8014D890,
    &D_actor_443500_8014E23C,
    &D_actor_443500_8014E7B0,
    &D_actor_443500_8014EDF4,
    &D_actor_443500_8014F1B4,
    &D_actor_443500_8014F578,
    &D_actor_443500_8014FBF4,
    &D_actor_443500_8015047C,
    &D_actor_443500_80151D58,
    &D_actor_443500_80152000,
    &D_actor_443500_80152310,
    &D_actor_443500_8015253C,
    &D_actor_443500_80152AD4,
    &D_actor_443500_80152DC8,
    &D_actor_443500_80153558,
    &D_actor_443500_80153C6C,
    &D_actor_443500_80153F40,
    &D_actor_443500_8015483C,
    &D_actor_443500_80154DB4,
    &D_actor_443500_80155634,
    &D_actor_443500_80156364,
    &D_actor_443500_80156A3C,
    &D_actor_443500_80157054,
    &D_actor_443500_80157520,
    &D_actor_443500_80157B58,
    &D_actor_443500_80157EBC,
    &D_actor_443500_80158094,
    &D_actor_443500_801584B0,
    &D_actor_443500_8015866C,
};

AnimationSet** D_actor_443500_80158724[1] = {
    D_actor_443500_80158694,
};

AnimationPlayRequest D_actor_443500_80158728 = { { .sets = NULL }, 28, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE };

void             func_actor_443500_8013253C(Task*);
void             func_actor_443500_80132738(Task*);
extern TmdSource D_actor_443500_80149978;

TaskDesc D_actor_443500_8015873C[2] = {
    { (TASK_BODY_TMD | 0x100), 192, func_actor_443500_80132738, { .model = &D_actor_443500_8014977C } },
    { TASK_BODY_TMD, 192, func_actor_443500_8013253C, { .model = &D_actor_443500_80149978 } },
};

s32 func_actor_443500_801327E0(Task*, s32, AnimationPlayRequest*, s32);
s32 func_actor_443500_8013297C(Task*, s32, s32, s32);

Actor443500MessageEntry D_actor_443500_80158754[4] = {
    { 2003, { .call0 = func_actor_443500_801327E0 } },
    { 2004, { .call1 = actorMsgPlaceEuler } },
    { 2005, { .call2 = func_actor_443500_8013297C } },
    { 2147483647, { .call0 = NULL } },
};

SVECTOR D_actor_443500_80158774[2] = {
#include "assets/actor_443500_collision_269B8_normals.inc"
};

SVECTOR D_actor_443500_80158784[6] = {
#include "assets/actor_443500_collision_269B8_verts.inc"
};

GpGridFace D_actor_443500_801587B4[2] = {
#include "assets/actor_443500_collision_269B8_faces.inc"
};

s16 D_actor_443500_801587CC[4] = {
#include "assets/actor_443500_collision_269B8_cells.inc"
};

#define GRID_CELL(i) (&D_actor_443500_801587CC[i])
s16* D_actor_443500_801587D4[1] = {
#include "assets/actor_443500_collision_269B8_table.inc"
};
#undef GRID_CELL

GpGridParams D_actor_443500_801587D8 = { NULL, D_actor_443500_80158774, D_actor_443500_80158784, D_actor_443500_801587B4, D_actor_443500_801587D4, -0x3EF8, -5156, 1, 1, 4000, 2 };

void func_actor_443500_80131E3C(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x240, 0x100);
        return;
    }
    Gp_ResetCap();
}

void func_actor_443500_80131E84(s32 arg0)
{
    if (GameFlag_GetNibble(0xDF) > 0) {
        if (arg0 != 0) {
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x240, 0x100);
            return;
        }
        Gp_ResetCap();
    }
}

void func_actor_443500_80131EE4(void)
{
    Gp_RunCapCmd(GameFlag_GetNibble(0xDF) == 0 ? 6 : 9, 0);
}

void func_actor_443500_80131F18(void)
{
    Task_SpawnFromTable(&D_shelter_r47_80187618, 0, 1, 0);
    Gp_MsgPlayer3F3(0);
    Gp_MsgPlayerWeapon(0);
}

void func_actor_443500_80131F58(void)
{
    Task_SpawnFromTable(&D_actor_443500_80140E38, 0, 0, 0);
}

void func_actor_443500_80131F88(Task* arg0)
{
    s16 temp_v0;
    s32 temp_a0;

    temp_a0 = (((0x1E - arg0->killCountdown) * 0xFF) / 30) & 0xFF;
    Fade_DrawOverlay(temp_a0, temp_a0, temp_a0, 2);
    temp_v0             = (u16)arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x1E) {
        taskKill(arg0);
    }
}

void func_actor_443500_8013201C(s16 arg0)
{
    Gp_StartCapSlot(5, 1, arg0);
}

/// Applies the 0xFF-terminated area record list at `D_shelter_r47_8018A638` through
/// `Gp_ApplyAreaRecs`. It is reached only through the function pointers in
/// the actor's data.
void func_actor_443500_80132048(void)
{
    Gp_ApplyAreaRecs(D_shelter_r47_8018A638);
}

void func_actor_443500_8013206C(s8 arg0)
{
    Mc_SaveData[0].state.sceneEvent = arg0;
}

/// Spawn handler: allocates the work block, seeds its head from the parent
/// model, starts the actor's child task and copies the location it spawns over
/// from the session key onto that child's model, then installs the animation
/// table, the exit callback and the tick handler.
static void func_actor_443500_80132078(Task* task)
{
    Actor443500Work* work;
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    u8               areaByte0;
    GpAreaVariant*   rec;
    AreaPlacement*   entry;
    TmdObject*       model;
    Task*            spawned;
    s32              idx;
    u32              raw;

    work = memCalloc(0x4C4, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work         = work;
    work->model.animId = -1;
    work->model.bank   = -1;
    work->field_4BC    = -1;
    work->field_4C0    = task->extra.tmd->flags;
    spawned            = Task_SpawnFromTable(D_actor_443500_8015873C, 1, 4, task);
    if (spawned != NULL) {
        sessionKey = &gGameSession->location.loc;
        raw        = ((GpEnemy*)task->spawnArg2.pointer)->placeKey;
        model      = spawned->extra.tmd;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        areaByte0  = sessionKey->view;
        idx        = raw >> 12;
        key.view   = areaByte0;
        areaSyncLocationVariant(&key);
        rec = Gp_GetNestedAreaRec(&key);
        /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled
           index onto the table (`addu s0, s0, v0`). */
        entry                    = gpAreaPlaceAt(rec->field_0, idx);
        model->texturePageOffset = entry->texturePageOffset;
        model->clutRowOffset     = entry->clutRowOffset;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    func_actor_443500_8013297C(task, 0x7D5, 0, 0);
    func_actor_443500_801327E0(task, 0x7D3, &D_actor_443500_80158728, 0);
    func_actor_443500_801327C4(task);
    task->msgTable     = D_actor_443500_80158754;
    task->exitCallback = func_actor_443500_801327A4;
    task->state++;
}

/// Per-frame tick: while the view is live and idle, views 0..3 hide the model
/// (saving `TmdObject::flags` into `field_4C0`) and views 4..5 restore that
/// saved word, showing the model through message 0x7D5 when flag 0x83 is set.
/// Ticks animation slots 1..0x13 once `model.ticking` is latched, restarting 0x7D3
/// when slot 1 reports the clip ended. The `model.animId == 0x1C` path is the
/// default clip's sound: `field_4BA` counts to 0xF for a Type6 (views 4/5) or
/// Type7 (view 3) cue, TypeA otherwise while the view is ready, and resets when
/// slot 1 reports `ANIMATION_SLOT_FOLLOWED_JUMP`. A visible model gets a ground
/// shadow and a rebuilt child-part matrix; `field_4BC` then counts down to free
/// the buffers.
static void func_actor_443500_801321F0(Task* task)
{
    Actor443500Work* work;
    TmdObject*       extra;
    VECTOR3          pos;
    s32              i;
    u8               view;

    extra = task->extra.tmd;
    work  = (Actor443500Work*)task->work;
    if (gGameSession->viewReady != 0 && gGameSession->eventState == 0 &&
        gGameSession->cutsceneHold == 0) {
        view = gGameSession->location.loc.view;
        if (view < 4) {
            work->field_4C0 = extra->flags;
            extra->flags    = extra->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else if (view < 6) {
            if (GameFlag_GetNibble(0x83) > 0) {
                func_actor_443500_80132A68(0);
                func_actor_443500_8013297C(task, 0x7D5, 1, 0);
            }
            extra->flags = work->field_4C0;
        }
    }
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        if (gGameSession->eventState == 0 && (work->rig.slots[1].flags & ANIMATION_SLOT_REACHED_END)) {
            func_actor_443500_801327E0(task, 0x7D3, &D_actor_443500_80158728, 0);
        }
    }
    if (work->model.animId == 0x1C) {
        work->field_4BA++;
        if (work->field_4BA == 0xF) {
            switch (gGameSession->location.loc.view) {
                case 5:
                    SndEvt_EnqueueType6(0x542F0001, 9, 0);
                    break;
                case 4:
                    SndEvt_EnqueueType6(0x542F0001, -0xA, 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType7(0x542F0001, 0x1E);
                    break;
            }
        } else if (gGameSession->viewReady != 0) {
            switch (gGameSession->location.loc.view) {
                case 5:
                    SndEvt_EnqueueTypeA(0x542F0001, 9, 0);
                    break;
                case 4:
                    SndEvt_EnqueueTypeA(0x542F0001, -0xA, 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType7(0x542F0001, 0x1E);
                    break;
            }
        }
        if (work->rig.slots[1].flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
            work->field_4BA = 0;
        }
    }
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, gRoomEffectState->groundShadowShade);
        }
        if (gGameSession->viewReady != 0) {
            Gp_UpdateCoord(&task->extra.tmd->coords[1]);
            func_800D7A9C(extra, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
        }
    }
    if (work->field_4BC >= 0) {
        if (work->field_4BC == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_4BC--;
    }
}

/// Dispatcher of the actor's child task: runs its current state handler from
/// `D_actor_443500_80131E24`, copying the table onto the stack before the call.
void func_actor_443500_8013253C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_443500_80131E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

/// Per-frame state of the actor's child task: mirrors the hidden bit 0x80 and
/// the buffers-live bit 0x4 of the parent's `TmdObject` - the task the spawn
/// handler passed as `Task::spawnArg2` - onto the child's own model. When the
/// parent's 0x4 is clear the child's is cleared too and its buffers are
/// reallocated through `Tmd_AllocBuffers`.
static void func_actor_443500_801326A0(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = ((Task*)task->spawnArg2.pointer)->extra.tmd;
    object       = task->extra.tmd;

    if (!(parentObject->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        object->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        object->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (!(parentObject->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        object->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        Tmd_AllocBuffers(object);
        return;
    }
    object->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
}

/// Per-frame dispatcher of the main task: runs its spawn, tick or exit state
/// from `D_actor_443500_80131E30`, skipping the frame while `Gp_StateF0.field_4` is
/// set.
void func_actor_443500_80132738(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_443500_80131E30;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Exit callback the spawn handler installs, and the third state of the main
/// task: hands the task to `Gp_EnemyTaskExit`.
static void func_actor_443500_801327A4(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Points the model's `TmdObject::lightMtx` / `colorMtx` at the work block's
/// own `light` / `color` matrices, so the actor draws with its own lighting.
static void func_actor_443500_801327C4(Task* task)
{
    TmdObject*       ext;
    Actor443500Work* work;

    ext           = task->extra.tmd;
    work          = (Actor443500Work*)task->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Applies the requested animation bank and clip to this actor's rig.
///
/// A changed bank installs its set table. The requested clip is applied to the slots.
/// Blends an already ticking rig when requested, using a whole-frame duration;
/// otherwise resets the slots before ticking them.
s32 func_actor_443500_801327E0(Task* task, s32 anim, AnimationPlayRequest* params, s32 arg3)
{
    Actor443500Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor443500Work*)task->work;
    ext  = task->extra.tmd;
    if (params->source.index != work->model.bank) {
        work->model.bank = params->source.index;
        func_800B3F84(&work->rig.anim, D_actor_443500_80158724[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->model.animId = params->animationId;
    if (params->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->rig.anim, i, work->model.animId, 0, params->blendFrames);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    work->field_4BA     = 0;
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Message-0x7D5 handler: the four-way switch on `mode` over the `TmdObject`
/// parked in `Task::extra`. `mode` drives `TmdObject::flags`: bit 0x80 marks
/// the actor hidden and bit 0x4 the display buffers being live.
///
///   mode 0  hide, drop 0x4
///   mode 1  show, `Tmd_AllocBuffers`, drop 0x4
///   mode 2  hide, latch `mode` in the work block's `field_4BC`, raise 0x4
///   mode 3  show, raise 0x4
///
/// Any other mode returns 1; the four known ones return 0. Either way the
/// resulting flags are mirrored onto `Actor443500Work::field_4C0`, the slot
/// the spawn handler seeds from the model's own flags. `field_4BC` is the word
/// the spawn handler seeds to -1 and the tick counts down to free the buffers.
/// `anim` and `arg3` are unused -- the dispatch passes four arguments.
s32 func_actor_443500_8013297C(Task* task, s32 anim, s32 mode, s32 arg3)
{
    TmdObject*       obj;
    s32              ret;
    Actor443500Work* work;

    obj  = task->extra.tmd;
    work = (Actor443500Work*)task->work;
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
            work->field_4BC = mode;
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
    work->field_4C0 = obj->flags;
    return ret;
}

/// Copy the overlay's layout template `D_actor_443500_801587D8` into the live
/// table at `D_shelter_r47_8018828C`. When `arg0` is nonzero, shift the six live target
/// positions by `(0, 0x7D0, 0)` afterwards. The per-frame tick calls this
/// with 0 before showing the model.
static void func_actor_443500_80132A68(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_shelter_r47_8018828C;
    src = &D_actor_443500_801587D8;

    for (i = 0; i < 2; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0x7D0;
    }
    d.vz = 0;

    for (i = 0; i < 6; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}
