#include "gameplay/captions.h"

#include "types.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/cap.h"
#include "cap.h"
#include "captions.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/gameflag.h"
#include "gameflag.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "object_task.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "player_actor.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sound.h"

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

/// Stage nibble position in a packed sound-bank id; zero selects an absolute id.
enum {
    SOUND_STAGE_NIBBLE_MASK  = 0x0F000000,
    SOUND_STAGE_NIBBLE_SHIFT = 24
};
/// Resolves a nonzero sound-id stage nibble using the live current session.
///
/// `soundId` must be a writable s32 lvalue with no evaluation side effects:
/// it is evaluated repeatedly. A zero stage nibble leaves the value unchanged.
/// Other bits are retained; substitution requires a live `gGameSession` and
/// a stage that fits four bits. Use as a standalone compound statement.
#define SOUND_RESOLVE_CURRENT_STAGE(soundId)                                           \
    {                                                                                  \
        if ((soundId) & SOUND_STAGE_NIBBLE_MASK) {                                     \
            (soundId) &= ~SOUND_STAGE_NIBBLE_MASK;                                     \
            (soundId) |= gGameSession->location.loc.stage << SOUND_STAGE_NIBBLE_SHIFT; \
        }                                                                              \
    }

static const TaskFuncTable3 D_800974C8;

void Gp_EvtCapWeaponTask(Task* arg0);

static void _objectTaskRoomIdleState(Task* unusedTask);

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
    { { { TASK_BODY_NONE, 32 } }, capDelayedTextureMessageTask, { NULL } },
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
    _objectTaskRoomIdleState,
    taskKill,
} };

void capRunCommand(s32 commandIndex, s16 playbackMode)
{
    /// Advances and stores a CAP counter after the playback attempt.
    ///
    /// `command` must be a side-effect-free pointer expression and `variantKey`
    /// a writable s32 lvalue: both are evaluated repeatedly. `flagId` is evaluated
    /// once only for a persisted counter. Byte/nibble stores retain truncation.
    /// Use as a standalone compound statement; the macro is undefined after this function.
#define CAP_ADVANCE_COMMAND_COUNTER(command, flagId, variantKey)                                   \
    {                                                                                              \
        if (((variantKey) < (command)->counterLimit) || ((command)->flags & CAP_COMMAND_BRANCH)) { \
            (variantKey)++;                                                                        \
        } else if ((command)->flags & CAP_COMMAND_WRAP) {                                          \
            (variantKey) = 0;                                                                      \
        }                                                                                          \
        if ((command)->flags & CAP_COMMAND_PERSIST) {                                              \
            gameFlagSetNibble((flagId), (variantKey));                                             \
        } else {                                                                                   \
            (command)->counter = (variantKey);                                                     \
        }                                                                                          \
    }

    enum { CAP_COMMAND_DEFAULT_VARIANT = 0 };

    CapCommand* command;
    s32         flagId;
    s32         variantKey;
    s32         objectOffset;

    for (;;) {
        command = Gp_CapCmds[commandIndex].command;
        flagId  = command->flagIndexLo | (command->flagIndexHi << 8);
        switch (command->opcode) {
            case CAP_COMMAND_PLAIN:
                capStartSequenceSlot(commandIndex, playbackMode, CAP_COMMAND_DEFAULT_VARIANT);
                return;
            case CAP_COMMAND_COUNTER:
                // Select the variant from a live flag or the loaded command's counter.
                if (command->flags & CAP_COMMAND_PERSIST) {
                    variantKey = gameFlagGetNibble(flagId);
                } else {
                    variantKey = command->counter;
                }
                // An overflow branch skips both playback and counter advancement.
                if ((command->flags & CAP_COMMAND_BRANCH) && command->counterLimit < variantKey) {
                    commandIndex = command->nextIndex;
                    continue;
                }
                capStartSequenceSlot(commandIndex, playbackMode, variantKey);
                CAP_ADVANCE_COMMAND_COUNTER(command, flagId, variantKey);
                return;
            case CAP_COMMAND_FLAG:
                variantKey = gameFlagGetNibble(flagId);
                capStartSequenceSlot(commandIndex, playbackMode, variantKey);
                return;
            case CAP_COMMAND_ROOM:
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_COMMAND, commandIndex, 0);
                return;
            case CAP_COMMAND_TALLY:
                // Count the requested run of two-bit object states equal to 0, 1 or 3.
                variantKey = 0;
                for (objectOffset = 0; objectOffset < command->bitFlagCount; objectOffset++) {
                    if (areaGetCurrentObjectState(command->bitFlagIndex + objectOffset) == 0 ||
                        areaGetCurrentObjectState(command->bitFlagIndex + objectOffset) == 1 ||
                        areaGetCurrentObjectState(command->bitFlagIndex + objectOffset) == 3) {
                        variantKey++;
                    }
                }
                if ((command->flags & CAP_COMMAND_BRANCH) && variantKey == 0) {
                    commandIndex = command->nextIndex;
                    continue;
                }
                capStartSequenceSlot(commandIndex, playbackMode, variantKey);
                return;
            default:
                return;
        }
    }
}

#undef CAP_ADVANCE_COMMAND_COUNTER

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
                capRunCommand(arg0->spawnArg1.value, mode);
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
    capUpdateControlTask,
    taskKill,
} };

void gameFlagSetNibbleIfPresent(s32 optionalFlagId, s32 value)
{
    if (optionalFlagId != GAME_FLAG_OPTIONAL_NONE) {
        gameFlagSetNibble(optionalFlagId, value);
    }
}

void capRunCommandWithTransition(s32 commandIndex)
{
    capRunCommand(commandIndex, CAP_PLAYBACK_DISPLAY_TRANSITION);
}

void playerActorSetDrawMode(s32 drawMode)
{
    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
}

void playerActorSetScriptedControl(s32 resume)
{
    AnimationPlayRequest request;

    if (resume == GAME_ACTOR_SCRIPTED_CONTROL_HOLD) {
        request              = Gp_WeaponMsgRec;
        request.source.index = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
    } else {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
    }
}

void sceneSetPlacedActorDrawMode(s32 placeIndex, s32 drawMode)
{
    Task* actorTask;
    s32   placeKey;

    placeKey = (placeIndex << ENEMY_PLACE_INDEX_SHIFT) | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | gGameSession->location.loc.area;
    TASK_MESSAGE_DISPATCH_SECOND_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_FIND_PLACED_ACTOR, placeKey, &actorTask);
    if (actorTask != NULL) {
        taskMessageDispatch(actorTask, ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
    }
}

void playerActorWriteWeaponAnimationBankIndex(s32* bankIndexOut)
{
    *bankIndexOut = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
}

void companionWriteAnimationBankIndex(s32* bankIndexOut)
{
    *bankIndexOut = Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant;
}

void playerStateRestoreFullHpMp(void)
{
    PlayerStatus* status;

    status     = &gPlayerStatus;
    status->hp = status->hpMax;
    status->mp = status->mpMax;
}

void companionRestoreFullHp(void)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHpMax;
}

void capSpawnEventIfIdle(s32 commandIndex, s32 eventFlags)
{
    enum { CAP_EVENT_TASK_NORMAL = 0 };

    if (capIsBusy() == 0) {
        taskSpawnFromTable(Gp_EvtSpawnTable, CAP_EVENT_TASK_NORMAL, eventFlags, commandIndex);
    }
}

void sndEvtRequestStageScriptStart(s32 soundId, s32 panOffset, s32 attenuation)
{
    SOUND_RESOLVE_CURRENT_STAGE(soundId);
    sndEvtRequestScriptStart(soundId, (s8)panOffset, (s8)attenuation);
}

s32 sndScriptResolveStageId(s32 soundId)
{
    SOUND_RESOLVE_CURRENT_STAGE(soundId);
    return soundId;
}

void sndEvtRequestStageScriptStop(s32 soundSelector, s32 stopControl)
{
    SOUND_RESOLVE_CURRENT_STAGE(soundSelector);
    sndEvtRequestScriptStop(soundSelector, stopControl);
}

#undef SOUND_RESOLVE_CURRENT_STAGE

void companionSetDrawMode(s32 drawMode)
{
    Task* companionTask;

    companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (companionTask != NULL) {
        taskMessageDispatch(companionTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
    }
}

void companionSetScriptedControl(s32 resume)
{
    Task*                companionTask;
    AnimationPlayRequest request;

    companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (companionTask != NULL) {
        if (resume == GAME_ACTOR_SCRIPTED_CONTROL_HOLD) {
            request              = Gp_WeaponMsgRec;
            request.source.index = Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant;
            TASK_MESSAGE_DISPATCH_POINTER(companionTask, ANIMATION_MESSAGE_PLAY, &request, 0);
        } else {
            taskMessageDispatch(companionTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        }
    }
}

void gameFlagSetPackedByte(s32 nibbleIndex, s32 packedValue)
{
    gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.packedFlags[nibbleIndex / 2] = packedValue;
}

s32 gameFlagGetPackedByte(s32 nibbleIndex)
{
    return gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.packedFlags[nibbleIndex / 2];
}

s32 objectTaskResolveDefaultRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { OBJECT_TASK_ROOM_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return OBJECT_TASK_ROOM_TRANSITION_ALLOWED;
}

s32 objectTaskRefuseDefaultRoomKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Keeps the room-selector task alive after setup, including its fallback message receiver.
static void _objectTaskRoomIdleState(Task* unusedTask)
{
}

void objectTaskRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_800974C8;
    stateHandlers.funcs[task->state](task);
}

void gameFlagClearLiveNibbles(void)
{
    s32 flagId;

    for (flagId = 0; flagId < GAME_FLAG_NIBBLE_COUNT; flagId++) {
        gameFlagSetNibble(flagId, 0);
    }
}

void Gp_SpawnEvt1(s32 arg0, s32 arg1)
{
    taskSpawnFromTable(Gp_EvtSpawnTable, 1, arg0, arg1);
}
