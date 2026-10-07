#include "types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
s32 D_mine_cavern_8018EB50;

#include "rooms/mine_cavern.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "mine_cavern_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

// Message-table callbacks use the argument views required by this TU.

extern TaskDesc D_mine_cavern_8018E3F4;

u16 D_mine_cavern_8018E360 = 6016;

u16 D_mine_cavern_8018E362 = 2048;

u16 D_mine_cavern_8018E364 = 2048;

u16 D_mine_cavern_8018E366 = 950;

u16 D_mine_cavern_8018E368 = 2710;

SVECTOR D_mine_cavern_8018E36C[6] = {
    { 200, -2060, 5160, 0 },
    { 9000, -2040, 8710, 0 },
    { 0x4560, -2060, 5000, 0 },
    { 0x36B0, -2060, 290, 0 },
    { 4550, -2090, 280, 0 },
    { 6910, -2780, 3700, 0 },
};

SVECTOR D_mine_cavern_8018E39C[4] = {
    { 4550, -700, 340, 0 },
    { 0x3566, -800, 330, 0 },
    { 4340, -800, 8710, 0 },
    { 0x3552, -800, 8710, 0 },
};

u8 D_mine_cavern_8018E3BC[4][8] = {
    { 4, 22, 5, 23, 24, 25, 18, 19 },
    { 3, 2, 0, 0, 0, 0, 0, 0 },
    { 6, 21, 16, 0, 0, 0, 0, 0 },
    { 7, 8, 20, 17, 0, 0, 0, 0 },
};

s16 D_mine_cavern_8018E3DC = 0;

CVECTOR D_mine_cavern_8018E3E0[5] = {
    { 30, 30, 30, 0 },
    { 25, 25, 25, 0 },
    { 17, 21, 22, 0 },
    { 7, 15, 16, 0 },
    { 0, 9, 11, 0 },
};

TaskDesc D_mine_cavern_8018E3F4 = { { { TASK_BODY_NONE, 96 } }, func_mine_cavern_80182DC8, { .value = 0 } };

static TmdBone _gMineCavernModel10F60Skeleton[1] = {
#include "assets/mine_cavern_model_10F60_skeleton.inc"
};

static u32 _gMineCavernModel10F60PartVerts[1] = {
#include "assets/mine_cavern_model_10F60_partVerts.inc"
};

static SVECTOR _gMineCavernModel10F60Verts[20] = {
#include "assets/mine_cavern_model_10F60_verts.inc"
};

static SVECTOR _gMineCavernModel10F60Normals[11] = {
#include "assets/mine_cavern_model_10F60_normals.inc"
};

static u32 _gMineCavernModel10F60Stream[104] = {
#include "assets/mine_cavern_model_10F60_stream.inc"
};

static TmdSource _gMineCavernModel10F60 = {
    0,
    728,
    0,
    1,
    _gMineCavernModel10F60PartVerts,
    _gMineCavernModel10F60Verts,
    _gMineCavernModel10F60Normals,
    _gMineCavernModel10F60Skeleton,
    _gMineCavernModel10F60Stream,
};

static TmdBone _gMineCavernModel11244Skeleton[1] = {
#include "assets/mine_cavern_model_11244_skeleton.inc"
};

static u32 _gMineCavernModel11244PartVerts[1] = {
#include "assets/mine_cavern_model_11244_partVerts.inc"
};

static SVECTOR _gMineCavernModel11244Verts[20] = {
#include "assets/mine_cavern_model_11244_verts.inc"
};

static SVECTOR _gMineCavernModel11244Normals[11] = {
#include "assets/mine_cavern_model_11244_normals.inc"
};

static u32 _gMineCavernModel11244Stream[174] = {
#include "assets/mine_cavern_model_11244_stream.inc"
};

static TmdSource _gMineCavernModel11244 = {
    0,
    1248,
    0,
    1,
    _gMineCavernModel11244PartVerts,
    _gMineCavernModel11244Verts,
    _gMineCavernModel11244Normals,
    _gMineCavernModel11244Skeleton,
    _gMineCavernModel11244Stream,
};

DamageAttack D_mine_cavern_8018EAE0[1] = {
    { 18, 0 },
};

EnemyParams D_mine_cavern_8018EAE4 = { D_mine_cavern_8018EAE0, 30, 0, 0, 0, 0, 0, 0, 0 };

u8 D_mine_cavern_8018EAF4[36] = {
    0,
    2,
    2,
    2,
    0,
    0,
    0,
    10,
    10,
    10,
    30,
    30,
    30,
    10,
    30,
    15,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    30,
    30,
    30,
    30,
    10,
    1,
    30,
    30,
    30,
    0,
    0,
    0,
};

SVECTOR D_mine_cavern_8018EB18[4] = {
    { 4550, 0, 310, 0 },
    { 0x3566, 0, 330, 0 },
    { 4340, 0, 8710, 0 },
    { 0x3552, 0, 8710, 0 },
};

TaskDesc D_mine_cavern_8018EB38[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_mine_cavern_80183A68, { .model = &_gMineCavernModel10F60 } },
    { { { TASK_BODY_TMD, 96 } }, func_mine_cavern_80183C10, { .model = &_gMineCavernModel11244 } },
};

s32 D_mine_cavern_8018EB54;

s32 D_mine_cavern_8018EB58;

u16 D_mine_cavern_8018EB5C;

/// One byte of gameplay state. Read back with `lb` elsewhere, so it is signed.

static void func_mine_cavern_8017DDFC(Task* arg0);
static void func_mine_cavern_8017DEE4(Task* task);

s32 func_mine_cavern_8017D908(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);

    if (in->areaId == GAME_AREA_MINE_SECRET_PASSAGE) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE) != 1) {
            if (in->queryOnly != ROOM_EVENT_EXECUTE) {
                return 0;
            }
            Gp_SetNibbleIf(in->flagId, 2);
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && gGameSession->location.loc.variant == gSceneCombatState.signals.bytes.battlePhase) {
                Gp_RunCapCmd1(9);
                return 0;
            }
            Gp_RunCapCmd1(0xD);
            if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS) != 0) {
                return 0;
            }
            gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS, 1);
            return 0;
        }
        if (in->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS) != 2) {
            gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS, 2);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 0);
        }
    }

    if (in->areaId == GAME_AREA_MINE_GORGE) {
        if (gGameSession->location.loc.variant == 1 || gGameSession->location.loc.variant == 4) {
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                    Gp_RunCapCmd1(0xB);
                }
                return 0;
            }
        }
    }
    return 1;
}

s32 func_mine_cavern_8017DAA0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    u8 temp;

    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) != 0) {
            return 0;
        }
        if (gSceneCombatState.signals.bytes.battlePhase == arg2 && (gGameSession->location.loc.variant == arg2 || gGameSession->location.loc.variant == 4)) {
            Gp_RunCapCmd1(0xA);
        } else if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE) == 1) {
            Gp_RunCapCmd1(0x11);
        } else if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE) == 3) {
            Gp_RunCapCmd1(0x12);
            taskSpawnFromTable(D_mine_cavern_80183CA4, 0, 0, 0);
        } else if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) != 2) {
            Gp_RunCapCmd1(5);
            taskSpawnFromTable(D_mine_cavern_80183CA4, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(5);
        }
    }
    temp = gGameSession->location.loc.variant;
    if (temp == 1 || temp == 4) {
        switch (arg2) {
            case 8:
                Gp_StartCapSlot(8, 1, gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED) & 1);
                break;
            case 14:
                Gp_StartCapSlot(0xE, 1, ((u32)gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED) >> 1) & 1);
                break;
            case 15:
                Gp_StartCapSlot(0xF, 1, ((u32)gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED) >> 2) & 1);
                break;
            case 16:
                Gp_StartCapSlot(0x10, 1, ((u32)gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED) >> 3) & 1);
                break;
        }
    }
    return 0;
}

/// Room script callback that does nothing and reports 0.
s32 func_mine_cavern_8017DC50(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_mine_cavern_8017DC58(Task* task, s32 msgId, DirectionActionRequest* request, s32 arg3)
{
    if ((request->actionId == 6) && (gameFlagGetNibble(GAME_FLAG_0C4) == 1)) {
        Gp_RunCapCmd1(6);
    }
    return 0;
}

/// Advances the cavern's collapse sequence one step: flag 0xE6 goes 0 -> 1
/// (bit 0 of `Gp_StateC08.flags` set) and 1 -> 2 (quake shake, then camera
/// pan), each step writing `D_mine_cavern_8018EB50` to the step number.
s32 func_mine_cavern_8017DC9C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS) == 0) {
        Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
        Gp_PulseState1C();
        gameFlagSetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS, 1);
        D_mine_cavern_8018EB50 = 1;
    } else if (gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS) == 1) {
        func_800E3FAC(0xA2, 0x3D);
        func_800E8634(D_mine_cavern_80188A3C, 0, D_mine_cavern_80188D24);
        gameFlagSetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS, 2);
    }
    return 0;
}

s32 func_mine_cavern_8017DD38(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0xD) {
        sndEvtRequestScriptStart(0x54020000 | 0xD, 0, 0);
    }
    return 0;
}

void func_mine_cavern_8017DD6C(Task* task)
{
    if (capIsBusy() == 0) {
        if (capGetVariantKey() == 0xB) {
            gameFlagSetNibble(GAME_FLAG_0C4, 1);
            gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE, 2);
            gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON, 0);
        }
        if (capGetVariantKey() == 0x15) {
            gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, 1);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_MINE_CAVERN, 0);
        }
        taskKill(task);
    }
}

static void func_mine_cavern_8017DDFC(Task* arg0)
{
    arg0->msgTable = D_mine_cavern_80183C6C;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.variant == 1) && (gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_INTRO_SEEN) == 0)) {
        func_800E8634(D_mine_cavern_80187C74, 0, D_mine_cavern_8018804C);
        func_mine_cavern_8017E394();
        gameFlagSetNibble(GAME_FLAG_MINE_CAVERN_INTRO_SEEN, 1);
    } else {
        gStageSceneMusicEntry = 1;
    }
    taskSpawnFromTable(&D_mine_cavern_8018E3F4, 0, 0, 0);
    if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) != 0) {
        mineCavernSetProgressSpritesHidden(1);
    } else {
        mineCavernSetProgressSpritesHidden(0);
    }
    arg0->state            = arg0->state + 1;
    D_mine_cavern_8018EB50 = 0;
}

static void func_mine_cavern_8017DEE4(Task* task)
{
    s32 flag;

    flag = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS);
    if ((flag == 1) && (D_mine_cavern_8018EB50 == flag) && (Gp_StateC08.mode != D_mine_cavern_8018EB50)) {
        func_800E8634(D_mine_cavern_80188214, 0, D_mine_cavern_801887B4);
        D_mine_cavern_8018EB50 = 2;
    }
}

/// The room task's state handlers, run by `func_mine_cavern_8017DF54`.
static const TaskFuncTable3 D_mine_cavern_8017D5C4 = {
    { func_mine_cavern_8017DDFC, func_mine_cavern_8017DEE4, taskKill },
};

/// Runs the room task's current state handler from the room's three-entry
/// table, copying the table onto the stack before the call.
void func_mine_cavern_8017DF54(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_cavern_8017D5C4;
    sp.funcs[task->state](task);
}

void func_mine_cavern_8017DFAC(s32 arg0)
{
    if ((gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS) == 1 && D_mine_cavern_8018EB54 == 0) ||
        (gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS) == 2 && D_mine_cavern_8018EB54 == 1)) {
        sceneReleaseBattleRefWithRewards(Gp_LookupSlot4(0), 0x1E);
        gSceneCombatState.signals.bytes.endDelayFrames = arg0;
        gGameSession->flowFlags                       |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        D_mine_cavern_8018EB54                        += 1;
        return;
    }
    if (arg0 < gSceneCombatState.signals.bytes.endDelayFrames) {
        gSceneCombatState.signals.bytes.endDelayFrames = arg0;
    }
}

void func_mine_cavern_8017E088(s16 arg0)
{
    Gp_StartCapSlot(arg0, 1, 1);
}

void func_mine_cavern_8017E0B4(void)
{
    gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
    if (gSceneCombatState.battleRefs == 0) {
        (sceneAcquireBattleRef)(0);
    }
    sceneEngageBattle(1);
}

void func_mine_cavern_8017E0F4(s32 arg0)
{
    if (arg0 != 0) {
        gGameSession->flowFlags &= (0xFF ^ GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
        return;
    }
    gGameSession->flowFlags |= GAME_SESSION_FLOW_SKIP_AREA_MUSIC;
    gGameSession->flowFlags |= GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY;
}

/// Room script callback: stores its argument into `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent`.
void func_mine_cavern_8017E150(s8 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = arg0;
}

void func_mine_cavern_8017E15C(void)
{
    Gp_ApplyAreaRecs(D_mine_cavern_8018E32C);
}

/// Room script callback: selects its argument as the scene music entry
/// (`gStageSceneMusicEntry`).
void func_mine_cavern_8017E180(u8 arg0)
{
    gStageSceneMusicEntry = arg0;
}

void func_mine_cavern_8017E18C(Task* task)
{
    task->killCountdown++;
    switch (task->killCountdown) {
        case 0x21:
        case 0x6:
        case 0x40:
        case 0x7C:
        case 0x60:
        case 0x8C:
            sndEvtRequestScriptStart(0x1000003A, 0, 0x30);
            break;
        case 0x50:
        case 0x12:
        case 0x30:
        case 0x70:
        case 0x87:
        case 0x218:
            sndEvtRequestScriptStart(0x10000039, 0, 0x30);
            break;
    }
    if ((gGameSession->evtSkipped != 0) || (task->killCountdown >= 0x219)) {
        taskKill(task);
    }
}

void func_mine_cavern_8017E2D8(void)
{
    sndEvtRequestMidiStop(0, 0x64);
}

/// Sets bit 0 of `Gp_StateC08.flags` and requests all-effect cancellation on `gRoomEffectState`.
void func_mine_cavern_8017E2FC(void)
{
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    Gp_PulseState1C();
}
