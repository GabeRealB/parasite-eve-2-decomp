#ifndef AYA_REPLAY_BONUS_H
#define AYA_REPLAY_BONUS_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/ui.h"

/// The list of item ids the replay-bonus screen offers. `func_replay_bonus_80115D60`
/// fills `itemIds` (one `s16` per unlocked item); `func_replay_bonus_801175D0` and
/// `func_replay_bonus_801175F0` read it back, treating an id below 0x100 as an index
/// into `Gp_ItemDescs` and anything above as an index into the high table.
typedef struct ReplayBonusItemList {
    /* 0x00 */ byte pad_0[0x1C];
    /* 0x1C */ s16* itemIds;
} ReplayBonusItemList;

/// The overlay's view of the menu context passed alongside the `UiList`:
/// only the item-list pointer is known.
typedef struct ReplayBonusCtx {
    /* 0x00 */ byte                 pad_0[0x28];
    /* 0x28 */ ReplayBonusItemList* itemList;
} ReplayBonusCtx;

s16 func_replay_bonus_801175D0(UiList* list, ReplayBonusCtx* ctx, s32 index);

#endif
