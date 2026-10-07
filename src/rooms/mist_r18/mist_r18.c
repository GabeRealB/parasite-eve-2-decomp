#include "rooms/mist_r18.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/starter_inventory.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"
#include "../../shared/backdrop_crossfade.h"

extern WorldCoordRoomLights D_mist_r18_80186E44[1];

s32 func_map_akropolis_8017A038(void);
s32 func_map_akropolis_80179FC8(s32 arg0, s32 arg1);

static void _modelPlacementAttachPartTask(Task* childTask);
static void func_mist_r18_8017D960(Task* task);
static void func_mist_r18_8017DBB8(s32 shade, s32 arg1);
static void func_mist_r18_8017DD7C(Task* task);
static void func_mist_r18_8017E39C(Task* task);
static void func_mist_r18_8017E448(PrimDrawParams* sprite);
static void func_mist_r18_8017E534(PrimDrawParams* sprite, u32 clutX, s32 clutY);
static void func_mist_r18_8017E654(s16 abr, s16 x, s16 y, s32 otIdx);
static void func_mist_r18_8017E8B8(Task* task);
static void func_mist_r18_8017ECF4(Task* arg0);

/// The room's task-spawn table; its entries are started by index from the
/// room's callbacks.
extern TaskDesc D_mist_r18_80184F04[];
/// Spawn descriptor handed to entry 5 of `D_mist_r18_80184F04`.
extern TextStream D_mist_r18_80184EE4;
extern EvsCommand D_mist_r18_8018522C[];
extern EvsCommand D_mist_r18_8018576C[];
extern EvsCommand D_mist_r18_80185AE4[];
extern EvsCommand D_mist_r18_80185EBC[];
extern EvsCommand D_mist_r18_8018603C[];
extern EvsCommand D_mist_r18_801861BC[];
extern EvsCommand D_mist_r18_8018639C[];
extern EvsCommand D_mist_r18_8018645C[];
extern EvsCommand D_mist_r18_8018651C[];
extern EvsCommand D_mist_r18_80186564[];
/// The two prop tasks `func_mist_r18_8017E6D8` spawns and
/// `func_mist_r18_8017E784` tears down, by index.
extern Task* D_mist_r18_80186E90;
extern Task* D_mist_r18_80186E94;
/// Handle of the task `func_mist_r18_8017EA2C` spawns.
extern Task* D_mist_r18_80186E98;
/// Step of the cutscene sequence `func_mist_r18_8017D960` walks.
extern s32 D_mist_r18_80186E9C;
/// Set by `func_mist_r18_8017D960` when the alternate cutscene branch ran.
extern s32 D_mist_r18_80186EA0;

/// State handlers of the attached-model task `func_mist_r18_8017E2C8`
/// dispatches: attach to the parent's part, an empty idle state, then
/// `taskKill`.
static const TaskFuncTable3 D_mist_r18_8017D5C4 = {
    { _modelPlacementAttachPartTask, func_mist_r18_8017E39C, taskKill },
};

/// State handlers of the room's cutscene task `func_mist_r18_8017ED64`
/// dispatches: set-up, the cutscene step, then `taskKill`.
static const TaskFuncTable3 D_mist_r18_8017D5D0 = {
    { func_mist_r18_8017ECF4, func_mist_r18_8017D960, taskKill },
};

/// State handlers of the backdrop task `func_mist_r18_8017E854` dispatches:
/// blit the backdrop into the framebuffer, fade it in, fade it out, then
/// `taskKill`.
static const TaskFuncTable4 D_mist_r18_8017D5DC = {
    { func_mist_r18_8017DD7C, func_mist_r18_8017E8B8, crossfadeOutState, taskKill },
};

extern AreaResource D_mist_r18_80186BD8[3];

extern WorldCollisionGrid D_mist_r18_801866F8[1];

extern AnimationSet* D_mist_r18_80184F64[11];
void                 func_mist_r18_8017E6D8(s32);
void                 func_mist_r18_8017E784(s32);
void                 func_mist_r18_8017E7F0(void);
void                 func_mist_r18_8017E824(void);
void                 func_mist_r18_8017EA2C(void);
void                 func_mist_r18_8017EA60(void);
void                 func_mist_r18_8017EB48(void);
void                 func_mist_r18_8017EBB8(void);
void                 func_mist_r18_8017EBF8(void);
void                 func_mist_r18_8017EC38(void);
void                 func_mist_r18_8017EC58(void);
void                 func_mist_r18_8017EC78(void);
void                 func_mist_r18_8017ECC0(s8);
void                 func_mist_r18_8017ECCC(void);

static AnimationSet _gMistR18Animation02274;
static AnimationSet _gMistR18Animation030E8;
static AnimationSet _gMistR18Animation0375C;
static AnimationSet _gMistR18Animation03C90;
static AnimationSet _gMistR18Animation0478C;
static AnimationSet _gMistR18Animation04DD4;
static AnimationSet _gMistR18Animation05AA4;
static AnimationSet _gMistR18Animation07230;
static AnimationSet _gMistR18Animation075CC;
static AnimationSet _gMistR18Animation078C0;
static TmdSource    _gMistR18Actor213000Model072AC;
static TmdSource    _gMistR18Actor213000Prop;
void                func_mist_r18_8017D5EC(Task*);
void                func_mist_r18_8017DA8C(Task*);
void                func_mist_r18_8017E2C8(Task*);
void                func_mist_r18_8017E3A4(Task*);
void                func_mist_r18_8017E854(Task*);
void                func_mist_r18_8017EA98(Task*);
void                func_mist_r18_8017EC98(Task*);

static TmdBone _gMistR18Actor213000Model072ACSkeleton[1] = {
#include "assets/actor_213000_model_072AC_skeleton.inc"
};

static u32 _gMistR18Actor213000Model072ACPartVerts[1] = {
#include "assets/actor_213000_model_072AC_partVerts.inc"
};

static SVECTOR _gMistR18Actor213000Model072ACVerts[14] = {
#include "assets/actor_213000_model_072AC_verts.inc"
};

static SVECTOR _gMistR18Actor213000Model072ACNormals[17] = {
#include "assets/actor_213000_model_072AC_normals.inc"
};

static u32 _gMistR18Actor213000Model072ACStream[98] = {
#include "assets/actor_213000_model_072AC_stream.inc"
};

static TmdSource _gMistR18Actor213000Model072AC = {
    0,
    652,
    0,
    1,
    _gMistR18Actor213000Model072ACPartVerts,
    _gMistR18Actor213000Model072ACVerts,
    _gMistR18Actor213000Model072ACNormals,
    _gMistR18Actor213000Model072ACSkeleton,
    _gMistR18Actor213000Model072ACStream,
};

static TmdBone _gMistR18Actor213000PropSkeleton[1] = {
#include "assets/actor_213000_prop_skeleton.inc"
};

static u32 _gMistR18Actor213000PropPartVerts[1] = {
#include "assets/actor_213000_prop_partVerts.inc"
};

static SVECTOR _gMistR18Actor213000PropVerts[14] = {
#include "assets/actor_213000_prop_verts.inc"
};

static u32 _gMistR18Actor213000PropStream[79] = {
#include "assets/actor_213000_prop_stream.inc"
};

static TmdSource _gMistR18Actor213000Prop = {
    0,
    528,
    0,
    1,
    _gMistR18Actor213000PropPartVerts,
    _gMistR18Actor213000PropVerts,
    &_gMistR18Actor213000PropVerts[14],
    _gMistR18Actor213000PropSkeleton,
    _gMistR18Actor213000PropStream,
};

static AnimationPackedPose _gMistR18Animation02274Bank1[7] = {
#include "assets/mist_r18_animation_02274_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation02274Bank4[113] = {
#include "assets/mist_r18_animation_02274_bank4.inc"
};

static AnimationRecord _gMistR18Animation02274Records[221] = {
#include "assets/mist_r18_animation_02274_records.inc"
};

static u16 _gMistR18Animation02274Indices[20] = {
#include "assets/mist_r18_animation_02274_indices.inc"
};

static AnimationSet _gMistR18Animation02274 = {
    _gMistR18Animation02274Records,
    _gMistR18Animation02274Indices,
    { NULL, _gMistR18Animation02274Bank1, NULL, NULL, _gMistR18Animation02274Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation030E8Bank1[5] = {
#include "assets/mist_r18_animation_030E8_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation030E8Bank4[395] = {
#include "assets/mist_r18_animation_030E8_bank4.inc"
};

static AnimationRecord _gMistR18Animation030E8Records[495] = {
#include "assets/mist_r18_animation_030E8_records.inc"
};

static u16 _gMistR18Animation030E8Indices[20] = {
#include "assets/mist_r18_animation_030E8_indices.inc"
};

static AnimationSet _gMistR18Animation030E8 = {
    _gMistR18Animation030E8Records,
    _gMistR18Animation030E8Indices,
    { NULL, _gMistR18Animation030E8Bank1, NULL, NULL, _gMistR18Animation030E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation0375CBank1[7] = {
#include "assets/mist_r18_animation_0375C_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation0375CBank4[154] = {
#include "assets/mist_r18_animation_0375C_bank4.inc"
};

static AnimationRecord _gMistR18Animation0375CRecords[218] = {
#include "assets/mist_r18_animation_0375C_records.inc"
};

static u16 _gMistR18Animation0375CIndices[20] = {
#include "assets/mist_r18_animation_0375C_indices.inc"
};

static AnimationSet _gMistR18Animation0375C = {
    _gMistR18Animation0375CRecords,
    _gMistR18Animation0375CIndices,
    { NULL, _gMistR18Animation0375CBank1, NULL, NULL, _gMistR18Animation0375CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation03C90Bank1[2] = {
#include "assets/mist_r18_animation_03C90_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation03C90Bank4[95] = {
#include "assets/mist_r18_animation_03C90_bank4.inc"
};

static AnimationRecord _gMistR18Animation03C90Records[212] = {
#include "assets/mist_r18_animation_03C90_records.inc"
};

static u16 _gMistR18Animation03C90Indices[20] = {
#include "assets/mist_r18_animation_03C90_indices.inc"
};

static AnimationSet _gMistR18Animation03C90 = {
    _gMistR18Animation03C90Records,
    _gMistR18Animation03C90Indices,
    { NULL, _gMistR18Animation03C90Bank1, NULL, NULL, _gMistR18Animation03C90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation0478CBank1[16] = {
#include "assets/mist_r18_animation_0478C_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation0478CBank4[285] = {
#include "assets/mist_r18_animation_0478C_bank4.inc"
};

static AnimationRecord _gMistR18Animation0478CRecords[350] = {
#include "assets/mist_r18_animation_0478C_records.inc"
};

static u16 _gMistR18Animation0478CIndices[20] = {
#include "assets/mist_r18_animation_0478C_indices.inc"
};

static AnimationSet _gMistR18Animation0478C = {
    _gMistR18Animation0478CRecords,
    _gMistR18Animation0478CIndices,
    { NULL, _gMistR18Animation0478CBank1, NULL, NULL, _gMistR18Animation0478CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation04DD4Bank1[18] = {
#include "assets/mist_r18_animation_04DD4_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation04DD4Bank4[59] = {
#include "assets/mist_r18_animation_04DD4_bank4.inc"
};

static AnimationRecord _gMistR18Animation04DD4Records[269] = {
#include "assets/mist_r18_animation_04DD4_records.inc"
};

static u16 _gMistR18Animation04DD4Indices[20] = {
#include "assets/mist_r18_animation_04DD4_indices.inc"
};

static AnimationSet _gMistR18Animation04DD4 = {
    _gMistR18Animation04DD4Records,
    _gMistR18Animation04DD4Indices,
    { NULL, _gMistR18Animation04DD4Bank1, NULL, NULL, _gMistR18Animation04DD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation05AA4Bank1[18] = {
#include "assets/mist_r18_animation_05AA4_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation05AA4Bank4[342] = {
#include "assets/mist_r18_animation_05AA4_bank4.inc"
};

static AnimationRecord _gMistR18Animation05AA4Records[404] = {
#include "assets/mist_r18_animation_05AA4_records.inc"
};

static u16 _gMistR18Animation05AA4Indices[20] = {
#include "assets/mist_r18_animation_05AA4_indices.inc"
};

static AnimationSet _gMistR18Animation05AA4 = {
    _gMistR18Animation05AA4Records,
    _gMistR18Animation05AA4Indices,
    { NULL, _gMistR18Animation05AA4Bank1, NULL, NULL, _gMistR18Animation05AA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation07230Bank1[2] = {
#include "assets/mist_r18_animation_07230_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation07230Bank4[685] = {
#include "assets/mist_r18_animation_07230_bank4.inc"
};

static AnimationRecord _gMistR18Animation07230Records[796] = {
#include "assets/mist_r18_animation_07230_records.inc"
};

static u16 _gMistR18Animation07230Indices[20] = {
#include "assets/mist_r18_animation_07230_indices.inc"
};

static AnimationSet _gMistR18Animation07230 = {
    _gMistR18Animation07230Records,
    _gMistR18Animation07230Indices,
    { NULL, _gMistR18Animation07230Bank1, NULL, NULL, _gMistR18Animation07230Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation075CCBank1[3] = {
#include "assets/mist_r18_animation_075CC_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation075CCBank4[86] = {
#include "assets/mist_r18_animation_075CC_bank4.inc"
};

static AnimationRecord _gMistR18Animation075CCRecords[116] = {
#include "assets/mist_r18_animation_075CC_records.inc"
};

static u16 _gMistR18Animation075CCIndices[20] = {
#include "assets/mist_r18_animation_075CC_indices.inc"
};

static AnimationSet _gMistR18Animation075CC = {
    _gMistR18Animation075CCRecords,
    _gMistR18Animation075CCIndices,
    { NULL, _gMistR18Animation075CCBank1, NULL, NULL, _gMistR18Animation075CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation078C0Bank1[6] = {
#include "assets/mist_r18_animation_078C0_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation078C0Bank4[52] = {
#include "assets/mist_r18_animation_078C0_bank4.inc"
};

static AnimationRecord _gMistR18Animation078C0Records[99] = {
#include "assets/mist_r18_animation_078C0_records.inc"
};

static u16 _gMistR18Animation078C0Indices[20] = {
#include "assets/mist_r18_animation_078C0_indices.inc"
};

static AnimationSet _gMistR18Animation078C0 = {
    _gMistR18Animation078C0Records,
    _gMistR18Animation078C0Indices,
    { NULL, _gMistR18Animation078C0Bank1, NULL, NULL, _gMistR18Animation078C0Bank4, NULL, NULL, NULL },
};

u8 D_mist_r18_80184EA8[60] = {
    18,
    30,
    41,
    45,
    30,
    38,
    27,
    30,
    43,
    69,
    56,
    65,
    69,
    69,
    53,
    61,
    61,
    61,
    69,
    69,
    52,
    62,
    55,
    55,
    0,
    12,
    TEXT_STREAM_LINE_BREAK,
    12,
    66,
    8,
    66,
    18,
    66,
    19,
    66,
    69,
    2,
    30,
    39,
    45,
    30,
    43,
    65,
    69,
    69,
    11,
    40,
    44,
    69,
    0,
    39,
    32,
    30,
    37,
    30,
    44,
    TEXT_STREAM_END,
    0,
    0,
    0,
};

TextStream D_mist_r18_80184EE4 = { -150, -90, 704, 48, 16, 260, 1, 0, D_mist_r18_80184EA8, Caption_Glyphs, 13, 45, 216, 29 };

TaskDesc D_mist_r18_80184F04[8] = {
    { { { TASK_BODY_TMD, 192 } }, func_mist_r18_8017E2C8, { .model = &_gMistR18Actor213000Model072AC } },
    { { { TASK_BODY_TMD, 192 } }, func_mist_r18_8017E2C8, { .model = &_gMistR18Actor213000Prop } },
    { { { TASK_BODY_COORD, 192 } }, func_mist_r18_8017DA8C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_r18_8017E854, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, func_mist_r18_8017EA98, { .model = &_gMistR18Actor213000Prop } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_r18_8017D5EC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_r18_8017E3A4, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_r18_8017EC98, { .value = 0 } },
};

AnimationSet* D_mist_r18_80184F64[11] = {
    NULL,
    &_gMistR18Animation02274,
    &_gMistR18Animation030E8,
    &_gMistR18Animation0375C,
    &_gMistR18Animation03C90,
    &_gMistR18Animation0478C,
    &_gMistR18Animation04DD4,
    &_gMistR18Animation05AA4,
    &_gMistR18Animation07230,
    &_gMistR18Animation075CC,
    &_gMistR18Animation078C0,
};

AnimationBankCopyRequest D_mist_r18_80184F90 = { { .sets = D_mist_r18_80184F64 }, ARRAY_SIZE(D_mist_r18_80184F64) };

AnimationPlayRequest D_mist_r18_80184F98 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80184FAC = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80184FC0 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80184FD4 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80184FE8 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80184FFC = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185010 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185024 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185038 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_8018504C = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185060 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185074 = { { .sets = NULL }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185088 = { { .sets = NULL }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_8018509C = { { .sets = NULL }, 2, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801850B0 = { { .sets = NULL }, 3, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801850C4 = { { .sets = NULL }, 4, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801850D8 = { { .sets = NULL }, 5, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801850EC = { { .sets = NULL }, 6, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185100 = { { .sets = NULL }, 7, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185114 = { { .sets = NULL }, 8, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185128 = { { .sets = NULL }, 9, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_8018513C = { { .sets = NULL }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185150 = { { .sets = NULL }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185164 = { { .sets = NULL }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185178 = { { .sets = NULL }, 2, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_8018518C = { { .sets = NULL }, 3, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801851A0 = { { .sets = NULL }, 4, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801851B4 = { { .sets = NULL }, 5, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801851C8 = { { .sets = NULL }, 6, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801851DC = { { .sets = NULL }, 7, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801851F0 = { { .sets = NULL }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_mist_r18_80185204 = { { 0, 128, 0, 0 }, { 0, 0, 0, 0 } };

ActorCommand D_mist_r18_8018521C = { 0 };

ActorCommand D_mist_r18_80185220 = { { .loc = { 0, 0 } }, 1 };

ActorCommand D_mist_r18_80185224 = { { .loc = { 0, 0 } }, 2 };

ActorCommand D_mist_r18_80185228 = { { .loc = { 0, 0 } }, 3 };

EvsCommand D_mist_r18_8018522C[56] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EBB8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EBF8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EC38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EA2C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_r18_80184F90 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80184F98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017E7F0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017E824 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_r18_80185204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80184FAC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185088 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185164 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EC58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80184FC0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_8018509C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801850B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51120003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51120004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_r18_8018521C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_r18_80185220 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185074 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185150 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_8018576C[37] = {
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_r18_8017E6D8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_r18_80185204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_r18_80184F90 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80184FD4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185074 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185150 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80184FE8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801850C4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185178 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801850D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80184FFC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80185010 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801850EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_8018518C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_r18_80185224 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EA60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_r18_8017E784 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_r18_80185228 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185074 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185150 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_80185AE4[41] = {
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_r18_8017E6D8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_r18_80185204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_r18_80184F90 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80185024 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185074 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185150 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_mist_r18_8017ECC0 }, { .value = DISPLAY_DEPTH_SHIFT_4X }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_mist_r18_8017ECC0 }, { .value = DISPLAY_DEPTH_SHIFT_1X }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80185038 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185100 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801851A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801851B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185114 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801851C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_8018504C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185128 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801851DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_r18_8017E784 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185074 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185150 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_mist_r18_8017ECC0 }, { .value = DISPLAY_DEPTH_SHIFT_1X }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EB48 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_80185EBC[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_r18_80185204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_r18_80184F90 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80184F98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EA60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EA2C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_8018603C[16] = {
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_r18_80184F90 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_r18_80185204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80185060 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185074 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185150 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017ECCC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_801861BC[20] = {
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_r18_80184F90 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_r18_80185204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_r18_80184F90 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_r18_80185060 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_8018513C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801851F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017ECCC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_8018639C[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EC78 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_r18_80185220 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_8018645C[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EA60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_r18_8017E784 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_r18_80185228 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_8018651C[3] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_mist_r18_8017ECC0 }, { .value = DISPLAY_DEPTH_SHIFT_1X }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EB48 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_80186564[7] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EA60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_r18_8017EA2C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

WorldCollisionRoomResources D_mist_r18_8018660C[1] = {
    { D_mist_r18_801866F8, NULL, NULL, NULL },
};

u8* D_mist_r18_8018661C[1] = {
    gViewIdentityMap,
};

ViewCount D_mist_r18_80186620[2] = { 10, 0 };

WorldCoordRoomLighting D_mist_r18_80186624[1] = {
    { D_mist_r18_80186E44, NULL },
};

DirectionWarpEntry D_mist_r18_8018662C[1] = {
    { { { .word = 2048 }, -6000, 0, 3218 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -6000, 0, 3218 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gMistR18Collision09138Normals[1] = {
#include "assets/mist_r18_collision_09138_normals.inc"
};

static SVECTOR _gMistR18Collision09138Verts[4] = {
#include "assets/mist_r18_collision_09138_verts.inc"
};

static WorldCollisionGridFace _gMistR18Collision09138Faces[1] = {
#include "assets/mist_r18_collision_09138_faces.inc"
};

static s16 _gMistR18Collision09138Cells[24] = {
#include "assets/mist_r18_collision_09138_cells.inc"
};

#define GRID_CELL(i) (&_gMistR18Collision09138Cells[i])
static s16* _gMistR18Collision09138Table[12] = {
#include "assets/mist_r18_collision_09138_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mist_r18_801866F8[1] = {
    { NULL, _gMistR18Collision09138Normals, _gMistR18Collision09138Verts, _gMistR18Collision09138Faces, _gMistR18Collision09138Table, 7000, 5000, 4, 3, 4000, 1 },
};

ViewCamera D_mist_r18_8018671C[10] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x6978, -500 } }, 447 },
    { { { { 1821, 0, 3668 }, { 23, 4095, -11 }, { -3668, 25, 1821 } }, { -1440, 1500, -620 } }, 230 },
    { { { { 1698, 0, 3727 }, { 836, 3991, -381 }, { -3632, 919, 1655 } }, { 1290, 1500, -3770 } }, 257 },
    { { { { -1337, 0, 3871 }, { 8, 4095, 3 }, { -3871, 9, -1337 } }, { 1970, 1440, -4180 } }, 329 },
    { { { { -3828, 0, -1455 }, { 265, 4027, -699 }, { 1430, -748, -3764 } }, { 5550, 800, -5320 } }, 680 },
    { { { { 1376, 0, 3857 }, { 1511, 3768, -539 }, { -3549, 1605, 1266 } }, { 4520, 1670, -2880 } }, 257 },
    { { { { 2471, 0, -3266 }, { 87, 4094, 66 }, { 3264, -110, 2470 } }, { 5490, 1190, -3000 } }, 257 },
    { { { { 2471, 0, -3266 }, { 87, 4094, 66 }, { 3264, -110, 2470 } }, { 5490, 1190, -3000 } }, 257 },
    { { { { 2471, 0, -3266 }, { 87, 4094, 66 }, { 3264, -110, 2470 } }, { 5490, 1190, -3000 } }, 257 },
    { { { { 2471, 0, -3266 }, { 87, 4094, 66 }, { 3264, -110, 2470 } }, { 5490, 1190, -3000 } }, 257 },
};

SpriteBatch D_mist_r18_80186884[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_r18_80186894[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_r18_801868B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_r18_801868C4[10] = {
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -56, 80, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, 8, 80, 750, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 72, 80, 750, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -56, 56, 825, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 56, 825, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 56, 825, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 8, 56, 825, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 56, 825, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 56, 825, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 64, 825, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_r18_8018698C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_r18_801869A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_r18_801869B4[17] = {
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 88, 288, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -96, 72, 323, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -64, 80, 298, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -64, 64, 352, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -24, 72, 310, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -24, 56, 350, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 16, 64, 318, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 56, 64, 283, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, 64, 318, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 16, 48, 350, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 40, 362, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 64, 40, 362, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 88, 32, 362, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, 40, 375, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 48, 375, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -152, 88, 307, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -136, 80, 302, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_r18_80186B08[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_r18_80186B20[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_r18_80186B30[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_r18_80186B40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_r18_80186B50[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_mist_r18_80186B60[10] = {
    { { .empty = D_mist_r18_80186884 }, D_mist_r18_80186884, NULL },
    { { .empty = D_mist_r18_80186894 }, D_mist_r18_80186894, NULL },
    { { .empty = D_mist_r18_801868B4 }, D_mist_r18_801868B4, NULL },
    { { .elements = D_mist_r18_801868C4 }, D_mist_r18_8018698C, NULL },
    { { .empty = D_mist_r18_801869A4 }, D_mist_r18_801869A4, NULL },
    { { .elements = D_mist_r18_801869B4 }, D_mist_r18_80186B08, NULL },
    { { .empty = D_mist_r18_80186B20 }, D_mist_r18_80186B20, NULL },
    { { .empty = D_mist_r18_80186B30 }, D_mist_r18_80186B30, NULL },
    { { .empty = D_mist_r18_80186B40 }, D_mist_r18_80186B40, NULL },
    { { .empty = D_mist_r18_80186B50 }, D_mist_r18_80186B50, NULL },
};

AreaResource D_mist_r18_80186BD8[3] = {
    { 144, 130, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_113000_8013ABB4 },
    { 105, 130, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_213000_80157DE0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_mist_r18_80186BFC[13] = {
    { NULL, NULL },
    { D_map_akropolis_8017BD8C, D_mist_r18_80186BD8 },
    { NULL, NULL },
    { D_map_akropolis_8017BD8C, D_mist_r18_80186BD8 },
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

WorldCoordPointLight D_mist_r18_80186C64[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5700, -2300, 2050 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5700, -2300, 4800 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3800, -2300, 4800 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3800, -2300, 2050 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5450, -1200, 4000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 800, 1200 },
};

WorldCoordRoomLights D_mist_r18_80186E44[1] = {
    { 0, NULL, ARRAY_SIZE(D_mist_r18_80186C64), D_mist_r18_80186C64, 0, NULL },
};

WorldCollisionFootstepSounds D_mist_r18_80186E5C = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionSurfaceProperties D_mist_r18_80186E68[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mist_r18_80186E5C },
};

WorldCollisionSurfaceProperties* D_mist_r18_80186E70[8] = {
    D_mist_r18_80186E68,
    D_mist_r18_80186E68,
    D_mist_r18_80186E68,
    D_mist_r18_80186E68,
    D_mist_r18_80186E68,
    D_mist_r18_80186E68,
    D_mist_r18_80186E68,
    D_mist_r18_80186E68,
};

Task* D_mist_r18_80186E90;

Task* D_mist_r18_80186E94;

Task* D_mist_r18_80186E98;

s32 D_mist_r18_80186E9C;

s32 D_mist_r18_80186EA0;

/// Typewriter text task for the room's message box: state 0 measures the
/// script (or, for a negative per-glyph delay, reveals all of it at once) and
/// arms the countdown, state 1 draws the revealed glyphs each frame and
/// advances one glyph whenever the countdown runs out. Any other state, or a
/// session that has left the message, kills the task.
void func_mist_r18_8017D5EC(Task* task)
{
    PrimDrawParams sprite;
    TextStream*    spawn;
    s32            i;

    spawn = task->spawnArg2.pointer;
    if (gGameSession->eventState == 0) {
        task->state = -1;
    }

    switch (task->state) {
        case 0:
            spawn->cursor = 0;
            if (spawn->charDelay < 0) {
                i = 0;
                if (spawn->chars[0] != TEXT_STREAM_END) {
                    do {
                        i++;
                        spawn->cursor++;
                    } while (spawn->chars[i] != TEXT_STREAM_END);
                }
                task->killCountdown = spawn->delayReload;
            } else {
                task->killCountdown = spawn->charDelay;
            }
            task->state++;
            return;

        case 1:
            sprite.x         = spawn->x;
            sprite.y         = spawn->y;
            sprite.u         = spawn->tpageX;
            sprite.v         = spawn->tpageY;
            sprite.r         = 0x80;
            sprite.g         = 0x80;
            sprite.b         = 0x80;
            sprite.semiTrans = 0;
            sprite.unused_12 = ONE;

            if (spawn->chars[spawn->cursor - 1] != TEXT_STREAM_END) {
                for (i = 0; i < spawn->cursor; i++) {
                    if (spawn->chars[i] == TEXT_STREAM_LINE_BREAK) {
                        sprite.x  = spawn->x;
                        sprite.y += spawn->lineHeight;
                    } else {
                        sprite.u = spawn->glyphs[spawn->chars[i]].u + (spawn->tpageX & 0x3F);
                        sprite.v = spawn->glyphs[spawn->chars[i]].v + (u8)spawn->tpageY;
                        sprite.w = spawn->glyphs[spawn->chars[i]].width;
                        sprite.h = spawn->glyphs[spawn->chars[i]].height;
                        if (sprite.h != 0) {
                            func_mist_r18_8017E534(&sprite, spawn->clutX, spawn->clutY);
                        }
                        sprite.x += spawn->glyphs[spawn->chars[i]].width;
                    }
                }

                func_mist_r18_8017E654(1, spawn->tpageX, spawn->tpageY, 4);

                if (--task->killCountdown < 0) {
                    spawn->cursor++;
                    if (spawn->chars[spawn->cursor] == TEXT_STREAM_END) {
                        task->killCountdown = spawn->delayReload;
                    } else {
                        task->killCountdown = spawn->charDelay;
                    }
                }

                // The plate sits three pixels above and left of the pen.
                sprite.x         = spawn->x - 3;
                sprite.y         = spawn->y - 3;
                sprite.w         = spawn->boxWidth;
                sprite.h         = spawn->boxHeight;
                sprite.b         = 0;
                sprite.g         = 0;
                sprite.r         = 0;
                sprite.semiTrans = 1;
                func_mist_r18_8017E448(&sprite);
                func_mist_r18_8017E654(0, 0, 0, 5);
                return;
            }
            task->state++;
            return;

        default:
            break;
    }
    taskKill(task);
}

/// Cutscene step of the room's cutscene task: while no event is running and
/// neither gate is set, start the next script of the sequence whose step
/// `D_mist_r18_80186E9C` holds. At step 4 it branches on `func_8017A038`,
/// setting `D_mist_r18_80186EA0` and staying on that step when the alternate
/// script runs.
static void func_mist_r18_8017D960(Task* task)
{
    s32 state;

    if ((gGameSession->eventState == 0) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
        state = D_mist_r18_80186E9C;
        if (state == 1) {
            func_800E8634(D_mist_r18_80185EBC, 0, D_mist_r18_80186564);
            D_mist_r18_80186E9C = 2;
        } else if (state == 2) {
            func_800E8634(D_mist_r18_8018576C, 0, D_mist_r18_8018645C);
            D_mist_r18_80186EA0 = 0;
            D_mist_r18_80186E9C = 3;
        } else if (state == 3) {
            func_800E8614(D_mist_r18_8018603C, 0);
            D_mist_r18_80186E9C = 4;
        } else if (state == 4) {
            if (func_map_akropolis_8017A038() != 1) {
                func_800E8614(D_mist_r18_801861BC, 0);
                D_mist_r18_80186EA0 = 1;
                return;
            }
            func_800E8634(D_mist_r18_80185AE4, 0, D_mist_r18_8018651C);
            D_mist_r18_80186E9C = 5;
        }
    }
}

/// Fade task for the room's backdrop tint: state 0 arms the fade, states 1/3
/// ramp `killCountdown` up to 0x80 and back down to 0, state 2 holds until the
/// hold counter runs out (or the session's skip gate is set). Every state but
/// the last redraws through `func_mist_r18_8017DBB8`.
void func_mist_r18_8017DA8C(Task* task)
{
    s32 shade;

    shade = 1;
    switch (task->state) {
        case 0:
            task->killCountdown = 0;
            func_mist_r18_8017DBB8(1, 0);
            task->state++;
            break;
        case 1:
            task->killCountdown += 0x15;
            if (task->killCountdown >= 0x81) {
                shade = 0;
                task->state++;
            }
            break;
        case 2:
            shade = 0;
            task->spawnArg1.value--;
            if ((task->spawnArg1.value <= 0) || (gGameSession->evtSkipped != 0)) {
                task->state++;
            }
            break;
        case 3:
            task->killCountdown -= 0x15;
            if (task->killCountdown < 0x16) {
                task->state++;
            }
            break;
        default:
            taskKill(task);
            return;
    }
    func_mist_r18_8017DBB8(shade, task->killCountdown);
}

/// Draw the room's two backdrop tint sprites (upper-left and lower-right
/// halves of the mist overlay) plus the trailing tpage packet. `shade` picks
/// the sprite code - shade-texture (0x65) while the fade is ramping in,
/// semi-transparent (0x66) otherwise - and `arg1` is the grey level written
/// into all three colour channels.
static void func_mist_r18_8017DBB8(s32 shade, s32 arg1)
{
    SPRT*     sprt;
    DR_TPAGE* tp;
    s16       x;
    s16       y;

    x              = -0x96;
    y              = -0x5A;
    sprt           = gGpuPrimCursor;
    gGpuPrimCursor = sprt + 1;
    setSprt(sprt);
    if (shade == 0) {
        sprt->code = 0x65;
    } else {
        sprt->code = 0x66;
    }
    setXY0(sprt, x, y);
    sprt->clut = 0x43C0;
    setWH(sprt, 0xCF, 0x23);
    setRGB0(sprt, arg1, arg1, arg1);
    setUV0(sprt, 0, 0);
    addPrim(gGpuCurrentOt + 4, sprt);

    x              = -0x22;
    y              = 0x36;
    sprt           = gGpuPrimCursor;
    gGpuPrimCursor = sprt + 1;
    setSprt(sprt);
    if (shade == 0) {
        sprt->code = 0x65;
    } else {
        sprt->code = 0x66;
    }
    setXY0(sprt, x, y);
    setRGB0(sprt, arg1, arg1, arg1);
    setUV0(sprt, 0, 0x24);
    sprt->clut = 0x43C1;
    setWH(sprt, 0xB7, 0x23);
    addPrim(gGpuCurrentOt + 4, sprt);

    tp             = gGpuPrimCursor;
    gGpuPrimCursor = tp + 1;
    setDrawTPage(tp, 1, 0, 0x2B);
    addPrim(gGpuCurrentOt + 4, tp);
}

/// Blit the room's backdrop out of the off-screen VRAM staging area into the
/// two framebuffer halves, bracketing both `MoveImage`s with STP writes so the
/// copied pixels keep their mask bit. The source row depends on which display
/// buffer is live, then the task advances a state.
static void func_mist_r18_8017DD7C(Task* task)
{
    RECT     rect;
    DR_STP*  stp;
    DR_MOVE* mv;
    s16      x;
    s16      y;

    if (gDisplayState.drawBuffer == 0) {
        x = 0;
        y = 0;
    } else {
        x = 0;
        y = 0x110;
    }

    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(gGpuCurrentOt + 8, stp);

    mv             = gGpuPrimCursor;
    gGpuPrimCursor = mv + 1;
    rect.x         = x;
    rect.y         = y;
    rect.w         = 0xC0;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x340, 0);
    addPrim(gGpuCurrentOt + 8, mv);

    mv             = gGpuPrimCursor;
    gGpuPrimCursor = mv + 1;
    rect.x         = x + 0xC0;
    rect.y         = y;
    rect.w         = 0x80;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x280, 0x100);
    addPrim(gGpuCurrentOt + 8, mv);

    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(gGpuCurrentOt + 8, stp);

    task->killCountdown = 0;
    task->state++;
}

#include "../../shared/backdrop_crossfade_live.inc.c"

/// Redraw the room's two backdrop halves as semi-transparent `SPRT`s in OT
/// slot 8, tinting both with `shade`, then append each half's tpage.
void crossfadeDrawBackdrop(s32 shade)
{
    SPRT* p;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    crossfadeSetTpage(0x340, 0);

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = 0x20;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    crossfadeSetTpage(0x280, 0x100);
}

/// Per-frame entry point of the attached-model task: run the handler its state
/// selects from `D_mist_r18_8017D5C4` (attach to the parent's part, an empty
/// idle state, then `taskKill`), copied onto the stack each frame.
void func_mist_r18_8017E2C8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_r18_8017D5C4;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Idle state of the attached-model task: nothing to do until it is killed.
static void func_mist_r18_8017E39C(Task* task)
{
}

/// Redraw the room's sprite rectangle each frame until the spawn countdown in
/// `Task::spawnArg1` runs out, then kill the task.
///
/// `Task::spawnArg2` points at the `RECT` to cover, in draw-environment
/// pixels; the task borrows it and reads it again every frame.
void func_mist_r18_8017E3A4(Task* task)
{
    PrimDrawParams sprite;
    RECT*          rect;

    rect = task->spawnArg2.pointer;

    if (task->state == 0) {
        sprite.x         = rect->x;
        sprite.y         = rect->y;
        sprite.w         = rect->w;
        sprite.h         = rect->h;
        sprite.b         = 0;
        sprite.g         = 0;
        sprite.r         = 0;
        sprite.semiTrans = 1;
        func_mist_r18_8017E448(&sprite);
        func_mist_r18_8017E654(0, 0, 0, 5);

        if (--task->spawnArg1.value > 0) {
            return;
        }
    }
    taskKill(task);
}

/// Emit the sprite's screen rectangle as a flat-shaded `TILE` into OT slot 5.
static void func_mist_r18_8017E448(PrimDrawParams* sprite)
{
    TILE* tile;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    if (sprite->semiTrans == 0) {
        SetShadeTex(tile, 1);
        SetSemiTrans(tile, 0);
    } else {
        SetShadeTex(tile, 0);
        SetSemiTrans(tile, 1);
    }
    tile->r0 = sprite->r;
    tile->g0 = sprite->g;
    tile->b0 = sprite->b;
    tile->x0 = sprite->x;
    tile->y0 = sprite->y;
    tile->w  = sprite->w - 1;
    tile->h  = sprite->h - 1;
    AddPrim(gGpuCurrentOt + 5, tile);
}

/// Emit the sprite's screen rectangle as a textured `SPRT` into OT slot 4,
/// with the CLUT taken from the framebuffer position `clutX`/`clutY`.
static void func_mist_r18_8017E534(PrimDrawParams* sprite, u32 clutX, s32 clutY)
{
    SPRT* p;
    u8    v;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    SetSprt(p);
    if (sprite->semiTrans == 0) {
        SetShadeTex(p, 1);
        SetSemiTrans(p, 0);
    } else {
        SetShadeTex(p, 0);
        SetSemiTrans(p, 1);
    }
    p->r0   = sprite->r;
    p->g0   = sprite->g;
    p->b0   = sprite->b;
    p->x0   = sprite->x;
    p->y0   = sprite->y;
    p->u0   = sprite->u;
    v       = sprite->v;
    p->clut = getClut(clutX, clutY);
    p->v0   = v;
    p->w    = sprite->w - 1;
    p->h    = sprite->h - 1;
    AddPrim(gGpuCurrentOt + 4, p);
}

/// Append a `DR_TPAGE` for the given tpage to OT slot `otIdx`.
static void func_mist_r18_8017E654(s16 abr, s16 x, s16 y, s32 otIdx)
{
    DR_TPAGE* dr;

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    SetDrawTPage(dr, 1, 0, GetTPage(0, abr, x, y));
    AddPrim(gGpuCurrentOt + otIdx, dr);
}

/// Spawn prop task `idx` (0 or 1) into its slot if it is not already running,
/// and clear bit 7 of its model's flags. Other indices do nothing.
void func_mist_r18_8017E6D8(s32 idx)
{
    Task** slot;
    Task*  task;

    switch (idx) {
        case 0:
            slot = &D_mist_r18_80186E90;
            break;
        case 1:
            slot = &D_mist_r18_80186E94;
            break;
        default:
            slot = NULL;
            break;
    }

    if ((slot != NULL) && (*slot == NULL)) {
        task  = taskSpawnFromTable(D_mist_r18_80184F04, idx, 8, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
        *slot = task;
        if (task != NULL) {
            task->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

/// Kill and clear prop task `idx` (0 or 1); other indices do nothing.
void func_mist_r18_8017E784(s32 idx)
{
    if (idx == 0) {
        if (D_mist_r18_80186E90 != NULL) {
            taskKill(D_mist_r18_80186E90);
        }
        D_mist_r18_80186E90 = NULL;
    } else if (idx == 1) {
        if (D_mist_r18_80186E94 != NULL) {
            taskKill(D_mist_r18_80186E94);
        }
        D_mist_r18_80186E94 = NULL;
    }
}

void func_mist_r18_8017E7F0(void)
{
    taskSpawnFromTable(D_mist_r18_80184F04, 5, 0, &D_mist_r18_80184EE4);
}

/// Spawn entry 3 of the room's task table.
void func_mist_r18_8017E824(void)
{
    taskSpawnFromTable(D_mist_r18_80184F04, 3, 0, 0);
}

/// Per-frame entry point of the backdrop task: run the handler its state
/// selects from `D_mist_r18_8017D5DC`, copied onto the stack each frame.
void func_mist_r18_8017E854(Task* task)
{
    TaskFuncTable4 states;

    states = D_mist_r18_8017D5DC;
    states.funcs[task->state](task);
}

/// Fade the room in. `Task::killCountdown` is reused as the 0..0x80 fade level.
static void func_mist_r18_8017E8B8(Task* task)
{
    u16 fade;

    fade                = (u16)task->killCountdown + 8;
    task->killCountdown = fade;
    if ((s16)fade >= 0x40) {
        task->killCountdown = 0x40;
    }
    if ((gGameSession->viewReady != 0) || (gGameSession->location.loc.view != 2)) {
        task->killCountdown = 0x80;
        task->state++;
    }
}

#include "../../shared/backdrop_crossfade_out.inc.c"

#include "../../shared/backdrop_crossfade_tpage.inc.c"

/// Spawn entry 4 of the room's task table and keep its handle in
/// `D_mist_r18_80186E98`, which `func_mist_r18_8017EA60` kills.
void func_mist_r18_8017EA2C(void)
{
    D_mist_r18_80186E98 = taskSpawnFromTable(D_mist_r18_80184F04, 4, 0, 0);
}

void func_mist_r18_8017EA60(void)
{
    if (D_mist_r18_80186E98 != NULL) {
        taskKill(D_mist_r18_80186E98);
    }
    D_mist_r18_80186E98 = NULL;
}

void func_mist_r18_8017EA98(Task* task)
{
    GfxCoord*  coord;
    TmdObject* obj;

    if (task->state == 0) {
        coord               = task->extra.tmd->coords;
        coord->coord.t[0]   = -0x1496;
        coord->coord.t[1]   = -0x2DA;
        coord->coord.t[2]   = 0xB90;
        coord->param.rot.vx = 0x6AA;
        coord->param.rot.vy = -0xF8E;
        coord->param.rot.vz = -0x333;
        RotMatrixZYX(&coord->param.rot, &coord->coord);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        obj                 = task->extra.tmd;
        obj->otOffset       = -8;
        obj->flags         &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        task->state++;
    }
}

void func_mist_r18_8017EB48(void)
{
    Gp_InitStarterInv();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_MIST_PARKING;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 3;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 3;
    gDisplayState.spriteVariant                                 = 1;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(0, 0x11, 0, 0);
}

void func_mist_r18_8017EBB8(void)
{
    gGameSession->viewDirty = 1;
    CdCmd_StartOverlay(1U, 0x1EU, 0xBU);
    cdCmdStageSceneAudioStart();
}

void func_mist_r18_8017EBF8(void)
{
    if (taskSpawnFromTable(D_mist_r18_80184F04, 7, 0, 0) != NULL) {
        D_801156F9 = 1;
    }
}

void func_mist_r18_8017EC38(void)
{
    cdCmdEnqueueScenePlayback();
}

void func_mist_r18_8017EC58(void)
{
    Gp_RestoreStreamRng();
}

/// Clear the queued CD command and restart the CD queue.
void func_mist_r18_8017EC78(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_mist_r18_8017EC98(Task* task)
{
    if (gGameSession->viewReady != 1) {
        D_801156F9 = 0;
    }
}

void func_mist_r18_8017ECC0(s8 arg0)
{
    gDisplayState.otDepthShift = arg0;
}

void func_mist_r18_8017ECCC(void)
{
    func_map_akropolis_80179FC8(0, D_mist_r18_80186EA0);
}

static void func_mist_r18_8017ECF4(Task* arg0)
{
    D_mist_r18_80186E90 = 0;
    D_mist_r18_80186E94 = 0;
    D_mist_r18_80186E98 = 0;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    func_800E8634(D_mist_r18_8018522C, 0, D_mist_r18_8018639C);
    arg0->state         = (s32)(arg0->state + 1);
    D_mist_r18_80186E9C = 1;
}

/// Per-frame entry point of the room's cutscene task: run the handler its state
/// selects from `D_mist_r18_8017D5D0` (set-up, the cutscene step
/// `func_mist_r18_8017D960`, then `taskKill`), copied onto the stack each
/// frame.
void func_mist_r18_8017ED64(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_r18_8017D5D0;
    sp.funcs[task->state](task);
}
