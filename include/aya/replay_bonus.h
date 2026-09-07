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
/// VLC-decoded MDEC bitstream; `DecDCTin` source and `DecDCTvlc2` dest.
extern u_long* D_replay_bonus_80119260;
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
    /* 0x08 */ u16  x;
    /* 0x0A */ u16  y;
    /* 0x0C */ u16  w;
    /* 0x0E */ u16  h;
} ReplayBonusStream;
STATIC_ASSERT_SIZEOF(ReplayBonusStream, 0x10);

/// One row of the replay-bonus price ladder (`D_replay_bonus_80118F78`,
/// thirteen rows). `spendThreshold` is the running total
/// `func_replay_bonus_80115CA4` has to reach for the row to be the starting
/// index — the last row's is `S32_MAX`, so it never does on its own — and
/// `items` are the three ids `func_replay_bonus_80117484` then offers.
/// `Mc_SaveData.field_934` (also imported as `D_80072A9C`) holds one bit per
/// row; all 13 bits set (`0x1FFF`) means every tier is taken.
typedef struct ReplayBonusShopTier {
    /* 0x0 */ u32  spendThreshold;
    /* 0x4 */ s16  items[3];
    /* 0xA */ byte pad_A[2];
} ReplayBonusShopTier;
STATIC_ASSERT_SIZEOF(ReplayBonusShopTier, 0xC);

extern ReplayBonusShopTier D_replay_bonus_80118F78[13];

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

/// STF credits header (`D_replay_bonus_80119294`). `speed` is the 8.8
/// increment added to the fractional accumulator each frame; `hold0` /
/// `hold1` are multiplied by 6 for `Task::killCountdown`.
typedef struct ReplayBonusStfHdr {
    /* 0x0 */ u16 speed;
    /* 0x2 */ u8  hold0;
    /* 0x3 */ u8  hold1;
} ReplayBonusStfHdr;

/// One STF credits row (`D_replay_bonus_80119298`). `y` is the line's
/// vertical position; the last row's `y - 0x1E0` is the scroll stop.
typedef struct ReplayBonusStfLine {
    /* 0x0 */ void* unk0;
    /* 0x4 */ s32   y;
} ReplayBonusStfLine;
STATIC_ASSERT_SIZEOF(ReplayBonusStfLine, 0x8);

/// STF row-table header, followed immediately by `count` credits rows.
typedef struct ReplayBonusStfTable {
    /* 0x0 */ s32 count;
} ReplayBonusStfTable;
STATIC_ASSERT_SIZEOF(ReplayBonusStfTable, 0x4);

/// STF file offsets are relocated in place when `field_C` is positive.
typedef struct ReplayBonusStfFile {
    /* 0x00 */ char magic[4];
    /* 0x04 */ s32  unk4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s32  field_10;
    /* 0x14 */ s32  field_14;
} ReplayBonusStfFile;
STATIC_ASSERT_SIZEOF(ReplayBonusStfFile, 0x18);

/// TaskDesc table spawned from the credits task (hold / fade / stream workers).
extern TaskDesc D_replay_bonus_8011922C;
/// Relocated STF header.
extern ReplayBonusStfHdr* D_replay_bonus_80119294;
/// Relocated STF row array; `D_replay_bonus_801192A0` is the count.
extern ReplayBonusStfLine* D_replay_bonus_80119298;
extern s32                 D_replay_bonus_801192A0;
/// Integer scroll Y; starts at `-0x1E0`.
extern s32 D_replay_bonus_801192A4;
/// 8.8 fractional accumulator for the scroll.
extern s32 D_replay_bonus_801192A8;
/// Frame counter incremented while the credits draw.
extern s32 D_replay_bonus_801192B0;

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

/// Totals block at `D_replay_bonus_80119274`. `field_4` is the BP drawn on the
/// Balance screen; `field_C` is the BP drawn for NEXT REPLAY BONUS. Offset 8 is
/// also `D_replay_bonus_8011927C`, the EXP override for that preview.
/// Offset 0x10 is the separately-named `D_replay_bonus_80119284` shop row;
/// offset 0x14 is `D_replay_bonus_80119288` (EXTRA BONUS), added to `field_C`
/// as `field_14` so the sum uses the totals base.
typedef struct ReplayBonusTotals {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s32 field_14;
} ReplayBonusTotals;

extern ReplayBonusTotals D_replay_bonus_80119274;
extern s32               D_replay_bonus_8011927C;

extern UiList D_replay_bonus_80119130;
extern s32    D_80067634;
extern s8     D_80072177;
extern u8     D_replay_bonus_80115774[];
extern u8     D_replay_bonus_80115784[];
extern u8     D_replay_bonus_80115790[];
extern u8     D_replay_bonus_8011579C[];

u16* func_replay_bonus_80115C68(void);
s32  func_replay_bonus_80115CA4(void);
void func_replay_bonus_80115D60(UiList* list, ReplayBonusCtx* ctx);
void func_replay_bonus_80115ED0(Task* arg0);
s32  func_replay_bonus_801173A8(void);
void func_replay_bonus_80117E04(void);
s32  func_replay_bonus_80118B6C(ReplayBonusStfFile* file, s32 index);
void func_replay_bonus_80118F00(s32 arg0);
s16  func_replay_bonus_80117484(s32 arg0, s32 arg1);
s16  func_replay_bonus_801175D0(UiList* list, ReplayBonusCtx* ctx, s32 index);
s32  func_replay_bonus_801175F0(UiList* list, ReplayBonusCtx* ctx);
void func_replay_bonus_801176A8(DialogPrompt* prompt, UiObject* obj);

#endif
