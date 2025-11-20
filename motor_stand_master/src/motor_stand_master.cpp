#include <Arduino.h>
#include <motor_stand_master_definitions.h>

void transmit(String type, String value){
  String signal;
  if(value == ""){
    signal = type;
  }
  else{
    signal = type + value;
  }
  int length = signal.length();
  Wire.beginTransmission(9);
  Wire.write(signal.c_str(), length);
  Wire.endTransmission();
}

class Test{
private:
  String file_name, RPM_markers;
  int max_throttle_pwm, increment_pwm;
  long increment_length; 
  long current_throttle_pwm;
  long prev_ramp_up_finish_timestamp;
  bool piecewise;
  bool read_ramp_up_data;
  bool done_throttling;
  int THROTTLE_UP_DELAY = 10;

  void transmit(String type, String value){
    String signal;
    if(value == ""){
      signal = type;
    }
    else{
      signal = type + value;
    }
    int length = signal.length();
    Wire.beginTransmission(9);
    Wire.write(signal.c_str(), length);
    Wire.endTransmission();
  }

  void testing_screen(){
    lcd.clear();
    lcd.print("RUNNING TEST");
    lcd.setCursor(0, 1);
    lcd.print("TEST #: " + file_name);
    lcd.setCursor(0, 2);
    lcd.print("INCR. LENGTH: " + String(increment_length) + " s");
    lcd.setCursor(0, 3);
    lcd.print("THROTTLE:" + String(map(current_throttle_pwm, ESC_MIN, ESC_MAX, 0, 100)));
    Serial.println("Starting: Test Num: " + file_name + " | Increment: " + String(increment_pwm));
    start_motor = true;
    transmit("b", "");
  }

  void pause_screen(){
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("PRESS * TO RESUME");
    lcd.setCursor(0, 3);
  }

public:
  Test(String file_name, int max_throttle_pwm, int increment_pwm, long increment_length)
      : file_name(file_name), 
      max_throttle_pwm(max_throttle_pwm), 
      increment_pwm(increment_pwm),
      increment_length(increment_length)
  {
    piecewise = false;
    read_ramp_up_data = false;
    done_throttling = false;
  }

  Test() 
  : file_name(""),
    RPM_markers(""),
    max_throttle_pwm(0),
    increment_pwm(0),
    increment_length(0),
    current_throttle_pwm(0),
    prev_ramp_up_finish_timestamp(0),
    piecewise(false),
    read_ramp_up_data(false),
    done_throttling(false)
  {}

  void set_piecewise() {
    piecewise = true;
  }

  void set_read_ramp_up_data() {
    read_ramp_up_data = true;
  }

  void throttle_down(){
    esc.writeMicroseconds(1000);
  }

  void end_testing(){
    transmit("e", "");
    setup();
  }

  void pause(){
    if(piecewise){
      transmit("w", "");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("THROTTLING DOWN");
      throttle_down();

      pause_screen();
    }
  }

  void resume(){
    if(piecewise){
      transmit("w", "");
      int next_cycle_length = min(current_throttle_pwm + increment_pwm, MAX_THROTTLE);
      throttle_up(next_cycle_length);
      testing_screen();
    }
  }

  void pause_data_reading(){
    transmit("w", "");
  }

  void resume_data_reading(){
    transmit("g", "");
  }

  String page_type(){
    return "Test";
  }

  void throttle_up(int next_throttle){
    if(!read_ramp_up_data){
      pause_data_reading();
    }

    int next_cycle_length;
    if(next_throttle == -1){
      next_cycle_length = min(current_throttle_pwm + increment_pwm, max_throttle_pwm);
    }
    else{
      next_cycle_length = next_throttle;
    }

    int starting_throttle = current_throttle_pwm;
    for(current_throttle_pwm = starting_throttle; current_throttle_pwm <= next_cycle_length; current_throttle_pwm++){
      if(INTERRUPTED){
        return; //return to the main loop and throttle down
      }
      esc.writeMicroseconds(current_throttle_pwm);
      int percent = map(current_throttle_pwm, ESC_MIN, ESC_MAX, 0, 100);
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
    current_throttle_pwm--;

    if(!read_ramp_up_data){
      resume_data_reading();
    }
    prev_ramp_up_finish_timestamp = millis(); //update the timestamp of when the last ramp up sequence finished
  }

  bool increment_done(){
    return millis() >= prev_ramp_up_finish_timestamp + increment_length;
  }
}; 

class TaringPage{
private:
  String tare_name;
  String tare_value;

  void update_taring_ui(String status) { //This method updates the taring UI
    if(status == "Inputting"){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(tare_name + ": " + tare_value);
      lcd.setCursor(0, 2);
      lcd.print("NEXT: " +  String(ENTER_INPUT) + "| SKIP: " + String(SKIP_TARE) + "    ");
      lcd.setCursor(0, 3);
      if(tare_name == "Torque"){
        lcd.print("UNITS: N.mm");
      }
      else if(tare_name == "Thrust"){
        lcd.print("Units: mN");
      }
    }
    else if(status == "Sending"){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PRESS " + String(SEND_INPUT) + " TO TARE");
      if(tare_name == "Analog"){
        lcd.setCursor(0, 1);
        lcd.print("ANALOG SENSORS");
      }
      lcd.setCursor(0, 3);
      lcd.print("BACK: " + String(BACK_BUTTON));
      lcd.setCursor(0, 1);
    }
    else if(status == "Taring"){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CALIBRATING...");
    }
  }

public:
  TaringPage(String tare_name) :
    tare_name(tare_name) { //CONSTRUCTOR
    tare_value = "";
  }

  void update_tare_value(int inputted_digit){ //This method adds a digit to the current known tare
    if(tare_name != "Analog"){
      tare_value += inputted_digit;
      lcd.setCursor(0, 0);
      lcd.print(tare_name + ": " + tare_value);
    }
  }

  void delete_digit() { //This method deletes a digit from the current known tare
    if(tare_name != "Analog"){
      if(tare_value.length() != 0){
        tare_value.remove(tare_value.length() - 1);
        lcd.setCursor(0, 0);
        lcd.print(tare_name + ": " + tare_value);
      }
    }
  }

  String get_tare_value() { //This method retrieves the current tare value being built up by the user
    return tare_value; 
  }

  void confirmation_page(){
    update_taring_ui("Sending");
  }

  void set_up_page() { //resets tare value for when the user backs up a page
    tare_value = "";
    update_taring_ui("inputting");
  }

  void save_tare_value() { //This method tells the slave to save the tare value to the EEPROM or for analog, tare
    if(tare_name == "Torque"){
      transmit("r", tare_value);
    }
    else if(tare_name == "Thrust"){
      transmit("q", tare_value);
    }
    else if(tare_name == "Analog"){
      transmit("a", "");
    }
    update_taring_ui("Taring");
    while(1){
      Wire.requestFrom(9, 1);
      if(Wire.read() == 1){
        break;
      }
      delay(100);
    }
  }

  String page_type() {
    return "TaringPage";
  }
};

class ParameterPage{
String parameter_name;
String parameter_value; 

private:
  static String file_name, RPM_markers;
  static int max_throttle, increment;
  static long increment_length;
  
  void update_parameter_ui() {
    if(parameter_name != "Finalize"){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(parameter_name + ": " + parameter_value);
      lcd.setCursor(0, 2);
      lcd.print("NEXT: " +  String(ENTER_INPUT) + " | BACK: " + String(BACK_BUTTON));
      lcd.setCursor(0, 3);
      lcd.print("THROTTLE:OFF");
      lcd.setCursor(0, 1);
    }
    else{
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PRESS" + String(SEND_INPUT) + "TO START");
    }
  }

public:
  ParameterPage(String parameter_name) : 
    parameter_name(parameter_name)
  { //CONSTRUCTOR
    parameter_value = "";
  }

  void update_parameter_value(int inputted_digit) {
    parameter_value += inputted_digit;
    lcd.setCursor(0, 0);
    lcd.print(parameter_name + ": " + parameter_value);
  }

  void delete_digit() {
    if(parameter_value.length() != 0){
      parameter_value.remove(parameter_value.length() - 1);
      lcd.setCursor(0, 0);
      lcd.print(parameter_name + ": " + parameter_value);
    }
  }

  String get_parameter_value() {
    return parameter_value;
  }

  void save_parameter_value() {
    if(parameter_name == "File Name"){
      file_name = parameter_value;
    }
    else if(parameter_name == "Max Throttle"){
      max_throttle = parameter_value.toInt();
    }
    else if(parameter_name == "Increment"){
      increment = parameter_value.toInt();
    }
    else if(parameter_name == "RPM Markers"){
      RPM_markers = parameter_value;
    }
    else if(parameter_name == "Increment Length"){
      increment_length = parameter_value.toInt();
    }
  }

  void set_up_page() {
    parameter_value = "";
    update_parameter_ui();
  }

  Test send_parameters() {
    if(parameter_name == "Finalize"){
      transmit("f", file_name);
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

      max_throttle = min(max(max_throttle, 0), 100); //constraining it between 0 and 100
      int max_throttle_pwm = map(max_throttle, 0, 100, ESC_MIN, ESC_MAX);

      increment = min(increment, max_throttle);
      int increment_pwm = map(increment, 0, 100, 0, ESC_MAX - ESC_MIN); //1% -> 99% written in terms of PWM cycle length, assuming a linear mapping
      
      Serial.println("Transmitting RPM markers...");
      transmit("m", RPM_markers);

      increment_length = (long) increment_length * 1000;
      Serial.println("TEST PARAMETERS CONFIRMED!");

      return Test(file_name, max_throttle_pwm, increment_pwm, increment_length);
    }
  }

  String page_type() {
    return "ParameterPage";
  }
}; 

class ChoicePage {
private:
  String prompt;
  void (*choiceA)();
  void (*choiceB)();

  void choice_ui() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(prompt);
    lcd.setCursor(0, 3);
    lcd.print("YES: A | NO: B");
  }

public:
  ChoicePage(String prompt, void (*choiceA)(), void (*choiceB)()) : 
    prompt(prompt), 
    choiceA(choiceA), 
    choiceB(choiceB)
  {}

  void runChoiceA() { //run choice A
    choiceA();
  }

  void runChoiceB() { //run choice B
    choiceB();
  }

  void set_up_page() {
    choice_ui();
  }

  String page_type() {
    return "ChoicePage";
  }
}; 



void interrupt(){
  INTERRUPTED = true;
}

void skip_taring(){
  Wire.beginTransmission(9);
  Wire.write('p');
  Wire.endTransmission();
}

void tare(){

}

void run_piecewise(){
  set_piecewise = false;
}

void not_piecewise(){
  set_piecewise = true;
}

void record_ramp_up(){
}

void no_record_ramp_up(){
}

//DEFINE PAGES
ChoicePage use_prev_tare = ChoicePage("USE PREV TARE?", skip_taring, tare);
ChoicePage read_ramp_up = ChoicePage("READ RAMP UP DATA?", record_ramp_up, no_record_ramp_up);
ChoicePage piecewise = ChoicePage("PIECEWISE?", run_piecewise, not_piecewise);
ChoicePage* choice_pages[3] = {&read_ramp_up, &piecewise, &use_prev_tare};

TaringPage tare_torque = TaringPage("Torque");
TaringPage tare_thrust = TaringPage("Thrust");
TaringPage tare_analog = TaringPage("Analog");
TaringPage* taring_pages[3] = {&tare_torque, &tare_thrust, &tare_analog};

ParameterPage file_name = ParameterPage("File Name");
ParameterPage max_throttle = ParameterPage("Max Throttle");
ParameterPage increment = ParameterPage("Increment");
ParameterPage markers = ParameterPage("RPM Markers");
ParameterPage increment_length = ParameterPage("Increment Length");
ParameterPage finalize = ParameterPage("Finalize");
ParameterPage* parameter_pages[6] = {&file_name, &max_throttle, &increment, &markers, &increment_length, &finalize};

Test test;
void setup() {
  page_index = 0;
  page_type = 0;
  pinMode(INTERRUPT_PIN, INPUT_PULLUP); //set default switch position to HIGH
  pinMode(STATUS_LED_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), interrupt, FALLING); //when switch is pressed down

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
  banner_status = 0;

  lcd.setCursor(0, 0);
  lcd.print("Loading ...");

  //wait for SD card to initialize
  lcd.setCursor(0, 1);
  lcd.print("INIT SD CARD");
  while(1){
    Wire.requestFrom(9, 1);
    delay(100);
    if(Wire.read() == 1){
      break;
    }
    delay(100);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Loading .....");

  Serial.println("READY");
}

void loop() {
  char key = keypad.getKey();
  digitalWrite(STATUS_LED_PIN, HIGH);
  if(key){
    switch(page_type){
      case 0: //Choice

      case 1: //Taring

      case 2: //Parameters
        ParameterPage* curr_page = parameter_pages[page_index];
        if(key == ENTER_INPUT){
          if(page_index < 5){
            page_index++;
            parameter_pages[page_index] -> set_up_page();
          }
        }
        else if(key == SEND_INPUT){
          if(page_index == 5){
            test = curr_page -> send_parameters();
          }
        }
        else if(key == BACK_BUTTON){
          if(page_index > 0){
            page_index--;
            parameter_pages[page_index] -> set_up_page();
          }
        }
        else if(key == DELETE){
          curr_page -> delete_digit();
        }
        else if(key > '0' && key < '9'){
          curr_page -> update_parameter_value(key);
        }
    }
  }
}