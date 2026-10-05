/* Part of the screen wave library; see screen_wave.h. */

/// Runs the screen wave from an event. Called with zero or less, it sets the
/// MDEC decode mode to 2, fills the package's `gScreenWaveSpawnCtx` (peak 0x60
/// reached in one step, tinted 0x40/0x80/0x80) and spawns the wave task from
/// `gScreenWaveTaskDesc` with it; called with a positive value, it stores that
/// value as `state`, so `SCREEN_WAVE_RAMP_FALLING` fades it out and
/// `SCREEN_WAVE_RAMP_FINISHED` ends it.
void screenWaveRun(s32 arg0)
{
    CdCmdQueue* queue = &gCdCmdQueue;

    if (arg0 <= 0) {
        queue->imageMdecMode                = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
        gScreenWaveSpawnCtx.span            = 1;
        gScreenWaveSpawnCtx.scale           = 0x60;
        gScreenWaveSpawnCtx.r               = 0x40;
        gScreenWaveSpawnCtx.modulateTexture = SCREEN_WAVE_MODULATE_TEXTURE;
        gScreenWaveSpawnCtx.g               = 0x80;
        gScreenWaveSpawnCtx.b               = 0x80;
        taskSpawnFromTable(gScreenWaveTaskDesc, 0, 0, &gScreenWaveSpawnCtx);
        return;
    }
    gScreenWaveSpawnCtx.state = arg0;
}
