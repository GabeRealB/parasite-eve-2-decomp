#ifndef MAIN_PRIVATE_TEXT_H
#define MAIN_PRIVATE_TEXT_H

#include "types.h"

#include "main/task_types.h"
#include "main/text.h"
#include "text_types.h"

extern GlyphUvwh Caption_Glyphs[];

s32 TextStream_Draw(TextStream* stream, u8* arg1, s16* arg2, s32 arg3);

s32 Text_MeasureMultiLine(u8* arg0);

/// Task callback that does nothing.
///
/// A task pointed at it stays in its list and is still ticked, but performs no
/// work. It is what a task holds while it is on its way out: on `exitCallback`,
/// so a teardown already under way cannot be run a second time, and on
/// `callback`, so a task whose death is pending stops acting before it is
/// collected. A descriptor can also spawn it where an inert task is wanted.
void textNoopCallback(Task* task);

#endif // MAIN_PRIVATE_TEXT_H
