/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Selects the declared static void(Task*) dispatcher defined by this inclusion.
///
/// Defaults to the dangle dispatcher; carriers bind a second instance for the
/// lurk shift. Pair it with `MAD_CHASER_FOUR_STEP_HANDLERS` for that behavior.
/// This bare identifier affects only the definition, with no argument
/// evaluation or token construction. The fragment undefines both bindings.
#ifndef MAD_CHASER_FOUR_STEP_STATE
#define MAD_CHASER_FOUR_STEP_STATE _madChaserDangleState
#endif

/// Selects the four phase handlers for one Mad Chaser dispatcher instance.
///
/// Bind to a declared, readable `TaskFuncTable4` object identifier before
/// including this fragment; const tables are allowed. Each dispatcher call
/// evaluates the binding once and copies all four callback pointers, then
/// selects by the signed halfword of `MadChaserWork::subState` (0..3, with no
/// terminator or bounds check). Entries must be non-NULL void(Task*) callbacks.
/// Table storage is borrowed only for the copy; callback code stays loaded
/// through the call. There are no arguments or manufactured tokens.
///
/// Defaults to `_gMadChaserDangleSteps`. `actor_04400`, `actor_341700` and
/// `actor_342400` pair the lurk-shift table with `MAD_CHASER_FOUR_STEP_STATE`
/// for their second inclusion. Both bindings are undefined at the end of
/// each inclusion; rebind any overrides for the next instance.
#ifndef MAD_CHASER_FOUR_STEP_HANDLERS
#define MAD_CHASER_FOUR_STEP_HANDLERS _gMadChaserDangleSteps
#endif

/// Runs the current phase of a four-step Mad Chaser behavior.
///
/// Requires the task's live `MadChaserWork` and subState in 0..3; there is no
/// bounds check. The default sequence starts the dangle, sways, falls and lands.
/// The lurk-shift instance opens its animation, starts the right step, moves
/// right and returns left. Copies all four callback pointers before reading
/// subState as a signed halfword and calling one handler. The callback owns
/// phase transitions; the dispatcher retains no task, work or table storage.
static void MAD_CHASER_FOUR_STEP_STATE(Task* task)
{
    MadChaserWork* work         = task->work;
    TaskFuncTable4 stepHandlers = MAD_CHASER_FOUR_STEP_HANDLERS;

    stepHandlers.funcs[(s16)work->subState](task);
}

#undef MAD_CHASER_FOUR_STEP_STATE
#undef MAD_CHASER_FOUR_STEP_HANDLERS
