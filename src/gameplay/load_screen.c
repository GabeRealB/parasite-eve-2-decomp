#include "loading.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "area_transitions.h"
#include "captions.h"
#include "companion_load.h"
#include "gameplay/direction.h"
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

static inline u16 _gpAdvanceAreaCd(void);

static void Gp_InitStageVisit(GameLocationKey* arg0);

void func_80724748(GameLocationKey* arg0);

static inline u16 _gpAdvanceAreaCd(void)
{
    switch (D_80114C74) {
        case 0:
            D_80114C70 = LOADING_AREA_INIT;
            D_80114C74 = 1;
        case 1:
            if (func_800AA120()) {
                Gp_AreaCdPhase = LOADING_AREA_INIT;
                D_80114C74++;
            }
            return 0;
        case 2:
            if (Gp_PollAreaCdLoads()) {
                return 1;
            }
        default:
            return 0;
    }
}

ViewCountTable*                    Gp_ViewCountTables[5] = { &D_map_akropolis_8017ABC0, &D_map_dryfield_8017AA28, &D_map_dryfield_full_8017A93C, &D_map_shelter_8017B110, &D_map_neo_ark_8017AB88 };
ViewIndexTable*                    Gp_ViewIndexTables[5] = { &D_map_akropolis_8017AC68, &D_map_dryfield_8017ABFC, &D_map_dryfield_full_8017AB10, &D_map_shelter_8017B548, &D_map_neo_ark_8017ADB0 };
SpriteAreaTable*                   Gp_SprtTables[5]      = { &D_map_akropolis_8017AB1C, &D_map_dryfield_8017AC98, &D_map_dryfield_full_8017ABAC, &D_map_shelter_8017B610, &D_map_neo_ark_8017AE38 };
WorldCollisionStageResources*      Gp_RoomObjTables[5]   = { &D_map_akropolis_8017AAC8, &D_map_dryfield_8017AAC4, &D_map_dryfield_full_8017A9D8, &D_map_shelter_8017B3B8, &D_map_neo_ark_8017ACA0 };
DirectionWarpEntry**               Gp_WarpTables[5]      = { D_map_akropolis_8017AB20, D_map_dryfield_8017A8F8, D_map_dryfield_full_8017A80C, D_map_shelter_8017AF88, D_map_neo_ark_8017AA80 };
WorldCoordRoomLighting**           Gp_RoomCoordTables[5] = { D_map_akropolis_8017AA28, D_map_dryfield_8017A860, D_map_dryfield_full_8017A774, D_map_shelter_8017AEC4, D_map_neo_ark_8017A9FC };
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
    Gp_ResetMenuLock();
    displaySetShakeY(0);
    taskSpawn(0, 0x1D, 0, 0);
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
        Gp_SpawnPlayer(&D_80114CB0, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId & 0xFFFF, 0, &spawnOptions);
        Gp_SetupCompanionActor(&warpEntry.companion, &spawnOptions);
        gDisplayState.control.flags.pendingPlayerPos = 0;
    } else {
        playerId                        = (u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId;
        spawnOptions.initialAnimationId = 1;
        spawnOptions.startScripted      = warpEntry.flags & DIRECTION_WARP_FLAG_SCRIPTED_PLAYER;
        Gp_SpawnPlayer(&warpEntry.player, (s8)playerId & 0xFFFF, 0, &spawnOptions);
        spawnOptions.startScripted = 0;
        Gp_SetupCompanionActor(&warpEntry.companion, &spawnOptions);
    }
    model                    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd;
    model->texturePageOffset = 6;
    model->clutRowOffset     = 0;
    tmdBuildBufferHalf(model);
    tmdBuildBufferHalf(model);
    viewApplyCurrentCamera();
    gameSetTaskSlot(taskSpawn(1, 0x23, 0, 0), GAME_TASK_SLOT_SCENE);
    gameSetTaskSlot(taskSpawn(6, 4, 0, 0), GAME_TASK_SLOT_ROOM_EFFECT);
    taskSpawn(9, 6, 0, 0);
    taskSpawn(9, 0x11, 0, 0);
    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0) && (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0xB)) {
        taskSpawn((s32)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene, 1, 0, 0);
    }
    Gp_SpawnPlaces(sess);
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

void Gp_BeginSessionTask(Task* arg0)
{
    CdCmdQueue*   queue;
    DisplayState* ds;
    u16           one;

    queue = &gCdCmdQueue;
    gameClearTaskSlots();
    ds               = &gDisplayState;
    ds->stopTaskWalk = 1;
    taskResetDefaultList();
    gpuClearFrameOrderingTable(0);
    gpuClearFrameOrderingTable(1);
    one = 1;
    Mem_Init();
    cdCmdRequestCancel();
    gGameSession->location           = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location;
    gGameSession->spriteVariant      = ds->spriteVariant;
    queue->suppressMoviePresentation = one;
    if ((arg0->spawnArg1.value & 0xF) == 0) {
        MoveImage(
            &gDisplayState.dispEnv[ds->drawBuffer ^ 1].disp,
            ds->dispEnv[ds->drawBuffer].disp.x,
            ds->dispEnv[ds->drawBuffer].disp.y);
        ds->control.flags.imageSource = DISPLAY_IMAGE_NONE;
        displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
    }
    taskSpawn(0, 0x1C, arg0->spawnArg1.value & 0xF, 0);
    ds->skipDraw                      = 0;
    queue->blockGamePause             = one;
    queue->releasePauseBlockAfterFade = one;
    D_8007A394                        = 0;
}

void Gp_LoadWaitBoot(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    CdCmdQueue*   queue;
    McSaveData*   save;
    GameSession*  session;
    s32           color;
    s32           queued;
    s32           buf;
    s8            yoff;

    Pad_RemapState->loadingActive = GAME_DEBUG_LOADING_ACTIVE;
    queue                         = &gCdCmdQueue;
    if (cdCmdIsIdle() & 0xFFFF) {
        if ((u8)LoadUi_PollDiskSwap()) {
            return;
        }
        queue->holdBootImage = 1;
        if (queue->bootLoadActive != 0) {
            Fs_EnsureBootLoadStarted();
        }
        memFillBytes(Stream_Slots, 0, sizeof(Stream_Slots));
        session = gGameSession;
        save    = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        if (session->loadedCharacterId != save->state.characterId || session->loadedConfigSet != gPlayerStatus.resourceVariant) {
            GameSession* sess;

            Gp_EnqueueConfigCd(0);
            Gp_EnqueueHeldWeaponCd();
            sess                    = gGameSession;
            sess->loadedCharacterId = save->state.characterId;
            sess->loadedConfigSet   = gPlayerStatus.resourceVariant;
        }
        attachmentEnqueueHealingSoundLoad();
        task->state++;
    }
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
}

void Gp_LoadWaitStage(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
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
    if (cdCmdIsIdle() & 0xFFFF) {
        if (gGameSession->location.loc.stage != gGameSession->loadedStage) {
            Gp_EnqueueStageCd();
            gGameSession->loadedStage = gGameSession->location.loc.stage;
        }
        task->state++;
    }
}

void Gp_LoadState2(Task* task)
{
    TILE*            tile;
    DR_TPAGE*        dr;
    DisplayState*    ds;
    s32              color;
    s32              queued;
    s32              buf;
    s8               yoff;
    McSaveData*      save;
    GameLocationKey* sess;

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
    if (cdCmdIsIdle() & 0xFFFF) {
        sess = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
        Gp_InitStageVisit(sess);
        save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        memConfigureImageMemory(save->state.location.loc.stage, save->state.location.loc.area);
        if ((GAME_LOCATION_WORD(save->state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 5, 0, 0)) {
            memSelectAuxHeapRegion(true);
        }
        memInitAuxHeap();
        Gp_ApplyNpcRoomSnd();
        Snd_InitFromStage(gGameSession->location.loc.stage, gGameSession->location.loc.area);
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT && gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= 4) {
            gStageSceneMusicEntry = 1;
        } else {
            gStageSceneMusicEntry = 0;
        }
        gGameSession->deathSoundCountdown = 1;
        gGameSession->deathFadeFrames     = GAME_SESSION_DEATH_FADE_DEFAULT;
        gGameSession->deathRestartDelay   = 0x1E;
        gStageMusicParams.fadeOutTicks    = 0x3C;
        gStageMusicParams.field_2         = 0;
        taskSpawnFromTable(&Stage_MusicTaskDesc, 0, 0, 0);
        task->state++;
    }
}

void Gp_LoadWaitCompanion(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    s32           color;
    s32           queued;
    s32           buf;
    s8            yoff;
    u8            param1[8];
    u8            param2[8];
    u8            flag;

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
    if (cdCmdIsIdle() & 0xFFFF) {
        param1[3] = gGameSession->location.loc.stage;
        param1[2] = gGameSession->location.loc.area;
        param1[1] = gGameSession->location.loc.room;
        param1[0] = 0;
        param1[4] = 0;
        param2[0] = 1;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        flag = Gp_PickCompanion();
        if (flag != 0) {
            gGameSession->companionType = flag;
            Gp_EnqueueCompanionCd(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant);
        }
        task->state++;
    }
}

void Gp_LoadWaitSave(Task* task)
{
    TILE*            tile;
    DR_TPAGE*        dr;
    DisplayState*    ds;
    s32              color;
    s32              queued;
    s32              buf;
    s8               yoff;
    u8               param1[8];
    u8               param2[8];
    GameLocationKey* saveKey;
    GameSession*     sess;

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
    if (cdCmdIsIdle() & 0xFFFF) {
        GameSession* session;

        session = gGameSession;
        if ((GAME_LOCATION_WORD(session->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 1, 0, 0)) {
            if (session->location.loc.room >= 4) {
                Snd_InitFromStage(session->location.loc.stage, session->location.loc.area);
                param1[3] = gGameSession->location.loc.stage;
                param1[2] = gGameSession->location.loc.area;
                param1[0] = 0x16;
                param2[0] = 1;
                param2[1] = 0;
                param2[2] = 0;
                param2[3] = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            }
        }
        sess = gGameSession;
        if (sess->applySaveVariant == 1) {
            areaSetPlacementVariant(&sess->location.loc, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.variant, AREA_VARIANT_RESET_IF_CHANGED);
            gGameSession->applySaveVariant = 0;
        }
        saveKey = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
        Gp_MarkAreaVisited(saveKey);
        areaSyncLocationVariant(saveKey);
        gGameSession->location.loc.variant = saveKey->variant;
        cdCmdPrepareViewMovie();
        D_80114C74 = 0;
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
        Tmd_InitLists();
        ds2 = &gDisplayState;
        actorRenderComposeAndDrawActiveModels(&Gpu_OtBuffers[ds2->drawBuffer]);
        task->state++;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.interlace != 0) {
            ds2->dispEnv[1].isinter = 1;
            ds2->dispEnv[0].isinter = 1;
        }
    }
}

void Gp_FadeGrayHold(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    CdCmdQueue*   queue;
    s32           color;
    s32           buf;
    s8            yoff;

    queue = &gCdCmdQueue;
    ds    = &gDisplayState;
    color = 0x64;
    buf   = ds->otBuffer;
    tile  = &Gp_FadeTiles[buf];
    dr    = &Gp_FadeTpages[buf];
    if (queue->bootLoadActive == 0) {
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
    task->killCountdown++;
    if (task->killCountdown >= 7) {
        queue->holdBootImage = 0;
        task->state++;
    }
}

static void Gp_InitStageVisit(GameLocationKey* arg0)
{
    McSaveData*           save;
    GameFlagStageHeader** banks;
    GameFlagStageHeader*  bank;

    banks = Gp_FlagBanks;
    save  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    if ((save->state.visitFlags & 1) == 0) {
        save->state.visitFlags = 1;
        Gp_ClearAllFlagNibbles();
        Gp_ApplyNewGameAreaFlags();
        save->state.companionHpMax = 0x64;
        save->state.companionHp    = 0x64;
        func_800B8014();
    }
    if ((((s8)save->state.visitFlags >> arg0->stage) & 1) == 0) {
        bank                  = banks[arg0->stage];
        bank->visitedAreas[0] = 0;
        bank->visitedAreas[1] = 0;
        areaSeedStageObjectStates(arg0->stage);
        if (gDisplayState.debugMode != 0) {
            func_80724748(arg0);
        }
    }
}
