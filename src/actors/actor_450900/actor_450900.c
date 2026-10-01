#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_b6_growth_room.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[16];
        GpCopyArg            copies[2];
        AnimationPlayRequest arguments[5];
    } data;
    s32 words[45];
} Actor450900AnimStorage5EC0;
STATIC_ASSERT_SIZEOF(Actor450900AnimStorage5EC0, 180);
STATIC_ASSERT(OFFSET_OF(Actor450900AnimStorage5EC0, data.arguments[1]) == 100, actor450900_ally_anim_offset);

extern Actor450900AnimStorage5EC0 D_actor_450900_80135EC0;

// This record has an independently materialized address in the caller.
// The symbol view shares the union backing; it allocates no extra storage.
extern AnimationPlayRequest Actor450900AllyAnim __asm__("D_actor_450900_80135EC0+100");

extern s32                  D_8017A99C;
extern s8                   D_actor_450900_80135E70;
extern s32                  D_actor_450900_80135E74;
extern AnimationPlayRequest D_actor_450900_80135FEC;
extern AnimationPlayRequest D_actor_450900_801360B4;
extern ActorTransform       D_actor_450900_80136458;
extern EvsCommand           D_actor_450900_80136470[];
extern EvsCommand           D_actor_450900_80136680[];
extern EvsCommand           D_actor_450900_80136890[];
extern EvsCommand           D_actor_450900_80136B00[];
extern EvsCommand           D_actor_450900_80136BD8[];
extern s32                  D_actor_450900_80136C98;

/// The save-point capture task spawned by `func_actor_450900_80131E38`, kept
/// alive until `func_actor_450900_80132548` kills it. Script opcode 0xD reaches
/// both this and `func_actor_450900_80132678`, so its one argument is the
/// opcode's immediate.
extern Task* D_actor_450900_80136C9C;

/// This overlay's own spawn table, six `TaskDesc` entries. Index 0 is the exit
/// handler `taskKill`; 1..5 are the overlay's state handlers, and the "next
/// stage" of each is the next entry: `func_actor_450900_80131E38` spawns 5 on
/// its way through, and `func_actor_450900_80132834` spawns 4
/// (`func_actor_450900_8013235C`, the save-data teardown) when the ally has
/// walked past the trigger line.
extern TaskDesc D_actor_450900_80135E78[];

extern Actor450900AnimStorage5EC0 D_actor_450900_80135EC0;
extern AnimationSet               D_actor_450900_80132DA0;
extern AnimationSet               D_actor_450900_801330F8;
extern AnimationSet               D_actor_450900_8013358C;
extern AnimationSet               D_actor_450900_80133754;
extern AnimationSet               D_actor_450900_80133970;
extern AnimationSet               D_actor_450900_80133B24;
extern AnimationSet               D_actor_450900_80133F0C;
extern AnimationSet               D_actor_450900_80134248;
extern AnimationSet               D_actor_450900_80134410;
extern AnimationSet               D_actor_450900_80134850;
extern AnimationSet               D_actor_450900_80134F14;
extern AnimationSet               D_actor_450900_80135158;
extern AnimationSet               D_actor_450900_80135558;
extern AnimationSet               D_actor_450900_80135754;
extern AnimationSet               D_actor_450900_80135A90;
extern AnimationSet               D_actor_450900_80135E48;

extern Actor450900AnimStorage5EC0 D_actor_450900_80135EC0;
extern AnimationPlayRequest       D_actor_450900_80135F74;
extern AnimationPlayRequest       D_actor_450900_80136014;
extern AnimationPlayRequest       D_actor_450900_80136028;
extern AnimationPlayRequest       D_actor_450900_8013603C;
extern AnimationPlayRequest       D_actor_450900_80136050;
extern AnimationPlayRequest       D_actor_450900_80136064;
extern AnimationPlayRequest       D_actor_450900_80136078;
void                              func_actor_450900_80132518(s32);
void                              func_actor_450900_80132678(u8);
void                              func_actor_450900_80132684(s32);
void                              func_actor_450900_80132724(void);

AnimationPackedPose D_actor_450900_801328A4[5] = {
#include "assets/actor_450900_animation_00F80_bank1.inc"
};

AnimationPackedRotation D_actor_450900_801328E0[101] = {
#include "assets/actor_450900_animation_00F80_bank4.inc"
};

AnimationRecord D_actor_450900_80132A74[193] = {
#include "assets/actor_450900_animation_00F80_records.inc"
};

u16 D_actor_450900_80132D78[20] = {
#include "assets/actor_450900_animation_00F80_indices.inc"
};

AnimationSet D_actor_450900_80132DA0 = {
    D_actor_450900_80132A74,
    D_actor_450900_80132D78,
    { NULL, D_actor_450900_801328A4, NULL, NULL, D_actor_450900_801328E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80132DC8[4] = {
#include "assets/actor_450900_animation_012D8_bank1.inc"
};

AnimationPackedRotation D_actor_450900_80132DF8[41] = {
#include "assets/actor_450900_animation_012D8_bank4.inc"
};

AnimationRecord D_actor_450900_80132E9C[141] = {
#include "assets/actor_450900_animation_012D8_records.inc"
};

u16 D_actor_450900_801330D0[20] = {
#include "assets/actor_450900_animation_012D8_indices.inc"
};

AnimationSet D_actor_450900_801330F8 = {
    D_actor_450900_80132E9C,
    D_actor_450900_801330D0,
    { NULL, D_actor_450900_80132DC8, NULL, NULL, D_actor_450900_80132DF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80133120[8] = {
#include "assets/actor_450900_animation_0176C_bank1.inc"
};

AnimationPackedRotation D_actor_450900_80133180[90] = {
#include "assets/actor_450900_animation_0176C_bank4.inc"
};

AnimationRecord D_actor_450900_801332E8[159] = {
#include "assets/actor_450900_animation_0176C_records.inc"
};

u16 D_actor_450900_80133564[20] = {
#include "assets/actor_450900_animation_0176C_indices.inc"
};

AnimationSet D_actor_450900_8013358C = {
    D_actor_450900_801332E8,
    D_actor_450900_80133564,
    { NULL, D_actor_450900_80133120, NULL, NULL, D_actor_450900_80133180, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_801335B4[2] = {
#include "assets/actor_450900_animation_01934_bank1.inc"
};

AnimationPackedRotation D_actor_450900_801335CC[21] = {
#include "assets/actor_450900_animation_01934_bank4.inc"
};

AnimationRecord D_actor_450900_80133620[67] = {
#include "assets/actor_450900_animation_01934_records.inc"
};

u16 D_actor_450900_8013372C[20] = {
#include "assets/actor_450900_animation_01934_indices.inc"
};

AnimationSet D_actor_450900_80133754 = {
    D_actor_450900_80133620,
    D_actor_450900_8013372C,
    { NULL, D_actor_450900_801335B4, NULL, NULL, D_actor_450900_801335CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_8013377C[2] = {
#include "assets/actor_450900_animation_01B50_bank1.inc"
};

AnimationPackedRotation D_actor_450900_80133794[28] = {
#include "assets/actor_450900_animation_01B50_bank4.inc"
};

AnimationRecord D_actor_450900_80133804[81] = {
#include "assets/actor_450900_animation_01B50_records.inc"
};

u16 D_actor_450900_80133948[20] = {
#include "assets/actor_450900_animation_01B50_indices.inc"
};

AnimationSet D_actor_450900_80133970 = {
    D_actor_450900_80133804,
    D_actor_450900_80133948,
    { NULL, D_actor_450900_8013377C, NULL, NULL, D_actor_450900_80133794, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80133998[2] = {
#include "assets/actor_450900_animation_01D04_bank1.inc"
};

AnimationPackedRotation D_actor_450900_801339B0[21] = {
#include "assets/actor_450900_animation_01D04_bank4.inc"
};

AnimationRecord D_actor_450900_80133A04[62] = {
#include "assets/actor_450900_animation_01D04_records.inc"
};

u16 D_actor_450900_80133AFC[20] = {
#include "assets/actor_450900_animation_01D04_indices.inc"
};

AnimationSet D_actor_450900_80133B24 = {
    D_actor_450900_80133A04,
    D_actor_450900_80133AFC,
    { NULL, D_actor_450900_80133998, NULL, NULL, D_actor_450900_801339B0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80133B4C[5] = {
#include "assets/actor_450900_animation_020EC_bank1.inc"
};

AnimationPackedRotation D_actor_450900_80133B88[67] = {
#include "assets/actor_450900_animation_020EC_bank4.inc"
};

AnimationRecord D_actor_450900_80133C94[148] = {
#include "assets/actor_450900_animation_020EC_records.inc"
};

u16 D_actor_450900_80133EE4[20] = {
#include "assets/actor_450900_animation_020EC_indices.inc"
};

AnimationSet D_actor_450900_80133F0C = {
    D_actor_450900_80133C94,
    D_actor_450900_80133EE4,
    { NULL, D_actor_450900_80133B4C, NULL, NULL, D_actor_450900_80133B88, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80133F34[3] = {
#include "assets/actor_450900_animation_02428_bank1.inc"
};

AnimationPackedRotation D_actor_450900_80133F58[53] = {
#include "assets/actor_450900_animation_02428_bank4.inc"
};

AnimationRecord D_actor_450900_8013402C[125] = {
#include "assets/actor_450900_animation_02428_records.inc"
};

u16 D_actor_450900_80134220[20] = {
#include "assets/actor_450900_animation_02428_indices.inc"
};

AnimationSet D_actor_450900_80134248 = {
    D_actor_450900_8013402C,
    D_actor_450900_80134220,
    { NULL, D_actor_450900_80133F34, NULL, NULL, D_actor_450900_80133F58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80134270[2] = {
#include "assets/actor_450900_animation_025F0_bank1.inc"
};

AnimationPackedRotation D_actor_450900_80134288[23] = {
#include "assets/actor_450900_animation_025F0_bank4.inc"
};

AnimationRecord D_actor_450900_801342E4[65] = {
#include "assets/actor_450900_animation_025F0_records.inc"
};

u16 D_actor_450900_801343E8[20] = {
#include "assets/actor_450900_animation_025F0_indices.inc"
};

AnimationSet D_actor_450900_80134410 = {
    D_actor_450900_801342E4,
    D_actor_450900_801343E8,
    { NULL, D_actor_450900_80134270, NULL, NULL, D_actor_450900_80134288, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80134438[6] = {
#include "assets/actor_450900_animation_02A30_bank1.inc"
};

AnimationPackedRotation D_actor_450900_80134480[89] = {
#include "assets/actor_450900_animation_02A30_bank4.inc"
};

AnimationRecord D_actor_450900_801345E4[145] = {
#include "assets/actor_450900_animation_02A30_records.inc"
};

u16 D_actor_450900_80134828[20] = {
#include "assets/actor_450900_animation_02A30_indices.inc"
};

AnimationSet D_actor_450900_80134850 = {
    D_actor_450900_801345E4,
    D_actor_450900_80134828,
    { NULL, D_actor_450900_80134438, NULL, NULL, D_actor_450900_80134480, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80134878[4] = {
#include "assets/actor_450900_animation_030F4_bank1.inc"
};

AnimationPackedRotation D_actor_450900_801348A8[151] = {
#include "assets/actor_450900_animation_030F4_bank4.inc"
};

AnimationRecord D_actor_450900_80134B04[250] = {
#include "assets/actor_450900_animation_030F4_records.inc"
};

u16 D_actor_450900_80134EEC[20] = {
#include "assets/actor_450900_animation_030F4_indices.inc"
};

AnimationSet D_actor_450900_80134F14 = {
    D_actor_450900_80134B04,
    D_actor_450900_80134EEC,
    { NULL, D_actor_450900_80134878, NULL, NULL, D_actor_450900_801348A8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80134F3C[3] = {
#include "assets/actor_450900_animation_03338_bank1.inc"
};

AnimationPackedRotation D_actor_450900_80134F60[28] = {
#include "assets/actor_450900_animation_03338_bank4.inc"
};

AnimationRecord D_actor_450900_80134FD0[88] = {
#include "assets/actor_450900_animation_03338_records.inc"
};

u16 D_actor_450900_80135130[20] = {
#include "assets/actor_450900_animation_03338_indices.inc"
};

AnimationSet D_actor_450900_80135158 = {
    D_actor_450900_80134FD0,
    D_actor_450900_80135130,
    { NULL, D_actor_450900_80134F3C, NULL, NULL, D_actor_450900_80134F60, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80135180[10] = {
#include "assets/actor_450900_animation_03738_bank1.inc"
};

AnimationPackedRotation D_actor_450900_801351F8[85] = {
#include "assets/actor_450900_animation_03738_bank4.inc"
};

AnimationRecord D_actor_450900_8013534C[121] = {
#include "assets/actor_450900_animation_03738_records.inc"
};

u16 D_actor_450900_80135530[20] = {
#include "assets/actor_450900_animation_03738_indices.inc"
};

AnimationSet D_actor_450900_80135558 = {
    D_actor_450900_8013534C,
    D_actor_450900_80135530,
    { NULL, D_actor_450900_80135180, NULL, NULL, D_actor_450900_801351F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80135580[4] = {
#include "assets/actor_450900_animation_03934_bank1.inc"
};

AnimationPackedRotation D_actor_450900_801355B0[17] = {
#include "assets/actor_450900_animation_03934_bank4.inc"
};

AnimationRecord D_actor_450900_801355F4[78] = {
#include "assets/actor_450900_animation_03934_records.inc"
};

u16 D_actor_450900_8013572C[20] = {
#include "assets/actor_450900_animation_03934_indices.inc"
};

AnimationSet D_actor_450900_80135754 = {
    D_actor_450900_801355F4,
    D_actor_450900_8013572C,
    { NULL, D_actor_450900_80135580, NULL, NULL, D_actor_450900_801355B0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_8013577C[8] = {
#include "assets/actor_450900_animation_03C70_bank1.inc"
};

AnimationPackedRotation D_actor_450900_801357DC[65] = {
#include "assets/actor_450900_animation_03C70_bank4.inc"
};

AnimationRecord D_actor_450900_801358E0[98] = {
#include "assets/actor_450900_animation_03C70_records.inc"
};

u16 D_actor_450900_80135A68[20] = {
#include "assets/actor_450900_animation_03C70_indices.inc"
};

AnimationSet D_actor_450900_80135A90 = {
    D_actor_450900_801358E0,
    D_actor_450900_80135A68,
    { NULL, D_actor_450900_8013577C, NULL, NULL, D_actor_450900_801357DC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_450900_80135AB8[3] = {
#include "assets/actor_450900_animation_04028_bank1.inc"
};

AnimationPackedRotation D_actor_450900_80135ADC[81] = {
#include "assets/actor_450900_animation_04028_bank4.inc"
};

AnimationRecord D_actor_450900_80135C20[128] = {
#include "assets/actor_450900_animation_04028_records.inc"
};

u16 D_actor_450900_80135E20[20] = {
#include "assets/actor_450900_animation_04028_indices.inc"
};

AnimationSet D_actor_450900_80135E48 = {
    D_actor_450900_80135C20,
    D_actor_450900_80135E20,
    { NULL, D_actor_450900_80135AB8, NULL, NULL, D_actor_450900_80135ADC, NULL, NULL, NULL },
};

s8 D_actor_450900_80135E70 = 0;

s32 D_actor_450900_80135E74 = 0;

void func_actor_450900_80131E38(Task*);
void func_actor_450900_8013207C(Task*);
void func_actor_450900_8013223C(Task*);
void func_actor_450900_8013235C(Task*);
void func_actor_450900_80132548(Task*);

TaskDesc D_actor_450900_80135E78[6] = {
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_450900_80131E38, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_450900_8013207C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_450900_8013223C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_450900_8013235C, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_actor_450900_80132548, { .value = 0 } },
};

Actor450900AnimStorage5EC0 D_actor_450900_80135EC0 = { .data = { { &D_actor_450900_80133F0C, &D_actor_450900_80134248, &D_actor_450900_80134410, &D_actor_450900_80134850, &D_actor_450900_80134F14, &D_actor_450900_80135E48, &D_actor_450900_80135158, &D_actor_450900_80135558, &D_actor_450900_80135754, &D_actor_450900_80135A90, &D_actor_450900_80132DA0, &D_actor_450900_801330F8, &D_actor_450900_8013358C, &D_actor_450900_80133754, &D_actor_450900_80133970, &D_actor_450900_80133B24 }, { { { .words = &D_actor_450900_80135EC0.words[10] }, 32 }, { { .words = D_actor_450900_80135EC0.words }, 32 } }, { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_450900_80135F74 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_80135F88[5] = {
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_actor_450900_80135FEC = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 12, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450900_80136000 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_80136014 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450900_80136028 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_8013603C = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_80136050 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_80136064 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_80136078 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450900_8013608C = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450900_801360A0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_801360B4 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_450900_801360C8 = { { 3450, 0, 6378, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450900_801360E0 = { { 3300, 0, 6378, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_450900_801360F8 = { { 750, 0, 3710, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_actor_450900_80136110[21] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450900_80135EC0.data.copies[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450900_80135EC0.data.copies }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450900_801360F8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_growth_room_8017D82C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_801360A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136014 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_801360A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450900_801360C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136078 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_450900_80136308[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450900_801360C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450900_801360F8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136078 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

ActorTransform D_actor_450900_80136458 = { { 0, 0, 0, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_actor_450900_80136470[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_450900_801360C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450900_80135EC0.data.copies[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450900_80135EC0.data.copies }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450900_80132724 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_450900_80136458 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1018 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_801360A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1018 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_450900_80136680[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450900_80132724 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_450900_80136458 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1018 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80135EC0.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132684 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_801360B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1018 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_450900_80136890[26] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_growth_room_8017D82C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450900_80135EC0.data.copies[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450900_80135EC0.data.copies }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450900_80132724 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_450900_80136458 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1018 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80135F74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_450900_801360E0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1018 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_450900_80136B00[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_450900_80135EC0.data.copies[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136028 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_8013603C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136050 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_450900_80136BD8[8] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55170001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 900 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

s32 D_actor_450900_80136C98;

Task* D_actor_450900_80136C9C;

static void func_actor_450900_801327A8(void);
static void func_actor_450900_80132834(void);

/// State handler that runs the save-point capture. State 0 spawns the capture
/// task `func_actor_450900_80132548` into `D_actor_450900_80136C9C`; state 1
/// waits for `D_8017A99C`, the AI tick counter, to pass 0x30C with save data in
/// the slot, then arms the flag `func_actor_450900_80132518` toggles and, on
/// every 210th tick, plays the ally's voice cue at its own pan and depth and
/// posts the `0x3F7` / `0x3E8` / `0x3F9` messages to the slot-0xA task; 0x3C
/// ticks later it posts `0x3E8` alone, with the capture-indicator animation.
/// The one-shot `D_actor_450900_80135E74` retires the handler after one pass.
void func_actor_450900_80131E38(Task* task)
{
    GfxCoord* coord;
    s32       state;
    s32       t;
    s8        pan;
    s8        depth;
    Task*     companionTask;

    companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    state         = task->state;
    switch (state) {
        case 0:
            D_actor_450900_80135E70 = 0;
            D_actor_450900_80136C9C = Task_SpawnFromTable(D_actor_450900_80135E78, 5, 0, 0);
            task->state             = task->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (gGameSession->eventState != 0) {
                break;
            }
            if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
                break;
            }
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0xB) {
                D_8017A99C = D_8017A99C + 1;
            }
            t = D_8017A99C - 0x30C;
            if (D_actor_450900_80135E74 == 0 && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp > 0 && t >= 0) {
                D_actor_450900_80135E70 = state;
                if (t % 210 == 0) {
                    coord = (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION))->extra.tmd->coords;
                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    depth = (s8)worldCoordGetOriginAudioDepth(coord);
                    if (rand() & 1) {
                        SndEvt_EnqueueType6(0x55170005, pan, depth);
                    } else {
                        SndEvt_EnqueueType6(0x55170006, pan, depth);
                    }
                    Gp_AllyAnimId(&Actor450900AllyAnim.source.index);
                    Gp_DispatchMsgPtr(companionTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &D_actor_450900_80135EC0.data.copies[0], 0);
                    Gp_DispatchMsgPtr(companionTask, ANIMATION_MESSAGE_PLAY, &Actor450900AllyAnim, 0);
                    Gp_DispatchMsg(companionTask, 0x3F9, 0x40010, 0);
                } else if (t % 210 == 0x3C) {
                    Gp_AllyAnimId(&D_actor_450900_801360B4.source.index);
                    Gp_DispatchMsgPtr(companionTask, ANIMATION_MESSAGE_PLAY, &D_actor_450900_801360B4, 0);
                }
            }
            break;
    }
}

void func_actor_450900_8013207C(Task* task)
{
    GfxCoord* coord;
    Task*     slot;
    s32       value;
    s8        pan;
    s8        depth;

    slot  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    value = task->state;
    switch (value) {
        case 0:
            task->killCountdown = 0;
            task->state         = task->state + 1;
            return;
        case 1:
            if ((Gp_CapBusy() == 0) && (gGameSession->eventState == 0) && (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) && ((D_8017A99C - 0x456) >= 0)) {
                if ((D_8017A99C - 0x456) % 210 == 0) {
                    coord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    depth = (s8)worldCoordGetOriginAudioDepth(coord);
                    if (rand() & 1) {
                        SndEvt_EnqueueType6(0x55170003, pan, depth);
                    } else {
                        SndEvt_EnqueueType6(0x55170004, pan, depth);
                    }
                    Gp_DispatchMsgPtr(slot, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &D_actor_450900_80135EC0.data.copies[1], 0);
                    Gp_PlayerWeaponId(&D_actor_450900_80135FEC.source.index);
                    Gp_DispatchMsgPtr(slot, ANIMATION_MESSAGE_PLAY, &D_actor_450900_80135FEC, 0);
                } else if ((D_8017A99C - 0x456) % 210 == 0x46) {
                    value = D_actor_450900_80136C98;
                    value++;
                    D_actor_450900_80136C98 = value;
                    SCHED_BARRIER();
                    Gp_DispatchMsg(slot, 0x3F1, 0, 0);
                }
            }
            return;
    }
}

void func_actor_450900_8013223C(Task* task)
{
    switch (task->state) {
        case 0:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_RunCapCmd(1, 0);
            task->state = task->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xB) {
                goto kill;
            }
            GameFlag_SetNibble(0xD8, 1);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_RunCapCmd(2, 0);
            func_800E8614(D_actor_450900_80136B00, 0);
            task->state = task->state + 1;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
            kill:
                Gp_MsgPlayerWeapon(1);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
            }
            break;
    }
}

void func_actor_450900_8013235C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StartCapSlot(0xB, 1, 1);
            task->state = task->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xB) {
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
            } else {
                func_800E8614(D_actor_450900_80136BD8, 0);
                task->state = task->state + 1;
            }
            break;
        case 3:
            if (gGameSession->eventState == 2) {
                task->state = task->state + 1;
            }
            break;
        case 4:
            GameFlag_SetNibble(0x4D, 0);
            GameFlag_SetNibble(0xFC, 1);
            GameFlag_SetNibble(0xA5, 0);
            GameFlag_SetNibble(0xD9, 0);
            GameFlag_SetNibble(0xAB, 1);
            GameFlag_SetNibble(0x1C7, 0);
            GameFlag_SetNibble(0xD2, 0);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 8);
            SndEvt_EnqueueType7(0x80000000, 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = 0xF;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 3;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType     = 0;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 1;
            gDisplayState.spriteVariant                                = 1;
            Task_Spawn(0, 0x11, 0, 0);
            Gp_RestoreStreamRng();
            taskKill(task);
            break;
    }
}

/// Script callback: arms or disarms the save-point capture task's flag
/// (`Task::spawnArg1`, the value `func_actor_450900_80132548` tests to decide
/// which way the capture cursor sweeps).
void func_actor_450900_80132518(s32 arg0)
{
    if (D_actor_450900_80136C9C != NULL) {
        if (arg0 == 1) {
            D_actor_450900_80136C9C->spawnArg1.value = 0;
            return;
        }
        D_actor_450900_80136C9C->spawnArg1.value = 1;
    }
}

/// State handler of the save-point capture task `func_actor_450900_80131E38`
/// spawns. State 0 allocates the head-aim record the capture cursor sweeps with
/// (`memCalloc(0xC, false)` into `Task::work`); state 1 ramps its `rate` one
/// 0x200 step per frame, up or down according to `Task::spawnArg1` (the flag
/// `func_actor_450900_80132518` arms), and hands the record to `func_800B17D4`
/// between the slot-3 task and the ally's own slot-0xA task. Any other state
/// kills the task and drops the overlay's handle to it.
void func_actor_450900_80132548(Task* task)
{
    GpHeadAim* aim;
    Task*      playerTask;
    u16        rate;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (task->state) {
        case 0:
            aim = memCalloc(sizeof(GpHeadAim), false);
            if (aim == NULL) {
                taskKill(task);
                return;
            }
            task->work      = aim;
            aim->yawLimit   = 0x100;
            aim->pitchLimit = 0x200;
            task->state++;
            /* fallthrough */
        case 1:
            aim = (GpHeadAim*)task->work;
            if (task->spawnArg1.value != 0) {
                rate      = aim->rate + 0x200;
                aim->rate = rate;
                if ((s16)rate >= 0x1001) {
                    aim->rate = 0x1000;
                }
            } else {
                rate      = aim->rate - 0x200;
                aim->rate = rate;
                if ((s16)rate < 0) {
                    aim->rate = 0;
                }
            }
            func_800B17D4(playerTask, gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), aim);
            return;
        default:
            taskKill(task);
            D_actor_450900_80136C9C = NULL;
            return;
    }
}

void func_actor_450900_80132678(u8 arg0)
{
    gSceneCombatState.actorControl = arg0;
}

/// Plays the ally's voice cue at its own pan and depth: `arg0` picks the
/// non-random id, otherwise one of the two `0x55170005/6` takes is chosen.
void func_actor_450900_80132684(s32 arg0)
{
    GfxCoord* coord;
    s8        pan;
    s8        depth;

    coord = (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION))->extra.tmd->coords;
    pan   = (s8)worldCoordGetOriginAudioPan(coord);
    depth = (s8)worldCoordGetOriginAudioDepth(coord);
    if (arg0 != 0) {
        SndEvt_EnqueueType6(0x55170007, pan, depth);
    } else if (rand() & 1) {
        SndEvt_EnqueueType6(0x55170005, pan, depth);
    } else {
        SndEvt_EnqueueType6(0x55170006, pan, depth);
    }
}

/// Script callback (opcode 0xD): refreshes the root coordinates of the slot-0xA
/// and slot-3 tasks and stores the 12-bit `ratan2` heading from the slot-3
/// object to the slot-0xA object in `D_actor_450900_80136458.rot.vy`, the halfword at
/// offset 0x12 of the `D_actor_450900_80136458` block the same scripts then post
/// with message `0x3EE`.
void func_actor_450900_80132724(void)
{
    GfxCoord* target;
    GfxCoord* origin;

    target = (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION))->extra.tmd->coords;
    origin = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    Gp_UpdateCoord(target);
    Gp_UpdateCoord(origin);
    D_actor_450900_80136458.rot.vy =
        ratan2(target->coord.t[0] - origin->coord.t[0], target->coord.t[2] - origin->coord.t[2]) & 0xFFF;
}

/// Spawns the ally's save-point state handler. Once flag 0xD8 is set (the
/// capture ran) the one-shot `D_actor_450900_80135E74` swaps the ally onto the
/// `D_actor_450900_80136890` handler the first time through, and every later
/// call just re-arms the idle capture. Before the flag is set the handler is
/// picked by the AI tick counter `D_8017A99C`: the low-traffic
/// `D_actor_450900_80136470` below 0x30C, `D_actor_450900_80136680` at or above
/// it. The three calls are written out at each site - the `jal` is shared only
/// because `jump.c` cross-jumps the identical tails.
static void func_actor_450900_801327A8(void)
{
    if (GameFlag_GetNibble(0xD8) != 0) {
        if (D_actor_450900_80135E74 == 0) {
            D_actor_450900_80135E74 = 1;
            func_800E8614(D_actor_450900_80136890, 0);
        } else {
            Gp_SpawnIfCapIdle(0xC, 1);
        }
    } else if (D_8017A99C < 0x30C) {
        func_800E8614(D_actor_450900_80136470, 0);
    } else {
        func_800E8614(D_actor_450900_80136680, 0);
    }
}

/// Reads the root coordinate of the slot-0xA task's `TmdObject` (reached through
/// `Task::extra`, as `func_actor_450900_80132684` does) and, when its world Z is
/// below -0x76C, spawns entry 4 of `D_actor_450900_80135E78`
/// (`func_actor_450900_8013235C`); otherwise it starts capture slot 0xB with
/// `Gp_StartCapSlot`.
static void func_actor_450900_80132834(void)
{
    GfxCoord* coord;

    coord = (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION))->extra.tmd->coords;
    if (coord->coord.t[2] < -0x76C) {
        Task_SpawnFromTable(D_actor_450900_80135E78, 4, 0, 0);
    } else {
        Gp_StartCapSlot(0xB, 1, 0);
    }
}
