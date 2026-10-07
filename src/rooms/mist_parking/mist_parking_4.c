#include "rooms/mist_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "mist_parking_private.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

void              func_mist_parking_801846A4(s32 arg0);
extern EvsCommand D_mist_parking_80190C74[];
extern EvsCommand D_mist_parking_80190D64[];
extern EvsCommand D_mist_parking_80190E84[];
extern EvsCommand D_mist_parking_80191034[];

extern s8 D_mist_parking_801908C8[];

void func_mist_parking_80183D58(Task*);
void func_mist_parking_80183EAC(Task*);
void func_mist_parking_801842DC(Task*);
void func_mist_parking_8018451C(Task*);
void func_mist_parking_80184668(Task*);

extern AnimationPlayRequest D_mist_parking_801908A0;
extern AnimationPlayRequest D_mist_parking_801908B4;
extern AnimationPlayRequest D_mist_parking_80190944;
extern AnimationPlayRequest D_mist_parking_801909F8;
extern ActorCommand         D_mist_parking_80190BA4;
extern ActorCommand         D_mist_parking_80190BA8;

void func_mist_parking_80184408(s32);
void func_mist_parking_80184428(s32);
void func_mist_parking_80184468(s32);
void func_mist_parking_801844EC(void);
void func_mist_parking_8018459C(void);
void func_mist_parking_801845D0(s32);
void func_mist_parking_80184624(s32);
void func_mist_parking_801846A4(s32);

TaskDesc D_mist_parking_80190824[5] = {
    { { { TASK_BODY_NONE, 192 } }, func_mist_parking_8018451C, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_mist_parking_80183D58, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_parking_80184668, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_parking_80183EAC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_parking_801842DC, { .value = 0 } },
};

AnimationSet* D_mist_parking_80190860[4] = {
    NULL,
    &gMistParkingAnimation129F8,
    &gMistParkingAnimation12DCC,
    &gMistParkingAnimation1323C,
};

AnimationBankCopyRequest D_mist_parking_80190870 = { { .sets = D_mist_parking_80190860 }, ARRAY_SIZE(D_mist_parking_80190860) };

AnimationPlayRequest D_mist_parking_80190878 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8019088C = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_801908A0 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_801908B4 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

s8 D_mist_parking_801908C8[124] = {
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
};

AnimationPlayRequest D_mist_parking_80190944 = { { .sets = NULL }, 6, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190958[8] = {
    { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 13, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_801909F8 = { { .index = 0 }, 15, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190A0C[20] = {
    { { .index = 0 }, 16, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 17, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 18, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 19, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 20, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 21, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 22, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 23, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 24, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 25, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 26, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 27, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 28, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 29, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 30, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 31, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 32, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 33, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 34, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 35, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

GameActorMoveAnim D_mist_parking_80190B9C = { 0, 0x10000 };

ActorCommand D_mist_parking_80190BA4 = { { .loc = { 0, 0 } }, 2 };

ActorCommand D_mist_parking_80190BA8 = { { .loc = { 0, 0 } }, 3 };

AnimationPlayRequest D_mist_parking_80190BAC = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_80190BC0 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190BD4[3] = {
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_80190C10 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190C24 = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190C38 = { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190C4C = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190C60 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_ENABLE };

EvsCommand D_mist_parking_80190C74[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_parking_8018459C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_80190BA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_801908A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190944 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_80190D64[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80184408 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_801908B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_801909F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_801845D0 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_801908B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_80190BA8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_80190E84[18] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80184408 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_801909F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80184624 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_801846A4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80184428 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_parking_801844EC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_80191034[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80184408 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_801909F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80184624 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80184468 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

void func_mist_parking_80183BAC(s32 arg0)
{
    WorldCollisionGrid* dst;
    WorldCollisionGrid* src;
    SVECTOR             d;
    s32                 i;

    dst = &D_mist_parking_80192204;
    src = &D_mist_parking_8018FCB8;

    for (i = 0; i < 2; i++) {
        dst->normals[i].vx = src->normals[i].vx;
        dst->normals[i].vy = src->normals[i].vy;
        dst->normals[i].vz = src->normals[i].vz;
        dst->faces[i]      = src->faces[i];
    }

    for (i = 0; i < 6; i++) {
        dst->vertices[i].vx = src->vertices[i].vx;
        dst->vertices[i].vy = src->vertices[i].vy;
        dst->vertices[i].vz = src->vertices[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0x7D0;
    }
    d.vz = 0;

    for (i = 0; i < 6; i++) {
        dst->vertices[i].vx += d.vx;
        dst->vertices[i].vy += d.vy;
        dst->vertices[i].vz += d.vz;
    }
}

void func_mist_parking_80183D58(Task* task)
{
    GameActor* actor;
    Enemy*     enemy;
    s32        idx;
    s32        flag;
    u16        tick;

    actor = (GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work;
    if (D_801156F9 == 0) {
        idx = actor->animationSlots[1].nextPose.indices.setIndex - ANIMATION_BANK_BASE_SET_COUNT;
        if ((idx > 0) && (idx < D_mist_parking_80190870.wordCount)) {
            flag = D_mist_parking_801908C8[idx];
        } else {
            flag = 0;
        }
        if (task->state == 0) {
            if ((flag != 0) || (task->spawnArg1.value != 0)) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            enemy = sceneFindEnemyByPlaceKey(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8));
            animationAimHeadAtTask(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), enemy->task, 0x200, 0x100, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

void func_mist_parking_80183EAC(Task* task)
{
    MistParkingShopTalkState* talk = &D_mist_parking_80195334;
    s32                       cmd;
    s32                       i;
    s16                       prize;
    s32                       key;
    u16                       tick;
    u16                       tick2;

    switch (task->state) {
        case 0:
            memFillBytes(talk, 0, sizeof(*talk));
            func_mist_parking_801846A4(1);
            evsStartScript(D_mist_parking_80191154, EVENT_SCRIPT_HUD_KEEP);
            Gp_RunCapCmd(6, 0);
            task->state++;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            for (i = 0; i < 5; i++) {
                if (gameFlagGetNibble(i + 0x125) == 2) {
                    task->state = 2;
                    return;
                }
            }
            task->state = 6;
            break;
        case 2:
            func_mist_parking_801846A4(2);
            evsStartScript(D_mist_parking_80191154, EVENT_SCRIPT_HUD_KEEP);
            Gp_RunCapCmd(1, 0);
            talk->businessDone = 1;
            task->state++;
            break;
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            if (capGetVariantKey() == 1) {
                Gp_RunCapCmd(7, 0);
                talk->prizeTimer          = 10;
                talk->prizeClosingCommand = 2;
                task->state               = 4;
            } else {
                talk->prizeClosingCommand = 3;
                task->state               = 5;
            }
            break;
        case 4:
            if (capIsBusy() != 0) {
                return;
            }
            // Give each prize ten frames: the caption of one still waiting here
            // starts halfway, and its state flag moves on when the turn ends.
            talk->prizeTimer--;
            if (talk->prizeTimer == 5) {
                prize = talk->prizeIndex;
                if (gameFlagGetNibble(prize + 0x125) == 2) {
                    Gp_StartCapSlot(5, 0, prize);
                }
                return;
            }
            if (talk->prizeTimer != 0) {
                return;
            }
            prize = talk->prizeIndex;
            if (areaGetCurrentObjectState(prize + 0x20) != 1) {
                gameFlagSetNibble(prize + 0x125, 3);
            }
            talk->prizeTimer = 10;
            talk->prizeIndex++;
            if (talk->prizeIndex >= 5) {
                task->state++;
            }
            break;
        case 5:
            evsStartScript(D_mist_parking_80191154, EVENT_SCRIPT_HUD_KEEP);
            Gp_RunCapCmd(talk->prizeClosingCommand, 0);
            task->state++;
            break;
        case 6:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            tick                = task->killCountdown + 1;
            task->killCountdown = tick;
            if ((s16)tick == 0xA) {
                func_mist_parking_801846A4(1);
                Gp_RunCapCmd(9, 0);
                task->killCountdown = 0;
                task->state++;
            }
            break;
        case 7:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case 8:
            key                   = capGetVariantKey();
            task->spawnArg1.value = key;
            if (key == 6) {
                evsStartScript(D_mist_parking_80191214, EVENT_SCRIPT_HUD_KEEP);
                talk->businessDone = 1;
            } else if (key == 7) {
                evsStartScript(D_mist_parking_80191304, EVENT_SCRIPT_HUD_KEEP);
                talk->businessDone = 1;
            } else {
                evsStartScript(D_mist_parking_801913C4, EVENT_SCRIPT_HUD_KEEP);
            }
            task->state++;
            break;
        case 9:
            tick2               = task->killCountdown + 1;
            task->killCountdown = tick2;
            if ((s16)tick2 == 0xA) {
                switch (task->spawnArg1.value) {
                    case 6:
                        Gp_RunCapCmd(7, 0);
                        break;
                    case 7:
                        Gp_RunCapCmd(8, 0);
                        break;
                    case 8:
                        if (talk->businessDone != 0) {
                            Gp_RunCapCmd(7, 0);
                        } else {
                            Gp_RunCapCmd(0xA, 0);
                        }
                        break;
                }
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            cmd                 = 0xA;
            task->killCountdown = 0;
            if (task->spawnArg1.value == 7) {
                cmd = 6;
            }
            task->state = cmd;
            break;
        case 10:
            Gp_MsgPlayerWeapon(1);
            func_mist_parking_801846A4(0);
            taskKill(task);
            break;
    }
}

void func_mist_parking_801842DC(Task* task)
{
    s32 key;

    switch (task->state) {
        case 0:
            func_mist_parking_801846A4(1);
            evsStartScript(D_mist_parking_80190C74, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case 1:
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case 2:
            key                   = capGetVariantKey();
            task->spawnArg1.value = key;
            switch (key) {
                case 1:
                    evsStartScript(D_mist_parking_80190D64, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case 2:
                    evsStartScript(D_mist_parking_80190E84, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case 3:
                    evsStartScript(D_mist_parking_80191034, EVENT_SCRIPT_HUD_KEEP);
                    break;
            }
            task->state++;
            break;
        case 4:
            if (task->spawnArg1.value == 1) {
                Gp_MsgPlayerWeapon(1);
            }
            func_mist_parking_801846A4(0);
            taskKill(task);
            break;
    }
}

void func_mist_parking_80184408(s32 arg0)
{
    Gp_RunCapCmd(arg0, 0);
}

void func_mist_parking_80184428(s32 arg0)
{
    taskSpawnFromTable(D_mist_parking_8018FC24, 0, arg0, 0);
    gGameSession->freezeRoomObjs = 1;
}

void func_mist_parking_80184468(s32 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = arg0;
    gDisplayState.spriteVariant                                 = 1;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(0, 0x11, 0, 0);
    if (arg0 == 5) {
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, GAME_FLOW_LOAD_CAPTION_NORMAL);
    }
}

/// Spawns entry 0 of `D_mist_parking_80190824`.
void func_mist_parking_801844EC(void)
{
    taskSpawnFromTable(D_mist_parking_80190824, 0, 0, 0);
}

void func_mist_parking_8018451C(Task* task)
{
    func_800BC4BC();
    gPlayerStatus.resourceVariant                               = 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_ACROPOLIS_PLAZA;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
    gDisplayState.spriteVariant                                 = 1;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(0, 0x11, 0, 0);
    gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, GAME_FLOW_LOAD_CAPTION_NORMAL);
}

/// Spawns entry 1 of `D_mist_parking_80190824` and keeps its handle in
/// `D_mist_parking_8019532C.task`.
void func_mist_parking_8018459C(void)
{
    D_mist_parking_8019532C.task = taskSpawnFromTable(D_mist_parking_80190824, 1, 0, 0);
}

/// Hands `phase` (0 or 1) to the task in `D_mist_parking_8019532C.task` as its
/// `spawnArg1`; any other value kills the task and drops the handle.
void func_mist_parking_801845D0(s32 phase)
{
    Task* t = D_mist_parking_8019532C.task;

    if (t == NULL) {
        return;
    }
    switch (phase) {
        case 0:
        case 1:
            t->spawnArg1.value = phase;
            break;
        default:
            taskKill(D_mist_parking_8019532C.task);
            D_mist_parking_8019532C.task = NULL;
            break;
    }
}

void func_mist_parking_80184624(s32 arg0)
{
    displayQueueModeTask(taskGetDescAt(D_mist_parking_80190824, 2U), arg0, 0, STAGE_ENTRY_RELOAD);
}

void func_mist_parking_80184668(Task* arg0)
{
    s32 temp_v0;

    temp_v0               = arg0->spawnArg1.value - 1;
    arg0->spawnArg1.value = temp_v0;
    if (temp_v0 < 0) {
        taskKill(arg0);
        stageRequestModeTaskExit();
    }
}

void func_mist_parking_801846A4(s32 arg0)
{
    capReset();
    switch (arg0) {
        case 1:
            Gp_CapFile = 0;
            capSelectLoadedFile(1);
            capSetTexturePage(0x140, 0x100);
            break;
        case 2:
            Gp_CapFile = 0;
            capSelectLoadedFile(2);
            capSetTexturePage(0x2C0, 0);
            break;
    }
}

/// Drops the handle in `D_mist_parking_8019532C.task` without killing the task.
/// Its caller passes an argument, which is unused.
void func_mist_parking_8018471C(s32 arg0)
{
    D_mist_parking_8019532C.task = NULL;
}
