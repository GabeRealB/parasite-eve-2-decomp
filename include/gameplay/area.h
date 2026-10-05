#ifndef GAMEPLAY_AREA_H
#define GAMEPLAY_AREA_H

#include "common.h"

#include "gameplay/areaplace.h"

#include "main/gameflag_types.h"
#include "main/task_types.h"

/// Selectors for an area's global-library file-group base.
///
/// The loader adds the decimal file number's hundreds to this base. The base-60
/// selector queues base files only for entries with a placement whose fileIdLow=0.
enum {
    /// Selects file-group base 10 for an area's global-library loads.
    ///
    /// Store this index in `AreaResource.fileGroupIndex`; its value zero
    /// selects base 10. For file numbers 0..999, the stage-zero
    /// ID is `100000 + fileNumber * 100 + low`, in the regular actor-slot-1
    /// family. `low` is zero for a base load or `AreaPlacement.fileIdLow` for
    /// an additional file. The selector does not require a task descriptor.
    LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR = 0,
    AREA_RESOURCE_FILE_GROUP_BASE_20         = 1,
    AREA_RESOURCE_FILE_GROUP_BASE_30         = 2,
    AREA_RESOURCE_FILE_GROUP_BASE_40         = 3,
    AREA_RESOURCE_FILE_GROUP_BASE_50         = 4,
    AREA_RESOURCE_FILE_GROUP_BASE_60         = 5,
    AREA_RESOURCE_FILE_GROUP_BASE_0          = 6,
    AREA_RESOURCE_FILE_GROUP_BASE_1          = 7,
    AREA_RESOURCE_FILE_GROUP_BASE_2          = 8
};

/// An area resource's file selection and task recipes, keyed by placement-entry ID.
///
/// Tables end with `AREA_PLACEMENT_END` in `entryId`; the remaining terminator
/// fields are zero. The loader uses `fileGroupIndex` and `fileNumber` to select a
/// global-library file: group = base + fileNumber / 100, hundreds = fileNumber
/// % 100, and the low component is zero for a base file or the placement's
/// `fileIdLow` for an additional file. Live file numbers must name catalogued files.
///
/// The table and its task descriptors borrow loaded-overlay storage. Keep them
/// valid during loading, spawning and model-flag updates. Spawnable entries
/// require a non-NULL `taskTable` and a `taskIndex` within its live descriptors;
/// loading-only entries may have no task table. Model-flag updates use the first
/// descriptor's flags independently of the spawn index.
typedef struct {
    u16       entryId;        // Matches AreaPlacement.entryId; AREA_PLACEMENT_END ends the table
    s16       fileNumber;     // Signed decimal file number, split into group offset and hundreds component
    u8        fileGroupIndex; // Base selector (0..5 groups 10..60, 6..8 groups 0..2)
    u8        taskIndex;      // Descriptor index used to spawn this entry's actor; no bounds check
    byte      unknown_6[2];   // Zero in all tables; no access established and role unproven
    TaskDesc* taskTable;      // Borrowed descriptor table, or NULL for a loading-only entry
} AreaResource;
STATIC_ASSERT_SIZEOF(AreaResource, 0xC);

/// One area layout: its placement table and the resource table those placements name.
///
/// A room publishes a table of these, indexed by `GameLocationKey.variant`.
/// Index 0 is an empty slot and variant 1 is the default. A published slot
/// stores both tables or neither. The tables are borrowed from the loaded
/// room; a NULL pointer means that variant is absent.
typedef struct {
    AreaPlacement* placements; // Placement table for this variant, or NULL
    AreaResource*  resources;  // Resource table those placements name, or NULL
} AreaVariant;
STATIC_ASSERT_SIZEOF(AreaVariant, 8);

/// One area in a stage table: its layout variants and its saved placement state.
///
/// `Gp_AreaTables[stage]` is an array of these, indexed by area id. Index 0 is
/// empty, and an area the stage does not publish stores both pointers as NULL.
/// `variants` is borrowed from the loaded room and indexed by placement variant.
/// `savedState` addresses that area's saved placement variant and spawn flags
/// in the stage save bank, so placement and map updates write the save directly.
typedef struct {
    AreaVariant*    variants;   // Layouts for this area, indexed by placement variant, or NULL
    AreaSavedState* savedState; // Saved placement variant and spawn flags in the stage bank, or NULL
} AreaRecord;
STATIC_ASSERT_SIZEOF(AreaRecord, 8);

/// The placement an inline accessor was handed. It exists for what the compiler
/// does with its argument: an inlined function's actual argument is expanded as
/// an address, scaled index first, which is the order the callers' element
/// addresses have and a plain `&records[index]` does not produce.
static __inline__ AreaPlacement* gpAreaPlaceRef(AreaPlacement* record)
{
    return record;
}

/// Resolve a placement index within its loaded room resource.
/// `index` must be within the borrowed `records` array; no bounds check is made.
#define gpAreaPlaceAt(records, index) gpAreaPlaceRef(&(records)[index])

#endif // GAMEPLAY_AREA_H
