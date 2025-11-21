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

extern int __heap_start, *__brkval;
int free_memory() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
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
  bool test_running;
  bool paused;
  int THROTTLE_UP_DELAY = 10;

  void testing_screen(){
    lcd.clear();
    lcd.print("RUNNING TEST");
    lcd.setCursor(0, 1);
    lcd.print("TEST #: " + file_name);
    lcd.setCursor(0, 2);
    lcd.print("INCR. LENGTH: " + String(increment_length / 1000) + " s");
    lcd.setCursor(0, 3);
    lcd.print("THROTTLE:" + String(map(current_throttle_pwm, ESC_MIN, ESC_MAX, 0, 100)));
    Serial.println("Starting: Test Num: " + file_name + " | Increment: " + String(increment_pwm));
  }

  void pause_screen(){
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("PRESS * TO RESUME");
    lcd.setCursor(0, 3);
  }

  void pause_data_reading(){
    transmit("w", "");
  }

  void resume_data_reading(){
    transmit("g", "");
  }

public:
  Test(String file_name, int max_throttle_pwm, int increment_pwm, long increment_length, bool piecewise, bool read_ramp_up_data)
  : file_name(file_name), 
    max_throttle_pwm(max_throttle_pwm), 
    increment_pwm(increment_pwm),
    increment_length(increment_length), 
    piecewise(piecewise), 
    read_ramp_up_data(read_ramp_up_data),
    done_throttling(false), 
    test_running(false), 
    paused(false)
  {
    current_throttle_pwm = ESC_MIN;
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
    done_throttling(false), 
    test_running(false), 
    paused(false)
  {
    current_throttle_pwm = ESC_MIN;
  }

  void throttle_down(){
    esc.writeMicroseconds(1000);
  }

  void start_testing(){
    test_running = true;
    transmit("b", "");
    testing_screen();
    prev_ramp_up_finish_timestamp = millis();
  }

  void end_testing(){
    transmit("e", "");
    test_running = false;
    setup();
  }

  bool is_running(){
    return test_running;
  }

  bool is_piecewise(){
    return piecewise;
  }

  bool is_paused(){
    return paused;
  }

  void pause(){
    if(piecewise){
      transmit("w", "");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("THROTTLING DOWN");
      throttle_down();

      pause_screen();
      paused = true;
    }
  }

  void resume(){
    if(piecewise){
      transmit("g", "");
      int next_cycle_length = min(current_throttle_pwm + increment_pwm, max_throttle_pwm);
      current_throttle_pwm = ESC_MIN;
      testing_screen();
      paused = false;
      throttle_up(next_cycle_length);
    }
  }

  int get_throttle_pwm(){
    return current_throttle_pwm;
  }

  void throttle_up(int next_throttle){
    //pause data reading if necessary
    if(!read_ramp_up_data){
      pause_data_reading();
    }

    //get the next increment in pwm cycles
    int next_cycle_length;
    if(next_throttle == -1){
      next_cycle_length = min(current_throttle_pwm + increment_pwm, max_throttle_pwm);
    }
    else{
      next_cycle_length = next_throttle;
    }

    //loop up to the next increment value
    int starting_throttle = current_throttle_pwm;
    for(current_throttle_pwm = starting_throttle; current_throttle_pwm <= next_cycle_length; current_throttle_pwm++){
      if(INTERRUPTED){
        return;
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

    //resume data reading if necessary
    if(!read_ramp_up_data){
      resume_data_reading();
    }

    //record when the ramp up completed so the machine can calculate when the next ramp up session should take place
    prev_ramp_up_finish_timestamp = millis(); //update the timestamp of when the last ramp up sequence finished
  }

  bool increment_done(){ //check if good to throttle up again (increment is complete)
    return (millis() - prev_ramp_up_finish_timestamp) >= increment_length;
  }

  bool test_done(){
    return increment_done() && (current_throttle_pwm == max_throttle_pwm);
  }

  String page_type(){
    return "Test";
  }
}; 

class TaringPage{
private:
  String tare_name;
  String tare_value;
  int status; //0 = inputting, 1 = sending, 2 = taring

  void update_taring_ui() { //This method updates the taring UI
    if(status == 0){ //inputting
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(tare_name + ": " + tare_value);
      lcd.setCursor(0, 2);
      lcd.print("NEXT: " +  String(ENTER_INPUT) + "| SKIP: " + String(SKIP_TARE) + "    ");
      lcd.setCursor(0, 3);
      if(tare_name == F("Torque")){
        lcd.print("UNITS: N.mm");
      }
      else if(tare_name == F("Thrust")){
        lcd.print("Units: mN");
      }
    } 
    else if(status == 1){ //sending
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PRESS " + String(SEND_INPUT) + " TO TARE");
      if(tare_name == F("Analog")){
        lcd.setCursor(0, 1);
        lcd.print("ANALOG SENSORS");
      }
      lcd.setCursor(0, 3);
      lcd.print("BACK: " + String(BACK_BUTTON));
      lcd.setCursor(0, 1);
    }
    else if(status == 2){ //taring
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CALIBRATING...");
    }
  }

public:
  TaringPage(String tare_name) :
    tare_name(tare_name), 
    tare_value("") 
  {}

  void update_tare_value(char inputted_digit){ //This method adds a digit to the current known tare
    if(tare_name != F("Analog") && tare_value.length() < 9){
      tare_value += inputted_digit;
      lcd.setCursor(0, 1);
      lcd.print(tare_value);
    }
  }

  void delete_digit() { //This method deletes a digit from the current known tare
    if(tare_name != F("Analog") && tare_value.length() > 0){
      tare_value.remove(tare_value.length() - 1);
      lcd.setCursor(0, 1);
      lcd.print(tare_value);
    }
  }

  String get_tare_value() { //This method retrieves the current tare value being built up by the user
    return tare_value; 
  }

  void skip_tare(){
    if(tare_name == F("Torque")){
      transmit("o", "");
    }
    else if(tare_name == F("Thrust")){
      transmit("u", "");
    }
    else if(tare_name == F("Analog")){
      transmit("l", "");
    }
  }

  void confirmation_page(){
    status = 1;
    update_taring_ui();
  }

  void set_up_page() { //resets tare value for when the user backs up a page
    tare_value = "";
    status = 0;
    update_taring_ui();
  }

  void save_tare_value() { //This method tells the slave to save the tare value to the EEPROM or for analog, tare
    if(tare_name == F("Torque")){
      transmit("q", tare_value);
    }
    else if(tare_name == F("Thrust")){
      transmit("r", tare_value);
    }
    else if(tare_name == F("Analog")){
      transmit("a", "");
    }
    status = 2;
    update_taring_ui();
    while(1){
      Wire.requestFrom(9, 1);
      if(Wire.read() == 1){
        break;
      }
      delay(100);
    }
  }

  int get_status(){
    return status;
  }

  String page_type() {
    return F("TaringPage");
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
    if(parameter_name != F("Finalize")){
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
    parameter_name(parameter_name), 
    parameter_value("")
  {}

  void update_parameter_value(char inputted_digit) {
    if(parameter_value.length() < 3){
      parameter_value += inputted_digit;
      lcd.setCursor(0, 1);
      lcd.print(parameter_value);
    }
  }

  void delete_digit() {
    if(parameter_value.length() > 0){
      parameter_value.remove(parameter_value.length() - 1);
      lcd.setCursor(0, 1);
      lcd.print(parameter_value);
    }
  }

  String get_parameter_value() {
    return parameter_value;
  }

  void save_parameter_value() {
    if(parameter_name == F("File Name")){
      file_name = parameter_value;
      Serial.println("Recieved:" + file_name + " | Value: " + parameter_value);
    }
    else if(parameter_name == F("Max Throttle")){
      max_throttle = parameter_value.toInt();
      Serial.println("Recieved:" + String(max_throttle) + " | Value: " + parameter_value);
    }
    else if(parameter_name == F("Increment")){
      increment = parameter_value.toInt();
      Serial.println("Recieved:" + String(increment) + " | Value: " + parameter_value);
    }
    else if(parameter_name == F("RPM Markers")){
      RPM_markers = parameter_value;
    }
    else if(parameter_name == F("Increment Length")){
      increment_length = parameter_value.toInt();
    }
  }

  void set_up_page() {
    parameter_value = "";
    update_parameter_ui();
  }

  Test send_parameters() {
    if(parameter_name == F("Finalize")){
      Serial.println("Interupt Status:" + String(INTERRUPTED));

      Serial.println("File Name: " + file_name);
      transmit("f", file_name);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("SETTING UP FILE..."));
      while(1){
        Wire.requestFrom(9, 1);
        if(Wire.read() == 1){
          break;
        }
        delay(100);
      }

      max_throttle = min(max(max_throttle, 0), 100); //constraining it between 0 and 100
      int max_throttle_pwm = map(max_throttle, 0, 100, ESC_MIN, ESC_MAX);

      Serial.println(F("Transmitting increment"));
      increment = min(increment, max_throttle);
      int increment_pwm = map(increment, 0, 100, 0, ESC_MAX - ESC_MIN); //1% -> 99% written in terms of PWM cycle length, assuming a linear mapping

      Serial.println(F("Transmitting RPM markers..."));
      transmit("m", RPM_markers);

      increment_length = (long) increment_length * 1000;
      Serial.println(F("TEST PARAMETERS CONFIRMED!"));

      return Test(file_name, max_throttle_pwm, increment_pwm, increment_length, IS_PIECEWISE, RECORD_THROTTLE_UP);
    }
    return Test();
  }

  String page_type() {
    return F("ParameterPage");
  }
}; 

//STATIC VARIABLE INITIALIZATIONS
String ParameterPage::file_name = "";
int ParameterPage::max_throttle = 0;
int ParameterPage::increment = 0;
String ParameterPage::RPM_markers = "";
long ParameterPage::increment_length = 0;

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
    lcd.print(F("YES: A | NO: B"));
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
    return F("ChoicePage");
  }
}; 

void interrupt(){
  INTERRUPTED = true;
}

void skip_taring(){
  transmit("p", "");
  skip_tare = true;
}

void tare(){
  skip_tare = false;
}

//set boolean flags for piecewise
void run_piecewise(){
  IS_PIECEWISE = true;
}

void not_piecewise(){
  IS_PIECEWISE = false;
}

//set boolean flags for throttle up 
void record_ramp_up(){
  RECORD_THROTTLE_UP = true; 
}

void no_record_ramp_up(){
  RECORD_THROTTLE_UP = false;
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
  test = Test();
  pinMode(INTERRUPT_PIN, INPUT_PULLUP); //set default switch position to HIGH
  pinMode(STATUS_LED_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), interrupt, FALLING); //when switch is pressed down
  INTERRUPTED = false;

  // Set up the LCD display
  lcd.init();
  lcd.backlight();

  //Initialize Serial 
  Serial.begin(9600);
  Serial.println(F("Setting up"));
  Serial.print(F("Free RAM (in bytes): "));
  Serial.println(free_memory());
  Serial.println("Interupt Status:" + String(INTERRUPTED));
  lcd.print(F("Loading ."));

  //Initialize I2C protocol (master)
  Wire.begin();
  lcd.setCursor(0, 0);
  lcd.print(F("Loading .."));
  
  //Initialize servo PWM and arm the ESC
  esc.attach(ESC_PIN); //set esc to pin
  esc.writeMicroseconds(MIN_THROTTLE); //minimum throttle; arm the esc

  lcd.setCursor(0, 0);
  lcd.print(F("Loading ..."));

  //wait for SD card to initialize
  lcd.setCursor(0, 1);
  lcd.print(F("INIT SD CARD"));
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
  lcd.print(F("Loading ....."));

  Serial.println(F("READY"));

  page_index = 0;
  page_type = 0;
  choice_pages[page_type] -> set_up_page();
}

void loop() {
  //KEYSTROKE LOGIC (IF USER INPUTS) FOR BEFORE TEST
  char key = keypad.getKey();
  digitalWrite(STATUS_LED_PIN, HIGH);
  if(key && !test.is_running()){ 
    if(page_type == 0){ //CHOICE
      ChoicePage* curr_page = choice_pages[page_index];
      if(key == 'A'){
        curr_page -> runChoiceA();
        page_index++;
        if(page_index == 3 && !skip_tare){
          page_index = 0;
          page_type = 1;
          taring_pages[page_index] -> set_up_page();
        }
        else if(page_index == 3 && skip_tare){
          page_index = 0;
          page_type = 2;
          parameter_pages[page_index] -> set_up_page();
        }
        else{
          choice_pages[page_index] -> set_up_page();
        }
      }
      else if(key == 'B'){
        curr_page -> runChoiceB();
        page_index++;
        if(page_index == 3 && !skip_tare){
          page_index = 0;
          page_type = 1;
          taring_pages[page_index] -> set_up_page();
        }
        else if(page_index == 3 && skip_tare){
          page_index = 0;
          page_type = 2;
          parameter_pages[page_index] -> set_up_page();
        }
        else{
          choice_pages[page_index] -> set_up_page();
        }
      }
      else if(key == BACK_BUTTON){
        if(page_index > 0){
          page_index--;
          choice_pages[page_index] -> set_up_page();
        }
      }
    }
    else if(page_type == 1){ //TARING
      TaringPage* curr_page = taring_pages[page_index];
      if(key == ENTER_INPUT && curr_page -> get_status() == 0){ //user clicks next
        curr_page -> confirmation_page(); 
      }
      else if(key == DELETE && curr_page -> get_status() == 0){
        curr_page -> delete_digit();
      }
      else if(key == BACK_BUTTON && curr_page -> get_status() == 1){
        curr_page -> set_up_page();
      }
      else if(key >= '0' && key <= '9' && curr_page -> get_status() == 0){
        curr_page -> update_tare_value(key);
      }
      else if(key == BACK_BUTTON && curr_page -> get_status() == 0){
        if(page_index > 0){
          page_index--;
          parameter_pages[page_index] -> set_up_page();
        }
      }
      else if(key == SKIP_TARE && (curr_page -> get_status() == 0 || curr_page -> get_status() == 1)){ //TODO: Add better logic for skipping (if skip, get the individual value from the EEPROM and set it)
        curr_page -> skip_tare();
        page_index++;
        if(page_index == 3){ //if done taring everything and sent, go to parameter pages
          page_index = 0;
          page_type = 2;
          parameter_pages[page_index] -> set_up_page();
        }
        else{
          taring_pages[page_index] -> set_up_page();
        }
      }
      else if(key == SEND_INPUT && curr_page -> get_status() == 1){
        curr_page -> save_tare_value();
        page_index++;
        if(page_index == 3){ //if done taring everything and sent, go to parameter pages
          page_index = 0;
          page_type = 2;
          parameter_pages[page_index] -> set_up_page();
        }
        else{
          taring_pages[page_index] -> set_up_page();
        }
      }
    }
    else if(page_type == 2){ //PARAMETERS
      ParameterPage* curr_page = parameter_pages[page_index];
      if(key == ENTER_INPUT){
        curr_page -> save_parameter_value();
        if(page_index < 5){
          page_index++;
          parameter_pages[page_index] -> set_up_page();
        }
      }
      else if(key == SEND_INPUT){
        if(page_index == 5){
          test = curr_page -> send_parameters();
          test.start_testing();
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
      else if(key >= '0' && key <= '9'){
        curr_page -> update_parameter_value(key);
      }
    }
  }

  //TEST LOGIC (MOTOR THROTTLE UP, TEST FLOW, ETC.)
  if(test.is_running()){
    if(INTERRUPTED){
      Serial.println("INterrupted");
      INTERRUPTED = false;
      test.end_testing();
    }
    if(test.is_piecewise()){ //PIECEWISE
      if(test.is_paused()){ //WHEN TEST IS PAUSED
        if(key && key == SEND_INPUT){
          test.resume();
        }
      }
      else{ //PAUSE WHEN INCREMENT DONE
        if(test.increment_done()){
          Serial.println("increment done");
          test.pause();
        }
      }
    }
    else{ //NOT PIECEWISE
      if(test.increment_done()){
        test.throttle_up(-1);
      } 
    }

    if(test.test_done()){
      Serial.println("DONE");
      test.end_testing();
    }
  }
}