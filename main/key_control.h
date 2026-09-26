#pragma once
#include "key_analyzer.h"
typedef enum { KEY_INPUT_PRESS, KEY_INPUT_RELEASE, KEY_INPUT_MODE, KEY_INPUT_CLEAR } key_input_event_t;
typedef struct { uint32_t generation; bool held, capture; key_mode_t mode; } key_input_t;
/* Complete-state overwrite mailbox: release cannot be dropped during DSP.
 * Generation distinguishes rapid re-presses from the previous recording. */
void key_input_apply(key_input_t *input, key_input_event_t event);
