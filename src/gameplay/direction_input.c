#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area_entry.h"
#include "area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/direction.h"
#include "direction.h"
#include "hud_sprites.h"
#include "loading.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/display.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

/* Define BSS before API headers to preserve first-declaration order. */
s16 D_80114CD0;

u16 Gp_DirFlags;

u16 D_80114CD4;

u16 Gp_DirPhase;

u8 Gp_DirByte;

u8 Gp_DirNibble;

u8 Gp_DirAlt;

u8 Gp_DirAltNibble;

u8 D_80114CDC;

u8 D_80114CDD;

u8 D_80114CDE;

s16 D_80114CE0;

RoomEventMsg Gp_WarpLoc;

s32 D_80114CF0;

s16 D_80114CF4;

u16 Gp_DirFadeLevel;

u8 D_80114CF8;

s32 Gp_AreaIdBits[2];

s16 D_80114D08;

#include "direction_input.h"

#include "gameplay/direction_input.h"

/// Five stage counts, followed by three unexplained nonzero bytes.
/// The tail is retained for review, not interpreted as additional stages.
s8 Gp_AreaIdCounts[8] = {
    17,
    38,
    38,
    49,
    33,
    -31,
    -1,
    34,
};

u8 gViewIdentityMap[VIEW_IDENTITY_MAP_LENGTH] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
};

void func_800AD6BC(void)
{
    Task*                slot;
    PlayerStatus*        cfg;
    u32                  flags;
    u32                  action;
    u32                  mask;
    DirectionActionTable funcs;

    funcs = Gp_DirActionFns;
    cfg   = &gPlayerStatus;
    slot  = gameGetTaskSlot(GAME_TASK_SLOT_VIEW_GATE);
    if (slot != NULL) {
        if (slot->spawnArg1.value != gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view) {
            viewChangeStub();
            D_80114D08 = 0xA;
        }
    }
    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
        D_80114D08 = 0xA;
    }
    if (D_80114CF8 == 0) {
        if (Gp_StateC08.mode == ATTACHMENT_MODE_IDLE) {
            gGameSession->dirActionBusy = 0;
            if (D_80114D08 != 0) {
                D_80114D08 = (u16)D_80114D08 - 1;
            }
            if (worldCollisionReadActionHit(&Gp_DirFlags, &Gp_DirByte, &Gp_DirNibble) != 0) {
                if (D_80114CD0 != (s16)Gp_DirFlags) {
                    D_80114CDC = 1;
                } else {
                    D_80114CDC = 0;
                }
                Gp_DirPhase = 0;
                flags       = Gp_DirFlags;
                mask        = flags & WORLD_COLLISION_TRIGGER_AUTOMATIC;
                if (gSceneCombatState.signals.bytes.endDelayFrames == 0) {
                    if (mask && (gDisplayState.pendingMode == DISPLAY_MODE_NONE) && !(gGameSession->padPressed & 0x10)) {
                        if (!(flags & WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE)) {
                            D_80114CF8 = 1;
                        } else if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
                            D_80114CF8 = 1;
                        }
                    } else if (cfg->interactionPressed != 0) {
                        if (!(gGameSession->padPressed & 0x10)) {
                            if (!(Gp_DirFlags & WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE)) {
                                if (D_80114D08 == 0) {
                                    D_80114CF8 = 1;
                                    D_80114D08 = 0xA;
                                }
                            } else if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
                                if (D_80114D08 == 0) {
                                    D_80114CF8 = 1;
                                    D_80114D08 = 0xA;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    D_80114CD0 = (s16)Gp_DirFlags;
    if (D_80114CF8 != 0) {
        gGameSession->dirActionBusy = 1;
        action                      = (u8)Gp_DirFlags;
        if (action != WORLD_COLLISION_TRIGGER_ACTION_CANCEL) {
            funcs.handlers[action]();
        } else {
            Gp_DirNibble    = 0;
            Gp_DirByte      = 0;
            Gp_DirAltNibble = 0;
            Gp_DirAlt       = 0;
            Gp_DirFlags     = 0;
            D_80114CD4      = 0;
            D_80114CF8      = 0;
        }
    } else {
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        Gp_DirFlags     = 0;
        D_80114CD4      = 0;
    }
    D_80114CDE = gSceneCombatState.signals.bytes.battlePhase;
}

void Gp_SetupDirWarp(void)
{
    Task*              slot7;
    Task*              slot3;
    PlayerStatus*      cfg;
    GameActor*         actor;
    GameLocationKey*   sess;
    DirectionWarpEntry warpEntry;
    ActorTransform     msg;
    SVECTOR            pos;
    SVECTOR            pos2;
    s32                stage;
    s32                room;
    s16                ret;

    sess  = &gGameSession->location.loc;
    stage = sess->stage;
    room  = sess->area;
    slot7 = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);
    slot3 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    cfg   = &gPlayerStatus;
    actor = slot3->work;

    if (gGameSession->eventState != 0) {
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
        return;
    }

    D_80114CF4      = 0;
    Gp_DirFadeLevel = 0;
    // The current endpoint supplies facing and sounds for this departure.
    warpEntry = Gp_WarpTables[stage - 1][room - 1][(Gp_DirNibble >> 4) - 1];

    Gp_WarpLoc.field_4   = 1;
    Gp_WarpLoc.room      = 1;
    Gp_WarpLoc.queryOnly = ROOM_EVENT_QUERY_ONLY;
    Gp_WarpLoc.areaId    = Gp_DirByte;
    Gp_WarpLoc.warp      = Gp_DirNibble & 0xF;
    Gp_WarpLoc.flagId    = warpEntry.mapFlagId;

    ret        = Gp_DispatchMsgPtrs(slot7, ROOM_EVENT_MESSAGE_RESOLVE, &Gp_WarpLoc, &Gp_WarpLoc);
    D_80114CF4 = ret;

    switch (ret) {
        case 1:
            if (warpEntry.departureSound != DIRECTION_WARP_SOUND_NONE) {
                D_80114CF0 = warpEntry.departureSound;
            } else {
                D_80114CF0 = 0;
            }
            msg.rot.vx = 0;
            msg.rot.vz = 0;
            msg.rot.vy = (warpEntry.player.yaw.word + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
            if (warpEntry.player.yaw.word == ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT_ALT || warpEntry.player.yaw.word == ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT) {
                pos.vx     = -0x5C1;
                pos.vy     = 0;
                pos.vz     = 0x9C1;
                msg.rot.vy = Gp_YawToPosXZ(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], &pos);
            } else if (warpEntry.player.yaw.word == ACTOR_SPAWN_YAW_KEEP_FACING) {
                msg.rot.vy = actor->rotation.vy;
            }
            TASK_MESSAGE_DISPATCH_POINTER(slot3, 0x3EE, &msg, 0);
            if (warpEntry.flags & DIRECTION_WARP_FLAG_FADE_DEPARTURE) {
                Gp_DirFadeLevel = 0x1E;
            }
            Gp_DirPhase++;
            break;

        case 0:
            if (warpEntry.blockedSound != DIRECTION_WARP_SOUND_NONE) {
                D_80114CF0 = warpEntry.blockedSound;
            } else {
                D_80114CF0 = 0;
            }
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                Gp_WarpLoc.field_4   = gSceneCombatState.signals.bytes.battlePhase;
                Gp_WarpLoc.room      = gSceneCombatState.signals.bytes.battlePhase;
                Gp_WarpLoc.queryOnly = ROOM_EVENT_EXECUTE;
                Gp_WarpLoc.areaId    = Gp_DirByte;
                Gp_WarpLoc.warp      = Gp_DirNibble & 0xF;
                Gp_WarpLoc.flagId    = warpEntry.mapFlagId;
                Gp_DispatchMsgPtrs(slot7, ROOM_EVENT_MESSAGE_RESOLVE, &Gp_WarpLoc, &Gp_WarpLoc);
                D_80114CF8              = 0;
                Gp_DirNibble            = 0;
                Gp_DirByte              = 0;
                Gp_DirFlags             = 0;
                cfg->interactionPressed = 0;
                if (D_80114CF0 != 0 && cfg->hp > 0) {
                    sndEvtRequestScriptStart(D_80114CF0, 0, 0);
                }
                return;
            }
            msg.rot.vx = 0;
            msg.rot.vz = 0;
            msg.rot.vy = (warpEntry.player.yaw.word + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
            if (warpEntry.player.yaw.word == ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT_ALT || warpEntry.player.yaw.word == ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT) {
                pos2.vx    = -0x5C1;
                pos2.vy    = 0;
                pos2.vz    = 0x9C1;
                msg.rot.vy = Gp_YawToPosXZ(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], &pos2);
            } else if (warpEntry.player.yaw.word == ACTOR_SPAWN_YAW_KEEP_FACING) {
                msg.rot.vy = actor->rotation.vy;
            }
            TASK_MESSAGE_DISPATCH_POINTER(slot3, 0x3EE, &msg, 0);
            Gp_DirPhase++;
            break;

        case 2:
            Gp_WarpLoc.field_4   = 1;
            Gp_WarpLoc.room      = 1;
            Gp_WarpLoc.queryOnly = ROOM_EVENT_EXECUTE;
            Gp_WarpLoc.areaId    = Gp_DirByte;
            Gp_WarpLoc.warp      = Gp_DirNibble & 0xF;
            Gp_WarpLoc.flagId    = warpEntry.mapFlagId;
            Gp_DispatchMsgPtrs(slot7, ROOM_EVENT_MESSAGE_RESOLVE, &Gp_WarpLoc, &Gp_WarpLoc);
            D_80114CF8              = 0;
            Gp_DirNibble            = 0;
            Gp_DirByte              = 0;
            Gp_DirFlags             = 0;
            cfg->interactionPressed = 0;
            break;
    }
}

void Gp_FadeDirWaitMsg(void)
{
    Task* playerTask;
    u8    fade;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
        Gp_DirFadeLevel += 0x1E;
        if ((s16)Gp_DirFadeLevel >= 0x100) {
            Gp_DirFadeLevel = 0xFF;
        }
    }
    if (taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        if (D_80114CF4 != 0) {
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        }
        Gp_DirPhase++;
    }
}

void Gp_CommitWarp(void)
{
    Task*              slot3;
    Task*              slot7;
    PlayerStatus*      cfg;
    GameLocationKey*   sess;
    DirectionWarpEntry warpEntry;
    RoomEventMsg*      loc;
    u8                 fade;

    slot3 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    cfg   = &gPlayerStatus;
    slot7 = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);

    sess      = &gGameSession->location.loc;
    warpEntry = Gp_WarpTables[sess->stage - 1][sess->area - 1][(Gp_DirNibble >> 4) - 1];

    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
        Gp_DirFadeLevel += 0x1E;
        if ((s16)Gp_DirFadeLevel >= 0x100) {
            Gp_DirFadeLevel = 0xFF;
        }
    }

    loc               = &Gp_WarpLoc;
    loc->field_4      = 1;
    loc->room         = 1;
    loc->queryOnly    = ROOM_EVENT_EXECUTE;
    Gp_WarpLoc.areaId = Gp_DirByte;
    loc->warp         = Gp_DirNibble & 0xF;
    loc->flagId       = warpEntry.mapFlagId;
    Gp_DispatchMsgPtrs(slot7, ROOM_EVENT_MESSAGE_RESOLVE, loc, loc);

    if (D_80114CF0 != 0) {
        if (cfg->hp > 0) {
            sndEvtRequestScriptStart(D_80114CF0, 0, 0);
        }
    }

    if (D_80114CF4 == 0) {
        taskMessageDispatch(slot3, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        D_80114CF8              = 0;
        Gp_DirNibble            = 0;
        Gp_DirByte              = 0;
        Gp_DirFlags             = 0;
        cfg->interactionPressed = 0;
    } else {
        Gp_DirPhase++;
    }
}

void Gp_WarpPhase4(void)
{
    u8 fade;

    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
        Gp_DirFadeLevel += 0x1E;
        if ((s16)Gp_DirFadeLevel >= 0x100) {
            Gp_DirFadeLevel = 0xFF;
        }
    }
    if (D_80114CF0 == 0 || sndScriptHasActiveId(D_80114CF0) == 0) {
        Gp_DirPhase++;
    }
}
