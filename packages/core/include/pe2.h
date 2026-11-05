/* What every unit includes first (`#include <core/pe2.h>`), from core, which a package names in its dependencies. A slot of a package is declared, filled by its declaration, and
 * read; the build decides what answers it. It reads a source twice: a first pass, with PE2_FIRST_PASS defined, leaves the live code with
 * every other macro expanded and those below it left as written, for the build to read. */
#ifndef PE2_H
#define PE2_H

/* What core gives a package beside the macros: its types and helpers. */
#include "common.h"

/* PE2_EDITOR_LABEL("Face"): what is declared after it, slots and options alike, is a section of that name in the editor, to the next label
 * or the end of the file, where the label of what included it goes on; "" closes it. */
#define PE2_EDITOR_LABEL(label)

/* PE2_OPTION_DEF(WEAPON_INDEX, int), PE2_OPTION_DEF(IDLE_TIMEOUT_TICKS, int, 900), PE2_OPTION_DEF(FINISH, enum, matte, gloss):
 * a value set on a package in the editor. An int, bool or string may give its default after its type, which what the
 * package sets overrides; one without has to be set. In no #if. */
#define PE2_OPTION_DEF(...)
#define PE2_OPTION(name) pe2_option_##name
#define PE2_OPTION_IS(name, member) pe2_option_##name##_is_##member

/* PE2_CONFIG(release), PE2_CONFIG_IS(target, psx): the build in use, by name, release or target. */
#define PE2_CONFIG(key) pe2_config_##key
#define PE2_CONFIG_IS(key, value) pe2_config_##key##_is_##value

#ifdef PE2_LANGUAGE_SERVER
#include "pe2_server.h"
#else
#include "pe2_options.h"
#endif

/* PE2_BIND_SELF(file): a slot holding a file, proven to be one that holds this package. The package built fills `self`
 * (a slot of its own, kept out of the editor), and the build says whether the file names it. */
#define PE2_BIND_SELF(binding)                \
    PE2_BIND(self, package, _self, _hidden)   \
    PE2_BIND(binding, file)                   \
    STATIC_ASSERT(PE2_QUERY(binding, contains, self), binding##_holds_this_package);

#ifndef PE2_FIRST_PASS

/* PE2_BIND(body, model), PE2_BIND(self, package, _self, _hidden): a slot, in the code's own word, of the type the code
 * holds; after it, what else it is - `_self` filled, where the package's declaration does not fill it, by the package
 * built, and `_hidden` kept out of the editor. A problem with it fires here. */
#define PE2_BIND(binding, ...) pe2_bind_##binding

/* PE2_BIND_ARG(body, _symbol, gBody): how the value at an address is made. */
#define PE2_BIND_ARG(...)

/* PE2_VALUE(body), PE2_VALUE(body, _buffer): what a slot holds as tokens, whole or at a sub-path, a last `_name` being what the target
 * derives from it. */
#define PE2_PICK(a, b, c, d, e, f, which, ...) which
#define PE2_VALUE(...) PE2_PICK(__VA_ARGS__, PE2_VALUE6, PE2_VALUE5, PE2_VALUE4, PE2_VALUE3, PE2_VALUE2, PE2_VALUE1, unused)(__VA_ARGS__)
#define PE2_VALUE1(binding) pe2_value_##binding
#define PE2_VALUE2(binding, a) pe2_value_##binding##_##a
#define PE2_VALUE3(binding, a, b) pe2_value_##binding##_##a##_##b
#define PE2_VALUE4(binding, a, b, c) pe2_value_##binding##_##a##_##b##_##c
#define PE2_VALUE5(binding, a, b, c, d) pe2_value_##binding##_##a##_##b##_##c##_##d
#define PE2_VALUE6(binding, a, b, c, d, e) pe2_value_##binding##_##a##_##b##_##c##_##d##_##e

/* PE2_QUERY(file, contains, self): what the build answers of what a slot holds, 1 or 0 - `contains`, whether it names,
 * anywhere in it, what another slot holds. */
#define PE2_QUERY(...) PE2_PICK(__VA_ARGS__, unused, unused, PE2_QUERY4, PE2_QUERY3, PE2_QUERY2, unused, unused)(__VA_ARGS__)
#define PE2_QUERY2(binding, key) pe2_query_##binding##_##key
#define PE2_QUERY3(binding, key, a) pe2_query_##binding##_##key##_##a
#define PE2_QUERY4(binding, key, a, b) pe2_query_##binding##_##key##_##a##_##b

#ifndef PE2_LANGUAGE_SERVER
#include "pe2_values.h"
#endif

#endif

#endif
