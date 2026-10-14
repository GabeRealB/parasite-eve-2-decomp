/* Private per-instance storage. Include at the original data position.
 * The configuration contract is documented in planar_reflection.h. */

static void _planarReflectionAttachmentTask(Task* reflectionTask);

/// Reflected-player coordinate part for each attachment or equipment slot.
///
/// Slots 0/1 select attachments and 2/3 equipment. Slots 0/2 use part 12;
/// slots 1/3 use part 8. The reflected player requires at least 13 coordinate
/// parts. Each including room owns one four-byte table for its loaded lifetime.
static u8 _gPlanarReflectionAttachmentParts[4] = {
    12,
    8,
    12,
    8,
};
