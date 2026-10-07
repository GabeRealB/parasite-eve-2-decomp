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

/// Queues the four edges of an action-prompt hotspot in an RGB byte colour.
///
/// `hotspot` is a borrowed hotspot in center-origin screen pixels, with Y
/// increasing downward. Far edges are x + w and y + h. The coordinate reads
/// use unsigned halfwords; each vertex retains the low 16 bits, preserving
/// negative coordinate bit patterns. Choice and hit fields are untouched,
/// including on a sentinel entry. The hotspot is not kept.
/// Requires room for four word-aligned LINE_F2 packets (64 bytes) at
/// `gGpuPrimCursor` and a writable ordering-table entry 1. Packets stay in the
/// frame arena until GPU drawing completes; no capacity check is performed.
/// Shelter R47's additional static instance has no callers.
void ACTION_PROMPT_OUTLINE_RECT(const ActionPromptHotspot* hotspot, u8 red, u8 green, u8 blue)
{
    enum {
        ACTION_PROMPT_OUTLINE_OT_SLOT = 1,
    };
    LINE_F2* line;

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
