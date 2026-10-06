#include "cap_captions.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/strings.h>

#include "types.h"

#include "gameplay/cap.h"
#include "gameplay/captions.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_targets.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"

static void CapCaption_RunSchedule(Task* task);

#ifndef CAP_CAPTION_DRAW_CURRENT_LINKAGE
#define CAP_CAPTION_DRAW_CURRENT_LINKAGE static
#endif
CAP_CAPTION_DRAW_CURRENT_LINKAGE void CapCaption_DrawCurrent(void);
static s32                            CapCaption_Relocate(CapFile* file);
/* The script selector and the caption drawer are file-local unless another
 * image calls this copy: a carrier whose copy is called from outside binds the
 * linkage to nothing and the name to its own exported one. */
#ifndef CAP_CAPTION_SELECT_SCRIPT_LINKAGE
#define CAP_CAPTION_SELECT_SCRIPT_LINKAGE static
#endif
CAP_CAPTION_SELECT_SCRIPT_LINKAGE s32 CapCaption_SelectScript(s16 arg0, s16 arg1, s32 arg2);
static s32                            CapCaption_DrawText(const u16* arg0, s32 arg1, s32 arg2, s32 arg3);
static s16                            CapCaption_TextTopY(const u16* arg0);
static void                           CapCaption_DrawCaret(void);
static s16                            CapCaption_CenterX(const u16* arg0);
static s16                            CapCaption_CenterLineX(const u16* arg0, s32 arg1);
static s16                            CapCaption_TextHeight(const u16* arg0);
static s32                            CapCaption_LineHeight(const u16* arg0);
static s32                            CapCaption_FindKeyedLine(s32 arg0);
static void                           CapCaption_TimedTask(Task* task);
static void                           CapCaption_CancelableTask(Task* task);
static void                           CapCaption_ShowModal(s16 arg0, s16 arg1, s16 arg2);

/// Plays scheduled captions as the scene clock counts down.
///
/// Schedule bounds use units of 30 scene-clock frames. The task's `spawnArg1`
/// supplies the per-line delay in frames. The first containing window supplies
/// both the script and its line key.
static void CapCaption_RunSchedule(Task* task)
{
    s32 i;
    s32 script;
    s32 key;
    s32 time;

    switch (task->state) {
        case CAP_CAPTION_SCHEDULE_INIT:
            task->state = CAP_CAPTION_SCHEDULE_RUNNING;
            break;
        case CAP_CAPTION_SCHEDULE_RUNNING:
            script = 0;
            // A selected window always supplies its key before script playback.
            for (i = 0; CapCaption_Data_80154514[i].upper != CAP_CAPTION_SCHEDULE_END; i++) {
                time = gGameSession->sceneClock;
                if ((CapCaption_Data_80154514[i].upper * CAP_CAPTION_SCHEDULE_FRAMES_PER_UNIT >= time) &&
                    (CapCaption_Data_80154514[i].lower * CAP_CAPTION_SCHEDULE_FRAMES_PER_UNIT < time)) {
                    script = CapCaption_Data_80154514[i].commandIndex;
                    key    = CapCaption_Data_80154514[i].key;
                    break;
                }
            }
            if (script != 0) {
                CapCaption_SelectScript(script, key, (s16)task->spawnArg1.value);
                CapCaption_DrawCurrent();
            }
            if ((capIsBusy() == 0) && (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING)) {
                gGameSession->sceneClock = (u16)gGameSession->sceneClock - 1;
            }
            break;
    }
}

CAP_CAPTION_DRAW_CURRENT_LINKAGE void CapCaption_DrawCurrent(void)
{
    if ((CapCaption_Data_8015E658 != NULL) &&
        (CapCaption_Data_8015E658[CapCaption_Data_8015E662].textRef.offset != CAP_TEXT_REF_END) &&
        (capIsBusy() == 0)) {
        CapCaption_DrawText(CapCaption_Data_8015E658[CapCaption_Data_8015E662].textRef.text, 0x80, 1,
                            CapCaption_Data_8015E658[CapCaption_Data_8015E662].control.text.title |
                                ((CapCaption_Data_8015E658[CapCaption_Data_8015E662].control.text.flags & CAP_SEQUENCE_TITLE_BANK) * 0x10));
        if (!(CapCaption_Data_8015E658[CapCaption_Data_8015E662].trigger.soundAndTextFlags & CAP_SEQUENCE_INSTANT_TEXT)) {
            CapCaption_DrawCaret();
        }
    }
}

/// Relocates a caption file in place, the counterpart of gameplay's
/// `Gp_RelocCapFile`, and publishes its glyph and script tables. Returns 0
/// when the "CAP" magic is missing.
static s32 CapCaption_Relocate(CapFile* file)
{
    s32                i;
    s32                count;
    s32                flag;
    CapSequenceRecord* rec;
    CapCommandRef*     ptr;
    CapSequenceTable*  sequenceTable;
    CapCommandTable*   commandTable;

    if (strncmp(file->magic, "CAP", 3) != 0) {
        return 0;
    }

    i = 0;
    if (file->glyphs.offset > 0) {
        file->glyphs.offset    += (u32)file;
        file->sequences.offset += (u32)file;
        file->commands.offset  += (u32)file;
        sequenceTable           = file->sequences.table;
        rec                     = sequenceTable->records;
        count                   = sequenceTable->count;
        if (count > 0) {
            flag = CAP_TEXT_REF_END;
            do {
                if (rec->textRef.offset != flag) {
                    rec->textRef.offset += (u32)file;
                } else {
                    rec++;
                }
                i++;
                rec++;
            } while (i < count);
        }
        commandTable = file->commands.table;
        i            = 0;
        count        = commandTable->count;
        ptr          = commandTable->entries;
        if (count > 0) {
            do {
                if (ptr->offset != 0) {
                    ptr->offset += (u32)file;
                }
                i++;
                ptr++;
            } while (i < count);
        }
    }

    CapCaption_Data_8015E654 = file->glyphs.cells;
    CapCaption_Data_8015E650 = (file->commands.table)->entries;
    return 1;
}

/// Starts playing the caption script `arg0` picks out of
/// `CapCaption_Data_8015E650`, keyed on `arg1`, and parks its per-line metrics in
/// the globals `CapCaption_DrawCurrent` reads. Returns 1 when there is no
/// such script, 0 once it is playing; `arg2` is the line delay.
CAP_CAPTION_SELECT_SCRIPT_LINKAGE s32 CapCaption_SelectScript(s16 arg0, s16 arg1, s32 arg2)
{
    CapSequenceRecord* caption;
    s16                entry;

    caption                  = CapCaption_Data_8015E650[arg0].sequence;
    CapCaption_Data_8015E658 = caption;
    if (caption == NULL) {
        return 1;
    }
    CapCaption_Data_8015E666    = arg1;
    entry                       = CapCaption_FindKeyedLine(1);
    CapCaption_Data_8015E662    = entry;
    CapCaption_Data_8015E660    = arg2;
    CapCaption_Data_8015E65C    = CapCaption_CenterX(CapCaption_Data_8015E658[entry].textRef.text);
    CapCaption_Data_8015E65E    = CapCaption_TextTopY(CapCaption_Data_8015E658[CapCaption_Data_8015E662].textRef.text);
    CapCaption_Data_8015E664    = CapCaption_TextHeight(CapCaption_Data_8015E658[CapCaption_Data_8015E662].textRef.text);
    CapCaption_Data_8015E66C[0] = 0x1E;
    return 0;
}

static s32 CapCaption_DrawText(const u16* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    const u16*     text;
    const u16*     body;
    s32            title;
    s16            sc;
    u32            shifted;
    s32            titleWidth;
    s16            lineIdx;
    s16            x;
    s32            y;
    s16            i;
    u16            code;
    s16            centered;
    s32            palette;
    s16            t;
    s16            t2;
    s16            glyphY;
    s32            top;
    POLY_G4*       bg;
    POLY_G4*       bg2;
    DR_MODE*       dm;
    POLY_FT4*      ft;
    POLY_GT4*      gt;
    POLY_GT4*      gt2;
    TextGlyphCell* icon;

    lineIdx = 0;
    title   = arg3;
    text    = arg0;
    x       = CapCaption_CenterLineX(arg0, 0) - 0xA0;
    y       = (u16)CapCaption_Data_8015E65E - 0x78;

    bg             = gGpuPrimCursor;
    gGpuPrimCursor = bg + 1;
    setlen(bg, 8);
    setcode(bg, 0x3A);
    setRGB0(bg, 0, 0, 0);
    setRGB1(bg, 0, 0, 0);
    setRGB2(bg, 0, 0x40, 0x20);
    setRGB3(bg, 0, 0x40, 0x20);
    bg->x0 = (u16)CapCaption_Data_8015E65C - 0xA7;
    bg->y0 = ((u16)CapCaption_Data_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)CapCaption_Data_8015E664;
    bg->x1 = (u16)CapCaption_Data_8015E65C - CapCaption_Data_8015E65C * 2 + 0xAB;
    bg->y1 = ((u16)CapCaption_Data_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)CapCaption_Data_8015E664;
    bg->x2 = (u16)CapCaption_Data_8015E65C - 0xA7;
    bg->y2 = ((u16)CapCaption_Data_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)CapCaption_Data_8015E664 + (u16)CapCaption_Data_8015E664;
    bg->x3 = (u16)CapCaption_Data_8015E65C - CapCaption_Data_8015E65C * 2 + 0xAB;
    bg->y3 = ((u16)CapCaption_Data_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)CapCaption_Data_8015E664 + (u16)CapCaption_Data_8015E664;
    addPrim(&gGpuCurrentOt[3], bg);
    bg2            = gGpuPrimCursor;
    gGpuPrimCursor = bg2 + 1;
    *bg2           = *bg;
    addPrim(&gGpuCurrentOt[3], bg2);
    dm             = gGpuPrimCursor;
    gGpuPrimCursor = dm + 1;
    setlen(dm, 1);
    dm->code[0] = 0xE100020A;
    addPrim(&gGpuCurrentOt[3], dm);

    body = text;
    if (title & 0xFF) {
        ft             = gGpuPrimCursor;
        gGpuPrimCursor = ft + 1;
        setlen(ft, 9);
        setcode(ft, 0x2D);
        title      = title - 1;
        top        = ((u16)CapCaption_Data_8015E660 - 0x77) - (u16)CapCaption_Data_8015E664;
        ft->x0     = (u16)CapCaption_Data_8015E65C - 0xA7;
        ft->y0     = (top - gDisplayState.vramYOffset) - CapCaption_Data_8015E654[title & 0xFF].height;
        titleWidth = CapCaption_Data_8015E654[title & 0xFF].width - 0xA7;
        ft->x1     = (u16)CapCaption_Data_8015E65C + titleWidth;
        ft->y1     = (top - gDisplayState.vramYOffset) - CapCaption_Data_8015E654[title & 0xFF].height;
        ft->x2     = (u16)CapCaption_Data_8015E65C - 0xA7;
        ft->y2     = top - gDisplayState.vramYOffset;
        titleWidth = CapCaption_Data_8015E654[title & 0xFF].width - 0xA7;
        ft->x3     = (u16)CapCaption_Data_8015E65C + titleWidth;
        ft->y3     = top - gDisplayState.vramYOffset;
        ft->u0     = CapCaption_Data_8015E654[title & 0xFF].u;
        ft->v0     = CapCaption_Data_8015E654[title & 0xFF].v;
        ft->u1     = CapCaption_Data_8015E654[title & 0xFF].u + CapCaption_Data_8015E654[title & 0xFF].width;
        ft->v1     = CapCaption_Data_8015E654[title & 0xFF].v;
        ft->u2     = CapCaption_Data_8015E654[title & 0xFF].u;
        ft->v2     = CapCaption_Data_8015E654[title & 0xFF].v + CapCaption_Data_8015E654[title & 0xFF].height;
        ft->u3     = CapCaption_Data_8015E654[title & 0xFF].u + CapCaption_Data_8015E654[title & 0xFF].width;
        ft->v3     = CapCaption_Data_8015E654[title & 0xFF].v + CapCaption_Data_8015E654[title & 0xFF].height;
        ft->clut   = 0x3D93;
        ft->tpage  = getTPage(0, 1, CapCaption_Data_801544EC, CapCaption_Data_801544EE);
        addPrim(&gGpuCurrentOt[2], ft);
    }

    centered = 1;
    i        = 0;
    while (1) {
        code    = body[i];
        shifted = (u32)code << 16;
        sc      = (s32)shifted >> 16;
        if (sc == -1) {
            break;
        }
        if (sc == -2) {
            t2                       = lineIdx + 1;
            lineIdx                  = t2;
            CapCaption_Data_8015E66A = y - 2;
            CapCaption_Data_8015E668 = x + 4;
            y                       += CapCaption_LineHeight(&body[i + 1]);
            if (centered != 0) {
                x = CapCaption_CenterLineX(arg0, t2) - 0xA0;
            } else {
                x = (u16)CapCaption_Data_8015E65C - 0xA0;
            }
            i++;
            continue;
        } else if (sc == -3) {
            x += 3;
            i++;
            continue;
        } else if ((code & 0xFF00) == 0x8400) {
            icon           = &D_8010FB70[code & 0xFF];
            ft             = gGpuPrimCursor;
            gGpuPrimCursor = ft + 1;
            setlen(ft, 9);
            setcode(ft, 0x2D);
            ft->clut  = 0x3C00;
            ft->tpage = 0x1E;
            t         = (y - gDisplayState.vramYOffset) + 1;
            ft->x0    = x;
            ft->y0    = t - icon->height;
            ft->x1    = x + icon->width;
            ft->y1    = t - icon->height;
            ft->x2    = x;
            ft->y2    = t;
            ft->x3    = x + icon->width;
            ft->y3    = t;
            ft->u0    = icon->u;
            ft->v0    = icon->v;
            ft->u1    = icon->u + icon->width;
            ft->v1    = icon->v;
            ft->u2    = icon->u;
            ft->v2    = icon->v + icon->height;
            ft->u3    = icon->u + icon->width;
            ft->v3    = icon->v + icon->height;
            addPrim(&gGpuCurrentOt[2], ft);
            x += icon->width;
            i++;
            continue;
        } else {
            palette        = (shifted >> 26) & 3;
            code           = code & 0x3FF;
            glyphY         = y - gDisplayState.vramYOffset;
            gt             = gGpuPrimCursor;
            gGpuPrimCursor = gt + 1;
            setcode(gt, 0x3C);
            setlen(gt, 12);
            setShadeTex(gt, 1);
            setRGB0(gt, 0x70, 0x70, 0x70);
            setRGB1(gt, 0x70, 0x70, 0x70);
            setRGB2(gt, 0x70, 0x70, 0x70);
            setRGB3(gt, 0x70, 0x70, 0x70);
            setSemiTrans(gt, 1);
            gt->clut  = palette | 0x3D50;
            gt->x0    = x;
            gt->tpage = getTPage(0, 1, CapCaption_Data_801544EC, CapCaption_Data_801544EE);
            gt->y0    = glyphY - CapCaption_Data_8015E654[code & 0x3FF].height;
            gt->x1    = x + CapCaption_Data_8015E654[code & 0x3FF].width;
            gt->y1    = glyphY - CapCaption_Data_8015E654[code & 0x3FF].height;
            gt->x2    = x;
            gt->y2    = glyphY;
            gt->x3    = x + CapCaption_Data_8015E654[code & 0x3FF].width;
            gt->y3    = glyphY;
            gt->u0    = CapCaption_Data_8015E654[code & 0x3FF].u;
            gt->v0    = CapCaption_Data_8015E654[code & 0x3FF].v;
            gt->u1    = CapCaption_Data_8015E654[code & 0x3FF].u + CapCaption_Data_8015E654[code & 0x3FF].width;
            gt->v1    = CapCaption_Data_8015E654[code & 0x3FF].v;
            gt->u2    = CapCaption_Data_8015E654[code & 0x3FF].u;
            gt->v2    = CapCaption_Data_8015E654[code & 0x3FF].v + CapCaption_Data_8015E654[code & 0x3FF].height;
            gt->u3    = CapCaption_Data_8015E654[code & 0x3FF].u + CapCaption_Data_8015E654[code & 0x3FF].width;
            gt->v3    = CapCaption_Data_8015E654[code & 0x3FF].v + CapCaption_Data_8015E654[code & 0x3FF].height;
            addPrim(&gGpuCurrentOt[2], gt);
            gt2            = gGpuPrimCursor;
            gGpuPrimCursor = gt2 + 1;
            *gt2           = *gt;
            gt2->tpage     = getTPage(0, GPU_BLEND_SUBTRACT, CapCaption_Data_801544EC, CapCaption_Data_801544EE);
            addPrim(&gGpuCurrentOt[2], gt2);
            x = CapCaption_Data_8015E654[(s16)code].width + x - 1;
        }
        i++;
    }
    return 0;
}

/// Top Y of the caption block the text stream `arg0` holds: every line after
/// the first `-2` adds its height (the tallest glyph's `height + 2`, or 2 when empty)
/// and the total is subtracted from `CapCaption_Data_8015E660`. Gameplay's
/// `capGetTextFirstBaselineY` is the same walk against a fixed 0xD0.
static s16 CapCaption_TextTopY(const u16* arg0)
{
    s16        lineH     = 0;
    s16        total     = 0;
    s16        i         = 0;
    s16        seenBreak = 0;
    const u16* text      = arg0;
    s16        code      = text[0];

    while (code != -1) {
        if (code == -2) {
            if (seenBreak) {
                if (lineH == 0) {
                    lineH = 2;
                }
                total += lineH;
            } else {
                seenBreak = 1;
            }
            lineH = 0;
        } else if (code != -3) {
            if (code >= 0) {
                if (lineH < CapCaption_Data_8015E654[code & 0x3FF].height + 2) {
                    lineH = CapCaption_Data_8015E654[code & 0x3FF].height + 2;
                }
            }
        }
        code = text[++i];
    }
    return CapCaption_Data_8015E660 - total;
}

/// Draws the pulsing "more text" caret: a Gouraud triangle at
/// (`CapCaption_Data_8015E668`, `CapCaption_Data_8015E66A`) whose grey level
/// ramps up to 15 and back down to 9. Same body as gameplay's `_capDrawContinueCaret`
/// without the VRAM Y offset.
static void CapCaption_DrawCaret(void)
{
    POLY_G3* prim;
    s32      c1;
    s32      c2;

    if (CapCaption_Data_8015E66C[0] != 0) {
        CapCaption_Data_8015E66C[0] -= 1;
        return;
    }
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG3(prim);
    c1 = (CapCaption_Data_801545E4 << 7) / 15;
    setRGB0(prim, c1, c1, c1);
    c1 = (CapCaption_Data_801545E4 * 192) / 15;
    c2 = c1;
    setRGB1(prim, c2, c2, c2);
    setRGB2(prim, c2, c2, c2);
    prim->x0 = CapCaption_Data_8015E668 + 3;
    prim->y0 = CapCaption_Data_8015E66A;
    prim->x1 = CapCaption_Data_8015E668;
    prim->x2 = CapCaption_Data_8015E668 + 7;
    prim->y1 = CapCaption_Data_8015E66A - 7;
    prim->y2 = CapCaption_Data_8015E66A - 7;
    addPrim(&gGpuCurrentOt[2], prim);
    if (CapCaption_Data_801545E8 == 0) {
        CapCaption_Data_801545E4 += 1;
        if (CapCaption_Data_801545E4 >= 0xF) {
            CapCaption_Data_801545E8 = 1;
        }
    } else {
        CapCaption_Data_801545E4 -= 1;
        if (CapCaption_Data_801545E4 < 9) {
            CapCaption_Data_801545E8 = 0;
        }
    }
}

/// Horizontal centring offset of the caption line the text stream `arg0`
/// starts with: the widest line's pixel width subtracted from the 0x140 screen
/// width, halved, minus 5. The walk is the one `CapCaption_LineHeight`
/// makes, and gameplay's `_capGetTextBlockLeftX` compiles to the same 0x110 bytes with
/// only the glyph table symbol differing — `-2` closes a line and keeps the
/// running maximum, `-3` and `0x8400`-masked codes indent it by 3 and 0x10, and
/// each glyph code (non-negative, `& 0x3FF` indexing `CapCaption_Data_8015E654`)
/// advances it by that glyph's `width - 1`.
static s16 CapCaption_CenterX(const u16* arg0)
{
    s16 lineW;
    s16 maxW;
    s16 i;
    s16 code;

    lineW = 0;
    maxW  = 0;
    i     = 0;
    code  = arg0[0];
    while (code != -1) {
        if (code == -2) {
            if (lineW > maxW) {
                maxW = lineW;
            }
            lineW = 0;
            code  = arg0[++i];
        } else if (code == -3) {
            lineW += 3;
            code   = arg0[++i];
        } else if ((code & 0xFF00) == 0x8400) {
            lineW += 0x10;
            code   = arg0[++i];
        } else if (code >= 0) {
            lineW += CapCaption_Data_8015E654[code & 0x3FF].width - 1;
            code   = arg0[++i];
        } else {
            code = arg0[++i];
        }
    }
    return (0x140 - maxW) / 2 - 5;
}

/// Horizontal centring offset of line `arg1` of the caption text stream
/// `arg0`: that line's pixel width subtracted from 0x140, halved, minus 5.
/// Same walk as `CapCaption_CenterX`, but keeps the width of the
/// selected line instead of the widest; gameplay's `_capGetTextLineLeftX`
/// compiles to the same bytes.
static s16 CapCaption_CenterLineX(const u16* arg0, s32 arg1)
{
    s16 lineW;
    s16 selectedW;
    s16 i;
    s16 lineIndex;
    s16 code;

    lineW     = 0;
    selectedW = 0;
    i         = 0;
    lineIndex = 0;
    code      = arg0[0];
    while (code != -1) {
        if (code == -2) {
            if (lineIndex == arg1) {
                selectedW = lineW;
            }
            lineW = 0;
            i++;
            lineIndex++;
            code = arg0[i];
        } else if (code == -3) {
            lineW += 3;
            code   = arg0[++i];
        } else if ((code & 0xFF00) == 0x8400) {
            lineW += 0x10;
            code   = arg0[++i];
        } else if (code >= 0) {
            lineW += CapCaption_Data_8015E654[code & 0x3FF].width - 1;
            code   = arg0[++i];
        } else {
            code = arg0[++i];
        }
    }
    return (0x140 - selectedW) / 2 - 5;
}

/// Total height of the caption block the text stream `arg0` holds: every `-2`
/// line break adds the line's height (the tallest glyph's `height + 2`, or 2 when
/// the line is empty). Gameplay's `capGetTextBlockHeight` is the same walk plus a
/// final `2 -> 0` clamp.
static s16 CapCaption_TextHeight(const u16* arg0)
{
    s16 lineH = 0;
    s16 total = 0;
    s16 i     = 0;
    s16 code  = arg0[0];

    while (code != -1) {
        if (code == -2) {
            if (lineH == 0) {
                lineH = 2;
            }
            total += lineH;
            lineH  = 0;
        } else if (code != -3) {
            if (code >= 0) {
                if (lineH < CapCaption_Data_8015E654[code & 0x3FF].height + 2) {
                    lineH = CapCaption_Data_8015E654[code & 0x3FF].height + 2;
                }
            }
        }
        code = arg0[++i];
    }
    return total;
}

/// Height of the caption line the text stream `arg0` starts with, walking it
/// the way gameplay's `_capGetTextLineAdvance` does — this overlay's caption system is
/// a copy of that one, and the two functions compile to the same 0xB8 bytes
/// with only the glyph table symbol differing.
///
/// The running maximum starts at 0 and each glyph code (non-negative, `& 0x3FF`
/// indexing `CapCaption_Data_8015E654`) raises it to that glyph's `height + 2`. Either
/// terminator ends the scan: `-2` leaves the maximum as it stands, `-1` forces
/// 0xD, and any other negative code is stepped over like a glyph without
/// touching the maximum. A maximum still at 0 — the stream opened with `-2` —
/// comes back as 2.
static s32 CapCaption_LineHeight(const u16* arg0)
{
    s16 height = 0;
    s16 i      = 0;
    s16 cont   = 1;
    s16 code   = arg0[0];

    do {
        if (code == -2) {
            cont = 0;
        } else if (code == -1) {
            cont   = 0;
            height = 0xD;
        } else if (code >= 0) {
            if (height < CapCaption_Data_8015E654[code & 0x3FF].height + 2) {
                height = CapCaption_Data_8015E654[code & 0x3FF].height + 2;
            }
            code = arg0[++i];
        } else {
            code = arg0[++i];
        }
    } while (cont);
    if (height == 0) {
        height = 2;
    }
    return height;
}

static s32 CapCaption_FindKeyedLine(s32 arg0)
{
    CapSequenceRecord* record;

    for (;;) {
        record = _capSequenceRecordAt(CapCaption_Data_8015E658, arg0);
        if (record->textRef.offset != CAP_TEXT_REF_END && record->key != CapCaption_Data_8015E666) {
            arg0++;
        } else {
            break;
        }
    }
    return arg0;
}

static void CapCaption_TimedTask(Task* task)
{
    s32 remaining;

    remaining             = task->spawnArg1.value - 1;
    task->spawnArg1.value = remaining;
    if (remaining <= 0) {
        taskKill(task);
    }
    CapCaption_DrawCurrent();
}

static void CapCaption_CancelableTask(Task* task)
{
    s32 remaining;
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            remaining             = task->spawnArg1.value - 1;
            task->spawnArg1.value = remaining;
            if ((remaining <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0)) {
                taskKill(task);
                stageRequestModeTaskExit();
            }
            break;
    }
    CapCaption_DrawCurrent();
}

static inline void CapCaption_ShowTimed(s16 arg0, s16 arg1, s16 arg2)
{
    CapCaption_SelectScript(arg0, arg1, 0xD0);
    taskSpawnFromTable(&CapCaption_Data_801544FC, 0, (s32)(arg2), 0);
}
