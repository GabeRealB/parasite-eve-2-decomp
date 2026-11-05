#ifndef APOBIOSIS_APOBIOSIS_H
#define APOBIOSIS_APOBIOSIS_H

#include <engine/task_types.h>

/// The Apobiosis cast: a flash and a ring of strips around the caster, then shards in growing bursts, two a frame at
/// levels 2 and 3, under a flickering flash that fades out. Applies the spell during the ring; a held attachment or a
/// room's PE cancellation frees it and its shards.
void apobiosisCastTask(Task* task);

/// An Apobiosis shard (`EFFECT_APOBIOSIS_SHARD`): a spinning sprite and a strip upward, drawn every other frame, pinned
/// to its spawn point when `spawnArg1` is nonzero and drifting otherwise; it joins the cast and is freed with it.
void apobiosisShardTask(Task* task);

#endif // APOBIOSIS_APOBIOSIS_H
