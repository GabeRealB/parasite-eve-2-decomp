/* Part of the action prompt library; see action_prompt.h. */

/// Initializes one reserved outline edge from screen pixels and RGB bytes.
///
/// `line` must address a writable LINE_F2. Arguments must be side-effect-free;
/// `line` is evaluated repeatedly. Coordinates narrow to signed halfwords and
/// colours to bytes. Expands to standalone statements without reserving or
/// linking a packet; coordinates are read as their stores are reached.
#define ACTION_PROMPT_INIT_OUTLINE_EDGE(line, startX, startY, endX, endY, red, green, blue) \
    setLineF2(line);                                                                        \
    (line)->x0 = (startX);                                                                  \
    (line)->y0 = (startY);                                                                  \
    (line)->x1 = (endX);                                                                    \
    (line)->y1 = (endY);                                                                    \
    (line)->r0 = (red);                                                                     \
    (line)->g0 = (green);                                                                   \
    (line)->b0 = (blue);

/// Queues an opaque four-line outline of one action-prompt hotspot.
///
/// Borrows readable geometry in center-origin screen pixels, with Y increasing
/// downward. Far edges are `x + w` and `y + h`; vertex coordinates retain their
/// low 16 bits, including for negative positions. RGB channels are 0..255.
/// Choice, prompt-kind and hit fields are ignored, so even a sentinel entry
/// queues an outline. The input is neither changed nor retained.
///
/// Requires four word-aligned `LINE_F2` packets of free arena space at
/// `gGpuPrimCursor` and a writable ordering-table entry 1. Advances the cursor
/// by `4 * sizeof(LINE_F2)` bytes without checking capacity; packets remain in
/// the frame arena until GPU drawing completes. Only actor_143000 calls the
/// ordinary instance; the other carriers retain unused copies.
static void ACTION_PROMPT_OUTLINE_RECT(const ActionPromptHotspot* hotspot, u8 red, u8 green, u8 blue)
{
    enum {
        ACTION_PROMPT_OUTLINE_OT_SLOT = 1,
    };
    LINE_F2* line;

    // Unsigned geometry loads preserve the signed pixel bits at each vertex store.
    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    ACTION_PROMPT_INIT_OUTLINE_EDGE(line, (u16)hotspot->x, (u16)hotspot->y, (u16)hotspot->x + (u16)hotspot->w, (u16)hotspot->y, red, green, blue);
    addPrim(gGpuCurrentOt + ACTION_PROMPT_OUTLINE_OT_SLOT, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    ACTION_PROMPT_INIT_OUTLINE_EDGE(line, (u16)hotspot->x + (u16)hotspot->w, (u16)hotspot->y, (u16)hotspot->x + (u16)hotspot->w, (u16)hotspot->y + (u16)hotspot->h, red, green, blue);
    addPrim(gGpuCurrentOt + ACTION_PROMPT_OUTLINE_OT_SLOT, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    ACTION_PROMPT_INIT_OUTLINE_EDGE(line, (u16)hotspot->x + (u16)hotspot->w, (u16)hotspot->y + (u16)hotspot->h, (u16)hotspot->x, (u16)hotspot->y + (u16)hotspot->h, red, green, blue);
    addPrim(gGpuCurrentOt + ACTION_PROMPT_OUTLINE_OT_SLOT, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    ACTION_PROMPT_INIT_OUTLINE_EDGE(line, (u16)hotspot->x, (u16)hotspot->y + (u16)hotspot->h, (u16)hotspot->x, (u16)hotspot->y, red, green, blue);
    addPrim(gGpuCurrentOt + ACTION_PROMPT_OUTLINE_OT_SLOT, line);
}

#undef ACTION_PROMPT_INIT_OUTLINE_EDGE
