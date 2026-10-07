/* Part of the action prompt library; see action_prompt.h. */

/// Updates every hotspot's hit flag for a point in center-origin screen pixels.
///
/// `hotspots` must remain writable through an entry whose `id` is
/// `ACTION_PROMPT_HOTSPOT_END`. That sentinel's hit flag is left untouched.
/// Rectangle edges, including `x + w` and `y + h`, are inside; the sums use
/// signed integer arithmetic without narrowing to 16 bits. Sets each preceding
/// entry's `hit` to 1 or 0 and returns 1 if any contains the point, otherwise 0.
/// An empty table returns 0. The table remains owned by the caller.
s32 ACTION_PROMPT_HIT_TEST(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY)
{
    s32 anyHit = 0;

    // Refresh the whole table so overlapping rectangles keep independent hits.
    while (hotspots->id != ACTION_PROMPT_HOTSPOT_END) {
        if ((cursorX >= hotspots->x) && ((hotspots->x + hotspots->w) >= cursorX) && (cursorY >= hotspots->y) && ((hotspots->y + hotspots->h) >= cursorY)) {
            hotspots->hit = 1;
            anyHit        = 1;
        } else {
            hotspots->hit = 0;
        }
        hotspots++;
    }
    return anyHit;
}
