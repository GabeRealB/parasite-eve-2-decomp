/* Shared by both units of replay_bonus. */

/// Returns the BP balance plus item credit from the first visible row to the list's end.
///
/// The owner task's work holds the list's borrowed `s16` item ids. The row
/// range must be nonnegative and fit that storage; each id must satisfy
/// `_replayBonusItemBp`'s catalog bounds. Starting at zero computes the full
/// clear total; scrolling backward progressively reveals the displayed total.
/// Caps at eight decimal digits without changing the player's balance.
static inline s32 _replayBonusSumTotalBp(const UiList* list, const UiObject* object)
{
    enum { REPLAY_BONUS_MAX_DISPLAYED_TOTAL_BP = 99999999 };
    s32                 itemIndex;
    s32                 totalBp;
    const PlayerStatus* playerStatus;

    playerStatus = &gPlayerStatus;
    totalBp      = 0;
    for (itemIndex = list->firstVisibleItemIndex.signedValue; itemIndex < list->itemCount; itemIndex++) {
        const s16* itemIds = object->owner->work;

        totalBp += _replayBonusItemBp(itemIds[itemIndex]);
    }
    totalBp += playerStatus->bp;
    if (totalBp > REPLAY_BONUS_MAX_DISPLAYED_TOTAL_BP) {
        totalBp = REPLAY_BONUS_MAX_DISPLAYED_TOTAL_BP;
    }
    return totalBp;
}
