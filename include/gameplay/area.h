#ifndef GAMEPLAY_AREA_H
#define GAMEPLAY_AREA_H

#include "common.h"

#include "gameplay/areaplace.h"

#include "main/gameflag_types.h"
#include "main/task_types.h"

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
    GpAreaPlace*  field_0;
    GpAreaTmdRec* field_4;
} GpAreaVariant;
STATIC_ASSERT_SIZEOF(GpAreaVariant, 8);

/// Outer stage/area record, distinct from the selected placement layout.
typedef struct _GpAreaRec {
    GpAreaVariant* field_0;
    GpAreaObj*     field_4;
} GpAreaRec;
STATIC_ASSERT_SIZEOF(GpAreaRec, 8);

/// Resolve a placement index within its loaded room resource.
/// The address word uses the PS1 representation; the returned record is typed.
static __inline__ GpAreaPlace* gpAreaPlaceAt(GpAreaPlace* records, s32 index)
{
    union {
        GpAreaPlace* records;
        u32          word;
    } base;
    union {
        GpAreaPlace* record;
        u32          word;
    } result;
    base.records = records;
    result.word  = index * sizeof(GpAreaPlace);
    result.word += base.word;
    return result.record;
}

#endif // GAMEPLAY_AREA_H
