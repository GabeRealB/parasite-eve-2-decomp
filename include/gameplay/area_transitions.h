#ifndef GAMEPLAY_AREA_TRANSITIONS_H
#define GAMEPLAY_AREA_TRANSITIONS_H

#include "gameplay/area_flags.h"

#include "main/session_types.h"
#include "main/task_types.h"

// Area transitions and persistent area-flag updates.

/// Mirror of `Gp_SetCurAreaFlag4` for an explicit key: clears
/// `AREA_SAVED_MAP_MARK` in `AreaSavedState.spawnFlags` on the record selected
/// by `Gp_AreaTables[key->stage]` + `key->area`. Null records are skipped, as
/// in the setter.
void Gp_ClearAreaFlag4(GameLocationKey* key);

void func_800AEE8C(Task* arg0);

/// Applies placement layouts and map marks from a saved-area update list.
///
/// Borrows an `AREA_APPLY_END`-terminated list synchronously. Each accepted
/// record sets its area's layout, removes saved enemy poses for that area, and
/// sets or clears `AREA_SAVED_MAP_MARK`. The high policy nibble selects the
/// live save's mode group; the ALWAYS policy also accepts unknown save modes.
/// Missing stage tables and missing saved state are skipped. Records need valid
/// 1-based stage and area selectors for their loaded directories; no bounds
/// checks are performed. Repeated records apply in list order.
void areaApplySavedUpdates(const AreaApplyRec* records);

#endif // GAMEPLAY_AREA_TRANSITIONS_H
