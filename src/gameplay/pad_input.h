#ifndef GAMEPLAY_PRIVATE_PAD_INPUT_H
#define GAMEPLAY_PRIVATE_PAD_INPUT_H

/// Publishes port-zero input after gameplay layout mapping and suppression.
///
/// Requires a live player work block when the player task exists. With no player
/// task, leaves input and lock state untouched. Uses saved layout A/B/C (0..2);
/// other layouts publish zero. This pass adds analog movement/run bits only to held buttons;
/// pressed/released samples come from the resident pad state, including UI repeat.
/// Updates automatic menu locking, display holds outside demos and the wheel
/// block countdown once per call, and clears the pad-script halt latch.
/// Menu-unlock delay advances only while automatic menu eligibility holds;
/// ineligible updates renew the input delays.
void padInputUpdate(void);

/// Resets gameplay input suppression and its transition history for room entry.
///
/// Arms an eight-update menu-unlock delay and a four-update ability-wheel block.
/// Clears its tracked display-hold count without releasing display holds.
/// Published input is unchanged until `padInputUpdate` runs with a player task.
void padInputResetSuppression(void);

#endif // GAMEPLAY_PRIVATE_PAD_INPUT_H
