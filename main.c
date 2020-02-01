#include "address_map_nios2.h"
#include "globals.h" // defines global values
#include "nios2_ctrl_reg_macros.h"
#include <stdio.h>

/*******************************************************************************

********************************************************************************/
/* Declare volatile pointers to I/O registers (volatile means that IO load
 * and store instructions will be used to access these pointer locations,
 * instead of regular memory loads and stores)
 */
volatile int * KEY_ptr =            (int*) KEY_BASE;
volatile int * slider_switch_ptr =  (int*) SW_BASE;
volatile int * ledg_ptr =           (int*) LEDG_BASE;
volatile int * ledr_ptr =           (int*) LEDR_BASE;
volatile int * hex_3_0_ptr =        (int*) HEX3_HEX0_BASE;
volatile int * hex_7_4_ptr =        (int*) HEX7_HEX4_BASE;
volatile int * interval_timer_ptr = (int*) TIMER_BASE;

/*
lut of mulitpliers to make calculation of digits for timer easy - multiply by
the multiplier to put the digit of interest in the ones place.
Multiplying by the
0th index gives the ones place
1st index gives the tens place
2nd index gives the hundreds place
3rd index gives the thousands place
*/
const double lut_multiplier[] = {
  1,
  0.1,
  0.01,
  0.001
};
// lut of values that display a corresponding number on the hex display
const int lut_num[] = {
  0x3F, // 0
  0x06, // 1
  0x5B, // 2
  0x4F, // 3
  0x66, // 4
  0x6D, // 5
  0x7D, // 6
  0x07, // 7
  0x7F, // 8
  0x6F, // 9
  0x40  // - (error)
};
// currently displayed digits, goes from index 0 LSB to index 3 MSB
int next_dig[4] = {1,1,1,1};
// digits to be displayed, goes from index 0 LSB to index 3 MSB
int current_dig[4] = {3,0,0,0};
// variable to deal with time
volatile double time;

// state of the game
int volatile state = IDLE;
// power - on or off
int volatile power = OFF;
// binary number that player must match
int number = 15;
// inputted number from player
int playerNum;
// show answer to binary number
int answers = OFF;
// flags to check if a key has been pressed
volatile int key0, key1, key2 = 0;
// answer to be submitted
int submit_answer = 0;

// flags to check for frame overrun
int task1flag, task2flag, task3flag = 0;
// flag to confirm a frame overrun occurred
int frame_overrun = 0;
// flag to indicate a new frame has been reached
int volatile frame = 0;

/*******************************************************************************
                                    Functions
*******************************************************************************/
/*
Intiallizes clock and enables interrupts
*/
int startup(void){
  // time initially is 30 seconds
  time = 30;

  /* set the interval timer period for incrementing the timer */
  int counter = 5000000; // 1/(50MHz) x (5,000,000) = 0.1 sec
  *(interval_timer_ptr + 0x2) = (counter & 0xFFFF);
  *(interval_timer_ptr + 0x3) = (counter >> 16) & 0xFFFF;

  /* start interval timer, enable its interrupts */
  *(interval_timer_ptr + 1) = 0x7; // STOP = 0, START = 1, CONT = 1, ITO = 1

  *(KEY_ptr + 2) = 0xF; // enable interrupts for all pushbuttons

  /* set interrupt mask bits for levels 0 (interval timer) and level 1
   * (pushbuttons) */
  NIOS2_WRITE_IENABLE(0x3);
  NIOS2_WRITE_STATUS(1); // enable Nios II interrupts
  return 0;
}


int display_timer(void){
  int i;
  for (i=0; i < 4; i++){
    next_dig[i] =  ((int) (time * lut_multiplier[i])) % 10;
  }

  for (i=0; i < 4; i++){
    if (current_dig[i] != next_dig[i]){
      current_dig[i] = next_dig[i];
      *(hex_3_0_ptr) &= ~(0xFF << i*BYTE_SIZE);
      *(hex_3_0_ptr) |= (lut_num[current_dig[i]] << i*BYTE_SIZE);
    }
  }
  return 0;
}

int display_question_time(){
  int i;
  for (i=0; i < 2; i++){
    next_dig[i] =  ((int) (time * lut_multiplier[i])) % 10;
  }

  for (i=0; i < 2; i++){
    if (current_dig[i] != next_dig[i]){
      current_dig[i] = next_dig[i];
      *(hex_7_4_ptr) &= ~(0xFF << i*BYTE_SIZE);
      *(hex_7_4_ptr) |= (lut_num[current_dig[i]] << i*BYTE_SIZE);
    }
  }
}

/**
Clears the hex display at the position given
Parameters:
  display: the hex display number to be cleared
*/
int clear_hex(int hexDisplayNum){

  if (hexDisplayNum < 4)
    *(hex_3_0_ptr) &= ~(0xFF << hexDisplayNum * BYTE_SIZE);
  else if (hexDisplayNum >= 4 && hexDisplayNum < 8)
    *(hex_7_4_ptr) &= ~(0xFF << (hexDisplayNum % 4) * BYTE_SIZE);
  else
    printf("Invalid hex display number");
  return 0;
}

/**
Clear all the hex displays
*/
int clear_all_hex(void){
  for (int i = 0; i < 8; i++){
    clear_hex(i);
  }
  return 0;
}

int read_switches(void){
  if ((*(slider_switch_ptr) & 0x20000) == 0x20000) // poll SW17
    power = ON;
  else
    power = OFF;

  if ((*(slider_switch_ptr) & 0x10000) == 0x10000) // poll SW16
    answers = ON;
  else
    answers = OFF;

  playerNum = *(slider_switch_ptr) & 0xFF;
  return 0;
}

int display_answers(void){
  int i;
  for (i=0; i<8; i++){
    // if the LED at point 1<<i is on and the answer has a 0 at that spot:
    if (((*ledr_ptr & 1<<i) == 1) && ((number & 1<<i) == 0))
      *ledr_ptr &= ~1<<i; //clear that bit
    *ledr_ptr |= number;
  }
  return 0;
}

int clear_LED(void){
  *ledr_ptr &= ~0xFF;
  return 0;
}


int read_keys(void){
  if (key0){
    if (state == PAUSE){
      state = IDLE;
    }
    key0 = 0;
  }
  if (key1){
    switch (state)
    {
      case IDLE:
        state = PLAY;
        break;
      case PLAY:
        state = PAUSE;
        break;
      case PAUSE:
        state = PLAY;
        break;
      default:
        state = IDLE;
    }
    key1 = 0;
  }
  if (key2){
    submit_answer = 1;
    key2 = 0;
  }
  return 0;
}


int task1(){
  task1flag = 1;
  if (power == ON){
    if (state != IDLE){
      display_question_time();
      if (answers == ON){ display_answers(); }
      else{ clear_LED(); }
    }

  }
  else{
    clear_all_hex();
    clear_LED();
  }
  task1flag = 0;
  return 0;
}

int task2(){
  task2flag = 1;
  read_switches();
  read_keys();
  task2flag = 0;
  return 0;
}

int task3(){
  task2flag = 1;
  if (power == ON){

  }
  else{
    state = IDLE;
  }
  task3flag = 0;
  return 0;
}

void frame_overrun_display(void){
  *ledg_ptr |= 0x
}

int main(void) {
  startup();

  while (1){
    if (frame){
      *ledg_ptr &= ~0x80;
      task1(); // update hex/leds
      task2(); // read switches and keys
      task3(); // update game states
      frame = 0;
    }
    if (!frame){
      *ledg_ptr |= 0x80; // turn on ledg7 when no task is running
    }
    if (frame_overrun){
      *ledg_ptr |= 0x100; // turn on ledg8 if a frame overrun occurs
    }

  }
}
