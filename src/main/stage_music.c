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

/// Request modes and timing values consumed by the stage music task.
///
/// Request mode is the untruncated `Task::spawnArg1.value`. Countdown time is
/// measured in task frames; the ambient fade is measured in audio updates.
enum {
    STAGE_MUSIC_REQUEST_ORDINARY         = 0,
    STAGE_MUSIC_REQUEST_COUNTDOWN        = 2,
    STAGE_MUSIC_REQUEST_LOAD_ONLY        = 3,
    STAGE_MUSIC_LOAD_ACTIVE              = 0,
    STAGE_MUSIC_LOAD_IDLE                = 0xFF,
    STAGE_MUSIC_COUNTDOWN_INACTIVE       = 0,
    STAGE_MUSIC_COUNTDOWN_ACTIVE         = 0xFF,
    STAGE_MUSIC_COUNTDOWN_TIMEOUT_FRAMES = 300,
    STAGE_MUSIC_COUNTDOWN_STOP_FRAMES    = 60,
    STAGE_MUSIC_AMBIENT_FADE_TICKS       = 30,
    STAGE_MUSIC_SEQUENCE_FILE_GROUP      = 4,
};

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

static void _stageMusicSelectEntry(Task* task);

static void _stageMusicLoadSequence(Task* task);

static void _stageMusicStartWhenCdIdle(Task* task);

static void _stageMusicTask(Task* task);

static void _stageMusicFinishWhenCdIdle(Task* task);

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
TaskDesc  Stage_MusicTaskDesc      = { { { TASK_BODY_NONE, 0xC0 } }, _stageMusicTask };
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
    _stageMusicSelectEntry,
    _stageMusicLoadSequence,
    _stageMusicStartWhenCdIdle,
    _stageMusicFinishWhenCdIdle,
} };

/// Selects the scene's countdown entry and starts its 300-task-frame load deadline.
///
/// Borrows the current stage's loaded map table; stage must be 1..5 and the
/// scene entry index must fit it. Writes both selection members, arms the
/// countdown and replaces its remaining task-frame count without allocation.
static inline void _stageMusicChooseCountdownEntry(_StageMusicSelection* selection)
{
    s32              countdownStageId;
    StageMusicEntry* countdownTable;
    u16              countdownEntryIndex;
    countdownStageId           = gGameSession->location.loc.stage;
    Stage_MusicCountdownActive = STAGE_MUSIC_COUNTDOWN_ACTIVE;
    countdownTable             = Stage_CountdownMusicTables[countdownStageId - 1];
    countdownEntryIndex        = gStageSceneMusicEntry;
    Stage_MusicCountdownFrames = STAGE_MUSIC_COUNTDOWN_TIMEOUT_FRAMES;
    selection->index           = countdownEntryIndex;
    selection->table           = countdownTable;
}

/// Allocates the task's music selection and chooses its area or countdown entry.
///
/// Reads the pending request's fade once. Stage must be 1..5, the map overlay
/// must remain loaded, and the area/scene or countdown index must fit its table.
/// Stops all MIDI sequences when room music is recorded, and the ambient loop
/// when excluded by the area's first entry. Selections that bypass loading
/// reach start-policy handling for every request mode.
/// Allocation failure or an unusable selection finishes the task immediately.
static void _stageMusicSelectEntry(Task* task)
{
    enum { STAGE_MUSIC_ALL_SEQUENCES = 0 };
    u8                    columnCount;
    u8                    sequenceId;
    _StageMusicSelection* selection;
    u8                    stageId;
    s32                   sceneColumn;
    s32                   requestMode;

    columnCount = Stage_MusicRowLengths[gGameSession->location.loc.stage - 1];
    selection   = memCalloc(sizeof(_StageMusicSelection), 0);
    if (selection == NULL) {
        gStageMusicLoadState = STAGE_MUSIC_LOAD_IDLE;
        taskKill(task);
        return;
    }
    // The task owns the selection block; its table stays borrowed from the map overlay.
    task->work = selection;
    if (gStageRoomSong != 0) {
        sndEvtRequestMidiStop(STAGE_MUSIC_ALL_SEQUENCES, 1);
        gStageRoomSong = 0;
    }
    stageId                    = gGameSession->location.loc.stage;
    sceneColumn                = stageMusicSelectColumn(stageId, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent, Stage_SceneEventLimits[stageId - 1]);
    requestMode                = task->spawnArg1.value;
    gStageMusicRow             = sceneColumn;
    Stage_MusicCountdownActive = STAGE_MUSIC_COUNTDOWN_INACTIVE;
    if (requestMode == STAGE_MUSIC_REQUEST_COUNTDOWN) {
        _stageMusicChooseCountdownEntry(selection);
    } else {
        selection->table = Stage_MusicTables[gGameSession->location.loc.stage - 1];
        selection->index =
            gStageMusicRow + (gGameSession->location.loc.area * columnCount);
        if ((selection->table[gGameSession->location.loc.area * columnCount].sequenceId != STAGE_MUSIC_AMBIENT_AREA) &&
            (gStageAmbientOn != 0)) {
            sndEvtRequestScriptStop(SOUND_STAGE_AMBIENT, STAGE_MUSIC_AMBIENT_FADE_TICKS);
            gStageAmbientOn = 0;
        }
    }
    // A no-sequence entry uses the stored fade; replacement stops add one before truncation.
    sequenceId = selection->table[selection->index].sequenceId;
    if (sequenceId == STAGE_MUSIC_NO_SEQUENCE) {
        sndEvtRequestMidiStop(gStageCurrentSong, gStageMusicParams.fadeOutTicks);
        gStageMusicLoadState = sequenceId;
        taskKill(task);
        return;
    }
    gStageMusicLoadState = STAGE_MUSIC_LOAD_ACTIVE;
    if (midiCanSelectSequence(selection->table[selection->index].sequenceId) == true) {
        if ((gStageCurrentSong != 0) && (midiIsSequenceBusy(gStageCurrentSong) != 0)) {
            sndEvtRequestMidiStop(gStageCurrentSong, (gStageMusicParams.fadeOutTicks + 1) & 0xFFFF);
        }
        task->state = task->state + 1;
        return;
    }
    if (selection->table[selection->index].startMode == STAGE_MUSIC_START_DEFERRED) {
        sndEvtRequestMidiStop(gStageCurrentSong, (gStageMusicParams.fadeOutTicks + 1) & 0xFFFF);
    } else if (midiIsSequenceBusy(gStageCurrentSong) == 0) {
        // This path also bypasses the load-only mode's gate in the load state.
        task->state = task->state + 2;
        return;
    }
    gStageMusicLoadState = STAGE_MUSIC_LOAD_IDLE;
    taskKill(task);
}

/// Queues the selected sequence file from the global CDF with its display-resource variant suffix.
///
/// Requires a free request-ring slot. The used key bytes and all four argument
/// bytes are copied synchronously; byte 1 of the four-byte file key is ignored.
/// The selection and its still-loaded map table are read-only and must identify a sequence.
/// Uses stage zero/group four; the resource variant supplies the ID-hundreds byte.
/// Does not check ring capacity; the caller advances immediately after enqueueing.
static inline void _stageMusicEnqueueSequenceLoad(const _StageMusicSelection* selection)
{
    u8 fileKey[4];
    u8 loadArgs[4];

    // The global CDF key uses bytes 0, 2 and 3; byte 1 is ignored.
    fileKey[3]  = 0;
    fileKey[2]  = STAGE_MUSIC_SEQUENCE_FILE_GROUP;
    fileKey[0]  = selection->table[selection->index].sequenceId;
    loadArgs[0] = gGameSession->spriteVariant;
    loadArgs[3] = 0;
    loadArgs[2] = 0;
    loadArgs[1] = CD_COMMAND_LOAD_DEFAULT;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadArgs);
}

/// Waits for outgoing music to stop, then queues the selected group-4 sequence file.
///
/// Requires the task-owned selection and a free CD request-ring slot. Load-only
/// requests and ordinary deferred entries skip starting and wait for CD idle.
/// A countdown request times out after 300 task frames, requesting a stop with
/// a one-audio-update duration at 60 frames remaining; the sound queue rounds
/// that duration to zero. Other requests wait without a timeout.
static void _stageMusicLoadSequence(Task* task)
{
    _StageMusicSelection* selection;
    s32                   requestMode;
    u8                    countdownActive;

    selection = task->work;
    if (midiIsSequenceBusy(gStageCurrentSong) == 0) {
        _stageMusicEnqueueSequenceLoad(selection);
        requestMode = task->spawnArg1.value;
        if (requestMode == STAGE_MUSIC_REQUEST_LOAD_ONLY) {
            task->state = task->state + 2;
            return;
        }
        if ((selection->table[selection->index].startMode == STAGE_MUSIC_START_DEFERRED) && (requestMode == STAGE_MUSIC_REQUEST_ORDINARY)) {
            task->state = task->state + 2;
            return;
        }
        task->state = task->state + 1;
        return;
    }
    // Countdown waits measure task frames; the stop request measures audio updates.
    countdownActive = Stage_MusicCountdownActive;
    if (countdownActive == STAGE_MUSIC_COUNTDOWN_ACTIVE) {
        Stage_MusicCountdownFrames = Stage_MusicCountdownFrames - 1;
        if (Stage_MusicCountdownFrames == STAGE_MUSIC_COUNTDOWN_STOP_FRAMES) {
            sndEvtRequestMidiStop(gStageCurrentSong, 1);
        }
        if (Stage_MusicCountdownFrames <= 0) {
            gStageMusicLoadState = countdownActive;
            taskKill(task);
        }
    }
}

/// Applies the selected entry's start policy, records its sequence and ends the task.
///
/// `entry` must be the entry named by the task-owned `selection`. Immediate
/// entries bypass view readiness; other startable entries wait for the view
/// only on ordinary requests. Never-start entries still become the recorded
/// current sequence. Queued starts require a matching loaded MIDI image at
/// dispatch; the task does not wait for admission or successful playback.
static inline void _stageMusicApplyEntry(Task* task, _StageMusicSelection* selection, StageMusicEntry* entry)
{
    u8 startMode;

    startMode = entry->startMode;
    if (startMode != STAGE_MUSIC_START_NEVER) {
        if (startMode != STAGE_MUSIC_START_IMMEDIATE) {
            if (task->spawnArg1.value == STAGE_MUSIC_REQUEST_ORDINARY) {
                if (gGameSession->viewReady != true) {
                    return;
                }
            }
        }
        sndEvtRequestMidiStart(entry->sequenceId, 0);
        midiApplyMusicVolume(MIDI_MUSIC_VOLUME_SAVED);
    }
    gStageMusicLoadState = STAGE_MUSIC_LOAD_IDLE;
    gStageCurrentSong    = selection->table[selection->index].sequenceId;
    taskKill(task);
}

/// Applies the selected music entry once the resident CD request queue is idle.
///
/// Requires a live task-owned selection and its still-loaded map table.
static void _stageMusicStartWhenCdIdle(Task* task)
{
    _StageMusicSelection* selection;

    selection = task->work;
    if (cdCmdIsIdle() != 0) {
        _stageMusicApplyEntry(task, selection, &selection->table[selection->index]);
    }
}

void stageMusicRequestAreaStart(s32 fadeInTicks)
{
    GameSession*     session;
    s32              stageIndex;
    s32              areaRowOffset;
    StageMusicEntry* areaTable;
    s32              entryIndex;

    session       = gGameSession;
    stageIndex    = session->location.loc.stage - 1;
    areaRowOffset = session->location.loc.area * Stage_MusicRowLengths[stageIndex];
    entryIndex    = (gStageMusicRow + areaRowOffset) & 0xFFFF;
    areaTable     = Stage_MusicTables[stageIndex];
    if (areaTable[entryIndex].sequenceId != STAGE_MUSIC_NO_SEQUENCE) {
        if (areaTable[entryIndex].startMode != STAGE_MUSIC_START_NEVER) {
            sndEvtRequestMidiStart(areaTable[entryIndex].sequenceId, fadeInTicks & 0xFFFF);
            gStageCurrentSong = areaTable[entryIndex].sequenceId;
            midiApplyMusicVolume(MIDI_MUSIC_VOLUME_SAVED);
        }
    }
}

void stageMusicRequestAreaStop(s32 fadeOutTicks)
{
    GameSession*     session;
    s32              stageIndex;
    s32              areaRowOffset;
    StageMusicEntry* areaTable;
    s32              entryIndex;

    session       = gGameSession;
    stageIndex    = session->location.loc.stage - 1;
    areaRowOffset = session->location.loc.area * Stage_MusicRowLengths[stageIndex];
    entryIndex    = (gStageMusicRow + areaRowOffset) & 0xFFFF;
    areaTable     = Stage_MusicTables[stageIndex];
    if (areaTable[entryIndex].sequenceId != STAGE_MUSIC_NO_SEQUENCE) {
        if (midiIsSequenceBusy(areaTable[entryIndex].sequenceId) != 0) {
            sndEvtRequestMidiStop(areaTable[entryIndex].sequenceId, (fadeOutTicks + 1) & 0xFFFF);
        }
    }
}

/// Dispatches one of the music loader's four states for a live music task.
///
/// `task->state` must be 0..3. State zero allocates the owned selection block;
/// subsequent states require it and its borrowed map table to remain valid.
static void _stageMusicTask(Task* task)
{
    TaskFuncTable4 musicStates;

    musicStates = Stage_TaskStates;
    musicStates.funcs[task->state](task);
}

/// Finishes a music task once the resident CD request queue is idle.
///
/// Marks the music request idle before teardown releases its selection block.
static void _stageMusicFinishWhenCdIdle(Task* task)
{
    if (cdCmdIsIdle() != 0) {
        gStageMusicLoadState = STAGE_MUSIC_LOAD_IDLE;
        taskKill(task);
    }
}

void stageMusicUpdateAreaAmbient(s32 unused)
{
    enum {
        STAGE_MUSIC_AMBIENT_MUTED       = 1,
        STAGE_MUSIC_AMBIENT_UNREQUESTED = 0,
        STAGE_MUSIC_AMBIENT_REQUESTED   = 1
    };
    GameSession*     session;
    s32              stageIndex;
    s32              areaRowOffset;
    StageMusicEntry* areaTable;

    session       = gGameSession;
    stageIndex    = session->location.loc.stage - 1;
    areaRowOffset = session->location.loc.area * Stage_MusicRowLengths[stageIndex];
    areaTable     = Stage_MusicTables[stageIndex];
    // Ambient eligibility belongs to the area's first column, independent of scene music.
    if (areaTable[areaRowOffset].sequenceId == STAGE_MUSIC_AMBIENT_AREA) {
        if (gameFlagGetNibble(GAME_FLAG_STAGE_AMBIENT_MUTED) == STAGE_MUSIC_AMBIENT_MUTED) {
            sndEvtRequestScriptStop(SOUND_STAGE_AMBIENT, STAGE_MUSIC_AMBIENT_FADE_TICKS);
            gStageAmbientOn = STAGE_MUSIC_AMBIENT_UNREQUESTED;
        } else if (gStageAmbientOn == STAGE_MUSIC_AMBIENT_UNREQUESTED) {
            sndEvtRequestScriptStart(SOUND_STAGE_AMBIENT, 0, 0);
            gStageAmbientOn = STAGE_MUSIC_AMBIENT_REQUESTED;
        }
    }
}
