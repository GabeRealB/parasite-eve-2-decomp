#ifndef ACTORS_SHARED_80136614_H
#define ACTORS_SHARED_80136614_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Measures `arg0` against the caller's own coordinate: the world delta between
/// the two `workm` translations is rotated into `arg0`'s frame with the GTE and
/// written to the 0x40-byte scratch block, which gives both the heading
/// difference and, from the two `coord` translations, the planar distance.
///
/// `arg1` receives the distance and the folded heading is returned. Carried by
/// `actor_107000`, `actor_207000` and `actor_207200`; the shared span is in
/// `configs/USA/overlays.toml`.
s32 ActorsShared80136614(GsCOORDINATE2* arg0, u32* arg1);

#endif
