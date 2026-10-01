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
    AREA_RESOURCE_FILE_GROUP_BASE_10 = 0,
    AREA_RESOURCE_FILE_GROUP_BASE_20 = 1,
    AREA_RESOURCE_FILE_GROUP_BASE_30 = 2,
    AREA_RESOURCE_FILE_GROUP_BASE_40 = 3,
    AREA_RESOURCE_FILE_GROUP_BASE_50 = 4,
    AREA_RESOURCE_FILE_GROUP_BASE_60 = 5,
    AREA_RESOURCE_FILE_GROUP_BASE_0  = 6,
    AREA_RESOURCE_FILE_GROUP_BASE_1  = 7,
    AREA_RESOURCE_FILE_GROUP_BASE_2  = 8
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

/// One selected layout: its placements and the resource entries they name.
typedef struct GpAreaVariant {
    AreaPlacement* field_0;
    AreaResource*  field_4;
} GpAreaVariant;
STATIC_ASSERT_SIZEOF(GpAreaVariant, 8);

/// Outer stage/area record, distinct from the selected placement layout.
typedef struct _GpAreaRec {
    GpAreaVariant* field_0;
    GpAreaObj*     field_4;
} GpAreaRec;
STATIC_ASSERT_SIZEOF(GpAreaRec, 8);

/// Resolve a placement index within its loaded room resource.
/// `index` must be within the borrowed `records` array; no bounds check is made.
static __inline__ AreaPlacement* gpAreaPlaceAt(AreaPlacement* records, s32 index)
{
    union {
        AreaPlacement* records;
        u32            word;
    } base;
    union {
        AreaPlacement* record;
        u32            word;
    } result;
    base.records = records;
    // Preserve the byte offset as the left operand of the address sum.
    result.word  = index * sizeof(AreaPlacement);
    result.word += base.word;
    return result.record;
}

#endif // GAMEPLAY_AREA_H
