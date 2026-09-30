/* The per-frame motion driver of one enemy family (actor_01200, actor_04000,
 * actor_123200 and actor_223600). The state handlers ask for an animation by
 * setting `motion` to 1 or 2 with its id in `requested`; the driver restarts
 * animation slots 1 to 5 on it, then advances them each frame at the combined
 * rate and keeps the frame counters the handlers read.
 *
 * Include this header in the prologue and anim_driver_tick.inc.c at the
 * driver's position.
 */

#ifndef SRC_SHARED_ANIM_DRIVER_H
#define SRC_SHARED_ANIM_DRIVER_H

#include "common.h"

#include "gameplay/animation.h"

#include "main/task_types.h"

/// The part of the family's work block the driver reads and writes. The rest
/// belongs to each package's own view of the block.
typedef struct AnimDriverWork {
    /* 0x000 */ byte             pad_0[0xC];
    /* 0x00C */ AnimationContext anim;
    /* 0x020 */ AnimationSlot    slots[6];
    /* 0x110 */ byte             poses[0x60];
    /* 0x170 */ s16              motion;    // 1 or 2 asks for a restart; 3 while running
    /* 0x172 */ s16              playing;   // the animation the slots were last restarted on
    /* 0x174 */ s16              requested; // the animation a restart plays
    /* 0x176 */ s16              rate;      // slot rate, added to `rateBias`
    /* 0x178 */ s16              rateBias;
    /* 0x17A */ s16              frame;     // frames since the restart
    /* 0x17C */ s16              cueFrames; // frames since the restart with slot 1's flag bit 1 set
} AnimDriverWork;

void animDriverTick(Task* task);

#endif /* SRC_SHARED_ANIM_DRIVER_H */
