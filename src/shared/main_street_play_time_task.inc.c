/* Part of the Dryfield main street library; see main_street.h. */

/// Finishes the main-street Ice Bag event once its CAP has ended.
///
/// CAP variant 1 restarts melting at the saved whole play minutes. If both
/// the Ice Bag and Bag of Water collection bits are held, clears the water
/// bit. Owns no work; destroys this task after the inventory changes.
static void _mainStreetFinishIceBagEvent(Task* task)
{
    enum { MAIN_STREET_ICE_BAG_TIMER_VARIANT = 1 };

    if (capIsBusy() == 0) {
        if (capGetVariantKey() == MAIN_STREET_ICE_BAG_TIMER_VARIANT) {
            inventoryResetIceBagTimer();
        }
        if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) != 0 && inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_BAG_OF_WATER) != 0) {
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_BAG_OF_WATER);
        }
        taskKill(task);
    }
}
