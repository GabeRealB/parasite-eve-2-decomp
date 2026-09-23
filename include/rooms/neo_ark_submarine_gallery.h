#ifndef ROOMS_NEO_ARK_SUBMARINE_GALLERY_H
#define ROOMS_NEO_ARK_SUBMARINE_GALLERY_H

#include "common.h"

#include <psyq/libgte.h>

/// Scratch block `func_neo_ark_submarine_gallery_80180E80` takes from
/// `G_SCRATCH_HEAD` for one quad of a prism. `v` holds the four corners after
/// rotation by the model's `workm` and translation by its offset; `sxy` is
/// their screen projection, copied onto the `POLY_G4` once one is allocated.
/// `flag` is `gte_stflg` of that projection (negative rejects the quad) and
/// `otz` is `gte_stszotz`, which picks the OT bucket.
typedef struct NeoArkSubmarineGalleryPrismScratch {
    SVECTOR v[4];
    s32     otz;
    s32     flag;
    DVECTOR sxy[4];
} NeoArkSubmarineGalleryPrismScratch;

#endif // ROOMS_NEO_ARK_SUBMARINE_GALLERY_H
