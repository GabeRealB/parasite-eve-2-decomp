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
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "world_collision.h"
#include "world_targets.h"

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

GpSpawnTransform D_80114CB0;

static inline u16 _gpAdvanceAreaCd(void);

static void Gp_InitStageVisit(GameLocationKey* arg0);

void func_80724748(GameLocationKey* arg0);

static inline u16 _gpAdvanceAreaCd(void)
{
    switch (D_80114C74) {
        case 0:
            D_80114C70 = 0;
            D_80114C74 = 1;
        case 1:
            if (func_800AA120()) {
                Gp_AreaCdPhase = 0;
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

GpViewCountTbl*   Gp_ViewCountTables[5] = { &D_map_akropolis_8017ABC0, &D_map_dryfield_8017AA28, &D_map_dryfield_full_8017A93C, &D_map_shelter_8017B110, &D_map_neo_ark_8017AB88 };
GpViewIndexTbl*   Gp_ViewIndexTables[5] = { &D_map_akropolis_8017AC68, &D_map_dryfield_8017ABFC, &D_map_dryfield_full_8017AB10, &D_map_shelter_8017B548, &D_map_neo_ark_8017ADB0 };
GpSprtTbl*        Gp_SprtTables[5]      = { &D_map_akropolis_8017AB1C, &D_map_dryfield_8017AC98, &D_map_dryfield_full_8017ABAC, &D_map_shelter_8017B610, &D_map_neo_ark_8017AE38 };
GpRoomObjTbl*     Gp_RoomObjTables[5]   = { &D_map_akropolis_8017AAC8, &D_map_dryfield_8017AAC4, &D_map_dryfield_full_8017A9D8, &D_map_shelter_8017B3B8, &D_map_neo_ark_8017ACA0 };
GpWarpRec**       Gp_WarpTables[5]      = { D_map_akropolis_8017AB20, D_map_dryfield_8017A8F8, D_map_dryfield_full_8017A80C, D_map_shelter_8017AF88, D_map_neo_ark_8017AA80 };
GpRoomCoordRec**  Gp_RoomCoordTables[5] = { D_map_akropolis_8017AA28, D_map_dryfield_8017A860, D_map_dryfield_full_8017A774, D_map_shelter_8017AEC4, D_map_neo_ark_8017A9FC };
GpRoomParamRec*** Gp_RoomParamTables[5] = { D_map_akropolis_8017AC6C, D_map_dryfield_8017AC9C, D_map_dryfield_full_8017ABB0, D_map_shelter_8017B614, D_map_neo_ark_8017AE3C };

void func_800AA548(s32 arg0)
{
    GpWarpRec        rec;
    GpActorFlags     flags;
    TmdObject*       model;
    GameLocationKey* sess;
    GameSession*     session;
    PlayerPos*       pos;
    s32              stage;
    s32              warp;
    u32              playerId;

    session                    = gGameSession;
    session->deathVariant      = 0;
    gDisplayState.otDepthShift = 0;
    sess                       = &session->at4.loc;
    if (Player_Status.hp <= 0) {
        Player_Status.hp = 1;
    }
    if ((Mc_SaveData[0].state.companionType != 0) && (Mc_SaveData[0].state.companionHp <= 0)) {
        Mc_SaveData[0].state.companionHp = 1;
    }
    Gp_LoadRoomParams();
    gGameSession->cutsceneHold = 0;
    Gp_ResetMenuLock();
    displaySetShakeY(0);
    Task_Spawn(0, 0x1D, 0, 0);
    Task_Spawn(0, 0x1A, 0, 0);
    Game_SetPtrSlot(Task_Spawn(4, 5, 0, 0), 9);
    Task_Spawn(0, 0x14, 0, 0);
    if ((arg0 & 0xFFFF) != 1) {
        Game_SetPtrSlot(Task_Spawn(0, 0x16, 0, 0), 1);
    }
    Game_SetPtrSlot(Task_Spawn(0, 0x10, 0, 0), 2);
    stage = sess->stage;
    warp  = sess->warp;
    rec   = Gp_WarpTables[stage - 1][sess->area - 1][warp - 1];
    if (!(gDisplayState.control.word & DISPLAY_ROOM_START_KEEP_VIEW_MASK)) {
        if (((GAME_LOCATION_WORD(gGameSession->at4.loc) & ~0xFF) == GAME_LOCATION_KEY(3, 24, 2, 0)) && (gGameSession->at4.loc.warp == 2)) {
            Mc_SaveData[0].state.at4.loc.view = gGameSession->at4.loc.view = 2;
        } else {
            Mc_SaveData[0].state.at4.loc.view = gGameSession->at4.loc.view = rec.field_34;
        }
    }
    Gp_ActorSlots[0] = NULL;
    Gp_ActorSlots[1] = NULL;
    if (gDisplayState.control.flags.pendingPlayerPos == 1) {
        pos                      = &(&Player_Status)[Mc_SaveData[0].state.characterId - 1].pos;
        D_80114CB0.words.field_0 = (s32)pos->yaw;
        D_80114CB0.words.field_4 = (s32)pos->x;
        D_80114CB0.words.field_8 = (s32)pos->y;
        D_80114CB0.words.field_C = (s32)pos->z;
        flags.field_0            = 0x23;
        flags.field_2            = 0;
        Gp_SpawnPlayer(&D_80114CB0.actor, Mc_SaveData[0].state.characterId & 0xFFFF, 0, &flags);
        Gp_SetupCompanionActor(&rec.companion.actor, &flags.field_0);
        gDisplayState.control.flags.pendingPlayerPos = 0;
    } else {
        playerId      = (u8)Mc_SaveData[0].state.characterId;
        flags.field_0 = 1;
        flags.field_2 = rec.field_35 & 1;
        Gp_SpawnPlayer(&rec.player.actor, (s8)playerId & 0xFFFF, 0, &flags);
        flags.field_2 = 0;
        Gp_SetupCompanionActor(&rec.companion.actor, &flags.field_0);
    }
    model        = (gameGetPtrSlot(3))->extra.tmd;
    model->tpage = 6;
    model->clut  = 0;
    tmdProcessStream(model);
    tmdProcessStream(model);
    Gp_LoadStageView();
    Game_SetPtrSlot(Task_Spawn(1, 0x23, 0, 0), 4);
    Game_SetPtrSlot(Task_Spawn(6, 4, 0, 0), 5);
    Task_Spawn(9, 6, 0, 0);
    Task_Spawn(9, 0x11, 0, 0);
    if ((Mc_SaveData[0].state.demoScene != 0) && (Mc_SaveData[0].state.demoScene != 0xB)) {
        Task_Spawn((s32)Mc_SaveData[0].state.demoScene, 1, 0, 0);
    }
    Gp_SpawnPlaces(sess);
    Gp_SpawnArea(sess);
    Gp_InitStateF0();
    Task_Spawn(1, 0xF, 0, 0);
    Task_Spawn(1, 0x10, 0, 0);
    stage = sess->stage;
    warp  = sess->warp;
    rec   = Gp_WarpTables[stage - 1][sess->area - 1][warp - 1];
    if ((u8)gGameSession->areaSetupDone != 0) {
        if (rec.field_28 != 0) {
            SndEvt_EnqueueType6(rec.field_28, 0, 0);
        }
        if (rec.field_36 != 0) {
            GameFlag_SetNibble((s32)rec.field_36, 1);
        }
    } else {
        gGameSession->areaSetupDone = 1;
    }
    CdCmd_Queue.field_210        = 0;
    gGameSession->freezeRoomObjs = 0;
}

void Gp_BeginSessionTask(Task* arg0)
{
    CdCmdQueue*   queue;
    DisplayState* ds;
    u16           one;

    queue = &CdCmd_Queue;
    Game_ClearPtrSlots();
    ds               = &gDisplayState;
    ds->stopTaskWalk = 1;
    Task_ResetDefaultList();
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    one = 1;
    Mem_Init();
    CdCmd_ActivatePhase1();
    gGameSession->at4.raw     = Mc_SaveData[0].state.at4.raw;
    gGameSession->sprtVariant = ds->spriteVariant;
    queue->field_20A          = one;
    if ((arg0->spawnArg1.value & 0xF) == 0) {
        MoveImage(
            &gDisplayState.dispEnv[ds->drawBuffer ^ 1].disp,
            ds->dispEnv[ds->drawBuffer].disp.x,
            ds->dispEnv[ds->drawBuffer].disp.y);
        ds->control.flags.imageSource = DISPLAY_IMAGE_NONE;
        Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
    }
    Task_Spawn(0, 0x1C, arg0->spawnArg1.value & 0xF, 0);
    ds->skipDraw     = 0;
    queue->field_244 = one;
    queue->field_248 = one;
    D_8007A394       = 0;
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

    Pad_RemapState->field_3 = 1;
    queue                   = &CdCmd_Queue;
    if (CdCmd_IsIdle() & 0xFFFF) {
        if ((u8)LoadUi_PollDiskSwap()) {
            return;
        }
        queue->field_22E = 1;
        if (queue->field_224 != 0) {
            Fs_EnsureBootLoadStarted();
        }
        Mem_Set(Stream_Slots, 0, sizeof(Stream_Slots));
        session = gGameSession;
        save    = &Mc_SaveData[0];
        if (session->loadedWeaponFamily != save->state.characterId || session->loadedConfigSet != Player_Status.field_26) {
            GameSession* sess;

            Gp_EnqueueConfigCd(0);
            Gp_EnqueueHeldWeaponCd();
            sess                     = gGameSession;
            sess->loadedWeaponFamily = save->state.characterId;
            sess->loadedConfigSet    = Player_Status.field_26;
        }
        Gp_EnqueueAttach7Cd();
        task->state++;
    }
    color  = 8;
    queued = CdCmd_Queue.field_224;
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
    queued = CdCmd_Queue.field_224;
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
    if (CdCmd_IsIdle() & 0xFFFF) {
        if (gGameSession->at4.loc.stage != gGameSession->loadedStage) {
            Gp_EnqueueStageCd();
            gGameSession->loadedStage = gGameSession->at4.loc.stage;
        }
        task->state++;
    }
}

void Gp_LoadState2(Task* task)
{
    TILE*             tile;
    DR_TPAGE*         dr;
    DisplayState*     ds;
    s32               color;
    s32               queued;
    s32               buf;
    s8                yoff;
    McSaveData*       save;
    StageMusicParams* pair;
    GameLocationKey*  sess;

    color  = 8;
    queued = CdCmd_Queue.field_224;
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
    if (CdCmd_IsIdle() & 0xFFFF) {
        sess = &Mc_SaveData[0].state.at4.loc;
        Gp_InitStageVisit(sess);
        save = &Mc_SaveData[0];
        Mem_ConfigureAuxHeap(save->state.at4.loc.stage, save->state.at4.loc.area);
        if ((GAME_LOCATION_WORD(save->state.at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 5, 0, 0)) {
            Mem_SetActiveAuxHeap(true);
        }
        Mem_InitAux();
        Gp_ApplyNpcRoomSnd();
        Snd_InitFromStage(gGameSession->at4.loc.stage, gGameSession->at4.loc.area);
        if (gGameSession->at4.loc.stage == 3 && GameFlag_GetNibble(0x7A) >= 4) {
            gStageSceneMusicEntry = 1;
        } else {
            gStageSceneMusicEntry = 0;
        }
        gGameSession->areaBgmCountdown  = 1;
        *(s8*)&gGameSession->field_12E  = -0x80;
        gGameSession->deathRestartDelay = 0x1E;
        pair                            = &gStageMusicParams;
        pair->fadeFrames                = 0x3C;
        pair->unusedCommandArg          = 0;
        Task_SpawnFromTable(&Stage_MusicTaskDesc, 0, 0, 0);
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
    queued = CdCmd_Queue.field_224;
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
    if (CdCmd_IsIdle() & 0xFFFF) {
        param1[3] = gGameSession->at4.loc.stage;
        param1[2] = gGameSession->at4.loc.area;
        param1[1] = gGameSession->at4.loc.room;
        param1[0] = 0;
        param1[4] = 0;
        param2[0] = 1;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        flag = Gp_PickCompanion();
        if (flag != 0) {
            gGameSession->companionType = flag;
            Gp_EnqueueCompanionCd(Mc_SaveData[0].state.companionType, Mc_SaveData[0].state.companionVariant);
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
    queued = CdCmd_Queue.field_224;
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
    if (CdCmd_IsIdle() & 0xFFFF) {
        GameSession* session;

        session = gGameSession;
        if ((GAME_LOCATION_WORD(session->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 1, 0, 0)) {
            if (session->at4.loc.room >= 4) {
                Snd_InitFromStage(session->at4.loc.stage, session->at4.loc.area);
                param1[3] = gGameSession->at4.loc.stage;
                param1[2] = gGameSession->at4.loc.area;
                param1[0] = 0x16;
                param2[0] = 1;
                param2[1] = 0;
                param2[2] = 0;
                param2[3] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
            }
        }
        sess = gGameSession;
        if (sess->applySaveVariant == 1) {
            areaSetPlacementVariant(&sess->at4.loc, Mc_SaveData[0].state.at4.loc.variant, AREA_VARIANT_RESET_IF_CHANGED);
            gGameSession->applySaveVariant = 0;
        }
        saveKey = &Mc_SaveData[0].state.at4.loc;
        Gp_MarkAreaVisited(saveKey);
        areaSyncLocationVariant(saveKey);
        gGameSession->at4.loc.variant = saveKey->variant;
        CdCmd_BuildVlcIfStream();
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
    queued = CdCmd_Queue.field_224;
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
        Gp_ClearObjHeads();
        Tmd_InitLists();
        ds2 = &gDisplayState;
        Gp_DrawActorTmdActive(&Gpu_OtBuffers[ds2->drawBuffer]);
        task->state++;
        if (Mc_SaveData[0].state.interlace != 0) {
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

    queue = &CdCmd_Queue;
    ds    = &gDisplayState;
    color = 0x64;
    buf   = ds->otBuffer;
    tile  = &Gp_FadeTiles[buf];
    dr    = &Gp_FadeTpages[buf];
    if (queue->field_224 == 0) {
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
        queue->field_22E = 0;
        task->state++;
    }
}

static void Gp_InitStageVisit(GameLocationKey* arg0)
{
    McSaveData*  save;
    GpFlagBank** banks;
    GpFlagBank*  bank;

    banks = Gp_FlagBanks;
    save  = &Mc_SaveData[0];
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
        Gp_ApplyBit2Bank(arg0->stage);
        if (gDisplayState.debugMode != 0) {
            func_80724748(arg0);
        }
    }
}
