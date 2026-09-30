#include "rooms/shelter_b1_pod_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern SVECTOR D_shelter_b1_pod_access_tunnel_80183A04[2];

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b1_pod_access_tunnel_80184D0C[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u8 D_shelter_b1_pod_access_tunnel_80184D0C_value __asm__("D_shelter_b1_pod_access_tunnel_80184D0C");

/// Work block of the task that scrolls one full-screen image vertically into
/// another. Its first state allocates it zeroed and sets `speed`; the drawing
/// state advances `offset` and places the seam between the images from it.
typedef struct {
    s32 speed;  ///< 16.16 per-frame step, sized so the scroll completes in the spawn argument's frame count
    s32 offset; ///< 16.16 scroll distance; its integer part is clamped to 240 lines
    s16 timer;  ///< Frames drawn so far; scrolling starts once it reaches 46
} _ShelterB1PodAccessTunnelWork;
STATIC_ASSERT_SIZEOF(_ShelterB1PodAccessTunnelWork, 0xC);

extern TaskDesc D_801348D8;

/// Descriptor of the event task the message handler spawns.
extern TaskDesc D_shelter_b1_pod_access_tunnel_801810CC;

/// The room's message table.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, RoomEventMsg*, RoomEventMsg*);
        s32 (*call2)(s32, s32, s32);
    } handler;
} ShelterB1PodAccessTunnelMessageEntry;
STATIC_ASSERT_SIZEOF(ShelterB1PodAccessTunnelMessageEntry, 8);

extern ShelterB1PodAccessTunnelMessageEntry D_shelter_b1_pod_access_tunnel_801810D8[6];

extern TaskDesc D_shelter_b1_pod_access_tunnel_80181108[];
extern GpEvsCmd D_shelter_b1_pod_access_tunnel_80181120[];
extern TaskDesc D_shelter_b1_pod_access_tunnel_801811C8;
extern TaskDesc D_shelter_b1_pod_access_tunnel_80182D2C[];
extern GpEvsCmd D_shelter_b1_pod_access_tunnel_80182FFC[];
extern GpEvsCmd D_shelter_b1_pod_access_tunnel_8018380C[];

/// Points the room task draws its bands between, depending on the view.
extern SVECTOR D_shelter_b1_pod_access_tunnel_801839A4[];
extern SVECTOR D_shelter_b1_pod_access_tunnel_801839E4[];

/// The two points of the twin trail, as offsets from its anchor frame. The
/// second is also reached under its own name.

extern RoomFadeStorage  D_shelter_b1_pod_access_tunnel_80184CFC;
extern RoomEventMsg     D_shelter_b1_pod_access_tunnel_80184D04;
extern RoomLatchedEvent D_shelter_b1_pod_access_tunnel_80184D10;

static void func_shelter_b1_pod_access_tunnel_8017DE10(Task* arg0);
static void func_shelter_b1_pod_access_tunnel_8017DED8(Task* task);
static void func_shelter_b1_pod_access_tunnel_8017E048(Task* task);
static void func_shelter_b1_pod_access_tunnel_8017E5B4(Task* task);
static void func_shelter_b1_pod_access_tunnel_8017E66C(s32 tpage, s16 arg1);
static void func_shelter_b1_pod_access_tunnel_8017E8F4(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b1_pod_access_tunnel_8017F3DC(GfxCoord* coord, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b1_pod_access_tunnel_8017F808(GfxCoord* coord, s16 arg1, u8* rgb);
static void func_shelter_b1_pod_access_tunnel_8018008C(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_shelter_b1_pod_access_tunnel_8018070C(GfxCoord* coord, s16 arg1, u8* rgb);

void func_shelter_b1_pod_access_tunnel_8017DF40(Task*);

void func_shelter_b1_pod_access_tunnel_8017DA74(Task*);
void func_shelter_b1_pod_access_tunnel_8017DC18(Task*);

s32  func_shelter_b1_pod_access_tunnel_8017D7B4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b1_pod_access_tunnel_8017DD68(void);
s32  func_shelter_b1_pod_access_tunnel_8017DD70(s32, s32, s32);
s32  func_shelter_b1_pod_access_tunnel_8017DDD8(void);
s32  func_shelter_b1_pod_access_tunnel_8017DDE0(s32, s32, s32);
void func_shelter_b1_pod_access_tunnel_8017D61C(Task*);

void func_shelter_b1_pod_access_tunnel_8017E44C(Task*);
void func_shelter_b1_pod_access_tunnel_8017E55C(Task*);
void func_shelter_b1_pod_access_tunnel_8017E778(Task*);

extern AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182D8C;
extern AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DA0;
extern AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DB4;
extern AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DC8;
extern AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DDC;
extern AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DF0;
extern AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182E04;
extern GpCopyArg            D_shelter_b1_pod_access_tunnel_80182D70;
extern GpXformArg           D_shelter_b1_pod_access_tunnel_80182E18;
extern GpXformArg           D_shelter_b1_pod_access_tunnel_80182E30;
extern GpXformArg           D_shelter_b1_pod_access_tunnel_80182E48;
void                        func_shelter_b1_pod_access_tunnel_8017E39C(void);
void                        func_shelter_b1_pod_access_tunnel_8017E3BC(void);
void                        func_shelter_b1_pod_access_tunnel_8017E3DC(void);
void                        func_shelter_b1_pod_access_tunnel_8017E3FC(void);
void                        func_shelter_b1_pod_access_tunnel_8017E41C(s32);
void                        func_shelter_b1_pod_access_tunnel_8017E52C(s32);
void                        func_shelter_b1_pod_access_tunnel_8017E704(void);
void                        func_shelter_b1_pod_access_tunnel_8017E734(s32);
void                        func_shelter_b1_pod_access_tunnel_8017E7B4(void);

TaskDesc D_shelter_b1_pod_access_tunnel_801810CC = { 0, 32, func_shelter_b1_pod_access_tunnel_8017D61C, { .model = NULL } };

ShelterB1PodAccessTunnelMessageEntry D_shelter_b1_pod_access_tunnel_801810D8[6] = {
    { 5102, { .call1 = func_shelter_b1_pod_access_tunnel_8017D7B4 } },
    { 5105, { .call0 = func_shelter_b1_pod_access_tunnel_8017DD68 } },
    { 5103, { .call0 = func_shelter_b1_pod_access_tunnel_8017DDD8 } },
    { 5104, { .call2 = func_shelter_b1_pod_access_tunnel_8017DD70 } },
    { 5106, { .call2 = func_shelter_b1_pod_access_tunnel_8017DDE0 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_shelter_b1_pod_access_tunnel_80181108[2] = {
    { 0, 32, func_shelter_b1_pod_access_tunnel_8017DA74, { .model = NULL } },
    { 0, 32, func_shelter_b1_pod_access_tunnel_8017DC18, { .model = NULL } },
};

GpEvsCmd D_shelter_b1_pod_access_tunnel_80181120[7] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_shelter_b1_pod_access_tunnel_801811C8 = { 0, 192, func_shelter_b1_pod_access_tunnel_8017DF40, { .model = NULL } };

AnimationPackedPose D_shelter_b1_pod_access_tunnel_801811D4[6] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_bank1.inc"
};

AnimationPackedRotation D_shelter_b1_pod_access_tunnel_8018121C[46] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_bank4.inc"
};

AnimationRecord D_shelter_b1_pod_access_tunnel_801812D4[109] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_records.inc"
};

u16 D_shelter_b1_pod_access_tunnel_80181488[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_indices.inc"
};

AnimationSet D_shelter_b1_pod_access_tunnel_801814B0 = {
    D_shelter_b1_pod_access_tunnel_801812D4,
    D_shelter_b1_pod_access_tunnel_80181488,
    { NULL, D_shelter_b1_pod_access_tunnel_801811D4, NULL, NULL, D_shelter_b1_pod_access_tunnel_8018121C, NULL, NULL, NULL },
};

AnimationPackedPose D_shelter_b1_pod_access_tunnel_801814D8[4] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_bank1.inc"
};

AnimationPackedRotation D_shelter_b1_pod_access_tunnel_80181508[49] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_bank4.inc"
};

AnimationRecord D_shelter_b1_pod_access_tunnel_801815CC[92] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_records.inc"
};

u16 D_shelter_b1_pod_access_tunnel_8018173C[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_indices.inc"
};

AnimationSet D_shelter_b1_pod_access_tunnel_80181764 = {
    D_shelter_b1_pod_access_tunnel_801815CC,
    D_shelter_b1_pod_access_tunnel_8018173C,
    { NULL, D_shelter_b1_pod_access_tunnel_801814D8, NULL, NULL, D_shelter_b1_pod_access_tunnel_80181508, NULL, NULL, NULL },
};

AnimationPackedPose D_shelter_b1_pod_access_tunnel_8018178C[14] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_bank1.inc"
};

AnimationPackedRotation D_shelter_b1_pod_access_tunnel_80181834[159] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_bank4.inc"
};

AnimationRecord D_shelter_b1_pod_access_tunnel_80181AB0[241] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_records.inc"
};

u16 D_shelter_b1_pod_access_tunnel_80181E74[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_indices.inc"
};

AnimationSet D_shelter_b1_pod_access_tunnel_80181E9C = {
    D_shelter_b1_pod_access_tunnel_80181AB0,
    D_shelter_b1_pod_access_tunnel_80181E74,
    { NULL, D_shelter_b1_pod_access_tunnel_8018178C, NULL, NULL, D_shelter_b1_pod_access_tunnel_80181834, NULL, NULL, NULL },
};

AnimationPackedPose D_shelter_b1_pod_access_tunnel_80181EC4[4] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_bank1.inc"
};

AnimationPackedRotation D_shelter_b1_pod_access_tunnel_80181EF4[35] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_bank4.inc"
};

AnimationRecord D_shelter_b1_pod_access_tunnel_80181F80[110] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_records.inc"
};

u16 D_shelter_b1_pod_access_tunnel_80182138[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_indices.inc"
};

AnimationSet D_shelter_b1_pod_access_tunnel_80182160 = {
    D_shelter_b1_pod_access_tunnel_80181F80,
    D_shelter_b1_pod_access_tunnel_80182138,
    { NULL, D_shelter_b1_pod_access_tunnel_80181EC4, NULL, NULL, D_shelter_b1_pod_access_tunnel_80181EF4, NULL, NULL, NULL },
};

AnimationPackedPose D_shelter_b1_pod_access_tunnel_80182188[2] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_bank1.inc"
};

AnimationPackedRotation D_shelter_b1_pod_access_tunnel_801821A0[29] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_bank4.inc"
};

AnimationRecord D_shelter_b1_pod_access_tunnel_80182214[94] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_records.inc"
};

u16 D_shelter_b1_pod_access_tunnel_8018238C[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_indices.inc"
};

AnimationSet D_shelter_b1_pod_access_tunnel_801823B4 = {
    D_shelter_b1_pod_access_tunnel_80182214,
    D_shelter_b1_pod_access_tunnel_8018238C,
    { NULL, D_shelter_b1_pod_access_tunnel_80182188, NULL, NULL, D_shelter_b1_pod_access_tunnel_801821A0, NULL, NULL, NULL },
};

AnimationPackedPose D_shelter_b1_pod_access_tunnel_801823DC[10] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_bank1.inc"
};

AnimationPackedRotation D_shelter_b1_pod_access_tunnel_80182454[135] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_bank4.inc"
};

AnimationRecord D_shelter_b1_pod_access_tunnel_80182670[168] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_records.inc"
};

u16 D_shelter_b1_pod_access_tunnel_80182910[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_indices.inc"
};

AnimationSet D_shelter_b1_pod_access_tunnel_80182938 = {
    D_shelter_b1_pod_access_tunnel_80182670,
    D_shelter_b1_pod_access_tunnel_80182910,
    { NULL, D_shelter_b1_pod_access_tunnel_801823DC, NULL, NULL, D_shelter_b1_pod_access_tunnel_80182454, NULL, NULL, NULL },
};

AnimationPackedPose D_shelter_b1_pod_access_tunnel_80182960[6] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_bank1.inc"
};

AnimationPackedRotation D_shelter_b1_pod_access_tunnel_801829A8[64] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_bank4.inc"
};

AnimationRecord D_shelter_b1_pod_access_tunnel_80182AA8[141] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_records.inc"
};

u16 D_shelter_b1_pod_access_tunnel_80182CDC[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_indices.inc"
};

AnimationSet D_shelter_b1_pod_access_tunnel_80182D04 = {
    D_shelter_b1_pod_access_tunnel_80182AA8,
    D_shelter_b1_pod_access_tunnel_80182CDC,
    { NULL, D_shelter_b1_pod_access_tunnel_80182960, NULL, NULL, D_shelter_b1_pod_access_tunnel_801829A8, NULL, NULL, NULL },
};

TaskDesc D_shelter_b1_pod_access_tunnel_80182D2C[3] = {
    { 0, 192, func_shelter_b1_pod_access_tunnel_8017E44C, { .model = NULL } },
    { 0, 192, func_shelter_b1_pod_access_tunnel_8017E55C, { .model = NULL } },
    { 0, 192, func_shelter_b1_pod_access_tunnel_8017E778, { .model = NULL } },
};

AnimationSet* D_shelter_b1_pod_access_tunnel_80182D50[8] = {
    NULL,
    &D_shelter_b1_pod_access_tunnel_801814B0,
    &D_shelter_b1_pod_access_tunnel_80181764,
    &D_shelter_b1_pod_access_tunnel_80181E9C,
    &D_shelter_b1_pod_access_tunnel_80182160,
    &D_shelter_b1_pod_access_tunnel_801823B4,
    &D_shelter_b1_pod_access_tunnel_80182938,
    &D_shelter_b1_pod_access_tunnel_80182D04,
};

GpCopyArg D_shelter_b1_pod_access_tunnel_80182D70 = { { .sets = D_shelter_b1_pod_access_tunnel_80182D50 }, 8 };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182D78 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182D8C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DA0 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DB4 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DC8 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DDC = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DF0 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182E04 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

GpXformArg D_shelter_b1_pod_access_tunnel_80182E18 = { { 1740, 0, -5620, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_shelter_b1_pod_access_tunnel_80182E30 = { { 1690, 0, -6060, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_shelter_b1_pod_access_tunnel_80182E48 = { { 2350, 0, -4800, 0 }, { 0, 0, 0, 0 } };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182E60[4] = {
    { { .index = 0 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182EB0 = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182EC4 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182ED8 = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182EEC = { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182F00 = { { .index = 0 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182F14 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182F28 = { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

GpXformArg D_shelter_b1_pod_access_tunnel_80182F3C = { { 1760, 0, -5100, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_shelter_b1_pod_access_tunnel_80182F54 = { { 1740, 0, -3970, 0 }, { 0, 2047, 0, 0 } };

GpXformArg D_shelter_b1_pod_access_tunnel_80182F6C = { { 4000, 0, -1650, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_shelter_b1_pod_access_tunnel_80182F84 = { { 6700, 0, -1650, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_shelter_b1_pod_access_tunnel_80182F9C = { { 7100, 0, 2920, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_shelter_b1_pod_access_tunnel_80182FB4 = { { 6930, 0, 3710, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_shelter_b1_pod_access_tunnel_80182FCC = { { 1760, 0, -4100, 0 }, { 0, 0, 0, 0 } };

GpCmdArg D_shelter_b1_pod_access_tunnel_80182FE4 = { { .loc = { 4, 17 } }, 1 };

GpCmdArg D_shelter_b1_pod_access_tunnel_80182FE8 = { { .loc = { 4, 17 } }, 2 };

GpSpawnAnimArg D_shelter_b1_pod_access_tunnel_80182FEC = { 10, 3 };

GpOverlayIds D_shelter_b1_pod_access_tunnel_80182FF4 = { 4, 10, 11 };

GpEvsCmd D_shelter_b1_pod_access_tunnel_80182FFC[86] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182D70 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 12, { .overlays = &D_shelter_b1_pod_access_tunnel_80182FF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E39C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182E04 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182E18 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E3BC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182DA0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182E30 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b1_pod_access_tunnel_8017E734 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182FE4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182F3C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182FCC }, { .storage = &D_shelter_b1_pod_access_tunnel_80182FEC } },
    { 4, { .value = 110 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182EB0 }, { .value = 0 } },
    { 4, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182EC4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E704 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182F54 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182ED8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182FE8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182F6C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182F84 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182EEC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182F9C }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b1_pod_access_tunnel_8017E52C }, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b1_pod_access_tunnel_8017E41C }, { .value = 150 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182F00 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182FB4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182F14 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182DB4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182E48 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 17, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 17, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182DC8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182DDC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182DF0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182D8C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E3DC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_shelter_b1_pod_access_tunnel_8018380C[17] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b1_pod_access_tunnel_80182E48 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E3FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E7B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_shelter_b1_pod_access_tunnel_801839A4[8] = {
    { 6073, -2726, -1860, 0 },
    { 5073, -2726, -1860, 0 },
    { 6073, -2726, -1628, 0 },
    { 5073, -2726, -1628, 0 },
    { 3262, -2726, -1860, 0 },
    { 2262, -2726, -1860, 0 },
    { 3262, -2726, -1628, 0 },
    { 2262, -2726, -1628, 0 },
};

SVECTOR D_shelter_b1_pod_access_tunnel_801839E4[4] = {
    { 1859, -2726, -4216, 0 },
    { 1859, -2726, -5216, 0 },
    { 1621, -2726, -4216, 0 },
    { 1621, -2726, -5216, 0 },
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
SVECTOR D_shelter_b1_pod_access_tunnel_80183A04[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

u8* D_shelter_b1_pod_access_tunnel_80183A14[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b1_pod_access_tunnel_80183A18[1] = {
    { { .bytes = { 14, 0 } } },
};

GpWarpRec D_shelter_b1_pod_access_tunnel_80183A1C[3] = {
    { { .words = { 0, 1698, 0, -6028 } }, { 0, 0, 0, 0 }, { .words = { 0, 1698, 0, -6028 } }, { 0, 0, 0, 0 }, 0x54110002, 0x54110001, 0, 2, 0, 0 },
    { { .words = { 3072, 6834, 0, -1772 } }, { 0, 0, 0, 0 }, { .words = { 3840, 6150, 0, -2450 } }, { 0, 0, 0, 0 }, 0x54110005, 0x54110004, 0x54110003, 4, 0, 450 },
    { { .words = { 2048, 4530, 0, -1050 } }, { 0, 0, 0, 0 }, { .words = { 3328, 5350, 0, -1750 } }, { 0, 0, 0, 0 }, 0x54110007, 0, 0, 4, 0, 438 },
};

SVECTOR D_shelter_b1_pod_access_tunnel_80183AC4[6] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_normals.inc"
};

SVECTOR D_shelter_b1_pod_access_tunnel_80183AF4[12] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_verts.inc"
};

GpGridFace D_shelter_b1_pod_access_tunnel_80183B54[10] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_faces.inc"
};

s16 D_shelter_b1_pod_access_tunnel_80183BCC[36] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b1_pod_access_tunnel_80183BCC[i])
s16* D_shelter_b1_pod_access_tunnel_80183C14[4] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b1_pod_access_tunnel_80183C24 = { NULL, D_shelter_b1_pod_access_tunnel_80183AC4, D_shelter_b1_pod_access_tunnel_80183AF4, D_shelter_b1_pod_access_tunnel_80183B54, D_shelter_b1_pod_access_tunnel_80183C14, -750, 6500, 2, 2, 4000, 10 };

GpViewRec D_shelter_b1_pod_access_tunnel_80183C48[14] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4000, 0x37D3, 2550 } }, 296 },
    { { { { -3999, 0, -882 }, { -236, 3946, 1070 }, { 850, 1096, -3853 } }, { -839, 2300, 805 } }, 230 },
    { { { { 3745, 0, -1658 }, { -128, 4083, -289 }, { 1653, 316, 3734 } }, { -769, 1450, 6475 } }, 230 },
    { { { { -824, 0, -4012 }, { 670, 4038, -137 }, { 3955, -684, -812 } }, { -759, 700, 995 } }, 225 },
    { { { { -1017, 0, -3967 }, { -2801, 2900, 718 }, { 2809, 2891, -720 } }, { -539, 2450, 5405 } }, 246 },
    { { { { -4005, 0, -854 }, { 48, 4089, -228 }, { 852, -233, -3999 } }, { -1167, 1170, 3136 } }, 447 },
    { { { { 4052, 0, -596 }, { 24, 4092, 167 }, { 595, -168, 4048 } }, { -1429, 960, 6385 } }, 230 },
    { { { { -4007, 0, -844 }, { 555, 3083, -2637 }, { 635, -2695, -3017 } }, { -1539, 160, 5455 } }, 230 },
    { { { { 4095, 0, -11 }, { 0, 4091, 189 }, { 11, -189, 4091 } }, { -1749, 950, 4365 } }, 230 },
    { { { { -1919, 0, -3618 }, { 554, 4047, -294 }, { 3575, -627, -1897 } }, { -3880, 991, 576 } }, 257 },
    { { { { 4089, 0, -229 }, { 137, 3281, 2447 }, { 183, -2450, 3276 } }, { -6550, 170, -1790 } }, 207 },
    { { { { -4019, 0, -790 }, { -621, 2528, 3162 }, { 487, 3222, -2480 } }, { -6710, 2630, -5070 } }, 329 },
    { { { { -3158, 0, -2608 }, { 29, 4095, -35 }, { 2607, -45, -3158 } }, { -1280, 1287, 2923 } }, 447 },
    { { { { 3985, 0, -946 }, { -429, 3649, -1809 }, { 843, 1859, 3550 } }, { -4527, 1724, 1418 } }, 251 },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_80183E40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_80183E50[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_pod_access_tunnel_80183E60[68] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, -16, 1004, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, -48, 991, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, -80, 980, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, 24, 825, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 112, 40, 825, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, -72, 825, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, -56, 825, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, -56, 825, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -56, 825, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, -32, 825, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 112, -16, 825, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 136, -16, 825, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 0, 825, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 0, 825, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 0, 825, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -8, 875, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -32, 875, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -64, 875, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, -64, 875, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 64, 40, 830, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 40, 835, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 40, 898, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 40, 1001, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 8, 982, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 8, 883, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, 8, 830, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 88, 8, 837, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 104, 40, 850, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 8, 850, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 8, 875, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 8, 875, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 128, 40, 875, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 40, 875, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, 128, -88, 875, { .fields = { 24, 64 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 24 } }, 128, -64, 875, { .fields = { 24, 88 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, 128, -40, 875, { .fields = { 112, 32 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 8 } }, 128, -24, 875, { .fields = { 96, 248 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, 128, -16, 875, { .fields = { 104, 152 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 24 } }, 128, 0, 875, { .fields = { 8, 224 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 24 } }, 96, 0, 875, { .fields = { 16, 24 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 24 } }, 96, -24, 875, { .fields = { 16, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 8 } }, 96, -32, 875, { .fields = { 24, 248 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, 96, -48, 875, { .fields = { 112, 0 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, 96, -64, 875, { .fields = { 112, 16 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 24 } }, 96, -88, 875, { .fields = { 8, 200 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, 64, -88, 850, { .fields = { 120, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 24 } }, 64, -72, 850, { .fields = { 0, 112 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, 64, -48, 850, { .fields = { 120, 184 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 88, -32, 850, { .fields = { 88, 32 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 64, -32, 850, { .fields = { 80, 128 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 80, -32, 850, { .fields = { 88, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 80, 0, 850, { .fields = { 8, 48 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 64, 0, 837, { .fields = { 16, 136 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 40, -88, 964, { .fields = { 72, 192 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 48, -88, 915, { .fields = { 72, 224 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 56, -88, 903, { .fields = { 88, 64 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 40, -56, 938, { .fields = { 96, 160 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 48, -56, 922, { .fields = { 96, 120 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 56, -56, 909, { .fields = { 104, 208 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 56, -16, 917, { .fields = { 104, 168 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 48, -16, 931, { .fields = { 112, 208 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 40, -16, 933, { .fields = { 120, 208 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 32, 64 } }, 128, 40, 640, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 32, 56 } }, 96, 48, 728, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 24, 48 } }, 72, 56, 814, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 32 } }, 56, 56, 906, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 24 } }, 48, 56, 973, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 16 } }, 32, 56, 992, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_801843B0[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 12, 0, 0, { 4, 0 } },
    { 15, 4, 0, 0, { 3, 0 } },
    { 19, 14, 0, 0, { 5, 0 } },
    { 33, 29, 0, 0, { 2, 0 } },
    { 62, 6, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_pod_access_tunnel_801843F0[19] = {
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -8, -32, 1629, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, 24, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 8, 0, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -72, -72, 0, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 72, 685, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 40, 695, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 8, 709, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 8, 675, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -24, 711, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, -24, 725, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -48, 730, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -72, 739, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, -24, 723, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -72, 734, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -120, -72, 665, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, -24, 660, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 8, 604, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 40, 684, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 72, 661, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_8018456C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 18, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_8018458C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_8018459C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_801845AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_801845BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_801845CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_801845DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_801845EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_801845FC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_8018460C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_pod_access_tunnel_8018461C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b1_pod_access_tunnel_8018462C[14] = {
    { { .empty = D_shelter_b1_pod_access_tunnel_80183E40 }, D_shelter_b1_pod_access_tunnel_80183E40, NULL },
    { { .empty = D_shelter_b1_pod_access_tunnel_80183E50 }, D_shelter_b1_pod_access_tunnel_80183E50, NULL },
    { { .elements = D_shelter_b1_pod_access_tunnel_80183E60 }, D_shelter_b1_pod_access_tunnel_801843B0, NULL },
    { { .elements = D_shelter_b1_pod_access_tunnel_801843F0 }, D_shelter_b1_pod_access_tunnel_8018456C, NULL },
    { { .empty = D_shelter_b1_pod_access_tunnel_8018458C }, D_shelter_b1_pod_access_tunnel_8018458C, NULL },
    { { .empty = D_shelter_b1_pod_access_tunnel_8018459C }, D_shelter_b1_pod_access_tunnel_8018459C, NULL },
    { { .empty = D_shelter_b1_pod_access_tunnel_801845AC }, D_shelter_b1_pod_access_tunnel_801845AC, NULL },
    { { .empty = D_shelter_b1_pod_access_tunnel_801845BC }, D_shelter_b1_pod_access_tunnel_801845BC, NULL },
    { { .empty = D_shelter_b1_pod_access_tunnel_801845CC }, D_shelter_b1_pod_access_tunnel_801845CC, NULL },
    { { .empty = D_shelter_b1_pod_access_tunnel_801845DC }, D_shelter_b1_pod_access_tunnel_801845DC, NULL },
    { { .empty = D_shelter_b1_pod_access_tunnel_801845EC }, D_shelter_b1_pod_access_tunnel_801845EC, NULL },
    { { .empty = D_shelter_b1_pod_access_tunnel_801845FC }, D_shelter_b1_pod_access_tunnel_801845FC, NULL },
    { { .empty = D_shelter_b1_pod_access_tunnel_8018460C }, D_shelter_b1_pod_access_tunnel_8018460C, NULL },
    { { .empty = D_shelter_b1_pod_access_tunnel_8018461C }, D_shelter_b1_pod_access_tunnel_8018461C, NULL },
};

GpPointLight D_shelter_b1_pod_access_tunnel_801846D4[1] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1299, -2529, -3253 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 9581, 0x3D41 },
};

GpRoomCoordSet D_shelter_b1_pod_access_tunnel_80184734 = { 0, NULL, 1, D_shelter_b1_pod_access_tunnel_801846D4, 0, NULL };

GpObj4C D_shelter_b1_pod_access_tunnel_8018474C[4] = {
    { NULL, NULL, NULL, { 1296, -1536, -3328, 0 }, { { -1936, -2016, 672, 0 }, { 1936, -2016, -672, 0 }, { -1936, 2016, 672, 0 }, { 1936, 2016, -672, 0 } }, { -1344, 0, -3872, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 1168, -1536, -3408, 0 }, { { 2080, -2016, -752, 0 }, { -2080, -2016, 752, 0 }, { 2080, 2016, -752, 0 }, { -2080, 2016, 752, 0 } }, { 1395, 0, 3860, 0 }, { 0, 0, 4096, 0 }, 2985, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 3136, -1536, -1312, 0 }, { { -119, -2016, -1591, 0 }, { 109, -2016, 1584, 0 }, { -119, 2016, -1591, 0 }, { 109, 2016, 1584, 0 } }, { 4095, 0, -295, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 3265, -1504, -1567, 0 }, { { 84, -2016, 1287, 0 }, { -98, -2016, -1299, 0 }, { 84, 2016, 1287, 0 }, { -98, 2016, -1299, 0 } }, { -4094, 0, 287, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 3, 4, 129, 0 },
};

GpObj3A D_shelter_b1_pod_access_tunnel_8018487C[1] = {
    { NULL, NULL, { 5024, -1296, -5152, 0 }, { { -2064, 2320, 2208, 0 }, { 2064, 2320, -2208, 0 }, { -2064, -2320, 2208, 0 }, { 2064, -2320, -2208, 0 } }, { 2999, 0, 2803, 0 }, { -35, 14 }, 129, 0 },
};

GpObj4C D_shelter_b1_pod_access_tunnel_801848B8[3] = {
    { NULL, NULL, NULL, { 1856, -48, -6496, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 0, 16, 18, 2, 0 },
    { NULL, NULL, NULL, { 7264, -48, -1760, 0 }, { { -704, 0, -1024, 0 }, { 704, 0, -1024, 0 }, { -704, 0, 1024, 0 }, { 704, 0, 1024, 0 } }, { 0, 4094, 0, 0 }, { -4096, 0, 0, 0 }, 1241, 0, 47, 33, 2, 0 },
    { NULL, NULL, NULL, { 4721, -64, -672, 0 }, { { -880, 0, -480, 0 }, { 880, 0, -480, 0 }, { -880, 0, 480, 0 }, { 880, 0, 480, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 1001, 2, 1, 255, 130, 0 },
};

GpAreaTmdRec D_shelter_b1_pod_access_tunnel_8018499C[2] = {
    { 132, 410, 0, 0, { 0, 0 }, D_8013D77C },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_pod_access_tunnel_801849B4[2] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_pod_access_tunnel_801849CC[2] = {
    { 11, 11, 0, 0, { 0, 0 }, D_80147400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_pod_access_tunnel_801849E4[2] = {
    { 49, 49, 0, 0, { 0, 0 }, D_80147400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_pod_access_tunnel_801849FC[2] = {
    { 23, 23, 0, 0, { 0, 0 }, D_80147AB8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_pod_access_tunnel_80184A14[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 23, 23, 1, 0, { 0, 0 }, D_8015FAB8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_pod_access_tunnel_80184A38[2] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_pod_access_tunnel_80184A50[2] = {
    { 132, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_pod_access_tunnel_80184A70[5] = {
    { 21, 0, 0, 730, -2400, -1050, 1024, 0, 0, 2, 0 },
    { 21, 0, 0, 730, -2400, -2400, 1024, 0, 0, 2, 0 },
    { 21, 10, 1024, 1000, -2200, -4750, 1024, 0, 0, 2, 0 },
    { 21, 10, 1024, 2750, -2200, -4750, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_pod_access_tunnel_80184AC0[3] = {
    { 11, 1, 0, 1750, 0, -1900, 1024, 0, 0, 2, 0 },
    { 11, 1, 0, 6950, 0, -2450, 3800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_pod_access_tunnel_80184AF0[3] = {
    { 49, 1, 0, 3500, 0, -2300, 3750, 0, 0, 2, 0 },
    { 49, 1, 0, 1750, 0, -2500, 2048, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_pod_access_tunnel_80184B20[2] = {
    { 23, 5, 1, 1600, 0, -1800, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_pod_access_tunnel_80184B40[5] = {
    { 21, 1, 0, 1200, -2400, -700, 2048, 0, 0, 2, 3 },
    { 21, 1, 0, 2000, -2400, -700, 2048, 0, 0, 2, 3 },
    { 21, 1, 0, 2800, -2400, -700, 2048, 0, 0, 2, 3 },
    { 23, 4, 1, 5700, 0, -1700, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_pod_access_tunnel_80184B90[7] = {
    { 21, 4, 0, 1200, -2400, -700, 2048, 0, 0, 2, 3 },
    { 21, 4, 0, 2000, -2400, -700, 2048, 0, 0, 2, 3 },
    { 21, 4, 0, 2800, -2400, -700, 2048, 0, 0, 2, 3 },
    { 21, 4, 0, 1200, -1800, -700, 2048, 0, 0, 2, 3 },
    { 21, 4, 0, 2000, -1800, -700, 2048, 0, 0, 2, 3 },
    { 21, 4, 0, 2800, -1800, -700, 2048, 0, 0, 2, 3 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b1_pod_access_tunnel_80184C00[23] = {
    { NULL, NULL },
    { D_shelter_b1_pod_access_tunnel_80184A50, D_shelter_b1_pod_access_tunnel_8018499C },
    { D_shelter_b1_pod_access_tunnel_80184A70, D_shelter_b1_pod_access_tunnel_801849B4 },
    { D_shelter_b1_pod_access_tunnel_80184AC0, D_shelter_b1_pod_access_tunnel_801849CC },
    { D_shelter_b1_pod_access_tunnel_80184AF0, D_shelter_b1_pod_access_tunnel_801849E4 },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_pod_access_tunnel_80184B20, D_shelter_b1_pod_access_tunnel_801849FC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_pod_access_tunnel_80184B40, D_shelter_b1_pod_access_tunnel_80184A14 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_pod_access_tunnel_80184B90, D_shelter_b1_pod_access_tunnel_80184A38 },
    { NULL, NULL },
};

s32 D_shelter_b1_pod_access_tunnel_80184CB8[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

GpRoomParamRec D_shelter_b1_pod_access_tunnel_80184CC4[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b1_pod_access_tunnel_80184CCC[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_shelter_b1_pod_access_tunnel_80184CD4[1] = {
    { 0, 0, 1, 0, D_shelter_b1_pod_access_tunnel_80184CB8 },
};

GpRoomParamRec* D_shelter_b1_pod_access_tunnel_80184CDC[8] = {
    D_shelter_b1_pod_access_tunnel_80184CC4,
    D_shelter_b1_pod_access_tunnel_80184CCC,
    D_shelter_b1_pod_access_tunnel_80184CD4,
    D_shelter_b1_pod_access_tunnel_80184CC4,
    D_shelter_b1_pod_access_tunnel_80184CC4,
    D_shelter_b1_pod_access_tunnel_80184CC4,
    D_shelter_b1_pod_access_tunnel_80184CC4,
    D_shelter_b1_pod_access_tunnel_80184CC4,
};

RoomFadeStorage D_shelter_b1_pod_access_tunnel_80184CFC = { 0 };

RoomEventMsg D_shelter_b1_pod_access_tunnel_80184D04 = { 0 };

u8 D_shelter_b1_pod_access_tunnel_80184D0C[4] = {
    0,
    63,
    253,
    225,
};

RoomLatchedEvent D_shelter_b1_pod_access_tunnel_80184D10;

static __inline__ s32 _shelterB1PodAccessTunnelStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);

/// The room's event task, spawned when the message handler latches an event.
/// State 0 runs the latched event's CAP command; state 1 waits for it to
/// finish and, when the event asks for it, starts helper task 0x31; states 2
/// and 3 play the event's stage sound, if any, and wait for it; state 4 writes
/// the latched message's destination into the save data and hands over to
/// task type 0x11.
void func_shelter_b1_pod_access_tunnel_8017D61C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b1_pod_access_tunnel_80184D10.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b1_pod_access_tunnel_80184D10.fade != 0) {
                    D_shelter_b1_pod_access_tunnel_80184CFC.fade.field_0 = 0;
                    D_shelter_b1_pod_access_tunnel_80184CFC.fade.field_1 = 0;
                    D_shelter_b1_pod_access_tunnel_80184CFC.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_shelter_b1_pod_access_tunnel_80184CFC.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b1_pod_access_tunnel_80184D10.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b1_pod_access_tunnel_80184D10.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b1_pod_access_tunnel_80184D10.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b1_pod_access_tunnel_80184D04.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b1_pod_access_tunnel_80184D04.field_2;
            Mc_SaveData[0].state.at4.loc.room = D_shelter_b1_pod_access_tunnel_80184D04.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

static __inline__ s32 _shelterB1PodAccessTunnelStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b1_pod_access_tunnel_80184D0C_value = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b1_pod_access_tunnel_80184D04 = *dst;
            D_shelter_b1_pod_access_tunnel_80184D10 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_801810CC, 0, 0, 0);
            D_shelter_b1_pod_access_tunnel_80184D0C_value = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_b1_pod_access_tunnel_8017D7B4(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->prefix.packed == 0x2F) {
        if (GameFlag_GetNibble(0x118) == 2) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(8);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x7A) >= 6) {
            if (in->field_5 == 0) {
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(D_shelter_b1_pod_access_tunnel_80181108, 1, 0, 0);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0xB3) == 0) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(3);
            }
            return 0;
        }
        if (in->field_5 == 0 && GameFlag_GetNibble(0xD1) == 0 && GameFlag_GetNibble(0x83) == 0) {
            Gp_FillAllyHp();
            GameFlag_SetNibble(0xD1, 1);
            GameFlag_SetNibble(0x4C, 7);
        }
    }
    if (in->prefix.packed == 0x10) {
        if (GameFlag_GetNibble(0x118) == 2) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(8);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0xD1) == 2) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(4);
            }
            return 2;
        }
        event.capCmd   = 6;
        event.stageSnd = 0x54110001;
        event.flagId   = 0x130;
        event.fade     = 0;
        return _shelterB1PodAccessTunnelStartEvent(out, &event);
    }
    return 1;
}

/// The room task's three states: set-up, idle and exit.
static const TaskFuncTable3 D_shelter_b1_pod_access_tunnel_8017D5D8 = {
    { func_shelter_b1_pod_access_tunnel_8017DE10, func_shelter_b1_pod_access_tunnel_8017DED8, taskKill },
};

void func_shelter_b1_pod_access_tunnel_8017DA74(Task* task)
{
    s32 var_v0;
    s32 room;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(GameFlag_GetNibble(0xFC) != 0 ? 5 : 1);
            Gp_StateF0.field_4 = 1;
            goto L_advance;
        case 1:
            var_v0 = Gp_CapBusy();
            goto L_idle;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                if (Gp_GetCapEventKey() == 1) {
                    GameFlag_SetNibble(0x1B6, 2);
                }
                Gp_StateF0.field_4 = 0;
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            GameFlag_SetNibble(0x1B6, 0);
            SndEvt_EnqueueType6(0x54110006, 0, 0);
            goto L_advance;
        case 3:
            var_v0 = SndVoice_HasActiveId(0x54110006);
        L_idle:
            if (var_v0 != 0) {
                return;
            }
        L_advance:
            task->state++;
            return;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData[0].state.at4.loc.area = 0x23;
            Mc_SaveData[0].state.at4.loc.warp = 3;
            Mc_SaveData[0].state.at4.loc.room = 1;
            room                              = GameFlag_GetNibble(0x118);
            if (room == 2) {
                Mc_SaveData[0].state.at4.loc.room = room;
            }
            gDisplayState.spriteVariant = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

void func_shelter_b1_pod_access_tunnel_8017DC18(Task* task)
{
    s32 var_v0;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(9);
            Gp_StateF0.field_4 = 1;
            goto L_advance;
        case 1:
            var_v0 = Gp_CapBusy();
            goto L_idle;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                Gp_StateF0.field_4 = 0;
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            SndEvt_EnqueueType6(0x54110004, 0, 0);
            goto L_advance;
        case 3:
            var_v0 = SndVoice_HasActiveId(0x54110004);
        L_idle:
            if (var_v0 != 0) {
                return;
            }
        L_advance:
            task->state++;
            return;
        case 4:
            GameFlag_SetNibble(0xB4, 1);
            GameFlag_SetNibble(0x1C1, 0);
            Mc_SaveData[0].state.sceneEvent   = 0x1C;
            Mc_SaveData[0].state.at4.loc.area = 0x17;
            Mc_SaveData[0].state.at4.loc.warp = 1;
            Mc_SaveData[0].state.at4.loc.room = 1;
            gDisplayState.spriteVariant       = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_shelter_b1_pod_access_tunnel_8017DD68(void)
{
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DD70(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x12F) != 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_pod_access_tunnel_80181108, 0, 0, 0);
        } else {
            GameFlag_SetNibble(0x12F, 1);
            Gp_RunCapCmd1(0xA);
        }
    }
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DDD8(void)
{
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DDE0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 6) {
        SndEvt_EnqueueType6(0x16, 0, 0);
    }
    return 0;
}

static void func_shelter_b1_pod_access_tunnel_8017DE10(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_pod_access_tunnel_801810D8;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x118) == 1) {
        Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_801811C8, 0, 0, 0);
        GameFlag_SetNibble(0x118, 2);
        func_800E3FAC(0xA2, 0x37);
    } else if (GameFlag_GetNibble(0x7E) == 0) {
        func_800E8634(D_shelter_b1_pod_access_tunnel_80182FFC, 0, D_shelter_b1_pod_access_tunnel_8018380C);
        func_800E3FAC(0xA2, 0x1E);
        GameFlag_SetNibble(0x7E, 1);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The idle state of the room's task-state table: it does nothing.
static void func_shelter_b1_pod_access_tunnel_8017DED8(Task* task)
{
    char pad[0x10];
}

/// Runs the room task through its state table, copied onto the stack first and
/// indexed by the task's state.
void func_shelter_b1_pod_access_tunnel_8017DEE8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_pod_access_tunnel_8017D5D8;
    sp.funcs[task->state](task);
}

/// Two-state task: state 0, unless blocked by `Gp_StateC08.field_A` or `gDisplayState.pendingMode`,
/// sends the slot-3 task a `AnimationPlayRequest` built from `Player_Status.weapon` (msg 0x3E8) and runs
/// `D_shelter_b1_pod_access_tunnel_80181120` through `func_800E8614`; state 1
/// sets `Mc_SaveData[0].state.sceneEvent` to 0x1D and kills this task once the session is idle.
void func_shelter_b1_pod_access_tunnel_8017DF40(Task* task)
{
    AnimationPlayRequest rec;
    s32                  state;
    s32                  weaponId;
    s32                  id;

    state = task->state;
    switch (state) {
        case 0:
            if (Gp_StateC08.field_A != 1 && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                weaponId                 = Player_Status.weapon;
                id                       = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                rec.source.index         = id;
                rec.animationId          = 1;
                rec.blend                = ANIMATION_BLEND_RESET;
                rec.blendFrames          = 0;
                rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &rec, 0);
                func_800E8614(D_shelter_b1_pod_access_tunnel_80181120, 0);
                task->state = task->state + 1;
            }
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                Mc_SaveData[0].state.sceneEvent = 0x1D;
                Task_RequestKill(task, 0);
            }
            break;
    }
}

/// Draw state of the vertical image scroll. After a delay the seam moves down
/// the screen: the image above it slides in from the top, bottom rows first,
/// while the one below is pushed off the bottom. Each image is drawn as two
/// sprites spanning the screen width. Kills the task once `viewReady` is set
/// or no event is running.
static void func_shelter_b1_pod_access_tunnel_8017E048(Task* task)
{
    _ShelterB1PodAccessTunnelWork* work;
    SPRT*                          p;
    s32                            y;

    work = task->work;
    if (gGameSession->viewReady != 0 || gGameSession->eventState == 0) {
        taskKill(task);
        return;
    }
    y = 0;
    if (work->timer++ >= 0x2E) {
        work->offset += work->speed;
        y             = work->offset >> 16;
        if (y > 0xF0) {
            y = 0xF0;
        }
    }

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x100;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = -0x11 - y;
    p->clut = 0x3FC0;
    p->h    = y + 1;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x340, 0);

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    p->x0 = 0x60;
    p->y0 = -0x78;
    p->w  = 0x40;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = -0x11 - y;
    p->clut = 0x3FC0;
    p->h    = y + 1;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x3C0, 0);

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    p->x0 = -0xA0;
    p->w  = 0x100;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = 0;
    p->y0   = y - 0x78;
    p->clut = 0x4000;
    p->h    = 0xF0 - y;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x240, 0x100);

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    p->x0 = 0x60;
    p->w  = 0x40;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = 0;
    p->y0   = y - 0x78;
    p->clut = 0x4000;
    p->h    = 0xF0 - y;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x2C0, 0x100);
}

/// Queues the replacement of overlay 0x82.
void func_shelter_b1_pod_access_tunnel_8017E39C(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Queues the load of overlay 0x81.
void func_shelter_b1_pod_access_tunnel_8017E3BC(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Restores the stream random-number state.
void func_shelter_b1_pod_access_tunnel_8017E3DC(void)
{
    Gp_RestoreStreamRng();
}

/// Cancels the queued overlay replacement and restarts the CD queue.
void func_shelter_b1_pod_access_tunnel_8017E3FC(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_shelter_b1_pod_access_tunnel_8017E41C(s32 arg0)
{
    Task_SpawnFromTable(D_shelter_b1_pod_access_tunnel_80182D2C, 0, arg0, 0);
}

void func_shelter_b1_pod_access_tunnel_8017E44C(Task* task)
{
    GpViewRec* view;
    VECTOR     vec;

    if (task->killCountdown < task->spawnArg1.value && gGameSession->at4.loc.view == 0xB) {
        view   = Gp_GetStageView(&gGameSession->at4.loc);
        vec.vx = 0;
        vec.vy = 0x10;
        vec.vz = 0;
        ApplyTransposeMatrixLV(&view->mtx, &vec, &vec);
        view->mtx.t[0] += vec.vx;
        view->mtx.t[1] += vec.vy;
        view->mtx.t[2] += vec.vz;
        Gp_TrySpawnViewTask(view);
        task->killCountdown++;
        return;
    }
    taskKill(task);
}

void func_shelter_b1_pod_access_tunnel_8017E52C(s32 arg0)
{
    Task_SpawnFromTable(D_shelter_b1_pod_access_tunnel_80182D2C, 1, arg0, 0);
}

/// The image-scroll task's three states: set-up, scroll and exit.
static const TaskFuncTable3 D_shelter_b1_pod_access_tunnel_8017D610 = {
    { func_shelter_b1_pod_access_tunnel_8017E5B4, func_shelter_b1_pod_access_tunnel_8017E048, taskKill },
};

/// Runs the image-scroll task through its state table, copied onto the stack
/// first and indexed by the task's state.
void func_shelter_b1_pod_access_tunnel_8017E55C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_pod_access_tunnel_8017D610;
    sp.funcs[task->state](task);
}

static void func_shelter_b1_pod_access_tunnel_8017E5B4(Task* task)
{
    _ShelterB1PodAccessTunnelWork* work;

    if (gGameSession->at4.loc.view != 0xB) {
        taskKill(task);
        return;
    }
    work = memCalloc(sizeof(_ShelterB1PodAccessTunnelWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = work;
    work->speed  = 0xF00000 / task->spawnArg1.value;
    task->state += 1;
}

/// Append an 8-bit, ABR-0 `DR_TPAGE` for VRAM origin (`tpage`, `arg1`) to OT
/// slot 1023.
static void func_shelter_b1_pod_access_tunnel_8017E66C(s32 tpage, s16 arg1)
{
    DR_TPAGE* p;
    s32       y;

    y              = arg1;
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setDrawTPage(p, 1, 0, getTPage(1, 0, tpage & 0x3C0, y));
    addPrim(gGpuCurrentOt + 1023, p);
}

void func_shelter_b1_pod_access_tunnel_8017E704(void)
{
    Task_SpawnFromTable(&D_801348D8, 0, 0, 0);
}

void func_shelter_b1_pod_access_tunnel_8017E734(s32 arg0)
{
    Display_InitModeObj(Task_GetDescAt(D_shelter_b1_pod_access_tunnel_80182D2C, 2U), arg0, 0, 0x100);
}

/// Counts the spawn argument down one per frame; once it goes negative, kills
/// the task and sets the stage's ending flag.
void func_shelter_b1_pod_access_tunnel_8017E778(Task* arg0)
{
    s32 temp_v0;

    temp_v0               = arg0->spawnArg1.value - 1;
    arg0->spawnArg1.value = temp_v0;
    if (temp_v0 < 0) {
        taskKill(arg0);
        Stage_SetEndingFlag();
    }
}

/// Room callback forwarding to `Gp_PulseState1C`.
void func_shelter_b1_pod_access_tunnel_8017E7B4(void)
{
    Gp_PulseState1C();
}

void func_shelter_b1_pod_access_tunnel_8017E7D4(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        D_80115758  = 0x601CD;
        D_8011572C  = 0x601E9;
        D_80115750  = 0x60205;
        arg0->state = 1;
    }
    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839E4;
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[0], 0x180, 0x111);
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[2], 0x180, 0x111);
        } break;
        case 3:
        case 7: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839A4;
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[0], 0x180, 0x111);
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[2], 0x180, 0x111);
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[4], 0x180, 0x111);
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[6], 0x180, 0x111);
        } break;
        case 4: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839A4;
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[0], 0x180, 0x111);
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[2], 0x180, 0x111);
        } break;
    }
}

/// Draws a gouraud band between the two points `arg0[0]` and `arg0[1]`,
/// projected through `gGfxViewCoord.workm`: a half-disc fan at each end and a
/// quad strip joining them, three `POLY_G4`s per 0x400 step of the screen
/// angle between the two centres. `arg1` scales the radii by depth. The lit
/// vertices take the colour packed 4 bits per channel in `arg2`, with the
/// frame counter's low bit blended in so the band flickers.
static void func_shelter_b1_pod_access_tunnel_8017E8F4(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      conn;
    u8                       r;
    u8                       g;
    u8                       b;

    p1       = arg0 + 1;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (OverlayPointPairScratch*)(head - 0x1C);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)ang;
            blend     = ((u8)ds->animFrame & 1) * 8;
            packed    = arg2 << 16;
            tr        = (packed >> 20) & 0xF0;
            tg        = (packed >> 16) & 0xF0;
            r         = blend | tr;
            g         = blend | tg;
            b         = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = gGpuPrimCursor;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// A flash that swells and then fades. For as many ticks as the spawn argument
/// it brightens and grows two discs and a ring at its frame, all tinted
/// (level, level / 4, level / 2); at full brightness it draws a fade quad, then
/// draws a two-ring billboard that dims by 0x10 a tick and releases its work
/// block once the level falls to 0x10. It pauses while the room's event state
/// is set and releases the block when that state reaches 4.
void func_shelter_b1_pod_access_tunnel_8017F138(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.disp2d->coord;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1.value;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_shelter_b1_pod_access_tunnel_8017F808(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b1_pod_access_tunnel_8017F808(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_b1_pod_access_tunnel_8017F3DC(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_shelter_b1_pod_access_tunnel_8018070C(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows.
static void func_shelter_b1_pod_access_tunnel_8017F3DC(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s16                blackRadius = arg1;
    s16                tintRadius  = arg1 + arg2;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (blackRadius * 64) / block->otz;
        block->rInner = (tintRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows.
static void func_shelter_b1_pod_access_tunnel_8017F808(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(RoomFanScratch);
}

/// A twin trail. The first tick allocates sixteen coordinate frames, eight for
/// each trail, and seeds them all from the two points offset from the anchor,
/// so both trails start collapsed. Each later tick re-places the two points,
/// records them in the next slot of each ring of eight and draws the trails
/// between the rings as a beam. The work block is released once the tick count
/// reaches the spawn argument. It idles while the room's event state is 2 or
/// more.
void func_shelter_b1_pod_access_tunnel_8017FB9C(Task* task)
{
    GfxCoord   coord;
    GfxCoord*  coords;
    GfxCoord*  objCoord;
    GfxCoord*  dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.disp2d->coord;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = memCalloc(sizeof(GfxCoord[16]), 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work             = coords;
                objCoord->parent       = work->parent;
                objCoord->coord.t[0]   = D_shelter_b1_pod_access_tunnel_80183A04[0].vx;
                objCoord->coord.t[1]   = D_shelter_b1_pod_access_tunnel_80183A04[0].vy;
                objCoord->coord.t[2]   = D_shelter_b1_pod_access_tunnel_80183A04[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_shelter_b1_pod_access_tunnel_80183A04[1];
                coord.coord.t[0]   = vec->vx;
                coord.coord.t[1]   = vec->vy;
                coord.coord.t[2]   = vec->vz;
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst         = &coords[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &coords[i + 8];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                coord.parent = work->parent;
                {
                    SVECTOR* edge    = &D_shelter_b1_pod_access_tunnel_80183A04[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                dst         = &coords[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &coords[(work->age & 7) + 8];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &coords[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &coords[i + 8];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                func_shelter_b1_pod_access_tunnel_8018008C(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the beam between two rings of eight coordinate frames as seven
/// gouraud quads, walking back from slot `arg2`, each quad joining two adjacent
/// slots of both rings and dimmer the older it is. `arg3` packs the colour as
/// three multipliers, at bits 8, 4 and 0. A quad whose projection overflows is
/// skipped.
static void func_shelter_b1_pod_access_tunnel_8018008C(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GfxCoord*          a;
    GfxCoord*          b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    blk = SCRATCH_PUSH(RoomDraw03Scratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// A spark burst. The first tick spawns its flash effect; then, for a non-zero
/// spawn argument, it sprays randomly jittered sparks each tick, and for zero
/// it draws a fixed ring and one widening by 0x30 a tick, both dimming by 0x20
/// a tick. Either way it releases its work block after seven ticks. It pauses
/// while the room's event state is set and releases the block when that state
/// reaches 4.
void func_shelter_b1_pod_access_tunnel_80180484(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.disp2d->coord;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_shelter_b1_pod_access_tunnel_8017F3DC(objCoord, 0x100, 0x100, rgb);
            func_shelter_b1_pod_access_tunnel_8017F3DC(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Queues a star-shaped glow at the projected world position of `arg0`: a
/// disc of radius `arg1` scaled by depth, shaded half `arg2` at the centre, an
/// inner disc of half that radius at full `arg2`, and four thin rays at right
/// angles, alternately reaching the radius and twice it, all fading to black
/// at the rim. Nothing is drawn when the projection overflows.
static void func_shelter_b1_pod_access_tunnel_8018070C(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}
