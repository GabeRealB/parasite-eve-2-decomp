#include "text.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "main/display.h"
#include "display.h"
#include "main/text.h"

/// Ordering-table tags used by the caption and fade-tile page commands.
enum {
    PRIMITIVE_CAPTION_OT_INDEX = 4,
    PRIMITIVE_FADE_OT_INDEX    = 5,
};

static void _primDrawCaptionSprite(const PrimDrawParams* draw, u32 clutX, s32 clutY);

static void _primDrawTexturePage(s32 blendMode, s32 tpageX, s32 tpageY, s32 otIndex);

static s32 _primDrawTimedDimTile(const RECT* rect, const u8* phase, s16* framesLeft);

static void _primDrawTile(const PrimDrawParams* draw);

TextGlyphCell Caption_Glyphs[] = {
#include "assets/caption_glyphs.inc"
};

s32 textDrawStream(TextStream* stream, u8* phase, s16* framesLeft, s32 skipMarkedGlyphs)
{
    enum {
        TEXT_STREAM_PHASE_ARM           = 0,
        TEXT_STREAM_PHASE_REVEAL        = 1,
        TEXT_STREAM_STATUS_PROGRESS     = 0,
        TEXT_STREAM_STATUS_END_REACHED  = -1,
        TEXT_STREAM_STATUS_COMPLETE     = 1,
        TEXT_STREAM_TEXTURE_SHADE_UNITY = 0x80,
        TEXT_STREAM_PAGE_U_MASK         = 0x3F
    };
    PrimDrawParams draw;
    s32            revealStatus;
    s32            byteIndex;
    s32            glyphIndex;
    u8             scriptByte;

    /// Queues the revealed prefix, retaining pen advance for filtered glyphs.
    ///
    /// Captures stream, draw, skipMarkedGlyphs, byteIndex, glyphIndex and scriptByte.
    /// Requires initialized draw attributes and enough primitive/OT storage.
#define TEXT_DRAW_STREAM_PREFIX()                                                                        \
    {                                                                                                    \
        for (byteIndex = 0; byteIndex < stream->cursor; byteIndex++) {                                   \
            scriptByte = stream->chars[byteIndex];                                                       \
            if (scriptByte == TEXT_STREAM_LINE_BREAK) {                                                  \
                draw.x  = stream->x;                                                                     \
                draw.y += stream->lineHeight;                                                            \
            } else if (scriptByte != TEXT_STREAM_END) {                                                  \
                glyphIndex = scriptByte & TEXT_STREAM_GLYPH_INDEX_MASK;                                  \
                if (((s8)scriptByte >= 0) || (skipMarkedGlyphs == 0)) {                                  \
                    draw.u = stream->glyphs[glyphIndex].u +                                              \
                             (stream->tpageX & TEXT_STREAM_PAGE_U_MASK);                                 \
                    draw.v = stream->glyphs[glyphIndex].v +                                              \
                             (u8)stream->tpageY;                                                         \
                    draw.w = stream->glyphs[glyphIndex].width;                                           \
                    draw.h = stream->glyphs[glyphIndex].height;                                          \
                    if (draw.h != 0) {                                                                   \
                        _primDrawCaptionSprite(&draw, stream->clutX,                                     \
                                               stream->clutY);                                           \
                    }                                                                                    \
                }                                                                                        \
                draw.x +=                                                                                \
                    stream->glyphs[glyphIndex].width;                                                    \
            }                                                                                            \
        }                                                                                                \
        _primDrawTexturePage(GPU_BLEND_ADD, stream->tpageX, stream->tpageY, PRIMITIVE_CAPTION_OT_INDEX); \
    }

    revealStatus = TEXT_STREAM_STATUS_PROGRESS;
    switch (*phase) {
        case TEXT_STREAM_PHASE_ARM:
            // Start the reveal. A negative per-glyph delay shows every byte
            // before the terminator and holds the finished caption.
            stream->cursor = 0;
            if (stream->charDelay < 0) {
                byteIndex = 0;
                if (*stream->chars != TEXT_STREAM_END) {
                    do {
                        byteIndex++;
                        stream->cursor++;
                    } while (stream->chars[byteIndex] != TEXT_STREAM_END);
                }
                *framesLeft = stream->delayReload;
            } else {
                *framesLeft = stream->charDelay;
            }
            (*phase)++;
            break;
        case TEXT_STREAM_PHASE_REVEAL:
            // Draw the revealed prefix. Step the cursor once the countdown expires.
            draw.x         = stream->x;
            draw.y         = stream->y;
            draw.u         = stream->tpageX;
            draw.v         = stream->tpageY;
            draw.r         = TEXT_STREAM_TEXTURE_SHADE_UNITY;
            draw.g         = TEXT_STREAM_TEXTURE_SHADE_UNITY;
            draw.b         = TEXT_STREAM_TEXTURE_SHADE_UNITY;
            draw.semiTrans = 0;
            draw.unused_12 = ONE;
            if (stream->chars[stream->cursor - 1] != TEXT_STREAM_END) {
                TEXT_DRAW_STREAM_PREFIX();
                *framesLeft = *framesLeft - 1;
                if (*framesLeft < 0) {
                    stream->cursor = stream->cursor + 1;
                    if (stream->chars[stream->cursor] == TEXT_STREAM_END) {
                        revealStatus = TEXT_STREAM_STATUS_END_REACHED;
                        *framesLeft  = stream->delayReload;
                    } else {
                        *framesLeft = stream->charDelay;
                    }
                }
            } else {
                (*phase)++;
            }
            break;
        default:
            revealStatus = TEXT_STREAM_STATUS_COMPLETE;
            break;
    }
    return revealStatus;
#undef TEXT_DRAW_STREAM_PREFIX
}

/// Queues a caption sprite using a borrowed rectangle, texture origin and CLUT.
///
/// Consumes one aligned SPRT slot and OT tag 4, retained through GPU completion.
/// CLUT X is a 16-word-aligned VRAM coordinate (0..1008); Y is a row (0..511).
/// X keeps its unsigned packing shift. UV narrows to bytes; packet dimensions
/// are the supplied pixel width/height minus one, narrowed to halfwords.
/// Opaque sprites use raw texture colour; semitransparent sprites are modulated.
/// The caller must also queue the appropriate texture page at the caption tag.
static void _primDrawCaptionSprite(const PrimDrawParams* draw, u32 clutX, s32 clutY)
{
    SPRT* sprite;
    u8    textureV;

    sprite            = (SPRT*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(sprite + 1);
    SetSprt(sprite);
    if (draw->semiTrans == 0) {
        SetShadeTex(sprite, 1);
        SetSemiTrans(sprite, 0);
    } else {
        SetShadeTex(sprite, 0);
        SetSemiTrans(sprite, 1);
    }
    sprite->r0   = draw->r;
    sprite->g0   = draw->g;
    sprite->b0   = draw->b;
    sprite->x0   = draw->x;
    sprite->y0   = draw->y;
    sprite->u0   = draw->u;
    textureV     = draw->v;
    sprite->clut = getClut(clutX, clutY);
    sprite->v0   = textureV;
    sprite->w    = draw->w - 1;
    sprite->h    = draw->h - 1;
    AddPrim(gGpuCurrentOt + PRIMITIVE_CAPTION_OT_INDEX, sprite);
}

/// Queues a 4-bit texture-page and blend-mode command for subsequent primitives.
///
/// `blendMode` is the unshifted GPU blend selector (0..3). VRAM X/Y are words
/// and rows; all three inputs narrow to signed halfwords at the SDK boundary.
/// `otIndex` must select a current ordering-table tag (callers use 4 or 5).
/// Requires one word-aligned DR_TPAGE slot at the primitive cursor, retained
/// until GPU completion. Enables drawing into the display area and disables dithering.
static void _primDrawTexturePage(s32 blendMode, s32 tpageX, s32 tpageY, s32 otIndex)
{
    enum { PRIMITIVE_TEXTURE_DEPTH_4BIT = 0 };
    DR_TPAGE* pagePacket;

    pagePacket        = (DR_TPAGE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(pagePacket + 1);
    SetDrawTPage(pagePacket, true, false, GetTPage(PRIMITIVE_TEXTURE_DEPTH_4BIT, (s16)blendMode, (s16)tpageX, (s16)tpageY));
    AddPrim(gGpuCurrentOt + otIndex, pagePacket);
}

/// Draws a black average-blended rectangle and advances its callback countdown.
///
/// Retained standalone helper with no callers. Phase zero draws and subtracts
/// one from the signed halfword countdown, returning 0 while it remains positive;
/// any other phase returns 1 without drawing. Phase is borrowed and never changed.
/// Phase must be readable; rect/countdown need valid storage only in phase zero.
/// The subtraction narrows back to s16 before testing; callers must avoid wrap.
/// Rectangle dimensions use the tile emitter's minus-one pixel convention.
/// Requires one TILE and DR_TPAGE in the aligned system arena and writable OT
/// tag 5; packets remain live through GPU completion. Returns 1 on completion.
static s32 _primDrawTimedDimTile(const RECT* rect, const u8* phase, s16* framesLeft)
{
    enum {
        PRIMITIVE_DIM_PHASE_DRAW = 0,
        PRIMITIVE_DIM_PROGRESS   = 0,
        PRIMITIVE_DIM_COMPLETE   = 1,
    };
    PrimDrawParams draw;
    s32            dimStatus;

    /// Builds the untextured black rectangle used by this timed dimming overlay.
    ///
    /// Captures local draw and the borrowed rect pointer, reading rect four times.
    /// Writes only fields consumed by the tile emitter; other members stay untouched.
#define PRIMITIVE_INIT_BLACK_DIM_TILE() \
    {                                   \
        draw.x         = rect->x;       \
        draw.y         = rect->y;       \
        draw.w         = rect->w;       \
        draw.h         = rect->h;       \
        draw.b         = 0;             \
        draw.g         = 0;             \
        draw.r         = 0;             \
        draw.semiTrans = true;          \
    }

    dimStatus = PRIMITIVE_DIM_PROGRESS;
    switch (*phase) {
        case PRIMITIVE_DIM_PHASE_DRAW:
            PRIMITIVE_INIT_BLACK_DIM_TILE();
            _primDrawTile(&draw);
            // OT insertion prepends the blend command so it runs before the tile.
            _primDrawTexturePage(GPU_BLEND_AVERAGE, 0, 0, PRIMITIVE_FADE_OT_INDEX);
            *framesLeft = *framesLeft - 1;
            if (*framesLeft > 0) {
                break;
            }
            // Fall through when the countdown completes.
        default:
            dimStatus = PRIMITIVE_DIM_COMPLETE;
            break;
    }
    return dimStatus;
#undef PRIMITIVE_INIT_BLACK_DIM_TILE
}

/// Sets the SDK packet flags for an opaque or semitransparent tile.
///
/// Borrows a writable TILE; nonzero semiTrans enables blending and clears the
/// raw-texture flag, while zero does the reverse. The latter flag has no texture
/// to affect on an untextured tile, but its packet bit is retained.
static inline void _primSetTileBlend(TILE* tile, s32 semiTrans)
{
    if (semiTrans == 0) {
        SetShadeTex(tile, 1);
        SetSemiTrans(tile, 0);
    } else {
        SetShadeTex(tile, 0);
        SetSemiTrans(tile, 1);
    }
}

/// Queues an untextured coloured rectangle at the fade ordering-table tag.
///
/// Borrows draw only for this call. X/Y are draw-environment pixels; packet
/// width/height are w-1/h-1 narrowed to halfwords (positive dimensions are the
/// normal domain). UV and unused fields are not read. Nonzero semiTrans enables
/// blending; the caller must prepend the desired draw-mode command at tag 5.
/// Requires one word-aligned TILE slot in Gpu_SysPrimCursor and writable OT tag 5.
/// The arena is byte-addressed; the queued packet is borrowed until GPU completion.
static void _primDrawTile(const PrimDrawParams* draw)
{
    TILE* tile;

    tile              = (TILE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(tile + 1);
    SetTile(tile);
    _primSetTileBlend(tile, draw->semiTrans);
    tile->r0 = draw->r;
    tile->g0 = draw->g;
    tile->b0 = draw->b;
    tile->x0 = draw->x;
    tile->y0 = draw->y;
    tile->w  = draw->w - 1;
    tile->h  = draw->h - 1;
    AddPrim(gGpuCurrentOt + PRIMITIVE_FADE_OT_INDEX, tile);
}
