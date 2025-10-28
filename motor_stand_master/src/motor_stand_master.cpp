#include <Arduino.h>
#include <motor_stand_master_definitions.h>

////////////////////////////////////////////////////////////////////////////////////////
//HELPER FUNCTIONS:

void lcd_home(){
  input = "";
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(parameter_names[parameter_index] + parameter_values[parameter_index]);
  lcd.setCursor(0, 2);
  lcd.print("NEXT: " +  String(ENTER_INPUT) + " | BACK: " + String(BACK_BUTTON));
  lcd.setCursor(0, 3);
  lcd.print("THROTTLE:OFF");
  lcd.setCursor(0, 1);
}

void tare_ui(){
  input = "";
  lcd.clear();
  lcd.print(tare_names[tare_index] + tare_values[tare_index]);
  lcd.setCursor(0, 2);
  lcd.print("NEXT: " +  String(ENTER_INPUT) + "| SKIP: " + String(SKIP_TARE) + "    ");
  lcd.setCursor(0, 3);
  if(tare_index == 0){
    lcd.print("UNITS: N.mm");
  }
  else if(tare_index == 1){
    lcd.print("UNITS: mN");
  }
  lcd.setCursor(0, 1);
}

void send_ui(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PRESS " + String(SEND_INPUT) + " TO TARE");
  if(tare_index == 2){
    lcd.setCursor(0, 1);
    lcd.print("ANALOG SENSORS");
  }
  if(tare_index != 2){
    lcd.setCursor(0, 3);
    lcd.print("BACK: " + String(BACK_BUTTON));
  }
  lcd.setCursor(0, 1);
}

void send_parameters(String type, String value){
  String signal = type + value;
  int length = signal.length();
  Wire.beginTransmission(9);
  Wire.write(signal.c_str(), length);
  Wire.endTransmission();
}

void testing_screen(){
  lcd.clear();
  lcd.print("RUNNING TEST");
  lcd.setCursor(0, 1);
  lcd.print("TEST #: " + parameter_values[0]);
  lcd.setCursor(0, 2);
  lcd.print("INCR. LENGTH: " + parameter_values[4] + " s");
  lcd.setCursor(0, 3);
  lcd.print("THROTTLE:" + String(map(cycle_length, ESC_MIN, ESC_MAX, 0, 100)));
  Serial.println("Starting: Test Num: " + parameter_values[0] + " | Increment: " + String(throttleIncrement));
  start_motor = true;
  Wire.beginTransmission(9);
  Wire.write('b');
  Wire.endTransmission();
  prev_interval_timestamp = millis();
}

void end_testing(){
  Wire.beginTransmission(9);
  Wire.write('e');
  Wire.endTransmission();
  setup();
}

void pause_screen(){
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("PRESS * TO RESUME");
  lcd.setCursor(0, 3);
}

void throttle_ramp_up(int start, int next_cycle_length){
  for(cycle_length = start; cycle_length <= next_cycle_length; cycle_length++){
    if(done_throttling){
      return; //return to the main loop and throttle down
    }
    esc.writeMicroseconds(cycle_length);
    int percent = map(cycle_length, ESC_MIN, ESC_MAX, 0, 100);
    if(percent < 10){
      lcd.setCursor(9, 3);
      lcd.print(String(percent) + " ");
    }
    else{
      lcd.setCursor(9, 3);
      lcd.print(String(percent));
    }
    delay(THROTTLE_UP_DELAY);
  }
  cycle_length--;
}

void throttle_down(){
  // if(!read_gradient){
  //   Wire.beginTransmission(9);
  //   Wire.write('w');
  //   Wire.endTransmission();
  // }
  // for(int i = cycle_length; i >= MIN_THROTTLE; i--){
  //   esc.writeMicroseconds(i);
  //   int throttle = map(i, ESC_MIN, ESC_MAX, 0, 100);
  //   if(throttle == 99){
  //     lcd.setCursor(11, 3);
  //     lcd.print(" ");
  //   }
  //   if(throttle == 9){
  //     lcd.setCursor(10, 3);
  //     lcd.print(" ");
  //   }
  //   lcd.setCursor(0, 3);
  //   lcd.print("THROTTLE:" + String(throttle));
  //   delay(THROTTLE_UP_DELAY/10);
  // }
  //delay(INCREMENT_TIME);
  esc.writeMicroseconds(1000);
}

void throttle_up(){
  if(cycle_length >= MAX_THROTTLE){
    if(millis() >= prev_interval_timestamp + INCREMENT_TIME){
      Serial.println("DONE THROTTLING");
      done_throttling = true;
    }
  }
  else{
    if(millis() >= prev_interval_timestamp + INCREMENT_TIME){
      if(piecewise){
        Wire.beginTransmission(9);
        Wire.write('w');
        Wire.endTransmission();

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("THROTTLING DOWN");

        throttle_down();
        pause_screen();
        paused = true;
        return;
      }
      if(!read_ramp_up_data){        
        Wire.beginTransmission(9);
        Wire.write('w');
        Wire.endTransmission();
      }

      int next_cycle_length = min(cycle_length + pwm_increment, MAX_THROTTLE);
      throttle_ramp_up(cycle_length, next_cycle_length);

      if(done_throttling){
        return; //return to the main loop and throttle down
      }

      if(!read_ramp_up_data){
        long ramp_up_delay_start = millis();
        while(millis() < ramp_up_delay_start + RAMP_UP_DELAY){
          if(done_throttling){
            return; //return to the main loop and throttle down
          }
        }
        Wire.beginTransmission(9);
        Wire.write('g');
        Wire.endTransmission();
      }
      prev_interval_timestamp = millis();
    }
  }
}

void interrupt(){
  if(start_motor){
    done_throttling = true;
  }
}

void setup_next_input(){
  if(parameter_index < PARAMETER_NUM){
    parameter_values[parameter_index] = input;
  }
  parameter_index++;
  if(parameter_index == PARAMETER_NUM){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SMOOTH DATA?");
    lcd.setCursor(0, 3);
    lcd.print("YES: A | NO: B");
  }
  else if(parameter_index == PARAMETER_NUM + 1){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PIECEWISE?");
    lcd.setCursor(0, 3);
    lcd.print("YES: A | NO: B");
  }
  else if(parameter_index == PARAMETER_NUM + 2){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PRESS " + String(SEND_INPUT) + " TO START");
  }
  else{
    lcd_home();
  }
}

void setup_prev_input(){
  parameter_index--;
  if(parameter_index == PARAMETER_NUM){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SMOOTH DATA?");
    lcd.setCursor(0, 3);
    lcd.print("YES: A | NO: B");
  }
  else if(parameter_index == PARAMETER_NUM + 1){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PIECEWISE?");
    lcd.setCursor(0, 3);
    lcd.print("YES: A | NO: B");
  }
  else{
    lcd_home();
  }
}

void send_inputs(){
  send_parameters("f", parameter_values[0]);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SETTING UP FILE...");
  while(1){
    Wire.requestFrom(9, 1);
    if(Wire.read() == 1){
      break;
    }
    delay(100);
  }

  int max_throttle_input = min(max(parameter_values[1].toInt(), 0), 100);
  MAX_THROTTLE = map(max_throttle_input, 0, 100, ESC_MIN, ESC_MAX);

  throttleIncrement = min(parameter_values[2].toInt(), max_throttle_input);
  pwm_increment = map(throttleIncrement, 0, 100, 0, ESC_MAX - ESC_MIN); //1% -> 99% written in terms of PWM cycle length, assuming a linear mapping
  
  Serial.println(parameter_values[3]);
  send_parameters("m", parameter_values[3]);

  INCREMENT_TIME = (long) parameter_values[4].toInt() * 1000;
  Serial.println("TEST PARAMETERS CONFIRMED");

  testing_screen();
}

void tare_torque(){
  send_parameters("q", tare_values[tare_index]); //tell slave to tare torque
  while(1){
    Wire.requestFrom(9, 1);
    if(Wire.read() == 1){
      break;
    }
    delay(100);
  }
  tare_index++;
  tare_ui();
  sending = false;
}

void tare_thrust(){
  send_parameters("r", tare_values[tare_index]); //tell slave to tare thrust
  while(1){
    Wire.requestFrom(9, 1);
    if(Wire.read() == 1){
      break;
    }
    delay(100);
  }
  tare_index++;
  send_ui();
}

void tare_analog(){
  Wire.beginTransmission(9);
  Wire.write('a');
  Wire.endTransmission();
  while(1){
    Wire.requestFrom(9, 1);
    if(Wire.read() == 1){
      break;
    }
    delay(100);
  }
  lcd_home();
  tared = true;
  sending = false;
}

void not_sending_tare_values(char key){
  if(key >= '0' && key <= '9' && input.length() < 9){
    input += key;
    lcd.print(key);
  }  
  else if(key == SKIP_TARE){ //check for tare skipping keystroke
    Serial.println("SKIP");
    Serial.println(tare_index); //0 - torque, 1 - thrust, 2 - analog, END
    tare_index++;
    Serial.println(tare_index);
    if(tare_index == 2){ //go to the send_ui because taring analog sensors goes straight to sending instead of tare_ui
      send_ui();
      sending = true;
    }
    else{
      Serial.println("going into tare ui");
      tare_ui();
      sending = false;
    }
  } 
  else if(key == ENTER_INPUT && input != "" && tare_index < TARE_NUM){
    tare_values[tare_index] = input;
    input = "";
    send_ui();
    sending = true;
  }
}

void send_tare_values(char key){
  Serial.println("sending");
  if(key == BACK_BUTTON && tare_index != 2){
    tare_ui();
    sending = false;
  }
  else if(tare_index == 2 && key == SKIP_TARE){ //for skipping during the analog part (since it goes straight to sending)
      lcd_home();
      tared = true;
      sending = false;
  }
  else if(key == SEND_INPUT){ //the button to zero the values
    lcd.setCursor(0, 1);
    lcd.print("CALIBRATING...");
    if(tare_index == 0){
      tare_torque();
    }
    else if(tare_index == 1){
      tare_thrust();
    }
    else if(tare_index == 2){ //tell slave to tare analog sensors
      tare_analog();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////
//MAIN DRIVER CODE:

void setup() {
  pinMode(INTERRUPT_PIN, INPUT_PULLUP); //set default switch position to HIGH
  pinMode(STATUS_LED_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), interrupt, FALLING); //when switch is pressed down

  for(int i = 0; i < PARAMETER_NUM; i++){
    parameter_values[i] = "";
  }
  input = "";
  parameter_index = 0;

  done_throttling = false;
  throttling_up = false;
  start_motor = false;
  cycle_length = MIN_THROTTLE;
  
  // Set up the LCD display
  lcd.init();
  lcd.backlight();
  //Initialize Serial 
  Serial.begin(9600);
  Serial.println("Setting up");
  lcd.print("Loading .");

  //Initialize I2C protocol (master)
  Wire.begin();
  lcd.setCursor(0, 0);
  lcd.print("Loading ..");
  
  //Initialize servo PWM and arm the ESC
  esc.attach(ESC_PIN); //set esc to pin
  esc.writeMicroseconds(MIN_THROTTLE); //minimum throttle; arm the esc

  lcd.setCursor(0, 0);
  lcd.print("Loading ...");

  while(1){
    Wire.requestFrom(9, 1);
    delay(100);
    if(Wire.read() == 1){
      break;
    }
    delay(100);
  }
  lcd.setCursor(0, 0);
  lcd.print("Loading .....");

  lcd.setCursor(0, 0);
  lcd.print("USE PREVIOUS TARE?");
  lcd.setCursor(0, 3);
  lcd.print("YES: A | NO: B");
  choosing = true;
  tared = false;

  Serial.println("READY");
}

void loop() {
  char key = keypad.getKey();
  digitalWrite(STATUS_LED_PIN, HIGH);

  if(!tared){
    if(key){
      if(!choosing){
        if(!sending){ //the user is inputting in calibration values but NOT sending it yet
          not_sending_tare_values(key);
        }
        else{ //the user is going to send the tare values to the other arduino to calibrate
          send_tare_values(key);
        }
      }
      else{
        if(key == 'A'){ //skip the whole tare part
          Wire.beginTransmission(9);
          Wire.write('p');
          Wire.endTransmission();
          delay(100);
          choosing = false;
          tared = true;
          parameter_index = 0;
          lcd_home();
        }
        else if(key == 'B'){ //do the taring process
          tare_index = 0;
          sending = false;
          choosing = false;
          tare_ui();
        }
      }
    }
  }
  else{ //can only do everything else once sensors have been tared
    if(start_motor && !paused){
      throttle_up();
    }

    if(done_throttling){
      Serial.println("THROTTLING DOWN");
      throttle_down();
      end_testing();
    }

    // Serial.println(cycle_length);
    // Serial.print(parameter_values[4].toInt() * 1000);
    // Serial.print(" | ");
    // Serial.println(INCREMENT_TIME);
    //if a keystroke has been entered from the keypad
    if(key){
      if(paused){
        if(key == SEND_INPUT){
          //unpause data recording before the throttle ramp up if yes gradeint reading
          if(read_ramp_up_data){
            Wire.beginTransmission(9);
            Wire.write('g');
            Wire.endTransmission();
          }
          testing_screen();
          int next_cycle_length = min(cycle_length + pwm_increment, MAX_THROTTLE);
          throttle_ramp_up(MIN_THROTTLE, next_cycle_length);
          if(done_throttling){
            Serial.println("THROTTLING DOWN");
            throttle_down();
            end_testing();
          }

          //unpause data recording after the throttle ramp up if no gradient reading
          if(!read_ramp_up_data){
            long ramp_up_delay_start = millis();
            while(millis() < ramp_up_delay_start + RAMP_UP_DELAY){
              if(done_throttling){
                Serial.println("THROTTLING DOWN");
                throttle_down();
                end_testing();
              }
            }
            Wire.beginTransmission(9);
            Wire.write('g');
            Wire.endTransmission();
          }

          prev_interval_timestamp = millis();
          paused = false;
        }
      }
      else{
        if(key == BACK_BUTTON && parameter_index > 0){
          setup_prev_input();
        }
        else if(key == ENTER_INPUT && input != "" && parameter_index < PARAMETER_NUM){
          setup_next_input();
        }
        else if(parameter_index == PARAMETER_NUM){
          if(key == 'A'){
            read_ramp_up_data = true;
            setup_next_input();
          }
          else if(key == 'B'){
            read_ramp_up_data = false;
            setup_next_input();
          }
        }
        else if(parameter_index == PARAMETER_NUM + 1){
          if(key == 'A'){
            piecewise = true;
            setup_next_input();
          }
          else if(key == 'B'){
            piecewise = false;
            setup_next_input();
          }
        }
        else if(key == SEND_INPUT && parameter_index == PARAMETER_NUM + 2){
          send_inputs();
        }
        else if(key >= '0' && key <= '9'){
          if(input.length() < 3){ 
            input += key;
            lcd.print(key);
          }
        }
      }
    }
  }
}