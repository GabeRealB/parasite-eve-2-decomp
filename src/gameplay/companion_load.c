#include "gameplay/companion_load.h"
#include "rooms/acropolis_plaza.h"

#include "common.h"

#include "gameplay/collision.h"
#include "companion_load.h"
#include "gameplay/hud_sprites.h"
#include "loading.h"
#include "gameplay/message.h"
#include "player_state.h"
#include "gameplay/room.h"
#include "scene_runtime.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_collision.h"
#include "world_collision.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

/// One schedule of a companion: the stage it appears in, and in which of that
/// stage's areas.
///
/// Each companion family has a table of these, and the family's
/// `GAME_FLAG_COMPANION_1_SCHEDULE` / `_2_` / `_3_` nibble selects the schedule
/// in force. Schedule 0 places the companion nowhere.
///
/// Families 2 and 3 treat a presence byte as a whole. Family 1 keeps presence
/// in the low nibble and the `companionVariant` that appears in the high one.
typedef struct {
    u8* areaPresence; // One byte per area of `stage`, indexed by area ID - 1 (0 absent); `NULL` when the companion appears nowhere
    u8  stage;        // `GAME_STAGE_*` whose areas `areaPresence` lists
} _CompanionSchedule;
STATIC_ASSERT_SIZEOF(_CompanionSchedule, 8);

extern _CompanionSchedule D_80114198[];

extern _CompanionSchedule D_801141F0[];

extern _CompanionSchedule D_80114248[];

extern u8 D_80114258[38];

extern u8 D_80114280[38];

extern u8 D_801142A8[38];

extern u8 D_801142D0[49];

extern u8 D_80114304[49];

extern u8 D_80114338[38];

extern u8 D_80114360[38];

extern u8 D_80114388[38];

extern u8 D_801143B0[38];

extern u8 D_801143D8[38];

extern u8 D_80114400[38];

extern u8 D_80114428[33];

extern u8 D_8011444C[33];

extern u8 D_80114470[49];

extern u8 D_801144A4[49];

extern u8 D_801144D8[49];

extern u8 D_8011450C[33];

extern u8 D_80114530[49];

/// 33 room flags, then the unexplained 3D F0 71 tail.
/// The tail is not a room-mask entry.
extern u8 D_80114564[36];

static void Gp_ClearFlagBank(s32 arg0);

void func_80724E2C(void);

_CompanionSchedule D_80114198[11] = {
    { NULL, GAME_STAGE_NONE },
    { D_80114360, GAME_STAGE_DRYFIELD },
    { D_80114388, GAME_STAGE_DRYFIELD },
    { D_801143B0, GAME_STAGE_DRYFIELD },
    { D_80114388, GAME_STAGE_DRYFIELD },
    { D_801143D8, GAME_STAGE_DRYFIELD_NIGHT },
    { D_80114388, GAME_STAGE_DRYFIELD_NIGHT },
    { D_80114400, GAME_STAGE_DRYFIELD_NIGHT },
    { D_80114428, GAME_STAGE_SHELTER_NEO_ARK },
    { D_8011444C, GAME_STAGE_SHELTER_NEO_ARK },
    { D_80114470, GAME_STAGE_MINE_SHELTER },
};
_CompanionSchedule D_801141F0[11] = {
    { NULL, GAME_STAGE_NONE },
    { D_80114258, GAME_STAGE_DRYFIELD_NIGHT },
    { D_80114280, GAME_STAGE_DRYFIELD_NIGHT },
    { D_801142A8, GAME_STAGE_DRYFIELD_NIGHT },
    { D_801142D0, GAME_STAGE_MINE_SHELTER },
    { D_80114304, GAME_STAGE_MINE_SHELTER },
    { D_80114338, GAME_STAGE_DRYFIELD_NIGHT },
    { D_801144A4, GAME_STAGE_MINE_SHELTER },
    { D_801144D8, GAME_STAGE_MINE_SHELTER },
    { D_8011450C, GAME_STAGE_SHELTER_NEO_ARK },
    { D_80114530, GAME_STAGE_MINE_SHELTER },
};
_CompanionSchedule D_80114248[2] = {
    { NULL, GAME_STAGE_NONE },
    { D_80114564, GAME_STAGE_SHELTER_NEO_ARK },
};
u8 D_80114258[38] = { 17, 0, 17, 0, 17, 17, 17, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114280[38] = { 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801142A8[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801142D0[49] = { 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114304[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 49, 49, 49, 49, 49, 49, 0, 0, 0 };
u8 D_80114338[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 0, 0, 0, 0, 0, 0 };
u8 D_80114360[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114388[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801143B0[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801143D8[38] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114400[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114428[33] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_8011444C[33] = { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114470[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801144A4[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0 };
u8 D_801144D8[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 33, 33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0 };
u8 D_8011450C[33] = { 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114530[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 65, 65, 65, 65, 65, 65, 0, 0, 0 };
/// 33 room flags, then the unexplained 3D F0 71 tail.
/// The tail is not a room-mask entry.
u8 D_80114564[36] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 61, 240, 113 };

s32 Gp_PickCompanion(void)
{
    McSaveData* save;
    u8*         areaPresence;
    s32         stage;
    u8          variant;

    save         = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    stage        = save->state.location.loc.stage;
    areaPresence = D_80114198[gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE)].areaPresence;
    if (areaPresence != NULL && D_80114198[gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE)].stage == stage && areaPresence[save->state.location.loc.area - 1] != 0) {
        GameSession* sess = gGameSession;

        save->state.companionType    = 2;
        save->state.companionVariant = 0;
        return (sess->companionType != 2) * 2;
    }

    areaPresence = D_801141F0[gameFlagGetNibble(GAME_FLAG_COMPANION_1_SCHEDULE)].areaPresence;
    if (areaPresence != NULL && D_801141F0[gameFlagGetNibble(GAME_FLAG_COMPANION_1_SCHEDULE)].stage == stage && (areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area - 1] & 0xF)) {
        GameSession* sess = gGameSession;

        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType = 1;
        if (sess->companionType == 1) {
            variant = areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area - 1] >> 4;
            if (sess->companionVariant == variant) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = variant;
                return 0;
            }
        }
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area - 1] >> 4;
        gGameSession->companionVariant                            = areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area - 1] >> 4;
        return 1;
    }

    areaPresence = D_80114248[gameFlagGetNibble(GAME_FLAG_COMPANION_3_SCHEDULE)].areaPresence;
    if (areaPresence != NULL && D_80114248[gameFlagGetNibble(GAME_FLAG_COMPANION_3_SCHEDULE)].stage == stage && areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area - 1] != 0) {
        GameSession* sess = gGameSession;

        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType    = 3;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = 0;
        if (sess->companionType == 3) {
            return 0;
        }
        return 3;
    }

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType    = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = 0;
    gGameSession->companionType                               = 0;
    gGameSession->companionVariant                            = 0;
    return 0;
}

void Gp_ApplyNpcRoomSnd(void)
{
    McSaveData* save;
    u8*         areaPresence;
    s32         stage;
    s32         flag;

    save  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    stage = save->state.location.loc.stage;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(3, 32, 0, 0)) {
        areaPresence = D_80114198[gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE)].areaPresence;
        if (areaPresence != NULL) {
            if (D_80114198[gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE)].stage == stage) {
                if (areaPresence[save->state.location.loc.area - 1] != 0) {
                    flag = 1;
                    goto done;
                }
            }
        }
        areaPresence = D_801141F0[gameFlagGetNibble(GAME_FLAG_COMPANION_1_SCHEDULE)].areaPresence;
        if (areaPresence != NULL) {
            if (D_801141F0[gameFlagGetNibble(GAME_FLAG_COMPANION_1_SCHEDULE)].stage == stage) {
                if (areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area - 1] & 0xF) {
                    flag = 1;
                    goto done;
                }
            }
        }
        areaPresence = D_80114248[gameFlagGetNibble(GAME_FLAG_COMPANION_3_SCHEDULE)].areaPresence;
        if (areaPresence != NULL) {
            if (D_80114248[gameFlagGetNibble(GAME_FLAG_COMPANION_3_SCHEDULE)].stage == stage) {
                if (areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area - 1] != 0) {
                    flag = 1;
                    goto done;
                }
            }
        }
    }
    flag = 0;
done:
    Snd_SetModeFlag(flag);
}

void Gp_SetupCompanionActor(const ActorSpawnTransform* spawnTransform, ActorSpawnOptions* options)
{
    McSaveData* save;
    s32         field;

    save  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    field = save->state.companionType;
    if (field != 0) {
        if (field == 2) {
            Gp_SpawnAlly(spawnTransform, save->state.companionType, gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE), options);
        } else {
            Gp_SpawnAlly(spawnTransform, field, 0, options);
        }
    }
}

static void Gp_ClearFlagBank(s32 arg0)
{
    GameFlagStageHeader* bank;

    bank                  = Gp_FlagBanks[arg0];
    bank->visitedAreas[0] = 0;
    bank->visitedAreas[1] = 0;
}

void Gp_MarkAreaVisited(GameLocationKey* arg0)
{
    McSaveData*          save;
    GameFlagStageHeader* bank;
    s32                  which;
    s32                  bit;
    s32                  mask;
    s32                  flags;

    bank = Gp_FlagBanks[arg0->stage];
    save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    if ((((s8)save->state.visitFlags >> arg0->stage) & 1) == 0) {
        save->state.visitFlags |= 1 << arg0->stage;
        if (gDisplayState.debugMode != 0) {
            func_80724E2C();
        }
    }

    which = 0;
    if (arg0->area >= 0x21) {
        which = 1;
        bit   = arg0->area - 0x21;
    } else {
        bit = arg0->area - 1;
    }

    mask  = 1;
    flags = bank->visitedAreas[which];
    if (((mask << bit) & flags) == 0) {
        bank->visitedAreas[which] = flags | (mask << bit);
        Gp_SetAreaFlag0(arg0);
    }
}

void func_800ABFF8(void)
{
}

void func_800AC000(void)
{
}

void Gp_SessionState1(Task* task)
{
    DisplayState* ds;
    s32           temp;

    ds             = &gDisplayState;
    ds->skipDraw   = 1;
    ds->holdState |= DISPLAY_HOLD_ACTIVE;
    temp           = task->spawnArg1.value & 0xF;
    if (temp != 0) {
        if (temp == 1) {
            ds->control.flags.imageSource = DISPLAY_IMAGE_NONE;
        }
    }
    task->state++;
}

void Gp_ResumeSessionTask(Task* task)
{
    SndBank_SetEnableFlags(0, 0x40000000);
    if (gGameSession->deathVariant != 0) {
        taskKill(task);
        return;
    }
    if ((task->spawnArg1.value & 0x10) == 0) {
        if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_FINISHED) {
            gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_RESUMED;
        }
        Gp_TriggerPeIfArmed();
    }
    task->state++;
}

void func_800AC0F0(Task* task)
{
    TaskFuncTable3 sp;

    sp = Gp_SessionStates;
    Pad_SetCooldown(0);
    *(volatile u8*)&gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
    sp.funcs[((volatile Task*)task)->state](task);
}

void Gp_LoadFinishTask(Task* task)
{
    if (gCdCmdQueue.bootLoadActive == 0) {
        gpuClearFrameOrderingTable(0);
        gpuClearFrameOrderingTable(1);
        Pad_RemapState->loadingActive = GAME_DEBUG_LOADING_IDLE;
        taskKill(task);
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 5, 0, 0)) {
            func_800AA548(1);
        } else {
            func_800AA548(0);
        }
        gDisplayState.holdCount  = 0;
        gDisplayState.holdState &= DISPLAY_HOLD_MODE_MASK;
        Display_AcquireRef();
        Task_Spawn(0, 0x21, 0, 0);
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 5, 0, 0)) {
            taskSpawnFromTable(D_acropolis_plaza_80183824, 0, 0, 0);
            cdCmdReservePlaybackBuffers();
            CdCmd_SelectMdecBuffer();
        }
    }
}

void Gp_LoadStateTask(Task* task)
{
    TaskFuncTable8 sp;
    DisplayState*  ds;

    sp = Gp_LoadStateFns;
    Pad_SetCooldown(0);
    ds = &gDisplayState;
    if (ds->demoScene != DISPLAY_DEMO_NONE) {
        if (Pad_ReadButtonsInv(0) & 0x800) {
            if (CdCmd_IsIdle() & 0xFFFF) {
                Wip_SysFlags.skipTitleIntro = 1;
                ds->gameMode                = DISPLAY_GAME_RESTART;
                return;
            }
        }
    }
    sp.funcs[task->state](task);
}

void Gp_FlashWhiteTask(Task* task)
{
    CdCmdQueue* queue;
    u8          fade;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            task->killCountdown = 0;
            task->state++;
        case 1:
            fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
            task->killCountdown++;
            if (task->killCountdown < 3) {
                return;
            }
            task->killCountdown = 0xFF;
            task->state++;
            break;
        case 2:
            fade = task->killCountdown;
            fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
            task->killCountdown -= 0x1E;
            if (task->killCountdown > 0) {
                return;
            }
            if ((s16)queue->releasePauseBlockAfterFade != 0) {
                queue->releasePauseBlockAfterFade = 0;
                queue->blockGamePause             = 0;
            }
            displayReleaseMenuHold();
            taskKill(task);
            break;
    }
}

s32 taskMessageDispatch(Task* receiver, s32 messageId, s32 firstArg, s32 secondArg)
{
    const TaskMessageEntry* entry;

    if (receiver->msgTable == NULL) {
        return 0;
    }
    entry = receiver->msgTable;

    // Match the ID before testing the reserved end marker.
    if (entry->messageId != messageId) {
        do {
            if (entry->messageId == TASK_MESSAGE_TABLE_END) {
                return 0;
            }
            entry++;
        } while (entry->messageId != messageId);
    }
    return entry->handler(receiver, messageId, firstArg, secondArg);
}

void Gp_LinkRoomObjectsSpawn(Task* task)
{
    GameLocationKey*                   sess;
    const WorldCollisionRoomResources* roomResources;
    WorldCollisionGrid*                grid;
    WorldCollisionTrigger*             viewBoundaryTriggers;
    WorldCollisionTrigger*             actionTriggers;
    WorldCollisionOccluder*            occluders;
    s32                                i;
    Task*                              spawned;

    sess          = &gGameSession->location.loc;
    roomResources = Gp_RoomObjTables[sess->stage - 1]->areaRooms[sess->area - 1];
    if (roomResources != NULL) {
        grid                 = roomResources[sess->room - 1].grid;
        viewBoundaryTriggers = roomResources[sess->room - 1].viewBoundaryTriggers;
        actionTriggers       = roomResources[sess->room - 1].actionTriggers;
        occluders            = roomResources[sess->room - 1].occluders;
        if (grid != NULL) {
            // Bind the room mesh to the current view before publishing it.
            grid->viewCoord = &gGfxViewCoord;
            Gp_GridParams   = grid;
        }
        if (viewBoundaryTriggers != NULL) {
            for (i = 0;; i++) {
                viewBoundaryTriggers[i].coord = &gGfxViewCoord;
                Gp_LinkObj4A(1, &viewBoundaryTriggers[i]);
                viewBoundaryTriggers[i].flags |= WORLD_COLLISION_TRIGGER_ENABLED;
                if (viewBoundaryTriggers[i].flags & WORLD_COLLISION_TRIGGER_LAST) {
                    break;
                }
            }
        }
        if (actionTriggers != NULL) {
            for (i = 0;; i++) {
                actionTriggers[i].coord = &gGfxViewCoord;
                Gp_LinkObj4A(0, &actionTriggers[i]);
                actionTriggers[i].flags |= WORLD_COLLISION_TRIGGER_ENABLED;
                if (actionTriggers[i].flags & WORLD_COLLISION_TRIGGER_LAST) {
                    break;
                }
            }
        }
        if (occluders != NULL) {
            for (i = 0;; i++) {
                Gp_LinkObj3A(0, &occluders[i]);
                occluders[i].flags |= WORLD_COLLISION_OCCLUDER_ENABLED;
                if (occluders[i].flags & WORLD_COLLISION_OCCLUDER_LAST) {
                    break;
                }
            }
        }
    }
    spawned = Task_Spawn(0, 0x1B, 0, 0);
    if (spawned != NULL) {
        taskReparent(task, spawned);
    }
    gGameSession->roomObjsDirty = 0;
    task->state++;
}
