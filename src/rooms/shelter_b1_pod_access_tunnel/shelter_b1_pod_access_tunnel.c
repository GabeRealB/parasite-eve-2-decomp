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
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

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
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b1_pod_access_tunnel_80184D0C[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u8 D_shelter_b1_pod_access_tunnel_80184D0C_value __asm__("D_shelter_b1_pod_access_tunnel_80184D0C");

enum {
    SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_DELAY_FRAMES = 46,  // Frames the lower image is held still before the scroll starts
    SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES        = 240, // Screen lines the seam travels, the height of either image
};

/// Work block of the task that scrolls one full-screen image vertically into
/// another: after a fixed delay the seam between the two moves down the screen
/// at a constant rate.
///
/// The task's set-up state allocates it zeroed and derives `speed` from the
/// spawn argument, a nonzero frame count the scroll is to take; the default
/// teardown frees it.
typedef struct {
    s32 speed;  // Lines the seam moves per frame, 16.16 fixed point
    s32 offset; // Lines scrolled so far, 16.16 fixed point; the seam is drawn at its integer part, capped at the full distance
    s16 timer;  // Frames drawn so far, counted up from 0; scrolling starts once the delay has passed
} _ShelterB1PodAccessTunnelImageScrollWork;
STATIC_ASSERT_SIZEOF(_ShelterB1PodAccessTunnelImageScrollWork, 0xC);

extern TaskDesc D_actor_141000_801348D8[];

/// Descriptor of the event task the message handler spawns.
extern TaskDesc D_shelter_b1_pod_access_tunnel_801810CC;

/// The room's message table.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_shelter_b1_pod_access_tunnel_801810D8[6];

extern TaskDesc   D_shelter_b1_pod_access_tunnel_80181108[];
extern EvsCommand D_shelter_b1_pod_access_tunnel_80181120[];
extern TaskDesc   D_shelter_b1_pod_access_tunnel_801811C8;
extern TaskDesc   D_shelter_b1_pod_access_tunnel_80182D2C[];
extern EvsCommand D_shelter_b1_pod_access_tunnel_80182FFC[];
extern EvsCommand D_shelter_b1_pod_access_tunnel_8018380C[];

/// Points the room task draws its bands between, depending on the view.
extern SVECTOR D_shelter_b1_pod_access_tunnel_801839A4[];
extern SVECTOR D_shelter_b1_pod_access_tunnel_801839E4[];

/// The two points of the twin trail, as offsets from its anchor frame. The
/// second is also reached under its own name.

extern RoomFadeStorage  gRoomEventFade;
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static void func_shelter_b1_pod_access_tunnel_8017DE10(Task* arg0);
static void func_shelter_b1_pod_access_tunnel_8017DED8(Task* task);
static void func_shelter_b1_pod_access_tunnel_8017E048(Task* task);
static void func_shelter_b1_pod_access_tunnel_8017E5B4(Task* task);
static void func_shelter_b1_pod_access_tunnel_8017E66C(s32 tpage, s16 arg1);

void func_shelter_b1_pod_access_tunnel_8017DF40(Task*);

void func_shelter_b1_pod_access_tunnel_8017DA74(Task*);
void func_shelter_b1_pod_access_tunnel_8017DC18(Task*);

s32 func_shelter_b1_pod_access_tunnel_8017D7B4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b1_pod_access_tunnel_8017DD68(Task*, s32, s32, s32);
s32 func_shelter_b1_pod_access_tunnel_8017DD70(Task*, s32, s32, s32);
s32 func_shelter_b1_pod_access_tunnel_8017DDD8(Task*, s32, s32, s32);
s32 func_shelter_b1_pod_access_tunnel_8017DDE0(Task*, s32, s32, s32);

void func_shelter_b1_pod_access_tunnel_8017E44C(Task*);
void func_shelter_b1_pod_access_tunnel_8017E55C(Task*);
void func_shelter_b1_pod_access_tunnel_8017E778(Task*);

extern AnimationPlayRequest     D_shelter_b1_pod_access_tunnel_80182D8C;
extern AnimationPlayRequest     D_shelter_b1_pod_access_tunnel_80182DA0;
extern AnimationPlayRequest     D_shelter_b1_pod_access_tunnel_80182DB4;
extern AnimationPlayRequest     D_shelter_b1_pod_access_tunnel_80182DC8;
extern AnimationPlayRequest     D_shelter_b1_pod_access_tunnel_80182DDC;
extern AnimationPlayRequest     D_shelter_b1_pod_access_tunnel_80182DF0;
extern AnimationPlayRequest     D_shelter_b1_pod_access_tunnel_80182E04;
extern AnimationBankCopyRequest D_shelter_b1_pod_access_tunnel_80182D70;
extern ActorTransform           D_shelter_b1_pod_access_tunnel_80182E18;
extern ActorTransform           D_shelter_b1_pod_access_tunnel_80182E30;
extern ActorTransform           D_shelter_b1_pod_access_tunnel_80182E48;
void                            func_shelter_b1_pod_access_tunnel_8017E39C(void);
void                            func_shelter_b1_pod_access_tunnel_8017E3BC(void);
void                            func_shelter_b1_pod_access_tunnel_8017E3DC(void);
void                            func_shelter_b1_pod_access_tunnel_8017E3FC(void);
void                            func_shelter_b1_pod_access_tunnel_8017E41C(s32);
void                            func_shelter_b1_pod_access_tunnel_8017E52C(s32);
void                            func_shelter_b1_pod_access_tunnel_8017E704(void);
void                            func_shelter_b1_pod_access_tunnel_8017E734(s32);
void                            func_shelter_b1_pod_access_tunnel_8017E7B4(void);

TaskDesc D_shelter_b1_pod_access_tunnel_801810CC = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b1_pod_access_tunnel_801810D8[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_pod_access_tunnel_8017D7B4 },
    { 5105, func_shelter_b1_pod_access_tunnel_8017DD68 },
    { 5103, func_shelter_b1_pod_access_tunnel_8017DDD8 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_pod_access_tunnel_8017DD70 },
    { ROOM_MESSAGE_SOUND, func_shelter_b1_pod_access_tunnel_8017DDE0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b1_pod_access_tunnel_80181108[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_pod_access_tunnel_8017DA74, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_pod_access_tunnel_8017DC18, { .value = 0 } },
};

EvsCommand D_shelter_b1_pod_access_tunnel_80181120[7] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_shelter_b1_pod_access_tunnel_801811C8 = { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_pod_access_tunnel_8017DF40, { .value = 0 } };

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation03EF0Bank1[6] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation03EF0Bank4[46] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation03EF0Records[109] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation03EF0Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation03EF0 = {
    _gShelterB1PodAccessTunnelAnimation03EF0Records,
    _gShelterB1PodAccessTunnelAnimation03EF0Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation03EF0Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation03EF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation041A4Bank1[4] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation041A4Bank4[49] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation041A4Records[92] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation041A4Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation041A4 = {
    _gShelterB1PodAccessTunnelAnimation041A4Records,
    _gShelterB1PodAccessTunnelAnimation041A4Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation041A4Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation041A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation048DCBank1[14] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation048DCBank4[159] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation048DCRecords[241] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation048DCIndices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation048DC = {
    _gShelterB1PodAccessTunnelAnimation048DCRecords,
    _gShelterB1PodAccessTunnelAnimation048DCIndices,
    { NULL, _gShelterB1PodAccessTunnelAnimation048DCBank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation048DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation04BA0Bank1[4] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation04BA0Bank4[35] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation04BA0Records[110] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation04BA0Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation04BA0 = {
    _gShelterB1PodAccessTunnelAnimation04BA0Records,
    _gShelterB1PodAccessTunnelAnimation04BA0Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation04BA0Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation04BA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation04DF4Bank1[2] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation04DF4Bank4[29] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation04DF4Records[94] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation04DF4Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation04DF4 = {
    _gShelterB1PodAccessTunnelAnimation04DF4Records,
    _gShelterB1PodAccessTunnelAnimation04DF4Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation04DF4Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation04DF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation05378Bank1[10] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation05378Bank4[135] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation05378Records[168] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation05378Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation05378 = {
    _gShelterB1PodAccessTunnelAnimation05378Records,
    _gShelterB1PodAccessTunnelAnimation05378Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation05378Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation05378Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation05744Bank1[6] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation05744Bank4[64] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation05744Records[141] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation05744Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation05744 = {
    _gShelterB1PodAccessTunnelAnimation05744Records,
    _gShelterB1PodAccessTunnelAnimation05744Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation05744Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation05744Bank4, NULL, NULL, NULL },
};

TaskDesc D_shelter_b1_pod_access_tunnel_80182D2C[3] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_pod_access_tunnel_8017E44C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_pod_access_tunnel_8017E55C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_pod_access_tunnel_8017E778, { .value = 0 } },
};

AnimationSet* D_shelter_b1_pod_access_tunnel_80182D50[8] = {
    NULL,
    &_gShelterB1PodAccessTunnelAnimation03EF0,
    &_gShelterB1PodAccessTunnelAnimation041A4,
    &_gShelterB1PodAccessTunnelAnimation048DC,
    &_gShelterB1PodAccessTunnelAnimation04BA0,
    &_gShelterB1PodAccessTunnelAnimation04DF4,
    &_gShelterB1PodAccessTunnelAnimation05378,
    &_gShelterB1PodAccessTunnelAnimation05744,
};

AnimationBankCopyRequest D_shelter_b1_pod_access_tunnel_80182D70 = { { .sets = D_shelter_b1_pod_access_tunnel_80182D50 }, ARRAY_SIZE(D_shelter_b1_pod_access_tunnel_80182D50) };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182D78 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182D8C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DA0 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DB4 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DC8 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DDC = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182DF0 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182E04 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_shelter_b1_pod_access_tunnel_80182E18 = { { 1740, 0, -5620, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_shelter_b1_pod_access_tunnel_80182E30 = { { 1690, 0, -6060, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_shelter_b1_pod_access_tunnel_80182E48 = { { 2350, 0, -4800, 0 }, { 0, 0, 0, 0 } };

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

ActorTransform D_shelter_b1_pod_access_tunnel_80182F3C = { { 1760, 0, -5100, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_shelter_b1_pod_access_tunnel_80182F54 = { { 1740, 0, -3970, 0 }, { 0, 2047, 0, 0 } };

ActorTransform D_shelter_b1_pod_access_tunnel_80182F6C = { { 4000, 0, -1650, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_shelter_b1_pod_access_tunnel_80182F84 = { { 6700, 0, -1650, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_shelter_b1_pod_access_tunnel_80182F9C = { { 7100, 0, 2920, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_shelter_b1_pod_access_tunnel_80182FB4 = { { 6930, 0, 3710, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_shelter_b1_pod_access_tunnel_80182FCC = { { 1760, 0, -4100, 0 }, { 0, 0, 0, 0 } };

ActorCommand D_shelter_b1_pod_access_tunnel_80182FE4 = { { .loc = { 4, 17 } }, 1 };

ActorCommand D_shelter_b1_pod_access_tunnel_80182FE8 = { { .loc = { 4, 17 } }, 2 };

ActorMotionWalkAnim D_shelter_b1_pod_access_tunnel_80182FEC = { .animationId = 10, .nextAnimId = 3 };

EvsSceneKey D_shelter_b1_pod_access_tunnel_80182FF4 = { 4, 10, 11 };

EvsCommand D_shelter_b1_pod_access_tunnel_80182FFC[86] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182D70 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_shelter_b1_pod_access_tunnel_80182FF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E39C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182E04 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182E18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E3BC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182DA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182E30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b1_pod_access_tunnel_8017E734 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b1_pod_access_tunnel_80182FE4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182F3C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182FCC } }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182FEC } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 110 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182EB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182EC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E704 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182F54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182ED8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b1_pod_access_tunnel_80182FE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182F6C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182F84 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182EEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182F9C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b1_pod_access_tunnel_8017E52C }, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b1_pod_access_tunnel_8017E41C }, { .value = 150 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182FB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182F14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182DB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182E48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182DC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182DDC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182DF0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_pod_access_tunnel_80182D8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E3DC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b1_pod_access_tunnel_8018380C[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b1_pod_access_tunnel_80182E48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E3FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_pod_access_tunnel_8017E7B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
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

#include "../../shared/room_visual_effects_trail_data.inc.c"

u8* D_shelter_b1_pod_access_tunnel_80183A14[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b1_pod_access_tunnel_80183A18[1] = { 14 };

DirectionWarpEntry D_shelter_b1_pod_access_tunnel_80183A1C[3] = {
    { { { .word = 0 }, 1698, 0, -6028 }, { 0, 0, 0, 0 }, { { .word = 0 }, 1698, 0, -6028 }, { 0, 0, 0, 0 }, 0x54110002, 0x54110001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 6834, 0, -1772 }, { 0, 0, 0, 0 }, { { .word = 3840 }, 6150, 0, -2450 }, { 0, 0, 0, 0 }, 0x54110005, 0x54110004, 0x54110003, 4, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_B2_LABORATORY },
    { { { .word = 2048 }, 4530, 0, -1050 }, { 0, 0, 0, 0 }, { { .word = 3328 }, 5350, 0, -1750 }, { 0, 0, 0, 0 }, 0x54110007, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_POD },
};

static SVECTOR _gShelterB1PodAccessTunnelCollision06664Normals[6] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_normals.inc"
};

static SVECTOR _gShelterB1PodAccessTunnelCollision06664Verts[12] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_verts.inc"
};

static WorldCollisionGridFace _gShelterB1PodAccessTunnelCollision06664Faces[10] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_faces.inc"
};

static s16 _gShelterB1PodAccessTunnelCollision06664Cells[36] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1PodAccessTunnelCollision06664Cells[i])
static s16* _gShelterB1PodAccessTunnelCollision06664Table[4] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_pod_access_tunnel_80183C24 = { NULL, _gShelterB1PodAccessTunnelCollision06664Normals, _gShelterB1PodAccessTunnelCollision06664Verts, _gShelterB1PodAccessTunnelCollision06664Faces, _gShelterB1PodAccessTunnelCollision06664Table, -750, 6500, 2, 2, 4000, 10 };

ViewCamera D_shelter_b1_pod_access_tunnel_80183C48[14] = {
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

SpriteBatch D_shelter_b1_pod_access_tunnel_80183E40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_80183E50[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_pod_access_tunnel_80183E60[68] = {
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
    { 143, 0x4000, { .fields = { 32, 24 } }, 128, -88, 875, { .fields = { 24, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 128, -64, 875, { .fields = { 24, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 128, -40, 875, { .fields = { 112, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, 128, -24, 875, { .fields = { 96, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 128, -16, 875, { .fields = { 104, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 128, 0, 875, { .fields = { 8, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 96, 0, 875, { .fields = { 16, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 96, -24, 875, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, 96, -32, 875, { .fields = { 24, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 96, -48, 875, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 96, -64, 875, { .fields = { 112, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 96, -88, 875, { .fields = { 8, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 64, -88, 850, { .fields = { 120, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 64, -72, 850, { .fields = { 0, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 64, -48, 850, { .fields = { 120, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 88, -32, 850, { .fields = { 88, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, 64, -32, 850, { .fields = { 80, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 80, -32, 850, { .fields = { 88, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, 80, 0, 850, { .fields = { 8, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, 64, 0, 837, { .fields = { 16, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 40, -88, 964, { .fields = { 72, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 48, -88, 915, { .fields = { 72, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 56, -88, 903, { .fields = { 88, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 40, -56, 938, { .fields = { 96, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 48, -56, 922, { .fields = { 96, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 56, -56, 909, { .fields = { 104, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 56, -16, 917, { .fields = { 104, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 48, -16, 931, { .fields = { 112, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 40, -16, 933, { .fields = { 120, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 32, 64 } }, 128, 40, 640, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 32, 56 } }, 96, 48, 728, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 24, 48 } }, 72, 56, 814, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 32 } }, 56, 56, 906, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 24 } }, 48, 56, 973, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 16 } }, 32, 56, 992, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_801843B0[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 12, 0, 0, { 4, 0 } },
    { 15, 4, 0, 0, { 3, 0 } },
    { 19, 14, 0, 0, { 5, 0 } },
    { 33, 29, 0, 0, { 2, 0 } },
    { 62, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_pod_access_tunnel_801843F0[19] = {
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

SpriteBatch D_shelter_b1_pod_access_tunnel_8018456C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_8018458C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_8018459C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_801845AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_801845BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_801845CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_801845DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_801845EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_801845FC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_8018460C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_access_tunnel_8018461C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b1_pod_access_tunnel_8018462C[14] = {
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

WorldCoordPointLight D_shelter_b1_pod_access_tunnel_801846D4[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1299, -2529, -3253 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 9581, 0x3D41 },
};

WorldCoordRoomLights D_shelter_b1_pod_access_tunnel_80184734 = { 0, NULL, ARRAY_SIZE(D_shelter_b1_pod_access_tunnel_801846D4), D_shelter_b1_pod_access_tunnel_801846D4, 0, NULL };

WorldCollisionTrigger D_shelter_b1_pod_access_tunnel_8018474C[4] = {
    { NULL, NULL, NULL, { 1296, -1536, -3328, 0 }, { { -1936, -2016, 672, 0 }, { 1936, -2016, -672, 0 }, { -1936, 2016, 672, 0 }, { 1936, 2016, -672, 0 } }, { -1344, 0, -3872, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1168, -1536, -3408, 0 }, { { 2080, -2016, -752, 0 }, { -2080, -2016, 752, 0 }, { 2080, 2016, -752, 0 }, { -2080, 2016, 752, 0 } }, { 1395, 0, 3860, 0 }, { 0, 0, 4096, 0 }, 2985, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3136, -1536, -1312, 0 }, { { -119, -2016, -1591, 0 }, { 109, -2016, 1584, 0 }, { -119, 2016, -1591, 0 }, { 109, 2016, 1584, 0 } }, { 4095, 0, -295, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3265, -1504, -1567, 0 }, { { 84, -2016, 1287, 0 }, { -98, -2016, -1299, 0 }, { 84, 2016, 1287, 0 }, { -98, 2016, -1299, 0 } }, { -4094, 0, 287, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_shelter_b1_pod_access_tunnel_8018487C[1] = {
    { NULL, NULL, { 5024, -1296, -5152, 0 }, { { -2064, 2320, 2208, 0 }, { 2064, 2320, -2208, 0 }, { -2064, -2320, 2208, 0 }, { 2064, -2320, -2208, 0 } }, { 2999, 0, 2803, 0 }, 3805, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_pod_access_tunnel_801848B8[3] = {
    { NULL, NULL, NULL, { 1856, -48, -6496, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, 16, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7264, -48, -1760, 0 }, { { -704, 0, -1024, 0 }, { 704, 0, -1024, 0 }, { -704, 0, 1024, 0 }, { 704, 0, 1024, 0 } }, { 0, 4094, 0, 0 }, { -4096, 0, 0, 0 }, 1241, WORLD_COLLISION_TRIGGER_ACTION_WARP, 47, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4721, -64, -672, 0 }, { { -880, 0, -480, 0 }, { 880, 0, -480, 0 }, { -880, 0, 480, 0 }, { 880, 0, 480, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 1001, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b1_pod_access_tunnel_8018499C[2] = {
    { 132, 410, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_141000_8013D77C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_pod_access_tunnel_801849B4[2] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_pod_access_tunnel_801849CC[2] = {
    { 11, 11, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101100_80147400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_pod_access_tunnel_801849E4[2] = {
    { 49, 49, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101100_80147400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_pod_access_tunnel_801849FC[2] = {
    { 23, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102300_80147AB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_pod_access_tunnel_80184A14[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 23, 23, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202300_8015FAB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_pod_access_tunnel_80184A38[2] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
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

AreaVariant D_shelter_b1_pod_access_tunnel_80184C00[23] = {
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

WorldCollisionFootstepSounds D_shelter_b1_pod_access_tunnel_80184CB8 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_b1_pod_access_tunnel_80184CC4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_pod_access_tunnel_80184CCC[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_pod_access_tunnel_80184CD4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_pod_access_tunnel_80184CB8 },
};

WorldCollisionSurfaceProperties* D_shelter_b1_pod_access_tunnel_80184CDC[8] = {
    D_shelter_b1_pod_access_tunnel_80184CC4,
    D_shelter_b1_pod_access_tunnel_80184CCC,
    D_shelter_b1_pod_access_tunnel_80184CD4,
    D_shelter_b1_pod_access_tunnel_80184CC4,
    D_shelter_b1_pod_access_tunnel_80184CC4,
    D_shelter_b1_pod_access_tunnel_80184CC4,
    D_shelter_b1_pod_access_tunnel_80184CC4,
    D_shelter_b1_pod_access_tunnel_80184CC4,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 D_shelter_b1_pod_access_tunnel_80184D0C[4] = {
    0,
    63,
    253,
    225,
};

RoomLatchedEvent gRoomEventLatched;

static __inline__ s32 _shelterB1PodAccessTunnelStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);

#include "../../shared/room_event_staged_task.inc.c"

static __inline__ s32 _shelterB1PodAccessTunnelStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b1_pod_access_tunnel_80184D0C_value = 0;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, 1);
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
    if (in->areaId == GAME_AREA_SHELTER_R47) {
        if (gameFlagGetNibble(GAME_FLAG_118) == 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_RunCapCmd1(8);
            }
            return 2;
        }
        if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= 6) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(D_shelter_b1_pod_access_tunnel_80181108, 1, 0, 0);
            }
            return 2;
        }
        if (gameFlagGetNibble(GAME_FLAG_B1_POD_TUNNEL_R47_DOOR_UNLOCKED) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(in->flagId, 2);
                Gp_RunCapCmd1(3);
            }
            return 0;
        }
        if (in->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_0D1) == 0 && gameFlagGetNibble(GAME_FLAG_083) == 0) {
            Gp_FillAllyHp();
            gameFlagSetNibble(GAME_FLAG_0D1, 1);
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 7);
        }
    }
    if (in->areaId == GAME_AREA_SHELTER_B1_STERILIZATION_ROOM) {
        if (gameFlagGetNibble(GAME_FLAG_118) == 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_RunCapCmd1(8);
            }
            return 2;
        }
        if (gameFlagGetNibble(GAME_FLAG_0D1) == 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_RunCapCmd1(4);
            }
            return 2;
        }
        event.capCmd   = 6;
        event.stageSnd = 0x54110001;
        event.flagId   = GAME_FLAG_B1_POD_TUNNEL_TO_STERILIZE_SCENE;
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
            Gp_RunCapCmd1(gameFlagGetNibble(GAME_FLAG_0FC) != 0 ? 5 : 1);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            goto L_advance;
        case 1:
            var_v0 = Gp_CapBusy();
            goto L_idle;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                if (Gp_GetCapEventKey() == 1) {
                    gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD, 2);
                }
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD, 0);
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_POD_TUNNEL_RIDE_TO_B2, 0, 0);
            goto L_advance;
        case 3:
            var_v0 = SndVoice_HasActiveId(SOUND_SHELTER_B1_POD_TUNNEL_RIDE_TO_B2);
        L_idle:
            if (var_v0 != 0) {
                return;
            }
        L_advance:
            task->state++;
            return;
        case 4:
            SndEvt_EnqueueType7(SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 3;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 1;
            room                                                       = gameFlagGetNibble(GAME_FLAG_118);
            if (room == 2) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = room;
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
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            goto L_advance;
        case 1:
            var_v0 = Gp_CapBusy();
            goto L_idle;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_POD_TUNNEL_GANTRY_TRANSIT, 0, 0);
            goto L_advance;
        case 3:
            var_v0 = SndVoice_HasActiveId(SOUND_SHELTER_B1_POD_TUNNEL_GANTRY_TRANSIT);
        L_idle:
            if (var_v0 != 0) {
                return;
            }
        L_advance:
            task->state++;
            return;
        case 4:
            gameFlagSetNibble(GAME_FLAG_B2_POD_TUNNEL_R48_DOOR_UNLOCKED, 1);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD_SERVICE_GANTRY, 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent        = 0x1C;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 1;
            gDisplayState.spriteVariant                                = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_shelter_b1_pod_access_tunnel_8017DD68(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DD70(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_B1_POD_ACCESS_TUNNEL_FIRST_USE) != 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_pod_access_tunnel_80181108, 0, 0, 0);
        } else {
            gameFlagSetNibble(GAME_FLAG_B1_POD_ACCESS_TUNNEL_FIRST_USE, 1);
            Gp_RunCapCmd1(0xA);
        }
    }
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DDD8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DDE0(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 6) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
    }
    return 0;
}

static void func_shelter_b1_pod_access_tunnel_8017DE10(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_pod_access_tunnel_801810D8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_118) == 1) {
        Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_801811C8, 0, 0, 0);
        gameFlagSetNibble(GAME_FLAG_118, 2);
        func_800E3FAC(0xA2, 0x37);
    } else if (gameFlagGetNibble(GAME_FLAG_POD_ACCESS_TUNNEL_SCENE_SEEN) == 0) {
        func_800E8634(D_shelter_b1_pod_access_tunnel_80182FFC, 0, D_shelter_b1_pod_access_tunnel_8018380C);
        func_800E3FAC(0xA2, 0x1E);
        gameFlagSetNibble(GAME_FLAG_POD_ACCESS_TUNNEL_SCENE_SEEN, 1);
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

/// Two-state task: state 0, unless blocked by `Gp_StateC08.mode` or `gDisplayState.pendingMode`,
/// sends the slot-3 task a `AnimationPlayRequest` built from `gPlayerStatus.weapon` (msg 0x3E8) and runs
/// `D_shelter_b1_pod_access_tunnel_80181120` through `func_800E8614`; state 1
/// sets `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent` to 0x1D and kills this task once the session is idle.
void func_shelter_b1_pod_access_tunnel_8017DF40(Task* task)
{
    AnimationPlayRequest rec;
    s32                  state;
    s32                  weaponId;
    s32                  id;

    state = task->state;
    switch (state) {
        case 0:
            if (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                weaponId                 = gPlayerStatus.weapon;
                id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                rec.source.index         = id;
                rec.animationId          = 1;
                rec.blend                = ANIMATION_BLEND_RESET;
                rec.blendFrames          = 0;
                rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &rec, 0);
                func_800E8614(D_shelter_b1_pod_access_tunnel_80181120, 0);
                task->state = task->state + 1;
            }
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0x1D;
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
    _ShelterB1PodAccessTunnelImageScrollWork* work;
    SPRT*                                     p;
    s32                                       y;

    work = task->work;
    if (gGameSession->viewReady != 0 || gGameSession->eventState == 0) {
        taskKill(task);
        return;
    }
    y = 0;
    if (work->timer++ >= SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_DELAY_FRAMES) {
        work->offset += work->speed;
        y             = work->offset >> 16;
        if (y > SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES) {
            y = SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES;
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
    p->h    = SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES - y;
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
    p->h    = SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES - y;
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
    ViewCamera* view;
    VECTOR      vec;

    if (task->killCountdown < task->spawnArg1.value && gGameSession->location.loc.view == 0xB) {
        view   = Gp_GetStageView(&gGameSession->location.loc);
        vec.vx = 0;
        vec.vy = 0x10;
        vec.vz = 0;
        ApplyTransposeMatrixLV(&view->transform, &vec, &vec);
        view->transform.t[0] += vec.vx;
        view->transform.t[1] += vec.vy;
        view->transform.t[2] += vec.vz;
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
    _ShelterB1PodAccessTunnelImageScrollWork* work;

    if (gGameSession->location.loc.view != 0xB) {
        taskKill(task);
        return;
    }
    work = memCalloc(sizeof(_ShelterB1PodAccessTunnelImageScrollWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = work;
    work->speed  = (SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES << 16) / task->spawnArg1.value;
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
    Task_SpawnFromTable(D_actor_141000_801348D8, 0, 0, 0);
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
        gRoomEffectFlashId      = EFFECT_SHELTER_B1_POD_ACCESS_TUNNEL_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_SHELTER_B1_POD_ACCESS_TUNNEL_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_SHELTER_B1_POD_ACCESS_TUNNEL_SPARK_BURST;
        arg0->state             = 1;
    }
    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839E4;
            glowDrawCapsule(&p[0], 0x180, 0x111);
            glowDrawCapsule(&p[2], 0x180, 0x111);
        } break;
        case 3:
        case 7: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839A4;
            glowDrawCapsule(&p[0], 0x180, 0x111);
            glowDrawCapsule(&p[2], 0x180, 0x111);
            glowDrawCapsule(&p[4], 0x180, 0x111);
            glowDrawCapsule(&p[6], 0x180, 0x111);
        } break;
        case 4: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839A4;
            glowDrawCapsule(&p[0], 0x180, 0x111);
            glowDrawCapsule(&p[2], 0x180, 0x111);
        } break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b1_pod_access_tunnel_8017F138(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b1_pod_access_tunnel_8017FB9C(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b1_pod_access_tunnel_80180484(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
