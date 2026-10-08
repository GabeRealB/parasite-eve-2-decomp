/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// 1BC.h keeps this out of scope on purpose: callers hand it a sign-extended
/// animation id, which a `u16` prototype would zero-extend.
/// Reseeds animation slots 1..0x12 when the actor's animation id changes,
/// handing each slot the blend weight the id selects from
/// `gGolemKnightBishopAnimBlend`; while the id is unchanged it instead ticks every
/// slot one frame and walks the id's frame counter up.
void golemKnightBishopTickAnim(Task* arg0)
{
    _golemKnightBishopTickAnimInline(arg0);
}
