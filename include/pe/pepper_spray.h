#ifndef INCLUDE_PE_PEPPER_SPRAY_H
#define INCLUDE_PE_PEPPER_SPRAY_H

#include "main/task_types.h"

/// Runs the nine-frame pepper-spray flash and spray effect.
///
/// Expects a freshly spawned coordinate-body effect task with an owned
/// `EffectWork` at `Task::spawnArg2.pointer` and a composed coordinate cache.
/// Startup seeds the billboard size and rotation and six retained cone angles,
/// activates transient light slot 0 and requests attachment-stat application.
/// Subsequent frames fade size and brightness while rotation stays fixed.
/// Releases the effect work and task at frame nine, or stops the sound and
/// releases them when the attachment is held or PE effects are not running.
void pepperSprayEffectTask(Task* task);

#endif // INCLUDE_PE_PEPPER_SPRAY_H
