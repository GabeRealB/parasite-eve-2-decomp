#include "types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
s32 D_actor_143000_80135C10;

s32 D_actor_143000_80135C14;

s32 D_actor_143000_80135C18;

s32 D_actor_143000_80135C1C;

#include "actor_143000_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/shelter_b2_laboratory.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[5];
        AnimationPlayRequest arguments[6];
    } data;
    s32 words[35];
} Actor143000AnimStorage50D4;
STATIC_ASSERT_SIZEOF(Actor143000AnimStorage50D4, 140);

extern Actor143000AnimStorage50D4 D_actor_143000_801350D4;

extern u8       D_actor_143000_801351AC;
extern GpEvsCmd D_actor_143000_801351B0[];
extern TaskDesc D_actor_143000_801350C8;
extern GpEvsCmd D_actor_143000_80135870[];
extern GpEvsCmd D_actor_143000_80135A20[];
extern GpEvsCmd D_actor_143000_80135AE0[];

extern u8 D_actor_143000_80135C38[];

void func_actor_143000_801344A8(s32);
void func_actor_143000_801344D8(void);
void func_actor_143000_8013450C(void);
void func_actor_143000_8013452C(u8);
void func_actor_143000_80134538(void);

void func_actor_143000_80133EE4(Task*);

TaskDesc D_actor_143000_801350B0[2] = {
    { 0, 192, taskKill, { .model = NULL } },
    { 0, 32, func_actor_143000_80133EE4, { .model = NULL } },
};

TaskDesc D_actor_143000_801350C8 = { 0, 32, func_actor_143000_80133CF0, { .model = NULL } };

Actor143000AnimStorage50D4 D_actor_143000_801350D4 = { .data = { { &D_actor_143000_80134840, &D_actor_143000_80134AEC, &D_actor_143000_80134D08, &D_actor_143000_80134EB0, &D_actor_143000_80135068 }, { { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, 0 }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, 0 }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, 0 }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, 0 }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, 0 }, { { .index = 1 }, 50, 1, 10, 0 } } } };

AnimationPlayRequest D_actor_143000_80135160 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

GpCopyArg D_actor_143000_80135174 = { { .words = D_actor_143000_801350D4.words }, 32 };

ActorTransform D_actor_143000_8013517C = { { 3270, 0, -1630, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_143000_80135194 = { { 3270, 0, -2630, 0 }, { 0, -2048, 0, 0 } };

u8 D_actor_143000_801351AC = 0;

GpEvsCmd D_actor_143000_801351B0[72] = {
    { 13, { .callbackNoArg = func_actor_143000_801344D8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 13, { .callbackSetText = func_800E6E44 }, { .value = -0x7FECBD08 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_143000_80135174 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_143000_8013517C }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.arguments[2] }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x541F0009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.arguments[2] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x541F000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.arguments[2] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x541F000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x541F000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.arguments[2] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x541F000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_actor_143000_8013452C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x541F000F }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_143000_801344A8 }, { .storage = &D_actor_143000_80135090 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_143000_801344A8 }, { .storage = &D_actor_143000_801350A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 13, { .callbackU8 = func_actor_143000_8013452C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.arguments[4] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 47, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b2_laboratory_801804FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_80135160 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_143000_80135194 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_143000_801350D4.data.arguments }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_143000_8013450C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_143000_80135870[18] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_actor_143000_8013452C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_143000_80135194 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_143000_801350D4.data.arguments }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b2_laboratory_801804FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_143000_80134538 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_143000_8013450C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_143000_80135A20[8] = {
    { 3, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_143000_80135174 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_143000_8013517C }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143000_801350D4.data.arguments[2] }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_143000_80135AE0[12] = {
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_143000_80135194 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_143000_801350D4.data.arguments }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_143000_80134538 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

s32 D_actor_143000_80135C00 = 0;

s32 D_actor_143000_80135C04 = 0;

Actor143000Spawn D_actor_143000_80135C08 = { 0 };

u8 D_actor_143000_80135C0C[4] = {
    0,
    101,
    2,
    57,
};

char D_actor_143000_80135C20[24];

u8 D_actor_143000_80135C38[8];

void func_actor_143000_801342F8(s32 x, s32 y, u16* codes, s32 index, s32 active);

void func_actor_143000_80133EE4(Task* arg0)
{
    Actor143000Spawn* spawn;
    s32               i;
    u8*               p;
    u8*               slot;

    spawn = arg0->spawnArg2.pointer;
    switch (arg0->state) {
        case 0:
            spawn->field_1 = 1;
            srand(gDisplayState.gameTick);
            Gp_MsgPlayerWeapon(0);
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x340, 0);
            Gp_SetItemSeenBit(0x121, 1);
            Gp_SetItemSeenBit(0x122, 1);
            func_800E8614(D_actor_143000_80135A20, 1);
            arg0->state++;
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                gGameSession->eventState = 1;
                arg0->state              = 2;
            }
            return;
        case 2:
            if (Gp_HasCollectedBit(0x121) != 0) {
                Gp_RunCapCmd(1, 0);
            } else {
                Gp_RunCapCmd(2, 0);
            }
            arg0->state++;
            return;
        case 3:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            return;
        case 4:
            switch (Gp_GetCapEventKey()) {
                case 0xA:
                    arg0->state = 0x14;
                    return;
                case 0x63:
                    Gp_RunCapCmd(0x20, 0);
                    arg0->state++;
                    return;
                default:
                    Gp_RunCapCmd(3, 0);
                    arg0->state = 6;
                    return;
            }
        case 5:
            if (Gp_CapBusy() == 0) {
                arg0->state = 0xA;
            }
            return;
        case 10:
            func_800E8614(D_actor_143000_80135AE0, 0);
            arg0->state++;
            return;
        case 11:
            if (gGameSession->eventState == 0) {
                arg0->state++;
            }
            return;
        case 12:
            Gp_ResetCap();
            taskKill(arg0);
            return;
        case 20:
            i = 10;
            p = &D_actor_143000_80135C38[i];
            do {
                *p = 0;
                i--;
                p--;
            } while (i >= 0);
            D_actor_143000_80135C18 = 0;
            D_actor_143000_80135C1C = 0;
            Gp_RunCapCmd(4, 0);
            arg0->state++;
            return;
        case 21:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            return;
        case 22:
            while (1) {
                D_actor_143000_80135C14 = (rand() * 11) >> 15;
                slot                    = &D_actor_143000_80135C38[D_actor_143000_80135C14];
                if (*slot == 0) {
                    *slot = 1;
                    break;
                }
            }
            Gp_RunCapCmd(D_actor_143000_80135C14 + 5, 0);
            arg0->state++;
            return;
        case 23:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            return;
        case 24:
            if (Gp_GetCapEventKey() == 0xB) {
                D_actor_143000_80135C1C++;
            }
            D_actor_143000_80135C18++;
            if (D_actor_143000_80135C18 >= 3) {
                if (D_actor_143000_80135C1C >= 3) {
                    arg0->state = 0x28;
                } else {
                    arg0->state = 0x1E;
                }
            } else {
                arg0->state = 0x16;
            }
            return;
        case 30:
            Gp_StartCapSlot(0x21, 0, (s16)D_actor_143000_80135C1C);
            arg0->state++;
            return;
        case 6:
        case 31:
            if (Gp_CapBusy() == 0) {
                arg0->state = 2;
            }
            return;
        case 40:
            Gp_RunCapCmd(0x22, 0);
            arg0->state++;
            return;
        case 41:
            if (Gp_CapBusy() == 0) {
                Gp_ResetCap();
                func_800E3FAC(0xA2, 0x27);
                GameFlag_SetNibble(0xD0, 2);
                GameFlag_SetNibble(0x4B, 0);
                Gp_ApplyAreaRecs(D_shelter_b2_laboratory_80186488);
                if (GameFlag_GetNibble(0x83) == 0) {
                    Gp_ApplyAreaRecs(D_shelter_b2_laboratory_8018649C);
                }
                GameFlag_SetNibble(3, 0);
                GameFlag_SetNibble(0x155, 3);
                func_800E8634(D_actor_143000_801351B0, 0, D_actor_143000_80135870);
                taskKill(arg0);
            }
            return;
    }
}

void func_actor_143000_801342F8(s32 x, s32 y, u16* codes, s32 index, s32 active)
{
    POLY_F4* prim;

    if (y < 0x59) {
        if (active != 0) {
            if ((codes[index] & 0xF000) == 0x3000) {
                Gp_PlayerWeaponId(&D_actor_143000_801350D4.data.arguments[3].source.index);
                Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &D_actor_143000_801350D4.data.arguments[3].source.index, 0);
                D_actor_143000_801351AC = 1;
            }
            if (codes[index] == 0xFFFE && D_actor_143000_801351AC == 1) {
                D_actor_143000_801351AC = 0;
                Gp_PlayerWeaponId(&D_actor_143000_801350D4.data.arguments[5].source.index);
                Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &D_actor_143000_801350D4.data.arguments[5].source.index, 0);
            }
        }
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyF4(prim);
        setRGB0(prim, 0, 0x7C, 0x2C);
        setXY4(prim, x + 4, y - 15, x + 19, y - 15, x + 4, y, x + 19, y);
        if (D_actor_143000_80135C10 & 4) {
            addPrim(&gGpuCurrentOt[2], prim);
        }
        D_actor_143000_80135C10++;
    }
}

void func_actor_143000_801344A8(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_143000_801350C8, 0, 0, arg0);
}

void func_actor_143000_801344D8(void)
{
    Gp_CapFile = 0;
    Gp_LoadCapFile(3);
    func_800E6D4C(0x180, 0x100);
}

void func_actor_143000_8013450C(void)
{
    Gp_ResetCap();
}

void func_actor_143000_8013452C(u8 arg0)
{
    Gp_StateF0.field_4 = arg0;
}

void func_actor_143000_80134538(void)
{
    Gp_SpawnWeaponEff();
}
