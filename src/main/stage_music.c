#include "types.h"

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "sound.h"
#include "main/stage_types.h"
#include "main/task.h"
#include "task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

/* Define BSS before API headers to preserve first-declaration order. */
static u8 Stage_MusicCountdownActive;

static s16 Stage_MusicCountdownFrames;

StageMusicParams gStageMusicParams;

#include "main/stage.h"

extern TmdSource D_80725F44;

/// Nonzero while the 0x60010001 ambient sound, started by a table entry of
/// 0x80, is playing.
static u8 gStageAmbientOn;

/// Where the current scene's rows begin in the stage's music table.
static u8 gStageMusicRow;

/// The song last started from the music table.
static u8 gStageCurrentSong;

static TaskIdPair* Stage_MusicTables[];

static TaskIdPair* Stage_CountdownMusicTables[];

static u8 Stage_MusicRowLengths[];

static u8 Stage_SceneEventLimits[];

static const TaskFuncTable4 Stage_TaskStates;

void func_80703FE8(Task* arg0);

void func_80704A78(Task* arg0);

void func_80704AD0(Task* arg0);

void func_80704BC8(Task* arg0);

static void Task_AllocIdMap(Task* task);

static void Stage_LoadOrCountdownTask(Task* task);

static void Stage_ApplyTableEntryWhenIdle(Task* task);

static void Stage_DispatchTaskTable(Task* task);

static void Stage_KillWhenIdle(Task* task);

u8 gStageMusicLoadState  = 0xFF;
u8 gStageSceneMusicEntry = 0;
/// Nonzero while the 0x60010001 ambient sound, started by a table entry of
/// 0x80, is playing.
static u8 gStageAmbientOn = 0;
u8        gStageRoomSong  = 0;
/// Where the current scene's rows begin in the stage's music table.
static u8 gStageMusicRow = 0;
/// The song last started from the music table.
static u8          gStageCurrentSong   = 0;
static TaskIdPair* Stage_MusicTables[] = {
    D_map_akropolis_8017C1B4,
    D_map_dryfield_8017BDE0,
    D_map_dryfield_full_8017D238,
    D_map_shelter_8017BE28,
    D_map_neo_ark_8017CB54,
};
static TaskIdPair* Stage_CountdownMusicTables[] = {
    D_map_akropolis_8017C304,
    D_map_dryfield_8017C004,
    D_map_dryfield_full_8017D594,
    D_map_shelter_8017C2D8,
    D_map_neo_ark_8017CDFC,
};
static u8 Stage_MusicRowLengths[]  = { 8, 7, 0xB, 0xC, 0xA };
static u8 Stage_SceneEventLimits[] = { 9, 8, 0xC, 9, 0x14 };
TaskDesc  Stage_MusicTaskDesc      = { { { TASK_BODY_NONE, 0xC0 } }, Stage_DispatchTaskTable };
TaskDesc  D_80062780[]             = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80704BC8 },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80703FE8 },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80704A78 },
    { { { TASK_BODY_TMD, 0xC0 } }, func_80704AD0, { &D_80725F44 } },
};

static const TaskFuncTable4 Stage_TaskStates = { {
    Task_AllocIdMap,
    Stage_LoadOrCountdownTask,
    Stage_ApplyTableEntryWhenIdle,
    Stage_KillWhenIdle,
} };

static void Task_AllocIdMap(Task* task)
{
    u8         temp_s4;
    u8         temp_s1;
    TaskIdMap* temp_v0;
    u8         temp_a0;
    s32        ret;
    s32        field34;

    temp_s4 = Stage_MusicRowLengths[gGameSession->location.loc.stage - 1];
    temp_v0 = memCalloc(8, 0);
    if (temp_v0 != NULL) {
        task->work = temp_v0;
        if (gStageRoomSong != 0) {
            SndEvt_EnqueueType2(0, 1);
            gStageRoomSong = 0;
        }
        temp_a0                    = gGameSession->location.loc.stage;
        ret                        = TaskIdMap_RemapIndex(temp_a0, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent, Stage_SceneEventLimits[temp_a0 - 1]);
        field34                    = task->spawnArg1.value;
        gStageMusicRow             = ret;
        Stage_MusicCountdownActive = 0;
        if (field34 == 2) {
            s32         f7;
            TaskIdPair* p;
            u16         v;
            f7                         = gGameSession->location.loc.stage;
            Stage_MusicCountdownActive = 0xFF;
            p                          = Stage_CountdownMusicTables[f7 - 1];
            v                          = gStageSceneMusicEntry;
            Stage_MusicCountdownFrames = 0x12C;
            temp_v0->index             = v;
            temp_v0->table             = p;
        } else {
            temp_v0->table = Stage_MusicTables[gGameSession->location.loc.stage - 1];
            temp_v0->index =
                gStageMusicRow + (gGameSession->location.loc.area * (temp_s4 & 0xFF));
            if ((temp_v0->table[gGameSession->location.loc.area * (temp_s4 & 0xFF)].id != 0x80) &&
                (gStageAmbientOn != 0)) {
                SndEvt_EnqueueType7(SOUND_STAGE_AMBIENT, 0x1E);
                gStageAmbientOn = 0;
            }
        }
        temp_s1 = temp_v0->table[temp_v0->index].id;
        if (temp_s1 == 0xFF) {
            SndEvt_EnqueueType2(gStageCurrentSong, gStageMusicParams.fadeFrames);
            gStageMusicLoadState = temp_s1;
            taskKill(task);
            return;
        }
        gStageMusicLoadState = 0;
        if (Midi_IsChannelFree(temp_v0->table[temp_v0->index].id) == 1) {
            if ((gStageCurrentSong != 0) && (Midi_IsBusy(gStageCurrentSong) != 0)) {
                SndEvt_EnqueueType2(gStageCurrentSong, (gStageMusicParams.fadeFrames + 1) & 0xFFFF);
            }
            task->state = task->state + 1;
            return;
        }
        if (temp_v0->table[temp_v0->index].type == 1) {
            SndEvt_EnqueueType2(gStageCurrentSong, (gStageMusicParams.fadeFrames + 1) & 0xFFFF);
            goto block_20;
        }
        if (Midi_IsBusy(gStageCurrentSong) == 0) {
            task->state = task->state + 2;
            return;
        }
    }
block_20:
    gStageMusicLoadState = 0xFF;
    taskKill(task);
}

static void Stage_LoadOrCountdownTask(Task* task)
{
    u8         param1[8];
    u8         param2[8];
    TaskIdMap* temp;
    s32        field34;
    u8         flag;

    temp = task->work;
    if (Midi_IsBusy(gStageCurrentSong) == 0) {
        param1[3] = 0;
        param1[2] = 4;
        param1[0] = temp->table[temp->index].id;
        param2[0] = gGameSession->spriteVariant;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        field34 = task->spawnArg1.value;
        if (field34 == 3) {
            task->state = task->state + 2;
            return;
        }
        if ((temp->table[temp->index].type == 1) && (field34 == 0)) {
            task->state = task->state + 2;
            return;
        }
        task->state = task->state + 1;
        return;
    }
    flag = Stage_MusicCountdownActive;
    if (flag == 0xFF) {
        Stage_MusicCountdownFrames = Stage_MusicCountdownFrames - 1;
        if (Stage_MusicCountdownFrames == 0x3C) {
            SndEvt_EnqueueType2(gStageCurrentSong, 1);
        }
        if (Stage_MusicCountdownFrames <= 0) {
            gStageMusicLoadState = flag;
            taskKill(task);
        }
    }
}

static void Stage_ApplyTableEntryWhenIdle(Task* task)
{
    TaskIdMap*  temp;
    TaskIdPair* entry;
    u8          type;

    temp = task->work;
    if (CdCmd_IsIdle() != 0) {
        entry = (TaskIdPair*)((temp->index << 1) + (u32)temp->table);
        type  = entry->type;
        if (type != 3) {
            if (type != 2) {
                if (task->spawnArg1.value == 0) {
                    if (gGameSession->viewReady != 1) {
                        return;
                    }
                }
            }
            SndEvt_EnqueueType1(entry->id, 0);
            Snd_ApplyVolumeTable(0);
        }
        gStageMusicLoadState = 0xFF;
        gStageCurrentSong    = temp->table[temp->index].id;
        taskKill(task);
    }
}

void Stage_RequestFromAreaTable(s32 arg0)
{
    GameSession* g;
    s32          idx;
    s32          product;
    TaskIdPair*  entry;
    s32          temp;

    g       = gGameSession;
    idx     = g->location.loc.stage - 1;
    product = g->location.loc.area * Stage_MusicRowLengths[idx];
    temp    = (gStageMusicRow + product) & 0xFFFF;
    entry   = Stage_MusicTables[idx];
    if (entry[temp].id != 0xFF) {
        if (entry[temp].type != 3) {
            SndEvt_EnqueueType1(entry[temp].id, arg0 & 0xFFFF);
            gStageCurrentSong = entry[temp].id;
            Snd_ApplyVolumeTable(0);
        }
    }
}

void Stage_RequestMidiFromMap(s32 arg0)
{
    GameSession* g;
    s32          idx;
    s32          product;
    TaskIdPair*  entry;
    s32          temp;

    g       = gGameSession;
    idx     = g->location.loc.stage - 1;
    product = g->location.loc.area * Stage_MusicRowLengths[idx];
    temp    = (gStageMusicRow + product) & 0xFFFF;
    entry   = Stage_MusicTables[idx];
    if (entry[temp].id != 0xFF) {
        if (Midi_IsBusy(entry[temp].id) != 0) {
            SndEvt_EnqueueType2(entry[temp].id, (arg0 + 1) & 0xFFFF);
        }
    }
}

static void Stage_DispatchTaskTable(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = Stage_TaskStates;
    handlers.funcs[task->state](task);
}

static void Stage_KillWhenIdle(Task* task)
{
    if (CdCmd_IsIdle() != 0) {
        gStageMusicLoadState = 0xFF;
        taskKill(task);
    }
}

void Stage_RequestSpecialFlag(s32 unused)
{
    GameSession* g;
    s32          idx;
    s32          product;
    TaskIdPair*  base;
    s32          one;

    g       = gGameSession;
    idx     = g->location.loc.stage - 1;
    product = g->location.loc.area * Stage_MusicRowLengths[idx];
    base    = Stage_MusicTables[idx];
    if (base[product].id == 0x80) {
        if (GameFlag_GetNibble(GAME_FLAG_STAGE_AMBIENT_MUTED) == 1) {
            one = 1;
            SndEvt_EnqueueType7(0x60010000 | one, 0x1E);
            gStageAmbientOn = 0;
        } else if (gStageAmbientOn == 0) {
            one = 1;
            SndEvt_EnqueueType6(SOUND_STAGE_AMBIENT, 0, 0);
            gStageAmbientOn = one;
        }
    }
}
