/* Part of the Glutton library; see glutton.h. */

#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
/// Requests a vertical screen shake from the dumping-hole Glutton host.
///
/// The successfully spawned host task and its work must still be alive. The
/// next shake update arms 5, 10 or 22 frames for `GLUTTON_SHAKE_SHORT`,
/// `GLUTTON_SHAKE_MEDIUM` or `GLUTTON_SHAKE_LONG` when the request differs from
/// the armed level; repeating it does not restart the shake. `level` is stored
/// as an unsigned byte without validation. Other values, including
/// `GLUTTON_SHAKE_NONE`, neither arm a changed request nor cancel the shake.
#endif
void GLUTTON_SET_SHAKE_LEVEL(s8 level)
{
    GluttonWork* work = GLUTTON_HOST_TASK->work;

    work->shakeLevel = level;
}
