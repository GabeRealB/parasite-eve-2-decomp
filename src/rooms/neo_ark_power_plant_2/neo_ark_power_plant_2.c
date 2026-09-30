#include "rooms/neo_ark_power_plant_2.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room_common.h"

extern GpObj3A                    D_neo_ark_power_plant_2_80182E78[1];
extern WorldCoordRoomAmbientEntry D_neo_ark_power_plant_2_80182EB4[10];

extern GpMsgEntry     D_neo_ark_power_plant_2_801801F8[];
extern GpEvsCmd       D_neo_ark_power_plant_2_801802A8[];
extern GpEvsCmd       D_neo_ark_power_plant_2_80180560[];
extern SVECTOR        D_neo_ark_power_plant_2_80180668;
extern SVECTOR        D_neo_ark_power_plant_2_80180678;
extern GpAreaApplyRec D_neo_ark_power_plant_2_80182F70[];
extern GpAreaApplyRec D_neo_ark_power_plant_2_80182F94[];

/// The smoke trail's two spawn offsets: `[0]` places the object's own frame
/// and `[1]` the second trail's frame. `D_neo_ark_power_plant_2_80180680[1]` is
/// `[1]` under its own name, which the per-frame path reads directly.

static void func_neo_ark_power_plant_2_8017D6F4(Task* task);
static void func_neo_ark_power_plant_2_8017D758(Task* task);
static void func_neo_ark_power_plant_2_8017DA54(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_neo_ark_power_plant_2_8017E098(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_neo_ark_power_plant_2_8017E4C4(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_power_plant_2_8017ED48(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_neo_ark_power_plant_2_8017F3C8(GfxCoord* arg0, s16 arg1, u8* arg2);

/// State table of the room's message-driven task, indexed by `Task::state`:
/// install the message table, watch for the room's event trigger, then kill
/// the task.
static const TaskFuncTable3 D_neo_ark_power_plant_2_8017D5C4 = {
    { func_neo_ark_power_plant_2_8017D6F4, func_neo_ark_power_plant_2_8017D758, taskKill },
};

extern GpAreaTmdRec D_neo_ark_power_plant_2_80182D80[3];
extern GpAreaTmdRec D_neo_ark_power_plant_2_80182DA4[3];
extern GpAreaTmdRec D_neo_ark_power_plant_2_80182DC8[2];

extern GpGridParams   D_neo_ark_power_plant_2_80180DC4[1];
extern GpObj4C        D_neo_ark_power_plant_2_801828C0[8];
extern GpObj4C        D_neo_ark_power_plant_2_80182B20[8];
extern GpRoomCoordSet D_neo_ark_power_plant_2_801828A8[1];

s32  func_neo_ark_power_plant_2_8017D5D0(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_neo_ark_power_plant_2_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_neo_ark_power_plant_2_8017D61C(Task*, s32, s32, TaskMessageArg);
s32  func_neo_ark_power_plant_2_8017D694(Task*, s32, TaskMessageArg, TaskMessageArg);
void func_neo_ark_power_plant_2_8017D69C(void);
void func_neo_ark_power_plant_2_8017D6D4(void);

AnimationPackedPose D_neo_ark_power_plant_2_8017FDF0[3] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_bank1.inc"
};

AnimationPackedRotation D_neo_ark_power_plant_2_8017FE14[28] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_bank4.inc"
};

AnimationRecord D_neo_ark_power_plant_2_8017FE84[88] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_records.inc"
};

u16 D_neo_ark_power_plant_2_8017FFE4[20] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_indices.inc"
};

AnimationSet D_neo_ark_power_plant_2_8018000C = {
    D_neo_ark_power_plant_2_8017FE84,
    D_neo_ark_power_plant_2_8017FFE4,
    { NULL, D_neo_ark_power_plant_2_8017FDF0, NULL, NULL, D_neo_ark_power_plant_2_8017FE14, NULL, NULL, NULL },
};

AnimationPackedPose D_neo_ark_power_plant_2_80180034[2] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_bank1.inc"
};

AnimationPackedRotation D_neo_ark_power_plant_2_8018004C[22] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_bank4.inc"
};

AnimationRecord D_neo_ark_power_plant_2_801800A4[65] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_records.inc"
};

u16 D_neo_ark_power_plant_2_801801A8[20] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_indices.inc"
};

AnimationSet D_neo_ark_power_plant_2_801801D0 = {
    D_neo_ark_power_plant_2_801800A4,
    D_neo_ark_power_plant_2_801801A8,
    { NULL, D_neo_ark_power_plant_2_80180034, NULL, NULL, D_neo_ark_power_plant_2_8018004C, NULL, NULL, NULL },
};

GpMsgEntry D_neo_ark_power_plant_2_801801F8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_power_plant_2_8017D5D8 },
    { 5105, func_neo_ark_power_plant_2_8017D5D0 },
    { 5103, func_neo_ark_power_plant_2_8017D694 },
    { 5104, func_neo_ark_power_plant_2_8017D61C },
    { 0x7FFFFFFF, NULL },
};

AnimationPlayRequest D_neo_ark_power_plant_2_80180220 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

ActorCommand D_neo_ark_power_plant_2_80180234 = { { .loc = { 5, 16 } }, 1 };

ActorCommand D_neo_ark_power_plant_2_80180238 = { { .loc = { 5, 16 } }, 2 };

ActorCommand D_neo_ark_power_plant_2_8018023C = { { .loc = { 5, 16 } }, 3 };

AnimationSet* D_neo_ark_power_plant_2_80180240[3] = {
    NULL,
    &D_neo_ark_power_plant_2_8018000C,
    &D_neo_ark_power_plant_2_801801D0,
};

AnimationPlayRequest D_neo_ark_power_plant_2_8018024C = { { .sets = D_neo_ark_power_plant_2_80180240 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_neo_ark_power_plant_2_80180260 = { { .sets = D_neo_ark_power_plant_2_80180240 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_neo_ark_power_plant_2_80180274 = { { 4000, -5000, -1540, 0 }, { 0, -2275, 0, 0 } };

GpScriptCmd D_neo_ark_power_plant_2_8018028C[5] = {
    { 257, 1 },
    { 258, 1282 },
    { 0x2803, 3843 },
    { 4, 4 },
    { 0, 0 },
};

GpScriptRec D_neo_ark_power_plant_2_801802A0[2] = {
    { 236, 77, 4, 1 },
    { 0, 0, 2, 0 },
};

GpEvsCmd D_neo_ark_power_plant_2_801802A8[29] = {
    { 13, { .callbackNoArg = func_neo_ark_power_plant_2_8017D69C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_power_plant_2_80180220 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55100006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55100007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 14, { .padCommands = D_neo_ark_power_plant_2_8018028C }, { .padRecords = D_neo_ark_power_plant_2_801802A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_neo_ark_power_plant_2_80180234 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_neo_ark_power_plant_2_80180274 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1012 }, { .storage = &D_neo_ark_power_plant_2_8018024C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1012 }, { .storage = &D_neo_ark_power_plant_2_80180260 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_neo_ark_power_plant_2_80180238 } }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_neo_ark_power_plant_2_80180560[11] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_neo_ark_power_plant_2_80180274 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_neo_ark_power_plant_2_8017D6D4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_neo_ark_power_plant_2_8018023C } }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_neo_ark_power_plant_2_80180668 = { 4000, -7000, -4400, 0 };

SVECTOR D_neo_ark_power_plant_2_80180670 = { 4000, -5000, -1540, 0 };

SVECTOR D_neo_ark_power_plant_2_80180678 = { 6140, -6085, -8500, 0 };

SVECTOR D_neo_ark_power_plant_2_80180680[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

GpRoomObjRec D_neo_ark_power_plant_2_80180690[1] = {
    { D_neo_ark_power_plant_2_80180DC4, D_neo_ark_power_plant_2_801828C0, D_neo_ark_power_plant_2_80182B20, D_neo_ark_power_plant_2_80182E78 },
};

GpRoomCoordRec D_neo_ark_power_plant_2_801806A0[1] = {
    { D_neo_ark_power_plant_2_801828A8, D_neo_ark_power_plant_2_80182EB4 },
};

u8* D_neo_ark_power_plant_2_801806A8[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_power_plant_2_801806AC[2] = {
    { { .bytes = { 9, 0 } } },
    { { .bytes = { 0, 0 } } },
};

GpWarpRec D_neo_ark_power_plant_2_801806B0[1] = {
    { { .words = { 3072, 4000, -5000, -400 } }, { 0, 0, 0, 0 }, { .words = { 3072, 4000, -5000, -400 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
};

SVECTOR D_neo_ark_power_plant_2_801806E8[11] = {
#include "assets/neo_ark_power_plant_2_collision_03804_normals.inc"
};

SVECTOR D_neo_ark_power_plant_2_80180740[82] = {
#include "assets/neo_ark_power_plant_2_collision_03804_verts.inc"
};

GpGridFace D_neo_ark_power_plant_2_801809D0[41] = {
#include "assets/neo_ark_power_plant_2_collision_03804_faces.inc"
};

s16 D_neo_ark_power_plant_2_80180BBC[236] = {
#include "assets/neo_ark_power_plant_2_collision_03804_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_power_plant_2_80180BBC[i])
s16* D_neo_ark_power_plant_2_80180D94[12] = {
#include "assets/neo_ark_power_plant_2_collision_03804_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_power_plant_2_80180DC4[1] = {
    { NULL, D_neo_ark_power_plant_2_801806E8, D_neo_ark_power_plant_2_80180740, D_neo_ark_power_plant_2_801809D0, D_neo_ark_power_plant_2_80180D94, 100, 0x2F44, 3, 4, 4000, 41 },
};

GpViewRec D_neo_ark_power_plant_2_80180DE8[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4500, 0x55F0, 6000 } }, 289 },
    { { { { -1159, 0, -3928 }, { -2285, 3331, 674 }, { 3195, 2382, -943 } }, { -791, 8408, 1169 } }, 230 },
    { { { { -875, 0, 4001 }, { 1486, 3802, 325 }, { -3714, 1522, -812 } }, { -6791, 7658, 1319 } }, 257 },
    { { { { -1124, 0, 3938 }, { 333, 4081, 95 }, { -3924, 346, -1120 } }, { -8841, 6358, 1319 } }, 230 },
    { { { { 3952, 0, 1075 }, { -23, 4095, 85 }, { -1075, -88, 3951 } }, { -8341, 6098, 8139 } }, 257 },
    { { { { -4002, 0, 868 }, { -45, 4090, -211 }, { -867, -216, -3997 } }, { -8441, 5798, 2039 } }, 289 },
    { { { { -2378, 0, 3334 }, { 2574, 2603, 1836 }, { -2119, 3162, -1511 } }, { -8341, 7898, 6139 } }, 257 },
    { { { { 3869, 0, 1342 }, { -153, 4069, 441 }, { -1334, -467, 3844 } }, { -4702, 6286, 3621 } }, 680 },
    { { { { -4095, 0, -13 }, { 4, 3863, -1361 }, { 12, -1361, -3862 } }, { -3992, 5736, 1231 } }, 230 },
};

SpriteBatch D_neo_ark_power_plant_2_80180F2C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_power_plant_2_80180F3C[64] = {
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 128, -48, 1319, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 72, -40, 1325, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, -40, 1325, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 48, -32, 1300, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 104, -32, 1300, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 104, -24, 1146, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 104, -16, 1111, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 104, -8, 1082, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 72, 0, 1070, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 0, 1033, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 144, 0, 1047, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 120, 8, 1084, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 96, 8, 1062, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 72, 8, 1080, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, 0, 1172, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -8, 1242, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, -16, 1205, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, 0, 1232, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -24, 1200, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 64, -24, 1225, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, -16, 1086, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 72, -16, 1111, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -8, 1032, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 80, -8, 1092, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -40, 1573, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -120, 1298, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, -120, 1395, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, -120, 1622, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, -104, 1644, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -32, 1163, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -32, 1191, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, -32, 1534, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -24, 1514, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 88, 32, 1035, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 88, -64, 1500, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, -56, 1512, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -48, 1542, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, -56, 1450, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, -56, 1450, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, 120, 0, 1061, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, -32, 1450, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -40, 1475, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -40, 1500, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -32, 1525, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 48, 1097, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 48, 1097, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -80, 48, 1097, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 40, 1097, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 48, 1097, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -112, 40, 1163, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -112, 32, 1225, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 56, 794, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -72, 2000, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -72, -80, 1475, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -40, 1175, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, 16, 925, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -56, 16, 800, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 8, 1000, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 40, 1056, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, 0, 1000, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 24, 1025, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 24, 925, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 32, 875, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, 40, 875, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_8018143C[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 6, 0 } },
    { 24, 16, 0, 0, { 1, 0 } },
    { 40, 4, 0, 0, { 4, 0 } },
    { 44, 5, 0, 0, { 0, 0 } },
    { 49, 1, 0, 0, { 5, 0 } },
    { 50, 1, 0, 0, { 3, 0 } },
    { 51, 6, 0, 0, { 7, 0 } },
    { 57, 7, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_power_plant_2_8018148C[70] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 96, 864, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 48, 746, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, 64, 806, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 48, 750, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 32, 810, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, 24, 852, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, 16, 888, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, 16, 942, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, 8, 1007, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, 0, 1029, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 40, 779, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -136, 64, 843, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, 64, 879, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, 64, 874, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 64, 971, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 48, 1049, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 16, 936, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -160, 8, 998, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 24, 911, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 32, 845, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 40, 765, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -160, 0, 1044, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -8, 1093, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 0, 1172, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 0, 1162, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 16, 1011, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 16, 1011, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 24, 949, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 32, 899, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 32, 899, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 32, 899, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 40, 838, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 40, 997, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 48, 788, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 48, 784, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 48, 788, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 56, 754, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 56, 754, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 56, 929, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 64, 718, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 64, 798, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 72, 686, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 72, 686, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 72, 807, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 80, 657, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 80, 657, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 80, 702, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 88, 631, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 88, 631, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 96, 606, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 96, 561, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 104, 584, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 104, 584, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 112, 559, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 112, 559, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 32, 859, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 40, 812, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 48, 772, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 56, 743, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 64, 728, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 72, 715, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 72, 718, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 80, 702, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 80, 700, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 88, 690, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 88, 690, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 96, 678, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 96, 678, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 104, 667, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 112, 656, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_80181A04[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 3, 0 } },
    { 23, 2, 0, 0, { 0, 0 } },
    { 25, 30, 0, 0, { 2, 0 } },
    { 55, 15, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_power_plant_2_80181A34[30] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -144, -8, 1375, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -96, -8, 1375, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -96, 0, 1200, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -144, 0, 1200, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -136, 8, 1150, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 8, 1150, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -48, -8, 1375, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -88, -8, 1375, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -144, -16, 1375, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 0, 1151, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, 0, 950, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 0, 950, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 0, 925, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 0, 900, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 8, 875, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -104, 8, 875, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 24, 875, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 250, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, 0, 1000, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, 0, 750, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 48, 850, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, 16, 0x9E59, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, 64, 675, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, 32, 500, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 88, 525, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, 48, 375, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 96, 425, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 64, 250, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 80, 250, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 104, 250, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_80181C8C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 3, 0 } },
    { 6, 3, 0, 0, { 0, 0 } },
    { 9, 8, 0, 0, { 2, 0 } },
    { 17, 13, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_power_plant_2_80181CBC[28] = {
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 0, 1350, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -160, 8, 1350, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 16, 1350, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 16, 1350, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -160, 24, 1350, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -72, 48, 1350, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 48, 1350, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 48, 1350, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 48, 1350, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -128, 24, 1350, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, -128, 64, 1350, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -160, -120, 575, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, -96, 575, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, 0, 775, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -160, 56, 775, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -136, 0, 850, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -136, 56, 850, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 8, 875, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 56, 875, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 24, 950, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 64, 950, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, 32, 1075, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, 64, 1075, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 56, 1100, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -112, 8, 2000, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 48 } }, 40, 8, 2000, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -8, 8, 2000, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, 8, 2000, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_80181EEC[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 13, 0, 0, { 2, 0 } },
    { 24, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_power_plant_2_80181F14[8] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -8, 869, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, -8, 903, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, -8, 939, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 64, -40, 1575, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 48, -40, 1625, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 32, -40, 1700, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 16, -40, 2000, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 8, -40, 2000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_80181FB4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_power_plant_2_80181FD4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_power_plant_2_80181FE4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_power_plant_2_80181FF4[4] = {
    { 143, 0x3FC0, { .fields = { 80, 48 } }, -160, 72, 525, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 48 } }, 80, 72, 525, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 80 } }, -80, 40, 475, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 80 } }, 0, 40, 475, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_power_plant_2_80182044[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_neo_ark_power_plant_2_8018205C[9] = {
    { { .empty = D_neo_ark_power_plant_2_80180F2C }, D_neo_ark_power_plant_2_80180F2C, NULL },
    { { .elements = D_neo_ark_power_plant_2_80180F3C }, D_neo_ark_power_plant_2_8018143C, NULL },
    { { .elements = D_neo_ark_power_plant_2_8018148C }, D_neo_ark_power_plant_2_80181A04, NULL },
    { { .elements = D_neo_ark_power_plant_2_80181A34 }, D_neo_ark_power_plant_2_80181C8C, NULL },
    { { .elements = D_neo_ark_power_plant_2_80181CBC }, D_neo_ark_power_plant_2_80181EEC, NULL },
    { { .elements = D_neo_ark_power_plant_2_80181F14 }, D_neo_ark_power_plant_2_80181FB4, NULL },
    { { .empty = D_neo_ark_power_plant_2_80181FD4 }, D_neo_ark_power_plant_2_80181FD4, NULL },
    { { .empty = D_neo_ark_power_plant_2_80181FE4 }, D_neo_ark_power_plant_2_80181FE4, NULL },
    { { .elements = D_neo_ark_power_plant_2_80181FF4 }, D_neo_ark_power_plant_2_80182044, NULL },
};

GpPointLight D_neo_ark_power_plant_2_801820C8[21] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 990, -7530, -0x2710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 990, -7530, -7350 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 990, -7530, -4650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 990, -7530, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, -650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, -4650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, -7350 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, -0x2710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 1966, 1884, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6340, -7990, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4510, -5320, -0x2710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 1966, 1884, { 0, 0 } }, 1500, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5770, -7990, -7380 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5770, -7990, -4670 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4840, -6520, -2600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 1966, 1884, { 0, 0 } }, 1500, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3280, -7990, -4670 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3280, -7990, -7380 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3280, -7990, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3280, -7990, 700 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5770, -7990, 730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7980, -7500, 730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 990, -7530, 700 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3194, 3112, { 0, 0 } }, 1000, 4000 },
};

GpRoomCoordSet D_neo_ark_power_plant_2_801828A8[1] = {
    { 0, NULL, 21, D_neo_ark_power_plant_2_801820C8, 0, NULL },
};

GpObj4C D_neo_ark_power_plant_2_801828C0[8] = {
    { NULL, NULL, NULL, { 3040, -7040, 128, 0 }, { { 3, -2256, -1209, 0 }, { -2, -2256, 1209, 0 }, { 3, 2256, -1209, 0 }, { -2, 2256, 1209, 0 } }, { 4099, 0, 7, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 3135, -7104, 96, 0 }, { { -2, -2256, 1209, 0 }, { 3, -2256, -1209, 0 }, { -2, 2256, 1209, 0 }, { 3, 2256, -1209, 0 } }, { -4102, 0, -10, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 4096, -7104, -2272, 0 }, { { -2, -2256, 1209, 0 }, { 3, -2256, -1209, 0 }, { -2, 2256, 1209, 0 }, { 3, 2256, -1209, 0 } }, { -4102, 0, -10, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 3967, -7040, -2240, 0 }, { { 3, -2256, -1209, 0 }, { -2, -2256, 1209, 0 }, { 3, 2256, -1209, 0 }, { -2, 2256, 1209, 0 } }, { 4099, 0, 7, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 6304, -7104, -2336, 0 }, { { 3, -2256, -1209, 0 }, { -2, -2256, 1209, 0 }, { 3, 2256, -1209, 0 }, { -2, 2256, 1209, 0 } }, { 4099, 0, 7, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 6382, -7104, -2272, 0 }, { { -2, -2256, 1209, 0 }, { 3, -2256, -1209, 0 }, { -2, 2256, 1209, 0 }, { 3, 2256, -1209, 0 } }, { -4102, 0, -10, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 7887, -7072, -4785, 0 }, { { 2024, -2256, 142, 0 }, { -2023, -2256, -142, 0 }, { 2024, 2256, 142, 0 }, { -2023, 2256, -142, 0 } }, { -288, 0, 4090, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 7839, -7040, -4705, 0 }, { { -2024, -2256, -139, 0 }, { 2024, -2256, 140, 0 }, { -2024, 2256, -139, 0 }, { 2024, 2256, 140, 0 } }, { 281, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 6, 5, 129, 0 },
};

GpObj4C D_neo_ark_power_plant_2_80182B20[8] = {
    { NULL, NULL, NULL, { 3744, -64, -1616, 0 }, { { -320, 0, -496, 0 }, { 320, 0, -496, 0 }, { -320, 0, 496, 0 }, { 320, 0, 496, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 590, 1, 5, 64, 2, 0 },
    { NULL, NULL, NULL, { 4064, -5056, -480, 0 }, { { -336, 0, -496, 0 }, { 336, 0, -496, 0 }, { -336, 0, 496, 0 }, { 336, 0, 496, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 596, 257, 37, 64, 2, 0 },
    { NULL, NULL, NULL, { 4607, -769, -1600, 0 }, { { -16, -1391, -496, 0 }, { 17, 1392, -496, 0 }, { -16, -1391, 496, 0 }, { 17, 1392, 496, 0 } }, { -4101, 42, 0, 0 }, { -4096, 0, 0, 0 }, 1476, 0x8005, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 4927, -4673, -480, 0 }, { { -17, 1392, 496, 0 }, { 16, -1391, 496, 0 }, { -17, 1392, -496, 0 }, { 16, -1391, -496, 0 } }, { -4101, -49, 0, 0 }, { -4096, 0, 0, 0 }, 1476, 0x8000, 33, 18, 2, 0 },
    { NULL, NULL, NULL, { 256, -48, -1120, 0 }, { { -320, 0, -496, 0 }, { 320, 0, -496, 0 }, { -320, 0, 496, 0 }, { 320, 0, 496, 0 } }, { 0, 4113, 0, 0 }, { 4096, 0, 0, 0 }, 590, 0, 15, 18, 2, 0 },
    { NULL, NULL, NULL, { 6720, -5152, -7600, 0 }, { { -320, 0, -896, 0 }, { 320, 0, -896, 0 }, { -320, 0, 896, 0 }, { 320, 0, 896, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 951, 2, 3, 255, 2, 0 },
    { NULL, NULL, NULL, { 4288, -5152, -2160, 0 }, { { -2720, 0, -368, 0 }, { 2720, 0, -368, 0 }, { -2720, 0, 368, 0 }, { 2720, 0, 368, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 2733, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 6864, -5120, -4256, 0 }, { { -400, 0, -2352, 0 }, { 400, 0, -2352, 0 }, { -400, 0, 2352, 0 }, { 400, 0, 2352, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 2374, 2, 2, 255, 130, 0 },
};

GpAreaTmdRec D_neo_ark_power_plant_2_80182D80[3] = {
    { 53, 53, 0, 0, { 0, 0 }, D_8013D3FC },
    { 21, 21, 1, 0, { 0, 0 }, D_8014DC30 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_power_plant_2_80182DA4[3] = {
    { 57, 57, 0, 0, { 0, 0 }, D_801491F8 },
    { 21, 21, 1, 0, { 0, 0 }, D_8014DC30 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_power_plant_2_80182DC8[2] = {
    { 39, 39, 3, 0, { 0, 0 }, D_801540E0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_neo_ark_power_plant_2_80182DE0[19] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B7D0, D_neo_ark_power_plant_2_80182D80 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017B850, D_neo_ark_power_plant_2_80182DA4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017B8B0, D_neo_ark_power_plant_2_80182DC8 },
    { NULL, NULL },
};

GpObj3A D_neo_ark_power_plant_2_80182E78[1] = {
    { NULL, NULL, { 6064, -6976, -6944, 0 }, { { 208, 2944, 1760, 0 }, { -208, 2944, -1760, 0 }, { 208, -2944, 1760, 0 }, { -208, -2944, -1760, 0 } }, { 4068, 0, -481, 0 }, { 106, 13 }, 129, 0 },
};

WorldCoordRoomAmbientEntry D_neo_ark_power_plant_2_80182EB4[10] = {
    { .viewCount = ARRAY_SIZE(D_neo_ark_power_plant_2_80182EB4) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1639, 1232, 1229, 1384 } },
    { .color = { 16, 16, 16, 16 } },
};

s32 D_neo_ark_power_plant_2_80182F04[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

s32 D_neo_ark_power_plant_2_80182F10[3] = {
    0x10000015,
    0x10000017,
    0x10000019,
};

s32 D_neo_ark_power_plant_2_80182F1C[3] = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

GpRoomParamRec D_neo_ark_power_plant_2_80182F28[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_power_plant_2_80182F30[1] = {
    { 0, 0, 1, 0, D_neo_ark_power_plant_2_80182F10 },
};

GpRoomParamRec D_neo_ark_power_plant_2_80182F38[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_neo_ark_power_plant_2_80182F40[1] = {
    { 0, 0, 1, 0, D_neo_ark_power_plant_2_80182F04 },
};

GpRoomParamRec D_neo_ark_power_plant_2_80182F48[1] = {
    { 0, 0, 1, 0, D_neo_ark_power_plant_2_80182F1C },
};

GpRoomParamRec* D_neo_ark_power_plant_2_80182F50[8] = {
    D_neo_ark_power_plant_2_80182F28,
    D_neo_ark_power_plant_2_80182F28,
    D_neo_ark_power_plant_2_80182F30,
    D_neo_ark_power_plant_2_80182F38,
    D_neo_ark_power_plant_2_80182F40,
    D_neo_ark_power_plant_2_80182F48,
    D_neo_ark_power_plant_2_80182F28,
    D_neo_ark_power_plant_2_80182F28,
};

GpAreaApplyRec D_neo_ark_power_plant_2_80182F70[9] = {
    { 5, 8, 11, 1 },
    { 5, 11, 4, 1 },
    { 5, 13, 4, 1 },
    { 5, 21, 4, 17 },
    { 5, 21, 7, 33 },
    { 5, 29, 3, 1 },
    { 5, 32, 3, 1 },
    { 4, 18, 11, 0 },
    { 255, 0, 0, 0 },
};

GpAreaApplyRec D_neo_ark_power_plant_2_80182F94[4] = {
    { 5, 12, 2, 1 },
    { 5, 14, 2, 1 },
    { 5, 30, 2, 1 },
    { 255, 0, 0, 0 },
};

/// Message handler the room's message table names for one of its entries:
/// accepts the message and does nothing.
s32 func_neo_ark_power_plant_2_8017D5D0(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// passes both on to `func_map_neo_ark_80179B14`. Always returns 1.
s32 func_neo_ark_power_plant_2_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

s32 func_neo_ark_power_plant_2_8017D61C(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    s32 cmd;

    switch (arg2) {
        case 2:
            if (GameFlag_GetNibble(0xDF) == 0) {
                cmd = 2;
            } else {
                cmd = 5;
            }
            break;
        case 3:
            cmd = 7;
            if (Gp_StateF0.prefix.bytes.field_0 != 2) {
                cmd = GameFlag_GetNibble(0x147) != 0 ? 6 : 3;
            }
            break;
        default:
            goto done;
    }
    Gp_RunCapCmd1(cmd);
done:
    return 0;
}

s32 func_neo_ark_power_plant_2_8017D694(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

void func_neo_ark_power_plant_2_8017D69C(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

void func_neo_ark_power_plant_2_8017D6D4(void)
{
    Gp_HaltPadScripts();
}

static void func_neo_ark_power_plant_2_8017D6F4(Task* arg0)
{
    u8 temp_v1;

    arg0->msgTable = D_neo_ark_power_plant_2_801801F8;
    Game_SetPtrSlot(arg0, 7);
    temp_v1 = gGameSession->location.loc.variant;
    if (temp_v1 == 1) {
        gGameSession->flowFlags = temp_v1;
    }
    arg0->state = arg0->state + 1;
}

static void func_neo_ark_power_plant_2_8017D758(Task* task)
{
    Task* temp_v0;

    if (GameFlag_GetNibble(0xDF) == 0) {
        temp_v0 = Gp_LookupSlot4(0);
        if ((temp_v0 != 0) && (Gp_DispatchMsg(temp_v0, 0x7D6, 0, 0) == 0) && (Gp_StateC08.field_A != 1) &&
            (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
            GameFlag_SetNibble(0xDF, 1);
            GameFlag_SetNibble(0xB9, 1);
            GameFlag_SetNibble(0x1BC, 0);
            Gp_ApplyAreaRecs(D_neo_ark_power_plant_2_80182F70);
            if (GameFlag_GetNibble(0xF3) != 0) {
                Gp_ApplyAreaRecs(D_neo_ark_power_plant_2_80182F94);
            }
            Mc_SaveData[0].state.sceneEvent = 0x17;
            func_800E3FAC(0xA2, 0x2E);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 7);
            func_800E8634(D_neo_ark_power_plant_2_801802A8, 0, D_neo_ark_power_plant_2_80180560);
        }
    }
}

/// Dispatches the room's message-driven task through its three-state table
/// `D_neo_ark_power_plant_2_8017D5C4`, copied onto the stack before the call.
void func_neo_ark_power_plant_2_8017D854(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_power_plant_2_8017D5C4;
    sp.funcs[task->state](task);
}

void func_neo_ark_power_plant_2_8017D8AC(Task* arg0)
{
    u32           rnd;
    u16           intensity;
    GpPointLight* work;
    GpCoord64*    light;

    if (arg0->state == 0) {
        D_80115758  = 0x601DC;
        D_8011572C  = 0x601F8;
        D_80115750  = 0x60214;
        arg0->state = 1;
    }
    switch ((u8)Gp_GetViewIndex()) {
        case 6:
            if (GameFlag_GetNibble(0x147) != 0) {
                if (Gp_State1C->effectControl == ROOM_EFFECT_CONTROL_RUNNING && GameFlag_GetNibble(0xDF) == 0) {
                    rnd         = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState = rnd;
                    if (((rnd >> 16) & 7) == 0) {
                        Gp_SpawnEff(0x600E0, NULL, 0x400, &D_neo_ark_power_plant_2_80180678);
                    }
                }
            } else {
                func_neo_ark_power_plant_2_8017DA54(&D_neo_ark_power_plant_2_80180678, 0x300, 0x334);
            }
            break;
        case 8:
            light                                  = &Gp_RoomCoords[4];
            light->framesLeft                      = 4;
            work                                   = &light->light;
            work->inner                            = 0x400;
            work->outer                            = 0x4000;
            light->light.head.u.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            rnd                                    = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState                            = rnd;
            intensity                              = ((rnd >> 16) & 0x700) + 0x800;
            work->head.b                           = intensity;
            work->head.r                           = intensity >> 1;
            work->head.g                           = intensity >> 1;
            work->head.u.at.local.t[0]             = D_neo_ark_power_plant_2_80180668.vx;
            work->head.u.at.local.t[1]             = D_neo_ark_power_plant_2_80180668.vy;
            work->head.u.at.local.t[2]             = D_neo_ark_power_plant_2_80180668.vz;
            break;
    }
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, unless
/// the GTE flags the projection, queues four gouraud `POLY_G4` wedges filling a
/// disc around it, tinted at the centre and black at the rim. `arg1` is the
/// radius in world units, scaled by depth; `arg2` is the tint as three 4-bit
/// channels (red at bit 8, green at bit 4, blue at bit 0), with 8 added to each
/// on odd display frames so the glow flickers.
static void func_neo_ark_power_plant_2_8017DA54(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw31Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    block = SCRATCH_PUSH(RoomDraw31Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1          = ((s16)arg1 * 64) / block->otz;
        ang           = 0;
        blend         = ((u8)gDisplayState.animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw31Scratch);
}

/// Flash burst. State 0 derives a per-frame step of `0x100 / spawnArg1` for
/// the brightness and the radius; state 1 spends the spawn argument one frame
/// at a time, drawing a bright disc, a half-bright one at twice the radius and
/// a ring closing in from 0x300, then whites the screen out with a fade quad
/// and moves to state 2. State 2 draws a star-shaped afterglow at three times
/// the radius while dimming it by 0x10 a frame, and releases the work block
/// once it has faded; the block is also released once the room's event state
/// reaches 4.
void func_neo_ark_power_plant_2_8017DDF4(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1.value;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_neo_ark_power_plant_2_8017E4C4(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_power_plant_2_8017E4C4(coord, (s16)((u16)work->angle * 2), rgb);
                func_neo_ark_power_plant_2_8017E098(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_neo_ark_power_plant_2_8017F3C8(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags the projection, queues sixteen gouraud `POLY_G4` wedges that
/// form a ring between the radii `arg1` and `arg1 + arg2`, in world units
/// scaled by depth. The edge at `arg1` is black and the edge at `arg1 + arg2`
/// takes the colour `rgb`.
static void func_neo_ark_power_plant_2_8017E098(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                next;
    s32                outer;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = ((s16)arg1 * 64) / block->otz;
        block->rInner = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw02Scratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags the projection, queues eight gouraud `POLY_G4` wedges filling
/// a disc around the projected point, `rgb` at the centre and black at the rim.
/// `arg1` is the radius in world units, scaled by depth.
static void func_neo_ark_power_plant_2_8017E4C4(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Twin smoke trail. State 0 allocates sixteen `GfxCoord`s, eight per
/// trail, and seeds them all from the two spawn offsets so each trail starts
/// collapsed on its origin. State 1 advances one slot of each trail per frame,
/// re-derives all sixteen against the view and draws them. The task frees
/// itself once its age reaches the spawn argument, and idles while the room's
/// event state is 2 or more.
void func_neo_ark_power_plant_2_8017E858(Task* task)
{
    GfxCoord   coord;
    GfxCoord*  coords;
    GfxCoord*  objCoord;
    GfxCoord*  dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.coordBody->coord;

    if (Gp_State1C->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = memCalloc(sizeof(GfxCoord[16]), 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work             = coords;
                objCoord->parent       = work->parent;
                objCoord->coord.t[0]   = D_neo_ark_power_plant_2_80180680[0].vx;
                objCoord->coord.t[1]   = D_neo_ark_power_plant_2_80180680[0].vy;
                objCoord->coord.t[2]   = D_neo_ark_power_plant_2_80180680[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_neo_ark_power_plant_2_80180680[1];
                coord.coord.t[0]   = vec->vx;
                coord.coord.t[1]   = vec->vy;
                coord.coord.t[2]   = vec->vz;
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst         = &coords[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &coords[i + 8];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                coord.parent = work->parent;
                {
                    SVECTOR* edge    = &D_neo_ark_power_plant_2_80180680[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                dst         = &coords[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &coords[(work->age & 7) + 8];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &coords[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &coords[i + 8];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                func_neo_ark_power_plant_2_8017ED48(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `arg2`. Each quad spans `workm.t` of two
/// adjacent slots on `arg0` and `arg1`. The leading edge is scaled by
/// `0x40 - 9 * i` and the trailing edge by nine less. `arg3` is the beam
/// colour, three 2-bit channels at bits 8, 4 and 0 that each multiply that
/// fade. Dropped when `gte_stflg` is negative.
static void func_neo_ark_power_plant_2_8017ED48(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GfxCoord*          a;
    GfxCoord*          b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    blk = SCRATCH_PUSH(RoomDraw03Scratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw03Scratch);
}

/// Spark burst. State 0 fires the burst's effect, then a non-zero spawn
/// argument starts a stream of jittered sparks (state 1) and a zero one two
/// rings, a fixed one and one growing by 0x30 a frame, both dimming each
/// frame (state 2).
/// Either way the task reaches state 3 after seven frames and releases its
/// work block, or earlier once the room's event state reaches 4.
void func_neo_ark_power_plant_2_8017F140(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.coordBody->coord;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, ((Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_neo_ark_power_plant_2_8017E098(objCoord, 0x100, 0x100, rgb);
            func_neo_ark_power_plant_2_8017E098(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags the projection, queues a star-shaped glow of gouraud `POLY_G4`
/// wedges: sixteen around the circle, alternating full radius at half
/// intensity and half radius at full intensity, then four spikes a quarter
/// turn apart, two reaching the full radius and two twice it. `arg1` sizes it
/// in world units scaled by depth; every wedge fades to black at its rim.
static void func_neo_ark_power_plant_2_8017F3C8(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBillboardScratch);
}

/// Sets `field_4` of the third sprite command in the sixth record of the
/// current area's entry in the current stage's sprite table to the low byte of
/// `arg0`; values other than 0 and 1 leave it unchanged.
void func_neo_ark_power_plant_2_8017FD88(s32 arg0)
{
    GameLocationKey* sess = &gGameSession->location.loc;
    SpriteBatch*     batches;
    s32              mode;

    batches = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][5].field_4;
    mode    = arg0 & 0xFF;
    if (mode == 0) {
        batches[2].hidden = 0;
    } else if (mode == 1) {
        batches[2].hidden = 1;
    }
}
