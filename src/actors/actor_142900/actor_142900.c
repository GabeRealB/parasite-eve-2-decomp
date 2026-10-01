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

extern AnimationSet D_actor_142900_801323E8;
extern AnimationSet D_actor_142900_801326BC;
extern AnimationSet D_actor_142900_801329A8;
extern AnimationSet D_actor_142900_80132F0C;
extern AnimationSet D_actor_142900_80133190;
extern AnimationSet D_actor_142900_80134594;
extern AnimationSet D_actor_142900_80134BCC;
extern AnimationSet D_actor_142900_80134F30;
extern AnimationSet D_actor_142900_801353E8;
extern AnimationSet D_actor_142900_80135730;
extern AnimationSet D_actor_142900_80135DAC;
extern AnimationSet D_actor_142900_80136080;
extern AnimationSet D_actor_142900_801362A0;

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

extern AnimationSet D_actor_142900_8013343C;
extern AnimationSet D_actor_142900_801336DC;
extern AnimationSet D_actor_142900_801338B4;
extern AnimationSet D_actor_142900_80134020;
extern AnimationSet D_actor_142900_80136624;
extern AnimationSet D_actor_142900_80136950;
extern AnimationSet D_actor_142900_80136D9C;
extern AnimationSet D_actor_142900_801370C4;
extern AnimationSet D_actor_142900_80137384;
extern AnimationSet D_actor_142900_801375D8;

void func_actor_142900_80131E24(Task*);

AnimationPackedPose D_actor_142900_80132024[7] = {
#include "assets/actor_142900_animation_005C8_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80132078[71] = {
#include "assets/actor_142900_animation_005C8_bank4.inc"
};

AnimationRecord D_actor_142900_80132194[139] = {
#include "assets/actor_142900_animation_005C8_records.inc"
};

u16 D_actor_142900_801323C0[20] = {
#include "assets/actor_142900_animation_005C8_indices.inc"
};

AnimationSet D_actor_142900_801323E8 = {
    D_actor_142900_80132194,
    D_actor_142900_801323C0,
    { NULL, D_actor_142900_80132024, NULL, NULL, D_actor_142900_80132078, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80132410[4] = {
#include "assets/actor_142900_animation_0089C_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80132440[54] = {
#include "assets/actor_142900_animation_0089C_bank4.inc"
};

AnimationRecord D_actor_142900_80132518[95] = {
#include "assets/actor_142900_animation_0089C_records.inc"
};

u16 D_actor_142900_80132694[20] = {
#include "assets/actor_142900_animation_0089C_indices.inc"
};

AnimationSet D_actor_142900_801326BC = {
    D_actor_142900_80132518,
    D_actor_142900_80132694,
    { NULL, D_actor_142900_80132410, NULL, NULL, D_actor_142900_80132440, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_801326E4[4] = {
#include "assets/actor_142900_animation_00B88_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80132714[60] = {
#include "assets/actor_142900_animation_00B88_bank4.inc"
};

AnimationRecord D_actor_142900_80132804[95] = {
#include "assets/actor_142900_animation_00B88_records.inc"
};

u16 D_actor_142900_80132980[20] = {
#include "assets/actor_142900_animation_00B88_indices.inc"
};

AnimationSet D_actor_142900_801329A8 = {
    D_actor_142900_80132804,
    D_actor_142900_80132980,
    { NULL, D_actor_142900_801326E4, NULL, NULL, D_actor_142900_80132714, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_801329D0[8] = {
#include "assets/actor_142900_animation_010EC_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80132A30[111] = {
#include "assets/actor_142900_animation_010EC_bank4.inc"
};

AnimationRecord D_actor_142900_80132BEC[190] = {
#include "assets/actor_142900_animation_010EC_records.inc"
};

u16 D_actor_142900_80132EE4[20] = {
#include "assets/actor_142900_animation_010EC_indices.inc"
};

AnimationSet D_actor_142900_80132F0C = {
    D_actor_142900_80132BEC,
    D_actor_142900_80132EE4,
    { NULL, D_actor_142900_801329D0, NULL, NULL, D_actor_142900_80132A30, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80132F34[4] = {
#include "assets/actor_142900_animation_01370_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80132F64[50] = {
#include "assets/actor_142900_animation_01370_bank4.inc"
};

AnimationRecord D_actor_142900_8013302C[79] = {
#include "assets/actor_142900_animation_01370_records.inc"
};

u16 D_actor_142900_80133168[20] = {
#include "assets/actor_142900_animation_01370_indices.inc"
};

AnimationSet D_actor_142900_80133190 = {
    D_actor_142900_8013302C,
    D_actor_142900_80133168,
    { NULL, D_actor_142900_80132F34, NULL, NULL, D_actor_142900_80132F64, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_801331B8[4] = {
#include "assets/actor_142900_animation_0161C_bank1.inc"
};

AnimationPackedRotation D_actor_142900_801331E8[52] = {
#include "assets/actor_142900_animation_0161C_bank4.inc"
};

AnimationRecord D_actor_142900_801332B8[87] = {
#include "assets/actor_142900_animation_0161C_records.inc"
};

u16 D_actor_142900_80133414[20] = {
#include "assets/actor_142900_animation_0161C_indices.inc"
};

AnimationSet D_actor_142900_8013343C = {
    D_actor_142900_801332B8,
    D_actor_142900_80133414,
    { NULL, D_actor_142900_801331B8, NULL, NULL, D_actor_142900_801331E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80133464[4] = {
#include "assets/actor_142900_animation_018BC_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80133494[50] = {
#include "assets/actor_142900_animation_018BC_bank4.inc"
};

AnimationRecord D_actor_142900_8013355C[86] = {
#include "assets/actor_142900_animation_018BC_records.inc"
};

u16 D_actor_142900_801336B4[20] = {
#include "assets/actor_142900_animation_018BC_indices.inc"
};

AnimationSet D_actor_142900_801336DC = {
    D_actor_142900_8013355C,
    D_actor_142900_801336B4,
    { NULL, D_actor_142900_80133464, NULL, NULL, D_actor_142900_80133494, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80133704[2] = {
#include "assets/actor_142900_animation_01A94_bank1.inc"
};

AnimationPackedRotation D_actor_142900_8013371C[16] = {
#include "assets/actor_142900_animation_01A94_bank4.inc"
};

AnimationRecord D_actor_142900_8013375C[76] = {
#include "assets/actor_142900_animation_01A94_records.inc"
};

u16 D_actor_142900_8013388C[20] = {
#include "assets/actor_142900_animation_01A94_indices.inc"
};

AnimationSet D_actor_142900_801338B4 = {
    D_actor_142900_8013375C,
    D_actor_142900_8013388C,
    { NULL, D_actor_142900_80133704, NULL, NULL, D_actor_142900_8013371C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_801338DC[12] = {
#include "assets/actor_142900_animation_02200_bank1.inc"
};

AnimationPackedRotation D_actor_142900_8013396C[188] = {
#include "assets/actor_142900_animation_02200_bank4.inc"
};

AnimationRecord D_actor_142900_80133C5C[231] = {
#include "assets/actor_142900_animation_02200_records.inc"
};

u16 D_actor_142900_80133FF8[20] = {
#include "assets/actor_142900_animation_02200_indices.inc"
};

AnimationSet D_actor_142900_80134020 = {
    D_actor_142900_80133C5C,
    D_actor_142900_80133FF8,
    { NULL, D_actor_142900_801338DC, NULL, NULL, D_actor_142900_8013396C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80134048[2] = {
#include "assets/actor_142900_animation_02774_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80134060[135] = {
#include "assets/actor_142900_animation_02774_bank4.inc"
};

AnimationRecord D_actor_142900_8013427C[188] = {
#include "assets/actor_142900_animation_02774_records.inc"
};

u16 D_actor_142900_8013456C[20] = {
#include "assets/actor_142900_animation_02774_indices.inc"
};

AnimationSet D_actor_142900_80134594 = {
    D_actor_142900_8013427C,
    D_actor_142900_8013456C,
    { NULL, D_actor_142900_80134048, NULL, NULL, D_actor_142900_80134060, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_801345BC[9] = {
#include "assets/actor_142900_animation_02DAC_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80134628[132] = {
#include "assets/actor_142900_animation_02DAC_bank4.inc"
};

AnimationRecord D_actor_142900_80134838[219] = {
#include "assets/actor_142900_animation_02DAC_records.inc"
};

u16 D_actor_142900_80134BA4[20] = {
#include "assets/actor_142900_animation_02DAC_indices.inc"
};

AnimationSet D_actor_142900_80134BCC = {
    D_actor_142900_80134838,
    D_actor_142900_80134BA4,
    { NULL, D_actor_142900_801345BC, NULL, NULL, D_actor_142900_80134628, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80134BF4[6] = {
#include "assets/actor_142900_animation_03110_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80134C3C[72] = {
#include "assets/actor_142900_animation_03110_bank4.inc"
};

AnimationRecord D_actor_142900_80134D5C[107] = {
#include "assets/actor_142900_animation_03110_records.inc"
};

u16 D_actor_142900_80134F08[20] = {
#include "assets/actor_142900_animation_03110_indices.inc"
};

AnimationSet D_actor_142900_80134F30 = {
    D_actor_142900_80134D5C,
    D_actor_142900_80134F08,
    { NULL, D_actor_142900_80134BF4, NULL, NULL, D_actor_142900_80134C3C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80134F58[6] = {
#include "assets/actor_142900_animation_035C8_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80134FA0[83] = {
#include "assets/actor_142900_animation_035C8_bank4.inc"
};

AnimationRecord D_actor_142900_801350EC[181] = {
#include "assets/actor_142900_animation_035C8_records.inc"
};

u16 D_actor_142900_801353C0[20] = {
#include "assets/actor_142900_animation_035C8_indices.inc"
};

AnimationSet D_actor_142900_801353E8 = {
    D_actor_142900_801350EC,
    D_actor_142900_801353C0,
    { NULL, D_actor_142900_80134F58, NULL, NULL, D_actor_142900_80134FA0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80135410[5] = {
#include "assets/actor_142900_animation_03910_bank1.inc"
};

AnimationPackedRotation D_actor_142900_8013544C[66] = {
#include "assets/actor_142900_animation_03910_bank4.inc"
};

AnimationRecord D_actor_142900_80135554[109] = {
#include "assets/actor_142900_animation_03910_records.inc"
};

u16 D_actor_142900_80135708[20] = {
#include "assets/actor_142900_animation_03910_indices.inc"
};

AnimationSet D_actor_142900_80135730 = {
    D_actor_142900_80135554,
    D_actor_142900_80135708,
    { NULL, D_actor_142900_80135410, NULL, NULL, D_actor_142900_8013544C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80135758[10] = {
#include "assets/actor_142900_animation_03F8C_bank1.inc"
};

AnimationPackedRotation D_actor_142900_801357D0[142] = {
#include "assets/actor_142900_animation_03F8C_bank4.inc"
};

AnimationRecord D_actor_142900_80135A08[223] = {
#include "assets/actor_142900_animation_03F8C_records.inc"
};

u16 D_actor_142900_80135D84[20] = {
#include "assets/actor_142900_animation_03F8C_indices.inc"
};

AnimationSet D_actor_142900_80135DAC = {
    D_actor_142900_80135A08,
    D_actor_142900_80135D84,
    { NULL, D_actor_142900_80135758, NULL, NULL, D_actor_142900_801357D0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80135DD4[4] = {
#include "assets/actor_142900_animation_04260_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80135E04[58] = {
#include "assets/actor_142900_animation_04260_bank4.inc"
};

AnimationRecord D_actor_142900_80135EEC[91] = {
#include "assets/actor_142900_animation_04260_records.inc"
};

u16 D_actor_142900_80136058[20] = {
#include "assets/actor_142900_animation_04260_indices.inc"
};

AnimationSet D_actor_142900_80136080 = {
    D_actor_142900_80135EEC,
    D_actor_142900_80136058,
    { NULL, D_actor_142900_80135DD4, NULL, NULL, D_actor_142900_80135E04, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_801360A8[2] = {
#include "assets/actor_142900_animation_04480_bank1.inc"
};

AnimationPackedRotation D_actor_142900_801360C0[25] = {
#include "assets/actor_142900_animation_04480_bank4.inc"
};

AnimationRecord D_actor_142900_80136124[85] = {
#include "assets/actor_142900_animation_04480_records.inc"
};

u16 D_actor_142900_80136278[20] = {
#include "assets/actor_142900_animation_04480_indices.inc"
};

AnimationSet D_actor_142900_801362A0 = {
    D_actor_142900_80136124,
    D_actor_142900_80136278,
    { NULL, D_actor_142900_801360A8, NULL, NULL, D_actor_142900_801360C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_801362C8[6] = {
#include "assets/actor_142900_animation_04804_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80136310[69] = {
#include "assets/actor_142900_animation_04804_bank4.inc"
};

AnimationRecord D_actor_142900_80136424[118] = {
#include "assets/actor_142900_animation_04804_records.inc"
};

u16 D_actor_142900_801365FC[20] = {
#include "assets/actor_142900_animation_04804_indices.inc"
};

AnimationSet D_actor_142900_80136624 = {
    D_actor_142900_80136424,
    D_actor_142900_801365FC,
    { NULL, D_actor_142900_801362C8, NULL, NULL, D_actor_142900_80136310, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_8013664C[7] = {
#include "assets/actor_142900_animation_04B30_bank1.inc"
};

AnimationPackedRotation D_actor_142900_801366A0[56] = {
#include "assets/actor_142900_animation_04B30_bank4.inc"
};

AnimationRecord D_actor_142900_80136780[106] = {
#include "assets/actor_142900_animation_04B30_records.inc"
};

u16 D_actor_142900_80136928[20] = {
#include "assets/actor_142900_animation_04B30_indices.inc"
};

AnimationSet D_actor_142900_80136950 = {
    D_actor_142900_80136780,
    D_actor_142900_80136928,
    { NULL, D_actor_142900_8013664C, NULL, NULL, D_actor_142900_801366A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80136978[6] = {
#include "assets/actor_142900_animation_04F7C_bank1.inc"
};

AnimationPackedRotation D_actor_142900_801369C0[78] = {
#include "assets/actor_142900_animation_04F7C_bank4.inc"
};

AnimationRecord D_actor_142900_80136AF8[159] = {
#include "assets/actor_142900_animation_04F7C_records.inc"
};

u16 D_actor_142900_80136D74[20] = {
#include "assets/actor_142900_animation_04F7C_indices.inc"
};

AnimationSet D_actor_142900_80136D9C = {
    D_actor_142900_80136AF8,
    D_actor_142900_80136D74,
    { NULL, D_actor_142900_80136978, NULL, NULL, D_actor_142900_801369C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_80136DC4[5] = {
#include "assets/actor_142900_animation_052A4_bank1.inc"
};

AnimationPackedRotation D_actor_142900_80136E00[69] = {
#include "assets/actor_142900_animation_052A4_bank4.inc"
};

AnimationRecord D_actor_142900_80136F14[98] = {
#include "assets/actor_142900_animation_052A4_records.inc"
};

u16 D_actor_142900_8013709C[20] = {
#include "assets/actor_142900_animation_052A4_indices.inc"
};

AnimationSet D_actor_142900_801370C4 = {
    D_actor_142900_80136F14,
    D_actor_142900_8013709C,
    { NULL, D_actor_142900_80136DC4, NULL, NULL, D_actor_142900_80136E00, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_801370EC[4] = {
#include "assets/actor_142900_animation_05564_bank1.inc"
};

AnimationPackedRotation D_actor_142900_8013711C[55] = {
#include "assets/actor_142900_animation_05564_bank4.inc"
};

AnimationRecord D_actor_142900_801371F8[89] = {
#include "assets/actor_142900_animation_05564_records.inc"
};

u16 D_actor_142900_8013735C[20] = {
#include "assets/actor_142900_animation_05564_indices.inc"
};

AnimationSet D_actor_142900_80137384 = {
    D_actor_142900_801371F8,
    D_actor_142900_8013735C,
    { NULL, D_actor_142900_801370EC, NULL, NULL, D_actor_142900_8013711C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142900_801373AC[2] = {
#include "assets/actor_142900_animation_057B8_bank1.inc"
};

AnimationPackedRotation D_actor_142900_801373C4[30] = {
#include "assets/actor_142900_animation_057B8_bank4.inc"
};

AnimationRecord D_actor_142900_8013743C[93] = {
#include "assets/actor_142900_animation_057B8_records.inc"
};

u16 D_actor_142900_801375B0[20] = {
#include "assets/actor_142900_animation_057B8_indices.inc"
};

AnimationSet D_actor_142900_801375D8 = {
    D_actor_142900_8013743C,
    D_actor_142900_801375B0,
    { NULL, D_actor_142900_801373AC, NULL, NULL, D_actor_142900_801373C4, NULL, NULL, NULL },
};

Actor142900TaskTable D_actor_142900_80137600 = { .native = { { 0, 192, { .enemyCleanup = Gp_DestroyEnemy }, { .value = 0 } }, { 0, 192, { .task = func_actor_142900_80131E24 }, { .value = 0 } } } };

Actor142900AnimStorage7618 D_actor_142900_80137618 = { .data = {
                                                           { &D_actor_142900_801323E8, &D_actor_142900_801326BC, &D_actor_142900_801329A8, &D_actor_142900_80134594, &D_actor_142900_80134BCC, &D_actor_142900_80134F30, &D_actor_142900_801353E8, &D_actor_142900_80135730, &D_actor_142900_80135DAC, &D_actor_142900_80136080, &D_actor_142900_801362A0, &D_actor_142900_80132F0C, &D_actor_142900_80133190 },
                                                           { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE } } } };

Actor142900AnimStorage7764 D_actor_142900_80137764 = { .data = {
                                                           { &D_actor_142900_8013343C, &D_actor_142900_801336DC, &D_actor_142900_801338B4, &D_actor_142900_80134020, &D_actor_142900_80136624, &D_actor_142900_80136950, &D_actor_142900_80136D9C, &D_actor_142900_801370C4, &D_actor_142900_80137384, &D_actor_142900_801375D8 },
                                                           { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE } } } };

GpCopyArg D_actor_142900_80137890 = { { .words = D_actor_142900_80137764.words }, 32 };

GpCopyArg D_actor_142900_80137898 = { { .words = D_actor_142900_80137618.words }, 32 };

ActorTransform D_actor_142900_801378A0 = { { 0x2C60, 0, -1050, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_142900_801378B8 = { { 0x2C60, 0, 190, 0 }, { 0, 2048, 0, 0 } };

EvsCommand D_actor_142900_801378D0[87] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_142900_80137890 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_142900_80137898 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_142900_801378A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_142900_801378B8 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 7 }, { .value = 0 }, { .value = 5100 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_142900_801380F8[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_142900_80131FDC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_142900_80131F5C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142900_80137764.data.arguments[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_142900_801378A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
