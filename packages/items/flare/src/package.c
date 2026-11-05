#include "flare.h"

#include <core/pe2.h>

#include <gameplay/display.h>
#include <gameplay/attachment_state.h>
#include <gameplay/attachments.h>
#include <gameplay/effects.h>
#include <gameplay/room_effects.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/sound.h>

PE2_EDITOR_LABEL("Growth")
/// Frames the glow grows for, brightening to the peak; then the spell applies and it fades.
PE2_OPTION_DEF(GROWTH_FRAMES, int, 30)
PE2_OPTION_DEF(PEAK_BRIGHTNESS, int, 255)
/// The discs' radius as they start, and what they gain each frame: the brightness gained, shifted up; the outer disc is
/// twice the inner.
PE2_OPTION_DEF(DISC_RADIUS, int, 0x100)
PE2_OPTION_DEF(DISC_GROWTH_SHIFT, int, 3)
/// The two bands closing in as it grows: where each ends, its width, and how far out it is per frame left, as a shift.
PE2_OPTION_DEF(INNER_BAND_RADIUS, int, 0x800)
PE2_OPTION_DEF(INNER_BAND_WIDTH, int, 0x100)
PE2_OPTION_DEF(INNER_BAND_SPEED_SHIFT, int, 4)
PE2_OPTION_DEF(OUTER_BAND_RADIUS, int, 0xC00)
PE2_OPTION_DEF(OUTER_BAND_WIDTH, int, 0xC0)
PE2_OPTION_DEF(OUTER_BAND_SPEED_SHIFT, int, 5)
/// How much dimmer the outer band is than the inner, as a shift.
PE2_OPTION_DEF(OUTER_BAND_DIM_SHIFT, int, 1)

PE2_EDITOR_LABEL("Color and fade")
/// How much dimmer green and blue are than red, as shifts: the glow is pink.
PE2_OPTION_DEF(GREEN_SHIFT, int, 2)
PE2_OPTION_DEF(BLUE_SHIFT, int, 1)
/// Brightness and radius the discs lose each frame as they fade, under two screen flashes.
PE2_OPTION_DEF(FADE_STEP, int, 8)
PE2_OPTION_DEF(FADE_RADIUS_STEP, int, 0x30)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(PE2_OPTION(GROWTH_FRAMES) > 0 && PE2_OPTION(FADE_STEP) > 0, growth_and_fade);

void flareEffectTask(Task* task)
{
    enum {
        STATE_INITIALIZE = 0,
        STATE_GROW       = 1,
        STATE_FADE       = 2
    };
    /// Draws the two discs, writing their color into `rgbOutput` for the bands and flashes.
#define DRAW_GLOW_DISCS(centreCoord, effectWork, rgbOutput)                                 \
    {                                                                                       \
        (rgbOutput)[0] = (effectWork)->scale;                                               \
        (rgbOutput)[1] = (effectWork)->scale >> PE2_OPTION(GREEN_SHIFT);                    \
        (rgbOutput)[2] = (effectWork)->scale >> PE2_OPTION(BLUE_SHIFT);                     \
        effectDrawGouraudDisc((centreCoord), (effectWork)->angle, (rgbOutput));             \
        effectDrawGouraudDisc((centreCoord), (s16)((effectWork)->angle << 1), (rgbOutput)); \
    }
    EffectWork*      work;
    const GfxCoord*  effectCoord;
    AttachmentState* attachment;
    s32              soundPan;
    s8               soundDepth;
    u8               glowRgb[3];

    attachment  = &gAttachmentState;
    work        = task->spawnArg2.pointer;
    effectCoord = task->extra.coordBody->coord;
    // A held attachment or any non-running PE control ends both sound and drawing.
    if ((attachment->effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING)) {
        sndEvtRequestScriptStop(SOUND_FLARE_USE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        effectKillTask(work, task);
        return;
    }

    work->age++;
    switch (task->state) {
        case STATE_INITIALIZE:
            task->spawnArg1.value = PE2_OPTION(GROWTH_FRAMES);
            work->scale           = 0;
            work->angle           = PE2_OPTION(DISC_RADIUS);
            work->step            = (PE2_OPTION(PEAK_BRIGHTNESS) + 1) / task->spawnArg1.value;
            task->state           = STATE_GROW;
            // Normalize both spatial sound arguments to signed bytes before queuing.
            soundPan   = (s8)worldCoordGetOriginAudioPan(effectCoord);
            soundDepth = worldCoordGetOriginAudioDepth(effectCoord);
            sndEvtRequestScriptStart(SOUND_FLARE_USE, soundPan, soundDepth);
            return;
        case STATE_GROW:
            // Brighten the discs while the two surrounding glow bands contract.
            work->scale += work->step;
            work->angle += work->step << PE2_OPTION(DISC_GROWTH_SHIFT);
            task->spawnArg1.value--;
            DRAW_GLOW_DISCS(effectCoord, work, glowRgb);
            effectDrawOuterGlowBand(effectCoord, (s16)((task->spawnArg1.value << PE2_OPTION(INNER_BAND_SPEED_SHIFT)) + PE2_OPTION(INNER_BAND_RADIUS)), PE2_OPTION(INNER_BAND_WIDTH), glowRgb);
            glowRgb[0] >>= PE2_OPTION(OUTER_BAND_DIM_SHIFT);
            glowRgb[1] >>= PE2_OPTION(OUTER_BAND_DIM_SHIFT);
            glowRgb[2] >>= PE2_OPTION(OUTER_BAND_DIM_SHIFT);
            effectDrawOuterGlowBand(effectCoord, (s16)((task->spawnArg1.value << PE2_OPTION(OUTER_BAND_SPEED_SHIFT)) + PE2_OPTION(OUTER_BAND_RADIUS)), PE2_OPTION(OUTER_BAND_WIDTH), glowRgb);
            if (task->spawnArg1.value == 0) {
                // Request the gameplay result after the final growth draw.
                work->scale        = PE2_OPTION(PEAK_BRIGHTNESS);
                task->state        = STATE_FADE;
                work->period       = 0x600; // Nothing reads it
                work->step         = 0;
                attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
            }
            return;
        case STATE_FADE:
            if (work->scale >= PE2_OPTION(FADE_STEP) + 1) {
                // Both screen passes use the brightness from before this update's decay.
                DRAW_GLOW_DISCS(effectCoord, work, glowRgb);
                work->scale -= PE2_OPTION(FADE_STEP);
                work->angle -= PE2_OPTION(FADE_RADIUS_STEP);
                effectDrawScreenTint(glowRgb, GPU_BLEND_ADD);
                effectDrawScreenTint(glowRgb, GPU_BLEND_ADD);
                return;
            }
            effectKillTask(work, task);
            return;
    }
#undef DRAW_GLOW_DISCS
}
