#include "rooms/dryfield_night_driveway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_dryfield_night_driveway_80182120[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u8 D_dryfield_night_driveway_80182120_value __asm__("D_dryfield_night_driveway_80182120");

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[4];
        AnimationPlayRequest arguments[2];
    } data;
    s32 words[14];
} DryfieldNightDrivewayAnimStorageF8C4;
STATIC_ASSERT_SIZEOF(DryfieldNightDrivewayAnimStorageF8C4, 56);

extern DryfieldNightDrivewayAnimStorageF8C4 D_dryfield_night_driveway_8017F8C4;

/// Descriptor of the room's event task, which the event gate spawns.
extern TaskDesc D_dryfield_night_driveway_8017E678;

/// Descriptor table of two tasks (`func_dryfield_night_driveway_8017DAF4`,
/// `func_dryfield_night_driveway_8017DB8C`), ended by a 0xFFFF entry; the
/// event gate spawns entry 1.
extern TaskDesc D_dryfield_night_driveway_8017F34C[];

/// Blocks the room's tasks hand to `func_800E8614` / `func_800E8634`.
extern GpEvsCmd D_dryfield_night_driveway_8017F3D4[];
extern GpEvsCmd D_dryfield_night_driveway_8017F54C[];
extern GpEvsCmd D_dryfield_night_driveway_8017F6CC[];
extern GpEvsCmd D_dryfield_night_driveway_8017F998[];
extern GpEvsCmd D_dryfield_night_driveway_8017FB00[];

/// Message table the room task installs at `Task::msgTable`.
extern GpMsgEntry D_dryfield_night_driveway_8017F7A4[];

/// Three pairs of beam end points, back to back: the first pair at `B0[0]`,
/// the second at `B0[2]` and the third at `D0`. `D0` is its own symbol because
/// the code reaches the third pair by name while it indexes the array for the
/// second.

/// Spawn argument of the helper task 0x31 the event task starts.
extern RoomFadeStorage D_dryfield_night_driveway_80182110;

/// The message and the event the event gate latched for the event task, and
/// the flag it sets when it latches one.
extern RoomEventMsg     D_dryfield_night_driveway_80182118;
extern RoomLatchedEvent D_dryfield_night_driveway_80182124;

static void func_dryfield_night_driveway_8017DCFC(Task* arg0);
static void func_dryfield_night_driveway_8017DD7C(Task* task);

extern GpGridParams               D_dryfield_night_driveway_80180C0C[1];
extern GpObj3A                    D_dryfield_night_driveway_80181FFC[2];
extern GpObj4C                    D_dryfield_night_driveway_801818E8[6];
extern GpObj4C                    D_dryfield_night_driveway_80181DC8[4];
extern WorldCoordRoomAmbientEntry D_dryfield_night_driveway_80182074[11];
extern GpRoomCoordSet             D_dryfield_night_driveway_80181DB0[1];

extern AnimationPlayRequest D_dryfield_night_driveway_8017F380;
extern AnimationPlayRequest D_dryfield_night_driveway_8017F3A8;
extern AnimationSet         D_dryfield_night_driveway_8017EE30;
extern AnimationSet         D_dryfield_night_driveway_8017F044;
extern AnimationSet         D_dryfield_night_driveway_8017F324;
extern GpCopyArg            D_dryfield_night_driveway_8017F378;
s32                         func_dryfield_night_driveway_8017D7A0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                         func_dryfield_night_driveway_8017DC94(Task*, s32, s32, GpMessageArg);
s32                         func_dryfield_night_driveway_8017DCE4(Task*, s32, GpMessageArg, GpMessageArg);
s32                         func_dryfield_night_driveway_8017DCEC(Task*, s32, GpMessageArg, GpMessageArg);
s32                         func_dryfield_night_driveway_8017DCF4(Task*, s32, GpMessageArg, GpMessageArg);
void                        func_dryfield_night_driveway_8017DC6C(s32);
void                        func_dryfield_night_driveway_8017DC78(s16);
void                        func_dryfield_night_driveway_8017DC88(u8);

void func_dryfield_night_driveway_8017D608(Task*);
void func_dryfield_night_driveway_8017DAF4(Task*);
void func_dryfield_night_driveway_8017DB8C(Task*);

TaskDesc D_dryfield_night_driveway_8017E678 = { 0, 32, func_dryfield_night_driveway_8017D608, { .model = NULL } };

AnimationPackedPose D_dryfield_night_driveway_8017E684[10] = {
#include "assets/dryfield_night_driveway_animation_0150C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_driveway_8017E6FC[95] = {
#include "assets/dryfield_night_driveway_animation_0150C_bank4.inc"
};

AnimationRecord D_dryfield_night_driveway_8017E878[139] = {
#include "assets/dryfield_night_driveway_animation_0150C_records.inc"
};

u16 D_dryfield_night_driveway_8017EAA4[20] = {
#include "assets/dryfield_night_driveway_animation_0150C_indices.inc"
};

AnimationSet D_dryfield_night_driveway_8017EACC = {
    D_dryfield_night_driveway_8017E878,
    D_dryfield_night_driveway_8017EAA4,
    { NULL, D_dryfield_night_driveway_8017E684, NULL, NULL, D_dryfield_night_driveway_8017E6FC, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_driveway_8017EAF4[6] = {
#include "assets/dryfield_night_driveway_animation_01870_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_driveway_8017EB3C[75] = {
#include "assets/dryfield_night_driveway_animation_01870_bank4.inc"
};

AnimationRecord D_dryfield_night_driveway_8017EC68[104] = {
#include "assets/dryfield_night_driveway_animation_01870_records.inc"
};

u16 D_dryfield_night_driveway_8017EE08[20] = {
#include "assets/dryfield_night_driveway_animation_01870_indices.inc"
};

AnimationSet D_dryfield_night_driveway_8017EE30 = {
    D_dryfield_night_driveway_8017EC68,
    D_dryfield_night_driveway_8017EE08,
    { NULL, D_dryfield_night_driveway_8017EAF4, NULL, NULL, D_dryfield_night_driveway_8017EB3C, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_driveway_8017EE58[2] = {
#include "assets/dryfield_night_driveway_animation_01A84_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_driveway_8017EE70[24] = {
#include "assets/dryfield_night_driveway_animation_01A84_bank4.inc"
};

AnimationRecord D_dryfield_night_driveway_8017EED0[83] = {
#include "assets/dryfield_night_driveway_animation_01A84_records.inc"
};

u16 D_dryfield_night_driveway_8017F01C[20] = {
#include "assets/dryfield_night_driveway_animation_01A84_indices.inc"
};

AnimationSet D_dryfield_night_driveway_8017F044 = {
    D_dryfield_night_driveway_8017EED0,
    D_dryfield_night_driveway_8017F01C,
    { NULL, D_dryfield_night_driveway_8017EE58, NULL, NULL, D_dryfield_night_driveway_8017EE70, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_driveway_8017F06C[5] = {
#include "assets/dryfield_night_driveway_animation_01D64_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_driveway_8017F0A8[60] = {
#include "assets/dryfield_night_driveway_animation_01D64_bank4.inc"
};

AnimationRecord D_dryfield_night_driveway_8017F198[89] = {
#include "assets/dryfield_night_driveway_animation_01D64_records.inc"
};

u16 D_dryfield_night_driveway_8017F2FC[20] = {
#include "assets/dryfield_night_driveway_animation_01D64_indices.inc"
};

AnimationSet D_dryfield_night_driveway_8017F324 = {
    D_dryfield_night_driveway_8017F198,
    D_dryfield_night_driveway_8017F2FC,
    { NULL, D_dryfield_night_driveway_8017F06C, NULL, NULL, D_dryfield_night_driveway_8017F0A8, NULL, NULL, NULL },
};

TaskDesc D_dryfield_night_driveway_8017F34C[3] = {
    { 0, 32, func_dryfield_night_driveway_8017DAF4, { .model = NULL } },
    { 0, 32, func_dryfield_night_driveway_8017DB8C, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

AnimationSet* D_dryfield_night_driveway_8017F370[2] = {
    &D_dryfield_night_driveway_8017EACC,
    NULL,
};

GpCopyArg D_dryfield_night_driveway_8017F378 = { { .sets = D_dryfield_night_driveway_8017F370 }, 2 };

AnimationPlayRequest D_dryfield_night_driveway_8017F380 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F394 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 7, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F3A8 = { { .index = 1 }, 32, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_night_driveway_8017F3BC = { { -1067, 0, 1407, 0 }, { 0, 0, 0, 0 } };

GpEvsCmd D_dryfield_night_driveway_8017F3D4[14] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_driveway_8017F378 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F3A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5219000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_driveway_8017DC6C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F380 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

AnimationPlayRequest D_dryfield_night_driveway_8017F524 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F538 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpEvsCmd D_dryfield_night_driveway_8017F54C[16] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_dryfield_night_driveway_8017DC88 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F524 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x52190009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_night_driveway_8017DC78 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F538 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_driveway_8017F6CC[9] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_night_driveway_8017DC78 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpMsgEntry D_dryfield_night_driveway_8017F7A4[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_driveway_8017D7A0 },
    { 5105, func_dryfield_night_driveway_8017DCE4 },
    { 5103, func_dryfield_night_driveway_8017DCF4 },
    { 5104, func_dryfield_night_driveway_8017DCEC },
    { 5106, func_dryfield_night_driveway_8017DC94 },
    { 0x7FFFFFFF, NULL },
};

ActorTransform D_dryfield_night_driveway_8017F7D4 = { { -700, 0, 1320, 0 }, { 0, 1365, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F7EC = { { -700, 0, 1320, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F804 = { { 3300, 0, 790, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F81C = { { 100, 0, 790, 0 }, { 0, 3413, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F834 = { { -3000, 0, 790, 0 }, { 0, 3413, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F84C = { { -9527, 0, -1500, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F864 = { { -9527, 0, -1500, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F87C = { { -3800, 0, -1500, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F894 = { { -8800, 0, -1500, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F8AC = { { -3800, 0, -1500, 0 }, { 0, 3413, 0, 0 } };

DryfieldNightDrivewayAnimStorageF8C4 D_dryfield_night_driveway_8017F8C4 = { .data = { { &D_dryfield_night_driveway_8017EE30, &D_dryfield_night_driveway_8017F044, &D_dryfield_night_driveway_8017F324, NULL }, { { { .index = 1 }, 47, 0, 0, 1 }, { { .index = 1 }, 47, 0, 0, 1 } } } };

AnimationPlayRequest D_dryfield_night_driveway_8017F8FC = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F910 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

GpCopyArg D_dryfield_night_driveway_8017F924 = { { .words = D_dryfield_night_driveway_8017F8C4.words }, 10 };

AnimationPlayRequest D_dryfield_night_driveway_8017F92C = { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F940 = { { .index = 6 }, 38, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F954 = { { .index = 6 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F968 = { { .index = 6 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F97C = { { .index = 6 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpOverrideArg D_dryfield_night_driveway_8017F990 = { 4, 9 };

GpEvsCmd D_dryfield_night_driveway_8017F998[15] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_driveway_8017F7EC }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_driveway_8017F834 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F524 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_driveway_8017FB00[51] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F524 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F92C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_driveway_8017F924 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_driveway_8017F7D4 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_driveway_8017F804 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5319000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1019 }, { .storage = &D_dryfield_night_driveway_8017F81C }, { .storage = &D_dryfield_night_driveway_8017F990 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F8C4.data.arguments[1] }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F8FC }, { .value = 0 } },
    { 29, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_dryfield_night_driveway_8017F81C }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F954 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F97C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F940 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F968 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1019 }, { .storage = &D_dryfield_night_driveway_8017F834 }, { .storage = &D_dryfield_night_driveway_8017F990 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F910 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_dryfield_night_driveway_8017F7EC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_driveway_8017FFC8[14] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_driveway_8017F864 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_driveway_8017F8AC }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F524 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_driveway_80180118[49] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F524 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F92C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_driveway_8017F924 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_driveway_8017F84C }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_driveway_8017F87C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1019 }, { .storage = &D_dryfield_night_driveway_8017F894 }, { .storage = &D_dryfield_night_driveway_8017F990 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_dryfield_night_driveway_8017F894 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F954 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F97C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F8C4.data.arguments[1] }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F8FC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F940 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F968 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1019 }, { .storage = &D_dryfield_night_driveway_8017F8AC }, { .storage = &D_dryfield_night_driveway_8017F990 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F910 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_dryfield_night_driveway_8017F864 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_dryfield_night_driveway_801805B0[6] = {
    { -0x27B0, -2800, -1210, 0 },
    { -0x27B0, -3100, -1640, 0 },
    { -3960, -2070, 890, 0 },
    { -3960, -2070, 140, 0 },
    { 5740, -2160, 440, 0 },
    { 5740, -2430, 900, 0 },
};

GpRoomCoordRec D_dryfield_night_driveway_801805E0[2] = {
    { D_dryfield_night_driveway_80181DB0, D_dryfield_night_driveway_80182074 },
    { D_dryfield_night_driveway_80181DB0, D_dryfield_night_driveway_80182074 },
};

GpRoomObjRec D_dryfield_night_driveway_801805F0[2] = {
    { D_dryfield_night_driveway_80180C0C, D_dryfield_night_driveway_801818E8, D_dryfield_night_driveway_80181DC8, D_dryfield_night_driveway_80181FFC },
    { D_dryfield_night_driveway_80180C0C, D_dryfield_night_driveway_801818E8, D_dryfield_night_driveway_80181DC8, D_dryfield_night_driveway_80181FFC },
};

u8 D_dryfield_night_driveway_80180610[12] = {
    1,
    9,
    10,
    4,
    5,
    6,
    4,
    8,
    2,
    3,
    0,
    0,
};

u8* D_dryfield_night_driveway_8018061C[2] = {
    D_8010CAF8,
    D_dryfield_night_driveway_80180610,
};

GpViewCountRec D_dryfield_night_driveway_80180624[2] = {
    { { .bytes = { 10, 0 } } },
    { { .bytes = { 10, 0 } } },
};

GpWarpRec D_dryfield_night_driveway_80180628[4] = {
    { { .words = { 1024, -3400, 0, 600 } }, { 0, 0, 0, 0 }, { .words = { 1024, -3400, 0, 600 } }, { 0, 0, 0, 0 }, 0x53190004, 0x53190003, 0, 4, 0, 474 },
    { { .words = { 0x7800, -1300, 0, 1700 } }, { 0, 0, 0, 0 }, { .words = { 1024, -3400, 0, 600 } }, { 0, 0, 0, 0 }, 0, 0, 0, 4, 0, 0 },
    { { .words = { 1024, -9500, 4, -1418 } }, { 0, 0, 0, 0 }, { .words = { 1024, -9500, 4, -1418 } }, { 0, 0, 0, 0 }, 0x53190002, 0x53190001, 0, 2, 0, 482 },
    { { .words = { 0x7FFF, -1300, 0, 1700 } }, { 0, 0, 0, 0 }, { .words = { 1024, -3400, 0, 600 } }, { 0, 0, 0, 0 }, 0, 0, 0, 4, 0, 0 },
};

SVECTOR D_dryfield_night_driveway_80180708[13] = {
#include "assets/dryfield_night_driveway_collision_0364C_normals.inc"
};

SVECTOR D_dryfield_night_driveway_80180770[64] = {
#include "assets/dryfield_night_driveway_collision_0364C_verts.inc"
};

GpGridFace D_dryfield_night_driveway_80180970[23] = {
#include "assets/dryfield_night_driveway_collision_0364C_faces.inc"
};

s16 D_dryfield_night_driveway_80180A84[154] = {
#include "assets/dryfield_night_driveway_collision_0364C_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_driveway_80180A84[i])
s16* D_dryfield_night_driveway_80180BB8[21] = {
#include "assets/dryfield_night_driveway_collision_0364C_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_driveway_80180C0C[1] = {
    { NULL, D_dryfield_night_driveway_80180708, D_dryfield_night_driveway_80180770, D_dryfield_night_driveway_80180970, D_dryfield_night_driveway_80180BB8, 0x2B16, 5210, 7, 3, 4000, 23 },
};

GpViewRec D_dryfield_night_driveway_80180C30[10] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1950, 0x7530, -2670 } }, 380 },
    { { { { -651, 0, 4043 }, { -292, 4085, -47 }, { -4033, -296, -649 } }, { 4393, 820, 1267 } }, 230 },
    { { { { -852, 0, 4006 }, { 449, 4070, 95 }, { -3980, 459, -846 } }, { 425, 1344, 1172 } }, 230 },
    { { { { -1441, 0, 3833 }, { 939, 3971, 353 }, { -3717, 1003, -1397 } }, { -2544, 1724, -2548 } }, 230 },
    { { { { -781, 0, -4020 }, { -1703, 3710, 331 }, { 3642, 1735, -707 } }, { 1820, 2220, -2312 } }, 230 },
    { { { { 287, 0, 4085 }, { 2747, 3031, -193 }, { -3024, 2754, 213 } }, { 29, 1719, -2342 } }, 230 },
    { { { { -1441, 0, 3833 }, { 939, 3971, 353 }, { -3717, 1003, -1397 } }, { -2544, 1724, -2548 } }, 230 },
    { { { { 1822, 0, -3668 }, { -2650, 2831, -1317 }, { 2535, 2959, 1260 } }, { 1959, 2418, -462 } }, 289 },
    { { { { -651, 0, 4043 }, { -292, 4085, -47 }, { -4033, -296, -649 } }, { 4393, 820, 1267 } }, 230 },
    { { { { -852, 0, 4006 }, { 449, 4070, 95 }, { -3980, 459, -846 } }, { 425, 1344, 1172 } }, 230 },
};

SpriteBatch D_dryfield_night_driveway_80180D98[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_driveway_80180DA8[9] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 112, 375, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 40, 978, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -32, 88, 375, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 80, 375, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 88, 375, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 56, 54, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 128, 48, 62, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 104, 40, 81, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, 64, 32, 131, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_80180E5C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_driveway_80180E7C[17] = {
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 240, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -120, 80, 240, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -56, 88, 240, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 240, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, 24, 104, 240, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 112, 206, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, 72, 500, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 104, 500, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 104, 500, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 64, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, 56, 500, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 72, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 0, 900, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 48, 903, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 48, 0, 910, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 144 } }, 56, -120, 918, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, -48, 931, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_80180FD0[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 6, 6, 0, 0, { 3, 0 } },
    { 12, 1, 0, 0, { 2, 0 } },
    { 13, 2, 0, 0, { 4, 0 } },
    { 15, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_dryfield_night_driveway_80181008[2] = {
    { { 0, 0, 222, 239 }, 925 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_dryfield_night_driveway_8018101C[54] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -64, -16, 1125, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, -16, 1075, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -24, 1025, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, -160, -120, 835, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 72 } }, -160, -72, 860, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 64 } }, -160, 0, 910, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 128 } }, -40, -120, 1800, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 128 } }, -8, -120, 1825, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 128 } }, 24, -120, 1825, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 56, -80, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, -40, 1000, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -80, 1000, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 104, -16, 1000, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, 0, 1000, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 56, -96, 1000, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, -96, 1000, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -88, 825, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 40, -16, 875, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -88, 800, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, -64, 825, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 104, -16, 825, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 104, 0, 850, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, 48, 850, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 40, -104, 750, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 88, -104, 750, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -104, 762, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 80, -104, 762, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -104, 762, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -80, 851, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -56, 837, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, -40, 845, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 72, -64, 845, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 72, -56, 845, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -16, 1000, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, -24, 845, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 64, -40, 845, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 88, -56, 837, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -32, 837, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 80, -16, 837, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -96, 845, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 507, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 507, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 88, 48, 507, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 120, 56, 507, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 8, 900, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, 16, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 72, 16, 875, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 16, 875, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, 8, 850, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 0, 1064, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 72, 0, 1064, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 104, 8, 1064, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, 0, 1064, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, 0, 1064, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_80181454[13] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 0, 0, 0, { 6, 0 } },
    { 3, 3, 0, 0, { 2, 0 } },
    { 6, 3, 0, 0, { 7, 0 } },
    { 9, 7, 0, 0, { 5, 0 } },
    { 16, 9, 0, 0, { 8, 0 } },
    { 25, 3, 0, 0, { 4, 0 } },
    { 28, 12, 0, 0, { 9, 0 } },
    { 40, 4, 0, 0, { 0, 0 } },
    { 44, 5, 0, 0, { 10, 0 } },
    { 49, 5, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_driveway_801814BC[12] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 0, 969, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 8, 948, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 16, 888, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 24, 895, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 32, 890, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 40, 885, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 48, 880, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 56, 880, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 32, 650, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 80, 650, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, 32, 650, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, 72, 681, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_801815AC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_driveway_801815CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_driveway_801815DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_driveway_801815EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_driveway_801815FC[9] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 112, 375, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 40, 978, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -32, 88, 375, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 80, 375, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 88, 375, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 56, 54, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 128, 48, 62, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 104, 40, 81, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, 64, 32, 131, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_801816B0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_driveway_801816D0[17] = {
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 240, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -120, 80, 240, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -56, 88, 240, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 240, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, 24, 104, 240, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 112, 206, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, 72, 500, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 104, 500, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 104, 500, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 64, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, 56, 500, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 72, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 0, 900, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 48, 903, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 48, 0, 910, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 144 } }, 56, -120, 918, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, -48, 931, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_80181824[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 6, 6, 0, 0, { 3, 0 } },
    { 12, 1, 0, 0, { 2, 0 } },
    { 13, 2, 0, 0, { 4, 0 } },
    { 15, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_dryfield_night_driveway_8018185C[2] = {
    { { 0, 0, 222, 239 }, 925 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtRec D_dryfield_night_driveway_80181870[10] = {
    { { .empty = D_dryfield_night_driveway_80180D98 }, D_dryfield_night_driveway_80180D98, NULL },
    { { .elements = D_dryfield_night_driveway_80180DA8 }, D_dryfield_night_driveway_80180E5C, NULL },
    { { .elements = D_dryfield_night_driveway_80180E7C }, D_dryfield_night_driveway_80180FD0, D_dryfield_night_driveway_80181008 },
    { { .elements = D_dryfield_night_driveway_8018101C }, D_dryfield_night_driveway_80181454, NULL },
    { { .elements = D_dryfield_night_driveway_801814BC }, D_dryfield_night_driveway_801815AC, NULL },
    { { .empty = D_dryfield_night_driveway_801815CC }, D_dryfield_night_driveway_801815CC, NULL },
    { { .empty = D_dryfield_night_driveway_801815DC }, D_dryfield_night_driveway_801815DC, NULL },
    { { .empty = D_dryfield_night_driveway_801815EC }, D_dryfield_night_driveway_801815EC, NULL },
    { { .elements = D_dryfield_night_driveway_801815FC }, D_dryfield_night_driveway_801816B0, NULL },
    { { .elements = D_dryfield_night_driveway_801816D0 }, D_dryfield_night_driveway_80181824, D_dryfield_night_driveway_8018185C },
};

GpObj4C D_dryfield_night_driveway_801818E8[6] = {
    { NULL, NULL, NULL, { -6369, -1536, -1504, 0 }, { { 0, -2560, 1024, 0 }, { 0, -2560, -1024, 0 }, { 0, 2560, 1024, 0 }, { 0, 2560, -1024, 0 } }, { -4095, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2757, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -6816, -1472, -1568, 0 }, { { 0, -2496, -1024, 0 }, { 0, -2496, 1024, 0 }, { 0, 2496, -1024, 0 }, { 0, 2496, 1024, 0 } }, { 4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2697, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -2770, -1600, -856, 0 }, { { -1521, -2624, 0, 0 }, { 1522, -2624, 1, 0 }, { -1521, 2624, 0, 0 }, { 1522, 2624, 1, 0 } }, { 1, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -2675, -1376, -1017, 0 }, { { 1554, -2400, 1, 0 }, { -1553, -2400, 0, 0 }, { 1554, 2400, 1, 0 }, { -1553, 2400, 0, 0 } }, { -3, 0, 4097, 0 }, { 0, 0, 4096, 0 }, 2850, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 294, -1520, 1474, 0 }, { { -435, -2544, 2215, 0 }, { 435, -2544, -2214, 0 }, { -435, 2544, 2215, 0 }, { 435, 2544, -2214, 0 } }, { -4034, 0, -793, 0 }, { 0, 0, 4096, 0 }, 3396, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -26, -1488, 1301, 0 }, { { 435, -2512, -2213, 0 }, { -435, -2512, 2214, 0 }, { 435, 2512, -2213, 0 }, { -435, 2512, 2214, 0 } }, { 4031, 0, 792, 0 }, { 0, 0, 4096, 0 }, 3367, 0, 5, 4, 129, 0 },
};

GpPointLight D_dryfield_night_driveway_80181AB0[8] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2950, -2500, -1400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3686, 3276, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7437, -2500, -1400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3686, 3276, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4803, -2500, -1400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3686, 3276, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1618, -1633, -537 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3686, 3276, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3352, -2500, 705 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3686, 4096, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1510, -2500, 2719 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1670, -2500, 2899 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4860, -2500, 705 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3686, 4096, { 0, 0 } }, 1000, 3000 },
};

GpRoomCoordSet D_dryfield_night_driveway_80181DB0[1] = {
    { 0, NULL, 8, D_dryfield_night_driveway_80181AB0, 0, NULL },
};

GpObj4C D_dryfield_night_driveway_80181DC8[4] = {
    { NULL, NULL, NULL, { -3680, -48, 624, 0 }, { { -352, 0, -592, 0 }, { 352, 0, -592, 0 }, { -352, 0, 592, 0 }, { 352, 0, 592, 0 } }, { 0, 4107, 0, 0 }, { 4096, 0, 0, 0 }, 686, 0, 23, 17, 2, 0 },
    { NULL, NULL, NULL, { -1488, -48, 2496, 0 }, { { -1200, 0, -1216, 0 }, { 1200, 0, -1216, 0 }, { -1200, 0, 1024, 0 }, { 1200, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1707, 0, 32, 33, 4, 0 },
    { NULL, NULL, NULL, { -9664, -48, -1440, 0 }, { { -352, 0, -352, 0 }, { 352, 0, -352, 0 }, { -352, 0, 352, 0 }, { 352, 0, 352, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 497, 0, 2, 55, 2, 0 },
    { NULL, NULL, NULL, { 5984, -64, 1344, 0 }, { { -352, 0, -1024, 0 }, { 352, 0, -1024, 0 }, { -352, 0, 1024, 0 }, { 352, 0, 1024, 0 } }, { 0, 4094, 0, 0 }, { -4096, 0, 0, 0 }, 1078, 2, 7, 0, 130, 0 },
};

GpAreaTmdRec D_dryfield_night_driveway_80181EF8[2] = {
    { 37, 37, 0, 0, { 0, 0 }, D_80139DAC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_driveway_80181F10[2] = {
    { 37, 37, 0, 0, { 0, 0 }, D_80139DAC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_driveway_80181F28[3] = {
    { 15, 15, 0, 0, { 0, 0 }, D_8013BE28 },
    { 25, 25, 1, 0, { 0, 0 }, D_8014F9A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_driveway_80181F4C[22] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017C5B8, D_dryfield_night_driveway_80181EF8 },
    { NULL, NULL },
    { D_map_dryfield_full_8017C698, D_dryfield_night_driveway_80181F10 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017C768, D_dryfield_night_driveway_80181F28 },
};

GpObj3A D_dryfield_night_driveway_80181FFC[2] = {
    { NULL, NULL, { -5648, -2128, 1792, 0 }, { { -1648, 3152, 2624, 0 }, { 1648, 3152, -2624, 0 }, { -1648, -3152, 2624, 0 }, { 1648, -3152, -2624, 0 } }, { 3483, 0, 2187, 0 }, { 52, 17 }, 1, 0 },
    { NULL, NULL, { 1824, -2160, -2656, 0 }, { { -1648, 3184, 2624, 0 }, { 1648, 3184, -2624, 0 }, { -1648, -3184, 2624, 0 }, { 1648, -3184, -2624, 0 } }, { 3478, 0, 2184, 0 }, { 82, 17 }, 129, 0 },
};

WorldCoordRoomAmbientEntry D_dryfield_night_driveway_80182074[11] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_driveway_80182074) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 616, 617 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 615, 617, 616, 616 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

s32 D_dryfield_night_driveway_801820CC[3] = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

GpRoomParamRec D_dryfield_night_driveway_801820D8[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_driveway_801820E0[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_dryfield_night_driveway_801820E8[1] = {
    { 0, 0, 1, 0, D_dryfield_night_driveway_801820CC },
};

GpRoomParamRec* D_dryfield_night_driveway_801820F0[8] = {
    D_dryfield_night_driveway_801820D8,
    D_dryfield_night_driveway_801820E0,
    D_dryfield_night_driveway_801820E8,
    D_dryfield_night_driveway_801820D8,
    D_dryfield_night_driveway_801820D8,
    D_dryfield_night_driveway_801820D8,
    D_dryfield_night_driveway_801820D8,
    D_dryfield_night_driveway_801820D8,
};

RoomFadeStorage D_dryfield_night_driveway_80182110 = { 0 };

RoomEventMsg D_dryfield_night_driveway_80182118 = { 0 };

u8 D_dryfield_night_driveway_80182120[4] = {
    0,
    2,
    0,
    0,
};

RoomLatchedEvent D_dryfield_night_driveway_80182124 = { 0 };

static void func_dryfield_night_driveway_8017DDE4(SVECTOR* arg0, s32 arg1);

/// The room's event task, spawned by the event gate. State 0 runs the latched
/// event's CAP command; state 1 waits for it to finish and, when the event
/// asks for it, starts helper task 0x31; states 2 and 3 play the event's stage
/// sound and wait for it (state 2 skips to 4 when there is none); state 4
/// writes the latched message's destination into the save data and hands over
/// to task type 0x11.
void func_dryfield_night_driveway_8017D608(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_dryfield_night_driveway_80182124.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_dryfield_night_driveway_80182124.fade != 0) {
                    D_dryfield_night_driveway_80182110.fade.field_0 = 0;
                    D_dryfield_night_driveway_80182110.fade.field_1 = 0;
                    D_dryfield_night_driveway_80182110.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_dryfield_night_driveway_80182110.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_dryfield_night_driveway_80182124.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_dryfield_night_driveway_80182124.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_dryfield_night_driveway_80182124.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_dryfield_night_driveway_80182118.areaId;
            Mc_SaveData[0].state.at4.loc.warp = D_dryfield_night_driveway_80182118.warp;
            Mc_SaveData[0].state.at4.loc.room = D_dryfield_night_driveway_80182118.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// The room task's three states: set up, idle, kill.
static const TaskFuncTable3 D_dryfield_night_driveway_8017D5D8 = {
    { func_dryfield_night_driveway_8017DCFC, func_dryfield_night_driveway_8017DD7C, taskKill },
};

/// Event gate for the driveway. Every message is answered by editing the copy
/// in `out`; the two that matter are message 0x17, which reports whether the
/// road flag is clear and otherwise stages the pending request at
/// `D_dryfield_night_driveway_80182124` for `func_dryfield_night_driveway_8017D608`
/// to replay as a CAP command, and message 0x20, which reports the gate flag and
/// spawns the cutscene task at `D_dryfield_night_driveway_8017F34C`.
s32 func_dryfield_night_driveway_8017D7A0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent  req;
    RoomLatchedEvent* p;
    s32               fl;

    *out = *in;
    if (in->areaId == 0x17 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        fl        = GameFlag_GetNibble(0x47) == 0;
        out->room = fl ? 1 : 2;
    }
    if (in->areaId == 0x20 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        fl        = GameFlag_GetNibble(0x51) == 0;
        out->room = fl ? 2 : 1;
        if (GameFlag_GetNibble(0x53) != 0) {
            out->room = out->room + 2;
        }
    }
    if (in->areaId == 2 && GameFlag_GetNibble(0x61) != 0) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(6);
            Gp_SetNibbleIf(in->flagId, 2);
        }
        return 2;
    }
    if (in->areaId == 0x20) {
        if (GameFlag_GetNibble(0x3A) != 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gGameSession->at4.loc.stage == 2) {
                    if (gGameSession->at4.loc.variant == 1) {
                        if (GameFlag_GetNibble(0x50) == 0) {
                            Task_SpawnFromTable(D_dryfield_night_driveway_8017F34C, 1, 0, 0);
                            return 0;
                        }
                    }
                }
                if (gGameSession->at4.loc.variant == 1 && Gp_StateF0.prefix.bytes.field_0 == gGameSession->at4.loc.variant) {
                    return 0;
                }
                Gp_RunCapCmd1(1);
                return 0;
            }
            return 0;
        }
        if (in->queryOnly == ROOM_EVENT_EXECUTE && GameFlag_GetNibble(0x4B) == 1) {
            GameFlag_SetNibble(0x4B, 2);
        }
    }
    if (in->areaId == 0x17) {
        if (GameFlag_GetNibble(0x30) == 1) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(in->flagId, 2);
                Gp_RunCapCmd1(2);
                return 2;
            }
            return 2;
        }
        req.capCmd                               = 9;
        req.stageSnd                             = 0x52190003;
        req.flagId                               = 0x11C;
        req.fade                                 = 0;
        p                                        = &req;
        D_dryfield_night_driveway_80182120_value = 0;
        if (GameFlag_GetNibble(p->flagId) == 0 || p->flagId == 0) {
            if (out->queryOnly == ROOM_EVENT_EXECUTE) {
                D_dryfield_night_driveway_80182118 = *out;
                D_dryfield_night_driveway_80182124 = req;
                if (p->flagId != 0) {
                    GameFlag_SetNibble(p->flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_night_driveway_8017E678, 0, 0, 0);
                D_dryfield_night_driveway_80182120_value = 1;
                return 2;
            }
            return 2;
        }
    }
    return 1;
}

/// Task callback: on its first tick it hides the display and hands control to
/// the captioned cutscene; on every later tick it kills the task and clears the
/// collected bit. Either way it advances its own state.
void func_dryfield_night_driveway_8017DAF4(Task* arg0)
{
    if (arg0->state == 0) {
        gGameSession->hideHud = 1;
        D_80115768            = 1;
        SetDispMask(0);
        func_800E3FAC(0xA2, 0x10);
        func_800E8634(D_dryfield_night_driveway_8017F54C, 0, D_dryfield_night_driveway_8017F6CC);
    } else {
        taskKill(arg0);
        Gp_ClearCollectedBit(0x114);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Task callback: a four-step script. State 0 queues the weapon message and the
/// captioned command, state 1 waits one tick, state 2 starts the cutscene at
/// `D_dryfield_night_driveway_8017F3D4`, and state 3 - reached by falling out of
/// state 2 - clears area flag 4 for the current location and kills the task once
/// `eventState` is zero.
void func_dryfield_night_driveway_8017DB8C(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(1);
            arg0->state += 1;
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            func_800E8614(D_dryfield_night_driveway_8017F3D4, 0);
            arg0->state += 1;
            /* fallthrough */
        case 3:
            if (gGameSession->eventState == 0) {
                Gp_ClearAreaFlag4(&gGameSession->at4.loc);
                taskKill(arg0);
            }
            return;
    }
}

/// Script callback: stores its argument into `Gp_StateF0.field_1A`.
void func_dryfield_night_driveway_8017DC6C(s32 arg0)
{
    Gp_StateF0.field_1A = arg0;
}

/// Script callback: stores its argument into the session's `viewDirty`.
void func_dryfield_night_driveway_8017DC78(s16 arg0)
{
    gGameSession->viewDirty = arg0;
}

/// Script callback: stores its argument into `D_80115768`.
void func_dryfield_night_driveway_8017DC88(u8 arg0)
{
    D_80115768 = arg0;
}

/// Script-event hook: events 8 and 10 each queue their stage sound; every
/// event returns 0.
s32 func_dryfield_night_driveway_8017DC94(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    switch (arg2) {
        case 8:
            Gp_EnqueueStageSnd6(0x52190008, 0, 0);
            break;
        case 10:
            Gp_EnqueueStageSnd6(0x5219000A, 0, 0);
            break;
    }
    return 0;
}

/// Message handlers that answer 0 (messages 0x13F1, 0x13F0 and 0x13EF of the
/// room's message table).
s32 func_dryfield_night_driveway_8017DCE4(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_night_driveway_8017DCEC(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_night_driveway_8017DCF4(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// First state of the room task: installs the room's message table and takes
/// pointer slot 7; when slot 0xA is set and the room was entered by warp 4, it
/// also hands `D_dryfield_night_driveway_8017FB00` and
/// `D_dryfield_night_driveway_8017F998` to `func_800E8634`. Then advances the
/// state.
static void func_dryfield_night_driveway_8017DCFC(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_driveway_8017F7A4;
    Game_SetPtrSlot(arg0, 7);
    if ((gameGetPtrSlot(0xA) != 0) && (gGameSession->at4.loc.warp == 4)) {
        func_800E8634(D_dryfield_night_driveway_8017FB00, 0, D_dryfield_night_driveway_8017F998);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Empty task state: the middle entry of the room task's state table. Its only
/// trace is a 0x10-byte stack frame.
static void func_dryfield_night_driveway_8017DD7C(Task* task)
{
    char pad[0x10];
}

/// Room task: copies the state table onto the stack and runs the entry for the
/// task's current state.
void func_dryfield_night_driveway_8017DD8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_driveway_8017D5D8;
    sp.funcs[task->state](task);
}

/// Draws a light shaft between the two world points `arg0[0]` and `arg0[1]`:
/// a fan of gouraud wedges around each projected point, joined by wedges
/// spanning the two, the sweep oriented along the screen-space line between
/// them. Each radius is `(s16)arg1 * 64` over that point's OTZ. Nothing is
/// drawn unless both points project. The lit vertices take a brightness that
/// flickers with the frame counter.
static void func_dryfield_night_driveway_8017DDE4(SVECTOR* arg0, s32 arg1)
{
    SVECTOR*                 p1;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    s32                      raw;
    s32                      ang;
    s32                      angEnd;
    s32                      limit;
    s32                      angStart;
    s32                      t;
    s32                      t2;
    s32                      t3;
    s32                      conn;
    s32                      scaled;
    s32                      blend;

    p1 = arg0 + 1;
    SCRATCH_PUSH(OverlayPointPairScratch);
    block = SCRATCH_HEAD(OverlayPointPairScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / block->otz0;
            block->r1 = scaled / block->otz1;
            raw       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)raw;
            blend     = (((u8)ds->animFrame & 1) * 0x10) | 0x20;
            angEnd    = ang + 0x800;
            if (ang < angEnd) {
                angStart = ang;
                limit    = angEnd;
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
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
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, blend, blend, blend);
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

                    prim           = gGpuPrimCursor;
                    t3             = ang + 0x800;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
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
    SCRATCH_POP(OverlayPointPairScratch);
}

/// Room draw hook: sets the effect mode to 2, then draws the beams the current
/// view (`gGameSession->at4.loc.view`) shows - views 2 and 9 the first pair, 4
/// and 7 the second, 5 the third, and 3 and 10 both the first and second.
void func_dryfield_night_driveway_8017E5CC(Task* unused)
{
    Gp_State1C->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    switch (gGameSession->at4.loc.view) {
        case 2:
        case 9:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[0], 0x180);
            break;
        case 4:
        case 7:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[2], 0x180);
            break;
        case 5:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[4], 0x180);
            break;
        case 3:
        case 10:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[0], 0x180);
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[2], 0x180);
            break;
    }
}
