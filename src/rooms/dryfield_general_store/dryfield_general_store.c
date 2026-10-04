#include "rooms/dryfield_general_store.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
#include "../../shared/room_events.h"
#include "../../shared/general_store.h"

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;
extern u8           gRoomEventActive;

/// Descriptor of the event task `roomEventTask`.
extern TaskDesc gRoomEventTaskDesc;

extern TaskDesc gStoreTaskDescs[];

/// The stage byte `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` held when the cutscene began, saved by
/// `storeCutsceneTask`'s first state and restored into
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` when the cutscene is cut short.
extern u8 gStoreSavedView;

/// The two script arguments, latched from the message that armed the cutscene
/// task `gStoreTaskDescs`; the task itself reads them back to
/// place its actors.
extern u8 gStoreWarp;
extern u8 gStoreRoom;

/// The 4-byte record `storeCutsceneTask` hands the helper
/// task 0x31 when the script's CAP event key asks for it.
extern ScreenFade gStoreFade;

extern TaskMessageEntry D_dryfield_general_store_8017E188[];
extern s32              D_dryfield_general_store_8017E1B8;
extern TaskDesc         D_dryfield_general_store_8017E4C0;
extern s32              D_dryfield_general_store_8017E55C;
extern s32              D_dryfield_general_store_8017E560;
extern s32              D_dryfield_general_store_8017E564;
extern EvsCommand       D_dryfield_general_store_8017E568[];

static void func_dryfield_general_store_8017DEAC(Task* arg0);
static void func_dryfield_general_store_8017DF4C(Task* task);

extern AnimationPlayRequest     D_dryfield_general_store_8017E4FC;
extern AnimationBankCopyRequest D_dryfield_general_store_8017E4E0;
extern GameActorMoveAnim        D_dryfield_general_store_8017E554;
extern ActorTransform           D_dryfield_general_store_8017E524;
extern ActorTransform           D_dryfield_general_store_8017E53C;
extern s32                      D_dryfield_general_store_8017E560;
void                            func_dryfield_general_store_8017E130(s32);

extern WorldCollisionGrid         D_dryfield_general_store_8017F238[1];
extern WorldCollisionOccluder     D_dryfield_general_store_80184F78[4];
extern WorldCollisionTrigger      D_dryfield_general_store_801840EC[28];
extern WorldCollisionTrigger      D_dryfield_general_store_8018493C[21];
extern WorldCoordRoomAmbientEntry D_dryfield_general_store_80185500[17];
extern WorldCoordRoomLights       D_dryfield_general_store_801854E8[1];

s32  func_dryfield_general_store_8017DDF4(Task*, s32, s32, s32);
s32  func_dryfield_general_store_8017DDFC(Task*, s32, RoomEventMsg*, s32);
void func_dryfield_general_store_8017DFB4(Task*);
void func_dryfield_general_store_8017E064(Task*);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc gStoreTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, storeToggleTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, storeCutsceneTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_dryfield_general_store_8017E188[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, storeDoorMsg },
    { 5105, func_dryfield_general_store_8017DDF4 },
    { ROOM_MESSAGE_COMMAND, storeActionMsg },
    { ROOM_MESSAGE_SOUND, storeSoundMsg },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_general_store_8017DDFC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 D_dryfield_general_store_8017E1B8 = 0x90302;

static AnimationPackedPose _gDryfieldGeneralStoreAnimation00ED8Bank1[6] = {
#include "assets/dryfield_general_store_animation_00ED8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldGeneralStoreAnimation00ED8Bank4[46] = {
#include "assets/dryfield_general_store_animation_00ED8_bank4.inc"
};

static AnimationRecord _gDryfieldGeneralStoreAnimation00ED8Records[109] = {
#include "assets/dryfield_general_store_animation_00ED8_records.inc"
};

static u16 _gDryfieldGeneralStoreAnimation00ED8Indices[20] = {
#include "assets/dryfield_general_store_animation_00ED8_indices.inc"
};

static AnimationSet _gDryfieldGeneralStoreAnimation00ED8 = {
    _gDryfieldGeneralStoreAnimation00ED8Records,
    _gDryfieldGeneralStoreAnimation00ED8Indices,
    { NULL, _gDryfieldGeneralStoreAnimation00ED8Bank1, NULL, NULL, _gDryfieldGeneralStoreAnimation00ED8Bank4, NULL, NULL, NULL },
};

TaskDesc D_dryfield_general_store_8017E4C0 = { { { TASK_BODY_NONE, 192 } }, func_dryfield_general_store_8017DFB4, { .value = 0 } };

TaskDesc D_dryfield_general_store_8017E4CC = { { { TASK_BODY_NONE, 192 } }, func_dryfield_general_store_8017E064, { .value = 0 } };

AnimationSet* D_dryfield_general_store_8017E4D8[2] = {
    NULL,
    &_gDryfieldGeneralStoreAnimation00ED8,
};

AnimationBankCopyRequest D_dryfield_general_store_8017E4E0 = { { .sets = D_dryfield_general_store_8017E4D8 }, ARRAY_SIZE(D_dryfield_general_store_8017E4D8) };

AnimationPlayRequest D_dryfield_general_store_8017E4E8 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_general_store_8017E4FC = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_general_store_8017E510 = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_general_store_8017E524 = { { 1735, 0, 6800, 0 }, { 0, -2047, 0, 0 } };

ActorTransform D_dryfield_general_store_8017E53C = { { 1505, 0, 5558, 0 }, { 0, -2047, 0, 0 } };

GameActorMoveAnim D_dryfield_general_store_8017E554 = { 4, 9 };

s32 D_dryfield_general_store_8017E55C = 770;

s32 D_dryfield_general_store_8017E560 = 0x10302;

s32 D_dryfield_general_store_8017E564 = 0x20302;

EvsCommand D_dryfield_general_store_8017E568[11] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_general_store_8017E4E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_general_store_8017E4FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_general_store_8017E524 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1019 }, { .message = { .pointer = &D_dryfield_general_store_8017E53C } }, { .message = { .pointer = &D_dryfield_general_store_8017E554 } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .pointer = &D_dryfield_general_store_8017E560 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_general_store_8017E130 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

WorldCollisionRoomResources D_dryfield_general_store_8017E670[1] = {
    { D_dryfield_general_store_8017F238, D_dryfield_general_store_801840EC, D_dryfield_general_store_8018493C, D_dryfield_general_store_80184F78 },
};

u8* D_dryfield_general_store_8017E680[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_general_store_8017E684[1] = { 16 };

WorldCoordRoomLighting D_dryfield_general_store_8017E688[1] = {
    { D_dryfield_general_store_801854E8, D_dryfield_general_store_80185500 },
};

DirectionWarpEntry D_dryfield_general_store_8017E690[3] = {
    { { { .word = 0 }, 3100, 0, 439 }, { 0, 0, 0, 0 }, { { .word = 1536 }, 2496, 0, 920 }, { 0, 0, 0, 0 }, 0x52030004, 0x52030003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 489 },
    { { { .word = 3072 }, 9669, 0, 4832 }, { 0, 0, 0, 0 }, { { .word = 512 }, 9128, 0, 4172 }, { 0, 0, 0, 0 }, 0x52030002, 0x52030001, 0x5203000B, 7, DIRECTION_WARP_FLAG_NONE, 470 },
    { { { .word = 3072 }, 8649, 0, 7411 }, { 0, 0, 0, 0 }, { { .word = 512 }, 8209, 0, 6766 }, { 0, 0, 0, 0 }, 0x5203000E, 0x5203000D, 0x52030006, 14, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldGeneralStoreCollision01C78Normals[13] = {
#include "assets/dryfield_general_store_collision_01C78_normals.inc"
};

static SVECTOR _gDryfieldGeneralStoreCollision01C78Verts[137] = {
#include "assets/dryfield_general_store_collision_01C78_verts.inc"
};

static WorldCollisionGridFace _gDryfieldGeneralStoreCollision01C78Faces[83] = {
#include "assets/dryfield_general_store_collision_01C78_faces.inc"
};

static s16 _gDryfieldGeneralStoreCollision01C78Cells[292] = {
#include "assets/dryfield_general_store_collision_01C78_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldGeneralStoreCollision01C78Cells[i])
static s16* _gDryfieldGeneralStoreCollision01C78Table[9] = {
#include "assets/dryfield_general_store_collision_01C78_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_general_store_8017F238[1] = {
    { NULL, _gDryfieldGeneralStoreCollision01C78Normals, _gDryfieldGeneralStoreCollision01C78Verts, _gDryfieldGeneralStoreCollision01C78Faces, _gDryfieldGeneralStoreCollision01C78Table, 0, 0, 3, 3, 4000, 83 },
};

ViewCamera D_dryfield_general_store_8017F25C[16] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -5000, 0x7530, -4000 } }, 589 },
    { { { { -3828, 0, -1456 }, { -650, 3664, 1709 }, { 1303, 1829, -3425 } }, { -750, 2800, -5920 } }, 230 },
    { { { { 3759, 0, -1625 }, { -775, 3599, -1793 }, { 1428, 1954, 3304 } }, { -800, 2800, -1820 } }, 230 },
    { { { { 905, 0, 3994 }, { 1197, 3907, -271 }, { -3810, 1227, 864 } }, { -8200, 2100, -220 } }, 230 },
    { { { { 1275, 0, -3892 }, { -1336, 3847, -437 }, { 3655, 1406, 1197 } }, { -4140, 2100, -170 } }, 230 },
    { { { { -4009, 0, 835 }, { 379, 3648, 1822 }, { -744, 1861, -3571 } }, { -9190, 2600, -5570 } }, 230 },
    { { { { 4002, 0, 868 }, { 540, 3206, -2491 }, { -679, 2549, 3133 } }, { -9190, 2950, -2370 } }, 230 },
    { { { { 796, 0, 4017 }, { 679, 4037, -134 }, { -3960, 692, 784 } }, { -8120, 1510, -2590 } }, 230 },
    { { { { 792, 0, -4018 }, { -676, 4037, -133 }, { 3961, 689, 781 } }, { -4200, 1500, -2620 } }, 230 },
    { { { { 749, 0, 4026 }, { 2222, 3415, -413 }, { -3357, 2260, 625 } }, { -7720, 2610, -4690 } }, 230 },
    { { { { 789, 0, -4019 }, { -2297, 3360, -451 }, { 3297, 2341, 647 } }, { -4720, 2610, -4690 } }, 230 },
    { { { { -673, 0, 4040 }, { -895, 3994, -149 }, { -3939, -907, -656 } }, { -6300, 710, -7480 } }, 230 },
    { { { { -584, 0, -4054 }, { -338, 4081, 48 }, { 4039, 342, -582 } }, { -1670, 1600, -7510 } }, 230 },
    { { { { -640, 0, -4045 }, { -2705, 3045, 428 }, { 3007, 2739, -476 } }, { -6760, 2770, -7430 } }, 230 },
    { { { { -1669, 0, 3740 }, { 2316, 3216, 1033 }, { -2936, 2536, -1310 } }, { -1030, 1830, -7050 } }, 289 },
    { { { { 1350, 0, -3866 }, { -3231, 2250, -1128 }, { 2124, 3422, 741 } }, { -8560, 2030, -7190 } }, 257 },
};

SpriteBatch D_dryfield_general_store_8017F49C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_8017F4AC[101] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 0, -8, 1034, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -16, 974, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, -40, 1162, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, -16, 1550, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, -32, 1478, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, -48, 1425, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -32, 1011, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -24, 1158, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, -8, 1225, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -80, 432, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, -32, 450, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 104, -88, 627, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 120, -88, 462, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, -64, 469, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, -32, 475, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, -32, 483, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -64, 436, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 136, -120, 385, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 128, -64, 418, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 88, 72, 562, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, 64, 550, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 56, 500, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, 56, 500, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, 80, 477, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 80, 80, 575, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 96, 612, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 88, 530, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 72, 440, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, -56, 960, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 24, -56, 738, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, 112, -120, 595, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 64, -120, 633, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 16, -120, 680, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 0, -120, 723, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 88, 242, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 56, 80, 252, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 96, 242, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, 104, 226, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 16, 1348, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -160, -56, 1329, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -144, -56, 1285, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, -56, 1240, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, -56, 1245, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, -56, 1255, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, -56, 1275, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -40, 1359, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -16, 1328, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -40, 1307, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -16, 1350, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -40, 1253, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -16, 1343, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -64, -48, 1300, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, -32, 1350, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, -16, 1400, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 0, 1425, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -128, 8, 1408, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -144, 8, 1392, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -112, 8, 1337, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -88, -40, 1287, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, -16, 1350, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, 8, 1375, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -160, -16, 950, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -144, -16, 912, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -160, 0, 962, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -144, 0, 937, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 16, 987, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 40, 1025, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 64, 1025, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -128, 8, 987, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 24, 975, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 40, 1025, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 56, 1037, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 48, 1025, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 16, 978, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 56, 974, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, -120, 1346, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, -120, 1504, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, -104, 1139, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, -112, 1227, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -104, 1104, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, 0, 1000, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 48, 0, 942, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, 8, 1025, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -8, 999, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -8, 962, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -8, 967, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 8, 1025, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, 8, 1025, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 8, 962, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 48, 24, 987, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 48, 40, 1000, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 48, 56, 1000, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 24, 987, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 48, 1000, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 24, 1037, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 48, 1037, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 24, 1025, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, 40, 1075, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -8, 48, 1075, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 48, 8, 950, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 16, 987, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_8017FC90[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 9, 10, 0, 0, { 5, 0 } },
    { 19, 9, 0, 0, { 4, 0 } },
    { 28, 6, 0, 0, { 6, 0 } },
    { 34, 4, 0, 0, { 1, 0 } },
    { 38, 23, 0, 0, { 7, 0 } },
    { 61, 14, 0, 0, { 3, 0 } },
    { 75, 5, 0, 0, { 8, 0 } },
    { 80, 21, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_8017FCE8[105] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 112, 481, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 112, 388, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -16, 112, 487, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 104, 439, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, 96, 490, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, 104, 468, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 88, 528, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 104, 495, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, 88, 531, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, 104, 495, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 88, 531, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 104, 495, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 104, 397, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 104, 397, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -88, 440, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -48, 492, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -88, 482, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -56, 533, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -64, 521, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -144, -96, 416, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -104, 428, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 16, 1278, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 1125, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 1057, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -120, 1000, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, -120, 1004, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 0, -32, 1162, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 0, -80, 1125, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 0, -120, 1061, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, -80, 1091, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, -56, 1100, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, -32, 1132, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, -8, 1150, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -40, -80, 1093, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -56, 1106, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -40, 1112, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -32, -24, 1162, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -32, 0, 1212, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, -80, 1012, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, -80, 984, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, -64, 1000, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, -48, 1000, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, -32, 1000, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, -16, 1050, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 8, 1100, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -24, 1037, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -8, 1062, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, 8, 1087, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -96, -80, 1000, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, -80, 1025, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, -56, 1000, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, -32, 1012, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, -8, 1062, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, 16, 1112, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 1000, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -152, -40, 996, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -144, -16, 1025, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, 8, 1025, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 32, 1050, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, -120, 1072, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 40, -80, 1282, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, -32, 1377, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 72, -120, 1250, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 72, -80, 1300, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 72, -40, 1350, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -120, 1250, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -80, 1350, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -120, 1250, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -80, 1385, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, -40, 1509, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 104, -40, 1350, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, -24, 1350, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, -24, 1200, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 0, 1200, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, -40, 1251, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, -24, 1225, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, -8, 1225, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 24, 8, 1247, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -40, 1238, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -24, 1208, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -8, 1204, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 8, 8, 1214, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, -32, 987, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, -16, 1025, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, 0, 1075, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 104, 16, 1100, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 104, 40, 1100, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 96, -8, 1031, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 88, 24, 1125, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 88, 40, 1150, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -40, 932, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 120, -24, 975, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -8, 975, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 136, -40, 914, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 152, -40, 940, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 136, -24, 931, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 120, 8, 1000, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, 24, 1025, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, 48, 1025, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 136, -8, 926, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 136, 16, 947, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 136, 40, 993, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, -24, 926, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 8, 1075, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 96, 8, 1075, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_8018051C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 14, 7, 0, 0, { 3, 0 } },
    { 21, 51, 0, 0, { 2, 0 } },
    { 72, 10, 0, 0, { 4, 0 } },
    { 82, 23, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_80180554[85] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, -48, 1602, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -56, -40, 1553, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -40, -40, 1603, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, -40, 1603, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -8, -40, 1603, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -48, 1600, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 8, -48, 1646, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, -24, 1636, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -24, -24, 1656, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, -24, 1697, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -72, -112, 1550, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -32, -112, 1550, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 8, -112, 1550, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, -48, 1000, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, -32, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, -16, 1000, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 0, 1050, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 0, 1050, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -56, 16, 1062, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -56, 32, 1080, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -40, 32, 1075, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -40, 911, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -24, 716, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, -16, 1237, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, 0, 1262, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 16, 1287, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 16, 1301, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, -40, 1049, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -24, 1078, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -8, 1100, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 16, 1112, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 40, 1125, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 40, 1103, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -48, 1073, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -32, 872, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -16, 900, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 48, 8, 912, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 48, 32, 950, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 48, 56, 950, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, -32, 769, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -32, 772, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -32, 721, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -40, 916, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 72, -16, 750, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 72, 32, 775, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, -16, 675, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, 32, 650, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 120, -16, 596, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, 80, 962, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 0, 600, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, -40, 1212, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 112, 8, 637, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 72, 8, 750, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 72, 800, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 675, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, -40, 1247, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, -24, 1216, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 120, -48, 1221, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, -48, 1128, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, -32, 1200, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, -32, 1125, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, -8, 1150, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 120, -8, 1187, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -64, 1800, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, -104, 1832, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, -104, 1848, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -112, 1861, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 96, -80, 1997, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 96, -56, 2057, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -40, 2104, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 112, -80, 1850, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 112, -64, 2023, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 112, -48, 2014, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 128, -80, 1825, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 144, -80, 1652, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 128, -48, 1900, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 144, -48, 1801, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 136, -64, 1764, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 144, -64, 1750, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, -32, 2111, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, -120, 735, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, -120, 753, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 8, -120, 788, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 56, -112, 805, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, 104, -112, 847, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_80180BF8[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 3, 0 } },
    { 10, 3, 0, 0, { 4, 0 } },
    { 13, 8, 0, 0, { 1, 0 } },
    { 21, 34, 0, 0, { 5, 0 } },
    { 55, 8, 0, 0, { 0, 0 } },
    { 63, 17, 0, 0, { 6, 0 } },
    { 80, 5, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_80180C40[71] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -88, 1276, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, -88, 1356, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -64, 1275, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -64, 1250, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -32, 1303, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -32, 1250, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 0, 1303, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 0, 1305, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -80, 1587, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, -64, 1625, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -80, 1575, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -64, 1618, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, -80, 1537, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -64, 1550, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, -40, 1022, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -24, 551, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -24, 589, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 0, 589, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 0, 550, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -24, 608, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, 0, 612, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, -24, 725, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, 0, 737, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -32, 725, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, -48, 810, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 24, 525, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 24, 612, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 24, 712, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 56, 550, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 56, 625, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 56, 743, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 88, 562, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 88, 637, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 88, 750, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -48, 863, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -48, 966, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -48, 1026, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -64, -32, 825, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, -8, 862, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, 24, 887, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, 56, 875, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 88, 875, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -32, 1000, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -8, 950, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 16, 975, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 40, 975, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -32, 1040, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -16, 1067, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, 0, 1087, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, 16, 1087, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 32, 1087, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 8, 1125, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 24, 1125, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -56, 1137, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -56, 1090, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, -56, 1157, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -48, 987, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, -48, 1058, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, -48, 1100, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, -48, 1172, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -32, 1000, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, -32, 1071, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, -32, 1125, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -32, 1149, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -120, 1388, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, -120, 1515, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -96, 1415, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, -96, 1551, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -72, 1437, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, -72, 1572, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -64, 1648, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_801811CC[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 8, 6, 0, 0, { 3, 0 } },
    { 14, 39, 0, 0, { 2, 0 } },
    { 53, 11, 0, 0, { 4, 0 } },
    { 64, 7, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_80181204[78] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -16, 1327, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -64, 1350, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -40, 1350, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -16, 1375, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -80, 1343, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -56, 1339, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -32, 1350, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -8, 1386, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -96, 1299, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -80, 1322, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -56, 1300, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -32, 1327, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -48, 1050, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -8, 1137, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 0, 1137, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 16, 1062, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, -40, 975, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -40, 968, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -48, 957, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, -48, 999, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -48, 1013, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, -48, 1026, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, -48, 1049, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -48, 1015, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, -56, 1099, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, -56, 1115, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -56, 1047, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, -32, 1043, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, -8, 1083, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, -32, 1019, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, -8, 1025, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 88, -32, 1002, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 88, -8, 1042, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 64, -32, 981, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 64, -8, 1025, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 16, 1062, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, -24, 987, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, -8, 1025, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 8, 1062, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 24, 1062, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -24, 964, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -8, 1011, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 8, 1062, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 24, 1062, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -40, 1050, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -40, 1050, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -24, 1100, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -24, 1100, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -8, 1137, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 8, 1187, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 8, 1162, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 24, 1175, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 0, 570, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, 0, 587, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 24, 569, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, 48, 575, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 80, 610, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, 24, 625, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 48, 612, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 80, 625, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 0, 625, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 8, 675, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 16, 662, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 32, 662, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 24, 687, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 40, 687, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 64, 725, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 750, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 56, 675, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 88, 725, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 16, 675, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 32, 675, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 48, 725, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 64, 725, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 80, 775, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 96, 800, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 72, 775, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 88, 775, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_8018181C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 1, 0 } },
    { 12, 40, 0, 0, { 2, 0 } },
    { 52, 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_80181844[77] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, 96, 475, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, 32, 387, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 120, 32, 312, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 136, 40, 387, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 64, 378, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 144, 80, 398, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 144, 96, 392, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, 48, 387, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 120, 48, 387, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 96, 40, 400, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 56, 425, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 64, 400, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 112, 64, 412, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 80, 412, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 80, 387, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 96, 96, 462, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 112, 96, 387, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 88, 452, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 128, 104, 403, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 72, 425, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 88, 475, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 72, 104, 475, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, 32, 800, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, 40, 843, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, -16, 600, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -136, -16, 664, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -120, -24, 650, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -112, -16, 621, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -88, -16, 662, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, 8, 650, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, 8, 637, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 32, 725, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 64, 800, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, 8, 650, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, 32, 675, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, 56, 712, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, 80, 775, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 8, 675, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 24, 675, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, 8, 700, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, 24, 712, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, 8, 725, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, 24, 800, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 40, 675, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, 40, 775, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 56, 700, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, 56, 775, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 72, 725, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, 72, 812, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -72, 40, 800, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -72, 64, 900, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 48, 862, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 88, 775, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, 88, 775, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, 88, 812, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 871, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 990, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -160, -40, 1108, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 837, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -80, 975, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -120, -40, 1069, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -120, 805, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -80, 975, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -40, 1050, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, -120, 776, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, -80, 950, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, -40, 1050, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 0, -120, 800, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 0, -80, 925, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 0, -40, 1000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, -120, 750, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, -80, 875, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 40, -40, 1000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -120, 725, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, -80, 875, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, -40, 925, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -16, 930, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_80181E48[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 1, 0 } },
    { 22, 33, 0, 0, { 2, 0 } },
    { 55, 22, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_80181E70[99] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -24, 1600, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, -40, 1525, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -32, 1624, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, -16, 1575, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -48, 0, 1612, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -24, 1542, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -8, 1543, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 8, 1565, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -104, 1361, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, -104, 1387, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -96, -104, 1421, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -64, -104, 1464, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, -80, 1908, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -80, -64, 1479, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -56, -64, 1503, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -48, 200, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -8, 200, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 32, 225, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 72, 225, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, -48, 250, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, -16, 250, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, 16, 262, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, 48, 275, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 80, 237, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -120, -40, 308, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -120, -8, 300, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -120, 24, 300, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -120, 56, 300, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -120, 88, 300, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, -40, 375, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, -8, 387, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, 24, 387, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, 56, 381, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, 88, 387, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -88, -40, 537, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -88, -8, 531, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -88, 24, 512, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -88, 56, 475, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -88, 88, 475, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -72, -40, 823, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -72, -8, 800, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -72, 24, 825, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -72, 56, 825, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -72, 88, 750, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 8, 1143, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -56, 24, 1143, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -56, 48, 925, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -56, 72, 825, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -56, 96, 750, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -32, 500, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 8, 512, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 48, 525, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, 88, 525, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, -48, 537, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, -8, 537, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 32, 550, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 112, 72, 562, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -48, 575, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -8, 575, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, 32, 587, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 72, 593, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, -48, 625, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, -8, 637, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, 32, 637, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 72, 662, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 48, -48, 775, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 48, -16, 775, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 48, 16, 781, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 48, 48, 781, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 80, 787, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 32, -48, 900, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, -16, 912, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 32, 16, 912, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 32, 48, 918, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, 80, 916, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -40, 1125, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 16, -8, 1162, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 16, 24, 1162, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, 56, 1125, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -40, 1200, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, -16, 1200, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 8, 1212, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 24, 1225, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 128, -120, 971, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 128, -88, 978, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 128, -56, 985, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 104, -120, 1215, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 104, -88, 1325, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -56, 1333, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 80, -104, 1565, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -32, 1344, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, -96, 1790, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -72, 1825, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, -48, 1825, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, -96, 1926, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, -72, 1982, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -48, 1890, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -72, 1583, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -72, 1450, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_8018262C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 8, 7, 0, 0, { 3, 0 } },
    { 15, 34, 0, 0, { 2, 0 } },
    { 49, 34, 0, 0, { 4, 0 } },
    { 83, 16, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_80182664[90] = {
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 8, -64, 1437, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 8, -40, 1450, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 8, -16, 1443, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 8, 1450, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 24, -64, 1362, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 24, -40, 1416, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 24, -16, 1441, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, 8, 1450, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -32, 1075, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -16, 1066, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 56, 8, 1087, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 1075, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 56, 56, 1075, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 56, 80, 800, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 24, 1087, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, 40, 1100, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 64, -48, 900, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 64, -16, 900, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 64, 16, 900, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 64, 48, 800, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 80, 775, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, -48, 540, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 96, -48, 540, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, -16, 619, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 96, -16, 475, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, 16, 550, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 96, 16, 475, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, 48, 550, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 96, 48, 481, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 80, 562, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 80, 487, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, -40, 425, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 0, 429, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 40, 431, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 80, 431, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, -40, 375, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 8, 375, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 56, 375, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 136, 96, 375, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -120, 853, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -88, 866, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -56, 975, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -136, -120, 971, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -136, -88, 988, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -136, -56, 997, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -112, -112, 1098, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, -88, 1119, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, -56, 1100, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -96, -104, 1229, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -96, -72, 1256, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, -40, 475, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, 8, 487, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, 56, 487, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 104, 500, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, -40, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, 8, 512, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, 56, 512, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -128, 104, 525, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -104, -40, 550, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -104, 8, 562, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -104, 56, 568, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -104, 104, 568, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, -56, 525, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, -56, 537, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -48, 609, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -16, 681, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, 16, 600, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, 48, 600, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, 80, 625, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, 104, 625, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, -40, 737, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, -40, 855, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -40, 975, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -24, 736, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, -24, 825, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, 8, 725, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -48, 8, 825, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -64, 40, 725, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -48, 40, 825, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, 72, 737, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 104, 750, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, 72, 825, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -24, 975, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -32, -8, 987, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -32, 16, 1000, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -32, 40, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 64, 1000, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 0, 1112, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 16, 1112, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -16, 32, 1125, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_80182D6C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 31, 0, 0, { 0, 0 } },
    { 39, 11, 0, 0, { 2, 0 } },
    { 50, 40, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_80182D9C[81] = {
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, -88, 1291, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, -64, 1371, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -160, -40, 1435, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, -88, 1397, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, -64, 1388, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -144, -40, 1453, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, -88, 1425, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, -72, 1422, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, -56, 1418, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, -40, 1466, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -112, -80, 1540, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -112, -64, 1576, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, -48, 1500, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -64, -56, 903, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 332, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 396, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -112, 80, 500, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -112, 40, 575, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 80, 725, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -56, 80, 775, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 0, 447, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -136, 0, 481, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -80, 56, 775, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -56, 56, 875, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -136, -16, 551, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -112, 16, 522, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -112, -8, 530, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -112, -32, 599, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, -32, 610, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -120, -48, 797, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, -48, 743, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -80, 40, 925, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -88, 16, 675, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -64, 16, 1025, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, -8, 750, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -64, -8, 1125, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 0, 1125, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -88, -32, 750, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -64, -32, 1125, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, -48, 718, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, -48, 1125, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, -64, 726, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, -64, 865, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, -64, 935, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, -72, 881, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, -72, 912, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, -72, 1335, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, -56, 1375, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, -40, 1375, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -16, -24, 1282, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -8, -80, 1415, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -64, 1214, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -48, 1216, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -8, -32, 1242, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, 104, -120, 500, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, -56, 500, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, 0, 525, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, 104, 56, 550, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 56, -120, 625, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 56, -72, 650, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 56, -24, 650, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 56, 24, 700, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 56, 72, 725, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 32, -120, 775, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 32, -72, 800, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 32, -24, 850, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 32, 24, 900, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 72, 900, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, 96, 900, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 16, -120, 875, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 16, -88, 900, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 16, -48, 950, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 16, -8, 1000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 32, 1000, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, 48, 975, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -120, 937, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 0, -104, 950, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 0, -72, 975, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 0, -40, 1100, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -8, 1100, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, 8, 1125, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_801833F0[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 3, 0 } },
    { 13, 33, 0, 0, { 0, 0 } },
    { 46, 8, 0, 0, { 2, 0 } },
    { 54, 27, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_80183420[50] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -56, 725, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -56, 810, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -64, 767, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -56, 738, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, -48, 792, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -48, 703, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, -48, 690, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 112, -48, 650, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, -32, 875, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, -32, 680, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 88, -32, 680, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 104, -32, 601, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, -32, 625, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -8, 1000, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, -8, 700, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, -8, 575, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 96, -8, 514, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 112, -8, 541, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 128, -8, 475, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 144, -8, 450, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, 16, 1025, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, 40, 900, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 64, 825, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, 96, 826, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 96, 825, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 64, 16, 700, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 96, 16, 440, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 16, 375, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 64, 56, 737, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 96, 56, 400, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 128, 56, 361, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 64, 96, 700, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 96, 425, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 96, 327, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -120, 508, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -64, 517, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, -160, -8, 529, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, -160, 56, 544, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -112, -120, 601, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -112, -64, 618, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, -112, -8, 625, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, -112, 56, 625, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -64, -120, 625, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -64, -64, 750, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -64, -8, 750, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -64, 48, 750, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -32, -120, 697, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -32, -80, 900, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -32, -40, 950, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, -8, 963, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_80183808[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 1, 0 } },
    { 34, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_80183828[34] = {
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -104, -120, 700, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -104, -56, 650, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -104, 8, 650, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -104, 72, 650, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -72, -120, 1025, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -72, -72, 900, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -72, -24, 900, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -72, 24, 875, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -72, 72, 850, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, -104, 1100, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, -72, 1100, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -40, 1100, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -72, 1221, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -56, 1125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -40, 1125, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -24, 1125, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -8, 1075, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 8, 1050, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 32, 1025, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 56, 1000, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 80, 1000, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -48, 1225, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, -32, 1225, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -16, 1225, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, -160, -120, 575, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, -160, -56, 525, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, -160, 8, 525, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, -160, 72, 550, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, 40, 935, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, 40, 1000, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 80, 918, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 1000, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, 0, 952, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, 0, 1025, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_80183AD0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_80183AE8[41] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -48, 32, 800, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -32, 72, 800, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -32, 112, 321, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, 16, 832, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -56, 16, 800, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, 32, 684, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 32, 762, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, 48, 618, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 48, 656, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, 48, 800, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, 40, 800, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, 56, 800, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, 64, 454, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, 64, 454, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 64, 454, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 64, 750, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -112, 88, 353, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -88, 88, 352, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -64, 88, 352, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, 88, 750, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 120, -120, 575, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 120, -56, 600, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 120, 0, 587, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 56 } }, 120, 64, 600, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 80, -120, 670, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 80, -56, 675, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 80, 0, 687, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 80, 64, 687, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 48, -120, 882, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 48, -56, 900, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 48, 0, 900, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 48, 48, 900, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 96, 780, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 24, -112, 1250, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 24, -56, 1300, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 24, 0, 1300, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, 56, 1198, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 0, -72, 1875, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 0, -32, 1875, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 8, 1875, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 40, 1508, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_80183E1C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 1, 0 } },
    { 20, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_general_store_80183E3C[22] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 8, 842, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 120, -120, 500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 120, -48, 500, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 120, 24, 550, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 88, -120, 511, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 88, -56, 600, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 88, 8, 625, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 88, 72, 675, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, -120, 633, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, -72, 663, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, -24, 721, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 24, 800, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 72, 712, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, -120, 762, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 40, -88, 828, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 40, -56, 861, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 40, -24, 898, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 32, 849, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, -32, 801, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 8, 835, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, 96, 550, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 96, 550, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_general_store_80183FF4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_general_store_8018400C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_general_store_8018401C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_general_store_8018402C[16] = {
    { { .empty = D_dryfield_general_store_8017F49C }, D_dryfield_general_store_8017F49C, NULL },
    { { .elements = D_dryfield_general_store_8017F4AC }, D_dryfield_general_store_8017FC90, NULL },
    { { .elements = D_dryfield_general_store_8017FCE8 }, D_dryfield_general_store_8018051C, NULL },
    { { .elements = D_dryfield_general_store_80180554 }, D_dryfield_general_store_80180BF8, NULL },
    { { .elements = D_dryfield_general_store_80180C40 }, D_dryfield_general_store_801811CC, NULL },
    { { .elements = D_dryfield_general_store_80181204 }, D_dryfield_general_store_8018181C, NULL },
    { { .elements = D_dryfield_general_store_80181844 }, D_dryfield_general_store_80181E48, NULL },
    { { .elements = D_dryfield_general_store_80181E70 }, D_dryfield_general_store_8018262C, NULL },
    { { .elements = D_dryfield_general_store_80182664 }, D_dryfield_general_store_80182D6C, NULL },
    { { .elements = D_dryfield_general_store_80182D9C }, D_dryfield_general_store_801833F0, NULL },
    { { .elements = D_dryfield_general_store_80183420 }, D_dryfield_general_store_80183808, NULL },
    { { .elements = D_dryfield_general_store_80183828 }, D_dryfield_general_store_80183AD0, NULL },
    { { .elements = D_dryfield_general_store_80183AE8 }, D_dryfield_general_store_80183E1C, NULL },
    { { .elements = D_dryfield_general_store_80183E3C }, D_dryfield_general_store_80183FF4, NULL },
    { { .empty = D_dryfield_general_store_8018400C }, D_dryfield_general_store_8018400C, NULL },
    { { .empty = D_dryfield_general_store_8018401C }, D_dryfield_general_store_8018401C, NULL },
};

WorldCollisionTrigger D_dryfield_general_store_801840EC[28] = {
    { NULL, NULL, NULL, { 2000, -1216, 4064, 0 }, { { 1872, 2240, 0, 0 }, { 1872, -2240, 0, 0 }, { -1872, 2240, 0, 0 }, { -1872, -2240, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 2918, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1920, -1232, 4320, 0 }, { { -1872, 2256, 0, 0 }, { -1872, -2256, 0, 0 }, { 1872, 2256, 0, 0 }, { 1872, -2256, 0, 0 } }, { 0, 0, -4125, 0 }, { 0, 0, 4096, 0 }, 2930, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3616, -1232, 784, 0 }, { { 0, 2256, -1088, 0 }, { 0, -2256, -1088, 0 }, { 0, 2256, 1088, 0 }, { 0, -2256, 1088, 0 } }, { 4111, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2495, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3808, -1248, 912, 0 }, { { 0, 2272, 1312, 0 }, { 0, -2272, 1312, 0 }, { 0, 2272, -1312, 0 }, { 0, -2272, -1312, 0 } }, { -4101, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6176, -1232, 832, 0 }, { { 0, 2256, 1104, 0 }, { 0, -2256, 1104, 0 }, { 0, 2256, -1104, 0 }, { 0, -2256, -1104, 0 } }, { -4104, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2508, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5935, -1216, 751, 0 }, { { 12, 2240, -1281, 0 }, { 12, -2240, -1281, 0 }, { -11, 2240, 1282, 0 }, { -11, -2240, 1282, 0 } }, { 4110, 0, 36, 0 }, { 0, 0, 4096, 0 }, 2572, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7712, -1200, 608, 0 }, { { 0, 2224, -1136, 0 }, { 0, -2224, -1136, 0 }, { 0, 2224, 1136, 0 }, { 0, -2224, 1136, 0 } }, { 4114, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2495, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7872, -1216, 528, 0 }, { { 0, 2240, 1216, 0 }, { 0, -2240, 1216, 0 }, { 0, 2240, -1216, 0 }, { 0, -2240, -1216, 0 } }, { -4114, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8863, -1200, 3647, 0 }, { { -1247, 2224, 448, 0 }, { -1247, -2224, 448, 0 }, { 1248, 2224, -447, 0 }, { 1248, -2224, -447, 0 } }, { -1386, 0, -3863, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8894, -1184, 3422, 0 }, { { 1440, 2208, -511, 0 }, { 1440, -2208, -511, 0 }, { -1439, 2208, 512, 0 }, { -1439, -2208, 512, 0 } }, { 1372, 0, 3863, 0 }, { 0, 0, 4096, 0 }, 2684, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7583, -1104, 3038, 0 }, { { 0, 2128, -1008, 0 }, { 0, -2128, -1008, 0 }, { 0, 2128, 1008, 0 }, { 0, -2128, 1008, 0 } }, { 4123, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 6, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7808, -1200, 2975, 0 }, { { 0, 2224, 912, 0 }, { 0, -2224, 912, 0 }, { 0, 2224, -912, 0 }, { 0, -2224, -912, 0 } }, { -4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 9, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6272, -1232, 2976, 0 }, { { 0, 2256, 848, 0 }, { 0, -2256, 848, 0 }, { 0, 2256, -848, 0 }, { 0, -2256, -848, 0 } }, { -4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6048, -1216, 2976, 0 }, { { 0, 2240, -784, 0 }, { 0, -2240, -784, 0 }, { 0, 2240, 784, 0 }, { 0, -2240, 784, 0 } }, { 4101, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3584, -1200, 2912, 0 }, { { 0, 2224, -816, 0 }, { 0, -2224, -816, 0 }, { 0, 2224, 816, 0 }, { 0, -2224, 816, 0 } }, { 4102, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 8, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3744, -1152, 2912, 0 }, { { 0, 2176, 816, 0 }, { 0, -2176, 816, 0 }, { 0, 2176, -816, 0 }, { 0, -2176, -816, 0 } }, { -4102, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 2, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3808, -1136, 5056, 0 }, { { 0, 2160, -784, 0 }, { 0, -2160, -784, 0 }, { 0, 2160, 784, 0 }, { 0, -2160, 784, 0 } }, { 4105, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2289, 0, 10, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3968, -1168, 5056, 0 }, { { 0, 2192, 816, 0 }, { 0, -2192, 816, 0 }, { 0, 2192, -816, 0 }, { 0, -2192, -816, 0 } }, { -4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 3, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6240, -1184, 5120, 0 }, { { 0, 2208, 752, 0 }, { 0, -2208, 752, 0 }, { 0, 2208, -752, 0 }, { 0, -2208, -752, 0 } }, { -4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 10, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5984, -1168, 5056, 0 }, { { 0, 2192, -848, 0 }, { 0, -2192, -848, 0 }, { 0, 2192, 848, 0 }, { 0, -2192, 848, 0 } }, { 4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 11, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7648, -1168, 5024, 0 }, { { 0, 2192, -912, 0 }, { 0, -2192, -912, 0 }, { 0, 2192, 912, 0 }, { 0, -2192, 912, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2374, 0, 7, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7840, -1136, 4960, 0 }, { { 0, 2160, 848, 0 }, { 0, -2160, 848, 0 }, { 0, 2160, -848, 0 }, { 0, -2160, -848, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 11, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1408, -1152, 5856, 0 }, { { 1136, 2176, 0, 0 }, { 1136, -2176, 0, 0 }, { -1136, 2176, 0, 0 }, { -1136, -2176, 0, 0 } }, { 0, 0, 4117, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 12, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1376, -1216, 5984, 0 }, { { -1072, 2240, 0, 0 }, { -1072, -2240, 0, 0 }, { 1072, 2240, 0, 0 }, { 1072, -2240, 0, 0 } }, { 0, 0, -4118, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 3, 12, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3905, -1120, 7072, 0 }, { { 0, 2144, -1104, 0 }, { 0, -2144, -1104, 0 }, { 0, 2144, 1104, 0 }, { 0, -2144, 1104, 0 } }, { 4107, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 13, 12, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4162, -1184, 7072, 0 }, { { 0, 2208, 1168, 0 }, { 0, -2208, 1168, 0 }, { 0, 2208, -1168, 0 }, { 0, -2208, -1168, 0 } }, { -4114, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2495, 0, 12, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7648, -1200, 7264, 0 }, { { 0, 2224, -1168, 0 }, { 0, -2224, -1168, 0 }, { 0, 2224, 1168, 0 }, { 0, -2224, 1168, 0 } }, { 4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2508, 0, 14, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7904, -1200, 7040, 0 }, { { 0, 2224, 1232, 0 }, { 0, -2224, 1232, 0 }, { 0, 2224, -1232, 0 }, { 0, -2224, -1232, 0 } }, { -4101, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2534, 0, 13, 14, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_general_store_8018493C[21] = {
    { NULL, NULL, NULL, { 2816, -48, 224, 0 }, { { -480, 0, -224, 0 }, { 480, 0, -224, 0 }, { -480, 0, 224, 0 }, { 480, 0, 224, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 527, WORLD_COLLISION_TRIGGER_ACTION_WARP, 1, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9792, -48, 4736, 0 }, { { 224, 0, -480, 0 }, { 224, 0, 480, 0 }, { -224, 0, -480, 0 }, { -224, 0, 480, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 527, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8704, -48, 7488, 0 }, { { 224, 0, -480, 0 }, { 224, 0, 480, 0 }, { -224, 0, -480, 0 }, { -224, 0, 480, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 527, WORLD_COLLISION_TRIGGER_ACTION_WARP, 38, 51, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1680, -64, 2432, 0 }, { { -976, 0, -864, 0 }, { 976, 0, -864, 0 }, { -976, 0, 864, 0 }, { 976, 0, 864, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5888, -64, 3360, 0 }, { { -2224, 0, -336, 0 }, { 2224, 0, -336, 0 }, { -2224, 0, 336, 0 }, { 2224, 0, 336, 0 } }, { 0, 4107, 0, 0 }, { 200, 0, -4091, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5824, -64, 1344, 0 }, { { -2224, 0, -256, 0 }, { 2224, 0, -256, 0 }, { -2224, 0, 256, 0 }, { 2224, 0, 256, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, -4096, 0 }, 2231, WORLD_COLLISION_TRIGGER_ACTION_CAP, 23, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7568, -64, 5520, 0 }, { { -1344, 0, -464, 0 }, { 1344, 0, -464, 0 }, { -1344, 0, 464, 0 }, { 1344, 0, 464, 0 } }, { 0, 4108, 0, 0 }, { 201, 0, -4091, 0 }, 1419, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1056, -64, 6976, 0 }, { { -448, 0, -608, 0 }, { 448, 0, -608, 0 }, { -448, 0, 608, 0 }, { 448, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 3920, 0, -1190, 0 }, 754, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3968, -64, 6432, 0 }, { { -800, 0, -352, 0 }, { 800, 0, -352, 0 }, { -800, 0, 352, 0 }, { 800, 0, 352, 0 } }, { 0, 4119, 0, 0 }, { -201, 0, 4091, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4880, -64, 5504, 0 }, { { -1248, 0, -416, 0 }, { 1248, 0, -416, 0 }, { -1248, 0, 416, 0 }, { 1248, 0, 416, 0 } }, { 0, 4095, 0, 0 }, { 201, 0, -4091, 0 }, 1311, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9568, -64, 2032, 0 }, { { -528, 0, -1344, 0 }, { 528, 0, -1344, 0 }, { -528, 0, 1344, 0 }, { 528, 0, 1344, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2704, -64, 5312, 0 }, { { -832, 0, -528, 0 }, { 832, 0, -528, 0 }, { -832, 0, 528, 0 }, { 832, 0, 528, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 985, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5856, -64, 2672, 0 }, { { -2224, 0, -352, 0 }, { 2224, 0, -352, 0 }, { -2224, 0, 352, 0 }, { 2224, 0, 352, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4095, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5920, -64, 4576, 0 }, { { -2224, 0, -368, 0 }, { 2224, 0, -368, 0 }, { -2224, 0, 368, 0 }, { 2224, 0, 368, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4095, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -64, 4560, 0 }, { { -624, 0, -1072, 0 }, { 624, 0, -1072, 0 }, { -624, 0, 1072, 0 }, { 624, 0, 1072, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1234, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8240, -64, 6384, 0 }, { { 432, 0, -304, 0 }, { 432, 0, 304, 0 }, { -432, 0, -304, 0 }, { -432, 0, 304, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 527, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5024, -64, 7136, 0 }, { { 432, 0, -1072, 0 }, { 432, 0, 1072, 0 }, { -432, 0, -1072, 0 }, { -432, 0, 1072, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 1616, -64, 6368, 0 }, { { 672, 0, -368, 0 }, { 672, 0, 368, 0 }, { -672, 0, -368, 0 }, { -672, 0, 368, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 765, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 4160, -64, 610, 0 }, { { 512, 0, -368, 0 }, { 512, 0, 368, 0 }, { -512, 0, -368, 0 }, { -512, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 630, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6544, -64, 160, 0 }, { { 1744, 0, -368, 0 }, { 1744, 0, 368, 0 }, { -1744, 0, -368, 0 }, { -1744, 0, 368, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 1778, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 336, -64, 800, 0 }, { { 512, 0, -944, 0 }, { 512, 0, 944, 0 }, { -512, 0, -944, 0 }, { -512, 0, 944, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1070, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_general_store_80184F78[4] = {
    { NULL, NULL, { 6128, -1376, 6144, 0 }, { { -4176, -2496, 0, 0 }, { 4176, -2496, 0, 0 }, { -4176, 2496, 0, 0 }, { 4176, 2496, 0, 0 } }, { 0, 0, -4113, 0 }, 4857, 1, 0 },
    { NULL, NULL, { 5872, -160, 4128, 0 }, { { -2144, -1152, 0, 0 }, { 2144, -1152, 0, 0 }, { -2144, 1152, 0, 0 }, { 2144, 1152, 0, 0 } }, { 0, 0, -4114, 0 }, 2428, 1, 0 },
    { NULL, NULL, { 5889, -16, 2016, 0 }, { { -2144, -1328, 0, 0 }, { 2144, -1328, 0, 0 }, { -2144, 1328, 0, 0 }, { 2144, 1328, 0, 0 } }, { 0, 0, -4096, 0 }, 2521, 1, 0 },
    { NULL, NULL, { 1824, -64, 1568, 0 }, { { 0, -768, 1376, 0 }, { 0, -640, -1376, 0 }, { 0, 640, 1376, 0 }, { 0, 768, -1376, 0 } }, { -4100, 0, 0, 0 }, 1572, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_general_store_80185068[12] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4855, -1177, 5307 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 901, 1556, 1966 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7855, -1177, 5307 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 901, 1556, 1966 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8855, -1177, 5307 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 901, 1556, 1966 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -1745, 900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -1745, 900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6750, -1745, 900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8000, -1745, 900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -3505, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3768 }, { 0, 0 } }, 3000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -1250, 7700 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2375, 2211, 1966 }, { 0, 0 } }, 4000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1497, -1750, 6760 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3686, 2867 }, { 0, 0 } }, 250, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6855, -1177, 5307 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 901, 1556, 1966 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5855, -1177, 5307 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 901, 1556, 1966 }, { 0, 0 } }, 1000, 1500 },
};

WorldCoordRoomLights D_dryfield_general_store_801854E8[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_general_store_80185068), D_dryfield_general_store_80185068, 0, NULL },
};

WorldCoordRoomAmbientEntry D_dryfield_general_store_80185500[17] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_general_store_80185500) - 1 },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 479, 480, 482, 479 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 478, 482, 480, 480 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
};

AreaResource D_dryfield_general_store_80185588[2] = {
    { 40, 40, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor04000_D0C6E0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_general_store_801855A0[3] = {
    { 140, 236, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_223600_80150B48 },
    { 40, 40, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor04000_D0C6E0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_general_store_801855C4[9] = {
    { 140, 0, 0, -3473, 0, 993, 4096, 0, 0, 2, 0 },
    { 140, 0, 0, -3473, 0, 993, 4096, 0, 0, 2, 0 },
    { 140, 0, 0, -3473, 0, 993, 4096, 0, 0, 2, 0 },
    { 40, 0, 0, 3470, 0, 3606, 1024, 0, 0, 2, 0 },
    { 40, 0, 0, 1048, 0, 4979, -1024, 0, 0, 2, 0 },
    { 40, 0, 0, 3468, 0, 4772, 0, 0, 0, 2, 0 },
    { 40, 0, 0, 1234, 0, 3641, 128, 0, 0, 2, 0 },
    { 40, 0, 0, -1234, 0, 3641, -128, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_dryfield_general_store_80185654[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017AE44, D_dryfield_general_store_80185588 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_general_store_801855C4, D_dryfield_general_store_801855A0 },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_dryfield_general_store_801856BC = {
    0x10000051,
    0x10000053,
    0x10000054,
};

WorldCollisionSurfaceProperties D_dryfield_general_store_801856C8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_general_store_801856D0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_general_store_801856BC },
};

WorldCollisionSurfaceProperties* D_dryfield_general_store_801856D8[8] = {
    D_dryfield_general_store_801856C8,
    D_dryfield_general_store_801856D0,
    D_dryfield_general_store_801856C8,
    D_dryfield_general_store_801856C8,
    D_dryfield_general_store_801856C8,
    D_dryfield_general_store_801856C8,
    D_dryfield_general_store_801856C8,
    D_dryfield_general_store_801856C8,
};

u8 gStoreSavedView = 0;

ScreenFade gStoreFade = { 0 };

RoomEventMsg gRoomEventMsg = { 0 };

u8 gRoomEventActive = 0;

u8 gStoreWarp = 0;

u8 gStoreRoom = 0;

u8 D_dryfield_general_store_8018570B = 208;

RoomEventReq gRoomEventReq = { 0, 0, 0, 0, 0, 0 };

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/general_store_door_msg.inc.c"

#include "../../shared/general_store_cutscene_task.inc.c"

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_general_store_8017DF5C`: the entry state
/// `func_dryfield_general_store_8017DEAC`, the idle state
/// `func_dryfield_general_store_8017DF4C`, then `taskKill`.
static const TaskFuncTable3 D_dryfield_general_store_8017D5F4 = {
    { func_dryfield_general_store_8017DEAC, func_dryfield_general_store_8017DF4C, taskKill },
};

#include "../../shared/general_store_toggle_task.inc.c"

#include "../../shared/general_store_action_msg.inc.c"

#include "../../shared/general_store_sound_msg.inc.c"

/// Message handler that takes no action and reports the message as not
/// handled.
s32 func_dryfield_general_store_8017DDF4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message handler on the slot-4 table that owns the store's story flag 0x5E:
/// message 1 spawns the cutscene task once the flag is still clear, message 2
/// arms the cutscene object and then both paths advance the flag.
s32 func_dryfield_general_store_8017DDFC(Task* task, s32 msgId, RoomEventMsg* arg2, s32 arg3)
{
    switch (arg2->warp) {
        case 1:
            if (gameFlagGetNibble(GAME_FLAG_GENERAL_STORE_CUTSCENE_STATE) == 0) {
                Task_SpawnFromTable(&D_dryfield_general_store_8017E4C0, 0, 0, 0);
                gameFlagSetNibble(GAME_FLAG_GENERAL_STORE_CUTSCENE_STATE, 1);
            }
            break;
        case 2:
            if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED && gameFlagGetNibble(GAME_FLAG_GENERAL_STORE_CUTSCENE_STATE) == 1) {
                func_800E8614(D_dryfield_general_store_8017E568, 1);
            }
            gameFlagSetNibble(GAME_FLAG_GENERAL_STORE_CUTSCENE_STATE, 2);
            break;
    }
    return 0;
}

static void func_dryfield_general_store_8017DEAC(Task* arg0)
{
    arg0->msgTable = D_dryfield_general_store_8017E188;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_GENERAL_STORE_CUTSCENE_STATE) == 0) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_dryfield_general_store_8017E1B8, ACTOR_COMMAND_MESSAGE_APPLY);
    } else if (gameFlagGetNibble(GAME_FLAG_GENERAL_STORE_CUTSCENE_STATE) == 1) {
        gameFlagSetNibble(GAME_FLAG_GENERAL_STORE_CUTSCENE_STATE, 2);
    }
    arg0->state = arg0->state + 1;
    D_80115598  = 1;
}

/// Idle state of the room task: does nothing, though it reserves a stack
/// frame.
static void func_dryfield_general_store_8017DF4C(Task* task)
{
    char pad[0x10];
}

/// The room task: runs the state `D_dryfield_general_store_8017D5F4` names for
/// `task->state`, through a stack copy of the table.
void func_dryfield_general_store_8017DF5C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_general_store_8017D5F4;
    sp.funcs[task->state](task);
}

void func_dryfield_general_store_8017DFB4(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_dryfield_general_store_8017E55C, ACTOR_COMMAND_MESSAGE_APPLY);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GENERAL_STORE, 0x0F), 0, 0);
            arg0->killCountdown = 0x5A;
            arg0->state++;
            return;
        case 1:
            temp_v0             = (u16)arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if (temp_v0 < 0) {
                taskKill(arg0);
            }
            return;
    }
}

void func_dryfield_general_store_8017E064(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_dryfield_general_store_8017E560, ACTOR_COMMAND_MESSAGE_APPLY);
            arg0->killCountdown = 0x5A;
            arg0->state++;
            return;
        case 1:
            temp_v0             = (u16)arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if (temp_v0 < 0) {
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_dryfield_general_store_8017E564, ACTOR_COMMAND_MESSAGE_APPLY);
                Gp_MsgPlayerWeapon(1);
                taskKill(arg0);
            }
            return;
    }
}

/// Arms `gSceneCombatState` with `arg0`.
void func_dryfield_general_store_8017E130(s32 arg0)
{
    Gp_ArmStateF0(arg0);
}

void func_dryfield_general_store_8017E150(Task* unused)
{
}
