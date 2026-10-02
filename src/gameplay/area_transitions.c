#include "gameplay/area_transitions.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/area.h"
#include "area_transitions.h"
#include "gameplay/captions.h"
#include "captions.h"
#include "gameplay/direction.h"
#include "direction.h"
#include "gameplay/direction_input.h"
#include "direction_input.h"
#include "geometry.h"
#include "gameplay/loading.h"
#include "loading.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "world_collision.h"

#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

/// 2-byte record in 0xFF-terminated lists walked by `Gp_ApplyAreaFlag4List` and
/// `Gp_ApplyNewGameAreaFlags`. `field_0` indexes a `GpAreaRec` table (same role as
/// `GameLocationKey.area`); `field_1` is the apply flag (nonzero →
/// `GpAreaObj.spawnFlags |= 4`).
typedef struct _GpAreaFlagRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
} GpAreaFlagRec;
STATIC_ASSERT_SIZEOF(GpAreaFlagRec, 2);

/// Tables of no-arg callbacks copied onto the stack by the sibling
/// dispatchers. `Gp_DirAction0` copies the 6-entry `Gp_WarpPhaseFns`;
/// `Gp_DirAction1` copies the 5-entry `D_80093990`.
typedef void (*GpVoidFunc)(void);

typedef struct {
    GpVoidFunc funcs[5];
} GpVoidFuncTable5;

typedef struct {
    GpVoidFunc funcs[6];
} GpVoidFuncTable6;

/// `Gp_AreaTables[1]`, `[2]`, `[4]`, `[5]`. Splat labels the later slots as
/// their own symbols; `Gp_ApplyNewGameAreaFlags` loads each as a `GpAreaRec*`.
#define Gp_AreaTableStg1 Gp_AreaTables[1]

#define Gp_AreaTableStg2 Gp_AreaTables[2]

#define Gp_AreaTableStg4 Gp_AreaTables[4]

#define Gp_AreaTableStg5 Gp_AreaTables[5]

/// 0xFF-terminated `GpAreaFlagRec` lists applied by `Gp_ApplyNewGameAreaFlags` to
/// `Gp_AreaTableStg1` / `Gp_AreaTableStg2` / `Gp_AreaTableStg4` / `Gp_AreaTableStg5` (stages 1, 2,
/// 4, 5 of `Gp_AreaTables`).
extern struct _GpAreaFlagRec Gp_NewGameFlagsStg1[];

extern struct _GpAreaFlagRec Gp_NewGameFlagsStg2[];

extern struct _GpAreaFlagRec Gp_NewGameFlagsStg4[];

extern struct _GpAreaFlagRec Gp_NewGameFlagsStg5[];

/// The flag entry `table[idx]`: its low 11 bits select a flag nibble, and its
/// bit 0x800 is added onto that nibble's value.
extern const TaskFuncTable3 Gp_DirTaskStates;

static const GpVoidFuncTable6 Gp_WarpPhaseFns;

static const GpVoidFuncTable5 D_80093990;

static inline s32 _gpGetAreaFlag4(GameLocationKey* key);

static inline s16 _gpStageFlagNibble(u16* table, s16 idx);

static void Gp_InitDirState(Task* arg0);

static void Gp_DirTaskState1(Task* task);

static u8 Gp_GetViewCountLo(void);

static void Gp_DirAction0(void);

static void Gp_DirAction1(void);

static void Gp_ClearDirCursor(void);

static void Gp_PostMsg13EF(void);

static void Gp_SpawnEvt1IfCapIdle(void);

static void Gp_FadeDirAdvance(void);

static void Gp_CommitSaveLoc(void);

static void Gp_MsgPlayer3EE(void);

static void Gp_MsgPlayer3F0(void);

static void Gp_MsgPlayer3EF(void);

static void Gp_ApplyAreaFlag4List(s16 arg0, GpAreaFlagRec* arg1);

static inline s32 _gpGetAreaFlag4(GameLocationKey* key)
{
    GpAreaRec* rec;
    GpAreaObj* obj;
    s32        val;

    rec = Gp_AreaTables[key->stage];
    if (rec != NULL) {
        obj = rec[key->area].field_4;
        if (obj != NULL) {
            val = obj->spawnFlags & 4;
            return val != 0;
        }
    }
    return 0;
}

struct _GpAreaFlagRec Gp_NewGameFlagsStg1[20] = {
    { 1, 0 },
    { 2, 0 },
    { 3, 1 },
    { 4, 0 },
    { 5, 0 },
    { 6, 0 },
    { 7, 1 },
    { 8, 0 },
    { 9, 0 },
    { 10, 1 },
    { 11, 1 },
    { 12, 0 },
    { 13, 0 },
    { 14, 1 },
    { 15, 1 },
    { 16, 0 },
    { 18, 0 },
    { 19, 0 },
    { 255, 0 },
    { 0, 0 },
};
struct _GpAreaFlagRec Gp_NewGameFlagsStg2[28] = {
    { 1, 0 },
    { 2, 0 },
    { 3, 0 },
    { 5, 1 },
    { 6, 1 },
    { 7, 0 },
    { 9, 0 },
    { 11, 0 },
    { 12, 1 },
    { 15, 0 },
    { 16, 0 },
    { 18, 1 },
    { 19, 1 },
    { 20, 1 },
    { 21, 0 },
    { 22, 0 },
    { 23, 0 },
    { 24, 0 },
    { 25, 0 },
    { 26, 0 },
    { 27, 0 },
    { 29, 0 },
    { 30, 0 },
    { 32, 1 },
    { 34, 1 },
    { 38, 0 },
    { 255, 0 },
    { 0, 0 },
};
struct _GpAreaFlagRec Gp_NewGameFlagsStg4[46] = {
    { 1, 0 },
    { 2, 0 },
    { 3, 1 },
    { 4, 0 },
    { 5, 0 },
    { 6, 0 },
    { 7, 0 },
    { 8, 0 },
    { 9, 0 },
    { 10, 1 },
    { 11, 1 },
    { 12, 1 },
    { 13, 0 },
    { 14, 1 },
    { 15, 1 },
    { 16, 0 },
    { 17, 0 },
    { 18, 1 },
    { 19, 1 },
    { 20, 0 },
    { 21, 0 },
    { 22, 0 },
    { 23, 0 },
    { 24, 1 },
    { 25, 1 },
    { 26, 0 },
    { 27, 1 },
    { 28, 1 },
    { 29, 1 },
    { 30, 0 },
    { 31, 0 },
    { 32, 1 },
    { 33, 1 },
    { 34, 0 },
    { 35, 1 },
    { 39, 1 },
    { 40, 0 },
    { 41, 0 },
    { 42, 1 },
    { 43, 1 },
    { 44, 1 },
    { 45, 1 },
    { 46, 1 },
    { 47, 0 },
    { 48, 0 },
    { 255, 0 },
};
struct _GpAreaFlagRec Gp_NewGameFlagsStg5[34] = {
    { 1, 0 },
    { 2, 1 },
    { 3, 1 },
    { 4, 0 },
    { 5, 1 },
    { 6, 0 },
    { 7, 0 },
    { 8, 1 },
    { 9, 0 },
    { 10, 0 },
    { 11, 0 },
    { 12, 0 },
    { 13, 1 },
    { 14, 1 },
    { 15, 1 },
    { 16, 1 },
    { 17, 1 },
    { 18, 1 },
    { 19, 0 },
    { 20, 0 },
    { 21, 0 },
    { 22, 0 },
    { 23, 0 },
    { 24, 1 },
    { 25, 1 },
    { 26, 0 },
    { 27, 1 },
    { 28, 0 },
    { 29, 0 },
    { 30, 1 },
    { 31, 0 },
    { 32, 1 },
    { 33, 0 },
    { 255, 0 },
};

void Gp_ApplyNewGameAreaFlags(void)
{
    {
        GpAreaRec*     tbl;
        GpAreaObj*     obj;
        GpAreaFlagRec* rec;

        rec = Gp_NewGameFlagsStg1;
        tbl = Gp_AreaTableStg1;
        if (tbl != NULL) {
            for (; rec->field_0 != 0xFF; rec++) {
                if (rec->field_1 != 0) {
                    obj = tbl[rec->field_0].field_4;
                    if (obj != NULL) {
                        obj->spawnFlags |= 0x4;
                    }
                }
            }
        }
    }
    {
        GpAreaRec*     tbl;
        GpAreaObj*     obj;
        GpAreaFlagRec* rec;

        rec = Gp_NewGameFlagsStg2;
        tbl = Gp_AreaTableStg2;
        if (tbl != NULL) {
            for (; rec->field_0 != 0xFF; rec++) {
                if (rec->field_1 != 0) {
                    obj = tbl[rec->field_0].field_4;
                    if (obj != NULL) {
                        obj->spawnFlags |= 0x4;
                    }
                }
            }
        }
    }
    {
        GpAreaRec*     tbl;
        GpAreaObj*     obj;
        GpAreaFlagRec* rec;

        rec = Gp_NewGameFlagsStg4;
        tbl = Gp_AreaTableStg4;
        if (tbl != NULL) {
            for (; rec->field_0 != 0xFF; rec++) {
                if (rec->field_1 != 0) {
                    obj = tbl[rec->field_0].field_4;
                    if (obj != NULL) {
                        obj->spawnFlags |= 0x4;
                    }
                }
            }
        }
    }
    {
        GpAreaRec*     tbl;
        GpAreaObj*     obj;
        GpAreaFlagRec* rec;

        rec = Gp_NewGameFlagsStg5;
        tbl = Gp_AreaTableStg5;
        if (tbl != NULL) {
            for (; rec->field_0 != 0xFF; rec++) {
                if (rec->field_1 != 0) {
                    obj = tbl[rec->field_0].field_4;
                    if (obj != NULL) {
                        obj->spawnFlags |= 0x4;
                    }
                }
            }
        }
    }
}

void Gp_RebuildAreaIdBits(void)
{
    GameLocationKey  key;
    GameLocationKey* sess;
    s32              count;
    s32              i;
    u8               stage;

    sess      = &gGameSession->location.loc;
    stage     = sess->stage;
    key.room  = 1;
    key.view  = 2;
    key.stage = stage;
    if (gGameSession->location.loc.stage - 1 < 5) {
        count = Gp_AreaIdCounts[sess->stage - 1];
        for (i = 1; i <= count; i++) {
            key.area = i;
            if (_gpGetAreaFlag4(&key) == 1) {
                if (Gp_GetAreaFlag2(&key) == 1) {
                    if (key.area <= 32) {
                        Gp_AreaIdBits[0] &= ~(1 << (key.area - 1));
                    } else {
                        Gp_AreaIdBits[1] &= ~(1 << (key.area - 33));
                    }
                } else {
                    if (key.area <= 32) {
                        Gp_AreaIdBits[0] |= 1 << (key.area - 1);
                    } else {
                        Gp_AreaIdBits[1] |= 1 << (key.area - 33);
                    }
                }
            } else {
                if (key.area <= 32) {
                    Gp_AreaIdBits[0] &= ~(1 << (key.area - 1));
                } else {
                    Gp_AreaIdBits[1] &= ~(1 << (key.area - 33));
                }
            }
        }
    }
}

/// The flag entry `table[idx]`: its low 11 bits select a flag nibble, and its
/// bit 0x800 is added onto that nibble's value.
const TaskFuncTable3 Gp_DirTaskStates = { {
    Gp_InitDirState,
    Gp_DirTaskState1,
    taskKill,
} };

const GpDirActionTable Gp_DirActionFns = { {
    [WORLD_COLLISION_TRIGGER_ACTION_WARP] = Gp_DirAction0,
    Gp_DirAction1,
    [WORLD_COLLISION_TRIGGER_ACTION_CAP] = Gp_PostDirIfCapIdle,
    Gp_RunDirAction,
    Gp_ClearDirCursor,
    Gp_PostMsg13EF,
    Gp_SpawnEvt1IfCapIdle,
} };

static const GpVoidFuncTable6 Gp_WarpPhaseFns = { {
    Gp_SetupDirWarp,
    Gp_FadeDirWaitMsg,
    Gp_FadeDirAdvance,
    Gp_CommitWarp,
    Gp_WarpPhase4,
    Gp_CommitSaveLoc,
} };

static const GpVoidFuncTable5 D_80093990 = { {
    Gp_MsgPlayer3EE,
    Gp_MsgPlayer3F0,
    Gp_MsgPlayer3EF,
    Gp_MsgPlayerDirFacing,
    Gp_CommitDirWarp,
} };

static inline s16 _gpStageFlagNibble(u16* table, s16 idx)
{
    return GameFlag_GetNibble(table[idx] & 0x7FF) + (table[idx] & 0x800);
}

s16 Gp_LookupStageFlag(s16 idx)
{
    switch (gGameSession->location.loc.stage) {
        case GAME_STAGE_ACROPOLIS:
            if (idx >= 0xE) {
                break;
            }
            return _gpStageFlagNibble(D_map_akropolis_8017AA0C, idx);
        case GAME_STAGE_DRYFIELD:
            if (idx >= 0x1D) {
                break;
            }
            return _gpStageFlagNibble(D_map_dryfield_8017A824, idx);
        case GAME_STAGE_DRYFIELD_NIGHT:
            if (idx >= 0x1E) {
                break;
            }
            if (idx == 0x1D) {
                if (GameFlag_GetNibble(GAME_FLAG_07F) == 0) {
                    return 0;
                }
                return 0x802;
            }
            return _gpStageFlagNibble(D_map_dryfield_full_8017A738, idx);
        case GAME_STAGE_MINE_SHELTER:
            if (idx >= 0x1E) {
                break;
            }
            if (idx == 0 && GameFlag_GetNibble(GAME_FLAG_STORY_CHAPTER) == 6) {
                return GameFlag_GetNibble(D_map_shelter_8017AD88[0] & 0x7FF) + 0x800;
            }
            return _gpStageFlagNibble(D_map_shelter_8017AD88, idx);
        case GAME_STAGE_SHELTER_NEO_ARK:
            if (idx >= 9) {
                break;
            }
            return _gpStageFlagNibble(D_map_neo_ark_8017A9A0, idx);
    }
    return -1;
}

void Gp_ClearAreaFlag4(GameLocationKey* key)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = Gp_AreaTables[key->stage];
    if (rec != NULL) {
        obj = rec[key->area].field_4;
        if (obj != NULL) {
            obj->spawnFlags &= 0xFB;
        }
    }
}

static void Gp_InitDirState(Task* arg0)
{
    D_80114CDE       = 0;
    D_80114CDD       = 0;
    Gp_DirFlags      = 0;
    D_80114CD0       = 0;
    D_80114CDC       = 0;
    Gp_DirByte       = 0;
    Gp_DirNibble     = 0;
    Gp_DirPhase      = 0;
    D_80114CF8       = 0;
    D_80114CE0       = 1;
    Gp_AreaIdBits[0] = 0;
    Gp_AreaIdBits[1] = 0;
    D_80114D08       = 0xA;
    arg0->state++;
}

static void Gp_DirTaskState1(Task* task)
{
    Gp_CommitObj4CSave();
    func_800AD6BC();
}

s32 Gp_YawToPosXZ(Task* arg0, GpPosXZ* arg1)
{
    SVECTOR   vec;
    GfxCoord* coord;

    coord  = arg0->extra.tmd->coords;
    vec.vx = arg1->vx - (u16)coord->coord.t[0];
    vec.vy = 0;
    vec.vz = arg1->vz - (u16)coord->coord.t[2];
    VectorNormalSS(&vec, &vec);
    return ratan2(vec.vx, vec.vz) & 0xFFF;
}

void func_800AEE8C(Task* arg0)
{
    TaskFuncTable3 sp;
    Task*          playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    sp         = Gp_DirTaskStates;
    if (playerTask != NULL) {
        sp.funcs[arg0->state](arg0);
    }
}

static u8 Gp_GetViewCountLo(void)
{
    GameSession*    session;
    GpViewCountTbl* tbl;

    session = gGameSession;
    tbl     = Gp_ViewCountTables[session->location.loc.stage - 1];
    return (u8)tbl->field_0[session->location.loc.area - 1][session->location.loc.room - 1];
}

static void Gp_DirAction0(void)
{
    GpVoidFuncTable6 sp;

    sp = Gp_WarpPhaseFns;
    sp.funcs[(s16)Gp_DirPhase]();
}

static void Gp_DirAction1(void)
{
    GpVoidFuncTable5 sp;

    sp = D_80093990;
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_FINISHED) {
        if (D_80114CDE == 1) {
            D_80114CDD = D_80114CDE;
        }
    }
    if (D_80114CDD != 0) {
        gSceneCombatState.signals.bytes.endDelayFrames = SCENE_COMBAT_END_DELAY_FRAMES;
    }
    sp.funcs[(s16)Gp_DirPhase]();
}

static void Gp_ClearDirCursor(void)
{
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    D_80114CF8      = 0;
}

static void Gp_PostMsg13EF(void)
{
    DirectionActionRequest request;
    Task*                  roomTask;

    if (gGameSession->eventState == 0) {
        if (Gp_CapBusy() == 0) {
            request.control  = Gp_DirFlags;
            request.actionId = Gp_DirByte;
            request.argument = Gp_DirNibble;
            roomTask         = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);
            TASK_MESSAGE_DISPATCH_POINTER(roomTask, DIRECTION_MESSAGE_ROOM_ACTION, &request, 0);
        }
    }
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    D_80114CF8      = 0;
    if (D_80114CDC == 0) {
        gGameSession->dirActionBusy = 0;
    }
}

static void Gp_SpawnEvt1IfCapIdle(void)
{
    if (gGameSession->eventState == 0) {
        if (Gp_CapBusy() == 0) {
            Gp_SpawnEvt1(Gp_DirByte, Gp_DirNibble);
        }
    }
    D_80114CF8      = 0;
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
}

static void Gp_FadeDirAdvance(void)
{
    u8 fade;

    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        Fade_DrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
        Gp_DirFadeLevel += 0x1E;
        if ((s16)Gp_DirFadeLevel >= 0x100) {
            Gp_DirFadeLevel = 0xFF;
        }
    }
    Gp_DirPhase++;
}

static void Gp_CommitSaveLoc(void)
{
    u8 fade;

    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        Fade_DrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = (u8)Gp_WarpLoc.areaId;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = Gp_WarpLoc.warp;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = Gp_WarpLoc.room;
    Task_Spawn(0, 0x11, 0, 0);
    D_80114CF8   = 0;
    Gp_DirNibble = 0;
    Gp_DirByte   = 0;
    Gp_DirFlags  = 0;
}

static void Gp_MsgPlayer3EE(void)
{
    ActorTransform sp;
    Task*          playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gGameSession->eventState != 0) {
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
        D_80114CDD      = 0;
    } else {
        sp.rot.vx = 0;
        sp.rot.vz = 0;
        sp.rot.vy = Gp_DirNibble << 4;
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, 0x3EE, &sp, 0);
        Gp_DirPhase++;
    }
}

static void Gp_MsgPlayer3F0(void)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        Gp_DirPhase++;
    }
}

static void Gp_MsgPlayer3EF(void)
{
    GpFacingArg sp;
    Task*       playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    sp.field_0 = (Gp_DirFlags >> 8) & 1;
    sp.field_4 = Gp_DirByte & 0xF;
    TASK_MESSAGE_DISPATCH_POINTER(playerTask, 0x3EF, &sp, 0);
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    Gp_DirPhase++;
}

void Gp_SetCurAreaFlag4(void)
{
    GameLocationKey* key;
    GpAreaRec*       rec;
    GpAreaObj*       obj;

    key = &gGameSession->location.loc;
    rec = Gp_AreaTables[key->stage];
    if (rec != NULL) {
        obj = rec[key->area].field_4;
        if (obj != NULL) {
            obj->spawnFlags |= 0x4;
        }
    }
}

static void Gp_ApplyAreaFlag4List(s16 arg0, GpAreaFlagRec* arg1)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = Gp_AreaTables[arg0];
    if (rec != NULL) {
        for (; arg1->field_0 != 0xFF; arg1++) {
            if (arg1->field_1 != 0) {
                obj = rec[arg1->field_0].field_4;
                if (obj != NULL) {
                    obj->spawnFlags |= 0x4;
                }
            }
        }
    }
}
