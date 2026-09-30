#include "rooms/dryfield_junk_yard.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

// Retained exporter slots follow the active spotlights. Their contents
// include stale/incomplete addresses; preserve them as bytes pending review.
typedef struct {
    GpSpotLight active[2];
    u8          retained[648];
} DryfieldJunkYardSpotLightStorage;
STATIC_ASSERT_SIZEOF(DryfieldJunkYardSpotLightStorage, 864);

extern DryfieldJunkYardSpotLightStorage D_dryfield_junk_yard_80181854;

/// Block `func_dryfield_junk_yard_8017D658` carves off the scratch stack
/// (`0x1F8003FC`, one `addiu` of `-0x18`) to hold the model's world position
/// while `Gp_DrawEffGroundQuad` draws the ground quad there. Only `pos` is
/// written; the rest of the 0x18 bytes is not touched here.
typedef struct {
    VECTOR3 pos;
    byte    pad_C[0xC];
} DjyGroundQuadScratch;
STATIC_ASSERT_SIZEOF(DjyGroundQuadScratch, 0x18);

/// Resident routine at the fixed address `0x80724608`, outside every image
/// the build links. The room hands it the slot-0xA game pointer, two
/// constants and the `"DOG"` name below; what it does with them is unproven.
void func_80724608(void* owner, s32 arg1, s32 arg2, void* name);

/// Main-executable halfword the room task's second state waits on before it
/// calls `func_80724608`.
/// Signed byte of gameplay state the room's script callback stores into.

/// The room's message table, published in `Task::msgTable` for
/// `Gp_DispatchMsg` to walk.
extern GpMsgEntry           D_dryfield_junk_yard_8017DD20[];
extern TaskDesc             D_dryfield_junk_yard_8017DD48[];
extern AnimationPlayRequest D_dryfield_junk_yard_8017DD88;
extern AnimationPlayRequest D_dryfield_junk_yard_8017DDD8;
extern AnimationPlayRequest D_dryfield_junk_yard_8017DDEC;
extern ActorTransform       D_dryfield_junk_yard_8017DE00;
extern ActorTransform       D_dryfield_junk_yard_8017DE18;
extern ActorTransform       D_dryfield_junk_yard_8017DE30;
extern GpEvsCmd             D_dryfield_junk_yard_8017DE48[];
extern GpEvsCmd             D_dryfield_junk_yard_8017E028[];
extern GpEvsCmd             D_dryfield_junk_yard_8017E160[];
extern GpEvsCmd             D_dryfield_junk_yard_8017E2B0[];
extern GpEvsCmd             D_dryfield_junk_yard_8017E3D0[];
extern GpEvsCmd             D_dryfield_junk_yard_8017E490[];
extern GpEvsCmd             D_dryfield_junk_yard_8017E658[];

static void func_dryfield_junk_yard_8017D658(Task* task);
static void func_dryfield_junk_yard_8017D708(Task* arg0);
static void func_dryfield_junk_yard_8017DC60(Task* task);

/// The room task's states: set up, start the named sequence once the stream
/// is ready, then `taskKill`.
static const TaskFuncTable3 D_dryfield_junk_yard_8017D5C4 = {
    { func_dryfield_junk_yard_8017D708, func_dryfield_junk_yard_8017DC60, taskKill },
};

/// Name the room task's second state hands to `func_80724608`.
static const char D_dryfield_junk_yard_8017D5D0[] = "DOG";

void func_dryfield_junk_yard_8017D848(Task*);
s32  func_dryfield_junk_yard_8017D994(Task*, s32, s32, TaskMessageArg);
s32  func_dryfield_junk_yard_8017DA44(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_dryfield_junk_yard_8017DA4C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_dryfield_junk_yard_8017DB78(Task*, s32, DirectionActionRequest* msg, TaskMessageArg);

extern AnimationPlayRequest       D_dryfield_junk_yard_8017DD60;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DD74;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DD88;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DD9C;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DDB0;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DDC4;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DDEC;
extern GpGridParams               D_dryfield_junk_yard_8017F4C8[1];
extern GpObj3A                    D_dryfield_junk_yard_80181518[1];
extern GpObj4C                    D_dryfield_junk_yard_80180C7C[10];
extern GpObj4C                    D_dryfield_junk_yard_80180F74[19];
extern WorldCoordRoomAmbientEntry D_dryfield_junk_yard_80181BCC[8];
extern GpRoomCoordSet             D_dryfield_junk_yard_80181BB4[1];
extern ActorTransform             D_dryfield_junk_yard_8017DE00;
extern TaskDesc                   D_8014D8A4;
void                              func_dryfield_junk_yard_8017DC54(s8);

extern DryfieldJunkYardSpotLightStorage D_dryfield_junk_yard_80181854;
extern GpPointLight                     D_dryfield_junk_yard_80181554[8];

GpMsgEntry D_dryfield_junk_yard_8017DD20[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_junk_yard_8017DA4C },
    { 5105, func_dryfield_junk_yard_8017DA44 },
    { 5104, func_dryfield_junk_yard_8017D994 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_junk_yard_8017DB78 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_junk_yard_8017DD48[2] = {
    { 0, 32, func_dryfield_junk_yard_8017D848, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

AnimationPlayRequest D_dryfield_junk_yard_8017DD60 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DD74 = { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DD88 = { { .index = 6 }, 34, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DD9C = { { .index = 6 }, 35, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DDB0 = { { .index = 6 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DDC4 = { { .index = 6 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DDD8 = { { .index = 6 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DDEC = { { .index = 6 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_junk_yard_8017DE00 = { { 0x288B, 0, 3893, 0 }, { 0, 2700, 0, 0 } };

ActorTransform D_dryfield_junk_yard_8017DE18 = { { 0x530A, 0, 3407, 0 }, { 0, 3074, 0, 0 } };

ActorTransform D_dryfield_junk_yard_8017DE30 = { { 0x54C4, 0, 3800, 0 }, { 0, 2048, 0, 0 } };

GpEvsCmd D_dryfield_junk_yard_8017DE48[20] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_junk_yard_8017DE00 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .storage = &D_dryfield_junk_yard_8017DD88 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD9C }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DDB0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DDC4 }, { .value = 0 } },
    { 15, { .value = 0x40720009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_junk_yard_8017E028[13] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_junk_yard_8017DE00 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .storage = &D_dryfield_junk_yard_8017DDEC }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_junk_yard_8017E160[14] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_dryfield_junk_yard_8017DE30 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x40720009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .storage = &D_dryfield_junk_yard_8017DDC4 }, { .value = 0 } },
    { 4, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .storage = &D_dryfield_junk_yard_8017DD74 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_junk_yard_8017E2B0[12] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_junk_yard_8017DE30 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .storage = &D_dryfield_junk_yard_8017DD74 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_junk_yard_8017E3D0[8] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x521A0008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_junk_yard_8017E490[19] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_junk_yard_8017DE30 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .storage = &D_dryfield_junk_yard_8017DDEC }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .storage = &D_dryfield_junk_yard_8017DDB0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x40720009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .storage = &D_dryfield_junk_yard_8017DDC4 }, { .value = 0 } },
    { 4, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS8 = func_dryfield_junk_yard_8017DC54 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_junk_yard_8017E658[11] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS8 = func_dryfield_junk_yard_8017DC54 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_dryfield_junk_yard_8017E760[1] = {
#include "assets/dryfield_junk_yard_model_01720_skeleton.inc"
};

u32 D_dryfield_junk_yard_8017E784[1] = {
#include "assets/dryfield_junk_yard_model_01720_partVerts.inc"
};

SVECTOR D_dryfield_junk_yard_8017E788[54] = {
#include "assets/dryfield_junk_yard_model_01720_verts.inc"
};

u32 D_dryfield_junk_yard_8017E938[234] = {
#include "assets/dryfield_junk_yard_model_01720_stream.inc"
};

TmdSource D_dryfield_junk_yard_8017ECE0 = {
    0,
    1640,
    0,
    1,
    D_dryfield_junk_yard_8017E784,
    D_dryfield_junk_yard_8017E788,
    &D_dryfield_junk_yard_8017E788[54],
    D_dryfield_junk_yard_8017E760,
    D_dryfield_junk_yard_8017E938,
};

GpRoomObjRec D_dryfield_junk_yard_8017ED04[1] = {
    { D_dryfield_junk_yard_8017F4C8, D_dryfield_junk_yard_80180C7C, D_dryfield_junk_yard_80180F74, D_dryfield_junk_yard_80181518 },
};

GpRoomCoordRec D_dryfield_junk_yard_8017ED14[1] = {
    { D_dryfield_junk_yard_80181BB4, D_dryfield_junk_yard_80181BCC },
};

u8* D_dryfield_junk_yard_8017ED1C[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_junk_yard_8017ED20[1] = {
    { { .bytes = { 7, 0 } } },
};

GpWarpRec D_dryfield_junk_yard_8017ED24[3] = {
    { { .words = { 0, 5030, 0, 390 } }, { 0, 0, 0, 0 }, { .words = { 2048, 4800, 0, 1000 } }, { 0, 0, 0, 0 }, 0x521A0004, 0x521A0003, 0, 2, 0, 472 },
    { { .words = { 0, 0x55A1, 0, 1639 } }, { 0, 0, 0, 0 }, { .words = { 0, 0x55A1, 0, 1639 } }, { 0, 0, 0, 0 }, 0x521A0002, 0x521A0001, 0, 5, 0, 471 },
    { { .words = { 0, 1859, 0, 672 } }, { 0, 0, 0, 0 }, { .words = { 0, 1859, 0, 672 } }, { 0, 0, 0, 0 }, 0x521A0006, 0x521A0005, 0, 3, 0, 0 },
};

SVECTOR D_dryfield_junk_yard_8017EDCC[33] = {
#include "assets/dryfield_junk_yard_collision_01F08_normals.inc"
};

SVECTOR D_dryfield_junk_yard_8017EED4[88] = {
#include "assets/dryfield_junk_yard_collision_01F08_verts.inc"
};

GpGridFace D_dryfield_junk_yard_8017F194[36] = {
#include "assets/dryfield_junk_yard_collision_01F08_faces.inc"
};

s16 D_dryfield_junk_yard_8017F344[166] = {
#include "assets/dryfield_junk_yard_collision_01F08_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_junk_yard_8017F344[i])
s16* D_dryfield_junk_yard_8017F490[14] = {
#include "assets/dryfield_junk_yard_collision_01F08_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_junk_yard_8017F4C8[1] = {
    { NULL, D_dryfield_junk_yard_8017EDCC, D_dryfield_junk_yard_8017EED4, D_dryfield_junk_yard_8017F194, D_dryfield_junk_yard_8017F490, 2400, 608, 7, 2, 4000, 36 },
};

GpAreaTmdRec D_dryfield_junk_yard_8017F4EC[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_junk_yard_8017F4F8[2] = {
    { 25, 25, 0, 0, { 0, 0 }, D_801379A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_junk_yard_8017F510[2] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_junk_yard_8017F528[3] = {
    { 1, 0, 0, 5685, 0, 2272, 4096, 0, 0, 2, 0 },
    { 1, 0, 0, 5485, 0, 2072, 4352, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_dryfield_junk_yard_8017F558[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017B9B4, D_dryfield_junk_yard_8017F4EC },
    { D_map_dryfield_8017B9C4, D_dryfield_junk_yard_8017F4F8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_junk_yard_8017F528, D_dryfield_junk_yard_8017F510 },
    { NULL, NULL },
    { NULL, NULL },
};

GpViewRec D_dryfield_junk_yard_8017F5C0[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2DE6, 0x61A8, -5750 } }, 257 },
    { { { { 965, 0, -3980 }, { -535, 4058, -129 }, { 3944, 550, 956 } }, { 1022, 2299, -989 } }, 282 },
    { { { { 783, 0, -4020 }, { -3412, 2165, -665 }, { 2125, 3476, 414 } }, { 1120, 4985, -2136 } }, 239 },
    { { { { 1009, 0, -3969 }, { -1187, 3908, -301 }, { 3788, 1224, 962 } }, { -2958, 2969, -1227 } }, 282 },
    { { { { 66, 0, 4095 }, { 1199, 3916, -19 }, { -3915, 1199, 63 } }, { -0x63AC, 2717, -2429 } }, 289 },
    { { { { 1933, 0, -3610 }, { -957, 3949, -512 }, { 3481, 1085, 1864 } }, { -8412, 1167, -2987 } }, 257 },
    { { { { 1162, 0, 3927 }, { 1136, 3920, -336 }, { -3759, 1185, 1113 } }, { -0x4970, 2301, -1670 } }, 269 },
};

SpriteBatch D_dryfield_junk_yard_8017F6BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_junk_yard_8017F6CC[30] = {
    { 143, 0x3FC0, { .fields = { 32, 128 } }, -40, -104, 3066, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 24, -72, 6500, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 120 } }, 56, -112, 5000, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -72, 64, 1304, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, -96, -32, 1248, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 1068, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -88, 48, 1249, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 56, 1146, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 72 } }, -160, 48, 937, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 40, 1038, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -120, 40, 988, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -112, 40, 1281, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 48, 1249, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -112, 56, 1036, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -104, 56, 1074, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -80, 64, 1281, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -80, 40, 1282, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, -24, 4346, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, -16, 4080, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 24, -8, 3816, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 64, 1172, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 64, 1189, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 120, 72, 1150, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 72, 1160, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, -16, 3495, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -40, 3638, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 80, -72, 3625, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 88, -104, 3625, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 96 } }, 96, 24, 3500, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 144 } }, 96, -120, 3500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_junk_yard_8017F924[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { 3, 14, 0, 0, { 3, 0 } },
    { 17, 3, 0, 0, { 2, 0 } },
    { 20, 4, 0, 0, { 4, 0 } },
    { 24, 6, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_junk_yard_8017F95C[60] = {
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -104, 0, 1538, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, 0, 1522, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -64, -56, 1522, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -104, -56, 1538, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -64, -120, 1372, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -104, -120, 1388, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, -80, 1541, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, -72, 1541, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, -104, -80, 1528, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -64, 1558, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -56, 1577, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -48, 1578, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 56, 984, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 64, 1124, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 64, 1038, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 64, 976, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 72, 955, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 72, 951, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 72, 952, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 72, 949, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 72, 951, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 80, 942, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 80, 937, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 80, 934, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 930, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 80, 931, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 88, 929, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 88, 923, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 88, 920, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 88, 915, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 88, 916, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 88, 918, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 96, 916, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 96, 906, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 96, 903, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 96, 900, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 96, 903, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 904, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 104, 918, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 898, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 104, 888, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 104, 884, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 104, 885, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 104, 890, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 112, 963, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 112, 938, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 931, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 112, 887, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 878, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 886, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -48, 24, 950, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 56, 56, 1134, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -136, 24, 1220, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -88, 32, 1270, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 48, 1196, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, 56, 1174, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 64, 1158, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 24, 72, 1142, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, 48, 1184, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, 48, 1200, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_junk_yard_8017FE0C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 3, 0, 0, { 2, 0 } },
    { 12, 48, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_junk_yard_8017FE34[25] = {
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, -16, 2084, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 0, 2125, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 16, 2042, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 0, 2069, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -24, 2101, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, -40, 2069, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, -120, 2069, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 80, -88, 3836, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, -120, 3836, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, -80, 6000, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -112, 6000, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, -56, 3550, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 32, -48, 3452, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 24, -40, 3366, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -32, 2914, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -32, 2838, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, -32, 3473, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -24, 2857, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 104, -120, 2500, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, -72, 2750, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -32, 2545, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 120, 40, 2500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -16, 2500, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -72, 2500, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 2500, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_junk_yard_80180028[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 7, 0, 0, { 2, 0 } },
    { 18, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_junk_yard_80180050[97] = {
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 16, -112, 8250, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -96, -48, 1825, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, -120, 1825, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 32, -80, 4051, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 4051, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 8, -88, 7006, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 2314, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 2762, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -32, 2398, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -32, 2533, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -24, 2312, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -16, 2122, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -8, 1963, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 0, 1825, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 8, 1720, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, 16, 1627, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -40, 2182, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -32, 1941, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -64, -24, 1907, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -72, -16, 1862, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -72, -8, 1785, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -80, 0, 1689, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -24, 2039, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 24 } }, -80, 8, 1673, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 72, 1250, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 1256, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, -48, 1372, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -88, 0, 1267, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, 16, 1379, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -56, 40, 1314, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -56, 0, 1388, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 1250, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 1250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 1250, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 1250, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 1250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 1250, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 1250, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -80, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -40, 1250, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 0, 1250, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -120, 1290, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -80, 1290, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -40, 1290, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 0, 1290, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 104, 569, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -88, 24, 1275, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -120, 40, 1250, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 40, 1253, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -112, 72, 1131, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -120, 80, 1079, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -120, 88, 1032, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -120, 96, 610, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 1521, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, 120, -16, 1266, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 96, -8, 1584, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, -8, 1200, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 128, 16, 1200, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 24, 1050, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 144, 40, 1075, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, 48, 974, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, 48, 931, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 136, 40, 1035, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 128, 24, 1102, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 16, 1203, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 0, 1290, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, 8, 1376, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 0, 1476, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 8, 1618, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 0, 1290, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 120, -16, 1237, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 128, -16, 1151, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 136, -8, 1067, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 144, -8, 990, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 152, -8, 959, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -48, 2676, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -48, 2513, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -56, 2167, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 72, -56, 2248, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 72, -48, 2192, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 72, -32, 2202, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, 80, -24, 2196, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -24, 2034, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -16, 1882, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 32 } }, 88, -16, 1887, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -152, 96, 575, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -112, 96, 575, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -72, 96, 575, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -32, 96, 575, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 8, 96, 600, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, 96, 575, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, 96, 500, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, 64, 575, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -24, 2394, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -48, 2252, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -56, 2177, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -80, -72, 2583, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_junk_yard_801807E4[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 6, 0 } },
    { 5, 1, 0, 0, { 1, 0 } },
    { 6, 18, 0, 0, { 8, 0 } },
    { 24, 29, 0, 0, { 0, 0 } },
    { 53, 16, 0, 0, { 5, 0 } },
    { 69, 6, 0, 0, { 2, 0 } },
    { 75, 10, 0, 0, { 9, 0 } },
    { 85, 8, 0, 0, { 4, 0 } },
    { 93, 3, 0, 0, { 7, 0 } },
    { 96, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_junk_yard_80180844[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_junk_yard_80180854[47] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 56, 719, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 64, 553, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -56, -88, 4064, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -120, 2402, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, -32, 2312, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -32, 2340, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 8, 1222, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 24, 1098, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -64, 16, 1209, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -120, 16, 1177, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -128, 32, 1056, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -80, 32, 657, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -48, 40, 593, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -136, 40, 597, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 56, 718, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -48, 56, 530, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 16 } }, -144, 56, 540, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -48, 72, 487, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -160, 72, 491, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, -40, 88, 447, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, -160, 88, 447, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 16 } }, -32, 104, 408, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 16 } }, -160, 104, 411, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -8, 1363, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -8, 1312, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, -8, 1246, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, 0, 1148, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 1347, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 112, -16, 1388, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -24, 1012, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, -8, 1146, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, 0, 1078, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -8, 995, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 32, 1004, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -24, 2186, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, 0, 1521, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 16, 1234, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, -8, 1283, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 0, 1243, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 0, 1243, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, -16, 1314, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, -8, 1300, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -64, 1156, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -56, 1031, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -48, 1122, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 8, 1084, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, 56, 960, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_junk_yard_80180C00[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 1, 0 } },
    { 34, 8, 0, 0, { 2, 0 } },
    { 42, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_junk_yard_80180C28[7] = {
    { { .empty = D_dryfield_junk_yard_8017F6BC }, D_dryfield_junk_yard_8017F6BC, NULL },
    { { .elements = D_dryfield_junk_yard_8017F6CC }, D_dryfield_junk_yard_8017F924, NULL },
    { { .elements = D_dryfield_junk_yard_8017F95C }, D_dryfield_junk_yard_8017FE0C, NULL },
    { { .elements = D_dryfield_junk_yard_8017FE34 }, D_dryfield_junk_yard_80180028, NULL },
    { { .elements = D_dryfield_junk_yard_80180050 }, D_dryfield_junk_yard_801807E4, NULL },
    { { .empty = D_dryfield_junk_yard_80180844 }, D_dryfield_junk_yard_80180844, NULL },
    { { .elements = D_dryfield_junk_yard_80180854 }, D_dryfield_junk_yard_80180C00, NULL },
};

GpObj4C D_dryfield_junk_yard_80180C7C[10] = {
    { NULL, NULL, NULL, { 3568, -1408, 2448, 0 }, { { 467, -2432, -3264, 0 }, { -494, -2432, 3235, 0 }, { 467, 2432, -3264, 0 }, { -494, 2432, 3235, 0 } }, { 4052, 0, 599, 0 }, { 0, 0, 4096, 0 }, 4087, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 3680, -1376, 2336, 0 }, { { -517, -2400, 3225, 0 }, { 476, -2400, -3264, 0 }, { -517, 2400, 3225, 0 }, { 476, 2400, -3264, 0 } }, { -4056, 0, -621, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 7324, -1328, 2299, 0 }, { { 335, -2352, -3268, 0 }, { -337, -2352, 3267, 0 }, { 335, 2352, -3268, 0 }, { -337, 2352, 3267, 0 } }, { 4074, 0, 418, 0 }, { 0, 0, 4096, 0 }, 4039, 0, 4, 2, 1, 0 },
    { NULL, NULL, NULL, { 7483, -1296, 2174, 0 }, { { -337, -2320, 3267, 0 }, { 335, -2320, -3268, 0 }, { -337, 2320, 3267, 0 }, { 335, 2320, -3268, 0 } }, { -4076, 0, -420, 0 }, { 0, 0, 4096, 0 }, 4015, 0, 2, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x3FFF, -1296, 3775, 0 }, { { -407, -2320, 1963, 0 }, { 403, -2320, -1967, 0 }, { -407, 2320, 1963, 0 }, { 403, 2320, -1967, 0 } }, { -4033, 0, -832, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 7, 5, 1, 0 },
    { NULL, NULL, NULL, { 0x3F40, -1328, 3727, 0 }, { { 400, -2352, -1951, 0 }, { -404, -2352, 1947, 0 }, { 400, 2352, -1951, 0 }, { -404, 2352, 1947, 0 } }, { 4027, 0, 830, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 5, 7, 1, 0 },
    { NULL, NULL, NULL, { 0x3400, -1344, 928, 0 }, { { 1262, -2352, -2822, 0 }, { -1262, -2352, 2822, 0 }, { 1262, 2352, -2822, 0 }, { -1262, 2352, 2822, 0 } }, { 3740, 0, 1672, 0 }, { 0, 0, 4096, 0 }, 3882, 0, 7, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x3430, -1312, 5328, 0 }, { { -1330, -2352, -1782, 0 }, { 1330, -2352, 1782, 0 }, { -1330, 2352, -1782, 0 }, { 1330, 2352, 1782, 0 } }, { 3290, 0, -2457, 0 }, { 0, 0, 4096, 0 }, 3228, 0, 7, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x3531, -1344, 5393, 0 }, { { 1310, -2352, 1786, 0 }, { -1310, -2352, -1786, 0 }, { 1310, 2352, 1786, 0 }, { -1310, 2352, -1786, 0 } }, { -3316, 0, 2430, 0 }, { 0, 0, 4096, 0 }, 3228, 0, 4, 7, 1, 0 },
    { NULL, NULL, NULL, { 0x345F, -1345, 1296, 0 }, { { -1058, -2352, 2442, 0 }, { 1058, -2352, -2442, 0 }, { -1058, 2352, 2442, 0 }, { 1058, 2352, -2442, 0 } }, { -3765, 0, -1632, 0 }, { 0, 0, 4096, 0 }, 3547, 0, 4, 7, 129, 0 },
};

GpObj4C D_dryfield_junk_yard_80180F74[19] = {
    { NULL, NULL, NULL, { 5248, -48, 336, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, 0, 24, 18, 2, 0 },
    { NULL, NULL, NULL, { 0x5600, -48, 1568, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, 0, 27, 33, 2, 0 },
    { NULL, NULL, NULL, { 432, -64, 4592, 0 }, { { -896, 0, -1056, 0 }, { 896, 0, -1056, 0 }, { -896, 0, 1056, 0 }, { 896, 0, 1056, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1384, 2, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { 6623, -64, 2783, 0 }, { { -935, 0, 3140, 0 }, { -306, 0, -3261, 0 }, { 307, 0, 3262, 0 }, { 936, 0, -3139, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 3268, 0x8005, 1, 0, 3, 0 },
    { NULL, NULL, NULL, { 0x4A1F, -64, 3487, 0 }, { { -449, 0, 1009, 0 }, { -148, 0, -1017, 0 }, { 149, 0, 1018, 0 }, { 450, 0, -1008, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 1101, 0x8005, 2, 0, 3, 0 },
    { NULL, NULL, NULL, { 1744, -64, 351, 0 }, { { -1040, 0, -592, 0 }, { 1040, 0, -592, 0 }, { -1040, 0, 849, 0 }, { 1040, 0, 849, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1342, 2, 6, 255, 4, 0 },
    { NULL, NULL, NULL, { 0x41A2, -64, 3328, 0 }, { { -2624, 0, -272, 0 }, { 2624, 0, -656, 0 }, { -2624, 0, 656, 0 }, { 2624, 0, 272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 2697, 2, 16, 0, 2, 0 },
    { NULL, NULL, NULL, { 352, -64, 2336, 0 }, { { -896, 0, -1056, 0 }, { 896, 0, -1056, 0 }, { -896, 0, 1056, 0 }, { 896, 0, 1056, 0 } }, { 0, 4100, 0, 0 }, { 3973, 0, -995, 0 }, 1384, 2, 14, 0, 2, 0 },
    { NULL, NULL, NULL, { 1376, -64, 4096, 0 }, { { -480, 0, -1344, 0 }, { 448, 0, -1280, 0 }, { -480, 0, 1312, 0 }, { 512, 0, 1312, 0 } }, { 0, 4100, 0, 0 }, { -4077, 0, -402, 0 }, 1425, 2, 15, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x5088, -64, 3056, 0 }, { { -1032, 0, -528, 0 }, { 1112, 0, -528, 0 }, { -1032, 0, 528, 0 }, { 952, 0, 528, 0 } }, { 0, 4106, 0, 0 }, { 200, 0, 4090, 0 }, 1227, 2, 17, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x51B0, -64, 4144, 0 }, { { -1840, 0, -464, 0 }, { 1840, 0, -464, 0 }, { -1840, 0, 464, 0 }, { 1840, 0, 464, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1894, 2, 18, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x4140, -64, 4032, 0 }, { { -2384, 0, -464, 0 }, { 2384, 0, -464, 0 }, { -2384, 0, 464, 0 }, { 2384, 0, 464, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2428, 2, 24, 0, 2, 0 },
    { NULL, NULL, NULL, { 2960, -64, 3136, 0 }, { { -1680, 0, -416, 0 }, { 1648, 0, -352, 0 }, { -1680, 0, 384, 0 }, { 1712, 0, 384, 0 } }, { 0, 4098, 0, 0 }, { -201, 0, -4091, 0 }, 1750, 2, 15, 0, 2, 0 },
    { NULL, NULL, NULL, { 4224, -64, 4160, 0 }, { { -576, 0, -1344, 0 }, { 352, 0, -1280, 0 }, { -576, 0, 1312, 0 }, { 800, 0, 1312, 0 } }, { 0, 4100, 0, 0 }, { 3856, 0, -1380, 0 }, 1536, 2, 15, 0, 2, 0 },
    { NULL, NULL, NULL, { 864, -64, 608, 0 }, { { -704, 0, -672, 0 }, { 704, 0, -672, 0 }, { -704, 0, 672, 0 }, { 704, 0, 672, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 972, 2, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x3660, -64, 4720, 0 }, { { -239, 0, -1216, 0 }, { 848, 0, -1216, 0 }, { -1455, 0, 1216, 0 }, { 848, 0, 1216, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 1894, 2, 24, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x3587, -64, 2471, 0 }, { { -318, 0, 1507, 0 }, { -895, 0, -1373, 0 }, { 880, 0, 1326, 0 }, { 335, 0, -1458, 0 } }, { 0, 4101, 0, 0 }, { -4091, 0, 201, 0 }, 1634, 2, 16, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x5320, -64, 2032, 0 }, { { -552, 0, -704, 0 }, { 632, 0, -704, 0 }, { -552, 0, 704, 0 }, { 472, 0, 704, 0 } }, { 0, 4104, 0, 0 }, { 4050, 0, 601, 0 }, 944, 2, 17, 0, 2, 0 },
    { NULL, NULL, NULL, { 7520, -64, 4704, 0 }, { { -3216, 0, -624, 0 }, { 3216, 0, -624, 0 }, { -3216, 0, 624, 0 }, { 3216, 0, 624, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 3268, 2, 11, 0, 130, 0 },
};

GpObj3A D_dryfield_junk_yard_80181518[1] = {
    { NULL, NULL, { 0x4980, -544, 1536, 0 }, { { 0, 1024, -1024, 0 }, { 0, -1024, -1024, 0 }, { 0, 1024, 1024, 0 }, { 0, -1024, 1024, 0 } }, { 4096, 0, 0, 0 }, { -88, 5 }, 129, 0 },
};

GpPointLight D_dryfield_junk_yard_80181554[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 505, -2000, 6031 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3094, 3094, 3094 }, { 0, 0 } }, 0, 7200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1931, -2000, -3662 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3094, 3094, 3094 }, { 0, 0 } }, 0, 7200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9505, 161, -2977 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3278, 3278, 3278 }, { 0, 0 } }, 0, 9103 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6211, -1300, 4996 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3278, 3278, 3278 }, { 0, 0 } }, 0, 6008 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3699, -1637, 7534 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3564, 3564, 3564 }, { 0, 0 } }, 0, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3CEF, 440, 103 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4066, 4096, 4096 }, { 0, 0 } }, 0, 5125 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x59EE, -2000, 1018 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 0, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4652, -2000, 6020 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4066, 4096, 4096 }, { 0, 0 } }, 0, 4000 },
};

DryfieldJunkYardSpotLightStorage D_dryfield_junk_yard_80181854 = {
    {
        { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -2140, 0, 3502 }, { 1222, -3850, 746 }, { 3285, 1432, 2002 } }, { -8050, -2992, 2469 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2043, 2050, 2050 }, { 0, 0 } }, { 3496, 745, 1999, 0 }, 0x783C, 0x783D, 671 },
        { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -1983, 0, -3594 }, { -1114, -3902, 614 }, { -3417, 1271, 1884 } }, { 0x7CCE, -3078, 2935 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3320, 3320, 3320 }, { 0, 0 } }, { -3586, 613, 1881, 0 }, 0x4A38, 0x55F0, 671 },
    },
    {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x08,
        0x00,
        0x00,
        0x00,
        0x74,
        0xC4,
        0x18,
        0x80,
        0x02,
        0x00,
        0x00,
        0x00,
        0x74,
        0xC7,
        0x00,
        0x00,
        0x49,
        0x00,
        0x00,
        0x10,
        0x4B,
        0x00,
        0x00,
        0x10,
        0x49,
        0x00,
        0x00,
        0x10,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x70,
        0xC8,
        0x18,
        0x80,
        0x70,
        0xC8,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x70,
        0xC8,
        0x18,
        0x80,
        0xC8,
        0x00,
        0x00,
        0x00,
        0xA8,
        0xFF,
        0x60,
        0x00,
        0xFC,
        0x02,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8E,
        0x00,
        0xC0,
        0x3F,
        0xC8,
        0x00,
        0x08,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x21,
        0x03,
        0x38,
        0xE0,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8E,
        0x00,
        0xC0,
        0x3F,
        0xC8,
        0x00,
        0x08,
        0x00,
        0xA8,
        0xFF,
        0x00,
        0x00,
        0x4B,
        0x03,
        0x38,
        0xD8,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8E,
        0x00,
        0xC0,
        0x3F,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8E,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA8,
        0xFF,
        0x38,
        0x00,
        0xE9,
        0x03,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8E,
        0x00,
        0x00,
        0x00,
        0xC8,
        0x00,
        0x08,
        0x00,
        0xA8,
        0xFF,
        0x30,
        0x00,
        0x2D,
        0x04,
        0x38,
        0xB8,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8E,
        0x00,
        0xC0,
        0x3F,
        0xC8,
        0x00,
        0x08,
        0x00,
        0xA8,
        0xFF,
        0x28,
        0x00,
        0x7B,
        0x04,
        0x38,
        0xB0,
        0x80,
        0x80,
        0x00,
        0x00,
        0x8E,
        0x00,
        0xC0,
        0x3F,
        0xC8,
        0x00,
        0x08,
        0x00,
        0xA8,
        0xFF,
        0x20,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC8,
        0x00,
        0x08,
        0x00,
        0xA8,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x1B,
        0x00,
        0x0D,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x08,
        0x00,
        0x50,
        0x00,
        0x70,
        0x00,
        0x6F,
        0x02,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x08,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xD9,
        0x02,
        0x00,
        0x38,
        0x80,
        0x80,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x00,
        0x00,
        0x50,
        0x00,
        0x50,
        0x00,
        0x05,
        0x03,
        0x00,
        0x58,
        0x80,
        0x80,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x28,
        0x00,
        0x08,
        0x00,
        0x50,
        0x00,
        0x48,
        0x00,
        0x36,
        0x03,
        0x78,
        0x70,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x28,
        0x00,
        0x08,
        0x00,
        0x50,
        0x00,
        0x40,
        0x00,
        0x6E,
        0x03,
        0x00,
        0x30,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x50,
        0x00,
        0x30,
        0x00,
        0xF7,
        0x03,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x00,
        0x00,
        0x50,
        0x00,
        0x28,
        0x00,
        0x4C,
        0x04,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x08,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB1,
        0x04,
        0x08,
        0x10,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x08,
        0x00,
        0x50,
        0x00,
        0x00,
        0x00,
        0xF0,
        0xFF,
        0x08,
        0x28,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8E,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x60,
        0xFF,
        0x60,
        0x00,
        0xBA,
        0x02,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8E,
        0x00,
        0x00,
        0x00,
        0x30,
        0x00,
        0x08,
        0x00,
        0x60,
        0xFF,
        0x58,
        0x00,
        0xC9,
        0x02,
        0x20,
        0x18,
    },
};

GpRoomCoordSet D_dryfield_junk_yard_80181BB4[1] = {
    { 0, NULL, 8, D_dryfield_junk_yard_80181554, 2, D_dryfield_junk_yard_80181854.active },
};

WorldCoordRoomAmbientEntry D_dryfield_junk_yard_80181BCC[8] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_junk_yard_80181BCC) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 400, 400, 400, 400 } },
    { .color = { 350, 350, 350, 350 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 400, 400, 400, 400 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 450, 450, 450, 450 } },
};

s32 D_dryfield_junk_yard_80181C0C[3] = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

GpRoomParamRec D_dryfield_junk_yard_80181C18[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_junk_yard_80181C20[1] = {
    { 0, 0, 1, 0, D_dryfield_junk_yard_80181C0C },
};

GpRoomParamRec* D_dryfield_junk_yard_80181C28[8] = {
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C20,
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C18,
};

/// Model task tick: reads the 2-bit game flag named by the spawn object's
/// `field_8`, clears the model's flags and sets them to 0x84 when the flag
/// reads 2 (otherwise zeroing `otOffset`), then runs the model's draw below.
void func_dryfield_junk_yard_8017D5F4(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj        = (GpItemObj8*)task->spawnArg2.pointer;
    tmd        = task->extra.tmd;
    flag       = Gp_GetCurBit2Flag(obj->field_8);
    tmd->flags = 0;
    if (flag == 2) {
        tmd->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    } else {
        tmd->otOffset = 0;
    }
    func_dryfield_junk_yard_8017D658(task);
}

/// Model draw: unless the model's flags carry bit 0x80 or it has no buffer
/// yet, refreshes its world matrix and draws a 0x1A0 by 0xC0 ground-effect
/// quad at its world position.
static void func_dryfield_junk_yard_8017D658(Task* task)
{
    DjyGroundQuadScratch* scratch;
    GfxCoord*             coord;
    TmdObject*            tmd;

    tmd   = task->extra.tmd;
    coord = tmd->coords;
    if ((tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) == 0 && tmd->buffer != 0) {
        scratch                                    = SCRATCH_STACK_CURSOR(DjyGroundQuadScratch) - 1;
        SCRATCH_STACK_CURSOR(DjyGroundQuadScratch) = scratch;
        Gp_UpdateCoord(coord);
        scratch->pos.vx = coord->workm.t[0];
        scratch->pos.vy = coord->workm.t[1];
        scratch->pos.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&scratch->pos, 0x1A0, 0xC0);
        SCRATCH_STACK_RELEASE_BLOCK(DjyGroundQuadScratch);
    }
}

/// State 0 of the room task: publishes the message table and claims game
/// pointer slot 7. With a slot-0xA task present, it sends that task its
/// opening messages while nibble 0x38 is clear, then either latches nibble
/// 0x39 and starts the `func_800E8634` sequence (once nibble 0x28 has reached
/// 2) or, on a visit whose `warp` is 2, sends it message 0x3E9. Advances the
/// state either way.
static void func_dryfield_junk_yard_8017D708(Task* arg0)
{
    arg0->msgTable = D_dryfield_junk_yard_8017DD20;
    Game_SetPtrSlot(arg0, 7);
    if (gameGetPtrSlot(0xA) != 0) {
        if (GameFlag_GetNibble(0x38) == 0) {
            Gp_DispatchMsgPtr(gameGetPtrSlot(0xA), 0x3E9, &D_dryfield_junk_yard_8017DE00, 0);
            Gp_AllyAnimId(&D_dryfield_junk_yard_8017DD88.source.index);
            Gp_DispatchMsgPtr(gameGetPtrSlot(0xA), ANIMATION_MESSAGE_PLAY, &D_dryfield_junk_yard_8017DD88, 0);
        }
        if ((GameFlag_GetNibble(0x39) == 0) && (GameFlag_GetNibble(0x28) >= 2)) {
            GameFlag_SetNibble(0x39, 1);
            func_800E8634(D_dryfield_junk_yard_8017E490, 0, D_dryfield_junk_yard_8017E658);
        } else if (gGameSession->location.loc.warp == 2) {
            Gp_DispatchMsgPtr(gameGetPtrSlot(0xA), 0x3E9, &D_dryfield_junk_yard_8017DE30, 0);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The sequence task `D_dryfield_junk_yard_8017DD48` describes, spawned by
/// the 0x13EF handler. State 0 starts a `func_800E8634` sequence and state 1
/// waits for `gGameSession->eventState` to clear. States 3, 5 and 7 send the
/// slot-0xA task a message (`0x3EE`, `0x3E8`, `0x3E8`) with a payload; states
/// 4 and 6 send the bare `0x3F0` / `0x3ED` and hold while it answers nonzero.
/// State 7, and state 2 directly, end in `taskKill`.
///
/// Every case writes its own `task->state + 1; return;`: cross jumping folds
/// those identical tails into the one increment block, and folds cases 4 and
/// 6's `Gp_DispatchMsg(..., 0, 0)` into one call.
void func_dryfield_junk_yard_8017D848(Task* task)
{
    switch (task->state) {
        case 0:
            func_800E8634(D_dryfield_junk_yard_8017DE48, 0, D_dryfield_junk_yard_8017E028);
            task->state = task->state + 1;
            return;
        case 1:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 3:
            Gp_DispatchMsgPtr(gameGetPtrSlot(0xA), 0x3EE, &D_dryfield_junk_yard_8017DE18, 0);
            task->state = task->state + 1;
            return;
        case 4:
            if (Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3F0, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 5:
            Gp_DispatchMsgPtr(gameGetPtrSlot(0xA), ANIMATION_MESSAGE_PLAY, &D_dryfield_junk_yard_8017DDD8, 0);
            task->state = task->state + 1;
            return;
        case 6:
            if (Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3ED, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 7:
            Gp_DispatchMsgPtr(gameGetPtrSlot(0xA), ANIMATION_MESSAGE_PLAY, &D_dryfield_junk_yard_8017DDEC, 0);
            /* fallthrough */
        case 2:
            taskKill(task);
            return;
    }
}

/// Handler for message 0x13F0 in the room's message table, keyed by `arg2`.
/// Point 6 plays CAP command 0xC until nibble 0x3A is set, and 6 after. Point 8
/// plays command 9 unless bit flag 0x1C is set; with it set, command 8 plays
/// only while nibble 0x73 is still clear and 0x7C is set, and otherwise the
/// point's own CAP slot starts. Always returns 0.
s32 func_dryfield_junk_yard_8017D994(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    switch (arg2) {
        case 6:
            Gp_RunCapCmd1(GameFlag_GetNibble(0x3A) <= 0 ? 0xC : 6);
            break;
        case 8:
            if (Gp_GetCurBit2Flag(0x1C) == 1) {
                if (GameFlag_GetNibble(0x73) == 0 && GameFlag_GetNibble(0x7C) != 0) {
                    Gp_RunCapCmd1(8);
                } else {
                    Gp_StartCapSlot(arg2, 1, 0);
                }
            } else {
                Gp_RunCapCmd1(9);
            }
            break;
    }
    return 0;
}

/// Handler for message 0x13F1 in the room's message table: does nothing and
/// returns 0.
s32 func_dryfield_junk_yard_8017DA44(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table. Copies the
/// incoming record to the outgoing one, then edits the copy: message 0x18
/// answers `room` 2 once nibble 0x7A has reached 4, else 1. Message 0x1B,
/// while nibble 0x38 is 1, returns 2, advancing the nibble to 2 and starting a
/// `func_800E8634` sequence; otherwise, with nibble 0x28 still clear, it
/// answers `warp` 2 and sets nibbles 0x28 and 0x4B. Returns 1.
///
/// `queryOnly` non-zero means "report only", which suppresses every side effect.
s32 func_dryfield_junk_yard_8017DA4C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->areaId == 0x18 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (GameFlag_GetNibble(0x7A) >= 4) {
            out->room = 2;
        } else {
            out->room = 1;
        }
    }
    if (in->areaId == 0x1B) {
        if (GameFlag_GetNibble(0x38) == 1) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                GameFlag_SetNibble(0x38, 2);
                func_800E8634(D_dryfield_junk_yard_8017E3D0, 0, D_dryfield_junk_yard_8017E2B0);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x28) == 0 && in->queryOnly == ROOM_EVENT_EXECUTE) {
            out->warp = 2;
            GameFlag_SetNibble(0x28, 1);
            GameFlag_SetNibble(0x4B, 4);
        }
    }
    return 1;
}

/// Handler for message 0x13EF in the room's message table. When the record's
/// `actionId` is 1 and nibble 0x38 is clear, it latches the nibble to 1 and
/// spawns the sequence task. When it is 2, the slot-0xA task stands at x
/// 0x5209 or beyond and nibble 0x38 is 1, it advances the nibble to 2 and
/// starts a `func_800E8634` sequence. Always returns 0.
s32 func_dryfield_junk_yard_8017DB78(Task* task, s32 msgId, DirectionActionRequest* msg, TaskMessageArg arg3)
{
    Task* player;

    if (msg->actionId == 1) {
        if (GameFlag_GetNibble(0x38) == 0) {
            GameFlag_SetNibble(0x38, 1);
            Task_SpawnFromTable(D_dryfield_junk_yard_8017DD48, 0, 0, 0);
        }
    }
    if (msg->actionId == 2) {
        player = gameGetPtrSlot(0xA);
        if ((player != NULL) && (player->extra.tmd->coords->coord.t[0] >= 0x5209) &&
            (GameFlag_GetNibble(0x38) == 1)) {
            GameFlag_SetNibble(0x38, 2);
            func_800E8634(D_dryfield_junk_yard_8017E160, 0, D_dryfield_junk_yard_8017E2B0);
        }
    }
    return 0;
}

/// Room script callback, named by two of the room's script records (command
/// 0xD, argument 5): stores its argument into `Mc_SaveData[0].state.sceneEvent`.
void func_dryfield_junk_yard_8017DC54(s8 arg0)
{
    Mc_SaveData[0].state.sceneEvent = arg0;
}

/// State 1 of the room task: once `gDisplayState.debugMode` is non-zero and a slot-0xA
/// task exists, calls `func_80724608` on that task with the `"DOG"` name. The
/// state never advances, so it repeats every frame.
static void func_dryfield_junk_yard_8017DC60(Task* task)
{
    if ((gDisplayState.debugMode != 0) && (gameGetPtrSlot(0xA) != 0)) {
        func_80724608(gameGetPtrSlot(0xA), -0x8C, 0xA, D_dryfield_junk_yard_8017D5D0);
    }
}

/// The room task: copies its three-state table to the stack and runs the
/// entry the task's state selects.
void func_dryfield_junk_yard_8017DCB4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_junk_yard_8017D5C4;
    sp.funcs[task->state](task);
}

/// Sets the room effect mode to 2.
void func_dryfield_junk_yard_8017DD0C(Task* unused)
{
    Gp_State1C->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}
