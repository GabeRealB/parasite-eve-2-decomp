#include "text.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "main/display.h"
#include "display.h"
#include "main/text.h"

static void Prim_DrawSprt(PrimDrawParams* draw, u32 arg1, s32 arg2);

static void Prim_DrawTPage(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

static s32 Prim_DrawFadeTile(RECT* rect, u8* arg1, s16* arg2);

static void Prim_DrawTile(PrimDrawParams* draw);

TextGlyphCell Caption_Glyphs[] = {
#include "assets/caption_glyphs.inc"
};

s32 TextStream_Draw(TextStream* stream, u8* arg1, s16* arg2, s32 arg3)
{
    PrimDrawParams sp;
    s32            ret;
    s32            i;
    s32            glyphIdx;
    u8             ch;
    s16            tmp6;
    s16            h;

    ret = 0;
    switch (*arg1) {
        case 0:
            // Start the reveal. A negative per-glyph delay shows every byte
            // before the terminator and holds the finished caption.
            stream->cursor = 0;
            if (stream->charDelay < 0) {
                i = 0;
                if (*stream->chars != TEXT_STREAM_END) {
                    do {
                        i++;
                        stream->cursor++;
                    } while (stream->chars[i] != TEXT_STREAM_END);
                }
                *arg2 = stream->delayReload;
            } else {
                *arg2 = stream->charDelay;
            }
            (*arg1)++;
            break;
        case 1:
            // Draw the revealed prefix. Step the cursor once the countdown expires.
            sp.x         = stream->x;
            sp.y         = stream->y;
            sp.u         = stream->tpageX;
            tmp6         = stream->tpageY;
            sp.r         = 0x80;
            sp.g         = 0x80;
            sp.b         = 0x80;
            sp.semiTrans = 0;
            sp.unused_12 = ONE;
            sp.v         = tmp6;
            if (stream->chars[stream->cursor - 1] != TEXT_STREAM_END) {
                for (i = 0; i < stream->cursor; i++) {
                    ch = stream->chars[i];
                    if (ch == TEXT_STREAM_LINE_BREAK) {
                        sp.x  = stream->x;
                        sp.y += stream->lineHeight;
                    } else if (ch != TEXT_STREAM_END) {
                        glyphIdx = ch & TEXT_STREAM_GLYPH_INDEX_MASK;
                        if (((s8)ch >= 0) || (arg3 == 0)) {
                            sp.u = stream->glyphs[glyphIdx].u +
                                   (stream->tpageX & 0x3F);
                            sp.v = stream->glyphs[glyphIdx].v +
                                   (u8)stream->tpageY;
                            sp.w = stream->glyphs[glyphIdx].width;
                            h    = stream->glyphs[glyphIdx].height;
                            sp.h = h;
                            if (h != 0) {
                                Prim_DrawSprt(&sp, stream->clutX,
                                              stream->clutY);
                            }
                        }
                        sp.x +=
                            stream->glyphs[glyphIdx].width;
                    }
                }
                Prim_DrawTPage(1, stream->tpageX, stream->tpageY, 4);
                *arg2 = *arg2 - 1;
                if (*arg2 < 0) {
                    stream->cursor = stream->cursor + 1;
                    if (stream->chars[stream->cursor] == TEXT_STREAM_END) {
                        ret   = -1;
                        *arg2 = stream->delayReload;
                    } else {
                        *arg2 = stream->charDelay;
                    }
                }
            } else {
                (*arg1)++;
            }
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

static void Prim_DrawSprt(PrimDrawParams* draw, u32 arg1, s32 arg2)
{
    SPRT* p;
    u8    v;

    p                 = (SPRT*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(p + 1);
    SetSprt(p);
    if (draw->semiTrans == 0) {
        SetShadeTex(p, 1);
        SetSemiTrans(p, 0);
    } else {
        SetShadeTex(p, 0);
        SetSemiTrans(p, 1);
    }
    p->r0   = draw->r;
    p->g0   = draw->g;
    p->b0   = draw->b;
    p->x0   = draw->x;
    p->y0   = draw->y;
    p->u0   = draw->u;
    v       = draw->v;
    p->clut = getClut(arg1, arg2);
    p->v0   = v;
    p->w    = draw->w - 1;
    p->h    = draw->h - 1;
    AddPrim(gGpuCurrentOt + 4, p);
}

static void Prim_DrawTPage(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    DR_TPAGE* p;

    p                 = (DR_TPAGE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(p + 1);
    SetDrawTPage(p, 1, 0, GetTPage(0, (s16)arg0, (s16)arg1, (s16)arg2) & 0xFFFF);
    AddPrim(gGpuCurrentOt + arg3, p);
}

static s32 Prim_DrawFadeTile(RECT* rect, u8* arg1, s16* arg2)
{
    PrimDrawParams sp;
    s32            ret;

    ret = 0;
    switch (*arg1) {
        case 0:
            sp.x         = rect->x;
            sp.y         = rect->y;
            sp.w         = rect->w;
            sp.h         = rect->h;
            sp.b         = 0;
            sp.g         = 0;
            sp.r         = 0;
            sp.semiTrans = 1;
            Prim_DrawTile(&sp);
            Prim_DrawTPage(0, 0, 0, 5);
            *arg2 = *arg2 - 1;
            if (*arg2 > 0) {
                break;
            }
            /* The countdown has run out: the fade is finished. */
        default:
            ret = 1;
            break;
    }
    return ret;
}

static void Prim_DrawTile(PrimDrawParams* draw)
{
    TILE* p;

    p                 = (TILE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(p + 1);
    SetTile(p);
    if (draw->semiTrans == 0) {
        SetShadeTex(p, 1);
        SetSemiTrans(p, 0);
    } else {
        SetShadeTex(p, 0);
        SetSemiTrans(p, 1);
    }
    p->r0 = draw->r;
    p->g0 = draw->g;
    p->b0 = draw->b;
    p->x0 = draw->x;
    p->y0 = draw->y;
    p->w  = draw->w - 1;
    p->h  = draw->h - 1;
    AddPrim(gGpuCurrentOt + 5, p);
}
