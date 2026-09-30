#ifndef GAMEPLAY_WORLD_TARGETS_TYPES_H
#define GAMEPLAY_WORLD_TARGETS_TYPES_H

#include <psyq/sys/types.h>

#include "common.h"

/// Bits stored in a target node's `flags` byte.
///
/// Not lockable removes the node from lock-on, the reticle and the radar
/// blip. Keep-scanned puts a not-lockable node back into the position refresh
/// and the area scans. Hide HP draws `????` in place of the HP value and bar.
/// No other bit is read.
enum {
    WORLD_TARGET_NOT_LOCKABLE = 0x01,
    WORLD_TARGET_KEEP_SCANNED = 0x04,
    WORLD_TARGET_HIDE_HP      = 0x08
};

/// `state.word` bits that select scan participation.
///
/// Not lockable alone excludes the node; keep-scanned includes it again.
/// The mask is the low byte of the word, which is `flags`.
#define WORLD_TARGET_SCAN_MASK (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED)

/// Byte mask that clears `WORLD_TARGET_NOT_LOCKABLE` and keeps the other flag bits.
#define WORLD_TARGET_NOT_LOCKABLE_CLEAR ((~WORLD_TARGET_NOT_LOCKABLE) & 0xFF)

/// One entry on the list of enemies the targeting passes track.
///
/// Every enemy carries this as its `node` member, and the tracked list links
/// those members through `next`. Lock-on, the reticle, the radar and the area
/// scans walk the list and recover the enemy from the node. The enemy's actor
/// writes `flags`. `targeted` and `onList` record the actor-slot lock and
/// list membership. A pass that tests lock and scan eligibility together reads
/// `state.word`: its low byte is `flags`, and its top byte is not read or
/// written on its own. That byte's role is unproven.
typedef struct WorldTargetNode {
    struct WorldTargetNode* next; // Successor on the tracked list; NULL at the tail
    union {
        struct {
            u8 flags;    // WORLD_TARGET_* lock, scan and HP-readout bits
            u8 targeted; // Actor-slot lock (0 clear, 1 a slot is aimed at this node)
            u8 onList;   // Tracked-list membership (0 off, 1 on)
        } parts;
        u32 word;        // Same storage as `parts`; low byte is `flags`, top byte unread
    } state;
} WorldTargetNode;
STATIC_ASSERT_SIZEOF(WorldTargetNode, 0x8);
STATIC_ASSERT(OFFSET_OF(WorldTargetNode, state) == 4, WorldTargetNode_state);

#endif // GAMEPLAY_WORLD_TARGETS_TYPES_H
