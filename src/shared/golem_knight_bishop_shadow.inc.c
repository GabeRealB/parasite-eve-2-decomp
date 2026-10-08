/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Draws the GOLEM's ground shadow from its already-composed coordinates.
///
/// Uses part 3's world X/Z and the root's world height, with a half-side of
/// 768 world units. A zero shade becomes the negative no-shadow sentinel;
/// negative shades suppress drawing. `task` owns a live GOLEM model/work block.
static void _golemKnightBishopDrawShadow(Task* task)
{
    _golemKnightBishopDrawShadowInline(task);
}
