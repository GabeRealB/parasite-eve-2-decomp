/* Part of the room events library; see room_events.h. */

/// Tests whether the live action-trigger list contains a latched room event.
///
/// Returns 1 for a room action with event ID 255 and nonzero hit, or 0 when
/// none exists, including an empty list. Borrows NULL-terminated collision
/// records without changing the links or consuming the hit.
static inline s32 _roomEventTriggerHit(void)
{
    const WorldCollisionTrigger* trigger;

    for (trigger = gWorldCollisionActionTriggerListHead; trigger != NULL; trigger = trigger->next) {
        if (trigger->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && trigger->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && trigger->hit != 0) {
            return 1;
        }
    }
    return 0;
}
