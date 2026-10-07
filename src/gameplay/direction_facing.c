#include "gameplay/area_transitions.h"

#include "common.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "direction_input.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"

#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/acropolis_bridge.h"

#include "rooms/acropolis_fountain.h"

#include "rooms/acropolis_helicopter_landing_pad.h"

#include "rooms/shelter_b4_reservoir.h"

#include "rooms/shelter_b4_water_supply.h"

/// Surface classes along one stair flight whose surface changes on the way.
///
/// A stair trigger normally carries a single surface class for its whole
/// flight; one that sets bit 7 of its first parameter selects a flight record
/// instead. Each row gives the `GameActor.surfaceClass` to apply once a given
/// number of steps has been taken, so a climb of `n` steps reads entries 0
/// through `n` of the row for its direction. The rows are borrowed and are
/// only read; most belong to the room that owns the flight.
typedef struct {
    u8* ascent;  // Row for a walk up the flight, indexed by steps taken so far
    u8* descent; // Row for a walk down the flight, indexed the same way
} _DirectionStairSurfaces;
STATIC_ASSERT_SIZEOF(_DirectionStairSurfaces, 8);

extern _DirectionStairSurfaces D_801149FC[];

extern u8 D_801149E8[10];

extern u8 D_801149F4[8];

u8 D_801149E8[10] = {
    4,
    4,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6
};
u8 D_801149F4[8] = {
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4
};

_DirectionStairSurfaces D_801149FC[5] = {
    { D_acropolis_bridge_80189A9C, &D_acropolis_bridge_80189A9C[12] },
    { D_801149E8, D_801149F4 },
    { D_shelter_b4_water_supply_801826FC, &D_shelter_b4_water_supply_801826FC[16] },
    { &D_shelter_b4_water_supply_801826FC[32], &D_shelter_b4_water_supply_801826FC[48] },
    { D_shelter_b4_reservoir_801850C8, D_shelter_b4_reservoir_801850D8 }
};

void Gp_MsgPlayerDirFacing(void)
{
    Task*      slot;
    GameActor* actor;
    u8         flags;
    s32        surfaceIndexBase;
    u8*        row;

    actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    flags = Gp_DirByte;
    if (flags & 0x80) {
        surfaceIndexBase = actor->scriptMotion.surfaceIndexBase;
        if (Gp_DirFlags & 0x100) {
            row                 = D_801149FC[(flags & 0x70) >> 4].descent;
            actor->surfaceClass = row[(flags & 0xF) - surfaceIndexBase];
        } else {
            row                 = D_801149FC[(flags & 0x70) >> 4].ascent;
            actor->surfaceClass = row[(flags & 0xF) - surfaceIndexBase];
        }
    } else {
        actor->surfaceClass = (flags & 0x70) >> 4;
    }

    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (taskMessageDispatch(slot, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        taskMessageDispatch(slot, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
        D_80114CDD      = 0;
    } else if (worldCollisionReadActionHit(&D_80114CD4, &Gp_DirAlt, &Gp_DirAltNibble)) {
        if ((u8)D_80114CD4 == WORLD_COLLISION_TRIGGER_ACTION_WARP) {
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_DirPhase++;
        }
    }
}

void Gp_CommitDirWarp(void)
{
    Task*         slot;
    RoomEventMsg* loc;
    McSaveData*   save;

    slot = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);
    loc  = &Gp_WarpLoc;

    // The area selector is a byte; assigning the halfword clears its high byte.
    Gp_WarpLoc.areaId = Gp_DirAlt;
    loc->warp         = Gp_DirAltNibble & 0xF;
    loc->field_4      = 1;
    loc->room         = 1;
    loc->queryOnly    = ROOM_EVENT_EXECUTE;
    TASK_MESSAGE_DISPATCH_POINTERS(slot, ROOM_EVENT_MESSAGE_RESOLVE, loc, loc);

    save                          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    save->state.location.loc.area = (u8)Gp_WarpLoc.areaId;
    save->state.location.loc.warp = loc->warp;
    save->state.location.loc.room = loc->room;
    taskSpawn(0, 0x11, 0, 0);

    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CF8      = 0;
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    D_80114CD4      = 0;
    D_80114CDD      = 0;
}

void Gp_PostDirIfCapIdle(void)
{
    if (gGameSession->eventState == 0) {
        if (capIsBusy() == 0) {
            if (Gp_DirNibble == WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_COMMAND, Gp_DirByte, 0);
            } else {
                capSpawnEventIfIdle(Gp_DirByte, Gp_DirNibble);
            }
        }
    }
    D_80114CF8      = 0;
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    if (D_80114CDC == 0) {
        gGameSession->dirActionBusy = 0;
    }
}

void Gp_RunDirAction(void)
{
    void (*fns[2])(s32, s32) = { func_acropolis_fountain_8017DA78, func_acropolis_helicopter_landing_pad_8017EF60 };

    if (gGameSession->eventState != 0) {
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
    } else {
        fns[(Gp_DirFlags >> 8) & 0x7F](Gp_DirByte, Gp_DirNibble);
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
    }
}

void Gp_ApplyAreaRecs(AreaApplyRec* recs)
{
    GameLocationKey  key;
    AreaRecord*      tbl;
    AreaSavedState*  areaState;
    GameLocationKey* sess;
    s32              i;
    s32              stage;
    s32              mask;
    s8               apply;
    s8               mode;

    apply = 0;
    sess  = &gGameSession->location.loc;
    for (i = 0; recs[i].stage != AREA_APPLY_END; i++) {
        stage     = recs[i].stage;
        tbl       = Gp_AreaTables[stage];
        key.stage = stage;
        key.area  = recs[i].area;
        key.room  = 1;
        key.view  = sess->view;
        mask      = recs[i].policy & AREA_APPLY_MODE_MASK;
        if (mask == AREA_APPLY_MODE_ALWAYS) {
            apply = 1;
        } else {
            mode = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode;
            if (mode == 0 || mode == 2) {
                if (mask == AREA_APPLY_MODE_REPLAY_SCAVENGER) {
                    apply = 1;
                }
            } else if (mode == 1 || mode == 3) {
                if (mask == AREA_APPLY_MODE_BOUNTY_NIGHTMARE) {
                    apply = 1;
                }
            } else {
                apply = 0;
            }
        }
        if (apply) {
            areaSetPlacementVariant(&key, recs[i].variant, AREA_VARIANT_RESET_ALWAYS);
            if (tbl != NULL) {
                areaState = tbl[recs[i].area].savedState;
                if (areaState != NULL) {
                    if (recs[i].policy & AREA_APPLY_MAP_MARK_MASK) {
                        areaState->spawnFlags |= AREA_SAVED_MAP_MARK;
                    } else {
                        areaState->spawnFlags &= 0xFF ^ AREA_SAVED_MAP_MARK;
                    }
                }
            }
        }
        apply = 0;
    }
}
