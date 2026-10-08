#include "item_placement.h"

#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "area_flags.h"
#include "gameplay/enemy.h"
#include "gameplay/item_placement.h"
#include "gameplay/scene_runtime.h"
#include "scene_runtime.h"

#include "gameplay/damage.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task_types.h"

static inline s32 _gpGetCurBit2Flag(s32 arg0);

static inline void _gpSpawnPlace(AreaObjectSpawn* spawn, AreaObjectPlace* place);

static void _areaSpawnRoomObjectByFlagIndex(u16 flagIndex);

static inline s32 _gpGetCurBit2Flag(s32 arg0)
{
    u32* p;
    u32  word;
    s32  shift;

    p      = &Gp_Bit2Banks[gGameSession->location.loc.stage].objectStates[arg0 >> 4];
    shift  = (arg0 & 0xF) * 2;
    word   = *p;
    word  &= 3 << shift;
    word >>= shift;
    return word;
}
static inline void _gpSpawnPlace(AreaObjectSpawn* spawn, AreaObjectPlace* place)
{
    Enemy*     enemy;
    Task*      task;
    TmdObject* extra;
    GfxCoord*  coord;
    u16        id;

    id = spawn->kind;
    while (id != AREA_OBJECT_SPAWN_END) {
        if (id == place->kind) {
            enemy = enemySpawnFromTable(&spawn->taskDesc, 0, spawn->kind, NULL);
            if (enemy != NULL) {
                task = enemy->task;
                if (task->bodyKind != TASK_BODY_NONE) {
                    extra               = task->extra.tmd;
                    coord               = extra->coords;
                    enemy->placeKey     = place->flagIndex | (place->placeKeyHigh << ENEMY_PLACE_STAGE_SHIFT);
                    enemy->workType     = place->kind;
                    coord->coord.t[0]   = place->x;
                    coord->coord.t[1]   = place->y;
                    coord->coord.t[2]   = place->z;
                    coord->param.rot.vy = place->yaw;
                    if (coord->param.rot.vy != 0) {
                        gfxRotMatrixY(&coord->coord, (s16)place->yaw, 1);
                    }
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                }
            }
            return;
        }
        spawn++;
        id = spawn->kind;
    }
}

AreaObjectPlace D_80114588[] = {
    { 0x2E, 0x804, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x1, 0x703, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x2, 0xA0, 0x0, 0x3, 0, 0, 0, 0x0 },
    { 0x26, 0x65, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x27, 0xB, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xB, 0x118, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_801145F8[] = {
    { 0xC, 0x116, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0x1B, 0x119, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114628[] = {
    { 0xF, 0x5, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x10, 0x3, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114658[] = {
    { 0x28, 0xAD, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114678[] = {
    { 0x29, 0xD, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114698[] = {
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_801146A8[] = {
    { 0x12, 0x8, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x14, 0x1, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_801146D8[] = {
    { 0x2A, 0x7, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_801146F8[] = {
    { 0xD, 0xAC, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114718[] = {
    { 0x9, 0x113, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x2D, 0x806, 0x0, 0x101, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114748[] = {
    { 0x15, 0x5, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114768[] = {
    { 0x16, 0x11B, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0x17, 0x1, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114798[] = {
    { 0x19, 0x110, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x1A, 0x1, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x1E, 0x11E, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0x1F, 0x5, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x20, 0x7, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x21, 0x39, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114808[] = {
    { 0x6, 0x115, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114828[] = {
    { 0x13, 0xAE, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114848[] = {
    { 0x4, 0x10F, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0x23, 0x705, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x3, 0x111, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114888[] = {
    { 0x1D, 0x1, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_801148A8[] = {
    { 0x7, 0x114, 0x0, 0x101, 1745, 0, 280, 0x0 },
    { 0x1C, 0x68, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_801148D8[] = {
    { 0x8, 0x112, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0x2F, 0x805, 0x0, 0x101, 0, 0, 0, 0x0 },
    { 0x22, 0x82, 0x0, 0x2, 4128, 0, -256, 0x800 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114918[] = {
    { 0x2B, 0xD, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114938[] = {
    { 0xE, 0x704, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x30, 0x810, 0x0, 0x101, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114968[] = {
    { 0xA, 0x117, 0x0, 0x1, -5950, -500, -20, 0x0 },
    { 0x2C, 0x38, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_80114998[] = {
    { 0x25, 0x3C, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace D_801149B8[] = {
    { 0x11, 0x3A, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x5, 0xC, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

/// Spawns the first saved-area placement with this flag index when its live-stage state is nonzero.
///
/// `flagIndex` is an object-state index in 0..63. Selects the room table from
/// the live save's stage/area, but reads the two-bit state in the session's
/// current stage. Missing room/place tables or an absent index do nothing.
/// Both locations must be valid for their loaded tables; a matched placement
/// requires a live, END-terminated spawn table. A first match consumes the
/// search even if its state is zero, its kind is absent or allocation fails.
/// Spawned tasks own their enemy/model storage; referenced overlay resources
/// must stay loaded. Placement uses the record's parent-frame XYZ and yaw.
static void _areaSpawnRoomObjectByFlagIndex(u16 flagIndex)
{
    const GameLocationKey* savedLocation;
    const AreaObjectRoom*  stageRooms;
    AreaObjectPlace*       place;
    u16                    placeFlagIndex;

    savedLocation = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
    stageRooms    = Gp_Bit2Banks[savedLocation->stage].rooms;
    if (stageRooms == NULL) {
        return;
    }
    place = stageRooms[savedLocation->area].places.list;
    if (place == NULL) {
        return;
    }
    placeFlagIndex = place->flagIndex;
    while (placeFlagIndex != AREA_OBJECT_PLACE_END) {
        if (placeFlagIndex == flagIndex) {
            if (_gpGetCurBit2Flag(placeFlagIndex) != 0) {
                _gpSpawnPlace(stageRooms[savedLocation->area].spawns, place);
            }
            return;
        }
        place++;
        placeFlagIndex = place->flagIndex;
    }
}

/// Places a spawned model-backed room object and publishes its key and kind.
///
/// `task` is the enemy's live TMD task with a writable model root. Borrows
/// `place` only for this call; the packed key narrows to u16 after ORing
/// flagIndex with placeKeyHigh shifted by eight. XYZ use game units in the
/// root's parent frame and yaw uses 4096 units per turn, narrowed to s16.
/// A nonzero yaw replaces the root rotation; zero records zero and retains
/// the matrix's existing rotation. Marks composition dirty in either case.
static inline void _areaApplyRoomObjectPlacement(const Task* task, Enemy* enemy, const AreaObjectPlace* place)
{
    TmdObject* model;
    GfxCoord*  root;

    model              = task->extra.tmd;
    root               = model->coords;
    enemy->placeKey    = place->flagIndex | (place->placeKeyHigh << ENEMY_PLACE_STAGE_SHIFT);
    enemy->workType    = place->kind;
    root->coord.t[0]   = place->x;
    root->coord.t[1]   = place->y;
    root->coord.t[2]   = place->z;
    root->param.rot.vy = place->yaw;
    if (root->param.rot.vy != 0) {
        gfxRotMatrixY(&root->coord, (s16)place->yaw, GRAPHICS_ROTATION_REPLACE);
    }
    root->composeStamp = GRAPHICS_COORD_DIRTY;
}

void areaSpawnRoomObjects(const GameLocationKey* location)
{
    AreaObjectRoom*  areaObjects;
    AreaObjectPlace* place;
    AreaObjectSpawn* spawnEntry;
    Enemy*           enemy;
    Task*            task;
    u16              tableEnd;
    u16              spawnKind;

    areaObjects = Gp_Bit2Banks[location->stage].rooms;
    if (areaObjects == NULL) {
        return;
    }
    place = areaObjects[location->area].places.list;
    if (place == NULL) {
        return;
    }
    // Both lists use 0xFFFF; retain one halfword for the nested comparisons.
    tableEnd = AREA_OBJECT_PLACE_END;
    if (place->flagIndex == tableEnd) {
        return;
    }
    do {
        spawnEntry = areaObjects[location->area].spawns;
        spawnKind  = spawnEntry->kind;
        if (spawnKind != tableEnd) {
            do {
                if (spawnKind == place->kind) {
                    enemy = enemySpawnFromTable(&spawnEntry->taskDesc, 0, spawnEntry->kind, NULL);
                    if (enemy != NULL) {
                        task = enemy->task;
                        if (task->bodyKind != TASK_BODY_NONE) {
                            _areaApplyRoomObjectPlacement(task, enemy, place);
                        }
                    }
                    break;
                }
                spawnEntry++;
                spawnKind = spawnEntry->kind;
            } while (spawnKind != tableEnd);
        }
        place++;
    } while (place->flagIndex != tableEnd);
}
