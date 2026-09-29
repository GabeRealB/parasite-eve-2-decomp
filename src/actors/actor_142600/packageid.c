#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/task_types.h"

// This package contains no code. Its retained data shares the existing ID TU.

static const s32 packageId = PKG_ID;

typedef struct {
    u16   flags;
    u16   priority;
    void  (*callback)(GpEnemy*, Task*);
    void* argument;
} Actor142600RetainedTaskSeed;
STATIC_ASSERT_SIZEOF(Actor142600RetainedTaskSeed, 12);

// The copy descriptors request 32 words. Both banks include the following
// animation arguments in their backing allocation, as the original copies do.
typedef union {
    struct {
        GpAnimSet* sets[7];
        GpAnimArg  arguments[8];
    } data;
    s32 words[47];
} Actor142600AnimationBankA;
STATIC_ASSERT_SIZEOF(Actor142600AnimationBankA, 188);

typedef union {
    struct {
        GpAnimSet* sets[13];
        GpAnimArg  arguments[17];
    } data;
    s32 words[98];
} Actor142600AnimationBankB;
STATIC_ASSERT_SIZEOF(Actor142600AnimationBankB, 392);

extern Actor142600AnimationBankA D_actor_142600_80135E30;
extern Actor142600AnimationBankB D_actor_142600_80135EEC;

extern Actor142600AnimationBankA D_actor_142600_80135E30;
extern Actor142600AnimationBankB D_actor_142600_80135EEC;
extern GpCopyArg                 D_actor_142600_80136074;
extern GpCopyArg                 D_actor_142600_8013607C;

AnimationPackedPose D_actor_142600_80131E24[3] = {
#include "assets/actor_142600_animation_00224_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80131E48[27] = {
#include "assets/actor_142600_animation_00224_bank4.inc"
};

AnimationRecord D_actor_142600_80131EB4[90] = {
#include "assets/actor_142600_animation_00224_records.inc"
};

u16 D_actor_142600_8013201C[20] = {
#include "assets/actor_142600_animation_00224_indices.inc"
};

GpAnimSet D_actor_142600_80132044 = {
    D_actor_142600_80131EB4,
    D_actor_142600_8013201C,
    { NULL, D_actor_142600_80131E24, NULL, NULL, D_actor_142600_80131E48, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_8013206C[2] = {
#include "assets/actor_142600_animation_00464_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80132084[41] = {
#include "assets/actor_142600_animation_00464_bank4.inc"
};

AnimationRecord D_actor_142600_80132128[77] = {
#include "assets/actor_142600_animation_00464_records.inc"
};

u16 D_actor_142600_8013225C[20] = {
#include "assets/actor_142600_animation_00464_indices.inc"
};

GpAnimSet D_actor_142600_80132284 = {
    D_actor_142600_80132128,
    D_actor_142600_8013225C,
    { NULL, D_actor_142600_8013206C, NULL, NULL, D_actor_142600_80132084, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_801322AC[2] = {
#include "assets/actor_142600_animation_006A4_bank1.inc"
};

AnimationPackedRotation D_actor_142600_801322C4[41] = {
#include "assets/actor_142600_animation_006A4_bank4.inc"
};

AnimationRecord D_actor_142600_80132368[77] = {
#include "assets/actor_142600_animation_006A4_records.inc"
};

u16 D_actor_142600_8013249C[20] = {
#include "assets/actor_142600_animation_006A4_indices.inc"
};

GpAnimSet D_actor_142600_801324C4 = {
    D_actor_142600_80132368,
    D_actor_142600_8013249C,
    { NULL, D_actor_142600_801322AC, NULL, NULL, D_actor_142600_801322C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_801324EC[2] = {
#include "assets/actor_142600_animation_008F4_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80132504[30] = {
#include "assets/actor_142600_animation_008F4_bank4.inc"
};

AnimationRecord D_actor_142600_8013257C[92] = {
#include "assets/actor_142600_animation_008F4_records.inc"
};

u16 D_actor_142600_801326EC[20] = {
#include "assets/actor_142600_animation_008F4_indices.inc"
};

GpAnimSet D_actor_142600_80132714 = {
    D_actor_142600_8013257C,
    D_actor_142600_801326EC,
    { NULL, D_actor_142600_801324EC, NULL, NULL, D_actor_142600_80132504, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_8013273C[2] = {
#include "assets/actor_142600_animation_00D38_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80132754[102] = {
#include "assets/actor_142600_animation_00D38_bank4.inc"
};

AnimationRecord D_actor_142600_801328EC[145] = {
#include "assets/actor_142600_animation_00D38_records.inc"
};

u16 D_actor_142600_80132B30[20] = {
#include "assets/actor_142600_animation_00D38_indices.inc"
};

GpAnimSet D_actor_142600_80132B58 = {
    D_actor_142600_801328EC,
    D_actor_142600_80132B30,
    { NULL, D_actor_142600_8013273C, NULL, NULL, D_actor_142600_80132754, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_80132B80[5] = {
#include "assets/actor_142600_animation_010E4_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80132BBC[71] = {
#include "assets/actor_142600_animation_010E4_bank4.inc"
};

AnimationRecord D_actor_142600_80132CD8[129] = {
#include "assets/actor_142600_animation_010E4_records.inc"
};

u16 D_actor_142600_80132EDC[20] = {
#include "assets/actor_142600_animation_010E4_indices.inc"
};

GpAnimSet D_actor_142600_80132F04 = {
    D_actor_142600_80132CD8,
    D_actor_142600_80132EDC,
    { NULL, D_actor_142600_80132B80, NULL, NULL, D_actor_142600_80132BBC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_80132F2C[2] = {
#include "assets/actor_142600_animation_013D4_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80132F44[59] = {
#include "assets/actor_142600_animation_013D4_bank4.inc"
};

AnimationRecord D_actor_142600_80133030[103] = {
#include "assets/actor_142600_animation_013D4_records.inc"
};

u16 D_actor_142600_801331CC[20] = {
#include "assets/actor_142600_animation_013D4_indices.inc"
};

GpAnimSet D_actor_142600_801331F4 = {
    D_actor_142600_80133030,
    D_actor_142600_801331CC,
    { NULL, D_actor_142600_80132F2C, NULL, NULL, D_actor_142600_80132F44, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_8013321C[4] = {
#include "assets/actor_142600_animation_016C8_bank1.inc"
};

AnimationPackedRotation D_actor_142600_8013324C[54] = {
#include "assets/actor_142600_animation_016C8_bank4.inc"
};

AnimationRecord D_actor_142600_80133324[103] = {
#include "assets/actor_142600_animation_016C8_records.inc"
};

u16 D_actor_142600_801334C0[20] = {
#include "assets/actor_142600_animation_016C8_indices.inc"
};

GpAnimSet D_actor_142600_801334E8 = {
    D_actor_142600_80133324,
    D_actor_142600_801334C0,
    { NULL, D_actor_142600_8013321C, NULL, NULL, D_actor_142600_8013324C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_80133510[2] = {
#include "assets/actor_142600_animation_01884_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80133528[20] = {
#include "assets/actor_142600_animation_01884_bank4.inc"
};

AnimationRecord D_actor_142600_80133578[65] = {
#include "assets/actor_142600_animation_01884_records.inc"
};

u16 D_actor_142600_8013367C[20] = {
#include "assets/actor_142600_animation_01884_indices.inc"
};

GpAnimSet D_actor_142600_801336A4 = {
    D_actor_142600_80133578,
    D_actor_142600_8013367C,
    { NULL, D_actor_142600_80133510, NULL, NULL, D_actor_142600_80133528, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_801336CC[8] = {
#include "assets/actor_142600_animation_01D98_bank1.inc"
};

AnimationPackedRotation D_actor_142600_8013372C[116] = {
#include "assets/actor_142600_animation_01D98_bank4.inc"
};

AnimationRecord D_actor_142600_801338FC[165] = {
#include "assets/actor_142600_animation_01D98_records.inc"
};

u16 D_actor_142600_80133B90[20] = {
#include "assets/actor_142600_animation_01D98_indices.inc"
};

GpAnimSet D_actor_142600_80133BB8 = {
    D_actor_142600_801338FC,
    D_actor_142600_80133B90,
    { NULL, D_actor_142600_801336CC, NULL, NULL, D_actor_142600_8013372C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_80133BE0[2] = {
#include "assets/actor_142600_animation_02000_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80133BF8[42] = {
#include "assets/actor_142600_animation_02000_bank4.inc"
};

AnimationRecord D_actor_142600_80133CA0[86] = {
#include "assets/actor_142600_animation_02000_records.inc"
};

u16 D_actor_142600_80133DF8[20] = {
#include "assets/actor_142600_animation_02000_indices.inc"
};

GpAnimSet D_actor_142600_80133E20 = {
    D_actor_142600_80133CA0,
    D_actor_142600_80133DF8,
    { NULL, D_actor_142600_80133BE0, NULL, NULL, D_actor_142600_80133BF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_80133E48[10] = {
#include "assets/actor_142600_animation_02468_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80133EC0[96] = {
#include "assets/actor_142600_animation_02468_bank4.inc"
};

AnimationRecord D_actor_142600_80134040[136] = {
#include "assets/actor_142600_animation_02468_records.inc"
};

u16 D_actor_142600_80134260[20] = {
#include "assets/actor_142600_animation_02468_indices.inc"
};

GpAnimSet D_actor_142600_80134288 = {
    D_actor_142600_80134040,
    D_actor_142600_80134260,
    { NULL, D_actor_142600_80133E48, NULL, NULL, D_actor_142600_80133EC0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_801342B0[5] = {
#include "assets/actor_142600_animation_0276C_bank1.inc"
};

AnimationPackedRotation D_actor_142600_801342EC[55] = {
#include "assets/actor_142600_animation_0276C_bank4.inc"
};

AnimationRecord D_actor_142600_801343C8[103] = {
#include "assets/actor_142600_animation_0276C_records.inc"
};

u16 D_actor_142600_80134564[20] = {
#include "assets/actor_142600_animation_0276C_indices.inc"
};

GpAnimSet D_actor_142600_8013458C = {
    D_actor_142600_801343C8,
    D_actor_142600_80134564,
    { NULL, D_actor_142600_801342B0, NULL, NULL, D_actor_142600_801342EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_801345B4[8] = {
#include "assets/actor_142600_animation_02B40_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80134614[84] = {
#include "assets/actor_142600_animation_02B40_bank4.inc"
};

AnimationRecord D_actor_142600_80134764[117] = {
#include "assets/actor_142600_animation_02B40_records.inc"
};

u16 D_actor_142600_80134938[20] = {
#include "assets/actor_142600_animation_02B40_indices.inc"
};

GpAnimSet D_actor_142600_80134960 = {
    D_actor_142600_80134764,
    D_actor_142600_80134938,
    { NULL, D_actor_142600_801345B4, NULL, NULL, D_actor_142600_80134614, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_80134988[7] = {
#include "assets/actor_142600_animation_02F30_bank1.inc"
};

AnimationPackedRotation D_actor_142600_801349DC[62] = {
#include "assets/actor_142600_animation_02F30_bank4.inc"
};

AnimationRecord D_actor_142600_80134AD4[149] = {
#include "assets/actor_142600_animation_02F30_records.inc"
};

u16 D_actor_142600_80134D28[20] = {
#include "assets/actor_142600_animation_02F30_indices.inc"
};

GpAnimSet D_actor_142600_80134D50 = {
    D_actor_142600_80134AD4,
    D_actor_142600_80134D28,
    { NULL, D_actor_142600_80134988, NULL, NULL, D_actor_142600_801349DC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_80134D78[7] = {
#include "assets/actor_142600_animation_0329C_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80134DCC[74] = {
#include "assets/actor_142600_animation_0329C_bank4.inc"
};

AnimationRecord D_actor_142600_80134EF4[104] = {
#include "assets/actor_142600_animation_0329C_records.inc"
};

u16 D_actor_142600_80135094[20] = {
#include "assets/actor_142600_animation_0329C_indices.inc"
};

GpAnimSet D_actor_142600_801350BC = {
    D_actor_142600_80134EF4,
    D_actor_142600_80135094,
    { NULL, D_actor_142600_80134D78, NULL, NULL, D_actor_142600_80134DCC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_801350E4[6] = {
#include "assets/actor_142600_animation_03784_bank1.inc"
};

AnimationPackedRotation D_actor_142600_8013512C[110] = {
#include "assets/actor_142600_animation_03784_bank4.inc"
};

AnimationRecord D_actor_142600_801352E4[166] = {
#include "assets/actor_142600_animation_03784_records.inc"
};

u16 D_actor_142600_8013557C[20] = {
#include "assets/actor_142600_animation_03784_indices.inc"
};

GpAnimSet D_actor_142600_801355A4 = {
    D_actor_142600_801352E4,
    D_actor_142600_8013557C,
    { NULL, D_actor_142600_801350E4, NULL, NULL, D_actor_142600_8013512C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_801355CC[5] = {
#include "assets/actor_142600_animation_03AEC_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80135608[74] = {
#include "assets/actor_142600_animation_03AEC_bank4.inc"
};

AnimationRecord D_actor_142600_80135730[109] = {
#include "assets/actor_142600_animation_03AEC_records.inc"
};

u16 D_actor_142600_801358E4[20] = {
#include "assets/actor_142600_animation_03AEC_indices.inc"
};

GpAnimSet D_actor_142600_8013590C = {
    D_actor_142600_80135730,
    D_actor_142600_801358E4,
    { NULL, D_actor_142600_801355CC, NULL, NULL, D_actor_142600_80135608, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_80135934[2] = {
#include "assets/actor_142600_animation_03CC4_bank1.inc"
};

AnimationPackedRotation D_actor_142600_8013594C[16] = {
#include "assets/actor_142600_animation_03CC4_bank4.inc"
};

AnimationRecord D_actor_142600_8013598C[76] = {
#include "assets/actor_142600_animation_03CC4_records.inc"
};

u16 D_actor_142600_80135ABC[20] = {
#include "assets/actor_142600_animation_03CC4_indices.inc"
};

GpAnimSet D_actor_142600_80135AE4 = {
    D_actor_142600_8013598C,
    D_actor_142600_80135ABC,
    { NULL, D_actor_142600_80135934, NULL, NULL, D_actor_142600_8013594C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_142600_80135B0C[5] = {
#include "assets/actor_142600_animation_03FDC_bank1.inc"
};

AnimationPackedRotation D_actor_142600_80135B48[66] = {
#include "assets/actor_142600_animation_03FDC_bank4.inc"
};

AnimationRecord D_actor_142600_80135C50[97] = {
#include "assets/actor_142600_animation_03FDC_records.inc"
};

u16 D_actor_142600_80135DD4[20] = {
#include "assets/actor_142600_animation_03FDC_indices.inc"
};

GpAnimSet D_actor_142600_80135DFC = {
    D_actor_142600_80135C50,
    D_actor_142600_80135DD4,
    { NULL, D_actor_142600_80135B0C, NULL, NULL, D_actor_142600_80135B48, NULL, NULL, NULL },
};

Actor142600RetainedTaskSeed D_actor_142600_80135E24 = { 0, 192, Gp_DestroyEnemy, NULL };

Actor142600AnimationBankA D_actor_142600_80135E30 = { .data = { { &D_actor_142600_80132044, &D_actor_142600_80132284, &D_actor_142600_801324C4, &D_actor_142600_80132714, &D_actor_142600_80132B58, &D_actor_142600_80132F04, &D_actor_142600_801331F4 }, { { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 48, 0, 0, 0 }, { { .index = 1 }, 49, 0, 0, 0 }, { { .index = 1 }, 50, 0, 0, 0 }, { { .index = 1 }, 51, 0, 0, 0 }, { { .index = 1 }, 52, 0, 0, 0 }, { { .index = 1 }, 53, 0, 0, 0 } } } };

Actor142600AnimationBankB D_actor_142600_80135EEC = { .data = { { &D_actor_142600_801334E8, &D_actor_142600_801336A4, &D_actor_142600_80133BB8, &D_actor_142600_80133E20, &D_actor_142600_80134960, &D_actor_142600_80134D50, &D_actor_142600_801350BC, &D_actor_142600_8013458C, &D_actor_142600_801355A4, &D_actor_142600_80134288, &D_actor_142600_8013590C, &D_actor_142600_80135AE4, &D_actor_142600_80135DFC }, { { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 48, 0, 0, 0 }, { { .index = 1 }, 49, 0, 0, 0 }, { { .index = 1 }, 50, 0, 0, 0 }, { { .index = 1 }, 1, 0, 0, 1 }, { { .index = 1 }, 1, 1, 20, 0 }, { { .index = 6 }, 1, 0, 0, 1 }, { { .index = 1 }, 51, 0, 0, 0 }, { { .index = 1 }, 52, 0, 0, 0 }, { { .index = 1 }, 53, 0, 0, 0 }, { { .index = 1 }, 54, 1, 10, 0 }, { { .index = 1 }, 55, 1, 10, 0 }, { { .index = 1 }, 56, 1, 10, 0 }, { { .index = 1 }, 57, 0, 0, 0 }, { { .index = 1 }, 58, 0, 0, 0 }, { { .index = 1 }, 59, 0, 0, 0 } } } };

GpCopyArg D_actor_142600_80136074 = { { .words = D_actor_142600_80135EEC.words }, 32 };

GpCopyArg D_actor_142600_8013607C = { { .words = D_actor_142600_80135E30.words }, 32 };

GpXformArg D_actor_142600_80136084 = { { -5860, 0, 1540, 0 }, { 0, 2275, 0, 0 } };

GpXformArg D_actor_142600_8013609C = { { -6440, 0, 110, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_actor_142600_801360B4 = { { -6000, 0, 948, 0 }, { 0, 2048, 0, 0 } };

GpXformArg D_actor_142600_801360CC = { { -6442, 0, 1540, 0 }, { 0, 2048, 0, 0 } };

GpEvsCmd D_actor_142600_801360E4[76] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_142600_80136074 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_142600_8013607C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[5] }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[7] }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[1] }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_142600_80136084 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_142600_8013609C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[5] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[11] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[7] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[4] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[2] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[5] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[7] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[2] }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[4] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[6] }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[5] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[6] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[14] }, { .value = 0 } },
    { 4, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[15] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[5] }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[12] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[2] }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135E30.data.arguments[4] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[13] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_142600_801360B4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[5] }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_142600_801360CC }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[7] }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_142600_80136804[15] = {
    { 19, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_142600_80135EEC.data.arguments[7] }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_142600_801360CC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_142600_801360B4 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};
