/* Part of the Mad Chaser library; see mad_chaser.h. */

// Each carrier binds MAD_CHASER_REACTION_STATE_HANDLER to a declared
// void(Task*) callback before inclusion, then undefines it. Its prior declaration
// establishes linkage. The object-like binding names only the definition;
// it evaluates no arguments, captures no values and constructs no tokens.
// The carrier's _madChaserRecoilLight and _madChaserRecoilLightRecover bindings
// select the entry and continuation callbacks for light recoil, heavy recoil
// or buildup-status hold. Both must have the TaskFunc signature.

/// Dispatches the entry or continuation of a Mad Chaser combat hit reaction.
///
/// Borrows live task-owned `MadChaserWork`; `subState` must be 0 for entry or
/// 1 for continuation. Builds the complete two-callback table on the stack,
/// reads the selector as a signed halfword and calls one loaded handler without
/// a bounds check. Entry advances to continuation; continuation owns recovery
/// and behavior changes. The combat frame callback advances animation afterwards.
///
/// Light recoil requires a current clip in 1..19 on entry, with live enemy/model
/// and initialized animation storage. Entry saves the interrupted stance and
/// requests an eight-frame normal-rate blend. Upright recovery restarts repeated
/// light hits at twice normal rate, otherwise lets new hits replace the behavior
/// before an animation boundary resumes walking. Low recovery waits for a
/// boundary, jump or held pose to claim the alert and enter its behavior.
/// The task and carrier must remain live through the selected callback.
///
/// Heavy recoil selects a clip from the interrupted stance, then waits for a
/// boundary, jump or held pose to resume walking or alert. Status hold requires
/// a started enemy buildup reaction: it enters the hold pose, then ticks it until
/// walking resumes. These phases also require live enemy/model and initialized
/// animation storage. This dispatcher retains no pointers after the call.
void MAD_CHASER_REACTION_STATE_HANDLER(Task* task)
{
    MadChaserWork* work           = task->work;
    TaskFunc       stepHandlers[] = {
        _madChaserRecoilLight,
        _madChaserRecoilLightRecover,
    };

    stepHandlers[(s16)work->subState](task);
}
