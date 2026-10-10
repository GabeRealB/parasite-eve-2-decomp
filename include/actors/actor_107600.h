#ifndef INCLUDE_ACTORS_ACTOR_107600_H
#define INCLUDE_ACTORS_ACTOR_107600_H

#include "main/task_types.h"

/// Descriptor slots for the gallery mount and its carried target models.
enum {
    ACTOR_107600_TASK_MOUNT       = 0,
    ACTOR_107600_TASK_TARGET_BASE = 1,
    ACTOR_107600_TASK_COUNT       = 19,
};

/// Shooting-gallery mount and target spawn recipes, with no terminator.
///
/// Slot 0 creates the mount; target kind + 1 selects its recipe in slots 1..18.
/// All create TMD bodies with automatic primitive buffers at priority 96.
/// The mount selects a kind from its spawn argument's low nibble, so it never
/// selects the final two target recipes. Courses use kinds 0..12; the fixed
/// demonstration uses kind 13, whose HP-table backing is unproven.
///
/// Mount spawn argument 1 packs kind in bits 0..3, behaviour (0 standing,
/// 1 hanging, 2 fixed) in 12..15, path (0..61) in 16..23, hold seconds in
/// 24..27, rotation in bit 28, attack enable in 29 and forced placement in 30.
/// Nonfixed mounts require a live course-controller parent. The mount creates
/// and owns its child target; enemy spawns supply a live `Enemy` in argument 2.
/// Target argument 1 keeps kind in bits 0..3, mount/target handshake signals in
/// 4..7, mount behaviour in 16..19 and attack enable in 29.
/// Keep actor_107600 and the gallery's borrowed models loaded until tasks exit.
extern TaskDesc gActor107600TaskDescriptors[ACTOR_107600_TASK_COUNT];

#endif // INCLUDE_ACTORS_ACTOR_107600_H
