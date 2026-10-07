/* Part of the screen wave library; see screen_wave.h. */

/// Starts or steers the package's screen wave from a one-word event-script request.
///
/// A non-positive `request` seeds the persistent `gScreenWaveSpawnCtx` for a
/// one-frame rise to strength 0x60, tinted RGB 0x40/0x80/0x80, enables 16-bit
/// image mask bits and spawns task-table entry 0. A positive request stores
/// its low signed halfword as the ramp state: `SCREEN_WAVE_RAMP_FALLING`
/// fades the wave, and `SCREEN_WAVE_RAMP_FINISHED` ends it. Zero requests
/// another spawn, so it cannot request a rising ramp on an existing task.
///
/// Scripts must avoid overlapping spawns, keep the package loaded until its
/// wave ends, and send state requests after the wave's initialization tick,
/// which resets the context's state to rising. Spawn failure is not reported.
static void _screenWaveRun(s32 request)
{
    enum {
        SCREEN_WAVE_SCRIPT_RISE_FRAMES   = 1,
        SCREEN_WAVE_SCRIPT_PEAK_STRENGTH = 0x60,
        SCREEN_WAVE_SCRIPT_RED           = 0x40,
        SCREEN_WAVE_SCRIPT_GREEN_BLUE    = 0x80,
    };

    CdCmdQueue* queue = &gCdCmdQueue;

    if (request <= 0) {
        queue->imageMdecMode                = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
        gScreenWaveSpawnCtx.span            = SCREEN_WAVE_SCRIPT_RISE_FRAMES;
        gScreenWaveSpawnCtx.scale           = SCREEN_WAVE_SCRIPT_PEAK_STRENGTH;
        gScreenWaveSpawnCtx.r               = SCREEN_WAVE_SCRIPT_RED;
        gScreenWaveSpawnCtx.modulateTexture = SCREEN_WAVE_MODULATE_TEXTURE;
        gScreenWaveSpawnCtx.g               = SCREEN_WAVE_SCRIPT_GREEN_BLUE;
        gScreenWaveSpawnCtx.b               = SCREEN_WAVE_SCRIPT_GREEN_BLUE;
        taskSpawnFromTable(gScreenWaveTaskDesc, 0, 0, &gScreenWaveSpawnCtx);
        return;
    }
    gScreenWaveSpawnCtx.state = request;
}
