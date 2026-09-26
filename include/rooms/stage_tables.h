#ifndef ROOMS_STAGE_TABLES_H
#define ROOMS_STAGE_TABLES_H

/* Room objects the per-stage tables reach. A map UI overlay carries one
 * pointer per room for each kind of room record - its objects, views, sprites,
 * exits and so on - and each points into that room's own package, which is
 * loaded at the room slot while the room is current. Every room loads at the
 * same address, so each symbol is named after the room that holds it.
 */

#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

// dryfield_r04
extern GpRoomObjRec    D_dryfield_r04_8017D5C4[];
extern u8*             D_dryfield_r04_8017D5D4[];
extern GpViewCountRec  D_dryfield_r04_8017D5D8[];
extern GpWarpRec       D_dryfield_r04_8017D5E4[];
extern GpViewRec       D_dryfield_r04_8017E218[];
extern GpSprtRec       D_dryfield_r04_8017E280[];
extern GpRoomParamRec* D_dryfield_r04_8017E2AC[];

#endif /* ROOMS_STAGE_TABLES_H */
