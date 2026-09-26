#ifndef MAPUI_STAGE_TABLES_H
#define MAPUI_STAGE_TABLES_H

/* The per-stage tables a map UI overlay carries, which gameplay's own tables
 * index by stage: the map screen's rooms, markers, icons and names, and the
 * stage's room records, enemy and task descriptors, placements and item
 * grants. They live in the overlay because only the current stage's map is
 * loaded. Every map overlay loads at the same address, so each table is named
 * after the map that holds it.
 */

#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/areaplace.h"
#include "gameplay/gameplay.h"

// map_dryfield

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_dryfield_80179BCC[];
/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern GpMapRec D_map_dryfield_80179BD0[];
/// This stage's `Gp_MapMarkTables` entry: the room markers, each a model in the
/// map picture its map room id selects.
extern GpMapMark D_map_dryfield_80179E00[];
/// This stage's `D_8010F0E0` entry, ended by a room id of 0.
extern GpMapFlagIcon D_map_dryfield_80179F38[];
/// This stage's `D_8010F0CC` entry, ended by a room id of 0.
extern GpMapIcon D_map_dryfield_80179FEC[];
/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern GpMapName D_map_dryfield_8017A044[];
/// This stage's `Gp_Bit2Banks` list table, by room, ended by a -1 list.
extern GpBit2List D_map_dryfield_8017A564[];
/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern GpTaskDesc D_map_dryfield_8017A6A4[];
/// This stage's flag table for `Gp_LookupStageFlag`.
extern u16 D_map_dryfield_8017A824[];
/// This stage's entries in `Gp_RoomCoordTables`, `Gp_WarpTables`,
/// `Gp_ViewCountTables`, `Gp_RoomObjTables`, `Gp_ViewTables`,
/// `Gp_ViewIndexTables`, `Gp_SprtTables` and `Gp_RoomParamTables`: each leads to
/// one pointer per room into that room's package.
extern GpRoomCoordRec*  D_map_dryfield_8017A860[];
extern GpWarpRec*       D_map_dryfield_8017A8F8[];
extern GpViewCountTbl   D_map_dryfield_8017AA28;
extern GpRoomObjTbl     D_map_dryfield_8017AAC4;
extern GpViewTbl        D_map_dryfield_8017AB60;
extern GpViewIndexTbl   D_map_dryfield_8017ABFC;
extern GpSprtTbl        D_map_dryfield_8017AC98;
extern GpRoomParamRec** D_map_dryfield_8017AC9C[];
/// Area placement lists, each ended by an entry id of 0xFF. The rooms' area
/// records point at them, one list per location.
extern GpAreaPlace D_map_dryfield_8017AD34[];
extern GpAreaPlace D_map_dryfield_8017AD44[];
extern GpAreaPlace D_map_dryfield_8017AD74[];
extern GpAreaPlace D_map_dryfield_8017ADE4[];
extern GpAreaPlace D_map_dryfield_8017AE14[];
extern GpAreaPlace D_map_dryfield_8017AE44[];
extern GpAreaPlace D_map_dryfield_8017AEA4[];
extern GpAreaPlace D_map_dryfield_8017AED4[];
extern GpAreaPlace D_map_dryfield_8017AF04[];
extern GpAreaPlace D_map_dryfield_8017AF44[];
extern GpAreaPlace D_map_dryfield_8017AF64[];
extern GpAreaPlace D_map_dryfield_8017AF94[];
extern GpAreaPlace D_map_dryfield_8017AFA4[];
extern GpAreaPlace D_map_dryfield_8017B004[];
extern GpAreaPlace D_map_dryfield_8017B054[];
extern GpAreaPlace D_map_dryfield_8017B0B4[];
extern GpAreaPlace D_map_dryfield_8017B0E4[];
extern GpAreaPlace D_map_dryfield_8017B154[];
extern GpAreaPlace D_map_dryfield_8017B1C4[];
extern GpAreaPlace D_map_dryfield_8017B1D4[];
extern GpAreaPlace D_map_dryfield_8017B204[];
extern GpAreaPlace D_map_dryfield_8017B284[];
extern GpAreaPlace D_map_dryfield_8017B2A4[];
extern GpAreaPlace D_map_dryfield_8017B2D4[];
extern GpAreaPlace D_map_dryfield_8017B354[];
extern GpAreaPlace D_map_dryfield_8017B394[];
extern GpAreaPlace D_map_dryfield_8017B3F4[];
extern GpAreaPlace D_map_dryfield_8017B474[];
extern GpAreaPlace D_map_dryfield_8017B4B4[];
extern GpAreaPlace D_map_dryfield_8017B534[];
extern GpAreaPlace D_map_dryfield_8017B564[];
extern GpAreaPlace D_map_dryfield_8017B5E4[];
extern GpAreaPlace D_map_dryfield_8017B644[];
extern GpAreaPlace D_map_dryfield_8017B664[];
extern GpAreaPlace D_map_dryfield_8017B674[];
extern GpAreaPlace D_map_dryfield_8017B6A4[];
extern GpAreaPlace D_map_dryfield_8017B724[];
extern GpAreaPlace D_map_dryfield_8017B744[];
extern GpAreaPlace D_map_dryfield_8017B7A4[];
extern GpAreaPlace D_map_dryfield_8017B7E4[];
extern GpAreaPlace D_map_dryfield_8017B984[];
extern GpAreaPlace D_map_dryfield_8017B994[];
extern GpAreaPlace D_map_dryfield_8017B9B4[];
extern GpAreaPlace D_map_dryfield_8017B9C4[];
extern GpAreaPlace D_map_dryfield_8017BA14[];
extern GpAreaPlace D_map_dryfield_8017BA34[];
extern GpAreaPlace D_map_dryfield_8017BA44[];
extern GpAreaPlace D_map_dryfield_8017BA84[];
extern GpAreaPlace D_map_dryfield_8017BAF4[];
extern GpAreaPlace D_map_dryfield_8017BB24[];
extern GpAreaPlace D_map_dryfield_8017BB54[];
extern GpAreaPlace D_map_dryfield_8017BB74[];
extern GpAreaPlace D_map_dryfield_8017BB84[];
extern GpAreaPlace D_map_dryfield_8017BC14[];
extern GpAreaPlace D_map_dryfield_8017BCC4[];
/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items granted by
/// location, each ended by a key of -1.
extern GpGiveRec D_map_dryfield_8017BCE4[];
extern GpGiveRec D_map_dryfield_8017BD80[];

#endif /* MAPUI_STAGE_TABLES_H */
