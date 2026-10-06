/* The per-frame animation driver of one enemy family (actor_01200, actor_04000,
 * actor_123200 and actor_223600). A state handler asks for an animation by
 * storing its set index in `requestedSet` and a restart request in `state`.
 * The next tick restarts slots 1 to 5 on that set; every tick after it
 * advances them at the combined rate and keeps the two counters.
 *
 * Include this header in the prologue and anim_driver_tick.inc.c at the
 * driver's position.
 */

#ifndef SRC_SHARED_ANIM_DRIVER_H
#define SRC_SHARED_ANIM_DRIVER_H

#include "common.h"

#include "actors/actor.h"

#include "gameplay/animation.h"

/// Values of `AnimDriverWork::state`.
///
/// A zero-filled block holds 0, on which a tick does nothing. The driver
/// handles the two restart requests identically.
enum {
    ANIM_DRIVER_STATE_RESTART_1 = 1, // Restart requested
    ANIM_DRIVER_STATE_RESTART_2 = 2, // Restart requested
    ANIM_DRIVER_STATE_PLAYING   = 3  // The slots advance on every tick
};

/// First rig slot the driver restarts and advances; slot 0 is never driven.
///
/// It is also the slot whose control jumps `AnimDriverWork::jumpCount` counts.
enum { ANIM_DRIVER_FIRST_SLOT = 1 };

/// The head of every work block the driver runs on, reached through `Task::work`.
///
/// The carrier's own fields follow `jumpCount` directly. `rate` and `rateBias`
/// are signed sixteenths of a frame per tick, `ANIMATION_RATE_ONE` being
/// normal speed. Their sum is stored into each driven slot's eight-bit
/// `AnimationSlot::rate` before every advance, so it has to stay within
/// -128..127; zero holds the pose and a negative sum plays backwards. A
/// restart leaves the slots at `ANIMATION_RATE_ONE` until the first advance
/// installs the sum again.
typedef struct {
    byte          carrierState[0xC]; // The carrier's own state words; the driver never touches them
    ActorAnimRig6 rig;               // Playback storage, bound by the carrier; the driver uses slots 1 to 5
    s16           state;             // ANIM_DRIVER_STATE_* (0 idle, 1 or 2 restart requested, 3 playing)
    s16           playingSet;        // Animation set index the slots were last restarted on
    s16           requestedSet;      // Animation set index the next restart plays
    s16           rate;              // Playback rate in sixteenths of a frame per tick
    s16           rateBias;          // Added to `rate`, in the same units
    s16           tickCount;         // Advancing ticks since the last restart
    s16           jumpCount;         // Advancing ticks since the last restart on which the first driven slot followed a control jump
} AnimDriverWork;

#endif /* SRC_SHARED_ANIM_DRIVER_H */
