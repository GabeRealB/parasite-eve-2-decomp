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

extern s16 D_shelter_b3_dumping_hole_8018809C;

extern AnimationSet* D_shelter_b3_dumping_hole_801880A0[6];

extern TaskDesc D_shelter_b3_dumping_hole_80189ADC[2];

extern EvsCommand D_shelter_b3_dumping_hole_8018B080[39];

extern EvsCommand D_shelter_b3_dumping_hole_8018B428[14];

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
