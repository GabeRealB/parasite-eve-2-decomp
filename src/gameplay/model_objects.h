#ifndef GAMEPLAY_PRIVATE_MODEL_OBJECTS_H
#define GAMEPLAY_PRIVATE_MODEL_OBJECTS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "main/coord.h"

extern const CVECTOR gGpColorGrey;

extern const CVECTOR gGpColorWhite;

/// Brings a coordinate's world matrix up to date, and its ancestors' with it.
///
/// The chain of `sub` links is walked to `root` and composed on the way back
/// down, so a coordinate's world matrix is its parent's world matrix multiplied
/// by its own local one. Each coordinate records in `flg` the stamp of the pass
/// that last rebuilt it, and is rebuilt when that stamp is older than its
/// parent's — which is what clearing `flg` asks for. A coordinate whose `flg` is
/// clear at the walk's end has never been composed and takes its local matrix
/// as its world matrix.
///
/// `parity` is the value written to the top bit of `flg`; it differs from one
/// pass to the next, which is what tells the walk an ancestor has already been
/// reached in this one. `root` ends the walk: the composed matrices are left
/// relative to it, and its own world matrix is neither updated nor folded in,
/// because the caller that passes one applies that transformation itself.
/// `NULL` stops at the top of the chain.
void _gpUpdateCoordTree(GpCoord* coord, s32 stamp, s32 parity, GpCoord* root);

#endif // GAMEPLAY_PRIVATE_MODEL_OBJECTS_H
