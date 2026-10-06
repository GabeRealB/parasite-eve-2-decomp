#include "gameplay/captions.h"

#include "types.h"

#include "gameplay/cap.h"
#include "cap.h"
#include "captions.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "object_task.h"
#include "gameplay/player_actor.h"
#include "player_actor.h"
#include "gameplay/scene_combat.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/wipsys.h"

CapActionRequest D_801155A0;

extern TaskDesc Gp_EvtSpawnTable[3];

extern AnimationPlayRequest D_8010FB10;

extern AnimationPlayRequest D_8010FB24;

extern AnimationPlayRequest Gp_WeaponMsgRec;

static const TaskFuncTable3 D_800974C8;

void Gp_EvtCapWeaponTask(Task* arg0);

static void func_800E4020(Task* task);

TaskDesc Gp_EvtSpawnTable[3] = {
    { { { TASK_BODY_NONE, 32 } }, Gp_EvtCapTask, { NULL } },
    { { { TASK_BODY_NONE, 32 } }, Gp_EvtCapWeaponTask, { NULL } },
    { { { TASK_DESC_END, 0 } }, NULL, { NULL } },
};

AnimationPlayRequest D_8010FB10 = { { 1 }, 32, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_8010FB24 = { { 1 }, 33, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest Gp_WeaponMsgRec = { { 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

TaskDesc D_8010FB4C[3] = {
    { { { TASK_BODY_NONE, 32 } }, capClearUnstartedSequenceTask, { NULL } },
    { { { TASK_BODY_NONE, 32 } }, Gp_DelayedMsgTask, { NULL } },
    { { { TASK_DESC_END, 0 } }, NULL, { NULL } },
};

TextGlyphCell D_8010FB70[4] = {
    { 16, 96, 16, 16 },
    { 32, 96, 16, 16 },
    { 16, 112, 16, 16 },
    { 32, 112, 16, 16 },
};

s32 D_8010FB80 = 8;

s32 D_8010FB84 = 0;

s32 Gp_CapCaretGrey = 8;

s32 Gp_CapCaretDir = 0;

static const TaskFuncTable3 D_800974C8 = { {
    func_800E31E8,
    func_800E4020,
    taskKill,
} };

void Gp_RunCapCmd(s32 arg0, s16 arg1)
{
    CapCommand* command;
    s32         flagId;
    s32         val;
    s32         i;

    for (;;) {
        command = Gp_CapCmds[arg0].command;
        flagId  = command->flagIndexLo | (command->flagIndexHi << 8);
        switch (command->opcode) {
            case CAP_COMMAND_PLAIN:
                Gp_StartCapSlot(arg0, arg1, 0);
                return;
            case CAP_COMMAND_COUNTER:
                // Persist keeps the counter in a game flag. Otherwise it is this command's byte.
                if (command->flags & CAP_COMMAND_PERSIST) {
                    val = gameFlagGetNibble(flagId);
                } else {
                    val = command->counter;
                }
                // Above the limit, BRANCH continues at nextIndex without playing or advancing.
                if ((command->flags & CAP_COMMAND_BRANCH) && command->counterLimit < val) {
                    arg0 = command->nextIndex;
                    continue;
                }
                Gp_StartCapSlot(arg0, arg1, val);
                if ((val < command->counterLimit) || (command->flags & CAP_COMMAND_BRANCH)) {
                    val++;
                } else if (command->flags & CAP_COMMAND_WRAP) {
                    val = 0;
                }
                if (command->flags & CAP_COMMAND_PERSIST) {
                    gameFlagSetNibble(flagId, val);
                } else {
                    command->counter = val;
                }
                return;
            case CAP_COMMAND_FLAG:
                val = gameFlagGetNibble(flagId);
                Gp_StartCapSlot(arg0, arg1, val);
                return;
            case CAP_COMMAND_ROOM:
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_COMMAND, arg0, 0);
                return;
            case CAP_COMMAND_TALLY:
                // Count two-bit flags whose value is 0, 1 or 3.
                val = 0;
                for (i = 0; i < command->bitFlagCount; i++) {
                    if (Gp_GetCurBit2Flag(command->bitFlagIndex + i) == 0 ||
                        Gp_GetCurBit2Flag(command->bitFlagIndex + i) == 1 ||
                        Gp_GetCurBit2Flag(command->bitFlagIndex + i) == 3) {
                        val++;
                    }
                }
                if ((command->flags & CAP_COMMAND_BRANCH) && val == 0) {
                    arg0 = command->nextIndex;
                    continue;
                }
                Gp_StartCapSlot(arg0, arg1, val);
                return;
            default:
                return;
        }
    }
}

void Gp_EvtCapWeaponTask(Task* arg0)
{
    s32                  flags;
    GameActor*           actor;
    s32                  mode;
    AnimationPlayRequest recB;
    AnimationPlayRequest recA;

    flags = arg0->spawnArg2.value;
    actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    switch (arg0->state) {
        case 0:
            if ((flags & 1) && (flags != 0xFF)) {
                recA              = Gp_WeaponMsgRec;
                recA.source.index = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &recA, 0);
            }
            recB              = D_8010FB10;
            recB.source.index = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3FA, 0, 0);
            arg0->state++;
            break;
        case 1:
            arg0->state++;
            break;
        case 2:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                arg0->state++;
            }
            if (actor->mode != GAME_ACTOR_MODE_SCRIPTED) {
                taskKill(arg0);
            }
            break;
        case 3:
            if ((flags & 1) && (flags != 0xFF)) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            }
            if ((flags & 2) && (flags != 0xFF)) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            }
            if ((flags & 4) && (flags != 0xFF)) {
                mode = 2;
            } else if ((flags & 1) == 0) {
                mode = 3;
            } else {
                mode = 0;
            }
            if (flags == 0xFF) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_COMMAND, arg0->spawnArg1.value, mode);
            } else {
                Gp_RunCapCmd(arg0->spawnArg1.value, mode);
            }
            arg0->state++;
            break;
        case 4:
            if (capIsBusy() == 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                arg0->state++;
            }
            break;
        case 5:
            if (D_80115598 != 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_SOUND, arg0->spawnArg2.value + 0x64, 0);
            }
            recB              = D_8010FB24;
            recB.source.index = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3FA, 1, 0);
            arg0->state++;
            break;
        case 6:
            arg0->state++;
            break;
        case 7:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                arg0->state++;
            }
            if (actor->mode != GAME_ACTOR_MODE_SCRIPTED) {
                taskKill(arg0);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            }
            break;
        case 8:
            taskKill(arg0);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            break;
    }
}

const char          Gp_StrCapMagic[] = "CAP";
const CapTextLayout D_80097518       = { 0 };
const char          Gp_StrEvsFmt[]   = "evs%d_%d_%d.txt";

const TaskFuncTable3 Gp_CapTaskStates = { {
    Gp_InitCapTask,
    Gp_CapTaskState1,
    taskKill,
} };

void Gp_SetNibbleIf(s32 arg0, s32 arg1)
{
    if (arg0 != 0) {
        gameFlagSetNibble(arg0, arg1);
    }
}

void Gp_RunCapCmd1(s32 arg0)
{
    Gp_RunCapCmd(arg0, 1);
}

void Gp_MsgPlayer3F3(s32 arg0)
{
    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

void Gp_MsgPlayerWeapon(s32 arg0)
{
    AnimationPlayRequest sp;

    if (arg0 == 0) {
        sp              = Gp_WeaponMsgRec;
        sp.source.index = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &sp, 0);
    } else {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
    }
}

void Gp_MsgSlot4Chain(s32 arg0, s32 arg1)
{
    Task* out;

    arg0 = (arg0 << ENEMY_PLACE_INDEX_SHIFT) | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | gGameSession->location.loc.area;
    TASK_MESSAGE_DISPATCH_SECOND_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_FIND_PLACED_ACTOR, arg0, &out);
    if (out != 0) {
        taskMessageDispatch(out, ACTOR_MESSAGE_SET_MODEL_DRAW, arg1, 0);
    }
}

void Gp_PlayerWeaponId(s32* arg0)
{
    *arg0 = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
}

void Gp_AllyAnimId(s32* arg0)
{
    *arg0 = Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant;
}

void Gp_FillPlayerHpMp(void)
{
    PlayerStatus* p;

    p     = &gPlayerStatus;
    p->hp = p->hpMax;
    p->mp = p->mpMax;
}

void Gp_FillAllyHp(void)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHpMax;
}

void Gp_SpawnIfCapIdle(s32 arg0, s32 arg1)
{
    if (capIsBusy() == 0) {
        taskSpawnFromTable(Gp_EvtSpawnTable, 0, arg1, arg0);
    }
}

void Gp_EnqueueStageSnd6(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= gGameSession->location.loc.stage << 24;
    }
    sndEvtRequestScriptStart(arg0, (s8)arg1, (s8)arg2);
}

s32 Gp_PackStageSndId(s32 arg0)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= gGameSession->location.loc.stage << 24;
    }
    return arg0;
}

void Gp_EnqueueStageSnd7(s32 arg0, s32 arg1)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= gGameSession->location.loc.stage << 24;
    }
    sndEvtRequestScriptStop(arg0, arg1);
}

void Gp_MsgAlly3F3(s32 arg0)
{
    Task* slot;

    slot = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (slot != NULL) {
        taskMessageDispatch(slot, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
    }
}

void Gp_MsgAllyWeapon(s32 arg0)
{
    Task*                slot;
    AnimationPlayRequest sp;

    slot = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (slot != NULL) {
        if (arg0 == 0) {
            sp              = Gp_WeaponMsgRec;
            sp.source.index = Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant;
            TASK_MESSAGE_DISPATCH_POINTER(slot, ANIMATION_MESSAGE_PLAY, &sp, 0);
        } else {
            taskMessageDispatch(slot, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        }
    }
}

void func_800E3FAC(s32 arg0, s32 arg1)
{
    gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.packedFlags[arg0 / 2] = arg1;
}

s32 func_800E3FCC(s32 arg0)
{
    return gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.packedFlags[arg0 / 2];
}

/// Location-message fallback of `D_8010FAD4`, the table installed on pointer
/// slot 7: copies the requested location onto the outgoing record and answers
/// 1, leaving the decision to whoever reads the reply.
s32 func_800E3FF0(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    return 1;
}

s32 func_800E4018(Task* task, s32 msgId, s32 firstArg, s32 secondArg)
{
    return 0;
}

static void func_800E4020(Task* task)
{
}

void func_800E4028(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_800974C8;
    sp.funcs[arg0->state](arg0);
}

void Gp_ClearAllFlagNibbles(void)
{
    s32 i;

    for (i = 0; i < GAME_FLAG_NIBBLE_COUNT; i++) {
        gameFlagSetNibble(i, 0);
    }
}

void Gp_SpawnEvt1(s32 arg0, s32 arg1)
{
    taskSpawnFromTable(Gp_EvtSpawnTable, 1, arg0, arg1);
}
