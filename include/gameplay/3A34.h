#ifndef GAMEPLAY_3A34_H
#define GAMEPLAY_3A34_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/268.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"

/// Singly-linked node unlinked by `func_800DAB38` / linked by `func_800DABEC`.
/// `field_6 == 1` means the node is on the `D_80115268` list.
/// `func_800DAC54` returns a 2-bit mask of `D_80115760[]` slots whose
/// `actor->field_90C` is this node.
/// `func_800DACAC` assigns the node to `D_80115760[0]->actor->field_90C`,
/// clears the previous node's `field_5`, sets this node's `field_5 = 1`,
/// and clears `field_4` bit 0.
/// `func_800DACF8` is the inverse: it nulls any `D_80115760[]` slot whose
/// `field_90C` is this node, clears `field_5`, and sets `field_4` bit 0.
typedef struct _GpLinkNode {
    /* 0x00 */ struct _GpLinkNode* next;
    /* 0x04 */ u8                  field_4;
    /* 0x05 */ u8                  field_5;
    /* 0x06 */ u8                  field_6;
} GpLinkNode;
STATIC_ASSERT_SIZEOF(GpLinkNode, 0x8);

/// Linked object used as a list head/node by the 3A34 pair/filter helpers.
/// `next` is at 0x0, `prev` at 0x4, and `flags` at 0x1E. Bit 0x8 means the
/// node is on the `D_8010FA8C` list (set by `func_800E15AC`, cleared by
/// `func_800E1638`, keeping bits 0x7). `func_8010C980` fills `field_8` /
/// `field_C` / the 0x10 SVECTOR / `field_18` / `field_1C` and ORs `flags`
/// with 0x8000 after linking. Embedded as 0x20-byte nodes in `GameActor`
/// (`field_AC` / `field_CC` / `field_EC` / `field_10C` / `field_12C`).
/// Full object size is not known for other list users.
typedef struct _GpObj {
    /* 0x00 */ struct _GpObj* next;
    /* 0x04 */ struct _GpObj* prev;
    /* 0x08 */ void*          field_8;
    /* 0x0C */ GpRec18*       field_C;
    /* 0x10 */ s16            field_10;
    /* 0x12 */ s16            field_12;
    /* 0x14 */ s16            field_14;
    /* 0x16 */ byte           pad_16[2];
    /* 0x18 */ s32            field_18;
    /* 0x1C */ s16            field_1C;
    /* 0x1E */ u16            flags;
} GpObj;
STATIC_ASSERT_SIZEOF(GpObj, 0x20);

/// 4-byte table entry packed by `func_800E2C40` / `func_800E2BF8` as
/// `(field_0 & 0xFFF) | ((field_2 & 0xF) << 12) | 0x40000`.
typedef struct _GpU16Pair {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpU16Pair;
STATIC_ASSERT_SIZEOF(GpU16Pair, 0x4);

/// 4-byte table entry at `D_80114028`. `func_800E2CD4(idx, 0)` returns
/// `field_0` for index `(u16)idx`.
typedef struct _GpRec4 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpRec4;
STATIC_ASSERT_SIZEOF(GpRec4, 0x4);

/// 6-byte table entry at `D_80114054`. `func_800E2CD4(idx, 1)` returns
/// `field_0` for index `(u16)idx`.
typedef struct _GpRec6 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
} GpRec6;
STATIC_ASSERT_SIZEOF(GpRec6, 0x6);

/// 10-byte table entry at `D_80113390`. Selected when the id's 0x8000 bit
/// is clear. `func_800E2D3C` / `func_800E2D90` / `func_800E3194` return
/// `field_4` / `field_6` / `field_8` for index `id & 0x7F`.
typedef struct _GpRec10 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ u16 field_8;
} GpRec10;
STATIC_ASSERT_SIZEOF(GpRec10, 0xA);

/// 12-byte ranked slot inserted by `func_800D9B9C`. That helper walks a
/// 4-entry table (indices 0..3) from `arg4` toward 0 and keeps slots in
/// descending `field_4` order. A non-positive key is ignored. When the
/// new key is larger than slot `arg4`, that slot is copied to `arg4+1`
/// (if `arg4 < 3`) and the search recurses; otherwise the record is
/// stored at `arg4+1` when there is room.
typedef struct _GpRec12 {
    /* 0x0 */ s32 field_0; // payload from arg2
    /* 0x4 */ s32 field_4; // descending sort key (arg1)
    /* 0x8 */ s32 field_8; // payload from arg3
} GpRec12;
STATIC_ASSERT_SIZEOF(GpRec12, 0xC);

/// 16-byte table entry at `D_8011398C`. Selected when the id's 0x8000 bit
/// is set. `func_800E2D3C` / `func_800E2D90` / `func_800E3194` return
/// `field_A` / `field_C` / `field_E` for index `id & 0x7F`.
typedef struct _GpRec16 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ u16 field_8;
    /* 0xA */ u16 field_A;
    /* 0xC */ u16 field_C;
    /* 0xE */ u16 field_E;
} GpRec16;
STATIC_ASSERT_SIZEOF(GpRec16, 0x10);

/// Table source pointed to by `GpObj50.field_50`. `field_0` is the
/// `GpU16Pair` array packed by `func_800E2BF8`. Nearby helpers also
/// load bytes at +0xB / +0xD of this object.
typedef struct _GpPairSrc {
    /* 0x00 */ GpU16Pair* field_0;
} GpPairSrc;
STATIC_ASSERT_SIZEOF(GpPairSrc, 0x4);

/// Object whose pointer at 0x50 is a `GpPairSrc*` used by `func_800E2BF8`.
/// Same object family as `GpObj4C` (flags at 0x4C).
typedef struct _GpObj50 {
    /* 0x00 */ byte       pad_0[0x50];
    /* 0x50 */ GpPairSrc* field_50;
} GpObj50;
STATIC_ASSERT_SIZEOF(GpObj50, 0x54);

/// 0x18-byte slot in the table at `GpObj54.field_54`. Occupied when the
/// first word's low 2 bits equal 1. `func_800E1C58` claims the first free
/// slot: payload in `field_4`, clears `field_2` / `field_8` / `field_A` /
/// `field_C` / `field_10` / `field_12` / `field_14`, ORs bit 0 into
/// `field_0`, and increments `D_801153F0.field_5`.
typedef struct _GpSlot18 {
    /* 0x00 */ u16   field_0;
    /* 0x02 */ s16   field_2;
    /* 0x04 */ void* field_4;
    /* 0x08 */ s16   field_8;
    /* 0x0A */ s16   field_A;
    /* 0x0C */ s16   field_C;
    /* 0x0E */ s16   field_E;
    /* 0x10 */ s16   field_10;
    /* 0x12 */ s16   field_12;
    /* 0x14 */ s16   field_14;
    /* 0x16 */ s16   field_16;
} GpSlot18;
STATIC_ASSERT_SIZEOF(GpSlot18, 0x18);

/// Sparse overlay whose pointer at 0x54 is a `GpSlot18` table walked by
/// `func_800E1C58`. Same object family as `GpObj50` / `GpObj4C`.
typedef struct _GpObj54 {
    /* 0x00 */ byte      pad_0[0x54];
    /* 0x54 */ GpSlot18* field_54;
} GpObj54;
STATIC_ASSERT_SIZEOF(GpObj54, 0x58);

/// 8-byte record in tables pointed to by `D_8010CBA4`. Indexed 1-based
/// by `GameSessionFrom4.field_1`. `func_800D9C64` returns the record
/// (or NULL). `func_800D9654` returns `field_0`. `func_800D957C` walks
/// `field_4` as a nested 8-byte table, falling back to `D_8010F9E4`.
typedef struct _GpCbA4Rec {
    /* 0x0 */ s32   field_0;
    /* 0x4 */ void* field_4;
} GpCbA4Rec;
STATIC_ASSERT_SIZEOF(GpCbA4Rec, 8);

/// Three packed `SVECTOR3`s filled by `func_800D9C3C`. Each vector's
/// components are set to the same s16 argument.
typedef struct _GpSVec3x3 {
    /* 0x00 */ SVECTOR3 field_0;
    /* 0x06 */ SVECTOR3 field_6;
    /* 0x0C */ SVECTOR3 field_C;
} GpSVec3x3;
STATIC_ASSERT_SIZEOF(GpSVec3x3, 0x12);

/// Object whose pointer at 0x20 is a `MATRIX*` whose translation
/// `t[0]/t[1]/t[2]` is set by `func_800D9550` from three s16 args.
typedef struct _GpObj20 {
    /* 0x00 */ byte    pad_0[0x20];
    /* 0x20 */ MATRIX* field_20;
} GpObj20;
STATIC_ASSERT_SIZEOF(GpObj20, 0x24);

/// Object whose word at 0x38 is returned by `func_800D9788`. Adjacent light
/// helpers load 0x38/0x3C/0x40 as three s32s and take `&obj->field_38` as a
/// `VECTOR*`.
typedef struct _GpObj38 {
    /* 0x00 */ byte pad_0[0x38];
    /* 0x38 */ s32  field_38;
    /* 0x3C */ s32  field_3C;
    /* 0x40 */ s32  field_40;
} GpObj38;
STATIC_ASSERT_SIZEOF(GpObj38, 0x44);

/// Sparse overlay of the same light object as `GpObj38`. `func_800D9718`
/// treats `field_44` as a room-id filter against `Game_Session->field_4`
/// (0 = any room), writes `0x1000` (GTE ONE) to `field_4A`, and returns a
/// weighted `field_50/52/54` luminance. Nearby handwritten light helpers
/// also `lh` `field_4A` into GTE IR0 and load 0x50 as three halfwords.
typedef struct _GpObj44 {
    /* 0x00 */ byte pad_0[0x44];
    /* 0x44 */ s16  field_44;
    /* 0x46 */ byte pad_46[4];
    /* 0x4A */ s16  field_4A;
    /* 0x4C */ byte pad_4C[4];
    /* 0x50 */ s16  field_50;
    /* 0x52 */ s16  field_52;
    /* 0x54 */ s16  field_54;
} GpObj44;
STATIC_ASSERT_SIZEOF(GpObj44, 0x56);

/// Sparse overlay whose signed halfword at 0x40 is added (unsigned-clamped by
/// `arg2`) into `D_801153F0.field_14` by `func_800E2C78` when
/// `(arg1 & 0x7F)` is 0x19..0x1B.
typedef struct _GpObj40 {
    /* 0x00 */ byte pad_0[0x40];
    /* 0x40 */ s16  field_40;
} GpObj40;
STATIC_ASSERT_SIZEOF(GpObj40, 0x42);

/// Object whose flags byte at 0x4C is OR'd by `func_800E3008`. Nearby
/// helpers treat 0x4C as a flag field (bits 0x1, 0x2, 0x4). `field_4E`
/// packs two 2-bit modes (current in bits 0-1, previous in bits 2-3)
/// plus a high-nibble flag; `func_800D930C` rotates the current mode
/// into the previous slot and starts `field_4F` as a 0x10 blend timer.
/// `next` and signed `field_4B` are the `D_8011556C` list walked by
/// `func_800E0B08`, which clears a non-zero `field_4B`. `func_800E1BF0`
/// walks the same list and, on a pending `field_4B`, copies `field_46` /
/// `field_48` / `field_49` to its out-params and sets `D_80115424`. The
/// same node type is the `D_80115554` list walked by `func_800E1B80`: a
/// pending `field_4B` copies `field_49` into `Mc_SaveData.field_4` when
/// `field_48` matches `Game_Session->field_4`.
typedef struct _GpObj4C {
    /* 0x00 */ struct _GpObj4C* next;
    /* 0x04 */ byte             pad_4[0x42];
    /* 0x46 */ u16              field_46;
    /* 0x48 */ u8               field_48;
    /* 0x49 */ u8               field_49;
    /* 0x4A */ u8               field_4A;
    /* 0x4B */ s8               field_4B;
    /* 0x4C */ u8               field_4C;
    /* 0x4D */ byte             pad_4D;
    /* 0x4E */ u8               field_4E;
    /* 0x4F */ u8               field_4F;
} GpObj4C;
STATIC_ASSERT_SIZEOF(GpObj4C, 0x50);

/// Sparse overlay of the same object family as `GpObj4C` (flags at 0x4C).
/// `func_800E301C` ORs bit 0x2 into `field_4C`, clears `field_58` /
/// `field_5B`, and writes `field_5D` from `D_80114C08.field_0 % 10` when
/// the id has the 0x8000 bit and low 6 bits != 0x31.
typedef struct _GpObj5D {
    /* 0x00 */ byte pad_0[0x4C];
    /* 0x4C */ u8   field_4C;
    /* 0x4D */ byte pad_4D[0xB];
    /* 0x58 */ u8   field_58;
    /* 0x59 */ byte pad_59[2];
    /* 0x5B */ u8   field_5B;
    /* 0x5C */ byte pad_5C;
    /* 0x5D */ u8   field_5D;
} GpObj5D;
STATIC_ASSERT_SIZEOF(GpObj5D, 0x5E);

/// 0x4C list node appended to `D_8010FAB0[index]` by `func_800E1688` and
/// unlinked by `func_800E1708`. `func_800E1758` empties the whole list.
/// `field_4A` bit 0x20 means the node is on that list (cleared on unlink,
/// keeping bits 0x87); bit 0x80 marks the last element of an array walked
/// at +0x4C. Callers also store `D_80070F10` at +0x8 and OR bit 0x40 into
/// `field_4A`.
typedef struct _GpObj4A {
    /* 0x00 */ struct _GpObj4A* next;
    /* 0x04 */ struct _GpObj4A* prev;
    /* 0x08 */ byte             pad_8[0x42];
    /* 0x4A */ u8               field_4A;
    /* 0x4B */ byte             pad_4B;
} GpObj4A;
STATIC_ASSERT_SIZEOF(GpObj4A, 0x4C);

/// 0x3C list node appended to `D_8010FAB8[index]` by `func_800E17B4` and
/// unlinked by `func_800E1834`. `func_800E1884` empties the whole list.
/// `field_3A` bit 0x20 means the node is on that list (cleared on unlink,
/// keeping bits 0x87). Same link/flag layout as `GpObj4A`, with the flag
/// byte at 0x3A instead of 0x4A. Full object size is not known yet.
typedef struct _GpObj3A {
    /* 0x00 */ struct _GpObj3A* next;
    /* 0x04 */ struct _GpObj3A* prev;
    /* 0x08 */ byte             pad_8[0x32];
    /* 0x3A */ u8               field_3A;
    /* 0x3B */ byte             pad_3B;
} GpObj3A;
STATIC_ASSERT_SIZEOF(GpObj3A, 0x3C);

/// Global at `D_801153F0`. `field_0` is a state byte (1 if first set by
/// `func_800DB4E0`; 2 when the last `field_6` ref is released). `field_1`
/// is an alternate-active flag (`func_800A7508` / `func_800A7CB0` /
/// `func_800A7CF4` / `func_800A7D54`); last-ref release sets it to 0x3C.
/// `field_2` is a bitset (`func_800DB500` sets bit `arg0 - 1` when
/// `arg0 != 0`). `field_3` is cleared with `field_2` on last-ref release
/// (also written as `D_801153F3` by `func_800DB530`). `field_4` is also
/// `D_801153F4`. `field_5` is a u8 count incremented by `func_800E1C58`
/// when it claims a `GpSlot18`. `field_6` is a u16
/// refcount incremented by `func_800DB53C` and decremented by
/// `func_800DB558` / `func_800DB630` / `func_800DB6B4`. Last-ref
/// release in `func_800DB630` also clears words at 0x8 / 0xC / 0x10.
/// `func_800E2C78` adds into
/// `field_14` when `(arg1 & 0x7F)` is 0x19..0x1B.
typedef struct _GpStateF0 {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u8   field_2;
    /* 0x03 */ u8   field_3;
    /* 0x04 */ u8   field_4;
    /* 0x05 */ u8   field_5;
    /* 0x06 */ u16  field_6;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s32  field_10;
    /* 0x14 */ s32  field_14;
} GpStateF0;
STATIC_ASSERT_SIZEOF(GpStateF0, 0x18);

/// 0xC slot in the 32-entry table at `D_80115270`. `func_800DAF98` clears
/// `field_0` / `field_4` / `field_6`. `func_800DA6E8` binds `field_0` and
/// bumps `field_4`; `func_800DA7B8` treats `field_6` as a countdown and
/// stores projected screen XY at 0x8.
typedef struct _GpSlot70 {
    /* 0x0 */ void* field_0;
    /* 0x4 */ s16   field_4;
    /* 0x6 */ s16   field_6;
    /* 0x8 */ s16   field_8;
    /* 0xA */ s16   field_A;
} GpSlot70;
STATIC_ASSERT_SIZEOF(GpSlot70, 0xC);

/// Pending flags written by `func_800D5B14` and consumed by `func_800CE294`.
/// `D_8010F888 == 1` requests `func_800AC464(..., 0x402, ...)`.
extern s32 D_8010F888;

/// Signed pending item id consumed by `func_800D68C4`. `func_800D5B14`
/// stores the id, or its negation for the second `GpItemSlot` pair.
/// `func_800CE294` also consumes it (with `D_8010F890`) via `func_801088D4`.
extern s32 D_8010F88C;

/// Non-zero when `D_8010F88C` should be applied by `func_800CE294`.
extern s32 D_8010F890;

/// Pending id consumed by `func_800CE294`; `0x3E` also calls `func_8010A1B0`.
extern s32 D_8010F894;

/// Head of the `GpLinkNode` list walked by `func_800DAB38` / `func_800DABEC`
/// / `func_800A784C`.
extern GpLinkNode* D_80115268;

/// 32-entry marker/slot table cleared by `func_800DAF98`.
extern GpSlot70 D_80115270[0x20];

/// Per-stage pointer table. Index is `GameSessionFrom4.field_3 - 1`.
/// Each entry is an array of `GpCbA4Rec*`, indexed by `field_2 - 1`.
extern GpCbA4Rec** D_8010CBA4[];

/// Default 8-byte record copied by `func_800D9CE8`. Also the fallback
/// pointer returned by `func_800D957C` when a table lookup fails.
extern GBytes8 D_8010F9E4;

/// Flag set by `func_800D94B8` when an override SVECTOR is stored at
/// `D_80114F20`. Cleared when that function is called with NULL, and
/// also by `func_800D9D18`.
extern u8 D_80114F18;

/// Override SVECTOR copied by `func_800D94B8` from its argument.
extern SVECTOR D_80114F20;

/// Flag set by `func_800D9504` when an override SVECTOR is stored at
/// `D_80115258`. Cleared when that function is called with NULL, and
/// also by `func_800D9D18`.
extern u8 D_80115250;

/// Override SVECTOR copied by `func_800D9504` from its argument.
extern SVECTOR D_80115258;

/// 8.8 fixed-point pair lerped toward projected screen coords by
/// `func_800D9DFC`. Reset to `0xFFF00000` by `func_800DAFD0`.
extern s32 D_8010F9EC;
extern s32 D_8010F9F0;

/// Two-entry table of `GpObj4A` list heads. `func_800E1688` appends to
/// `D_8010FAB0[index]`; `func_800E1758` walks and clears that list.
extern GpObj4A* D_8010FAB0[2];

/// One-entry table of `GpObj3A` list heads. `func_800E17B4` appends to
/// `D_8010FAB8[index]`; `func_800E1884` walks and clears that list.
extern GpObj3A* D_8010FAB8[1];

extern GpStateF0 D_801153F0;
extern u8        D_801153F3;
extern u8        D_801153F4;
/// Head of the `GpObj4C` list walked by `func_800E1B80`.
extern GpObj4C* D_80115554;
extern GpObj4C* D_8011556C;
extern GpObj* D_80115570;
extern GpObj* D_80115574;
extern GpObj* D_80115578;
extern GpObj* D_8011557C;
extern GpObj* D_80115580;
extern GpObj* D_80115588;
extern GpObj* D_8011558C;
extern GpObj* D_80115590;
extern u8     D_80115598;
/// Set to 1 by `func_800E1BF0` when a pending `D_8011556C` node is found;
/// `func_800DB72C` then calls `func_800E0B08` to clear those flags.
extern s32    D_80115424;
extern s32    D_80115448;

/// 4-byte records selected by `func_800E2CD4(..., 0)`.
extern GpRec4 D_80114028[];

/// 6-byte records selected by `func_800E2CD4(..., 1)`.
extern GpRec6 D_80114054[];

/// 10-byte records selected by `func_800E3194` when the id's 0x8000 bit is
/// clear. Indexed by `id & 0x7F`.
extern GpRec10 D_80113390[];

/// 16-byte records selected by `func_800E3194` when the id's 0x8000 bit is
/// set. Indexed by `id & 0x7F`.
extern GpRec16 D_8011398C[];

/// "Weapon" string drawn by `func_800D6AA4`.
extern char D_80097454[];

s32        func_800D68C4(s32 arg0);
GpItemRec* func_800D6910(s32 arg0);
GpItemRec* func_800D6994(s32 arg0);
GpItemRec* func_800D6A24(s32 arg0, GpItemScan* arg1);
void       func_800D6AA4(Task* arg0);
void  func_800D8684(Task* arg0);
void  func_800D930C(GpObj4C* arg0, s32 arg1);
s32   func_800D9340(GpObj38* arg0);
s32   func_800D937C(GpObj38* arg0);
void  func_800D94B8(SVECTOR* arg0);
void  func_800D9504(SVECTOR* arg0);
void  func_800D9550(GpObj20* arg0, s16 arg1, s16 arg2, s16 arg3);
s32   func_800D9618(void);
s32   func_800D9654(GameSessionFrom4* arg0);
void  func_800D96C8(Task* arg0);
s32   func_800D9718(GpObj44* arg0);
s32   func_800D9788(GpObj38* arg0);
void  func_800D9B9C(GpRec12* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void  func_800D9D18(Task* arg0);
void  func_800D9C3C(GpSVec3x3* arg0, s16 arg1, s16 arg2, s16 arg3);
GpCbA4Rec* func_800D9C64(GameSessionFrom4* arg0);
void  func_800D9CC8(Task* arg0);
void  func_800D9CE8(GBytes8* arg0);
void  func_800D9DFC(void);
void  func_800DAB38(GpLinkNode* node);
void  func_800DABEC(GpLinkNode* node);
s32   func_800DAC54(GpLinkNode* arg0);
void  func_800DACAC(GpLinkNode* node);
void  func_800DACF8(GpLinkNode* node);
void* func_800DA2A0(GpActorWork* arg0, VECTOR3* pos, s32 arg2);
void* func_800DAD54(GpActorWork* arg0);
void* func_800DAD78(GpActorWork* arg0);
void* func_800DADE4(GpActorWork* arg0, VECTOR3* pos);
void  func_800DAF98(void);
void  func_800DAFD0(void);
void  func_800DB0D8(void);
void  func_800DB4E0(s32 arg0);
void  func_800DB500(s32 arg0);
void  func_800DB530(s32 arg0);
void  func_800DB53C(void);
void  func_800DB630(void);
void  func_800DB6B4(void);
void  func_800DB72C(void);
void func_800DB900(GpObj* node);
void func_800DC528(GpObj* node);
void func_800DCB80(GpObj* node);
void func_800DD940(GpObj* node);
void func_800DDDF8(GpObj* node);
void func_800E0414(GpObj* a, GpObj* b);
void func_800E0540(GpObj* node);
void func_800E0608(GpObj* node, s32 mask, s32 match);
void func_800E06AC(GpObj* node, s32 mask, s32 match);
s32  func_800E076C(void);
void func_800E0B08(void);
void func_800E15AC(s32 arg0, GpObj* arg1);
void func_800E1638(GpObj* node);
void func_800E1708(s32 arg0, GpObj4A* arg1);
void func_800E1758(s32 arg0);
void func_800E1834(s32 arg0, GpObj3A* arg1);
void func_800E1884(s32 arg0);
void func_800E18E0(GpRec18* arg0, s32 arg1, s32 arg2);
s32  func_800E19B8(GpRec18* arg0, s32 arg1);
s32  func_800E1A1C(GpRec18* arg0, s32 arg1);
void func_800E1A6C(GpRec18* arg0);
s32  func_800E1ACC(u8* arg0);
s32  func_800E1B24(s32 arg0);
void func_800E1B80(void);
s32  func_800E1BF0(u16* arg0, u8* arg1, u8* arg2);
void func_800E1C58(GpObj54* arg0, void* arg1);
s32  func_800E2438(s32 arg0, s32 arg1, s32* arg2, s32 arg3);
s32  func_800E2BF8(GpObj50* arg0, s32 arg1);
s32  func_800E2C40(GpU16Pair* arg0, s32 arg1);
void func_800E2C78(GpObj40* arg0, s32 arg1, s32 arg2);
s32  func_800E2CD4(s32 arg0, s32 arg1);
s32  func_800E2D3C(s32 arg0);
s32  func_800E2D90(s32 arg0);
void func_800E3008(GpObj4C* arg0);
void func_800E301C(GpObj5D* arg0, s32 arg1);
s32  func_800E3194(s32 arg0);
void func_800E337C(Task* arg0);
void func_8010154C(void);

#endif // GAMEPLAY_3A34_H
