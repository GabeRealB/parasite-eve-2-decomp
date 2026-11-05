#ifndef GAMEPLAY_GLUTTON_RAIN_EFFECT_H
#define GAMEPLAY_GLUTTON_RAIN_EFFECT_H

/// Phases carried in the Glutton rain blob effect's `Task::spawnArg1.value`.
///
/// Spawn `EFFECT_GLUTTON_RAIN_BLOB` with START. Its first running update emits
/// a particle and advances to FLYING, which follows the borrowed rain-projectile
/// coordinate until its owner requests BURST on landing. BURST emits the landing
/// particles and advances to END; the next running update releases the effect.
/// Requests require a still-live effect task. Room-effect suspension defers them,
/// while cancellation releases the effect immediately after its final redraw.
enum {
    EFFECT_GLUTTON_RAIN_BLOB_START  = 0,
    EFFECT_GLUTTON_RAIN_BLOB_FLYING = 1,
    EFFECT_GLUTTON_RAIN_BLOB_BURST  = 2,
    EFFECT_GLUTTON_RAIN_BLOB_END    = 3
};

#endif // GAMEPLAY_GLUTTON_RAIN_EFFECT_H
