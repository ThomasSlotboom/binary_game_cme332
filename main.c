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
// struct to hold the game's information about time
struct {
  int tenCounter; // counts every frame (1 frame = 100 ms)
  int time; // counts down during each question
  int total_game_time; // keeps track of total game time
  int start_time;
} t;
// state of the game
int volatile state = IDLE;
// power - on or off
int volatile power = OFF;
// flags to check for frame overrun
int task1flag, task2flag, task3flag = 0;
// flag to confirm a frame overrun occurred
int frame_overrun = 0;
// flag to indicate a new frame has been reached
int volatile frameFlag = 0;
// flags to check if a key has been pressed
volatile int key0, key1, key2 = 0;
// show answer to binary number
int answers = OFF;
// answer to be submitted
int submit_answer = OFF;
// binary number that player must match
int number;
// inputted number from player
int playerNum;
// The question number the palyer is on;
int question_number = 1;
// level of the game;
int level = 1;
// The score of the player
int score = 0;
// point awarded when a question is answered correctly
int points_awarded = 1;


/*******************************************************************************
                                    Functions
*******************************************************************************/
void reset_game(void);
void rand_num(void);

/*
Intiallizes clock and enables interrupts
*/
void startup(void){
  // time initially is 30 seconds

  reset_game();

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
}

/*******************************************************************************
                            Task1(View)
                    Display hex and LEDs
*******************************************************************************/

/*
Check to see if a bit needs to be changed and clear that bit to zero if
necessary, then load the register with a value. Loads one byte of the register.
Helps efficiently clear registers of bits so that LED and hex displays aren't
given a 50% duty cycle from constantly being turned off each frame.
Paramters:
  ptr - pointer to register to be changed
  number - number to be loaded into the register
  offset - offset from the beginning of the register (in bytes)
*/
void load_register(volatile int* ptr, int num, int offset){
  int regWalker = (*ptr)>>offset*BYTE_SIZE;
  int numWalker = num;
  int startIndex = offset*BYTE_SIZE;
  int endIndex = startIndex + BYTE_SIZE;
  for (int i=startIndex; i<endIndex; i++){
    // if the reg at index i is on and the number has a 0 at that spot:
    if (((regWalker & 1) == 1) && ((numWalker & 1) == 0)){
      *ptr &= ~(1<<i); //clear that bit
    }
    regWalker >>= 1;
    numWalker >>= 1;
  }
  *ptr |= (num<<(offset*BYTE_SIZE));
}

// display answers on ledr7-0
void display_answers(void){
  load_register(ledr_ptr, number, 0);
}

// display decimal number on hex2-0
void display_question(void){
  int dig_1, dig_10, dig_100;
  dig_1 = number % 10;
  dig_10 = ((number - dig_1) / 10) % 10;
  dig_100 = ((number - dig_10*10 - dig_1)) / 100;

  load_register(hex_3_0_ptr, lut_num[dig_1], 0);
  load_register(hex_3_0_ptr, lut_num[dig_10], 1);
  load_register(hex_3_0_ptr, lut_num[dig_100], 2);
}

void display_timer(void){
  int dig_1, dig_10;
  dig_1 = t.time % 10;
  dig_10 = ((t.time - dig_1) / 10) % 10;

  load_register(hex_7_4_ptr, lut_num[dig_1], 0);
  load_register(hex_7_4_ptr, lut_num[dig_10], 1);
}

void display_score(void){
  int dig_1, dig_10;
  dig_1 = score % 10;
  dig_10 = ((score - dig_1) / 10) % 10;
  load_register(hex_7_4_ptr, lut_num[dig_1], 2);
  load_register(hex_7_4_ptr, lut_num[dig_10], 3);
}

void display_total_time(){
  int seconds, minutes;
  seconds = t.total_game_time % 60;
  minutes = t.total_game_time / 60;

  int dig_1, dig_10, dig_100, dig_1000;
  dig_1 = seconds % 10;
  dig_10 = ((seconds - dig_1) / 10);
  dig_100 = minutes % 10;
  dig_1000 = (minutes - dig_100) / 10;

  load_register(hex_3_0_ptr, lut_num[dig_1], 0);
  load_register(hex_3_0_ptr, lut_num[dig_10], 1);
  load_register(hex_3_0_ptr, lut_num[dig_100], 2);
  load_register(hex_3_0_ptr, lut_num[dig_1000], 3);
}

/**
Clears the hex display at the position given
Parameters:
  display: the hex display number to be cleared
*/
void clear_hex(int hexDisplayNum){
  int* address;
  if (hexDisplayNum < 4)
    address = hex_3_0_ptr;
  else if (hexDisplayNum >= 4 && hexDisplayNum < 8)
    address = hex_7_4_ptr;
  else{
    printf("Invalid hex display number");
    return;
  }
  *(address) &= ~(0xFF << (hexDisplayNum % 4) * BYTE_SIZE);

}

/*
Clear all the hex displays
*/
void clear_all_hex(void){
  for (int i = 0; i < 8; i++){
    clear_hex(i);
  }
}

void clear_all_red_LED(){
  *ledr_ptr &= ~0xFF;
}

/*
Clear all the red and green LEDs
*/
void clear_all_LED(void){
  *ledr_ptr &= ~0xFFFF;
  *ledg_ptr &= ~0xFFFF;
}

void task1(){
  task1flag = 1;
  *ledg_ptr |= 0x2; // ledg1 on

  if (power == ON){
    if (state != IDLE){
      display_timer();
      display_score();
      if  (state == PLAY){

        display_question();
        clear_hex(3);
        if (answers == ON){
          display_answers();
        }
        else {
          clear_all_red_LED();
        }
      }
      else{
        clear_hex(0);
        clear_hex(1);
        clear_hex(2);
        clear_hex(3);
        clear_all_red_LED();
      }

    }
    else{
      display_total_time();
      display_score();
      clear_hex(4);
      clear_hex(5);
    }
  }
  else{
    clear_all_hex();
    clear_all_LED();
  }
  // for (int i=0;i<30000000;i++){} // check for frame overrun

  task1flag = 0;
  *ledg_ptr &= ~0x2; // ledg1 off
}

/*******************************************************************************
                            Task2 (Controller)
                    Read switches and buttons
*******************************************************************************/


void read_keys(void){
  if (key0){
    if (state == PAUSE){
      state = IDLE;
      reset_game();
    }
    key0 = 0;
  }
  if (key1){
    switch (state)
    {
      case IDLE:
        state = PLAY;
        reset_game();
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
    if (state == PLAY)
      submit_answer = ON;
    key2 = 0;
  }
}

void read_switches(void){
  if ((*(slider_switch_ptr) & 0x20000) == 0x20000){ // poll SW17
    power = ON;
  }
  else
    power = OFF;

  if ((*(slider_switch_ptr) & 0x10000) == 0x10000) // poll SW16
    answers = ON;
  else
    answers = OFF;

  playerNum = *(slider_switch_ptr) & 0xFF;
}

void task2(void){
  task2flag = 1;
  *ledg_ptr |= 0x4; // ledg2 on

  read_switches();
  read_keys();

  task2flag = 0;
  *ledg_ptr &= ~0x4; // ledg2 off
}

/*******************************************************************************
                            Task3 (Model)
                    Update values within game, and states
*******************************************************************************/

// get a random number from the modulus of the lower snapshot of the timer
void rand_num(void){
  *(interval_timer_ptr +0x4) = number;
  unsigned int snapshot = *(interval_timer_ptr +0x4);
  number = snapshot&0xFF;
  if (number == 0){
    number = 1;
  }
}

void timer_handler(void){
  if (state == PLAY){
    t.tenCounter++;
    if (t.tenCounter == 10){
      t.tenCounter = 0;
      t.total_game_time++;
      if (t.time > 0){
        t.time--;
      }
      else
        t.time = 0;
    }
  }
}

void reset_game(void){
  t.time = t.start_time;
  t.tenCounter = 0;
  t.total_game_time = 0;
  t.start_time = 30;
  question_number = 0;
  score = 0;
  rand_num();
}

void level_handler(void){
  level = question_number / 10; // 1 level for every 10 questions
  t.start_time = 30 - level;
  points_awarded = 1 + level;
}

void task3(void){
  task3flag = 1;
  *ledg_ptr |= 0x8; // ledg3 on

  if (power == ON){
    timer_handler();
    if (t.time == 0){
      state = IDLE;
    }
    level_handler();


    if (state != IDLE){
      if (submit_answer){
        if (playerNum == number){
          question_number++;
          rand_num();
          t.time = t.start_time;
          t.tenCounter = 0;
          score += points_awarded;
        }
        else
        submit_answer = OFF;
      }
    }
  }
  else{ // if power is off, reset game and go to idle state
    reset_game();
    state = IDLE;
  }

  task3flag = 0;
  *ledg_ptr &= ~0x8; // ledg3 off
}

/*******************************************************************************
                            Main
*******************************************************************************/


void main(void) {
  startup();

  while (1){
    if (frameFlag){
      *ledg_ptr &= ~0x80; // tasks are running, led7 off
      task1(); // update hex/leds
      task2(); // read switches and keys
      task3(); // update game states
      frameFlag = 0;
    }
    if (power == ON){
      if (!frameFlag){
        *ledg_ptr |= 0x80; // turn on ledg7 when no task is running
      }
      if (frame_overrun){
        *ledg_ptr |= 0x100; // turn on ledg8 if a frame overrun occurs
      }
    }
    else{
      clear_all_LED();
    }
  }
}
