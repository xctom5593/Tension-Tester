// Thomas Forsythe 
// 6/22/20
// Using an Arduino Mega, Ramps FD with two DVR8825, LCD, Keypad, loadcell with amplifier, two 100:1 stepper motors, and a 24 V PSU, a tension/compression tester is realized with the following code.
/////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////Stepper Motor Inialization////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
const int E0_STEP_PIN = 36; //left stepper
const int E0_DIR_PIN = 28;
const int E0_ENABLE_PIN = 42;

const int E1_STEP_PIN = 43; //right stepper
const int E1_DIR_PIN = 41;
const int E1_ENABLE_PIN = 39;

boolean leftstate = 1; // used in jogging machine menu to enable/disable individual stepper motors incase motors become out of sync.
boolean rightstate = 1;

float steps = 0; // incremented during F1 Test.
float distance = 0; // units in mm. distance = steps*(6.0/20000.0) (lead screw move 6mm per revolution & 20,000 steps per revolution)
boolean stepstate = 0; // used during "TIMER5_COMPA_vect" ISR
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////LCD Initialization//////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,20,4); // (I2C address, LCD char width, LCD char height)
/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////Keypad Initialization//////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
#include <Keypad.h>
const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
char hexaKeys[ROWS][COLS] = //define the cymbols on the buttons of the keypads, must be 1 char.
{
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'G','F','E','D'}
};
byte rowPins[ROWS] = {23, 25, 27, 29}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {31, 33, 35, 37}; //connect to the column pinouts of the keypad
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); //initialize an instance of class NewKeypad
char selection; //used to store char obtained from keypad
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////Loadcell Amplifier Inialization/////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
#include "HX711.h"
const int LOADCELL_DOUT_PIN = 64;
const int LOADCELL_SCK_PIN = 65;
HX711 scale;

const long LOADCELL_DIVIDER = 8761; // used to obtained units of kg from HX711 loadcell amplifier. obtained by applying the known weight from my body (using bathroom scale) on the loadcell (tareval = adcval - OFFSET, kgval = tareval/LOADCELL_DIVIDER) 
long OFFSET; //used during iniatilization and and F3 mode to elimate any offset present on loadcell system. 
float adcval;
float tareval;
float kgval;
int avgcount = 1; // used to keep place of count during F3 mode.
int avgval = 10; // since the loadcell updates at 10 Hz, update the lcd with adval, tareval, and kgval values every 10 counts or LCD screen is hard to read. Change this value to 80 if HX711 is used in 80 Hz mode.
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////Program Begin///////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
void setup()
{
  //initialize stepper pins
  pinMode(E0_STEP_PIN,OUTPUT); 
  pinMode(E0_DIR_PIN,OUTPUT);
  pinMode(E0_ENABLE_PIN,OUTPUT);
  pinMode(E1_STEP_PIN,OUTPUT);
  pinMode(E1_DIR_PIN,OUTPUT);
  pinMode(E1_ENABLE_PIN,OUTPUT);
  digitalWrite(E0_STEP_PIN,LOW);
  digitalWrite(E0_DIR_PIN,LOW); //Low = clockwise = together/compression
  digitalWrite(E0_ENABLE_PIN,HIGH); //Enable = active low
  digitalWrite(E1_STEP_PIN,LOW);
  digitalWrite(E1_DIR_PIN,LOW);
  digitalWrite(E1_ENABLE_PIN,HIGH);

  // initialize LCD screen
  lcd.init();
  lcd.backlight();
  lcd.print("   Initializing");

  // initialize HX711 load cell amplifier.
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(LOADCELL_DIVIDER);
  OFFSET = scale.read_average(10); //take 10 readings to use as OFFSET
  scale.set_offset(OFFSET); 

}

void loop() 
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("F1: Run Test");
  lcd.setCursor(0,1);
  lcd.print("F2: Jog Machine");
  lcd.setCursor(0,2);
  lcd.print("F3: Read Scale Value");
  
  selection = waitforkey(); //see Functions section
  switch (selection)
  {
    case 'A': // F1: Run Test section
      lcd.clear();
      lcd.print("Press start to begin");
      lcd.setCursor(0,3);
      lcd.print("F1: Exit");
      do
      {
        selection = waitforkey();
        if(selection == 'G') // G = start button
        {
          steps = 0;
          kgval = 0;
          stepstate = 0;
          
          digitalWrite(E0_ENABLE_PIN,LOW); //Enable = active low
          digitalWrite(E1_ENABLE_PIN,LOW); 
          digitalWrite(E0_DIR_PIN, HIGH); //HIGH = anti-clockwise = apart/tension
          digitalWrite(E1_DIR_PIN, HIGH);

          //the following 7 lines of code drive the stepper motors by setting up an interrupt service routine that occurs every 500 us. See Interrupt Vectors section.
          cli();//stop all interrupts
          TCCR5A = 0x00;  
          TCCR5B = 0x09; //Clear Timer 5 on compare match with OCR5A, Inialize Timer 5 with prescaler of 1.
          TCNT5 = 0x0000; //Initialize Timer 5 with value of 0;
          OCR5A = 0x1F3F; //Output compare match 5A value. Equal to 500 us when using prescaler of 1.
          TIMSK5 = 0x02; //enable interrupt for Output compare match A.
          sei();//allow all interrupts
          
          
          Serial.begin(115200);
          Serial.println("mm,kg");
          
          lcd.clear();
          lcd.print("Use Serial Monitor");
          lcd.setCursor(0,3);
          lcd.print("Press stop to halt");
          do
          {
            selection = customKeypad.getKey();  // waitforkey(); not used here since that is a blocking routine (processor will do nothing until key is pressed)
            if(scale.is_ready()) // checks if the HX711 loadcell amplifier has a value ready to be read
            {
              kgval = scale.get_units(); // gets value in kg from HX711.
              distance = steps*(6.0/20000.0); // units in mm. distance = steps*(6.0/20000.0) (lead screw move 6mm per revolution & 20,000 steps per revolution)
              Serial.print(distance,3); // the ",3" prints out distance to 3 decimal places.
              Serial.print(",");
              Serial.println(kgval,3);
            }
          }
          while(selection != 'E'); // E = stop button, test will continue indefinately until stop button is pressed

          //the following 8 lines disable the stepper motors, serial stream, and interrupt routine that drives the stepper motors.
          digitalWrite(E0_ENABLE_PIN,HIGH); 
          digitalWrite(E1_ENABLE_PIN,HIGH);
          
          Serial.end();

          cli();//stop all interrupts
          TCCR5A = 0x00;  
          TCCR5B = 0x08; //Stop Timer 5
          TIMSK5 = 0x00; //disable interrupt for Output compare match A.
          sei();//allow all interrupts
          
          lcd.clear();
          lcd.print("Press start to begin");
          lcd.setCursor(0,3);
          lcd.print("F1: Exit");
        }
      }
      while(selection != 'A'); // A = F1 which pressing will return user to main menu.
      break;
      
    case 'B': // F2: Jog Machine section. Known bug where sometimes motor(s) continue to move up after '2' is no longer pressed. Testing with another keypad still caused bug to appear.
      lcd.print("2: Move Apart");
      lcd.setCursor(0,1);
      lcd.print("8: Move Together");
      lcd.setCursor(0,2);
      lcd.print("Left:E Right:E");
      lcd.setCursor(0,3);
      lcd.print("F1: Exit");
      do
      {
       selection = waitforkey();
       if(selection == '2') fastmove(1,leftstate,rightstate); // move up
       if(selection == '8') fastmove(0,leftstate,rightstate); // move down
       if(selection == '4') //  '4' and '6' are used to disable Individual steppers incase steppers become out of sync with each other.
       {
        leftstate = !leftstate;
        if(leftstate)
        {
          lcd.setCursor(5,2);
          lcd.print("E");
        }
        else
        {
          lcd.setCursor(5,2);
          lcd.print("D");
        }
       }
       if(selection == '6')
       {
        rightstate = !rightstate;
        if(rightstate)
        {
          lcd.setCursor(13,2);
          lcd.print("E");
        }
        else
        {
          lcd.setCursor(13,2);
          lcd.print("D");
        }
       }
      }
      while(selection != 'A'); // A = F1 which pressing will return user to main menu.
      break;
      
    case 'C': // F3: Read Scale Value section. Used to read HX711 raw, tared, and kg values. Also taring can be performed here
      avgcount = 1; 
      adcval = 0;
      lcd.clear();
      lcd.print("RAW:");
      lcd.setCursor(0,1);
      lcd.print("Tared:");
      lcd.setCursor(0,2);
      lcd.print("Kg:");
      lcd.setCursor(0,3);
      lcd.print("F1: Exit   F2: Tare");
      do
      {
        selection = customKeypad.getKey();
        if(scale.is_ready()) // checks if the HX711 loadcell amplifier has a value ready to be read
        {
          adcval += scale.read();
          avgcount++;
        }
        if (avgcount >= avgval) 
        {
          adcval = adcval/avgcount; //the reason math is used to obtained the tare value and kg value instead of "scale.get_value" "scale.get_units()" is that calling them will result in another reading which may differ.
          lcd.setCursor(4,0);
          lcd.print("           ");
          lcd.setCursor(4,0);
          lcd.print(adcval,0);
          
          tareval = adcval - OFFSET;
          lcd.setCursor(6,1);
          lcd.print("         ");
          lcd.setCursor(6,1);
          lcd.print(tareval,0);
          
          kgval = tareval/LOADCELL_DIVIDER;
          lcd.setCursor(3,2);
          lcd.print("            ");
          lcd.setCursor(3,2);
          lcd.print(kgval, 3);
          avgcount = 1;
        }
       if(selection == 'B') // B = F2 which will perform the following tare routine.
       {
        OFFSET = scale.read_average(50);
        scale.set_offset(OFFSET);
       }
      }
      while(selection != 'A'); // A = F1 which pressing will return user to main menu.
      break;
    
    default:
      lcd.clear();
      lcd.print("Invalid Selection");
      lcd.setCursor(0,1);
      lcd.print("made");
      delay(1000);
      break;
  }

}
/////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////Functions/////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
//// this is a blocking routine (processor will do nothing until key is pressed)
char waitforkey()
{
  char Key;
  do
  {
    Key = customKeypad.getKey();
  }
  while(!Key);
  return Key;
}

void fastmove(boolean dir, boolean left, boolean right)
{
  int coolpin; //since I couldn't get the Keypad library to report if the keypad key was continually pressed, the individual ports corresponding to the multiplexed key are read to determine when button was pressed.
  int i = 300; //used to ramp up speed to stepper motor
  if(dir == 1) coolpin = 23; //since keypad key '2' and '8' are on the same column (pin 33), only the row pin needs changed depending on which key is pressed (dir = 1 = UP). 
  else coolpin = 27;
  
  digitalWrite(coolpin, HIGH); 
  digitalWrite(E0_ENABLE_PIN,!right); // ! is needed since enable is active LOW
  digitalWrite(E1_ENABLE_PIN,!left);
  digitalWrite(E0_DIR_PIN, dir);
  digitalWrite(E1_DIR_PIN, dir);
  
  while(digitalRead(33) && i > 0) //this ramps up the speed to stepper motors so they don't stall. end speed = 1/(2 * 200 us) = 2,500 steps/sec.
  {
    digitalWrite(E0_STEP_PIN,LOW);
    digitalWrite(E1_STEP_PIN,LOW);
    delayMicroseconds(200 + i);
    digitalWrite(E0_STEP_PIN,HIGH);
    digitalWrite(E1_STEP_PIN,HIGH);
    delayMicroseconds(200 + i);
    i--;       
  }
  while(digitalRead(33))
  {
    digitalWrite(E0_STEP_PIN,LOW);
    digitalWrite(E1_STEP_PIN,LOW);
    delayMicroseconds(200);
    digitalWrite(E0_STEP_PIN,HIGH);
    digitalWrite(E1_STEP_PIN,HIGH);
    delayMicroseconds(200);
  }
  digitalWrite(coolpin, LOW);
  digitalWrite(E0_ENABLE_PIN,HIGH);
  digitalWrite(E1_ENABLE_PIN,HIGH);
}
/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////Interrupt Vectors//////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
ISR(TIMER5_COMPA_vect) //interrupt service routine that gets called when Timer 0 matches Output Compare A. (occurs every 500 us when enabled). used to drive stepper motors so they move without interruption.
{
  if(stepstate == 0)
  {
    stepstate = 1;
    digitalWrite(E0_STEP_PIN,HIGH);
    digitalWrite(E1_STEP_PIN,HIGH);
    steps++;
  }
  else
  {
    stepstate = 0;
    digitalWrite(E0_STEP_PIN,LOW);
    digitalWrite(E1_STEP_PIN,LOW);
  }

}
