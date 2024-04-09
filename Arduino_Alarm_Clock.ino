/*
AUTHOR: Will May
PROJECT: Alarm Clock with 555 Timer IC
Components: Custom PCB controlled by an Arduino Micro

The alarm clock uses 12 LED lights for the hours and two
7-segment displays for the minutes. The clock is controlled
by an Arduino Micro and the time is kept by an interrupt
that is triggered every second. The clock can be set by two
buttons that increase the hours and minutes. The alarm can
be set by another button and will trigger a tone when the
alarm time is reached. The clock also plays the Westminster
Chimes every 15 minutes. The clock can be toggled between
AM and PM by the 12 hour cycle.
 */

#include "pitches.h"

const bool debug = false;

byte minHighNibble;
byte minLowNibble; 

int hours = 1;
int minutes = 0;
int seconds = 0;
int alarm_hours = 0;
int alarm_minutes = 0;

bool pm_time = false;
bool alarm_pm = false;
bool alarm_on = false;
bool west_min_played = false;
volatile int timeKeeper;

const int PM_LED = 6;
const int speaker = 7;
const int button_minutes = 3;
const int button_hours = 4;
const int alarm_button = 5;

//Westminster Chimes by sections
const int west_min_tones[][4] = {{NOTE_GS4, NOTE_FS4, NOTE_E4, NOTE_B3},     //Part 1
                                   {NOTE_E4, NOTE_GS4, NOTE_FS4, NOTE_B3},   //Part 2
                                   {NOTE_E4, NOTE_FS4, NOTE_GS4, NOTE_E4},   //Part 3
                                   {NOTE_GS4, NOTE_E4, NOTE_FS4, NOTE_B3},   //Part 4
                                   {NOTE_B3, NOTE_FS4, NOTE_GS4, NOTE_E4}};  //Part 5


void setup() {
  Serial.begin(9600);
  pinMode(button_minutes, INPUT_PULLUP);    //INPUT Pullup reverses the pin logic i.e. HIGH = not pressed
  pinMode(button_hours, INPUT_PULLUP);
  pinMode(alarm_button, INPUT_PULLUP);
  pinMode(PM_LED, OUTPUT);

  attachInterrupt (0, Sixty_Hz, RISING);    // Use pin 2, rising edge, jump to ISR called "Sixty_Hz" when triggered
  DDRB = B11111111;                         // sets Arduino PortB pins 0 to 7 as outputs
  DDRC = B11111111;                         // sets Arduino PortC pins 0 to 7 as outputs
  PORTC = hours-1;                          // Display the hours by outputting the binary value to Port C
}

void loop() {
  if (!digitalRead(alarm_button)){
    set_alarm();
  }
  delay(200);
  seconds++;
  rollover();
  displayMinutes(minutes);

  // For debugging purposes
  if (debug){
    //Visually verfiy the time on the serial monitor
    Serial.println((String)hours +":"+(String)minutes +":"+(String)seconds);
  }
  
  check_quarter_min();
  set_time();
  check_alarm();
}


//********* Function Declarations ******************

void rollover(){
   if (seconds > 59){     
    // seconds overflow, reset seconds and increment minutes
    seconds = 0;         
    minutes++;           
    if (minutes > 59){    
    // minutes overflow, reset minutes and increment hours
      minutes = 0;      
      hours++;           
     
     if (hours == 12){
      change_to_pm(pm_time);
     } 
     if (hours > 12){      
        hours = 1;
     }
     // Adjust, since hour 1 is bit 000
     PORTC = hours-1;   
    }
  }
}

void alarm_rollover(){
  if (alarm_minutes > 59){
  alarm_minutes = 0;
  }
  if (alarm_hours > 12){
    alarm_hours = 1;
  }
}

void set_time(){
  if (!digitalRead(button_minutes)){
    //Button 1 is pressed: increase the minutes
    delay(20);
    minutes++;
    if (minutes > 59){
      minutes = 0;
    }
  }
  
  if (!digitalRead(button_hours)){
    // Button 2 is pressed: increase the hours
    delay(20);
    hours++;
    if (hours == 12){change_to_pm(pm_time);}
    if (hours > 12){
      hours = 1;
    }
    PORTC = hours-1;
  }
}

void set_alarm(){
  delay(150);

  while (!alarm_on){
    displayMinutes(alarm_minutes);
  
    if (!digitalRead(button_minutes)){
      // Button 1 is pressed 
      delay(250);
      alarm_minutes++;
      alarm_rollover();
      Serial.println("ALARM: "+(String)alarm_hours +":"+(String)alarm_minutes);
    }
    
    if (!digitalRead(button_hours)){
      // Button 2 is pressed
      delay(250);
      alarm_hours++;
      alarm_rollover();
      PORTC = alarm_hours-1;
      Serial.println("ALARM: "+(String)alarm_hours +":"+(String)alarm_minutes);
    }
  
    if (!digitalRead(alarm_button)){
      // Alarm button is pressed again, the alarm is set for that prefixed time
      delay(250);
      alarm_on = true;

      if (debug){
        // Visually verify the alarm time on the serial monitor
        Serial.println("Alarm set to: "+(String)alarm_hours +":"+(String)alarm_minutes);
      }    
    }
  }
}

void check_alarm(){
  if ((hours == alarm_hours && minutes == alarm_minutes) && alarm_on){
     for (int i = 0; i < 5; i++){
      tone(speaker, 700);
      delay(250);
      noTone(speaker);
     }
     alarm_on = false;
  }
}

void play_tone(int t, int for_this_long){
    tone(speaker, t);
    delay(for_this_long);
    noTone(speaker);
}

// Plays a section/ or full westminister tones based on the minutes
void play_westmin_tones(int col){
  for (int i = 0; i < 3; i++){
    play_tone(west_min_tones[col][i], 400);
  }
  play_tone(west_min_tones[col][3], 800);  //Hold that last note for a second
}

// Check fot the quarterly signals that trigger the westminister tones
void check_quarter_min(){
    if (minutes == 15 && !west_min_played){
       play_westmin_tones(0);
       west_min_played = true;
    }
    else if (minutes == 30  && !west_min_played){
      play_westmin_tones(1);
      play_westmin_tones(2);
      west_min_played = true;
    }
    else if (minutes == 45  && !west_min_played){
      play_westmin_tones(3);
      play_westmin_tones(4);
      play_westmin_tones(0);
      west_min_played = true;
    }
    else if (minutes == 0  && !west_min_played){
      play_westmin_tones(1);
      play_westmin_tones(2);
      play_westmin_tones(3);
      play_westmin_tones(4);
      west_min_played = true;
    }
    if (minutes % 15 != 0){
      west_min_played = false;
    }
}

// Toggle the AM/PM led based on the the current hourly cycle
void change_to_pm(bool& pm){
  if (pm){
    digitalWrite(PM_LED, HIGH);
    pm_time = false;
  }
  else {
    digitalWrite(PM_LED, LOW);
    pm_time = true;
  }
}

// This is the interrupt (ISR) that is triggered by a low to high transition on pin 2
void Sixty_Hz(){ 
 timeKeeper++;
 if (timeKeeper >= 60){  
   // 1 second has passed
   timeKeeper = 0;
   seconds++;  
 }
}

// Display the minutes on the two digital number display
void displayMinutes(int& minute)
{                              
  minHighNibble = minute/10;

  //Convert data from binary to BCD
  minLowNibble = minute%10;    
  PORTB = minLowNibble;        
  
  // Pulse the 1st digit data load input
  PORTB = PORTB | B00010000;   
  PORTB = PORTB & B11101111;
  PORTB = minHighNibble; 
  
  // Pulse the 2nd digit data load input
  PORTB = PORTB | B00100000;   
  PORTB = PORTB & B11011111; 
}
