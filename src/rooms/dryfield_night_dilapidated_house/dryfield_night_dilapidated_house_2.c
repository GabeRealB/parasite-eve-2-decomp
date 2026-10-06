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
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
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
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"

/// The prism corners, eight per prism: a lit ring of four, then the far ring.
extern SVECTOR gGlowPrismCorners[];

extern WorldCollisionGrid D_dryfield_night_dilapidated_house_80187D44[1];

extern WorldCollisionTrigger D_dryfield_night_dilapidated_house_801892A0[8];

void func_dryfield_night_dilapidated_house_8017DB20(Task*);
void func_dryfield_night_dilapidated_house_8017DCE0(Task*);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_night_dilapidated_house_8017E700[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_dilapidated_house_8017D8DC },
    { 5105, func_dryfield_night_dilapidated_house_8017D8D4 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_dilapidated_house_8017D968 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_dilapidated_house_8017D960 },
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

AnimationSet* D_dryfield_night_dilapidated_house_801864BC[25] = {
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

AnimationBankCopyRequest D_dryfield_night_dilapidated_house_80186520 = { { .sets = D_dryfield_night_dilapidated_house_801864BC }, ARRAY_SIZE(D_dryfield_night_dilapidated_house_801864BC) };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186528 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_8018653C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186550 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186564[2] = {
    { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_dryfield_night_dilapidated_house_8018658C = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801865A0 = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801865B4 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801865C8 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801865DC = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801865F0 = { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186604[6] = {
    { { .index = 1 }, 58, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 59, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 60, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 61, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 62, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 63, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_dryfield_night_dilapidated_house_8018667C = { { .index = 1 }, 64, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186690 = { { .index = 1 }, 65, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801866A4[5] = {
    { { .index = 1 }, 66, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 67, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 68, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 69, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 70, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186708 = { { .index = 1 }, 71, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_dryfield_night_dilapidated_house_8018671C = { { 2900, 0, 0, 0 }, { 0, -1024, 0, 0 } };

AnimationSet* D_dryfield_night_dilapidated_house_80186734[16] = {
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

AnimationBankCopyRequest D_dryfield_night_dilapidated_house_80186774 = { { .sets = D_dryfield_night_dilapidated_house_80186734 }, ARRAY_SIZE(D_dryfield_night_dilapidated_house_80186734) };

AnimationPlayRequest D_dryfield_night_dilapidated_house_8018677C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186790 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801867A4 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801867B8 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801867CC = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801867E0 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801867F4 = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186808 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_8018681C = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186830 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186844 = { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186858 = { { .index = 1 }, 58, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_8018686C = { { .index = 1 }, 59, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186880 = { { .index = 1 }, 60, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_80186894 = { { .index = 1 }, 61, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_dilapidated_house_801868A8 = { { .index = 1 }, 62, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_dryfield_night_dilapidated_house_801868BC = { { 400, 0, 0, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_dryfield_night_dilapidated_house_801868D4 = { { 1540, 0, 0, 0 }, { 0, 1024, 0, 0 } };

EvsSceneKey D_dryfield_night_dilapidated_house_801868EC = { 3, 50, 11 };

EvsCommand D_dryfield_night_dilapidated_house_801868F4[88] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_dilapidated_house_8017DAF0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_dryfield_night_dilapidated_house_801868EC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_dilapidated_house_8017DA70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_dilapidated_house_8017DA90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_dilapidated_house_80186520 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_dilapidated_house_80186774 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_dilapidated_house_8018671C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_dilapidated_house_801868BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186550 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801867B8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018653C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801867CC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_dryfield_night_dilapidated_house_801868D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186880 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018658C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801865A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018653C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801867E0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_dilapidated_house_801868D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801865C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186880 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801865F0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186708 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018667C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186790 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801868A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018653C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186808 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018681C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186830 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186708 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186844 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186858 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018686C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186690 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018653C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186880 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_dilapidated_house_8017DAB0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_dilapidated_house_80187134[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_dilapidated_house_8018671C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_dilapidated_house_801868D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_night_dilapidated_house_8017DAD0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_night_dilapidated_house_801872B4[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_dilapidated_house_8017DCE0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_dilapidated_house_8017DB20, { .value = 0 } },
};

SVECTOR gGlowPrismCorners[24] = {
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

WorldCoordRoomLighting D_dryfield_night_dilapidated_house_8018738C[1] = {
    { D_dryfield_night_dilapidated_house_80189B60, D_dryfield_night_dilapidated_house_8018A054 },
};

WorldCollisionRoomResources D_dryfield_night_dilapidated_house_80187394[1] = {
    { D_dryfield_night_dilapidated_house_80187D44, D_dryfield_night_dilapidated_house_801892A0, D_dryfield_night_dilapidated_house_80189B78, D_dryfield_night_dilapidated_house_80189F08 },
};

u8* D_dryfield_night_dilapidated_house_801873A4[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_night_dilapidated_house_801873A8[1] = { 11 };

DirectionWarpEntry D_dryfield_night_dilapidated_house_801873AC[3] = {
    { { { .word = 0 }, 1596, 0, -2540 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2296, 0, -1578 }, { 0, 0, 0, 0 }, 0x53090002, 0x53090001, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, 467 },
    { { { .word = 1024 }, -5403, 2, -400 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -5184, 2, 200 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, 1596, 0, -2540 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2296, 0, -1578 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldNightDilapidatedHouseCollision0A784Normals[10] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_normals.inc"
};

static SVECTOR _gDryfieldNightDilapidatedHouseCollision0A784Verts[116] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightDilapidatedHouseCollision0A784Faces[70] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_faces.inc"
};

static s16 _gDryfieldNightDilapidatedHouseCollision0A784Cells[208] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightDilapidatedHouseCollision0A784Cells[i])
static s16* _gDryfieldNightDilapidatedHouseCollision0A784Table[6] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_dilapidated_house_80187D44[1] = {
    { NULL, _gDryfieldNightDilapidatedHouseCollision0A784Normals, _gDryfieldNightDilapidatedHouseCollision0A784Verts, _gDryfieldNightDilapidatedHouseCollision0A784Faces, _gDryfieldNightDilapidatedHouseCollision0A784Table, 6000, 3200, 3, 2, 4000, 70 },
};

ViewCamera D_dryfield_night_dilapidated_house_80187D68[11] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 900, 0x4E20, 0 } }, 490 },
    { { { { 3872, 0, -1335 }, { -1200, 1793, -3481 }, { 584, 3682, 1695 } }, { 5380, 4300, 2840 } }, 207 },
    { { { { -2538, 0, 3214 }, { 1329, 3729, 1049 }, { -2926, 1694, -2310 } }, { 200, 2400, -200 } }, 257 },
    { { { { -1264, 0, 3896 }, { 486, 4063, 158 }, { -3865, 511, -1254 } }, { -3950, 2000, -1850 } }, 230 },
    { { { { -1264, 0, -3896 }, { -486, 4063, 158 }, { 3865, 511, -1254 } }, { 3950, 2000, -1850 } }, 230 },
    { { { { -3842, 0, -1419 }, { -415, 3916, 1123 }, { 1357, 1198, -3674 } }, { -600, 2000, -600 } }, 257 },
    { { { { -2678, 0, 3098 }, { -221, 4085, -191 }, { -3090, -292, -2671 } }, { -5725, 703, -3247 } }, 447 },
    { { { { -1861, 0, 3648 }, { 0, 4096, 0 }, { -3648, 0, -1861 } }, { -4445, 1450, -1015 } }, 447 },
    { { { { -2246, 0, -3425 }, { 0, 4096, 0 }, { 3425, 0, -2246 } }, { -1229, 1400, -1022 } }, 447 },
    { { { { -3166, 0, -2598 }, { -493, 4021, 600 }, { 2550, 777, -3108 } }, { -720, 1805, -1720 } }, 447 },
    { { { { -2242, 0, 3427 }, { 0, 4096, 0 }, { -3427, 0, -2242 } }, { -3395, 1400, -1085 } }, 447 },
};

SpriteBatch D_dryfield_night_dilapidated_house_80187EF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_dilapidated_house_80187F04[71] = {
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

SpriteBatch D_dryfield_night_dilapidated_house_80188490[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 49, 0, 0, { 2, 0 } },
    { 58, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_dilapidated_house_801884B8[62] = {
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

SpriteBatch D_dryfield_night_dilapidated_house_80188990[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 41, 0, 0, { 1, 0 } },
    { 41, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_dilapidated_house_801889B0[45] = {
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

SpriteBatch D_dryfield_night_dilapidated_house_80188D34[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 45, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_dilapidated_house_80188D4C[7] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 16, 1429, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 0, 1397, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, 0, 1475, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 8, 1392, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 8, 1313, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, 8, 1313, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 8, 1126, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_dilapidated_house_80188DD8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_dilapidated_house_80188DF0[11] = {
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

SpriteBatch D_dryfield_night_dilapidated_house_80188ECC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_dilapidated_house_80188EE4[9] = {
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

SpriteBatch D_dryfield_night_dilapidated_house_80188F98[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_dilapidated_house_80188FB0[15] = {
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

SpriteBatch D_dryfield_night_dilapidated_house_801890DC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_dilapidated_house_801890F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_dilapidated_house_80189104[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_dilapidated_house_80189114[12] = {
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

SpriteBatch D_dryfield_night_dilapidated_house_80189204[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_dilapidated_house_8018921C[11] = {
    { { .empty = D_dryfield_night_dilapidated_house_80187EF4 }, D_dryfield_night_dilapidated_house_80187EF4, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80187F04 }, D_dryfield_night_dilapidated_house_80188490, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_801884B8 }, D_dryfield_night_dilapidated_house_80188990, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_801889B0 }, D_dryfield_night_dilapidated_house_80188D34, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80188D4C }, D_dryfield_night_dilapidated_house_80188DD8, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80188DF0 }, D_dryfield_night_dilapidated_house_80188ECC, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80188EE4 }, D_dryfield_night_dilapidated_house_80188F98, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80188FB0 }, D_dryfield_night_dilapidated_house_801890DC, NULL },
    { { .empty = D_dryfield_night_dilapidated_house_801890F4 }, D_dryfield_night_dilapidated_house_801890F4, NULL },
    { { .empty = D_dryfield_night_dilapidated_house_80189104 }, D_dryfield_night_dilapidated_house_80189104, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80189114 }, D_dryfield_night_dilapidated_house_80189204, NULL },
};

WorldCollisionTrigger D_dryfield_night_dilapidated_house_801892A0[8] = {
    { NULL, NULL, NULL, { -4160, -1696, -1792, 0 }, { { 0, -2112, -1024, 0 }, { 0, -2112, 1024, 0 }, { 0, 2112, -1024, 0 }, { 0, 2112, 1024, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4064, -1376, -1824, 0 }, { { 0, -2400, 1024, 0 }, { 0, -2400, -1024, 0 }, { 0, 2400, 1024, 0 }, { 0, 2400, -1024, 0 } }, { -4117, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1937, -1376, -1937, 0 }, { { -2182, -2400, 2208, 0 }, { 2183, -2400, -2207, 0 }, { -2182, 2400, 2208, 0 }, { 2183, 2400, -2207, 0 } }, { -2920, 0, -2887, 0 }, { 0, 0, 4096, 0 }, 3916, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2017, -1360, -2017, 0 }, { { 2183, -2384, -2207, 0 }, { -2182, -2384, 2208, 0 }, { 2183, 2384, -2207, 0 }, { -2182, 2384, 2208, 0 } }, { 2913, 0, 2880, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 220, -1392, 318, 0 }, { { -30, -2416, -3113, 0 }, { 4, -2416, 3091, 0 }, { -30, 2416, -3113, 0 }, { 4, 2416, 3091, 0 } }, { 4098, 0, -23, 0 }, { 0, 0, 4096, 0 }, 3932, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2974, -1312, -1986, 0 }, { { -2235, -2336, -137, 0 }, { 2226, -2336, 127, 0 }, { -2235, 2336, -137, 0 }, { 2226, 2336, 127, 0 } }, { 241, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 3228, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 318, -1360, 352, 0 }, { { 13, -2384, 3100, 0 }, { -23, -2384, -3109, 0 }, { 13, 2384, 3100, 0 }, { -23, 2384, -3109, 0 } }, { -4098, 0, 23, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2975, -1472, -2048, 0 }, { { 2226, -2496, 127, 0 }, { -2235, -2496, -137, 0 }, { 2226, 2496, 127, 0 }, { -2235, 2496, -137, 0 } }, { -243, 0, 4090, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_night_dilapidated_house_80189500[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -1500, 500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2000, 3549 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2500, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2256, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2256, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2256, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2500, -1500, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2000, 6400 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1500, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2000, 3003 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2500, -1500, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1232, 1232 }, { 0, 0 } }, 2000, 3000 },
};

WorldCoordSpotLight D_dryfield_night_dilapidated_house_80189800[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -ONE, 0, 0 }, { 0, -2902, 2901 }, { 0, 2901, 2901 } }, { -5000, -2500, -4000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 0, 0 }, { 0, 0 } }, { 0, 2896, 2896, 0 }, 100, 3000, 625 },
};

/// Unreferenced bytes following the room's cone light in the overlay image.
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

/// Entry 1 of the room's two-entry descriptor table, the task that plays a
/// stream. It blanks the display and allocates the auxiliary buffers, looks
/// up the stream slot for the current location with view 0x65 or 0x64
/// (0x65 on disc 2, by `Wip_SysFlags.discNumber`) and queues CD command 0x61 for it,
/// shows the display once the queue's `field_1FA` is set, and blanks it again
/// when the CD goes idle - or, on the pad's 0x800 flag, early, activating CD
/// phase 1. Once the CD is idle it restores the stream state, clears the
/// image buffers, shows the display again, kills itself and restores session
/// image memory and game-loop presentation.
void func_dryfield_night_dilapidated_house_8017DB20(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    s16         slot;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state = task->state + 1;
            return;
        case 1:
            key = gGameSession->location;
            if (Wip_SysFlags.discNumber == GAME_MAIN_DISC_2) {
                key.loc.view = 0x65;
            } else {
                key.loc.view = 0x64;
            }
            slot         = streamFindMovieSlot(&key.loc, 0, 0);
            slotParam[0] = slot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state = task->state + 1;
            return;
        case 2:
            if (queue->movieReady == 0) {
                return;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case 3:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            return;
        case 4:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
            task->state = task->state + 1;
            return;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
                return;
            }
            memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
            SetDispMask(1);
            taskKill(task);
            displayResumeGameLoop();
            return;
    }
}

/// Entry 0 of the room's two-entry descriptor table: spawns entry 1, the
/// stream-playing task, with an ordering table, sets `gDisplayState.control.flags.flipMode`, spawns the
/// view tasks and kills itself.
void func_dryfield_night_dilapidated_house_8017DCE0(Task* arg0)
{
    Display_SpawnWithOt(D_dryfield_night_dilapidated_house_801872B4, 1, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

#include "../../shared/glow_draw_prism.inc.c"

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
    viewMask = 1 << gGameSession->location.loc.view;
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
