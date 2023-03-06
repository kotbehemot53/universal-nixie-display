#include <Arduino.h>
#include <Wire.h>

//TODO: add command to run intro
//TODO: include code execution in the waits (is it worth it?) or at least the interrupts?
//DONE: measure elapsed frame time and subtract from bright/dim waits (is it worth it?)
//TODO: instead of "manual" gradual dimming, include ready routines for it and commands to trigger them (is it really needed?)
//TODO: think about improving points dimming (is it possible?)
//TODO: think about pull-downs on anodes & maybe shorter AFTER_IMAGE_US thanks to that?
//TODO: think about testing PWM dimming (each anode on a PWM output) using PWM registers with no clock divider (at max freq) (is it worth it?)

//DONE: support HV circuit switching (command)
//DONE: support commas
//DONE: 2-6 digits support

//you can change this to adjust the code to the physical display length (values 2-6 are allowed)
const unsigned short DIGITS_USED = 6;

//don't change this:
const unsigned short MAX_DIGITS_USED = 6;

//number commands - last 4 bits set the number, anything above 9 is no light at all on the lamp
//REMEMBER - all CMDs within <0x80 - 0xD0> are treated as a number command - so don't use those for other purposes!
const byte CMD_NUM = 0x80;

//regular commands
const byte CMD_OFF = 0x10; //turn the lamps off
const byte CMD_ON = 0x11; //turn the lamps on
const byte CMD_INTRO_ON = 0x12; //turn the intro on
const byte CMD_FIN = 0x20; //loads number and dimming values from buffers to output (renders new frame)
const byte CMD_NOOP = 0x30;
const byte CMD_START = 0x40; //currently unused - works as NOOP
const byte CMD_POINT = 0x50; //last 4 bits determine which point should be turned on (only values 0 and 1 are allowed)
const byte CMD_DIMMER = 0x60; //last 4 bits set the duty cycle; it's assigned to the latest digit set
const byte CMD_LAMP_POINT_L = 0xE0; //last 4 bits determine which left comma should be on (only values between 0 and DIGITS_USED - 1 are allowed)
const byte CMD_LAMP_POINT_R = 0xF0; //last 4 bits determine which right comma should be on (only values between 0 and DIGITS_USED - 1 are allowed)

//responses
const byte RESP_SUCCESS = 0x00;
const byte RESP_FAIL = 0b01010101;

//pins
const byte OUT_BCD[] = {4,5,6,7}; //pins for encoding currently displayed number (in BCD)
const byte OUT_POINTS[] = {12,13}; //pins for setting decimal points (neon lamps)
//array size must equal MAX_DIGITS_USED
const byte OUT_ANODES[] = {2,3,8,9,10,11}; //pins for multiplexing lamps (they switch power to particular anodes)
const byte OUT_LAMP_COMMAS[] = {A1, A2}; //pins for decimal points in main lamps
const byte HV_ENABLE = A0;
const byte STATUS = A3;

//times
//Afterimage occurs below 300 us
const unsigned short AFTER_IMAGE_US = 300; //interframe interval to avoid afterimages
const unsigned short FRAME_US = 1000; //frame time
const float DIMMING_CURVE_POWER = 2.1;

//values
const byte LAMP_OFF = 15;

//buffers & other globals
//TODO: should these be volatile? it triggers warnings...
bool point_vals[2] = {false, false};
bool point_vals_buffer[2] = {false, false};
bool points_off[2] = {false, false};

//array sizes must equal MAX_DIGITS_USED
bool commas_off[MAX_DIGITS_USED] = {false,false,false,false,false,false};
bool l_comma_vals[MAX_DIGITS_USED] = {false,false,false,false,false,false};
bool l_comma_vals_buffer[MAX_DIGITS_USED] = {false,false,false,false,false,false};
bool r_comma_vals[MAX_DIGITS_USED] = {false,false,false,false,false,false};
bool r_comma_vals_buffer[MAX_DIGITS_USED] = {false,false,false,false,false,false};

//array sizes must equal MAX_DIGITS_USED
byte nums[MAX_DIGITS_USED] = {0, 0, 0, 0, 0, 0};
byte nums_buffer[MAX_DIGITS_USED] = {15, 15, 15, 15, 15, 15};
byte fail_nums[MAX_DIGITS_USED] = {15, 15, 15, 15, 15, 15};

//array sizes must equal MAX_DIGITS_USED
unsigned short bright_times[] = {FRAME_US,FRAME_US,FRAME_US,FRAME_US,FRAME_US,FRAME_US};
unsigned short dim_times[] = {0,0,0,0,0,0};
byte dimmer_buffer[] = {15,15,15,15,15,15};
byte dimmer_defaults[] = {15,15,15,15,15,15};

byte curr_lamp_idx = 0;
byte curr_l_point_idx = 0;
byte curr_r_point_idx = 0;

volatile bool new_frame = false;

//in the beginning, intro is on
bool intro_mode = true;

//debug
unsigned long current_frame_no = 0;

/**
 * Renders given number
 */
void numOut( byte num )
{
    for (byte i = 0; i < 4; i++) {
        digitalWrite( OUT_BCD[i], ( num & ( 1 << i ) ) != 0 );
    }
}

/**
 * Multiplexes one lamp
 */
void multiplexDigit(byte anode_idx)
{
    bool show_num = (nums[anode_idx] >= 0) && (nums[anode_idx] < 10);
    //points are multiplexed together with anode 0 and 1:
    bool show_point = (anode_idx == 0 || anode_idx == 1) && point_vals[anode_idx];

    bool show_l_comma = l_comma_vals[anode_idx];
    bool show_r_comma = r_comma_vals[anode_idx];

    unsigned long numProcessHighStart = micros();
    if (show_l_comma) {
        digitalWrite(OUT_LAMP_COMMAS[0], HIGH);
    }
    if (show_r_comma) {
        digitalWrite(OUT_LAMP_COMMAS[1], HIGH);
    }
    if (show_num) {
        numOut(nums[anode_idx]); //important NOT to set the numOut when there's no need - at high framerates it confuses the russian and causes microblinks on the off lamp
        digitalWrite(OUT_ANODES[anode_idx], HIGH );
    }
    if (show_point) {
        digitalWrite(OUT_POINTS[anode_idx], HIGH );
    }
    unsigned long numProcessHighEnd = micros();

    //variable delay to compensate for unpredictable duration of the section above
    unsigned int final_delay = (bright_times[anode_idx]) - (numProcessHighEnd - numProcessHighStart);
    if (final_delay > bright_times[anode_idx]) { //a way to check < 0 (unsigned numbers!) - both for overflow & too long processing
        final_delay = 1;
    }
    delayMicroseconds(final_delay);


    unsigned long numProcessLowStart = micros();
    if (show_num) {
        digitalWrite(OUT_ANODES[anode_idx], LOW );
    }
    if (show_point) {
        digitalWrite(OUT_POINTS[anode_idx], LOW );
    }
    if (show_l_comma) {
        digitalWrite(OUT_LAMP_COMMAS[0], LOW);
    }
    if (show_r_comma) {
        digitalWrite(OUT_LAMP_COMMAS[1], LOW);
    }
    unsigned long numProcessLowEnd = micros();

    //variable delay to compensate for unpredictable duration of the section above
    final_delay = (dim_times[anode_idx]) - (numProcessLowEnd - numProcessLowStart);
    if (final_delay > dim_times[anode_idx]) { //a way to check < 0 (unsigned numbers!) - both for overflow & too long processing
        final_delay = 1;
    }
    delayMicroseconds(final_delay);
}

void setupIntroValues()
{
    unsigned short frames_60 = 10;
    unsigned short frames_2digs = 50;
    unsigned short frames_2 = 300;

    if (point_vals[0] == point_vals[1]) {
        point_vals[0] = !point_vals[1];
    }
    if (current_frame_no % 100 == 0) {
        point_vals[0] = !point_vals[0];
        point_vals[1] = !point_vals[1];
    }

    for (byte j = 0; j < DIGITS_USED; j++) {
        //right commas looped sweep
        if (current_frame_no % (DIGITS_USED * frames_60) == j * frames_60) {
            for (byte k = 0; k < DIGITS_USED; k++) {
                if (k == j) {
                    r_comma_vals[k] = true;
                } else {
                    r_comma_vals[k] = false;
                }
            }
        }
        //numbers on one by one
        if ((current_frame_no >= j * frames_2digs) && (current_frame_no % 10 == 0)) {
            nums[j] = random() % 10;
        }
        //brightness gradual up
        float booblator = (float)(current_frame_no - j * frames_2digs);
        if (booblator < 1)
            booblator = 1.0;
        else if (booblator > (float)frames_2)
            booblator = (float)frames_2;
        float multiplier = pow(booblator/(float)frames_2, 2.5);
        bright_times[j] = multiplier * FRAME_US;
        dim_times[j] = (1.0 - multiplier) * FRAME_US;
    }
}

/**
 * Actual command handling
 */
byte handleInput (byte in_byte)
{
    //we received a command, so we're serious now...
    intro_mode = false;

    if ((in_byte & 0xF0) == CMD_START) {
        //unused now - same as NOOP
        return RESP_SUCCESS;
    }
    if (in_byte == CMD_ON) {
        digitalWrite(HV_ENABLE, HIGH);

        return RESP_SUCCESS;
    }
    if (in_byte == CMD_OFF) {
        digitalWrite(HV_ENABLE, LOW);

        return RESP_SUCCESS;
    }
    if (in_byte == CMD_INTRO_ON) {
        //...except if we don't want to be serious
        intro_mode = true;

        return RESP_SUCCESS;
    }
    if ((in_byte & 0xF0) == CMD_LAMP_POINT_L) {
        curr_l_point_idx = in_byte & 0x0F;
        if (curr_l_point_idx >= DIGITS_USED) {
            //digit above the number of digits used is not allowed
            return RESP_FAIL;
        }

        l_comma_vals_buffer[curr_l_point_idx] = true;

        return RESP_SUCCESS;
    }
    if ((in_byte & 0xF0) == CMD_LAMP_POINT_R) {
        curr_r_point_idx = in_byte & 0x0F;
        if (curr_r_point_idx >= DIGITS_USED) {
            //digit above the number of digits used is not allowed
            return RESP_FAIL;
        }

        r_comma_vals_buffer[curr_r_point_idx] = true;

        return RESP_SUCCESS;
    }
    if (in_byte & CMD_NUM) {  //a number was sent
        curr_lamp_idx = (in_byte & 0x70) >> 4;
        if (curr_lamp_idx >= DIGITS_USED) {
            //digit above the number of digits used is not allowed
            return RESP_FAIL;
        }

        nums_buffer[curr_lamp_idx] = in_byte & 0x0F;

        return RESP_SUCCESS;
    }
    if ((in_byte & 0xF0) == CMD_FIN) {
        new_frame = true;

        return RESP_SUCCESS;
    }
    if ((in_byte & 0xF0) == CMD_NOOP) {
        return RESP_SUCCESS;
    }
    if ((in_byte & 0xF0) == CMD_POINT) {
        byte point_idx = in_byte & 0x0F;
        if (point_idx < 0 || point_idx > 1) {
            return RESP_FAIL;
        }

        point_vals_buffer[point_idx] = true;

        return RESP_SUCCESS;
    }
    if ((in_byte & 0xF0) == CMD_DIMMER) {
        dimmer_buffer[curr_lamp_idx] = in_byte & 0x0F;

        //if dimming set to 0, shut the lamp off completely
        if (!dimmer_buffer[curr_lamp_idx]) {
            nums_buffer[curr_lamp_idx] = LAMP_OFF;
        }

        return RESP_SUCCESS;
    }

    return RESP_FAIL;
}

/**
 * Handles incoming message event
 */
void receiveEvent(int howMany)
{
    while(Wire.available()) // loop through all
    {
        byte c = Wire.read(); // receive byte
        handleInput(c); //return it?
    }
}

void handleNewFrame()
{
    memcpy(point_vals, point_vals_buffer, 2*sizeof(bool));
    memcpy(point_vals_buffer, points_off, 2*sizeof(bool));
    memcpy(nums, nums_buffer,MAX_DIGITS_USED*sizeof(byte));
    memcpy(nums_buffer, fail_nums, MAX_DIGITS_USED*sizeof(byte));
    memcpy(l_comma_vals, l_comma_vals_buffer,MAX_DIGITS_USED*sizeof(byte));
    memcpy(l_comma_vals_buffer, commas_off, MAX_DIGITS_USED*sizeof(byte));
    memcpy(r_comma_vals, r_comma_vals_buffer,MAX_DIGITS_USED*sizeof(byte));
    memcpy(r_comma_vals_buffer, commas_off, MAX_DIGITS_USED*sizeof(byte));
    for (byte i = 0; i < MAX_DIGITS_USED; i++) {
        float multiplier = pow( (float)(dimmer_buffer[i] + 1)/16.0, DIMMING_CURVE_POWER );
        bright_times[i] =(short) ((float)FRAME_US * multiplier);
        dim_times[i] = (short) ((float)FRAME_US * ( 1 - multiplier ));
    }
    memcpy(dimmer_buffer, dimmer_defaults, MAX_DIGITS_USED*sizeof(byte));
    curr_lamp_idx = 0;
}

//silly - pauses lamps
//void statusBlink(int count = 1) {
//    for (int i = 0; i < count; i++) {
//        digitalWrite(STATUS, LOW);
//        delay(100);
//        digitalWrite(STATUS, HIGH);
//        delay(100);
//    }
//}

void debugSetABunchOfNumbers() {
    handleInput(CMD_START);
    handleInput(CMD_NUM | 0x00 | 1);
//    handleInput(CMD_DIMMER | 2);
    handleInput(CMD_NUM | 0x10 | 2);
//    handleInput(CMD_DIMMER | 2);
    handleInput(CMD_NUM | 0x20 | 3);
//    handleInput(CMD_DIMMER | 2);
    handleInput(CMD_NUM | 0x30 | 4);
//    handleInput(CMD_DIMMER | 2);
    handleInput(CMD_NUM | 0x40 | 5);
//    handleInput(CMD_DIMMER | 2);
    handleInput(CMD_NUM | 0x50 | 6);
//    handleInput(CMD_DIMMER | 2);
    handleInput(CMD_POINT | 0);
    handleInput(CMD_LAMP_POINT_R | 2);
    handleInput(CMD_LAMP_POINT_L | 2);
    handleInput(CMD_FIN);
}

void setup() {
    //status led init
    pinMode(STATUS, OUTPUT);
    digitalWrite(STATUS, LOW);

    //lamps init
    for (byte i = 0; i < MAX_DIGITS_USED; i++) {
        pinMode( OUT_ANODES[i], OUTPUT );
        digitalWrite( OUT_ANODES[i], LOW );
        nums[i] = 15;
    }

    //lamp points init
    for (byte i = 0; i < 2; i++) {
        pinMode(OUT_LAMP_COMMAS[i], OUTPUT );
        digitalWrite(OUT_LAMP_COMMAS[i], LOW );
    }

    //digits init
    for (byte i = 0; i < 4; i++) {
        pinMode( OUT_BCD[i], OUTPUT );
        digitalWrite( OUT_BCD[i], LOW );
    }

    //decimal points init
    for (byte i = 0; i < 2; i++) {
        pinMode( OUT_POINTS[i], OUTPUT );
        digitalWrite( OUT_POINTS[i], LOW );
    }

    //power init
    delay(100);
    pinMode(HV_ENABLE, OUTPUT);
    digitalWrite(HV_ENABLE, HIGH);
    digitalWrite(STATUS, HIGH);

    //communications init
    Wire.begin(0x4); // TODO: move the address to a const
    Wire.onReceive(receiveEvent); // register event

    Serial.begin(115200);

//    statusBlink(2);
    Serial.println("hullo");

    //debug
//    debugSetABunchOfNumbers();
//    handleInput(CMD_OFF);
}

void loop() {

    if (intro_mode) {
        setupIntroValues();
    }

    //digits & points on-off
    for (unsigned short i = 0; i < DIGITS_USED - 1; i++) {
        multiplexDigit(i);
        delayMicroseconds(AFTER_IMAGE_US); //Afterimage occurs below 300 us
    }
    //last digit treated separately due to adaptive delay afterwards
    multiplexDigit(DIGITS_USED - 1);

    unsigned long newFrameProcessStart = micros();
    if (new_frame) {
        handleNewFrame();
        new_frame = false;
    }
    unsigned long newFrameProcessEnd = micros();

    //variable delay to compensate for unpredictable duration of the above section (happens only sometimes)
    unsigned short lastDelay = AFTER_IMAGE_US - (newFrameProcessEnd - newFrameProcessStart);
    if (lastDelay > AFTER_IMAGE_US) { //a way of checking if < 0 (it's unsigned!) - both for overflow and for too long duration above
        lastDelay = 1;
    }
    delayMicroseconds(lastDelay);

    //debug
    current_frame_no++;
}