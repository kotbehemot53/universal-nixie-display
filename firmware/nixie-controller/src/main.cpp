#include <Arduino.h>
#include <Wire.h>

//TODO: add command to run intro
//TODO: include code execution in the waits (is it worth it?)
//DONE: measure elapsed frame time and subtract from bright/dim waits (is it worth it?)
//TODO: instead of "manual" gradual dimming, include ready routines for it and commands to trigger them (is it really needed?)
//TODO: think about improving points dimming (is it possible?)
//TODO: think about pull-downs on anodes & maybe shorter AFTER_IMAGE_US thanks to that?
//TODO: think about testing PWM dimming (each anode on a PWM output) using PWM registers with no clock divider (at max freq) (is it worth it?)

//TODO: support HV circuit switching (command)
//TODO: support commas
//DONE: 2-6 digits support

//you can change this to adjust the code to the physical display length (values 2-6 are allowed)
const short DIGITS_USED = 6;

//don't change this:
const short MAX_DIGITS_USED = 6;

//number commands - last 4 bits set the number, anything above 9 is no light at all on the lamp
//REMEMBER - all CMDs with most significant bit set (>0x80) are treated as a number command - so don't use those for other purposes!
//now they are defined procedurally
//const byte CMD_NUM1 = 0x80;
//const byte CMD_NUM2 = 0x90;
//const byte CMD_NUM3 = 0xA0;
//const byte CMD_NUM4 = 0xB0;
//const byte CMD_NUM5 = 0xC0;
//const byte CMD_NUM6 = 0xD0;

//regular commands
const byte CMD_FIN = 0x20; //loads number and dimming values from buffers to output (renders new frame)
const byte CMD_NOOP = 0x30;
const byte CMD_START = 0x40; //currently unused - works as NOOP
const byte CMD_POINT = 0x50; //last 4 bits determine which point should be turned on (only vales 0 and 1 are allowed)
const byte CMD_DIMMER = 0x60; //last 4 bits set the duty cycle; it's assigned to the latest digit set

//responses
const byte RESP_SUCCESS = 0x00;
const byte RESP_FAIL = 0b01010101;

//pins
const byte OUT_BCD[] = {4,5,6,7}; //pins for encoding currently displayed number (in BCD)
const byte OUT_POINTS[] = {13,12}; //pins for setting decimal points (neon lamps)
//array size must equal MAX_DIGITS_USED
const byte OUT_ANODES[] = {2,3,8,9,10,11}; //pins for multiplexing lamps (they switch power to particular anodes)
//TODO: actually support the points! now they are always off
const byte LAMP_POINTS[] = {A1, A2}; //pins for decimal points in main lamps
const byte HV_ENABLE = A0;
const byte STATUS = A3;

//times
//Afterimage occurs below 300 us
const short AFTER_IMAGE_US = 300; //interframe interval to avoid afterimages
const short FRAME_US = 1000; //frame time
const float DIMMING_CURVE_POWER = 2.1;

//values
const byte LAMP_OFF = 15;

//buffers & other globals
//TODO: should these be volatile? it triggers warnings...
bool point_vals[2] = {false, false};
bool point_vals_buffer[2] = {false, false};
bool points_off[2] = {false, false};

//array sizes must equal MAX_DIGITS_USED
byte nums[MAX_DIGITS_USED] = {0, 0, 0, 0, 0, 0};
byte nums_buffer[MAX_DIGITS_USED] = {15, 15, 15, 15, 15, 15};
byte fail_nums[MAX_DIGITS_USED] = {15, 15, 15, 15, 15, 15};

//array sizes must equal MAX_DIGITS_USED
short bright_times[] = {FRAME_US,FRAME_US,FRAME_US,FRAME_US,FRAME_US,FRAME_US};
short dim_times[] = {0,0,0,0,0,0};
byte dimmer_buffer[] = {15,15,15,15,15,15};
byte dimmer_defaults[] = {15,15,15,15,15,15};

byte curr_lamp_idx = 0;

volatile bool new_frame = false;


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
    bool show_num = (nums[5 - anode_idx] >= 0) && (nums[5 - anode_idx] < 10);
    //points are multiplexed together with anode 0 and 1:
    bool show_point = (anode_idx == 0 || anode_idx == 1) && point_vals[anode_idx];

    unsigned long numProcessHighStart = micros();
    if (show_num) {
        numOut(nums[5 - anode_idx]); //important NOT to set the numOut when there's no need - at high framerates it confuses the russian and causes microblinks on the off lamp
        digitalWrite( OUT_ANODES[anode_idx], HIGH );
    }
    if (show_point) {
        digitalWrite( OUT_POINTS[anode_idx], HIGH );
    }
    unsigned long numProcessHighEnd = micros();
    unsigned int finalDelay = (bright_times[5 - anode_idx]) - (numProcessHighEnd - numProcessHighStart);
    if (finalDelay > bright_times[5 - anode_idx]) {
        finalDelay = 0;
    }
//    delayMicroseconds(bright_times[3 - anode_idx]);
    delayMicroseconds(finalDelay);
//    Serial.println(String("a: ") + String(finalDelay));


    unsigned long numProcessLowStart = micros();
    if (show_num) {
        digitalWrite( OUT_ANODES[anode_idx], LOW );
    }
    if (show_point) {
        digitalWrite( OUT_POINTS[anode_idx], LOW );
    }
    unsigned long numProcessLowEnd = micros();
    finalDelay = (dim_times[5 - anode_idx]) - (numProcessLowEnd - numProcessLowStart);
    if (finalDelay > dim_times[5 - anode_idx]) {
        finalDelay = 0;
    }
//    delayMicroseconds(dim_times[3 - anode_idx]);
    delayMicroseconds(finalDelay);
//    Serial.println(String("b: ") + String(finalDelay));
}

/**
 * Renders the intro
 */
void doIntro()
{
    //MUST be divisible by any possible DIGITS_USED!
    short frames = 600;

    short frames_2 = frames/2;
    short frames_2digs = frames / (DIGITS_USED * 2);
    short current_digit;

    for (short i = 0; i < frames; i++) {
        for (byte j = 0; j < DIGITS_USED; j++) {
            current_digit = j + MAX_DIGITS_USED - DIGITS_USED;
            if (i == j * frames_2digs) {
                nums[current_digit] = 6;
            }
            if (i == j*frames_2) {
                point_vals[1-j] = true;
            }
            float booblator = (float)(i - j * frames_2digs);
            if (booblator < 1)
                booblator = 1.0;
            else if (booblator > (float)frames_2)
                booblator = (float)frames_2;
            float multiplier = pow(booblator/(float)frames_2, 2.5);
            bright_times[current_digit] = multiplier * FRAME_US;
            dim_times[current_digit] = (1.0 - multiplier) * FRAME_US;
        }
        loop();
    }
    Serial.println("intro done");
}

/**
 * Actual command handling
 */
byte handleInput (byte in_byte)
{
    if ((in_byte & 0xF0) == CMD_START) {
        //unused now - same as NOOP
//    Serial.println("strt");
        return RESP_SUCCESS;
    }
    if (in_byte & 0x80) {  //a number was sent
//    Serial.print("num ");
//    Serial.print((in_byte & 0x70) >> 4);
//    Serial.print(":");
//    Serial.println(in_byte & 0x0F);
        curr_lamp_idx = (in_byte & 0x70) >> 4;
        if (curr_lamp_idx >= DIGITS_USED) {
            //digit above the number of digits used is not allowed
            return RESP_FAIL;
        }

        nums_buffer[curr_lamp_idx] = in_byte & 0x0F;

        return RESP_SUCCESS;
    }
    if ((in_byte & 0xF0) == CMD_FIN) {
//    Serial.println("end");
        new_frame = true;

        return RESP_SUCCESS;
    }
    if ((in_byte & 0xF0) == CMD_NOOP) {
//    Serial.print("noop: ");
//    Serial.println(in_byte);

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
    for (byte i = 0; i < MAX_DIGITS_USED; i++) {
        float multiplier = pow( (float)(dimmer_buffer[i] + 1)/16.0, DIMMING_CURVE_POWER );
        bright_times[i] =(short) ((float)FRAME_US * multiplier);
        dim_times[i] = (short) ((float)FRAME_US * ( 1 - multiplier ));
//      Serial.println(bright_times[i]);
//      Serial.println(dim_times[i]);
    }
//    Serial.println("-");
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
        pinMode( LAMP_POINTS[i], OUTPUT );
        digitalWrite( LAMP_POINTS[i], LOW );
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
    Wire.begin(0x4);
    Wire.onReceive(receiveEvent); // register event

    Serial.begin(115200);

//  for (byte i = 0; i < 4; i++) {
//    nums[i] = 6;
//  }
//  points[0] = false;
//  points[1] = false;

    //intro
    doIntro();

//    statusBlink(2);
    Serial.println("hullo");
}

void loop() {
    //digits & points on-off
    for (short i = 0; i < DIGITS_USED - 1; i++) {
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

    //shortening the final afterimage delay to take varying frame length into account (new_frame section
    //   happens only sometimes)
    delayMicroseconds(AFTER_IMAGE_US - (newFrameProcessEnd - newFrameProcessStart)); //Afterimage occurs below 300 us

//    Serial.println(String(newFrameProcessEnd - newFrameProcessStart));

//DEBUG
//    handleInput(CMD_START);
//    handleInput(CMD_NUM1 | 1);
//    handleInput(CMD_NUM2 | 2);
//    handleInput(CMD_NUM3 | 3);
//    handleInput(CMD_FIN);
}