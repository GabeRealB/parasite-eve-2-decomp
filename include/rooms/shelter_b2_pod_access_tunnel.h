#ifndef ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H
#define ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Event parameters latched into the room's pending event when an event
/// starts. `field_8` is the game flag checked and set as the event starts
/// (0 for none); the other fields are only copied here, so their roles are
/// unproven from this room.
typedef struct ShelterB2PodAccessTunnelEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} ShelterB2PodAccessTunnelEvent;
STATIC_ASSERT_SIZEOF(ShelterB2PodAccessTunnelEvent, 0xC);

/// The three-state table the room task driver
/// `func_shelter_b2_pod_access_tunnel_8017DC14` dispatches through.
extern const TaskFuncTable3 D_shelter_b2_pod_access_tunnel_8017D5D8;

/// Draws a glowing beam along the segment `arg0[0]`..`arg0[1]`.
void func_shelter_b2_pod_access_tunnel_8017DF64(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H
