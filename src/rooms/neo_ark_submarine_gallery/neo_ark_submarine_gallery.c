#include "rooms/neo_ark_submarine_gallery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_submarine_gallery_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/area_flags.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room_common.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// 0x1E pair the gallery hands `taskSpawn` for the helper it raises in state 3,
/// the same shape `D_mine_mesa_80189B38` has.
extern RoomFadeStorage D_neo_ark_submarine_gallery_8018591C;

/// Staging save location the gallery commits: area / warp / room
/// hold what `func_neo_ark_submarine_gallery_8017EA0C` copies out of the
/// incoming location, and `func_neo_ark_submarine_gallery_8017E86C` moves those
/// same three bytes into `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area` / `field_8` / `field_5`.
extern RoomEventMsg D_neo_ark_submarine_gallery_80185924;

AreaApplyRec D_neo_ark_submarine_gallery_8018590C[4] = {
    { 5, 12, 4, 0 },
    { 5, 14, 4, 0 },
    { 5, 30, 4, 0 },
    { 255, 0, 0, 0 },
};

RoomFadeStorage D_neo_ark_submarine_gallery_8018591C = { 0 };

RoomEventMsg D_neo_ark_submarine_gallery_80185924 = { 0 };

static void func_neo_ark_submarine_gallery_8017EB50(Task* arg0);

static void func_neo_ark_submarine_gallery_8017EBC4(Task* arg0);

static s32  func_neo_ark_submarine_gallery_8017EC24(u16 arg0, s32 arg1);
static void func_neo_ark_submarine_gallery_8017EED8(Task* arg0);
static void func_neo_ark_submarine_gallery_8017EF14(Task* arg0);
static void func_neo_ark_submarine_gallery_8017EF8C(Task* arg0);

#include "../../shared/water_refraction_task.inc.c"

#include "../../shared/water_distort_band_task.inc.c"

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_submarine_gallery_8017EBCC`: set-up, idle, then kill.
static const TaskFuncTable3 D_neo_ark_submarine_gallery_8017D614 = {
    { func_neo_ark_submarine_gallery_8017EB50, func_neo_ark_submarine_gallery_8017EBC4, taskKill }
};

/// Runs the gallery's save sequence once state 0 has asked for the caption.
/// State 1 waits for that caption, state 2 takes the confirm key or backs out,
/// state 3 raises the helper task 0x31 and queues the sound event, state 4
/// waits for that voice, and state 5 - the commit - copies the staged location
/// into `gMcSaveData` and reloads. Every state advances by one except a
/// confirmed cancel and the commit itself.
void func_neo_ark_submarine_gallery_8017E86C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(9, CAP_PLAYBACK_IN_PLACE);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (capIsBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (capGetVariantKey() != 0xA) {
                taskKill(arg0);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_TriggerPeIfArmed();
            arg0->state++;
            break;
        case 3:
            D_neo_ark_submarine_gallery_8018591C.fade.blend      = SCREEN_FADE_SUBTRACT;
            D_neo_ark_submarine_gallery_8018591C.fade.phase      = SCREEN_FADE_RUNNING;
            D_neo_ark_submarine_gallery_8018591C.fade.rampFrames = 0x1E;
            taskSpawn(1, 0x31, 0, &D_neo_ark_submarine_gallery_8018591C.fade);
            sndEvtRequestScriptStart(SOUND_NEO_ARK_SUB_GALLERY_TO_ISLAND, 0, 0);
            arg0->state++;
            break;
        case 4:
            if (sndScriptHasActiveId(SOUND_NEO_ARK_SUB_GALLERY_TO_ISLAND) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_neo_ark_submarine_gallery_80185924.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_neo_ark_submarine_gallery_80185924.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_neo_ark_submarine_gallery_80185924.areaId)[1];
            taskSpawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_neo_ark_submarine_gallery_8017EA04(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Gallery message handler. Message 0xE, while the incoming location still
/// reports no pending flag, latches the save location the outgoing message
/// carries and starts the cutscene the gallery leads out of. Returns 0 for that
/// message and 1 for every other one.
s32 func_neo_ark_submarine_gallery_8017EA0C(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    mapNeoArkResolveRoomVariant(src, dst);
    if (src->areaId == GAME_AREA_NEO_ARK_ISLAND) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_neo_ark_submarine_gallery_80185924.warp              = (u8)dst->areaId;
            D_neo_ark_submarine_gallery_80185924.field_4           = dst->warp;
            ((u8*)&D_neo_ark_submarine_gallery_80185924.areaId)[1] = dst->room;
            taskSpawnFromTable(&D_neo_ark_submarine_gallery_801818AC, 0, 0, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_neo_ark_submarine_gallery_8017EABC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 2:
            if (D_neo_ark_submarine_gallery_801818B8 == 1) {
                capSpawnEventIfIdle(2, CAP_EVENT_NO_FLAGS);
            } else {
                capSpawnEventIfIdle(4, CAP_EVENT_NO_FLAGS);
            }
            break;
        case 3:
            if (gGameSession->location.loc.variant == 4) {
                capSpawnEventIfIdle(5, CAP_EVENT_NO_FLAGS);
            } else if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                capSpawnEventIfIdle(3, CAP_EVENT_NO_FLAGS);
            } else {
                capSpawnEventIfIdle(6, CAP_EVENT_NO_FLAGS);
            }
            break;
    }
    return 0;
}

s32 func_neo_ark_submarine_gallery_8017EB48(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

static void func_neo_ark_submarine_gallery_8017EB50(Task* arg0)
{
    arg0->msgTable = D_neo_ark_submarine_gallery_80181884;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 4) {
        taskSpawnFromTable(D_neo_ark_submarine_gallery_801818BC, 0, 0, 0);
    }
    arg0->state = (s32)(arg0->state + 1);
}

static void func_neo_ark_submarine_gallery_8017EBC4(Task* arg0)
{
}

/// Entry task tick: dispatches on the task's state through
/// `D_neo_ark_submarine_gallery_8017D614`, copied to the stack first.
void func_neo_ark_submarine_gallery_8017EBCC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_submarine_gallery_8017D614;
    sp.funcs[task->state](task);
}

/// Sweeps a 32-wedge red disc of radius `arg0` through the view matrix: each
/// step projects the fan's centre and the two rim points 0x80 apart and, when
/// the projection passes, queues one semi-transparent `POLY_G3` plus its
/// drawing-mode packet (tpage 0x2A) into `gGpuCurrentOt[(otz >> 4) + 0x18]`.
/// The disc sits at view-space height 0x14B4.
static s32 func_neo_ark_submarine_gallery_8017EC24(u16 arg0, s32 arg1)
{
    SVECTOR  p0;
    SVECTOR  p1;
    SVECTOR  p2;
    long     sxy0;
    long     sxy1;
    long     sxy2;
    long     p;
    long     flag;
    POLY_G3* prim;
    DR_MODE* dr;
    s32      otz;
    s32      ang;
    s16      i;
    s16      y;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    y = 0x14B4;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    for (i = 0; i < 0x20; i++) {
        p0.vx = 0;
        p0.vy = y;
        p0.vz = 0;
        ang   = (i << 16) >> 9;
        p1.vx = (rsin(ang) * arg0) >> 12;
        p1.vy = y;
        p1.vz = (rcos(ang) * arg0) >> 12;
        ang   = ang + 0x80;
        p2.vx = (rsin(ang) * arg0) >> 12;
        p2.vy = y;
        p2.vz = (rcos(ang) * arg0) >> 12;
        otz   = RotTransPers3(&p0, &p1, &p2, &sxy0, &sxy1, &sxy2, &p, &flag);
        if (flag >= 0) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG3(prim);
            setRGB0(prim, 0xFF, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, 0, 0);
            setSemiTrans(prim, 1);
            GPU_PRIMITIVE_XY_WORD(prim, 0) = sxy0;
            GPU_PRIMITIVE_XY_WORD(prim, 1) = sxy1;
            GPU_PRIMITIVE_XY_WORD(prim, 2) = sxy2;
            addPrim(&gGpuCurrentOt[(otz >> 4) + 0x18], prim);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 0, 0x2A);
            addPrim(&gGpuCurrentOt[(otz >> 4) + 0x18], dr);
        }
    }
}

static void func_neo_ark_submarine_gallery_8017EED8(Task* arg0)
{
    if (gGameSession->location.loc.variant != 4) {
        arg0->killCountdown = 0;
    } else {
        arg0->killCountdown = 0x780;
    }
    arg0->state = (s32)(arg0->state + 1);
}

static void func_neo_ark_submarine_gallery_8017EF14(Task* arg0)
{
    s32 mode;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] != NULL) {
        mode = 4;
        if (gGameSession->location.loc.variant != mode && gGameSession->battleResetPending != 0) {
            gGameSession->location.loc.variant = mode;
        }
        if (arg0->killCountdown < 0x780) {
            arg0->killCountdown = (s16)((u16)arg0->killCountdown + 0x10);
        }
        func_neo_ark_submarine_gallery_8017EC24((u16)arg0->killCountdown, mode);
    }
}

static void func_neo_ark_submarine_gallery_8017EF8C(Task* arg0)
{
}

/// State handlers of the disc task, indexed by its state through
/// `func_neo_ark_submarine_gallery_8017EF94`: set-up, the per-frame disc sweep,
/// then an idle state.
static const TaskFuncTable3 D_neo_ark_submarine_gallery_8017D63C = {
    { func_neo_ark_submarine_gallery_8017EED8, func_neo_ark_submarine_gallery_8017EF14,
      func_neo_ark_submarine_gallery_8017EF8C }
};

/// Disc task tick: dispatches on the task's state through
/// `D_neo_ark_submarine_gallery_8017D63C`, copied to the stack first.
void func_neo_ark_submarine_gallery_8017EF94(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_submarine_gallery_8017D63C;
    sp.funcs[task->state](task);
}
