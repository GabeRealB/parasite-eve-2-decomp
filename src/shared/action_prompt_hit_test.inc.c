/* Part of the action prompt library; see action_prompt.h. */

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
