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

// CAP text words are read as signed halfwords for control-token dispatch.
enum {
    CAP_CAPTION_TEXT_END              = -1,
    CAP_CAPTION_TEXT_LINE_BREAK       = -2,
    CAP_CAPTION_TEXT_SPACER           = -3,
    CAP_CAPTION_TEXT_FAMILY_MASK      = 0xFF00,
    CAP_CAPTION_TEXT_ICON             = 0x8400,
    CAP_CAPTION_TEXT_GLYPH_INDEX_MASK = 0x3FF,
    CAP_CAPTION_TEXT_BYTE_MASK        = 0xFF,
    CAP_CAPTION_TEXT_SPACER_WIDTH     = 3,
    CAP_CAPTION_TEXT_ICON_WIDTH       = 16,
    CAP_CAPTION_TEXT_LINE_GAP         = 2,
    CAP_CAPTION_TEXT_END_LINE_ADVANCE = 13,
    CAP_CAPTION_TEXT_SCREEN_WIDTH     = 320,
    CAP_CAPTION_TEXT_LEFT_BIAS        = 5
};

// The caret draws its current level before stepping between these endpoints.
enum { CAP_CAPTION_CARET_PULSE_MIN = 8,
       CAP_CAPTION_CARET_PULSE_MAX = 15 };

static void CapCaption_RunSchedule(Task* task);

static bool _capCaptionRelocateFile(CapFile* file);
/* The script selector and the caption drawer are file-local unless another
 * image calls this copy: a carrier whose copy is called from outside binds the
 * linkage to nothing and the name to its own exported one. */
#ifndef CAP_CAPTION_SELECT_SCRIPT_LINKAGE
#define CAP_CAPTION_SELECT_SCRIPT_LINKAGE static
#endif
CAP_CAPTION_SELECT_SCRIPT_LINKAGE s32 CapCaption_SelectScript(s16 arg0, s16 arg1, s32 arg2);
static s32                            _capCaptionDrawText(const u16* textStream, s32 unusedDrawArg, s32 unusedRevealAll, s32 titleIndex);
static s16                            _capCaptionGetTextFirstBaselineY(const u16* textStream);
static void                           _capCaptionDrawContinueCaret(void);
static s16                            _capCaptionGetTextBlockLeftX(const u16* text);
static s16                            _capCaptionGetTextLineLeftX(const u16* text, s32 selectedLineIndex);
static s16                            _capCaptionGetTextBlockHeight(const u16* text);
static s32                            _capCaptionGetTextLineAdvance(const u16* text);
static s32                            _capCaptionFindRecordByKey(s32 recordIndex);
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
                CAP_CAPTION_DRAW_CURRENT();
            }
            if ((capIsBusy() == 0) && (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING)) {
                gGameSession->sceneClock = (u16)gGameSession->sceneClock - 1;
            }
            break;
    }
}

/// Queues the selected caption and, unless instant text is requested, its caret.
///
/// A null sequence, terminal record or busy gameplay CAP suppresses both draws.
/// Requires a relocated CAP resource, a valid selected record and cached pixel
/// metrics. Text/title cells and textures must remain loaded, and OT entries 2/3
/// and primitive storage must be writable through GPU completion. The title's
/// low byte selects no title (0) or glyph index + 1 (1..255); the published bank
/// bit is retained but ignored. Drawing does not advance the selected record.
/// Eligible caret calls consume its thirty-call delay, then draw and step its
/// pulse; suppressed or instant-text draws freeze the caret state.
CAP_CAPTION_DRAW_CURRENT_LINKAGE void CAP_CAPTION_DRAW_CURRENT(void)
{
    enum {
        CAP_CAPTION_RETAINED_DRAW_ARG            = 0x80,
        CAP_CAPTION_RETAINED_REVEAL_ARG          = 1,
        CAP_CAPTION_TITLE_BANK_TO_SELECTOR_SCALE = 0x10
    };

    if ((CAP_CAPTION_SEQUENCE != NULL) &&
        (CAP_CAPTION_SEQUENCE[CAP_CAPTION_RECORD_INDEX].textRef.offset != CAP_TEXT_REF_END) &&
        (capIsBusy() == 0)) {
        // The two retained arguments and title selector's bank bit are ignored.
        _capCaptionDrawText(CAP_CAPTION_SEQUENCE[CAP_CAPTION_RECORD_INDEX].textRef.text,
                            CAP_CAPTION_RETAINED_DRAW_ARG, CAP_CAPTION_RETAINED_REVEAL_ARG,
                            CAP_CAPTION_SEQUENCE[CAP_CAPTION_RECORD_INDEX].control.text.title |
                                ((CAP_CAPTION_SEQUENCE[CAP_CAPTION_RECORD_INDEX].control.text.flags & CAP_SEQUENCE_TITLE_BANK) * CAP_CAPTION_TITLE_BANK_TO_SELECTOR_SCALE));
        if (!(CAP_CAPTION_SEQUENCE[CAP_CAPTION_RECORD_INDEX].trigger.soundAndTextFlags & CAP_SEQUENCE_INSTANT_TEXT)) {
            _capCaptionDrawContinueCaret();
        }
    }
}

/// Advances the continuation caret's brightness pulse by one level.
///
/// `pulseLevel` is an integer scale in 8..15; the drawer converts level / 15
/// to separate vertex RGB intensities. `falling` is 0 while rising and 1 while
/// falling. The pointers borrow distinct writable s32 words for this call.
/// At 8 the direction must be rising, and at 15 falling. Reversal follows the
/// increment/decrement; no clamping is performed. The drawer advances this
/// state only after emitting a caret, so hidden or countdown calls freeze it.
static inline void _capCaptionStepCaretPulse(s32* pulseLevel, s32* falling)
{
    enum { CAP_CAPTION_CARET_PULSE_RISING  = 0,
           CAP_CAPTION_CARET_PULSE_FALLING = 1 };

    if (*falling == CAP_CAPTION_CARET_PULSE_RISING) {
        (*pulseLevel)++;
        if (*pulseLevel >= CAP_CAPTION_CARET_PULSE_MAX) {
            *falling = CAP_CAPTION_CARET_PULSE_FALLING;
        }
    } else {
        (*pulseLevel)--;
        if (*pulseLevel <= CAP_CAPTION_CARET_PULSE_MIN) {
            *falling = CAP_CAPTION_CARET_PULSE_RISING;
        }
    }
}

/// Relocates a loaded CAP file in place and selects its glyph and command tables.
///
/// Accepts any three-byte "CAP" prefix, returning false only on a magic mismatch.
/// The caller supplies a complete writable file with valid offsets and counts;
/// this does not validate its length. Tables borrow that storage for subsequent
/// caption selection, measurement and drawing, so the file must remain loaded.
/// A positive glyph offset triggers relocation; repeated calls with relocated
/// KSEG0 addresses only republish the tables and return true.
static bool _capCaptionRelocateFile(CapFile* file)
{
    /// Rebases a text reference, preserving terminal records and the next command.
    ///
    /// recordCursor is a writable CapSequenceRecord* local within fileBase,
    /// a complete writable CapFile*. endTextRef is CAP_TEXT_REF_END. Arguments
    /// have no side effects and are evaluated repeatedly. The caller counts
    /// this record then advances one more slot; the terminal's extra skip is
    /// uncounted and leaves the following command's contents unchanged.
#define CAP_CAPTION_RELOCATE_TEXT_RECORD(recordCursor, fileBase, endTextRef) \
    {                                                                        \
        if ((recordCursor)->textRef.offset != (endTextRef)) {                \
            (recordCursor)->textRef.offset += (u32)(fileBase);               \
        } else {                                                             \
            (recordCursor)++;                                                \
        }                                                                    \
    }

    s32                entryIndex;
    s32                sequenceRecordCount;
    s32                sequenceEndRef;
    s32                commandCount;
    CapSequenceRecord* record;
    CapCommandRef*     commandRef;
    CapSequenceTable*  sequenceTable;
    CapCommandTable*   commandTable;

    enum { CAP_CAPTION_MAGIC_PREFIX_BYTES = 3 };

    if (strncmp(file->magic, "CAP", CAP_CAPTION_MAGIC_PREFIX_BYTES) != 0) {
        return false;
    }

    entryIndex = 0;
    // Relocated KSEG0 addresses are negative in the signed offset word.
    if (file->glyphs.offset > 0) {
        // Rebase serialized byte offsets using the 32-bit file address.
        file->glyphs.offset    += (u32)file;
        file->sequences.offset += (u32)file;
        file->commands.offset  += (u32)file;
        sequenceTable           = file->sequences.table;
        record                  = sequenceTable->records;
        sequenceRecordCount     = sequenceTable->count;
        // Terminal records skip the next sequence command without relocating it.
        if (sequenceRecordCount > 0) {
            sequenceEndRef = CAP_TEXT_REF_END;
            do {
                CAP_CAPTION_RELOCATE_TEXT_RECORD(record, file, sequenceEndRef);
                entryIndex++;
                record++;
            } while (entryIndex < sequenceRecordCount);
#undef CAP_CAPTION_RELOCATE_TEXT_RECORD
        }
        commandTable = file->commands.table;
        entryIndex   = 0;
        commandCount = commandTable->count;
        commandRef   = commandTable->entries;
        if (commandCount > 0) {
            do {
                if (commandRef->offset != 0) {
                    commandRef->offset += (u32)file;
                }
                entryIndex++;
                commandRef++;
            } while (entryIndex < commandCount);
        }
    }

    // These tables stay owned by the loaded resource, including on repeat calls.
    CAP_CAPTION_GLYPH_CELLS  = file->glyphs.cells;
    CapCaption_Data_8015E650 = file->commands.table->entries;
    return true;
}

/// Starts playing the caption script `arg0` picks out of
/// `CapCaption_Data_8015E650`, keyed on `arg1`, and parks its per-line metrics in
/// the globals `CAP_CAPTION_DRAW_CURRENT` reads. Returns 1 when there is no
/// such script, 0 once it is playing; `arg2` is the line delay.
CAP_CAPTION_SELECT_SCRIPT_LINKAGE s32 CapCaption_SelectScript(s16 arg0, s16 arg1, s32 arg2)
{
    CapSequenceRecord* caption;
    s16                entry;

    caption              = CapCaption_Data_8015E650[arg0].sequence;
    CAP_CAPTION_SEQUENCE = caption;
    if (caption == NULL) {
        return 1;
    }
    CapCaption_Data_8015E666      = arg1;
    entry                         = _capCaptionFindRecordByKey(1);
    CAP_CAPTION_RECORD_INDEX      = entry;
    CAP_CAPTION_BOTTOM_BASELINE_Y = arg2;
    CAP_CAPTION_BLOCK_LEFT_X      = _capCaptionGetTextBlockLeftX(CAP_CAPTION_SEQUENCE[entry].textRef.text);
    CAP_CAPTION_FIRST_BASELINE_Y  = _capCaptionGetTextFirstBaselineY(CAP_CAPTION_SEQUENCE[CAP_CAPTION_RECORD_INDEX].textRef.text);
    CAP_CAPTION_BLOCK_HEIGHT      = _capCaptionGetTextBlockHeight(CAP_CAPTION_SEQUENCE[CAP_CAPTION_RECORD_INDEX].textRef.text);
    CAP_CAPTION_CARET_DRAWS_LEFT  = CAP_CAPTION_CARET_DELAY_DRAWS;
    return 0;
}

/// Queues the complete caption, its background and optional title, returning zero.
///
/// Requires previously selected block metrics, loaded glyph textures/palettes,
/// writable OT entries 2 and 3, and enough primitive storage through GPU completion.
/// Text is borrowed for this call and must be readable through the end token;
/// glyph indices use the low ten bits and icon selectors must be in 0..3.
/// Lines are centered in 320 screen pixels before conversion to draw coordinates.
/// Only the low byte of titleIndex is read (0 none, otherwise glyph index + 1).
/// The two unused arguments are retained interface slots; all text is drawn.
/// The end token must lie at word index 0..32767. Pen X, line indices and packet
/// coordinates retain halfword narrowing; used glyph/title indices must exist
/// in the selected file's glyph table.
static s32 _capCaptionDrawText(const u16* textStream, s32 unusedDrawArg, s32 unusedRevealAll, s32 titleIndex)
{
    enum {
        CAP_CAPTION_DRAW_ORIGIN_X                    = 160,
        CAP_CAPTION_DRAW_ORIGIN_Y                    = 120,
        CAP_CAPTION_BOX_LEFT_ORIGIN_X                = 167,
        CAP_CAPTION_BOX_RIGHT_ORIGIN_X               = 171,
        CAP_CAPTION_BOX_BOTTOM_ORIGIN_Y              = 119,
        CAP_CAPTION_BACKGROUND_GREEN                 = 64,
        CAP_CAPTION_BACKGROUND_BLUE                  = 32,
        CAP_CAPTION_BACKGROUND_DRAW_MODE             = 0xE100020A,
        CAP_CAPTION_TITLE_CLUT                       = 0x3D93,
        CAP_CAPTION_ICON_CLUT                        = 0x3C00,
        CAP_CAPTION_ICON_TPAGE                       = 0x1E,
        CAP_CAPTION_GLYPH_CLUT_BASE                  = 0x3D50,
        CAP_CAPTION_GLYPH_PALETTE_MASK               = 3,
        CAP_CAPTION_GLYPH_PALETTE_SHIFT_IN_HIGH_WORD = 26,
        CAP_CAPTION_GLYPH_PACKET_CODE                = 0x3C,
        CAP_CAPTION_GLYPH_GREY                       = 0x70,
        CAP_CAPTION_BACKGROUND_OT_INDEX              = 3,
        CAP_CAPTION_TEXT_OT_INDEX                    = 2
    };
    const u16*     text;
    const u16*     body;
    s32            title;
    s16            signedCode;
    u32            codeHighWord;
    s32            titleRightOffsetX;
    s16            lineIndex;
    s16            penX;
    s32            baselineY;
    s16            textIndex;
    u16            code;
    s16            centered;
    s32            paletteIndex;
    s16            iconBaselineY;
    s16            nextLineIndex;
    s16            glyphBaselineY;
    s32            boxTopY;
    POLY_G4*       background;
    POLY_G4*       backgroundCopy;
    DR_MODE*       drawMode;
    POLY_FT4*      flatQuad;
    POLY_GT4*      glyphQuad;
    POLY_GT4*      glyphOutline;
    TextGlyphCell* iconCell;

    lineIndex = 0;
    title     = titleIndex;
    text      = textStream;
    penX      = _capCaptionGetTextLineLeftX(textStream, 0) - CAP_CAPTION_DRAW_ORIGIN_X;
    baselineY = (u16)CAP_CAPTION_FIRST_BASELINE_Y - CAP_CAPTION_DRAW_ORIGIN_Y;

    // Queue two blended background passes, then their draw-mode command.
    background     = gGpuPrimCursor;
    gGpuPrimCursor = background + 1;
    setPolyG4(background);
    setSemiTrans(background, 1);
    setRGB0(background, 0, 0, 0);
    setRGB1(background, 0, 0, 0);
    setRGB2(background, 0, CAP_CAPTION_BACKGROUND_GREEN, CAP_CAPTION_BACKGROUND_BLUE);
    setRGB3(background, 0, CAP_CAPTION_BACKGROUND_GREEN, CAP_CAPTION_BACKGROUND_BLUE);
    background->x0 = (u16)CAP_CAPTION_BLOCK_LEFT_X - CAP_CAPTION_BOX_LEFT_ORIGIN_X;
    background->y0 = ((u16)CAP_CAPTION_BOTTOM_BASELINE_Y - CAP_CAPTION_BOX_BOTTOM_ORIGIN_Y) - gDisplayState.vramYOffset - (u16)CAP_CAPTION_BLOCK_HEIGHT;
    background->x1 = (u16)CAP_CAPTION_BLOCK_LEFT_X - CAP_CAPTION_BLOCK_LEFT_X * 2 + CAP_CAPTION_BOX_RIGHT_ORIGIN_X;
    background->y1 = ((u16)CAP_CAPTION_BOTTOM_BASELINE_Y - CAP_CAPTION_BOX_BOTTOM_ORIGIN_Y) - gDisplayState.vramYOffset - (u16)CAP_CAPTION_BLOCK_HEIGHT;
    background->x2 = (u16)CAP_CAPTION_BLOCK_LEFT_X - CAP_CAPTION_BOX_LEFT_ORIGIN_X;
    background->y2 = ((u16)CAP_CAPTION_BOTTOM_BASELINE_Y - CAP_CAPTION_BOX_BOTTOM_ORIGIN_Y) - gDisplayState.vramYOffset - (u16)CAP_CAPTION_BLOCK_HEIGHT + (u16)CAP_CAPTION_BLOCK_HEIGHT;
    background->x3 = (u16)CAP_CAPTION_BLOCK_LEFT_X - CAP_CAPTION_BLOCK_LEFT_X * 2 + CAP_CAPTION_BOX_RIGHT_ORIGIN_X;
    background->y3 = ((u16)CAP_CAPTION_BOTTOM_BASELINE_Y - CAP_CAPTION_BOX_BOTTOM_ORIGIN_Y) - gDisplayState.vramYOffset - (u16)CAP_CAPTION_BLOCK_HEIGHT + (u16)CAP_CAPTION_BLOCK_HEIGHT;
    addPrim(&gGpuCurrentOt[CAP_CAPTION_BACKGROUND_OT_INDEX], background);
    backgroundCopy  = gGpuPrimCursor;
    gGpuPrimCursor  = backgroundCopy + 1;
    *backgroundCopy = *background;
    addPrim(&gGpuCurrentOt[CAP_CAPTION_BACKGROUND_OT_INDEX], backgroundCopy);
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setlen(drawMode, 1);
    drawMode->code[0] = CAP_CAPTION_BACKGROUND_DRAW_MODE;
    addPrim(&gGpuCurrentOt[CAP_CAPTION_BACKGROUND_OT_INDEX], drawMode);

    body = text;
    // The title is a one-based glyph selector; its upper bits are ignored.
    if (title & CAP_CAPTION_TEXT_BYTE_MASK) {
        flatQuad       = gGpuPrimCursor;
        gGpuPrimCursor = flatQuad + 1;
        setPolyFT4(flatQuad);
        setShadeTex(flatQuad, 1);
        title             = title - 1;
        boxTopY           = ((u16)CAP_CAPTION_BOTTOM_BASELINE_Y - CAP_CAPTION_BOX_BOTTOM_ORIGIN_Y) - (u16)CAP_CAPTION_BLOCK_HEIGHT;
        flatQuad->x0      = (u16)CAP_CAPTION_BLOCK_LEFT_X - CAP_CAPTION_BOX_LEFT_ORIGIN_X;
        flatQuad->y0      = (boxTopY - gDisplayState.vramYOffset) - CAP_CAPTION_GLYPH_CELLS[title & CAP_CAPTION_TEXT_BYTE_MASK].height;
        titleRightOffsetX = CAP_CAPTION_GLYPH_CELLS[title & CAP_CAPTION_TEXT_BYTE_MASK].width - CAP_CAPTION_BOX_LEFT_ORIGIN_X;
        flatQuad->x1      = (u16)CAP_CAPTION_BLOCK_LEFT_X + titleRightOffsetX;
        flatQuad->y1      = (boxTopY - gDisplayState.vramYOffset) - CAP_CAPTION_GLYPH_CELLS[title & CAP_CAPTION_TEXT_BYTE_MASK].height;
        flatQuad->x2      = (u16)CAP_CAPTION_BLOCK_LEFT_X - CAP_CAPTION_BOX_LEFT_ORIGIN_X;
        flatQuad->y2      = boxTopY - gDisplayState.vramYOffset;
        titleRightOffsetX = CAP_CAPTION_GLYPH_CELLS[title & CAP_CAPTION_TEXT_BYTE_MASK].width - CAP_CAPTION_BOX_LEFT_ORIGIN_X;
        flatQuad->x3      = (u16)CAP_CAPTION_BLOCK_LEFT_X + titleRightOffsetX;
        flatQuad->y3      = boxTopY - gDisplayState.vramYOffset;
        setUVWH(flatQuad, CAP_CAPTION_GLYPH_CELLS[title & CAP_CAPTION_TEXT_BYTE_MASK].u, CAP_CAPTION_GLYPH_CELLS[title & CAP_CAPTION_TEXT_BYTE_MASK].v, CAP_CAPTION_GLYPH_CELLS[title & CAP_CAPTION_TEXT_BYTE_MASK].width, CAP_CAPTION_GLYPH_CELLS[title & CAP_CAPTION_TEXT_BYTE_MASK].height);
        flatQuad->clut  = CAP_CAPTION_TITLE_CLUT;
        flatQuad->tpage = getTPage(0, GPU_BLEND_ADD, _gCapCaptionTexturePageX, _gCapCaptionTexturePageY);
        addPrim(&gGpuCurrentOt[CAP_CAPTION_TEXT_OT_INDEX], flatQuad);
    }

    centered  = 1;
    textIndex = 0;
    // Advance the baseline at breaks and emit icons or two-pass glyphs.
    while (1) {
        code         = body[textIndex];
        codeHighWord = (u32)code << 16;
        signedCode   = (s32)codeHighWord >> 16;
        if (signedCode == CAP_CAPTION_TEXT_END) {
            break;
        }
        if (signedCode == CAP_CAPTION_TEXT_LINE_BREAK) {
            nextLineIndex            = lineIndex + 1;
            lineIndex                = nextLineIndex;
            CAP_CAPTION_CARET_TIP_Y  = baselineY - 2;
            CAP_CAPTION_CARET_LEFT_X = penX + 4;
            baselineY               += _capCaptionGetTextLineAdvance(&body[textIndex + 1]);
            if (centered != 0) {
                penX = _capCaptionGetTextLineLeftX(textStream, nextLineIndex) - CAP_CAPTION_DRAW_ORIGIN_X;
            } else {
                penX = (u16)CAP_CAPTION_BLOCK_LEFT_X - CAP_CAPTION_DRAW_ORIGIN_X;
            }
            textIndex++;
            continue;
        } else if (signedCode == CAP_CAPTION_TEXT_SPACER) {
            penX += CAP_CAPTION_TEXT_SPACER_WIDTH;
            textIndex++;
            continue;
        } else if ((code & CAP_CAPTION_TEXT_FAMILY_MASK) == CAP_CAPTION_TEXT_ICON) {
            iconCell       = &D_8010FB70[code & CAP_CAPTION_TEXT_BYTE_MASK];
            flatQuad       = gGpuPrimCursor;
            gGpuPrimCursor = flatQuad + 1;
            setPolyFT4(flatQuad);
            setShadeTex(flatQuad, 1);
            flatQuad->clut  = CAP_CAPTION_ICON_CLUT;
            flatQuad->tpage = CAP_CAPTION_ICON_TPAGE;
            iconBaselineY   = (baselineY - gDisplayState.vramYOffset) + 1;
            flatQuad->x0    = penX;
            flatQuad->y0    = iconBaselineY - iconCell->height;
            flatQuad->x1    = penX + iconCell->width;
            flatQuad->y1    = iconBaselineY - iconCell->height;
            flatQuad->x2    = penX;
            flatQuad->y2    = iconBaselineY;
            flatQuad->x3    = penX + iconCell->width;
            flatQuad->y3    = iconBaselineY;
            setUVWH(flatQuad, iconCell->u, iconCell->v, iconCell->width, iconCell->height);
            addPrim(&gGpuCurrentOt[CAP_CAPTION_TEXT_OT_INDEX], flatQuad);
            penX += iconCell->width;
            textIndex++;
            continue;
        } else {
            paletteIndex   = (codeHighWord >> CAP_CAPTION_GLYPH_PALETTE_SHIFT_IN_HIGH_WORD) & CAP_CAPTION_GLYPH_PALETTE_MASK;
            code           = code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK;
            glyphBaselineY = baselineY - gDisplayState.vramYOffset;
            glyphQuad      = gGpuPrimCursor;
            gGpuPrimCursor = glyphQuad + 1;
            setcode(glyphQuad, CAP_CAPTION_GLYPH_PACKET_CODE);
            setlen(glyphQuad, sizeof(*glyphQuad) / sizeof(u32) - 1);
            setShadeTex(glyphQuad, 1);
            setRGB0(glyphQuad, CAP_CAPTION_GLYPH_GREY, CAP_CAPTION_GLYPH_GREY, CAP_CAPTION_GLYPH_GREY);
            setRGB1(glyphQuad, CAP_CAPTION_GLYPH_GREY, CAP_CAPTION_GLYPH_GREY, CAP_CAPTION_GLYPH_GREY);
            setRGB2(glyphQuad, CAP_CAPTION_GLYPH_GREY, CAP_CAPTION_GLYPH_GREY, CAP_CAPTION_GLYPH_GREY);
            setRGB3(glyphQuad, CAP_CAPTION_GLYPH_GREY, CAP_CAPTION_GLYPH_GREY, CAP_CAPTION_GLYPH_GREY);
            setSemiTrans(glyphQuad, 1);
            glyphQuad->clut  = paletteIndex | CAP_CAPTION_GLYPH_CLUT_BASE;
            glyphQuad->x0    = penX;
            glyphQuad->tpage = getTPage(0, GPU_BLEND_ADD, _gCapCaptionTexturePageX, _gCapCaptionTexturePageY);
            glyphQuad->y0    = glyphBaselineY - CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].height;
            glyphQuad->x1    = penX + CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].width;
            glyphQuad->y1    = glyphBaselineY - CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].height;
            glyphQuad->x2    = penX;
            glyphQuad->y2    = glyphBaselineY;
            glyphQuad->x3    = penX + CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].width;
            glyphQuad->y3    = glyphBaselineY;
            setUVWH(glyphQuad, CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].u, CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].v, CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].width, CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].height);
            addPrim(&gGpuCurrentOt[CAP_CAPTION_TEXT_OT_INDEX], glyphQuad);
            glyphOutline        = gGpuPrimCursor;
            gGpuPrimCursor      = glyphOutline + 1;
            *glyphOutline       = *glyphQuad;
            glyphOutline->tpage = getTPage(0, GPU_BLEND_SUBTRACT, _gCapCaptionTexturePageX, _gCapCaptionTexturePageY);
            addPrim(&gGpuCurrentOt[CAP_CAPTION_TEXT_OT_INDEX], glyphOutline);
            penX = CAP_CAPTION_GLYPH_CELLS[(s16)code].width + penX - 1;
        }
        textIndex++;
    }
    return 0;
}

/// Returns the first caption baseline in screen pixels from the selected bottom Y.
///
/// Subtracts the heights of closed lines after the first line break. Each is
/// the tallest nonnegative glyph's height + 2, or two pixels if empty;
/// negative tokens including inline icons do not affect height. An unfinished
/// final line contributes nothing. Requires loaded glyph metrics and text
/// readable through its end token at word index 0..32767. Heights and the result
/// narrow to s16.
static s16 _capCaptionGetTextFirstBaselineY(const u16* textStream)
{
    s16        lineHeight           = 0;
    s16        followingLinesHeight = 0;
    s16        textIndex            = 0;
    s16        seenFirstBreak       = 0;
    const u16* text                 = textStream;
    s16        code                 = text[0];

    while (code != CAP_CAPTION_TEXT_END) {
        if (code == CAP_CAPTION_TEXT_LINE_BREAK) {
            if (seenFirstBreak) {
                if (lineHeight == 0) {
                    lineHeight = CAP_CAPTION_TEXT_LINE_GAP;
                }
                followingLinesHeight += lineHeight;
            } else {
                seenFirstBreak = 1;
            }
            lineHeight = 0;
        } else if (code != CAP_CAPTION_TEXT_SPACER) {
            if (code >= 0) {
                if (lineHeight < CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].height + CAP_CAPTION_TEXT_LINE_GAP) {
                    lineHeight = CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].height + CAP_CAPTION_TEXT_LINE_GAP;
                }
            }
        }
        code = text[++textIndex];
    }
    return CAP_CAPTION_BOTTOM_BASELINE_Y - followingLinesHeight;
}

/// Queues the pulsing continuation triangle after its draw-call countdown expires.
///
/// The countdown loses one per call and suppresses drawing while nonzero.
/// The triangle uses the last line-break pen position directly in draw coordinates;
/// no VRAM Y adjustment is applied. Its inclusive 8..15 pulse is drawn before
/// stepping the level and reversing direction. Requires OT entry 2 and primitive
/// storage retained through GPU completion.
static void _capCaptionDrawContinueCaret(void)
{
    POLY_G3* caret;
    s32      grey;

    if (CAP_CAPTION_CARET_DRAWS_LEFT != 0) {
        CAP_CAPTION_CARET_DRAWS_LEFT -= 1;
        return;
    }
    caret          = gGpuPrimCursor;
    gGpuPrimCursor = caret + 1;
    setPolyG3(caret);
    grey = (_gCapCaptionCaretPulseLevel << 7) / CAP_CAPTION_CARET_PULSE_MAX;
    setRGB0(caret, grey, grey, grey);
    grey = (_gCapCaptionCaretPulseLevel * 192) / CAP_CAPTION_CARET_PULSE_MAX;
    setRGB1(caret, grey, grey, grey);
    setRGB2(caret, grey, grey, grey);
    caret->x0 = CAP_CAPTION_CARET_LEFT_X + 3;
    caret->y0 = CAP_CAPTION_CARET_TIP_Y;
    caret->x1 = CAP_CAPTION_CARET_LEFT_X;
    caret->x2 = CAP_CAPTION_CARET_LEFT_X + 7;
    caret->y1 = CAP_CAPTION_CARET_TIP_Y - 7;
    caret->y2 = CAP_CAPTION_CARET_TIP_Y - 7;
    addPrim(&gGpuCurrentOt[2], caret);
    _capCaptionStepCaretPulse(&_gCapCaptionCaretPulseLevel, &_gCapCaptionCaretPulseFalling);
}

/// Returns the biased screen-space left X of the widest closed caption line.
///
/// Uses (320 - width) / 2 - 5 pixels. Only line-break tokens commit a width;
/// an unfinished last line is omitted. Spacers advance three pixels, inline
/// icons sixteen, and nonnegative glyph codes their cell width minus one.
/// Other negative codes are skipped. Requires loaded glyph metrics and text
/// readable through its end token, at word index 0..32767. Widths narrow to s16.
static s16 _capCaptionGetTextBlockLeftX(const u16* text)
{
    s16 lineWidth;
    s16 maxWidth;
    s16 textIndex;
    s16 code;

    lineWidth = 0;
    maxWidth  = 0;
    textIndex = 0;
    code      = text[0];
    while (code != CAP_CAPTION_TEXT_END) {
        if (code == CAP_CAPTION_TEXT_LINE_BREAK) {
            if (lineWidth > maxWidth) {
                maxWidth = lineWidth;
            }
            lineWidth = 0;
            code      = text[++textIndex];
        } else if (code == CAP_CAPTION_TEXT_SPACER) {
            lineWidth += CAP_CAPTION_TEXT_SPACER_WIDTH;
            code       = text[++textIndex];
        } else if ((code & CAP_CAPTION_TEXT_FAMILY_MASK) == CAP_CAPTION_TEXT_ICON) {
            lineWidth += CAP_CAPTION_TEXT_ICON_WIDTH;
            code       = text[++textIndex];
        } else if (code >= 0) {
            lineWidth += CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].width - 1;
            code       = text[++textIndex];
        } else {
            code = text[++textIndex];
        }
    }
    return (CAP_CAPTION_TEXT_SCREEN_WIDTH - maxWidth) / 2 - CAP_CAPTION_TEXT_LEFT_BIAS;
}

/// Returns the biased screen-space left X of one zero-based caption line.
///
/// Uses (320 - width) / 2 - 5 pixels, measuring glyphs, spacers and icons as
/// `_capCaptionGetTextBlockLeftX` does. The requested line must end in a
/// line-break token to contribute its width; an absent or unfinished line
/// uses width zero. Requires loaded glyph metrics and text readable through
/// its end token at word index 0..32767; line indices and widths narrow to s16.
static s16 _capCaptionGetTextLineLeftX(const u16* text, s32 selectedLineIndex)
{
    s16 lineWidth;
    s16 selectedWidth;
    s16 textIndex;
    s16 lineIndex;
    s16 code;

    lineWidth     = 0;
    selectedWidth = 0;
    textIndex     = 0;
    lineIndex     = 0;
    code          = text[0];
    while (code != CAP_CAPTION_TEXT_END) {
        if (code == CAP_CAPTION_TEXT_LINE_BREAK) {
            if (lineIndex == selectedLineIndex) {
                selectedWidth = lineWidth;
            }
            lineWidth = 0;
            textIndex++;
            lineIndex++;
            code = text[textIndex];
        } else if (code == CAP_CAPTION_TEXT_SPACER) {
            lineWidth += CAP_CAPTION_TEXT_SPACER_WIDTH;
            code       = text[++textIndex];
        } else if ((code & CAP_CAPTION_TEXT_FAMILY_MASK) == CAP_CAPTION_TEXT_ICON) {
            lineWidth += CAP_CAPTION_TEXT_ICON_WIDTH;
            code       = text[++textIndex];
        } else if (code >= 0) {
            lineWidth += CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].width - 1;
            code       = text[++textIndex];
        } else {
            code = text[++textIndex];
        }
    }
    return (CAP_CAPTION_TEXT_SCREEN_WIDTH - selectedWidth) / 2 - CAP_CAPTION_TEXT_LEFT_BIAS;
}

/// Returns the pixel height accumulated at caption line-break tokens.
///
/// Each closed line contributes its tallest nonnegative glyph's height + 2,
/// or two pixels without such glyphs. Negative tokens, including inline icons,
/// do not affect height, and an unfinished final line contributes nothing.
/// Requires loaded glyph metrics and text readable through its end token;
/// the end token must lie at word index 0..32767. Heights and the sum narrow to s16.
static s16 _capCaptionGetTextBlockHeight(const u16* text)
{
    s16 lineHeight  = 0;
    s16 blockHeight = 0;
    s16 textIndex   = 0;
    s16 code        = text[0];

    while (code != CAP_CAPTION_TEXT_END) {
        if (code == CAP_CAPTION_TEXT_LINE_BREAK) {
            if (lineHeight == 0) {
                lineHeight = CAP_CAPTION_TEXT_LINE_GAP;
            }
            blockHeight += lineHeight;
            lineHeight   = 0;
        } else if (code != CAP_CAPTION_TEXT_SPACER) {
            if (code >= 0) {
                if (lineHeight < CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].height + CAP_CAPTION_TEXT_LINE_GAP) {
                    lineHeight = CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].height + CAP_CAPTION_TEXT_LINE_GAP;
                }
            }
        }
        code = text[++textIndex];
    }
    return blockHeight;
}

/// Returns the pixel baseline advance for the following caption line.
///
/// Stops at the first line break or end token. A break returns the tallest
/// nonnegative glyph's height + 2, or two pixels if no such glyph was seen.
/// End of text returns thirteen pixels regardless of preceding glyph heights.
/// Other negative codes, including inline icons, are skipped. Requires loaded
/// metrics and readable text through either terminator, at word index 0..32767;
/// line heights retain signed-halfword narrowing.
static s32 _capCaptionGetTextLineAdvance(const u16* text)
{
    s16 lineHeight = 0;
    s16 textIndex  = 0;
    s16 scanning   = 1;
    s16 code       = text[0];

    do {
        if (code == CAP_CAPTION_TEXT_LINE_BREAK) {
            scanning = 0;
        } else if (code == CAP_CAPTION_TEXT_END) {
            scanning   = 0;
            lineHeight = CAP_CAPTION_TEXT_END_LINE_ADVANCE;
        } else if (code >= 0) {
            if (lineHeight < CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].height + CAP_CAPTION_TEXT_LINE_GAP) {
                lineHeight = CAP_CAPTION_GLYPH_CELLS[code & CAP_CAPTION_TEXT_GLYPH_INDEX_MASK].height + CAP_CAPTION_TEXT_LINE_GAP;
            }
            code = text[++textIndex];
        } else {
            code = text[++textIndex];
        }
    } while (scanning);
    if (lineHeight == 0) {
        lineHeight = CAP_CAPTION_TEXT_LINE_GAP;
    }
    return lineHeight;
}

/// Finds the selected variant key from a sequence's starting record slot.
///
/// recordIndex counts twelve-byte slots from the selected sequence command,
/// whose slot zero is not a text record. Returns the first matching slot or
/// the terminal slot if no key matches. Requires a selected relocated sequence
/// readable through its terminal record; retains no pointer and changes no record.
static s32 _capCaptionFindRecordByKey(s32 recordIndex)
{
    CapSequenceRecord* record;

    for (;;) {
        record = _capSequenceRecordAt(CAP_CAPTION_SEQUENCE, recordIndex);
        if (record->textRef.offset != CAP_TEXT_REF_END && record->key != CapCaption_Data_8015E666) {
            recordIndex++;
        } else {
            break;
        }
    }
    return recordIndex;
}

static void CapCaption_TimedTask(Task* task)
{
    s32 remaining;

    remaining             = task->spawnArg1.value - 1;
    task->spawnArg1.value = remaining;
    if (remaining <= 0) {
        taskKill(task);
    }
    CAP_CAPTION_DRAW_CURRENT();
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
    CAP_CAPTION_DRAW_CURRENT();
}

static inline void CapCaption_ShowTimed(s16 arg0, s16 arg1, s16 arg2)
{
    CapCaption_SelectScript(arg0, arg1, 0xD0);
    taskSpawnFromTable(&CapCaption_Data_801544FC, 0, (s32)(arg2), 0);
}
