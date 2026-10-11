#ifndef SRC_ROOMS_SHELTER_B3_DUMPING_HOLE_SHELTER_B3_DUMPING_HOLE_PRIVATE_H
#define SRC_ROOMS_SHELTER_B3_DUMPING_HOLE_SHELTER_B3_DUMPING_HOLE_PRIVATE_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/evs.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB3DumpingHoleModel0A0CC;

extern TmdSource gShelterB3DumpingHoleModel0A348;

extern TmdSource gShelterB3DumpingHoleModel0A5EC;

/// Values stored in the incinerator-exit block latch.
enum {
    SHELTER_B3_DUMPING_HOLE_INCINERATOR_EXIT_OPEN    = 0,
    SHELTER_B3_DUMPING_HOLE_INCINERATOR_EXIT_BLOCKED = 1,
};

/// Incinerator-exit block latch (0 open, 1 blocked), stored as a signed halfword.
///
/// Starts blocked on overlay load and clears when the collapse script starts.
/// The exit query bypasses this latch in room 2. Shared by the room's source
/// files; the room overlay owns its lifetime.
extern s16 gShelterB3DumpingHoleIncineratorExitBlocked;

extern AnimationSet* D_shelter_b3_dumping_hole_801880A0[6];

enum { SHELTER_B3_DUMPING_HOLE_COLLAPSE_TASK_SHARD = 1 };

/// Collapse director and triangular-shard task recipes, both at priority 192.
///
/// Entry 0 owns a TMD body and allocates its primitive buffer explicitly.
/// Entry 1 owns one coordinate and borrows a director-owned shard spawn record
/// through its first update. Both allocate their own work; argument 1 is unused.
/// Keep the room loaded until the director, callbacks and shards finish.
extern TaskDesc gShelterB3DumpingHoleCollapseTaskDescriptors[2];

/// First-arrival scene for placement variant 1 through warp 3.
///
/// Installs player clips 48..50, hides placement 0, stages shake and sprites,
/// then plays CAP command 2 and restores weapons at the final player pose/view.
/// All 39 commands end at `EVENT_SCRIPT_OPCODE_END`; waits count unfrozen
/// interpreter updates. The room and borrowed actors/resources must stay loaded.
extern EvsCommand gShelterB3DumpingHoleArrivalEventScript[39];

/// Skip path for the first-arrival scene, restoring its final player pose and view.
///
/// Stops both arrival sounds, requests player-sprite teardown and restores weapons
/// before scene cleanup. All 14 commands end at `EVENT_SCRIPT_OPCODE_END`;
/// keep the room and its borrowed payloads live until playback and effects finish.
extern EvsCommand gShelterB3DumpingHoleArrivalEventSkipScript[14];

/// Retained byte cleared during room initialization; its role is unproven.
extern u8 D_shelter_b3_dumping_hole_8018F4A4;

/// Returns the incinerator-exit block flag, except that room 2 always permits passage.
///
/// The flag starts set and is cleared when the collapse event starts. The room's
/// transition handler treats a nonzero result as a refused incinerator departure.
s16 shelterB3DumpingHoleIsIncineratorExitBlocked(void);

/// Selects the room's loaded CAP caption data and font texture-page origin.
///
/// X counts VRAM words and Y counts rows. `dataResourceIndex` is zero-based
/// among data resources. Requires completed bundle loading and storage/textures
/// that remain live during caption use. Invalid data keeps the prior captions;
/// page coordinates are stored regardless. Performs no I/O or allocation.
void shelterB3DumpingHoleSelectCaptionResource(s16 texturePageX, s16 texturePageY, s16 dataResourceIndex);

#endif // SRC_ROOMS_SHELTER_B3_DUMPING_HOLE_SHELTER_B3_DUMPING_HOLE_PRIVATE_H
