/* Shared by the rooms with a target the player shoots at: acropolis_bridge and mine_cavern. */

/// Returns the first attack key in a contiguous contact prefix and copies its point.
///
/// Borrows `contactCount` readable entries and writable `hitPoint` for this call.
/// Stops at the first zero key or the signed count; a nonpositive count reads
/// nothing. Returns zero and leaves the point unchanged when no attack is found.
/// Copies only XYZ in world-coordinate units, preserving the point's fourth
/// halfword. Contact flags do not affect the scan; no pointers are retained.
static inline s32 _roomFindAttackContact(SVECTOR* hitPoint, const WorldCollisionContact* contacts, s16 contactCount)
{
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        if (contacts[contactIndex].key.value == 0) {
            break;
        }
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            hitPoint->vx = contacts[contactIndex].point.vx;
            hitPoint->vy = contacts[contactIndex].point.vy;
            hitPoint->vz = contacts[contactIndex].point.vz;
            return contacts[contactIndex].key.value;
        }
    }
    return 0;
}
