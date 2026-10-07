/* Part of the action prompt library; see action_prompt.h. */

#ifndef ACTION_PROMPT_CURSOR_DRAW_HELPERS_DEFINED
/// Keeps the geometry constants and inline helper unique when this fragment is included twice.
#define ACTION_PROMPT_CURSOR_DRAW_HELPERS_DEFINED

/// Cursor geometry and the 4-bit texture and palettes in the prompt's VRAM atlas.
enum {
    ACTION_PROMPT_CURSOR_ORIGIN_OFFSET = 2,
    ACTION_PROMPT_CURSOR_WIDTH         = 16,
    ACTION_PROMPT_CURSOR_HEIGHT        = 23,
    ACTION_PROMPT_CURSOR_TEXTURE_U     = 0,
    ACTION_PROMPT_CURSOR_TEXTURE_V     = 232,
    ACTION_PROMPT_CURSOR_TEXTURE_PAGE  = getTPage(0, GPU_BLEND_AVERAGE, 896, 256),
    ACTION_PROMPT_CURSOR_IDLE_CLUT     = getClut(128, 242),
    ACTION_PROMPT_CURSOR_HOTSPOT_CLUT  = getClut(112, 242)
};

/// Places the cursor quad around its point in screen pixels, narrowing each edge to s16.
static inline void _actionPromptSetCursorVertices(POLY_FT4* cursorQuad, s32 x, s32 y)
{
    s16 edgeX;
    s16 edgeY;

    edgeX          = x - ACTION_PROMPT_CURSOR_ORIGIN_OFFSET;
    cursorQuad->x2 = edgeX;
    cursorQuad->x0 = edgeX;
    edgeX          = x + (ACTION_PROMPT_CURSOR_WIDTH - ACTION_PROMPT_CURSOR_ORIGIN_OFFSET);
    cursorQuad->x3 = edgeX;
    cursorQuad->x1 = edgeX;
    edgeY          = y - ACTION_PROMPT_CURSOR_ORIGIN_OFFSET;
    cursorQuad->y1 = edgeY;
    cursorQuad->y0 = edgeY;
    edgeY          = y + (ACTION_PROMPT_CURSOR_HEIGHT - ACTION_PROMPT_CURSOR_ORIGIN_OFFSET);
    cursorQuad->y3 = edgeY;
    cursorQuad->y2 = edgeY;
}
#endif

/// Queues the point-and-click cursor as an opaque, unmodulated textured quad.
///
/// `x` and `y` are pixels from the screen center, with Y increasing downward;
/// callers supply X in [-160, 159] and Y in [-110, 110]. The 16-by-23 quad starts
/// two pixels above and left of that point. Hidden mode queues nothing; hotspot
/// mode selects the hotspot palette, and every other nonzero mode uses idle.
/// Requires a word-aligned packet arena with room for one `POLY_FT4` and a
/// writable current ordering-table tag. The packet is borrowed until the GPU
/// consumes the frame; this routine advances the arena cursor without a bound
/// check and links the packet at `gGpuCurrentOt[0]`.
void ACTION_PROMPT_DRAW_CURSOR(s32 x, s32 y, s32 mode)
{
    POLY_FT4* cursorQuad;

    if (mode == ACTION_PROMPT_MODE_HIDDEN) {
        return;
    }

    cursorQuad     = gGpuPrimCursor;
    gGpuPrimCursor = cursorQuad + 1;

    _actionPromptSetCursorVertices(cursorQuad, x, y);

    cursorQuad->tpage = ACTION_PROMPT_CURSOR_TEXTURE_PAGE;
    if (mode == ACTION_PROMPT_MODE_HOTSPOT) {
        cursorQuad->clut = ACTION_PROMPT_CURSOR_HOTSPOT_CLUT;
    } else {
        cursorQuad->clut = ACTION_PROMPT_CURSOR_IDLE_CLUT;
    }

    // Raw-texture mode ignores RGB bytes, so they need no initialization.
    setUVWH(cursorQuad, ACTION_PROMPT_CURSOR_TEXTURE_U, ACTION_PROMPT_CURSOR_TEXTURE_V,
            ACTION_PROMPT_CURSOR_WIDTH, ACTION_PROMPT_CURSOR_HEIGHT);
    setPolyFT4(cursorQuad);
    setShadeTex(cursorQuad, true);

    addPrim(gGpuCurrentOt, cursorQuad);
}
