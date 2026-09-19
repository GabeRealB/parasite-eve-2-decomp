#ifndef GAMEPLAY_AREAPLACE_H
#define GAMEPLAY_AREAPLACE_H

#include "common.h"

/// One placement of an area: a model the area starts, where it stands and how
/// it is dressed. A 0x10-byte record in the 0xFF-terminated table at nested
/// `GpAreaRec.field_0`, which the area spawn walks against the area's
/// `GpAreaTmdRec` table.
///
/// `entryId` is the id of the entry a placement dresses -- that entry holds the
/// task to start -- so a placement is spawned by matching the two, and 0xFF ends
/// the table. `variant` and `mode` are the placement's spawn parameters, which
/// the spawn hands the started task as its first spawn argument; the actor the
/// placement starts reads them back from here and gives them its own meaning.
/// `rowIndex` is likewise the actor's: it selects the row that actor reads out
/// of its own per-placement data tables.
///
/// `x` / `y` / `z` and `yaw` place the spawned model's root coordinate in the
/// world; `tpage` and `clut` are the texture page and CLUT its `TmdObject` is
/// dressed with.
///
/// The loader that streams an area in (`Gp_PollAreaCdLoads`) walks the same list
/// for a purpose of its own, forwarding three of these bytes into the CD command
/// that brings the area's files in: it reads `pad_C` as param1[0] and `tpage` /
/// `clut` as param2[2] / param2[3]. That is the same record read a second way,
/// not a second kind of record, so the fields keep the names the spawn gives
/// them and the loader is what passes three of them along.
///
/// The overlays that hold a placement share this type only, so it sits in a
/// header of its own: including the area machinery's header instead would drag
/// the gameplay prototypes into translation units that redeclare them locally
/// against their own view of a shared object.
typedef struct {
    /* 0x00 */ u8  entryId;
    /* 0x01 */ u8  variant;
    /* 0x02 */ u16 mode;
    /* 0x04 */ s16 x;
    /* 0x06 */ s16 y;
    /* 0x08 */ s16 z;
    /* 0x0A */ s16 yaw;
    /* 0x0C */ u8  pad_C; // Role unproven: the spawn reads nothing here, the CD loader forwards it
    /* 0x0D */ u8  tpage;
    /* 0x0E */ u8  clut;
    /* 0x0F */ u8  rowIndex;
} GpAreaPlace;
STATIC_ASSERT_SIZEOF(GpAreaPlace, 0x10);

#endif
