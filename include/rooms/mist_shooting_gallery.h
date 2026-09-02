#ifndef ROOMS_MIST_SHOOTING_GALLERY_H
#define ROOMS_MIST_SHOOTING_GALLERY_H

#include "common.h"

#include "main/task.h"

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

extern Task* D_mist_shooting_gallery_8018E0C4;

#endif // ROOMS_MIST_SHOOTING_GALLERY_H
