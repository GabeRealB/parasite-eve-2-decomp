#ifndef MAPUI_MAPUI_H
#define MAPUI_MAPUI_H

/* The map UI family's interface to the resident code: the per-map tables main
 * indexes by map (Akropolis, Dryfield, Dryfield full view, Shelter, Neo Ark).
 * Every map overlay loads at the same address, so each table is named after
 * the map that holds it.
 */

#include "common.h"

#include "main/gfx.h"
#include "main/task.h"

/// Image slots of each map, indexed by `Gfx_ImageSlotTables`.
extern GfxImageSlot D_map_akropolis_8017A048[];
extern GfxImageSlot D_map_dryfield_80179A14[];
extern GfxImageSlot D_map_dryfield_full_801799A4[];
extern GfxImageSlot D_map_shelter_80179B40[];
extern GfxImageSlot D_map_neo_ark_80179DB8[];

/// Per-map tables `D_8005DCB4` indexes (maps without one have NULL there).
extern s32 D_map_akropolis_8017A0F8[];
extern s32 D_map_dryfield_80179B4C[];
extern s32 D_map_neo_ark_80179EC8[];

/// Per-map task id tables `D_8006273C` and `D_80062750` index.
extern TaskIdPair D_map_akropolis_8017C1B4[];
extern TaskIdPair D_map_dryfield_8017BDE0[];
extern TaskIdPair D_map_dryfield_full_8017D238[];
extern TaskIdPair D_map_shelter_8017BE28[];
extern TaskIdPair D_map_neo_ark_8017CB54[];
extern TaskIdPair D_map_akropolis_8017C304[];
extern TaskIdPair D_map_dryfield_8017C004[];
extern TaskIdPair D_map_dryfield_full_8017D594[];
extern TaskIdPair D_map_shelter_8017C2D8[];
extern TaskIdPair D_map_neo_ark_8017CDFC[];

#endif /* MAPUI_MAPUI_H */
