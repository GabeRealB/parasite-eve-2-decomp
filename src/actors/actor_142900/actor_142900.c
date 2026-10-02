#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/shelter_b2_elevator.h"

// The fixed 32-word animation copy reaches beyond the pointer bank.
// Typed fields and the copy view cover the complete retained allocation.
typedef union {
    struct {
        AnimationSet*        sets[13];
        AnimationPlayRequest arguments[14];
    } data;
    s32 words[83];
} Actor142900AnimStorage7618;
STATIC_ASSERT_SIZEOF(Actor142900AnimStorage7618, 332);
extern Actor142900AnimStorage7618 D_actor_142900_80137618;

// The fixed 32-word animation copy reaches beyond the pointer bank.
// Typed fields and the copy view cover the complete retained allocation.
typedef union {
    struct {
        AnimationSet*        sets[10];
        AnimationPlayRequest arguments[13];
    } data;
    s32 words[75];
} Actor142900AnimStorage7764;
STATIC_ASSERT_SIZEOF(Actor142900AnimStorage7764, 300);
extern Actor142900AnimStorage7764 D_actor_142900_80137764;

static AnimationSet _gActor142900Animation005C8;
static AnimationSet _gActor142900Animation0089C;
static AnimationSet _gActor142900Animation00B88;
static AnimationSet _gActor142900Animation010EC;
static AnimationSet _gActor142900Animation01370;
static AnimationSet _gActor142900Animation02774;
static AnimationSet _gActor142900Animation02DAC;
static AnimationSet _gActor142900Animation03110;
static AnimationSet _gActor142900Animation035C8;
static AnimationSet _gActor142900Animation03910;
static AnimationSet _gActor142900Animation03F8C;
static AnimationSet _gActor142900Animation04260;
static AnimationSet _gActor142900Animation04480;

// Keep the stored callback signature alongside the scheduler's task view.
typedef union {
    TaskDesc tasks[2];
    struct {
        u16 flags;
        u16 priority;
        union {
            TaskFunc task;
            void     (*enemyCleanup)(Enemy*, Task*);
        } callback;
        TaskSpawnArg arg;
    } native[2];
} Actor142900TaskTable;
STATIC_ASSERT_SIZEOF(Actor142900TaskTable, 0x18);

extern Actor142900TaskTable D_actor_142900_80137600;
extern s32                  D_actor_142900_801382A8;
extern s32                  D_actor_142900_801382AC;

extern ActorTransform D_actor_142900_801378A0;
void                  func_actor_142900_80131F5C(void);
void                  func_actor_142900_80131FDC(s32);

void func_actor_142900_80131F5C(void);
void func_actor_142900_80131FDC(s32);

static AnimationSet _gActor142900Animation0161C;
static AnimationSet _gActor142900Animation018BC;
static AnimationSet _gActor142900Animation01A94;
static AnimationSet _gActor142900Animation02200;
static AnimationSet _gActor142900Animation04804;
static AnimationSet _gActor142900Animation04B30;
static AnimationSet _gActor142900Animation04F7C;
static AnimationSet _gActor142900Animation052A4;
static AnimationSet _gActor142900Animation05564;
static AnimationSet _gActor142900Animation057B8;

void func_actor_142900_80131E24(Task*);

static AnimationPackedPose _gActor142900Animation005C8Bank1[7] = {
#include "assets/actor_142900_animation_005C8_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation005C8Bank4[71] = {
#include "assets/actor_142900_animation_005C8_bank4.inc"
};

static AnimationRecord _gActor142900Animation005C8Records[139] = {
#include "assets/actor_142900_animation_005C8_records.inc"
};

static u16 _gActor142900Animation005C8Indices[20] = {
#include "assets/actor_142900_animation_005C8_indices.inc"
};

static AnimationSet _gActor142900Animation005C8 = {
    _gActor142900Animation005C8Records,
    _gActor142900Animation005C8Indices,
    { NULL, _gActor142900Animation005C8Bank1, NULL, NULL, _gActor142900Animation005C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation0089CBank1[4] = {
#include "assets/actor_142900_animation_0089C_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation0089CBank4[54] = {
#include "assets/actor_142900_animation_0089C_bank4.inc"
};

static AnimationRecord _gActor142900Animation0089CRecords[95] = {
#include "assets/actor_142900_animation_0089C_records.inc"
};

static u16 _gActor142900Animation0089CIndices[20] = {
#include "assets/actor_142900_animation_0089C_indices.inc"
};

static AnimationSet _gActor142900Animation0089C = {
    _gActor142900Animation0089CRecords,
    _gActor142900Animation0089CIndices,
    { NULL, _gActor142900Animation0089CBank1, NULL, NULL, _gActor142900Animation0089CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation00B88Bank1[4] = {
#include "assets/actor_142900_animation_00B88_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation00B88Bank4[60] = {
#include "assets/actor_142900_animation_00B88_bank4.inc"
};

static AnimationRecord _gActor142900Animation00B88Records[95] = {
#include "assets/actor_142900_animation_00B88_records.inc"
};

static u16 _gActor142900Animation00B88Indices[20] = {
#include "assets/actor_142900_animation_00B88_indices.inc"
};

static AnimationSet _gActor142900Animation00B88 = {
    _gActor142900Animation00B88Records,
    _gActor142900Animation00B88Indices,
    { NULL, _gActor142900Animation00B88Bank1, NULL, NULL, _gActor142900Animation00B88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation010ECBank1[8] = {
#include "assets/actor_142900_animation_010EC_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation010ECBank4[111] = {
#include "assets/actor_142900_animation_010EC_bank4.inc"
};

static AnimationRecord _gActor142900Animation010ECRecords[190] = {
#include "assets/actor_142900_animation_010EC_records.inc"
};

static u16 _gActor142900Animation010ECIndices[20] = {
#include "assets/actor_142900_animation_010EC_indices.inc"
};

static AnimationSet _gActor142900Animation010EC = {
    _gActor142900Animation010ECRecords,
    _gActor142900Animation010ECIndices,
    { NULL, _gActor142900Animation010ECBank1, NULL, NULL, _gActor142900Animation010ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation01370Bank1[4] = {
#include "assets/actor_142900_animation_01370_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation01370Bank4[50] = {
#include "assets/actor_142900_animation_01370_bank4.inc"
};

static AnimationRecord _gActor142900Animation01370Records[79] = {
#include "assets/actor_142900_animation_01370_records.inc"
};

static u16 _gActor142900Animation01370Indices[20] = {
#include "assets/actor_142900_animation_01370_indices.inc"
};

static AnimationSet _gActor142900Animation01370 = {
    _gActor142900Animation01370Records,
    _gActor142900Animation01370Indices,
    { NULL, _gActor142900Animation01370Bank1, NULL, NULL, _gActor142900Animation01370Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation0161CBank1[4] = {
#include "assets/actor_142900_animation_0161C_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation0161CBank4[52] = {
#include "assets/actor_142900_animation_0161C_bank4.inc"
};

static AnimationRecord _gActor142900Animation0161CRecords[87] = {
#include "assets/actor_142900_animation_0161C_records.inc"
};

static u16 _gActor142900Animation0161CIndices[20] = {
#include "assets/actor_142900_animation_0161C_indices.inc"
};

static AnimationSet _gActor142900Animation0161C = {
    _gActor142900Animation0161CRecords,
    _gActor142900Animation0161CIndices,
    { NULL, _gActor142900Animation0161CBank1, NULL, NULL, _gActor142900Animation0161CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation018BCBank1[4] = {
#include "assets/actor_142900_animation_018BC_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation018BCBank4[50] = {
#include "assets/actor_142900_animation_018BC_bank4.inc"
};

static AnimationRecord _gActor142900Animation018BCRecords[86] = {
#include "assets/actor_142900_animation_018BC_records.inc"
};

static u16 _gActor142900Animation018BCIndices[20] = {
#include "assets/actor_142900_animation_018BC_indices.inc"
};

static AnimationSet _gActor142900Animation018BC = {
    _gActor142900Animation018BCRecords,
    _gActor142900Animation018BCIndices,
    { NULL, _gActor142900Animation018BCBank1, NULL, NULL, _gActor142900Animation018BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation01A94Bank1[2] = {
#include "assets/actor_142900_animation_01A94_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation01A94Bank4[16] = {
#include "assets/actor_142900_animation_01A94_bank4.inc"
};

static AnimationRecord _gActor142900Animation01A94Records[76] = {
#include "assets/actor_142900_animation_01A94_records.inc"
};

static u16 _gActor142900Animation01A94Indices[20] = {
#include "assets/actor_142900_animation_01A94_indices.inc"
};

static AnimationSet _gActor142900Animation01A94 = {
    _gActor142900Animation01A94Records,
    _gActor142900Animation01A94Indices,
    { NULL, _gActor142900Animation01A94Bank1, NULL, NULL, _gActor142900Animation01A94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation02200Bank1[12] = {
#include "assets/actor_142900_animation_02200_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation02200Bank4[188] = {
#include "assets/actor_142900_animation_02200_bank4.inc"
};

static AnimationRecord _gActor142900Animation02200Records[231] = {
#include "assets/actor_142900_animation_02200_records.inc"
};

static u16 _gActor142900Animation02200Indices[20] = {
#include "assets/actor_142900_animation_02200_indices.inc"
};

static AnimationSet _gActor142900Animation02200 = {
    _gActor142900Animation02200Records,
    _gActor142900Animation02200Indices,
    { NULL, _gActor142900Animation02200Bank1, NULL, NULL, _gActor142900Animation02200Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation02774Bank1[2] = {
#include "assets/actor_142900_animation_02774_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation02774Bank4[135] = {
#include "assets/actor_142900_animation_02774_bank4.inc"
};

static AnimationRecord _gActor142900Animation02774Records[188] = {
#include "assets/actor_142900_animation_02774_records.inc"
};

static u16 _gActor142900Animation02774Indices[20] = {
#include "assets/actor_142900_animation_02774_indices.inc"
};

static AnimationSet _gActor142900Animation02774 = {
    _gActor142900Animation02774Records,
    _gActor142900Animation02774Indices,
    { NULL, _gActor142900Animation02774Bank1, NULL, NULL, _gActor142900Animation02774Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation02DACBank1[9] = {
#include "assets/actor_142900_animation_02DAC_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation02DACBank4[132] = {
#include "assets/actor_142900_animation_02DAC_bank4.inc"
};

static AnimationRecord _gActor142900Animation02DACRecords[219] = {
#include "assets/actor_142900_animation_02DAC_records.inc"
};

static u16 _gActor142900Animation02DACIndices[20] = {
#include "assets/actor_142900_animation_02DAC_indices.inc"
};

static AnimationSet _gActor142900Animation02DAC = {
    _gActor142900Animation02DACRecords,
    _gActor142900Animation02DACIndices,
    { NULL, _gActor142900Animation02DACBank1, NULL, NULL, _gActor142900Animation02DACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation03110Bank1[6] = {
#include "assets/actor_142900_animation_03110_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation03110Bank4[72] = {
#include "assets/actor_142900_animation_03110_bank4.inc"
};

static AnimationRecord _gActor142900Animation03110Records[107] = {
#include "assets/actor_142900_animation_03110_records.inc"
};

static u16 _gActor142900Animation03110Indices[20] = {
#include "assets/actor_142900_animation_03110_indices.inc"
};

static AnimationSet _gActor142900Animation03110 = {
    _gActor142900Animation03110Records,
    _gActor142900Animation03110Indices,
    { NULL, _gActor142900Animation03110Bank1, NULL, NULL, _gActor142900Animation03110Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation035C8Bank1[6] = {
#include "assets/actor_142900_animation_035C8_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation035C8Bank4[83] = {
#include "assets/actor_142900_animation_035C8_bank4.inc"
};

static AnimationRecord _gActor142900Animation035C8Records[181] = {
#include "assets/actor_142900_animation_035C8_records.inc"
};

static u16 _gActor142900Animation035C8Indices[20] = {
#include "assets/actor_142900_animation_035C8_indices.inc"
};

static AnimationSet _gActor142900Animation035C8 = {
    _gActor142900Animation035C8Records,
    _gActor142900Animation035C8Indices,
    { NULL, _gActor142900Animation035C8Bank1, NULL, NULL, _gActor142900Animation035C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation03910Bank1[5] = {
#include "assets/actor_142900_animation_03910_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation03910Bank4[66] = {
#include "assets/actor_142900_animation_03910_bank4.inc"
};

static AnimationRecord _gActor142900Animation03910Records[109] = {
#include "assets/actor_142900_animation_03910_records.inc"
};

static u16 _gActor142900Animation03910Indices[20] = {
#include "assets/actor_142900_animation_03910_indices.inc"
};

static AnimationSet _gActor142900Animation03910 = {
    _gActor142900Animation03910Records,
    _gActor142900Animation03910Indices,
    { NULL, _gActor142900Animation03910Bank1, NULL, NULL, _gActor142900Animation03910Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation03F8CBank1[10] = {
#include "assets/actor_142900_animation_03F8C_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation03F8CBank4[142] = {
#include "assets/actor_142900_animation_03F8C_bank4.inc"
};

static AnimationRecord _gActor142900Animation03F8CRecords[223] = {
#include "assets/actor_142900_animation_03F8C_records.inc"
};

static u16 _gActor142900Animation03F8CIndices[20] = {
#include "assets/actor_142900_animation_03F8C_indices.inc"
};

static AnimationSet _gActor142900Animation03F8C = {
    _gActor142900Animation03F8CRecords,
    _gActor142900Animation03F8CIndices,
    { NULL, _gActor142900Animation03F8CBank1, NULL, NULL, _gActor142900Animation03F8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation04260Bank1[4] = {
#include "assets/actor_142900_animation_04260_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation04260Bank4[58] = {
#include "assets/actor_142900_animation_04260_bank4.inc"
};

static AnimationRecord _gActor142900Animation04260Records[91] = {
#include "assets/actor_142900_animation_04260_records.inc"
};

static u16 _gActor142900Animation04260Indices[20] = {
#include "assets/actor_142900_animation_04260_indices.inc"
};

static AnimationSet _gActor142900Animation04260 = {
    _gActor142900Animation04260Records,
    _gActor142900Animation04260Indices,
    { NULL, _gActor142900Animation04260Bank1, NULL, NULL, _gActor142900Animation04260Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation04480Bank1[2] = {
#include "assets/actor_142900_animation_04480_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation04480Bank4[25] = {
#include "assets/actor_142900_animation_04480_bank4.inc"
};

static AnimationRecord _gActor142900Animation04480Records[85] = {
#include "assets/actor_142900_animation_04480_records.inc"
};

static u16 _gActor142900Animation04480Indices[20] = {
#include "assets/actor_142900_animation_04480_indices.inc"
};

static AnimationSet _gActor142900Animation04480 = {
    _gActor142900Animation04480Records,
    _gActor142900Animation04480Indices,
    { NULL, _gActor142900Animation04480Bank1, NULL, NULL, _gActor142900Animation04480Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation04804Bank1[6] = {
#include "assets/actor_142900_animation_04804_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation04804Bank4[69] = {
#include "assets/actor_142900_animation_04804_bank4.inc"
};

static AnimationRecord _gActor142900Animation04804Records[118] = {
#include "assets/actor_142900_animation_04804_records.inc"
};

static u16 _gActor142900Animation04804Indices[20] = {
#include "assets/actor_142900_animation_04804_indices.inc"
};

static AnimationSet _gActor142900Animation04804 = {
    _gActor142900Animation04804Records,
    _gActor142900Animation04804Indices,
    { NULL, _gActor142900Animation04804Bank1, NULL, NULL, _gActor142900Animation04804Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation04B30Bank1[7] = {
#include "assets/actor_142900_animation_04B30_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation04B30Bank4[56] = {
#include "assets/actor_142900_animation_04B30_bank4.inc"
};

static AnimationRecord _gActor142900Animation04B30Records[106] = {
#include "assets/actor_142900_animation_04B30_records.inc"
};

static u16 _gActor142900Animation04B30Indices[20] = {
#include "assets/actor_142900_animation_04B30_indices.inc"
};

static AnimationSet _gActor142900Animation04B30 = {
    _gActor142900Animation04B30Records,
    _gActor142900Animation04B30Indices,
    { NULL, _gActor142900Animation04B30Bank1, NULL, NULL, _gActor142900Animation04B30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation04F7CBank1[6] = {
#include "assets/actor_142900_animation_04F7C_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation04F7CBank4[78] = {
#include "assets/actor_142900_animation_04F7C_bank4.inc"
};

static AnimationRecord _gActor142900Animation04F7CRecords[159] = {
#include "assets/actor_142900_animation_04F7C_records.inc"
};

static u16 _gActor142900Animation04F7CIndices[20] = {
#include "assets/actor_142900_animation_04F7C_indices.inc"
};

static AnimationSet _gActor142900Animation04F7C = {
    _gActor142900Animation04F7CRecords,
    _gActor142900Animation04F7CIndices,
    { NULL, _gActor142900Animation04F7CBank1, NULL, NULL, _gActor142900Animation04F7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation052A4Bank1[5] = {
#include "assets/actor_142900_animation_052A4_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation052A4Bank4[69] = {
#include "assets/actor_142900_animation_052A4_bank4.inc"
};

static AnimationRecord _gActor142900Animation052A4Records[98] = {
#include "assets/actor_142900_animation_052A4_records.inc"
};

static u16 _gActor142900Animation052A4Indices[20] = {
#include "assets/actor_142900_animation_052A4_indices.inc"
};

static AnimationSet _gActor142900Animation052A4 = {
    _gActor142900Animation052A4Records,
    _gActor142900Animation052A4Indices,
    { NULL, _gActor142900Animation052A4Bank1, NULL, NULL, _gActor142900Animation052A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation05564Bank1[4] = {
#include "assets/actor_142900_animation_05564_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation05564Bank4[55] = {
#include "assets/actor_142900_animation_05564_bank4.inc"
};

static AnimationRecord _gActor142900Animation05564Records[89] = {
#include "assets/actor_142900_animation_05564_records.inc"
};

static u16 _gActor142900Animation05564Indices[20] = {
#include "assets/actor_142900_animation_05564_indices.inc"
};

static AnimationSet _gActor142900Animation05564 = {
    _gActor142900Animation05564Records,
    _gActor142900Animation05564Indices,
    { NULL, _gActor142900Animation05564Bank1, NULL, NULL, _gActor142900Animation05564Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142900Animation057B8Bank1[2] = {
#include "assets/actor_142900_animation_057B8_bank1.inc"
};

static AnimationPackedRotation _gActor142900Animation057B8Bank4[30] = {
#include "assets/actor_142900_animation_057B8_bank4.inc"
};

static AnimationRecord _gActor142900Animation057B8Records[93] = {
#include "assets/actor_142900_animation_057B8_records.inc"
};

static u16 _gActor142900Animation057B8Indices[20] = {
#include "assets/actor_142900_animation_057B8_indices.inc"
};

static AnimationSet _gActor142900Animation057B8 = {
    _gActor142900Animation057B8Records,
    _gActor142900Animation057B8Indices,
    { NULL, _gActor142900Animation057B8Bank1, NULL, NULL, _gActor142900Animation057B8Bank4, NULL, NULL, NULL },
};

Actor142900TaskTable D_actor_142900_80137600 = { .native = { { 0, 192, { .enemyCleanup = Gp_DestroyEnemy }, { .value = 0 } }, { 0, 192, { .task = func_actor_142900_80131E24 }, { .value = 0 } } } };

Actor142900AnimStorage7618 D_actor_142900_80137618 = { .data = {
                                                           { &_gActor142900Animation005C8, &_gActor142900Animation0089C, &_gActor142900Animation00B88, &_gActor142900Animation02774, &_gActor142900Animation02DAC, &_gActor142900Animation03110, &_gActor142900Animation035C8, &_gActor142900Animation03910, &_gActor142900Animation03F8C, &_gActor142900Animation04260, &_gActor142900Animation04480, &_gActor142900Animation010EC, &_gActor142900Animation01370 },
                                                           { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE } } } };

Actor142900AnimStorage7764 D_actor_142900_80137764 = { .data = {
                                                           { &_gActor142900Animation0161C, &_gActor142900Animation018BC, &_gActor142900Animation01A94, &_gActor142900Animation02200, &_gActor142900Animation04804, &_gActor142900Animation04B30, &_gActor142900Animation04F7C, &_gActor142900Animation052A4, &_gActor142900Animation05564, &_gActor142900Animation057B8 },
                                                           { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE } } } };

GpCopyArg D_actor_142900_80137890 = { { .words = D_actor_142900_80137764.words }, 32 };

GpCopyArg D_actor_142900_80137898 = { { .words = D_actor_142900_80137618.words }, 32 };

ActorTransform D_actor_142900_801378A0 = { { 0x2C60, 0, -1050, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_142900_801378B8 = { { 0x2C60, 0, 190, 0 }, { 0, 2048, 0, 0 } };

EvsCommand D_actor_142900_801378D0[87] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_actor_142900_80137890 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_actor_142900_80137898 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_142900_801378A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_142900_801378B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_142900_80131FDC }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_142900_80131FDC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541A0003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM }, { .value = 0 }, { .value = 5100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x541A0001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[7] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[8] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[9] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[10] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[9] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[11] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[10] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[12] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 48 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137618.data.arguments[13] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[11] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 48 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[12] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_142900_80131F5C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_142900_801380F8[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_142900_80131FDC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_142900_80131F5C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_142900_801378A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 D_actor_142900_801382A8;

s32 D_actor_142900_801382AC;

void func_actor_142900_80131E24(Task* arg0)
{
    extern void displaySetShakeY();
    s32         var_a0;

    if (D_actor_142900_801382AC == 2) {
        D_actor_142900_801382AC = 3;
        D_actor_142900_801382A8 = 0x14;
    }
    var_a0 = rsin((arg0->killCountdown << 0xC) / 60) / 1024;
    if (D_actor_142900_801382AC == 1) {
        arg0->killCountdown = arg0->killCountdown + 1;
    }
    if (D_actor_142900_801382AC == 3) {
        var_a0                  = var_a0 * D_actor_142900_801382A8 / 20;
        D_actor_142900_801382A8 = D_actor_142900_801382A8 - 1;
        arg0->killCountdown     = arg0->killCountdown + 5;
        if (D_actor_142900_801382A8 == 0) {
            D_actor_142900_801382AC = 0;
        }
    }
    if (D_actor_142900_801382AC == 0) {
        displaySetShakeY(0);
        taskKill(arg0);
    } else {
        displaySetShakeY(var_a0);
    }
}

void func_actor_142900_80131F5C(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
        Gp_ApplyAreaRecs(D_shelter_b2_elevator_8017E9F8);
        GameFlag_SetNibble(0x4C, 0);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = 0x1B;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 2;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 1;
        gDisplayState.spriteVariant                                = 1;
        Task_Spawn(0, 0x11, 0, 0);
    }
}

void func_actor_142900_80131FDC(s32 arg0)
{
    if (arg0 == 1) {
        Task_SpawnFromTable(D_actor_142900_80137600.tasks, 1, 0, 0);
    }
    D_actor_142900_801382AC = arg0;
}
