#include "item_placement.h"

#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "area_flags.h"
#include "gameplay/enemy.h"
#include "gameplay/item_placement.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "scene_runtime.h"

#include "gameplay/damage.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task_types.h"

/// Catalog and UI kinds used by Dryfield's shared placed-object lists.
///
/// Banks 0 and 1 are catalog ids, bank 7 selects item-box UI, and bank 8
/// selects save-point UI. Key-item kinds reuse the collection-id constants.
enum {
    AREA_DRYFIELD_KIND_RECOVERY_1               = 0x1,
    AREA_DRYFIELD_KIND_RECOVERY_3               = 0x3,
    AREA_DRYFIELD_KIND_COLA                     = 0x5,
    AREA_DRYFIELD_KIND_MP_BOOST_2               = 0x7,
    AREA_DRYFIELD_KIND_PENICILLIN               = 0x8,
    AREA_DRYFIELD_KIND_LIPSTICK                 = 0xB,
    AREA_DRYFIELD_KIND_SNAIL_MAGAZINE           = 0xC,
    AREA_DRYFIELD_KIND_BELT_POUCH               = 0xD,
    AREA_DRYFIELD_KIND_HOLY_WATER               = 0x38,
    AREA_DRYFIELD_KIND_OFUDA                    = 0x39,
    AREA_DRYFIELD_KIND_FLARE                    = 0x3A,
    AREA_DRYFIELD_KIND_PROTEIN_CAPSULE          = 0x3C,
    AREA_DRYFIELD_KIND_TACTICAL_VEST            = 0x65,
    AREA_DRYFIELD_KIND_CHICKEN_PLATE            = 0x68,
    AREA_DRYFIELD_KIND_M950                     = 0x82,
    AREA_DRYFIELD_KIND_9MM_PARABELLUM           = 0xA0,
    AREA_DRYFIELD_KIND_BUCKSHOT                 = 0xAC,
    AREA_DRYFIELD_KIND_FIREFLY                  = 0xAD,
    AREA_DRYFIELD_KIND_RIFLED_SLUG              = 0xAE,
    AREA_DRYFIELD_KIND_SALOON_KEY               = 0x110,
    AREA_DRYFIELD_KIND_GAS_STATION_ITEM_BOX     = 0x703,
    AREA_DRYFIELD_KIND_MOTEL_ROOM_6_ITEM_BOX    = 0x704,
    AREA_DRYFIELD_KIND_GARAGE_ITEM_BOX          = 0x705,
    AREA_DRYFIELD_KIND_GAS_STATION_SAVE_POINT   = 0x804,
    AREA_DRYFIELD_KIND_TRAILER_COACH_SAVE_POINT = 0x805,
    AREA_DRYFIELD_KIND_MOTEL_LOBBY_SAVE_POINT   = 0x806,
    AREA_DRYFIELD_KIND_MOTEL_ROOM_6_SAVE_POINT  = 0x810
};

/// Initial saved states: available, collected, or replenishable full-stack pickup.
///
/// The container lid also uses the collected state to suppress opening.
/// State bit 8 has no current reader; its stored 0x100 is retained below and
/// its purpose remains unproven. AREA_OBJECT_PLACE_PROMPT controls whether the
/// pickup panel collects immediately instead of asking for confirmation.
enum {
    AREA_DRYFIELD_STATE_AVAILABLE     = 1,
    AREA_DRYFIELD_STATE_COLLECTED     = 2,
    AREA_DRYFIELD_STATE_REPLENISHABLE = 3
};

static inline s32 _gpGetCurBit2Flag(s32 arg0);

static inline void _gpSpawnPlace(AreaObjectSpawn* spawn, AreaObjectPlace* place);

static void _areaSpawnRoomObjectByFlagIndex(u16 flagIndex);

static inline s32 _gpGetCurBit2Flag(s32 arg0)
{
    u32* p;
    u32  word;
    s32  shift;

    p      = &gAreaObjectStages[gGameSession->location.stage].objectStates[arg0 >> 4];
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

AreaObjectPlace gAreaDryfieldGasStationObjectPlaces[7] = {
    { 0x2E, AREA_DRYFIELD_KIND_GAS_STATION_SAVE_POINT, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x1, AREA_DRYFIELD_KIND_GAS_STATION_ITEM_BOX, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x2, AREA_DRYFIELD_KIND_9MM_PARABELLUM, 0x0, AREA_DRYFIELD_STATE_REPLENISHABLE, 0, 0, 0, 0x0 },
    { 0x26, AREA_DRYFIELD_KIND_TACTICAL_VEST, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x27, AREA_DRYFIELD_KIND_LIPSTICK, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0xB, INVENTORY_COLLECTION_ID_GASOLINE, 0x0, AREA_DRYFIELD_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldMainStreetObjectPlaces[3] = {
    { 0xC, INVENTORY_COLLECTION_ID_TRUCK_KEY, 0x0, AREA_DRYFIELD_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT, 0, 0, 0, 0x0 },
    { 0x1B, INVENTORY_COLLECTION_ID_ICE_BAG, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldGeneralStoreObjectPlaces[3] = {
    { 0xF, AREA_DRYFIELD_KIND_COLA, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x10, AREA_DRYFIELD_KIND_RECOVERY_3, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldBackStreetObjectPlaces[2] = {
    { 0x28, AREA_DRYFIELD_KIND_FIREFLY, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldSouvenirShopObjectPlaces[2] = {
    { 0x29, AREA_DRYFIELD_KIND_BELT_POUCH, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldWarehouseObjectPlaces[1] = {
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldMotelRoom2ObjectPlaces[3] = {
    { 0x12, AREA_DRYFIELD_KIND_PENICILLIN, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x14, AREA_DRYFIELD_KIND_RECOVERY_1, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldMotelRoom3ObjectPlaces[2] = {
    { 0x2A, AREA_DRYFIELD_KIND_MP_BOOST_2, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldParkingLotObjectPlaces[2] = {
    { 0xD, AREA_DRYFIELD_KIND_BUCKSHOT, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldMotelLobbyObjectPlaces[3] = {
    { 0x9, INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x2D, AREA_DRYFIELD_KIND_MOTEL_LOBBY_SAVE_POINT, 0x0, AREA_DRYFIELD_STATE_AVAILABLE | 0x100, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldSaloonGRObjectPlaces[2] = {
    { 0x15, AREA_DRYFIELD_KIND_COLA, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldGRKitchenObjectPlaces[3] = {
    { 0x16, INVENTORY_COLLECTION_ID_BOTTLECAP_MAGNET, 0x0, AREA_DRYFIELD_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT, 0, 0, 0, 0x0 },
    { 0x17, AREA_DRYFIELD_KIND_RECOVERY_1, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldWaterTankObjectPlaces[7] = {
    { 0x19, AREA_DRYFIELD_KIND_SALOON_KEY, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x1A, AREA_DRYFIELD_KIND_RECOVERY_1, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x1E, INVENTORY_COLLECTION_ID_SUV_KEY, 0x0, AREA_DRYFIELD_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT, 0, 0, 0, 0x0 },
    { 0x1F, AREA_DRYFIELD_KIND_COLA, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x20, AREA_DRYFIELD_KIND_MP_BOOST_2, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x21, AREA_DRYFIELD_KIND_OFUDA, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldBreezewayObjectPlaces[2] = {
    { 0x6, INVENTORY_COLLECTION_ID_FACTORY_KEY, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldFactoryObjectPlaces[2] = {
    { 0x13, AREA_DRYFIELD_KIND_RIFLED_SLUG, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldGarageObjectPlaces[4] = {
    { 0x4, INVENTORY_COLLECTION_ID_MOTEL_ROOM_6_KEY, 0x0, AREA_DRYFIELD_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT, 0, 0, 0, 0x0 },
    { 0x23, AREA_DRYFIELD_KIND_GARAGE_ITEM_BOX, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x3, INVENTORY_COLLECTION_ID_MONKEY_WRENCH, 0x0, AREA_DRYFIELD_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldDrivewayObjectPlaces[2] = {
    { 0x1D, AREA_DRYFIELD_KIND_RECOVERY_1, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldJunkYardObjectPlaces[3] = {
    { 0x7, INVENTORY_COLLECTION_ID_WIRE_ROPE, 0x0, AREA_DRYFIELD_STATE_AVAILABLE | 0x100, 1745, 0, 280, 0x0 },
    { 0x1C, AREA_DRYFIELD_KIND_CHICKEN_PLATE, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldTrailerCoachObjectPlaces[4] = {
    { 0x8, INVENTORY_COLLECTION_ID_LOBBY_KEY, 0x0, AREA_DRYFIELD_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT, 0, 0, 0, 0x0 },
    { 0x2F, AREA_DRYFIELD_KIND_TRAILER_COACH_SAVE_POINT, 0x0, AREA_DRYFIELD_STATE_AVAILABLE | 0x100, 0, 0, 0, 0x0 },
    { 0x22, AREA_DRYFIELD_KIND_M950, 0x0, AREA_DRYFIELD_STATE_COLLECTED, 4128, 0, -256, ACTOR_TRANSFORM_ANGLE_HALF_TURN },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldMotelRoom5ObjectPlaces[2] = {
    { 0x2B, AREA_DRYFIELD_KIND_BELT_POUCH, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldMotelRoom6ObjectPlaces[3] = {
    { 0xE, AREA_DRYFIELD_KIND_MOTEL_ROOM_6_ITEM_BOX, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x30, AREA_DRYFIELD_KIND_MOTEL_ROOM_6_SAVE_POINT, 0x0, AREA_DRYFIELD_STATE_AVAILABLE | 0x100, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldMotelLoftObjectPlaces[3] = {
    { 0xA, INVENTORY_COLLECTION_ID_JERRY_CAN, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, -5950, -500, -20, 0x0 },
    { 0x2C, AREA_DRYFIELD_KIND_HOLY_WATER, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldWaterHoleObjectPlaces[2] = {
    { 0x25, AREA_DRYFIELD_KIND_PROTEIN_CAPSULE, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

AreaObjectPlace gAreaDryfieldCellarObjectPlaces[3] = {
    { 0x11, AREA_DRYFIELD_KIND_FLARE, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { 0x5, AREA_DRYFIELD_KIND_SNAIL_MAGAZINE, 0x0, AREA_DRYFIELD_STATE_AVAILABLE, 0, 0, 0, 0x0 },
    { AREA_OBJECT_PLACE_END, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
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

    savedLocation = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location;
    stageRooms    = gAreaObjectStages[savedLocation->stage].rooms;
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

    areaObjects = gAreaObjectStages[location->stage].rooms;
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
