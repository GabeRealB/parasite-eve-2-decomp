#ifndef INCLUDE_ACTORS_ACTOR_403600_H
#define INCLUDE_ACTORS_ACTOR_403600_H

#include "common.h"
#include "types.h"

#include "main/coord.h"

/// Samples of its wave an `Actor403600Ripple` keeps: two for each of the
/// sixteen rings its disc is drawn in.
enum { ACTOR_403600_RIPPLE_SAMPLE_COUNT = 0x20 };

/// A ripple spreading over a disc that is drawn with the captured frame: the
/// wave's source at the centre and the samples of it still travelling outward.
///
/// Every tick the source's phase and strength are recorded as one sample at
/// `head`, which steps backward through the two sample rings. The disc is
/// drawn as twelve sectors of sixteen rings, and ring *n* shows the sample
/// taken 2*n* ticks earlier, so the wave moves out one ring every two ticks.
/// A sample lifts its ring out of the disc's plane by the sine of its phase
/// times its strength, and shifts the texels of the captured frame the ring
/// shows by an amount that grows with its strength; both fade toward the rim.
///
/// The owner sets `emitting` while the source is fed. The strength then rises,
/// and falls away once the owner clears it; a ripple whose samples all have
/// phase 0 has died out. actor_403600 allocates one zeroed for each ripple
/// task, and actor_361100 allocates its own, sets `shallow` and runs it
/// through actor_403600's tick and draw.
typedef struct {
    s16      phase[ACTOR_403600_RIPPLE_SAMPLE_COUNT];    // Wave phase of each sample, 4096 per turn; 0 in a slot that recorded no wave
    s16      strength[ACTOR_403600_RIPPLE_SAMPLE_COUNT]; // Wave strength of each sample, 0 to 0x1000
    s32      head;                                       // Slot of the newest sample; steps back one slot a tick and wraps
    s32      sourcePhase;                                // Phase the next sample takes: restarts at 0 on the tick emission begins, then advances 0x180 a sample, or 0x100 when `shallow`
    s32      sourceStrength;                             // Strength the next sample takes: rises 0x200 a tick to 0x1000 while `emitting`, falls 0x80 a tick to 0 otherwise. No sample is recorded while it is 0
    s16      wasEmitting;                                // `emitting` as the previous tick found it, which tells the tick emission begins
    s16      emitting;                                   // Written by the owner (0 the wave dies away, nonzero the source is fed)
    GfxCoord clipCoord;                                  // actor_403600 only: the disc's plane as a coordinate of its own under the disc's, turned half a turn about Z for a ripple spawned in mode 2. While the package's model drawers are pointed at it they flatten every vertex on its positive-Y side onto the plane
    s32      shallow;                                    // How far a sample lifts its ring (0 an eighth of the wave's height, 1 a 256th of it, with the slower phase advance)
    s32      clipCountdown;                              // actor_403600 only, counted down each tick. Spawn mode 1 starts it at 8, cuts the owner's model at `clipCoord` until it reaches 0 and then hides the model; mode 2 starts it at 0x1F, shows the model when it reaches 0 and cuts it for eight ticks more
} Actor403600Ripple;
STATIC_ASSERT_SIZEOF(Actor403600Ripple, 0xE8);

extern u8* D_actor_403600_8016069C;

#endif // INCLUDE_ACTORS_ACTOR_403600_H
