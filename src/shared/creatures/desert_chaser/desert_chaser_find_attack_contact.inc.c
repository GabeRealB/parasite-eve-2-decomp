/* Part of the desert chaser library; see desert_chaser.h. */

/// Returns the first attack contact key and copies its world-space hit position.
///
/// Reads a sphere's `DESERT_CHASER_CONTACTS` entries up to the first zero key.
/// Returns zero without changing `hitPosition` when no attack is found.
/// Only XYZ are copied; the vector's fourth halfword is untouched. Inputs
/// and writable output must remain live through the call.
static inline s32 _desertChaserFindAttackContact(const WorldCollisionContact* contacts,
                                                 SVECTOR*                     hitPosition)
{
    s16 contactIndex;
    for (contactIndex = 0; contactIndex < DESERT_CHASER_CONTACTS; contactIndex++) {
        if (!contacts[contactIndex].key.value)
            break;
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            hitPosition->vx = contacts[contactIndex].point.vx;
            hitPosition->vy = contacts[contactIndex].point.vy;
            hitPosition->vz = contacts[contactIndex].point.vz;
            return contacts[contactIndex].key.value;
        }
    }
    return 0;
}
