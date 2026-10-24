/* Private per-instance storage. Include at the original data position.
 * The configuration contract is documented in cap_captions.h. */

static void _capCaptionCancelableTask(Task* task);

static void _capCaptionTimedTask(Task* task);

enum {
    CAP_CAPTION_SCHEDULE_EARLY_NOTICE_COMMAND = 16,
    CAP_CAPTION_SCHEDULE_FINAL_NOTICE_COMMAND = 17,
};

/// Bodyless task descriptor for drawing the currently selected caption for a tick duration.
///
/// The first spawn argument is a signed tick count, decremented before drawing;
/// a nonpositive remainder kills the task but still draws on that tick. Caption
/// selection and resources belong to the carrier and must outlive the task.
static TaskDesc _gCapCaptionTimedTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _capCaptionTimedTask, { .value = 0 } };

/// Bodyless mode-task recipe for a caption cancellable by port zero's cancel button.
///
/// Spawn argument 1 is a signed tick duration, consumed after the arming tick.
/// Expiry or cancellation kills the task and requests mode exit, still drawing
/// that tick. Caption selection and resources must remain live through mode exit.
static TaskDesc _gCapCaptionCancelableTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _capCaptionCancelableTask, { .value = 0 } };

/// Caption notices for a descending scene clock, followed by a terminating window.
///
/// Bounds count 30 clock frames per unit and select (lower, upper]. Command 16
/// uses keys 5..1 at 300, 240, 180, 120 and 60 units; command 17 uses key 30
/// at 30 units and keys 5..0 for the final countdown. Key 0 remains visible
/// through negative clock values above -3 units. Each carrier's loaded caption
/// resource must supply these command/key pairs before scheduling starts.
static CapCaptionScheduleWindow _gCapCaptionSceneClockSchedule[13] = {
    { 300, 295, CAP_CAPTION_SCHEDULE_EARLY_NOTICE_COMMAND, 5 },
    { 240, 235, CAP_CAPTION_SCHEDULE_EARLY_NOTICE_COMMAND, 4 },
    { 180, 175, CAP_CAPTION_SCHEDULE_EARLY_NOTICE_COMMAND, 3 },
    { 120, 115, CAP_CAPTION_SCHEDULE_EARLY_NOTICE_COMMAND, 2 },
    { 60, 55, CAP_CAPTION_SCHEDULE_EARLY_NOTICE_COMMAND, 1 },
    { 30, 25, CAP_CAPTION_SCHEDULE_FINAL_NOTICE_COMMAND, 30 },
    { 5, 4, CAP_CAPTION_SCHEDULE_FINAL_NOTICE_COMMAND, 5 },
    { 4, 3, CAP_CAPTION_SCHEDULE_FINAL_NOTICE_COMMAND, 4 },
    { 3, 2, CAP_CAPTION_SCHEDULE_FINAL_NOTICE_COMMAND, 3 },
    { 2, 1, CAP_CAPTION_SCHEDULE_FINAL_NOTICE_COMMAND, 2 },
    { 1, 0, CAP_CAPTION_SCHEDULE_FINAL_NOTICE_COMMAND, 1 },
    { 0, -3, CAP_CAPTION_SCHEDULE_FINAL_NOTICE_COMMAND, 0 },
    { CAP_CAPTION_SCHEDULE_END, 0, 0, 0 },
};

// Initial pulse state retained across caption selections.
enum { CAP_CAPTION_CARET_INITIAL_LEVEL     = 8,
       CAP_CAPTION_CARET_INITIAL_DIRECTION = 0 };

/// Current continuation-caret brightness scale, inclusive integer range 8..15.
///
/// Drawing converts level / 15 to vertex intensities, then steps the pulse.
/// Caption suppression and the initial caret countdown leave this level frozen.
static s32 _gCapCaptionCaretPulseLevel = CAP_CAPTION_CARET_INITIAL_LEVEL;

/// Continuation-caret pulse direction (0 rising, 1 falling).
///
/// Reverses after stepping to either brightness endpoint and persists across
/// caption selections. Kept as a word because the pulse helper updates its address.
static s32 _gCapCaptionCaretPulseFalling = CAP_CAPTION_CARET_INITIAL_DIRECTION;
