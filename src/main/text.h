#ifndef MAIN_PRIVATE_TEXT_H
#define MAIN_PRIVATE_TEXT_H

#include "types.h"

#include "main/text.h"
#include "text_types.h"

s32 TextStream_Draw(TextStream* stream, u8* arg1, s16* arg2, s32 arg3);

s32 Text_MeasureMultiLine(u8* arg0);

#endif // MAIN_PRIVATE_TEXT_H
