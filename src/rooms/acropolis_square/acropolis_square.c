#include "rooms/acropolis_square.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_cutscene.h"

extern UiObjectDesc D_800611E4;

/// Index of the mirrored player's coordinate part each held-object reflection
/// is parented to, by `Task::spawnArg1`.
static u8 Reflection_Data_8017FC8C[];

/// Task descriptor of the held-object reflections the mirror spawns.
static TaskDesc D_acropolis_square_80183468[];

/// Labels of the four menu entries of the play-data menu panel: "Save",
/// "Play Data", "Weapon Data" and "PE Data".
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Row labels of the play-data statistics panel, one per row
/// `func_acropolis_square_8017F46C` draws.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// The suffix appended to that panel's count rows.
static u8 Telephone_Data_80181A70[];

/// The "%" suffix the room's percentage formatters append.
static u8 Telephone_Data_80181A78[];

/// Help texts of the statistics panel's nine rows, handed to the UI holder for
/// the selected row.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// The play-data menu panel's list.
static UiList Telephone_Data_80181C44;

/// The usage panel's list.
static UiList Telephone_Data_80181C6C;

/// UI descriptor the play-data panels spawn when they first open.
static UiObjectDesc Telephone_Data_80181C90;

/// UI descriptors the "Play Data" entry and the two usage entries open.
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;

/// The telephone menu panel's list.
static UiList Telephone_Data_80181CF4;

/// Task descriptor table of the room's cutscenes: entry 0 is the cutscene
/// runner, spawned with a cutscene record as its argument, and entry 1 the
/// sound task the runner spawns for the scene.
extern TaskDesc gRoomCutsceneTaskDescs[];

/// The room's message table, installed on the room entry task.
extern TaskMessageEntry D_acropolis_square_801837C4[];

extern TaskDesc         D_acropolis_square_80183808[];
extern s32              D_acropolis_square_8018382C;
extern s32              D_acropolis_square_80183830;
extern EvsCommand       D_acropolis_square_80183834[];
extern EvsCommand       D_acropolis_square_8018399C[];
extern EvsCommand       D_acropolis_square_801838DC[];
extern EvsCommand       D_acropolis_square_80183A5C[];
extern s32              D_acropolis_square_80183B34[];
extern TaskMessageEntry D_acropolis_square_80183B58[2];
extern s16              D_acropolis_square_80183B68[];
extern s32              D_acropolis_square_80183B98;

/// The area records applied when a scene ends with game-flag nibble 0x7A at 1,
/// nibble 0 at 2 and the save's location at 0x0101 in its upper half.

extern s32   D_acropolis_square_80188898;
extern Task* D_acropolis_square_8018889C;
extern s32   D_acropolis_square_801888A0;
extern s32   D_acropolis_square_801888A4;

/// The scene sub-task while it runs, NULL otherwise.
extern Task* gRoomCutsceneSoundTask;

/// The cutscene record the room hands entry 0 of `gRoomCutsceneTaskDescs`.
extern RoomCutsceneRecStorage D_acropolis_square_801888AC;

extern GfxCoord D_acropolis_square_801888CC;

/// Defines the reflection scale at the shared implementation's include position.
///
/// 1 lets `planar_reflection.inc.c` include `planar_reflection_rodata.inc.c`.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 1
#include "../../shared/planar_reflection.h"

#define TELEPHONE_TITLE_BYTES "Telephone\0\xDC\xDD"
#include "../../shared/telephone.h"

static void func_acropolis_square_80182260(Task* task);
static void func_acropolis_square_801822A4(Task* task);

s32        func_acropolis_square_80181794(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_acropolis_square_801819BC(Task*, s32, s32, s32);
s32        func_acropolis_square_801820D8(Task* task, s32 msgId, const void* firstArg, s32 arg3);
static s32 _acropolisSquareRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
s32        func_acropolis_square_80182110(Task*, s32, s32, s32);
void       func_acropolis_square_80181AEC(Task*);
void       func_acropolis_square_80181DD0(Task*);
void       func_acropolis_square_80182148(Task*);
void       func_acropolis_square_80182200(s32);

extern WorldCollisionGrid         D_acropolis_square_8018519C[1];
extern WorldCollisionTrigger      D_acropolis_square_801851C0[16];
extern WorldCollisionTrigger      D_acropolis_square_80185680[26];
extern WorldCoordRoomAmbientEntry D_acropolis_square_80186480[16];
extern WorldCoordRoomLights       D_acropolis_square_80186468[1];

#include "../../shared/planar_reflection_data.inc.c"

static TaskDesc D_acropolis_square_80183468[2] = {
    { { { TASK_BODY_NONE, 112 } }, acropolisSquarePlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 112 } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// Borrows this overlay's two reflection task descriptors.
///
/// Slot 0 spawns the player reflection; slot 1 spawns an attachment or equipment
/// reflection. There is no terminator. The table and its callbacks remain valid
/// while the overlay is loaded; the caller neither owns nor copies the table.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return D_acropolis_square_80183468;
}

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

enum { ACROPOLIS_SQUARE_MESSAGE_USE_KEY_ITEM = 5105 };

TaskMessageEntry D_acropolis_square_801837C4[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_square_80181794 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_square_801820D8 },
    { ACROPOLIS_SQUARE_MESSAGE_USE_KEY_ITEM, _acropolisSquareRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, func_acropolis_square_801819BC },
    { ROOM_MESSAGE_SOUND, func_acropolis_square_80182110 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationPlayRequest D_acropolis_square_801837F4 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

TaskDesc D_acropolis_square_80183808[3] = {
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_square_80181AEC, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_square_80182148, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_square_80181DD0, { .value = 0 } },
};

s32 D_acropolis_square_8018382C = 0;

s32 D_acropolis_square_80183830 = 0;

EvsCommand D_acropolis_square_80183834[7] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM_EFFECT }, { .value = 0 }, { .value = 3103 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5101000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 148 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_square_801838DC[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_square_8018399C[8] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_square_801837F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_square_80182200 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 395 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_square_80183A5C[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_square_80182200 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 D_acropolis_square_80183B34[9] = {
    0,
    0x51010001,
    0,
    0,
    0,
    0x51010005,
    0x51010006,
    0x51010007,
    0x51010008,
};

s32 func_acropolis_square_8018344C(Task*, s32, s32, s32);

TaskMessageEntry D_acropolis_square_80183B58[2] = {
    { 3103, func_acropolis_square_8018344C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s16 D_acropolis_square_80183B68[24] = {
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    3784,
    4096,
    3784,
    2896,
    1567,
    0,
    -1567,
    -2896,
    -3784,
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    0,
};

s32 D_acropolis_square_80183B98 = 0;

WorldCollisionRoomResources D_acropolis_square_80183B9C[1] = {
    { D_acropolis_square_8018519C, D_acropolis_square_801851C0, D_acropolis_square_80185680, NULL },
};

u8* D_acropolis_square_80183BAC[1] = {
    gViewIdentityMap,
};

ViewCount D_acropolis_square_80183BB0[1] = { 15 };

WorldCoordRoomLighting D_acropolis_square_80183BB4[1] = {
    { D_acropolis_square_80186468, D_acropolis_square_80186480 },
};

DirectionWarpEntry D_acropolis_square_80183BBC[7] = {
    { { { .word = 1024 }, -6510, -2136, 511 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6510, -2136, 511 }, { 0, 0, 0, 0 }, 0x51010003, 0x51010002, 0x51010004, 13, DIRECTION_WARP_FLAG_NONE, 503 },
    { { { .word = 3072 }, 6513, -2138, 140 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 6513, -2138, 140 }, { 0, 0, 0, 0 }, 0x51010003, 0x51010002, 0x51010004, 6, DIRECTION_WARP_FLAG_NONE, 502 },
    { { { .word = 256 }, -4227, -1535, -3725 }, { 0, 0, 0, 0 }, { { .word = 256 }, -4227, -1535, -3725 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3840 }, 4199, -1535, -3916 }, { 0, 0, 0, 0 }, { { .word = 3840 }, 4199, -1535, -3916 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 256 }, -4703, -1000, -4644 }, { 0, 0, 0, 0 }, { { .word = 256 }, -4419, -1200, -4340 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3840 }, 4905, -1000, -4576 }, { 0, 0, 0, 0 }, { { .word = 3840 }, 4481, -1240, -4416 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -6510, -2136, 511 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6510, -2136, 511 }, { 0, 0, 0, 0 }, 0x51010003, 0x51010002, 0x51010004, 15, DIRECTION_WARP_FLAG_NONE, 503 },
};

static SVECTOR _gAcropolisSquareCollision07BDCNormals[82] = {
#include "assets/acropolis_square_collision_07BDC_normals.inc"
};

static SVECTOR _gAcropolisSquareCollision07BDCVerts[262] = {
#include "assets/acropolis_square_collision_07BDC_verts.inc"
};

static WorldCollisionGridFace _gAcropolisSquareCollision07BDCFaces[115] = {
#include "assets/acropolis_square_collision_07BDC_faces.inc"
};

static s16 _gAcropolisSquareCollision07BDCCells[506] = {
#include "assets/acropolis_square_collision_07BDC_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisSquareCollision07BDCCells[i])
static s16* _gAcropolisSquareCollision07BDCTable[16] = {
#include "assets/acropolis_square_collision_07BDC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_square_8018519C[1] = {
    { NULL, _gAcropolisSquareCollision07BDCNormals, _gAcropolisSquareCollision07BDCVerts, _gAcropolisSquareCollision07BDCFaces, _gAcropolisSquareCollision07BDCTable, 7000, 8000, 4, 4, 4000, 115 },
};

WorldCollisionTrigger D_acropolis_square_801851C0[16] = {
    { NULL, NULL, NULL, { -2050, -3681, 2655, 0 }, { { -1080, 2717, 3957, 0 }, { 1070, 2724, -3953, 0 }, { -1074, -2723, 3949, 0 }, { 1075, -2716, -3960, 0 } }, { 3952, 2, 1074, 0 }, { 0, 4096, 0, 0 }, 4910, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2253, -3648, -4067, 0 }, { { -1346, 2813, 704, 0 }, { 1332, 2628, -777, 0 }, { -1339, -2627, 707, 0 }, { 1336, -2812, -780, 0 } }, { 1986, 4, 3590, 0 }, { 0, 4096, 0, 0 }, 3197, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1682, -3744, -3572, 0 }, { { 1535, 2813, 886, 0 }, { -1565, 2628, -890, 0 }, { 1535, -2627, 883, 0 }, { -1563, -2812, -896, 0 } }, { 2045, 1, -3574, 0 }, { 0, 4096, 0, 0 }, 3328, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2267, -3584, -3843, 0 }, { { 1497, 2813, -816, 0 }, { -1503, 2628, 766, 0 }, { 1486, -2627, -817, 0 }, { -1510, -2812, 762, 0 } }, { -1917, 3, -3632, 0 }, { 0, 4096, 0, 0 }, 3278, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4159, -3648, -770, 0 }, { { 3327, 2718, 116, 0 }, { -3328, 2723, -122, 0 }, { 3321, -2722, 116, 0 }, { -3335, -2717, -124, 0 } }, { 146, -1, -4110, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4222, -3648, -1250, 0 }, { { -3337, 2718, -120, 0 }, { 3322, 2723, 111, 0 }, { -3331, -2722, -117, 0 }, { 3329, -2717, 111, 0 } }, { -143, 1, 4111, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1635, -3712, 3102, 0 }, { { 763, 2718, -3250, 0 }, { -772, 2723, 3235, 0 }, { 761, -2722, -3245, 0 }, { -772, -2717, 3241, 0 } }, { -4005, 0, -949, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1660, -3616, 2814, 0 }, { { 531, 2718, 3289, 0 }, { -533, 2723, -3287, 0 }, { 531, -2722, 3285, 0 }, { -535, -2717, -3293, 0 } }, { 4060, 0, -658, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2269, -3680, 2750, 0 }, { { -697, 2718, -3264, 0 }, { 688, 2723, 3252, 0 }, { -696, -2722, -3259, 0 }, { 690, -2717, 3258, 0 } }, { -4025, 0, 855, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4579, -2464, -2338, 0 }, { { -4100, 2717, 117, 0 }, { 4094, 2724, -120, 0 }, { -4094, -2723, 119, 0 }, { 4100, -2716, -116, 0 } }, { 118, 1, 4094, 0 }, { 0, 4096, 0, 0 }, 4910, 0, 8, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4675, -2432, -2050, 0 }, { { 4101, 2717, -116, 0 }, { -4094, 2724, 120, 0 }, { 4094, -2723, -119, 0 }, { -4100, -2716, 117, 0 } }, { -119, 2, -4095, 0 }, { 0, 4096, 0, 0 }, 4910, 0, 4, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5648, -3456, 1024, 0 }, { { -859, 2717, 326, 0 }, { 850, 2724, -323, 0 }, { -848, -2723, 324, 0 }, { 859, -2716, -324, 0 } }, { 1460, 0, 3847, 0 }, { 0, 4096, 0, 0 }, 2862, 0, 8, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5249, -3488, -211, 0 }, { { 406, 2717, 983, 0 }, { -403, 2724, -972, 0 }, { 404, -2723, 973, 0 }, { -406, -2716, -981, 0 } }, { 3791, 1, -1570, 0 }, { 0, 4096, 0, 0 }, 2918, 0, 8, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5459, -3552, 1054, 0 }, { { 955, 2717, -367, 0 }, { -946, 2724, 365, 0 }, { 946, -2723, -364, 0 }, { -954, -2716, 368, 0 } }, { -1478, 0, -3835, 0 }, { 0, 4096, 0, 0 }, 2896, 0, 13, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5154, -3488, -306, 0 }, { { -482, 2717, -1106, 0 }, { 480, 2724, 1096, 0 }, { -480, -2723, -1096, 0 }, { 482, -2716, 1105, 0 } }, { -3758, 1, 1640, 0 }, { 0, 4096, 0, 0 }, 2974, 0, 13, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1696, -3648, -3826, 0 }, { { -1574, 2813, -843, 0 }, { 1560, 2628, 827, 0 }, { -1574, -2627, -836, 0 }, { 1560, -2812, 836, 0 } }, { -1936, 4, 3632, 0 }, { 0, 4096, 0, 0 }, 3318, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_square_80185680[26] = {
    { NULL, NULL, NULL, { -6416, -2240, 0, 0 }, { { -432, 0, -1088, 0 }, { 432, 0, -1088, 0 }, { -432, 0, 1088, 0 }, { 432, 0, 1088, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 17, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6320, -2237, -64, 0 }, { { -480, 0, -1024, 0 }, { 480, 0, -1024, 0 }, { -480, 0, 1024, 0 }, { 480, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4161, -1664, -3873, 0 }, { { 489, 0, -473, 0 }, { 672, 0, 40, 0 }, { -671, 0, -39, 0 }, { -488, 0, 474, 0 } }, { 0, 4096, 0, 0 }, { 1189, 0, 3920, 0 }, 680, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 70, 144, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4286, -1667, -3969, 0 }, { { 704, 0, -39, 0 }, { 521, 0, 473, 0 }, { -519, 0, -473, 0 }, { -703, 0, 40, 0 } }, { 0, 4105, 0, 0 }, { -1380, 0, 3857, 0 }, 704, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 70, 112, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4528, -2242, -1728, 0 }, { { 2128, 0, -176, 0 }, { 2128, 0, 176, 0 }, { -2128, 0, -176, 0 }, { -2128, 0, 176, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 2126, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 67, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4753, -1632, -2481, 0 }, { { 2188, 0, -166, 0 }, { 2197, 0, 186, 0 }, { -2196, 0, -185, 0 }, { -2187, 0, 167, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 2202, WORLD_COLLISION_TRIGGER_ACTION_FACING, 67, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4560, -2243, -1765, 0 }, { { 2064, 0, -176, 0 }, { 2064, 0, 176, 0 }, { -2064, 0, -176, 0 }, { -2064, 0, 176, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 2063, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 67, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4574, -1638, -2432, 0 }, { { 2064, 0, -224, 0 }, { 2064, 0, 224, 0 }, { -2064, 0, -224, 0 }, { -2064, 0, 224, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 2063, WORLD_COLLISION_TRIGGER_ACTION_FACING, 67, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4433, -1376, -4385, 0 }, { { -1197, 1024, 433, 0 }, { 1198, 1024, -433, 0 }, { -1197, -1024, 433, 0 }, { 1198, -1024, -433, 0 } }, { 1399, 0, 3869, 0 }, { 1567, 0, 3784, 0 }, 1634, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 3, 52, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4494, -1376, -4418, 0 }, { { -1141, 1024, -417, 0 }, { 1142, 1024, 418, 0 }, { -1141, -1024, -417, 0 }, { 1142, -1024, 418, 0 } }, { -1410, 0, 3848, 0 }, { -1257, 0, 3898, 0 }, 1588, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 9, 68, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 52, -1680, -3974, 0 }, { { 1496, 0, -1411, 0 }, { 1110, 0, 439, 0 }, { -1444, 0, -1415, 0 }, { -1127, 0, 436, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 2048, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 168, -2272, -80, 0 }, { { 888, 0, 192, 0 }, { 1112, 0, 1601, 0 }, { -839, 0, 192, 0 }, { -1159, 0, 1601, 0 } }, { 0, 4098, 0, 0 }, { -201, 0, 4091, 0 }, 1974, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 0, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6272, -2328, 2400, 0 }, { { -912, 0, -736, 0 }, { 912, 0, -736, 0 }, { -912, 0, 736, 0 }, { 912, 0, 736, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4464, -1200, -4609, 0 }, { { 888, 0, -537, 0 }, { 1040, 0, -152, 0 }, { -1040, 0, 153, 0 }, { -888, 0, 538, 0 } }, { 0, 4103, 0, 0 }, { -1380, 0, -3857, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_FACING | WORLD_COLLISION_TRIGGER_AUTOMATIC, 67, 16, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4689, -1152, -4801, 0 }, { { -1054, 0, -113, 0 }, { -858, 0, -621, 0 }, { 858, 0, 621, 0 }, { 1054, 0, 113, 0 } }, { 0, 4102, 0, 0 }, { 1567, 0, -3784, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_FACING | WORLD_COLLISION_TRIGGER_AUTOMATIC, 67, 240, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3568, -2304, 3600, 0 }, { { 1680, 0, -1312, 0 }, { 1680, 0, 1473, 0 }, { -1679, 0, -1472, 0 }, { -1679, 0, 1313, 0 } }, { 0, 4108, 0, 0 }, { -201, 0, 4091, 0 }, 2231, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -1552, -2272, -448, 0 }, { { -1376, 0, -1088, 0 }, { 768, 0, -1088, 0 }, { -1120, 0, 1184, 0 }, { 768, 0, 1376, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1750, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1536, -2272, -640, 0 }, { { -1952, 0, -672, 0 }, { 1248, 0, -672, 0 }, { -1280, 0, 1568, 0 }, { 1248, 0, 1408, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 2063, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -769, -1696, -6753, 0 }, { { -943, 0, 302, 0 }, { 1265, 0, -16, 0 }, { -560, 0, 1105, 0 }, { 1040, 0, 787, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5577, -2272, -1575, 0 }, { { -824, 0, -504, 0 }, { 1288, 0, -504, 0 }, { -856, 0, 1000, 0 }, { 1288, 0, 1480, 0 } }, { 0, 4103, 0, 0 }, { 4096, 0, 0, 0 }, 1958, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1104, -2304, 5136, 0 }, { { -992, 0, -848, 0 }, { 672, 0, -848, 0 }, { -992, 0, 80, 0 }, { 672, 0, 80, 0 } }, { 0, 4107, 0, 0 }, { 4017, 0, -799, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 13, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1824, -2240, 3801, 0 }, { { 3504, 0, -2168, 0 }, { 3504, 0, 2249, 0 }, { -3503, 0, -1720, 0 }, { -3503, 0, 1641, 0 } }, { 0, 4102, 0, 0 }, { -201, 0, 4091, 0 }, 4159, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -5281, -1640, -2401, 0 }, { { -991, 0, -568, 0 }, { 623, 0, -972, 0 }, { -696, 0, 744, 0 }, { 1066, 0, 797, 0 } }, { 0, 4117, 0, 0 }, { 3513, 0, -2106, 0 }, 1330, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -128, -1664, -3296, 0 }, { { 7440, 0, -176, 0 }, { 7440, 0, 176, 0 }, { -7440, 0, -176, 0 }, { -7440, 0, 176, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 7437, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 14, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -1664, -4784, 0 }, { { 1248, 0, -640, 0 }, { 1248, 0, 641, 0 }, { -1247, 0, -640, 0 }, { -1247, 0, 641, 0 } }, { 0, 4102, 0, 0 }, { 201, 0, -4091, 0 }, 1402, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 192, -2252, -1664, 0 }, { { 7248, 0, -304, 0 }, { 7248, 0, 272, 0 }, { -7248, 0, -272, 0 }, { -7248, 0, 304, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 7240, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 16, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_square_80185E38[2] = {
    { 19, 118, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_111800_8013A468 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_square_80185E50[3] = {
    { NULL, NULL },
    { D_map_akropolis_8017ACBC, D_acropolis_square_80185E38 },
    { NULL, NULL },
};

/// The square's 16 point lights, contributing to model lighting in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities use
/// 12 fractional bits (`ONE` is full intensity). The loaded room overlay owns
/// these writable records: room updates parent and compose their transforms,
/// and lighting queries overwrite attenuation. Borrowed pointers must not
/// survive unloading the overlay.
static WorldCoordPointLight _gAcropolisSquarePointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3005, -4865, 2030 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3031, 2949, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 10,
        .outer = 8250,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5380, -2055, -3345 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2375, 2293, 2211 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 3500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5360, -2055, -3345 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2375, 2293, 2211 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 3500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4395, -1475, -4140 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1884, 1802, 1720 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 750,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4405, -1475, -4140 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1884, 1802, 1720 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 750,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5104, -900, -5740 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2703, 2621, 2539 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5037, -900, -5783 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2703, 2621, 2539 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6430, -4865, 5 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3276, 3194, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 650,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6445, -4865, 5 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3276, 3194, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 650,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3108, -4865, 2030 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3031, 2949, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 10,
        .outer = 8250,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1960, -4265, -5722 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2949, 2949, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 10,
        .outer = 7500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1970, -4265, -5722 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3031, 3031, 2949 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 10,
        .outer = 7500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4925, -1925, -2165 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2211, 2129, 2048 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1250,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5090, -1925, -2165 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2211, 2129, 2048 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1250,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2869, -1925, -2165 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2211, 2129, 2048 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1250,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2805, -1925, -2165 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2211, 2129, 2048 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1250,
    },
};

WorldCoordRoomLights D_acropolis_square_80186468[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisSquarePointLights), _gAcropolisSquarePointLights, 0, NULL },
};

WorldCoordRoomAmbientEntry D_acropolis_square_80186480[16] = {
    { .viewCount = ARRAY_SIZE(D_acropolis_square_80186480) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

SpriteBatch D_acropolis_square_80186500[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_square_80186510[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_square_80186520[73] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -80, 2750, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 2750, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 0, 2750, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 0, 2875, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 8, 2450, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 0, 2450, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 8, 2450, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 40, 1550, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, 32, 1575, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 32, 1600, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 32, 1625, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 24, 1650, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 24, 1675, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 16, 1700, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -104, 16, 1725, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 40, 1750, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 48, 625, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 48, 625, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 56, 625, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 48, 625, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -104, 88, 625, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -80, 64, 625, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 88, 625, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 88, 500, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 56, 500, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, 64, 625, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -24, 88, 625, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 56, 625, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 72, 625, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 24, 72, 625, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, 72, 625, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 88, 64, 625, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 120, 64, 625, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 64, 625, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 104, 625, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 104, 625, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 56, 625, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 40, 1650, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 40, 1650, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, 16, 1750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 16, 1725, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 32, 1700, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 32, 1675, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 24, 2475, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 24, 2425, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -32, 24, 2325, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -8, 32, 2312, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 16, 32, 2312, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 24, 2375, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 56, 32, 2425, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -8, 2500, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 16, 2500, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, 16, 2450, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -16, 2450, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -24, 16, 2425, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 16, 2400, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -24, -16, 2425, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -24, 2400, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -16, 2425, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, 16, 2425, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, 16, 2450, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -8, 2500, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -16, 2450, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 16, 2500, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -24, 2450, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -24, 2450, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -64, 2450, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, -64, 2450, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -112, 2450, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -88, 2450, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -88, 2450, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, -120, 2450, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -120, 2450, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_square_80186AD4[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { 4, 3, 0, 0, { 5, 0 } },
    { 7, 9, 0, 0, { 4, 0 } },
    { 16, 11, 0, 0, { 6, 0 } },
    { 27, 10, 0, 0, { 1, 0 } },
    { 37, 6, 0, 0, { 7, 0 } },
    { 43, 7, 0, 0, { 3, 0 } },
    { 50, 14, 0, 0, { 8, 0 } },
    { 64, 9, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_square_80186B2C[41] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 64, 1337, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 64, 1325, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 64, 1175, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 64, 1180, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 48, 1180, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 64, 1212, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 40, 1212, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 40, 1225, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 64, 1225, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 32, 1337, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 32, 1325, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, 24, 1375, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 64, 1375, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 112, 875, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 104, 875, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 112, 875, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 96, 875, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 875, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -40, 88, 875, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 72, 875, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 80, 875, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 88, 875, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 48, 1556, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 112, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 104, 1337, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 104, 1300, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 104, 1300, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 104, 1250, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 88, 1325, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 88, 1312, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 88, 1350, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 88, 1325, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 72, 1412, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 72, 1406, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, 72, 1356, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 64, 1506, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 56, 1562, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 64, 1530, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 56, 1562, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 48, 1706, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 40, 1712, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_square_80186E60[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 9, 0, 0, { 2, 0 } },
    { 22, 19, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_square_80186E88[31] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, 80, 1118, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 80, 1106, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 80, 1112, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 64, 1112, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, 80, 1115, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 56, 1115, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 80, 1117, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, 48, 1117, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, 40, 1118, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 40, 1120, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 80, 1120, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 24, 1120, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 112, 937, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 72, 937, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, 88, 937, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 104, 937, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 96, 1275, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 104, 1200, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 104, 1200, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 104, 1200, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 104, 1200, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 88, 1300, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 88, 1300, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 88, 1300, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 80, 1300, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 80, 1350, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 80, 1350, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 72, 1400, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 72, 1375, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 64, 1450, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 1450, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_square_801870F4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 1, 0 } },
    { 12, 4, 0, 0, { 2, 0 } },
    { 16, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_square_8018711C[91] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 32, 1487, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 40, 1500, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 72, 1450, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 40, 1450, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 24, 1450, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 40, 1487, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 72, 1487, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 16, 1500, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 16, 1500, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 24, 1500, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 48, 1350, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 48, 1350, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -72, 32, 1475, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 32, 1500, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -72, 24, 1500, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -32, 56, 1375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -32, 48, 1400, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, 40, 1425, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, 72, 1350, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -104, 56, 1362, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -72, 56, 1362, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -40, 56, 1362, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 48, 1375, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 48, 1375, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, 48, 1375, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 88, 1275, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -128, 88, 1275, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 72, 1275, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, 72, 1275, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, 72, 1275, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 56, 1275, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -128, 64, 1275, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 64, 1275, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, 40, 1275, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, 104, 775, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -48, 104, 775, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -8, 104, 775, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, 104, 775, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 88, 800, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -48, 88, 812, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -8, 88, 812, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 88, 812, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 72, 862, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -48, 72, 862, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 72, 862, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -48, 64, 875, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, 64, 875, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 64, 875, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 48, 1225, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 120, 72, 1212, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 56, 1212, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 56, 1212, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 40, 1225, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 120, 48, 1225, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 48, 3000, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 16, 56, 2875, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 48, 56, 2875, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 80, 56, 2875, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 56, 2875, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 112, 56, 2875, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 16, 40, 3000, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, 40, 3000, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, 40, 3000, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, 40, 3000, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 32, 3125, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 48, 32, 3125, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 32, 3125, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 24, 3250, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 24, 3250, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 0, 2762, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -40, 2762, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, -48, 2762, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, 0, 2762, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, -40, 2762, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, -48, 2750, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, -48, 2762, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 0, -40, 2925, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 24, 2925, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, 0, 2925, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, -40, 2925, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, -48, 2925, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, 0, 2925, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 8, 88, 987, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 88, 987, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 88, 987, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 8, 72, 1000, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 72, 1000, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 72, 1000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 56, 1062, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 56, 1062, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 64, 1062, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_square_80187838[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 6, 0 } },
    { 7, 8, 0, 0, { 1, 0 } },
    { 15, 3, 0, 0, { 8, 0 } },
    { 18, 7, 0, 0, { 0, 0 } },
    { 25, 9, 0, 0, { 5, 0 } },
    { 34, 14, 0, 0, { 2, 0 } },
    { 48, 6, 0, 0, { 9, 0 } },
    { 54, 15, 0, 0, { 4, 0 } },
    { 69, 13, 0, 0, { 7, 0 } },
    { 82, 9, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_square_80187898[94] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 24, 2050, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 0, 2050, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -32, 2050, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, -72, 2050, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -72, 2050, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 8, 2050, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 136, -24, 2050, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, -48, 2050, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 24, 2062, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 24, 2025, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 40, 1912, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 32, 2025, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 64, 2025, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 32, 2025, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 96, 16, 2025, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 24, 2037, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 32, 2062, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 32, 2050, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 2050, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 16, 2050, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 32, 2037, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 64, 2037, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 32, 2025, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 64, 2025, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 64, 2025, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 64, 1200, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 24, 1200, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -16, 1200, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -56, 1200, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -96, 1200, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, -104, 1200, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, -88, 1200, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, 40, 1987, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 56, 1987, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, 8, 1987, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -24, 1987, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -56, 1987, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -48, 1987, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 16, 2150, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, -16, 2150, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, -32, 2150, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 8, 2250, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -24, 2250, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 24, 2125, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 24, 2125, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 32, 2125, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 32, 2125, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 40, 2250, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 32, 2250, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 48, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 40, 2125, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 32, 2125, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 40, 2125, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 48, 2112, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 48, 2112, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 40, 2112, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 40, 2112, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 32, 2112, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 56, 2100, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 40, 2100, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 40, 2100, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 56, 1550, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 1550, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 56, 1600, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -64, 48, 1600, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -64, 40, 1700, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 32, 1750, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 48, 1600, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 88, 1250, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 64, 1312, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 56, 1312, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -144, 72, 1300, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 72, 1300, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 64, 1312, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 72, 802, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 88, 780, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -104, 96, 775, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -72, 96, 750, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -32, 96, 750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 8, 96, 750, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 80, 787, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -72, 80, 780, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -32, 80, 780, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -72, 72, 795, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 72, 795, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 48, 1950, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, 40, 1950, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 32, 1950, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 88, 72, 1300, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 120, 72, 1300, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, 64, 1305, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 56, 1305, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, 64, 1305, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, 56, 1305, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_square_80187FF0[18] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 13, 0 } },
    { 8, 17, 0, 0, { 2, 0 } },
    { 25, 7, 0, 0, { 9, 0 } },
    { 32, 6, 0, 0, { 3, 0 } },
    { 38, 3, 0, 0, { 11, 0 } },
    { 41, 2, 0, 0, { 0, 0 } },
    { 43, 4, 0, 0, { 12, 0 } },
    { 47, 2, 0, 0, { 1, 0 } },
    { 49, 4, 0, 0, { 8, 0 } },
    { 53, 5, 0, 0, { 6, 0 } },
    { 58, 3, 0, 0, { 15, 0 } },
    { 61, 7, 0, 0, { 7, 0 } },
    { 68, 6, 0, 0, { 10, 0 } },
    { 74, 11, 0, 0, { 4, 0 } },
    { 85, 3, 0, 0, { 14, 0 } },
    { 88, 6, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_square_80188080[53] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 48, 1125, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 80, 1075, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 40, 1125, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 80, 1075, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 16, 1125, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 40, 1125, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 80, 1075, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 80, 1075, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 40, 1125, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, 24, 2185, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 24, 2185, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 32, 2185, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 24, 2185, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 32, 2185, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 24, 2225, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 24, 2225, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, 16, 2225, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 16, 2225, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 104, 1250, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 80, 1250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 80, 1250, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 64, 1400, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 56, 1400, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 80, 1250, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 56, 1400, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 64, 1400, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 72, 1393, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 72, 56, 1395, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 72, 48, 1417, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 24, 1200, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 1125, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 64, 1162, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 40, 1175, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 32, 1187, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, 32, 1200, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, 40, 1350, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, 0, 1350, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -40, 1350, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, -72, 1350, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -144, -96, 1350, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 2375, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 8, 2375, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -32, 2375, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, -56, 2375, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -56, 2375, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -8, 8, 3375, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -24, 3375, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, -40, 3375, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 0, 3375, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -40, 3375, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 16, 3375, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -16, 3375, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, -40, 3375, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_square_801884A4[13] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 5, 0, 0, { 6, 0 } },
    { 14, 4, 0, 0, { 2, 0 } },
    { 18, 8, 0, 0, { 7, 0 } },
    { 26, 3, 0, 0, { 5, 0 } },
    { 29, 6, 0, 0, { 8, 0 } },
    { 35, 5, 0, 0, { 4, 0 } },
    { 40, 5, 0, 0, { 9, 0 } },
    { 45, 3, 0, 0, { 0, 0 } },
    { 48, 2, 0, 0, { 10, 0 } },
    { 50, 3, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_square_8018850C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_square_8018851C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_square_8018852C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_square_8018853C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_square_8018854C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_square_8018855C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_square_8018856C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_square_8018857C[15] = {
    { { .empty = D_acropolis_square_80186500 }, D_acropolis_square_80186500, NULL },
    { { .empty = D_acropolis_square_80186510 }, D_acropolis_square_80186510, NULL },
    { { .elements = D_acropolis_square_80186520 }, D_acropolis_square_80186AD4, NULL },
    { { .elements = D_acropolis_square_80186B2C }, D_acropolis_square_80186E60, NULL },
    { { .elements = D_acropolis_square_80186E88 }, D_acropolis_square_801870F4, NULL },
    { { .elements = D_acropolis_square_8018711C }, D_acropolis_square_80187838, NULL },
    { { .elements = D_acropolis_square_80187898 }, D_acropolis_square_80187FF0, NULL },
    { { .elements = D_acropolis_square_80188080 }, D_acropolis_square_801884A4, NULL },
    { { .empty = D_acropolis_square_8018850C }, D_acropolis_square_8018850C, NULL },
    { { .empty = D_acropolis_square_8018851C }, D_acropolis_square_8018851C, NULL },
    { { .empty = D_acropolis_square_8018852C }, D_acropolis_square_8018852C, NULL },
    { { .empty = D_acropolis_square_8018853C }, D_acropolis_square_8018853C, NULL },
    { { .empty = D_acropolis_square_8018854C }, D_acropolis_square_8018854C, NULL },
    { { .empty = D_acropolis_square_8018855C }, D_acropolis_square_8018855C, NULL },
    { { .empty = D_acropolis_square_8018856C }, D_acropolis_square_8018856C, NULL },
};

ViewCamera D_acropolis_square_80188630[15] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x704E, 100 } }, 348 },
    { { { { 3936, 0, 1132 }, { 14, 4095, -48 }, { -1132, 50, 3936 } }, { -3740, 3950, 0x2FEE } }, 195 },
    { { { { 4080, 0, 358 }, { 94, 3950, -1079 }, { -345, 1083, 3934 } }, { -370, 4950, 0x2828 } }, 207 },
    { { { { 3737, 0, 1674 }, { 531, 3884, -1185 }, { -1588, 1299, 3544 } }, { 1990, 4870, 9170 } }, 263 },
    { { { { 3750, 0, -1647 }, { -542, 3867, -1235 }, { 1555, 1349, 3540 } }, { -1930, 4800, 8370 } }, 246 },
    { { { { -1334, 0, -3872 }, { -103, 4094, 35 }, { 3871, 109, -1334 } }, { 2600, 3765, -4090 } }, 246 },
    { { { { -920, 0, -3991 }, { 300, 4084, -69 }, { 3979, -308, -917 } }, { 6160, 3515, -3630 } }, 207 },
    { { { { 4041, 0, 667 }, { 0, 4095, -3 }, { -667, 3, 4041 } }, { 3090, 3725, 5820 } }, 235 },
    { { { { -872, 0, -4002 }, { -1083, 3942, 236 }, { 3852, 1109, -839 } }, { -5555, 4045, -2590 } }, 269 },
    { { { { -2704, 0, -3076 }, { -109, 4093, 96 }, { 3074, 145, -2702 } }, { 1780, 3230, -1710 } }, 230 },
    { { { { -4018, 0, -791 }, { -243, 3897, 1236 }, { 753, 1260, -3823 } }, { 130, 2975, -880 } }, 257 },
    { { { { 3944, 0, 1103 }, { 808, 2787, -2890 }, { -750, 3001, 2684 } }, { -1310, 3273, 5158 } }, 329 },
    { { { { -704, 0, 4035 }, { 189, 4091, 33 }, { -4030, 192, -703 } }, { 1800, 3413, -820 } }, 257 },
    { { { { 1064, 0, -3955 }, { -2268, 3355, -610 }, { 3239, 2349, 872 } }, { -4075, 5389, -1590 } }, 269 },
    { { { { 1034, 0, -3963 }, { 258, 4087, 67 }, { 3954, -266, 1031 } }, { 7815, 3515, 1095 } }, 418 },
};

WorldCollisionFootstepSounds D_acropolis_square_8018884C = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionSurfaceProperties D_acropolis_square_80188858[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_square_80188860[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_square_8018884C },
};

WorldCollisionSurfaceProperties* D_acropolis_square_80188868[8] = {
    D_acropolis_square_80188858,
    D_acropolis_square_80188858,
    D_acropolis_square_80188858,
    D_acropolis_square_80188858,
    D_acropolis_square_80188860,
    D_acropolis_square_80188858,
    D_acropolis_square_80188858,
    D_acropolis_square_80188858,
};

AreaApplyRec D_acropolis_square_80188888[4] = {
    { 1, 3, 3, 1 },
    { 1, 4, 3, 1 },
    { 1, 19, 2, 1 },
    { 255, 0, 0, 0 },
};

s32 D_acropolis_square_80188898 = 0;

Task* D_acropolis_square_8018889C = NULL;

s32 D_acropolis_square_801888A0 = 0;

s32 D_acropolis_square_801888A4 = 0;

Task* gRoomCutsceneSoundTask = NULL;

RoomCutsceneRecStorage D_acropolis_square_801888AC = { 0 };

GfxCoord D_acropolis_square_801888CC = { 0 };

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

#include "../../shared/planar_reflection.inc.c"

void acropolisSquarePlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

#include "../../shared/telephone.inc.c"

void func_acropolis_square_80180804(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

/// State handlers of the room entry task `func_acropolis_square_80182308`,
/// indexed by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_acropolis_square_8017D6B4 = {
    {
        func_acropolis_square_80182260,
        func_acropolis_square_801822A4,
        taskKill,
    },
};

s32 func_acropolis_square_80181794(Task* task, s32 msgId, RoomEventMsg* arg2, RoomEventMsg* arg3)
{
    GameLocationKey key; // filled in but never used: the areaSetPlacementVariant call the
                         // sibling rooms make with it is absent here
    u16 temp_s1;

    key.stage = GAME_STAGE_ACROPOLIS;
    key.area  = GAME_AREA_ACROPOLIS_CAFETERIA;
    *arg3     = *arg2;
    if (arg2->areaId == GAME_AREA_ACROPOLIS_FORKED_ROAD) {
        if ((D_acropolis_square_8018382C != 0) && (arg2->queryOnly == ROOM_EVENT_EXECUTE)) {
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 2);
        }
        if (arg2->areaId == GAME_AREA_ACROPOLIS_FORKED_ROAD) {
            if (gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 1) {
                arg3->room = 2;
            }
        }
        return 1;
    }
    if (arg2->areaId == GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL) {
        if ((D_acropolis_square_8018382C != 0) && (arg2->queryOnly == ROOM_EVENT_EXECUTE)) {
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 2);
        }
        if (gameFlagGetNibble(0) < 2) {
            return 1;
        }
        if ((gameFlagGetNibble(0) == 2) || (gameFlagGetNibble(0) >= 3)) {
            if (arg2->queryOnly == ROOM_EVENT_EXECUTE) {
                do {
                    Gp_SetNibbleIf(arg2->flagId, 2);
                    Gp_RunCapCmd1(1);
                } while (0);
            }
            return 0;
        }
    }
    if (arg2->areaId == GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL) {
        if (gameFlagGetNibble(0) < 2) {
            return 1;
        }
        if ((gameFlagGetNibble(0) == 2) || (gameFlagGetNibble(0) >= 3)) {
            if (arg2->queryOnly == ROOM_EVENT_EXECUTE) {
                do {
                    Gp_SetNibbleIf(arg2->flagId, 2);
                    Gp_RunCapCmd1(1);
                } while (0);
            }
            return 0;
        }
    }
    temp_s1 = arg2->areaId;
    if (temp_s1 == 3) {
        if (arg2->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(0) < 2) {
                if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) < 2) {
                    arg3->room = 1;
                } else {
                    arg3->room = 2;
                }
            } else {
                arg3->room = temp_s1;
            }
        }
    }
    return 1;
}
s32 func_acropolis_square_801819BC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 var_a0;

    if (arg2 == 2) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == 7) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 1;
        }
        D_acropolis_square_801888AC.rec.view            = 9;
        D_acropolis_square_801888AC.rec.capSlot         = 1;
        D_acropolis_square_801888AC.rec.capFile         = 1;
        D_acropolis_square_801888AC.rec.startSound      = 0x51010001;
        D_acropolis_square_801888AC.rec.endSound        = 0x51010007;
        D_acropolis_square_801888AC.rec.sceneSound      = 0x51010006;
        D_acropolis_square_801888AC.rec.afterSceneSound = 0x5101000B;
        D_acropolis_square_801888AC.rec.skipScene       = D_acropolis_square_8018382C;
        D_acropolis_square_8018382C                     = 0;
        taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, 2, &D_acropolis_square_801888AC.rec);
    }
    if ((arg2 == 0xE) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_SQUARE_TRIGGER_E_SEEN) == 0)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_SQUARE_TRIGGER_E_SEEN, 1);
        Gp_SpawnIfCapIdle(0xE, 1);
    }
    if ((arg2 == 0x10) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_SQUARE_CONTROLS_HINT) == 0)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_SQUARE_CONTROLS_HINT, 1);
        var_a0 = 0x11;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout != 1) {
            var_a0 = 0x10;
        }
        Gp_SpawnIfCapIdle(var_a0, 1);
    }
    return 0;
}
/// Siren task for the square. States 0-2 arm the scene and tick, 3 fires the
/// first siren blast, 4 repeats it every 0x79 frames until the player answers,
/// and 5 waits for the scripted phase to advance before handing the scene off
/// to the slot-5 task and killing itself.
void func_acropolis_square_80181AEC(Task* task)
{
    s32 pan;
    s32 pan2;
    s32 pan3;
    s32 count;
    s32 count2;
    u32 state;

    state = task->state;
    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            D_acropolis_square_8018382C = 1;
            D_acropolis_square_80188898 = 0;
            func_800E8634(D_acropolis_square_80183834, 0, D_acropolis_square_801838DC);
            goto advance;

        case 3:
            D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
            D_acropolis_square_801888CC.coord.t[1] = -0xF96;
            D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
            D_acropolis_square_801888CC.parent     = &gGfxViewCoord;
            actorRenderComposeCoord(&D_acropolis_square_801888CC);
            pan = worldCoordGetOriginAudioPan(&D_acropolis_square_801888CC);
            sndEvtRequestScriptStart(
                SOUND_ACROPOLIS_SQUARE_SIREN, (s8)pan, (s8)worldCoordGetOriginAudioDepth(&D_acropolis_square_801888CC));
            goto advance;

        case 4:
            count                       = D_acropolis_square_80188898 + 1;
            D_acropolis_square_80188898 = count;
            if (count >= 0x79) {
                D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
                D_acropolis_square_801888CC.coord.t[1] = -0xF96;
                D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
                D_acropolis_square_80188898            = 0;
                D_acropolis_square_801888CC.parent     = &gGfxViewCoord;
                actorRenderComposeCoord(&D_acropolis_square_801888CC);
                pan2 = worldCoordGetOriginAudioPan(&D_acropolis_square_801888CC);
                sndEvtRequestScriptStart(SOUND_ACROPOLIS_SQUARE_SIREN, (s8)pan2,
                                         (s8)worldCoordGetOriginAudioDepth(&D_acropolis_square_801888CC));
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            Gp_MsgPlayerWeapon(1);
            /* fallthrough */

        case 1:
        case 2:
        advance:
            task->state += 1;
            return;

        case 5:
            if ((u32)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view - 5) >= 3U) {
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == 9) {
                    goto checkArmed;
                }
                goto handOff;
            }
        checkArmed:
            if (D_acropolis_square_8018382C == 0) {
            handOff:
                if (D_acropolis_square_8018382C != 0) {
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                    gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 2);
                    D_acropolis_square_8018382C = 0;
                }
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM_EFFECT), 0xC1F, 0, 0);
                sndEvtRequestScriptStop(SOUND_ACROPOLIS_SQUARE_SIREN, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                taskKill(task);
                return;
            }
            count2                      = D_acropolis_square_80188898 + 1;
            D_acropolis_square_80188898 = count2;
            if (count2 >= 0x79) {
                D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
                D_acropolis_square_801888CC.coord.t[1] = -0xF96;
                D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
                D_acropolis_square_80188898            = 0;
                D_acropolis_square_801888CC.parent     = &gGfxViewCoord;
                actorRenderComposeCoord(&D_acropolis_square_801888CC);
                pan3 = worldCoordGetOriginAudioPan(&D_acropolis_square_801888CC);
                sndEvtRequestScriptStart(SOUND_ACROPOLIS_SQUARE_SIREN, (s8)pan3,
                                         (s8)worldCoordGetOriginAudioDepth(&D_acropolis_square_801888CC));
            }
            break;
    }
}
/// Scrolling backdrop task: three 256x240 sprite strips (the last one half
/// width) tiled across the screen from `D_acropolis_square_801888A0`, each with
/// its own texture page. States 0-3 slide the strip in and hold it for a while,
/// state 4 kills the task; every state still draws.
void func_acropolis_square_80181DD0(Task* task)
{
    SPRT*     p;
    DR_TPAGE* dr;
    s32       x;
    s32       i;
    s32       tpageX;
    s32       count;
    s32       count2;
    s32       pos;

    switch (task->state) {
        case 0:
            D_acropolis_square_801888A0 = -0x140;
            D_acropolis_square_801888A4 = 0;
            task->state                += 1;
            break;

        case 1:
            count                       = D_acropolis_square_801888A4 + 1;
            D_acropolis_square_801888A4 = count;
            if (count >= 0x2E) {
                task->state += 1;
            }
            break;

        case 2:
            pos                         = D_acropolis_square_801888A0 + 1;
            D_acropolis_square_801888A0 = pos;
            if (pos >= 0) {
                D_acropolis_square_801888A4 = 0;
                task->state                += 1;
            }
            break;

        case 3:
            count2                      = D_acropolis_square_801888A4 + 1;
            D_acropolis_square_801888A4 = count2;
            if (count2 >= 0x1F) {
                task->state += 1;
            }
            break;

        case 4:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xD;
            taskKill(task);
            break;
    }

    x = D_acropolis_square_801888A0;
    for (i = 0; i < 3; i++) {
        tpageX         = 0x1C0 + i * 0x80;
        p              = gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        setlen(p, 4);
        setcode(p, 0x65);
        p->x0 = x - 0xA0;
        p->y0 = -0x78;
        p->u0 = 0;
        p->v0 = 0;
        if (i == 2) {
            p->w = 0x80;
            p->h = 0xF0;
        } else {
            p->w = 0x100;
            p->h = 0xF0;
        }
        p->clut = GetClut(0, 0xFF);
        addPrim(&gGpuCurrentOt[4], p);

        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, GetTPage(1, 0, tpageX, 0x100));
        addPrim(&gGpuCurrentOt[4], dr);

        x += 0x100;
    }
}

#include "../../shared/room_cutscene_sound_task.inc.c"

s32 func_acropolis_square_801820D8(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 0) {
        Gp_SpawnIfCapIdle(5, 0);
    }
    return 0;
}

/// Refuses every key-item-use request with result 0 and no side effects.
///
/// Handles `ACROPOLIS_SQUARE_MESSAGE_USE_KEY_ITEM` on the room task. The item
/// ID and second argument are ignored; no payload storage is accessed.
static s32 _acropolisSquareRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return 0;
}

s32 func_acropolis_square_80182110(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    sndEvtRequestScriptStart(D_acropolis_square_80183B34[arg2], 0, 0);
    return 0;
}

void func_acropolis_square_80182148(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(5);
            task->state++;
            return;
        case 1:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 7;
            task->state++;
            return;
        case 3:
            Gp_RunCapCmd1(5);
            task->state++;
            return;
        case 4:
        case 5:
            task->state++;
            return;
        case 6:
            Gp_RunCapCmd1(5);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 8;
            task->state++;
            return;
        case 2:
        case 7:
            gameFlagSetNibble(GAME_FLAG_015, 1);
            taskKill(task);
            return;
    }
}

void func_acropolis_square_80182200(s32 arg0)
{
    switch (arg0) { /* irregular */
        case 0:
            D_acropolis_square_8018889C = taskSpawnFromTable(D_acropolis_square_80183808, 2, 0, 0);
            return;
        case 1:
            taskKill(D_acropolis_square_8018889C);
            return;
    }
}

/// First state of the room entry task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances the state.
static void func_acropolis_square_80182260(Task* task)
{
    task->msgTable = D_acropolis_square_801837C4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

static void func_acropolis_square_801822A4(Task* task)
{
    char pad[0x10];

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == 7 && D_acropolis_square_80183830 == 0) {
        D_acropolis_square_80183830                         = 1;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 2;
        func_800E8634(D_acropolis_square_8018399C, 0, D_acropolis_square_80183A5C);
    }
}

/// Room entry task: runs the state handler `D_acropolis_square_8017D6B4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_acropolis_square_80182308(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_square_8017D6B4;
    sp.funcs[task->state](task);
}

s32 func_acropolis_square_80182360(s32 unused)
{
    GameLocationKey key;

    if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_SQUARE_01F) == 0) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_SQUARE_01F, 1);
        key.stage = GAME_STAGE_ACROPOLIS;
        key.area  = GAME_AREA_ACROPOLIS_SQUARE;
        areaSetPlacementVariant(&key, 2, AREA_VARIANT_RESET_ALWAYS);
        gGameSession->eventState = 1;
        taskSpawnFromTable(D_acropolis_square_80183808, 0, 0, 0);
        return 0;
    }
    return 1;
}

void func_acropolis_square_801823DC(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    switch (task->state) { /* irregular */
        case 0:
            task->msgTable = D_acropolis_square_80183B58;
            gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
            D_acropolis_square_80183B98 = 0;
            Task_Spawn(1, 0x25, 0, 0);
            Task_Spawn(1, 0x25, 1, 0);
            task->state++;
            return;
        case 1:
            if ((0x268 >> (gGameSession->location.loc.view - 1)) & 1) {
                work->move.vx = 0x19AA;
                work->move.vy = -0xF96;
                work->move.vz = 0x8DE;
                Gp_SpawnEff(EFFECT_ACROPOLIS_SQUARE_BEACON_GLOW, coord, D_acropolis_square_80183B98 * 0x10000218 + 0x10E08,
                            &work->move);
            }
            if (gGameSession->location.loc.view == 0xE) {
                work->move.vx = 0x18D2;
                work->move.vy = -0x100B;
                work->move.vz = 0x8AB;
                Gp_SpawnEff(EFFECT_ACROPOLIS_SQUARE_BEACON_GLOW, coord, D_acropolis_square_80183B98 * 0x218 + 0x10010608,
                            &work->move);
            }
            if (gGameSession->location.loc.view == 9) {
                work->move.vx = 0x19AA;
                work->move.vy = -0xF96;
                work->move.vz = 0x8E8;
                Gp_SpawnEff(EFFECT_ACROPOLIS_SQUARE_BEACON_GLOW, coord, D_acropolis_square_80183B98 * 0x118 + 0x80010308,
                            &work->move);
            }
            return;
    }
}

/// Queues a beacon-glow primitive with additive blending at its projected depth.
///
/// `primitive` is an initialized, writable `POLY_G4` or `LINE_G3` in the frame
/// arena. `sortingDepth` borrows a readable signed word holding SZ3 / 4 before
/// display scaling; the caller rejects depths below 17. The scaled depth wraps
/// to tag 0..1023 in the current table. Keep the depth word separate from packet
/// storage; no depth pointer is retained.
/// Requires word-aligned space for one `DR_TPAGE` at `gGpuPrimCursor`, consumed
/// without a capacity check. Both packets must live until GPU drawing completes.
/// The dithered additive draw mode persists until another command replaces it.
static inline void _acropolisSquareQueueBeaconGlow(void* primitive, const s32* sortingDepth)
{
    // Sixteen depth units per tag, expressed as four-byte table offsets.
    enum { ACROPOLIS_SQUARE_GLOW_DEPTH_TO_BYTE_OFFSET_SHIFT = 2 };

    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                ((((u32)*sortingDepth << gDisplayState.otDepthShift) >> ACROPOLIS_SQUARE_GLOW_DEPTH_TO_BYTE_OFFSET_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            primitive);
    // Prepending the draw mode after the primitive makes the GPU apply it first.
    gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, *sortingDepth);
}

void acropolisSquareBeaconGlowTask(Task* task)
{
    enum {
        ACROPOLIS_SQUARE_GLOW_MIN_DEPTH            = 17,
        ACROPOLIS_SQUARE_GLOW_PULSE_RATE_MASK      = 0xFF,
        ACROPOLIS_SQUARE_GLOW_RADIUS_SHIFT         = 8,
        ACROPOLIS_SQUARE_GLOW_RADIUS_MASK          = 0xFF,
        ACROPOLIS_SQUARE_GLOW_CYAN_SHIFT           = 16,
        ACROPOLIS_SQUARE_GLOW_STREAKS              = 0x10000000,
        ACROPOLIS_SQUARE_GLOW_PULSE_FALLING        = 0x80,
        ACROPOLIS_SQUARE_GLOW_PULSE_RAMP_MASK      = 0x7F,
        ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS   = 12,
        ACROPOLIS_SQUARE_GLOW_FAN_RADIUS_FACTOR    = 0x600,
        ACROPOLIS_SQUARE_GLOW_RAY_RADIUS_FACTOR    = 0xC0,
        ACROPOLIS_SQUARE_GLOW_DIAMOND_RADIUS_SHIFT = 9
    };
    RoomGlowRadiiScratch* projection;
    POLY_G4*              quad;
    LINE_G3*              streak;
    GfxCoord*             effectCoord;
    void*                 effectWork;
    s32                   segmentIndex;
    s32                   pulsePhase;
    s32                   glowValue;
    s32                   radiusScale;
    s16                   intensity;
    s16                   cyan;
    s16                   halfIntensity;

    // Project the composed effect center after narrowing to game coordinates.
    effectCoord = task->extra.coordBody->coord;
    effectWork  = task->spawnArg2.pointer;
    actorRenderComposeCoord(effectCoord);
    projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
    projection->worldPos.vx = (u16)effectCoord->workm.t[0];
    projection->worldPos.vy = (u16)effectCoord->workm.t[1];
    projection->worldPos.vz = (u16)effectCoord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPos);
    gte_rtps();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    if (projection->otz >= ACROPOLIS_SQUARE_GLOW_MIN_DEPTH) {
        // Fold the frame phase into a triangular red or cyan brightness ramp.
        pulsePhase  = gDisplayState.animFrame;
        pulsePhase *= task->spawnArg1.value & ACROPOLIS_SQUARE_GLOW_PULSE_RATE_MASK;
        cyan        = (task->spawnArg1.value >> ACROPOLIS_SQUARE_GLOW_CYAN_SHIFT) & 1;
        if (pulsePhase & ACROPOLIS_SQUARE_GLOW_PULSE_FALLING) {
            glowValue = ~pulsePhase & ACROPOLIS_SQUARE_GLOW_PULSE_RAMP_MASK;
        } else {
            glowValue = pulsePhase & ACROPOLIS_SQUARE_GLOW_PULSE_RAMP_MASK;
        }
        intensity = glowValue * 2;
        glowValue = task->spawnArg1.value;
        // The sign bit selects the round fan; the other form is a flat diamond.
        if (glowValue < 0) {
            radiusScale             = (glowValue >> ACROPOLIS_SQUARE_GLOW_RADIUS_SHIFT) & ACROPOLIS_SQUARE_GLOW_RADIUS_MASK;
            projection->outerRadius = (radiusScale * ACROPOLIS_SQUARE_GLOW_FAN_RADIUS_FACTOR) / projection->otz;
            projection->innerRadius = (radiusScale * ACROPOLIS_SQUARE_GLOW_RAY_RADIUS_FACTOR) / projection->otz;
            for (segmentIndex = 0; segmentIndex < 0x10; segmentIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, (intensity * (cyan ^ 1)) >> 1, (cyan * intensity) >> 1, (cyan * intensity) >> 1);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 4]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 5]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 1]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 6]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 2]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                _acropolisSquareQueueBeaconGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 4]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 5]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 1]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 6]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 2]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                _acropolisSquareQueueBeaconGlow(quad, &projection->otz);
            }
            // Add two opposing pairs of long rays over the circular fan.
            halfIntensity = intensity >> 1;
            for (segmentIndex = 2; segmentIndex < 0x10; segmentIndex += 8) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_square_80183B68[segmentIndex]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_square_80183B68[segmentIndex - 4]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 4]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS - 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS - 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_square_80183B68[segmentIndex + 8]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_square_80183B68[segmentIndex + 4]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                _acropolisSquareQueueBeaconGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_square_80183B68[segmentIndex + 4]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_square_80183B68[segmentIndex]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 8]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_square_80183B68[segmentIndex + 4]) >> ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_square_80183B68[segmentIndex + 0xC]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_square_80183B68[segmentIndex + 8]) >> (ACROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                _acropolisSquareQueueBeaconGlow(quad, &projection->otz);
            }
        } else {
            projection->outerRadius = (((glowValue >> ACROPOLIS_SQUARE_GLOW_RADIUS_SHIFT) & ACROPOLIS_SQUARE_GLOW_RADIUS_MASK) << ACROPOLIS_SQUARE_GLOW_DIAMOND_RADIUS_SHIFT) / projection->otz;
            for (segmentIndex = 0; segmentIndex < 2; segmentIndex++) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx - projection->outerRadius;
                quad->x1 = quad->x2 = projection->screenPos.vx;
                quad->x3            = projection->screenPos.vx + projection->outerRadius;
                quad->y0 = quad->y2 = quad->y3 = projection->screenPos.vy;
                quad->y1                       = (projection->screenPos.vy - projection->outerRadius) + projection->outerRadius * (segmentIndex + segmentIndex);
                addPrim((&gGpuCurrentOt[((u32)projection->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF]),
                        quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz);
            }
            if (task->spawnArg1.value & ACROPOLIS_SQUARE_GLOW_STREAKS) {
                for (segmentIndex = 0; segmentIndex < 2; segmentIndex++) {
                    streak         = gGpuPrimCursor;
                    gGpuPrimCursor = streak + 1;
                    setLineG3(streak);
                    setRGB0(streak, 0, 0, 0);
                    setRGB1(streak, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                    setRGB2(streak, 0, 0, 0);
                    streak->x0 = projection->screenPos.vx + projection->outerRadius * (segmentIndex * 3 - 1);
                    streak->y0 = projection->screenPos.vy - projection->outerRadius * (segmentIndex + 1);
                    streak->x1 = projection->screenPos.vx;
                    streak->y1 = projection->screenPos.vy;
                    streak->x2 = projection->screenPos.vx - projection->outerRadius * (segmentIndex * 3 - 1);
                    streak->y2 = projection->screenPos.vy + projection->outerRadius * (segmentIndex + 1);
                    _acropolisSquareQueueBeaconGlow(streak, &projection->otz);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    // This is a one-frame counted effect, including when projection rejects it.
    effectKillTask(effectWork, task);
}

s32 func_acropolis_square_8018344C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    D_acropolis_square_80183B98 = arg2;
    return 0;
}

/// Unused empty function retained in the overlay image; its original role is unproven.
static void _acropolisSquareNoOp(void)
{
}
