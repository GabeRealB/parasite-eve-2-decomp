#include "rooms/dryfield_night_trailer_coach.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "dryfield_night_trailer_coach_private.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(s32, s32, RoomEventMsg*, RoomEventMsg*);
        s32 (*call2)(s32, s32, s32);
    } handler;
} DryfieldNightTrailerCoachMessageEntry;
STATIC_ASSERT_SIZEOF(DryfieldNightTrailerCoachMessageEntry, 8);

/// The "%" suffix the room's percentage formatters append.
static u8 Telephone_Data_80181A78[];

/// The item id the shop list's cursor last rested on.
static s32 Shop_Data_801819EC;

/// Task descriptor table the room's cutscene tasks spawn from: the room spawns
/// entry 0 with a cutscene record as its argument, and
/// `func_dryfield_night_trailer_coach_80181DB0` spawns entry 1 for the scene.
extern TaskDesc D_dryfield_night_trailer_coach_80184FE4[];

#define TELEPHONE_TITLE_BYTES "Telephone\0N\xF2"
#include "../../shared/telephone.h"

/// The 0xFFFF-terminated item id lists `func_dryfield_night_trailer_coach_8017D81C`
/// chooses from.
static u16 Shop_Data_801815F8[];
static u16 Shop_Data_80181600[];
static u16 Shop_Data_80181608[];
static u16 Shop_Data_80181610[];
static u16 Shop_Data_80181620[];
static u16 Shop_Data_80181630[];
static u16 Shop_Data_80181640[];
static u16 Shop_Data_80181648[];
static u16 Shop_Data_80181658[];
static u16 Shop_Data_80181668[];
static u16 Shop_Data_80181678[];
static u16 Shop_Data_80181680[];
static u16 Shop_Data_80181694[];
static u16 Shop_Data_801816AC[];
static u16 Shop_Data_801816C0[];
static u16 Shop_Data_801816C8[];
static u16 Shop_Data_801816D8[];
static u16 Shop_Data_801816F0[];
static u16 Shop_Data_80181704[];
static u16 Shop_Data_8018170C[];
static u16 Shop_Data_80181720[];
static u16 Shop_Data_8018173C[];
static u16 Shop_Data_8018174C[];
static u16 Shop_Data_80181758[];
static u16 Shop_Data_80181770[];
static u16 Shop_Data_8018178C[];
static u16 Shop_Data_801817A0[];
static u16 Shop_Data_801817A8[];
static u16 Shop_Data_801817BC[];
static u16 Shop_Data_801817DC[];
static u16 Shop_Data_801817EC[];
static u16 Shop_Data_801817F8[];
static u16 Shop_Data_80181810[];
static u16 Shop_Data_80181814[];
static u16 Shop_Data_80181818[];
static u16 Shop_Data_80181820[];
static u16 Shop_Data_80181830[];
static u16 Shop_Data_80181838[];
static u16 Shop_Data_80181840[];
static u16 Shop_Data_80181848[];
static u16 Shop_Data_80181854[];
static u16 Shop_Data_8018185C[];
static u16 Shop_Data_80181868[];
static u16 Shop_Data_80181870[];
static u16 Shop_Data_8018187C[];
static u16 Shop_Data_80181888[];
static u16 Shop_Data_80181890[];
static u16 Shop_Data_80181898[];
static u16 Shop_Data_801818A4[];
static u16 Shop_Data_801818B0[];
static u16 Shop_Data_801818B8[];
static u16 Shop_Data_801818C4[];
static u16 Shop_Data_801818D0[];
static u16 Shop_Data_801818DC[];
static u16 Shop_Data_801818E0[];
static u16 Shop_Data_801818EC[];
static u16 Shop_Data_801818F8[];
static u16 Shop_Data_80181904[];
static u16 Shop_Data_8018190C[];
static u16 Shop_Data_80181918[];
static u16 Shop_Data_80181924[];
static u16 Shop_Data_80181930[];
static u16 Shop_Data_80181938[];
static u16 Shop_Data_80181944[];

/// The list returned when no case matches.
static u16 Shop_Data_80181AD4[];

#define SHOP_CHARGE_TITLE_BYTES "Charge\0\xFD"
#include "../../shared/shop.h"

s32 func_dryfield_night_trailer_coach_801826A0(void);
s32 func_dryfield_night_trailer_coach_801826A8(s32, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_trailer_coach_801826EC(s32, s32, s32);
s32 func_dryfield_night_trailer_coach_80182800(void);
s32 func_dryfield_night_trailer_coach_80182808(s32, s32, s32);

void func_dryfield_night_trailer_coach_80181DB0(Task*);
void func_dryfield_night_trailer_coach_8018243C(Task*);
void func_dryfield_night_trailer_coach_80182610(Task*);

extern GpAnimArg D_dryfield_night_trailer_coach_80187A20;
extern GpAnimSet D_dryfield_night_trailer_coach_80185398;
extern GpAnimSet D_dryfield_night_trailer_coach_80185590;
extern GpAnimSet D_dryfield_night_trailer_coach_801858E4;
extern GpAnimSet D_dryfield_night_trailer_coach_80185ADC;
extern GpAnimSet D_dryfield_night_trailer_coach_80185DE0;
extern GpAnimSet D_dryfield_night_trailer_coach_80186208;
extern GpAnimSet D_dryfield_night_trailer_coach_80186844;
extern GpAnimSet D_dryfield_night_trailer_coach_80186B5C;
extern GpAnimSet D_dryfield_night_trailer_coach_80186E88;
extern GpAnimSet D_dryfield_night_trailer_coach_801873D0;
extern GpAnimSet D_dryfield_night_trailer_coach_80187924;

void func_dryfield_night_trailer_coach_8018283C(void);
void func_dryfield_night_trailer_coach_80182864(void);

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc D_dryfield_night_trailer_coach_801846D0 = { 0, 192, Shop_SessionTask, { .model = NULL } };

TmdBone D_dryfield_night_trailer_coach_801846DC[3] = {
#include "assets/dryfield_night_trailer_coach_model_076E0_skeleton.inc"
};

u32 D_dryfield_night_trailer_coach_80184748[3] = {
#include "assets/dryfield_night_trailer_coach_model_076E0_partVerts.inc"
};

SVECTOR D_dryfield_night_trailer_coach_80184754[56] = {
#include "assets/dryfield_night_trailer_coach_model_076E0_verts.inc"
};

SVECTOR D_dryfield_night_trailer_coach_80184914[6] = {
#include "assets/dryfield_night_trailer_coach_model_076E0_normals.inc"
};

u32 D_dryfield_night_trailer_coach_80184944[215] = {
#include "assets/dryfield_night_trailer_coach_model_076E0_stream.inc"
};

TmdSource D_dryfield_night_trailer_coach_80184CA0 = {
    0,
    1768,
    0,
    3,
    D_dryfield_night_trailer_coach_80184748,
    D_dryfield_night_trailer_coach_80184754,
    D_dryfield_night_trailer_coach_80184914,
    D_dryfield_night_trailer_coach_801846DC,
    D_dryfield_night_trailer_coach_80184944,
};

#include "../../shared/telephone_data.inc.c"

TaskDesc D_dryfield_night_trailer_coach_80184FE4[3] = {
    { 0, 32, func_dryfield_night_trailer_coach_80181DB0, { .model = NULL } },
    { 0, 32, func_dryfield_night_trailer_coach_80182610, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

AnimationPackedPose D_dryfield_night_trailer_coach_80185008[3] = {
#include "assets/dryfield_night_trailer_coach_animation_07DD8_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_trailer_coach_8018502C[81] = {
#include "assets/dryfield_night_trailer_coach_animation_07DD8_bank4.inc"
};

AnimationRecord D_dryfield_night_trailer_coach_80185170[128] = {
#include "assets/dryfield_night_trailer_coach_animation_07DD8_records.inc"
};

u16 D_dryfield_night_trailer_coach_80185370[20] = {
#include "assets/dryfield_night_trailer_coach_animation_07DD8_indices.inc"
};

GpAnimSet D_dryfield_night_trailer_coach_80185398 = {
    D_dryfield_night_trailer_coach_80185170,
    D_dryfield_night_trailer_coach_80185370,
    { NULL, D_dryfield_night_trailer_coach_80185008, NULL, NULL, D_dryfield_night_trailer_coach_8018502C, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_trailer_coach_801853C0[3] = {
#include "assets/dryfield_night_trailer_coach_animation_07FD0_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_trailer_coach_801853E4[34] = {
#include "assets/dryfield_night_trailer_coach_animation_07FD0_bank4.inc"
};

AnimationRecord D_dryfield_night_trailer_coach_8018546C[63] = {
#include "assets/dryfield_night_trailer_coach_animation_07FD0_records.inc"
};

u16 D_dryfield_night_trailer_coach_80185568[20] = {
#include "assets/dryfield_night_trailer_coach_animation_07FD0_indices.inc"
};

GpAnimSet D_dryfield_night_trailer_coach_80185590 = {
    D_dryfield_night_trailer_coach_8018546C,
    D_dryfield_night_trailer_coach_80185568,
    { NULL, D_dryfield_night_trailer_coach_801853C0, NULL, NULL, D_dryfield_night_trailer_coach_801853E4, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_trailer_coach_801855B8[5] = {
#include "assets/dryfield_night_trailer_coach_animation_08324_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_trailer_coach_801855F4[58] = {
#include "assets/dryfield_night_trailer_coach_animation_08324_bank4.inc"
};

AnimationRecord D_dryfield_night_trailer_coach_801856DC[120] = {
#include "assets/dryfield_night_trailer_coach_animation_08324_records.inc"
};

u16 D_dryfield_night_trailer_coach_801858BC[20] = {
#include "assets/dryfield_night_trailer_coach_animation_08324_indices.inc"
};

GpAnimSet D_dryfield_night_trailer_coach_801858E4 = {
    D_dryfield_night_trailer_coach_801856DC,
    D_dryfield_night_trailer_coach_801858BC,
    { NULL, D_dryfield_night_trailer_coach_801855B8, NULL, NULL, D_dryfield_night_trailer_coach_801855F4, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_trailer_coach_8018590C[3] = {
#include "assets/dryfield_night_trailer_coach_animation_0851C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_trailer_coach_80185930[34] = {
#include "assets/dryfield_night_trailer_coach_animation_0851C_bank4.inc"
};

AnimationRecord D_dryfield_night_trailer_coach_801859B8[63] = {
#include "assets/dryfield_night_trailer_coach_animation_0851C_records.inc"
};

u16 D_dryfield_night_trailer_coach_80185AB4[20] = {
#include "assets/dryfield_night_trailer_coach_animation_0851C_indices.inc"
};

GpAnimSet D_dryfield_night_trailer_coach_80185ADC = {
    D_dryfield_night_trailer_coach_801859B8,
    D_dryfield_night_trailer_coach_80185AB4,
    { NULL, D_dryfield_night_trailer_coach_8018590C, NULL, NULL, D_dryfield_night_trailer_coach_80185930, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_trailer_coach_80185B04[5] = {
#include "assets/dryfield_night_trailer_coach_animation_08820_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_trailer_coach_80185B40[55] = {
#include "assets/dryfield_night_trailer_coach_animation_08820_bank4.inc"
};

AnimationRecord D_dryfield_night_trailer_coach_80185C1C[103] = {
#include "assets/dryfield_night_trailer_coach_animation_08820_records.inc"
};

u16 D_dryfield_night_trailer_coach_80185DB8[20] = {
#include "assets/dryfield_night_trailer_coach_animation_08820_indices.inc"
};

GpAnimSet D_dryfield_night_trailer_coach_80185DE0 = {
    D_dryfield_night_trailer_coach_80185C1C,
    D_dryfield_night_trailer_coach_80185DB8,
    { NULL, D_dryfield_night_trailer_coach_80185B04, NULL, NULL, D_dryfield_night_trailer_coach_80185B40, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_trailer_coach_80185E08[5] = {
#include "assets/dryfield_night_trailer_coach_animation_08C48_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_trailer_coach_80185E44[90] = {
#include "assets/dryfield_night_trailer_coach_animation_08C48_bank4.inc"
};

AnimationRecord D_dryfield_night_trailer_coach_80185FAC[141] = {
#include "assets/dryfield_night_trailer_coach_animation_08C48_records.inc"
};

u16 D_dryfield_night_trailer_coach_801861E0[20] = {
#include "assets/dryfield_night_trailer_coach_animation_08C48_indices.inc"
};

GpAnimSet D_dryfield_night_trailer_coach_80186208 = {
    D_dryfield_night_trailer_coach_80185FAC,
    D_dryfield_night_trailer_coach_801861E0,
    { NULL, D_dryfield_night_trailer_coach_80185E08, NULL, NULL, D_dryfield_night_trailer_coach_80185E44, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_trailer_coach_80186230[14] = {
#include "assets/dryfield_night_trailer_coach_animation_09284_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_trailer_coach_801862D8[135] = {
#include "assets/dryfield_night_trailer_coach_animation_09284_bank4.inc"
};

AnimationRecord D_dryfield_night_trailer_coach_801864F4[202] = {
#include "assets/dryfield_night_trailer_coach_animation_09284_records.inc"
};

u16 D_dryfield_night_trailer_coach_8018681C[20] = {
#include "assets/dryfield_night_trailer_coach_animation_09284_indices.inc"
};

GpAnimSet D_dryfield_night_trailer_coach_80186844 = {
    D_dryfield_night_trailer_coach_801864F4,
    D_dryfield_night_trailer_coach_8018681C,
    { NULL, D_dryfield_night_trailer_coach_80186230, NULL, NULL, D_dryfield_night_trailer_coach_801862D8, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_trailer_coach_8018686C[7] = {
#include "assets/dryfield_night_trailer_coach_animation_0959C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_trailer_coach_801868C0[54] = {
#include "assets/dryfield_night_trailer_coach_animation_0959C_bank4.inc"
};

AnimationRecord D_dryfield_night_trailer_coach_80186998[103] = {
#include "assets/dryfield_night_trailer_coach_animation_0959C_records.inc"
};

u16 D_dryfield_night_trailer_coach_80186B34[20] = {
#include "assets/dryfield_night_trailer_coach_animation_0959C_indices.inc"
};

GpAnimSet D_dryfield_night_trailer_coach_80186B5C = {
    D_dryfield_night_trailer_coach_80186998,
    D_dryfield_night_trailer_coach_80186B34,
    { NULL, D_dryfield_night_trailer_coach_8018686C, NULL, NULL, D_dryfield_night_trailer_coach_801868C0, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_trailer_coach_80186B84[7] = {
#include "assets/dryfield_night_trailer_coach_animation_098C8_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_trailer_coach_80186BD8[56] = {
#include "assets/dryfield_night_trailer_coach_animation_098C8_bank4.inc"
};

AnimationRecord D_dryfield_night_trailer_coach_80186CB8[106] = {
#include "assets/dryfield_night_trailer_coach_animation_098C8_records.inc"
};

u16 D_dryfield_night_trailer_coach_80186E60[20] = {
#include "assets/dryfield_night_trailer_coach_animation_098C8_indices.inc"
};

GpAnimSet D_dryfield_night_trailer_coach_80186E88 = {
    D_dryfield_night_trailer_coach_80186CB8,
    D_dryfield_night_trailer_coach_80186E60,
    { NULL, D_dryfield_night_trailer_coach_80186B84, NULL, NULL, D_dryfield_night_trailer_coach_80186BD8, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_trailer_coach_80186EB0[8] = {
#include "assets/dryfield_night_trailer_coach_animation_09E10_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_trailer_coach_80186F10[121] = {
#include "assets/dryfield_night_trailer_coach_animation_09E10_bank4.inc"
};

AnimationRecord D_dryfield_night_trailer_coach_801870F4[173] = {
#include "assets/dryfield_night_trailer_coach_animation_09E10_records.inc"
};

u16 D_dryfield_night_trailer_coach_801873A8[20] = {
#include "assets/dryfield_night_trailer_coach_animation_09E10_indices.inc"
};

GpAnimSet D_dryfield_night_trailer_coach_801873D0 = {
    D_dryfield_night_trailer_coach_801870F4,
    D_dryfield_night_trailer_coach_801873A8,
    { NULL, D_dryfield_night_trailer_coach_80186EB0, NULL, NULL, D_dryfield_night_trailer_coach_80186F10, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_trailer_coach_801873F8[9] = {
#include "assets/dryfield_night_trailer_coach_animation_0A364_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_trailer_coach_80187464[106] = {
#include "assets/dryfield_night_trailer_coach_animation_0A364_bank4.inc"
};

AnimationRecord D_dryfield_night_trailer_coach_8018760C[188] = {
#include "assets/dryfield_night_trailer_coach_animation_0A364_records.inc"
};

u16 D_dryfield_night_trailer_coach_801878FC[20] = {
#include "assets/dryfield_night_trailer_coach_animation_0A364_indices.inc"
};

GpAnimSet D_dryfield_night_trailer_coach_80187924 = {
    D_dryfield_night_trailer_coach_8018760C,
    D_dryfield_night_trailer_coach_801878FC,
    { NULL, D_dryfield_night_trailer_coach_801873F8, NULL, NULL, D_dryfield_night_trailer_coach_80187464, NULL, NULL, NULL },
};

DryfieldNightTrailerCoachMessageEntry D_dryfield_night_trailer_coach_8018794C[6] = {
    { 5102, { .call1 = func_dryfield_night_trailer_coach_801826A8 } },
    { 5105, { .call0 = func_dryfield_night_trailer_coach_801826A0 } },
    { 5103, { .call0 = func_dryfield_night_trailer_coach_80182800 } },
    { 5104, { .call2 = func_dryfield_night_trailer_coach_801826EC } },
    { 5106, { .call2 = func_dryfield_night_trailer_coach_80182808 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_dryfield_night_trailer_coach_8018797C = { 0, 32, func_dryfield_night_trailer_coach_8018243C, { .model = NULL } };

GpXformArg D_dryfield_night_trailer_coach_80187988 = { { 4870, 0, -900, 0 }, { 0, -1536, 0, 0 } };

GpXformArg D_dryfield_night_trailer_coach_801879A0 = { { 4600, 0, -1300, 0 }, { 0, -1479, 0, 0 } };

GpXformArg D_dryfield_night_trailer_coach_801879B8 = { { 4400, 0, -2400, 0 }, { 0, 512, 0, 0 } };

GpAnimArg D_dryfield_night_trailer_coach_801879D0 = { { .index = 1 }, 1, 0, 0, 1 };

GpAnimArg D_dryfield_night_trailer_coach_801879E4 = { { .index = 1 }, 47, 1, 8, 0 };

GpAnimArg D_dryfield_night_trailer_coach_801879F8 = { { .index = 1 }, 47, 1, 8, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187A0C = { { .index = 1 }, 52, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187A20 = { { .index = 1 }, 48, 1, 8, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187A34 = { { .index = 1 }, 53, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187A48[4] = {
    { { .index = 1 }, 49, 0, 0, 0 },
    { { .index = 1 }, 50, 0, 0, 0 },
    { { .index = 1 }, 51, 0, 0, 0 },
    { { .index = 1 }, 55, 0, 0, 0 },
};

GpAnimArg D_dryfield_night_trailer_coach_80187A98 = { { .index = 1 }, 56, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187AAC = { { .index = 1 }, 57, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187AC0 = { { .index = 0 }, 0, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187AD4 = { { .index = 0 }, 1, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187AE8[2] = {
    { { .index = 0 }, 2, 0, 0, 0 },
    { { .index = 0 }, 3, 0, 0, 0 },
};

GpAnimArg D_dryfield_night_trailer_coach_80187B10 = { { .index = 0 }, 4, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187B24 = { { .index = 0 }, 5, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187B38 = { { .index = 0 }, 6, 1, 8, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187B4C = { { .index = 0 }, 7, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187B60 = { { .index = 0 }, 8, 1, 8, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187B74 = { { .index = 0 }, 9, 1, 8, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187B88 = { { .index = 0 }, 10, 1, 8, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187B9C[2] = {
    { { .index = 0 }, 8, 1, 0, 0 },
    { { .index = 1 }, 0, 0, 0, 0 },
};

GpAnimArg D_dryfield_night_trailer_coach_80187BC4 = { { .index = 1 }, 1, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187BD8 = { { .index = 1 }, 2, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187BEC = { { .index = 1 }, 3, 1, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187C00[3] = {
    { { .index = 1 }, 4, 1, 0, 0 },
    { { .index = 1 }, 5, 0, 0, 0 },
    { { .index = 1 }, 6, 0, 0, 0 },
};

GpAnimArg D_dryfield_night_trailer_coach_80187C3C = { { .index = 1 }, 7, 1, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187C50 = { { .index = 1 }, 2, 1, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187C64 = { { .index = 2 }, 0, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187C78 = { { .index = 2 }, 1, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187C8C = { { .index = 2 }, 2, 0, 0, 0 };

GpAnimArg D_dryfield_night_trailer_coach_80187CA0 = { { .index = 2 }, 3, 1, 0, 0 };

GpAnimSet* D_dryfield_night_trailer_coach_80187CB4[12] = {
    &D_dryfield_night_trailer_coach_801873D0,
    &D_dryfield_night_trailer_coach_80186208,
    &D_dryfield_night_trailer_coach_80185590,
    &D_dryfield_night_trailer_coach_801858E4,
    &D_dryfield_night_trailer_coach_80185ADC,
    &D_dryfield_night_trailer_coach_80187924,
    &D_dryfield_night_trailer_coach_80185DE0,
    &D_dryfield_night_trailer_coach_80185398,
    &D_dryfield_night_trailer_coach_80186844,
    &D_dryfield_night_trailer_coach_80186B5C,
    &D_dryfield_night_trailer_coach_80186E88,
    NULL,
};

GpCopyArg D_dryfield_night_trailer_coach_80187CE4 = { { .sets = D_dryfield_night_trailer_coach_80187CB4 }, 11 };

GpAnimArg D_dryfield_night_trailer_coach_80187CEC = { { .index = 6 }, 34, 0, 0, 0 };

GpEvsCmd D_dryfield_night_trailer_coach_80187D00[25] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_trailer_coach_80187CE4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_trailer_coach_80187988 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187C78 }, { .value = 0 } },
    { 15, { .value = 0x531B0008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187C8C }, { .value = 0 } },
    { 15, { .value = 0x531B0009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_80187A20 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187BC4 }, { .value = 0 } },
    { 4, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187C50 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B24 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_trailer_coach_80187F58[14] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_trailer_coach_80187988 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B24 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_trailer_coach_801880A8[14] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B38 }, { .value = 0 } },
    { 4, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B4C }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B60 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_trailer_coach_8018283C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_trailer_coach_801881F8[14] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackResult = func_800D4D2C }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B74 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B10 }, { .value = 0 } },
    { 4, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B24 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_trailer_coach_80188348[19] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B38 }, { .value = 0 } },
    { 4, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B4C }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B60 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_trailer_coach_80182864 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B74 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B10 }, { .value = 0 } },
    { 4, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B24 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_trailer_coach_80188510[21] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 24 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_trailer_coach_801879D0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B38 }, { .value = 0 } },
    { 4, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187BC4 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187BD8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187C3C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_trailer_coach_80182864 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B74 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B10 }, { .value = 0 } },
    { 4, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_trailer_coach_80187B24 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

/// Texts and panel descriptors of the shop list's two special rows (ids
/// 0xFFFE and 0xFFFC) and of the panel a bought item opens.
static u8 Shop_Data_80181A20[];

static u8 Shop_Data_80181A0C[];

static UiObjectDesc Shop_Data_80181BD8;

static u8 Shop_Data_80181A1C[];

static UiObjectDesc Shop_Data_80181B84;

static RoomShopTier Shop_Data_80181950[13];

/// Messages and labels of the shop's panels.
static u8 Shop_Data_801819F0[];

static u8 Shop_Data_80181A04[];

static u8 Shop_Data_80181A5C[];

static u8 Shop_Data_80181A64[];

static u8 Shop_Data_80181A70[];

static u8 Shop_Data_80181A78[];

static u8 Shop_Data_80181A80[];

static u8 Shop_Data_80181A94[];

static u8 Shop_Data_80181AA4[];

static u8 Shop_Data_80181AC4[];

static u8 Shop_Data_80181AD0[];

/// Row handlers, lists and panel descriptors of the shop's panels.
static UiListItemFunc Shop_Data_80181AD8[];

static UiList Shop_Data_80181AE0;

static UiList Shop_Data_80181B0C;

static UiObjectDesc Shop_Data_80181B4C;

static UiObjectDesc Shop_Data_80181B68;

static UiObjectDesc Shop_Data_80181BA0;

static UiObjectDesc Shop_Data_80181BF4;

static UiObjectDesc Shop_Data_80181C10;

/// Descriptor of the panel `func_dryfield_night_trailer_coach_8017FEC0` opens.
static UiObjectDesc Shop_Data_80181B30;

static u8 Telephone_Data_80181A20[];

static u8 Telephone_Data_80181A28[];

static u8 Telephone_Data_80181A2C[];

static u8 Telephone_Data_80181A34[];

static u8 Telephone_Data_80181A40[];

static u8 Telephone_Data_80181A50[];

static u8 Telephone_Data_80181A58[];

static u8 Telephone_Data_80181A60[];

static u8 Telephone_Data_80181A68[];

static u8 Telephone_Data_80181A70[];

static u8 Telephone_Data_80181A7C[];

static u8 Telephone_Data_80181AA8[];

static u8 Telephone_Data_80181ACC[];

static u8 Telephone_Data_80181AFC[];

static u8 Telephone_Data_80181B30[];

static u8 Telephone_Data_80181B64[];

static u8 Telephone_Data_80181B9C[];

static u8 Telephone_Data_80181BD0[];

static u8 Telephone_Data_80181C08[];

static const char Telephone_Data_8017D638[];

extern UiObjectDesc D_800611E4;

/// Lists of the usage panel and of the play-data menu, and the descriptor of
/// the frame the usage panel spawns.
static UiList Telephone_Data_80181C6C;

static UiList Telephone_Data_80181CF4;

static UiObjectDesc Telephone_Data_80181C90;

/// List of the menu panel `func_dryfield_night_trailer_coach_80181844` draws.
static UiList Telephone_Data_80181C44;

/// Texts of the four menu rows below, and the panels two of them open.
static u8 Telephone_Data_801819F8[];

static u8 Telephone_Data_80181A00[];

static u8 Telephone_Data_80181A0C[];

static u8 Telephone_Data_80181A18[];

static UiObjectDesc Telephone_Data_80181CAC;

static UiObjectDesc Telephone_Data_80181CC8;

extern DryfieldNightTrailerCoachMessageEntry D_dryfield_night_trailer_coach_8018794C[6];

extern GpXformArg D_dryfield_night_trailer_coach_801879B8;

extern GpAnimArg D_dryfield_night_trailer_coach_80187CEC;

extern GpEvsCmd D_dryfield_night_trailer_coach_80187D00[];

extern GpEvsCmd D_dryfield_night_trailer_coach_80187F58[];

extern GpEvsCmd D_dryfield_night_trailer_coach_801880A8[];

extern GpEvsCmd D_dryfield_night_trailer_coach_801881F8[];

extern GpEvsCmd D_dryfield_night_trailer_coach_80188348[];

extern GpEvsCmd D_dryfield_night_trailer_coach_80188510[];

static void func_dryfield_night_trailer_coach_8018231C(Task* task);

static void func_dryfield_night_trailer_coach_80182898(Task* task);

/// Cutscene trigger for the trailer coach at night. Where the day version has
/// its own record and a save-view reset, this one only runs at the two ends of
/// the visit.
///
/// Request 0xE forces area 8 for the scene, fills the room's cutscene record
/// the same way the motel lobby fills its own -- save view 8, slot 1, and the
/// cap file picked by `GameFlag_GetNibble(0x7A)` (file 1 below four, file 2 at
/// four or more) -- then hands it to `D_dryfield_night_trailer_coach_80184FE4`. Request 3
/// spawns entry 0 of the room's task table at `0x8018797C` and request 0x17
/// asks the cap system to run command 0x17. Always returns 0.
extern TaskDesc D_dryfield_night_trailer_coach_8018797C;

static inline s32 Shop_AddItemCount(s32 item, s32 count);

#include "../../shared/shop.inc.c"

#undef SHOP_CHARGE_TITLE_BYTES

#include "../../shared/telephone.inc.c"

void func_dryfield_night_trailer_coach_8018138C(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

/// The area records applied when a scene ends with game-flag nibble 0x7A at 1,
/// nibble 0 at 2 and the save's location at 0x0101 in its upper half.

/// The room's cutscene runner: suppresses the player and ally HUD, loads and
/// starts the scene's caption slot, lets confirm or cancel cut the scene
/// sub-task short, applies the story-flag side effects when the scene ends,
/// and restores everything before killing itself.
void func_dryfield_night_trailer_coach_80181DB0(Task* task)
{
    RoomCutsceneRec* rec;
    s32              killOut;
    s32              flag;
    s32              cmd;
    s32              fadeA;
    s32              fadeB;

    rec = (RoomCutsceneRec*)task->spawnArg2.pointer;
    switch (task->state) {
        case 0:
            D_dryfield_night_trailer_coach_8018C218 = NULL;
            Gp_MsgPlayerWeapon(0);
            if (Mc_SaveData[0].state.companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (rec->field_0 > 0) {
                D_80115694                        = Mc_SaveData[0].state.at4.loc.view;
                Mc_SaveData[0].state.at4.loc.view = rec->field_0;
            } else {
                D_80115694 = -rec->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (rec->field_4 != 0) {
                SndEvt_EnqueueType6(rec->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (rec->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(rec->field_3);
                fadeB = 0;
                fadeA = rec->field_14;
                if (fadeA == 0) {
                    fadeA = 0x3C0;
                } else {
                    fadeB = rec->field_16;
                }
                func_800E6D4C(fadeA, fadeB);
            }
            if (rec->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_dryfield_night_trailer_coach_8018C218 = Task_SpawnFromTable(D_dryfield_night_trailer_coach_80184FE4, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_dryfield_night_trailer_coach_8018C218);
                task->state++;
            } else if (Task_PollKill(D_dryfield_night_trailer_coach_8018C218, &killOut) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (rec->field_2 == 0) {
                SndEvt_EnqueueType6(rec->field_C, 0, 0);
            }
            flag = GameFlag_GetNibble(0x7A);
            if (flag > 0) {
                if (flag >= 5) {
                    if (flag == 5) {
                        if (GameFlag_GetNibble(0x111) != 0) {
                            if (GameFlag_GetNibble(0x112) == 0) {
                                GameFlag_SetNibble(3, 0);
                                GameFlag_SetNibble(0x155, 9);
                                GameFlag_SetNibble(0x112, 1);
                            }
                        }
                    }
                }
            }
            if (rec->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(rec->field_1, 0);
            }
            if (GameFlag_GetNibble(0x7A) == 1) {
                if (GameFlag_GetNibble(0) == 2) {
                    GameFlag_SetNibble(0, 3);
                    GameFlag_SetNibble(0xE, 4);
                    if ((GAME_LOCATION_WORD(Mc_SaveData[0].state.at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 1, 0, 0)) {
                        Gp_ApplyAreaRecs(D_acropolis_square_80188888);
                        func_800E3FAC(0xA2, 5);
                    }
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((GameFlag_GetNibble(0x155) == 0xE) && (GameFlag_GetNibble(3) == 0)) {
                    GameFlag_SetNibble(3, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1.value);
                    task->state++;
                }
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            Mc_SaveData[0].state.at4.loc.view = D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(rec->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData[0].state.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (rec->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            taskKill(task);
            break;
        case 20:
            Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    GameFlag_SetNibble(3, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

// Message-table callbacks use the argument views required by this TU.

/// State handlers of the room's task `func_dryfield_night_trailer_coach_801828CC`
/// runs: its set-up, a per-frame state and the kill.
static const TaskFuncTable3 D_dryfield_night_trailer_coach_8017D7DC = {
    {
        func_dryfield_night_trailer_coach_8018231C,
        func_dryfield_night_trailer_coach_80182898,
        taskKill,
    },
};

static void func_dryfield_night_trailer_coach_8018231C(Task* task)
{
    s32* state;

    task->msgTable = D_dryfield_night_trailer_coach_8018794C;
    Game_SetPtrSlot(task, 7);
    if (gameGetPtrSlot(0xA) != NULL) {
        Gp_DispatchMsgPtr(gameGetPtrSlot(0xA), 0x3E9, &D_dryfield_night_trailer_coach_801879B8, 0);
        Gp_DispatchMsgPtr(gameGetPtrSlot(0xA), 0x3E8, &D_dryfield_night_trailer_coach_80187CEC, 0);
    }
    if (Mc_SaveData[0].state.at4.loc.warp == 2) {
        func_800E8634(D_dryfield_night_trailer_coach_80187D00, 0, D_dryfield_night_trailer_coach_80187F58);
    }
    if (Mc_SaveData[0].state.at4.loc.warp == 3) {
        func_800E8634(D_dryfield_night_trailer_coach_80189080, 0, D_dryfield_night_trailer_coach_801892C0);
        Gp_SetCurBit2Flag(0x22, 1);
    }
    if (func_800E3FCC(0xA2) == 0x25) {
        func_800E3FAC(0xA2, 0x26);
    }
    gDisplayState.otDepthShift = 3;
    /* Through a pointer rather than as `task->state`: a member access is
       struct memory, which the scheduler lets pass the store above it, and the
       original keeps the two in source order. */
    state   = &task->state;
    *state += 1;
}

void func_dryfield_night_trailer_coach_8018243C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            if (GameFlag_GetNibble(0x61) == 0) {
                func_800E8614(D_dryfield_night_trailer_coach_801880A8, 1);
                task->state++;
                break;
            }
            if (GameFlag_GetNibble(0xE0) == 0 && GameFlag_GetNibble(0x7A) >= 4) {
                GameFlag_SetNibble(0xE0, 1);
                func_800E8614(D_dryfield_night_trailer_coach_80188510, 0);
            } else {
                func_800E8614(D_dryfield_night_trailer_coach_80188348, 0);
            }
            taskKill(task);
            break;
        case 2:
            if (Gp_GetCapEventKey() == 0xB) {
                func_800E8614(D_dryfield_night_trailer_coach_801881F8, 0);
            } else if (Gp_GetCapEventKey() == 0xC) {
                func_800E8614(D_dryfield_night_trailer_coach_80188708, 1);
            } else if (Gp_GetCapEventKey() == 0xD) {
                if (GameFlag_GetNibble(0x5B) == 0) {
                    GameFlag_SetNibble(0x5B, 1);
                    GameFlag_SetNibble(0x4C, 0);
                    Gp_ApplyAreaRecs(D_dryfield_night_trailer_coach_8018C208);
                    func_800E8634(D_dryfield_night_trailer_coach_801889A8, 1,
                                  D_dryfield_night_trailer_coach_80188F00);
                    func_800E3FAC(0xA2, 0x13);
                    Mc_SaveData[0].state.sceneEvent = 2;
                } else {
                    func_800E8614(D_dryfield_night_trailer_coach_80188858, 1);
                }
            }
            task->state++;
            break;
        case 1:
        case 3:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case 4:
            taskKill(task);
            break;
    }
}

/// Plays the sound event in `spawnArg2` on its first frame and again at frame
/// 0x50, then asks for the task's own kill at frame 0x78.
void func_dryfield_night_trailer_coach_80182610(Task* task)
{
    switch (task->state) {
        case 0x50:
        case 0x0:
            SndEvt_EnqueueType6(task->spawnArg2.value, 0, 0);
            task->state += 1;
            break;
        case 0x78:
            Task_RequestKill(task, 0);
            break;
        default:
            task->state += 1;
            break;
    }
}

s32 func_dryfield_night_trailer_coach_801826A0(void)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// forwards both to `func_map_dryfield_full_80179954`. Always returns 1.
s32 func_dryfield_night_trailer_coach_801826A8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_dryfield_full_80179954(in, out);
    return 1;
}

s32 func_dryfield_night_trailer_coach_801826EC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xE) {
        Mc_SaveData[0].state.at4.loc.warp               = 1;
        D_dryfield_night_trailer_coach_8018C21C.field_0 = 8;
        D_dryfield_night_trailer_coach_8018C21C.field_1 = 1;
        if (GameFlag_GetNibble(0x7A) < 4) {
            D_dryfield_night_trailer_coach_8018C21C.field_14 = 0x380;
            D_dryfield_night_trailer_coach_8018C21C.field_3  = 1;
        } else {
            D_dryfield_night_trailer_coach_8018C21C.field_14 = 0x3C0;
            D_dryfield_night_trailer_coach_8018C21C.field_3  = 2;
        }
        D_dryfield_night_trailer_coach_8018C21C.field_2  = 0;
        D_dryfield_night_trailer_coach_8018C21C.field_4  = 0x531B0003;
        D_dryfield_night_trailer_coach_8018C21C.field_8  = 0x531B0005;
        D_dryfield_night_trailer_coach_8018C21C.field_10 = 0x531B0004;
        D_dryfield_night_trailer_coach_8018C21C.field_C  = 0x531B0006;
        Task_SpawnFromTable(D_dryfield_night_trailer_coach_80184FE4, 0, 3, &D_dryfield_night_trailer_coach_8018C21C);
    }
    if (arg2 == 3) {
        Task_SpawnFromTable(&D_dryfield_night_trailer_coach_8018797C, 0, 0, 0);
    }
    if (arg2 == 0x17) {
        Gp_RunCapCmd1(0x17);
    }
    return 0;
}

s32 func_dryfield_night_trailer_coach_80182800(void)
{
    return 0;
}

s32 func_dryfield_night_trailer_coach_80182808(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        Gp_EnqueueStageSnd6(0x531B000D, 0, 0);
    }
    return 0;
}

void func_dryfield_night_trailer_coach_8018283C(void)
{
    Gp_StartCapSlot(9, 0, 1);
}

void func_dryfield_night_trailer_coach_80182864(void)
{
    func_800D4D2C((GameFlag_GetNibble(0xE0) == 0) ? 0x20 : 0x21);
}

static void func_dryfield_night_trailer_coach_80182898(Task* task)
{
    char pad[0x10];

    if (Mc_SaveData[0].state.at4.loc.view == 5) {
        gDisplayState.otDepthShift = 0;
    } else {
        gDisplayState.otDepthShift = 3;
    }
}

/// Runs the handler for the task's state from a stack copy of
/// `D_dryfield_night_trailer_coach_8017D7DC`.
void func_dryfield_night_trailer_coach_801828CC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_trailer_coach_8017D7DC;
    sp.funcs[task->state](task);
}
