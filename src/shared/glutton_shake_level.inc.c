/* Part of the Glutton library; see glutton.h. */

/// Writes a screen-shake level into the host's `field_EAC` through
/// `GLUTTON_HOST_TASK`; the host's shake driver starts a shake when it differs
/// from the armed level. Nothing in either package calls it.
void gluttonSetShakeLevel(s8 arg0)
{
    ((Actor403200Work*)GLUTTON_HOST_TASK->work)->field_EAC = arg0;
}
