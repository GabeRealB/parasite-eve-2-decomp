/* Part of the action prompt library; see action_prompt.h. */

/// Hit-tests (`x`, `y`) against the hotspot table `table`, terminated by
/// `ACTION_PROMPT_HOTSPOT_END`: raises `hit` on every entry whose rectangle
/// contains the point, including the far edges `x + w` and `y + h`, clears
/// `hit` on the others, and answers whether any entry was hit.
s32 actionPromptHitTest(ActionPromptHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != ACTION_PROMPT_HOTSPOT_END) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}
