#include "rooms/acropolis_west_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

/// World X of both elevator-door leaves when the door is shut: the line the
/// two leaves meet on.
#define ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_X (-1000)

/// World units a door leaf slides per frame while the door opens or closes.
#define ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_SPEED 20

/// Travel of a fully retracted door leaf, in world units; a leaf is 710 wide,
/// so this clears the opening.
#define ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX 720

/// Work block of one leaf of the elevator's sliding door, held in `Task::work`.
///
/// The door is two mirrored leaves, each its own task with its own block. The
/// task's `spawnArg1` selects opening (1), closing (-1) or rest (0), and its
/// `spawnArg2` the side the leaf retracts to (-1 or 1 along X).
typedef struct {
    s32 travel; // World units slid from the shut position, 0 to `ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX`
} _AcropolisWestElevatorHallDoorLeafWork;
STATIC_ASSERT_SIZEOF(_AcropolisWestElevatorHallDoorLeafWork, 4);

extern TaskDesc         D_acropolis_west_elevator_hall_80184568[];
extern EvsCommand       D_acropolis_west_elevator_hall_80184620[];
extern EvsCommand       D_acropolis_west_elevator_hall_80184890[];
extern s32              D_acropolis_west_elevator_hall_801849C8;
extern TaskMessageEntry D_acropolis_west_elevator_hall_801849CC[];
extern TaskMessageEntry D_acropolis_west_elevator_hall_801849F4[];

/// Index of the mirror model's coordinate part each held-object reflection is
/// parented to, by the reflection's `spawnArg1`.
static u8 Reflection_Data_8017FC8C[];

/// The mirror's task table: entry 0 runs the mirror itself, entry 1 a
/// held-object reflection.
static TaskDesc D_acropolis_west_elevator_hall_801802A8[];

/// The lift bay's two 256-entry RGB555 CLUTs and the blend destination:
/// `D_acropolis_west_elevator_hall_80184A04` is the unlit base palette,
/// `D_acropolis_west_elevator_hall_80184C04` the lit one and
/// `D_acropolis_west_elevator_hall_80184E04` the blended result.
///
/// The blend result is written only as colours.
/// `D_acropolis_west_elevator_hall_80185004` borrows it as packed words
/// because that is the form the GPU upload takes.
extern u16            D_acropolis_west_elevator_hall_80184A04[];
extern u16            D_acropolis_west_elevator_hall_80184C04[];
extern u16            D_acropolis_west_elevator_hall_80184E04[256];
extern GpuImageUpload D_acropolis_west_elevator_hall_80185004[];

/// The hall's two elevator-car tasks, spawned by the room task.
extern Task* D_acropolis_west_elevator_hall_80186AE4[];

/// Keeps the reflection scale at this room's earlier rodata position.
///
/// 0 requires `planar_reflection_rodata.inc.c` before the shared implementation.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 0
#include "../../shared/planar_reflection.h"
// Exported instance: another image refers to this package's copy by name.
#define redBeaconTask acropolisWestElevatorHallRedBeaconTask
#include "../../shared/red_beacon.h"

static void func_acropolis_west_elevator_hall_8017F354(Task* task);
static void func_acropolis_west_elevator_hall_8017F568(Task* arg0);
static void func_acropolis_west_elevator_hall_8017F64C(Task* task);
static void func_acropolis_west_elevator_hall_8017F6F0(Task* task);

/// Scale applied to held-object reflections in slots 2 and up: it mirrors
/// them across X.
#include "../../shared/planar_reflection_rodata.inc.c"

/// State handlers of the room task: set-up, the cutscene hand-off and
/// `taskKill`.
static const TaskFuncTable3 D_acropolis_west_elevator_hall_8017D5D4 = {
    { func_acropolis_west_elevator_hall_8017F568, func_acropolis_west_elevator_hall_8017F354, taskKill },
};

/// State handlers of an elevator-car task: set-up, travel and `taskKill`.
static const TaskFuncTable3 D_acropolis_west_elevator_hall_8017D5E0 = {
    { func_acropolis_west_elevator_hall_8017F64C, func_acropolis_west_elevator_hall_8017F6F0, taskKill },
};

/// Position of the first effect `func_acropolis_west_elevator_hall_8017F7D4`
/// spawns in view 2.
static const SVECTOR D_acropolis_west_elevator_hall_8017D5EC = { -0x1518, -0x720, 0xAC, 0 };

/// Position of the effect `func_acropolis_west_elevator_hall_8017F7D4` spawns
/// in view 5.
static const SVECTOR D_acropolis_west_elevator_hall_8017D5F4 = { -0x79, -0x876, 0x703, 0 };

s32 func_acropolis_west_elevator_hall_8017F470(Task*, s32, s32, s32);
s32 func_acropolis_west_elevator_hall_8017F498(Task*, s32, s32, s32);
s32 func_acropolis_west_elevator_hall_8017F4C0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_acropolis_west_elevator_hall_8017F560(Task*, s32, s32, s32);
s32 func_acropolis_west_elevator_hall_80180274(Task*, s32, s32, s32);

extern AnimationPlayRequest     D_acropolis_west_elevator_hall_80184598;
extern AnimationBankCopyRequest D_acropolis_west_elevator_hall_80184590;
extern ActorTransform           D_acropolis_west_elevator_hall_801845AC;
extern ActorTransform           D_acropolis_west_elevator_hall_801845C4;

static AnimationSet _gAcropolisWestElevatorHallAnimation06F80;
static TmdSource    _gAcropolisWestElevatorHallModel02DE8;
static TmdSource    _gAcropolisWestElevatorHallModel03058;
void                func_acropolis_west_elevator_hall_8017F418(Task*);

extern WorldCollisionGrid    D_acropolis_west_elevator_hall_801852FC[1];
extern WorldCollisionTrigger D_acropolis_west_elevator_hall_80185320[4];
extern WorldCollisionTrigger D_acropolis_west_elevator_hall_80185450[5];
extern WorldCoordRoomLights  D_acropolis_west_elevator_hall_801869E4[1];

#include "../../shared/planar_reflection_data.inc.c"

static TaskDesc D_acropolis_west_elevator_hall_801802A8[2] = {
    { { { TASK_BODY_NONE, 112 } }, func_acropolis_west_elevator_hall_8017F304, { .value = 0 } },
    { { { TASK_BODY_NONE, 112 } }, Reflection_HeldObjectTask, { .value = 0 } },
};

static inline TaskDesc* Reflection_GetTasks(void)
{
    return D_acropolis_west_elevator_hall_801802A8;
}

static TmdBone _gAcropolisWestElevatorHallModel02DE8Skeleton[1] = {
#include "assets/acropolis_west_elevator_hall_model_02DE8_skeleton.inc"
};

static u32 _gAcropolisWestElevatorHallModel02DE8PartVerts[1] = {
#include "assets/acropolis_west_elevator_hall_model_02DE8_partVerts.inc"
};

static SVECTOR _gAcropolisWestElevatorHallModel02DE8Verts[24] = {
#include "assets/acropolis_west_elevator_hall_model_02DE8_verts.inc"
};

static u32 _gAcropolisWestElevatorHallModel02DE8Stream[89] = {
#include "assets/acropolis_west_elevator_hall_model_02DE8_stream.inc"
};

static TmdSource _gAcropolisWestElevatorHallModel02DE8 = {
    0,
    576,
    0,
    1,
    _gAcropolisWestElevatorHallModel02DE8PartVerts,
    _gAcropolisWestElevatorHallModel02DE8Verts,
    &_gAcropolisWestElevatorHallModel02DE8Verts[24],
    _gAcropolisWestElevatorHallModel02DE8Skeleton,
    _gAcropolisWestElevatorHallModel02DE8Stream,
};

static TmdBone _gAcropolisWestElevatorHallModel03058Skeleton[1] = {
#include "assets/acropolis_west_elevator_hall_model_03058_skeleton.inc"
};

static u32 _gAcropolisWestElevatorHallModel03058PartVerts[1] = {
#include "assets/acropolis_west_elevator_hall_model_03058_partVerts.inc"
};

static SVECTOR _gAcropolisWestElevatorHallModel03058Verts[24] = {
#include "assets/acropolis_west_elevator_hall_model_03058_verts.inc"
};

static u32 _gAcropolisWestElevatorHallModel03058Stream[89] = {
#include "assets/acropolis_west_elevator_hall_model_03058_stream.inc"
};

static TmdSource _gAcropolisWestElevatorHallModel03058 = {
    0,
    576,
    0,
    1,
    _gAcropolisWestElevatorHallModel03058PartVerts,
    _gAcropolisWestElevatorHallModel03058Verts,
    &_gAcropolisWestElevatorHallModel03058Verts[24],
    _gAcropolisWestElevatorHallModel03058Skeleton,
    _gAcropolisWestElevatorHallModel03058Stream,
};

static AnimationPackedPose _gAcropolisWestElevatorHallAnimation06F80Bank1[100] = {
#include "assets/acropolis_west_elevator_hall_animation_06F80_bank1.inc"
};

static AnimationPackedRotation _gAcropolisWestElevatorHallAnimation06F80Bank4[1644] = {
#include "assets/acropolis_west_elevator_hall_animation_06F80_bank4.inc"
};

static AnimationRecord _gAcropolisWestElevatorHallAnimation06F80Records[1990] = {
#include "assets/acropolis_west_elevator_hall_animation_06F80_records.inc"
};

static u16 _gAcropolisWestElevatorHallAnimation06F80Indices[20] = {
#include "assets/acropolis_west_elevator_hall_animation_06F80_indices.inc"
};

static AnimationSet _gAcropolisWestElevatorHallAnimation06F80 = {
    _gAcropolisWestElevatorHallAnimation06F80Records,
    _gAcropolisWestElevatorHallAnimation06F80Indices,
    { NULL, _gAcropolisWestElevatorHallAnimation06F80Bank1, NULL, NULL, _gAcropolisWestElevatorHallAnimation06F80Bank4, NULL, NULL, NULL },
};

TaskDesc D_acropolis_west_elevator_hall_80184568[3] = {
    { { { TASK_BODY_TMD, 192 } }, func_acropolis_west_elevator_hall_8017F418, { .model = &_gAcropolisWestElevatorHallModel02DE8 } },
    { { { TASK_BODY_TMD, 192 } }, func_acropolis_west_elevator_hall_8017F418, { .model = &_gAcropolisWestElevatorHallModel03058 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

AnimationSet* D_acropolis_west_elevator_hall_8018458C[1] = {
    &_gAcropolisWestElevatorHallAnimation06F80,
};

AnimationBankCopyRequest D_acropolis_west_elevator_hall_80184590 = { { .sets = D_acropolis_west_elevator_hall_8018458C }, ARRAY_SIZE(D_acropolis_west_elevator_hall_8018458C) };

AnimationPlayRequest D_acropolis_west_elevator_hall_80184598 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_acropolis_west_elevator_hall_801845AC = { { 0x2710, 0, 2527, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_acropolis_west_elevator_hall_801845C4 = { { 220, 0, 2650, 0 }, { 0, 2048, 0, 0 } };

// Retained parameter record; layout follows the adjacent script arguments.
ActorTransform D_acropolis_west_elevator_hall_801845DC = { { 220, 0, 2650, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_acropolis_west_elevator_hall_801845F4 = { { -1388, 0, 922, 0 }, { 0, 2048, 0, 0 } };

AnimationPlayRequest D_acropolis_west_elevator_hall_8018460C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_acropolis_west_elevator_hall_80184620[26] = {
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 67 }, { .value = 67 }, { .value = 78 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1020 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_west_elevator_hall_8018460C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_acropolis_west_elevator_hall_80184590 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_west_elevator_hall_801845AC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM_EFFECT }, { .value = 0 }, { .value = 3100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51110003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM }, { .value = 0 }, { .value = 5100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_west_elevator_hall_801845C4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_west_elevator_hall_80184598 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51110004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM }, { .value = 0 }, { .value = 5101 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1020 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 130 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_west_elevator_hall_801845F4 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_west_elevator_hall_80184890[13] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_west_elevator_hall_801845F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_west_elevator_hall_8018460C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 D_acropolis_west_elevator_hall_801849C8 = 0;

TaskMessageEntry D_acropolis_west_elevator_hall_801849CC[5] = {
    { 5100, func_acropolis_west_elevator_hall_8017F470 },
    { 5101, func_acropolis_west_elevator_hall_8017F498 },
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_west_elevator_hall_8017F4C0 },
    { 5105, func_acropolis_west_elevator_hall_8017F560 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskMessageEntry D_acropolis_west_elevator_hall_801849F4[2] = {
    { 3100, func_acropolis_west_elevator_hall_80180274 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u16 D_acropolis_west_elevator_hall_80184A04[256] = { 0 };

u16 D_acropolis_west_elevator_hall_80184C04[256] = {
    0,
    0x8421,
    0x8842,
    0x8C63,
    0x98C7,
    0x94A6,
    0x9085,
    0x98C8,
    0x8C64,
    0x9086,
    0x8843,
    0x8C65,
    0x8422,
    0x8844,
    0x8423,
    0x8001,
    0x94C9,
    0xB5D0,
    0xA96D,
    0xA54C,
    0x98E9,
    0x94C8,
    0x8865,
    0xB1AF,
    0xA12B,
    0x90A7,
    0xBE12,
    0xAD8E,
    0x9D0A,
    0x8C86,
    0xD6FA,
    0xD2D9,
    0xC676,
    0xC255,
    0xB5F2,
    0xDB1B,
    0xCEB8,
    0xCA97,
    0xBE34,
    0xBA13,
    0x9D2C,
    0x990B,
    0xADD2,
    0xA9B1,
    0xD2D8,
    0xC675,
    0xE37E,
    0xC254,
    0xD2FA,
    0xB5F1,
    0xB1D0,
    0x98E8,
    0xC276,
    0xA56D,
    0xB1F2,
    0xA16E,
    0x94E9,
    0x90C8,
    0x8864,
    0xA54B,
    0xCA96,
    0xA12A,
    0xE79F,
    0xDF5D,
    0xBE33,
    0xBA12,
    0xCED9,
    0xC697,
    0xADAF,
    0x94C7,
    0xA98E,
    0xBE55,
    0xBA34,
    0xB613,
    0x90A6,
    0xADD1,
    0x9D2B,
    0xA9B0,
    0xA58F,
    0x990A,
    0xADF3,
    0xA9D2,
    0x8443,
    0x9D09,
    0x8C85,
    0xE37D,
    0xD2F9,
    0xE39F,
    0xC275,
    0xDF7E,
    0xB1F1,
    0xA16D,
    0xADF2,
    0xA9D1,
    0xA5B0,
    0xA18F,
    0xCED8,
    0xBE54,
    0x9D4C,
    0xEBBF,
    0xDB3B,
    0xCAB7,
    0xC696,
    0xBA33,
    0xB612,
    0xA9AF,
    0x992B,
    0xB5F0,
    0xA56C,
    0x94E8,
    0xE7BF,
    0xA14B,
    0x90C7,
    0xA18E,
    0xBA11,
    0xADAE,
    0xA98D,
    0x9D2A,
    0x9909,
    0x8CA6,
    0x8885,
    0xEFDF,
    0x9509,
    0xEBDF,
    0xF3FF,
    0x94C6,
    0x90A5,
    0x9908,
    0x8C84,
    0x8863,
    0x8CA5,
    0x8442,
    0x8884,
    0x8021,
    0xFC00,
    0xFC00,
    0xFC00,
    0xFC00,
    0xFC00,
    0xFC00,
    0xFC00,
    0xFC00,
    0xFC00,
    0xFC00,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
};

u16 D_acropolis_west_elevator_hall_80184E04[256] = { 0 };

GpuImageUpload D_acropolis_west_elevator_hall_80185004[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 270, 256, 1 }, (u_long*)D_acropolis_west_elevator_hall_80184E04 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

WorldCollisionRoomResources D_acropolis_west_elevator_hall_80185024[1] = {
    { D_acropolis_west_elevator_hall_801852FC, D_acropolis_west_elevator_hall_80185320, D_acropolis_west_elevator_hall_80185450, NULL },
};

u8* D_acropolis_west_elevator_hall_80185034[1] = {
    gViewIdentityMap,
};

ViewCount D_acropolis_west_elevator_hall_80185038[1] = { 5 };

WorldCoordRoomLighting D_acropolis_west_elevator_hall_8018503C[1] = {
    { D_acropolis_west_elevator_hall_801869E4, NULL },
};

DirectionWarpEntry D_acropolis_west_elevator_hall_80185044[2] = {
    { { { .word = 2048 }, -1234, -30, 1924 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -1234, -30, 1924 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 5384, -30, 1039 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 5384, -30, 1039 }, { 0, 0, 0, 0 }, 0x51110002, 0x51110001, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, 503 },
};

static SVECTOR _gAcropolisWestElevatorHallCollision07D3CNormals[7] = {
#include "assets/acropolis_west_elevator_hall_collision_07D3C_normals.inc"
};

static SVECTOR _gAcropolisWestElevatorHallCollision07D3CVerts[29] = {
#include "assets/acropolis_west_elevator_hall_collision_07D3C_verts.inc"
};

static WorldCollisionGridFace _gAcropolisWestElevatorHallCollision07D3CFaces[14] = {
#include "assets/acropolis_west_elevator_hall_collision_07D3C_faces.inc"
};

static s16 _gAcropolisWestElevatorHallCollision07D3CCells[52] = {
#include "assets/acropolis_west_elevator_hall_collision_07D3C_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisWestElevatorHallCollision07D3CCells[i])
static s16* _gAcropolisWestElevatorHallCollision07D3CTable[6] = {
#include "assets/acropolis_west_elevator_hall_collision_07D3C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_west_elevator_hall_801852FC[1] = {
    { NULL, _gAcropolisWestElevatorHallCollision07D3CNormals, _gAcropolisWestElevatorHallCollision07D3CVerts, _gAcropolisWestElevatorHallCollision07D3CFaces, _gAcropolisWestElevatorHallCollision07D3CTable, 5480, 2000, 3, 2, 4000, 14 },
};

WorldCollisionTrigger D_acropolis_west_elevator_hall_80185320[4] = {
    { NULL, NULL, NULL, { -994, -1504, 95, 0 }, { { -616, -1504, -2431, 0 }, { -616, 1504, -2431, 0 }, { 617, -1504, 2432, 0 }, { 617, 1504, 2432, 0 } }, { -3972, 0, 1006, 0 }, { 0, 0, 0, 0 }, 2918, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1281, -1536, 111, 0 }, { { -605, 1504, -2321, 0 }, { -605, -1504, -2321, 0 }, { 606, 1504, 2322, 0 }, { 606, -1504, 2322, 0 } }, { 3966, 0, -1036, 0 }, { 0, 0, 0, 0 }, 2827, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2943, -1504, 511, 0 }, { { -2, 1504, -2612, 0 }, { -2, -1504, -2612, 0 }, { -3, 1504, 2603, 0 }, { -3, -1504, 2603, 0 } }, { 4101, 0, 0, 0 }, { 0, 0, 0, 0 }, 3007, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3135, -1440, 479, 0 }, { { 0, 1504, 2608, 0 }, { 0, -1504, 2608, 0 }, { 0, 1504, -2608, 0 }, { 0, -1504, -2608, 0 } }, { -4105, 0, 0, 0 }, { 0, 0, 0, 0 }, 3007, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_west_elevator_hall_80185450[5] = {
    { NULL, NULL, NULL, { 5184, -48, 1152, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 1, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3936, -48, 1839, 0 }, { { 864, 0, -480, 0 }, { 864, 0, 481, 0 }, { -864, 0, -480, 0 }, { -864, 0, 481, 0 } }, { 0, 4097, 0, 0 }, { 201, 0, -4091, 0 }, 987, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -976, -76, 1840, 0 }, { { -1152, 0, -496, 0 }, { 1152, 0, -496, 0 }, { -1152, 0, 496, 0 }, { 1152, 0, 496, 0 } }, { 0, 4095, 0, 0 }, { 601, 0, -4052, 0 }, 1254, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2976, -71, 2688, 0 }, { { -1056, 0, -480, 0 }, { 1056, 0, -480, 0 }, { -1056, 0, 480, 0 }, { 1056, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { -402, 0, -4077, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5248, -64, -80, 0 }, { { -416, 0, -1392, 0 }, { 416, 0, -1392, 0 }, { -416, 0, 1392, 0 }, { 416, 0, 1392, 0 } }, { 0, 4120, 0, 0 }, { 4096, 0, 0, 0 }, 1448, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

SpriteBatch D_acropolis_west_elevator_hall_801855CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_west_elevator_hall_801855DC[26] = {
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -160, 24, 1125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -128, 24, 1200, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -104, 32, 1200, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 40, 64, 1800, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -104, 64, 1850, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -64, 40, 1868, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, 40, 1867, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -16, 40, 1866, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 8, 40, 1865, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -88, 40, 1867, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -104, 40, 1867, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 24, 16, 1912, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 24, -8, 1922, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 24, -40, 1956, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -72, -40, 1951, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -72, 16, 1919, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -72, -8, 1927, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -72, -32, 1956, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 32, 40, 1850, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 40, 24, 1831, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 40, 0, 1831, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 40, -24, 1831, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -104, 32, 1853, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -96, 16, 1858, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -96, 0, 1858, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -96, -24, 1858, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_west_elevator_hall_801857E4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 23, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_west_elevator_hall_80185804[63] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 495, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, 8, 1500, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 64, 637, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 80, 72, 637, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 72, 104, 625, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, 104, 625, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 72, 88, 625, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, 88, 625, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 72, 80, 628, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 112, 80, 628, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 104, 625, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 152, -120, 628, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 144, -80, 643, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 144, -40, 657, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, 136, -8, 688, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 136, 24, 698, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 136, 48, 711, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 136, 64, 683, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 152, 88, 640, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 144, 88, 612, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 136, 88, 625, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 128, 64, 775, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 128, 104, 637, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 120, 56, 850, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, 96, 850, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 112, 88, 882, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 112, 48, 882, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 104, 40, 956, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 104, 80, 956, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 96, 64, 1025, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 96, 32, 1045, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 88, 24, 1109, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 88, 56, 1093, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 80, 48, 1218, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 80, 16, 1284, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 80, -16, 1254, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 80, -48, 1228, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, 80, -96, 1205, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 48, -72, 1840, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 48, -48, 1916, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 48, -24, 1948, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 48, -8, 2375, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 48, 8, 2130, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 56, -8, 1888, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 56, 16, 1877, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 64, -8, 1702, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 64, 24, 1588, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 72, 8, 1397, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 72, 32, 1379, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, 24, 0, 2382, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, 24, -24, 2301, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 24 } }, 24, -48, 2356, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 24 } }, 24, -72, 2263, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 24 } }, -16, -64, 2385, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -48, -64, 2482, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -48, -40, 2527, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -48, -16, 2577, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -16, 0, 2504, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 0, -16, 2481, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 0, -24, 2451, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 8, -40, 2403, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 0, -40, 2437, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -16, -40, 2452, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_west_elevator_hall_80185CF0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 38, 0, 0, { 2, 0 } },
    { 49, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_west_elevator_hall_80185D18[60] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, -112, 678, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 120, -104, 841, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -88, 1107, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -56, 1129, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -40, 1213, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -16, 1222, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 8, 1202, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 120, -56, 745, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -40, 725, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 136, 0, 744, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 128, 24, 761, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 128, 40, 762, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 56, 782, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 32, 1292, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 32, 1266, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 32, 1041, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, 40, 932, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 56, 842, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 56, 1168, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 72, 1017, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 88, 901, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 96, 853, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, 72, 788, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 72, 722, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 88, 677, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 104, 629, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -16, -40, 1422, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -40, 1379, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, -40, 1343, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -24, 1413, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, -8, 1444, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, 8, 1465, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, 24, 1486, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 40, 1502, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, 40, 1435, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -24, 32, 1550, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -48, 32, 1560, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 16, 1598, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -24, 1561, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -64, -80, 1449, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -80, 1471, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -24, 1531, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 16, 1541, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 16, 1583, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -24, 1547, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, -80, 1436, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -16, -80, 1332, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 40, 1388, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 24, 1379, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 0, 1391, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -40, 1300, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 48, -80, 1250, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -88, 1085, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -40, 1198, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, 0, 1247, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 40, 1285, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 64, 40, 1427, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 64, 24, 1394, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 64, 0, 1269, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, -40, 1289, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_west_elevator_hall_801861C8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 1, 0 } },
    { 26, 34, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_west_elevator_hall_801861E8[26] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 40, 1003, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 48, 955, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 943, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 72, 873, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 873, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 874, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 48 } }, -160, -120, 571, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 128, 48 } }, -64, -120, 497, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 48 } }, 64, -120, 617, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 24 } }, -160, -72, 700, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 16 } }, -160, -48, 700, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 16 } }, -160, -32, 700, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, -160, -16, 700, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -160, 0, 750, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -160, 16, 775, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 24 } }, -160, 32, 775, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -160, 56, 775, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 24 } }, 64, -72, 700, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, 56, -48, 700, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, 56, -32, 700, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, 56, -16, 700, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, 48, 0, 750, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, 48, 16, 775, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 16 } }, 48, 32, 775, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, 48, 48, 775, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 48, 64, 775, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_west_elevator_hall_801863F0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_west_elevator_hall_80186408[5] = {
    { { .empty = D_acropolis_west_elevator_hall_801855CC }, D_acropolis_west_elevator_hall_801855CC, NULL },
    { { .elements = D_acropolis_west_elevator_hall_801855DC }, D_acropolis_west_elevator_hall_801857E4, NULL },
    { { .elements = D_acropolis_west_elevator_hall_80185804 }, D_acropolis_west_elevator_hall_80185CF0, NULL },
    { { .elements = D_acropolis_west_elevator_hall_80185D18 }, D_acropolis_west_elevator_hall_801861C8, NULL },
    { { .elements = D_acropolis_west_elevator_hall_801861E8 }, D_acropolis_west_elevator_hall_801863F0, NULL },
};

/// Authored point lights for model shading in every west elevator hall view.
///
/// Positions and inner/outer falloff radii use integer world units; RGB intensities
/// have 12 fractional bits (`ONE` is full intensity). The room overlay owns this
/// writable array: coordinate updates set its parent and cached transforms, and
/// lighting queries overwrite attenuation. Borrowed pointers expire on room unload.
static WorldCoordPointLight _gAcropolisWestElevatorHallPointLights[] = {
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4540, -1500, -1830 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 4840, -1500, 2900 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 5410, -1660, 2170 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 5410, -1660, -90 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 5020, -1760, 1040 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3031, 3112, 3276 },
        },
        .inner = 500,
        .outer = 2500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -3800, -1880, 2030 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 2048, 409, 409 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, -1960, 0 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 901, 983, 1146 },
        },
        .inner = 1000,
        .outer = 7000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -1000, -1340, 3000 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3686, 3358, 2867 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -3800, -370, 2030 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 2703, 2785, 2375 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -6310, -1650, 100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 1884, 2048, 2129 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -1700, -1650, -2800 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 1884, 2048, 2129 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 1690, -1650, -2420 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 1884, 2048, 2129 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 5550, -1650, -2190 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 1884, 2048, 2129 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -1000, -1640, 1700 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3686, 3358, 2867 },
        },
        .inner = 500,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 3000, -1440, 2500 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 500,
        .outer = 1500,
    },
};

WorldCoordRoomLights D_acropolis_west_elevator_hall_801869E4[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisWestElevatorHallPointLights), _gAcropolisWestElevatorHallPointLights, 0, NULL },
};

ViewCamera D_acropolis_west_elevator_hall_801869FC[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -100, 0x7530, -500 } }, 617 },
    { { { { -27, 0, 4095 }, { -657, 4042, -4 }, { -4042, -657, -27 } }, { -1918, 620, -769 } }, 257 },
    { { { { 792, 0, -4018 }, { -689, 4035, -135 }, { 3959, 702, 780 } }, { 3941, 2050, 660 } }, 230 },
    { { { { 1311, 0, -3880 }, { -766, 4015, -259 }, { 3803, 809, 1285 } }, { -338, 2340, 430 } }, 230 },
    { { { { 4095, 0, 0 }, { 0, 3401, -2281 }, { 0, 2281, 3401 } }, { 1000, 2820, 268 } }, 257 },
};

WorldCollisionFootstepSounds D_acropolis_west_elevator_hall_80186AB0 = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionSurfaceProperties D_acropolis_west_elevator_hall_80186ABC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_west_elevator_hall_80186AB0 },
};

WorldCollisionSurfaceProperties* D_acropolis_west_elevator_hall_80186AC4[8] = {
    D_acropolis_west_elevator_hall_80186ABC,
    D_acropolis_west_elevator_hall_80186ABC,
    D_acropolis_west_elevator_hall_80186ABC,
    D_acropolis_west_elevator_hall_80186ABC,
    D_acropolis_west_elevator_hall_80186ABC,
    D_acropolis_west_elevator_hall_80186ABC,
    D_acropolis_west_elevator_hall_80186ABC,
    D_acropolis_west_elevator_hall_80186ABC,
};

Task* D_acropolis_west_elevator_hall_80186AE4[2] = { 0 };

#include "../../shared/planar_reflection.inc.c"

void func_acropolis_west_elevator_hall_8017F304(Task* task)
{
    Reflection_PlayerTask(task);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

/// Runs the one-shot cutscene hand-off for the west elevator hall: once the
/// session reports state 8 == 1 the room spawns its scripted task pair, opens
/// the story flags for the elevator and marks the sequence as running; the
/// second block retires it again when the session goes idle.
///
/// `args` and the scratch block above it are dead here - the dispatch that
/// consumed them is gone - but the compiler still reserves and fills them, so
/// they have to stay for the frame layout to match.
static void func_acropolis_west_elevator_hall_8017F354(Task* task)
{
    s32 args[2] = { 0, 4 };
    u8  scratch[0x210];
    u8  sessionState;

    if (D_acropolis_west_elevator_hall_801849C8 == 0) {
        sessionState = gGameSession->location.loc.warp;
        if (sessionState == 1) {
            D_acropolis_west_elevator_hall_801849C8 = sessionState;
            func_800E8634(D_acropolis_west_elevator_hall_80184620, 0, D_acropolis_west_elevator_hall_80184890);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 1);
            gameFlagSetNibble(GAME_FLAG_STORY_CHAPTER, 1);
            func_800E3FAC(0xA2, 1);
        }
    }
    if (D_acropolis_west_elevator_hall_801849C8 == 1 && gGameSession->eventState == 0) {
        D_acropolis_west_elevator_hall_801849C8 = 2;
    }
}

/// Per-frame entry of an elevator-car task: runs the state its `state` field
/// selects from `D_acropolis_west_elevator_hall_8017D5E0` (set-up, travel,
/// then kill).
void func_acropolis_west_elevator_hall_8017F418(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_west_elevator_hall_8017D5E0;
    sp.funcs[task->state](task);
}

/// Sets both of the hall's elevator-car tasks moving forwards, by storing 1 in
/// each task's `spawnArg1` (the per-frame step direction the car task reads).
s32 func_acropolis_west_elevator_hall_8017F470(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    D_acropolis_west_elevator_hall_80186AE4[0]->spawnArg1.value = 1;
    D_acropolis_west_elevator_hall_80186AE4[1]->spawnArg1.value = 1;
    return 0;
}

/// Sets both elevator-car tasks moving backwards, by storing -1 in each task's
/// `spawnArg1`.
s32 func_acropolis_west_elevator_hall_8017F498(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    D_acropolis_west_elevator_hall_80186AE4[0]->spawnArg1.value = -1;
    D_acropolis_west_elevator_hall_80186AE4[1]->spawnArg1.value = -1;
    return 0;
}

s32 func_acropolis_west_elevator_hall_8017F4C0(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    if (src->areaId == GAME_AREA_ACROPOLIS_SQUARE && gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) == 0 && src->queryOnly == ROOM_EVENT_EXECUTE) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS, 1);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 1;
        dst->warp                                           = 7;
    }
    return 1;
}

s32 func_acropolis_west_elevator_hall_8017F560(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

static void func_acropolis_west_elevator_hall_8017F568(Task* arg0)
{
    arg0->msgTable = D_acropolis_west_elevator_hall_801849CC;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    D_acropolis_west_elevator_hall_80186AE4[0] =
        taskSpawnFromTable(D_acropolis_west_elevator_hall_80184568, 0, 0, -1);
    D_acropolis_west_elevator_hall_80186AE4[1] =
        taskSpawnFromTable(D_acropolis_west_elevator_hall_80184568, 1, 0, 1);
    arg0->state = (s32)(arg0->state + 1);
}

/// Per-frame entry of the room task: runs the state its `state` field selects
/// from `D_acropolis_west_elevator_hall_8017D5D4` (set-up, the cutscene
/// hand-off, then kill).
void func_acropolis_west_elevator_hall_8017F5F4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_west_elevator_hall_8017D5D4;
    sp.funcs[task->state](task);
}

/// Second state of the elevator task: allocates its scratch block, parks the
/// car model at its starting position and parents it to the room's view
/// coordinate system.
static void func_acropolis_west_elevator_hall_8017F64C(Task* task)
{
    TmdObject*                              extra;
    GfxCoord*                               coord;
    _AcropolisWestElevatorHallDoorLeafWork* work;

    extra = task->extra.tmd;
    coord = extra->coords;
    work  = memCalloc(sizeof(_AcropolisWestElevatorHallDoorLeafWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work          = work;
    work->travel        = 0;
    extra->flags        = 0;
    coord->parent       = &gGfxViewCoord;
    coord->coord.t[0]   = ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_X;
    coord->coord.t[1]   = -20;
    coord->coord.t[2]   = 0x974;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state++;
}

/// Fourth state of the elevator task: drives the car along its shaft from the
/// task's per-frame step, clamps the travel to [0, 0x2D0], and refreshes the
/// model's world matrix and lighting from the resulting position.
static void func_acropolis_west_elevator_hall_8017F6F0(Task* task)
{
    VECTOR                                  pos;
    TmdObject*                              extra;
    GfxCoord*                               coord;
    _AcropolisWestElevatorHallDoorLeafWork* work;

    work  = task->work;
    extra = task->extra.tmd;
    coord = extra->coords;

    work->travel += task->spawnArg1.value * ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_SPEED;
    if (work->travel < 0) {
        work->travel = 0;
    }
    if (work->travel > ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX) {
        work->travel = ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX;
    }
    coord->coord.t[0] = (work->travel * task->spawnArg2.value) + ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_X;
    if (gGameSession->location.loc.view == 5) {
        extra->flags = 0;
    } else {
        extra->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    worldCoordSetModelLighting(extra, &pos, 0, 3);
}

/// Third state of the elevator task: on the two session phases that use it,
/// spawns the lift's ambient effects around the room's coordinate system.
void func_acropolis_west_elevator_hall_8017F7D4(Task* task)
{
    SVECTOR   pos;
    SVECTOR   altPos;
    GfxCoord* coord;

    coord = task->extra.coordBody->coord;
    switch (task->state) {
        case 0:
            task->msgTable = D_acropolis_west_elevator_hall_801849F4;
            gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
            Task_Spawn(1, 0x25, 0, 0);
            Task_Spawn(1, 0x25, 1, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (gGameSession->location.loc.view == 2) {
                pos = D_acropolis_west_elevator_hall_8017D5EC;
                Gp_SpawnEff(EFFECT_ACROPOLIS_WEST_ELEVATOR_HALL_RED_BEACON, coord, RED_BEACON_ARG(4, 0x18), &pos);
                pos.vx = -0x1800;
                pos.vy = -0x4F0;
                pos.vz = -0x600;
                Gp_SpawnEff(EFFECT_ACROPOLIS_WEST_ELEVATOR_HALL_RED_BEACON, coord, RED_BEACON_ARG(3, 0x8), &pos);
                pos.vx = -0x1800;
                pos.vy = -0x4F0;
                pos.vz = -0x2C0;
                Gp_SpawnEff(EFFECT_ACROPOLIS_WEST_ELEVATOR_HALL_RED_BEACON, coord, RED_BEACON_ARG(3, 0x8), &pos);
            }
            if (gGameSession->location.loc.view == 5) {
                altPos = D_acropolis_west_elevator_hall_8017D5F4;
                Gp_SpawnEff(EFFECT_ACROPOLIS_WEST_ELEVATOR_HALL_LIGHT, coord, 0, &altPos);
            }
            return;
    }
}

/// Per-frame update of the lift bay's lighting: ramps `EffectWork::scale`
/// from 0 to 0x1000 in 0x800 steps, re-blending the bay CLUT towards its lit
/// palette on every step it takes, and latching `angle` once the ramp is
/// full. On every session phase but 5 the CLUT is then blended straight back
/// to the unlit palette and the effect's work object is released, so only
/// phase 5 keeps the lit bay on screen.
void func_acropolis_west_elevator_hall_8017F990(Task* task)
{
    EffectWork* work;
    s32         i;
    s32         blend;

    work  = (EffectWork*)task->spawnArg2.pointer;
    blend = 0;
    if (work->angle == 0) {
        work->scale = work->scale + 0x800;
        if (work->scale == 0x1000) {
            work->angle = 1;
        }
        blend = 1;
    }

    if (blend != 0) {
        for (i = 0; i < 0x100; i += 0x10) {
            Gp_BlendRgb555Clut(&D_acropolis_west_elevator_hall_80184C04[i],
                               &D_acropolis_west_elevator_hall_80184A04[i], work->scale,
                               &D_acropolis_west_elevator_hall_80184E04[i]);
        }
        Gp_LoadImages(D_acropolis_west_elevator_hall_80185004);
    }

    if (gGameSession->location.loc.view != 5) {
        for (i = 0; i < 0x100; i += 0x10) {
            Gp_BlendRgb555Clut(&D_acropolis_west_elevator_hall_80184C04[i],
                               &D_acropolis_west_elevator_hall_80184A04[i], 0,
                               &D_acropolis_west_elevator_hall_80184E04[i]);
        }
        Gp_LoadImages(D_acropolis_west_elevator_hall_80185004);
        effectKillTask(work, task);
    }
}

#include "../../shared/red_beacon_task.inc.c"

/// Copies 82 scanlines through DR_MOVE packets, applying a horizontal cosine
/// distortion to a 120-pixel-wide strip of the current frame buffer. During
/// alternate 128-frame intervals, each row uses a random divisor for the
/// displacement. The packets share OT slot 0x72, then the task retires.
void func_acropolis_west_elevator_hall_8017FE18(Task* task)
{
    RECT     rect;
    DR_MOVE* mv;
    void*    mem;
    s32      otIndex;
    s32      i;
    s32      base;
    s32      x;
    s32      y;
    s32      t;

    mem  = task->spawnArg2.pointer;
    base = gDisplayState.drawBuffer * 0x110 + 0x50;

    for (i = 0; i < 0x52; i++) {
        otIndex = 0x72;
        y       = i;
        y      += base;
        t       = 0x800 - rcos((gDisplayState.animFrame + i * 2) * 16);
        if (gDisplayState.animFrame & 0x80) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            x               = t / (s32)(((gRandomLcgState >> 16) & 0x3F) + 0xC0) + 0x50;
        } else {
            x = t / 0x100 + 0x50;
        }

        rect.x         = x;
        rect.y         = y;
        rect.w         = 0x78;
        rect.h         = 1;
        mv             = gGpuPrimCursor;
        gGpuPrimCursor = mv + 1;
        SetDrawMove(mv, &rect, 0x50, i + base);

        addPrim(&gGpuCurrentOt[otIndex], mv);
    }

    effectKillTask(mem, task);
}

/// Draws one frame of the hall's soft light billboard and then retires the
/// task. The effect coordinate is projected through `GsWSMATRIX` with a
/// single `RTPS`, and the resulting screen point becomes the centre of
/// a semi-transparent `POLY_FT4` whose half-extent shrinks with distance
/// (`0x6700 / otz`). Sprites closer than `otz == 0x11` are skipped entirely,
/// which is why the primitive is claimed from `gGpuPrimCursor` before the
/// depth test but only filled in and linked afterwards.
void func_acropolis_west_elevator_hall_8017FFE4(Task* arg0)
{
    RoomGlowSpriteScratch* block;
    GfxCoord*              coord;
    void*                  mem;
    POLY_FT4*              prim;

    coord = arg0->extra.coordBody->coord;
    mem   = arg0->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    block              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
    block->worldPos.vx = coord->workm.t[0];
    block->worldPos.vy = coord->workm.t[1];
    block->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        prim->tpage       = 0xAB;
        prim->clut        = 0x4380;
        prim->u0          = 0;
        prim->v0          = 0;
        prim->u1          = 0x67;
        prim->v1          = 0;
        prim->u2          = 0;
        prim->v2          = 0x67;
        prim->u3          = 0x67;
        prim->v3          = 0x67;
        prim->code       |= 3;
        block->halfExtent = 0x6700 / block->otz;
        prim->x0 = prim->x2 = block->screenPos.vx - block->halfExtent;
        prim->x1 = prim->x3 = block->screenPos.vx + block->halfExtent;
        prim->y0 = prim->y1 = block->screenPos.vy - block->halfExtent;
        prim->y2 = prim->y3 = block->screenPos.vy + block->halfExtent;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    effectKillTask(mem, arg0);
}

s32 func_acropolis_west_elevator_hall_80180274(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    Gp_SpawnEff(EFFECT_ACROPOLIS_WEST_ELEVATOR_BAY_LIGHTS, NULL, 0, NULL);
    return 0;
}
