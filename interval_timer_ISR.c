#include "address_map_nios2.h"
#include "globals.h" // defines global values

extern volatile int time;
extern volatile int tenCounter;
extern volatile int * interval_timer_ptr;
extern volatile int state;
extern volatile int frameFlag;
extern volatile int task1flag, task2flag, task3flag;
extern volatile int frame_overrun;

/*******************************************************************************
 * Interval timer interrupt service routine
 ******************************************************************************/
void interval_timer_ISR() {
    *(interval_timer_ptr) = 0; // clear the interrupt
    if (task1flag || task2flag || task3flag){
      frame_overrun = 1;
    }
    frameFlag = 1;

    return;
}
