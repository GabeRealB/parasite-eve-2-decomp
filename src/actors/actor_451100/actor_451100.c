#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[4];
        GpCopyArg            copy;
        AnimationPlayRequest arguments[5];
        GpEvsCmd             commands[6];
    } data;
    s32 words[67];
} Actor451100AnimStorage510C;
STATIC_ASSERT_SIZEOF(Actor451100AnimStorage510C, 268);

extern Actor451100AnimStorage510C D_actor_451100_8013510C;

/// Work block of the actor `func_actor_451100_801322D4` dispatches, published
/// by its spawn handler so the actor's message handlers and animation helpers
/// reach it without the task.
extern Actor260500Work* D_actor_451100_8014E744;

/// That same actor's task, stored by its spawn handler for the handlers that
/// need the task but are not given it.
extern Task* D_actor_451100_8014E748;

/// Reset argument the first actor forwards to every reseeded slot.
extern s16 D_actor_451100_8013F700;

/// Picks the distance `func_actor_451100_80131F84` walks the model each frame:
/// 0 steps 0x3C forward, 1 steps 0xF back, 2 steps 0x19 forward.
extern s16 D_actor_451100_8014E74C;

extern AnimationSet* D_actor_451100_8013F740[37];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*);
        s32 (*call2)(Task*, s32, ActorCommand* request);
        s32 (*call3)(Task*, s32, ActorTransform*);
        s32 (*call4)(Task*, s32, VECTOR*);
        s32 (*call5)(Task*, s32, VECTOR*, s32);
        s32 (*call6)(Task*, s32, s32);
    } handler;
} Actor451100MsgEntry;
STATIC_ASSERT_SIZEOF(Actor451100MsgEntry, 8);

extern Actor451100MsgEntry D_actor_451100_8013F704[];
extern TaskDesc            D_actor_451100_8014E6E4[];
extern Actor451100MsgEntry D_actor_451100_8014E6B4[];
extern u8                  D_actor_451100_8014E6FC[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void func_actor_451100_80131F84(Task* task);
static void func_actor_451100_80132330(GpEnemy* enemy, Task* task);
static void func_actor_451100_801323B4(Task* task);
static void func_actor_451100_801323DC(void);
static void func_actor_451100_80132428(void);
static void func_actor_451100_801324B8(void);
static void func_actor_451100_8013280C(Task* task);
static void func_actor_451100_80132A1C(Task* task);
static void func_actor_451100_80132C28(GpEnemy* enemy, Task* task);
static void func_actor_451100_80132CAC(Task* task);
static void func_actor_451100_80132CD4(Task* task);
static void func_actor_451100_80132D70(Task* task);
static void func_actor_451100_80132DBC(Task* task);
static void func_actor_451100_80132E34(Task* task);

extern TmdSource D_actor_451100_80146060;
extern TmdSource D_actor_451100_80146318;
void             func_actor_451100_80132BD4(Task*);
void             func_actor_451100_801330B0(Task*);

s32 func_actor_451100_80132E98(Task*, s32, AnimationPlayRequest*);
s32 func_actor_451100_80132F04(Task*, s32, s32);
s32 func_actor_451100_80132F68(Task*, s32, ActorTransform* placement);
s32 func_actor_451100_80132FE0(void);
s32 func_actor_451100_80132FE8(Task*, s32, VECTOR*);

s32  func_actor_451100_80132538(Task*, s32, AnimationPlayRequest*);
s32  func_actor_451100_801325C8(Task*, s32, s32);
s32  func_actor_451100_80132610(Task*, s32, ActorTransform* placement);
s32  func_actor_451100_8013268C(Task*, s32, ActorCommand* msg);
s32  func_actor_451100_801326B0(Task*, s32, VECTOR*, s32);
void func_actor_451100_801322D4(Task*);

extern AnimationPlayRequest D_actor_451100_80134D98;
extern AnimationPlayRequest D_actor_451100_80134DAC;
extern AnimationPlayRequest D_actor_451100_80134DC0;
extern AnimationPlayRequest D_actor_451100_80134DD4;
extern AnimationPlayRequest D_actor_451100_80134DE8;
extern AnimationPlayRequest D_actor_451100_80134DFC;
extern AnimationPlayRequest D_actor_451100_80134E10;
extern AnimationPlayRequest D_actor_451100_80134E24;
extern AnimationPlayRequest D_actor_451100_80134E38;
extern AnimationPlayRequest D_actor_451100_80134E4C;
extern AnimationPlayRequest D_actor_451100_80134E60;
extern AnimationPlayRequest D_actor_451100_80134E74;
extern AnimationPlayRequest D_actor_451100_80134E88;
extern AnimationPlayRequest D_actor_451100_80134E9C;
extern AnimationPlayRequest D_actor_451100_80134EB0;
extern AnimationPlayRequest D_actor_451100_80134EC4;
extern AnimationPlayRequest D_actor_451100_80134ED8;
extern AnimationPlayRequest D_actor_451100_80134EEC;
extern AnimationPlayRequest D_actor_451100_80134F00;
extern AnimationPlayRequest D_actor_451100_80134F14;
extern AnimationPlayRequest D_actor_451100_80134F28;
extern AnimationPlayRequest D_actor_451100_80134F3C;
extern AnimationPlayRequest D_actor_451100_80134F50;
extern AnimationPlayRequest D_actor_451100_80134F64;
extern AnimationPlayRequest D_actor_451100_80134F78;
extern AnimationPlayRequest D_actor_451100_80134F8C;
extern AnimationPlayRequest D_actor_451100_80134FA0;
extern AnimationPlayRequest D_actor_451100_80134FB4;
extern AnimationPlayRequest D_actor_451100_80134FC8;
extern AnimationPlayRequest D_actor_451100_80134FDC;
extern AnimationPlayRequest D_actor_451100_80134FF0;
extern AnimationPlayRequest D_actor_451100_80135004;
extern AnimationPlayRequest D_actor_451100_80135018;
extern AnimationPlayRequest D_actor_451100_8013502C;
extern AnimationPlayRequest D_actor_451100_801350D0;
extern AnimationPlayRequest D_actor_451100_801350E4;
extern AnimationPlayRequest D_actor_451100_801350F8;
extern ActorTransform       D_actor_451100_80135040;
extern ActorTransform       D_actor_451100_80135058;
extern ActorTransform       D_actor_451100_80135070;
extern ActorTransform       D_actor_451100_80135088;
extern ActorTransform       D_actor_451100_801350A0;
extern ActorTransform       D_actor_451100_801350B8;

extern Actor451100AnimStorage510C D_actor_451100_8013510C;

AnimationPackedPose D_actor_451100_80133120[6] = {
#include "assets/actor_451100_animation_015DC_bank1.inc"
};

AnimationPackedRotation D_actor_451100_80133168[46] = {
#include "assets/actor_451100_animation_015DC_bank4.inc"
};

AnimationRecord D_actor_451100_80133220[109] = {
#include "assets/actor_451100_animation_015DC_records.inc"
};

u16 D_actor_451100_801333D4[20] = {
#include "assets/actor_451100_animation_015DC_indices.inc"
};

AnimationSet D_actor_451100_801333FC = {
    D_actor_451100_80133220,
    D_actor_451100_801333D4,
    { NULL, D_actor_451100_80133120, NULL, NULL, D_actor_451100_80133168, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_80133424[10] = {
#include "assets/actor_451100_animation_01CD8_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013349C[147] = {
#include "assets/actor_451100_animation_01CD8_bank4.inc"
};

AnimationRecord D_actor_451100_801336E8[250] = {
#include "assets/actor_451100_animation_01CD8_records.inc"
};

u16 D_actor_451100_80133AD0[20] = {
#include "assets/actor_451100_animation_01CD8_indices.inc"
};

AnimationSet D_actor_451100_80133AF8 = {
    D_actor_451100_801336E8,
    D_actor_451100_80133AD0,
    { NULL, D_actor_451100_80133424, NULL, NULL, D_actor_451100_8013349C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_80133B20[4] = {
#include "assets/actor_451100_animation_01F50_bank1.inc"
};

AnimationPackedRotation D_actor_451100_80133B50[51] = {
#include "assets/actor_451100_animation_01F50_bank4.inc"
};

AnimationRecord D_actor_451100_80133C1C[75] = {
#include "assets/actor_451100_animation_01F50_records.inc"
};

u16 D_actor_451100_80133D48[20] = {
#include "assets/actor_451100_animation_01F50_indices.inc"
};

AnimationSet D_actor_451100_80133D70 = {
    D_actor_451100_80133C1C,
    D_actor_451100_80133D48,
    { NULL, D_actor_451100_80133B20, NULL, NULL, D_actor_451100_80133B50, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_80133D98[44] = {
#include "assets/actor_451100_animation_02F50_bank1.inc"
};

AnimationPackedRotation D_actor_451100_80133FA8[370] = {
#include "assets/actor_451100_animation_02F50_bank4.inc"
};

AnimationRecord D_actor_451100_80134570[502] = {
#include "assets/actor_451100_animation_02F50_records.inc"
};

u16 D_actor_451100_80134D48[20] = {
#include "assets/actor_451100_animation_02F50_indices.inc"
};

AnimationSet D_actor_451100_80134D70 = {
    D_actor_451100_80134570,
    D_actor_451100_80134D48,
    { NULL, D_actor_451100_80133D98, NULL, NULL, D_actor_451100_80133FA8, NULL, NULL, NULL },
};

AnimationPlayRequest D_actor_451100_80134D98 = { { .index = 0 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134DAC = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134DC0 = { { .index = 0 }, 20, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134DD4 = { { .index = 0 }, 21, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134DE8 = { { .index = 0 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134DFC = { { .index = 0 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E10 = { { .index = 0 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E24 = { { .index = 0 }, 25, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E38 = { { .index = 0 }, 26, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E4C = { { .index = 0 }, 27, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E60 = { { .index = 0 }, 28, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E74 = { { .index = 0 }, 29, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E88 = { { .index = 0 }, 30, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134E9C = { { .index = 0 }, 31, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134EB0 = { { .index = 0 }, 32, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134EC4 = { { .index = 0 }, 33, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134ED8 = { { .index = 0 }, 34, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134EEC = { { .index = 0 }, 35, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F00 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F14 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F28 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F3C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F50 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F64 = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F78 = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134F8C = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134FA0 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134FB4 = { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134FC8 = { { .index = 0 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134FDC = { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80134FF0 = { { .index = 0 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80135004 = { { .index = 0 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_80135018 = { { .index = 0 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_451100_8013502C = { { .index = 0 }, 16, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_451100_80135040 = { { -1500, 3000, 0, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_451100_80135058 = { { -1500, 3000, 0, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_451100_80135070 = { { 1500, 3000, 0, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_451100_80135088 = { { 1000, 3000, 200, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_451100_801350A0 = { { 1500, 3000, 0, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_451100_801350B8 = { { 1000, 3000, 200, 0 }, { 0, 1024, 0, 0 } };

AnimationPlayRequest D_actor_451100_801350D0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_451100_801350E4 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_451100_801350F8 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

Actor451100AnimStorage510C D_actor_451100_8013510C = { .data = { { &D_actor_451100_80134D70, &D_actor_451100_801333FC, &D_actor_451100_80133AF8, &D_actor_451100_80133D70 }, { { .words = D_actor_451100_8013510C.words }, 32 }, { { { .index = 1 }, 47, 1, 8, 1 }, { { .index = 1 }, 48, 1, 8, 1 }, { { .index = 1 }, 48, 0, 0, 1 }, { { .index = 1 }, 49, 1, 8, 1 }, { { .index = 1 }, 50, 1, 8, 1 } }, { { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134FC8 }, { .value = 0 } }, { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134E88 }, { .value = 0 } }, { 4, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } }, { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134E9C }, { .value = 0 } }, { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134FDC }, { .value = 0 } }, { 45, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } } } } };

GpOverlayIds D_actor_451100_80135218 = { 5, 11, 11 };

GpEvsCmd D_actor_451100_80135220[146] = {
    { 12, { .overlays = &D_actor_451100_80135218 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_451100_8013510C.data.copy }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350D0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_451100_80135040 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_451100_80135070 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_451100_801350A0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134D98 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DAC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350E4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134E60 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134FA0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134E74 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134FB4 }, { .value = 0 } },
    { 4, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DC0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F00 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350F8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DD4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F14 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DC0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F00 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F28 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DE8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.arguments[1] }, { .value = 0 } },
    { 44, { .commands = D_actor_451100_8013510C.data.commands }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_451100_8013510C.data.commands }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_451100_8013510C.data.commands }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_451100_8013510C.data.commands }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_451100_8013510C.data.commands }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_451100_8013510C.data.commands }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134FF0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134EB0 }, { .value = 0 } },
    { 4, { .value = 61 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134E9C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134FDC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F3C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DFC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.arguments[1] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134E10 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F50 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DC0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F00 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DD4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F14 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DC0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F00 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134E10 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F50 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DC0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F00 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DD4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F14 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DC0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F00 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134E10 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F50 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DC0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F00 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134EEC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_8013502C }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DC0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F00 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DD4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F14 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134DC0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F00 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.arguments[4] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134EC4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80135004 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134E24 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F64 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_451100_80135058 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_451100_80135088 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_451100_801350B8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134ED8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80135018 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350E4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134E38 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F78 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_451100_80134E4C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_451100_80134F8C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_451100_80135040 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.arguments[2] }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_8013510C.data.arguments[0] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 34, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350D0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_451100_80135FD0[13] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_451100_80135040 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350D0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_451100_80136108[7] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 15, { .value = 0x550C0004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_451100_801350D0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_451100_801361B0[19] = {
#include "assets/actor_451100_model_09B44_skeleton.inc"
};

u32 D_actor_451100_8013645C[19] = {
#include "assets/actor_451100_model_09B44_partVerts.inc"
};

SVECTOR D_actor_451100_801364A8[365] = {
#include "assets/actor_451100_model_09B44_verts.inc"
};

SVECTOR D_actor_451100_80137010[385] = {
#include "assets/actor_451100_model_09B44_normals.inc"
};

u32 D_actor_451100_80137C18[3923] = {
#include "assets/actor_451100_model_09B44_stream.inc"
};

TmdSource D_actor_451100_8013B964 = {
    0,
    21760,
    5992,
    19,
    D_actor_451100_8013645C,
    D_actor_451100_801364A8,
    D_actor_451100_80137010,
    D_actor_451100_801361B0,
    D_actor_451100_80137C18,
};

AnimationPackedPose D_actor_451100_8013B988[17] = {
#include "assets/actor_451100_animation_09E1C_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013BA54[16] = {
#include "assets/actor_451100_animation_09E1C_bank4.inc"
};

AnimationRecord D_actor_451100_8013BA94[96] = {
#include "assets/actor_451100_animation_09E1C_records.inc"
};

u16 D_actor_451100_8013BC14[20] = {
#include "assets/actor_451100_animation_09E1C_indices.inc"
};

AnimationSet D_actor_451100_8013BC3C = {
    D_actor_451100_8013BA94,
    D_actor_451100_8013BC14,
    { NULL, D_actor_451100_8013B988, NULL, NULL, D_actor_451100_8013BA54, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013BC64[66] = {
#include "assets/actor_451100_animation_0A428_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013BF7C[16] = {
#include "assets/actor_451100_animation_0A428_bank4.inc"
};

AnimationRecord D_actor_451100_8013BFBC[153] = {
#include "assets/actor_451100_animation_0A428_records.inc"
};

u16 D_actor_451100_8013C220[20] = {
#include "assets/actor_451100_animation_0A428_indices.inc"
};

AnimationSet D_actor_451100_8013C248 = {
    D_actor_451100_8013BFBC,
    D_actor_451100_8013C220,
    { NULL, D_actor_451100_8013BC64, NULL, NULL, D_actor_451100_8013BF7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013C270[31] = {
#include "assets/actor_451100_animation_0A780_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013C3E4[16] = {
#include "assets/actor_451100_animation_0A780_bank4.inc"
};

AnimationRecord D_actor_451100_8013C424[85] = {
#include "assets/actor_451100_animation_0A780_records.inc"
};

u16 D_actor_451100_8013C578[20] = {
#include "assets/actor_451100_animation_0A780_indices.inc"
};

AnimationSet D_actor_451100_8013C5A0 = {
    D_actor_451100_8013C424,
    D_actor_451100_8013C578,
    { NULL, D_actor_451100_8013C270, NULL, NULL, D_actor_451100_8013C3E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013C5C8[24] = {
#include "assets/actor_451100_animation_0AA68_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013C6E8[16] = {
#include "assets/actor_451100_animation_0AA68_bank4.inc"
};

AnimationRecord D_actor_451100_8013C728[78] = {
#include "assets/actor_451100_animation_0AA68_records.inc"
};

u16 D_actor_451100_8013C860[20] = {
#include "assets/actor_451100_animation_0AA68_indices.inc"
};

AnimationSet D_actor_451100_8013C888 = {
    D_actor_451100_8013C728,
    D_actor_451100_8013C860,
    { NULL, D_actor_451100_8013C5C8, NULL, NULL, D_actor_451100_8013C6E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013C8B0[37] = {
#include "assets/actor_451100_animation_0AE40_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013CA6C[16] = {
#include "assets/actor_451100_animation_0AE40_bank4.inc"
};

AnimationRecord D_actor_451100_8013CAAC[99] = {
#include "assets/actor_451100_animation_0AE40_records.inc"
};

u16 D_actor_451100_8013CC38[20] = {
#include "assets/actor_451100_animation_0AE40_indices.inc"
};

AnimationSet D_actor_451100_8013CC60 = {
    D_actor_451100_8013CAAC,
    D_actor_451100_8013CC38,
    { NULL, D_actor_451100_8013C8B0, NULL, NULL, D_actor_451100_8013CA6C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013CC88[32] = {
#include "assets/actor_451100_animation_0B1A8_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013CE08[16] = {
#include "assets/actor_451100_animation_0B1A8_bank4.inc"
};

AnimationRecord D_actor_451100_8013CE48[86] = {
#include "assets/actor_451100_animation_0B1A8_records.inc"
};

u16 D_actor_451100_8013CFA0[20] = {
#include "assets/actor_451100_animation_0B1A8_indices.inc"
};

AnimationSet D_actor_451100_8013CFC8 = {
    D_actor_451100_8013CE48,
    D_actor_451100_8013CFA0,
    { NULL, D_actor_451100_8013CC88, NULL, NULL, D_actor_451100_8013CE08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013CFF0[31] = {
#include "assets/actor_451100_animation_0B558_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013D164[17] = {
#include "assets/actor_451100_animation_0B558_bank4.inc"
};

AnimationRecord D_actor_451100_8013D1A8[106] = {
#include "assets/actor_451100_animation_0B558_records.inc"
};

u16 D_actor_451100_8013D350[20] = {
#include "assets/actor_451100_animation_0B558_indices.inc"
};

AnimationSet D_actor_451100_8013D378 = {
    D_actor_451100_8013D1A8,
    D_actor_451100_8013D350,
    { NULL, D_actor_451100_8013CFF0, NULL, NULL, D_actor_451100_8013D164, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013D3A0[97] = {
#include "assets/actor_451100_animation_0BCD4_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013D82C[17] = {
#include "assets/actor_451100_animation_0BCD4_bank4.inc"
};

AnimationRecord D_actor_451100_8013D870[151] = {
#include "assets/actor_451100_animation_0BCD4_records.inc"
};

u16 D_actor_451100_8013DACC[20] = {
#include "assets/actor_451100_animation_0BCD4_indices.inc"
};

AnimationSet D_actor_451100_8013DAF4 = {
    D_actor_451100_8013D870,
    D_actor_451100_8013DACC,
    { NULL, D_actor_451100_8013D3A0, NULL, NULL, D_actor_451100_8013D82C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013DB1C[30] = {
#include "assets/actor_451100_animation_0C1E0_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013DC84[69] = {
#include "assets/actor_451100_animation_0C1E0_bank4.inc"
};

AnimationRecord D_actor_451100_8013DD98[144] = {
#include "assets/actor_451100_animation_0C1E0_records.inc"
};

u16 D_actor_451100_8013DFD8[20] = {
#include "assets/actor_451100_animation_0C1E0_indices.inc"
};

AnimationSet D_actor_451100_8013E000 = {
    D_actor_451100_8013DD98,
    D_actor_451100_8013DFD8,
    { NULL, D_actor_451100_8013DB1C, NULL, NULL, D_actor_451100_8013DC84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013E028[17] = {
#include "assets/actor_451100_animation_0C45C_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013E0F4[16] = {
#include "assets/actor_451100_animation_0C45C_bank4.inc"
};

AnimationRecord D_actor_451100_8013E134[72] = {
#include "assets/actor_451100_animation_0C45C_records.inc"
};

u16 D_actor_451100_8013E254[20] = {
#include "assets/actor_451100_animation_0C45C_indices.inc"
};

AnimationSet D_actor_451100_8013E27C = {
    D_actor_451100_8013E134,
    D_actor_451100_8013E254,
    { NULL, D_actor_451100_8013E028, NULL, NULL, D_actor_451100_8013E0F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013E2A4[3] = {
#include "assets/actor_451100_animation_0C600_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013E2C8[16] = {
#include "assets/actor_451100_animation_0C600_bank4.inc"
};

AnimationRecord D_actor_451100_8013E308[60] = {
#include "assets/actor_451100_animation_0C600_records.inc"
};

u16 D_actor_451100_8013E3F8[20] = {
#include "assets/actor_451100_animation_0C600_indices.inc"
};

AnimationSet D_actor_451100_8013E420 = {
    D_actor_451100_8013E308,
    D_actor_451100_8013E3F8,
    { NULL, D_actor_451100_8013E2A4, NULL, NULL, D_actor_451100_8013E2C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013E448[7] = {
#include "assets/actor_451100_animation_0C7F0_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013E49C[16] = {
#include "assets/actor_451100_animation_0C7F0_bank4.inc"
};

AnimationRecord D_actor_451100_8013E4DC[67] = {
#include "assets/actor_451100_animation_0C7F0_records.inc"
};

u16 D_actor_451100_8013E5E8[20] = {
#include "assets/actor_451100_animation_0C7F0_indices.inc"
};

AnimationSet D_actor_451100_8013E610 = {
    D_actor_451100_8013E4DC,
    D_actor_451100_8013E5E8,
    { NULL, D_actor_451100_8013E448, NULL, NULL, D_actor_451100_8013E49C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013E638[5] = {
#include "assets/actor_451100_animation_0C9B8_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013E674[16] = {
#include "assets/actor_451100_animation_0C9B8_bank4.inc"
};

AnimationRecord D_actor_451100_8013E6B4[63] = {
#include "assets/actor_451100_animation_0C9B8_records.inc"
};

u16 D_actor_451100_8013E7B0[20] = {
#include "assets/actor_451100_animation_0C9B8_indices.inc"
};

AnimationSet D_actor_451100_8013E7D8 = {
    D_actor_451100_8013E6B4,
    D_actor_451100_8013E7B0,
    { NULL, D_actor_451100_8013E638, NULL, NULL, D_actor_451100_8013E674, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013E800[71] = {
#include "assets/actor_451100_animation_0D2B4_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013EB54[100] = {
#include "assets/actor_451100_animation_0D2B4_bank4.inc"
};

AnimationRecord D_actor_451100_8013ECE4[242] = {
#include "assets/actor_451100_animation_0D2B4_records.inc"
};

u16 D_actor_451100_8013F0AC[20] = {
#include "assets/actor_451100_animation_0D2B4_indices.inc"
};

AnimationSet D_actor_451100_8013F0D4 = {
    D_actor_451100_8013ECE4,
    D_actor_451100_8013F0AC,
    { NULL, D_actor_451100_8013E800, NULL, NULL, D_actor_451100_8013EB54, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013F0FC[32] = {
#include "assets/actor_451100_animation_0D61C_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013F27C[16] = {
#include "assets/actor_451100_animation_0D61C_bank4.inc"
};

AnimationRecord D_actor_451100_8013F2BC[86] = {
#include "assets/actor_451100_animation_0D61C_records.inc"
};

u16 D_actor_451100_8013F414[20] = {
#include "assets/actor_451100_animation_0D61C_indices.inc"
};

AnimationSet D_actor_451100_8013F43C = {
    D_actor_451100_8013F2BC,
    D_actor_451100_8013F414,
    { NULL, D_actor_451100_8013F0FC, NULL, NULL, D_actor_451100_8013F27C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8013F464[18] = {
#include "assets/actor_451100_animation_0D8B8_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8013F53C[16] = {
#include "assets/actor_451100_animation_0D8B8_bank4.inc"
};

AnimationRecord D_actor_451100_8013F57C[77] = {
#include "assets/actor_451100_animation_0D8B8_records.inc"
};

u16 D_actor_451100_8013F6B0[20] = {
#include "assets/actor_451100_animation_0D8B8_indices.inc"
};

AnimationSet D_actor_451100_8013F6D8 = {
    D_actor_451100_8013F57C,
    D_actor_451100_8013F6B0,
    { NULL, D_actor_451100_8013F464, NULL, NULL, D_actor_451100_8013F53C, NULL, NULL, NULL },
};

s16 D_actor_451100_8013F700 = 8;

Actor451100MsgEntry D_actor_451100_8013F704[6] = {
    { 2003, { .call1 = func_actor_451100_80132538 } },
    { 2005, { .call6 = func_actor_451100_801325C8 } },
    { 2004, { .call3 = func_actor_451100_80132610 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call2 = func_actor_451100_8013268C } },
    { 2013, { .call5 = func_actor_451100_801326B0 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_451100_8013F734 = { 1, 192, func_actor_451100_801322D4, { .model = &D_actor_451100_8013B964 } };

AnimationSet* D_actor_451100_8013F740[37] = {
    NULL,
    &D_actor_451100_8013BC3C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &D_actor_451100_8013BC3C,
    &D_actor_451100_8013C248,
    &D_actor_451100_8013C5A0,
    &D_actor_451100_8013C888,
    &D_actor_451100_8013CC60,
    &D_actor_451100_8013CFC8,
    &D_actor_451100_8013D378,
    &D_actor_451100_8013DAF4,
    &D_actor_451100_8013E000,
    &D_actor_451100_8013E27C,
    &D_actor_451100_8013E420,
    &D_actor_451100_8013E610,
    &D_actor_451100_8013E7D8,
    &D_actor_451100_8013F0D4,
    &D_actor_451100_8013F43C,
    &D_actor_451100_8013F6D8,
    NULL,
};

TmdBone D_actor_451100_8013F7D4[19] = {
#include "assets/actor_451100_model_14240_skeleton.inc"
};

u32 D_actor_451100_8013FA80[19] = {
#include "assets/actor_451100_model_14240_partVerts.inc"
};

SVECTOR D_actor_451100_8013FACC[432] = {
#include "assets/actor_451100_model_14240_verts.inc"
};

SVECTOR D_actor_451100_8014084C[444] = {
#include "assets/actor_451100_model_14240_normals.inc"
};

u32 D_actor_451100_8014162C[4749] = {
#include "assets/actor_451100_model_14240_stream.inc"
};

TmdSource D_actor_451100_80146060 = {
    0,
    26564,
    6624,
    19,
    D_actor_451100_8013FA80,
    D_actor_451100_8013FACC,
    D_actor_451100_8014084C,
    D_actor_451100_8013F7D4,
    D_actor_451100_8014162C,
};

TmdBone D_actor_451100_80146084[1] = {
#include "assets/actor_451100_model_144F8_skeleton.inc"
};

u32 D_actor_451100_801460A8[1] = {
#include "assets/actor_451100_model_144F8_partVerts.inc"
};

SVECTOR D_actor_451100_801460AC[14] = {
#include "assets/actor_451100_model_144F8_verts.inc"
};

SVECTOR D_actor_451100_8014611C[14] = {
#include "assets/actor_451100_model_144F8_normals.inc"
};

u32 D_actor_451100_8014618C[99] = {
#include "assets/actor_451100_model_144F8_stream.inc"
};

TmdSource D_actor_451100_80146318 = {
    0,
    636,
    0,
    1,
    D_actor_451100_801460A8,
    D_actor_451100_801460AC,
    D_actor_451100_8014611C,
    D_actor_451100_80146084,
    D_actor_451100_8014618C,
};

AnimationPackedPose D_actor_451100_8014633C[14] = {
#include "assets/actor_451100_animation_14A28_bank1.inc"
};

AnimationPackedRotation D_actor_451100_801463E4[81] = {
#include "assets/actor_451100_animation_14A28_bank4.inc"
};

AnimationRecord D_actor_451100_80146528[190] = {
#include "assets/actor_451100_animation_14A28_records.inc"
};

u16 D_actor_451100_80146820[20] = {
#include "assets/actor_451100_animation_14A28_indices.inc"
};

AnimationSet D_actor_451100_80146848 = {
    D_actor_451100_80146528,
    D_actor_451100_80146820,
    { NULL, D_actor_451100_8014633C, NULL, NULL, D_actor_451100_801463E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_80146870[46] = {
#include "assets/actor_451100_animation_159B4_bank1.inc"
};

AnimationPackedRotation D_actor_451100_80146A98[255] = {
#include "assets/actor_451100_animation_159B4_bank4.inc"
};

AnimationRecord D_actor_451100_80146E94[582] = {
#include "assets/actor_451100_animation_159B4_records.inc"
};

u16 D_actor_451100_801477AC[20] = {
#include "assets/actor_451100_animation_159B4_indices.inc"
};

AnimationSet D_actor_451100_801477D4 = {
    D_actor_451100_80146E94,
    D_actor_451100_801477AC,
    { NULL, D_actor_451100_80146870, NULL, NULL, D_actor_451100_80146A98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_801477FC[23] = {
#include "assets/actor_451100_animation_16110_bank1.inc"
};

AnimationPackedRotation D_actor_451100_80147910[150] = {
#include "assets/actor_451100_animation_16110_bank4.inc"
};

AnimationRecord D_actor_451100_80147B68[232] = {
#include "assets/actor_451100_animation_16110_records.inc"
};

u16 D_actor_451100_80147F08[20] = {
#include "assets/actor_451100_animation_16110_indices.inc"
};

AnimationSet D_actor_451100_80147F30 = {
    D_actor_451100_80147B68,
    D_actor_451100_80147F08,
    { NULL, D_actor_451100_801477FC, NULL, NULL, D_actor_451100_80147910, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_80147F58[6] = {
#include "assets/actor_451100_animation_166E8_bank1.inc"
};

AnimationPackedRotation D_actor_451100_80147FA0[148] = {
#include "assets/actor_451100_animation_166E8_bank4.inc"
};

AnimationRecord D_actor_451100_801481F0[188] = {
#include "assets/actor_451100_animation_166E8_records.inc"
};

u16 D_actor_451100_801484E0[20] = {
#include "assets/actor_451100_animation_166E8_indices.inc"
};

AnimationSet D_actor_451100_80148508 = {
    D_actor_451100_801481F0,
    D_actor_451100_801484E0,
    { NULL, D_actor_451100_80147F58, NULL, NULL, D_actor_451100_80147FA0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_80148530[7] = {
#include "assets/actor_451100_animation_16F24_bank1.inc"
};

AnimationPackedRotation D_actor_451100_80148584[205] = {
#include "assets/actor_451100_animation_16F24_bank4.inc"
};

AnimationRecord D_actor_451100_801488B8[281] = {
#include "assets/actor_451100_animation_16F24_records.inc"
};

u16 D_actor_451100_80148D1C[20] = {
#include "assets/actor_451100_animation_16F24_indices.inc"
};

AnimationSet D_actor_451100_80148D44 = {
    D_actor_451100_801488B8,
    D_actor_451100_80148D1C,
    { NULL, D_actor_451100_80148530, NULL, NULL, D_actor_451100_80148584, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_80148D6C[32] = {
#include "assets/actor_451100_animation_176C8_bank1.inc"
};

AnimationPackedRotation D_actor_451100_80148EEC[149] = {
#include "assets/actor_451100_animation_176C8_bank4.inc"
};

AnimationRecord D_actor_451100_80149140[224] = {
#include "assets/actor_451100_animation_176C8_records.inc"
};

u16 D_actor_451100_801494C0[20] = {
#include "assets/actor_451100_animation_176C8_indices.inc"
};

AnimationSet D_actor_451100_801494E8 = {
    D_actor_451100_80149140,
    D_actor_451100_801494C0,
    { NULL, D_actor_451100_80148D6C, NULL, NULL, D_actor_451100_80148EEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_80149510[13] = {
#include "assets/actor_451100_animation_17D50_bank1.inc"
};

AnimationPackedRotation D_actor_451100_801495AC[109] = {
#include "assets/actor_451100_animation_17D50_bank4.inc"
};

AnimationRecord D_actor_451100_80149760[250] = {
#include "assets/actor_451100_animation_17D50_records.inc"
};

u16 D_actor_451100_80149B48[20] = {
#include "assets/actor_451100_animation_17D50_indices.inc"
};

AnimationSet D_actor_451100_80149B70 = {
    D_actor_451100_80149760,
    D_actor_451100_80149B48,
    { NULL, D_actor_451100_80149510, NULL, NULL, D_actor_451100_801495AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_80149B98[97] = {
#include "assets/actor_451100_animation_1924C_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8014A024[380] = {
#include "assets/actor_451100_animation_1924C_bank4.inc"
};

AnimationRecord D_actor_451100_8014A614[652] = {
#include "assets/actor_451100_animation_1924C_records.inc"
};

u16 D_actor_451100_8014B044[20] = {
#include "assets/actor_451100_animation_1924C_indices.inc"
};

AnimationSet D_actor_451100_8014B06C = {
    D_actor_451100_8014A614,
    D_actor_451100_8014B044,
    { NULL, D_actor_451100_80149B98, NULL, NULL, D_actor_451100_8014A024, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8014B094[25] = {
#include "assets/actor_451100_animation_19C78_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8014B1C0[231] = {
#include "assets/actor_451100_animation_19C78_bank4.inc"
};

AnimationRecord D_actor_451100_8014B55C[325] = {
#include "assets/actor_451100_animation_19C78_records.inc"
};

u16 D_actor_451100_8014BA70[20] = {
#include "assets/actor_451100_animation_19C78_indices.inc"
};

AnimationSet D_actor_451100_8014BA98 = {
    D_actor_451100_8014B55C,
    D_actor_451100_8014BA70,
    { NULL, D_actor_451100_8014B094, NULL, NULL, D_actor_451100_8014B1C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8014BAC0[17] = {
#include "assets/actor_451100_animation_1A2CC_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8014BB8C[139] = {
#include "assets/actor_451100_animation_1A2CC_bank4.inc"
};

AnimationRecord D_actor_451100_8014BDB8[195] = {
#include "assets/actor_451100_animation_1A2CC_records.inc"
};

u16 D_actor_451100_8014C0C4[20] = {
#include "assets/actor_451100_animation_1A2CC_indices.inc"
};

AnimationSet D_actor_451100_8014C0EC = {
    D_actor_451100_8014BDB8,
    D_actor_451100_8014C0C4,
    { NULL, D_actor_451100_8014BAC0, NULL, NULL, D_actor_451100_8014BB8C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8014C114[2] = {
#include "assets/actor_451100_animation_1A4F8_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8014C12C[29] = {
#include "assets/actor_451100_animation_1A4F8_bank4.inc"
};

AnimationRecord D_actor_451100_8014C1A0[84] = {
#include "assets/actor_451100_animation_1A4F8_records.inc"
};

u16 D_actor_451100_8014C2F0[20] = {
#include "assets/actor_451100_animation_1A4F8_indices.inc"
};

AnimationSet D_actor_451100_8014C318 = {
    D_actor_451100_8014C1A0,
    D_actor_451100_8014C2F0,
    { NULL, D_actor_451100_8014C114, NULL, NULL, D_actor_451100_8014C12C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8014C340[6] = {
#include "assets/actor_451100_animation_1A7D0_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8014C388[33] = {
#include "assets/actor_451100_animation_1A7D0_bank4.inc"
};

AnimationRecord D_actor_451100_8014C40C[111] = {
#include "assets/actor_451100_animation_1A7D0_records.inc"
};

u16 D_actor_451100_8014C5C8[20] = {
#include "assets/actor_451100_animation_1A7D0_indices.inc"
};

AnimationSet D_actor_451100_8014C5F0 = {
    D_actor_451100_8014C40C,
    D_actor_451100_8014C5C8,
    { NULL, D_actor_451100_8014C340, NULL, NULL, D_actor_451100_8014C388, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8014C618[2] = {
#include "assets/actor_451100_animation_1AA90_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8014C630[52] = {
#include "assets/actor_451100_animation_1AA90_bank4.inc"
};

AnimationRecord D_actor_451100_8014C700[98] = {
#include "assets/actor_451100_animation_1AA90_records.inc"
};

u16 D_actor_451100_8014C888[20] = {
#include "assets/actor_451100_animation_1AA90_indices.inc"
};

AnimationSet D_actor_451100_8014C8B0 = {
    D_actor_451100_8014C700,
    D_actor_451100_8014C888,
    { NULL, D_actor_451100_8014C618, NULL, NULL, D_actor_451100_8014C630, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8014C8D8[18] = {
#include "assets/actor_451100_animation_1B91C_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8014C9B0[332] = {
#include "assets/actor_451100_animation_1B91C_bank4.inc"
};

AnimationRecord D_actor_451100_8014CEE0[525] = {
#include "assets/actor_451100_animation_1B91C_records.inc"
};

u16 D_actor_451100_8014D714[20] = {
#include "assets/actor_451100_animation_1B91C_indices.inc"
};

AnimationSet D_actor_451100_8014D73C = {
    D_actor_451100_8014CEE0,
    D_actor_451100_8014D714,
    { NULL, D_actor_451100_8014C8D8, NULL, NULL, D_actor_451100_8014C9B0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8014D764[32] = {
#include "assets/actor_451100_animation_1C1E4_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8014D8E4[180] = {
#include "assets/actor_451100_animation_1C1E4_bank4.inc"
};

AnimationRecord D_actor_451100_8014DBB4[266] = {
#include "assets/actor_451100_animation_1C1E4_records.inc"
};

u16 D_actor_451100_8014DFDC[20] = {
#include "assets/actor_451100_animation_1C1E4_indices.inc"
};

AnimationSet D_actor_451100_8014E004 = {
    D_actor_451100_8014DBB4,
    D_actor_451100_8014DFDC,
    { NULL, D_actor_451100_8014D764, NULL, NULL, D_actor_451100_8014D8E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_451100_8014E02C[14] = {
#include "assets/actor_451100_animation_1C86C_bank1.inc"
};

AnimationPackedRotation D_actor_451100_8014E0D4[130] = {
#include "assets/actor_451100_animation_1C86C_bank4.inc"
};

AnimationRecord D_actor_451100_8014E2DC[226] = {
#include "assets/actor_451100_animation_1C86C_records.inc"
};

u16 D_actor_451100_8014E664[20] = {
#include "assets/actor_451100_animation_1C86C_indices.inc"
};

AnimationSet D_actor_451100_8014E68C = {
    D_actor_451100_8014E2DC,
    D_actor_451100_8014E664,
    { NULL, D_actor_451100_8014E02C, NULL, NULL, D_actor_451100_8014E0D4, NULL, NULL, NULL },
};

Actor451100MsgEntry D_actor_451100_8014E6B4[6] = {
    { 2003, { .call1 = func_actor_451100_80132E98 } },
    { 2005, { .call6 = func_actor_451100_80132F04 } },
    { 2004, { .call3 = func_actor_451100_80132F68 } },
    { 2011, { .call0 = func_actor_451100_80132FE0 } },
    { 2013, { .call4 = func_actor_451100_80132FE8 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_451100_8014E6E4[2] = {
    { 1, 96, func_actor_451100_80132BD4, { .model = &D_actor_451100_80146060 } },
    { 257, 96, func_actor_451100_801330B0, { .model = &D_actor_451100_80146318 } },
};

u8 D_actor_451100_8014E6FC[72] = {
    0,
    0,
    0,
    0,
    72,
    104,
    20,
    128,
    212,
    119,
    20,
    128,
    48,
    127,
    20,
    128,
    8,
    133,
    20,
    128,
    68,
    141,
    20,
    128,
    232,
    148,
    20,
    128,
    112,
    155,
    20,
    128,
    108,
    176,
    20,
    128,
    152,
    186,
    20,
    128,
    236,
    192,
    20,
    128,
    24,
    195,
    20,
    128,
    240,
    197,
    20,
    128,
    176,
    200,
    20,
    128,
    60,
    215,
    20,
    128,
    4,
    224,
    20,
    128,
    140,
    230,
    20,
    128,
    0,
    0,
    0,
    0,
};

Actor260500Work* D_actor_451100_8014E744 = NULL;

Task* D_actor_451100_8014E748;

s16 D_actor_451100_8014E74C;

static void func_actor_451100_80131E24(GpEnemy* enemy, Task* task);
static void func_actor_451100_801328A8(GpEnemy* enemy, Task* task);

/// State 0 of the `func_actor_451100_801322D4` dispatcher: allocates the work
/// block, publishes it in `D_actor_451100_8014E744` and on the task's work
/// slot, points the model's light and color matrices and its animation context
/// at it, then runs the per-frame update once and advances the task to state 1.
///
/// Every access to the block after the null check goes through
/// `D_actor_451100_8014E744` rather than the `memCalloc` result, which is why
/// the pointer is reloaded at each use.
static void func_actor_451100_80131E24(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor260500Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x4B8, 0);
    D_actor_451100_8014E744 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_451100_801323B4;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->lightMtx                = &D_actor_451100_8014E744->light;
    obj->colorMtx                = &D_actor_451100_8014E744->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_451100_8014E748      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_451100_8014E744->rig.anim, D_actor_451100_8013F740, obj,
                  D_actor_451100_8014E744->rig.poses, D_actor_451100_8014E744->rig.slots);
    D_actor_451100_8014E744->st.animId  = 0x14;
    D_actor_451100_8014E744->st.state   = 2;
    D_actor_451100_8014E744->st.travel  = 0;
    D_actor_451100_8014E744->turnFrames = 0;
    task->msgTable                      = D_actor_451100_8013F704;
    func_actor_451100_80131F84(task);
    task->state += 1;
}

/// Step routine of the actor `func_actor_451100_801322D4` dispatches, run each
/// frame and by its "start animation" opcode: states 1 and 2 run their one-shot
/// animation reseed and leave the work block in state 3; state 3 walks the
/// model while `travel` counts down in clips 0xE, 2 and 0xF (stride picked by
/// `D_actor_451100_8014E74C`), dropping to clip 0xD with a reset argument of 10
/// when it runs out, turns it while `turnFrames` counts down in clip 3, then ticks
/// the animation.
static void func_actor_451100_80131F84(Task* task)
{
    GfxCoord*        coord = task->extra.tmd->coords;
    Actor260500Work* work  = (Actor260500Work*)task->work;

    if (D_actor_451100_8014E744->st.state == 1) {
        func_actor_451100_801324B8();
        D_actor_451100_8014E744->st.state = 3;
    } else if (D_actor_451100_8014E744->st.state == 2) {
        func_actor_451100_80132428();
        D_actor_451100_8014E744->st.state = 3;
    } else if (D_actor_451100_8014E744->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_451100_8014E74C) {
                    case 0:
                        actorMoveModelForward(task, 0x3C);
                        break;
                    case 1:
                        actorMoveModelForward(task, -0xF);
                        break;
                    case 2:
                        actorMoveModelForward(task, 0x19);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state          = 1;
                    D_actor_451100_8013F700 = 10;
                    work->st.animId         = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->turnFrames--;
        }
        func_actor_451100_801323DC();
    }
}

/// Task handler of the actor whose work block this overlay publishes: runs
/// the handler for the task's state from a two-entry table built on the stack
/// (0 spawns, 1 runs a frame), refreshing `D_actor_451100_8014E744` from the
/// task's work slot first so the handlers can reach the block without the
/// task.
void func_actor_451100_801322D4(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_451100_80131E24,
        func_actor_451100_80132330,
    };

    D_actor_451100_8014E744 = (Actor260500Work*)task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// State 1 of the `func_actor_451100_801322D4` dispatcher, run each frame:
/// refreshes the model root's world matrix, relights the model from a point
/// 0x320 above its translation, then runs the step routine and draws the
/// ground shadow.
static void func_actor_451100_80132330(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_451100_80131F84(task);
    func_actor_451100_8013280C(task);
}

/// Exit callback the `func_actor_451100_801322D4` spawn handler installs on the
/// actor's task: tears down the enemy the task was spawned for.
static void func_actor_451100_801323B4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Ticks animation slots 1..0x12 of the published work block, the last step of
/// `func_actor_451100_80131F84`'s state 3.
static void func_actor_451100_801323DC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_451100_8014E744->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Restarts the animation without a reset argument, the step routine's state
/// 2: marks animation slots 1..0x12 as reset-pending and reseeds each of them
/// from the current animation id, then records that id as the one now
/// playing. Reaches the block through `D_actor_451100_8014E744`.
static void func_actor_451100_80132428(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_451100_8014E744->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_451100_8014E744->rig.anim, i, D_actor_451100_8014E744->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_451100_8014E744->st.appliedAnimId = D_actor_451100_8014E744->st.animId;
}

/// Restarts the animation with the reset argument in
/// `D_actor_451100_8013F700`, the step routine's state 1: reseeds animation
/// slots 1..0x12 from the current animation id and records that id as the one
/// now playing.
static void func_actor_451100_801324B8(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_451100_8014E744->rig.anim, i, D_actor_451100_8014E744->st.animId, 0,
                      D_actor_451100_8013F700);
        i++;
    } while (i < 0x13);
    D_actor_451100_8014E744->st.appliedAnimId = D_actor_451100_8014E744->st.animId;
}

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 0x25 and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 func_actor_451100_80132538(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    if (args->animationId < 0x25) {
        D_actor_451100_8014E744->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            D_actor_451100_8014E744->st.state = 1;
            D_actor_451100_8013F700           = args->blendFrames;
        } else {
            D_actor_451100_8014E744->st.state = 2;
        }
        D_actor_451100_8014E744->st.field_6 = 0;
        func_actor_451100_80131F84(D_actor_451100_8014E748);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler of `D_actor_451100_8013F704`: bit 0 of `arg2` clears
/// `TmdObject::flags` on the model of the task in `D_actor_451100_8014E748`,
/// showing it, and its absence sets 0x80, hiding it; bit 1 additionally ORs in
/// 0x4.
s32 func_actor_451100_801325C8(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = D_actor_451100_8014E748->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_HIDDEN;
    }
    if (arg2 & 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Message 0x7D4 handler of `D_actor_451100_8013F704`, the placement opcode:
/// yaws the task's root coordinate to `placement->rot.vy`, caching that yaw in
/// the published work block, then drops the placement translation into the
/// matrix and marks it dirty.
s32 func_actor_451100_80132610(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord* coord;
    u16       yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_451100_8014E744->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Message 0x7DB handler of `D_actor_451100_8013F704`: a zero payload
/// halfword sets the published block's `turnFrames` to 0x14, the count of frames
/// the step routine turns the model while clip 3 plays.
s32 func_actor_451100_8013268C(Task* task, s32 arg1, ActorCommand* msg)
{
    if (msg->command == 0) {
        D_actor_451100_8014E744->turnFrames = 0x14;
    }
    return 0;
}

/// Message 0x7DD handler of `D_actor_451100_8013F704`, the "walk to" opcode:
/// records `mode` in `D_actor_451100_8014E74C`, turns the model to face
/// `target` (away from it in mode 1) caching the yaw in the work block, and
/// leaves in `travel` the number of frames the step routine needs to cover the
/// planar distance at that mode's stride: 0x3C in mode 0, 0xF in mode 1 and
/// 0x19 in mode 2.
s32 func_actor_451100_801326B0(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GfxCoord*        coord;
    Actor260500Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = task->extra.tmd->coords;
    work                    = (Actor260500Work*)task->work;
    D_actor_451100_8014E74C = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_451100_8014E74C == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_451100_8014E74C) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}

/// Draws the ground shadow quad under the model root of the actor
/// `func_actor_451100_801322D4` dispatches, unless the model is hidden
/// (`flags & 0x80`) or has no buffer yet. The root's world translation is
/// staged in a scratchpad `VECTOR3`, and the quad's brightness follows the
/// room's current ground shade.
static void func_actor_451100_8013280C(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & TMD_OBJECT_HIDDEN) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShadowShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// State 0 of the `func_actor_451100_80132BD4` dispatcher: allocates the
/// actor's 0x4C0-byte `Actor150400Work` block and hangs it off the task, spawns
/// entry 1 of `D_actor_451100_8014E6E4` (the sub-model task
/// `func_actor_451100_801330B0`), hands it to `Task_Reparent` with this task
/// and keeps it in `pairTask`, then seeds the animation and runs the step
/// routine once.
///
/// `memCalloc`'s result goes through an untyped `block` that `work` is copied
/// from: the raw pointer is what the `Task::work` store and the null test read,
/// so it stays a short-lived `$v0` quantity while the typed copy takes the
/// callee-saved home it needs across the calls below. Assigning the call result
/// straight to `work` collapses the two into one pseudo and puts `$s1` in all
/// three places.
static void func_actor_451100_801328A8(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor150400Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;
    GpEnemy*         spawned;
    void*            block;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    block      = memCalloc(0x4C0, false);
    work       = (Actor150400Work*)block;
    task->work = block;
    if (block == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_451100_80132CAC;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    work->enemy                  = enemy;
    spawned                      = Gp_SpawnEnemyFromTable(D_actor_451100_8014E6E4, 1, 0, enemy);
    Task_Reparent(task, spawned->task);
    work->pairTask = spawned->task;
    obj->lightMtx  = &work->light;
    obj->colorMtx  = &work->color;
    vec.vx         = coord->workm.t[0];
    vec.vy         = coord->workm.t[1] - 0x320;
    vec.vz         = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_451100_8014E6FC, obj, work->rig.poses,
                  work->rig.slots);
    work->st.animId = 1;
    work->st.state  = 2;
    task->msgTable  = D_actor_451100_8014E6B4;
    func_actor_451100_80132A1C(task);
    task->state += 1;
}

/// Step routine of the actor `func_actor_451100_80132BD4` dispatches, run each
/// frame and by its "start animation" opcode. States 1 and 2 restart the clip
/// (with and without the reset argument) and advance to 3; state 3 walks the
/// model 0x11 units a frame while clip 4 plays and `travel` is non-zero,
/// dropping back to clip 1 with 0xA in `animArg` when the count runs out, then
/// ticks the animation.
static void func_actor_451100_80132A1C(Task* task)
{
    Actor150400Work* work;
    s16              animId;

    work = (Actor150400Work*)task->work;
    if (work->st.state == 1) {
        func_actor_451100_80132E34(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_451100_80132DBC(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveModelForward(task, 0x11);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_451100_80132D70(task);
        return;
    }
}

/// Task handler of the actor whose work block lives only on its task, entry 0
/// of `D_actor_451100_8014E6E4`: runs the handler for the task's state from a
/// two-entry table built on the stack (0 spawns, 1 runs a frame), passing the
/// task's `GpEnemy` as well as the task.
void func_actor_451100_80132BD4(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_451100_801328A8,
        func_actor_451100_80132C28,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// State 1 of the `func_actor_451100_80132BD4` dispatcher, run each frame:
/// refreshes the model root's coordinate, relights the model from a point 800
/// above its translation, then runs the step routine and draws the ground
/// shadow.
static void func_actor_451100_80132C28(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_451100_80132A1C(task);
    func_actor_451100_80132CD4(task);
}

/// Exit callback the `func_actor_451100_80132BD4` spawn handler installs on the
/// actor's task: tears down the enemy the task was spawned for.
static void func_actor_451100_80132CAC(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Draws the ground shadow quad under the model root of the actor
/// `func_actor_451100_80132BD4` dispatches, unless the model is hidden
/// (`flags & 0x80`) or has no buffer yet; the same body as
/// `func_actor_451100_8013280C`.
static void func_actor_451100_80132CD4(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & TMD_OBJECT_HIDDEN) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShadowShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks animation slots 1..0x12 of the task's work block.
static void func_actor_451100_80132D70(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Restarts the animation without a reset argument, the step routine's state
/// 2: marks animation slots 1..0x12 as reset-pending, reseeds each from the
/// current animation id and records that id as the one now playing.
static void func_actor_451100_80132DBC(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
    }
    work->st.appliedAnimId = work->st.animId;
}

/// Restarts the animation with `animArg` as the reset argument, the step
/// routine's state 1, then records the animation id as the one now playing.
static void func_actor_451100_80132E34(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x13);
    work->st.appliedAnimId = work->st.animId;
}

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 0x12 and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 func_actor_451100_80132E98(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    Actor150400Work* work;

    work = (Actor150400Work*)task->work;
    if (args->animationId < 0x12) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state = 1;
            work->animArg  = args->blendFrames;
        } else {
            work->st.state = 2;
        }
        work->st.field_6 = 0;
        func_actor_451100_80132A1C(task);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler of `D_actor_451100_8014E6B4`: bit 0 of `flags`
/// clears `TmdObject::flags` on both the actor's model and its `pairTask`'s,
/// showing them, and its absence sets 0x80, hiding them; bit 1 additionally
/// ORs in 0x4.
s32 func_actor_451100_80132F04(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = task->extra.tmd;
    other = ((Actor150400Work*)task->work)->pairTask->extra.tmd;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = TMD_OBJECT_HIDDEN;
        other->flags = TMD_OBJECT_HIDDEN;
    }

    if (flags & 2) {
        self->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        other->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Message 0x7D4 handler of `D_actor_451100_8014E6B4`, the placement opcode:
/// yaws the actor's root coordinate to `placement->rot.vy`, caching that yaw
/// in the work block, then drops the placement translation into the matrix
/// and marks it dirty.
s32 func_actor_451100_80132F68(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord*        coord;
    Actor150400Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Message 0x7DB handler of `D_actor_451100_8014E6B4`: accepts the message and
/// does nothing.
s32 func_actor_451100_80132FE0(void)
{
    return 0;
}

/// Message 0x7DD handler of `D_actor_451100_8014E6B4`, the "walk to" opcode:
/// turns the actor's root coordinate to face `target`, caching the yaw in the
/// work block, and leaves the horizontal distance to it, in seventeenths, in
/// `travel` for the step routine to count down.
s32 func_actor_451100_80132FE8(Task* task, s32 arg1, VECTOR* target)
{
    GfxCoord*        coord;
    Actor150400Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    dx           = target->vx - coord->coord.t[0];
    dz           = target->vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 17;
    return 0;
}

/// Per-frame handler of the sub-model task, entry 1 of
/// `D_actor_451100_8014E6E4`, reached with the sub-model's own `TmdObject` in
/// `Task::extra` and the actor holding it as `Task::parent`. On its first tick
/// it lights the sub-model with the parent's two leading work matrices and
/// hangs its root coordinate off coordinate 8 of the parent's model; after that
/// it only marks the coordinate dirty each frame so it follows that part.
void func_actor_451100_801330B0(Task* task)
{
    char       pad[0x10];
    Task*      parent = task->parent;
    TmdObject* obj    = task->extra.tmd;
    GfxCoord*  coord  = obj->coords;
    GfxCoord*  sub    = &parent->extra.tmd->coords[8];
    MATRIX*    work   = (MATRIX*)parent->work;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            obj->lightMtx       = work;
            obj->colorMtx       = work + 1;
            coord->parent       = sub;
            task->state++;
            break;
        case 1:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
