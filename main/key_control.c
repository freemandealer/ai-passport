#include "key_control.h"
void key_input_apply(key_input_t *in, key_input_event_t event) {
    switch (event) {
    case KEY_INPUT_PRESS:
        if (!in->held) { ++in->generation; in->held = in->capture = true; }
        break;
    case KEY_INPUT_RELEASE: in->held = false; break;
    case KEY_INPUT_MODE:
        ++in->generation;
        in->held = in->capture = false;
        in->mode = in->mode == KEY_HUM ? KEY_MUSIC : KEY_HUM;
        break;
    case KEY_INPUT_CLEAR:
        /* ADC ladder callbacks for the new key may precede OK's release.
         * Cancel is unconditional; a late release cannot undo this command. */
        ++in->generation;
        in->held = in->capture = false;
        break;
    }
}
