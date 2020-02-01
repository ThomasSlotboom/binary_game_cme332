#include "address_map_nios2.h"
#include "globals.h" // defines global values

extern volatile double time;
extern volatile int * interval_timer_ptr;
extern volatile int state;
extern volatile int frame;
extern volatile int task1flag, task2flag, task3flag;
extern volatile int frame_overrun;

/*******************************************************************************
 * Interval timer interrupt service routine
 ******************************************************************************/
void interval_timer_ISR() {
    // volatile int * interval_timer_ptr = (int *)TIMER_BASE;
    *(interval_timer_ptr) = 0; // clear the interrupt

    if (state == PLAY){
      if (time > 0)
        time = time - 0.1;
      else
        time = 30;
    }
    if (task1flag || task2flag || task3flag){
      frame_overrun = 1;
    }
    frame = 1;

    return;
}
