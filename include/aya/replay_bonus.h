#ifndef AYA_REPLAY_BONUS_H
#define AYA_REPLAY_BONUS_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/ui.h"

/// Replay-bonus item-id table (`0x4E` ids, then a `0xFFFF` terminator).
/// `func_replay_bonus_80117598` tests membership; `func_replay_bonus_80115D60`
/// walks the same list when filling `ReplayBonusItemList`.
extern u16 D_replay_bonus_8011908C[];

/// Double-buffered MDEC strip pixels. `func_replay_bonus_801158C0` LoadImage's
/// one 16-pixel-wide column from `buf[(flip << 5) * (s16)height]`.
extern u8* D_replay_bonus_8011925C;
/// Full image width in pixels; strip count is `width / 16`.
extern s16 D_replay_bonus_80119264;
/// Image height in pixels, also RECT.h of each uploaded strip.
extern u16 D_replay_bonus_80119266;
/// VRAM destination x of strip 0.
extern u16 D_replay_bonus_80119268;
/// VRAM destination y.
extern u16 D_replay_bonus_8011926A;
/// Nonzero while a strip decode is in flight; the out-callback clears it.
extern s16 D_replay_bonus_8011926C;
/// Current strip index; dest x is `vramX + strip * 16`.
extern s16 D_replay_bonus_8011926E;
/// 0/1 selector for the double-buffer; toggled after each LoadImage.
extern u16 D_replay_bonus_80119270;

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

s16  func_replay_bonus_801175D0(UiList* list, ReplayBonusCtx* ctx, s32 index);
void func_replay_bonus_801176A8(DialogPrompt* prompt, UiObject* obj);

#endif
