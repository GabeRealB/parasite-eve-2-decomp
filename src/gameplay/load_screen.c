#include "loading.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/captions.h"
#include "area_transitions.h"
#include "gameflag.h"
#include "gameplay/companion_load.h"
#include "companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/hud_sprites.h"
#include "hud_sprites.h"
#include "item_placement.h"
#include "items.h"
#include "gameplay/loading.h"
#include "pad_input.h"
#include "player_actor.h"
#include "gameplay/room.h"
#include "scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "world_collision.h"
#include "world_coords.h"

#include "main/areas.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

TILE Gp_FadeTiles[2];

DR_TPAGE Gp_FadeTpages[2];

ActorSpawnTransform D_80114CB0;

/// Map-marker state written on arrival after the initial session setup.
enum { DIRECTION_WARP_MAP_FLAG_ARRIVED = 1 };

/// Loading-screen darkness is subtracted from each 8-bit framebuffer channel.
enum {
    LOADING_FADE_WAIT_DARKNESS = 8,
};

/// Suffix selecting the area's base/view-resource folder.
enum { LOADING_AREA_FOLDER_SUFFIX = 1 };

/// Saved visit-flag bit recording completion of one-off new-game setup.
enum { AREA_NEW_GAME_SETUP_DONE = 1 };

static inline void _loadingDrawFadeOverlay(TILE* fadeTile, DR_TPAGE* blendCommand, const DisplayState* displayState, s32 darkness);

static inline u16 _gpAdvanceAreaCd(void);

static void _areaInitializeStageVisit(GameLocationKey* location);

void func_80724748(GameLocationKey* arg0);

static inline u16 _gpAdvanceAreaCd(void)
{
    switch (D_80114C74) {
        case 0:
            D_80114C70 = LOADING_AREA_INIT;
            D_80114C74 = 1;
        case 1:
            if (loadingPollAreaBaseResources()) {
                Gp_AreaCdPhase = LOADING_AREA_INIT;
                D_80114C74++;
            }
            return 0;
        case 2:
            if (loadingPollAreaPlacementFiles()) {
                return 1;
            }
        default:
            return 0;
    }
}

ViewCountTable*               Gp_ViewCountTables[5]            = { &D_map_akropolis_8017ABC0, &D_map_dryfield_8017AA28, &D_map_dryfield_full_8017A93C, &D_map_shelter_8017B110, &D_map_neo_ark_8017AB88 };
ViewIndexTable*               gViewIndexTables[5]              = { &D_map_akropolis_8017AC68, &gMapDryfieldViewIndexTable, &D_map_dryfield_full_8017AB10, &gMapShelterViewIndexTable, &D_map_neo_ark_8017ADB0 };
SpriteAreaTable*              gSpriteAreaTables[5]             = { &D_map_akropolis_8017AB1C, &gMapDryfieldSpriteAreaTable, &D_map_dryfield_full_8017ABAC, &gMapShelterSpriteAreaTable, &D_map_neo_ark_8017AE38 };
WorldCollisionStageResources* Gp_RoomObjTables[5]              = { &D_map_akropolis_8017AAC8, &D_map_dryfield_8017AAC4, &D_map_dryfield_full_8017A9D8, &D_map_shelter_8017B3B8, &D_map_neo_ark_8017ACA0 };
DirectionWarpEntry**          Gp_WarpTables[5]                 = { D_map_akropolis_8017AB20, D_map_dryfield_8017A8F8, D_map_dryfield_full_8017A80C, D_map_shelter_8017AF88, D_map_neo_ark_8017AA80 };
WorldCoordRoomLighting**      gWorldCoordRoomLightingTables[5] = {
    [GAME_STAGE_ACROPOLIS - 1]       = D_map_akropolis_8017AA28,
    [GAME_STAGE_DRYFIELD - 1]        = gMapDryfieldRoomLightingTables,
    [GAME_STAGE_DRYFIELD_NIGHT - 1]  = D_map_dryfield_full_8017A774,
    [GAME_STAGE_MINE_SHELTER - 1]    = gMapShelterRoomLightingTables,
    [GAME_STAGE_SHELTER_NEO_ARK - 1] = D_map_neo_ark_8017A9FC,
};
WorldCollisionSurfaceProperties*** Gp_RoomParamTables[5] = { D_map_akropolis_8017AC6C, D_map_dryfield_8017AC9C, D_map_dryfield_full_8017ABB0, D_map_shelter_8017B614, D_map_neo_ark_8017AE3C };

void func_800AA548(s32 arg0)
{
    DirectionWarpEntry warpEntry;
    ActorSpawnOptions  spawnOptions;
    TmdObject*         model;
    GameLocationKey*   sess;
    GameSession*       session;
    PlayerPos*         savedPos;
    s32                stage;
    s32                warp;
    u32                playerId;

    session                    = gGameSession;
    session->deathVariant      = 0;
    gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_1X;
    sess                       = &session->location.loc;
    if (gPlayerStatus.hp <= 0) {
        gPlayerStatus.hp = 1;
    }
    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType != 0) && (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0)) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp = 1;
    }
    worldCollisionLoadSurfacePushbackFlags();
    gGameSession->cutsceneHold = 0;
    padInputResetSuppression();
    displaySetShakeY(0);
    taskSpawn(PLAY_CLOCK_TASK_BANK, PLAY_CLOCK_TASK_TYPE, 0, 0);
    taskSpawn(0, 0x1A, 0, 0);
    gameSetTaskSlot(taskSpawn(4, 5, 0, 0), 9);
    taskSpawn(0, 0x14, 0, 0);
    if ((arg0 & 0xFFFF) != 1) {
        gameSetTaskSlot(taskSpawn(0, 0x16, 0, 0), GAME_TASK_SLOT_VIEW_GATE);
    }
    gameSetTaskSlot(taskSpawn(0, 0x10, 0, 0), 2);
    // The destination endpoint supplies actor placements and the default view.
    stage     = sess->stage;
    warp      = sess->warp;
    warpEntry = Gp_WarpTables[stage - 1][sess->area - 1][warp - 1];
    if (!(gDisplayState.control.word & DISPLAY_ROOM_START_KEEP_VIEW_MASK)) {
        if (((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_KEY(0xFF, 0xFF, 0xFF, 0)) == GAME_LOCATION_KEY(3, 24, 2, 0)) && (gGameSession->location.loc.warp == 2)) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = gGameSession->location.loc.view = 2;
        } else {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = gGameSession->location.loc.view = warpEntry.initialView;
        }
    }
    gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]    = NULL;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = NULL;
    if (gDisplayState.control.flags.pendingPlayerPos == 1) {
        // Restore the captured signed coordinates instead of the warp's start.
        savedPos                        = &(&gPlayerStatus)[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1].pos;
        D_80114CB0.yaw.word             = savedPos->yaw;
        D_80114CB0.x                    = savedPos->x;
        D_80114CB0.y                    = savedPos->y;
        D_80114CB0.z                    = savedPos->z;
        spawnOptions.initialAnimationId = 0x23;
        spawnOptions.startScripted      = 0;
        playerActorSpawn(&D_80114CB0, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId & 0xFFFF, 0, &spawnOptions);
        companionSpawnScheduledActor(&warpEntry.companion, &spawnOptions);
        gDisplayState.control.flags.pendingPlayerPos = 0;
    } else {
        playerId                        = (u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId;
        spawnOptions.initialAnimationId = 1;
        spawnOptions.startScripted      = warpEntry.flags & DIRECTION_WARP_FLAG_SCRIPTED_PLAYER;
        playerActorSpawn(&warpEntry.player, (s8)playerId & 0xFFFF, 0, &spawnOptions);
        spawnOptions.startScripted = 0;
        companionSpawnScheduledActor(&warpEntry.companion, &spawnOptions);
    }
    model                    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd;
    model->texturePageOffset = 6;
    model->clutRowOffset     = 0;
    tmdBuildBufferHalf(model);
    tmdBuildBufferHalf(model);
    viewApplyCurrentCamera();
    gameSetTaskSlot(taskSpawn(1, 0x23, 0, 0), GAME_TASK_SLOT_SCENE);
    gameSetTaskSlot(taskSpawn(6, 4, 0, 0), GAME_TASK_SLOT_ROOM_EFFECT);
    taskSpawn(CAP_CONTROL_TASK_BANK, CAP_CONTROL_TASK_TYPE, 0, 0);
    taskSpawn(9, 0x11, 0, 0);
    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0) && (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0xB)) {
        taskSpawn((s32)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene, 1, 0, 0);
    }
    areaSpawnRoomObjects(sess);
    areaSpawnPlacements(sess);
    sceneResetCombatState();
    taskSpawn(1, 0xF, 0, 0);
    taskSpawn(1, 0x10, 0, 0);
    // Read arrival effects after the room's setup has run.
    stage     = sess->stage;
    warp      = sess->warp;
    warpEntry = Gp_WarpTables[stage - 1][sess->area - 1][warp - 1];
    if (gGameSession->areaSetupDone != 0) {
        if (warpEntry.arrivalSound != DIRECTION_WARP_SOUND_NONE) {
            sndEvtRequestScriptStart(warpEntry.arrivalSound, 0, 0);
        }
        if (warpEntry.mapFlagId != DIRECTION_WARP_MAP_FLAG_NONE) {
            gameFlagSetNibble(warpEntry.mapFlagId, DIRECTION_WARP_MAP_FLAG_ARRIVED);
        }
    } else {
        gGameSession->areaSetupDone = 1;
    }
    gCdCmdQueue.viewMovieSelected = 0;
    gGameSession->freezeRoomObjs  = 0;
}

/// Discards the default task list and both frame ordering tables before heap reuse.
///
/// Previous tasks and GPU packet users must already be disposable or finished.
/// The stop request prevents the walker from reading its discarded cursor again.
static inline void _gameFlowDiscardSessionTasksAndPackets(DisplayState* displayState)
{
    enum { GAME_FLOW_STOP_TASK_WALK = 1 };

    displayState->stopTaskWalk = GAME_FLOW_STOP_TASK_WALK;
    taskResetDefaultList();
    gpuClearFrameOrderingTable(0);
    gpuClearFrameOrderingTable(1);
}

void gameFlowRebuildSessionTask(Task* task)
{
    enum {
        GAME_FLOW_LOADING_TASK_BANK           = 0,
        GAME_FLOW_LOADING_TASK_SLOT           = 0x1C,
        LOAD_UI_DISK_SWAP_CHECK_REQUIRED_DISC = 0,
    };
    CdCmdQueue*   cdQueue;
    DisplayState* displayState;

    cdQueue = &gCdCmdQueue;
    // Stop the walker before discarding its current task and all heap allocations.
    gameClearTaskSlots();
    displayState = &gDisplayState;
    _gameFlowDiscardSessionTasksAndPackets(displayState);
    memInitHeaps();
    cdCmdRequestCancel();
    gGameSession->location             = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location;
    gGameSession->spriteVariant        = displayState->spriteVariant;
    cdQueue->suppressMoviePresentation = true;
    // Preserve the presented frame in the other framebuffer for capture mode.
    if ((task->spawnArg1.value & GAME_FLOW_RELOAD_DISPLAY_MODE_MASK) == GAME_FLOW_RELOAD_CAPTURE_FRAME) {
        MoveImage(
            &gDisplayState.dispEnv[displayState->drawBuffer ^ 1].disp,
            displayState->dispEnv[displayState->drawBuffer].disp.x,
            displayState->dispEnv[displayState->drawBuffer].disp.y);
        displayState->control.flags.imageSource = DISPLAY_IMAGE_NONE;
        displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
    }
    // This spawn can reuse the discarded task's storage; consume its option first.
    taskSpawn(GAME_FLOW_LOADING_TASK_BANK, GAME_FLOW_LOADING_TASK_SLOT, task->spawnArg1.value & GAME_FLOW_RELOAD_DISPLAY_MODE_MASK, 0);
    displayState->skipDraw              = false;
    cdQueue->blockGamePause             = true;
    cdQueue->releasePauseBlockAfterFade = true;
    D_8007A394                          = LOAD_UI_DISK_SWAP_CHECK_REQUIRED_DISC;
}

/// Queues the loading screen's subtractive full-screen overlay in the foreground.
///
/// Uses the low byte of darkness for all RGB channels on a 320-by-240 TILE.
/// Screen Y compensates for the display's signed-byte vertical offset. Packets
/// must be distinct, writable, current-frame storage kept alive until the GPU
/// consumes the ordering table. Requires foreground tag -16 in bounds; command
/// insertion after the tile makes the subtractive blend execute before it.
/// The caller gates use while the boot-image loader owns presentation. Borrows
/// the display without modifying it and neither allocates nor retains packets.
static inline void _loadingDrawFadeOverlay(TILE* fadeTile, DR_TPAGE* blendCommand, const DisplayState* displayState, s32 darkness)
{
    enum {
        LOADING_FADE_TILE_WORDS           = sizeof(TILE) / sizeof(u32) - 1,
        LOADING_FADE_TILE_SEMITRANSPARENT = 0x62,
        LOADING_FADE_WIDTH_PIXELS         = 320,
        LOADING_FADE_HEIGHT_PIXELS        = 240,
        LOADING_FADE_FOREGROUND_TAG       = -16,
        LOADING_FADE_TPAGE_WORDS          = sizeof(DR_TPAGE) / sizeof(u32) - 1,
        LOADING_FADE_TPAGE_OPCODE         = 0xE1000000,
        LOADING_FADE_DRAW_TO_DISPLAY      = 0x200,
    };
    s8 screenShakeY;

    setlen(fadeTile, LOADING_FADE_TILE_WORDS);
    setcode(fadeTile, LOADING_FADE_TILE_SEMITRANSPARENT);
    fadeTile->r0 = darkness;
    fadeTile->g0 = darkness;
    fadeTile->b0 = darkness;
    fadeTile->x0 = -LOADING_FADE_WIDTH_PIXELS / 2;
    screenShakeY = displayState->vramYOffset;
    fadeTile->w  = LOADING_FADE_WIDTH_PIXELS;
    fadeTile->h  = LOADING_FADE_HEIGHT_PIXELS;
    fadeTile->y0 = -LOADING_FADE_HEIGHT_PIXELS / 2 - screenShakeY;
    addPrim(gGpuCurrentOt + LOADING_FADE_FOREGROUND_TAG, fadeTile);
    // OT insertion reverses these links, so the blend mode executes first.
    setlen(blendCommand, LOADING_FADE_TPAGE_WORDS);
    blendCommand->code[0] = LOADING_FADE_TPAGE_OPCODE | (LOADING_FADE_DRAW_TO_DISPLAY | (GPU_BLEND_SUBTRACT << 5));
    addPrim(gGpuCurrentOt + LOADING_FADE_FOREGROUND_TAG, blendCommand);
}

void loadingPrepareCharacterResourcesTask(Task* task)
{
    enum { LOADING_CHARACTER_RESOURCES_FULL_LOAD = 0 };
    TILE*         fadeTile;
    DR_TPAGE*     blendCommand;
    DisplayState* displayState;
    CdCmdQueue*   cdQueue;
    McSaveData*   liveSave;
    GameSession*  session;
    s32           darkness;
    u16           bootLoadActive;
    s32           packetBufferIndex;

    Pad_RemapState->loadingActive = GAME_DEBUG_LOADING_ACTIVE;
    cdQueue                       = &gCdCmdQueue;
    if (cdCmdIsIdle()) {
        if (loadUiPollDiskSwap() != LOAD_UI_DISK_SWAP_COMPLETE) {
            return;
        }
        // Hold the boot image while the resource-loading states run.
        cdQueue->holdBootImage = true;
        if (cdQueue->bootLoadActive != 0) {
            gameFlowEnsureLoadScreenImageStarted();
        }
        memFillBytes(Stream_Slots, 0, sizeof(Stream_Slots));
        session  = gGameSession;
        liveSave = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        if (session->loadedCharacterId != liveSave->state.characterId || session->loadedConfigSet != gPlayerStatus.resourceVariant) {
            GameSession* resourceSession;

            loadingEnqueueCharacterResources(LOADING_CHARACTER_RESOURCES_FULL_LOAD);
            loadingEnqueueEquippedWeaponResources();
            // Cache requests now; later states wait for CD completion.
            resourceSession                    = gGameSession;
            resourceSession->loadedCharacterId = liveSave->state.characterId;
            resourceSession->loadedConfigSet   = gPlayerStatus.resourceVariant;
        }
        attachmentEnqueueHealingSoundLoad();
        task->state++;
    }
    darkness          = LOADING_FADE_WAIT_DARKNESS;
    bootLoadActive    = gCdCmdQueue.bootLoadActive;
    displayState      = &gDisplayState;
    packetBufferIndex = displayState->otBuffer;
    fadeTile          = &Gp_FadeTiles[packetBufferIndex];
    blendCommand      = &Gp_FadeTpages[packetBufferIndex];
    if (bootLoadActive == 0) {
        _loadingDrawFadeOverlay(fadeTile, blendCommand, displayState, darkness);
    }
}

void loadingEnqueueStageResourcesTask(Task* task)
{
    TILE*         fadeTile;
    DR_TPAGE*     blendCommand;
    DisplayState* displayState;
    s32           darkness;
    u16           bootLoadActive;
    s32           packetBufferIndex;

    darkness          = LOADING_FADE_WAIT_DARKNESS;
    bootLoadActive    = gCdCmdQueue.bootLoadActive;
    displayState      = &gDisplayState;
    packetBufferIndex = displayState->otBuffer;
    fadeTile          = &Gp_FadeTiles[packetBufferIndex];
    blendCommand      = &Gp_FadeTpages[packetBufferIndex];
    if (bootLoadActive == 0) {
        _loadingDrawFadeOverlay(fadeTile, blendCommand, displayState, darkness);
    }
    // Cache the requested stage immediately; the next phase waits for completion.
    if (cdCmdIsIdle()) {
        if (gGameSession->location.loc.stage != gGameSession->loadedStage) {
            loadingEnqueueStageResources();
            gGameSession->loadedStage = gGameSession->location.loc.stage;
        }
        task->state++;
    }
}

void loadingInitializeAreaMemoryAndAudioTask(Task* task)
{
    enum {
        LOADING_NIGHT_MUSIC_FIRST_CHAPTER = 4,
        LOADING_SCENE_MUSIC_DEFAULT       = 0,
        LOADING_SCENE_MUSIC_NIGHT         = 1,
        LOADING_DEATH_SOUND_INITIAL_TICKS = 1,
        LOADING_DEATH_RESTART_DELAY_TICKS = 30,
        LOADING_MUSIC_FADE_OUT_TICKS      = 60,
        STAGE_MUSIC_REQUEST_ORDINARY      = 0,
    };
    TILE*            fadeTile;
    DR_TPAGE*        blendCommand;
    DisplayState*    displayState;
    s32              darkness;
    u16              bootLoadActive;
    s32              packetBufferIndex;
    McSaveData*      liveSave;
    GameLocationKey* savedLocation;

    darkness          = LOADING_FADE_WAIT_DARKNESS;
    bootLoadActive    = gCdCmdQueue.bootLoadActive;
    displayState      = &gDisplayState;
    packetBufferIndex = displayState->otBuffer;
    fadeTile          = &Gp_FadeTiles[packetBufferIndex];
    blendCommand      = &Gp_FadeTpages[packetBufferIndex];
    if (bootLoadActive == 0) {
        _loadingDrawFadeOverlay(fadeTile, blendCommand, displayState, darkness);
    }
    if (cdCmdIsIdle()) {
        savedLocation = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
        _areaInitializeStageVisit(savedLocation);
        // Reuse area image storage only after the preceding CD requests finish.
        liveSave = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        memConfigureImageMemory(liveSave->state.location.loc.stage, liveSave->state.location.loc.area);
        if ((GAME_LOCATION_WORD(liveSave->state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 0, 0)) {
            memSelectAuxHeapRegion(true);
        }
        memInitAuxHeap();
        // Retain scheduled companion sounds before resetting the area's sound context.
        companionConfigureSoundBankRetention();
        sndScriptResetForArea(gGameSession->location.loc.stage, gGameSession->location.loc.area);
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT && gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= LOADING_NIGHT_MUSIC_FIRST_CHAPTER) {
            gStageSceneMusicEntry = LOADING_SCENE_MUSIC_NIGHT;
        } else {
            gStageSceneMusicEntry = LOADING_SCENE_MUSIC_DEFAULT;
        }
        gGameSession->deathSoundCountdown = LOADING_DEATH_SOUND_INITIAL_TICKS;
        gGameSession->deathFadeFrames     = GAME_SESSION_DEATH_FADE_DEFAULT;
        gGameSession->deathRestartDelay   = LOADING_DEATH_RESTART_DELAY_TICKS;
        gStageMusicParams.fadeOutTicks    = LOADING_MUSIC_FADE_OUT_TICKS;
        gStageMusicParams.field_2         = 0;
        taskSpawnFromTable(&Stage_MusicTaskDesc, 0, STAGE_MUSIC_REQUEST_ORDINARY, 0);
        task->state++;
    }
}

void loadingEnqueueAreaAndCompanionResourcesTask(Task* task)
{
    enum { LOADING_AREA_BASE_FILE_INDEX       = 0,
           LOADING_COMPANION_NO_RESOURCE_LOAD = 0 };
    TILE*         fadeTile;
    DR_TPAGE*     blendCommand;
    DisplayState* displayState;
    s32           darkness;
    u16           bootLoadActive;
    s32           packetBufferIndex;
    struct {
        _LoadingFileKey key;
        u8              field_4; // Written zero beyond the queued key; role unproven
    } areaRequest;
    _LoadingFileArgs loadArgs;
    u8               companionTypeToLoad;

    darkness          = LOADING_FADE_WAIT_DARKNESS;
    bootLoadActive    = gCdCmdQueue.bootLoadActive;
    displayState      = &gDisplayState;
    packetBufferIndex = displayState->otBuffer;
    fadeTile          = &Gp_FadeTiles[packetBufferIndex];
    blendCommand      = &Gp_FadeTpages[packetBufferIndex];
    if (bootLoadActive == 0) {
        _loadingDrawFadeOverlay(fadeTile, blendCommand, displayState, darkness);
    }
    // The area request is copied before companion selection can enqueue more loads.
    if (cdCmdIsIdle()) {
        areaRequest.key.stage          = gGameSession->location.loc.stage;
        areaRequest.key.fileGroup      = gGameSession->location.loc.area;
        areaRequest.key.ignoredByQueue = gGameSession->location.loc.room;
        areaRequest.key.fileIndex      = LOADING_AREA_BASE_FILE_INDEX;
        areaRequest.field_4            = 0;
        loadArgs.fileIdHundreds        = LOADING_AREA_FOLDER_SUFFIX;
        loadArgs.loadMode              = CD_COMMAND_LOAD_DEFAULT;
        loadArgs.imageXPageOffset      = 0;
        loadArgs.imageYOffset          = 0;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &areaRequest.key, &loadArgs);
        companionTypeToLoad = companionSelectForArea();
        if (companionTypeToLoad != LOADING_COMPANION_NO_RESOURCE_LOAD) {
            gGameSession->companionType = companionTypeToLoad;
            companionEnqueueResources(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant);
        }
        task->state++;
    }
}

void loadingPrepareAreaStateTask(Task* task)
{
    enum {
        LOADING_GAS_STATION_EXTRA_FILE_FIRST_ROOM = 4,
        LOADING_GAS_STATION_SOUND_FILE_INDEX      = 22,
    };
    TILE*            fadeTile;
    DR_TPAGE*        blendCommand;
    DisplayState*    displayState;
    s32              darkness;
    u16              bootLoadActive;
    s32              packetBufferIndex;
    _LoadingFileKey  fileKey;
    _LoadingFileArgs loadArgs;
    GameLocationKey* savedLocation;
    GameSession*     session;

    darkness          = LOADING_FADE_WAIT_DARKNESS;
    bootLoadActive    = gCdCmdQueue.bootLoadActive;
    displayState      = &gDisplayState;
    packetBufferIndex = displayState->otBuffer;
    fadeTile          = &Gp_FadeTiles[packetBufferIndex];
    blendCommand      = &Gp_FadeTpages[packetBufferIndex];
    if (bootLoadActive == 0) {
        _loadingDrawFadeOverlay(fadeTile, blendCommand, displayState, darkness);
    }
    if (cdCmdIsIdle()) {
        GameSession* soundSession;

        soundSession = gGameSession;
        if ((GAME_LOCATION_WORD(soundSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 0, 0)) {
            if (soundSession->location.loc.room >= LOADING_GAS_STATION_EXTRA_FILE_FIRST_ROOM) {
                sndScriptResetForArea(soundSession->location.loc.stage, soundSession->location.loc.area);
                fileKey.stage             = gGameSession->location.loc.stage;
                fileKey.fileGroup         = gGameSession->location.loc.area;
                fileKey.fileIndex         = LOADING_GAS_STATION_SOUND_FILE_INDEX;
                loadArgs.fileIdHundreds   = LOADING_AREA_FOLDER_SUFFIX;
                loadArgs.loadMode         = CD_COMMAND_LOAD_DEFAULT;
                loadArgs.imageXPageOffset = 0;
                loadArgs.imageYOffset     = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &fileKey, &loadArgs);
            }
        }
        // Restore saved placements before the visit and variant reconciliation.
        session = gGameSession;
        if (session->applySaveVariant == true) {
            areaSetPlacementVariant(&session->location.loc, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.variant, AREA_VARIANT_RESET_IF_CHANGED);
            gGameSession->applySaveVariant = false;
        }
        savedLocation = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
        areaMarkVisited(savedLocation);
        areaSyncLocationVariant(savedLocation);
        gGameSession->location.loc.variant = savedLocation->variant;
        cdCmdPrepareViewMovie();
        D_80114C74 = LOADING_AREA_INIT;
        task->state++;
    }
}

void Gp_LoadWaitAreaCd(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    DisplayState* ds2;
    s32           color;
    s32           queued;
    s32           buf;
    s8            yoff;

    color  = 8;
    queued = gCdCmdQueue.bootLoadActive;
    ds     = &gDisplayState;
    buf    = ds->otBuffer;
    tile   = &Gp_FadeTiles[buf];
    dr     = &Gp_FadeTpages[buf];
    if (queued == 0) {
        setlen(tile, 3);
        setcode(tile, 0x62);
        tile->r0 = color;
        tile->g0 = color;
        tile->b0 = color;
        tile->x0 = -0xA0;
        yoff     = ds->vramYOffset;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        tile->y0 = -0x78 - yoff;
        addPrim(gGpuCurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(gGpuCurrentOt - 0x10, dr);
    }

    if (_gpAdvanceAreaCd()) {
        worldCollisionResetListsAndGrid();
        actorRenderResetLists();
        ds2 = &gDisplayState;
        actorRenderComposeAndDrawActiveModels(&Gpu_OtBuffers[ds2->drawBuffer]);
        task->state++;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.interlace != 0) {
            ds2->dispEnv[1].isinter = 1;
            ds2->dispEnv[0].isinter = 1;
        }
    }
}

void loadingHoldFadeAndReleaseBootImageTask(Task* task)
{
    enum { LOADING_FADE_HOLD_DARKNESS = 100,
           LOADING_FADE_HOLD_TICKS    = 7 };
    TILE*         fadeTile;
    DR_TPAGE*     blendCommand;
    DisplayState* displayState;
    CdCmdQueue*   cdQueue;
    s32           darkness;
    s32           packetBufferIndex;

    cdQueue           = &gCdCmdQueue;
    displayState      = &gDisplayState;
    darkness          = LOADING_FADE_HOLD_DARKNESS;
    packetBufferIndex = displayState->otBuffer;
    fadeTile          = &Gp_FadeTiles[packetBufferIndex];
    blendCommand      = &Gp_FadeTpages[packetBufferIndex];
    if (cdQueue->bootLoadActive == 0) {
        _loadingDrawFadeOverlay(fadeTile, blendCommand, displayState, darkness);
    }
    // Count the hold even while the boot-image machine owns presentation.
    task->killCountdown++;
    if (task->killCountdown >= LOADING_FADE_HOLD_TICKS) {
        cdQueue->holdBootImage = 0;
        task->state++;
    }
}

/// Installs the initial live flags, map marks, companion health and inventory.
///
/// Called only while the save's new-game setup bit is clear. Replaces its visit
/// flags with that bit; the save and resident state must be live and writable.
static inline void _areaInitializeNewGameState(McSaveData* liveSave)
{
    enum { AREA_NEW_GAME_COMPANION_HP = 100 };

    liveSave->state.visitFlags = AREA_NEW_GAME_SETUP_DONE;
    gameFlagClearLiveNibbles();
    areaApplyNewGameMapMarks();
    liveSave->state.companionHpMax = AREA_NEW_GAME_COMPANION_HP;
    liveSave->state.companionHp    = AREA_NEW_GAME_COMPANION_HP;
    inventoryInitializeNewGame();
}

/// Initializes new-game state and an unvisited stage's area/object state before loading.
///
/// Requires a live save and writable location with stage 1..5, resident flag banks
/// and loaded object-state tables. Bit 0 of the saved visit flags gates new-game
/// flags, map marks, companion HP and inventory initialization. An unset stage
/// bit clears its two visited-area words and seeds object state; nighttime
/// Dryfield retains the daytime object words. Does not mark the stage visited;
/// a later area visit does that. Debug mode forwards the location to its hook.
static void _areaInitializeStageVisit(GameLocationKey* location)
{
    McSaveData*           liveSave;
    GameFlagStageHeader** stageFlagBanks;
    GameFlagStageHeader*  stageFlags;

    stageFlagBanks = Gp_FlagBanks;
    liveSave       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    if ((liveSave->state.visitFlags & AREA_NEW_GAME_SETUP_DONE) == 0) {
        _areaInitializeNewGameState(liveSave);
    }
    if ((((s8)liveSave->state.visitFlags >> location->stage) & 1) == 0) {
        stageFlags                  = stageFlagBanks[location->stage];
        stageFlags->visitedAreas[0] = 0;
        stageFlags->visitedAreas[1] = 0;
        areaSeedStageObjectStates(location->stage);
        if (gDisplayState.debugMode != 0) {
            func_80724748(location);
        }
    }
}
