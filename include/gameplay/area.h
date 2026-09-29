#ifndef GAMEPLAY_AREA_H
#define GAMEPLAY_AREA_H

#include "common.h"

#include "gameplay/areaplace.h"

#include "main/gameflag_types.h"
#include "main/task_types.h"

/// Terminates both area placement lists and their resource-entry lists.
enum { AREA_TABLE_END_ID = 0xFF };

/// One 0xFF-terminated area resource entry. The CD loader uses field_2/field_4;
/// spawning uses field_5/field_8. TaskDesc.flags also controls the model flags.
typedef struct _GpAreaTmdRec {
    u16       field_0;
    u16       field_2;
    u8        field_4;
    u8        field_5;
    byte      pad_6[2];
    TaskDesc* field_8;
} GpAreaTmdRec;
STATIC_ASSERT_SIZEOF(GpAreaTmdRec, 0xC);

/// One selected layout: its placements and the resource entries they name.
typedef struct GpAreaVariant {
    AreaPlacement* field_0;
    GpAreaTmdRec*  field_4;
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
