#include "rooms/dryfield_night_dilapidated_house.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "dryfield_night_dilapidated_house_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/gamemain.h"
#include "main/wipsys_types.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room/room_events.h"
#include "../../shared/effects/glow_draw.h"

static WorldCollisionGrid _gDryfieldNightDilapidatedHouseCollisionGrid;

static WorldCollisionTrigger _gDryfieldNightDilapidatedHouseViewBoundaries[8];

static void _dryfieldNightDilapidatedHouseMovieTask(Task* task);
static void _dryfieldNightDilapidatedHouseLaunchMovieTask(Task* task);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

TaskMessageEntry gDryfieldNightDilapidatedHouseRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, dryfieldNightDilapidatedHouseGateBackStreetExit },
    { ROOM_MESSAGE_USE_KEY_ITEM, dryfieldNightDilapidatedHouseRefuseKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, dryfieldNightDilapidatedHouseIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, dryfieldNightDilapidatedHouseIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation01444Bank1[6] = {
#include "assets/dryfield_night_dilapidated_house_animation_01444_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation01444Bank4[46] = {
#include "assets/dryfield_night_dilapidated_house_animation_01444_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation01444Records[109] = {
#include "assets/dryfield_night_dilapidated_house_animation_01444_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation01444Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_01444_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation01444 = {
    _gDryfieldNightDilapidatedHouseAnimation01444Records,
    _gDryfieldNightDilapidatedHouseAnimation01444Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation01444Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation01444Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation01C10Bank1[22] = {
#include "assets/dryfield_night_dilapidated_house_animation_01C10_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation01C10Bank4[182] = {
#include "assets/dryfield_night_dilapidated_house_animation_01C10_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation01C10Records[231] = {
#include "assets/dryfield_night_dilapidated_house_animation_01C10_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation01C10Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_01C10_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation01C10 = {
    _gDryfieldNightDilapidatedHouseAnimation01C10Records,
    _gDryfieldNightDilapidatedHouseAnimation01C10Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation01C10Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation01C10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation01DE4Bank1[2] = {
#include "assets/dryfield_night_dilapidated_house_animation_01DE4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation01DE4Bank4[15] = {
#include "assets/dryfield_night_dilapidated_house_animation_01DE4_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation01DE4Records[76] = {
#include "assets/dryfield_night_dilapidated_house_animation_01DE4_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation01DE4Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_01DE4_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation01DE4 = {
    _gDryfieldNightDilapidatedHouseAnimation01DE4Records,
    _gDryfieldNightDilapidatedHouseAnimation01DE4Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation01DE4Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation01DE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation0219CBank1[7] = {
#include "assets/dryfield_night_dilapidated_house_animation_0219C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation0219CBank4[82] = {
#include "assets/dryfield_night_dilapidated_house_animation_0219C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation0219CRecords[115] = {
#include "assets/dryfield_night_dilapidated_house_animation_0219C_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation0219CIndices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0219C_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation0219C = {
    _gDryfieldNightDilapidatedHouseAnimation0219CRecords,
    _gDryfieldNightDilapidatedHouseAnimation0219CIndices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation0219CBank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation0219CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation02368Bank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_02368_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation02368Bank4[19] = {
#include "assets/dryfield_night_dilapidated_house_animation_02368_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation02368Records[67] = {
#include "assets/dryfield_night_dilapidated_house_animation_02368_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation02368Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_02368_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation02368 = {
    _gDryfieldNightDilapidatedHouseAnimation02368Records,
    _gDryfieldNightDilapidatedHouseAnimation02368Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation02368Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation02368Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation02734Bank1[6] = {
#include "assets/dryfield_night_dilapidated_house_animation_02734_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation02734Bank4[64] = {
#include "assets/dryfield_night_dilapidated_house_animation_02734_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation02734Records[141] = {
#include "assets/dryfield_night_dilapidated_house_animation_02734_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation02734Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_02734_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation02734 = {
    _gDryfieldNightDilapidatedHouseAnimation02734Records,
    _gDryfieldNightDilapidatedHouseAnimation02734Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation02734Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation02734Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation02A4CBank1[7] = {
#include "assets/dryfield_night_dilapidated_house_animation_02A4C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation02A4CBank4[54] = {
#include "assets/dryfield_night_dilapidated_house_animation_02A4C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation02A4CRecords[103] = {
#include "assets/dryfield_night_dilapidated_house_animation_02A4C_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation02A4CIndices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_02A4C_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation02A4C = {
    _gDryfieldNightDilapidatedHouseAnimation02A4CRecords,
    _gDryfieldNightDilapidatedHouseAnimation02A4CIndices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation02A4CBank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation02A4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation02E4CBank1[10] = {
#include "assets/dryfield_night_dilapidated_house_animation_02E4C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation02E4CBank4[85] = {
#include "assets/dryfield_night_dilapidated_house_animation_02E4C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation02E4CRecords[121] = {
#include "assets/dryfield_night_dilapidated_house_animation_02E4C_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation02E4CIndices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_02E4C_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation02E4C = {
    _gDryfieldNightDilapidatedHouseAnimation02E4CRecords,
    _gDryfieldNightDilapidatedHouseAnimation02E4CIndices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation02E4CBank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation02E4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation03048Bank1[4] = {
#include "assets/dryfield_night_dilapidated_house_animation_03048_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation03048Bank4[17] = {
#include "assets/dryfield_night_dilapidated_house_animation_03048_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation03048Records[78] = {
#include "assets/dryfield_night_dilapidated_house_animation_03048_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation03048Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_03048_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation03048 = {
    _gDryfieldNightDilapidatedHouseAnimation03048Records,
    _gDryfieldNightDilapidatedHouseAnimation03048Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation03048Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation03048Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation03384Bank1[8] = {
#include "assets/dryfield_night_dilapidated_house_animation_03384_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation03384Bank4[65] = {
#include "assets/dryfield_night_dilapidated_house_animation_03384_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation03384Records[98] = {
#include "assets/dryfield_night_dilapidated_house_animation_03384_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation03384Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_03384_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation03384 = {
    _gDryfieldNightDilapidatedHouseAnimation03384Records,
    _gDryfieldNightDilapidatedHouseAnimation03384Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation03384Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation03384Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation0378CBank1[6] = {
#include "assets/dryfield_night_dilapidated_house_animation_0378C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation0378CBank4[65] = {
#include "assets/dryfield_night_dilapidated_house_animation_0378C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation0378CRecords[155] = {
#include "assets/dryfield_night_dilapidated_house_animation_0378C_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation0378CIndices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0378C_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation0378C = {
    _gDryfieldNightDilapidatedHouseAnimation0378CRecords,
    _gDryfieldNightDilapidatedHouseAnimation0378CIndices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation0378CBank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation0378CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation039CCBank1[4] = {
#include "assets/dryfield_night_dilapidated_house_animation_039CC_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation039CCBank4[42] = {
#include "assets/dryfield_night_dilapidated_house_animation_039CC_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation039CCRecords[70] = {
#include "assets/dryfield_night_dilapidated_house_animation_039CC_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation039CCIndices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_039CC_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation039CC = {
    _gDryfieldNightDilapidatedHouseAnimation039CCRecords,
    _gDryfieldNightDilapidatedHouseAnimation039CCIndices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation039CCBank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation039CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation03DA0Bank1[8] = {
#include "assets/dryfield_night_dilapidated_house_animation_03DA0_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation03DA0Bank4[84] = {
#include "assets/dryfield_night_dilapidated_house_animation_03DA0_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation03DA0Records[117] = {
#include "assets/dryfield_night_dilapidated_house_animation_03DA0_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation03DA0Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_03DA0_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation03DA0 = {
    _gDryfieldNightDilapidatedHouseAnimation03DA0Records,
    _gDryfieldNightDilapidatedHouseAnimation03DA0Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation03DA0Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation03DA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation0410CBank1[7] = {
#include "assets/dryfield_night_dilapidated_house_animation_0410C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation0410CBank4[74] = {
#include "assets/dryfield_night_dilapidated_house_animation_0410C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation0410CRecords[104] = {
#include "assets/dryfield_night_dilapidated_house_animation_0410C_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation0410CIndices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0410C_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation0410C = {
    _gDryfieldNightDilapidatedHouseAnimation0410CRecords,
    _gDryfieldNightDilapidatedHouseAnimation0410CIndices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation0410CBank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation0410CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation04560Bank1[5] = {
#include "assets/dryfield_night_dilapidated_house_animation_04560_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation04560Bank4[80] = {
#include "assets/dryfield_night_dilapidated_house_animation_04560_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation04560Records[162] = {
#include "assets/dryfield_night_dilapidated_house_animation_04560_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation04560Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_04560_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation04560 = {
    _gDryfieldNightDilapidatedHouseAnimation04560Records,
    _gDryfieldNightDilapidatedHouseAnimation04560Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation04560Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation04560Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation047F4Bank1[4] = {
#include "assets/dryfield_night_dilapidated_house_animation_047F4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation047F4Bank4[51] = {
#include "assets/dryfield_night_dilapidated_house_animation_047F4_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation047F4Records[82] = {
#include "assets/dryfield_night_dilapidated_house_animation_047F4_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation047F4Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_047F4_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation047F4 = {
    _gDryfieldNightDilapidatedHouseAnimation047F4Records,
    _gDryfieldNightDilapidatedHouseAnimation047F4Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation047F4Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation047F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation04B20Bank1[7] = {
#include "assets/dryfield_night_dilapidated_house_animation_04B20_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation04B20Bank4[56] = {
#include "assets/dryfield_night_dilapidated_house_animation_04B20_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation04B20Records[106] = {
#include "assets/dryfield_night_dilapidated_house_animation_04B20_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation04B20Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_04B20_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation04B20 = {
    _gDryfieldNightDilapidatedHouseAnimation04B20Records,
    _gDryfieldNightDilapidatedHouseAnimation04B20Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation04B20Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation04B20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation04ED8Bank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_04ED8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation04ED8Bank4[81] = {
#include "assets/dryfield_night_dilapidated_house_animation_04ED8_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation04ED8Records[128] = {
#include "assets/dryfield_night_dilapidated_house_animation_04ED8_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation04ED8Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_04ED8_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation04ED8 = {
    _gDryfieldNightDilapidatedHouseAnimation04ED8Records,
    _gDryfieldNightDilapidatedHouseAnimation04ED8Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation04ED8Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation04ED8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation0522CBank1[5] = {
#include "assets/dryfield_night_dilapidated_house_animation_0522C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation0522CBank4[58] = {
#include "assets/dryfield_night_dilapidated_house_animation_0522C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation0522CRecords[120] = {
#include "assets/dryfield_night_dilapidated_house_animation_0522C_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation0522CIndices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0522C_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation0522C = {
    _gDryfieldNightDilapidatedHouseAnimation0522CRecords,
    _gDryfieldNightDilapidatedHouseAnimation0522CIndices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation0522CBank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation0522CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation05424Bank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_05424_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation05424Bank4[34] = {
#include "assets/dryfield_night_dilapidated_house_animation_05424_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation05424Records[63] = {
#include "assets/dryfield_night_dilapidated_house_animation_05424_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation05424Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_05424_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation05424 = {
    _gDryfieldNightDilapidatedHouseAnimation05424Records,
    _gDryfieldNightDilapidatedHouseAnimation05424Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation05424Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation05424Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation055E8Bank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_055E8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation055E8Bank4[27] = {
#include "assets/dryfield_night_dilapidated_house_animation_055E8_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation055E8Records[57] = {
#include "assets/dryfield_night_dilapidated_house_animation_055E8_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation055E8Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_055E8_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation055E8 = {
    _gDryfieldNightDilapidatedHouseAnimation055E8Records,
    _gDryfieldNightDilapidatedHouseAnimation055E8Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation055E8Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation055E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation058A8Bank1[4] = {
#include "assets/dryfield_night_dilapidated_house_animation_058A8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation058A8Bank4[40] = {
#include "assets/dryfield_night_dilapidated_house_animation_058A8_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation058A8Records[104] = {
#include "assets/dryfield_night_dilapidated_house_animation_058A8_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation058A8Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_058A8_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation058A8 = {
    _gDryfieldNightDilapidatedHouseAnimation058A8Records,
    _gDryfieldNightDilapidatedHouseAnimation058A8Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation058A8Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation058A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation05A6CBank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_05A6C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation05A6CBank4[27] = {
#include "assets/dryfield_night_dilapidated_house_animation_05A6C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation05A6CRecords[57] = {
#include "assets/dryfield_night_dilapidated_house_animation_05A6C_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation05A6CIndices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_05A6C_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation05A6C = {
    _gDryfieldNightDilapidatedHouseAnimation05A6CRecords,
    _gDryfieldNightDilapidatedHouseAnimation05A6CIndices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation05A6CBank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation05A6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation05D8CBank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_05D8C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation05D8CBank4[48] = {
#include "assets/dryfield_night_dilapidated_house_animation_05D8C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation05D8CRecords[123] = {
#include "assets/dryfield_night_dilapidated_house_animation_05D8C_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation05D8CIndices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_05D8C_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation05D8C = {
    _gDryfieldNightDilapidatedHouseAnimation05D8CRecords,
    _gDryfieldNightDilapidatedHouseAnimation05D8CIndices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation05D8CBank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation05D8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation06008Bank1[2] = {
#include "assets/dryfield_night_dilapidated_house_animation_06008_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation06008Bank4[32] = {
#include "assets/dryfield_night_dilapidated_house_animation_06008_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation06008Records[101] = {
#include "assets/dryfield_night_dilapidated_house_animation_06008_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation06008Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_06008_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation06008 = {
    _gDryfieldNightDilapidatedHouseAnimation06008Records,
    _gDryfieldNightDilapidatedHouseAnimation06008Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation06008Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation06008Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation0679CBank1[21] = {
#include "assets/dryfield_night_dilapidated_house_animation_0679C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation0679CBank4[156] = {
#include "assets/dryfield_night_dilapidated_house_animation_0679C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation0679CRecords[246] = {
#include "assets/dryfield_night_dilapidated_house_animation_0679C_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation0679CIndices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0679C_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation0679C = {
    _gDryfieldNightDilapidatedHouseAnimation0679CRecords,
    _gDryfieldNightDilapidatedHouseAnimation0679CIndices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation0679CBank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation0679CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation06D10Bank1[2] = {
#include "assets/dryfield_night_dilapidated_house_animation_06D10_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation06D10Bank4[135] = {
#include "assets/dryfield_night_dilapidated_house_animation_06D10_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation06D10Records[188] = {
#include "assets/dryfield_night_dilapidated_house_animation_06D10_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation06D10Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_06D10_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation06D10 = {
    _gDryfieldNightDilapidatedHouseAnimation06D10Records,
    _gDryfieldNightDilapidatedHouseAnimation06D10Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation06D10Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation06D10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation06EE8Bank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_06EE8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation06EE8Bank4[29] = {
#include "assets/dryfield_night_dilapidated_house_animation_06EE8_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation06EE8Records[60] = {
#include "assets/dryfield_night_dilapidated_house_animation_06EE8_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation06EE8Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_06EE8_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation06EE8 = {
    _gDryfieldNightDilapidatedHouseAnimation06EE8Records,
    _gDryfieldNightDilapidatedHouseAnimation06EE8Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation06EE8Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation06EE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation07304Bank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_07304_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation07304Bank4[80] = {
#include "assets/dryfield_night_dilapidated_house_animation_07304_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation07304Records[154] = {
#include "assets/dryfield_night_dilapidated_house_animation_07304_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation07304Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_07304_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation07304 = {
    _gDryfieldNightDilapidatedHouseAnimation07304Records,
    _gDryfieldNightDilapidatedHouseAnimation07304Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation07304Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation07304Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation074C0Bank1[2] = {
#include "assets/dryfield_night_dilapidated_house_animation_074C0_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation074C0Bank4[25] = {
#include "assets/dryfield_night_dilapidated_house_animation_074C0_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation074C0Records[60] = {
#include "assets/dryfield_night_dilapidated_house_animation_074C0_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation074C0Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_074C0_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation074C0 = {
    _gDryfieldNightDilapidatedHouseAnimation074C0Records,
    _gDryfieldNightDilapidatedHouseAnimation074C0Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation074C0Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation074C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation076A8Bank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_076A8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation076A8Bank4[33] = {
#include "assets/dryfield_night_dilapidated_house_animation_076A8_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation076A8Records[60] = {
#include "assets/dryfield_night_dilapidated_house_animation_076A8_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation076A8Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_076A8_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation076A8 = {
    _gDryfieldNightDilapidatedHouseAnimation076A8Records,
    _gDryfieldNightDilapidatedHouseAnimation076A8Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation076A8Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation076A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation07964Bank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_07964_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation07964Bank4[39] = {
#include "assets/dryfield_night_dilapidated_house_animation_07964_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation07964Records[107] = {
#include "assets/dryfield_night_dilapidated_house_animation_07964_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation07964Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_07964_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation07964 = {
    _gDryfieldNightDilapidatedHouseAnimation07964Records,
    _gDryfieldNightDilapidatedHouseAnimation07964Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation07964Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation07964Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation07B44Bank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_07B44_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation07B44Bank4[31] = {
#include "assets/dryfield_night_dilapidated_house_animation_07B44_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation07B44Records[60] = {
#include "assets/dryfield_night_dilapidated_house_animation_07B44_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation07B44Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_07B44_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation07B44 = {
    _gDryfieldNightDilapidatedHouseAnimation07B44Records,
    _gDryfieldNightDilapidatedHouseAnimation07B44Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation07B44Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation07B44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation07DA4Bank1[5] = {
#include "assets/dryfield_night_dilapidated_house_animation_07DA4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation07DA4Bank4[41] = {
#include "assets/dryfield_night_dilapidated_house_animation_07DA4_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation07DA4Records[76] = {
#include "assets/dryfield_night_dilapidated_house_animation_07DA4_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation07DA4Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_07DA4_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation07DA4 = {
    _gDryfieldNightDilapidatedHouseAnimation07DA4Records,
    _gDryfieldNightDilapidatedHouseAnimation07DA4Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation07DA4Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation07DA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation07FC8Bank1[3] = {
#include "assets/dryfield_night_dilapidated_house_animation_07FC8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation07FC8Bank4[21] = {
#include "assets/dryfield_night_dilapidated_house_animation_07FC8_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation07FC8Records[87] = {
#include "assets/dryfield_night_dilapidated_house_animation_07FC8_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation07FC8Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_07FC8_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation07FC8 = {
    _gDryfieldNightDilapidatedHouseAnimation07FC8Records,
    _gDryfieldNightDilapidatedHouseAnimation07FC8Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation07FC8Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation07FC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation08278Bank1[5] = {
#include "assets/dryfield_night_dilapidated_house_animation_08278_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation08278Bank4[52] = {
#include "assets/dryfield_night_dilapidated_house_animation_08278_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation08278Records[85] = {
#include "assets/dryfield_night_dilapidated_house_animation_08278_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation08278Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_08278_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation08278 = {
    _gDryfieldNightDilapidatedHouseAnimation08278Records,
    _gDryfieldNightDilapidatedHouseAnimation08278Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation08278Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation08278Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation0865CBank1[2] = {
#include "assets/dryfield_night_dilapidated_house_animation_0865C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation0865CBank4[87] = {
#include "assets/dryfield_night_dilapidated_house_animation_0865C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation0865CRecords[136] = {
#include "assets/dryfield_night_dilapidated_house_animation_0865C_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation0865CIndices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0865C_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation0865C = {
    _gDryfieldNightDilapidatedHouseAnimation0865CRecords,
    _gDryfieldNightDilapidatedHouseAnimation0865CIndices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation0865CBank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation0865CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation08C74Bank1[4] = {
#include "assets/dryfield_night_dilapidated_house_animation_08C74_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation08C74Bank4[147] = {
#include "assets/dryfield_night_dilapidated_house_animation_08C74_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation08C74Records[211] = {
#include "assets/dryfield_night_dilapidated_house_animation_08C74_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation08C74Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_08C74_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation08C74 = {
    _gDryfieldNightDilapidatedHouseAnimation08C74Records,
    _gDryfieldNightDilapidatedHouseAnimation08C74Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation08C74Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation08C74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDilapidatedHouseAnimation08ED4Bank1[4] = {
#include "assets/dryfield_night_dilapidated_house_animation_08ED4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDilapidatedHouseAnimation08ED4Bank4[36] = {
#include "assets/dryfield_night_dilapidated_house_animation_08ED4_bank4.inc"
};

static AnimationRecord _gDryfieldNightDilapidatedHouseAnimation08ED4Records[84] = {
#include "assets/dryfield_night_dilapidated_house_animation_08ED4_records.inc"
};

static u16 _gDryfieldNightDilapidatedHouseAnimation08ED4Indices[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_08ED4_indices.inc"
};

static AnimationSet _gDryfieldNightDilapidatedHouseAnimation08ED4 = {
    _gDryfieldNightDilapidatedHouseAnimation08ED4Records,
    _gDryfieldNightDilapidatedHouseAnimation08ED4Indices,
    { NULL, _gDryfieldNightDilapidatedHouseAnimation08ED4Bank1, NULL, NULL, _gDryfieldNightDilapidatedHouseAnimation08ED4Bank4, NULL, NULL, NULL },
};

// Scene playback resolves this stored bank word before dispatch. Blend durations
// count whole normal-rate animation frames; extended set IDs are base + table index.
enum {
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER    = 1,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_PLAYER_OPENING_SET  = 9,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES   = 20,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_MEDIUM_BLEND_FRAMES = 10,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_SHORT_BLEND_FRAMES  = 4,
};

/// Player animation-set extension for the first-visit scene.
///
/// The 25 pointers overwrite bank indices 47..71; entry zero is NULL, and
/// entries 1..24 are the scene's sets 48..71. Clip data stays borrowed from
/// this overlay throughout playback; later extension words are unchanged.
static AnimationSet* _gDryfieldNightDilapidatedHouseFirstVisitPlayerSets[] = {
    NULL,
    &_gDryfieldNightDilapidatedHouseAnimation01444,
    &_gDryfieldNightDilapidatedHouseAnimation01C10,
    &_gDryfieldNightDilapidatedHouseAnimation01DE4,
    &_gDryfieldNightDilapidatedHouseAnimation0219C,
    &_gDryfieldNightDilapidatedHouseAnimation02368,
    &_gDryfieldNightDilapidatedHouseAnimation02734,
    &_gDryfieldNightDilapidatedHouseAnimation02A4C,
    &_gDryfieldNightDilapidatedHouseAnimation02E4C,
    &_gDryfieldNightDilapidatedHouseAnimation03048,
    &_gDryfieldNightDilapidatedHouseAnimation03384,
    &_gDryfieldNightDilapidatedHouseAnimation0378C,
    &_gDryfieldNightDilapidatedHouseAnimation039CC,
    &_gDryfieldNightDilapidatedHouseAnimation03DA0,
    &_gDryfieldNightDilapidatedHouseAnimation0410C,
    &_gDryfieldNightDilapidatedHouseAnimation04560,
    &_gDryfieldNightDilapidatedHouseAnimation047F4,
    &_gDryfieldNightDilapidatedHouseAnimation04B20,
    &_gDryfieldNightDilapidatedHouseAnimation04ED8,
    &_gDryfieldNightDilapidatedHouseAnimation0522C,
    &_gDryfieldNightDilapidatedHouseAnimation05424,
    &_gDryfieldNightDilapidatedHouseAnimation055E8,
    &_gDryfieldNightDilapidatedHouseAnimation058A8,
    &_gDryfieldNightDilapidatedHouseAnimation05A6C,
    &_gDryfieldNightDilapidatedHouseAnimation05D8C,
};
STATIC_ASSERT(ARRAY_SIZE(_gDryfieldNightDilapidatedHouseFirstVisitPlayerSets) <= ANIMATION_BANK_EXTENSION_CAPACITY, _gDryfieldNightDilapidatedHouseFirstVisitPlayerSetsFitsBankExtension);

/// Installs the first-visit player sets in the selected animation-bank extension.
///
/// Copies the complete pointer table during message dispatch, without starting
/// playback. The selected bank must be loaded and writable, and the copied
/// clip pointers remain borrowed from this overlay until playback ends.
static AnimationBankCopyRequest _gDryfieldNightDilapidatedHouseFirstVisitPlayerBankCopy = {
    .sets  = _gDryfieldNightDilapidatedHouseFirstVisitPlayerSets,
    .count = ARRAY_SIZE(_gDryfieldNightDilapidatedHouseFirstVisitPlayerSets),
};

/// Unreferenced request-shaped record for clip 1; its intended receiver is unproven.
static AnimationPlayRequest D_dryfield_night_dilapidated_house_80186528 = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player set-48 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the equipped-weapon bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation48 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player opening animation request for the first-visit scene.
///
/// Uses native bank set 9 with an immediate reset and grid collision enabled.
/// The script resolves the equipped-weapon bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation9 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_PLAYER_OPENING_SET,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced request-shaped records for clips 50 and 51; their receiver is unproven.
static AnimationPlayRequest D_dryfield_night_dilapidated_house_80186564[2] = {
    { .source = { .index = 1 }, .animationId = 50, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = 51, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Player set-52 animation request for the first-visit dialogue cues.
///
/// Blends over 4 whole animation frames and enables grid collision.
/// The script resolves the equipped-weapon bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation52 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_SHORT_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player set-53 animation request for the first-visit dialogue cues.
///
/// Blends over 4 whole animation frames and enables grid collision.
/// The script resolves the equipped-weapon bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation53 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 6,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_SHORT_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced request-shaped record for clip 54; its intended receiver is unproven.
static AnimationPlayRequest D_dryfield_night_dilapidated_house_801865B4 = {
    .source               = { .index = 1 },
    .animationId          = 54,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player set-55 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the equipped-weapon bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation55 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 8,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced request-shaped record for clip 56; its intended receiver is unproven.
static AnimationPlayRequest D_dryfield_night_dilapidated_house_801865DC = {
    .source               = { .index = 1 },
    .animationId          = 56,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player set-57 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the equipped-weapon bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation57 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 10,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced request-shaped records for clips 58..63; their receiver is unproven.
static AnimationPlayRequest D_dryfield_night_dilapidated_house_80186604[6] = {
    { .source = { .index = 1 }, .animationId = 58, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = 59, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = 60, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = 61, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = 62, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = 63, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Player set-64 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the equipped-weapon bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation64 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 17,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player set-65 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the equipped-weapon bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation65 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 18,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced request-shaped records for clips 66..70; their receiver is unproven.
static AnimationPlayRequest D_dryfield_night_dilapidated_house_801866A4[5] = {
    { .source = { .index = 1 }, .animationId = 66, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = 67, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = 68, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = 69, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
    { .source = { .index = 1 }, .animationId = 70, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Player set-71 animation request for the first-visit dialogue cues.
///
/// Blends over 10 whole animation frames and enables grid collision.
/// The script resolves the equipped-weapon bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation71 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 24,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_MEDIUM_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player placement shared by the normal and skip paths of the first-visit scene.
///
/// Position uses whole room units and yaw uses 4096 units per turn.
/// Message dispatch copies the needed values.
static ActorTransform _gDryfieldNightDilapidatedHouseFirstVisitPlayerPlacement = {
    .pos = { 2900, 0, 0, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Companion animation-set extension for the first-visit scene.
///
/// The 16 pointers overwrite bank indices 47..62; entry zero is NULL, and
/// entries 1..15 are the scene's sets 48..62. Clip data stays borrowed from
/// this overlay throughout playback; later extension words are unchanged.
static AnimationSet* _gDryfieldNightDilapidatedHouseFirstVisitCompanionSets[] = {
    NULL,
    &_gDryfieldNightDilapidatedHouseAnimation06008,
    &_gDryfieldNightDilapidatedHouseAnimation07DA4,
    &_gDryfieldNightDilapidatedHouseAnimation07FC8,
    &_gDryfieldNightDilapidatedHouseAnimation08278,
    &_gDryfieldNightDilapidatedHouseAnimation0865C,
    &_gDryfieldNightDilapidatedHouseAnimation08C74,
    &_gDryfieldNightDilapidatedHouseAnimation06EE8,
    &_gDryfieldNightDilapidatedHouseAnimation07304,
    &_gDryfieldNightDilapidatedHouseAnimation074C0,
    &_gDryfieldNightDilapidatedHouseAnimation076A8,
    &_gDryfieldNightDilapidatedHouseAnimation07964,
    &_gDryfieldNightDilapidatedHouseAnimation07B44,
    &_gDryfieldNightDilapidatedHouseAnimation06D10,
    &_gDryfieldNightDilapidatedHouseAnimation0679C,
    &_gDryfieldNightDilapidatedHouseAnimation08ED4,
};
STATIC_ASSERT(ARRAY_SIZE(_gDryfieldNightDilapidatedHouseFirstVisitCompanionSets) <= ANIMATION_BANK_EXTENSION_CAPACITY, _gDryfieldNightDilapidatedHouseFirstVisitCompanionSetsFitsBankExtension);

/// Installs the first-visit companion sets in the selected animation-bank extension.
///
/// Copies the complete pointer table during message dispatch, without starting
/// playback. The selected bank must be loaded and writable, and the copied
/// clip pointers remain borrowed from this overlay until playback ends.
static AnimationBankCopyRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionBankCopy = {
    .sets  = _gDryfieldNightDilapidatedHouseFirstVisitCompanionSets,
    .count = ARRAY_SIZE(_gDryfieldNightDilapidatedHouseFirstVisitCompanionSets),
};

/// Unreferenced request-shaped record for clip 1; its intended receiver is unproven.
static AnimationPlayRequest D_dryfield_night_dilapidated_house_8018677C = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion set-48 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation48 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced request-shaped record for clip 49; its intended receiver is unproven.
static AnimationPlayRequest D_dryfield_night_dilapidated_house_801867A4 = {
    .source               = { .index = 1 },
    .animationId          = 49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion opening animation request for the first-visit scene.
///
/// Uses scene set 50 with an immediate reset and grid collision enabled.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation50 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion set-51 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation51 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion set-52 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation52 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced request-shaped record for clip 53; its intended receiver is unproven.
static AnimationPlayRequest D_dryfield_night_dilapidated_house_801867F4 = {
    .source               = { .index = 1 },
    .animationId          = 53,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion set-54 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation54 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 7,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion set-55 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation55 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 8,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion set-56 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation56 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 9,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion set-57 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation57 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 10,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion set-58 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation58 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 11,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion set-59 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation59 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 12,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion set-60 animation request for the first-visit dialogue cues.
///
/// Blends over 20 whole animation frames and enables grid collision.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation60 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 13,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced request-shaped record for clip 61; its intended receiver is unproven.
static AnimationPlayRequest D_dryfield_night_dilapidated_house_80186894 = {
    .source               = { .index = 1 },
    .animationId          = 61,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_LONG_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion set-62 animation request for the first-visit dialogue cues.
///
/// Blends over 10 whole animation frames and enables grid collision.
/// The script resolves the saved-companion bank in a request copy before
/// dispatch; clip data must stay live for playback.
static AnimationPlayRequest _gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation62 = {
    .source               = { .index = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_BANK_PLACEHOLDER },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 15,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_MEDIUM_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Companion starting placement for the normal first-visit scene.
///
/// Position uses whole room units and yaw uses 4096 units per turn.
/// Message dispatch copies the needed values.
static ActorTransform _gDryfieldNightDilapidatedHouseFirstVisitCompanionStartPlacement = {
    .pos = { 400, 0, 0, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Companion approach destination and final placement for the first-visit scene.
///
/// The normal script walks toward this position before placing the actor here;
/// the skip script places it here immediately. Position is in whole room units,
/// yaw in 4096 units per turn; message dispatch copies the needed values.
static ActorTransform _gDryfieldNightDilapidatedHouseFirstVisitCompanionDestination = {
    .pos = { 1540, 0, 0, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Scene/audio stream selector for the house's first-visit dialogue.
///
/// Selects group 3, stream 50, qualifier 11 (second qualifier zero). Caption
/// control and event playback borrow this key while the room remains loaded.
static EvsSceneKey _gDryfieldNightDilapidatedHouseFirstVisitSceneKey = {
    .group    = 3,
    .streamId = 50,
    .subId    = 11,
};

enum {
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_CAP_COMMAND               = 11,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_WEAPON_TARGET_COMPANION   = 1,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_SCENE_VIEW                = 7,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_FINAL_VIEW                = 5,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_USE_DEFAULT_FADE_DURATION = 0,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_KEEP_FADE_DURATION        = 0,
};

EvsCommand gDryfieldNightDilapidatedHouseFirstVisitScript[88] = {
    // Start dialogue and hide the actors before handing presentation to the movie.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldNightDilapidatedHouseStartMovie }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gDryfieldNightDilapidatedHouseFirstVisitSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldNightDilapidatedHouseStageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_WEAPON_TARGET_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_SCENE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldNightDilapidatedHouseEnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Install borrowed clips and placements before resuming actor drawing.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionDestination } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation52 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation53 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation52 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionDestination } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation55 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation57 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation71 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation64 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation62 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation55 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation56 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation71 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation57 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation58 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation59 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation65 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionAnimation60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Fade across the return to native actor control and the gameplay view.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_KEEP_FADE_DURATION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_WEAPON_TARGET_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldNightDilapidatedHouseFinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_FINAL_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gDryfieldNightDilapidatedHouseFirstVisitSkipScript[16] = {
    // Restore the normal path's final placements before cancelling the selected scene.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_USE_DEFAULT_FADE_DURATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDilapidatedHouseFirstVisitPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldNightDilapidatedHouseFirstVisitCompanionDestination } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_WEAPON_TARGET_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldNightDilapidatedHouseCancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_VISIT_FINAL_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gDryfieldNightDilapidatedHouseMovieTaskDescriptors[2] = {
    {
        .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
        .callback = _dryfieldNightDilapidatedHouseLaunchMovieTask,
        .data     = { .value = 0 },
    },
    {
        .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
        .callback = _dryfieldNightDilapidatedHouseMovieTask,
        .data     = { .value = 0 },
    },
};

/// Three light prisms in the drawing coordinate's local space, in game units.
///
/// Blocks at indices 0, 8 and 16 each contain four lit corners followed by
/// four dark-rim corners. The room effect borrows them while this overlay is
/// loaded; the shared drawer transforms them through the task coordinate.
static SVECTOR _gDryfieldNightDilapidatedHouseLightPrismCorners[24] = {
    { -5500, -2250, -3030, 0 },
    { -4500, -2250, -3030, 0 },
    { -4500, -1030, -3030, 0 },
    { -5500, -1030, -3030, 0 },
    { -5780, 0, -180, 0 },
    { -5000, 0, -180, 0 },
    { -4770, 0, -1660, 0 },
    { -5660, 0, -1660, 0 },
    { -3000, -2250, -3030, 0 },
    { -2000, -2250, -3030, 0 },
    { -2000, -1030, -3030, 0 },
    { -3000, -1030, -3030, 0 },
    { -3370, 0, -180, 0 },
    { -2500, 0, -180, 0 },
    { -2770, 0, -1660, 0 },
    { -3140, 0, -1660, 0 },
    { -500, -2250, -3030, 0 },
    { 500, -2250, -3030, 0 },
    { 500, -1030, -3030, 0 },
    { -500, -1030, -3030, 0 },
    { -870, 0, -180, 0 },
    { 0, 0, -180, 0 },
    { 230, 0, -1660, 0 },
    { -650, 0, -1660, 0 },
};

WorldCoordRoomLighting gDryfieldNightDilapidatedHouseRoomLighting[1] = {
    { .lights = &gDryfieldNightDilapidatedHouseRoomLights, .ambientTable = gDryfieldNightDilapidatedHouseAmbientByView },
};

WorldCollisionRoomResources gDryfieldNightDilapidatedHouseRoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldNightDilapidatedHouseCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightDilapidatedHouseViewBoundaries,
        .actionTriggers       = gDryfieldNightDilapidatedHouseActionTriggers,
        .occluders            = &gDryfieldNightDilapidatedHouseSightOccluder,
    },
};

u8* gDryfieldNightDilapidatedHouseViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldNightDilapidatedHouseViewCounts[1] = { ARRAY_SIZE(gDryfieldNightDilapidatedHouseViewCameras) };

enum {
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_BACK_STREET_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE, 2),
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_BACK_STREET_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE, 1),
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_BACK_STREET_MAP_FLAG        = 467,
};

DirectionWarpEntry gDryfieldNightDilapidatedHouseWarpEntries[3] = {
    {
        .player         = { { .word = 0 }, 1596, 0, -2540 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 2296, 0, -1578 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_BACK_STREET_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_BACK_STREET_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 6,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_BACK_STREET_MAP_FLAG,
    },
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -5403, 2, -400 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -5184, 2, 200 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { { .word = 0 }, 1596, 0, -2540 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 2296, 0, -1578 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 6,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Room-space face normals, using ONE for unit length.
static SVECTOR _gDryfieldNightDilapidatedHouseCollisionGridNormals[10] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_normals.inc"
};

/// Collision mesh positions in signed integer room units.
static SVECTOR _gDryfieldNightDilapidatedHouseCollisionGridVerts[116] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_verts.inc"
};

/// Indexed collision triangles and quads, with room surface classes.
static WorldCollisionGridFace _gDryfieldNightDilapidatedHouseCollisionGridFaces[70] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_faces.inc"
};

/// Six packed face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
///
/// The cell table selects each list's start. Retained zeros between some lists
/// and after the final terminator are not traversed as face indices.
static s16 _gDryfieldNightDilapidatedHouseCollisionGridCells[208] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldNightDilapidatedHouseCollisionGridCells` (208 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_NIGHT_DILAPIDATED_HOUSE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightDilapidatedHouseCollisionGridCells[(halfwordOffset)])

/// X-major cell directory, indexed as cellX * 2 + cellZ for the 3-by-2 grid.
static s16* _gDryfieldNightDilapidatedHouseCollisionGridTable[6] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_table.inc"
};
#undef DRYFIELD_NIGHT_DILAPIDATED_HOUSE_CELL_FACE_LIST_AT_OFFSET

/// The house's room collision mesh and six-cell XZ face directory.
///
/// Positions use integer room units; normals use ONE for unit length. The
/// 3-by-2 grid has 4000-unit cells and origin (-6000, -3200) on XZ. Activation
/// binds its view coordinate. This overlay owns the mutable descriptor and
/// all pools, which must remain loaded while collision queries borrow them.
static WorldCollisionGrid _gDryfieldNightDilapidatedHouseCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightDilapidatedHouseCollisionGridNormals,
    .vertices    = _gDryfieldNightDilapidatedHouseCollisionGridVerts,
    .faces       = _gDryfieldNightDilapidatedHouseCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightDilapidatedHouseCollisionGridTable,
    .xBias       = 6000,
    .zBias       = 3200,
    .cellCountX  = 3,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightDilapidatedHouseCollisionGridFaces),
};

ViewCamera gDryfieldNightDilapidatedHouseViewCameras[11] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 900, 20000, 0 } }, 490 },
    { { { { 3872, 0, -1335 }, { -1200, 1793, -3481 }, { 584, 3682, 1695 } }, { 5380, 4300, 2840 } }, 207 },
    { { { { -2538, 0, 3214 }, { 1329, 3729, 1049 }, { -2926, 1694, -2310 } }, { 200, 2400, -200 } }, 257 },
    { { { { -1264, 0, 3896 }, { 486, 4063, 158 }, { -3865, 511, -1254 } }, { -3950, 2000, -1850 } }, 230 },
    { { { { -1264, 0, -3896 }, { -486, 4063, 158 }, { 3865, 511, -1254 } }, { 3950, 2000, -1850 } }, 230 },
    { { { { -3842, 0, -1419 }, { -415, 3916, 1123 }, { 1357, 1198, -3674 } }, { -600, 2000, -600 } }, 257 },
    { { { { -2678, 0, 3098 }, { -221, 4085, -191 }, { -3090, -292, -2671 } }, { -5725, 703, -3247 } }, 447 },
    { { { { -1861, 0, 3648 }, { 0, ONE, 0 }, { -3648, 0, -1861 } }, { -4445, 1450, -1015 } }, 447 },
    { { { { -2246, 0, -3425 }, { 0, ONE, 0 }, { 3425, 0, -2246 } }, { -1229, 1400, -1022 } }, 447 },
    { { { { -3166, 0, -2598 }, { -493, 4021, 600 }, { 2550, 777, -3108 } }, { -720, 1805, -1720 } }, 447 },
    { { { { -2242, 0, 3427 }, { 0, ONE, 0 }, { -3427, 0, -2242 } }, { -3395, 1400, -1085 } }, 447 },
};

/// Decoded-background marker and empty foreground list for mapped house view 1.
///
/// The retained source-slot address is never read as SpriteSource elements.
/// The zero-count first record retains the decoded background; `SPRITE_BATCH_END` stops traversal.
static SpriteBatch _gDryfieldNightDilapidatedHouseView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite sources for mapped house view 2.
///
/// The 71 rectangles use pixels for position and size, texels for UV,
/// and unscaled ordering depths. Batches select ranges in source elements.
static SpriteSource _gDryfieldNightDilapidatedHouseView2SpriteSources[] = {
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, -48, 641, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 56, -48, 623, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -56, 617, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, -56, 596, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 80, -40, 581, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, -32, 520, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, -16, 500, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, -8, 471, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 8, 458, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 88, 337, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -120, 1066, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, -120, 1116, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -120, 1192, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -8, -120, 1185, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 0, -120, 1100, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 8, -120, 905, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 8, -64, 1048, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 16, -48, 1000, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 16, -120, 841, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 24, -120, 771, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 24, -40, 972, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 32, -72, 794, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, -120, 680, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 40, -120, 625, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, 40, 750, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 64, 32, 750, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, 32, 750, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -120, 589, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 56, -120, 541, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 56, -64, 636, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -56, 617, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -120, 514, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, -120, 488, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, -48, 599, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, -120, 451, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, -40, 556, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, 24, 750, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 96, 24, 466, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -40, 468, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, -120, 406, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 112, -120, 369, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 112, -40, 418, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, 24, 483, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 128, 24, 432, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, -40, 379, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 128, -120, 338, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -120, 312, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -40, 347, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 144, 24, 387, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 24, 1125, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 48, 56, 1125, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, -56, 649, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, -32, 841, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, -64, 762, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, 16, 1125, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, -24, 889, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 32, -24, 885, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 32, 16, 1107, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 375, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 80, 375, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -32, 64, 375, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, 72, 375, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -16, 64, 375, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 0, 56, 375, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 16, 56, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 32, 48, 375, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 48, 40, 375, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, 32, 375, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, 24, 375, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, 24, 375, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, 32, 375, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
};

/// Source-element boundaries of mapped views 2 and 3's separate drawing ranges.
enum {
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_2_FIRST_RANGE_COUNT  = 9,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_2_SECOND_RANGE_COUNT = 49,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_2_THIRD_RANGE_START  = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_2_FIRST_RANGE_COUNT + DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_2_SECOND_RANGE_COUNT,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_3_FIRST_RANGE_COUNT  = 41,
};

/// Foreground source ranges for mapped house view 2.
///
/// Ranges cover 0..8, 9..57 and 58..70 of the view's source array.
/// The zero-count first record retains the decoded background; `SPRITE_BATCH_END` stops traversal.
static SpriteBatch _gDryfieldNightDilapidatedHouseView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_2_FIRST_RANGE_COUNT, 0, 0, { 1, 0 } },
    { DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_2_FIRST_RANGE_COUNT, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_2_SECOND_RANGE_COUNT, 0, 0, { 2, 0 } },
    { DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_2_THIRD_RANGE_START, ARRAY_SIZE(_gDryfieldNightDilapidatedHouseView2SpriteSources) - DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_2_THIRD_RANGE_START, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite sources for mapped house view 3.
///
/// The 62 rectangles use pixels for position and size, texels for UV,
/// and unscaled ordering depths. Batches select ranges in source elements.
static SpriteSource _gDryfieldNightDilapidatedHouseView3SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -48, 1227, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -32, 1299, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 0, 1163, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -16, -120, 1159, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -8, -120, 1133, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -80, 1227, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -32, 1313, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 8, -32, 1268, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, -80, 1164, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 8, -120, 1083, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 24, -120, 1055, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -120, 1017, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, -120, 983, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 24, 8, 1500, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, 8, 1500, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -120, 1125, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 72, -72, 1125, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -16, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 88, -120, 897, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 88, -64, 823, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -8, 821, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -120, 766, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 104, -64, 792, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -8, 876, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -120, 743, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 120, -56, 882, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, 0, 766, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 136, -120, 734, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 136, -56, 659, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 136, 0, 741, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 8, 914, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -48, 700, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, -120, 716, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 16, 1500, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -8, 1390, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -72, 1015, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -72, 1015, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -16, 1127, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -16, 1102, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 56, -120, 1426, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 64, -120, 941, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 72, 854, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -160, 16, 703, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -144, 8, 738, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -128, 8, 776, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -96, 16, 856, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 56, 835, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -104, 8, 830, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -112, 8, 806, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 893, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 48, 862, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, 8, 936, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 80, -48, 845, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -88, 800, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -32, 858, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, 24, 930, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -88, 773, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -24, 834, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, 32, 912, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -88, 746, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -24, 876, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, 32, 837, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
};

/// Foreground source ranges for mapped house view 3.
///
/// Ranges cover 0..40 and 41..61 of the view's source array.
/// The zero-count first record retains the decoded background; `SPRITE_BATCH_END` stops traversal.
static SpriteBatch _gDryfieldNightDilapidatedHouseView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_3_FIRST_RANGE_COUNT, 0, 0, { 1, 0 } },
    { DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_3_FIRST_RANGE_COUNT, ARRAY_SIZE(_gDryfieldNightDilapidatedHouseView3SpriteSources) - DRYFIELD_NIGHT_DILAPIDATED_HOUSE_VIEW_3_FIRST_RANGE_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite sources for mapped house view 4.
///
/// The 45 rectangles use pixels for position and size, texels for UV,
/// and unscaled ordering depths. Batches select ranges in source elements.
static SpriteSource _gDryfieldNightDilapidatedHouseView4SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -64, 1503, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, -32, 2189, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 16, 2230, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, -32, 1391, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -128, -32, 1416, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -120, -32, 1523, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, 8, 1458, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -104, 0, 1523, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 8, 1601, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 8, 1665, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 32, 1698, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -96, 1376, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -48, 2240, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -8, 2262, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -24, 2271, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -48, 2180, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -96, 1348, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, -96, 1361, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -24, -32, 2155, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, -96, 1710, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, -32, 1987, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, -104, 1472, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, -48, 1844, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 0, 8, 1980, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 8, 1952, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 16, -48, 2017, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 16, -104, 1441, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, -104, 1411, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, -48, 1972, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 8, 2008, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 8, 1927, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 48, -48, 1932, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 48, -104, 1263, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -104, 1263, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -48, 1892, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 8, 1874, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, -112, 1162, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 80, -48, 1834, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 8, 1824, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 8, 1587, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -48, 1716, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -112, 1162, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, -112, 1162, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -48, 1798, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 8, 1628, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
};

/// Foreground source ranges for mapped house view 4.
///
/// The drawing range covers all 45 source elements.
/// The zero-count first record retains the decoded background; `SPRITE_BATCH_END` stops traversal.
static SpriteBatch _gDryfieldNightDilapidatedHouseView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightDilapidatedHouseView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite sources for mapped house view 5.
///
/// The 7 rectangles use pixels for position and size, texels for UV,
/// and unscaled ordering depths. Batches select ranges in source elements.
static SpriteSource _gDryfieldNightDilapidatedHouseView5SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 16, 1429, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 0, 1397, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, 0, 1475, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 8, 1392, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 8, 1313, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, 8, 1313, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 8, 1126, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

/// Foreground source ranges for mapped house view 5.
///
/// The drawing range covers all 7 source elements.
/// The zero-count first record retains the decoded background; `SPRITE_BATCH_END` stops traversal.
static SpriteBatch _gDryfieldNightDilapidatedHouseView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightDilapidatedHouseView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite sources for mapped house view 6.
///
/// The 11 rectangles use pixels for position and size, texels for UV,
/// and unscaled ordering depths. Batches select ranges in source elements.
static SpriteSource _gDryfieldNightDilapidatedHouseView6SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -8, 837, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 48, 828, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 88, 8, 834, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 96, 8, 834, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 104, -16, 804, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 112, -8, 861, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 120, -8, 861, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 128, 0, 834, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 136, 8, 808, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 144, 0, 834, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 152, 0, 735, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
};

/// Foreground source ranges for mapped house view 6.
///
/// The drawing range covers all 11 source elements.
/// The zero-count first record retains the decoded background; `SPRITE_BATCH_END` stops traversal.
static SpriteBatch _gDryfieldNightDilapidatedHouseView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightDilapidatedHouseView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite sources for mapped house view 7.
///
/// The 9 rectangles use pixels for position and size, texels for UV,
/// and unscaled ordering depths. Batches select ranges in source elements.
static SpriteSource _gDryfieldNightDilapidatedHouseView7SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -64, 2714, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -72, 2500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 112, 0, 2475, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 64, 0, 2700, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 64, -64, 2533, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 128, -72, 2433, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, 0, 2333, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 144, -80, 2426, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 144, -8, 2279, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
};

/// Foreground source ranges for mapped house view 7.
///
/// The drawing range covers all 9 source elements.
/// The zero-count first record retains the decoded background; `SPRITE_BATCH_END` stops traversal.
static SpriteBatch _gDryfieldNightDilapidatedHouseView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightDilapidatedHouseView7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite sources for mapped house view 8.
///
/// The 15 rectangles use pixels for position and size, texels for UV,
/// and unscaled ordering depths. Batches select ranges in source elements.
static SpriteSource _gDryfieldNightDilapidatedHouseView8SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 8, -80, 2329, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 8, 0, 2179, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 24, -80, 2164, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 24, 0, 2269, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -80, 2216, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 72, -88, 2185, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 72, -8, 2149, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, -88, 2137, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 88, -8, 2149, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 104, -88, 2101, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 104, -8, 1964, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 120, -88, 1966, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 120, -8, 1936, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 136, -88, 2027, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 136, -8, 1896, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
};

/// Foreground source ranges for mapped house view 8.
///
/// The drawing range covers all 15 source elements.
/// The zero-count first record retains the decoded background; `SPRITE_BATCH_END` stops traversal.
static SpriteBatch _gDryfieldNightDilapidatedHouseView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightDilapidatedHouseView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-background marker and empty foreground list for mapped house view 9.
///
/// The retained source-slot address is never read as SpriteSource elements.
/// The zero-count first record retains the decoded background; `SPRITE_BATCH_END` stops traversal.
static SpriteBatch _gDryfieldNightDilapidatedHouseView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-background marker and empty foreground list for mapped house view 10.
///
/// The retained source-slot address is never read as SpriteSource elements.
/// The zero-count first record retains the decoded background; `SPRITE_BATCH_END` stops traversal.
static SpriteBatch _gDryfieldNightDilapidatedHouseView10SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite sources for mapped house view 11.
///
/// The 12 rectangles use pixels for position and size, texels for UV,
/// and unscaled ordering depths. Batches select ranges in source elements.
static SpriteSource _gDryfieldNightDilapidatedHouseView11SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, -88, 2089, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 32, 0, 2087, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 48, -96, 1918, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 48, -8, 2035, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -96, 1976, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -96, 1936, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 104, -96, 1906, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 104, -8, 1898, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 120, -96, 1868, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 120, -8, 1861, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 136, -104, 1763, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 136, -8, 1734, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
};

/// Foreground source ranges for mapped house view 11.
///
/// The drawing range covers all 12 source elements.
/// The zero-count first record retains the decoded background; `SPRITE_BATCH_END` stops traversal.
static SpriteBatch _gDryfieldNightDilapidatedHouseView11SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightDilapidatedHouseView11SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightDilapidatedHouseSpriteViews[11] = {
    { { .empty = _gDryfieldNightDilapidatedHouseView1SpriteBatches }, _gDryfieldNightDilapidatedHouseView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDilapidatedHouseView2SpriteSources }, _gDryfieldNightDilapidatedHouseView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDilapidatedHouseView3SpriteSources }, _gDryfieldNightDilapidatedHouseView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDilapidatedHouseView4SpriteSources }, _gDryfieldNightDilapidatedHouseView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDilapidatedHouseView5SpriteSources }, _gDryfieldNightDilapidatedHouseView5SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDilapidatedHouseView6SpriteSources }, _gDryfieldNightDilapidatedHouseView6SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDilapidatedHouseView7SpriteSources }, _gDryfieldNightDilapidatedHouseView7SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDilapidatedHouseView8SpriteSources }, _gDryfieldNightDilapidatedHouseView8SpriteBatches, NULL },
    { { .empty = _gDryfieldNightDilapidatedHouseView9SpriteBatches }, _gDryfieldNightDilapidatedHouseView9SpriteBatches, NULL },
    { { .empty = _gDryfieldNightDilapidatedHouseView10SpriteBatches }, _gDryfieldNightDilapidatedHouseView10SpriteBatches, NULL },
    { { .elements = _gDryfieldNightDilapidatedHouseView11SpriteSources }, _gDryfieldNightDilapidatedHouseView11SpriteBatches, NULL },
};

/// Eight directed camera boundaries connecting logical views 2..6 in the house.
///
/// Includes both directions of 2/3, 3/4, 4/5 and 5/6; parameter0 is the source
/// view and parameter1 the destination. Positions, corner offsets and radii
/// use room units; authored normals use Q12. The final record carries LAST.
/// Room binding parents, links and enables these writable nodes; keep this
/// overlay loaded until their runtime list has been cleared.
static WorldCollisionTrigger _gDryfieldNightDilapidatedHouseViewBoundaries[8] = {
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { -4160, -1696, -1792, 0 },
        .vertices     = { { 0, -2112, -1024, 0 }, { 0, -2112, 1024, 0 }, { 0, 2112, -1024, 0 }, { 0, 2112, 1024, 0 } },
        .normal       = { 4097, 0, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 2346,
        .control      = 0,
        .parameter0   = 3,
        .parameter1   = 2,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { -4064, -1376, -1824, 0 },
        .vertices     = { { 0, -2400, 1024, 0 }, { 0, -2400, -1024, 0 }, { 0, 2400, 1024, 0 }, { 0, 2400, -1024, 0 } },
        .normal       = { -4117, 0, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 2598,
        .control      = 0,
        .parameter0   = 2,
        .parameter1   = 3,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { -1937, -1376, -1937, 0 },
        .vertices     = { { -2182, -2400, 2208, 0 }, { 2183, -2400, -2207, 0 }, { -2182, 2400, 2208, 0 }, { 2183, 2400, -2207, 0 } },
        .normal       = { -2920, 0, -2887, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 3916,
        .control      = 0,
        .parameter0   = 3,
        .parameter1   = 4,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { -2017, -1360, -2017, 0 },
        .vertices     = { { 2183, -2384, -2207, 0 }, { -2182, -2384, 2208, 0 }, { 2183, 2384, -2207, 0 }, { -2182, 2384, 2208, 0 } },
        .normal       = { 2913, 0, 2880, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 3907,
        .control      = 0,
        .parameter0   = 4,
        .parameter1   = 3,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 220, -1392, 318, 0 },
        .vertices     = { { -30, -2416, -3113, 0 }, { 4, -2416, 3091, 0 }, { -30, 2416, -3113, 0 }, { 4, 2416, 3091, 0 } },
        .normal       = { 4098, 0, -23, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 3932,
        .control      = 0,
        .parameter0   = 5,
        .parameter1   = 4,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2974, -1312, -1986, 0 },
        .vertices     = { { -2235, -2336, -137, 0 }, { 2226, -2336, 127, 0 }, { -2235, 2336, -137, 0 }, { 2226, 2336, 127, 0 } },
        .normal       = { 241, 0, -4092, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 3228,
        .control      = 0,
        .parameter0   = 6,
        .parameter1   = 5,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 318, -1360, 352, 0 },
        .vertices     = { { 13, -2384, 3100, 0 }, { -23, -2384, -3109, 0 }, { 13, 2384, 3100, 0 }, { -23, 2384, -3109, 0 } },
        .normal       = { -4098, 0, 23, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 3907,
        .control      = 0,
        .parameter0   = 4,
        .parameter1   = 5,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2975, -1472, -2048, 0 },
        .vertices     = { { 2226, -2496, 127, 0 }, { -2235, -2496, -137, 0 }, { 2226, 2496, 127, 0 }, { -2235, 2496, -137, 0 } },
        .normal       = { -243, 0, 4090, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 3347,
        .control      = 0,
        .parameter0   = 5,
        .parameter1   = 6,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST,
        .hit          = 0,
    },
};

WorldCoordPointLight gDryfieldNightDilapidatedHousePointLights[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -1500, 500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2000, 3549 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2500, -1500, -2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2256, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -1500, -2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -1500, -2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2256, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1500, -2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2256, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2500, -1500, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2000, 6400 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1500, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2000, 3003 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2500, -1500, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2000, 3000 },
};

WorldCoordSpotLight gDryfieldNightDilapidatedHouseConeLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -ONE, 0, 0 }, { 0, -2902, 2901 }, { 0, 2901, 2901 } }, { -5000, -2500, -4000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 0, 0 }, { 0, 0 } }, { 0, 2896, 2896, 0 }, 100, 3000, 625 },
};

/// 756 unreferenced bytes following the room's cone light in the overlay image.
///
/// Their original purpose is unproven. Preserve the representation, including
/// embedded address values, without exposing it as additional lights.
static u8 _gDryfieldNightDilapidatedHouseUnreferencedData[] = {
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
    0xA4,
    0x50,
    0x19,
    0x80,
    0x01,
    0x00,
    0x00,
    0x00,
    0xA4,
    0x53,
    0x00,
    0x00,
    0x2D,
    0xFC,
    0x80,
    0x10,
    0xD8,
    0xFF,
    0x00,
    0x00,
    0x55,
    0xFF,
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
    0xBE,
    0xE7,
    0x30,
    0xF3,
    0xDE,
    0x15,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x07,
    0x00,
    0x00,
    0x00,
    0xD4,
    0x03,
    0x00,
    0x00,
    0xF9,
    0xFF,
    0x00,
    0x00,
    0x2D,
    0xFC,
    0x00,
    0x00,
    0x07,
    0x00,
    0x00,
    0x00,
    0xD4,
    0x03,
    0xD0,
    0x10,
    0xF9,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xEB,
    0xEF,
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
    0x34,
    0x11,
    0x00,
    0x00,
    0x06,
    0x05,
    0x00,
    0x00,
    0xE0,
    0x54,
    0x19,
    0x80,
    0x48,
    0x54,
    0x19,
    0x80,
    0xF8,
    0x30,
    0x07,
    0x80,
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
    0xC0,
    0x03,
    0xE0,
    0x11,
    0x3A,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x51,
    0xF0,
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
    0x38,
    0x12,
    0x00,
    0x00,
    0x07,
    0x06,
    0x61,
    0x00,
    0x2C,
    0x55,
    0x19,
    0x80,
    0x94,
    0x54,
    0x19,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x5D,
    0xE8,
    0x30,
    0xF2,
    0x3C,
    0xFE,
    0x00,
    0x00,
    0xB5,
    0x03,
    0x30,
    0xEE,
    0x0C,
    0xFF,
    0x00,
    0x00,
    0x4B,
    0xFC,
    0x00,
    0x00,
    0xF5,
    0x00,
    0x00,
    0x00,
    0xB5,
    0x03,
    0xD0,
    0x11,
    0x0C,
    0xFF,
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
    0x78,
    0x55,
    0x19,
    0x80,
    0xE0,
    0x54,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x40,
    0xEB,
    0x60,
    0xF2,
    0x6F,
    0xF4,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0x80,
    0xEF,
    0x7C,
    0xF7,
    0x00,
    0x00,
    0x08,
    0x00,
    0x80,
    0xEF,
    0x84,
    0x08,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0x80,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00,
    0x80,
    0x10,
    0x84,
    0x08,
    0x00,
    0x00,
    0x02,
    0x10,
    0x00,
    0x00,
    0xF0,
    0xFF,
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
    0x8C,
    0x12,
    0x00,
    0x00,
    0x02,
    0x07,
    0x61,
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
    0x0C,
    0xF7,
    0x00,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0x80,
    0x10,
    0x0C,
    0xF7,
    0x00,
    0x00,
    0xFA,
    0xEF,
    0x00,
    0x00,
    0x0E,
    0x00,
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
    0xC2,
    0x12,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x56,
    0x19,
    0x80,
    0x78,
    0x55,
    0x19,
    0x80,
    0xF8,
    0x30,
    0x07,
    0x80,
    0x40,
    0xEB,
    0xE0,
    0xF4,
    0xE0,
    0x28,
    0x00,
    0x00,
    0xEB,
    0x00,
    0x80,
    0xEF,
    0xCE,
    0xFA,
    0x00,
    0x00,
    0x10,
    0xFF,
    0x80,
    0xEF,
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
    0x00,
    0x10,
    0x00,
    0x00,
    0x52,
    0x11,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x5C,
    0x56,
    0x19,
    0x80,
    0xC4,
    0x55,
    0x00,
    0x00,
    0xF8,
    0x30,
    0x07,
    0x80,
    0xA0,
    0xEB,
    0x00,
    0x00,
    0x40,
    0x29,
    0x00,
    0x00,
    0x11,
    0xFF,
    0x80,
    0xEF,
    0x2E,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xCF,
    0xFA,
    0x00,
    0x00,
    0x11,
    0xFF,
    0x80,
    0x10,
    0x2E,
    0x05,
    0x00,
    0x00,
    0xEC,
    0x00,
    0x80,
    0x10,
    0xCF,
    0xFA,
    0x00,
    0x00,
    0x3F,
    0xF0,
    0x00,
    0x00,
    0x2E,
    0xFD,
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
    0x22,
    0x05,
    0x60,
    0xEF,
    0xA3,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x5D,
    0xFD,
    0x00,
    0x00,
    0x22,
    0x05,
    0x00,
    0x00,
    0xA3,
    0x02,
    0x00,
    0x00,
    0xDF,
    0xFA,
    0x00,
    0x00,
    0x5D,
    0xFD,
    0x00,
    0x00,
    0xAA,
    0xF8,
    0x00,
    0x00,
    0x43,
    0x0E,
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
    0x8C,
    0x11,
    0x00,
    0x00,
    0x04,
    0x03,
    0x61,
    0x00,
    0xF4,
    0x56,
    0x19,
    0x80,
    0x5C,
    0x56,
    0x00,
    0x00,
    0xF8,
    0x30,
    0x07,
    0x80,
    0xCC,
    0xFB,
    0x40,
    0xF3,
    0x6D,
    0x14,
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
    0x78,
    0x07,
    0x00,
    0x00,
    0xD0,
    0xF1,
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
    0x6F,
    0x11,
    0x00,
    0x00,
    0x03,
    0x04,
    0x61,
    0x00,
    0x40,
    0x57,
    0x00,
    0x00,
    0xA8,
    0x56,
    0x19,
    0x80,
    0xF8,
    0x30,
    0x07,
    0x80,
    0x40,
    0xEC,
    0x60,
    0xF3,
};

STATIC_ASSERT_SIZEOF(_gDryfieldNightDilapidatedHouseUnreferencedData, 756);

/// Plays the house's skippable movie and restores game presentation.
///
/// Start this bodyless controller in state 0 with the room's movie slots and
/// display task loaded. Disc 2 selects stream 101; other discs select 100.
/// The lookup must succeed with slot 0..14; its failure sentinel is unchecked.
/// Requires exclusive movie/CD workspace use and intact saved VRAM images.
/// Start cancels playback; restoration waits for CD idle, restores memory,
/// clears the image workspace and releases this task before resuming drawing.
/// The no-sprite restore call does not itself clear the CD pause block.
static void _dryfieldNightDilapidatedHouseMovieTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_PREPARE           = 0,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_QUEUE             = 1,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_WAIT_READY        = 2,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_PLAY              = 3,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_BEGIN_RESTORE     = 4,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_RESTORE           = 5,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_DEFAULT_STREAM_ID = 100,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_DISC_2_STREAM_ID  = 101,
    };
    u8              streamArgs[4]; // Serialized command bytes; only the slot byte is consumed by playback.
    GameLocationKey movieKey;
    CdCmdQueue*     cdQueue;
    s16             movieSlot;

    cdQueue = &gCdCmdQueue;
    switch (task->state) {
        case DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_PREPARE:
            SetDispMask(false);
            streamPrepareMovieWorkspace(true);
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_QUEUE:
            movieKey = gGameSession->location;
            if (gGameMainPersistentState.discNumber == GAME_MAIN_DISC_2) {
                movieKey.view = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_DISC_2_STREAM_ID;
            } else {
                movieKey.view = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_DEFAULT_STREAM_ID;
            }
            movieSlot     = streamFindMovieSlot(&movieKey, 0, 0);
            streamArgs[0] = movieSlot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_WAIT_READY:
            if (cdQueue->movieReady == 0) {
                return;
            }
            SetDispMask(true);
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_PLAY:
            if (cdCmdIsIdle()) {
                SetDispMask(false);
                task->state = task->state + 1;
                return;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            SetDispMask(false);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            return;
        // Cancellation must drain before the decoder workspace can be restored.
        case DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_BEGIN_RESTORE:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            streamResetGameRestore();
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_RESTORE:
            if (streamPollGameRestore(false, false) == 0) {
                return;
            }
            memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
            SetDispMask(true);
            taskKill(task);
            displayResumeGameLoop();
            return;
    }
}

/// Hands presentation to the movie controller and queues the current camera packets.
///
/// Requires the loaded room's display-task table and current view resources.
/// Selects task-owned flipping even if the controller allocation fails. Camera
/// packets are queued after the display spawn; keep their resources live through
/// movie playback and display restoration.
static inline void _dryfieldNightDilapidatedHouseTakeMovieDisplay(void)
{
    enum { DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_CONTROLLER = 1 };

    displaySpawnTaskFromTable(gDryfieldNightDilapidatedHouseMovieTaskDescriptors, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_CONTROLLER, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
}

/// Transfers display presentation to the house's movie controller, then retires.
///
/// Requires game-loop display ownership, loaded movie descriptors and current
/// camera/packet resources. Controller allocation is unchecked: flip mode and
/// camera queuing still proceed if the display spawn fails. Keep the room
/// loaded through movie playback and game-display restoration.
static void _dryfieldNightDilapidatedHouseLaunchMovieTask(Task* task)
{
    // Queue the camera into the handed-off display before releasing the launcher.
    _dryfieldNightDilapidatedHouseTakeMovieDisplay();
    taskKill(task);
}

/// Selects the house's three local-space light prisms for its tinted drawer.
///
/// Binds the room-owned `SVECTOR[24]`; calls select eight-corner blocks at
/// element indices 0, 8 and 16. The fragment only reads these corners while
/// this overlay is loaded and undefines the binding after its inclusion.
#define GLOW_PRISM_CORNERS _gDryfieldNightDilapidatedHouseLightPrismCorners
#include "../../shared/effects/glow_draw_prism.inc.c"

void dryfieldNightDilapidatedHouseDrawLightPrismsTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_PRISM_VIEWS   = (1 << 2) | (1 << 3) | (1 << 4) | (1 << 7) | (1 << 8) | (1 << 11),
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SECOND_PRISM_VIEWS  = (1 << 3) | (1 << 4) | (1 << 7) | (1 << 8) | (1 << 11),
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_THIRD_PRISM_VIEWS   = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 11),
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_PRISM_CORNER  = 0,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SECOND_PRISM_CORNER = 8,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_THIRD_PRISM_CORNER  = 16,
    };

    GfxCoord* coord;
    s32       viewMask;

    coord    = task->extra.coordBody->coord;
    viewMask = 1 << gGameSession->location.view;
    actorRenderComposeCoord(coord);
    // Each block holds four lit corners followed by its four dark-rim corners.
    if (viewMask & DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_PRISM_VIEWS) {
        _glowDrawPrism(coord, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_PRISM_CORNER);
    }
    if (viewMask & DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SECOND_PRISM_VIEWS) {
        _glowDrawPrism(coord, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SECOND_PRISM_CORNER);
    }
    if (viewMask & DRYFIELD_NIGHT_DILAPIDATED_HOUSE_THIRD_PRISM_VIEWS) {
        _glowDrawPrism(coord, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_THIRD_PRISM_CORNER);
    }
}
