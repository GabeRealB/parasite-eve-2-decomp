/* Part of the screen wave library; see wave.h. */

/// Event callback: a non-positive `request` starts a tinted wave at full strength; a positive one sets its ramp
/// phase (`SCREEN_WAVE_RAMP_FALLING` fades it, `SCREEN_WAVE_RAMP_FINISHED` ends it).
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
        queue->imageMdecMode                      = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
        SCREEN_WAVE_SPAWN_CONTEXT.span            = SCREEN_WAVE_SCRIPT_RISE_FRAMES;
        SCREEN_WAVE_SPAWN_CONTEXT.scale           = SCREEN_WAVE_SCRIPT_PEAK_STRENGTH;
        SCREEN_WAVE_SPAWN_CONTEXT.r               = SCREEN_WAVE_SCRIPT_RED;
        SCREEN_WAVE_SPAWN_CONTEXT.modulateTexture = SCREEN_WAVE_MODULATE_TEXTURE;
        SCREEN_WAVE_SPAWN_CONTEXT.g               = SCREEN_WAVE_SCRIPT_GREEN_BLUE;
        SCREEN_WAVE_SPAWN_CONTEXT.b               = SCREEN_WAVE_SCRIPT_GREEN_BLUE;
        taskSpawnFromTable(_gScreenWaveTaskDescs, 0, 0, &SCREEN_WAVE_SPAWN_CONTEXT);
        return;
    }
    SCREEN_WAVE_SPAWN_CONTEXT.state = request;
}
