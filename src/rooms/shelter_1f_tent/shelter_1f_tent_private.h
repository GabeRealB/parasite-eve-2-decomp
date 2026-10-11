#ifndef SRC_ROOMS_SHELTER_1F_TENT_SHELTER_1F_TENT_PRIVATE_H
#define SRC_ROOMS_SHELTER_1F_TENT_SHELTER_1F_TENT_PRIVATE_H

#include "gameplay/area_flags.h"

#include "main/task_types.h"

#include "rooms/room.h"

/// First-arrival saved-layout updates across the Neo Ark and Mine/Shelter stages.
///
/// Fifty-two updates followed by `AREA_APPLY_END`. Most apply in every mode;
/// Power Plant 2 uses separate normal/Scavenger and Bounty/Nightmare layouts.
/// Each accepted update discards saved enemy poses and sets or clears its map
/// mark. The initializer borrows this read-only list during the loaded room visit.
extern const AreaApplyRec gShelter1fTentFirstArrivalAreaUpdates[53];

/// First-arrival Heliport layout 2 before the examined-item follow-up has run.
///
/// One every-mode update followed by `AREA_APPLY_END`; clears the map mark and
/// saved enemy poses. Borrowed when `GAME_FLAG_ITEM_125_FOLLOWUP_SEEN` is zero.
extern const AreaApplyRec gShelter1fTentHeliportBeforeItemFollowupAreaUpdates[2];

/// First-arrival Heliport layout 1 after the examined-item follow-up has run.
///
/// One every-mode update followed by `AREA_APPLY_END`; clears the map mark and
/// saved enemy poses. Borrowed when `GAME_FLAG_ITEM_125_FOLLOWUP_SEEN` is nonzero.
extern const AreaApplyRec gShelter1fTentHeliportAfterItemFollowupAreaUpdates[2];

/// First-arrival GOLEM Freezer 1 layout 21 after the examined-item follow-up has run.
///
/// One every-mode update followed by `AREA_APPLY_END`; clears the map mark and
/// saved enemy poses. Borrowed when `GAME_FLAG_ITEM_125_FOLLOWUP_SEEN` is nonzero.
extern const AreaApplyRec gShelter1fTentGolemFreezer1AfterItemFollowupAreaUpdates[2];

extern Task* gRoomCutsceneSoundTask;

/// Singleton request for the tent's replay of CAP slot/file 1 in view 5.
///
/// The room fills the scene and sound fields before spawning its runner. The
/// runner borrows this record through completion; leave it unchanged and keep
/// the tent and CAP resources loaded while the scene or its sound task runs.
extern RoomCutsceneRec gShelter1fTentReplayCutscene;

#endif // SRC_ROOMS_SHELTER_1F_TENT_SHELTER_1F_TENT_PRIVATE_H
