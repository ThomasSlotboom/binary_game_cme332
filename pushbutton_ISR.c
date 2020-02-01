#include "address_map_nios2.h"
#include "globals.h" // defines global values

extern volatile int * KEY_ptr;
extern volatile int state;
extern volatile int key0, key1, key2;


/*******************************************************************************
 * Pushbutton - Interrupt Service Routine
 *
 * This routine checks which KEY has been pressed and updates the global
 * variables as required.
 ******************************************************************************/
void pushbutton_ISR(void) {
    // KEY_ptr           = (int *)KEY_BASE;
    // slider_switch_ptr = (int *)SW_BASE;
    int            press;

    press          = *(KEY_ptr + 3); // read the pushbutton interrupt register
    *(KEY_ptr + 3) = press;          // Clear the interrupt

    if (press & 0x1){ key0 = 1;}
    if (press & 0x2){ key1 = 1;}
    if (press & 0x2){ key2 = 1;}


    return;
}
