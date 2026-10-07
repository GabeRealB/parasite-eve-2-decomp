#include "rooms/shelter_b2_main_corridor.h"

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
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
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
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

/// Empty presence flag so `water_effects.h` declares the shared
/// `_waterDrawSpinU16` and `_waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/water_effects.h"
// The latched-event symbol carries four unproven bytes after the event.
#define ROOM_EVENT_LATCHED gRoomEventLatched.event
// The departure symbol carries four unproven bytes after the record.
#define ROOM_DEPARTURE gRoomDeparture.departure
#include "../../shared/room_events.h"
#include "../../shared/room_variants.h"

static s32 _roomVariantResolveNeoArk(RoomEventMsg* request, RoomEventMsg* reply);

/// Storage of the departure task's descriptor, with the four bytes before it.
///
/// `desc` is the spawn recipe the room's exit task hands to the task system
/// once it has staged `gRoomDeparture`: a bodyless task running
/// `roomDepartureTask`. It is addressed on its own, as a single descriptor.
///
/// The four bytes before it open the room's initialised data. They are zero
/// in the image and have no established access. They are not alignment: the
/// last function ends on the boundary they start at, and the descriptor needs
/// only word alignment. The only other room that carries
/// `_roomVariantResolveNeoArk` has the same four zero bytes directly before
/// its own departure descriptor, and no other room with a departure
/// descriptor has them, so they are likely an unreferenced variable of that
/// shared source rather than part of the descriptor. Their role is unproven;
/// they stay in this allocation only to keep the descriptor at its address.
typedef struct {
    u8       unknown_0[4]; // Zero in the image; no access established and role unproven
    TaskDesc desc;         // Spawn recipe for the departure task; read as a single descriptor
} _ShelterB2MainCorridorDepartureTaskDescStorage;
STATIC_ASSERT_SIZEOF(_ShelterB2MainCorridorDepartureTaskDescStorage, 16);
extern _ShelterB2MainCorridorDepartureTaskDescStorage D_shelter_b2_main_corridor_801828E0;

/// Departure record plus four trailing bytes.
///
/// `departure` is what the departure task reads. The trailing bytes stay
/// zero; nothing reads or writes them, and their role is unproven.
typedef struct {
    RoomDeparture departure;  // Departure the handler staged for the task
    u8            unknown[4]; // Role unproven; zero, with no recovered access
} _RoomDepartureStorage;
STATIC_ASSERT_SIZEOF(_RoomDepartureStorage, 0x10);

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b2_main_corridor_8018965C[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u8 D_shelter_b2_main_corridor_8018965C_value __asm__("D_shelter_b2_main_corridor_8018965C");

/// Spawn argument of the helper task 0x31 the room's exit task starts.
extern RoomFadeStorage gRoomEventFade;

/// The outgoing message of the exit being taken; the exit task copies its
/// destination into the save location.
extern RoomEventMsg gRoomEventStagedMsg;

/// Cleared whenever the handler considers an exit, set once the exit task has
/// been spawned. Nothing else in the room reads it.

/// A second copy of the staged event block, taken whole once the block has been
/// passed through `_roomVariantResolveNeoArk`.
extern _RoomDepartureStorage gRoomDeparture;

/// The exit being taken, read by the exit task.
extern RoomLatchedEventStorage gRoomEventLatched;

/// The staged event block, read by the task spawned from
/// `D_shelter_b2_main_corridor_80182C44`.
extern RoomDeparture D_shelter_b2_main_corridor_80189684;

/// Descriptor of the task spawned after the staged block has been copied.

/// Descriptor of the task that carries out a staged exit.
extern TaskDesc D_shelter_b2_main_corridor_80182C08;

/// The room's message table, installed by its first task state.
extern TaskMessageEntry D_shelter_b2_main_corridor_80182C14[];

/// Descriptor of the tasks the room's message handler spawns.
extern TaskDesc D_shelter_b2_main_corridor_80182C44[];

/// Passed by address to `evsStartScript` when the room's one-shot flag event
/// fires; its contents are not read here.
extern EvsCommand D_shelter_b2_main_corridor_80182CA8[];

/// Tasks the room's first task state spawns.
extern TaskDesc D_shelter_b2_main_corridor_80182DE0[];

/// Light positions the per-view drawer places beams and glows at.
extern SVECTOR D_shelter_b2_main_corridor_80182F7C[];
extern SVECTOR D_shelter_b2_main_corridor_80182F9C[];
extern SVECTOR D_shelter_b2_main_corridor_80182FAC[];
extern SVECTOR D_shelter_b2_main_corridor_80182FBC[];
extern SVECTOR D_shelter_b2_main_corridor_80182FCC[];
extern SVECTOR D_shelter_b2_main_corridor_8018305C[];
extern SVECTOR D_shelter_b2_main_corridor_8018306C[];

/// The two points, relative to the effect's parent coordinate, that the trail
/// effect's two edges follow. The second is also read by its own name.

/// Areas the room re-applies when it clears its pending game-flag state.
extern AreaApplyRec D_shelter_b2_main_corridor_80189644[];

static void func_shelter_b2_main_corridor_8017E264(RoomEventMsg* msg);
static void _shelterB2MainCorridorInitializeRoomTask(Task* task);
static void _shelterB2MainCorridorIdleRoomTask(Task* task);
static void _shelterB2MainCorridorInitializeWaterTask(Task* task);

extern TaskDesc D_actor_100400_80147E48;

s32         func_shelter_b2_main_corridor_8017D9C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32         func_shelter_b2_main_corridor_8017DC88(Task* task, s32 msgId, const void* firstArg, s32);
static s32  _shelterB2MainCorridorRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _shelterB2MainCorridorIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _shelterB2MainCorridorHandleSoundMessage(Task* task, s32 messageId, s32 soundCommandId, s32 unusedArg);
void        func_shelter_b2_main_corridor_8017DEB0(Task*);
void        func_shelter_b2_main_corridor_8017E210(Task*);
static void _shelterB2MainCorridorWaterTask(Task* task);

enum { SHELTER_B2_MAIN_CORRIDOR_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Fixed beam/flare perspective scales and packed beam RGB factors for this room.
enum {
    SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE    = 0x200,
    SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE     = 0x111,
    SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_GREEN     = 0x10,
    SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_RED       = 0x100,
    SHELTER_B2_MAIN_CORRIDOR_FLARE_TEXTURE_COLUMN = 1,
    SHELTER_B2_MAIN_CORRIDOR_FLARE_RADIUS_SCALE   = 0x300,
};

_ShelterB2MainCorridorDepartureTaskDescStorage D_shelter_b2_main_corridor_801828E0 = { { 0 }, { { { TASK_BODY_NONE, 32 } }, roomDepartureTask, { .value = 0 } } };

static AnimationPackedPose _gShelterB2MainCorridorAnimation05620Bank1[2] = {
#include "assets/shelter_b2_main_corridor_animation_05620_bank1.inc"
};

static AnimationPackedRotation _gShelterB2MainCorridorAnimation05620Bank4[62] = {
#include "assets/shelter_b2_main_corridor_animation_05620_bank4.inc"
};

static AnimationRecord _gShelterB2MainCorridorAnimation05620Records[110] = {
#include "assets/shelter_b2_main_corridor_animation_05620_records.inc"
};

static u16 _gShelterB2MainCorridorAnimation05620Indices[20] = {
#include "assets/shelter_b2_main_corridor_animation_05620_indices.inc"
};

static AnimationSet _gShelterB2MainCorridorAnimation05620 = {
    _gShelterB2MainCorridorAnimation05620Records,
    _gShelterB2MainCorridorAnimation05620Indices,
    { NULL, _gShelterB2MainCorridorAnimation05620Bank1, NULL, NULL, _gShelterB2MainCorridorAnimation05620Bank4, NULL, NULL, NULL },
};

TaskDesc D_shelter_b2_main_corridor_80182C08 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b2_main_corridor_80182C14[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_main_corridor_8017D9C4 },
    { SHELTER_B2_MAIN_CORRIDOR_MESSAGE_USE_KEY_ITEM, _shelterB2MainCorridorRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b2_main_corridor_8017DC88 },
    { ROOM_MESSAGE_COMMAND, _shelterB2MainCorridorIgnoreCommandMessage },
    { ROOM_MESSAGE_SOUND, _shelterB2MainCorridorHandleSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b2_main_corridor_80182C44[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b2_main_corridor_8017DEB0, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b2_main_corridor_8017E210, { .value = 0 } },
};

AnimationSet* D_shelter_b2_main_corridor_80182C5C[1] = {
    &_gShelterB2MainCorridorAnimation05620,
};

AnimationBankCopyRequest D_shelter_b2_main_corridor_80182C60 = { { .sets = D_shelter_b2_main_corridor_80182C5C }, ARRAY_SIZE(D_shelter_b2_main_corridor_80182C5C) };

AnimationPlayRequest D_shelter_b2_main_corridor_80182C68 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b2_main_corridor_80182C7C = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_shelter_b2_main_corridor_80182C90 = { { 0, 0, 0, 0 }, { 0, 1024, 0, 0 } };

EvsCommand D_shelter_b2_main_corridor_80182CA8[13] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b2_main_corridor_80182C60 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b2_main_corridor_80182C68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_shelter_b2_main_corridor_80182C90 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b2_main_corridor_80182C7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_shelter_b2_main_corridor_80182DE0[1] = {
    { { { TASK_BODY_NONE, 96 } }, _shelterB2MainCorridorWaterTask, { .value = 0 } },
};

/// Four water rectangles rendered as Z-running wave strips, followed by the list end.
static RoomWaterSurface _gShelterB2MainCorridorWaterWaveSurfaces[] = {
    { -2400, -0x3C8C, 1600, 4400, 0 },
    { 900, -0x3C8C, 1500, 4500, 0 },
    { -2400, -8800, 1600, 5600, 0 },
    { 900, -8800, 1500, 5600, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

s16 gShelterB2MainCorridorWaterY = 150;

SVECTOR D_shelter_b2_main_corridor_80182E2C[8] = {
    { 1700, 1500, -4300, 0 },
    { -1700, 1500, -4300, 0 },
    { 1700, 1500, -6700, 0 },
    { -1700, 1500, -6700, 0 },
    { 1700, 1500, -7400, 0 },
    { -1700, 1500, -7400, 0 },
    { -1700, 1500, -4300, 0 },
    { 1700, 1500, -7400, 0 },
};

SVECTOR D_shelter_b2_main_corridor_80182E6C[8] = {
    { 1700, 3000, -0x34BC, 0 },
    { -1700, 3000, -0x34BC, 0 },
    { 1700, 3000, -7900, 0 },
    { -1700, 3000, -7900, 0 },
    { 1700, 3000, -0x300C, 0 },
    { -1700, 3000, -0x300C, 0 },
    { 1700, 3000, -7900, 0 },
    { -1700, 3000, -0x34BC, 0 },
};

SVECTOR D_shelter_b2_main_corridor_80182EAC[8] = {
    { -1700, 2200, -6200, 0 },
    { 1700, 2200, -6200, 0 },
    { -1700, 2200, -4800, 0 },
    { 1700, 2200, -4800, 0 },
    { -1700, 2200, -0x3200, 0 },
    { 1700, 2200, -0x3200, 0 },
    { 1700, 2200, -4800, 0 },
    { -1700, 2200, -0x3200, 0 },
};

SVECTOR* D_shelter_b2_main_corridor_80182EEC[4] = {
    D_shelter_b2_main_corridor_80182E2C,
    D_shelter_b2_main_corridor_80182E6C,
    D_shelter_b2_main_corridor_80182EAC,
    D_shelter_b2_main_corridor_80182E6C,
};

SVECTOR D_shelter_b2_main_corridor_80182EFC[12] = {
    { 0, 0, 0, 0 },
    { -1700, 0, -3900, 0 },
    { 1700, 0, -3900, 0 },
    { -1700, 0, -6000, 0 },
    { 1700, 0, -6000, 0 },
    { -1700, 0, -8000, 0 },
    { 1700, 0, -8000, 0 },
    { -1700, 0, -0x2FA8, 0 },
    { 1700, 0, -0x2FA8, 0 },
    { -1700, 0, -0x38A4, 0 },
    { 1700, 0, -0x38A4, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_shelter_b2_main_corridor_80182F5C[4] = {
    { -667, -1910, -0x4A3D, 0 },
    { -667, -1910, -0x45ED, 0 },
    { -667, -1910, -0x4309, 0 },
    { -667, -1910, -0x3EB4, 0 },
};

SVECTOR D_shelter_b2_main_corridor_80182F7C[4] = {
    { 668, -1910, -0x4A3D, 0 },
    { 668, -1910, -0x45ED, 0 },
    { 668, -1910, -0x4309, 0 },
    { 668, -1910, -0x3EB4, 0 },
};

SVECTOR D_shelter_b2_main_corridor_80182F9C[2] = {
    { -3246, -2265, -0x2988, 0 },
    { -3246, -2265, -9370, 0 },
};

SVECTOR D_shelter_b2_main_corridor_80182FAC[2] = {
    { -3246, -2265, -2620, 0 },
    { -3246, -2265, -1381, 0 },
};

SVECTOR D_shelter_b2_main_corridor_80182FBC[2] = {
    { 3260, -2265, -0x2988, 0 },
    { 3260, -2265, -9370, 0 },
};

SVECTOR D_shelter_b2_main_corridor_80182FCC[18] = {
    { 3260, -2265, -2620, 0 },
    { 3260, -2265, -1381, 0 },
    { -365, -2226, -180, 0 },
    { 365, -2226, -180, 0 },
    { -1293, -4768, -0x34A6, 0 },
    { -1293, -4768, -0x2CE0, 0 },
    { -1293, -4768, -9483, 0 },
    { -1293, -4768, -7483, 0 },
    { -1293, -4768, -5483, 0 },
    { -1293, -4768, -3483, 0 },
    { -1293, -4768, -1483, 0 },
    { 1293, -4768, -0x34A6, 0 },
    { 1293, -4768, -0x2CE0, 0 },
    { 1293, -4768, -9483, 0 },
    { 1293, -4768, -7483, 0 },
    { 1293, -4768, -5483, 0 },
    { 1293, -4768, -3483, 0 },
    { 1293, -4768, -1483, 0 },
};

SVECTOR D_shelter_b2_main_corridor_8018305C[2] = {
    { -4243, -45, -0x2816, 0 },
    { -4243, -45, -9767, 0 },
};

SVECTOR D_shelter_b2_main_corridor_8018306C[10] = {
    { -4243, -45, -2278, 0 },
    { -4243, -45, -1791, 0 },
    { -4816, -45, -9098, 0 },
    { -5308, -45, -9098, 0 },
    { -4816, -45, -1115, 0 },
    { -5308, -45, -1115, 0 },
    { -5847, -45, -0x2816, 0 },
    { -5847, -45, -9767, 0 },
    { -5847, -45, -2278, 0 },
    { -5847, -45, -1791, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

u8* D_shelter_b2_main_corridor_801830CC[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b2_main_corridor_801830D0[1] = { 13 };

DirectionWarpEntry D_shelter_b2_main_corridor_801830D4[6] = {
    { { { .word = 0 }, -67, -6, -0x4909 }, { 0, 0, 0, 0 }, { { .word = 0 }, -67, -6, -0x4909 }, { 0, 0, 0, 0 }, 0x54210002, 0x54210001, 0x5421000B, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1C7 },
    { { { .word = 1024 }, -4600, 0, -9900 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -4600, 0, -9900 }, { 0, 0, 0, 0 }, 0x5421000A, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 10, DIRECTION_WARP_FLAG_FADE_DEPARTURE, 431 },
    { { { .word = 3072 }, 3448, 0, -9932 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 3448, 0, -9932 }, { 0, 0, 0, 0 }, 0x54210002, 0x54210001, 0x5421000B, 5, DIRECTION_WARP_FLAG_NONE, 451 },
    { { { .word = 1024 }, -4900, 0, -1900 }, { 0, 0, 0, 0 }, { { .word = 1536 }, -5450, 0, -1400 }, { 0, 0, 0, 0 }, 0x5421000A, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 11, DIRECTION_WARP_FLAG_FADE_DEPARTURE, GAME_FLAG_MAP_MARK_B2_MAIN_CORRIDOR },
    { { { .word = 3072 }, 3467, 0, -1983 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 3467, 0, -1983 }, { 0, 0, 0, 0 }, 0x54210002, 0x54210001, DIRECTION_WARP_SOUND_NONE, 9, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, 31, 0, 72 }, { 0, 0, 0, 0 }, { { .word = 2304 }, -1000, 0, -1400 }, { 0, 0, 0, 0 }, 0x54210002, 0x54210001, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB2MainCorridorCollision06E80Normals[50] = {
#include "assets/shelter_b2_main_corridor_collision_06E80_normals.inc"
};

static SVECTOR _gShelterB2MainCorridorCollision06E80Verts[220] = {
#include "assets/shelter_b2_main_corridor_collision_06E80_verts.inc"
};

static WorldCollisionGridFace _gShelterB2MainCorridorCollision06E80Faces[104] = {
#include "assets/shelter_b2_main_corridor_collision_06E80_faces.inc"
};

static s16 _gShelterB2MainCorridorCollision06E80Cells[550] = {
#include "assets/shelter_b2_main_corridor_collision_06E80_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB2MainCorridorCollision06E80Cells[i])
static s16* _gShelterB2MainCorridorCollision06E80Table[32] = {
#include "assets/shelter_b2_main_corridor_collision_06E80_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b2_main_corridor_80184440 = { NULL, _gShelterB2MainCorridorCollision06E80Normals, _gShelterB2MainCorridorCollision06E80Verts, _gShelterB2MainCorridorCollision06E80Faces, _gShelterB2MainCorridorCollision06E80Table, 6972, 0x5032, 4, 8, 4000, 104 };

ViewCamera D_shelter_b2_main_corridor_80184464[13] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5EE9, 8000 } }, 230 },
    { { { { -3955, 0, -1064 }, { -138, 4061, 515 }, { 1055, 533, -3921 } }, { 821, 1576, 0x2726 } }, 275 },
    { { { { 3997, 0, -892 }, { -135, 4048, -608 }, { 881, 623, 3951 } }, { 821, 1576, 0x3C7F } }, 275 },
    { { { { 518, 0, 4062 }, { 964, 3979, -123 }, { -3946, 971, 504 } }, { -912, 1817, 0x28C1 } }, 246 },
    { { { { 1259, 0, -3897 }, { -722, 4024, -233 }, { 3830, 759, 1237 } }, { 1358, 1664, 0x2A60 } }, 275 },
    { { { { 3997, 0, -892 }, { -69, 4083, -309 }, { 889, 317, 3985 } }, { 821, 1489, 0x27DC } }, 275 },
    { { { { 4048, 0, -620 }, { -26, 4092, -175 }, { 619, 177, 4044 } }, { 821, 1379, 6524 } }, 275 },
    { { { { 671, 0, 4040 }, { 186, 4091, -30 }, { -4036, 189, 670 } }, { -1315, 1379, 2369 } }, 275 },
    { { { { -96, 0, -4094 }, { -364, 4079, 8 }, { 4078, 364, -96 } }, { 1207, 1533, 2043 } }, 275 },
    { { { { -3158, 0, 2608 }, { 2332, 1832, 2824 }, { -1167, 3663, -1413 } }, { 4382, 2885, 9189 } }, 225 },
    { { { { -3405, 0, -2276 }, { -2030, 1851, 3037 }, { 1028, 3653, -1539 } }, { 5906, 2948, 1065 } }, 240 },
    { { { { -4001, 0, 876 }, { 445, 3528, 2031 }, { -755, 2079, -3447 } }, { 4757, 1640, 9448 } }, 257 },
    { { { { -4093, 0, 153 }, { 78, 3517, 2096 }, { -131, 2098, -3515 } }, { 4965, 1600, 1422 } }, 257 },
};

SpriteBatch D_shelter_b2_main_corridor_80184638[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_main_corridor_80184648[104] = {
    { 143, 0x3FC0, { .fields = { 96, 144 } }, -24, -88, 0x3A63, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 72, -88, 1297, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, -24, 1312, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, -16, 1317, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, -8, 1321, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, 0, 1326, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, 8, 1331, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, 16, 1335, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, 24, 1340, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, 32, 1345, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, 40, 1349, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, 48, 1353, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, 112, 973, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -88, 112, 973, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -88, 104, 1029, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -88, 40, 0, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 40, 0, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, 40, 0, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, 40, 0, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 40, 0, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 40, 0, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 40, 0, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, 40, 0, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, 96, 1057, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, 88, 1107, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 48, 80, 1163, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, 104, 1013, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 48, 72, 1228, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 48, 64, 1302, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 48, 56, 1388, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 48, 1474, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 96, 1073, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -64, 88, 1123, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -56, 80, 1179, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, 72, 1212, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 1286, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -32, 56, 1372, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -24, 48, 1506, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -24, 40, 1436, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -64, 112, 957, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -56, 104, 1013, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -48, 96, 1057, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -40, 88, 1123, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -32, 80, 1179, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -24, 72, 1244, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -16, 64, 1302, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, 56, 1420, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 0, 48, 1506, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, 40, 1596, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, 48, 1474, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 24, 56, 1404, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 24, 64, 1300, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 72, 1244, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 8, 80, 1179, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 8, 88, 1123, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 96, 1041, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, 104, 1013, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 112, 973, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -120, 112, 667, { .fields = { 120, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -120, 104, 696, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -120, 96, 736, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -120, 88, 770, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -120, 80, 798, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -120, 72, 855, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -120, 64, 919, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -120, 56, 993, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -120, 48, 1097, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -120, 40, 1186, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -120, 32, 1313, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 8 } }, -120, 24, 1470, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 136, 8 } }, -120, 16, 1669, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 144, 8 } }, -120, 8, 1930, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 152, 8 } }, -120, 0, 2269, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 112, 659, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 104, 696, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 96, 736, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 88, 782, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 80, 834, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 72, 893, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 64, 961, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 56, 56, 1041, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 56, 48, 1135, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 56, 40, 1253, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 32, 1385, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 24, 1555, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 56, 16, 1779, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 56, 8, 2065, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 56, 0, 2308, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -80, 112, 3236, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -72, 104, 3273, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -64, 96, 3319, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -56, 88, 3357, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 80, 3407, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 40 } }, -24, 80, 1250, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -40, 72, 3498, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -32, 64, 3587, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -24, 56, 3681, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -16, 48, 3793, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -8, 40, 3926, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 0, 32, 4093, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, 24, 1807, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 16, 16, 2075, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 8, 2243, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 0, 0, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_main_corridor_80184E68[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 1, 0 } },
    { 12, 46, 0, 0, { 2, 0 } },
    { 58, 46, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_main_corridor_80184E90[143] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 8, 4245, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 16, 1676, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 16, 1668, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 24, 1530, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 32, 1388, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 32, 1388, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -64, -8, 2899, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -64, 0, 2419, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -64, 8, 2079, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -64, 16, 1825, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 120, 16, 1812, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 64, 16, 1825, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 8, 16, 1825, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 96, 24, 1628, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 32, 24, 1628, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -24, 24, 1628, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -72, 24, 1628, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -120, 24, 1660, { .fields = { 104, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 24, 1607, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -160, 32, 1504, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -104, 32, 1504, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -48, 32, 1472, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 8, 32, 1472, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 112, 32, 1527, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 64, 32, 1472, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 0, 40, 1344, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -48, 40, 1344, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -104, 40, 1376, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -160, 40, 1376, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 48, 1176, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 48, 1190, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 48, 1167, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 48, 1237, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 48, 1237, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -8, 56, 1147, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 56, 1147, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -56, 64, 1070, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 0, 64, 1070, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 8, 72, 1004, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -56, 72, 1004, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -56, 80, 945, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 8, 80, 945, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 16, 88, 894, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -56, 88, 878, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -56, 96, 849, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 16, 96, 849, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 16, 104, 808, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -56, 104, 808, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -56, 112, 771, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, 112, 771, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 24, 112, 771, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 64, 3349, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -144, 112, 1210, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 104, 1204, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 96, 1204, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 96, 1150, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 88, 1114, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 80, 1155, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 72, 1176, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 72, 1160, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, -8, 2640, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 0, 2198, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 8, 1885, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 16, 1699, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -88, 24, 1625, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, 16, 1636, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -120, 24, 1603, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -136, 16, 1611, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 24, 1563, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 16, 1563, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 48, 1080, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 56, 1072, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 64, 1543, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 72, 1534, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -160, 80, 1469, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -120, 56, 1054, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -120, 48, 1082, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -88, 48, 1096, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, 56, 1125, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 56, 981, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 112, 635, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 104, 717, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 96, 754, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 88, 796, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 80, 844, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 72, 881, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 64, 942, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 64, 2038, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -136, 64, 1774, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -136, 72, 1686, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -8, 2639, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -24, 0, 2315, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 8, 1888, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 16, 1760, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, 8, 1821, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 24, 16, 1793, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 48, 8, 1853, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 64, 16, 1850, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, 8, 1900, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 16, 1911, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 24, 1274, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 120, 56, 1442, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 104, 48, 1524, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 104, 40, 1335, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 104, 32, 1298, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 32, 1263, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 40, 1277, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 32, 1264, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 32, 40, 1238, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 48, 1112, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 64, 48, 1787, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 72, 56, 1660, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 56, 1029, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 64, 959, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 72, 902, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 64, 80, 844, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 72, 88, 796, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 96, 756, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 88, 104, 721, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 96, 112, 691, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 40, 1183, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 72, 1955, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 144, -8, 1278, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 152, -32, 1260, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 144, 0, 1284, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 136, 24, 1295, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 144, 24, 1226, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 40, -56, 2903, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 24, -56, 3102, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 16, -32, 3197, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 24, -40, 3126, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 24, -16, 3162, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 32, -40, 3071, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 32, -16, 2896, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -112, -16, 2866, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -128, -16, 2820, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -128, -40, 2786, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -136, -64, 2737, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -144, -16, 2436, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -144, -40, 2704, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -160, -16, 2072, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -160, -40, 2237, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -160, -64, 2413, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_main_corridor_801859BC[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 4, 0 } },
    { 6, 45, 0, 0, { 1, 0 } },
    { 51, 9, 0, 0, { 3, 0 } },
    { 60, 62, 0, 0, { 2, 0 } },
    { 122, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_main_corridor_801859F4[40] = {
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 16, 1168, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 24, 1055, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 32, 994, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 40, 941, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 48, 893, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 56, 728, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 64, 751, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 72, 708, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 80, 710, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 88, 688, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 96, 574, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 104, 538, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 24, 112, 550, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -104, 24, 1085, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -104, 32, 1027, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -104, 40, 974, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -104, 48, 917, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -104, 56, 889, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -104, 64, 782, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -104, 72, 682, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -104, 80, 672, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -104, 88, 598, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -104, 96, 581, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -104, 104, 550, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -104, 112, 534, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 104 } }, -32, 16, 0, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -104, 16, 0xFFE0, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 112, 683, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 104, 730, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 96, 764, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 88, 801, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 80, 843, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 72, 890, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 64, 942, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 56, 1001, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 48, 1069, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 40, 1147, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 32, 1238, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 24, 1327, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 8 } }, -88, 16, 1363, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_main_corridor_80185D14[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 1, 0 } },
    { 27, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_main_corridor_80185D34[141] = {
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 112, 623, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 104, 655, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 96, 690, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 88, 729, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 80, 773, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, 72, 822, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 64, 878, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 56, 942, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 48, 1015, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 40, 1100, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 32, 1201, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 24, 0xFFF0, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 16, 0xFFF0, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -160, 104, 703, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -160, 96, 698, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 88, 713, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 80, 741, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 80, 758, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 88, 758, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -104, 96, 748, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -96, 64, 862, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -96, 72, 858, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 64, 0, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -104, 112, 0, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 16, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 56, 0, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, 56, 0, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, 16, 0, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, 16, 0, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 16, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, 56, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 56, 0, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 40, 96, 0, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 96, 0, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 16, 1506, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -64, 16, 0, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 16, 0, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 24, 1385, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 24, 0, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 24, 0, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 80, 0, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 48, 0, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 64, 0, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 16, 0, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 32, 0, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 16, 0, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 32, 0, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 48, 0, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 64, 0, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -8, 96, 0, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, 96, 0, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -8, 56, 0, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 80, 0, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -64, 96, 0, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, 80, 0, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -8, 16, 0, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 40, 0, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 96, 0, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 96, 0, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -160, 112, 0, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 112, 709, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, 104, 0, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 104, 736, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -56, 40, 1151, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -96, 40, 0, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -96, 32, 0, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 32, 1230, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -64, 48, 999, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 48, 0, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -96, 48, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -32, 56, 0, { .fields = { 120, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -72, 56, 989, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 56, 0, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -160, 112, 660, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -160, 104, 770, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 168, 8 } }, -64, 56, 1229, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 160, 8 } }, -56, 48, 1308, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -104, 104, 599, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -32, 104, 903, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 32, 104, 903, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 32, 112, 868, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -32, 112, 884, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -104, 112, 750, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -24, 96, 941, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 40, 96, 941, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -24, 88, 983, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 40, 88, 983, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -24, 80, 1047, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 48, 80, 1047, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -16, 72, 1100, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 48, 72, 1100, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -16, 64, 1160, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 48, 64, 1160, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -64, 24, 0, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 40, 0, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -64, 32, 0, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 40, 0, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 64, 0, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 64, 0, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 72, 0, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, 96, 685, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, 88, 716, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -80, 72, 1080, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 64, 1133, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 80, 1074, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 88, 897, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, 88, 1050, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 96, 669, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, 96, 877, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 48, 0, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 64, 0, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 80, 0, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 40, 0, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, 40, 0, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -128, 24, 0, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -128, 48, 0, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 24, 0, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 24, 0, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -128, 64, 0, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -128, 80, 0, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 80, 1063, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 80, 1063, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 80, 788, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, 72, 1095, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, 72, 1082, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, 64, 1135, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, 64, 1148, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -24, 24, 1513, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, 24, 1460, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 64, 24, 1410, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -32, 32, 1398, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 16, 32, 1398, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 56, 32, 1398, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -160, 96, 750, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -144, 96, 0, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -40, 40, 1305, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 8, 40, 1305, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 48, 40, 1305, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 88, 40, 1305, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, 24, 0, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 24, 0, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_main_corridor_80186838[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 73, 0, 0, { 1, 0 } },
    { 73, 68, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_main_corridor_80186858[101] = {
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 112, 714, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 104, 756, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 96, 804, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 88, 858, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 80, 920, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 72, 993, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 64, 1078, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 56, 1180, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 48, 1305, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 40, 1461, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 32, 1663, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 24, 1847, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 40, 1478, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 32, 1656, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 112, 730, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 72, 104, 766, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 64, 96, 807, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 56, 88, 858, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 80, 928, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 72, 994, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 24, 64, 1097, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, 56, 1233, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 48, 1345, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 48, 112, 810, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -8, 112, 810, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 112, 810, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 104, 856, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 104, 856, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 40, 104, 856, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 32, 96, 908, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 96, 908, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 96, 908, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 88, 967, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 88, 967, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 24, 88, 967, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 80, 1035, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 80, 1035, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 80, 1035, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 72, 1115, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 72, 1115, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 8, 72, 1115, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 64, 1208, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 64, 1208, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 64, 1208, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 56, 1320, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 56, 1320, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 56, 1320, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 48, 1456, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 48, 1456, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 48, 1456, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 40, 1625, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 40, 1625, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 40, 1625, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 32, 1781, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 32, 1840, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 32, 1746, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, 64, 3326, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 88, 1083, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 96, 1097, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 104, 1103, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 112, 1090, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, 32, 3648, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 48, 1670, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 56, 1591, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 64, 1585, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 72, 1577, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 72, 1636, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 80, 1562, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 80, 1659, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, 24, 1916, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 24, 1946, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 24, 1898, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, 32, 2900, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 32, 2339, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 32, 1903, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, 40, 2812, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 40, 2288, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 40, 1889, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 32, 1669, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 40, 1953, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 48, 1872, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 32, 1750, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 32, 1709, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 40, 2576, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 40, 2654, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 48, 2381, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 48, 2221, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -160, 0, 1588, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -152, 32, 1612, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 64, 24, 1943, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 72, 0, 1903, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 80, 24, 1875, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 80, -24, 1856, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 80, -48, 1906, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 88, 0, 1877, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, 104, 24, 1640, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 112, 0, 1667, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 96, -24, 1870, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 96, -48, 1856, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 112, -48, 1741, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 112, -24, 1713, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_main_corridor_8018703C[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 1, 0 } },
    { 23, 33, 0, 0, { 4, 0 } },
    { 56, 5, 0, 0, { 3, 0 } },
    { 61, 8, 0, 0, { 5, 0 } },
    { 69, 18, 0, 0, { 2, 0 } },
    { 87, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_main_corridor_8018707C[68] = {
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -160, 104, 16, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -160, 96, 868, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -160, 88, 938, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, 80, 1114, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -160, 80, 1130, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -40, 88, 990, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 80, 88, 928, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 72, 80, 1005, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -40, 80, 1066, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 80, 72, 1171, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -160, 56, 1409, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 168, 8 } }, -40, 56, 1393, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 56, 1393, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 8 } }, 32, 48, 4040, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -160, 64, 1278, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -40, 64, 1262, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, 64, 64, 1262, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 144, 8 } }, -40, 96, 894, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 152, 8 } }, -40, 104, 870, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -40, 48, 4776, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 120, 8 } }, -64, 40, 4368, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 120, 8 } }, -64, 32, 3087, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -48, 112, 820, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 32, 112, 820, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -16, 72, 1139, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -104, 72, 1187, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -160, 72, 1171, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, 56, 32, 0, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 104, 96, 0, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 112, 104, 0, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -160, 48, 0, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 16 } }, -160, 32, 0, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -160, 112, 0, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 64, 112, 756, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 64, 104, 806, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 96, 878, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 64, 88, 985, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, 64, 80, 939, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 112, 705, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 104, 751, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 96, 805, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 88, 851, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 80, 889, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 88, 842, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 80, 0, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 96, 847, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 104, 848, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 112, 1422, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 32, 0x3388, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 32, 0x3455, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -80, 32, 0x356B, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 8, 32, 0x3D1F, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 48, 32, 0x393C, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 88, 32, 0x3A9A, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 32, 0x3BE4, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 64, 72, 976, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 112, 72, 949, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 56 } }, -32, 64, 0, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -32, 32, 0, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, 64, 64, 0, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 8 } }, -160, 72, 0, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 120, 96, 1587, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 96, 727, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 104, 88, 1079, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 104, 1320, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 104, 670, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 104, 112, 704, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 112, 0, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_main_corridor_801875CC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 1, 0 } },
    { 32, 35, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_main_corridor_801875EC[82] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 112, 0, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 56, 96, 733, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 48, 88, 789, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 40, 80, 856, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 72, 936, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 24, 64, 1036, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 56, 1147, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -128, 112, 624, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -128, 104, 636, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -128, 96, 682, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -128, 88, 756, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -120, 80, 860, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -112, 72, 939, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 8, 48, 0x3378, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 72, 112, 677, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 64, 104, 685, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 56, 1147, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 64, 1002, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 72 } }, -72, 48, 0, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 8, 56, 0, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, 64, 0, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 88, 0, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 96, 0, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -128, 48, 0, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 56, 0, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 56, 0, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 56, 0, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 48, 0, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 104, 48, 0, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 48, 0, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, 48, 0, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, 48, 0, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 48, 0, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 56, 0, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 64, 0, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 24, 72, 0, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, 80, 0, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 88, 0, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 96, 0, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 104, 0, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 72, 1114, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 80, 1026, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 88, 952, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -112, 96, 888, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -112, 104, 833, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 112, 768, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 112, 784, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 104, 833, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 96, 888, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 88, 952, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 80, 1026, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 72, 1114, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 40, 72, 1079, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 40, 80, 997, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 40, 88, 935, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 40, 96, 888, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 64, 1219, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -88, 48, 0x3333, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -48, 48, 0x3586, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -8, 48, 0x34AD, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 104, 833, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 112, 784, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 112, 800, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 104, 833, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, 64, 1219, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 64, 1219, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 72, 1114, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 72, 1114, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 64, 1219, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 80, 1026, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 88, 952, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 96, 888, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 104, 833, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 112, 784, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 112, 784, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 104, 833, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 88, 952, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 80, 1026, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 96, 888, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 40, 48, 0, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 48, 0, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, 48, 0, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_main_corridor_80187C54[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 40, 0, 0, { 1, 0 } },
    { 40, 42, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_main_corridor_80187C74[79] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 72, 0, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, 112, 689, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 104, 730, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 64, 96, 777, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 88, 830, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 80, 891, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 48, 72, 962, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 64, 1054, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 56, 1202, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -128, 112, 726, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -128, 104, 730, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -120, 96, 762, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -112, 88, 844, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -104, 80, 900, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -104, 72, 961, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -96, 64, 1046, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -88, 56, 1177, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 48, 0x314A, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, 48, 0x31AE, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 72 } }, -40, 48, 0, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 24, 80, 0, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -72, 88, 0, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 72, 48, 0, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 104, 48, 0, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 72, 0, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 72, 0, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 48, 0, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 88, 937, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 104, 0, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 56, 0, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 64, 0, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 56, 0, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 64, 0, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 72, 0, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 48, 0, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 48, 0, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 96, 0, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 104, 0, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 48, 0, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 48, 0, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -128, 48, 0, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -104, 112, 975, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -56, 112, 975, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 112, 975, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 104, 1020, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -56, 104, 1020, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -104, 104, 1020, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -104, 96, 1071, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -56, 96, 1071, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 96, 1071, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, 96, 1071, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, 88, 1129, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 88, 1129, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 88, 1113, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -96, 88, 1129, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -96, 80, 1195, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 80, 1195, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 80, 1195, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, 80, 1195, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 72, 1272, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 72, 1272, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 72, 1272, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 72, 1272, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 64, 1346, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 64, 1346, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 64, 1346, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 64, 1342, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 48, 7218, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -56, 48, 5634, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -8, 48, 5634, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 40, 48, 7078, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 104, 1020, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 112, 975, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 104, 988, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 112, 975, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, 48, 0, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 48, 0, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 80, 0, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 80, 0, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_main_corridor_801882A0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 41, 0, 0, { 1, 0 } },
    { 41, 38, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_main_corridor_801882C0[21] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, -16, 801, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -8, 787, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -8, 785, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -8, 797, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, -24, 666, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, -16, 781, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, -8, 500, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, -24, 500, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, -32, 646, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, -32, 701, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, -40, 640, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -72, 616, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -72, 623, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -72, 621, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, -72, 626, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, -72, 629, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, -72, 612, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -32, 686, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -32, 699, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -48, 645, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -72, 641, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_main_corridor_80188464[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_main_corridor_8018847C[45] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -32, 843, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -32, 832, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -32, 826, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -24, 841, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -24, 848, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -24, 1250, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, -32, 837, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, -64, 748, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -56, 734, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -56, 719, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -48, 711, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -88, 692, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -88, 684, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -88, 677, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -64, 664, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -88, 675, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -64, 678, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -88, 675, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -88, 682, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 96, 497, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 88, 499, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 80, 506, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 72, 513, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -40, 104, 485, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -40, 112, 484, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 96, 492, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 88, 499, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 80, 505, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 72, 1250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 72, 1250, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, -8, 579, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, 8, 572, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 8, 570, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, -8, 596, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, -8, 609, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, -8, 650, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -8, 692, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 8, 753, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 8, 778, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 40, 744, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 32, 720, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 24, 660, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 16, 624, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -136, 48, 1250, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -8, 1250, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_main_corridor_80188800[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 11, 0, 0, { 2, 0 } },
    { 30, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_main_corridor_80188828[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_main_corridor_80188838[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b2_main_corridor_80188848[13] = {
    { { .empty = D_shelter_b2_main_corridor_80184638 }, D_shelter_b2_main_corridor_80184638, NULL },
    { { .elements = D_shelter_b2_main_corridor_80184648 }, D_shelter_b2_main_corridor_80184E68, NULL },
    { { .elements = D_shelter_b2_main_corridor_80184E90 }, D_shelter_b2_main_corridor_801859BC, NULL },
    { { .elements = D_shelter_b2_main_corridor_801859F4 }, D_shelter_b2_main_corridor_80185D14, NULL },
    { { .elements = D_shelter_b2_main_corridor_80185D34 }, D_shelter_b2_main_corridor_80186838, NULL },
    { { .elements = D_shelter_b2_main_corridor_80186858 }, D_shelter_b2_main_corridor_8018703C, NULL },
    { { .elements = D_shelter_b2_main_corridor_8018707C }, D_shelter_b2_main_corridor_801875CC, NULL },
    { { .elements = D_shelter_b2_main_corridor_801875EC }, D_shelter_b2_main_corridor_80187C54, NULL },
    { { .elements = D_shelter_b2_main_corridor_80187C74 }, D_shelter_b2_main_corridor_801882A0, NULL },
    { { .elements = D_shelter_b2_main_corridor_801882C0 }, D_shelter_b2_main_corridor_80188464, NULL },
    { { .elements = D_shelter_b2_main_corridor_8018847C }, D_shelter_b2_main_corridor_80188800, NULL },
    { { .empty = D_shelter_b2_main_corridor_80188828 }, D_shelter_b2_main_corridor_80188828, NULL },
    { { .empty = D_shelter_b2_main_corridor_80188838 }, D_shelter_b2_main_corridor_80188838, NULL },
};

WorldCoordPointLight D_shelter_b2_main_corridor_801888E4[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2129, -1643, -681 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6401 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 88, -1642, -1382 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 1076, 1085 }, { 0, 0 } }, 759, 1859 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2129, -1643, -0x34D9 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3499, 3539, 3569 }, { 0, 0 } }, 2000, 9481 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 151, -1489, -0x3F08 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2397, 2374, 2389 }, { 0, 0 } }, 2942, 4661 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2366, -1643, -8648 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 8601 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2366, -1643, -681 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5869, 122, -1680 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5869, 122, -8548 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
};

WorldCoordRoomLights D_shelter_b2_main_corridor_80188BE4 = { 0, NULL, ARRAY_SIZE(D_shelter_b2_main_corridor_801888E4), D_shelter_b2_main_corridor_801888E4, 0, NULL };

WorldCollisionTrigger D_shelter_b2_main_corridor_80188BFC[18] = {
    { NULL, NULL, NULL, { -32, -1744, -0x32E0, 0 }, { { -2304, -3792, 192, 0 }, { 2304, -3792, -192, 0 }, { -2304, 3792, 192, 0 }, { 2304, 3792, -192, 0 } }, { -343, 0, -4108, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -15, -1728, -0x3382, 0 }, { { 2384, -3792, -192, 0 }, { -2384, -3792, 192, 0 }, { 2384, 3792, -192, 0 }, { -2384, 3792, 192, 0 } }, { 330, 0, 4104, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 96, -1664, -7136, 0 }, { { -2304, -3792, 192, 0 }, { 2304, -3792, -192, 0 }, { -2304, 3792, 192, 0 }, { 2304, 3792, -192, 0 } }, { -343, 0, -4108, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 160, -1441, -7328, 0 }, { { 2384, -3792, -192, 0 }, { -2384, -3792, 192, 0 }, { 2384, 3792, -192, 0 }, { -2384, 3792, 192, 0 } }, { 330, 0, 4104, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -176, -1408, -3584, 0 }, { { 1952, -3792, -32, 0 }, { -1952, -3792, 32, 0 }, { 1952, 3792, -32, 0 }, { -1952, 3792, 32, 0 } }, { 66, 0, 4097, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -96, -1441, -3456, 0 }, { { -1888, -3792, 32, 0 }, { 1888, -3792, -32, 0 }, { -1888, 3792, 32, 0 }, { 1888, 3792, -32, 0 } }, { -70, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1904, -1633, -0x2710, 0 }, { { 64, -3792, -1648, 0 }, { -64, -3792, 1648, 0 }, { 64, 3792, -1648, 0 }, { -64, 3792, 1648, 0 } }, { 4095, 0, 159, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2048, -1664, -0x2711, 0 }, { { -32, -3792, 1664, 0 }, { 32, -3792, -1664, 0 }, { -32, 3792, 1664, 0 }, { 32, 3792, -1664, 0 } }, { -4107, 0, -80, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1856, -1632, -0x2780, 0 }, { { -32, -3792, 1664, 0 }, { 32, -3792, -1664, 0 }, { -32, 3792, 1664, 0 }, { 32, 3792, -1664, 0 } }, { -4107, 0, -80, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2016, -1568, -0x2740, 0 }, { { 64, -3792, -1648, 0 }, { -64, -3792, 1648, 0 }, { 64, 3792, -1648, 0 }, { -64, 3792, 1648, 0 } }, { 4095, 0, 159, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2016, -1600, -1920, 0 }, { { 64, -3792, -1648, 0 }, { -64, -3792, 1648, 0 }, { 64, 3792, -1648, 0 }, { -64, 3792, 1648, 0 } }, { 4095, 0, 159, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 9, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2208, -1632, -1920, 0 }, { { -32, -3792, 1664, 0 }, { 32, -3792, -1664, 0 }, { -32, 3792, 1664, 0 }, { 32, 3792, -1664, 0 } }, { -4107, 0, -80, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 7, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1999, -1568, -2048, 0 }, { { 80, -3792, 1440, 0 }, { -80, -3792, -1440, 0 }, { 80, 3792, 1440, 0 }, { -80, 3792, -1440, 0 } }, { -4106, 0, 227, 0 }, { 0, 0, 4096, 0 }, 4055, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2128, -1600, -2015, 0 }, { { -80, -3792, -1456, 0 }, { 80, -3792, 1456, 0 }, { -80, 3792, -1456, 0 }, { 80, 3792, 1456, 0 } }, { 4093, 0, -226, 0 }, { 0, 0, 4096, 0 }, 4055, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4352, -1536, -9952, 0 }, { { 303, -3792, -1622, 0 }, { -306, -3792, 1618, 0 }, { 303, 3792, -1622, 0 }, { -306, 3792, 1618, 0 } }, { 4026, 0, 756, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 4, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4256, -1664, -9952, 0 }, { { -306, -3792, 1618, 0 }, { 303, -3792, -1622, 0 }, { -306, 3792, 1618, 0 }, { 303, 3792, -1622, 0 } }, { -4027, 0, -758, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 10, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4350, -1632, -1952, 0 }, { { -64, -3792, 1648, 0 }, { 64, -3792, -1648, 0 }, { -64, 3792, 1648, 0 }, { 64, 3792, -1648, 0 } }, { -4096, 0, -160, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 11, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4448, -1568, -1920, 0 }, { { 64, -3792, -1648, 0 }, { -64, -3792, 1648, 0 }, { 64, 3792, -1648, 0 }, { -64, 3792, 1648, 0 } }, { 4095, 0, 159, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 8, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b2_main_corridor_80189154[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_main_corridor_8018916C[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_main_corridor_80189184[3] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { 49, 49, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_201100_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_main_corridor_801891A8[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_main_corridor_801891B4[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 23, 23, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202300_8015FAB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_main_corridor_801891D8[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205700_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_main_corridor_801891FC[4] = {
    { 4, 0, 1, 1700, 1500, -7400, 0, 0, 0, 2, 0 },
    { 4, 0, 17, 1700, 3000, -0x34BC, 1024, 0, 0, 2, 0 },
    { 4, 0, 33, -1700, 2200, -0x3200, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_main_corridor_8018923C[4] = {
    { 4, 0, 1, 1700, 1500, -7400, 0, 0, 0, 2, 0 },
    { 4, 0, 17, 1700, 3000, -0x34BC, 1024, 0, 0, 2, 0 },
    { 4, 0, 33, -1700, 2200, -0x3200, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_main_corridor_8018927C[3] = {
    { 4, 0, 1, 1700, 1500, -7400, 0, 0, 0, 2, 4 },
    { 49, 2, 0, 0, 0, -0x2EE0, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_main_corridor_801892AC[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_main_corridor_801892BC[4] = {
    { 21, 20, 2048, -3600, -1750, -7000, 1024, 0, 0, 2, 7 },
    { 21, 20, 0, 3600, -1750, -7000, 3072, 0, 0, 2, 7 },
    { 23, 8, 1, 0, 0, -2000, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_main_corridor_801892FC[4] = {
    { 21, 23, 2048, -3600, -1750, -7000, 1024, 0, 0, 2, 7 },
    { 21, 23, 0, 3600, -1750, -7000, 3072, 0, 0, 2, 7 },
    { 57, 4, 1, 0, 0, -6000, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b2_main_corridor_8018933C[22] = {
    { NULL, NULL },
    { D_shelter_b2_main_corridor_801891FC, D_shelter_b2_main_corridor_80189154 },
    { D_shelter_b2_main_corridor_8018923C, D_shelter_b2_main_corridor_8018916C },
    { D_shelter_b2_main_corridor_8018927C, D_shelter_b2_main_corridor_80189184 },
    { D_shelter_b2_main_corridor_801892AC, D_shelter_b2_main_corridor_801891A8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_main_corridor_801892BC, D_shelter_b2_main_corridor_801891B4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_main_corridor_801892FC, D_shelter_b2_main_corridor_801891D8 },
};

WorldCollisionTrigger D_shelter_b2_main_corridor_801893EC[7] = {
    { NULL, NULL, NULL, { 0, -48, -0x4A80, 0 }, { { -1024, 0, -512, 0 }, { 1024, 0, -512, 0 }, { -1024, 0, 512, 0 }, { 1024, 0, 512, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_WARP, 27, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4992, -48, -0x2890, 0 }, { { 1056, 0, -752, 0 }, { 1056, 0, 752, 0 }, { -1056, 0, -752, 0 }, { -1056, 0, 752, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, 4095, 0 }, 1292, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 8, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3584, -48, -0x2740, 0 }, { { 512, 0, -1024, 0 }, { 512, 0, 1024, 0 }, { -512, 0, -1024, 0 }, { -512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_WARP, 31, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4960, -48, -2448, 0 }, { { 928, 0, -528, 0 }, { 928, 0, 528, 0 }, { -928, 0, -528, 0 }, { -928, 0, 528, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 7, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3328, -48, -1952, 0 }, { { 512, 0, -1024, 0 }, { 512, 0, 1024, 0 }, { -512, 0, -1024, 0 }, { -512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_WARP, 32, 81, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 32, -48, 192, 0 }, { { 1024, 0, 512, 0 }, { -1024, 0, 512, 0 }, { 1024, 0, -512, 0 }, { -1024, 0, -512, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_WARP, 34, 97, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3200, -64, -2016, 0 }, { { 512, 0, -1024, 0 }, { 512, 0, 1024, 0 }, { -512, 0, -1024, 0 }, { -512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionFootstepSounds D_shelter_b2_main_corridor_80189600 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_b2_main_corridor_8018960C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_main_corridor_80189614[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_main_corridor_8018961C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_main_corridor_80189600 },
};

WorldCollisionSurfaceProperties* D_shelter_b2_main_corridor_80189624[8] = {
    D_shelter_b2_main_corridor_8018960C,
    D_shelter_b2_main_corridor_80189614,
    D_shelter_b2_main_corridor_8018961C,
    D_shelter_b2_main_corridor_8018960C,
    D_shelter_b2_main_corridor_8018960C,
    D_shelter_b2_main_corridor_8018960C,
    D_shelter_b2_main_corridor_8018960C,
    D_shelter_b2_main_corridor_8018960C,
};

AreaApplyRec D_shelter_b2_main_corridor_80189644[2] = {
    { 5, 7, 1, 0 },
    { 255, 0, 0, 0 },
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 D_shelter_b2_main_corridor_8018965C[4] = {
    0,
    223,
    124,
    44,
};

/// Next byte for mixed water quad and draw-mode packets in a borrowed actor-load buffer.
///
/// The drawer resets this word-aligned cursor to the current 0xC000-byte half.
/// The four rectangles consume at most 0x1800 bytes; the buffer stays reserved
/// until the GPU finishes the frame's ordering table.
static u8* _gShelterB2MainCorridorWaterPacketCursor = NULL;

_RoomDepartureStorage gRoomDeparture = { 0 };

RoomLatchedEventStorage gRoomEventLatched = { { 0 }, { 0 } };

RoomDeparture D_shelter_b2_main_corridor_80189684 = { 0, 0, 0, 0, 0, { 0, 0 }, 0 };

#include "../../shared/room_event_departure_task.inc.c"

#include "../../shared/room_event_staged_task.inc.c"

static void _glowDrawBeam(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor);

s32 func_shelter_b2_main_corridor_8017D9C4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent  staged;
    RoomLatchedEvent* p;
    s32               capCmd;
    s16               flag;
    s32               sndId;

    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    if (in->areaId == GAME_AREA_SHELTER_B2_ELEVATOR_HALL && gameFlagGetNibble(GAME_FLAG_B2_CORRIDOR_ELEVATOR_HALL_UNLOCKED) == 0) {
        if (in->queryOnly != ROOM_EVENT_EXECUTE) {
            return 0;
        }
        Gp_SetNibbleIf(in->flagId, 2);
        Gp_RunCapCmd1(1);
        return 0;
    }
    if (in->areaId == GAME_AREA_SHELTER_B2_LABORATORY && gameFlagGetNibble(GAME_FLAG_B2_LABORATORY_DOOR_UNLOCKED) == 0) {
        if (in->queryOnly != ROOM_EVENT_EXECUTE) {
            return 0;
        }
        Gp_SetNibbleIf(in->flagId, 2);
        Gp_RunCapCmd1(2);
        return 0;
    }
    if ((in->areaId == GAME_AREA_SHELTER_B2_LABORATORY || in->areaId == GAME_AREA_SHELTER_B2_BREEDING_ROOM || in->areaId == GAME_AREA_SHELTER_B2_ELEVATOR_HALL) && gameFlagGetNibble(GAME_FLAG_0D1) == 2) {
        if (in->queryOnly != ROOM_EVENT_EXECUTE) {
            return 2;
        }
        Gp_RunCapCmd1(4);
        return 2;
    }
    if (in->areaId == GAME_AREA_SHELTER_B2_SEPTIC_TANK) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId           = 0x54210001;
        capCmd          = 0xA;
        staged.stageSnd = sndId;
        flag            = 0x133;
    } else if (in->areaId == GAME_AREA_SHELTER_B2_BREEDING_ROOM) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId           = 0x54210001;
        capCmd          = 9;
        staged.stageSnd = sndId;
        flag            = 0x134;
    } else if (in->areaId == GAME_AREA_SHELTER_B2_ELEVATOR_HALL) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId           = 0x54210001;
        capCmd          = 0xB;
        staged.stageSnd = sndId;
        flag            = 0x135;
    } else if (in->areaId == GAME_AREA_SHELTER_B2_LABORATORY) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId           = 0x54210001;
        capCmd          = 0xC;
        staged.stageSnd = sndId;
        flag            = 0x136;
    } else {
        return 1;
    }
    p                                         = &staged;
    staged.capCmd                             = capCmd;
    staged.flagId                             = flag;
    staged.fade                               = 0;
    D_shelter_b2_main_corridor_8018965C_value = 0;
    if (gameFlagGetNibble(p->flagId) == 0 || p->flagId == 0) {
        if (out->queryOnly != ROOM_EVENT_EXECUTE) {
            return 2;
        }
        gRoomEventStagedMsg     = *out;
        gRoomEventLatched.event = staged;
        if (p->flagId != 0) {
            gameFlagSetNibble(p->flagId, 1);
        }
        taskSpawnFromTable(&D_shelter_b2_main_corridor_80182C08, 0, 0, 0);
        D_shelter_b2_main_corridor_8018965C_value = 1;
        return 2;
    }
    return 1;
}

s32 func_shelter_b2_main_corridor_8017DC88(Task* arg0, s32 arg1, const void* firstArg, s32 arg3)
{
    s32 id;

    if (((const DirectionActionRequest*)firstArg)->actionId == 0xA) {
        if (((const DirectionActionRequest*)firstArg)->argument == 7) {
            if (gameFlagGetNibble(GAME_FLAG_B2_CORRIDOR_OBSERVATORY_ACCESS) != 0) {
                if (gameFlagGetNibble(GAME_FLAG_SHELTER_B2_MAIN_CORRIDOR_0DA) != 0) {
                    D_shelter_b2_main_corridor_80189684.stage = GAME_STAGE_SHELTER_NEO_ARK;
                    D_shelter_b2_main_corridor_80189684.area  = ((const DirectionActionRequest*)firstArg)->argument;
                } else {
                    D_shelter_b2_main_corridor_80189684.stage = GAME_STAGE_MINE_SHELTER;
                    D_shelter_b2_main_corridor_80189684.area  = 0x31;
                }
                D_shelter_b2_main_corridor_80189684.room     = 1;
                D_shelter_b2_main_corridor_80189684.warp     = 1;
                D_shelter_b2_main_corridor_80189684.sndEvent = 0;
                D_shelter_b2_main_corridor_80189684.facing   = ROOM_DEPARTURE_SKIP_FACING;
                Gp_MsgPlayerWeapon(0);
                taskSpawnFromTable(D_shelter_b2_main_corridor_80182C44, 0, 6, 0);
            } else {
                Gp_RunCapCmd1(3);
                taskSpawnFromTable(D_shelter_b2_main_corridor_80182C44, 1, 0x1C4, 0);
            }
        }
        if (((const DirectionActionRequest*)firstArg)->argument == 8) {
            if (gameFlagGetNibble(GAME_FLAG_0D1) == 2) {
                Gp_RunCapCmd1(4);
                return 0;
            }
            if (gameFlagGetNibble(GAME_FLAG_0F8) != 0) {
                Gp_RunCapCmd1(8);
                taskSpawnFromTable(D_shelter_b2_main_corridor_80182C44, 1, 0x1AF, 0);
                return 0;
            }
            if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) != 0) {
                id = 0xD;
            } else {
                id = 7;
            }
            D_shelter_b2_main_corridor_80189684.stage    = GAME_STAGE_SHELTER_NEO_ARK;
            D_shelter_b2_main_corridor_80189684.area     = ((const DirectionActionRequest*)firstArg)->argument;
            D_shelter_b2_main_corridor_80189684.room     = 1;
            D_shelter_b2_main_corridor_80189684.warp     = 1;
            D_shelter_b2_main_corridor_80189684.sndEvent = 0;
            D_shelter_b2_main_corridor_80189684.facing   = ROOM_DEPARTURE_SKIP_FACING;
            Gp_MsgPlayerWeapon(0);
            taskSpawnFromTable(D_shelter_b2_main_corridor_80182C44, 0, id, 0);
        }
    }
    if (((const DirectionActionRequest*)firstArg)->actionId == 1 && gameFlagGetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS) >= 2 && gameFlagGetNibble(GAME_FLAG_SHELTER_B2_MAIN_CORRIDOR_0D3) == 0) {
        gameFlagSetNibble(GAME_FLAG_SHELTER_B2_MAIN_CORRIDOR_0D3, 1);
        gameFlagSetNibble(GAME_FLAG_B2_CORRIDOR_OBSERVATORY_ACCESS, 1);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_B2_MAIN_CORRIDOR, 0);
        evsStartScript(D_shelter_b2_main_corridor_80182CA8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// The room task's three states, run by
/// `shelterB2MainCorridorRoomTask`: install the message table and
/// spawn the room's tasks, idle, and end.
static const TaskFuncTable3 D_shelter_b2_main_corridor_8017D5F0 = {
    { _shelterB2MainCorridorInitializeRoomTask, _shelterB2MainCorridorIdleRoomTask, taskKill }
};

void func_shelter_b2_main_corridor_8017DEB0(Task* arg0)
{
    RoomEventMsg        param;
    RoomVariantResolver resolve;

    switch (arg0->state) {
        case 0:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_RunCapCmd1(arg0->spawnArg1.value);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (capIsBusy() != 0) {
                break;
            }
            arg0->state++;
            break;
        case 2:
            if (capGetVariantKey() == 0xC) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                break;
            }
            arg0->state++;
            break;
        case 3:
            arg0->state++;
            break;
        case 4:
            if (D_shelter_b2_main_corridor_80189684.area == 7 && gameFlagGetNibble(GAME_FLAG_0D1) == 2) {
                gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 9);
            }
            if (D_shelter_b2_main_corridor_80189684.area == 0x31) {
                if (gameFlagGetNibble(GAME_FLAG_0D1) == 2) {
                    gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 9);
                }
                if (gameFlagGetNibble(GAME_FLAG_SHELTER_B2_MAIN_CORRIDOR_0DA) == 0) {
                    gameFlagSetNibble(GAME_FLAG_SHELTER_B2_MAIN_CORRIDOR_0DA, 1);
                    gameFlagSetNibble(GAME_FLAG_STORY_CHAPTER, 5);
                }
            }
            if (D_shelter_b2_main_corridor_80189684.area == 8) {
                if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 1) {
                    gameFlagSetNibble(GAME_FLAG_0F8, 1);
                }
            }
            resolve = _roomVariantResolveNeoArk;
            Gp_MsgPlayerWeapon(0);
            param.areaId    = D_shelter_b2_main_corridor_80189684.area;
            param.warp      = D_shelter_b2_main_corridor_80189684.warp;
            param.room      = D_shelter_b2_main_corridor_80189684.room;
            param.queryOnly = ROOM_EVENT_EXECUTE;
            resolve(&param, &param);
            D_shelter_b2_main_corridor_80189684.area = param.areaId;
            D_shelter_b2_main_corridor_80189684.warp = param.warp;
            D_shelter_b2_main_corridor_80189684.room = param.room;
            gRoomDeparture.departure                 = D_shelter_b2_main_corridor_80189684;
            taskSpawnFromTable(&D_shelter_b2_main_corridor_801828E0.desc, 0, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Binds the Neo Ark room-variant definition to this room's private resolver.
///
/// Supply one function identifier with the `RoomVariantResolver` signature;
/// its prologue declaration establishes static linkage. Keep the binding
/// through the fragment, then undefine it. No arguments, runtime evaluation,
/// captured values, stringification or token pasting are involved.
#define ROOM_VARIANT_RESOLVE_NEO_ARK _roomVariantResolveNeoArk
#include "../../shared/room_variants_neo_ark.inc.c"
#undef ROOM_VARIANT_RESOLVE_NEO_ARK

/// Refuses every key-item use in this room, returning the menu's unavailable result zero.
static s32 _shelterB2MainCorridorRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Ignores room commands and returns zero without changing room state.
static s32 _shelterB2MainCorridorIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Queues room sound script 9 for command 9; other commands are ignored, and the result is always zero.
static s32 _shelterB2MainCorridorHandleSoundMessage(Task* task, s32 messageId, s32 soundCommandId, s32 unusedArg)
{
    enum {
        SOUND_COMMAND_START_SCRIPT_9 = 9,
        SOUND_SCRIPT_9               = 0x54210009, // Bank 0x5421, instance 0, entry 9
    };

    if (soundCommandId == SOUND_COMMAND_START_SCRIPT_9) {
        sndEvtRequestScriptStart(SOUND_SCRIPT_9, 0, 0);
    }
    return 0;
}

/// Waits for the capture command the message handler started to finish, then
/// sets the game-flag nibble named by the task's spawn argument to 2 unless
/// the capture ended on event key 0xC, and ends the task.
void func_shelter_b2_main_corridor_8017E210(Task* arg0)
{
    if (capIsBusy() == 0) {
        if (capGetVariantKey() != 0xC) {
            gameFlagSetNibble(arg0->spawnArg1.value, 2);
        }
        taskKill(arg0);
    }
}

static void func_shelter_b2_main_corridor_8017E264(RoomEventMsg* msg)
{
    if ((gameFlagGetNibble(GAME_FLAG_COMPANION_1_SCHEDULE) == 9) && (gameFlagGetNibble(GAME_FLAG_0D1) == 3) && (msg->queryOnly == ROOM_EVENT_EXECUTE)) {
        gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 0);
        Gp_ApplyAreaRecs(D_shelter_b2_main_corridor_80189644);
    }
}

/// Registers the room's message receiver, starts its water task and advances to idle.
///
/// Called once in state zero; the loaded overlay supplies the borrowed message
/// table for the room task's lifetime.
static void _shelterB2MainCorridorInitializeRoomTask(Task* task)
{
    task->msgTable = D_shelter_b2_main_corridor_80182C14;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(D_shelter_b2_main_corridor_80182DE0, 0, 0, 0);
    task->state++;
}

/// Keeps the initialized room task live to receive messages without per-frame work.
static void _shelterB2MainCorridorIdleRoomTask(Task* task)
{
}

void shelterB2MainCorridorRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_b2_main_corridor_8017D5F0;
    states.funcs[task->state](task);
}

/// Selects the drawer's actor-load cursor reset and corridor view exclusions.
#define WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR 1
/// Binds the readable four-rectangle list, including its in-bounds terminator.
///
/// Supplies a `RoomWaterSurface*` without side effects or captured locals.
/// The shared strip include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_SURFACES _gShelterB2MainCorridorWaterWaveSurfaces
/// Binds the undisplaced water Y in signed world units.
///
/// Read once per draw from the room's `s16`; no side effects or captured locals.
/// The shared strip include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_HEIGHT gShelterB2MainCorridorWaterY
/// Binds the writable byte cursor for mixed water quad and draw-mode packets.
///
/// A stable `u8*` lvalue, read and advanced repeatedly; requires word alignment
/// and a borrowed arena reserved until GPU consumption. The shared strip
/// include consumes and undefines this binding.
/// This instance resets the cursor to the selected display buffer half.
#define WATER_WAVE_STRIPS_PACKET_CURSOR _gShelterB2MainCorridorWaterPacketCursor
/// Scales the seam's sine displacement to -64..64 world-coordinate Y units.
///
/// Integer shift count for `water_wave_strips.inc.c`; see its configuration
/// contract. The include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_AMPLITUDE_SHIFT 6
#include "../../shared/water_wave_strips.inc.c"

/// Draws the corridor's wave surfaces and publishes their signed world Y each tick.
///
/// State 0 clears the selected actor-load buffer's session word; state 1 draws
/// four rectangles except in Shelter views 10 and 11. The task stays live. The selected buffer
/// must remain available for both display halves until GPU consumption; each
/// drawing tick can use 0x1800 bytes of its current 0xC000-byte half.
static void _shelterB2MainCorridorWaterTask(Task* task)
{
    TaskFunc states[] = { _shelterB2MainCorridorInitializeWaterTask, _waterDrawWaveStrips };

    states[task->state](task);
    gGameSession->waterY = gShelterB2MainCorridorWaterY;
}

/// Clears the session word associated with the actor-load buffer used for water.
///
/// With no companion, selects buffer 2; otherwise selects buffer 1. The word's
/// nonzero meaning is unproven. Advances state 0 to the surface-drawing state 1.
static void _shelterB2MainCorridorInitializeWaterTask(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    task->state++;
}

/// Draws three additive flickering white beams for the corridor's views 10 and 11.
///
/// Borrows world-space endpoint pairs at point offsets 0, 4 and 8, selecting
/// every other pair from the interleaved view data. Screen-space start angles
/// are a quarter, half and quarter turn (4096 units per turn). Requires the
/// current view, scratch stack, ordering table and packet arena; queued
/// additive packets live until GPU consumption.
///
/// The view-10 caller spans adjacent array declarations; its C array boundary
/// is unproven.
static inline void _shelterB2MainCorridorDrawWhiteBeamGroup(const SVECTOR worldBeamPoints[])
{
    enum { SHELTER_B2_MAIN_CORRIDOR_WHITE_BEAM_POINT_STRIDE = 4 };

    _glowDrawBeam(worldBeamPoints, SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE);
    _glowDrawBeam(&worldBeamPoints[SHELTER_B2_MAIN_CORRIDOR_WHITE_BEAM_POINT_STRIDE], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE);
    _glowDrawBeam(&worldBeamPoints[2 * SHELTER_B2_MAIN_CORRIDOR_WHITE_BEAM_POINT_STRIDE], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE);
}

void shelterB2MainCorridorDrawViewGlowsTask(Task* task)
{
    enum { VIEW_GLOWS_INITIALIZE,
           VIEW_GLOWS_DRAW,
           VIEW_GLOWS_INDEX_MASK = 0xFF };

    switch (task->state) {
        case VIEW_GLOWS_INITIALIZE:
            // Install this loaded overlay's effect IDs before drawing its first view.
            gRoomEffectFlashId       = EFFECT_SHELTER_B2_MAIN_CORRIDOR_FLASH;
            gRoomEffectTwinTrailId   = EFFECT_SHELTER_B2_MAIN_CORRIDOR_TWIN_TRAIL;
            gRoomEffectSparkBurstId  = EFFECT_SHELTER_B2_MAIN_CORRIDOR_SPARK_BURST;
            gRoomEffectWaterRippleId = EFFECT_SHELTER_B2_MAIN_CORRIDOR_WATER_RIPPLE;
            gRoomEffectWaterSprayId  = EFFECT_SHELTER_B2_MAIN_CORRIDOR_WATER_SPRAY;
            task->state              = VIEW_GLOWS_DRAW;
            /* fallthrough */
        case VIEW_GLOWS_DRAW:
            switch (viewGetMappedIndex() & VIEW_GLOWS_INDEX_MASK) {
                case 2: {
                    const SVECTOR* beamPoints = D_shelter_b2_main_corridor_80182F7C;
                    _glowDrawBeam(&beamPoints[0], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, 0, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE);
                    _glowDrawBeam(&beamPoints[2], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, 0, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE);
                    break;
                }
                case 3: {
                    const SVECTOR* beamPoints = D_shelter_b2_main_corridor_80182FAC;
                    _glowDrawBeam(&beamPoints[0], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_GREEN);
                    _glowDrawBeam(&beamPoints[2], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, 0, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_GREEN);
                    _glowDrawBeam(&beamPoints[4], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, 0, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_GREEN);
                    _glowDrawBeam(&beamPoints[6], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_RED);
                    glowDrawFlareClipped(&beamPoints[14], SHELTER_B2_MAIN_CORRIDOR_FLARE_TEXTURE_COLUMN, SHELTER_B2_MAIN_CORRIDOR_FLARE_RADIUS_SCALE);
                    glowDrawFlareClipped(&beamPoints[21], SHELTER_B2_MAIN_CORRIDOR_FLARE_TEXTURE_COLUMN, SHELTER_B2_MAIN_CORRIDOR_FLARE_RADIUS_SCALE);
                    break;
                }
                case 4: {
                    const SVECTOR* beamPoints = D_shelter_b2_main_corridor_80182F9C;
                    _glowDrawBeam(&beamPoints[0], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_GREEN);
                    _glowDrawBeam(&beamPoints[24], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE);
                    _glowDrawBeam(&beamPoints[28], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE);
                    _glowDrawBeam(&beamPoints[32], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE);
                    break;
                }
                case 5:
                    _glowDrawBeam(D_shelter_b2_main_corridor_80182FBC, SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, 0, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_GREEN);
                    break;
                case 6:
                    _glowDrawBeam(D_shelter_b2_main_corridor_80182FAC, SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_GREEN);
                    // View 6 also includes the beams visible in view 7.
                    /* fallthrough */
                case 7: {
                    const SVECTOR* beamPoints = D_shelter_b2_main_corridor_80182FCC;
                    _glowDrawBeam(&beamPoints[0], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, 0, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_GREEN);
                    _glowDrawBeam(&beamPoints[2], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_RED);
                    break;
                }
                case 8: {
                    const SVECTOR* beamPoints = D_shelter_b2_main_corridor_80182FAC;
                    _glowDrawBeam(&beamPoints[0], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_GREEN);
                    _glowDrawBeam(&beamPoints[24], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE);
                    _glowDrawBeam(&beamPoints[28], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE);
                    _glowDrawBeam(&beamPoints[32], SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_WHITE);
                    break;
                }
                case 9:
                    _glowDrawBeam(D_shelter_b2_main_corridor_80182FCC, SHELTER_B2_MAIN_CORRIDOR_BEAM_RADIUS_SCALE, 0, SHELTER_B2_MAIN_CORRIDOR_BEAM_COLOR_GREEN);
                    break;
                case 10: {
                    const SVECTOR* beamPoints = D_shelter_b2_main_corridor_8018305C;
                    _shelterB2MainCorridorDrawWhiteBeamGroup(beamPoints);
                    break;
                }
                case 11: {
                    const SVECTOR* beamPoints = D_shelter_b2_main_corridor_8018306C;
                    _shelterB2MainCorridorDrawWhiteBeamGroup(beamPoints);
                    break;
                }
            }
            break;
    }
}

#include "../../shared/water_ripple_task.inc.c"

void shelterB2MainCorridorWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

/// Binds the uncomposed water-drift body's public void (Task*) callback to this room.
///
/// The public room header supplies its prototype. This binding names a function,
/// has no arguments or side effects, and is used only by the following include.
#define WATER_DRIFT_UNCOMPOSED_TASK shelterB2MainCorridorWaterDriftTask
#include "../../shared/water_drift_task_no_update.inc.c"
#undef WATER_DRIFT_UNCOMPOSED_TASK

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"

#include "../../shared/glow_draw_beam.inc.c"

#include "../../shared/glow_draw_flare_clipped.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelterB2MainCorridorRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelterB2MainCorridorRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b2_main_corridor_80181C98(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
