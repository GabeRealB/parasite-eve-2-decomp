/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Refreshes the GOLEM's room lighting and consumes its pending ambient tint.
///
/// `task` owns a live model, GOLEM work block and Enemy spawn argument. The root's
/// composed world position must be current; no coordinates are composed here.
static void _golemKnightBishopUpdateTint(Task* task)
{
    _golemKnightBishopUpdateTintInline(task);
}
