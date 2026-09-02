#ifndef ROOMS_MIST_R18_H
#define ROOMS_MIST_R18_H

#include "common.h"

/// Sprite description the overlay's two primitive emitters read from.
///
/// `func_mist_r18_8017E448` turns it into a flat-shaded `TILE` and
/// `func_mist_r18_8017E534` into a textured `SPRT`; both take the screen
/// rectangle from `x`/`y`/`w`/`h`, the colour from `r`/`g`/`b`, and pick the
/// `SetShadeTex` argument from `semiTrans`. Only the textured emitter reads
/// `u`/`v`, and only their low bytes: the text renderer builds them as
/// halfwords (glyph offset plus the page origin) before handing the sprite
/// over.
typedef struct MistR18Sprite {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 u;
    /* 0x06 */ s16 v;
    /* 0x08 */ s16 w;
    /* 0x0A */ s16 h;
    /* 0x0C */ u8  r;
    /* 0x0D */ u8  g;
    /* 0x0E */ u8  b;
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ s16 semiTrans;
    /* 0x12 */ s16 scale;
} MistR18Sprite;

STATIC_ASSERT_SIZEOF(MistR18Sprite, 0x14);

/// One glyph cell in the font table `MistR18TextSpawn::glyphs`: its position
/// within the texture page and the size of its screen rectangle. A cell whose
/// `h` is 0 (a space) draws nothing but still advances the pen by `w`.
typedef struct MistR18Glyph {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 v;
    /* 0x2 */ u8 w;
    /* 0x3 */ u8 h;
} MistR18Glyph;

STATIC_ASSERT_SIZEOF(MistR18Glyph, 0x4);

/// Spawn descriptor for the typewriter text task `func_mist_r18_8017D5F4`
/// drives, hung off `Task::spawnArg2`.
///
/// `script` is a run of glyph indices terminated by 0xFF, with 0xFE as a line
/// break; `index` is how many of them are revealed so far and doubles as the
/// draw count. `delay` is the per-glyph frame count reloaded into
/// `Task::killCountdown`, `delayEnd` the one used once the terminator is
/// reached; a negative `delay` reveals the whole script at once. `x`/`y` is
/// the pen origin, `u`/`v` the font page origin, `clutX`/`clutY` the palette
/// position and `boxW`/`boxH` the backing rectangle drawn behind the text.
typedef struct MistR18TextSpawn {
    /* 0x00 */ s16           x;
    /* 0x02 */ s16           y;
    /* 0x04 */ s16           u;
    /* 0x06 */ s16           v;
    /* 0x08 */ s16           clutX;
    /* 0x0A */ s16           clutY;
    /* 0x0C */ s16           delay;
    /* 0x0E */ s16           index;
    /* 0x10 */ u8*           script;
    /* 0x14 */ MistR18Glyph* glyphs;
    /* 0x18 */ s16           lineHeight;
    /* 0x1A */ s16           delayEnd;
    /* 0x1C */ s16           boxW;
    /* 0x1E */ s16           boxH;
} MistR18TextSpawn;

STATIC_ASSERT_SIZEOF(MistR18TextSpawn, 0x20);

/// Spawn descriptor for the sprite task `func_mist_r18_8017E3A4` drives: the
/// screen rectangle it redraws every frame, hung off `Task::spawnArg2`.
typedef struct MistR18SpriteSpawn {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 w;
    /* 0x06 */ s16 h;
} MistR18SpriteSpawn;

STATIC_ASSERT_SIZEOF(MistR18SpriteSpawn, 0x8);

#endif
