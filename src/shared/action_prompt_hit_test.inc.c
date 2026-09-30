/* Part of the action prompt library; see action_prompt.h. */

/// Hit-tests (`x`, `y`) against the hotspot table `table`, terminated by an
/// `id` of -1: raises `hit` on every entry whose rectangle contains the point
/// and clears it on the others, and answers whether any entry was hit.
s32 actionPromptHitTest(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
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
