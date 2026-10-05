#include "rooms/dryfield_underpass.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

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

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_variants.h"
#include "../../shared/underpass_switches.h"

extern TaskDesc         gUnderpassSwitchTaskDesc[];
extern TaskMessageEntry D_dryfield_underpass_8017E830[];
extern s32              D_dryfield_underpass_8017E89C;
extern EvsCommand       D_dryfield_underpass_8017E8D8[];
extern SVECTOR          D_dryfield_underpass_8017EAD0[8];
extern s16              D_dryfield_underpass_8017EB10[8];

extern WorldCollisionGrid     D_dryfield_underpass_8017F484[1];
extern WorldCollisionOccluder D_dryfield_underpass_80180AA8[3];
extern WorldCollisionTrigger  D_dryfield_underpass_80180388[16];
extern WorldCollisionTrigger  D_dryfield_underpass_80180848[2];
extern WorldCollisionTrigger  D_dryfield_underpass_801808E0[6];
extern WorldCoordRoomLights   D_dryfield_underpass_80180EBC[1];
extern WorldCoordRoomLights   D_dryfield_underpass_80181114[1];

extern AnimationPlayRequest     D_dryfield_underpass_8017E870;
extern AnimationPlayRequest     D_dryfield_underpass_8017E884;
extern ActorCommand             D_dryfield_underpass_8017E8A0;
extern ActorCommand             D_dryfield_underpass_8017E8A4;
extern ActorCommand             D_dryfield_underpass_8017E8A8;
extern AnimationBankCopyRequest D_dryfield_underpass_8017E868;
void                            func_dryfield_underpass_8017DA08(void);

s32 func_dryfield_underpass_8017D900(Task*, s32, s32, s32);
s32 func_dryfield_underpass_8017D908(Task*, s32, RoomEventMsg*, RoomEventMsg*);

static AnimationPackedPose _gDryfieldUnderpassAnimation00E64Bank1[10] = {
#include "assets/dryfield_underpass_animation_00E64_bank1.inc"
};

static AnimationPackedRotation _gDryfieldUnderpassAnimation00E64Bank4[126] = {
#include "assets/dryfield_underpass_animation_00E64_bank4.inc"
};

static AnimationRecord _gDryfieldUnderpassAnimation00E64Records[171] = {
#include "assets/dryfield_underpass_animation_00E64_records.inc"
};

static u16 _gDryfieldUnderpassAnimation00E64Indices[20] = {
#include "assets/dryfield_underpass_animation_00E64_indices.inc"
};

static AnimationSet _gDryfieldUnderpassAnimation00E64 = {
    _gDryfieldUnderpassAnimation00E64Records,
    _gDryfieldUnderpassAnimation00E64Indices,
    { NULL, _gDryfieldUnderpassAnimation00E64Bank1, NULL, NULL, _gDryfieldUnderpassAnimation00E64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldUnderpassAnimation01230Bank1[6] = {
#include "assets/dryfield_underpass_animation_01230_bank1.inc"
};

static AnimationPackedRotation _gDryfieldUnderpassAnimation01230Bank4[64] = {
#include "assets/dryfield_underpass_animation_01230_bank4.inc"
};

static AnimationRecord _gDryfieldUnderpassAnimation01230Records[141] = {
#include "assets/dryfield_underpass_animation_01230_records.inc"
};

static u16 _gDryfieldUnderpassAnimation01230Indices[20] = {
#include "assets/dryfield_underpass_animation_01230_indices.inc"
};

static AnimationSet _gDryfieldUnderpassAnimation01230 = {
    _gDryfieldUnderpassAnimation01230Records,
    _gDryfieldUnderpassAnimation01230Indices,
    { NULL, _gDryfieldUnderpassAnimation01230Bank1, NULL, NULL, _gDryfieldUnderpassAnimation01230Bank4, NULL, NULL, NULL },
};

TaskDesc gUnderpassSwitchTaskDesc[2] = {
    { { { TASK_BODY_NONE, 32 } }, underpassSwitchTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_dryfield_underpass_8017E830[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantUnderpassMsg },
    { 5105, func_dryfield_underpass_8017D900 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_underpass_8017D908 },
    { ROOM_MESSAGE_COMMAND, underpassSwitchMsg },
    { ROOM_MESSAGE_SOUND, underpassSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_dryfield_underpass_8017E860[2] = {
    &_gDryfieldUnderpassAnimation00E64,
    &_gDryfieldUnderpassAnimation01230,
};

AnimationBankCopyRequest D_dryfield_underpass_8017E868 = { { .sets = D_dryfield_underpass_8017E860 }, ARRAY_SIZE(D_dryfield_underpass_8017E860) };

AnimationPlayRequest D_dryfield_underpass_8017E870 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_underpass_8017E884 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

ActorCommand D_dryfield_underpass_8017E898 = { { .loc = { 5, 11 } }, 0 };

s32 D_dryfield_underpass_8017E89C = 0x10B05;

ActorCommand D_dryfield_underpass_8017E8A0 = { { .loc = { 5, 11 } }, 2 };

ActorCommand D_dryfield_underpass_8017E8A4 = { { .loc = { 5, 11 } }, 3 };

ActorCommand D_dryfield_underpass_8017E8A8 = { { .loc = { 5, 11 } }, 4 };

AnimationPlayRequest D_dryfield_underpass_8017E8AC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_dryfield_underpass_8017E8C0 = { { 0x3EE0, -1000, -3624, 0 }, { 0, -2048, 0, 0 } };

EvsCommand D_dryfield_underpass_8017E8D8[21] = {
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_underpass_8017E868 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_underpass_8017E884 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_underpass_8017E8A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_underpass_8017E870 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_underpass_8017E8A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_underpass_8017E8C0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_underpass_8017E8A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_underpass_8017DA08 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

SVECTOR D_dryfield_underpass_8017EAD0[8] = {
    { 0x4588, -3480, -1500, 0 },
    { 0x3B60, -3480, -6000, 0 },
    { 0x4588, -3480, -8700, 0 },
    { 0x34BC, -3480, -7200, 0 },
    { 9300, -3480, -9800, 0 },
    { 6200, -3480, -0x29CC, 0 },
    { 4850, -3420, -7200, 0 },
    { 500, -3480, -9700, 0 },
};

s16 D_dryfield_underpass_8017EB10[8] = {
    2052,
    8,
    536,
    528,
    16,
    64,
    32,
    160,
};

WorldCollisionRoomResources D_dryfield_underpass_8017EB20[6] = {
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180388, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180388, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180388, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180388, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180848, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180848, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
};

u8 D_dryfield_underpass_8017EB80[12] = {
    1,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

u8 D_dryfield_underpass_8017EB8C[12] = {
    1,
    11,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8 D_dryfield_underpass_8017EB98[12] = {
    1,
    21,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

u8 D_dryfield_underpass_8017EBA4[12] = {
    1,
    22,
    23,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8 D_dryfield_underpass_8017EBB0[12] = {
    1,
    24,
    25,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8* D_dryfield_underpass_8017EBBC[6] = {
    gViewIdentityMap,
    D_dryfield_underpass_8017EB80,
    D_dryfield_underpass_8017EB8C,
    D_dryfield_underpass_8017EB98,
    D_dryfield_underpass_8017EBA4,
    D_dryfield_underpass_8017EBB0,
};

ViewCount D_dryfield_underpass_8017EBD4[6] = { 26, 11, 11, 11, 11, 11 };

WorldCoordRoomLighting D_dryfield_underpass_8017EBE0[6] = {
    { D_dryfield_underpass_80180EBC, NULL },
    { D_dryfield_underpass_80181114, NULL },
    { D_dryfield_underpass_80180EBC, NULL },
    { D_dryfield_underpass_80181114, NULL },
    { D_dryfield_underpass_80180EBC, NULL },
    { D_dryfield_underpass_80180EBC, NULL },
};

DirectionWarpEntry D_dryfield_underpass_8017EC10[3] = {
    { { { .word = 0 }, 5303, -997, -0x2C94 }, { 0, 0, 0, 0 }, { { .word = 0 }, 5303, -997, -0x2C94 }, { 0, 0, 0, 0 }, 0x52260008, 0x52260007, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, 463 },
    { { { .word = 1024 }, 0x3D31, -1000, -4216 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 0x3D31, -1000, -4216 }, { 0, 0, 0, 0 }, 0x52260006, 0x52260005, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 462 },
    { { { .word = 2048 }, 1212, -1000, -7543 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 1212, -1000, -7543 }, { 0, 0, 0, 0 }, 0x52260001, 0x52260001, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldUnderpassCollision01EC4Normals[12] = {
#include "assets/dryfield_underpass_collision_01EC4_normals.inc"
};

static SVECTOR _gDryfieldUnderpassCollision01EC4Verts[70] = {
#include "assets/dryfield_underpass_collision_01EC4_verts.inc"
};

static WorldCollisionGridFace _gDryfieldUnderpassCollision01EC4Faces[46] = {
#include "assets/dryfield_underpass_collision_01EC4_faces.inc"
};

static s16 _gDryfieldUnderpassCollision01EC4Cells[346] = {
#include "assets/dryfield_underpass_collision_01EC4_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldUnderpassCollision01EC4Cells[i])
static s16* _gDryfieldUnderpassCollision01EC4Table[24] = {
#include "assets/dryfield_underpass_collision_01EC4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_underpass_8017F484[1] = {
    { NULL, _gDryfieldUnderpassCollision01EC4Normals, _gDryfieldUnderpassCollision01EC4Verts, _gDryfieldUnderpassCollision01EC4Faces, _gDryfieldUnderpassCollision01EC4Table, 3000, 0x32C8, 6, 4, 4000, 46 },
};

ViewCamera D_dryfield_underpass_8017F4A8[26] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7864, 0x6671, 5100 } }, 329 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { -4047, 0, 631 }, { -451, 2858, -2898 }, { -440, -2933, -2824 } }, { -0x4229, 2000, 6853 } }, 348 },
};

AreaResource D_dryfield_underpass_8017F850[2] = {
    { 5, 5, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_400500_80153D60 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_underpass_8017F868[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017BCC4, D_dryfield_underpass_8017F850 },
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
};

SpriteBatch D_dryfield_underpass_8017F8D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_8017F8E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_underpass_8017F8F0[16] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_underpass_8017FA30[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_underpass_8017FA48[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_underpass_8017FB24[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_underpass_8017FB3C[10] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_underpass_8017FC04[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_8017FC24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_8017FC34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_8017FC44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_underpass_8017FC54[11] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_underpass_8017FD30[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_8017FD50[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_8017FD60[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_8017FD70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_underpass_8017FD80[16] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_underpass_8017FEC0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_underpass_8017FED8[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_underpass_8017FFB4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_underpass_8017FFCC[10] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_underpass_80180094[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_801800B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_801800C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_801800D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_underpass_801800E4[11] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_underpass_801801C0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_801801E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_801801F0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_80180200[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_80180210[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_80180220[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_80180230[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_underpass_80180240[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_underpass_80180250[26] = {
    { { .empty = D_dryfield_underpass_8017F8D0 }, D_dryfield_underpass_8017F8D0, NULL },
    { { .empty = D_dryfield_underpass_8017F8E0 }, D_dryfield_underpass_8017F8E0, NULL },
    { { .elements = D_dryfield_underpass_8017F8F0 }, D_dryfield_underpass_8017FA30, NULL },
    { { .elements = D_dryfield_underpass_8017FA48 }, D_dryfield_underpass_8017FB24, NULL },
    { { .elements = D_dryfield_underpass_8017FB3C }, D_dryfield_underpass_8017FC04, NULL },
    { { .empty = D_dryfield_underpass_8017FC24 }, D_dryfield_underpass_8017FC24, NULL },
    { { .empty = D_dryfield_underpass_8017FC34 }, D_dryfield_underpass_8017FC34, NULL },
    { { .empty = D_dryfield_underpass_8017FC44 }, D_dryfield_underpass_8017FC44, NULL },
    { { .elements = D_dryfield_underpass_8017FC54 }, D_dryfield_underpass_8017FD30, NULL },
    { { .empty = D_dryfield_underpass_8017FD50 }, D_dryfield_underpass_8017FD50, NULL },
    { { .empty = D_dryfield_underpass_8017FD60 }, D_dryfield_underpass_8017FD60, NULL },
    { { .empty = D_dryfield_underpass_8017FD70 }, D_dryfield_underpass_8017FD70, NULL },
    { { .elements = D_dryfield_underpass_8017FD80 }, D_dryfield_underpass_8017FEC0, NULL },
    { { .elements = D_dryfield_underpass_8017FED8 }, D_dryfield_underpass_8017FFB4, NULL },
    { { .elements = D_dryfield_underpass_8017FFCC }, D_dryfield_underpass_80180094, NULL },
    { { .empty = D_dryfield_underpass_801800B4 }, D_dryfield_underpass_801800B4, NULL },
    { { .empty = D_dryfield_underpass_801800C4 }, D_dryfield_underpass_801800C4, NULL },
    { { .empty = D_dryfield_underpass_801800D4 }, D_dryfield_underpass_801800D4, NULL },
    { { .elements = D_dryfield_underpass_801800E4 }, D_dryfield_underpass_801801C0, NULL },
    { { .empty = D_dryfield_underpass_801801E0 }, D_dryfield_underpass_801801E0, NULL },
    { { .empty = D_dryfield_underpass_801801F0 }, D_dryfield_underpass_801801F0, NULL },
    { { .empty = D_dryfield_underpass_80180200 }, D_dryfield_underpass_80180200, NULL },
    { { .empty = D_dryfield_underpass_80180210 }, D_dryfield_underpass_80180210, NULL },
    { { .empty = D_dryfield_underpass_80180220 }, D_dryfield_underpass_80180220, NULL },
    { { .empty = D_dryfield_underpass_80180230 }, D_dryfield_underpass_80180230, NULL },
    { { .empty = D_dryfield_underpass_80180240 }, D_dryfield_underpass_80180240, NULL },
};

WorldCollisionTrigger D_dryfield_underpass_80180388[16] = {
    { NULL, NULL, NULL, { 0x4070, -2560, -5280, 0 }, { { -1616, -1888, 0, 0 }, { 1616, -1888, 0, 0 }, { -1616, 1888, 0, 0 }, { 1616, 1888, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2544, -5408, 0 }, { { 1616, -1840, 0, 0 }, { -1616, -1840, 0, 0 }, { 1616, 1840, 0, 0 }, { -1616, 1840, 0, 0 } }, { 0, 0, 4105, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3A3F, -2720, -8402, 0 }, { { 65, -1824, -1727, 0 }, { -64, -1824, 1728, 0 }, { 65, 1824, -1727, 0 }, { -64, 1824, 1728, 0 } }, { 4101, 0, 151, 0 }, { 0, 0, 4096, 0 }, 2508, 0, 3, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3ABE, -2736, -8307, 0 }, { { -81, -1808, 1758, 0 }, { 81, -1808, -1757, 0 }, { -81, 1808, 1758, 0 }, { 81, 1808, -1757, 0 } }, { -4096, 0, -190, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 9, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9023, -2688, -8449, 0 }, { { 0, -1824, 1615, 0 }, { 1, -1824, -1616, 0 }, { 0, 1824, 1615, 0 }, { 1, 1824, -1616, 0 } }, { -4102, 0, -2, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8799, -2736, -8577, 0 }, { { -79, -1808, -1614, 0 }, { 78, -1808, 1613, 0 }, { -79, 1808, -1614, 0 }, { 78, 1808, 1613, 0 } }, { 4091, 0, -200, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5214, -2656, -0x27A2, 0 }, { { 1616, -1824, 1, 0 }, { -1615, -1824, 0, 0 }, { 1616, 1824, 1, 0 }, { -1615, 1824, 0, 0 } }, { -2, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5150, -2624, -9954, 0 }, { { -1615, -1888, 0, 0 }, { 1616, -1888, 1, 0 }, { -1615, 1888, 0, 0 }, { 1616, 1888, 1, 0 } }, { 0, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3230, -2624, -8482, 0 }, { { -79, -1792, -2412, 0 }, { 79, -1792, 2413, 0 }, { -79, 1792, -2412, 0 }, { 79, 1792, 2413, 0 } }, { 4094, 0, -135, 0 }, { 0, 0, 4096, 0 }, 2996, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3422, -2624, -8513, 0 }, { { 63, -1856, 2224, 0 }, { -63, -1856, -2223, 0 }, { 63, 1856, 2224, 0 }, { -63, 1856, -2223, 0 } }, { -4094, 0, 115, 0 }, { 0, 0, 4096, 0 }, 2896, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -960, -2592, -8577, 0 }, { { 306, -1856, 1579, 0 }, { -322, -1856, -1589, 0 }, { 306, 1856, 1579, 0 }, { -322, 1856, -1589, 0 } }, { -4028, 0, 798, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1056, -2528, -8385, 0 }, { { -322, -1856, -1592, 0 }, { 308, -1856, 1578, 0 }, { -322, 1856, -1592, 0 }, { 308, 1856, 1578, 0 } }, { 4028, 0, -802, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3130, -2592, -8513, 0 }, { { 17, -1824, 1905, 0 }, { -16, -1824, -1904, 0 }, { 17, 1824, 1905, 0 }, { -16, 1824, -1904, 0 } }, { -4104, 0, 35, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3061, -2528, -8577, 0 }, { { 1, -1824, -1615, 0 }, { 0, -1824, 1616, 0 }, { 1, 1824, -1615, 0 }, { 0, 1824, 1616, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2624, -1029, 0 }, { { 1577, -1840, 182, 0 }, { -1630, -1840, -237, 0 }, { 1577, 1840, 182, 0 }, { -1630, 1840, -237, 0 } }, { -534, 0, 4073, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 10, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40F1, -2624, -962, 0 }, { { -1854, -1840, -209, 0 }, { 1834, -1840, 184, 0 }, { -1854, 1840, -209, 0 }, { 1834, 1840, 184, 0 } }, { 434, 0, -4078, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 2, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_underpass_80180848[2] = {
    { NULL, NULL, NULL, { 0x4090, -2560, -2752, 0 }, { { 2096, -1888, 0, 0 }, { -2096, -1888, 0, 0 }, { 2096, 1888, 0, 0 }, { -2096, 1888, 0, 0 } }, { 0, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4090, -2544, -2656, 0 }, { { -2144, -1840, 0, 0 }, { 2144, -1840, 0, 0 }, { -2144, 1840, 0, 0 }, { 2144, 1840, 0, 0 } }, { 0, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_underpass_801808E0[6] = {
    { NULL, NULL, NULL, { 5184, -1088, -0x2C70, 0 }, { { -1024, 0, -432, 0 }, { 1024, 0, -432, 0 }, { -1024, 0, 432, 0 }, { 1024, 0, 432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_WARP, 34, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1280, -1056, -7456, 0 }, { { -896, 0, -432, 0 }, { 896, 0, -432, 0 }, { -896, 0, 432, 0 }, { 896, 0, 432, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 993, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 51, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3C00, -1088, -4096, 0 }, { { 368, 0, -928, 0 }, { 368, 0, 928, 0 }, { -368, 0, -928, 0 }, { -368, 0, 928, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 997, WORLD_COLLISION_TRIGGER_ACTION_WARP, 32, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1522, -1056, -8433, 0 }, { { -431, 0, -831, 0 }, { 433, 0, -832, 0 }, { -431, 0, 832, 0 }, { 432, 0, 832, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 936, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x40E0, -1056, -576, 0 }, { { -1024, 0, -304, 0 }, { 1024, 0, -304, 0 }, { -1024, 0, 304, 0 }, { 1024, 0, 304, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x40C0, -1056, -6240, 0 }, { { 1808, 0, -384, 0 }, { 1808, 0, 384, 0 }, { -1808, 0, -384, 0 }, { -1808, 0, 384, 0 } }, { 0, 4099, 0, 0 }, { 3166, 0, -2598, 0 }, 1846, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_underpass_80180AA8[3] = {
    { NULL, NULL, { 1184, -2304, -0x2840, 0 }, { { -2912, -3328, 0, 0 }, { 2912, -3328, 0, 0 }, { -2912, 3328, 0, 0 }, { 2912, 3328, 0, 0 } }, { 0, 0, -4106, 0 }, 4404, 1, 0 },
    { NULL, NULL, { 0x2FB0, -2240, -0x2820, 0 }, { { -5840, -3264, 0, 0 }, { 5840, -3264, 0, 0 }, { -5840, 3264, 0, 0 }, { 5840, 3264, 0, 0 } }, { 0, 0, -4111, 0 }, 6675, 1, 0 },
    { NULL, NULL, { 8992, -3328, -4416, 0 }, { { -5840, -2848, 2496, 0 }, { 5840, -2848, -2496, 0 }, { -5840, 2848, 2496, 0 }, { 5840, 2848, -2496, 0 } }, { -1614, 0, -3777, 0 }, 6945, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_underpass_80180B5C[9] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3214, -1973, -3920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1966, 2621, 3276 }, { 0, 0 } }, 2500, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x44C0, -3483, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3C28, -3483, -6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x44C0, -3483, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9300, -3483, -9600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x34BC, -3483, -7400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6200, -3483, -0x2904 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -3483, -9500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4850, -3423, -7400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
};

WorldCoordRoomLights D_dryfield_underpass_80180EBC[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_underpass_80180B5C), D_dryfield_underpass_80180B5C, 0, NULL },
};

WorldCoordPointLight D_dryfield_underpass_80180ED4[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4268, -2483, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -2483, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3F48, -2483, -8000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2AF8, -2483, -8300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5850, -2423, -9100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x314C, -1973, -3920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 4096, 4096 }, { 0, 0 } }, 0, 8000 },
};

WorldCoordRoomLights D_dryfield_underpass_80181114[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_underpass_80180ED4), D_dryfield_underpass_80180ED4, 0, NULL },
};

WorldCollisionFootstepSounds D_dryfield_underpass_8018112C = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

WorldCollisionFootstepSounds D_dryfield_underpass_80181138 = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionSurfaceProperties D_dryfield_underpass_80181144[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_underpass_8018114C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_underpass_8018112C },
};

WorldCollisionSurfaceProperties D_dryfield_underpass_80181154[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_underpass_8018115C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_underpass_80181138 },
};

WorldCollisionSurfaceProperties* D_dryfield_underpass_80181164[8] = {
    D_dryfield_underpass_80181144,
    D_dryfield_underpass_80181144,
    D_dryfield_underpass_80181154,
    D_dryfield_underpass_8018115C,
    D_dryfield_underpass_8018114C,
    D_dryfield_underpass_80181144,
    D_dryfield_underpass_80181144,
    D_dryfield_underpass_80181144,
};

static void func_dryfield_underpass_8017D970(Task* arg0);
static void func_dryfield_underpass_8017DA00(Task* task);

#include "../../shared/underpass_switches_task.inc.c"

#include "../../shared/room_variants_underpass.inc.c"

#include "../../shared/underpass_switches_msg.inc.c"

#include "../../shared/underpass_sound_msg.inc.c"

static void _glowDrawFlareLocal(const GfxCoord* coord, const SVECTOR* localPoint, s32 textureIndex, s32 radiusScale);

/// Handler for message 0x13F1: does nothing and returns 0.
s32 func_dryfield_underpass_8017D900(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for message 0x13EF: the first time record `field_2` 1 arrives while
/// the session's place is 1 and nibble 0xC9 is clear, sets that nibble and
/// runs the room's script `D_dryfield_underpass_8017E8D8`. Always returns 0.
s32 func_dryfield_underpass_8017D908(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp_v1;

    temp_v1 = in->warp;
    if ((temp_v1 == 1) && (gGameSession->location.loc.variant == temp_v1) && (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) == 0)) {
        gameFlagSetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN, 1);
        func_800E8614(D_dryfield_underpass_8017E8D8, 0);
    }
    return 0;
}

/// First state of the room task: parks the room's message table in
/// `Task::msgTable` and publishes the task in pointer slot 7. While the
/// session's place is 1 and nibble 0xC9 is clear it also sends message 0x7DA,
/// with `D_dryfield_underpass_8017E89C`, to the task in slot 4. Then advances.
static void func_dryfield_underpass_8017D970(Task* arg0)
{
    arg0->msgTable = D_dryfield_underpass_8017E830;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.variant == 1) && (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) == 0)) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_dryfield_underpass_8017E89C, ACTOR_COMMAND_MESSAGE_APPLY);
    }
    arg0->state = arg0->state + 1;
}

/// Second state of the room task: idles.
static void func_dryfield_underpass_8017DA00(Task* task)
{
}

/// Picks the room variant to load next from nibbles 0xC9, 0x53 and 0x51, the
/// same choice the switch task `underpassSwitchTask` makes when it
/// toggles nibble 0x51, and writes it to the session's room and to
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room`, then flags the room objects dirty. Reached from the room's
/// script data.
void func_dryfield_underpass_8017DA08(void)
{
    RoomEventMsg  src;
    RoomEventMsg  dst;
    RoomEventMsg* s;
    RoomEventMsg* d;
    GameSession*  session;
    u8            room;

    d             = &dst;
    s             = &src;
    src.areaId    = GAME_AREA_DRYFIELD_UNDERPASS;
    src.queryOnly = ROOM_EVENT_EXECUTE;
    if (s->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) != 0) {
            if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
                d->room = 2;
            } else {
                d->room = 1;
            }
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
                dst.room = dst.room + 2;
            }
        } else {
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) != 0) {
                d->room = 5;
            } else {
                d->room = 6;
            }
        }
    }
    session                                                    = gGameSession;
    room                                                       = dst.room;
    session->location.loc.room                                 = room;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = room;
    gGameSession->roomObjsDirty                                = 1;
}

/// State handlers of the room task `func_dryfield_underpass_8017DAC8`, indexed
/// by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_dryfield_underpass_8017D5C4 = {
    { func_dryfield_underpass_8017D970, func_dryfield_underpass_8017DA00, taskKill },
};

/// Room task: runs the state handler `D_dryfield_underpass_8017D5C4` names for
/// `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_underpass_8017DAC8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_underpass_8017D5C4;
    sp.funcs[task->state](task);
}

#include "../../shared/glow_draw_flare_local.inc.c"

/// Per-frame effect on a coordinate task: draws the glow sprites the current visit
/// lights, one per point in `D_...EAD0` (in the task's local space) whose
/// `D_...EB10` bitmask contains the visit's bit (`gGameSession->location.loc.view`).
/// The whole effect is skipped unless nibble 0x53 is clear.
void func_dryfield_underpass_8017DE30(Task* task)
{
    GfxCoord* coord;
    s32       mask;
    s32       i;
    SVECTOR*  vec;
    s16*      flags;

    coord = task->extra.coordBody->coord;
    mask  = 1 << gGameSession->location.loc.view;
    if (gameFlagGetNibble(GAME_FLAG_053) == 0) {
        i     = 0;
        vec   = D_dryfield_underpass_8017EAD0;
        flags = D_dryfield_underpass_8017EB10;
        do {
            if (mask & *flags) {
                _glowDrawFlareLocal(coord, vec, 0, 0x280);
            }
            vec++;
            i++;
            flags++;
        } while (i < 8);
    }
}
