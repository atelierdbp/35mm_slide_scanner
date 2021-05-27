/*********************
Code for the 35mm slide scanner.  This uses the AdaFruit 2x16 LCD with built in 5 buttons on the board using the lcd.readButtons() routine in the MCP23017 file which simplifies construction. 
The LCD is found at https://www.adafruit.com/product/714#description

 The scanner will use these buttons to start different protocals which are: 
        "Home" which tests the home location of the two motors,
        "Load" which loads slides until the elevator is maxed out, 
        "Manual" scanning one slide at a time and 
        "Automatic" which scans all the slides until the elevator is empty 

 I am using microswitches to home the axes and one to detect the correct location of the slide.  However, these exibit hysteresis.  So, I back off the slide about 1 slide width before elevating
 the slide into the grab position.        

The code drives the TMC2088 stepper drivers which has microstepping using this table.   MS1 and MS2 are normally high. 
There won't be code for these but rather set on the motor control board to free up GPIO lines from the Arduino
          
          TMC2088 Stepper Motor microstepping code
                            MS2=HIGH  MS2=LOW
             MS1=HIGH        1/16      1/2
             MS1=LOW         1/4       1/8

This code is written for the AdaFruit 2x16 LCD with I2C interface. This has 5 buttons built in which makes it easy to control the apparatus. The buttons are arranged as follows on the LCD as shown here.

                                                        xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
                                                        x                                                             x
                                                        x                       LCD Area                              x
                                                        x                                                             x
                                                        x                                                             x
                                                        x               UP                           SELECT           x
                                                        x      LEFT              RIGHT                                x 
                                                        x              DOWN                                           x
                                                        xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx  
                                                       
April 27: Added welcome screen. 
May 12; Added Pause in Manual to focus camera. This means that to finish the manual scan I need to push manual twice.  But it allows me to stop the shuttle over teh light box to adjust the focus, white balance and magnification. 
March 15/21: Changed some of hte prompts to make them more logical.  I am using the word "ARM" to indicate the hopper microswitch.  
April 19/21: Dropped the LED wakeup and output sections
April 29/21: Changed prompt after hopper is empty

This program is free software. It can be redistributed and/or modified under the terms of the GNU General Public License as published by the Free Software Foundation. 
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License for more details. You can read a copy of the GNU General Public License at http://www.gnu.org/licenses/.

**********************/

// include the library code:
#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
#define Blue 0x1                        // This set the backlight color to blue

byte elevator_home_pin=3;           
byte shuttle_home_pin=4;
byte slide_top_pin=5;

byte led_pin=6;

byte elevator_step_pin=7;
byte elevator_dir_pin=8;

byte shuttle_step_pin=9;
byte shuttle_dir_pin=10;

byte photo_pin=11;

byte elevator_state;                  //a parameter which is set LOW when the elevator home switch is triggered with PULL_UP resistor in place on pin 3
byte shuttle_state;                   //a parameter which is set LOW when the shuttle home switch is triggered with PULL_UP resistor in place on pin 4

byte elevator_dir;                    //a parameter to define the elevator motion direction
byte shuttle_dir;                     //a parameter to define the shuttle motion direction

int elevator_count=0;                 //a running tally of the elevator location during the scan
int elevator_max=50000;               //the maximum distance the elevator can drop after homing
int elevator_hysteresis=4000;         //the amount of hysteresis for the slide microswitch
int load_value=8000;                  //the amount to drop the elevator for loading with each button push
int shuttle_count;                    //a running tally for the location of the shuttle during the scan
int shuttle_max=1000;                 //the maximum distance the shuttle can move after homing
int shuttle_photo=2425;               //the distance the shuttle moves from the grab -->photo positions (a +ve number) 
int open_hopper_move=2675;            // the distance the shutte moves from the grab to the open hopper position.  
int shuttle_dump=2200;                //the distance from the photo position to the dumb positoin (a +ve number)
int motor_delay=100;                  //the duration of the step pulse to the motor controller in microseconds
int LED_value=250;                    // the brightness of the light source for the photo background
int i;                                // a dummy parameter  
byte load_state;                      // a parameter to keep track of where the shuttle is during loading
int slide_number;                     // keeps track of number of slides during automatic or manual 

void setup() {
                 Serial.begin(9600);
                 lcd.begin(16, 2);                          // set up the LCD's number of columns and rows: 
                 lcd.setBacklight(Blue);
                 lcd.clear();
                 lcd.setCursor(0,0); lcd.print("   Welcome to");
                 lcd.setCursor(0,1); lcd.print(" SLIDE SCANNER");
                 delay(1000);
                 lcd.clear();
                 lcd.setCursor(0,0); lcd.print("  Home SCANNER  ");
                 lcd.setCursor(0,1); lcd.print("    Press H     ");
                 delay(1000);
                 
                
                  pinMode(elevator_home_pin,INPUT_PULLUP);
                  pinMode(shuttle_home_pin,INPUT_PULLUP);
                  pinMode(slide_top_pin,INPUT_PULLUP);
                  pinMode(elevator_step_pin,OUTPUT);
                  pinMode(elevator_dir_pin,OUTPUT);
                  pinMode(shuttle_step_pin,OUTPUT);
                  pinMode(shuttle_dir_pin,OUTPUT);
                  pinMode(photo_pin,OUTPUT);
                 
                 
                  
    
    
                 //homing();         //uncommenting this command will cause the unit to home on power-up.
   }


void homing()                       //routine for homing both axes
{
     
//..............................................homes the elevator     
    lcd.clear();
    elevator_dir=HIGH; 
    elevator_state= digitalRead(elevator_home_pin);
            lcd.setCursor(0,0);
            lcd.print(" Homing Elevator");
      while(elevator_state==HIGH) {
            elevator_go();
          
                      if(digitalRead(elevator_home_pin)==LOW)
                      {
                              elevator_state=LOW;
                              lcd.clear();
                              lcd.setCursor(0,0);
                              lcd.print("  Elevator SET");
                              elevator_count=0;
                       }
        }
        delay(500);
//..............................................homes the shuttle    
      shuttle_dir=HIGH; 
      shuttle_state= digitalRead(shuttle_home_pin);
      lcd.setCursor(0,0);
      lcd.print(" Homing Shuttle ");
            while(shuttle_state==HIGH) {
                 shuttle_go();
                
                     if(digitalRead(shuttle_home_pin)==LOW)
                      {
                              shuttle_state=LOW;
                              lcd.clear();
                              lcd.setCursor(0,0);
                              lcd.print("  Shuttle SET");
                              shuttle_count=0;
                      }
        }
        delay(500);
         lcd.clear();
         lcd.setCursor(0,0); lcd.print(" Homing Complete");
         lcd.setCursor(0,1); lcd.print("                ");
          delay(1500);
         lcd.clear();
         lcd.setCursor(0,0); lcd.print("To Open Hopper");
         lcd.setCursor(0,1); lcd.print("  Press R/C");
    
 }
    

 void elevator_go()                   //this pulses the elevator motor with no ramps
{
  digitalWrite(elevator_dir_pin,elevator_dir);
  digitalWrite(elevator_step_pin,HIGH);
  delayMicroseconds(50);
  digitalWrite(elevator_step_pin,LOW);
  delayMicroseconds(200);
}       

void shuttle_go()                         //this pulses the shuttle motor with no ramps.
{
  digitalWrite(shuttle_dir_pin,shuttle_dir);
  digitalWrite(shuttle_step_pin,HIGH);
  delayMicroseconds(10);
  digitalWrite(shuttle_step_pin,LOW);
  delayMicroseconds(300);
} 

  void take_photo()
{   
            lcd.clear();lcd.setCursor(0,0);lcd.print("  Taking Photo");
            delay(500);
            digitalWrite(photo_pin,HIGH);
            delay(400);
            digitalWrite(photo_pin,LOW);
            
            
  
           
}



void load()                             //this routine permits loading of the elevator hopper when pressing the "DOWN" button
{
  

  elevator_dir=LOW;
            lcd.clear();
            lcd.setCursor(0,0); lcd.print("Lowering Hopper ");
            
                   for(i=0;i<load_value;i++)
                       {
                        elevator_go();
                       }
             lcd.clear();
             lcd.setCursor(0,0); lcd.print("  More slides?  ");
             lcd.setCursor(0,1); lcd.print(" Press L again  ");
             delay(1200);
             lcd.clear();
             lcd.setCursor(0,0); lcd.print("       OR       ") ;
             delay(200);
             lcd.clear();
             lcd.setCursor(0,0); lcd.print("  If done CLOSE ") ;
             lcd.setCursor(0,1); lcd.print("  ARM press R/C ");

}

void open_hopper()
{
  //......................................... lowers hopper      

             elevator_dir=LOW; 
            {
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print(" Lower Elevator ");
                    for(i=0;i<elevator_hysteresis;i++)
                    {      elevator_go();
                           elevator_count++;
                    }
                          
                            
            }                                                            
                    delay(300);    

 //...........................................raises hopper till slide engaged
           
            elevator_dir=HIGH; 
            lcd.setCursor(0,0);
            lcd.print(" Raise Elevator ");
            {
            elevator_state= digitalRead(elevator_home_pin);
               while(elevator_state==HIGH)
                    {
                           elevator_state= digitalRead(elevator_home_pin);
                           elevator_go();
                           elevator_count--;
                    }
          
                            elevator_state=LOW;
            }                                                            
                    delay(500);     
            if(shuttle_count==0)      // moves shuttle to load position if at home position
            {
                shuttle_dir=LOW;
            lcd.setCursor(0,0);
            lcd.print(" Opening Hopper ");
                   
                              
            for(int i=0; i<open_hopper_move; i++)
                      {
                      shuttle_go();
                      shuttle_count++;
                      }
            lcd.clear();
            lcd.setCursor(0,0); lcd.print(" Drop Elevator ");
            
            delay(500);

            lcd.setCursor(0,0); lcd.print("    Open ARM    ");
            lcd.setCursor(0,1); lcd.print("  then Press L  ");
            }
                     
            else                     //closes shuttle if shuttle has previously opened the hopper   
            {
              shuttle_dir=HIGH; 
            lcd.setCursor(0,0);
            lcd.print(" Closing Hopper ");
              shuttle_state= digitalRead(shuttle_home_pin);
                      lcd.setCursor(0,0);
                      lcd.print(" Homing Shuttle ");
                              while(shuttle_state==HIGH)
                  {
                              shuttle_go();
                          
                              if(digitalRead(shuttle_home_pin)==LOW)
                                {
                                        shuttle_state=LOW;
                                        shuttle_count=0;
                                }
                  }
                  lcd.clear();
                  lcd.setCursor(0,0); lcd.print(" Slides Loaded");
                  lcd.setCursor(0,1); lcd.print(" Manual or Auto? ");
            
              }

            
 
}


 void manual_scan()
 {
 if(shuttle_count==0)      // moves shuttle to photo position if at home position
            {
 
 //.............................................. backout microswitch hysteresis for "elevator_hysteresis"  

          elevator_dir=LOW; 
            {
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print(" Lowering Hopper ");
                    for(i=0;i<elevator_hysteresis;i++)
                    {      elevator_go();
                           elevator_count++;
                    }
                          
                            
            }                                                            
                    delay(300);    

 //...........................................increments the elevator until the slide detector switch is closed which engages the slide
           
            elevator_dir=HIGH; 
            {
            elevator_state= digitalRead(elevator_home_pin);
            lcd.clear();
            lcd.setCursor(0,0); lcd.print("  Lifting Hopper ");
                    while(elevator_state==HIGH)
                    {
                           elevator_state= digitalRead(elevator_home_pin);
                           elevator_go();
                           elevator_count--;
                    }
          
                            elevator_state=LOW;
                           
                           
                     
              }                                                            
                    delay(500);     
 
 
 //......................................... move the slide to the photo position                         
            lcd.clear();
            lcd.setCursor(0,0); lcd.print(" Moving to photo");
            lcd.setCursor(0,1); lcd.print("    position");
            shuttle_dir=LOW; 
   
       for(int i=0; i<shuttle_photo; i++)
      {
            shuttle_go();
            shuttle_count++;
      }
            delay(300);

 
 //.............................................takes the photo
 
            take_photo();
            lcd.setCursor(0,1);lcd.print("Done --> M again");
            

            }
 //.............................................dumps the slide
 
            else                     //closes shuttle if shuttle has previously opened the hopper   
            {
       shuttle_dir=LOW; 
            lcd.setCursor(0,0);
            lcd.print(" Dumping Slide ");
                        for(i=0;i<shuttle_dump;i++)
                               {
                              shuttle_go();
                               }
                               delay(500);
   
 //..................................takes the shuttle to the load position  
       shuttle_dir=HIGH;                            
           lcd.clear();
           lcd.setCursor(0,0); lcd.print("  Press M again ");
           lcd.setCursor(0,1); lcd.print("   Until empty  ");
           
    
     
        while(digitalRead(shuttle_home_pin)==HIGH)
                  {
                           shuttle_go();
                  }
              delay(500);
              shuttle_count=0;
            }
                 
            
  }

 void auto_scan()
 {
  
     
 //.............................................. backout microswitch hysteresis for "elevator_hysteresis"  

          elevator_dir=LOW; 
            {
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print(" Lowering Hopper ");
                    for(i=0;i<elevator_hysteresis;i++)
                    {      elevator_go();
                           elevator_count++;
                    }
                          
                            
            }                                                            
                    delay(200);    

 //...........................................increments the elevator until the slide detector switch is closed which engages the slide
           
            elevator_dir=HIGH; 
            {
            elevator_state= digitalRead(elevator_home_pin);
            lcd.clear();
            lcd.setCursor(0,0); lcd.print("  Lifting Hopper ");
                    while(elevator_state==HIGH)
                    {
                           elevator_state= digitalRead(elevator_home_pin);
                           elevator_go();
                           elevator_count--;
                    }
          
                            elevator_state=LOW;
                           
                           
                     
              }                                                            
                    delay(200);     
 
 
 //......................................... move the slide to the photo position                         
            lcd.clear();
            lcd.setCursor(0,0); lcd.print(" Moving to photo");
            lcd.setCursor(0,1); lcd.print("    position");
             shuttle_dir=LOW; 
   
       for(int i=0; i<shuttle_photo; i++)
      {
            shuttle_go();
      }
            delay(300);

 
 //.............................................takes the photo
 
            take_photo();
            delay(500);
 
 //.............................................dumps the slide
 
            lcd.clear();lcd.setCursor(0,0);  lcd.print("  Dumping Slide");
           
            shuttle_dir=LOW; 

       for(int i=0; i<shuttle_dump; i++)
      {
            shuttle_go();
      }
            delay(200);                                   
 
 
 //..................................takes the shuttle to the load position  
       shuttle_dir=HIGH;                            
           lcd.clear();
           lcd.setCursor(0,0); lcd.print("  moving to get");
           lcd.setCursor(0,1); lcd.print("   next slide");
           
    
      shuttle_state= digitalRead(shuttle_home_pin);
        while(shuttle_state==HIGH)
        {
                 shuttle_go();
                
                     if(digitalRead(shuttle_home_pin)==LOW)
                      {
                              shuttle_state=LOW;
                      }
        }
              delay(100);
  }


void detect_buttons()
{
     uint8_t buttons = lcd.readButtons();   
        if (buttons)
        {
                if (buttons & BUTTON_UP)
                {
                   homing();                    // homes both motors against limit switches when "UP" button is pressed
                }
                if (buttons & BUTTON_DOWN)
                {
                            load();             //drops the elevator when the "DOWN" button is pressed
                }

                if (buttons & BUTTON_LEFT)
                {   
                  manual_scan(); 
                }
                
                if (buttons & BUTTON_RIGHT)     //does an "automatic" scan of a bunch of slides to a maximum of 100 slides
                {      
                    while(digitalRead(slide_top_pin)==HIGH)
                           {
                            auto_scan();
                            delay(200);
                            }
                            lcd.clear();
                            lcd.setCursor(0,0); lcd.print("Hopper is Empty");
                            lcd.setCursor(0,1); lcd.print("   Press RC");
                          
                }
                 if (buttons & BUTTON_SELECT)
                 {     
                  open_hopper();
                 }
        }
          
}



void loop()
      {
            detect_buttons();
            
       }
