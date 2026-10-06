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

/// The music-table entry a stage music task loads and starts; its work block.
///
/// The task's first state allocates it zeroed and chooses the entry once: an
/// ordinary room entry takes the stage's area table at the current area's row
/// and the scene's column, while a countdown request takes the stage's
/// countdown table at the entry number the scene selected. The later states
/// only read it. The table belongs to the stage's map overlay, which must stay
/// loaded while the task runs; the block itself is released with the task.
typedef struct {
    u16              index; // Entry within `table`; not checked against the table's length
    StageMusicEntry* table; // Borrowed area or countdown music table of the current stage
} _StageMusicSelection;
STATIC_ASSERT_SIZEOF(_StageMusicSelection, 0x8);

/* Define BSS before API headers to preserve first-declaration order. */
static u8 Stage_MusicCountdownActive;

static s16 Stage_MusicCountdownFrames;

StageMusicParams gStageMusicParams;

#include "main/stage.h"
#include "stage.h"

extern TmdSource D_80725F44;

/// Nonzero while the 0x60010001 ambient sound, started by a table entry of
/// 0x80, is playing.
static u8 gStageAmbientOn;

/// Where the current scene's rows begin in the stage's music table.
static u8 gStageMusicRow;

/// The song last started from the music table.
static u8 gStageCurrentSong;

static StageMusicEntry* Stage_MusicTables[];

static StageMusicEntry* Stage_CountdownMusicTables[];

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
static u8               gStageCurrentSong   = 0;
static StageMusicEntry* Stage_MusicTables[] = {
    D_map_akropolis_8017C1B4,
    D_map_dryfield_8017BDE0,
    D_map_dryfield_full_8017D238,
    D_map_shelter_8017BE28,
    D_map_neo_ark_8017CB54,
};
static StageMusicEntry* Stage_CountdownMusicTables[] = {
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
    u8                    temp_s4;
    u8                    temp_s1;
    _StageMusicSelection* selection;
    u8                    temp_a0;
    s32                   ret;
    s32                   field34;

    temp_s4   = Stage_MusicRowLengths[gGameSession->location.loc.stage - 1];
    selection = memCalloc(sizeof(_StageMusicSelection), 0);
    if (selection == NULL) {
        gStageMusicLoadState = 0xFF;
        taskKill(task);
        return;
    }
    task->work = selection;
    if (gStageRoomSong != 0) {
        SndEvt_EnqueueType2(0, 1);
        gStageRoomSong = 0;
    }
    temp_a0                    = gGameSession->location.loc.stage;
    ret                        = stageMusicSelectColumn(temp_a0, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent, Stage_SceneEventLimits[temp_a0 - 1]);
    field34                    = task->spawnArg1.value;
    gStageMusicRow             = ret;
    Stage_MusicCountdownActive = 0;
    if (field34 == 2) {
        s32              f7;
        StageMusicEntry* p;
        u16              v;
        f7                         = gGameSession->location.loc.stage;
        Stage_MusicCountdownActive = 0xFF;
        p                          = Stage_CountdownMusicTables[f7 - 1];
        v                          = gStageSceneMusicEntry;
        Stage_MusicCountdownFrames = 0x12C;
        selection->index           = v;
        selection->table           = p;
    } else {
        selection->table = Stage_MusicTables[gGameSession->location.loc.stage - 1];
        selection->index =
            gStageMusicRow + (gGameSession->location.loc.area * (temp_s4 & 0xFF));
        if ((selection->table[gGameSession->location.loc.area * (temp_s4 & 0xFF)].sequenceId != STAGE_MUSIC_AMBIENT_AREA) &&
            (gStageAmbientOn != 0)) {
            sndEvtRequestScriptStop(SOUND_STAGE_AMBIENT, 0x1E);
            gStageAmbientOn = 0;
        }
    }
    temp_s1 = selection->table[selection->index].sequenceId;
    if (temp_s1 == STAGE_MUSIC_NO_SEQUENCE) {
        SndEvt_EnqueueType2(gStageCurrentSong, gStageMusicParams.fadeOutTicks);
        gStageMusicLoadState = temp_s1;
        taskKill(task);
        return;
    }
    gStageMusicLoadState = 0;
    if (Midi_IsChannelFree(selection->table[selection->index].sequenceId) == 1) {
        if ((gStageCurrentSong != 0) && (Midi_IsBusy(gStageCurrentSong) != 0)) {
            SndEvt_EnqueueType2(gStageCurrentSong, (gStageMusicParams.fadeOutTicks + 1) & 0xFFFF);
        }
        task->state = task->state + 1;
        return;
    }
    if (selection->table[selection->index].startMode == STAGE_MUSIC_START_DEFERRED) {
        SndEvt_EnqueueType2(gStageCurrentSong, (gStageMusicParams.fadeOutTicks + 1) & 0xFFFF);
    } else if (Midi_IsBusy(gStageCurrentSong) == 0) {
        task->state = task->state + 2;
        return;
    }
    gStageMusicLoadState = 0xFF;
    taskKill(task);
}

static void Stage_LoadOrCountdownTask(Task* task)
{
    u8                    param1[8];
    u8                    param2[8];
    _StageMusicSelection* selection;
    s32                   field34;
    u8                    flag;

    selection = task->work;
    if (Midi_IsBusy(gStageCurrentSong) == 0) {
        param1[3] = 0;
        param1[2] = 4;
        param1[0] = selection->table[selection->index].sequenceId;
        param2[0] = gGameSession->spriteVariant;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        field34 = task->spawnArg1.value;
        if (field34 == 3) {
            task->state = task->state + 2;
            return;
        }
        if ((selection->table[selection->index].startMode == STAGE_MUSIC_START_DEFERRED) && (field34 == 0)) {
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

/// Starts `entry`, the entry `selection` names, unless its start mode holds it
/// back, then records the song and ends the task. A deferred entry leaves the
/// task running until the view is ready.
static inline void Stage_ApplyEntry(Task* task, _StageMusicSelection* selection, StageMusicEntry* entry)
{
    u8 startMode;

    startMode = entry->startMode;
    if (startMode != STAGE_MUSIC_START_NEVER) {
        if (startMode != STAGE_MUSIC_START_IMMEDIATE) {
            if (task->spawnArg1.value == 0) {
                if (gGameSession->viewReady != 1) {
                    return;
                }
            }
        }
        SndEvt_EnqueueType1(entry->sequenceId, 0);
        midiApplyMusicVolume(MIDI_MUSIC_VOLUME_SAVED);
    }
    gStageMusicLoadState = 0xFF;
    gStageCurrentSong    = selection->table[selection->index].sequenceId;
    taskKill(task);
}

static void Stage_ApplyTableEntryWhenIdle(Task* task)
{
    _StageMusicSelection* selection;

    selection = task->work;
    if (CdCmd_IsIdle() != 0) {
        Stage_ApplyEntry(task, selection, &selection->table[selection->index]);
    }
}

void Stage_RequestFromAreaTable(s32 arg0)
{
    GameSession*     g;
    s32              idx;
    s32              product;
    StageMusicEntry* entry;
    s32              temp;

    g       = gGameSession;
    idx     = g->location.loc.stage - 1;
    product = g->location.loc.area * Stage_MusicRowLengths[idx];
    temp    = (gStageMusicRow + product) & 0xFFFF;
    entry   = Stage_MusicTables[idx];
    if (entry[temp].sequenceId != STAGE_MUSIC_NO_SEQUENCE) {
        if (entry[temp].startMode != STAGE_MUSIC_START_NEVER) {
            SndEvt_EnqueueType1(entry[temp].sequenceId, arg0 & 0xFFFF);
            gStageCurrentSong = entry[temp].sequenceId;
            midiApplyMusicVolume(MIDI_MUSIC_VOLUME_SAVED);
        }
    }
}

void Stage_RequestMidiFromMap(s32 arg0)
{
    GameSession*     g;
    s32              idx;
    s32              product;
    StageMusicEntry* entry;
    s32              temp;

    g       = gGameSession;
    idx     = g->location.loc.stage - 1;
    product = g->location.loc.area * Stage_MusicRowLengths[idx];
    temp    = (gStageMusicRow + product) & 0xFFFF;
    entry   = Stage_MusicTables[idx];
    if (entry[temp].sequenceId != STAGE_MUSIC_NO_SEQUENCE) {
        if (Midi_IsBusy(entry[temp].sequenceId) != 0) {
            SndEvt_EnqueueType2(entry[temp].sequenceId, (arg0 + 1) & 0xFFFF);
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
    GameSession*     g;
    s32              idx;
    s32              product;
    StageMusicEntry* base;
    s32              one;

    g       = gGameSession;
    idx     = g->location.loc.stage - 1;
    product = g->location.loc.area * Stage_MusicRowLengths[idx];
    base    = Stage_MusicTables[idx];
    if (base[product].sequenceId == STAGE_MUSIC_AMBIENT_AREA) {
        if (gameFlagGetNibble(GAME_FLAG_STAGE_AMBIENT_MUTED) == 1) {
            one = 1;
            sndEvtRequestScriptStop(SOUND_ID(6, 1, 0) | one, 0x1E);
            gStageAmbientOn = 0;
        } else if (gStageAmbientOn == 0) {
            one = 1;
            sndEvtRequestScriptStart(SOUND_STAGE_AMBIENT, 0, 0);
            gStageAmbientOn = one;
        }
    }
}
