/* Part of the Dryfield main street library; see main_street.h. */

/// Waits for the CAP script to go idle, then records the play time when the
/// script's event key is 1, drops collected bit 0x11A when 0x119 is also held,
/// and ends.
void mainStreetPlayTimeTask(Task* task)
{
    if (capIsBusy() == 0) {
        if (capGetVariantKey() == 1) {
            Gp_MarkPlayTime();
        }
        if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) != 0 && inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_BAG_OF_WATER) != 0) {
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_BAG_OF_WATER);
        }
        taskKill(task);
    }
}
