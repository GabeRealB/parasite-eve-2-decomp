/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Updates the two animated Sucklerceph parts through the out-of-line frame hook.
///
/// Requires an initialized model and `SucklercephWork`. Uses the same request,
/// freeze and frame-count rules as `_sucklercephTickAnim`.
static void _sucklercephAnimate(Task* task)
{
    _sucklercephTickAnim(task);
}
