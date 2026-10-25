#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "main/areas.h"
#include "main/display.h"
#include "main/event_flags.h"
#include "display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "fs_types.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "text.h"

/* Define BSS before API headers to preserve first-declaration order. */
s16 gGameFlowLoadImageSlot;

u16 gGameFlowLoadScreenPhase;

s16 gGameFlowLoadScreenPresentationStep;

/// Text playback values used when initializing or retaining the revealed caption.
enum {
    GAME_FLOW_LOAD_CAPTION_PHASE_ARM        = 0,
    GAME_FLOW_LOAD_CAPTION_STATUS_PROGRESS  = 0,
    GAME_FLOW_LOAD_CAPTION_REDRAW_COUNTDOWN = 1,
};

/// Primary caption playback phase (0 arm, 1 reveal/redraw, 2 complete).
u8 _gGameFlowLoadCaptionPhase;

/// Playback phase of the suppressed secondary caption (0 arm, 1 reveal, 2 complete).
///
/// Remains in the arm phase while secondary drawing is suppressed.
u8 _gGameFlowLegacySecondaryLoadCaptionPhase;

/// Presentation frames elapsed in the secondary-start delay or post-reveal hold.
///
/// Reset when presentation begins and when both reveal results are negative.
/// Counts to 61 during reveal, then to 60 during hold; an extended hold does
/// not increment it further.
s16 _gGameFlowLoadCaptionElapsedFrames;

/// Primary text countdown in presentation frames; expiry is a value below zero.
///
/// The text drawer reloads the glyph or final-caption delay. Once the reveal
/// result is negative, presentation writes 1 each frame to keep redrawing.
s16 _gGameFlowLoadCaptionFramesLeft;

/// Frame countdown retained for the suppressed secondary text drawer.
///
/// Initialized to zero and forced to 1 after reveal or during hold/fade-out.
s16 _gGameFlowLegacySecondaryLoadCaptionFramesLeft;

/// Primary text result, latched once negative (0 progressing, -1 end reached).
///
/// Completion from the drawer is 1; retaining the first negative result lets
/// the presentation advance while the caption continues to redraw.
s16 _gGameFlowLoadCaptionRevealStatus;

/// Secondary text result, latched once negative (0 progressing, -1 end reached).
///
/// The suppressed secondary path records -1 after its 61-frame start delay.
s16 _gGameFlowLegacySecondaryLoadCaptionRevealStatus;

TextStream* gGameFlowLegacySecondaryLoadCaption;

TextStream* gGameFlowLoadCaption;

/// Signed subtraction level of the loading-image fade mask.
///
/// Reveal starts at 255 and finishes below zero; darkening starts at zero and
/// clears the framebuffers above 256. Drawing uses the low byte, retaining its
/// wrap at 256. The volatile byte views request actual reads of that byte.
s16 _gFadeBootImageMaskLevel;

GameLocationKey gGameFlowLoadScreenDestination;

s16 gGameFlowLoadAlternateCaption;

#include "fs.h"

static s32 _fadeBootImageToBlack(s16 levelStep);

static void _fadeBeginBootImageReveal(void);

static s32 _fadeBootImageFromBlack(s32 levelStep);

static void _gameFlowSelectAcropolisLoadScreenAssets(u8* imageFileIndex);

static void _gameFlowSelectDryfieldLoadScreenAssets(u8* imageFileIndex);

static void _gameFlowSelectDryfieldNightLoadScreenAssets(u8* imageFileIndex);

static void _gameFlowSelectMineShelterLoadScreenAssets(u8* imageFileIndex);

/// Story-chapter keys used by the loading-screen selectors.
enum {
    GAME_FLOW_LOAD_CHAPTER_MIST_ARRIVAL = 0,
    GAME_FLOW_LOAD_CHAPTER_3            = 3,
    GAME_FLOW_LOAD_CHAPTER_4            = 4,
    GAME_FLOW_LOAD_CHAPTER_5            = 5,
    GAME_FLOW_LOAD_CHAPTER_6            = 6,
};

/// Marks glyphs that a text-stream caller may omit while retaining their width.
///
/// The loading-screen drawer disables this filter, so marked time glyphs appear.
enum { GAME_FLOW_LOAD_CAPTION_FILTER_FLAG = 0x80 };

/// Category-zero file indices paired with each loading-screen caption context.
enum {
    GAME_FLOW_LOAD_FILE_CATEGORY                          = 0,
    GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION                 = 3,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_MOTEL_ROOM_6            = 4,
    GAME_FLOW_LOAD_IMAGE_ACROPOLIS_SQUARE                 = 5,
    GAME_FLOW_LOAD_IMAGE_ACROPOLIS_OTHER                  = 6,
    GAME_FLOW_LOAD_IMAGE_MIST_ARRIVAL                     = 7,
    GAME_FLOW_LOAD_IMAGE_MIST_DEPARTURE                   = 8,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_MAIN_STREET             = 10,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_TRAILER_COACH           = 11,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_3   = 12,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_3 = 13,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_3   = 14,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_4   = 15,
    GAME_FLOW_LOAD_IMAGE_MINE_REFUGE_CHAPTER_4            = 16,
    GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_4  = 17,
    GAME_FLOW_LOAD_IMAGE_SHELTER_INCINERATOR_CHAPTER_4    = 18,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_4   = 19,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_4 = 20,
    GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_4     = 21,
    GAME_FLOW_LOAD_IMAGE_SHELTER_POD_CHAPTER_4            = 22,
    GAME_FLOW_LOAD_IMAGE_MINE_REFUGE_CHAPTER_5            = 23,
    GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_5  = 24,
    GAME_FLOW_LOAD_IMAGE_SHELTER_INCINERATOR_CHAPTER_5    = 25,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_5   = 26,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_5 = 27,
    GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_5     = 28,
    GAME_FLOW_LOAD_IMAGE_SHELTER_POD_CHAPTER_5            = 29,
    GAME_FLOW_LOAD_IMAGE_SHELTER_NURSERY                  = 30,
    GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_5        = 31,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_5   = 32,
    GAME_FLOW_LOAD_IMAGE_SHELTER_HELIPORT                 = 35,
    GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_6  = 36,
    GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_6     = 37,
    GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_6        = 38,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_OTHER                   = 39,
    GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_4        = 40,
};

/// Loading-caption font placement, pixel dimensions and frame-countdown reloads.
///
/// Countdown expiry means decrementing below zero, so a reload of 1 or 3 gives
/// a two- or four-call glyph interval. Loading presentation retains the revealed
/// caption using its own hold/fade timing rather than the final countdown.
/// Backing-plate sizes remain in the records; the loading drawer paints no plate.
enum {
    GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X                         = 960,
    GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y                         = 260,
    GAME_FLOW_LOAD_CAPTION_FAST_GLYPH_COUNTDOWN_FRAMES         = 1,
    GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES         = 3,
    GAME_FLOW_LOAD_CAPTION_SHORT_FINAL_COUNTDOWN_FRAMES        = 150,
    GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES         = 300,
    GAME_FLOW_LOAD_CAPTION_MIST_ARRIVAL_FINAL_COUNTDOWN_FRAMES = 107,
    GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT                         = 13,
    GAME_FLOW_LOAD_CAPTION_BOX_WIDTH                           = 216,
    GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT                 = 29,
    GAME_FLOW_LOAD_CAPTION_THREE_LINE_BOX_HEIGHT               = 40,
};

/// Glyph script for the M.I.S.T. evening date and time loading caption.
///
/// Two lines; the shooting-gallery selector uses this detailed caption.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
/// The time glyphs carry `GAME_FLOW_LOAD_CAPTION_FILTER_FLAG`; loading-screen
/// playback draws them because filtering is disabled.
static u8 _gGameFlowLoadCaptionMistEveningChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0x3B | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x3E | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x34 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x3B | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0F | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0C | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
/// Resident evening date/time/place caption for the M.I.S.T. center.
///
/// Selected for shooting-gallery arrivals with the detailed-caption loading image.
/// 2 lines; glyph/final countdown reloads are 1/150 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionMistEvening = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_FAST_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionMistEveningChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_SHORT_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Zero-filled legacy secondary-caption record paired with `_gGameFlowLoadCaptionMistEvening`.
///
/// The selector retains its address, but loading presentation ignores the
/// secondary argument. Its null script/font pointers are never dereferenced.
static TextStream _gGameFlowLegacySecondaryLoadCaptionMistEvening = { 0 };
/// Glyph script for the Akropolis Tower evening date and time loading caption.
///
/// Two lines; the Acropolis plaza selector uses this detailed caption.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
/// The time glyphs carry `GAME_FLOW_LOAD_CAPTION_FILTER_FLAG`; loading-screen
/// playback draws them because filtering is disabled.
static u8 _gGameFlowLoadCaptionAcropolisEveningChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0x3B | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x3E | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x38 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x39 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0F | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0C | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    TEXT_STREAM_LINE_BREAK,
    0x00,
    0x24,
    0x2B,
    0x28,
    0x29,
    0x28,
    0x25,
    0x22,
    0x2C,
    0x45,
    0x13,
    0x28,
    0x30,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
/// Resident evening date/time/place caption for the Acropolis plaza.
///
/// Selected for the plaza detailed-caption loading image.
/// 2 lines; glyph/final countdown reloads are 1/150 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionAcropolisEvening = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_FAST_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionAcropolisEveningChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_SHORT_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Zero-filled legacy secondary-caption record paired with `_gGameFlowLoadCaptionAcropolisEvening`.
///
/// The selector retains its address, but loading presentation ignores the
/// secondary argument. Its null script/font pointers are never dereferenced.
static TextStream _gGameFlowLegacySecondaryLoadCaptionAcropolisEvening = { 0 };
/// Glyph script for the Mojave Desert afternoon date and time loading caption.
///
/// Two lines; the daytime gas-station alternate-caption path selects it.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
/// The time glyphs carry `GAME_FLOW_LOAD_CAPTION_FILTER_FLAG`; loading-screen
/// playback draws them because filtering is disabled.
static u8 _gGameFlowLoadCaptionDesertAfternoonChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x38,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0x34 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x35 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x3E | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x34 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x36 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0F | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0C | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    0x41,
    0x45,
    0x45,
    0x0D,
    0x1E,
    0x2F,
    0x1A,
    0x1D,
    0x1A,
    TEXT_STREAM_END,
};
/// Resident afternoon date/time/place caption for the Mojave Desert.
///
/// Selected by the alternate-caption request at the Dryfield gas station.
/// 2 lines; glyph/final countdown reloads are 1/150 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionDesertAfternoon = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_FAST_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionDesertAfternoonChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_SHORT_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Zero-filled legacy secondary-caption record paired with `_gGameFlowLoadCaptionDesertAfternoon`.
///
/// The selector retains its address, but loading presentation ignores the
/// secondary argument. Its null script/font pointers are never dereferenced.
static TextStream _gGameFlowLegacySecondaryLoadCaptionDesertAfternoon = { 0 };
/// Glyph script for the Dryfield outskirts mesa date, time and place loading caption.
///
/// Three lines; selected when entering the Mine mesa.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
/// The time glyphs carry `GAME_FLOW_LOAD_CAPTION_FILTER_FLAG`; loading-screen
/// playback draws them because filtering is disabled.
static u8 _gGameFlowLoadCaptionDryfieldMesaNightChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0x34 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x35 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x3E | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x3D | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x39 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x00 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0C | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x1E,
    0x2C,
    0x1A,
    0x45,
    0x28,
    0x27,
    0x45,
    0x2D,
    0x21,
    0x1E,
    0x45,
    0x28,
    0x2E,
    0x2D,
    0x2C,
    0x24,
    0x22,
    0x2B,
    0x2D,
    0x2C,
    0x45,
    0x28,
    0x1F,
    0x45,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x2B,
    0x32,
    0x1F,
    0x22,
    0x1E,
    0x25,
    0x1D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
/// Resident nighttime date/time/place caption for the Dryfield outskirts mesa.
///
/// Selected for Mine mesa arrivals.
/// 3 lines; glyph/final countdown reloads are 1/150 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionDryfieldMesaNight = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_FAST_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionDryfieldMesaNightChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_SHORT_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_THREE_LINE_BOX_HEIGHT,
};
/// Zero-filled legacy secondary-caption record paired with `_gGameFlowLoadCaptionDryfieldMesaNight`.
///
/// The selector retains its address, but loading presentation ignores the
/// secondary argument. Its null script/font pointers are never dereferenced.
static TextStream _gGameFlowLegacySecondaryLoadCaptionDryfieldMesaNight = { 0 };
/// Glyph script for the White House evening date/time/place loading caption.
///
/// Selected for Neo Ark R26 with the detailed-caption loading image.
/// Two lines in 57 bytes, including the line break and terminator. Bytes
/// select the low-seven-bit indices of `gTextCaptionGlyphCells`;
/// `TEXT_STREAM_LINE_BREAK` separates lines and `TEXT_STREAM_END` ends the script.
/// Storage remains resident and playback only reads its bytes.
/// Time glyphs carry `GAME_FLOW_LOAD_CAPTION_FILTER_FLAG`; loading playback
/// disables that filter, so these glyphs still appear.
static u8 _gGameFlowLoadCaptionWhiteHouseEveningChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0x39 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x3E | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x36 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x3A | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0F | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0C | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    TEXT_STREAM_LINE_BREAK,
    0x16,
    0x21,
    0x22,
    0x2D,
    0x1E,
    0x45,
    0x07,
    0x28,
    0x2E,
    0x2C,
    0x1E,
    0x41,
    0x45,
    0x45,
    0x16,
    0x1A,
    0x2C,
    0x21,
    0x22,
    0x27,
    0x20,
    0x2D,
    0x28,
    0x27,
    0x45,
    0x03,
    0x42,
    0x02,
    0x42,
    TEXT_STREAM_END,
};
STATIC_ASSERT_SIZEOF(_gGameFlowLoadCaptionWhiteHouseEveningChars, 57);
/// Resident evening date/time/place caption for the White House.
///
/// Selected for Neo Ark R26 with the detailed-caption loading image.
/// 2 lines; glyph/final countdown reloads are 1/150 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionWhiteHouseEvening = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_FAST_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionWhiteHouseEveningChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_SHORT_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Zero-filled legacy secondary-caption record paired with `_gGameFlowLoadCaptionWhiteHouseEvening`.
///
/// The selector retains its address, but loading presentation ignores the
/// secondary argument. Its null script/font pointers are never dereferenced.
static TextStream _gGameFlowLegacySecondaryLoadCaptionWhiteHouseEvening = { 0 };
/// Glyph script for the return to the Mojave Desert date/time/place loading caption.
///
/// Selected by the alternate-caption request for Neo Ark shrine/default areas.
/// Two lines in 50 bytes, including the line break and terminator. Bytes
/// select the low-seven-bit indices of `gTextCaptionGlyphCells`;
/// `TEXT_STREAM_LINE_BREAK` separates lines and `TEXT_STREAM_END` ends the script.
/// Storage remains resident and playback only reads its bytes.
/// Time glyphs carry `GAME_FLOW_LOAD_CAPTION_FILTER_FLAG`; loading playback
/// disables that filter, so these glyphs still appear.
static u8 _gGameFlowLoadCaptionDesertReturnChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0x36 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x3E | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x38 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x35 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0F | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0C | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    0x41,
    0x45,
    0x45,
    0x0D,
    0x1E,
    0x2F,
    0x1A,
    0x1D,
    0x1A,
    TEXT_STREAM_END,
};
STATIC_ASSERT_SIZEOF(_gGameFlowLoadCaptionDesertReturnChars, 50);
/// Resident return date/time/place caption for the Mojave Desert.
///
/// Selected by the alternate-caption request for Neo Ark shrine/default areas.
/// 2 lines; glyph/final countdown reloads are 1/150 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionDesertReturn = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_FAST_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionDesertReturnChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_SHORT_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Zero-filled legacy secondary-caption record paired with `_gGameFlowLoadCaptionDesertReturn`.
///
/// The selector retains its address, but loading presentation ignores the
/// secondary argument. Its null script/font pointers are never dereferenced.
static TextStream _gGameFlowLegacySecondaryLoadCaptionDesertReturn = { 0 };
/// Glyph script for the White House aftermath date, time and place loading caption.
///
/// Two lines; selected by the normal-caption request for Shelter R36.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
/// The time glyphs carry `GAME_FLOW_LOAD_CAPTION_FILTER_FLAG`; loading-screen
/// playback draws them because filtering is disabled.
static u8 _gGameFlowLoadCaptionWhiteHouseAftermathChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x3A,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0x3C | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x3E | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x3D | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x38 | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0F | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    0x0C | GAME_FLOW_LOAD_CAPTION_FILTER_FLAG,
    TEXT_STREAM_LINE_BREAK,
    0x16,
    0x21,
    0x22,
    0x2D,
    0x1E,
    0x45,
    0x07,
    0x28,
    0x2E,
    0x2C,
    0x1E,
    0x41,
    0x45,
    0x45,
    0x16,
    0x1A,
    0x2C,
    0x21,
    0x22,
    0x27,
    0x20,
    0x2D,
    0x28,
    0x27,
    0x45,
    0x03,
    0x42,
    0x02,
    0x42,
    TEXT_STREAM_END,
};
/// Resident aftermath date/time/place caption for the White House.
///
/// Selected for Shelter R36 when no alternate caption is requested.
/// 2 lines; glyph/final countdown reloads are 1/150 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionWhiteHouseAftermath = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_FAST_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionWhiteHouseAftermathChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_SHORT_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Zero-filled legacy secondary-caption record paired with `_gGameFlowLoadCaptionWhiteHouseAftermath`.
///
/// The selector retains its address, but loading presentation ignores the
/// secondary argument. Its null script/font pointers are never dereferenced.
static TextStream _gGameFlowLegacySecondaryLoadCaptionWhiteHouseAftermath = { 0 };
/// Glyph script for the Nature Museum flashback date, time and place loading caption.
///
/// Two lines; selected by the alternate-caption request for Shelter R36.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
static u8 _gGameFlowLoadCaptionMuseumFlashbackChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x34,
    0x41,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x34,
    0x45,
    0x45,
    0x3A,
    0x3E,
    0x34,
    0x39,
    0x0F,
    0x0C,
    TEXT_STREAM_LINE_BREAK,
    0x0D,
    0x1A,
    0x2D,
    0x2E,
    0x2B,
    0x1E,
    0x45,
    0x0C,
    0x2E,
    0x2C,
    0x1E,
    0x2E,
    0x26,
    0x41,
    0x45,
    0x45,
    0x0D,
    0x1E,
    0x30,
    0x45,
    0x18,
    0x28,
    0x2B,
    0x24,
    TEXT_STREAM_END,
};
/// Resident flashback date/time/place caption for the Nature Museum.
///
/// Selected by the alternate-caption request for Shelter R36.
/// 2 lines; glyph/final countdown reloads are 1/300 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionMuseumFlashback = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_FAST_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionMuseumFlashbackChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Zero-filled legacy secondary-caption record paired with `_gGameFlowLoadCaptionMuseumFlashback`.
///
/// The selector retains its address, but loading presentation ignores the
/// secondary argument. Its null script/font pointers are never dereferenced.
static TextStream _gGameFlowLegacySecondaryLoadCaptionMuseumFlashback = { 0 };
/// Glyph script for the M.I.S.T. arrival date and place loading caption.
///
/// Two lines; selected at M.I.S.T. parking while the story chapter is zero.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
static u8 _gGameFlowLoadCaptionMistArrivalChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
/// Resident arrival date/place caption for the M.I.S.T. center.
///
/// Selected at M.I.S.T. parking while the story chapter is zero.
/// 2 lines; glyph/final countdown reloads are 1/107 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionMistArrival = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_FAST_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionMistArrivalChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_MIST_ARRIVAL_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Glyph script for a retained, unselected M.I.S.T. date/place caption.
///
/// Only `_gGameFlowLoadCaptionMistUnused` references it; no loading selector reaches that record.
/// Two lines in 49 bytes, including the line break and terminator. Bytes
/// select the low-seven-bit indices of `gTextCaptionGlyphCells`;
/// `TEXT_STREAM_LINE_BREAK` separates lines and `TEXT_STREAM_END` ends the script.
/// Storage remains resident and playback only reads its bytes.
static u8 _gGameFlowLoadCaptionMistUnusedChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
STATIC_ASSERT_SIZEOF(_gGameFlowLoadCaptionMistUnusedChars, 49);
/// Retained, unselected M.I.S.T. date/place loading-caption playback record.
///
/// Uses resident glyph/script storage, a three-frame glyph countdown reload
/// and a 300-frame final reload. No loading-screen selector chooses this record.
/// Coordinates and backing-plate dimensions are in pixels; the loading drawer
/// does not draw the plate.
static TextStream _gGameFlowLoadCaptionMistUnused = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionMistUnusedChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Glyph script for the Akropolis Tower arrival date loading caption.
///
/// Two lines; the Acropolis square and ordinary tower-area selectors share this script.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
static u8 _gGameFlowLoadCaptionAcropolisChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x00,
    0x24,
    0x2B,
    0x28,
    0x29,
    0x28,
    0x25,
    0x22,
    0x2C,
    0x45,
    0x13,
    0x28,
    0x30,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
/// Resident date/place caption for ordinary Acropolis tower loading screens.
///
/// Selected for the square and the remaining ordinary tower areas.
/// 2 lines; glyph/final countdown reloads are 3/300 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionAcropolis = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionAcropolisChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Glyph script for the M.I.S.T. departure date and place loading caption.
///
/// Two lines; selected at M.I.S.T. parking after the initial story chapter.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
static u8 _gGameFlowLoadCaptionMistDepartureChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x38,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
/// Resident departure date/place caption for the M.I.S.T. center.
///
/// Selected at M.I.S.T. parking after the initial story chapter.
/// 2 lines; glyph/final countdown reloads are 3/300 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionMistDeparture = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionMistDepartureChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Glyph script for the first Dryfield visit date and place loading caption.
///
/// Two lines; used for daytime Dryfield and nighttime chapter-3 arrivals.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
static u8 _gGameFlowLoadCaptionDryfieldChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x38,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x2B,
    0x32,
    0x1F,
    0x22,
    0x1E,
    0x25,
    0x1D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
/// Resident date/place caption for first-visit Dryfield loading screens.
///
/// Uses the caption font at VRAM (960, 0) and CLUT (0, 260), drawn from
/// (-150, -90) in display pixels. Each glyph has a three-frame countdown;
/// the drawer advances when it decrements below zero. The final text countdown
/// reloads to 300 frames, but loading presentation keeps the revealed text alive
/// with its own hold/fade timing. Playback writes only the script cursor.
static TextStream _gGameFlowLoadCaptionDryfield = {
    .x           = -150,
    .y           = -90,
    .tpageX      = 960,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = 260,
    .charDelay   = 3,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionDryfieldChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = 13,
    .delayReload = 300,
    .boxWidth    = 216,
    .boxHeight   = 29,
};
/// Glyph script for the Mine shaft date and place loading caption.
///
/// Two lines; the Mine refuge selector shares it across story chapters.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
static u8 _gGameFlowLoadCaptionMineChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x22,
    0x27,
    0x1E,
    0x45,
    0x2C,
    0x21,
    0x1A,
    0x1F,
    0x2D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
/// Resident date/place caption for the Mine shaft.
///
/// Shared by Mine refuge arrivals in story chapters 4 and 5.
/// 2 lines; glyph/final countdown reloads are 3/300 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionMine = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionMineChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Glyph script for the Shelter dwelling-level date and place loading caption.
///
/// Two lines; shared by sterilization-room and underground-parking arrivals.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
static u8 _gGameFlowLoadCaptionShelterDwellingChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x30,
    0x1E,
    0x25,
    0x25,
    0x22,
    0x27,
    0x20,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
/// Resident date/place caption for the Shelter dwelling level.
///
/// Shared by sterilization-room and underground-parking arrivals in chapters 4, 5 and 6.
/// 2 lines; glyph/final countdown reloads are 3/300 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionShelterDwelling = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionShelterDwellingChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Glyph script for the Shelter waste-level date and place loading caption.
///
/// Two lines; selected for the incinerator control room.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
static u8 _gGameFlowLoadCaptionShelterWasteChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x16,
    0x1A,
    0x2C,
    0x2D,
    0x1E,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
/// Resident date/place caption for the Shelter waste level.
///
/// Shared by incinerator-control-room arrivals in chapters 4 and 5.
/// 2 lines; glyph/final countdown reloads are 3/300 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionShelterWaste = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionShelterWasteChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Glyph script for the return to Dryfield date and place loading caption.
///
/// Two lines; used for nighttime story chapters 4 and 5.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
static u8 _gGameFlowLoadCaptionDryfieldReturnChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x2B,
    0x32,
    0x1F,
    0x22,
    0x1E,
    0x25,
    0x1D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
/// Resident return date/place caption for nighttime Dryfield.
///
/// Shared by lobby, trailer and other-area arrivals in story chapters 4 and 5.
/// 2 lines; glyph/final countdown reloads are 3/300 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionDryfieldReturn = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionDryfieldReturnChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Glyph script for the Shelter laboratory-level date and place loading caption.
///
/// Two lines; the laboratory selector shares it across story chapters.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
static u8 _gGameFlowLoadCaptionShelterLaboratoryChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0B,
    0x1A,
    0x1B,
    0x28,
    0x2B,
    0x1A,
    0x2D,
    0x28,
    0x2B,
    0x32,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
/// Resident date/place caption for the Shelter laboratory level.
///
/// Shared by laboratory arrivals in story chapters 4, 5 and 6.
/// 2 lines; glyph/final countdown reloads are 3/300 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionShelterLaboratory = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionShelterLaboratoryChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Glyph script for the Shelter pod-shaft date and place loading caption.
///
/// Two lines; the Mine/Shelter selector uses it for the remaining areas.
/// Bytes index `gTextCaptionGlyphCells`; `TEXT_STREAM_LINE_BREAK` separates
/// lines and `TEXT_STREAM_END` terminates the script. Storage stays resident.
static u8 _gGameFlowLoadCaptionShelterPodChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0F,
    0x28,
    0x1D,
    0x45,
    0x2C,
    0x21,
    0x1A,
    0x1F,
    0x2D,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
/// Resident date/place caption for the Shelter pod shaft.
///
/// Shared by the remaining Mine/Shelter areas in chapters 4 and 5, and other chapters.
/// 2 lines; glyph/final countdown reloads are 3/300 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionShelterPod = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionShelterPodChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Glyph script for the Shelter experiment-level date/place loading caption.
///
/// Selected for Shelter B6 nursery arrivals.
/// Two lines in 45 bytes, including the line break and terminator. Bytes
/// select the low-seven-bit indices of `gTextCaptionGlyphCells`;
/// `TEXT_STREAM_LINE_BREAK` separates lines and `TEXT_STREAM_END` ends the script.
/// Storage remains resident and playback only reads its bytes.
static u8 _gGameFlowLoadCaptionShelterExperimentChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x04,
    0x31,
    0x29,
    0x1E,
    0x2B,
    0x22,
    0x26,
    0x1E,
    0x27,
    0x2D,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
STATIC_ASSERT_SIZEOF(_gGameFlowLoadCaptionShelterExperimentChars, 45);
/// Resident date/place caption for the Shelter experiment level.
///
/// Selected for Shelter B6 nursery arrivals.
/// 2 lines; glyph/final countdown reloads are 3/300 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionShelterExperiment = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionShelterExperimentChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};
/// Glyph script for the Shelter heliport date/place loading caption.
///
/// Selected for Neo Ark shrine/default areas without an alternate-caption request.
/// Two lines in 37 bytes, including the line break and terminator. Bytes
/// select the low-seven-bit indices of `gTextCaptionGlyphCells`;
/// `TEXT_STREAM_LINE_BREAK` separates lines and `TEXT_STREAM_END` ends the script.
/// Storage remains resident and playback only reads its bytes.
static u8 _gGameFlowLoadCaptionShelterHeliportChars[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x07,
    0x1E,
    0x25,
    0x22,
    0x29,
    0x28,
    0x2B,
    0x2D,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
STATIC_ASSERT_SIZEOF(_gGameFlowLoadCaptionShelterHeliportChars, 37);
/// Resident date/place caption for the Shelter heliport.
///
/// Selected for Neo Ark shrine/default areas when no alternate caption is requested.
/// 2 lines; glyph/final countdown reloads are 3/300 frames.
/// Storage remains resident; playback resets and advances the cursor in place.
/// Loading presentation supplies its own hold/fade timing after reveal.
static TextStream _gGameFlowLoadCaptionShelterHeliport = {
    .x           = -150,
    .y           = -90,
    .tpageX      = GAME_FLOW_LOAD_CAPTION_FONT_PAGE_X,
    .tpageY      = 0,
    .clutX       = 0,
    .clutY       = GAME_FLOW_LOAD_CAPTION_FONT_CLUT_Y,
    .charDelay   = GAME_FLOW_LOAD_CAPTION_SLOW_GLYPH_COUNTDOWN_FRAMES,
    .cursor      = 0,
    .chars       = _gGameFlowLoadCaptionShelterHeliportChars,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = GAME_FLOW_LOAD_CAPTION_LINE_HEIGHT,
    .delayReload = GAME_FLOW_LOAD_CAPTION_LONG_FINAL_COUNTDOWN_FRAMES,
    .boxWidth    = GAME_FLOW_LOAD_CAPTION_BOX_WIDTH,
    .boxHeight   = GAME_FLOW_LOAD_CAPTION_TWO_LINE_BOX_HEIGHT,
};

/// Boot-image mask colour range and its screen/ordering-table placement.
enum {
    FADE_BOOT_IMAGE_MAX_COLOR   = 255,
    FADE_BOOT_IMAGE_CLEAR_LEVEL = 256,
    FADE_BOOT_IMAGE_OT_INDEX    = -16,
};

/// Queues a grayscale subtractive mask over the 320x240 loading image.
///
/// `maskColor` is the amount subtracted from each RGB channel (0..255).
/// Requires word-aligned space for one TILE and one DR_TPAGE in the system
/// primitive arena, and a current OT with its reserved prefix. Packet storage
/// must survive GPU completion; this advances the arena cursor without a bound check.
static inline void _fadeDrawBootImageMask(u8 maskColor)
{
    TILE*     tile;
    DR_TPAGE* pagePacket;

    tile                 = gGpuSystemPrimCursor;
    gGpuSystemPrimCursor = tile + 1;
    setTile(tile);
    setSemiTrans(tile, true);
    tile->r0 = maskColor;
    tile->g0 = maskColor;
    tile->b0 = maskColor;
    tile->x0 = -FILE_SYSTEM_IMAGE_WIDTH / 2;
    tile->y0 = -FILE_SYSTEM_IMAGE_HEIGHT / 2;
    tile->w  = FILE_SYSTEM_IMAGE_WIDTH;
    tile->h  = FILE_SYSTEM_IMAGE_HEIGHT;
    addPrim(gGpuCurrentOt + FADE_BOOT_IMAGE_OT_INDEX, tile);

    pagePacket           = gGpuSystemPrimCursor;
    gGpuSystemPrimCursor = pagePacket + 1;
    // Head insertion makes the blend-mode command execute before the tile.
    setDrawTPage(pagePacket, false, true, getTPage(0, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt + FADE_BOOT_IMAGE_OT_INDEX, pagePacket);
}

/// Draws and strengthens the boot-image mask, then clears both buffers at completion.
///
/// Returns 1 once the signed mask level is above 256, otherwise adds `levelStep`
/// and returns 0. The caller starts at zero and uses a positive step that stays
/// in s16 range. Drawing uses the pre-step low byte, including its wrap at 256;
/// completion clears both 320x240 buffers and disables image-strip presentation.
static s32 _fadeBootImageToBlack(s16 levelStep)
{
    u8   maskColor;
    RECT clearRect;

    maskColor = _gFadeBootImageMaskLevel;
    _fadeDrawBootImageMask(maskColor);

    if (_gFadeBootImageMaskLevel > FADE_BOOT_IMAGE_CLEAR_LEVEL) {
        clearRect.y = 0;
        clearRect.x = 0;
        clearRect.w = FILE_SYSTEM_IMAGE_WIDTH;
        clearRect.h = FILE_SYSTEM_IMAGE_HEIGHT;
        ClearImage(&clearRect, 0, 0, 0);
        clearRect.y = DISPLAY_BUFFER_STRIDE_ROWS;
        ClearImage(&clearRect, 0, 0, 0);
        gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
        return 1;
    }
    _gFadeBootImageMaskLevel += levelStep;
    return 0;
}

/// Enables the boot-image display and initializes a full-strength subtractive mask.
///
/// Keeps the view and existing framebuffer contents; the white mask colour makes
/// the displayed image black. Requires primitive-arena and ordering-table space.
static void _fadeBeginBootImageReveal(void)
{
    u8 maskColor;

    displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
    SetDispMask(1);

    _gFadeBootImageMaskLevel = FADE_BOOT_IMAGE_MAX_COLOR;
    maskColor                = *(volatile u8*)&_gFadeBootImageMaskLevel;
    _fadeDrawBootImageMask(maskColor);
}

/// Draws the boot-image mask and reduces its signed level to reveal the image.
///
/// Enables image strips and returns 1 when the s16 result of subtracting
/// `levelStep` is negative. Starts at 255; the caller uses a positive step without
/// signed overflow. The current low byte is drawn before the level changes.
static s32 _fadeBootImageFromBlack(s32 levelStep)
{
    u8  maskColor;
    s16 nextLevel;

    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
    maskColor                               = *(volatile u8*)&_gFadeBootImageMaskLevel;
    _fadeDrawBootImageMask(maskColor);

    nextLevel                = _gFadeBootImageMaskLevel - levelStep;
    _gFadeBootImageMaskLevel = nextLevel;
    return nextLevel < 0;
}

/// Selects the MIST/Acropolis loading caption and category-zero image file.
///
/// Reads the retained destination area and distinguishes MIST arrival from
/// departure by story chapter. Writes one byte to `imageFileIndex`; the caller
/// clears the secondary caption before selection, and all captions remain resident.
static void _gameFlowSelectAcropolisLoadScreenAssets(u8* imageFileIndex)
{
    switch (gGameFlowLoadScreenDestination.area) {
        case GAME_AREA_AKROPOLIS_SQUARE:
            gGameFlowLoadCaption = &_gGameFlowLoadCaptionAcropolis;
            *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_ACROPOLIS_SQUARE;
            break;
        case GAME_AREA_AKROPOLIS_PLAZA:
            gGameFlowLoadCaption                = &_gGameFlowLoadCaptionAcropolisEvening;
            gGameFlowLegacySecondaryLoadCaption = &_gGameFlowLegacySecondaryLoadCaptionAcropolisEvening;
            *imageFileIndex                     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            break;
        case GAME_AREA_AKROPOLIS_EAST_ELEVATOR_HALL ... GAME_AREA_AKROPOLIS_CAFETERIA:
        case GAME_AREA_AKROPOLIS_SECURITY_ROOM ... GAME_AREA_MIST_R18:
        default:
            gGameFlowLoadCaption = &_gGameFlowLoadCaptionAcropolis;
            *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_ACROPOLIS_OTHER;
            break;
        case GAME_AREA_MIST_PARKING:
            if (gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) == GAME_FLOW_LOAD_CHAPTER_MIST_ARRIVAL) {
                gGameFlowLoadCaption = &_gGameFlowLoadCaptionMistArrival;
                *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_MIST_ARRIVAL;
            } else {
                gGameFlowLoadCaption = &_gGameFlowLoadCaptionMistDeparture;
                *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_MIST_DEPARTURE;
            }
            break;
        case GAME_AREA_MIST_SHOOTING_GALLERY:
            gGameFlowLoadCaption                = &_gGameFlowLoadCaptionMistEvening;
            gGameFlowLegacySecondaryLoadCaption = &_gGameFlowLegacySecondaryLoadCaptionMistEvening;
            *imageFileIndex                     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            break;
    }
}

/// Selects the daytime Dryfield loading caption and category-zero image file.
///
/// Reads the retained destination area and uses the alternate-caption selector
/// at the gas station. Writes one byte to `imageFileIndex`; the caller clears
/// the secondary caption first, and all selected captions remain resident.
static void _gameFlowSelectDryfieldLoadScreenAssets(u8* imageFileIndex)
{
    switch (gGameFlowLoadScreenDestination.area) {
        case GAME_AREA_DRYFIELD_GAS_STATION:
            if (gGameFlowLoadAlternateCaption == GAME_FLOW_LOAD_CAPTION_NORMAL) {
                gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfield;
                *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_MAIN_STREET;
            } else {
                gGameFlowLoadCaption                = &_gGameFlowLoadCaptionDesertAfternoon;
                gGameFlowLegacySecondaryLoadCaption = &_gGameFlowLegacySecondaryLoadCaptionDesertAfternoon;
                *imageFileIndex                     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            }
            break;
        case GAME_AREA_DRYFIELD_MAIN_STREET:
            gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfield;
            *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_MAIN_STREET;
            break;
        case GAME_AREA_DRYFIELD_TRAILER_COACH:
            gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfield;
            *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_TRAILER_COACH;
            break;
        case GAME_AREA_DRYFIELD_MOTEL_ROOM_6:
            gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfield;
            *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_MOTEL_ROOM_6;
            break;
        case GAME_AREA_DRYFIELD_GENERAL_STORE ... GAME_AREA_DRYFIELD_JUNK_YARD:
        case GAME_AREA_DRYFIELD_MOTEL_ROOM_5 ... GAME_AREA_DRYFIELD_MOTEL_BALCONY:
        case GAME_AREA_DRYFIELD_MOTEL_LOFT ... GAME_AREA_DRYFIELD_UNDERPASS:
        default:
            gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfield;
            *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_OTHER;
            break;
    }
}

/// Selects the nighttime Dryfield loading caption and category-zero image file.
///
/// Lobby, trailer and other-area images differ in story chapters 3, 4 and 5.
/// Chapters other than 4 and 5 use the chapter-3 image. Writes one byte to
/// `imageFileIndex` and leaves the caller-cleared secondary caption untouched.
static void _gameFlowSelectDryfieldNightLoadScreenAssets(u8* imageFileIndex)
{
    u16 storyChapter;

    storyChapter = gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER);
    switch (gGameFlowLoadScreenDestination.area) {
        case GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_3:
                default:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfield;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_3;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_4:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfieldReturn;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfieldReturn;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_5;
                    break;
            }
            break;
        case GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_3:
                default:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfield;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_3;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_4:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfieldReturn;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfieldReturn;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_5;
                    break;
            }
            break;
        case GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5:
        default:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_3:
                default:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfield;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_3;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_4:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfieldReturn;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionDryfieldReturn;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_5;
                    break;
            }
            break;
    }
}

/// Selects the Mine/Shelter loading caption and category-zero image file.
///
/// Uses the retained area, story chapter and alternate-caption selector. Chapter
/// 5 selects later images; chapter 6 has distinct sterilization, parking and lab
/// images. Other chapters use chapter-4 images. Writes one byte to
/// `imageFileIndex`; the caller clears the secondary caption before selection.
static void _gameFlowSelectMineShelterLoadScreenAssets(u8* imageFileIndex)
{
    u16 storyChapter;

    storyChapter = gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER);
    switch (gGameFlowLoadScreenDestination.area) {
        case GAME_AREA_MINE_MESA:
            gGameFlowLoadCaption                = &_gGameFlowLoadCaptionDryfieldMesaNight;
            gGameFlowLegacySecondaryLoadCaption = &_gGameFlowLegacySecondaryLoadCaptionDryfieldMesaNight;
            *imageFileIndex                     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            break;
        case GAME_AREA_MINE_REFUGE:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionMine;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_MINE_REFUGE_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionMine;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_MINE_REFUGE_CHAPTER_5;
                    break;
            }
            break;
        case GAME_AREA_SHELTER_B1_STERILIZATION_ROOM:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterDwelling;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterDwelling;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_5;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_6:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterDwelling;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_6;
                    break;
            }
            break;
        case GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterDwelling;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterDwelling;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_5;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_6:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterDwelling;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_6;
                    break;
            }
            break;
        case GAME_AREA_SHELTER_R36:
            if (gGameFlowLoadAlternateCaption == GAME_FLOW_LOAD_CAPTION_NORMAL) {
                gGameFlowLoadCaption                = &_gGameFlowLoadCaptionWhiteHouseAftermath;
                gGameFlowLegacySecondaryLoadCaption = &_gGameFlowLegacySecondaryLoadCaptionWhiteHouseAftermath;
                *imageFileIndex                     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            } else {
                gGameFlowLoadCaption                = &_gGameFlowLoadCaptionMuseumFlashback;
                gGameFlowLegacySecondaryLoadCaption = &_gGameFlowLegacySecondaryLoadCaptionMuseumFlashback;
                *imageFileIndex                     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            }
            break;
        case GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterWaste;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_INCINERATOR_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterWaste;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_INCINERATOR_CHAPTER_5;
                    break;
            }
            break;
        case GAME_AREA_SHELTER_B2_LABORATORY:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterLaboratory;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterLaboratory;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_5;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_6:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterLaboratory;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_6;
                    break;
            }
            break;
        case GAME_AREA_MINE_CAVERN ... GAME_AREA_MINE_GORGE:
        case GAME_AREA_MINE_FORKED_TUNNEL ... GAME_AREA_SHELTER_B1_MAIN_CORRIDOR:
        case GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL ... GAME_AREA_SHELTER_B1_ACCESS_TUNNEL:
        case GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 ... GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY:
        case GAME_AREA_SHELTER_B2_BREEDING_ROOM ... GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL:
        case GAME_AREA_SHELTER_R37 ... GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR:
        case GAME_AREA_SHELTER_B3_ELEVATOR_HALL ... GAME_AREA_SHELTER_R47:
        default:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterPod;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_POD_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterPod;
                    *imageFileIndex      = GAME_FLOW_LOAD_IMAGE_SHELTER_POD_CHAPTER_5;
                    break;
            }
            break;
    }
}

/// Clears both loading-screen framebuffers and blanks the display.
///
/// `clearRect` is borrowed writable scratch, left describing the second buffer.
/// Requires the 320x240 VRAM regions at rows 0 and 272 to be available for reuse.
/// Queues both clears, disables background-image presentation, then blanks GPU
/// output. Does not wait for GPU completion; the caller supplies the incoming
/// image and controls when it is revealed.
static inline void _gameFlowPrepareLoadScreenDisplay(RECT* clearRect)
{
    clearRect->y = 0;
    clearRect->x = 0;
    clearRect->w = FILE_SYSTEM_IMAGE_WIDTH;
    clearRect->h = FILE_SYSTEM_IMAGE_HEIGHT;
    ClearImage(clearRect, 0, 0, 0);
    clearRect->y = DISPLAY_BUFFER_STRIDE_ROWS;
    ClearImage(clearRect, 0, 0, 0);
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
    SetDispMask(false);
}

void gameFlowStartLoadScreenImage(void)
{
    CdCmdFileKey      imageKey;
    CdCmdFileLoadArgs loadOptions;

    RECT clearRect;

    // Discard the old image before selecting the next screen's resident captions.
    memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
    _gameFlowPrepareLoadScreenDisplay(&clearRect);
    imageKey.stage                      = GAME_STAGE_NONE;
    imageKey.fileGroup                  = GAME_FLOW_LOAD_FILE_CATEGORY;
    gGameFlowLegacySecondaryLoadCaption = NULL;
    switch (gGameFlowLoadScreenDestination.stage) {
        case GAME_STAGE_AKROPOLIS:
            _gameFlowSelectAcropolisLoadScreenAssets(&imageKey.fileIndex);
            break;
        case GAME_STAGE_DRYFIELD:
            _gameFlowSelectDryfieldLoadScreenAssets(&imageKey.fileIndex);
            break;
        case GAME_STAGE_DRYFIELD_NIGHT:
            _gameFlowSelectDryfieldNightLoadScreenAssets(&imageKey.fileIndex);
            break;
        case GAME_STAGE_MINE_SHELTER:
            _gameFlowSelectMineShelterLoadScreenAssets(&imageKey.fileIndex);
            break;
        case GAME_STAGE_SHELTER_NEO_ARK:
        default:
            // Retain the story-flag read even though this stage does not branch on it.
            gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER);
            switch (gGameFlowLoadScreenDestination.area) {
                case GAME_AREA_NEO_ARK_SHRINE:
                default:
                    if (gGameFlowLoadAlternateCaption == GAME_FLOW_LOAD_CAPTION_NORMAL) {
                        gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterHeliport;
                        imageKey.fileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_HELIPORT;
                    } else {
                        gGameFlowLoadCaption                = &_gGameFlowLoadCaptionDesertReturn;
                        gGameFlowLegacySecondaryLoadCaption = &_gGameFlowLegacySecondaryLoadCaptionDesertReturn;
                        imageKey.fileIndex                  = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
                    }
                    break;
                case GAME_AREA_SHELTER_B6_NURSERY:
                    gGameFlowLoadCaption = &_gGameFlowLoadCaptionShelterExperiment;
                    imageKey.fileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_NURSERY;
                    break;
                case GAME_AREA_NEO_ARK_R26:
                    gGameFlowLoadCaption                = &_gGameFlowLoadCaptionWhiteHouseEvening;
                    gGameFlowLegacySecondaryLoadCaption = &_gGameFlowLegacySecondaryLoadCaptionWhiteHouseEvening;
                    imageKey.fileIndex                  = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
                    break;
            }
            break;
    }
    // The queue copies the key and all four option bytes before these locals expire.
    loadOptions.fileIdHundreds   = 0;
    loadOptions.loadMode         = CD_COMMAND_LOAD_DEFAULT;
    loadOptions.imageXPageOffset = 0;
    loadOptions.imageYOffset     = 0;
    gGameFlowLoadImageSlot       = cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &imageKey, &loadOptions);
}

void gameFlowStepLoadScreenPresentation(TextStream* primaryCaption, TextStream* legacySecondaryCaption)
{
    enum {
        GAME_FLOW_LOAD_PRESENTATION_BEGIN     = 0,
        GAME_FLOW_LOAD_PRESENTATION_REVEAL    = 1,
        GAME_FLOW_LOAD_PRESENTATION_CAPTION   = 2,
        GAME_FLOW_LOAD_PRESENTATION_HOLD      = 3,
        GAME_FLOW_LOAD_PRESENTATION_FADE_OUT  = 4,
        GAME_FLOW_LOAD_SECONDARY_DELAY_FRAMES = 61,
        GAME_FLOW_LOAD_CAPTION_HOLD_FRAMES    = 60,
        GAME_FLOW_LOAD_FADE_COLOR_STEP        = 16,
        GAME_FLOW_LOAD_PHASE_IDLE             = 0,
        GAME_FLOW_LOAD_CAPTION_END_REACHED    = -1,
    };
    CdCmdQueue* queue;
    TextStream* secondaryCaption;
    s32         secondaryRevealStatus;
    s32         primaryRevealStatus;

    queue = &gCdCmdQueue;
    // Retain the suppressed secondary-caption path and its original timing.
    secondaryCaption = NULL;
    switch (gGameFlowLoadScreenPresentationStep) {
        case GAME_FLOW_LOAD_PRESENTATION_BEGIN:
            _gGameFlowLoadCaptionElapsedFrames               = 0;
            _gGameFlowLegacySecondaryLoadCaptionPhase        = GAME_FLOW_LOAD_CAPTION_PHASE_ARM;
            _gGameFlowLoadCaptionPhase                       = GAME_FLOW_LOAD_CAPTION_PHASE_ARM;
            _gGameFlowLegacySecondaryLoadCaptionFramesLeft   = 0;
            _gGameFlowLoadCaptionFramesLeft                  = 0;
            _gGameFlowLegacySecondaryLoadCaptionRevealStatus = GAME_FLOW_LOAD_CAPTION_STATUS_PROGRESS;
            _gGameFlowLoadCaptionRevealStatus                = GAME_FLOW_LOAD_CAPTION_STATUS_PROGRESS;
            _fadeBeginBootImageReveal();
            gGameFlowLoadScreenPresentationStep++;
            /* fallthrough */
        case GAME_FLOW_LOAD_PRESENTATION_REVEAL:
            if ((_fadeBootImageFromBlack(GAME_FLOW_LOAD_FADE_COLOR_STEP) & 0xFFFF) != 0) {
                gGameFlowLoadScreenPresentationStep++;
            }
            return;
        case GAME_FLOW_LOAD_PRESENTATION_CAPTION:
            if (_gGameFlowLoadCaptionRevealStatus < 0) {
                _gGameFlowLoadCaptionFramesLeft = GAME_FLOW_LOAD_CAPTION_REDRAW_COUNTDOWN;
            }
            primaryRevealStatus = textDrawStream(primaryCaption, &_gGameFlowLoadCaptionPhase, &_gGameFlowLoadCaptionFramesLeft, 0);
            if (_gGameFlowLoadCaptionRevealStatus >= 0) {
                _gGameFlowLoadCaptionRevealStatus = primaryRevealStatus;
            }
            if (_gGameFlowLoadCaptionElapsedFrames >= GAME_FLOW_LOAD_SECONDARY_DELAY_FRAMES) {
                if (_gGameFlowLegacySecondaryLoadCaptionRevealStatus < 0) {
                    _gGameFlowLegacySecondaryLoadCaptionFramesLeft = GAME_FLOW_LOAD_CAPTION_REDRAW_COUNTDOWN;
                }
                if (secondaryCaption != NULL) {
                    secondaryRevealStatus = textDrawStream(secondaryCaption, &_gGameFlowLegacySecondaryLoadCaptionPhase, &_gGameFlowLegacySecondaryLoadCaptionFramesLeft, 0);
                } else {
                    _gGameFlowLegacySecondaryLoadCaptionRevealStatus = GAME_FLOW_LOAD_CAPTION_END_REACHED;
                }
                if (_gGameFlowLegacySecondaryLoadCaptionRevealStatus >= 0) {
                    _gGameFlowLegacySecondaryLoadCaptionRevealStatus = secondaryRevealStatus;
                }
            } else {
                _gGameFlowLoadCaptionElapsedFrames++;
            }
            if (_gGameFlowLegacySecondaryLoadCaptionRevealStatus < 0 && _gGameFlowLoadCaptionRevealStatus < 0) {
                _gGameFlowLoadCaptionElapsedFrames = 0;
                gGameFlowLoadScreenPresentationStep++;
            }
            return;
        case GAME_FLOW_LOAD_PRESENTATION_HOLD:
            if (_gGameFlowLoadCaptionElapsedFrames >= GAME_FLOW_LOAD_CAPTION_HOLD_FRAMES) {
                if (queue->holdBootImage == 0) {
                    _gFadeBootImageMaskLevel = 0;
                    gGameFlowLoadScreenPresentationStep++;
                }
            } else {
                _gGameFlowLoadCaptionElapsedFrames++;
            }
            break;
        case GAME_FLOW_LOAD_PRESENTATION_FADE_OUT:
            if ((_fadeBootImageToBlack(GAME_FLOW_LOAD_FADE_COLOR_STEP) & 0xFFFF) != 0) {
                gGameFlowLoadScreenPhase   = GAME_FLOW_LOAD_PHASE_IDLE;
                gCdCmdQueue.bootLoadActive = 0;
            }
            break;
        default:
            return;
    }
    _gGameFlowLegacySecondaryLoadCaptionFramesLeft = GAME_FLOW_LOAD_CAPTION_REDRAW_COUNTDOWN;
    _gGameFlowLoadCaptionFramesLeft                = GAME_FLOW_LOAD_CAPTION_REDRAW_COUNTDOWN;
    // Keep the revealed caption visible throughout the hold and fade-out.
    textDrawStream(primaryCaption, &_gGameFlowLoadCaptionPhase, &_gGameFlowLoadCaptionFramesLeft, 0);
    if (secondaryCaption != NULL) {
        textDrawStream(secondaryCaption, &_gGameFlowLegacySecondaryLoadCaptionPhase, &_gGameFlowLegacySecondaryLoadCaptionFramesLeft, 0);
    }
}
