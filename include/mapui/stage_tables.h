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

// map_akropolis

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_akropolis_8017A14C[];
/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern GpMapRec D_map_akropolis_8017A154[];
/// This stage's `Gp_MapMarkTables` entry: the room markers, each a model in the
/// map picture its map room id selects.
extern GpMapMark D_map_akropolis_8017A288[];
/// This stage's `D_8010F0E0` entry, ended by a room id of 0.
extern GpMapFlagIcon D_map_akropolis_8017A330[];
/// This stage's `D_8010F0CC` entry, ended by a room id of 0.
extern GpMapIcon D_map_akropolis_8017A38C[];
/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern GpMapName D_map_akropolis_8017A3BC[];
/// This stage's `Gp_Bit2Banks` list table, by room, ended by a -1 list.
extern GpBit2List D_map_akropolis_8017A7FC[];
/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern GpTaskDesc D_map_akropolis_8017A8AC[];
/// This stage's flag table for `Gp_LookupStageFlag`.
extern u16 D_map_akropolis_8017AA0C[];
/// This stage's entries in `Gp_RoomCoordTables`, `Gp_RoomObjTables`,
/// `Gp_SprtTables`, `Gp_WarpTables`, `Gp_ViewCountTables`, `Gp_ViewTables`,
/// `Gp_ViewIndexTables` and `Gp_RoomParamTables`: each leads to one pointer per
/// room into that room's package.
extern GpRoomCoordRec*  D_map_akropolis_8017AA28[];
extern GpRoomObjTbl     D_map_akropolis_8017AAC8;
extern GpSprtTbl        D_map_akropolis_8017AB1C;
extern GpWarpRec*       D_map_akropolis_8017AB20[];
extern GpViewCountTbl   D_map_akropolis_8017ABC0;
extern GpViewTbl        D_map_akropolis_8017AC14;
extern GpViewIndexTbl   D_map_akropolis_8017AC68;
extern GpRoomParamRec** D_map_akropolis_8017AC6C[];
/// Area placement lists, each ended by an entry id of 0xFF. The rooms' area
/// records point at them, one list per location.
extern GpAreaPlace D_map_akropolis_8017ACBC[];
extern GpAreaPlace D_map_akropolis_8017ACDC[];
extern GpAreaPlace D_map_akropolis_8017ACFC[];
extern GpAreaPlace D_map_akropolis_8017AD2C[];
extern GpAreaPlace D_map_akropolis_8017AD7C[];
extern GpAreaPlace D_map_akropolis_8017ADEC[];
extern GpAreaPlace D_map_akropolis_8017AE3C[];
extern GpAreaPlace D_map_akropolis_8017AE6C[];
extern GpAreaPlace D_map_akropolis_8017AE9C[];
extern GpAreaPlace D_map_akropolis_8017AEDC[];
extern GpAreaPlace D_map_akropolis_8017AF1C[];
extern GpAreaPlace D_map_akropolis_8017AF3C[];
extern GpAreaPlace D_map_akropolis_8017AF6C[];
extern GpAreaPlace D_map_akropolis_8017AFCC[];
extern GpAreaPlace D_map_akropolis_8017AFEC[];
extern GpAreaPlace D_map_akropolis_8017B01C[];
extern GpAreaPlace D_map_akropolis_8017B04C[];
extern GpAreaPlace D_map_akropolis_8017B0DC[];
extern GpAreaPlace D_map_akropolis_8017B0FC[];
extern GpAreaPlace D_map_akropolis_8017B11C[];
extern GpAreaPlace D_map_akropolis_8017B17C[];
extern GpAreaPlace D_map_akropolis_8017B1AC[];
extern GpAreaPlace D_map_akropolis_8017B1DC[];
extern GpAreaPlace D_map_akropolis_8017B1FC[];
extern GpAreaPlace D_map_akropolis_8017B24C[];
extern GpAreaPlace D_map_akropolis_8017B29C[];
extern GpAreaPlace D_map_akropolis_8017B2EC[];
extern GpAreaPlace D_map_akropolis_8017B33C[];
extern GpAreaPlace D_map_akropolis_8017B35C[];
extern GpAreaPlace D_map_akropolis_8017B41C[];
extern GpAreaPlace D_map_akropolis_8017B48C[];
extern GpAreaPlace D_map_akropolis_8017B4FC[];
extern GpAreaPlace D_map_akropolis_8017B54C[];
extern GpAreaPlace D_map_akropolis_8017B5BC[];
extern GpAreaPlace D_map_akropolis_8017B5EC[];
extern GpAreaPlace D_map_akropolis_8017B65C[];
extern GpAreaPlace D_map_akropolis_8017B67C[];
extern GpAreaPlace D_map_akropolis_8017B6AC[];
extern GpAreaPlace D_map_akropolis_8017B6DC[];
extern GpAreaPlace D_map_akropolis_8017B70C[];
extern GpAreaPlace D_map_akropolis_8017B73C[];
extern GpAreaPlace D_map_akropolis_8017B7DC[];
extern GpAreaPlace D_map_akropolis_8017B80C[];
extern GpAreaPlace D_map_akropolis_8017B82C[];
extern GpAreaPlace D_map_akropolis_8017B85C[];
extern GpAreaPlace D_map_akropolis_8017B8DC[];
extern GpAreaPlace D_map_akropolis_8017B98C[];
extern GpAreaPlace D_map_akropolis_8017B9AC[];
extern GpAreaPlace D_map_akropolis_8017BA5C[];
extern GpAreaPlace D_map_akropolis_8017BA8C[];
extern GpAreaPlace D_map_akropolis_8017BABC[];
extern GpAreaPlace D_map_akropolis_8017BB1C[];
extern GpAreaPlace D_map_akropolis_8017BB9C[];
extern GpAreaPlace D_map_akropolis_8017BBFC[];
extern GpAreaPlace D_map_akropolis_8017BC7C[];
extern GpAreaPlace D_map_akropolis_8017BCEC[];
extern GpAreaPlace D_map_akropolis_8017BCFC[];
extern GpAreaPlace D_map_akropolis_8017BD0C[];
extern GpAreaPlace D_map_akropolis_8017BD2C[];
extern GpAreaPlace D_map_akropolis_8017BD4C[];
extern GpAreaPlace D_map_akropolis_8017BD8C[];
extern GpAreaPlace D_map_akropolis_8017BDBC[];
extern GpAreaPlace D_map_akropolis_8017BDEC[];
/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items granted by
/// location, each ended by a key of -1.
extern GpGiveRec D_map_akropolis_8017C0DC[];
extern GpGiveRec D_map_akropolis_8017C16C[];

// map_neo_ark

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_neo_ark_80179F1C[];
/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern GpMapRec D_map_neo_ark_80179F24[];
/// This stage's `Gp_MapMarkTables` entry: the room markers, each a model in the
/// map picture its map room id selects.
extern GpMapMark D_map_neo_ark_8017A110[];
/// This stage's `D_8010F0E0` entry, ended by a room id of 0.
extern GpMapFlagIcon D_map_neo_ark_8017A230[];
/// This stage's `D_8010F0CC` entry, ended by a room id of 0.
extern GpMapIcon D_map_neo_ark_8017A26C[];
/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern GpMapName D_map_neo_ark_8017A28C[];
/// This stage's `Gp_Bit2Banks` list table, by room, ended by a -1 list.
extern GpBit2List D_map_neo_ark_8017A6EC[];
/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern GpTaskDesc D_map_neo_ark_8017A804[];
/// This stage's flag table for `Gp_LookupStageFlag`.
extern u16 D_map_neo_ark_8017A9A0[];
/// This stage's entries in `Gp_RoomCoordTables`, `Gp_WarpTables`,
/// `Gp_ViewCountTables`, `Gp_RoomObjTables`, `Gp_ViewTables`,
/// `Gp_ViewIndexTables`, `Gp_SprtTables` and `Gp_RoomParamTables`: each leads to
/// one pointer per room, into that room's package or, for some rooms, at a
/// record this overlay holds itself.
extern GpRoomCoordRec*  D_map_neo_ark_8017A9FC[];
extern GpWarpRec*       D_map_neo_ark_8017AA80[];
extern GpViewCountTbl   D_map_neo_ark_8017AB88;
extern GpRoomObjTbl     D_map_neo_ark_8017ACA0;
extern GpViewTbl        D_map_neo_ark_8017AD28;
extern GpViewIndexTbl   D_map_neo_ark_8017ADB0;
extern GpSprtTbl        D_map_neo_ark_8017AE38;
extern GpRoomParamRec** D_map_neo_ark_8017AE3C[];
/// Area placement lists, each ended by an entry id of 0xFF. The rooms' area
/// records point at them, one list per location; one location's list runs on
/// into the next one's.
extern GpAreaPlace D_map_neo_ark_8017AEC0[];
extern GpAreaPlace D_map_neo_ark_8017AED0[];
extern GpAreaPlace D_map_neo_ark_8017AF00[];
extern GpAreaPlace D_map_neo_ark_8017AF30[];
extern GpAreaPlace D_map_neo_ark_8017AF80[];
extern GpAreaPlace D_map_neo_ark_8017AFD0[];
extern GpAreaPlace D_map_neo_ark_8017AFF0[];
extern GpAreaPlace D_map_neo_ark_8017B010[];
extern GpAreaPlace D_map_neo_ark_8017B030[];
extern GpAreaPlace D_map_neo_ark_8017B080[];
extern GpAreaPlace D_map_neo_ark_8017B0F0[];
extern GpAreaPlace D_map_neo_ark_8017B120[];
extern GpAreaPlace D_map_neo_ark_8017B160[];
extern GpAreaPlace D_map_neo_ark_8017B190[];
extern GpAreaPlace D_map_neo_ark_8017B1C0[];
extern GpAreaPlace D_map_neo_ark_8017B1F0[];
extern GpAreaPlace D_map_neo_ark_8017B210[];
extern GpAreaPlace D_map_neo_ark_8017B2C0[];
extern GpAreaPlace D_map_neo_ark_8017B300[];
extern GpAreaPlace D_map_neo_ark_8017B330[];
extern GpAreaPlace D_map_neo_ark_8017B360[];
extern GpAreaPlace D_map_neo_ark_8017B390[];
extern GpAreaPlace D_map_neo_ark_8017B3C0[];
extern GpAreaPlace D_map_neo_ark_8017B3D0[];
extern GpAreaPlace D_map_neo_ark_8017B410[];
extern GpAreaPlace D_map_neo_ark_8017B470[];
extern GpAreaPlace D_map_neo_ark_8017B4B0[];
extern GpAreaPlace D_map_neo_ark_8017B4E0[];
extern GpAreaPlace D_map_neo_ark_8017B530[];
extern GpAreaPlace D_map_neo_ark_8017B550[];
extern GpAreaPlace D_map_neo_ark_8017B590[];
extern GpAreaPlace D_map_neo_ark_8017B5C0[];
extern GpAreaPlace D_map_neo_ark_8017B600[];
extern GpAreaPlace D_map_neo_ark_8017B610[];
extern GpAreaPlace D_map_neo_ark_8017B630[];
extern GpAreaPlace D_map_neo_ark_8017B6E0[];
extern GpAreaPlace D_map_neo_ark_8017B700[];
extern GpAreaPlace D_map_neo_ark_8017B780[];
extern GpAreaPlace D_map_neo_ark_8017B7D0[];
extern GpAreaPlace D_map_neo_ark_8017B850[];
extern GpAreaPlace D_map_neo_ark_8017B8B0[];
extern GpAreaPlace D_map_neo_ark_8017B8D0[];
extern GpAreaPlace D_map_neo_ark_8017B950[];
extern GpAreaPlace D_map_neo_ark_8017B9E0[];
extern GpAreaPlace D_map_neo_ark_8017BA60[];
extern GpAreaPlace D_map_neo_ark_8017BB00[];
extern GpAreaPlace D_map_neo_ark_8017BB70[];
extern GpAreaPlace D_map_neo_ark_8017BBF0[];
extern GpAreaPlace D_map_neo_ark_8017BC70[];
extern GpAreaPlace D_map_neo_ark_8017BCE0[];
extern GpAreaPlace D_map_neo_ark_8017BD50[];
extern GpAreaPlace D_map_neo_ark_8017BD80[];
extern GpAreaPlace D_map_neo_ark_8017BDB0[];
extern GpAreaPlace D_map_neo_ark_8017BDC0[];
extern GpAreaPlace D_map_neo_ark_8017BDD0[];
extern GpAreaPlace D_map_neo_ark_8017BE40[];
extern GpAreaPlace D_map_neo_ark_8017BE70[];
extern GpAreaPlace D_map_neo_ark_8017BEB0[];
extern GpAreaPlace D_map_neo_ark_8017BEF0[];
extern GpAreaPlace D_map_neo_ark_8017BF40[];
extern GpAreaPlace D_map_neo_ark_8017BF90[];
extern GpAreaPlace D_map_neo_ark_8017C020[];
extern GpAreaPlace D_map_neo_ark_8017C050[];
extern GpAreaPlace D_map_neo_ark_8017C070[];
extern GpAreaPlace D_map_neo_ark_8017C090[];
extern GpAreaPlace D_map_neo_ark_8017C0E0[];
extern GpAreaPlace D_map_neo_ark_8017C0F0[];
extern GpAreaPlace D_map_neo_ark_8017C140[];
extern GpAreaPlace D_map_neo_ark_8017C1D0[];
extern GpAreaPlace D_map_neo_ark_8017C210[];
extern GpAreaPlace D_map_neo_ark_8017C270[];
extern GpAreaPlace D_map_neo_ark_8017C2A0[];
extern GpAreaPlace D_map_neo_ark_8017C2E0[];
extern GpAreaPlace D_map_neo_ark_8017C320[];
extern GpAreaPlace D_map_neo_ark_8017C350[];
extern GpAreaPlace D_map_neo_ark_8017C380[];
extern GpAreaPlace D_map_neo_ark_8017C3C0[];
extern GpAreaPlace D_map_neo_ark_8017C3F0[];
extern GpAreaPlace D_map_neo_ark_8017C420[];
extern GpAreaPlace D_map_neo_ark_8017C450[];
extern GpAreaPlace D_map_neo_ark_8017C4A0[];
extern GpAreaPlace D_map_neo_ark_8017C4D0[];
extern GpAreaPlace D_map_neo_ark_8017C510[];
extern GpAreaPlace D_map_neo_ark_8017C530[];
extern GpAreaPlace D_map_neo_ark_8017C550[];
extern GpAreaPlace D_map_neo_ark_8017C580[];
extern GpAreaPlace D_map_neo_ark_8017C620[];
extern GpAreaPlace D_map_neo_ark_8017C690[];
extern GpAreaPlace D_map_neo_ark_8017C720[];
extern GpAreaPlace D_map_neo_ark_8017C760[];
/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items granted by
/// location, each ended by a key of -1.
extern GpGiveRec D_map_neo_ark_8017C9B0[];
extern GpGiveRec D_map_neo_ark_8017CB0C[];
// map_dryfield_full

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_dryfield_full_80179ADC[];
/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern GpMapRec D_map_dryfield_full_80179AE0[];
/// This stage's `Gp_MapMarkTables` entry: the room markers, each a model in the
/// map picture its map room id selects.
extern GpMapMark D_map_dryfield_full_80179D10[];
/// This stage's `D_8010F0E0` entry, ended by a room id of 0.
extern GpMapFlagIcon D_map_dryfield_full_80179E48[];
/// This stage's `D_8010F0CC` entry, ended by a room id of 0.
extern GpMapIcon D_map_dryfield_full_80179F04[];
/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern GpMapName D_map_dryfield_full_80179F4C[];
/// This stage's `Gp_Bit2Banks` list table, by room, ended by a -1 list.
extern GpBit2List D_map_dryfield_full_8017A46C[];
/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern GpTaskDesc D_map_dryfield_full_8017A5AC[];
/// This stage's flag table for `Gp_LookupStageFlag`.
extern u16 D_map_dryfield_full_8017A738[];
/// This stage's entries in `Gp_RoomCoordTables`, `Gp_WarpTables`,
/// `Gp_ViewCountTables`, `Gp_RoomObjTables`, `Gp_ViewTables`,
/// `Gp_ViewIndexTables`, `Gp_SprtTables` and `Gp_RoomParamTables`: each leads to
/// one pointer per room into that room's package.
extern GpRoomCoordRec*  D_map_dryfield_full_8017A774[];
extern GpWarpRec*       D_map_dryfield_full_8017A80C[];
extern GpViewCountTbl   D_map_dryfield_full_8017A93C;
extern GpRoomObjTbl     D_map_dryfield_full_8017A9D8;
extern GpViewTbl        D_map_dryfield_full_8017AA74;
extern GpViewIndexTbl   D_map_dryfield_full_8017AB10;
extern GpSprtTbl        D_map_dryfield_full_8017ABAC;
extern GpRoomParamRec** D_map_dryfield_full_8017ABB0[];
/// Area placement lists, each ended by an entry id of 0xFF. The rooms' area
/// records point at them, one list per location.
extern GpAreaPlace D_map_dryfield_full_8017AC48[];
extern GpAreaPlace D_map_dryfield_full_8017AD18[];
extern GpAreaPlace D_map_dryfield_full_8017AD48[];
extern GpAreaPlace D_map_dryfield_full_8017AD78[];
extern GpAreaPlace D_map_dryfield_full_8017ADA8[];
extern GpAreaPlace D_map_dryfield_full_8017ADF8[];
extern GpAreaPlace D_map_dryfield_full_8017AE78[];
extern GpAreaPlace D_map_dryfield_full_8017AE98[];
extern GpAreaPlace D_map_dryfield_full_8017AEC8[];
extern GpAreaPlace D_map_dryfield_full_8017AF18[];
extern GpAreaPlace D_map_dryfield_full_8017AF58[];
extern GpAreaPlace D_map_dryfield_full_8017AFE8[];
extern GpAreaPlace D_map_dryfield_full_8017B048[];
extern GpAreaPlace D_map_dryfield_full_8017B0B8[];
extern GpAreaPlace D_map_dryfield_full_8017B128[];
extern GpAreaPlace D_map_dryfield_full_8017B1A8[];
extern GpAreaPlace D_map_dryfield_full_8017B1C8[];
extern GpAreaPlace D_map_dryfield_full_8017B2B8[];
extern GpAreaPlace D_map_dryfield_full_8017B308[];
extern GpAreaPlace D_map_dryfield_full_8017B358[];
extern GpAreaPlace D_map_dryfield_full_8017B408[];
extern GpAreaPlace D_map_dryfield_full_8017B438[];
extern GpAreaPlace D_map_dryfield_full_8017B4A8[];
extern GpAreaPlace D_map_dryfield_full_8017B508[];
extern GpAreaPlace D_map_dryfield_full_8017B558[];
extern GpAreaPlace D_map_dryfield_full_8017B5D8[];
extern GpAreaPlace D_map_dryfield_full_8017B648[];
extern GpAreaPlace D_map_dryfield_full_8017B6A8[];
extern GpAreaPlace D_map_dryfield_full_8017B708[];
extern GpAreaPlace D_map_dryfield_full_8017B798[];
extern GpAreaPlace D_map_dryfield_full_8017B7D8[];
extern GpAreaPlace D_map_dryfield_full_8017B858[];
extern GpAreaPlace D_map_dryfield_full_8017B8C8[];
extern GpAreaPlace D_map_dryfield_full_8017B938[];
extern GpAreaPlace D_map_dryfield_full_8017B998[];
extern GpAreaPlace D_map_dryfield_full_8017BA08[];
extern GpAreaPlace D_map_dryfield_full_8017BA38[];
extern GpAreaPlace D_map_dryfield_full_8017BAB8[];
extern GpAreaPlace D_map_dryfield_full_8017BB28[];
extern GpAreaPlace D_map_dryfield_full_8017BB78[];
extern GpAreaPlace D_map_dryfield_full_8017BBE8[];
extern GpAreaPlace D_map_dryfield_full_8017BC08[];
extern GpAreaPlace D_map_dryfield_full_8017BCC8[];
extern GpAreaPlace D_map_dryfield_full_8017BD38[];
extern GpAreaPlace D_map_dryfield_full_8017BDF8[];
extern GpAreaPlace D_map_dryfield_full_8017BE58[];
extern GpAreaPlace D_map_dryfield_full_8017BE78[];
extern GpAreaPlace D_map_dryfield_full_8017BEA8[];
extern GpAreaPlace D_map_dryfield_full_8017BF08[];
extern GpAreaPlace D_map_dryfield_full_8017BF88[];
extern GpAreaPlace D_map_dryfield_full_8017C038[];
extern GpAreaPlace D_map_dryfield_full_8017C0B8[];
extern GpAreaPlace D_map_dryfield_full_8017C158[];
extern GpAreaPlace D_map_dryfield_full_8017C198[];
extern GpAreaPlace D_map_dryfield_full_8017C1B8[];
extern GpAreaPlace D_map_dryfield_full_8017C228[];
extern GpAreaPlace D_map_dryfield_full_8017C238[];
extern GpAreaPlace D_map_dryfield_full_8017C288[];
extern GpAreaPlace D_map_dryfield_full_8017C2A8[];
extern GpAreaPlace D_map_dryfield_full_8017C378[];
extern GpAreaPlace D_map_dryfield_full_8017C448[];
extern GpAreaPlace D_map_dryfield_full_8017C4E8[];
extern GpAreaPlace D_map_dryfield_full_8017C538[];
extern GpAreaPlace D_map_dryfield_full_8017C568[];
extern GpAreaPlace D_map_dryfield_full_8017C578[];
extern GpAreaPlace D_map_dryfield_full_8017C5B8[];
extern GpAreaPlace D_map_dryfield_full_8017C698[];
extern GpAreaPlace D_map_dryfield_full_8017C768[];
extern GpAreaPlace D_map_dryfield_full_8017C7F8[];
extern GpAreaPlace D_map_dryfield_full_8017C828[];
extern GpAreaPlace D_map_dryfield_full_8017C878[];
extern GpAreaPlace D_map_dryfield_full_8017C8B8[];
extern GpAreaPlace D_map_dryfield_full_8017C8E8[];
extern GpAreaPlace D_map_dryfield_full_8017C948[];
extern GpAreaPlace D_map_dryfield_full_8017C968[];
extern GpAreaPlace D_map_dryfield_full_8017C9C8[];
extern GpAreaPlace D_map_dryfield_full_8017CA28[];
extern GpAreaPlace D_map_dryfield_full_8017CA88[];
extern GpAreaPlace D_map_dryfield_full_8017CAC8[];
extern GpAreaPlace D_map_dryfield_full_8017CAF8[];
extern GpAreaPlace D_map_dryfield_full_8017CB58[];
extern GpAreaPlace D_map_dryfield_full_8017CB98[];
extern GpAreaPlace D_map_dryfield_full_8017CBF8[];
extern GpAreaPlace D_map_dryfield_full_8017CC78[];
extern GpAreaPlace D_map_dryfield_full_8017CCE8[];
extern GpAreaPlace D_map_dryfield_full_8017CD08[];
extern GpAreaPlace D_map_dryfield_full_8017CD38[];
extern GpAreaPlace D_map_dryfield_full_8017CD48[];
extern GpAreaPlace D_map_dryfield_full_8017CD88[];
extern GpAreaPlace D_map_dryfield_full_8017CDE8[];
extern GpAreaPlace D_map_dryfield_full_8017CEA8[];
extern GpAreaPlace D_map_dryfield_full_8017CF58[];
extern GpAreaPlace D_map_dryfield_full_8017CF78[];
extern GpAreaPlace D_map_dryfield_full_8017CFD8[];
extern GpAreaPlace D_map_dryfield_full_8017D018[];
extern GpAreaPlace D_map_dryfield_full_8017D038[];
/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items granted by
/// location, each ended by a key of -1.
extern GpGiveRec D_map_dryfield_full_8017D0B8[];
extern GpGiveRec D_map_dryfield_full_8017D1CC[];

// map_shelter

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_shelter_80179CD0[];
/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern GpMapRec D_map_shelter_80179CD8[];
/// This stage's `Gp_MapMarkTables` entry: the room markers, each a model in the
/// map picture its map room id selects.
extern GpMapMark D_map_shelter_80179FA4[];
/// This stage's `D_8010F0E0` entry, ended by a room id of 0.
extern GpMapFlagIcon D_map_shelter_8017A134[];
/// This stage's `D_8010F0CC` entry, ended by a room id of 0.
extern GpMapIcon D_map_shelter_8017A1F0[];
/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern GpMapName D_map_shelter_8017A268[];
/// This stage's `Gp_Bit2Banks` list table, by room, ended by a -1 list.
extern GpBit2List D_map_shelter_8017A998[];
/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern GpTaskDesc D_map_shelter_8017AB30[];
/// This stage's flag table for `Gp_LookupStageFlag`.
extern u16 D_map_shelter_8017AD88[];
/// This stage's entries in `Gp_RoomCoordTables`, `Gp_WarpTables`,
/// `Gp_ViewCountTables`, `Gp_RoomObjTables`, `Gp_ViewTables`,
/// `Gp_ViewIndexTables`, `Gp_SprtTables` and `Gp_RoomParamTables`: each leads to
/// one pointer per room into that room's package, except that some rooms'
/// coordinate and object records are this overlay's own.
extern GpRoomCoordRec*  D_map_shelter_8017AEC4[];
extern GpWarpRec*       D_map_shelter_8017AF88[];
extern GpViewCountTbl   D_map_shelter_8017B110;
extern GpRoomObjTbl     D_map_shelter_8017B3B8;
extern GpViewTbl        D_map_shelter_8017B480;
extern GpViewIndexTbl   D_map_shelter_8017B548;
extern GpSprtTbl        D_map_shelter_8017B610;
extern GpRoomParamRec** D_map_shelter_8017B614[];
/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items granted by
/// location, each ended by a key of -1.
extern GpGiveRec D_map_shelter_8017BB58[];
extern GpGiveRec D_map_shelter_8017BD8C[];

#endif /* MAPUI_STAGE_TABLES_H */
