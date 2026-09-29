#include "actor_342400_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/actors_shared_80163354.h"

#include "actors/actors_shared_801673f8.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pairsrc.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_dumping_hole.h"

#include "rooms/shelter_b3_garbage_incinerator.h"

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

// the main enemy's `GpEnemy::param` record
extern u8 D_actor_342400_801739E8[]; // animation bank handed to `func_800B3F84`
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s16, VECTOR3*);
        void (*call1)(Task*, s32, GpCmdArg*);
    } handler;
} Actor3424002MessageEntry;
STATIC_ASSERT_SIZEOF(Actor3424002MessageEntry, 8);

extern Actor3424002MessageEntry D_actor_342400_80173A3C[3]; // stored into `Task::msgTable` by func_actor_342400_80163C58
extern u8                       D_actor_342400_80173A84[];  // per animation id (1-based): value for `field_44F`
extern u8                       D_actor_342400_80173A98[];  // per animation id (1-based): the animation to follow it

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_342400_80163C58(Task* task);
static void func_actor_342400_80163E70(Task* task);
static void func_actor_342400_801640B0(Task* arg0);
static void func_actor_342400_8016454C(Task* arg0);
static void func_actor_342400_801646B8(Task* arg0);
static void func_actor_342400_801648E4(Task* arg0);
static void func_actor_342400_80164CA4(Task* arg0);
static void func_actor_342400_80164DD4(Task* arg0);
static void func_actor_342400_80164F3C(Task* arg0);
static void func_actor_342400_8016513C(Task* arg0);
static void func_actor_342400_801652A0(Task* arg0);
static void func_actor_342400_801653DC(Task* arg0, s16 arg1);
static void func_actor_342400_80165CC0(Task* arg0);
static void func_actor_342400_80165E4C(Task* task);
static void func_actor_342400_80165FC0(Task* arg0);
static void func_actor_342400_80166180(Task* arg0);
static void func_actor_342400_801662EC(Task* arg0);
static void func_actor_342400_8016666C(Task* arg0);
static void func_actor_342400_80166B20(Task* arg0);
static void func_actor_342400_80166C68(Task* arg0);
static void func_actor_342400_80166DD4(Task* arg0);
static void func_actor_342400_80166F54(Task* arg0);
static void func_actor_342400_801670C0(Task* arg0);
static void func_actor_342400_801673F8(Task* arg0);
static void func_actor_342400_801676D4(Task* arg0);
static void func_actor_342400_8016784C(Task* arg0);
static void func_actor_342400_801679D4(Task* arg0);
static void func_actor_342400_80167B70(Task* arg0);
static void func_actor_342400_80167CDC(Task* arg0);
static void func_actor_342400_80167E78(Task* arg0);
static void func_actor_342400_80168010(Task* arg0);
static void func_actor_342400_80168174(Task* arg0);
static void func_actor_342400_80168394(Task* arg0);
static void func_actor_342400_80168530(Task* arg0);
static void func_actor_342400_80168A28(Task* arg0);
static void func_actor_342400_80168B74(Task* arg0);
static void func_actor_342400_80168F14(Task* arg0);
static void func_actor_342400_801690FC(Task* arg0);
static void func_actor_342400_801692E8(void);
static void func_actor_342400_80169408(Task* arg0);
static s16  func_actor_342400_8016945C(Task* arg0);
static void func_actor_342400_801694A8(Task* arg0, s32 arg1);
static s32  func_actor_342400_80169518(Task* arg0);
static void func_actor_342400_80169654(Task* arg0, s16 arg1, SVECTOR3* arg2);
static s32  func_actor_342400_80169728(Task* arg0, s16 arg1);
static s16  func_actor_342400_8016974C(Task* arg0);
void        func_actor_342400_8016978C(Task* arg0);
void        func_actor_342400_80169810(Task* arg0);
static void func_actor_342400_80169880(Task* arg0);
static void func_actor_342400_801698D4(Task* arg0, s32 step);
static void func_actor_342400_80169968(Task* arg0);
static void func_actor_342400_8016997C(Task* arg0);
static void func_actor_342400_80169990(Task* arg0);
static void func_actor_342400_801699A4(Task* arg0);
static void func_actor_342400_80169A2C(Task* arg0);
static void func_actor_342400_80169A98(Task* arg0);
static void func_actor_342400_80169B04(Task* arg0);
static void func_actor_342400_80169B58(Task* arg0);
static void func_actor_342400_80169BAC(Task* arg0);
static void func_actor_342400_80169C00(Task* arg0);
static void func_actor_342400_80169C84(Task* arg0);
static void func_actor_342400_80169CF8(Task* arg0);
static void func_actor_342400_80169D2C(Task* arg0);
static void func_actor_342400_80169DA4(Task* arg0);
static void func_actor_342400_80169E24(Task* arg0);
static void func_actor_342400_80169EC4(Task* arg0);
static void func_actor_342400_80169F30(Task* arg0);
static void func_actor_342400_8016A020(Task* arg0);
static void func_actor_342400_8016A084(Task* arg0);
static void func_actor_342400_8016A184(Task* arg0);
static void func_actor_342400_8016A240(Task* arg0);
static void func_actor_342400_8016A280(Task* arg0);
static void func_actor_342400_8016A2FC(Task* arg0);
static void func_actor_342400_8016A370(Task* arg0);
static void func_actor_342400_8016A494(Task* arg0);
static void func_actor_342400_8016A4FC(Task* arg0);
static void func_actor_342400_8016A538(Task* arg0);
static void func_actor_342400_8016A664(Task* arg0);
static void func_actor_342400_8016A724(Task* arg0);
static void func_actor_342400_8016A804(Task* arg0);
static void func_actor_342400_8016A884(Task* task);
static void func_actor_342400_8016A950(Task* arg0);
static void func_actor_342400_8016A9AC(Task* arg0);
static void func_actor_342400_8016A9C4(Task* arg0);
static void func_actor_342400_8016AA08(Task* arg0);
static void func_actor_342400_8016AA9C(Task* arg0);
static void func_actor_342400_8016AAB8(Task* arg0);
static void func_actor_342400_8016AB6C(Task* arg0);
static void func_actor_342400_8016AC80(Task* arg0);
static void func_actor_342400_8016AD94(Task* arg0);
static void func_actor_342400_8016AE24(Task* arg0);
static void func_actor_342400_8016AEAC(Task* arg0);
static void func_actor_342400_8016AF34(Task* arg0);
static void func_actor_342400_8016AFA8(Task* arg0);
static void func_actor_342400_8016B038(Task* arg0);
static void func_actor_342400_8016B0A0(Task* arg0);
static void func_actor_342400_8016B104(Task* arg0);
static void func_actor_342400_8016B1C8(Task* arg0);
static void func_actor_342400_8016B21C(Task* arg0);
static void func_actor_342400_8016B294(Task* arg0);
static void func_actor_342400_8016B33C(Task* arg0);
static void func_actor_342400_8016B370(Task* arg0);
static void func_actor_342400_8016B3C4(Task* arg0);
static void func_actor_342400_8016B414(Task* arg0);
static void func_actor_342400_8016B48C(Task* arg0);
static void func_actor_342400_8016B500(Task* arg0);
static void func_actor_342400_8016B5B0(Task* arg0);
static void func_actor_342400_8016B744(Task* arg0);
static void func_actor_342400_8016B84C(Task* arg0);
static void func_actor_342400_8016B914(Task* arg0);
static void func_actor_342400_8016B9A4(Task* arg0);
static void func_actor_342400_8016BA3C(Task* arg0);
static void func_actor_342400_8016BAF4(Task* arg0);
static void func_actor_342400_8016BB74(Task* arg0);
static void func_actor_342400_8016BBD0(Task* arg0);
static void func_actor_342400_8016BBD8(Task* arg0);
static void func_actor_342400_8016BC70(Task* task);
static void func_actor_342400_8016BD3C(Task* arg0);
static void func_actor_342400_8016BD98(Task* arg0);
static void func_actor_342400_8016BED8(Task* arg0);
static s32  func_actor_342400_8016BEF0(Task* arg0);

/// Six task-state handlers of the first enemy form, dispatched by
/// `func_actor_342400_80169810` on `Task::state`.
static const TaskFuncTable6 D_actor_342400_80161E68 = { {
    func_actor_342400_80163C58,
    func_actor_342400_8016666C,
    func_actor_342400_80164F3C,
    func_actor_342400_801640B0,
    func_actor_342400_80165FC0,
    func_actor_342400_80169880,
} };

/// Ten task-state handlers of the second enemy form, dispatched by
/// `func_actor_342400_8016978C` on `Task::state`.
static const TaskFuncTable10 D_actor_342400_80161E80 = { {
    func_actor_342400_80163E70,
    func_actor_342400_8016666C,
    func_actor_342400_80164F3C,
    func_actor_342400_801640B0,
    func_actor_342400_80165FC0,
    func_actor_342400_80169880,
    func_actor_342400_801670C0,
    func_actor_342400_80169408,
    func_actor_342400_80168F14,
    func_actor_342400_801690FC,
} };

/// Eleven state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable11 D_actor_342400_80161EA8 = { {
    func_actor_342400_80169968,
    func_actor_342400_8016997C,
    func_actor_342400_80169990,
    func_actor_342400_801699A4,
    func_actor_342400_80169A2C,
    func_actor_342400_80169A98,
    func_actor_342400_80169B04,
    func_actor_342400_80169B58,
    func_actor_342400_80169BAC,
    func_actor_342400_80169C00,
    func_actor_342400_80169C84,
} };

/// Sub-state handlers `func_actor_342400_80169C00` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161ED4 = { {
    func_actor_342400_80169DA4,
    func_actor_342400_80169E24,
    func_actor_342400_80169EC4,
} };

/// Sub-state handlers `func_actor_342400_801699A4` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161EE0 = { {
    func_actor_342400_8016454C,
    func_actor_342400_801646B8,
    func_actor_342400_80169F30,
} };

/// Sub-state handlers `func_actor_342400_80169A2C` dispatches by `field_422`.
static const TaskFuncTable5 D_actor_342400_80161EEC = { {
    func_actor_342400_8016A020,
    func_actor_342400_801648E4,
    func_actor_342400_80164CA4,
    func_actor_342400_80164DD4,
    func_actor_342400_8016A084,
} };

/// Sub-state handlers `func_actor_342400_80169A98` dispatches by `field_422`.
static const TaskFuncTable5 D_actor_342400_80161F00 = { {
    func_actor_342400_8016A184,
    func_actor_342400_8016A240,
    func_actor_342400_8016A280,
    func_actor_342400_8016A2FC,
    func_actor_342400_8016A370,
} };

/// Sub-state handlers `func_actor_342400_8016A494` dispatches by `field_422`.
static const TaskFuncTable4 D_actor_342400_80161F14 = { {
    func_actor_342400_8016A4FC,
    func_actor_342400_8016A538,
    func_actor_342400_8016513C,
    func_actor_342400_801652A0,
} };

void func_actor_342400_8016978C(Task*);
void func_actor_342400_80169810(Task*);

void func_actor_342400_801695C0(Task*, s32, GpCmdArg*);
void func_actor_342400_80169620(Task*, s16, VECTOR3*);

AnimationPackedPose D_actor_342400_80170598[6] = {
#include "assets/actor_342400_animation_0E9A4_bank1.inc"
};

AnimationPackedRotation D_actor_342400_801705E0[39] = {
#include "assets/actor_342400_animation_0E9A4_bank4.inc"
};

GpAnimRec D_actor_342400_8017067C[77] = {
#include "assets/actor_342400_animation_0E9A4_records.inc"
};

u16 D_actor_342400_801707B0[10] = {
#include "assets/actor_342400_animation_0E9A4_indices.inc"
};

GpAnimSet D_actor_342400_801707C4 = {
    D_actor_342400_8017067C,
    D_actor_342400_801707B0,
    { NULL, D_actor_342400_80170598, NULL, NULL, D_actor_342400_801705E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801707EC[3] = {
#include "assets/actor_342400_animation_0EB48_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80170810[21] = {
#include "assets/actor_342400_animation_0EB48_bank4.inc"
};

GpAnimRec D_actor_342400_80170864[60] = {
#include "assets/actor_342400_animation_0EB48_records.inc"
};

u16 D_actor_342400_80170954[10] = {
#include "assets/actor_342400_animation_0EB48_indices.inc"
};

GpAnimSet D_actor_342400_80170968 = {
    D_actor_342400_80170864,
    D_actor_342400_80170954,
    { NULL, D_actor_342400_801707EC, NULL, NULL, D_actor_342400_80170810, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80170990[20] = {
#include "assets/actor_342400_animation_0F01C_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80170A80[99] = {
#include "assets/actor_342400_animation_0F01C_bank4.inc"
};

GpAnimRec D_actor_342400_80170C0C[135] = {
#include "assets/actor_342400_animation_0F01C_records.inc"
};

u16 D_actor_342400_80170E28[10] = {
#include "assets/actor_342400_animation_0F01C_indices.inc"
};

GpAnimSet D_actor_342400_80170E3C = {
    D_actor_342400_80170C0C,
    D_actor_342400_80170E28,
    { NULL, D_actor_342400_80170990, NULL, NULL, D_actor_342400_80170A80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80170E64[25] = {
#include "assets/actor_342400_animation_0F5A0_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80170F90[110] = {
#include "assets/actor_342400_animation_0F5A0_bank4.inc"
};

GpAnimRec D_actor_342400_80171148[153] = {
#include "assets/actor_342400_animation_0F5A0_records.inc"
};

u16 D_actor_342400_801713AC[10] = {
#include "assets/actor_342400_animation_0F5A0_indices.inc"
};

GpAnimSet D_actor_342400_801713C0 = {
    D_actor_342400_80171148,
    D_actor_342400_801713AC,
    { NULL, D_actor_342400_80170E64, NULL, NULL, D_actor_342400_80170F90, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801713E8[2] = {
#include "assets/actor_342400_animation_0F6A0_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80171400[7] = {
#include "assets/actor_342400_animation_0F6A0_bank4.inc"
};

GpAnimRec D_actor_342400_8017141C[36] = {
#include "assets/actor_342400_animation_0F6A0_records.inc"
};

u16 D_actor_342400_801714AC[10] = {
#include "assets/actor_342400_animation_0F6A0_indices.inc"
};

GpAnimSet D_actor_342400_801714C0 = {
    D_actor_342400_8017141C,
    D_actor_342400_801714AC,
    { NULL, D_actor_342400_801713E8, NULL, NULL, D_actor_342400_80171400, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801714E8[2] = {
#include "assets/actor_342400_animation_0F7A0_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80171500[7] = {
#include "assets/actor_342400_animation_0F7A0_bank4.inc"
};

GpAnimRec D_actor_342400_8017151C[36] = {
#include "assets/actor_342400_animation_0F7A0_records.inc"
};

u16 D_actor_342400_801715AC[10] = {
#include "assets/actor_342400_animation_0F7A0_indices.inc"
};

GpAnimSet D_actor_342400_801715C0 = {
    D_actor_342400_8017151C,
    D_actor_342400_801715AC,
    { NULL, D_actor_342400_801714E8, NULL, NULL, D_actor_342400_80171500, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801715E8[24] = {
#include "assets/actor_342400_animation_0FBD0_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80171708[69] = {
#include "assets/actor_342400_animation_0FBD0_bank4.inc"
};

GpAnimRec D_actor_342400_8017181C[112] = {
#include "assets/actor_342400_animation_0FBD0_records.inc"
};

u16 D_actor_342400_801719DC[10] = {
#include "assets/actor_342400_animation_0FBD0_indices.inc"
};

GpAnimSet D_actor_342400_801719F0 = {
    D_actor_342400_8017181C,
    D_actor_342400_801719DC,
    { NULL, D_actor_342400_801715E8, NULL, NULL, D_actor_342400_80171708, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80171A18[22] = {
#include "assets/actor_342400_animation_10074_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80171B20[87] = {
#include "assets/actor_342400_animation_10074_bank4.inc"
};

GpAnimRec D_actor_342400_80171C7C[129] = {
#include "assets/actor_342400_animation_10074_records.inc"
};

u16 D_actor_342400_80171E80[10] = {
#include "assets/actor_342400_animation_10074_indices.inc"
};

GpAnimSet D_actor_342400_80171E94 = {
    D_actor_342400_80171C7C,
    D_actor_342400_80171E80,
    { NULL, D_actor_342400_80171A18, NULL, NULL, D_actor_342400_80171B20, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80171EBC[14] = {
#include "assets/actor_342400_animation_105CC_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80171F64[99] = {
#include "assets/actor_342400_animation_105CC_bank4.inc"
};

GpAnimRec D_actor_342400_801720F0[186] = {
#include "assets/actor_342400_animation_105CC_records.inc"
};

u16 D_actor_342400_801723D8[10] = {
#include "assets/actor_342400_animation_105CC_indices.inc"
};

GpAnimSet D_actor_342400_801723EC = {
    D_actor_342400_801720F0,
    D_actor_342400_801723D8,
    { NULL, D_actor_342400_80171EBC, NULL, NULL, D_actor_342400_80171F64, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80172414[8] = {
#include "assets/actor_342400_animation_10880_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80172474[54] = {
#include "assets/actor_342400_animation_10880_bank4.inc"
};

GpAnimRec D_actor_342400_8017254C[80] = {
#include "assets/actor_342400_animation_10880_records.inc"
};

u16 D_actor_342400_8017268C[10] = {
#include "assets/actor_342400_animation_10880_indices.inc"
};

GpAnimSet D_actor_342400_801726A0 = {
    D_actor_342400_8017254C,
    D_actor_342400_8017268C,
    { NULL, D_actor_342400_80172414, NULL, NULL, D_actor_342400_80172474, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801726C8[7] = {
#include "assets/actor_342400_animation_10B14_bank1.inc"
};

AnimationPackedRotation D_actor_342400_8017271C[50] = {
#include "assets/actor_342400_animation_10B14_bank4.inc"
};

GpAnimRec D_actor_342400_801727E4[79] = {
#include "assets/actor_342400_animation_10B14_records.inc"
};

u16 D_actor_342400_80172920[10] = {
#include "assets/actor_342400_animation_10B14_indices.inc"
};

GpAnimSet D_actor_342400_80172934 = {
    D_actor_342400_801727E4,
    D_actor_342400_80172920,
    { NULL, D_actor_342400_801726C8, NULL, NULL, D_actor_342400_8017271C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_8017295C[7] = {
#include "assets/actor_342400_animation_10DC4_bank1.inc"
};

AnimationPackedRotation D_actor_342400_801729B0[53] = {
#include "assets/actor_342400_animation_10DC4_bank4.inc"
};

GpAnimRec D_actor_342400_80172A84[83] = {
#include "assets/actor_342400_animation_10DC4_records.inc"
};

u16 D_actor_342400_80172BD0[10] = {
#include "assets/actor_342400_animation_10DC4_indices.inc"
};

GpAnimSet D_actor_342400_80172BE4 = {
    D_actor_342400_80172A84,
    D_actor_342400_80172BD0,
    { NULL, D_actor_342400_8017295C, NULL, NULL, D_actor_342400_801729B0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80172C0C[6] = {
#include "assets/actor_342400_animation_10FD8_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80172C54[42] = {
#include "assets/actor_342400_animation_10FD8_bank4.inc"
};

GpAnimRec D_actor_342400_80172CFC[58] = {
#include "assets/actor_342400_animation_10FD8_records.inc"
};

u16 D_actor_342400_80172DE4[10] = {
#include "assets/actor_342400_animation_10FD8_indices.inc"
};

GpAnimSet D_actor_342400_80172DF8 = {
    D_actor_342400_80172CFC,
    D_actor_342400_80172DE4,
    { NULL, D_actor_342400_80172C0C, NULL, NULL, D_actor_342400_80172C54, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80172E20[2] = {
#include "assets/actor_342400_animation_110F8_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80172E38[11] = {
#include "assets/actor_342400_animation_110F8_bank4.inc"
};

GpAnimRec D_actor_342400_80172E64[40] = {
#include "assets/actor_342400_animation_110F8_records.inc"
};

u16 D_actor_342400_80172F04[10] = {
#include "assets/actor_342400_animation_110F8_indices.inc"
};

GpAnimSet D_actor_342400_80172F18 = {
    D_actor_342400_80172E64,
    D_actor_342400_80172F04,
    { NULL, D_actor_342400_80172E20, NULL, NULL, D_actor_342400_80172E38, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80172F40[4] = {
#include "assets/actor_342400_animation_11234_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80172F70[18] = {
#include "assets/actor_342400_animation_11234_bank4.inc"
};

GpAnimRec D_actor_342400_80172FB8[34] = {
#include "assets/actor_342400_animation_11234_records.inc"
};

u16 D_actor_342400_80173040[10] = {
#include "assets/actor_342400_animation_11234_indices.inc"
};

GpAnimSet D_actor_342400_80173054 = {
    D_actor_342400_80172FB8,
    D_actor_342400_80173040,
    { NULL, D_actor_342400_80172F40, NULL, NULL, D_actor_342400_80172F70, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_8017307C[10] = {
#include "assets/actor_342400_animation_11430_bank1.inc"
};

AnimationPackedRotation D_actor_342400_801730F4[31] = {
#include "assets/actor_342400_animation_11430_bank4.inc"
};

GpAnimRec D_actor_342400_80173170[51] = {
#include "assets/actor_342400_animation_11430_records.inc"
};

u16 D_actor_342400_8017323C[10] = {
#include "assets/actor_342400_animation_11430_indices.inc"
};

GpAnimSet D_actor_342400_80173250 = {
    D_actor_342400_80173170,
    D_actor_342400_8017323C,
    { NULL, D_actor_342400_8017307C, NULL, NULL, D_actor_342400_801730F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80173278[16] = {
#include "assets/actor_342400_animation_1186C_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80173338[83] = {
#include "assets/actor_342400_animation_1186C_bank4.inc"
};

GpAnimRec D_actor_342400_80173484[125] = {
#include "assets/actor_342400_animation_1186C_records.inc"
};

u16 D_actor_342400_80173678[10] = {
#include "assets/actor_342400_animation_1186C_indices.inc"
};

GpAnimSet D_actor_342400_8017368C = {
    D_actor_342400_80173484,
    D_actor_342400_80173678,
    { NULL, D_actor_342400_80173278, NULL, NULL, D_actor_342400_80173338, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801736B4[5] = {
#include "assets/actor_342400_animation_119F8_bank1.inc"
};

AnimationPackedRotation D_actor_342400_801736F0[26] = {
#include "assets/actor_342400_animation_119F8_bank4.inc"
};

GpAnimRec D_actor_342400_80173758[43] = {
#include "assets/actor_342400_animation_119F8_records.inc"
};

u16 D_actor_342400_80173804[10] = {
#include "assets/actor_342400_animation_119F8_indices.inc"
};

GpAnimSet D_actor_342400_80173818 = {
    D_actor_342400_80173758,
    D_actor_342400_80173804,
    { NULL, D_actor_342400_801736B4, NULL, NULL, D_actor_342400_801736F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80173840[5] = {
#include "assets/actor_342400_animation_11BA0_bank1.inc"
};

AnimationPackedRotation D_actor_342400_8017387C[30] = {
#include "assets/actor_342400_animation_11BA0_bank4.inc"
};

GpAnimRec D_actor_342400_801738F4[46] = {
#include "assets/actor_342400_animation_11BA0_records.inc"
};

u16 D_actor_342400_801739AC[10] = {
#include "assets/actor_342400_animation_11BA0_indices.inc"
};

GpAnimSet D_actor_342400_801739C0 = {
    D_actor_342400_801738F4,
    D_actor_342400_801739AC,
    { NULL, D_actor_342400_80173840, NULL, NULL, D_actor_342400_8017387C, NULL, NULL, NULL },
};

u8 D_actor_342400_801739E8[84] = {
    0,
    0,
    0,
    0,
    196,
    7,
    23,
    128,
    104,
    9,
    23,
    128,
    60,
    14,
    23,
    128,
    192,
    19,
    23,
    128,
    192,
    20,
    23,
    128,
    192,
    21,
    23,
    128,
    240,
    25,
    23,
    128,
    148,
    30,
    23,
    128,
    236,
    35,
    23,
    128,
    160,
    38,
    23,
    128,
    52,
    41,
    23,
    128,
    228,
    43,
    23,
    128,
    248,
    45,
    23,
    128,
    24,
    47,
    23,
    128,
    84,
    48,
    23,
    128,
    80,
    50,
    23,
    128,
    140,
    54,
    23,
    128,
    24,
    56,
    23,
    128,
    192,
    57,
    23,
    128,
    0,
    0,
    0,
    0,
};

Actor3424002MessageEntry D_actor_342400_80173A3C[3] = {
    { 2004, { .call0 = func_actor_342400_80169620 } },
    { 2011, { .call1 = func_actor_342400_801695C0 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_342400_80173A54[2] = {
    { 1, 96, func_actor_342400_80169810, { .model = &D_actor_342400_80170560 } },
    { 257, 96, func_actor_342400_8016978C, { .model = &D_actor_342400_80170560 } },
};

TaskDesc D_actor_342400_80173A6C = { 2, 96, taskKill, { .model = NULL } };

TaskDesc D_actor_342400_80173A78 = { 1, 96, func_actor_342400_8016978C, { .model = &D_actor_342400_80170560 } };

u8 D_actor_342400_80173A84[20] = {
    0,
    1,
    0,
    1,
    0,
    1,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    1,
    0,
};

u8 D_actor_342400_80173A98[20] = {
    5,
    6,
    5,
    6,
    5,
    6,
    6,
    5,
    5,
    5,
    6,
    6,
    5,
    5,
    5,
    6,
    5,
    6,
    5,
    1,
};

u16 D_actor_342400_80173AAC = 0;

extern void* D_800678F0[1];

static void            func_actor_342400_80163354(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, u8 shade);
static void            func_actor_342400_801637DC(Task* arg0);
static void            func_actor_342400_801639A8(Task* arg0);
static __inline__ void enter_state(Task* arg0, s32 state);
static __inline__ void update_color(void* enemy, GfxCoord* coord);
static __inline__ s16  take_hit(Task* arg0);
static __inline__ void update_rotation(Task* arg0);
static __inline__ void calc_push(Task* arg0, GfxCoord* coord, GpRec18* rec, SVECTOR* out);
static __inline__ void set_state(Task* arg0, s32 state);
static __inline__ s32  take_request(Task* arg0);
static __inline__ s32  is_hit(Task* arg0);
static void            func_actor_342400_801664C4(Task* arg0);
static __inline__ s16  take_hit_nibble3(Task* arg0);
static __inline__ void set_state_s16(Task* arg0, s16 state);

/// Draws a flat textured quad at height `height` spanning the model parts
/// `firstJoint` and `secondJoint`, `width` wide on each side of the line
/// between them, tinted grey by `shade`. The working set lives in a frame
/// carved off the scratchpad and released again; nothing is drawn when the
/// two parts are the same or the quad is off screen.
static void func_actor_342400_80163354(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, u8 shade)
{
    ActorsShared80163354Scratch* s;
    s16                          angle;
    GfxCoord*                    secondCoord;
    GfxCoord*                    firstCoord;
    s32                          offset0;
    s32                          offset1;
    s32                          offset2;
    s32                          offset3;
    GfxCoord*                    coords;
    POLY_FT4*                    poly;

    coords      = task->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (ActorsShared80163354Scratch*)SCRATCH_PUSH_BYTES(sizeof(ActorsShared80163354Scratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->first.vy                = (s16)height;
        s->second.vy               = (s16)height;
        s->first.vx                = s->firstMatrix.t[0];
        s->first.vz                = s->firstMatrix.t[2];
        s->second.vx               = s->secondMatrix.t[0];
        s->second.vz               = s->secondMatrix.t[2];
        angle                      = ratan2(s->second.vx - s->first.vx, s->second.vz - s->first.vz);
        s->halfX                   = (s->first.vx - s->second.vx) / 2;
        s->halfZ                   = (s->first.vz - s->second.vz) / 2;
        offset0                    = rcos(angle) * width;
        s->corner0.vy              = (s16)height;
        s->corner0.vx              = s->halfX + (s->first.vx - (offset0 >> 0xC));
        s->corner0.vz              = s->halfZ + (s->first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1                    = rcos(angle) * width;
        s->corner1.vy              = (s16)height;
        s->corner1.vx              = s->halfX + (s->first.vx + (offset1 >> 0xC));
        s->corner1.vz              = s->halfZ + (s->first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2                    = rcos(angle) * width;
        s->corner2.vy              = (s16)height;
        s->corner2.vx              = (s->second.vx - (offset2 >> 0xC)) - s->halfX;
        s->corner2.vz              = (s->second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - s->halfZ;
        offset3                    = rcos(angle) * width;
        s->corner3.vy              = (s16)height;
        s->corner3.vx              = (s->second.vx + (offset3 >> 0xC)) - s->halfX;
        s->corner3.vz              = (s->second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - s->halfZ;
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = (u8*)poly + 0x28;
            setlen(poly, 9);
            poly->code            = 0x2E;
            PRIM_XY_WORD(poly, 0) = s->screen0;
            PRIM_XY_WORD(poly, 1) = s->screen1;
            PRIM_XY_WORD(poly, 2) = s->screen2;
            PRIM_XY_WORD(poly, 3) = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC)) / sizeof(*gGpuCurrentOt)]), poly);
        }
        SCRATCH_POP_BYTES(sizeof(ActorsShared80163354Scratch));
    }
}

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` copies into
 * its effect's `TmdObject`. It is declared as a one-element array for the
 * same reason as in `actor_400500`: as a bare scalar, GCC 2.8.1 decides the
 * store cannot alias the `TmdObject` loads and sinks it past them. */

static void func_actor_342400_801637DC(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* src;
    TmdObject* src2;

    D_800678F0[0] = &D_actor_342400_8016CB6C;
    eff           = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[6], 0x200, NULL);
    if (eff != NULL) {
        src        = arg0->extra.tmd;
        dst        = eff->task->extra.tmd;
        dst->tpage = src->tpage;
        dst->clut  = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((Gp_LcgState >> 16) & 1) {
        D_800678F0[0] = &D_actor_342400_8016D210;
        eff2          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[8], 0x200, NULL);
    } else {
        D_800678F0[0] = &D_actor_342400_8016D780;
        eff2          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[2], 0x200, NULL);
    }
    if (eff2 != NULL) {
        src2        = arg0->extra.tmd;
        dst2        = eff2->task->extra.tmd;
        dst2->tpage = src2->tpage;
        dst2->clut  = src2->clut;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[3], 0x200, NULL);
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[4], 0x200, NULL);
}

/// Turns model parts 5, 4 and 3 about Y by a third of `field_424` each: reads
/// each part's rotation back as Euler angles, adds to the yaw, rebuilds the
/// 3x3 and marks the coordinate dirty.
///
/// Each block keeps its own part pointer, and the identity goes through a
/// mix of the stack matrix and `ident` (words 2 and 4 through the pointer):
/// one shared part pointer comes out a saved register short.
static void func_actor_342400_801639A8(Task* arg0)
{
    SVECTOR          rot;
    OverlayMat       mtx;
    GpMtxWords*      ident;
    Actor341700Work* work;
    GfxCoord*        coords;
    MATRIX*          m5;
    MATRIX*          m4;
    MATRIX*          m3;

    work   = (Actor341700Work*)arg0->work;
    ident  = &mtx.ident;
    coords = arg0->extra.tmd->coords;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m5                = &coords[5].coord;
    Gp_MtxToEuler(m5, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m5->m[0][0]            = (u16)mtx.mat.m[0][0];
    m5->m[0][1]            = (u16)mtx.mat.m[0][1];
    m5->m[0][2]            = (u16)mtx.mat.m[0][2];
    m5->m[1][0]            = (u16)mtx.mat.m[1][0];
    m5->m[1][1]            = (u16)mtx.mat.m[1][1];
    m5->m[1][2]            = (u16)mtx.mat.m[1][2];
    m5->m[2][0]            = (u16)mtx.mat.m[2][0];
    m5->m[2][1]            = (u16)mtx.mat.m[2][1];
    m5->m[2][2]            = (u16)mtx.mat.m[2][2];
    coords[5].composeStamp = GRAPHICS_COORD_DIRTY;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m4                = &coords[4].coord;
    Gp_MtxToEuler(m4, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m4->m[0][0]            = (u16)mtx.mat.m[0][0];
    m4->m[0][1]            = (u16)mtx.mat.m[0][1];
    m4->m[0][2]            = (u16)mtx.mat.m[0][2];
    m4->m[1][0]            = (u16)mtx.mat.m[1][0];
    m4->m[1][1]            = (u16)mtx.mat.m[1][1];
    m4->m[1][2]            = (u16)mtx.mat.m[1][2];
    m4->m[2][0]            = (u16)mtx.mat.m[2][0];
    m4->m[2][1]            = (u16)mtx.mat.m[2][1];
    m4->m[2][2]            = (u16)mtx.mat.m[2][2];
    coords[4].composeStamp = GRAPHICS_COORD_DIRTY;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m3                = &coords[3].coord;
    Gp_MtxToEuler(m3, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m3->m[0][0]            = (u16)mtx.mat.m[0][0];
    m3->m[0][1]            = (u16)mtx.mat.m[0][1];
    m3->m[0][2]            = (u16)mtx.mat.m[0][2];
    m3->m[1][0]            = (u16)mtx.mat.m[1][0];
    m3->m[1][1]            = (u16)mtx.mat.m[1][1];
    m3->m[1][2]            = (u16)mtx.mat.m[1][2];
    m3->m[2][0]            = (u16)mtx.mat.m[2][0];
    m3->m[2][1]            = (u16)mtx.mat.m[2][1];
    m3->m[2][2]            = (u16)mtx.mat.m[2][2];
    coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Main enemy init. Allocates the 0x454-byte `Actor341700Work`, points the
/// model at the light / color matrices inside it, runs the animation context,
/// links the collision objects and the enemy's list node, and enters state 2
/// for spawn kind 1 (low nibble of `spawnArg1`), state 1 otherwise. The root
/// coord is lifted by 0x3C and its translation kept as the spawn position.
///
/// `one` is a separate variable set before `Gp_IncStateF0Ref`: the ROM holds
/// the constant in `$s0`, which GCC only picks for a pseudo that crosses a
/// call (sched2 then sinks the `li` below the `jal`).
static void func_actor_342400_80163C58(Task* task)
{
    GpEnemy*         enemy;
    GfxCoord*        root;
    Actor341700Work* work;
    TmdObject*       obj;
    Actor341700Work* w;
    GpEnemy*         e;
    GfxCoord*        coord;
    Actor341700Work* w2;
    Actor341700Work* w3;
    Actor341700Work* w4;
    s32              one;

    enemy      = task->spawnArg2.pointer;
    root       = task->extra.tmd->coords;
    task->work = memCalloc(0x454, 0);
    work       = (Actor341700Work*)task->work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    func_actor_342400_801692E8();
    obj                   = task->extra.tmd;
    w                     = (Actor341700Work*)task->work;
    e                     = task->spawnArg2.pointer;
    coord                 = obj->coords;
    task->msgTable        = D_actor_342400_80173A3C;
    obj->lightMtx         = &w->lightMtx;
    obj->colorMtx         = &w->colorMtx;
    e->param              = &D_actor_342400_80170588;
    e->recs               = w->rec_2EC;
    w->eff_3FC.coord      = &task->extra.tmd->coords[1];
    w->eff_3FC.spawnArgLo = 0x140;
    w->eff_3FC.spawnArgHi = 2;
    e->hp = e->hpMax = D_actor_342400_80170588.hpMax;
    func_800B3F84(&w->anim, D_actor_342400_801739E8, obj, w->field_21C, &w->slot_B4);
    w2            = (Actor341700Work*)task->work;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = 2;
    func_actor_342400_80165CC0(task);
    coord->parent = &gGfxViewCoord;
    func_actor_342400_80165E4C(task);
    w->field_7A = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0x800;
    enemy       = task->spawnArg2.pointer;
    Gp_LinkNode(&enemy->node);
    enemy->field_4            = &task->extra.tmd->coords->coord;
    enemy->field_48           = 0;
    enemy->bodyPos.vx         = 0;
    enemy->bodyPos.vy         = 0;
    enemy->bodyPos.vz         = 0;
    enemy->coord              = &task->extra.tmd->coords[1];
    enemy->node.state.b.flags = 4;
    one                       = 1;
    (Gp_IncStateF0Ref)(0);
    if ((task->spawnArg1.value & 0xF) == one) {
        w3            = (Actor341700Work*)task->work;
        task->state   = 2;
        w3->field_420 = 0;
        w3->field_422 = 0;
    } else {
        w4            = (Actor341700Work*)task->work;
        task->state   = one;
        w4->field_420 = 0;
        w4->field_422 = 0;
    }
    work->field_80    = root->coord.t[0];
    root->coord.t[1] -= 0x3C;
    work->field_82    = root->coord.t[1];
    work->field_84    = root->coord.t[2];
}

/// Variant of `func_actor_342400_80163C58`'s init: also destroys the enemy
/// when bit 16 of `spawnArg1` is set,
/// sets bit 0x80 of the model's `field_C` for spawn kind 2, and enters state 6
/// with `field_451` set and the collision flags 0x8000 / 0x4000 cleared on
/// `obj_2AC` / `obj_2CC`.
///
/// `two` is a variable for the same reason as `one` in the sibling: the ROM
/// keeps the constant in `$s5` across the calls. `kind` has to be its own
/// variable too - masking `flags` in place reuses `$v1` for the result.
static void func_actor_342400_80163E70(Task* task)
{
    TmdObject*       model;
    GpEnemy*         enemy;
    GfxCoord*        root;
    Actor341700Work* work;
    TmdObject*       obj;
    Actor341700Work* w;
    GpEnemy*         e;
    GfxCoord*        coord;
    Actor341700Work* w2;
    Actor341700Work* w3;
    GpEnemy*         e2;
    s32              flags;
    s32              kind;
    s32              two;

    model      = task->extra.tmd;
    enemy      = task->spawnArg2.pointer;
    root       = model->coords;
    task->work = memCalloc(0x454, 0);
    work       = (Actor341700Work*)task->work;
    if (work == NULL) {
        goto destroy;
    }
    func_actor_342400_801692E8();
    flags = task->spawnArg1.value;
    if ((flags >> 16) & 1) {
    destroy:
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    kind = flags & 0xF;
    two  = 2;
    if (kind == two) {
        model->flags |= 0x80;
    }
    obj                   = task->extra.tmd;
    w                     = (Actor341700Work*)task->work;
    e                     = task->spawnArg2.pointer;
    coord                 = obj->coords;
    task->msgTable        = D_actor_342400_80173A3C;
    obj->lightMtx         = &w->lightMtx;
    obj->colorMtx         = &w->colorMtx;
    e->param              = &D_actor_342400_80170588;
    e->recs               = w->rec_2EC;
    w->eff_3FC.coord      = &task->extra.tmd->coords[1];
    w->eff_3FC.spawnArgLo = 0x140;
    w->eff_3FC.spawnArgHi = two;
    e->hp = e->hpMax = D_actor_342400_80170588.hpMax;
    func_800B3F84(&w->anim, D_actor_342400_801739E8, obj, w->field_21C, &w->slot_B4);
    w2            = (Actor341700Work*)task->work;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = two;
    func_actor_342400_80165CC0(task);
    coord->parent = &gGfxViewCoord;
    func_actor_342400_80165E4C(task);
    w->field_7A = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0x800;
    (Gp_IncStateF0Ref)(0);
    e2 = task->spawnArg2.pointer;
    Gp_LinkNode(&e2->node);
    e2->field_4            = &task->extra.tmd->coords->coord;
    e2->field_48           = 0;
    e2->bodyPos.vx         = 0;
    e2->bodyPos.vy         = 0;
    e2->bodyPos.vz         = 0;
    e2->coord              = &task->extra.tmd->coords[1];
    e2->node.state.b.flags = 1;
    work->field_80         = root->coord.t[0];
    root->coord.t[1]      -= 0x3C;
    work->field_82         = root->coord.t[1];
    work->field_84         = root->coord.t[2];
    work->field_451        = 1;
    work->obj_2AC.flags   &= 0x7FFF;
    work->obj_2CC.flags   &= 0xBFFF;
    w3                     = (Actor341700Work*)task->work;
    task->state            = 6;
    w3->field_420          = 0;
    w3->field_422          = 0;
}

/// Moves the task to `state` with a fresh state machine.
static __inline__ void enter_state(Task* arg0, s32 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    arg0->state  = state;
    w->field_420 = 0;
    w->field_422 = 0;
}

/// Colours `enemy` from `coord`'s world position through a 0x10-byte
/// `VECTOR` taken off `G_SCRATCH_HEAD`. Inlined so each scratch-head access
/// keeps its own `lui` instead of sharing a CSE'd register.
static __inline__ void update_color(void* enemy, GfxCoord* coord)
{
    VECTOR* block = (VECTOR*)(SCRATCH_HEAD(u8) - 0x10);

    block->vx            = coord->workm.t[0];
    block->vy            = coord->workm.t[1];
    SCRATCH_HEAD(VECTOR) = block;
    block->vz            = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    SCRATCH_POP_BYTES(0x10);
}

/// Message 0x2C00 (see `field_44C`) consumes the message and restarts the
/// state machine: low nibble 2 enters state 3 at state index 10 unless
/// `field_438` is set, low nibble 3 enters state 7. Returns 1 when it did, so
/// the caller skips this frame's state handler.
///
/// Each arm has to `return 1` on its own, with `return 0` after them: that
/// leaves a `hit = 0` block between the second arm and the join, so jump2
/// cannot cross-jump the first arm's `field_422` store into the second's
/// (dbr later steals the `hit = 0` into the branch delay slots and the block
/// disappears). A flag set to 0 up front and to 1 in each arm cross-jumps.
static __inline__ s16 take_hit(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    Actor341700Work* w2;

    if ((work->field_44C & 0xF) == 2) {
        if (work->field_438 == 0) {
            work->field_44C = 0;
            enter_state(arg0, 3);
            w2            = (Actor341700Work*)arg0->work;
            w2->field_420 = 10;
            w2->field_422 = 0;
            return 1;
        }
    } else if ((work->field_44C & 0xF) == 3) {
        work->field_44C = 0;
        enter_state(arg0, 7);
        return 1;
    }
    return 0;
}

/// Wraps the pitch / heading / roll at 0x78..0x7C to 12 bits and rebuilds the
/// model root's rotation from them (Z, then X, then the heading) in a matrix
/// taken off `G_SCRATCH_HEAD`, copying the 3x3 into the root coordinate.
static __inline__ void update_rotation(Task* arg0)
{
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    MATRIX*          m     = (MATRIX*)(SCRATCH_HEAD(u8) - 0x20);
    GfxCoord*        coord = arg0->extra.tmd->coords;
    MATRIX*          dst;

    work->field_78      &= 0xFFF;
    work->field_7A      &= 0xFFF;
    work->field_7C      &= 0xFFF;
    MATRIX_PAIR(m, 0, 0) = 0x1000;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1) = 0x1000;
    MATRIX_PAIR(m, 2, 0) = 0;
    m->m[2][2]           = 0x1000;
    SCRATCH_HEAD(MATRIX) = m;
    RotMatrixZ(work->field_7C, m);
    RotMatrixX(work->field_78, m);
    RotMatrixY(work->field_7A, m);
    dst          = &coord->coord;
    dst->m[0][0] = m->m[0][0];
    dst->m[0][1] = m->m[0][1];
    dst->m[0][2] = m->m[0][2];
    dst->m[1][0] = m->m[1][0];
    dst->m[1][1] = m->m[1][1];
    dst->m[1][2] = m->m[1][2];
    dst->m[2][0] = m->m[2][0];
    dst->m[2][1] = m->m[2][1];
    SCRATCH_POP_BYTES(0x20);
    dst->m[2][2] = m->m[2][2];
}

/// Per-frame callback for the main enemy, the eleven-state counterpart of
/// `func_actor_342400_801670C0`. In mode 0 it aims at the nearest actor
/// (`func_actor_342400_801662EC`), lets a pending hit (`take_hit`) replace the state
/// handler, eases `field_424` toward zero, rebuilds the root rotation, and
/// then picks the next state: the `field_448` request once dead, state 4 when
/// dead, 8 / 9 for messages 4 / 5 while `field_438` is clear.
static void func_actor_342400_801640B0(Task* arg0)
{
    GpEnemy*         enemy = arg0->spawnArg2.pointer;
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable11  sp    = D_actor_342400_80161EA8;
    s32              cur;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            func_actor_342400_801662EC(arg0);
            if (take_hit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            func_actor_342400_80165CC0(arg0);
            cur             = (u16)work->field_424;
            work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
            func_actor_342400_801639A8(arg0);
            if (work->field_432 == 1) {
                func_actor_342400_80169654(arg0, 6, (SVECTOR3*)&work->field_98);
            }
            update_rotation(arg0);
            func_actor_342400_801653DC(arg0, 0);
            if (work->field_44A != 0) {
                work->field_44A--;
            }
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->hp <= 0) {
                enter_state(arg0, work->field_448);
            }
            if (work->field_438 == 0 && enemy->hp <= 0) {
                enter_state(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                enter_state(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                enter_state(arg0, 9);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            update_color(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            func_actor_342400_80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
            func_actor_342400_80163354(arg0, 1, 7, 0x80, 0, 0xFF);
            func_actor_342400_80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            return;
    }
}

/// Starts a walk: requests animation 7, plays the enemy's sound 1, draws a
/// random 0..0x7FF into `field_410` and advances the sub-state. The animation
/// speed and the turn step `field_436` grow with the distance to the nearer
/// player actor in `field_43A`, in bands of 1000.
static void func_actor_342400_8016454C(Task* arg0)
{
    Actor341700Work* work;
    s32              soundId;
    s32              pan;
    s16              step;

    work            = (Actor341700Work*)arg0->work;
    work->field_426 = 8;
    work->field_418 = 7;
    work->field_41C = 0x10;
    work->field_414 = 1;
    work->field_422++;
    soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0001;
    pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_410 = (Gp_LcgState >> 0x10) & 0x7FF;
    if (work->field_43A < 1000) {
        work->field_41C = 0x10;
        work->field_436 = 0x10;
        return;
    }
    if (work->field_43A < 2000) {
        work->field_41C = 0x14;
        step            = 0x12;
    } else if (work->field_43A < 3000) {
        work->field_41C = 0x18;
        step            = 0x14;
    } else if (work->field_43A < 4000) {
        work->field_41C = 0x1C;
        step            = 0x16;
    } else if (work->field_43A < 5000) {
        work->field_41C = 0x20;
        step            = 0x18;
    } else {
        work->field_41C = 0x40;
        step            = 0x20;
    }
    work->field_436 = step;
}

static void func_actor_342400_801646B8(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    s16              dist;
    s16              limit;
    s16              step;
    s16              angle;
    s16              speed;
    s32              soundId;
    s32              pan;

    dist = work->field_43A;
    if (dist < 1000) {
        limit = 0x10;
        step  = 0x10;
    } else if (dist < 2000) {
        step  = 0x12;
        limit = 0x14;
    } else if (dist < 3000) {
        step  = 0x14;
        limit = 0x18;
    } else if (dist < 4000) {
        step  = 0x16;
        limit = 0x1C;
    } else if (dist < 5000) {
        limit = 0x20;
        step  = 0x18;
    } else {
        limit = 0x40;
        step  = 0x20;
    }
    if (work->field_41C < limit) {
        work->field_41C = limit;
        work->field_436 = step;
    }
    func_actor_342400_801698D4(arg0, work->field_436);
    speed                                 = func_actor_342400_80169728(arg0, -0x10);
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (func_actor_342400_8016974C(arg0)) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0001;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_43A < work->field_410 + 2000 && (work->field_43A < 1500 || work->field_44A == 0) &&
        (u16)(((work->field_444 + 0x800) & 0xFFF) - 0x200) > 0xC00) {
        work->field_422++;
    }
}

static void func_actor_342400_801648E4(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    GfxCoord*        root = arg0->extra.tmd->coords;
    MATRIX           local;
    s16              angle;
    s32              soundId;
    s32              pan;
    s16              facing;
    s16              speed;

    if ((s16)++work->field_412 < 40) {
        if ((s16)func_actor_342400_80169518(arg0)) {
            return;
        }
    } else {
        work->field_438 = 1;
    }
    if ((s16)work->field_412 == 43) {
        GfxCoord* coords = arg0->extra.tmd->coords;
        SVECTOR*  v;

        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&gGfxViewCoord);
        coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&coords[6]);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[6].workm, &local);
        v                      = &work->field_98;
        v->vx                  = local.t[0];
        v->vy                  = local.t[1];
        v->vz                  = local.t[2];
        coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->field_412 >= 43 && work->field_412 <= 46) {
        work->field_432 = 1;
    } else {
        work->field_432 = 0;
    }
    if ((s16)work->field_412 == 46) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0005;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_412 == 45) {
        facing = (work->field_444 + 0x800) & 0xFFF;
        if (facing < 0x300) {
            work->field_40C = (facing + work->field_7A) & 0xFFF;
        } else if (facing >= 0xD00) {
            work->field_40C = (facing + work->field_7A) & 0xFFF;
        } else {
            work->field_40C = work->field_7A;
        }
    }
    if (work->field_412 >= 45 && work->field_412 <= 53) {
        angle                                 = work->field_40C;
        speed                                 = -250;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->obj_3AC.flags                  |= 0x8000;
    } else {
        work->obj_3AC.flags &= 0x7FFF;
    }
    if (work->field_412 >= 45 && work->field_412 <= 48 && work->field_43A < 0x171) {
        Actor341700Work* w;

        work->field_428      = 0;
        work->field_42A      = -200;
        w                    = (Actor341700Work*)arg0->work;
        w->field_426         = 2;
        w->field_41C         = 0x10;
        w->field_418         = 0x10;
        w->field_414         = 1;
        work->field_432      = 0;
        work->obj_3AC.flags &= 0x7FFF;
        work->field_422     += 2;
        return;
    }
    if ((s16)work->field_412 >= 47) {
        root->coord.t[1]     += work->field_42A;
        work->obj_2CC.pos.vy += work->field_42A;
        work->field_428      += 30;
        work->field_42A      += work->field_428;
        if (root->coord.t[1] >= (s16)work->field_92) {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_426         = 2;
            w->field_41C         = 0x10;
            w->field_418         = 0x12;
            w->field_414         = 1;
            root->coord.t[1]     = (s16)work->field_92;
            work->obj_2CC.pos.vy = 0;
            work->field_412      = 0;
            work->field_422++;
        }
    }
}

/// Plays sound 4 on the first frame and, once the hit flags are set, turns
/// the enemy around, draws a 0x5A..0xD9 cooldown into `field_44A`, requests
/// animation 0xD and moves the task to state 1.
static void func_actor_342400_80164CA4(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* work3;
    s32              soundId;
    s32              pan;
    u32              rand;

    work = (Actor341700Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (func_actor_342400_8016974C(arg0) != 0) {
        work->field_438  = 0;
        rand             = Gp_LcgState * 5 + 0x71357911;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work->field_7A  += 0x800;
        work2            = (Actor341700Work*)arg0->work;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 2;
        work3            = (Actor341700Work*)arg0->work;
        Gp_LcgState      = rand;
        arg0->state      = 1;
        work3->field_420 = 0;
        work3->field_422 = 0;
    }
}

/// Falls along the heading in `field_40C`: moves the root 0xC8 units a frame,
/// adds the accelerating drop `field_42A` to its Y and to the second hit
/// body, and once the root reaches the ground height saved in `field_92`
/// requests landing animation 0x13 and advances the sub-state.
static void func_actor_342400_80164DD4(Task* arg0)
{
    Actor341700Work* work;
    s16              angle;
    GfxCoord*        coord;
    Actor341700Work* anim;
    s32              speed;
    s32              dx;

    work                                  = (Actor341700Work*)arg0->work;
    angle                                 = work->field_40C;
    coord                                 = arg0->extra.tmd->coords;
    dx                                    = rsin(angle) << 4;
    speed                                 = 0xC8;
    arg0->extra.tmd->coords->coord.t[0]  += (dx * speed) >> 16;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 16;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]                    += work->field_42A;
    work->obj_2CC.pos.vy                 += work->field_42A;
    work->field_428                      += 0xE;
    work->field_42A                      += work->field_428;
    if (coord->coord.t[1] >= (s16)work->field_92) {
        anim                 = (Actor341700Work*)arg0->work;
        anim->field_426      = 2;
        anim->field_41C      = 0x10;
        anim->field_418      = 0x13;
        anim->field_414      = 1;
        coord->coord.t[1]    = (s16)work->field_92;
        work->obj_2CC.pos.vy = 0;
        work->field_412      = 0;
        work->field_422++;
    }
}

/// Per-frame callback shaped like `func_actor_342400_80165FC0`, with a
/// one-entry handler table. `Gp_StateF0.field_4` 2 hides the model; 0 runs the state
/// handler and the follow-up steps, then moves the task to state 4 when
/// `field_448` requests it and the enemy is out of HP; 0 and 1 both colour
/// it, run `func_actor_342400_80163354` for three part pairs and unhide it. The work block is reloaded through its own local
/// for the state reset, as the original does.
static void func_actor_342400_80164F3C(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2.pointer;
    GfxCoord*        coord = obj->coords;
    TaskFunc         sp[1] = { func_actor_342400_8016A494 };

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp[(s16)work->field_420](arg0);
            func_actor_342400_801653DC(arg0, 1);
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->hp <= 0) {
                Actor341700Work* w = (Actor341700Work*)arg0->work;

                arg0->state  = work->field_448;
                w->field_420 = 0;
                w->field_422 = 0;
            }
            func_actor_342400_80165CC0(arg0);
            if (work->field_432 == 1) {
                func_actor_342400_80169654(arg0, 6, (SVECTOR3*)&work->field_80);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            update_color(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            func_actor_342400_80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
            func_actor_342400_80163354(arg0, 1, 7, 0x80, 0, 0xFF);
            func_actor_342400_80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~0x80;
            return;
    }
}

/// Drops the model back to the ground: eases the pitch latched in
/// `field_434` back to zero while keeping the heading, adds the accelerating
/// drop to the root Y, and on landing requests animation 0xC and advances the
/// sub-state.
static void func_actor_342400_8016513C(Task* arg0)
{
    Actor341700Work* work;
    GfxCoord*        coord;
    OverlayMat       rot;
    OverlayMat*      src;
    MATRIX*          dst;
    Actor341700Work* anim;

    work               = (Actor341700Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    src                = &rot;
    src->ident.m00_m01 = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    work->field_434   += -work->field_434 >> 2;
    RotMatrixX(work->field_434, &src->mat);
    RotMatrixY(work->field_7A, &src->mat);
    dst                = &coord->coord;
    dst->m[0][0]       = src->mat.m[0][0];
    dst->m[0][1]       = src->mat.m[0][1];
    dst->m[0][2]       = src->mat.m[0][2];
    dst->m[1][0]       = src->mat.m[1][0];
    dst->m[1][1]       = src->mat.m[1][1];
    dst->m[1][2]       = src->mat.m[1][2];
    dst->m[2][0]       = src->mat.m[2][0];
    dst->m[2][1]       = src->mat.m[2][1];
    dst->m[2][2]       = src->mat.m[2][2];
    work->field_428   += 2;
    work->field_42A   += work->field_428;
    coord->coord.t[1] += work->field_42A;
    if (coord->coord.t[1] > 0) {
        work->field_412   = 0;
        coord->coord.t[1] = 0;
        anim              = (Actor341700Work*)arg0->work;
        anim->field_41C   = 0x20;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_422++;
    }
}

/// Plays sounds 4 and 3 on the first two frames; once the hit flags are set,
/// moves the task to state 3 with the state machine at state 3.
static void func_actor_342400_801652A0(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* next;
    Actor341700Work* next2;
    u32              soundId;
    s32              pan;

    work = (Actor341700Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId   = (u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0004;
        pan       = Gp_GetObjPan(arg0->extra.tmd->coords) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_412 == 2) {
        soundId   = (u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0003;
        pan       = Gp_GetObjPan(arg0->extra.tmd->coords) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (func_actor_342400_8016974C(arg0)) {
        next             = (Actor341700Work*)arg0->work;
        arg0->state      = 3;
        next->field_420  = 0;
        next->field_422  = 0;
        next2            = (Actor341700Work*)arg0->work;
        next2->field_420 = 3;
        next2->field_422 = 0;
    }
}

/// Push-out of the model from contact record `rec`: how far `coord` sits
/// inside the record's radius (`depth`), along the direction from the
/// record's centre to the root part, carried into grid space.
///
/// `rec` must stay an inline argument: `integrate.c` expands it with
/// `EXPAND_SUM`, giving `(i * 0x18 + work) + 0x2EC` rather than a loop giv.
static __inline__ void calc_push(Task* arg0, GfxCoord* coord, GpRec18* rec, SVECTOR* out)
{
    SVECTOR   pos;
    VECTOR    d;
    VECTOR    n;
    GfxCoord* c2;
    s32       t;
    s32       pen;

    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    c2     = arg0->extra.tmd->coords;
    d.vx   = pos.vx - rec->point.vx;
    d.vy   = 0;
    d.vz   = pos.vz - rec->point.vz;
    pen    = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen    = rec->depth - pen;
    if (pen <= 0) {
        t = 0;
    } else {
        t = pen;
    }
    pen  = t;
    d.vx = c2->workm.t[0] - rec->point.vx;
    d.vy = c2->workm.t[1] - rec->point.vy;
    d.vz = c2->workm.t[2] - rec->point.vz;
    VectorNormal(&d, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Per-frame contact handling for the overlay's enemy. Walks the eight
/// contact records: kind 1 (skipped when `arg1` is set) and kind 3 push the
/// model out, kind 2 applies a hit - damage, status effects and the pending
/// state request in `field_448` - unless `field_40E` is still cooling down.
/// Then ticks the status flags, applies `func_800E0C10`'s collision step
/// (snapping back to `field_60` when it reports a conflict) and moves the
/// root by the combined step and push-out.
static void func_actor_342400_801653DC(Task* arg0, s16 arg1)
{
    GpDeltaScratch   delta;
    SVECTOR          push;
    s16              maxX;
    s16              maxZ;
    s16              stepX;
    s16              stepZ;
    u8               blocked;
    Actor341700Work* work;
    GpEnemy*         enemy;
    GfxCoord*        coord;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s16              tick;
    s32              i;

    stepZ   = 0;
    maxX    = 0;
    maxZ    = 0;
    stepX   = 0;
    blocked = 0;
    work    = (Actor341700Work*)arg0->work;
    coord   = arg0->extra.tmd->coords;
    enemy   = arg0->spawnArg2.pointer;
    SCRATCH_PUSH_BYTES(8);
    work->field_41E = 0;
    for (i = 0; i < 8; i++) {
        switch (work->rec_2EC[i].key & 0xFFFF0000) {
            case 0x10000:
                if (arg1 != 0) {
                    break;
                }
            case 0x30000:
                calc_push(arg0, coord, &work->rec_2EC[i], &push);
                if (ABS(maxX) < ABS(push.vx)) {
                    maxX = push.vx;
                }
                if (ABS(maxZ) < ABS(push.vz)) {
                    maxZ = push.vz;
                }
                break;
            case 0x20000:
                if (work->field_40E == 0) {
                    work->field_41E = 1;
                    dmg             = Gp_ComputeDamage(work->rec_2EC[i].key, work->field_43A, 0, 0);
                    amount          = dmg;
                    work->field_40E = Gp_GetIdParam2(work->rec_2EC[i].key);
                    if (Gp_RollEnemyChance(enemy, work->rec_2EC[i].key, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                    func_800E2C78(enemy, work->rec_2EC[i].key, amount, 0);
                    func_800DA6E8(&enemy->node, amount, 0);
                    enemy->hp -= amount;
                    if (enemy->hp < 0) {
                        enemy->hp = 0;
                    }
                    func_800FDB18(Gp_GetIdParam1(work->rec_2EC[i].key) & 0xFFFF,
                                  &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
                    if (amount >= 0x28) {
                        work->field_448 = 2;
                    } else {
                        work->field_448 = 1;
                    }
                    switch (Gp_GetIdParam0(work->rec_2EC[i].key) & 0xFFFF) {
                        case 0:
                            break;
                        case 1:
                            Gp_SetObjFlag1(enemy);
                            break;
                        case 2:
                            Gp_SetObjFlag2(enemy, work->rec_2EC[i].key, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4(enemy, work->rec_2EC[i].key, 0);
                            break;
                        case 4:
                            work->field_448 = 4;
                            break;
                        case 5:
                            work->field_448 = 2;
                            break;
                        case 6:
                            work->field_448 = 4;
                            break;
                        case 7:
                            work->field_448 = 2;
                            break;
                        case 8:
                            work->field_448 = 3;
                            break;
                        case 9:
                            work->field_448 = 3;
                            break;
                    }
                } else if ((Gp_GetIdParam1(work->rec_2EC[i].key) & 0xFFFF) == 0xD) {
                    func_800FDB18(0xD, &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
                }
                break;
        }
    }

    if (enemy->reactionFlags & 1) {
        enemy->reactionFlags &= 0xFE;
        work->field_448       = 5;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= 0xFD;
        work->field_448       = 3;
    }
    if (enemy->reactionFlags & 0xC) {
        work->field_44E = 1;
        tmp             = Gp_TickObjFlag4(enemy);
        tick            = tmp;
        if (tick != 0) {
            enemy->hp -= tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->field_41E = 1;
            work->field_448 = 2;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
    }

    switch (func_800E0C10(work->rec_2EC, &delta, 8, NULL)) {
        case 0:
            break;
        case 1:
            stepZ = delta.vz.h.hi;
            stepX = delta.vx.w >> 16;
            if (delta.vx.w & 0xFFFF) {
                if (delta.vx.w > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.vz.w & 0xFFFF) {
                if (delta.vz.w > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case 2:
            coord->coord.t[0]   = work->field_60.vx;
            coord->coord.t[2]   = work->field_60.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            blocked             = 1;
            break;
    }

    Gp_ClearRec18Occupied(work->rec_2EC);
    if (work->field_43E != 0) {
        work->field_43E--;
    }
    if (work->field_40E > 0) {
        work->field_40E--;
    }
    if (blocked == 0) {
        work->field_80     += actorPickStep(stepX, maxX >> 3);
        work->field_84     += actorPickStep(stepZ, maxZ >> 3);
        coord->coord.t[0]  += actorPickStep(stepX, maxX >> 3);
        coord->coord.t[2]  += actorPickStep(stepZ, maxZ >> 3);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_POP_BYTES(8);
}

/// Runs the animation request in `field_414` on animation slots 1..8: kind 1
/// blends into animation `field_418` over `field_426` frames, kind 2 resets
/// the slots straight to it, and either records it as applied and moves to
/// kind 3, which counts frames in `field_41A`. Every frame each slot then
/// ticks at speed `field_41C`.
static void func_actor_342400_80165CC0(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* start;
    s32              i;
    s32              j;
    s32              k;

    work = (Actor341700Work*)arg0->work;
    if (work->field_414 == 1) {
        start = work;
        if (start->field_416 == start->field_418) {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].rate = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
        } else {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].rate = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
            start->field_426 = 0;
        }
        goto advance;
    }
    if (work->field_414 == 2) {
        start = work;
        for (j = 1; j < 9; j++) {
            Gp_AnimResetSlot(&start->anim, j, start->field_418);
            (&start->slot_B4)[j].rate = start->field_41C;
        }
    advance:
        start->field_416 = start->field_418;
        work->field_414  = 3;
        work->field_41A  = 0;
    } else if (work->field_414 == 3) {
        work->field_41A++;
    }
    for (k = 1; k < 9; k++) {
        (&work->slot_B4)[k].rate = work->field_41C;
        Gp_AnimTickIndex(&work->anim, k);
    }
}

/// Links the enemy's three hit bodies on model part 1: `obj_2AC` (radius
/// 0x170, over the eight records at `rec_2EC`, bit 0x8000 set),
/// `obj_3AC` (radius 0x170, keyed to the enemy, over the two records at
/// `rec_3CC`, bit 0x8000 clear) and `obj_2CC` (radius 0x224, sharing
/// `rec_2EC`, bit 0x4000 set).
static void func_actor_342400_80165E4C(Task* task)
{
    Actor341700Work* work = (Actor341700Work*)task->work;

    work->obj_2AC.coord    = &task->extra.tmd->coords[1];
    work->obj_2AC.ctx.recs = work->rec_2EC;
    work->obj_2AC.pos.vx   = 0;
    work->obj_2AC.pos.vy   = 0;
    work->obj_2AC.pos.vz   = 0;
    work->obj_2AC.key      = 0x3002C;
    work->obj_2AC.radius   = 0x170;
    work->obj_2AC.flags    = 1;
    Gp_LinkObj(2, &work->obj_2AC);
    Gp_InitRec18Table(work->rec_2EC, 8, 0);
    work->obj_2AC.flags |= 0x8000;

    work->obj_3AC.coord    = &task->extra.tmd->coords[1];
    work->obj_3AC.ctx.recs = work->rec_3CC;
    work->obj_3AC.pos.vx   = 0;
    work->obj_3AC.pos.vy   = 0;
    work->obj_3AC.pos.vz   = 0;
    work->obj_3AC.key      = Gp_PackObjPair(task->spawnArg2.pointer, 0);
    work->obj_3AC.radius   = 0x170;
    work->obj_3AC.flags    = 1;
    Gp_LinkObj(2, &work->obj_3AC);
    Gp_InitRec18Table(work->rec_3CC, 2, 0);
    work->obj_3AC.flags &= 0x7FFF;

    work->obj_2CC.coord    = &task->extra.tmd->coords[1];
    work->obj_2CC.ctx.recs = work->rec_2EC;
    work->obj_2CC.pos.vx   = 0;
    work->obj_2CC.pos.vy   = 0;
    work->obj_2CC.pos.vz   = 0;
    work->obj_2CC.key      = 0x3002C;
    work->obj_2CC.radius   = 0x224;
    work->obj_2CC.flags    = 1;
    Gp_LinkObj(2, &work->obj_2CC);
    work->obj_2CC.flags |= 0x4000;
}

/// Nine state handlers, indexed by `Actor341700Work::field_420`; copied to the
/// stack before dispatch.
static const TaskFuncTable9 D_actor_342400_80161F50 = { {
    func_actor_342400_8016A664,
    func_actor_342400_8016A724,
    func_actor_342400_8016A804,
    func_actor_342400_8016A884,
    func_actor_342400_8016A950,
    func_actor_342400_80166180,
    func_actor_342400_8016A9AC,
    func_actor_342400_8016A9C4,
    func_actor_342400_8016AA08,
} };

/// Per-frame callback of the main enemy. `Gp_StateF0.field_4` 2 hides the model,
/// 0 runs the current state handler (then colours it), 1 only colours it.
/// Unless `field_451` is set, it then runs `func_actor_342400_80163354` for
/// three part pairs.
static void func_actor_342400_80165FC0(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable9   sp    = D_actor_342400_80161F50;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            update_color(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                func_actor_342400_80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
                func_actor_342400_80163354(arg0, 1, 7, 0x80, 0, 0xFF);
                func_actor_342400_80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// Death shrink: restores the root matrix saved in `savedRootMtx`, scales it
/// on Y by `field_430` (0x40 smaller each frame), spawns effect 0x600A5 on
/// frame 4, sets the enemy's light mode 2 on frame 16, and after frame 32
/// hides the model and advances the state.
static void func_actor_342400_80166180(Task* arg0)
{
    Actor341700Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    VECTOR           scale;
    OverlayMat       m;
    GpMtxWords*      ident;
    SVECTOR          ofs;

    work             = (Actor341700Work*)arg0->work;
    ident            = &m.ident;
    obj              = arg0->extra.tmd;
    coord            = obj->coords;
    work->field_430 -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = (s16)work->field_430;
    scale.vz         = 0x1000;
    coord->coord     = work->savedRootMtx;
    m.ident.m00_m01  = 0x1000;
    m.ident.m02_m10  = 0;
    ident->m11_m12   = 0x1000;
    m.ident.m20_m21  = 0;
    ident->m22       = 0x1000;
    ScaleMatrix(&m.mat, &scale);
    MulMatrix(&coord->coord, &m.mat);
    if ((s16)++work->field_412 == 4) {
        ofs.vx = 0;
        ofs.vy = 0;
        ofs.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 3, &ofs);
    }
    if ((s16)work->field_412 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2.pointer, 2);
    }
    if ((s16)work->field_412 > 0x20) {
        obj->flags |= 0x80;
        work->field_420++;
    }
}

/// Aims at the nearer of the two player actors: saves the root position in
/// `field_60`, stores the offset to that actor in `field_88`..`field_8C` and
/// its horizontal distance in `field_43A`, and its heading relative to
/// `field_7A` in `field_444`. Nothing but the position is updated while
/// player slot 0 is empty.
static void func_actor_342400_801662EC(Task* arg0)
{
    Actor341700Work* work;
    GfxCoord*        coord;
    GfxCoord*        other;
    Task*            player;
    SVECTOR          d0;
    SVECTOR          d1;
    s32              dist;
    s32              dist2;

    work              = (Actor341700Work*)arg0->work;
    coord             = arg0->extra.tmd->coords;
    player            = Gp_ActorSlots[0];
    work->field_60.vx = coord->coord.t[0];
    work->field_60.vy = coord->coord.t[1];
    work->field_60.vz = coord->coord.t[2];
    if (player != NULL) {
        other = player->extra.tmd->coords;
        d0.vx = other->coord.t[0] - coord->coord.t[0];
        d0.vy = other->coord.t[1] - coord->coord.t[1];
        d0.vz = other->coord.t[2] - coord->coord.t[2];
        dist  = SquareRoot0(d0.vx * d0.vx + d0.vz * d0.vz);
        if (Gp_ActorSlots[1] != NULL) {
            other = Gp_ActorSlots[1]->extra.tmd->coords;
            d1.vx = other->coord.t[0] - coord->coord.t[0];
            d1.vy = other->coord.t[1] - coord->coord.t[1];
            d1.vz = other->coord.t[2] - coord->coord.t[2];
            dist2 = SquareRoot0(d1.vx * d1.vx + d1.vz * d1.vz);
            if (dist2 < dist) {
                dist  = dist2;
                d0.vx = d1.vx;
                d0.vy = d1.vy;
                d0.vz = d1.vz;
            }
        }
        // The loop notes keep VectorNormalSS's argument setup below these stores.
        do {
            work->field_88  = d0.vx;
            work->field_8A  = d0.vy;
            work->field_8C  = d0.vz;
            work->field_43A = dist;
        } while (0);
        VectorNormalSS(&d0, &d0);
        work->field_444 = (ratan2(d0.vx, d0.vz) - work->field_7A) & 0xFFF;
    }
}

/// Moves the state machine to `state` at sub-state 0, reloading the work
/// block through the task as the original does.
static __inline__ void set_state(Task* arg0, s32 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

/// Inlined copy of `func_actor_342400_80169518`: while `field_41E` is 1,
/// consumes the request in `field_448` (1..5 jump to states 6, 7, 8, 7, 9)
/// and returns 1; otherwise returns 0.
static __inline__ s32 take_request(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0:
                set_state(arg0, 6);
                break;
            case 1:
                set_state(arg0, 7);
                break;
            case 2:
                set_state(arg0, 8);
                break;
            case 3:
                set_state(arg0, 7);
                break;
            case 4:
                set_state(arg0, 9);
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

static __inline__ s32 is_hit(Task* arg0)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    if ((w->flags_EC.half & 1) || (w->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}

/// State handler: with `field_44F` 1, a pending request 1 while `field_41E`
/// is set queues animation 0xB (kind 2, speed 0x20); otherwise a consumed
/// request wins, and a hit moves to state 3. With `field_44F` clear, a hit
/// calls `func_actor_342400_801694A8` and moves to state 5. The request test
/// compares against the constant 1, which CSE folds into the `field_44F`
/// register; writing `== work->field_44F` reloads the byte instead.
static void func_actor_342400_801664C4(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (work->field_44F == 1) {
        if (work->field_41E != 0 && work->field_448 == 1) {
            work->field_41C = 0x20;
            work->field_418 = 0xB;
            work->field_414 = 2;
            return;
        }
        if (take_request(arg0) == 0 && is_hit(arg0)) {
            set_state(arg0, 3);
        }
    } else if (is_hit(arg0)) {
        func_actor_342400_801694A8(arg0, 1);
        set_state(arg0, 5);
    }
}

/// The five state handlers of the second enemy form, indexed by
/// `Actor341700Work::field_420`; copied to the stack before dispatch. It sits
/// between `func_actor_342400_801664C4`'s jump table and this function's own.
static const TaskFuncTable5 D_actor_342400_80161F8C = { {
    func_actor_342400_8016AE24,
    func_actor_342400_8016AEAC,
    func_actor_342400_8016AF34,
    func_actor_342400_8016AFA8,
    func_actor_342400_8016B038,
} };

/// Per-frame callback for the second enemy form, the five-state counterpart
/// of `func_actor_342400_801640B0`: in mode 0 it aims (`func_actor_342400_801662EC`),
/// lets a pending hit replace the state handler, rebuilds the root rotation,
/// then picks the next state - 4 when dead, 8 / 9 for messages 4 / 5, and
/// state 3 after a consumed `field_448` request. Mode 1 only recolours; both
/// clear bit 0x80 of the model's `field_C`, which mode 2 sets.
static void func_actor_342400_8016666C(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    GpEnemy*         enemy = arg0->spawnArg2.pointer;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable5   sp    = D_actor_342400_80161F8C;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            func_actor_342400_801662EC(arg0);
            if (take_hit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            func_actor_342400_80165CC0(arg0);
            func_actor_342400_801639A8(arg0);
            update_rotation(arg0);
            func_actor_342400_801653DC(arg0, 0);
            if (work->field_438 == 0 && enemy->hp <= 0) {
                enter_state(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                enter_state(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                enter_state(arg0, 9);
            } else if (take_request(arg0)) {
                work->field_438 = 0;
                enter_state(arg0, 3);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            update_color(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            func_actor_342400_80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
            func_actor_342400_80163354(arg0, 1, 7, 0x80, 0, 0xFF);
            func_actor_342400_80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~0x80;
            return;
    }
}

/// Sub-state handlers `func_actor_342400_8016AE24` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161FB4 = { {
    func_actor_342400_8016B0A0,
    func_actor_342400_8016B104,
    func_actor_342400_8016B1C8,
} };

/// Sub-state handlers `func_actor_342400_8016AEAC` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161FC0 = { {
    func_actor_342400_8016B21C,
    func_actor_342400_8016B294,
    func_actor_342400_80166B20,
} };

/// Sub-state handlers `func_actor_342400_8016AF34` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161FCC = { {
    func_actor_342400_8016B3C4,
    func_actor_342400_8016B414,
    func_actor_342400_80166C68,
} };

/// Sub-state handlers `func_actor_342400_8016B038` dispatches by `field_422`.
static const TaskFuncTable4 D_actor_342400_80161FD8 = { {
    func_actor_342400_8016B48C,
    func_actor_342400_8016B500,
    func_actor_342400_80166DD4,
    func_actor_342400_80166F54,
} };

/// Ten state handlers, indexed by `Actor341700Work::field_420`; copied to the
/// stack before dispatch.
static const TaskFuncTable10 D_actor_342400_80161FE8 = { {
    func_actor_342400_801673F8,
    func_actor_342400_801676D4,
    func_actor_342400_8016784C,
    func_actor_342400_801679D4,
    func_actor_342400_80167B70,
    func_actor_342400_80167CDC,
    func_actor_342400_80167E78,
    func_actor_342400_80168010,
    func_actor_342400_80168174,
    func_actor_342400_80168394,
} };

/// Sub-state handlers `func_actor_342400_80169C84` dispatches by `field_422`.
static const TaskFuncTable6 D_actor_342400_80162010 = { {
    func_actor_342400_8016B5B0,
    func_actor_342400_8016B744,
    func_actor_342400_80168530,
    func_actor_342400_80168A28,
    func_actor_342400_80168B74,
    func_actor_342400_80168A28,
} };

/// Five state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable5 D_actor_342400_80162028 = { {
    func_actor_342400_8016B9A4,
    func_actor_342400_8016BA3C,
    func_actor_342400_8016BAF4,
    func_actor_342400_8016BB74,
    func_actor_342400_8016BBD0,
} };

/// Seven state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable7 D_actor_342400_8016203C = { {
    func_actor_342400_8016BBD8,
    func_actor_342400_8016BA3C,
    func_actor_342400_8016BAF4,
    func_actor_342400_8016BC70,
    func_actor_342400_8016BD3C,
    func_actor_342400_8016BD98,
    func_actor_342400_8016BED8,
} };

/// Holds for `field_446` frames, then moves to state 2. Over the last 0x30
/// frames the head yaw `field_424` eases back to zero; before that, while a
/// player actor is within 0xDAC and roughly ahead, it turns toward it and
/// after 16 such frames arms `Gp_StateF0` and moves to state 3, and
/// otherwise it sways between two fixed yaws by bit 6 of `field_442`.
static void func_actor_342400_80166B20(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    Actor341700Work* state;
    Actor341700Work* state2;
    s32              angle;
    s32              cur;
    s32              aim;

    if ((s16)++work->field_412 > work->field_446) {
        state            = (Actor341700Work*)arg0->work;
        state->field_420 = 2;
        state->field_422 = 0;
        return;
    }
    if (work->field_446 - 0x30 < (s16)work->field_412) {
        cur             = (u16)work->field_424;
        work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
        return;
    }
    if (work->field_43A < 0xDAC && (aim = (u16)work->field_444, (aim < 0x3C0 || aim > 0xC40))) {
        angle           = (u16)work->field_424;
        work->field_424 = angle + ((s16)((aim - angle) * 16) >> 6);
        if (++work->field_42C >= 0x10) {
            Gp_ArmStateF0(1);
            state2            = (Actor341700Work*)arg0->work;
            state2->field_420 = 3;
            state2->field_422 = 0;
        }
    } else {
        // Both arms are spelled out: the cross-jumped tail leaves each its own
        // load of `field_424`, which a single update after an if/else lacks.
        if (!(((u16)work->field_442 >> 6) & 1)) {
            work->field_424 = (u16)work->field_424 + ((s16)(0x3800 - (u16)work->field_424 * 16) >> 9);
        } else {
            work->field_424 = (u16)work->field_424 + ((s16)(-0x3800 - (u16)work->field_424 * 16) >> 9);
        }
    }
}

/// Side-steps to the right of the heading at a speed scaled by `field_41C`
/// on frames 0x1D..0x29; once the hit flags are set, moves the task and the
/// state machine to state 3.
static void func_actor_342400_80166C68(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor341700Work*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                 = 0x1E;
        angle                                 = work->field_7A + 0x400;
        speed                                 = (((Actor341700Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    work2 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438 = 0;
        enter_state(arg0, 3);
        set_state(arg0, 3);
    }
}

/// The same side-step as `func_actor_342400_80166C68`, but once the hit
/// flags are set it requests animation 3 and advances the sub-state instead.
static void func_actor_342400_80166DD4(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor341700Work*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                 = 0x1E;
        angle                                 = work->field_7A + 0x400;
        speed                                 = (((Actor341700Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    work2 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438  = 0;
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 3;
        work2->field_414 = 1;
        work->field_412  = 0;
        work->field_422++;
    }
}

/// Unless `func_actor_342400_8016945C` takes over, side-steps to the left on
/// frames 0x17..0x23 and, once the hit flags are set, returns the state
/// machine to state 0.
static void func_actor_342400_80166F54(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* next;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor341700Work*)arg0->work;
    if ((func_actor_342400_8016945C(arg0) << 0x10) == 0) {
        if ((u16)(work->field_412++ - 0x17) < 0xD) {
            scale                                 = -0x1E;
            angle                                 = work->field_7A + 0x400;
            speed                                 = (((Actor341700Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
            arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
            arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        work2 = (Actor341700Work*)arg0->work;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_438 = 0;
            next            = (Actor341700Work*)arg0->work;
            next->field_420 = 0;
            next->field_422 = 0;
        }
    }
}

/// Message 0x2C00 with low nibble 3 (see `field_44C`) consumes the message and
/// moves the task to state 7 with a fresh state machine; returns 1 when it did,
/// so the caller skips this frame's state handler. The `s16` result is what
/// keeps the `move` between the flag and its test, and the reload through a
/// second local is what puts it in `$v1`.
static __inline__ s16 take_hit_nibble3(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    s16              hit  = 0;
    Actor341700Work* w2;

    if ((work->field_44C & 0xF) == 3) {
        hit             = 1;
        work->field_44C = 0;
        arg0->state     = 7;
        w2              = (Actor341700Work*)arg0->work;
        w2->field_420   = 0;
        w2->field_422   = 0;
    }
    return hit;
}

/// Per-frame callback, the ten-state counterpart of
/// `func_actor_342400_80165FC0`: in mode 0 a pending hit (`take_hit_nibble3`) replaces
/// the state handler, and the root rotation is rebuilt from 0x78..0x7C before
/// `func_actor_342400_801653DC`.
static void func_actor_342400_801670C0(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable10  sp    = D_actor_342400_80161FE8;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            if (take_hit_nibble3(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            func_actor_342400_80165CC0(arg0);
            update_rotation(arg0);
            func_actor_342400_801653DC(arg0, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            update_color(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                func_actor_342400_80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
                func_actor_342400_80163354(arg0, 1, 7, 0x80, 0, 0xFF);
                func_actor_342400_80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// `set_state` with an `s16` state. The narrower parameter is load-bearing:
/// with the `s32` one, `func_actor_342400_801673F8` no longer matches. Each
/// call site reloads `work`, and cross-jumping merges the identical stores,
/// which is what leaves one `lw` per arm in front of a shared tail.
static __inline__ void set_state_s16(Task* arg0, s16 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

/// Answers message 0x2C00 with low nibble 1 (latched in `field_44C`): shows
/// the model and arms its hit bodies, places the root at the spawn point
/// bits 8..11 pick from the current map's table (0x427 or 0x428, playing
/// sound 6 on 0x427), requests animation 7 with an upward launch, and starts
/// state 1, 4 or 7 by bits 4..7.
static void func_actor_342400_801673F8(Task* arg0)
{
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    TmdObject*       obj   = arg0->extra.tmd;
    GpEnemy*         enemy = arg0->spawnArg2.pointer;
    GfxCoord*        coord = obj->coords;
    Actor341700Work* w2;
    s32              id;
    s32              pan;
    u32              map;

    if ((work->field_44C & 0xF) == 1) {
        work->field_451      = 1;
        work->obj_2AC.flags |= 0x8000;
        work->obj_2CC.flags &= 0xBFFF;
        obj->flags          &= 0xFF7F;
        if ((arg0->spawnArg1.value & 0xF) != 2) {
            Tmd_AllocBuffers(obj);
            obj->flags &= 0xFFFB;
        }
        enemy->node.state.b.flags = 0;
        map                       = GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA;
        if (map == 0x4270000) {
            // The 7C store follows 7A here; written first, it schedules
            // ahead of the heading load.
            work->field_78    = 0;
            work->field_7A    = (D_shelter_b3_dumping_hole_8018B74C[(work->field_44C >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->field_7C    = 0;
            coord->coord.t[0] = D_shelter_b3_dumping_hole_8018B74C[(work->field_44C >> 8) & 0xF].x;
            coord->coord.t[1] = D_shelter_b3_dumping_hole_8018B74C[(work->field_44C >> 8) & 0xF].y;
            coord->coord.t[2] = D_shelter_b3_dumping_hole_8018B74C[(work->field_44C >> 8) & 0xF].z;
            id                = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x54270006;
            pan               = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        } else if (map == 0x4280000) {
            work->field_78    = 0;
            work->field_7A    = (D_shelter_b3_garbage_incinerator_801874C4[(work->field_44C >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->field_7C    = 0;
            coord->coord.t[0] = D_shelter_b3_garbage_incinerator_801874C4[(work->field_44C >> 8) & 0xF].x;
            coord->coord.t[1] = D_shelter_b3_garbage_incinerator_801874C4[(work->field_44C >> 8) & 0xF].y;
            coord->coord.t[2] = D_shelter_b3_garbage_incinerator_801874C4[(work->field_44C >> 8) & 0xF].z;
        }
        work->field_428 = 0;
        work->field_42A = 100;
        w2              = (Actor341700Work*)arg0->work;
        w2->field_41C   = 0x10;
        w2->field_418   = 7;
        w2->field_414   = 2;
        switch ((work->field_44C >> 4) & 0xF) {
            case 0:
                set_state_s16(arg0, 1);
                break;
            case 1:
                set_state_s16(arg0, 4);
                break;
            default:
                set_state_s16(arg0, 7);
                break;
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        work->field_44C = 0;
    }
}

/// Flies backwards off the heading, pitching up toward 0x800, under the
/// accelerating drop `field_42A`; on landing turns around, requests
/// animation 0x11, launches again and advances the state.
static void func_actor_342400_801676D4(Task* arg0)
{
    Actor341700Work* work;
    s16              angle;
    GfxCoord*        coord;
    Actor341700Work* anim;
    s32              speed;
    s32              dx;

    work                                  = (Actor341700Work*)arg0->work;
    angle                                 = work->field_7A;
    coord                                 = arg0->extra.tmd->coords;
    dx                                    = rsin(angle) << 4;
    speed                                 = -0x8C;
    arg0->extra.tmd->coords->coord.t[0]  += (dx * speed) >> 16;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 16;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_78                       += (0x800 - work->field_78) >> 3;
    coord->coord.t[1]                    += work->field_42A;
    work->field_428                      += 2;
    work->field_42A                      += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        work->field_7A   += 0x800;
        anim              = (Actor341700Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0x11;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x6E;
        work->field_420++;
    }
}

/// Plays sound 9 on the first frame and hops forward 0x50 units a frame
/// under the accelerating drop; on landing advances the state.
static void func_actor_342400_8016784C(Task* arg0)
{
    Actor341700Work* work;
    GfxCoord*        coord;
    s32              soundId;
    s32              pan;
    s16              angle;
    s16              speed;

    work  = (Actor341700Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    speed                                 = 0x50;
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]                    += work->field_42A;
    work->field_428                      += 4;
    work->field_42A                      += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_412   = 0;
        work->field_420++;
    }
}

static void func_actor_342400_801679D4(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* next;
    Actor341700Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor341700Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    speed                                 = 0x14;
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work2                                 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor341700Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor341700Work*)arg0->work;
        next2->field_420     = 5;
        next2->field_422     = 0;
    }
}

/// Flies backwards off the heading, pitching toward 0x200, under the
/// accelerating drop; on landing levels out, requests animation 0xC,
/// launches again and advances the state.
static void func_actor_342400_80167B70(Task* arg0)
{
    Actor341700Work* work;
    s16              angle;
    GfxCoord*        coord;
    Actor341700Work* anim;
    s32              speed;
    s32              dx;

    work                                  = (Actor341700Work*)arg0->work;
    angle                                 = work->field_7A;
    coord                                 = arg0->extra.tmd->coords;
    dx                                    = rsin(angle) << 4;
    speed                                 = -0x8C;
    arg0->extra.tmd->coords->coord.t[0]  += (dx * speed) >> 16;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 16;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_78                       += (0x200 - work->field_78) >> 5;
    coord->coord.t[1]                    += work->field_42A;
    work->field_428                      += 2;
    work->field_42A                      += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        anim              = (Actor341700Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x6E;
        work->field_420++;
    }
}

/// Plays sound 9 on the first frame and hops backwards 0x50 units a frame,
/// easing the pitch back to zero, under the accelerating drop; on landing
/// advances the state.
static void func_actor_342400_80167CDC(Task* arg0)
{
    Actor341700Work* work;
    GfxCoord*        coord;
    s32              soundId;
    s32              pan;
    s16              angle;
    s16              speed;

    work  = (Actor341700Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_412++;
    work->field_78 += -work->field_78 >> 5;
    if ((s16)work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    speed                                 = -0x50;
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]                    += work->field_42A;
    work->field_428                      += 4;
    work->field_42A                      += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_412   = 0;
        work->field_420++;
    }
}

/// Plays sound 9 on the first frame and backs off 0x14 units a frame; once
/// the hit flags are set, disarms the outer hit body and moves the task to
/// state 3 with the state machine at state 3.
static void func_actor_342400_80167E78(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* next;
    Actor341700Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor341700Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    speed                                 = -0x14;
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work2                                 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor341700Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor341700Work*)arg0->work;
        next2->field_420     = 3;
        next2->field_422     = 0;
    }
}

/// Flies backwards off the heading, pitching toward 0x200, under the
/// accelerating drop; on landing requests animation 0xC, launches high
/// (-0x12C) and advances the state.
static void func_actor_342400_80168010(Task* arg0)
{
    Actor341700Work* work;
    s16              angle;
    GfxCoord*        coord;
    Actor341700Work* anim;
    s32              speed;
    s32              dx;

    work                                  = (Actor341700Work*)arg0->work;
    angle                                 = work->field_7A;
    coord                                 = arg0->extra.tmd->coords;
    dx                                    = rsin(angle) << 4;
    speed                                 = -0x8C;
    arg0->extra.tmd->coords->coord.t[0]  += (dx * speed) >> 16;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 16;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_78                       += (0x200 - work->field_78) >> 5;
    coord->coord.t[1]                    += work->field_42A;
    work->field_428                      += 2;
    work->field_42A                      += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        anim              = (Actor341700Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x12C;
        work->field_420++;
    }
}

/// Plays sounds 9 and 3 on the first frame and backs off 0x5A units a frame,
/// pitching toward 0x800, under the accelerating drop; on landing levels
/// out, turns around, requests animation 0x11 and advances the state.
static void func_actor_342400_80168174(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* anim;
    GfxCoord*        coord;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;
    s16              angle;
    s16              speed;

    work  = (Actor341700Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_412++;
    work->field_78 += (0x800 - work->field_78) >> 3;
    if ((s16)work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        soundId2 = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0003;
        pan2     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    speed                                 = -0x5A;
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]                    += work->field_42A;
    work->field_428                      += 4;
    work->field_42A                      += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        work->field_7A   += 0x800;
        anim              = (Actor341700Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0x11;
        anim->field_414   = 2;
        work->field_412   = 0;
        work->field_420++;
    }
}

static void func_actor_342400_80168394(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* next;
    Actor341700Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor341700Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    speed                                 = 0x14;
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work2                                 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor341700Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor341700Work*)arg0->work;
        next2->field_420     = 5;
        next2->field_422     = 0;
    }
}

/// Sub-state handler, the steering counterpart of `func_actor_342400_80168B74`:
/// while the enemy lives it turns `field_7A` toward `field_70` and pushes the
/// root back along it, then slides toward `field_70` accelerating with
/// `field_42A`. Past 120 frames (or once dead) it eases y in and marks
/// `field_438`; while alive a hit flag plays sound 1. Within 800 units it
/// advances `field_422`, otherwise a dead enemy queues its follow-up animation.
static void func_actor_342400_80168530(Task* arg0)
{
    TmdObject*       obj;
    Actor341700Work* work;
    GpEnemy*         enemy;
    GfxCoord*        coord;
    GfxCoord*        c;
    VECTOR           d;
    SVECTOR          dir;
    VECTOR           sq;
    VECTOR*          out;
    s16              angle;
    s32              dist;
    s32              cond;
    s32              soundId;
    s32              pan;

    obj   = arg0->extra.tmd;
    work  = (Actor341700Work*)arg0->work;
    coord = obj->coords;
    enemy = (GpEnemy*)arg0->spawnArg2.pointer;
    work->field_412++;
    if (enemy->hp > 0) {
        Actor341700Work* w;
        s32              diff;
        s32              k;
        s32              step;

        if ((u32)(work->field_44F >> 1) < 0x40) {
            work->field_41C = work->field_44F >> 2;
            work->field_44F++;
        } else {
            work->field_41C = 0x40;
        }
        w      = (Actor341700Work*)arg0->work;
        c      = arg0->extra.tmd->coords;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        diff = (((u16)w->field_7A - ratan2(dir.vx, dir.vz)) << 20) >> 20;
        if (diff > 0x100) {
            w->field_7A -= 0x18;
        } else if (diff < -0x100) {
            w->field_7A += 0x18;
        }
        angle                                 = work->field_7A;
        k                                     = -0x10;
        step                                  = ((((Actor341700Work*)arg0->work)->field_41C * k) << 12) >> 16;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    work->field_428++;
    work->field_42A += work->field_428;
    {
        s32 step = work->field_42A >> 6;

        c      = arg0->extra.tmd->coords;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                 = ratan2(dir.vx, dir.vz);
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    d.vx = coord->coord.t[0] - work->field_70.vx;
    d.vy = coord->coord.t[1] - work->field_70.vy;
    d.vz = coord->coord.t[2] - work->field_70.vz;
    out  = &sq;
    gte_ldlvl(&d);
    gte_sqr0();
    gte_stlvnl(out);
    dist = SquareRoot0(sq.vx + sq.vy + sq.vz);
    if ((s16)work->field_412 > 120) {
        if (dist <= 3000) {
            work->field_438    = 1;
            coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 4;
        } else {
            work->field_438    = 1;
            coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 5;
        }
    } else if (enemy->hp > 0) {
        Actor341700Work* w2 = (Actor341700Work*)arg0->work;

        if ((w2->flags_EC.half & 1) || (w2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0001;
            pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
    } else {
        work->field_438    = 1;
        coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 5;
    }
    if (dist < 800) {
        work->field_422++;
        return;
    }
    if (enemy->hp <= 0) {
        if (work->field_448 != 4) {
            work->field_438 = 1;
            if (work->field_418 == 8) {
                if (work->field_440 == 0) {
                    Actor341700Work* w = (Actor341700Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 5;
                    w->field_414 = 1;
                } else {
                    Actor341700Work* w = (Actor341700Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 6;
                    w->field_414 = 1;
                }
            } else {
                Actor341700Work* w;
                s16              next;

                next         = D_actor_342400_80173A98[work->field_418 - 1];
                w            = (Actor341700Work*)arg0->work;
                w->field_426 = 4;
                w->field_41C = 0x10;
                w->field_418 = next;
                w->field_414 = 1;
            }
        } else {
            work->field_438 = 0;
        }
    }
}

/// Death: plays sound 3 unless the enemy's HP is already negative, releases
/// `Gp_StateF0`'s hold if it points at this enemy, unlinks the enemy node and
/// its three hit bodies, moves the task to state 5, tells slot-4 task 0 with
/// message 0x13F4, and hides the model.
static void func_actor_342400_80168A28(Task* arg0)
{
    Actor341700Work* objs;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    Actor341700Work* work;
    s32              soundId;
    s32              pan;

    work            = (Actor341700Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2.pointer;
    tmd             = arg0->extra.tmd;
    work->field_438 = 1;
    if (enemy->hp >= 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0003;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    enemy->recs = 0;
    objs        = (Actor341700Work*)arg0->work;
    Gp_UnlinkObj(&objs->obj_2AC);
    Gp_UnlinkObj(&objs->obj_2CC);
    Gp_UnlinkObj(&objs->obj_3AC);
    enter_state(arg0, 5);
    Gp_DispatchMsg(Gp_LookupSlot4(0), 0x13F4, 0, 0);
    tmd->flags |= 0x80;
}

/// Sub-state handler: slides the model's root toward `field_70` in x/z,
/// accelerating with `field_42A`; after 90 frames it also eases y in and marks
/// `field_438`. Within 800 units it advances `field_422`; if the enemy's HP is
/// gone instead, it queues the follow-up animation (or clears `field_438` when
/// state 4 is pending).
static void func_actor_342400_80168B74(Task* arg0)
{
    TmdObject*       obj;
    Actor341700Work* work;
    GpEnemy*         enemy;
    GfxCoord*        coord;
    GfxCoord*        c;
    VECTOR           d;
    SVECTOR          dir;
    VECTOR           sq;
    VECTOR*          out;
    s16              angle;
    s16              next;

    obj   = arg0->extra.tmd;
    work  = (Actor341700Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2.pointer;
    coord = obj->coords;
    work->field_412++;
    work->field_428++;
    work->field_42A += work->field_428;
    if ((s16)work->field_412 < 0x5A) {
        s32 step = work->field_42A >> 6;

        c      = arg0->extra.tmd->coords;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                 = ratan2(dir.vx, dir.vz);
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        s32 step;

        work->field_438 = 1;
        step            = work->field_42A >> 5;
        c               = arg0->extra.tmd->coords;
        dir.vx          = work->field_70.vx - c->coord.t[0];
        dir.vy          = 0;
        dir.vz          = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                 = ratan2(dir.vx, dir.vz);
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[1]                    += (work->field_70.vy - coord->coord.t[1]) >> 4;
    }
    d.vx = coord->coord.t[0] - work->field_70.vx;
    d.vy = coord->coord.t[1] - work->field_70.vy;
    d.vz = coord->coord.t[2] - work->field_70.vz;
    out  = &sq;
    gte_ldlvl(&d);
    gte_sqr0();
    gte_stlvnl(out);
    if (SquareRoot0(sq.vx + sq.vy + sq.vz) < 800) {
        work->field_422++;
        return;
    }
    if (enemy->hp <= 0) {
        SndEvt_EnqueueType7(0x402C0002, 1);
        if (work->field_448 != 4) {
            work->field_438 = 1;
            if (work->field_418 == 8) {
                if (work->field_440 == 0) {
                    Actor341700Work* w = (Actor341700Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 5;
                    w->field_414 = 1;
                } else {
                    Actor341700Work* w = (Actor341700Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 6;
                    w->field_414 = 1;
                }
            } else {
                Actor341700Work* w;

                next         = D_actor_342400_80173A98[work->field_418 - 1];
                w            = (Actor341700Work*)arg0->work;
                w->field_426 = 4;
                w->field_41C = 0x10;
                w->field_418 = next;
                w->field_414 = 1;
            }
        } else {
            work->field_438 = 0;
        }
    }
}

/// Per-frame callback, the five-state counterpart of
/// `func_actor_342400_801690FC`; unlike it, clears bit 0x80 of `field_C` on
/// the way out of modes 0 and 1.
static void func_actor_342400_80168F14(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable5   sp    = D_actor_342400_80162028;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            update_color(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                func_actor_342400_80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
                func_actor_342400_80163354(arg0, 1, 7, 0x80, 0, 0xFF);
                func_actor_342400_80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            obj->flags &= ~0x80;
            return;
    }
}

/// Per-frame callback, the seven-state counterpart of
/// `func_actor_342400_80165FC0`: in mode 0 it also spawns effect 3 on the
/// model's second coord part every 32 frames.
static void func_actor_342400_801690FC(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable7   sp    = D_actor_342400_8016203C;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            update_color(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                func_actor_342400_80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
                func_actor_342400_80163354(arg0, 1, 7, 0x80, 0, 0xFF);
                func_actor_342400_80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// Queues CD command 0x21 once, guarded by `Gp_StateF0.field_25`: the first parameter
/// is 2 or 3 in place 1 or 2 of stage 4 areas 0x27/0x28 and 1 everywhere
/// else.
static void func_actor_342400_801692E8(void)
{
    u8 param1[8];
    u8 param2[8];

    if (Gp_StateF0.field_25 == 0) {
        /* Each branch makes its own call; jump2's cross-jumping merges the
         * identical tails after sched2, which is why the argument setup is
         * duplicated per branch in the target. */
        if (gGameSession->at4.loc.stage == 4 && (u32)(gGameSession->at4.loc.area - 0x27) < 2 && gGameSession->at4.loc.place == 1) {
            param1[2] = 0xA;
            param1[0] = 2;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else if (gGameSession->at4.loc.stage == 4 && (u32)(gGameSession->at4.loc.area - 0x27) < 2 && gGameSession->at4.loc.place == 2) {
            param1[2] = 0xA;
            param1[0] = 3;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else {
            param1[2] = 0xA;
            param1[0] = 1;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        }
        Gp_StateF0.field_25 = 1;
    }
}

static void func_actor_342400_80169408(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016B84C,
        func_actor_342400_8016B914,
    };

    states[(s16)work->field_420](arg0);
}

/// Once bit 7 of `Gp_StateF0.field_1F` is set, puts the task in state 3 with
/// the state machine at state 5 and returns 1; otherwise returns 0.
static s16 func_actor_342400_8016945C(Task* arg0)
{
    if ((s8)Gp_StateF0.field_1F & 0x80) {
        enter_state(arg0, 3);
        set_state(arg0, 5);
        return 1;
    }
    return 0;
}

/// Claims or releases `Gp_StateF0`'s hold for this enemy. With `arg1` set it
/// claims the hold (bit 7 plus the enemy's slot) unless one is already held;
/// with `arg1` clear it releases the hold if it is this enemy's.
static void func_actor_342400_801694A8(Task* arg0, s32 arg1)
{
    if ((arg1 << 0x10) != 0) {
        if (!((s8)Gp_StateF0.field_1F & 0x80)) {
            Gp_StateF0.field_1F = (((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) | 0x80;
        }
    } else if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
}

/// While `field_41E` is 1, consumes the pending request in `field_448`:
/// requests 1..5 jump the state machine to states 6, 7, 8, 7 and 9 at
/// sub-state 0, anything else is just cleared. Returns 1 when `field_41E` is 1
/// and 0 otherwise. Each case reloads the work block through its own local;
/// one shared local lands in `$a0` instead of `$v1`.
static s32 func_actor_342400_80169518(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0: {
                Actor341700Work* w = (Actor341700Work*)arg0->work;
                w->field_420       = 6;
                w->field_422       = 0;
                break;
            }
            case 1: {
                Actor341700Work* w = (Actor341700Work*)arg0->work;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 2: {
                Actor341700Work* w = (Actor341700Work*)arg0->work;
                w->field_420       = 8;
                w->field_422       = 0;
                break;
            }
            case 3: {
                Actor341700Work* w = (Actor341700Work*)arg0->work;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 4: {
                Actor341700Work* w = (Actor341700Work*)arg0->work;
                w->field_420       = 9;
                w->field_422       = 0;
                break;
            }
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

/// Message handler: on message 0x2C00 whose low nibble is 1..5, store the
/// message halfword in `field_44C`. The five identical case bodies are
/// cross-jumped into one, but only separate bodies keep the jump table; a
/// single `case 1 ... 5` becomes a range test.
void func_actor_342400_801695C0(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (arg2->from.key == 0x2C00) {
        switch (arg2->command & 0xF) {
            case 1:
                work->field_44C = arg2->command;
                break;
            case 2:
                work->field_44C = arg2->command;
                break;
            case 3:
                work->field_44C = arg2->command;
                break;
            case 4:
                work->field_44C = arg2->command;
                break;
            case 5:
                work->field_44C = arg2->command;
                break;
        }
    }
}

/// Moves the model: writes `pos` into the root part's translation and marks
/// the coordinate dirty. `part` is accepted but unused.
void func_actor_342400_80169620(Task* task, s16 part, VECTOR3* pos)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = pos->vx;
    coord->coord.t[1]   = pos->vy;
    coord->coord.t[2]   = pos->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Moves the model so that part `arg1` lands on `arg2`: shifts the root
/// translation by the part's offset from the root in view space and marks
/// the part's coordinate dirty.
static void func_actor_342400_80169654(Task* arg0, s16 arg1, SVECTOR3* arg2)
{
    MATRIX    local;
    MATRIX    world;
    GfxCoord* coord;
    GfxCoord* coords;

    coords = arg0->extra.tmd->coords;
    coord  = &coords[arg1];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coords->workm, &local);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &world);
    coords->coord.t[0]  = arg2->vx - (world.t[0] - local.t[0]);
    coords->coord.t[1]  = arg2->vy - (world.t[1] - local.t[1]);
    coords->coord.t[2]  = arg2->vz - (world.t[2] - local.t[2]);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Scales `arg1` by the animation speed `field_41C`, in 1/16 units.
static s32 func_actor_342400_80169728(Task* arg0, s16 arg1)
{
    return (s32)((((Actor341700Work*)arg0->work)->field_41C * arg1) << 0xC) >> 0x10;
}

/// Returns 1 when the hit flags are set - bit 0 of the flag halfword or bits
/// 0x102 of the word - and 0 otherwise.
static s16 func_actor_342400_8016974C(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}

void func_actor_342400_8016978C(Task* arg0)
{
    TaskFuncTable10 sp;

    sp = D_actor_342400_80161E80;
    sp.funcs[arg0->state](arg0);
}

/// Runs the handler for the task's `Task::state` from the six-entry table.
void func_actor_342400_80169810(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_actor_342400_80161E68;
    sp.funcs[arg0->state](arg0);
}

static void func_actor_342400_80169880(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016AA9C,
        func_actor_342400_8016AAB8,
    };

    states[(s16)work->field_420](arg0);
}

/// Turns the heading `field_7A` by `step` toward the nearer player actor
/// (the offset in `field_88` / `field_8C`), leaving it alone within 0x100.
static void func_actor_342400_801698D4(Task* arg0, s32 step)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    SVECTOR          vec;
    s32              diff;
    u16              angle;
    s32              yaw;

    vec.vx = work->field_88;
    vec.vy = 0;
    vec.vz = work->field_8C;
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(-vec.vx, -vec.vz);
    angle = work->field_7A;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->field_7A = angle - step;
    } else if (diff < -0x100) {
        work->field_7A = angle + step;
    }
}

static void func_actor_342400_80169968(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

static void func_actor_342400_8016997C(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

static void func_actor_342400_80169990(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

/// Unless `func_actor_342400_80169518` consumes a pending request, runs the
/// sub-state handler for `field_422` from a three-entry table.
static void func_actor_342400_801699A4(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161EE0;
    if ((func_actor_342400_80169518(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

/// Runs the sub-state handler for `field_422` from a five-entry table.
static void func_actor_342400_80169A2C(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable5   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161EEC;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Runs the sub-state handler for `field_422` from another five-entry table.
static void func_actor_342400_80169A98(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable5   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161F00;
    sp.funcs[(s16)work->field_422](arg0);
}

static void func_actor_342400_80169B04(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016AB6C,
        func_actor_342400_801664C4,
    };

    states[(s16)work->field_422](arg0);
}

static void func_actor_342400_80169B58(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016AC80,
        func_actor_342400_8016AD94,
    };

    states[(s16)work->field_422](arg0);
}

static void func_actor_342400_80169BAC(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_80169CF8,
        func_actor_342400_80169D2C,
    };

    states[(s16)work->field_422](arg0);
}

static void func_actor_342400_80169C00(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161ED4;
    sp.funcs[(s16)work->field_422](arg0);
    if (work->field_44F == 1) {
        func_actor_342400_8016BEF0(arg0);
    }
}

static void func_actor_342400_80169C84(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable6   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80162010;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Requests animation 0xC and advances the sub-state.
static void func_actor_342400_80169CF8(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xC;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

/// Once the hit flags are set, requests animation 0xB; once the enemy's
/// flag-2 counter runs out, moves the state machine to state 3.
static void func_actor_342400_80169D2C(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    if ((func_actor_342400_8016974C(arg0) << 0x10) != 0) {
        work            = (Actor341700Work*)arg0->work;
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 0xB;
        work->field_414 = 1;
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

static void func_actor_342400_80169DA4(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    work->field_44F = D_actor_342400_80173A84[work->field_418 - 1];
    if (work->field_44F == 1) {
        Actor341700Work* w = (Actor341700Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 6;
        w->field_414 = 1;
    } else {
        Actor341700Work* w = (Actor341700Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 5;
        w->field_414 = 1;
    }
    work->field_422++;
}

/// Once the hit flags are set, requests animation 7 (when `field_44F` is 1)
/// or 1, and advances the sub-state.
static void func_actor_342400_80169E24(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* slow;
    Actor341700Work* fast;

    work = (Actor341700Work*)arg0->work;
    if (func_actor_342400_8016974C(arg0) != 0) {
        if (work->field_44F == 1) {
            fast            = (Actor341700Work*)arg0->work;
            fast->field_426 = 0x32;
            fast->field_41C = 0x10;
            fast->field_418 = 7;
            fast->field_414 = 1;
        } else {
            slow            = (Actor341700Work*)arg0->work;
            slow->field_426 = 0x1E;
            slow->field_41C = 0x10;
            slow->field_418 = 1;
            slow->field_414 = 1;
        }
        work->field_422 = work->field_422 + 1;
    }
}

/// Once the hit flags are set, moves the state machine to state 3 (when
/// `field_44F` is 1) or 5.
static void func_actor_342400_80169EC4(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (func_actor_342400_8016974C(arg0)) {
        if (work->field_44F == 1) {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_420 = 3;
            w->field_422 = 0;
        } else {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_420 = 5;
            w->field_422 = 0;
        }
    }
}

static void func_actor_342400_80169F30(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    s16              angle;
    s16              speed;

    func_actor_342400_801698D4(arg0, 0x10);
    speed                                 = func_actor_342400_80169728(arg0, -0x10);
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (func_actor_342400_8016974C(arg0)) {
        Actor341700Work* next = (Actor341700Work*)arg0->work;

        next->field_420 = 4;
        next->field_422 = 0;
    }
}

/// Saves the root Y as the ground height in `field_92`, requests animation 8
/// at speed 0x10, clears the frame counter and the motion halfwords, sets
/// `field_440` and advances the sub-state.
static void func_actor_342400_8016A020(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s16              tmp;

    work             = (Actor341700Work*)arg0->work;
    work->field_92   = (u16)arg0->extra.tmd->coords->coord.t[1];
    work2            = (Actor341700Work*)arg0->work;
    tmp              = 8;
    work2->field_426 = tmp;
    work2->field_418 = tmp;
    work2->field_41C = 0x10;
    tmp              = 1;
    work2->field_414 = tmp;
    work->field_412  = 0;
    work->field_428  = 0;
    work->field_42A  = -0x12C;
    work->field_440  = tmp;
    work->field_438  = 0;
    work->field_432  = 0;
    work->field_422  = work->field_422 + 1;
}

/// Plays sound 4 on the first frame; once the hit flags are set, draws a
/// 0x5A..0xD9 cooldown into `field_44A` and moves the state machine to
/// state 3.
static void func_actor_342400_8016A084(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              soundId;
    s32              pan;
    u32              rand;

    work            = (Actor341700Work*)arg0->work;
    work->field_438 = 0;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (func_actor_342400_8016974C(arg0) != 0) {
        rand             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState      = rand;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work2            = (Actor341700Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

/// Requests animation 9, clears the frame counter, advances the sub-state
/// and, while the enemy has HP left, plays sound 2.
static void func_actor_342400_8016A184(Task* arg0)
{
    Actor341700Work* work;
    GpEnemy*         enemy;
    s32              soundId;
    s32              pan;

    work            = (Actor341700Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2.pointer;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 9;
    work->field_414 = 1;
    work->field_412 = 0;
    work->field_422++;
    if (enemy->hp > 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0002;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
}

/// Advances the sub-state once the frame counter has passed 0x50.
static void func_actor_342400_8016A240(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((s16)ticks >= 0x51) {
        work->field_422 = work->field_422 + 1;
    }
}

/// Once the hit flags are set, releases this enemy's `Gp_StateF0` hold,
/// requests animation 0xF and advances the sub-state.
static void func_actor_342400_8016A280(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->work;
    if ((func_actor_342400_8016974C(arg0) << 0x10) != 0) {
        func_actor_342400_801694A8(arg0, 0);
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

/// Once the hit flags are set, marks the enemy busy (`field_438`), requests
/// animation 4 and advances the sub-state.
static void func_actor_342400_8016A2FC(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->work;
    if ((func_actor_342400_8016974C(arg0) << 0x10) != 0) {
        work->field_438  = 1;
        work->field_412  = 0;
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

static void func_actor_342400_8016A370(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    s16              angle;
    s16              speed;

    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        speed                                 = func_actor_342400_80169728(arg0, 0x1E);
        angle                                 = work->field_7A + 0x400;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (func_actor_342400_8016974C(arg0)) {
        Actor341700Work* next;

        work->field_438 = 0;
        next            = (Actor341700Work*)arg0->work;
        next->field_420 = 3;
        next->field_422 = 0;
    }
}

/// Runs the sub-state handler for `field_422` from a four-entry table.
static void func_actor_342400_8016A494(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable4   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161F14;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Sets `field_432`, requests animation 7 and advances the sub-state.
static void func_actor_342400_8016A4FC(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work             = (Actor341700Work*)arg0->work;
    work->field_432  = 1;
    work2            = (Actor341700Work*)arg0->work;
    work2->field_41C = 0x10;
    work2->field_418 = 7;
    work2->field_414 = 2;
    work->field_422  = work->field_422 + 1;
}

/// Rebuilds the root rotation: pitches about X by a sine sway driven by
/// `field_442`, turns by the heading `field_7A`, and copies the 3x3 into the
/// root coordinate. When `field_41E` is 1, latches that pitch into
/// `field_434`, clears the flag and three motion halfwords, and advances the
/// sub-state.
static void func_actor_342400_8016A538(Task* arg0)
{
    Actor341700Work* work;
    GfxCoord*        coord;
    OverlayMat       rot;
    OverlayMat*      src;
    MATRIX*          dst;
    s16              pitch;

    work               = (Actor341700Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    src                = &rot;
    src->ident.m00_m01 = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    pitch              = ((rsin(work->field_442 << 6) * 0x10) >> 7) - 0x400;
    RotMatrixX(pitch, &src->mat);
    RotMatrixY(work->field_7A, &src->mat);
    dst          = &coord->coord;
    dst->m[0][0] = src->mat.m[0][0];
    dst->m[0][1] = src->mat.m[0][1];
    dst->m[0][2] = src->mat.m[0][2];
    dst->m[1][0] = src->mat.m[1][0];
    dst->m[1][1] = src->mat.m[1][1];
    dst->m[1][2] = src->mat.m[1][2];
    dst->m[2][0] = src->mat.m[2][0];
    dst->m[2][1] = src->mat.m[2][1];
    dst->m[2][2] = src->mat.m[2][2];
    if (work->field_41E == 1) {
        work->field_41E = 0;
        work->field_432 = 0;
        work->field_428 = 0;
        work->field_42A = 0;
        work->field_434 = pitch;
        work->field_422++;
    }
}

/// Death cry: plays sound 2, releases this enemy's `Gp_StateF0` hold and
/// unlinks the enemy node. A pending request 4 hides the model and jumps to
/// state 7; otherwise the state advances.
static void func_actor_342400_8016A664(Task* arg0)
{
    GpEnemy*         enemy;
    Actor341700Work* work;
    TmdObject*       model;
    Actor341700Work* work2;

    enemy = (GpEnemy*)arg0->spawnArg2.pointer;
    model = arg0->extra.tmd;
    work  = (Actor341700Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x402C0002, 0xF);
    func_actor_342400_801694A8(arg0, 0);
    Gp_UnlinkNode(&enemy->node);
    if (work->field_448 == 4) {
        work->field_412  = 0;
        model->flags     = model->flags | 0x80;
        work2            = (Actor341700Work*)arg0->work;
        work2->field_420 = 7;
        work2->field_422 = 0;
        return;
    }
    work->field_420 = work->field_420 + 1;
}

static void func_actor_342400_8016A724(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* work3;
    Actor341700Work* work4;
    s16              anim;
    s16              next;

    work = (Actor341700Work*)arg0->work;
    Gp_ReleaseStateF0Add(arg0, 0);
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work2            = (Actor341700Work*)arg0->work;
            work2->field_426 = 4;
            work2->field_41C = 0x10;
            work2->field_418 = 5;
            work2->field_414 = 1;
        } else {
            work3            = (Actor341700Work*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 6;
            work3->field_414 = 1;
        }
    } else {
        next             = D_actor_342400_80173A98[anim - 1];
        work4            = (Actor341700Work*)arg0->work;
        work4->field_426 = 4;
        work4->field_41C = 0x10;
        work4->field_418 = next;
        work4->field_414 = 1;
    }
    func_actor_342400_80165CC0(arg0);
    work->field_420++;
}

/// Ticks the animation and, once the hit flags are set, advances the state.
static void func_actor_342400_8016A804(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    func_actor_342400_80165CC0(arg0);
    work2 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}

/// Starts the death shrink: detaches the enemy's records and unlinks its
/// three hit bodies, sets the Y scale `field_430` to 1.0, saves the root
/// matrix in `savedRootMtx`, sets the enemy's light mode 1, clears the frame
/// counter and advances the state.
static void func_actor_342400_8016A884(Task* task)
{
    GfxCoord*        coord = task->extra.tmd->coords;
    GpEnemy*         enemy = (GpEnemy*)task->spawnArg2.pointer;
    Actor341700Work* work  = (Actor341700Work*)task->work;
    Actor341700Work* objWork;

    enemy->recs = 0;

    objWork = (Actor341700Work*)task->work;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430    = 0x1000;
    work->savedRootMtx = coord->coord;

    Gp_SetLightMode(task->spawnArg2.pointer, 1);

    work->field_412 = 0;
    work->field_420++;
}

/// After 0x18 frames sets model flag 2, clears the frame counter, sets
/// `field_451` and advances the state.
static void func_actor_342400_8016A950(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;
    TmdObject*       model;

    work            = (Actor341700Work*)arg0->work;
    model           = arg0->extra.tmd;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->flags    = model->flags | 2;
        work->field_412 = 0U;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}

static void func_actor_342400_8016A9AC(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

/// Advances the state after two frames.
static void func_actor_342400_8016A9C4(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 2) {
        work->field_420 = work->field_420 + 1;
    }
}

static void func_actor_342400_8016AA08(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = arg0->extra.tmd;
    enemy = (GpEnemy*)arg0->spawnArg2.pointer;
    Tmd_FreeBuffers(model);
    model->flags |= 4;
    func_actor_342400_801637DC(arg0);
    Gp_ReleaseStateF0Add(arg0, 0);
    enemy->recs = 0;
    work        = (Actor341700Work*)arg0->work;
    Gp_UnlinkObj(&work->obj_2AC);
    Gp_UnlinkObj(&work->obj_2CC);
    Gp_UnlinkObj(&work->obj_3AC);
    work2            = (Actor341700Work*)arg0->work;
    arg0->state      = 5;
    work2->field_420 = 0;
    work2->field_422 = 0;
}

static void func_actor_342400_8016AA9C(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

/// After 0x24 frames destroys the enemy, first telling slot-4 task 0 with
/// message 0x13F4 when in place 1 of stage 4 areas 0x27/0x28.
static void func_actor_342400_8016AAB8(Task* arg0)
{
    Actor341700Work* work;
    u16              ticks;

    work            = (Actor341700Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x24) {
        if ((gGameSession->at4.loc.stage == 4) && ((u32)(gGameSession->at4.loc.area - 0x27) < 2U) && (gGameSession->at4.loc.place == 1)) {
            Gp_DispatchMsg(Gp_LookupSlot4(0), 0x13F4, 1, 0);
        }
        Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
    }
}

static void func_actor_342400_8016AB6C(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              soundId;
    s32              pan;

    work            = (Actor341700Work*)arg0->work;
    work->field_44F = D_actor_342400_80173A84[work->field_418 - 1];
    if (work->field_44F == 1) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xB;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0x11;
        work2->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    work->field_422++;
}

static void func_actor_342400_8016AC80(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              soundId;
    s32              pan;

    work            = (Actor341700Work*)arg0->work;
    work->field_44F = D_actor_342400_80173A84[work->field_418 - 1];
    if (work->field_44F == 1) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 2;
        work2->field_41C = 0x10;
        work2->field_418 = 0xC;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0x11;
        work2->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    work->field_422++;
}

static void func_actor_342400_8016AD94(Task* arg0)
{
    Actor341700Work* work;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (work->field_44F == 1) {
            work            = (Actor341700Work*)arg0->work;
            work->field_420 = 3;
            work->field_422 = 0;
        } else {
            func_actor_342400_801694A8(arg0, 1);
            work            = (Actor341700Work*)arg0->work;
            work->field_420 = 5;
            work->field_422 = 0;
        }
    }
}

static void func_actor_342400_8016AE24(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161FB4;
    if ((func_actor_342400_8016945C(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

static void func_actor_342400_8016AEAC(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161FC0;
    if ((func_actor_342400_8016945C(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

static void func_actor_342400_8016AF34(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016B33C,
        func_actor_342400_8016B370,
    };

    if (func_actor_342400_8016945C(arg0) == 0) {
        states[(s16)work->field_422](arg0);
    }
}

static void func_actor_342400_8016AFA8(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161FCC;
    if ((func_actor_342400_8016945C(arg0) << 0x10) != 0) {
        work->field_438 = 0;
        return;
    }
    sp.funcs[(s16)work->field_422](arg0);
}

/// Runs the sub-state handler for `field_422` from a four-entry table.
static void func_actor_342400_8016B038(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable4   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161FD8;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Requests animation 1, draws a 0x60..0x9F frame hold into `field_446`,
/// clears the frame counter and advances the sub-state.
static void func_actor_342400_8016B0A0(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 1;
    work->field_414 = 1;
    /* Rolling the LCG through the global rather than an m2c temporary is what
     * hoists its `lw` above the field stores; see DECOMPILATION_LEARNINGS.md. */
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_446 = ((Gp_LcgState >> 16) & 0x3F) + 0x60;
    work->field_412 = 0;
    work->field_422 = work->field_422 + 1;
}

/// Once the hold in `field_446` runs out, picks state 4 or 1 at random.
/// Before that, a player actor within 0xDAC moves the state machine to
/// state 3 and one within 0x1388 advances the sub-state.
static void func_actor_342400_8016B104(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    s16              dist;

    if (work->field_446 < (s16)work->field_412++) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_420 = 4;
            w->field_422 = 0;
        } else {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_420 = 1;
            w->field_422 = 0;
        }
        return;
    }
    dist = work->field_43A;
    if (dist < 0xDAC) {
        Actor341700Work* next = (Actor341700Work*)arg0->work;

        next->field_420 = 3;
        next->field_422 = 0;
        return;
    }
    if (dist < 0x1388) {
        work->field_422++;
    }
}

/// Once the hit flags are set, moves the state machine to state 1.
static void func_actor_342400_8016B1C8(Task* arg0)
{
    Actor341700Work* work;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = (Actor341700Work*)arg0->work;
        work->field_420 = 1;
        work->field_422 = 0;
    }
}

/// Once the hit flags are set, requests animation 0xD and advances the
/// sub-state.
static void func_actor_342400_8016B21C(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 1;
        work->field_422++;
    }
}

/// Once the hit flags are set, requests animation 0xE, clears the frame and
/// turn counters, draws a 0xB0..0xEF frame hold into `field_446` and
/// advances the sub-state.
static void func_actor_342400_8016B294(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 0xE;
        work2->field_414 = 1;
        Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
        work->field_412  = 0;
        work->field_42C  = 0;
        work->field_446  = ((Gp_LcgState >> 16) & 0x3F) + 0xB0;
        work->field_422++;
    }
}

/// Requests animation 0xF and advances the sub-state.
static void func_actor_342400_8016B33C(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

/// Once the hit flags are set, returns the state machine to state 0.
static void func_actor_342400_8016B370(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_420 = 0;
        work2->field_422 = 0;
    }
}

/// Requests animation 0xF, advances the sub-state and arms `Gp_StateF0`.
static void func_actor_342400_8016B3C4(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
    Gp_ArmStateF0(1);
}

/// Once the hit flags are set, marks the enemy busy (`field_438`), requests
/// animation 4 and advances the sub-state.
static void func_actor_342400_8016B414(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_412  = 0;
        work->field_438  = 1;
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422++;
    }
}

/// Unless `func_actor_342400_8016945C` takes over, requests animation 0xF
/// and advances the sub-state.
static void func_actor_342400_8016B48C(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->work;
    if (func_actor_342400_8016945C(arg0) == 0) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

/// Unless `func_actor_342400_8016945C` takes over, waits for the hit flags,
/// then marks the enemy busy, requests animation 4 and advances the
/// sub-state.
static void func_actor_342400_8016B500(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* work3;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((func_actor_342400_8016945C(arg0) << 0x10) == 0) {
        work2 = (Actor341700Work*)arg0->work;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_412  = 0;
            work->field_438  = 1;
            work3            = (Actor341700Work*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 4;
            work3->field_414 = 1;
            work->field_422  = work->field_422 + 1;
        }
    }
}

static void func_actor_342400_8016B5B0(Task* arg0)
{
    Actor341700Work* work;
    GfxCoord*        coords;
    GfxCoord*        current;
    SVECTOR*         pos;
    SVECTOR          local;
    VECTOR           result;
    s32              flag;

    work   = (Actor341700Work*)arg0->work;
    coords = arg0->extra.tmd->coords;
    SndEvt_EnqueueType7(0x402C0002, 1);
    work->field_90  = coords->coord.t[0];
    work->field_92  = coords->coord.t[1];
    work->field_94  = coords->coord.t[2];
    work->field_412 = 0;
    work->field_428 = 0;
    work->field_42A = 0;
    work->field_422++;
    pos     = &work->field_70;
    pos->vx = pos->vy = pos->vz = 0;
    current                     = &Gp_LookupSlot4(0)->extra.tmd->coords[3];
    local.vx                    = pos->vx;
    local.vy                    = pos->vy;
    local.vz                    = pos->vz;
    while (1) {
        if (current->parent == NULL) {
            return;
        }
        if (current == &gGfxViewCoord) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return;
        }
        gte_SetTransMatrix(&current->coord);
        gte_SetRotMatrix(&current->coord);
        gte_ldv0(&local);
        gte_rtv0tr();
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        current  = current->parent;
    }
}

static void func_actor_342400_8016B744(Task* arg0)
{
    Actor341700Work* work;
    s32              soundId;
    s32              pan;

    work = (Actor341700Work*)arg0->work;
    if (D_actor_342400_80173A84[work->field_418 - 1] == 0) {
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 9;
        work->field_414 = 1;
        soundId         = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0002;
        pan             = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_422 = 4;
        return;
    }
    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 7;
    work->field_414 = 1;
    work->field_44F = (u8)work->field_41C * 4;
    work->field_422++;
}

/// Plays sound 2, releases `Gp_StateF0`'s hold if it is this enemy's,
/// unlinks the enemy node, detaches its records and unlinks its three hit
/// bodies, hides the model and advances the state.
static void func_actor_342400_8016B84C(Task* arg0)
{
    Actor341700Work* work2;
    Actor341700Work* work;
    GpEnemy*         enemy;
    TmdObject*       model;

    work            = (Actor341700Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2.pointer;
    model           = arg0->extra.tmd;
    work->field_412 = 0;
    SndEvt_EnqueueType7(0x402C0002, 1);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    enemy->recs = 0;
    work2       = (Actor341700Work*)arg0->work;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    model->flags    = model->flags | 0x80;
    work->field_420 = work->field_420 + 1;
}

/// On frame 3 frees the model's buffers and sets model flag 4; after 0x24
/// frames destroys the enemy.
static void func_actor_342400_8016B914(Task* arg0)
{
    Actor341700Work* work;
    TmdObject*       model;
    u16              ticks;

    work            = (Actor341700Work*)arg0->work;
    model           = arg0->extra.tmd;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks == 3) {
        Tmd_FreeBuffers(model);
        model->flags |= 4;
    }
    if ((s16)work->field_412 >= 0x24) {
        Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
    }
}

static void func_actor_342400_8016B9A4(Task* arg0)
{
    Actor341700Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2.pointer;
    work  = (Actor341700Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}

static void func_actor_342400_8016BA3C(Task* arg0)
{
    Actor341700Work* work;
    s16              anim;
    s16              next;

    work = (Actor341700Work*)arg0->work;
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 5;
            work->field_414 = 1;
        } else {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 6;
            work->field_414 = 1;
        }
    } else {
        next            = D_actor_342400_80173A98[anim - 1];
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = next;
        work->field_414 = 1;
    }
    func_actor_342400_80165CC0(arg0);
    work->field_420++;
}

/// Ticks the animation and, once the hit flags are set, advances the state.
static void func_actor_342400_8016BAF4(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    func_actor_342400_80165CC0(arg0);
    work2 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}

/// Detaches the enemy's records and unlinks its three hit bodies, clears the
/// frame counter and advances the state.
static void func_actor_342400_8016BB74(Task* arg0)
{
    Actor341700Work* work2;
    Actor341700Work* work;

    work                                      = (Actor341700Work*)arg0->work;
    ((GpEnemy*)arg0->spawnArg2.pointer)->recs = 0;
    work2                                     = (Actor341700Work*)arg0->work;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

/// Empty state handler.
static void func_actor_342400_8016BBD0(Task* arg0)
{
}

static void func_actor_342400_8016BBD8(Task* arg0)
{
    Actor341700Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2.pointer;
    work  = (Actor341700Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}

/// Starts the death shrink, the same body as `func_actor_342400_8016A884`:
/// detaches the records, unlinks the three hit bodies, sets the Y scale to
/// 1.0, saves the root matrix, sets light mode 1 and advances the state.
static void func_actor_342400_8016BC70(Task* task)
{
    GpEnemy*         enemy = (GpEnemy*)task->spawnArg2.pointer;
    Actor341700Work* work  = (Actor341700Work*)task->work;
    GfxCoord*        coord = task->extra.tmd->coords;
    Actor341700Work* objWork;

    enemy->recs = 0;

    objWork = (Actor341700Work*)task->work;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430    = 0x1000;
    work->savedRootMtx = coord->coord;

    Gp_SetLightMode(task->spawnArg2.pointer, 1);

    work->field_412 = 0;
    work->field_420++;
}

/// After 0x18 frames sets model flag 2, clears the frame counter, sets
/// `field_451` and advances the state.
static void func_actor_342400_8016BD3C(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;
    TmdObject*       model;

    work            = (Actor341700Work*)arg0->work;
    model           = arg0->extra.tmd;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->flags    = model->flags | 2;
        work->field_412 = 0U;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}

/// Death shrink without the effect of `func_actor_342400_80166180`:
/// restores the saved root matrix, scales it on Y by `field_430` (0x40
/// smaller each frame), sets light mode 2 on frame 16, and after frame 32
/// hides the model, clears the frame counter and advances the state.
static void func_actor_342400_8016BD98(Task* arg0)
{
    Actor341700Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    VECTOR           scale;
    OverlayMat       m;
    GpMtxWords*      ident;

    work             = (Actor341700Work*)arg0->work;
    ident            = &m.ident;
    obj              = arg0->extra.tmd;
    coord            = obj->coords;
    work->field_430 -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = (s16)work->field_430;
    scale.vz         = 0x1000;
    coord->coord     = work->savedRootMtx;
    m.ident.m00_m01  = 0x1000;
    m.ident.m02_m10  = 0;
    ident->m11_m12   = 0x1000;
    m.ident.m20_m21  = 0;
    ident->m22       = 0x1000;
    ScaleMatrix(&m.mat, &scale);
    MulMatrix(&coord->coord, &m.mat);
    if ((s16)++work->field_412 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2.pointer, 2);
    }
    if ((s16)work->field_412 > 0x20) {
        obj->flags     |= 0x80;
        work->field_412 = 0;
        work->field_420++;
    }
}

static void func_actor_342400_8016BED8(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

/// While `field_41E` is 1, consumes the pending request in `field_448`:
/// request 3 moves the state machine to state 8 and request 5 to state 9,
/// anything else is just cleared. Returns 1 when `field_41E` is 1 and 0
/// otherwise.
static s32 func_actor_342400_8016BEF0(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (work->field_41E == 1) {
        switch (work->field_448) {
            case 3:
                work->field_420 = 8;
                work->field_422 = 0;
                break;
            case 5:
                work->field_420 = 9;
                work->field_422 = 0;
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}
