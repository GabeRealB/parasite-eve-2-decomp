#ifndef MAIN_STAGE_TYPES_H
#define MAIN_STAGE_TYPES_H

#include "common.h"

/// Parameters supplied by EVS command 20 when changing stage music.
/// The second halfword is retained command state; the current loader ignores it.
typedef struct StageMusicParams {
    u16 fadeFrames;
    u16 unusedCommandArg;
} StageMusicParams;

/// Sequence id in `StageMusicEntry::sequenceId` that selects no music.
///
/// It is the MIDI driver's unloaded-sequence marker: nothing is loaded, and a
/// stage music task that selects it fades the current song out instead.
#define STAGE_MUSIC_NO_SEQUENCE 0xFF

/// Sequence id that, in the first entry of an area's row, keeps the stage
/// ambient loop (`SOUND_STAGE_AMBIENT`) running in that area.
///
/// Only the row's first entry is tested for it; whether the value is also ever
/// selected and loaded as a sequence is unproven.
#define STAGE_MUSIC_AMBIENT_AREA 0x80

/// How a stage music task starts the sequence of the `StageMusicEntry` it selects.
enum {
    /// Started once loaded; on an ordinary room entry, not until the view is ready.
    STAGE_MUSIC_START_WHEN_VIEW_READY = 0,
    /// Loaded but not started on an ordinary room entry; if it is already
    /// loaded, the current song only fades out. A later area request starts it.
    STAGE_MUSIC_START_DEFERRED = 1,
    /// Started as soon as it is loaded, without waiting for the view.
    STAGE_MUSIC_START_IMMEDIATE = 2,
    /// Loaded only; the stage music routines never start it.
    STAGE_MUSIC_START_NEVER = 3,
};

/// One entry of a stage's music table: the MIDI sequence an area plays and how it starts.
///
/// Each stage map overlay supplies two tables. The area table holds one row per
/// area, of a stage-specific length, and the scene event chooses the column, so
/// an area's music follows story progress. The countdown table is indexed
/// directly by the scene's music entry number.
typedef struct {
    u8 sequenceId; // MIDI sequence id, also its file in group 4 (`STAGE_MUSIC_NO_SEQUENCE` none)
    u8 startMode;  // `STAGE_MUSIC_START_*` (0 when view ready, 1 deferred, 2 immediate, 3 never)
} StageMusicEntry;
STATIC_ASSERT_SIZEOF(StageMusicEntry, 0x2);

#endif // MAIN_STAGE_TYPES_H
