#include "rooms/shelter_b6_training_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b6_training_room_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

// Preserve the nonzero halfword after the three effect records.
// Its role is unresolved; it may be retained exporter padding.
typedef struct {
    RoomRingShape entries[3];
    u16           retained;
} ShelterB6TrainingRoomRingStorage;
STATIC_ASSERT_SIZEOF(ShelterB6TrainingRoomRingStorage, 20);
extern ShelterB6TrainingRoomRingStorage D_shelter_b6_training_room_80184404;

/// Scratchpad block `func_shelter_b6_training_room_80181FDC` takes from
/// the scratch stack: the two world points the textured strip joins, the first
/// point's projected depth, the GTE flag of the latest projection, the strip's
/// perspective-scaled half-width rotated into screen space, and both points'
/// screen positions.
typedef struct {
    SVECTOR from;
    SVECTOR to;
    s32     otz;
    s32     flag;
    s32     dx;
    s32     dy;
    DVECTOR sxy0;
    DVECTOR sxy1;
} _ShelterB6TrainingRoomRibbonScratch;

/// Scratchpad block `func_shelter_b6_training_room_80181368` takes from
/// the scratch stack: the six world-space points of the band's raised rim and of
/// its ground rim, then the projected depth, GTE flag and packed screen
/// positions of the quad being emitted (`sxy0` for `top[i]`, `sxy1`..`sxy3`
/// for `top[i + 1]`, `base[i]` and `base[i + 1]`).
typedef struct {
    SVECTOR top[6];
    SVECTOR base[6];
    s32     otz;
    s32     flag;
    u32     sxy0;
    u32     sxy1;
    u32     sxy2;
    u32     sxy3;
} _ShelterB6TrainingRoomBandScratch;

extern SVECTOR D_shelter_b6_training_room_80184334[];
extern u16     D_shelter_b6_training_room_801843FC[];

static void func_shelter_b6_training_room_80180530(GfxCoord* from, GfxCoord* to, s16 size, u16 color);
static void func_shelter_b6_training_room_80181368(EffectWork* mem, GfxCoord* coord, s32 band);
static void func_shelter_b6_training_room_80181BAC(GfxCoord* coord, s16 arg1, s16 arg2, s16 arg3);
static void func_shelter_b6_training_room_80181FDC(GfxCoord* arg0, GfxCoord* arg1, s32 arg2, s16 arg3);

GpMsgEntry D_shelter_b6_training_room_80182AF4[6] = {
    { 5102, func_shelter_b6_training_room_8017D640 },
    { 5105, func_shelter_b6_training_room_8017D638 },
    { 5103, func_shelter_b6_training_room_8017D75C },
    { 5104, func_shelter_b6_training_room_8017D684 },
    { 5108, func_shelter_b6_training_room_8017D764 },
    { 0x7FFFFFFF, NULL },
};

s32 D_shelter_b6_training_room_80182B24 = 0x11805;

AnimationPackedPose D_shelter_b6_training_room_80182B28[6] = {
#include "assets/shelter_b6_training_room_animation_05844_bank1.inc"
};

AnimationPackedRotation D_shelter_b6_training_room_80182B70[46] = {
#include "assets/shelter_b6_training_room_animation_05844_bank4.inc"
};

AnimationRecord D_shelter_b6_training_room_80182C28[109] = {
#include "assets/shelter_b6_training_room_animation_05844_records.inc"
};

u16 D_shelter_b6_training_room_80182DDC[20] = {
#include "assets/shelter_b6_training_room_animation_05844_indices.inc"
};

AnimationSet D_shelter_b6_training_room_80182E04 = {
    D_shelter_b6_training_room_80182C28,
    D_shelter_b6_training_room_80182DDC,
    { NULL, D_shelter_b6_training_room_80182B28, NULL, NULL, D_shelter_b6_training_room_80182B70, NULL, NULL, NULL },
};

AnimationPackedPose D_shelter_b6_training_room_80182E2C[13] = {
#include "assets/shelter_b6_training_room_animation_05F74_bank1.inc"
};

AnimationPackedRotation D_shelter_b6_training_room_80182EC8[171] = {
#include "assets/shelter_b6_training_room_animation_05F74_bank4.inc"
};

AnimationRecord D_shelter_b6_training_room_80183174[230] = {
#include "assets/shelter_b6_training_room_animation_05F74_records.inc"
};

u16 D_shelter_b6_training_room_8018350C[20] = {
#include "assets/shelter_b6_training_room_animation_05F74_indices.inc"
};

AnimationSet D_shelter_b6_training_room_80183534 = {
    D_shelter_b6_training_room_80183174,
    D_shelter_b6_training_room_8018350C,
    { NULL, D_shelter_b6_training_room_80182E2C, NULL, NULL, D_shelter_b6_training_room_80182EC8, NULL, NULL, NULL },
};

AnimationPackedPose D_shelter_b6_training_room_8018355C[3] = {
#include "assets/shelter_b6_training_room_animation_061B8_bank1.inc"
};

AnimationPackedRotation D_shelter_b6_training_room_80183580[28] = {
#include "assets/shelter_b6_training_room_animation_061B8_bank4.inc"
};

AnimationRecord D_shelter_b6_training_room_801835F0[88] = {
#include "assets/shelter_b6_training_room_animation_061B8_records.inc"
};

u16 D_shelter_b6_training_room_80183750[20] = {
#include "assets/shelter_b6_training_room_animation_061B8_indices.inc"
};

AnimationSet D_shelter_b6_training_room_80183778 = {
    D_shelter_b6_training_room_801835F0,
    D_shelter_b6_training_room_80183750,
    { NULL, D_shelter_b6_training_room_8018355C, NULL, NULL, D_shelter_b6_training_room_80183580, NULL, NULL, NULL },
};

AnimationPackedPose D_shelter_b6_training_room_801837A0[2] = {
#include "assets/shelter_b6_training_room_animation_063C0_bank1.inc"
};

AnimationPackedRotation D_shelter_b6_training_room_801837B8[29] = {
#include "assets/shelter_b6_training_room_animation_063C0_bank4.inc"
};

AnimationRecord D_shelter_b6_training_room_8018382C[75] = {
#include "assets/shelter_b6_training_room_animation_063C0_records.inc"
};

u16 D_shelter_b6_training_room_80183958[20] = {
#include "assets/shelter_b6_training_room_animation_063C0_indices.inc"
};

AnimationSet D_shelter_b6_training_room_80183980 = {
    D_shelter_b6_training_room_8018382C,
    D_shelter_b6_training_room_80183958,
    { NULL, D_shelter_b6_training_room_801837A0, NULL, NULL, D_shelter_b6_training_room_801837B8, NULL, NULL, NULL },
};

TaskDesc D_shelter_b6_training_room_801839A8 = { 0, 96, func_shelter_b6_training_room_8017D9C8, { .model = NULL } };

AnimationSet* D_shelter_b6_training_room_801839B4[5] = {
    NULL,
    &D_shelter_b6_training_room_80182E04,
    &D_shelter_b6_training_room_80183534,
    &D_shelter_b6_training_room_80183778,
    &D_shelter_b6_training_room_80183980,
};

GpCopyArg D_shelter_b6_training_room_801839C8 = { { .sets = D_shelter_b6_training_room_801839B4 }, 5 };

AnimationPlayRequest D_shelter_b6_training_room_801839D0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_801839E4 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_801839F8 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183A0C = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183A20 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_shelter_b6_training_room_80183A34 = { { 2300, 0, 8900, 0 }, { 0, 910, 0, 0 } };

ActorTransform D_shelter_b6_training_room_80183A4C = { { 2300, 0, 9000, 0 }, { 0, 1707, 0, 0 } };

AnimationPlayRequest D_shelter_b6_training_room_80183A64 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183A78 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183A8C = { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183AA0 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183AB4 = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183AC8 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_shelter_b6_training_room_80183ADC = { { 2850, 0, 8050, 0 }, { 0, -228, 0, 0 } };

AnimationPlayRequest D_shelter_b6_training_room_80183AF4[5] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
};

ActorTransform D_shelter_b6_training_room_80183B58 = { { 3250, 0, 9500, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_shelter_b6_training_room_80183B70 = { { 3250, 0, 9500, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_shelter_b6_training_room_80183B88 = { { 5000, 0, 0x2710, 0 }, { 0, 1024, 0, 0 } };

ActorCommand D_shelter_b6_training_room_80183BA0 = { { .loc = { 5, 24 } }, 1 };

ActorCommand D_shelter_b6_training_room_80183BA4 = { { .loc = { 5, 24 } }, 2 };

GpSpawnAnimArg D_shelter_b6_training_room_80183BA8 = { 3, 4 };

ActorCommand D_shelter_b6_training_room_80183BB0 = { { .loc = { 5, 24 } }, 1 };

EvsCommand D_shelter_b6_training_room_80183BB4[58] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_training_room_8017DAF8 }, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_shelter_b6_training_room_801839C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_801839E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_training_room_8017DB70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = D_shelter_b6_training_room_80184124 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b6_training_room_80183A34 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_shelter_b6_training_room_80183B58 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_shelter_b6_training_room_80183ADC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 4 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_training_room_8017D940 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_801839F8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_shelter_b6_training_room_80183A78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_training_room_80183BA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_shelter_b6_training_room_80183B70 }, { .storage = &D_shelter_b6_training_room_80183BA8 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55190001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_shelter_b6_training_room_80183B88 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b6_training_room_80183A4C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_80183A0C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_shelter_b6_training_room_80183A8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55190004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_shelter_b6_training_room_80183AA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_shelter_b6_training_room_80183A8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_80183A20 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55190002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_training_room_8017D974 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_shelter_b6_training_room_80183AB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_shelter_b6_training_room_80183AC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_training_room_8017D974 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_shelter_b6_training_room_80184124[14] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_training_room_8017DAF8 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_shelter_b6_training_room_801839C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_801839E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_training_room_8017DB70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b6_training_room_80183A4C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_training_room_8017D974 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_shelter_b6_training_room_80184274[7] = {
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55190005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_training_room_8017DAC8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_training_room_8017DB28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_shelter_b6_training_room_8018431C[2] = {
    { 0, 192, func_shelter_b6_training_room_8017DD98, { .model = NULL } },
    { 0, 192, func_shelter_b6_training_room_8017DBBC, { .model = NULL } },
};

SVECTOR D_shelter_b6_training_room_80184334[25] = {
    { 260, -1440, 0x283C, 0 },
    { 565, -1405, 0x283C, 0 },
    { 910, -1470, 0x283C, 0 },
    { 910, -1240, 0x283C, 0 },
    { 1180, -1470, 0x283C, 0 },
    { 1180, -1240, 0x283C, 0 },
    { 1765, -1440, 0x283C, 0 },
    { 2065, -1410, 0x283C, 0 },
    { 4950, -2250, 9750, 0 },
    { 4950, -2250, 9250, 0 },
    { 50, -2250, 1750, 0 },
    { 50, -2250, 1250, 0 },
    { 350, -4400, 9750, 0 },
    { 350, -4400, 7750, 0 },
    { 350, -4400, 6250, 0 },
    { 350, -4400, 4250, 0 },
    { 350, -4400, 2750, 0 },
    { 350, -4400, 750, 0 },
    { 4650, -4400, 9750, 0 },
    { 4650, -4400, 7750, 0 },
    { 4650, -4400, 6250, 0 },
    { 4650, -4400, 4250, 0 },
    { 4650, -4400, 2750, 0 },
    { 4650, -4400, 750, 0 },
    { 2500, -1000, 7750, 0 },
};

u16 D_shelter_b6_training_room_801843FC[4] = {
    258,
    532,
    1064,
    1596,
};

ShelterB6TrainingRoomRingStorage D_shelter_b6_training_room_80184404 = { { { 256, 2048, 512 }, { 512, 1536, 768 }, { 768, 1024, 1024 } }, 0xF23F };

void func_shelter_b6_training_room_8017DDE8(Task* task)
{
    s32 i;
    s32 j;

    if (task->state == 0) {
        D_shelter_b6_training_room_80185C98 = 0;
        for (j = 0; j < 3; j++) {
            for (i = 0; i < 6; i++) {
                D_shelter_b6_training_room_80185C60[task->spawnArg1.value][i] = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            }
        }
        task->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            break;
        case 3:
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[12], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[13], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[14], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[15], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[16], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[18], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[19], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[20], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[21], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[22], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 4:
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[12], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[13], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[14], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[18], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[19], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[20], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 5:
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 6:
            glowDrawDisc(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 7:
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[17], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[23], 0x280, 0x444);
            break;
        case 8:
            glowDrawDisc(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"

void func_shelter_b6_training_room_8017EE70(Task* arg0)
{
    u8          rgb[3];
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s16         step;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x100;
            mem->period = 0xE0;
            mem->step   = 0x100;
            arg0->state = 1;
        }
        rgb[0]     = mem->scale;
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        Gp_DrawRing(coord, (s16)(step * 2), rgb);
        RoomFx_DrawBurstGlow(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            Gp_DrawArc(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x80;
            return;
        }
        mem->scale -= 0x10;
        if (mem->scale < 0x10) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

#include "../../shared/room_visual_effects.inc.c"
#include "../../shared/room_visual_effects_glow_quad.inc.c"

void func_shelter_b6_training_room_8017F8B8(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        work->age++;
        switch (task->state) {
            case 0:
                task->state                         = 1;
                work->scale                         = 0;
                work->angle                         = 0x100;
                D_shelter_b6_training_room_80185C90 = NULL;
                work->step                          = 0x80 / task->spawnArg1.value;
            case 1:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    rgb[0] = work->scale >> 1;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((task->spawnArg1.value << 5) + 0x300), 0x100, rgb);
                    break;
                }
                work->scale += work->step;
                work->angle += work->step << 3;
                task->spawnArg1.value--;
                rgb[0] = work->scale >> 1;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((task->spawnArg1.value << 5) + 0x300), 0x100, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale                         = 0xFF;
                    task->state                         = 2;
                    work->period                        = 0x600;
                    work->step                          = 0;
                    D_shelter_b6_training_room_80185C90 = coord;
                }
                break;
            case 2:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    rgb[0] = work->scale >> 1;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, 0x300, 0x100, rgb);
                    break;
                }
                if (work->scale >= 9) {
                    rgb[0] = work->scale >> 1;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, 0x300, 0x100, rgb);
                    work->scale -= 8;
                    break;
                }
                task->state = 3;
                break;
            case 3:
                break;
            case 4:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    } else {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Draws a glowing capsule from the anchor coordinate
/// `D_shelter_b6_training_room_80185C90` to `coord`, doing nothing while no
/// anchor is set. Both world positions are projected, and nothing is drawn
/// unless both land on-screen. The capsule is drawn in two passes whose end
/// radii are `size * pass * 64 / otz`, each followed by the matching ground
/// capsule from `func_shelter_b6_training_room_80180530`. The fill colour comes
/// from the 4-bit-per-channel palette entry `color`, scaled by 16 and
/// brightened on alternate fields; the outer vertices are black.
void func_shelter_b6_training_room_8017FC40(GfxCoord* coord, s16 size, u16 color)
{
    void**           scratch;
    u8*              head;
    RoomBeamScratch* block;
    POLY_G4*         prim;
    s32              pass;
    u8               r;
    u8               g;
    u8               b;
    s32              blend;
    s32              scaled;
    s32              limit;
    s32              angStart;
    s32              ang;
    s32              next;
    s32              mid;
    s32              tr;
    s32              tg;

    if (D_shelter_b6_training_room_80185C90 == NULL) {
        return;
    }
    scratch        = SCRATCH_STACK_CURSOR_SLOT;
    head           = *scratch;
    block          = (RoomBeamScratch*)(*scratch = head - 0x2C);
    block->base.vx = D_shelter_b6_training_room_80185C90->workm.t[0];
    block->base.vy = D_shelter_b6_training_room_80185C90->workm.t[1];
    block->base.vz = D_shelter_b6_training_room_80185C90->workm.t[2];
    block->tip.vx  = coord->workm.t[0];
    block->tip.vy  = coord->workm.t[1];
    block->tip.vz  = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->base);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(&block->tip);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        gte_stszotz(&block->otz1);
        if (block->flag >= 0) {
            color = D_shelter_b6_training_room_801843FC[color];
            tr    = ((color >> 8) & 0xF) << 4;
            tg    = ((color >> 4) & 0xF) << 4;
            blend = ((u8)gDisplayState.animFrame & 1) << 4;
            r     = tr + blend;
            g     = tg + blend;
            b     = ((color & 0xF) << 4) + blend;
            for (pass = 1; pass < 3; pass++) {
                scaled    = size * (pass << 6);
                block->r0 = scaled / block->otz0;
                block->r1 = scaled / block->otz1;
                ang       = (s16)ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
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
                        prim->x0 = block->sx1 + ((block->r1 * rsin(ang + 0x800)) >> 12);
                        prim->y0 = block->sy1 + ((block->r1 * rcos(ang + 0x800)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(ang + 0xA00)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(ang + 0xA00)) >> 12);
                        prim->x2 = block->sx1;
                        prim->y2 = block->sy1;
                        prim->x3 = block->sx1 + ((block->r1 * rsin(ang + 0xC00)) >> 12);
                        prim->y3 = block->sy1 + ((block->r1 * rcos(ang + 0xC00)) >> 12);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);

                        prim           = gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                        prim->x1 = block->sx0 + ((block->r0 * rsin(ang + 0x200)) >> 12);
                        prim->y1 = block->sy0 + ((block->r0 * rcos(ang + 0x200)) >> 12);
                        next     = ang + 0x400;
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx0 + ((block->r0 * rsin(next)) >> 12);
                        prim->y3 = block->sy0 + ((block->r0 * rcos(next)) >> 12);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                        prim           = gGpuPrimCursor;
                        mid            = angStart + (ang - angStart) * 2;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, r, g, b);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(mid)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(mid)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(mid)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(mid)) >> 12);
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx1;
                        prim->y3 = block->sy1;
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                        ang = next;
                    } while (ang < limit);
                }
                func_shelter_b6_training_room_80180530(D_shelter_b6_training_room_80185C90, coord, size, color);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x2C);
}

/// Draws a glowing capsule between the ground points under `from` and `to`.
/// Both coordinates are traced down to the floor, and nothing is drawn unless
/// both traces land and both points project on-screen. Each end gets a
/// half-disc of radius `size * 64 / otz`, built from two gouraud quarter
/// fans, and one quad per half joins the two discs. The fill colour comes from
/// the 4-bit-per-channel palette entry `color`, doubled and brightened on
/// alternate fields; the outer vertices are black, so the glow fades outward.
static void func_shelter_b6_training_room_80180530(GfxCoord* from, GfxCoord* to, s16 size, u16 color)
{
    GfxCoord         c0;
    GfxCoord         c1;
    void**           scratch;
    u8*              head;
    RoomBeamScratch* block;
    POLY_G4*         prim;
    u8               r;
    u8               g;
    u8               b;
    s32              blend;
    s32              scaled;
    s32              limit;
    s32              angStart;
    s32              ang;
    s32              next;
    s32              mid;
    DisplayState*    ds;

    if (Gp_TraceGroundCoord(from, &c0) != 1 || Gp_TraceGroundCoord(to, &c1) != 1) {
        return;
    }
    scratch        = SCRATCH_STACK_CURSOR_SLOT;
    head           = *scratch;
    block          = (RoomBeamScratch*)(*scratch = head - 0x2C);
    block->base.vx = c0.workm.t[0];
    block->base.vy = c0.workm.t[1];
    block->base.vz = c0.workm.t[2];
    block->tip.vx  = c1.workm.t[0];
    block->tip.vy  = c1.workm.t[1];
    block->tip.vz  = c1.workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->base);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(&block->tip);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        gte_stszotz(&block->otz1);
        if (block->flag >= 0) {
            color     = D_shelter_b6_training_room_801843FC[color];
            r         = ((color >> 8) & 0xF) << 1;
            g         = ((color >> 4) & 0xF) << 1;
            b         = (color & 0xF) << 1;
            ds        = &gDisplayState;
            blend     = ((u8)ds->animFrame & 1) << 1;
            r        += blend;
            g        += blend;
            b        += blend;
            scaled    = size << 6;
            block->r0 = scaled / block->otz0;
            block->r1 = scaled / block->otz1;
            ang       = (s16)ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
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
                    prim->x0 = block->sx1 + ((block->r1 * rsin(ang + 0x800)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(ang + 0x800)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(ang + 0xA00)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(ang + 0xA00)) >> 12);
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(ang + 0xC00)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(ang + 0xC00)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);

                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(ang + 0x200)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(ang + 0x200)) >> 12);
                    next     = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(next)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(next)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim           = gGpuPrimCursor;
                    mid            = angStart + (ang - angStart) * 2;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(mid)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(mid)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(mid)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(mid)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = next;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x2C);
}

void func_shelter_b6_training_room_80180DB4(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    GpMtxWords* rot;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        work->age++;
        switch (task->state) {
            case 0:
                rot                 = (GpMtxWords*)&coord->coord;
                rot->m00_m01        = 0x1000;
                rot->m02_m10        = 0;
                rot->m11_m12        = 0x1000;
                rot->m20_m21        = 0;
                rot->m22            = 0x1000;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale         = 0;
                work->angle         = 0x100;
                work->step          = 0xC0 / task->spawnArg1.value;
                if (work->step == 0) {
                    work->step = 1;
                }
                task->state = 1;
            case 1:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 1;
                    rgb[2] = work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((task->spawnArg1.value % 15) * (work->scale << 2)), 0x100, rgb);
                    return;
                }
                work->scale += work->step;
                if (work->scale > 0xC0) {
                    work->scale = 0xC0;
                }
                work->angle = (u16)work->scale * 8 + 0x100;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 1;
                rgb[2] = work->scale >> 2;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((task->spawnArg1.value % 15) * (work->scale << 2)), 0x100, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    Gp_SpawnEff(0x601AA, coord, 0, NULL);
                    Gp_SpawnEff(0x601AA, coord, 1, NULL);
                    Gp_SpawnEff(0x601AA, coord, 2, NULL);
                    work->period = 0x600;
                    work->step   = 0;
                }
                return;
            case 2:
                if (work->scale >= 5) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 1;
                    rgb[2] = work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                        work->scale -= 4;
                    }
                    Gp_DrawFadeQuad(rgb, 1);
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

void func_shelter_b6_training_room_801811AC(Task* task)
{
    EffectWork* mem;
    GfxCoord*   coord;

    mem   = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1.value);
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    mem->age++;
    switch (task->state) {
        case 0:
            mem->scale          = 0x80;
            task->state         = task->spawnArg1.value + 1;
            coord->coord.t[1]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            return;
        case 1:
            if (mem->scale < 5) {
                goto release;
            }
            if (mem->period < 0xC00) {
                mem->period += 0xC0;
            } else {
                mem->scale -= 4;
            }
            mem->angle += 0x20;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1.value);
            return;
        case 2:
            if (mem->scale < 4) {
                goto release;
            }
            mem->scale -= 3;
            mem->angle += 0x40;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1.value);
            return;
        case 3:
            if (mem->scale < 5) {
                goto release;
            }
            mem->scale -= 4;
            mem->angle += 0x180;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1.value);
            return;
        case 4:
        release:
            Gp_ReleaseState1CMem(mem, task);
        default:
            return;
    }
}

/// Draws band `band` of a six-sided textured ring around `coord`: six
/// `POLY_FT4` quads joining a ground rim of radius `angle + radius` to a rim
/// raised by `period + lift` and widened by `step + spread`. Both rims are
/// rotated by the coordinate's `workm`, translated by its `t[]` and projected
/// through `GsWSMATRIX`. Quad `i` takes its texture column from
/// `(D_shelter_b6_training_room_80185C60[band][i] + age) % 6`, so each quad
/// animates on its own phase, and `scale` sets its brightness.
static void func_shelter_b6_training_room_80181368(EffectWork* mem, GfxCoord* coord, s32 band)
{
    void**                             scratch;
    u8*                                head;
    _ShelterB6TrainingRoomBandScratch* block;
    SVECTOR*                           bp;
    POLY_FT4*                          prim;
    RoomRingShape*                     shape;
    s32                                i;
    s32                                next;
    s32                                ang;
    s32                                u;
    s16                                frame;
    s16                                rTop;
    s16                                rBase;
    u16                                height;
    u16                                period;

    shape    = &D_shelter_b6_training_room_80184404.entries[band];
    period   = mem->period;
    rBase    = mem->angle;
    height   = period + (u16)shape->yOff;
    rBase   += (u16)shape->rInner;
    rTop     = rBase + mem->step + (u16)shape->rExtra;
    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = (u8*)*scratch;
    *scratch = head - 0x78;
    block    = (_ShelterB6TrainingRoomBandScratch*)(head - 0x78);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 6; i++) {
        ang              = i * 0x2AA;
        block->top[i].vx = (rsin(ang) * rTop) >> 12;
        block->top[i].vy = -height;
        block->top[i].vz = (rcos(ang) * rTop) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->top[i]);
        gte_rtv0();
        gte_stsv(&block->top[i]);
        block->top[i].vx  = (u16)block->top[i].vx + (u16)coord->workm.t[0];
        block->top[i].vy  = (u16)block->top[i].vy + (u16)coord->workm.t[1];
        block->top[i].vz  = (u16)block->top[i].vz + (u16)coord->workm.t[2];
        block->base[i].vx = (rsin(ang) * rBase) >> 12;
        bp                = &block->top[i] + 6;
        bp->vy            = 0;
        bp->vz            = (rcos(ang) * rBase) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->base[i]);
        gte_rtv0();
        gte_stsv(&block->base[i]);
        block->base[i].vx = (u16)block->base[i].vx + (u16)coord->workm.t[0];
        bp->vy            = (u16)bp->vy + (u16)coord->workm.t[1];
        bp->vz            = (u16)bp->vz + (u16)coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 6; i++) {
        gte_ldv0(&block->top[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = i + 1;
        gte_ldv3(&block->top[next % 6], &block->base[i], &block->base[next % 6]);
        gte_rtpt();
        frame = ((s8)D_shelter_b6_training_room_80185C60[band][i] + mem->age) % 6;
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            prim->code = 0x2E;
            setRGB0(prim, mem->scale, mem->scale, mem->scale);
            prim->tpage = 0x2A;
            prim->clut  = 0x4282;
            u           = frame * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = block->sxy0;
            prim->y0 = block->sxy0 >> 16;
            prim->x1 = block->sxy1;
            prim->y1 = block->sxy1 >> 16;
            prim->x2 = block->sxy2;
            prim->y2 = block->sxy2 >> 16;
            prim->x3 = block->sxy3;
            prim->y3 = block->sxy3 >> 16;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x78);
}

void func_shelter_b6_training_room_80181930(Task* task)
{
    GfxCoord* coord;
    u8        rgb[3];
    u32       shade;

    coord = task->extra.tmd->coords + 1;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        D_shelter_b6_training_room_80185C94 = coord;
        shade                               = ((gDisplayState.animFrame & 1) << 4) + 0x40;
        rgb[0]                              = shade;
        rgb[1]                              = shade;
        rgb[2]                              = shade >> 1;
        Gp_DrawRing(coord, 0x200, rgb);
        Gp_DrawRing(coord, 0x400, rgb);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & 3) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(0x600E0, task->extra.tmd->coords + (((gRandomLcgState >> 16) & 0xF) + 3), 0x10080, NULL);
        }
    }
}

void func_shelter_b6_training_room_80181A3C(Task* task)
{
    EffectWork* mem;
    GfxCoord*   coord;

    mem   = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        mem->age++;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (task->state == 0) {
            GpMtxWords* rot;
            u32         first;

            rot                 = (GpMtxWords*)&coord->coord;
            coord->parent       = mem->parent;
            rot->m00_m01        = 0x1000;
            rot->m02_m10        = 0;
            rot->m11_m12        = 0x1000;
            rot->m20_m21        = 0;
            rot->m22            = 0x1000;
            coord->coord.t[0]   = mem->pos.vx;
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            first               = gRandomLcgState;
            coord->coord.t[1]   = mem->pos.vy;
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            coord->coord.t[2]   = mem->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            mem->scale          = ((first >> 16) & 0x1FF) + 0x100;
            mem->angle          = (gRandomLcgState >> 16) & 0xFFF;
            mem->period         = ((gRandomLcgState >> 16) & 0xF) + 6;
            task->state         = 1;
        }
        func_shelter_b6_training_room_80181BAC(coord, mem->age, mem->scale, mem->angle);
        if (mem->age & 1) {
            func_shelter_b6_training_room_80181FDC(coord, D_shelter_b6_training_room_80185C94, mem->age >> 1, mem->scale);
        }
        if (mem->age > mem->period) {
            Gp_ReleaseState1CMem(mem, task);
        }
    }
}

/// Draws one spinning sprite frame: a semi-transparent `POLY_FT4` centred on
/// the coordinate's world position, projected by a single `RTPS`. Its half-size
/// is `arg2 * 39 / otz`, and the four corners are that half-size swung to
/// `arg3` and to `arg3 + 0x400`. `arg1` picks one of six 40-pixel-wide frames
/// from the texture page. Nothing is drawn if the point fails the GTE flag test.
static void func_shelter_b6_training_room_80181BAC(GfxCoord* coord, s16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    GpFxQuadScratch* vecp;
    POLY_FT4*        prim;
    s16              u;
    u16              vz;

    scratch                                   = SCRATCH_STACK_CURSOR_SLOT;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)coord->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)coord->workm.t[1];
    vz                                        = (u16)coord->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vecp                                      = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2A;
        prim->clut  = 0x42C9;
        prim->v0    = 0x38;
        prim->v1    = 0x38;
        prim->v2    = 0x5F;
        prim->v3    = 0x5F;
        u           = arg1 % 6;
        prim->u0    = u * 40;
        prim->u1    = u * 40 + 0x27;
        prim->u2    = u * 40;
        prim->u3    = u * 40 + 0x27;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(arg3)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(arg3)) >> 12;
        prim->x0    = block->sx + (u16)block->dx;
        prim->x3    = block->sx - (u16)block->dx;
        prim->y0    = block->sy - (u16)block->dy;
        prim->y3    = block->sy + (u16)block->dy;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(arg3 + 0x400)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(arg3 + 0x400)) >> 12;
        prim->x1    = block->sx + (u16)block->dx;
        prim->x2    = block->sx - (u16)block->dx;
        prim->y1    = block->sy - (u16)block->dy;
        prim->y2    = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

/// Draws a textured `POLY_FT4` strip between the world positions of two
/// coordinates. Both ends are projected and the strip is dropped if either
/// fails the GTE flag test. Its half-width is `arg3 * 23 / otz`, laid
/// perpendicular to the screen-space line between the ends, and `arg2`
/// selects one of four 128x24 texture frames. The primitive is queued at the
/// first end's depth.
static void func_shelter_b6_training_room_80181FDC(GfxCoord* arg0, GfxCoord* arg1, s32 arg2, s16 arg3)
{
    void**                               scratch;
    u8*                                  head;
    _ShelterB6TrainingRoomRibbonScratch* block;
    _ShelterB6TrainingRoomRibbonScratch* vecp;
    POLY_FT4*                            prim;
    s16                                  ang;
    u16                                  vz;

    scratch                                                        = SCRATCH_STACK_CURSOR_SLOT;
    head                                                           = *scratch;
    ((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->from.vx = (u16)arg0->workm.t[0];
    block                                                          = (_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28);
    block->from.vy                                                 = (u16)arg0->workm.t[1];
    block->from.vz                                                 = (u16)arg0->workm.t[2];
    block->to.vx                                                   = (u16)arg1->workm.t[0];
    block->to.vy                                                   = (u16)arg1->workm.t[1];
    vz                                                             = (u16)arg1->workm.t[2];
    *scratch                                                       = block;
    block->to.vz                                                   = vz;
    vecp                                                           = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->from);
    gte_rtps();
    gte_stsxy(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->sxy0);
    gte_stflg(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->otz);
        gte_ldv0(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->to);
        gte_rtps();
        gte_stsxy(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->sxy1);
        gte_stflg(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->flag);
        if (block->flag >= 0) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x42C8;
            prim->u0    = (arg2 & 1) << 7;
            prim->v0    = ((u32)(arg2 & 3) >> 1) * 24 - 0x30;
            prim->u1    = ((arg2 & 1) << 7) + 0x7F;
            prim->v1    = ((u32)(arg2 & 3) >> 1) * 24 - 0x30;
            prim->u2    = (arg2 & 1) << 7;
            prim->v2    = ((u32)(arg2 & 3) >> 1) * 24 - 0x19;
            prim->u3    = ((arg2 & 1) << 7) + 0x7F;
            prim->v3    = ((u32)(arg2 & 3) >> 1) * 24 - 0x19;
            ang         = ratan2(block->sxy1.vy - block->sxy0.vy, block->sxy1.vx - block->sxy0.vx);
            block->dx   = (((arg3 * 23) / block->otz) * rsin(ang)) >> 12;
            block->dy   = (((arg3 * 23) / block->otz) * rcos(ang)) >> 12;
            prim->x0    = (u16)block->sxy0.vx + (u16)block->dx;
            prim->x3    = (u16)block->sxy1.vx - (u16)block->dx;
            prim->y0    = (u16)block->sxy0.vy - (u16)block->dy;
            prim->y3    = (u16)block->sxy1.vy + (u16)block->dy;
            block->dx   = (((arg3 * 23) / block->otz) * rsin(ang + 0x400)) >> 12;
            block->dy   = (((arg3 * 23) / block->otz) * rcos(ang + 0x400)) >> 12;
            prim->x1    = (u16)block->sxy1.vx + (u16)block->dx;
            prim->x2    = (u16)block->sxy0.vx - (u16)block->dx;
            prim->y1    = (u16)block->sxy1.vy - (u16)block->dy;
            prim->y2    = (u16)block->sxy0.vy + (u16)block->dy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}

void func_shelter_b6_training_room_8018245C(Task* task)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         effectControl;
    u8          rgb[3];

    mem           = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    mem->age++;
    if (task->state == 0) {
        mem->scale = 0xC0;
        mem->angle = 0x200;
        D_shelter_b6_training_room_80185C98++;
        task->state           = 1;
        task->spawnArg1.value = D_shelter_b6_training_room_80185C98;
    }
    if (task->spawnArg1.value != D_shelter_b6_training_room_80185C98) {
        goto release;
    }
    rgb[0]      = mem->scale;
    rgb[1]      = mem->scale;
    rgb[2]      = mem->scale >> 1;
    mem->angle += 0x18;
    Gp_DrawArc(coord, (s16)(mem->angle * 2), 0, rgb);
    Gp_DrawRing(coord, (s16)((u16)mem->angle * 4), rgb);
    if (mem->age < 9) {
        return;
    }
    mem->scale -= 0x18;
    if (mem->scale < 0x18) {
    release:
        Gp_ReleaseState1CMem(mem, task);
    }
}

void func_shelter_b6_training_room_801825C0(Task* task)
{
    EffectWork* mem;
    GfxCoord*   coord;

    mem   = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        mem->age++;
        if (task->state == 0) {
            mem->move.vx    = 0;
            mem->move.vy    = 8;
            mem->move.vz    = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = ((gRandomLcgState >> 16) & 0xFFF) | 0x1000;
            task->state     = 1;
        }
        coord->coord.t[1]  += mem->move.vy;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (!(mem->age & 1)) {
            mem->index++;
        }
        if (mem->index < 8) {
            if (mem->age & 1) {
                Gp_DrawFxQuad(coord, mem->index, 0x400, mem->scale);
            }
        } else {
            Gp_ReleaseState1CMem(mem, task);
        }
    }
}

void func_shelter_b6_training_room_801826E0(Task* task)
{
    EffectWork* mem;
    GfxCoord*   coord;

    mem   = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        mem->age++;
        if (task->state == 0) {
            mem->move.vy = 0x20;
            mem->scale   = 0x80;
            mem->move.vx = 0;
            mem->move.vz = 0;
            task->state  = 1;
        }
        coord->coord.t[1]  += mem->move.vy;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (mem->age < 60) {
            if (mem->age & 1) {
                mem->index = (mem->index + 1) & 3;
                func_800EB6E8(coord, mem->index, 0x300, 0x80);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 3) == 0) {
                    Gp_SpawnEff(0x601AD, coord, 0, NULL);
                }
            }
        } else {
            Gp_ReleaseState1CMem(mem, task);
        }
    }
}

void func_shelter_b6_training_room_80182804(Task* task)
{
    EffectWork* mem;

    mem = task->spawnArg2.pointer;
    if (mem->age >= 0x15) {
        Gp_ReleaseState1CMem(mem, task);
        return;
    }
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        mem->age++;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->scale     += ((gRandomLcgState >> 16) & 0x1FF) + 0x200;
        mem->move.vx    = D_shelter_b6_training_room_80184334[24].vx + ((rcos(mem->scale) * 1000) >> 12);
        mem->move.vy    = D_shelter_b6_training_room_80184334[24].vy - mem->age * 200;
        mem->move.vz    = D_shelter_b6_training_room_80184334[24].vz + ((rsin(mem->scale) * 1000) >> 12);
        Gp_SpawnEff(0x601AE, NULL, 0, &mem->move);
    }
}

void func_shelter_b6_training_room_8018294C(Task* task)
{
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & 7) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(0x601AB, task->extra.tmd->coords + ((u16)((gRandomLcgState >> 16) % 18) + 1), 0, NULL);
        }
    }
}

void func_shelter_b6_training_room_80182A14(s32 arg0, s32 arg1)
{
    GameLocationKey* sess = &gGameSession->location.loc;
    GpSprtRec*       rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    SpriteBatch*     batches;
    s32              run = arg0 & 0xFF;
    s32              flag;

    if (run == 0) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            batches           = rec[2].field_4;
            batches[2].hidden = 1;
            batches           = rec[6].field_4;
            batches[1].hidden = 1;
            return;
        }
        if (flag == 1) {
            batches           = rec[2].field_4;
            batches[2].hidden = 0;
            batches           = rec[6].field_4;
            batches[1].hidden = 0;
            return;
        }
    } else if (run == 1) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            batches           = rec[2].field_4;
            batches[1].hidden = run;
            batches           = rec[6].field_4;
            batches[2].hidden = run;
            return;
        }
        if (flag == run) {
            batches           = rec[2].field_4;
            batches[1].hidden = 0;
            batches           = rec[6].field_4;
            batches[2].hidden = 0;
        }
    }
}
