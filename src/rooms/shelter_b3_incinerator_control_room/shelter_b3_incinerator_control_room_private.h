#ifndef SRC_ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_SHELTER_B3_INCINERATOR_CONTROL_ROOM_PRIVATE_H
#define SRC_ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_SHELTER_B3_INCINERATOR_CONTROL_ROOM_PRIVATE_H

#include "gameplay/area_flags.h"

#include "main/task_types.h"

#include "rooms/room.h"

/// Telephone interaction ID delivered by the room's action triggers.
enum { SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_COMMAND = 1 };

/// Saved-area updates applied on arrival warp 4 from the incinerator.
///
/// All save modes select variant 2 and clear map marks for the dumping hole and
/// incinerator. Bounty/Nightmare select variant 7 and set map marks for the
/// lower and upper sewers. The fifth entry is AREA_APPLY_END. Borrowed only
/// during application; no pointer is retained.
extern AreaApplyRec gShelterB3IncineratorControlRoomArrivalSavedUpdates[5];

extern Task* gRoomCutsceneSoundTask;

/// Mutable playback settings for repeat-use telephone conversations and the save menu.
///
/// The command handler fills view 8, CAP slot/file 1 and room sound cues 1..4,
/// then lends this record to the runner through completion. Follow-up command 7
/// opens the telephone menu through placed-object action 43. Both texture-page
/// coordinates remain zero from BSS initialization, selecting VRAM (960, 0).
/// Keep the room loaded and do not overwrite the record during playback.
extern RoomCutsceneRec gShelterB3IncineratorControlRoomTelephoneCutscene;

#endif // SRC_ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_SHELTER_B3_INCINERATOR_CONTROL_ROOM_PRIVATE_H
