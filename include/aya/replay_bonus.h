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

/// 16-byte MDEC stream context (`Mem_Calloc(0x10)`). Offset 0 is the VLC
/// table from `func_replay_bonus_80115C68`; `fileId` indexes `D_8006C338`;
/// `x,y,w,h` is the VRAM destination (`x=0x280`, `w=0xF0`, `h=0xB0`,
/// `y = (flip ^ 1) << 8`). Passed as spawnArg2 of the `D_replay_bonus_80118F6C`
/// task (`func_replay_bonus_801159A0`).
typedef struct ReplayBonusStream {
    /* 0x00 */ u16* table;
    /* 0x04 */ u16  fileId;
    /* 0x06 */ u16  unk6;
    /* 0x08 */ s16  x;
    /* 0x0A */ s16  y;
    /* 0x0C */ s16  w;
    /* 0x0E */ s16  h;
} ReplayBonusStream;
STATIC_ASSERT_SIZEOF(ReplayBonusStream, 0x10);

/// TaskDesc for the MDEC stream worker (`func_replay_bonus_801159A0`).
extern TaskDesc D_replay_bonus_80118F6C;
/// Stream phase: 0 idle, 1 running, 2 finished. Spawn is skipped when
/// `(u32)(phase - 1) < 2` (already running or finished).
extern u8 D_replay_bonus_80119225;
/// 0/1 VRAM-Y flip; xor'd when a stream finishes.
extern u8 D_replay_bonus_80119226;
/// Post-stream wait (`0x78` frames) written when the worker is polled dead.
extern u8 D_replay_bonus_80119227;
/// The live `D_replay_bonus_80118F6C` worker, or NULL.
extern Task* D_replay_bonus_80119228;
/// Heap pointer to the current `ReplayBonusStream`.
extern ReplayBonusStream* D_replay_bonus_801192BC;

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
