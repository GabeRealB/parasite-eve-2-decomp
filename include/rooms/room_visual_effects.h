#ifndef ROOMS_ROOM_VISUAL_EFFECTS_H
#define ROOMS_ROOM_VISUAL_EFFECTS_H

/// States of the room glow disc's task, including requests made by its owner.
///
/// Spawn starts at ATTACH and advances to GROW on the first active tick.
/// The owner may request FLICKER, RELEASE or CANCEL while the task is live.
/// RELEASE shrinks and fades the disc while expanding an orange ring; CANCEL
/// releases it on the next active tick. Room effect control still pauses these
/// requests at nonzero values below four and cancels at four or above.
enum {
    ROOM_VISUAL_EFFECTS_GLOW_DISC_ATTACH  = 0,
    ROOM_VISUAL_EFFECTS_GLOW_DISC_GROW    = 1,
    ROOM_VISUAL_EFFECTS_GLOW_DISC_FLICKER = 2,
    ROOM_VISUAL_EFFECTS_GLOW_DISC_RELEASE = 3,
    ROOM_VISUAL_EFFECTS_GLOW_DISC_CANCEL  = 4
};

#endif // ROOMS_ROOM_VISUAL_EFFECTS_H
