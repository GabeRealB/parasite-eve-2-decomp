/* Part of the screen negative library; see screen_negative.h. */

#ifndef SCREEN_NEGATIVE_WORD_PAIR_HELPER_DEFINED
#define SCREEN_NEGATIVE_WORD_PAIR_HELPER_DEFINED

/// Converts four RGB555 pixels in two writable words to their grayscale negative.
///
/// Requires two distinct word-aligned words in pixel order. Each output channel
/// is 31 - floor((3R + 4G + B) / 8); bit 15 is cleared. Loads both input words
/// before writing either output and retains neither pointer.
static inline void _screenNegativeConvertWordPair(u_long* firstWord, u_long* secondWord)
{
    enum {
        SCREEN_NEGATIVE_PAIR_CHANNEL_MASK = 0x001F001F,
        SCREEN_NEGATIVE_PAIR_GREEN_MASK   = 0x03E003E0,
        SCREEN_NEGATIVE_ODD_LANE_MASK     = 0x1F001F00,
        SCREEN_NEGATIVE_LANE_MAXIMA       = 0x1F1F1F1F,
        SCREEN_NEGATIVE_RED_WEIGHT        = 3,
        SCREEN_NEGATIVE_GREEN_WEIGHT      = 4,
        SCREEN_NEGATIVE_WEIGHT_SUM_SHIFT  = 3,
    };
    u32 secondPixelPair;
    u32 firstPixelPair;
    u32 packedLuminance;
    u32 packedChannel;

    secondPixelPair = *secondWord;
    firstPixelPair  = *firstWord;

    // Four byte lanes hold pixels in order 0, 2, 1, 3. Weighted sums
    // fit in one byte (at most 248), so they cannot carry between lanes.
    packedChannel     = secondPixelPair & SCREEN_NEGATIVE_PAIR_CHANNEL_MASK;
    packedChannel   <<= 8;
    packedChannel    |= firstPixelPair & SCREEN_NEGATIVE_PAIR_CHANNEL_MASK;
    packedLuminance   = packedChannel * SCREEN_NEGATIVE_RED_WEIGHT;
    packedChannel     = secondPixelPair & SCREEN_NEGATIVE_PAIR_GREEN_MASK;
    packedChannel   <<= 3;
    firstPixelPair  >>= 5;
    packedChannel    |= firstPixelPair & SCREEN_NEGATIVE_PAIR_CHANNEL_MASK;
    packedLuminance  += packedChannel * SCREEN_NEGATIVE_GREEN_WEIGHT;
    secondPixelPair >>= 2;
    packedChannel     = secondPixelPair & SCREEN_NEGATIVE_ODD_LANE_MASK;
    firstPixelPair  >>= 5;
    packedChannel    |= firstPixelPair & SCREEN_NEGATIVE_PAIR_CHANNEL_MASK;
    packedLuminance  += packedChannel;
    packedLuminance   = (packedLuminance >> SCREEN_NEGATIVE_WEIGHT_SUM_SHIFT) & SCREEN_NEGATIVE_LANE_MAXIMA;
    packedLuminance   = SCREEN_NEGATIVE_LANE_MAXIMA - packedLuminance;

    // Repack the alternating lanes as two RGB555 pixel pairs.
    firstPixelPair    = packedLuminance & SCREEN_NEGATIVE_PAIR_CHANNEL_MASK;
    firstPixelPair   |= (firstPixelPair << 10) | (firstPixelPair << 5);
    secondPixelPair   = packedLuminance & SCREEN_NEGATIVE_ODD_LANE_MASK;
    secondPixelPair >>= 8;
    secondPixelPair  |= (secondPixelPair << 10) | (secondPixelPair << 5);
    *firstWord        = firstPixelPair;
    *secondWord       = secondPixelPair;
}
#endif

/// Replaces the resident image workspace with its grayscale photographic negative.
///
/// `Fs_ImgBuffers` must hold a complete writable, word-aligned 320x240 RGB555
/// frame. Finish GPU capture or image decoding before calling, and keep other
/// users out of the workspace during the conversion. Each output channel is
/// 31 - floor((3R + 4G + B) / 8), in five-bit channel units; pixel bit 15 is
/// cleared. All 0x25800 frame bytes are rewritten in place, preserving pixel
/// order. The caller owns capture, upload and the workspace's lifetime.
static void SCREEN_NEGATIVE_FILTER(void)
{
    u_long(*frameWords)[FILE_SYSTEM_IMAGE_STRIP_COUNT * FILE_SYSTEM_IMAGE_STRIP_WORDS];
    s32     wordPairsProcessed;
    u_long* firstWord;
    u_long* secondWord;

    // View the whole frame as GPU words, including every adjacent strip.
    frameWords         = (u_long(*)[FILE_SYSTEM_IMAGE_STRIP_COUNT * FILE_SYSTEM_IMAGE_STRIP_WORDS]) Fs_ImgBuffers;
    firstWord          = *frameWords;
    wordPairsProcessed = 0;
    secondWord         = firstWord + 1;
    do {
        wordPairsProcessed++;
        _screenNegativeConvertWordPair(firstWord, secondWord);
        secondWord += 2;
        firstWord  += 2;
    } while (wordPairsProcessed < ARRAY_SIZE(*frameWords) / 2);
}
