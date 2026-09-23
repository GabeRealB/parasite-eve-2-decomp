#ifndef ROOMS_SHELTER_B1_POD_ACCESS_TUNNEL_H
#define ROOMS_SHELTER_B1_POD_ACCESS_TUNNEL_H

#include "common.h"

/// Event parameters latched into the room's pending event when an event
/// starts. `field_8` is the game flag checked and set as the event starts
/// (0 for none); the other fields are only copied here, so their roles are
/// unproven from this room.
typedef struct ShelterB1PodAccessTunnelEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} ShelterB1PodAccessTunnelEvent;
STATIC_ASSERT_SIZEOF(ShelterB1PodAccessTunnelEvent, 0xC);

/// Work block of the task that scrolls one full-screen image vertically into
/// another. Its first state allocates it zeroed and sets `speed`; the drawing
/// state advances `offset` and places the seam between the images from it.
typedef struct {
    s32 speed;  ///< 16.16 per-frame step, sized so the scroll completes in the spawn argument's frame count
    s32 offset; ///< 16.16 scroll distance; its integer part is clamped to 240 lines
    s16 timer;  ///< Frames drawn so far; scrolling starts once it reaches 46
} ShelterB1PodAccessTunnelWork;
STATIC_ASSERT_SIZEOF(ShelterB1PodAccessTunnelWork, 0xC);

void func_shelter_b1_pod_access_tunnel_8017E66C(s32 tpage, s16 arg1);

#endif // ROOMS_SHELTER_B1_POD_ACCESS_TUNNEL_H
