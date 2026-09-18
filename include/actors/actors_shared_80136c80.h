#ifndef ACTORS_SHARED_80136C80_H
#define ACTORS_SHARED_80136C80_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "main/task.h"

/// Movement work the tick reaches through `Task::work`. This is the head of the
/// block each carrier's own work type overlays (`Actor107000Work` from 0x214
/// on, `Actor207000Work` from its own), so the two are views of one block, not
/// one layout.
///
/// `field_0`/`field_2`/`field_4` are the per-frame velocity the tick folds onto
/// the model: negated into `recs[0]`'s position and added to the coordinate's
/// own translation. `recs[1]` is the collision record the hit test walks, and
/// `field_26` the flag word it trims to 0x3FFF once a hit lands.
typedef struct ActorsShared80136c80Work {
    /* 0x00 */ u16     field_0;
    /* 0x02 */ u16     field_2;
    /* 0x04 */ u16     field_4;
    /* 0x06 */ byte    pad_6[0x20];
    /* 0x26 */ u16     field_26;
    /* 0x28 */ GpRec18 recs[2];
} ActorsShared80136c80Work;
STATIC_ASSERT_SIZEOF(ActorsShared80136c80Work, 0x58);

/// Global mode the tick dispatches on: 0 runs the tick (zeroing the model
/// part's flag word first), 1 is a plain return and 2 hides the part with
/// `field_C = 0x80`. Published in the main executable; several overlays of the
/// specimen - `actor_300700`, `actor_105100`, `actor_503500` - read it too.
extern u8 D_801153F4;

/// Per-frame tick of the specimen's movement cycle; carried by `actor_107000`
/// and `actor_207000`, which both reach the work through `Task::work`.
void ActorsShared80136c80(Task* arg0);

#endif
