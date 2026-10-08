/* Part of the factory lift library; see factory_lift.h. */

/// Starts the operator panel's hotspot scan with a centered idle cursor.
///
/// Requires the live action prompt spawned during panel initialization.
/// Enables aiming and advances from ARM_PROMPT to IDLE without changing work.
static void _factoryPanelArmPrompt(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}
