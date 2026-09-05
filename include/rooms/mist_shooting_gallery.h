#ifndef ROOMS_MIST_SHOOTING_GALLERY_H
#define ROOMS_MIST_SHOOTING_GALLERY_H

#include "common.h"

#include "main/task.h"
#include "main/text.h"

#include <psyq/libgte.h>

/// Per-run state of the Mist shooting gallery mini-game: a 0x24-byte
/// `Mem_Calloc` allocation that `func_mist_shooting_gallery_80182B1C` stores at
/// `Task::idMap` of the gallery's controller task, which it also publishes in
/// `D_mist_shooting_gallery_8018E0C4` so the round scripts can reach it without
/// a task pointer. `difficulty` is seeded from the low nibble of the task's
/// `spawnArg1` and gates the scoring rules (`< 3` and `< 2` branches).
typedef struct MistShootingGalleryWork {
    /* 0x00 */ u16  field_00;
    /* 0x02 */ u16  field_02;
    /* 0x04 */ u16  field_04;
    /* 0x06 */ u16  field_06;
    /* 0x08 */ s16  field_08;
    /* 0x0A */ s16  field_0A;
    /* 0x0C */ s16  field_0C;
    /* 0x0E */ u8   field_0E;
    /* 0x0F */ byte pad_0F[0xD];
    /* 0x1C */ u8   difficulty;
    /* 0x1D */ u8   field_1D;
    /* 0x1E */ u8   field_1E;
    /* 0x1F */ u8   field_1F;
    /* 0x20 */ u8   field_20;
    /* 0x21 */ u8   field_21;
    /* 0x22 */ u8   field_22;
    /* 0x23 */ byte pad_23[0x1];
} MistShootingGalleryWork;
STATIC_ASSERT_SIZEOF(MistShootingGalleryWork, 0x24);

/// The five round scripts of the gallery mini-game, indexed by
/// `MistShootingGalleryWork::difficulty`. `func_mist_shooting_gallery_80184A14`
/// copies the whole table onto its stack before dispatching through it.
typedef struct MistShootingGalleryRounds {
    /* 0x00 */ void (*rounds[5])(void);
} MistShootingGalleryRounds;
STATIC_ASSERT_SIZEOF(MistShootingGalleryRounds, 0x14);

/// 0xC-byte spawn record for `func_mist_shooting_gallery_80184CD0`. The round
/// scripts index a table of these (reached through
/// `D_mist_shooting_gallery_80186900`) with a 12-byte stride. `idLo` / `idHi`
/// pack into the `Task_SpawnFromTable` arg2 the enemy is spawned with, and
/// `x` / `y` / `z` are written to the spawned object's
/// `GpCoordPose::coord.t[0..2]`.
typedef struct MistShootingGallerySpawn {
    /* 0x0 */ u16 field_00;
    /* 0x2 */ s16 idLo;
    /* 0x4 */ s16 idHi;
    /* 0x6 */ s16 x;
    /* 0x8 */ s16 y;
    /* 0xA */ s16 z;
} MistShootingGallerySpawn;
STATIC_ASSERT_SIZEOF(MistShootingGallerySpawn, 0xC);

/// The four bonus-mode blurbs shown by the gallery's help panel, indexed by
/// `D_80072177`. Like the round table, `func_mist_shooting_gallery_8017FAE8`
/// copies the whole thing onto its stack before indexing it.
typedef struct MistShootingGalleryModeTexts {
    /* 0x0 */ u8* text[4];
} MistShootingGalleryModeTexts;
STATIC_ASSERT_SIZEOF(MistShootingGalleryModeTexts, 0x10);

/// One row of the gallery's DATA panel. `gauge` picks the bar string out of
/// `MistShootingGalleryGauges`; `label` is the word printed beside it
/// ("EASY", "GOOD", "RICH", ...).
typedef struct MistShootingGalleryRating {
    /* 0x0 */ s32 gauge;
    /* 0x4 */ u8* label;
} MistShootingGalleryRating;
STATIC_ASSERT_SIZEOF(MistShootingGalleryRating, 0x8);

/// The four ratings one DATA row can show, indexed by `Mc_SaveData::field_F`
/// (the difficulty the save runs at). `func_mist_shooting_gallery_8017F128`
/// copies a whole table onto its stack before indexing it, like the round and
/// mode tables above.
typedef struct MistShootingGalleryRatings {
    /* 0x00 */ MistShootingGalleryRating entries[4];
} MistShootingGalleryRatings;
STATIC_ASSERT_SIZEOF(MistShootingGalleryRatings, 0x20);

/// The six gauge strings the DATA panel draws through
/// `MistShootingGalleryRating::gauge`; also stack-copied before use.
typedef struct MistShootingGalleryGauges {
    /* 0x00 */ u8* bars[6];
} MistShootingGalleryGauges;
STATIC_ASSERT_SIZEOF(MistShootingGalleryGauges, 0x18);

/// One row of the gallery's RESULT panel: the points one kill of that target
/// is worth and the name printed beside it ("Red Target", "Crow", ...).
/// `func_mist_shooting_gallery_8017E234` walks the 13-entry table
/// `D_mist_shooting_gallery_80184F98` in step with the per-target kill counts
/// in `MistShootingGalleryWork::pad_0F`.
typedef struct MistShootingGalleryTarget {
    /* 0x0 */ s32 points;
    /* 0x4 */ u8* name;
} MistShootingGalleryTarget;
STATIC_ASSERT_SIZEOF(MistShootingGalleryTarget, 0x8);

/// One position in a gallery course table: three halfwords plus the padding
/// halfword that rounds the record up to an 8-byte stride.
/// `func_mist_shooting_gallery_801801E4` copies these component by component
/// and never touches `pad`.
typedef struct MistShootingGalleryPos {
    /* 0x0 */ u16 x;
    /* 0x2 */ u16 y;
    /* 0x4 */ u16 z;
    /* 0x6 */ u16 pad;
} MistShootingGalleryPos;
STATIC_ASSERT_SIZEOF(MistShootingGalleryPos, 0x8);

/// The 0xC-byte record `MistShootingGalleryLayout::links` points at, one per
/// entry of `MistShootingGalleryLayout::positions`.
/// `func_mist_shooting_gallery_801801E4` copies it whole (as a 2-byte-aligned
/// block move, hence the `lwl`/`lwr` pairs), while
/// `func_mist_shooting_gallery_80180390` walks `field_00` as a four-entry
/// array and then reads `field_08` and `field_0A` separately.
typedef struct MistShootingGalleryLink {
    /* 0x0 */ u16 field_00[4];
    /* 0x8 */ u16 field_08;
    /* 0xA */ u16 field_0A;
} MistShootingGalleryLink;
STATIC_ASSERT_SIZEOF(MistShootingGalleryLink, 0xC);

/// The geometry of one shooting-gallery course. The overlay holds two
/// templates (`D_mist_shooting_gallery_80185198` and
/// `D_mist_shooting_gallery_801851F8`) plus the live copy
/// `D_mist_shooting_gallery_80189968` that the room actually reads;
/// `func_mist_shooting_gallery_801801E4` seeds the live copy from the first
/// template, and `func_mist_shooting_gallery_80180390` fans the live copy out
/// into the per-target work records. `positions` holds three entries,
/// `targets` eight, and `links` three — one per position.
typedef struct MistShootingGalleryLayout {
    /* 0x00 */ s32                      field_00;
    /* 0x04 */ MistShootingGalleryPos*  positions;
    /* 0x08 */ MistShootingGalleryPos*  targets;
    /* 0x0C */ MistShootingGalleryLink* links;
    /* 0x10 */ void*                    field_10;
    /* 0x14 */ s32                      field_14;
    /* 0x18 */ s32                      field_18;
    /* 0x1C */ s16                      field_1C;
    /* 0x1E */ s16                      field_1E;
    /* 0x20 */ s16                      field_20;
    /* 0x22 */ s16                      field_22;
} MistShootingGalleryLayout;
STATIC_ASSERT_SIZEOF(MistShootingGalleryLayout, 0x24);

/// 0x20-byte scratch block taken from `G_SCRATCH_HEAD` by the gallery's tracer
/// draw helper (`func_mist_shooting_gallery_801826C4`).
///
/// `vec` is the effect coordinate's world position (`workm.t`) truncated to
/// s16; it and the caller's endpoint `SVECTOR` are projected by one `RTPS`
/// each, filling `sxy0` / `sxy1` through `gte_stsxy`. `flag` is `gte_stflg` of
/// whichever projection just ran - both are tested, so an off-screen endpoint
/// drops the whole beam - and `otz` is `gte_stszotz` of the first point only,
/// serving as both the divisor of the beam's half-width and the OT index the
/// primitive is queued at. `dx` / `dy` are that half-width rotated by
/// `(arg3 * 23 / otz) * rsin|rcos(angle) >> 12`, applied once at the beam's
/// own angle and once at 90 degrees to it to give the `POLY_FT4` its four
/// corners.
typedef struct _MistShootingGalleryBeamScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ DVECTOR sxy0;
    /* 0x1C */ DVECTOR sxy1;
} MistShootingGalleryBeamScratch;
STATIC_ASSERT_SIZEOF(MistShootingGalleryBeamScratch, 0x20);

/// 0x1C-byte scratch block `func_mist_shooting_gallery_80182294` takes from
/// `G_SCRATCH_HEAD` to draw the gallery's muzzle flash.
///
/// It is the beam scratch above with the second projected point dropped: the
/// flash has a single world position, so `vec` is the effect coordinate's
/// `workm.t` truncated to s16, one `RTPS` fills `sxy` / `flag` / `otz`, and
/// `dx` / `dy` are the sprite's half-size `(arg2 * 39 / otz) * rsin|rcos(angle)
/// >> 12`, taken once at the flash's spin angle and once at 90 degrees to it so
/// the four `POLY_FT4` corners orbit the same point.
typedef struct _MistShootingGalleryFlashScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ DVECTOR sxy;
} MistShootingGalleryFlashScratch;
STATIC_ASSERT_SIZEOF(MistShootingGalleryFlashScratch, 0x1C);

extern Task* D_mist_shooting_gallery_8018E0C4;

#endif // ROOMS_MIST_SHOOTING_GALLERY_H
