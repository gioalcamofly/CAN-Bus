/****************************************************************************
CAN Read Demo for the SparkFun CAN Bus Shield. 

Written by Stephen McCoy. 
Original tutorial available here: http://www.instructables.com/id/CAN-Bus-Sniffing-and-Broadcasting-with-Arduino
Used with permission 2016. License CC By SA. 

Distributed as-is; no warranty is given.
*************************************************************************/

#include <Canbus.h>
#include <defaults.h>
#include <global.h>
#include <mcp2515.h>
#include <mcp2515_defs.h>
#include <string.h>
#include <avr/sleep.h>
#include <EEPROM.h>

//********************************Setup Loop*********************************//
#include <Wire.h> // Arduino's I2C library
 
#define LCD05_I2C_ADDRESS byte((0xC6)>>1)
#define LCD05_I2C_INIT_DELAY 100 // in milliseconds

// LCD05's command related definitions
#define COMMAND_REGISTER byte(0x00)
#define FIFO_AVAILABLE_LENGTH_REGISTER byte(0x00)
#define LCD_STYLE_16X2 byte(5)


#define UP A1
#define DOWN A3

// LCD05's command codes
#define CURSOR_HOME             byte(1)
#define SET_CURSOR              byte(2) // specify position with a byte in the interval 0-32/80
#define SET_CURSOR_COORDS       byte(3) // specify position with two bytes, line and column
#define HIDE_CURSOR             byte(4)
#define SHOW_UNDERLINE_CURSOR   byte(5)
#define SHOW_BLINKING_CURSOR    byte(6)
#define BACKSPACE               byte(8)
#define HORIZONTAL_TAB          byte(9) // advances cursor a tab space
#define SMART_LINE_FEED         byte(10) // moves the cursor to the next line in the same column
#define VERTICAL_TAB            byte(11) // moves the cursor to the previous line in the same column
#define CLEAR_SCREEN            byte(12)
#define CARRIAGE_RETURN         byte(13)
#define CLEAR_COLUMN            byte(17)
#define TAB_SET                 byte(18) // specify tab size with a byte in the interval 1-10
#define BACKLIGHT_ON            byte(19)
#define BACKLIGHT_OFF           byte(20) // this is the default
#define DISABLE_STARTUP_MESSAGE byte(21)
#define ENABLE_STARTUP_MESSAGE  byte(22)
#define SAVE_AS_STARTUP_SCREEN  byte(23)
#define SET_DISPLAY_TYPE        byte(24) // followed by the type, which is byte 5 for a 16x2 LCD style
#define CHANGE_ADDRESS          byte(25) // see LCD05 specification
#define CUSTOM_CHAR_GENERATOR   byte(27) // see LCD05 specification
#define DOUBLE_KEYPAD_SCAN_RATE byte(28)
#define NORMAL_KEYPAD_SCAN_RATE byte(29)
#define CONTRAST_SET            byte(30) // specify contrast level with a byte in the interval 0-255
#define BRIGHTNESS_SET          byte(31) // specify brightness level with a byte in the interval 0-255


char text[] = { "UP-S1; DOWN-S2;  LEFT:History" };
uint16_t rtc = 0;
uint8_t hour;
uint8_t minute;
uint8_t second;
volatile boolean flag = LOW;
volatile uint16_t thisDelay = 0;
volatile uint8_t secondsToSleep = 20;
volatile boolean sleep = LOW;

inline void write_command(byte command)
{ Wire.write(COMMAND_REGISTER); Wire.write(command); }

void set_display_type(byte address, byte type)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(SET_DISPLAY_TYPE);
  Wire.write(type);
  Wire.endTransmission();
}

void clear_screen(byte address)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(CLEAR_SCREEN);
  Wire.endTransmission();
}

void cursor_home(byte address)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(CURSOR_HOME);
  Wire.endTransmission();
}

void hide_cursor(byte address) {
  Wire.beginTransmission(address);
  write_command(HIDE_CURSOR);
  Wire.endTransmission();
}

void set_cursor(byte address, byte pos)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(CURSOR_HOME);
  Wire.write(pos);
  Wire.endTransmission();
}

void set_cursor_coords(byte address, byte line, byte column)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(CURSOR_HOME);
  Wire.write(line);
  Wire.write(column);
  Wire.endTransmission();
}

void show_blinking_cursor(byte address)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(SHOW_BLINKING_CURSOR);
  Wire.endTransmission();
}

void backlight_on(byte address)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(BACKLIGHT_ON);
  Wire.endTransmission();
}

void backlight_off(byte address)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(BACKLIGHT_OFF);
  Wire.endTransmission();
}

bool ascii_chars(byte address, byte* bytes, int length)
{
  if(length<=0) return false;
  Wire.beginTransmission(address); // start communication with LCD 05
  Wire.write(COMMAND_REGISTER);
  for(int i=0; i<length; i++, bytes++) Wire.write(*bytes);
  Wire.endTransmission();
  return true;
}

byte read_fifo_length(byte address)
{
  Wire.beginTransmission(address);
  Wire.write(FIFO_AVAILABLE_LENGTH_REGISTER);                           // Call the register for start of ranging data
  Wire.endTransmission();
  
  Wire.requestFrom(address,byte(1)); // start communication with LCD 05, request one byte
  while(!Wire.available()) { /* do nothing */ }
  return Wire.read();
}
 
bool backlight=false; 


String receiveMsg(tCAN message) {
   Serial.print("ID: ");
   Serial.print(message.id,HEX);
   Serial.print(", ");
   Serial.print("Data: ");
   Serial.print(message.header.length,DEC);
   String msg;
   for(int i=0;i<message.header.length;i++) {  
      msg.concat((char)message.data[i]);
   }
   Serial.println(msg);
   Serial.println("");
   return msg;
}

void printLCD(char msg[], uint8_t sizeMsg) {
  int fifo_length;
  while( (fifo_length=read_fifo_length(LCD05_I2C_ADDRESS))<4 ) { /*do nothing*/ }
  
  ascii_chars(LCD05_I2C_ADDRESS,(byte*)(msg),sizeMsg);
}

void send_string(String msg, int id){
  uint8_t tam = msg.length() +1;
  char x [tam];
  msg.toCharArray(x, sizeof(x));
  
  tCAN message;

  message.id = id; //formatted in HEX
  message.header.rtr = 0;
  message.header.length = tam-1; //formatted in DEC
  for(int i = 0; i < tam; i++){
    message.data[i] = msg[i];  
  }
  mcp2515_bit_modify(CANCTRL, (1<<REQOP2)|(1<<REQOP1)|(1<<REQOP0), 0);
  mcp2515_send_message(&message);
}

void goToSleep(){
  backlight_off(LCD05_I2C_ADDRESS);
  clear_screen(LCD05_I2C_ADDRESS);
  sleep = HIGH;
  EEPROM.put(0, 55);
  EEPROM.put(1, hour);
  EEPROM.put(2, minute);
  EEPROM.put(3, second);
  
  cli();

  TIMSK2 &= ~(1 << OCIE2A);
  Serial.flush();
  sei();
  SMCR |= (1<<(SM1)) | (1<<(SE));
  sleep_cpu();
  cli();
  SMCR &= ~(1<<(SE));  
  PCICR &= ~(1<<(PCIE1)); //Pinchange interrupt
  PCIFR |= (1<<(PCIF1));
  TIMSK2 |= (1 << OCIE2A);
  sei();  
  backlight_on(LCD05_I2C_ADDRESS);
}

void setRealTime() {
  Serial.println("Please, introduce the actual time in the format 'HH:MM:SS'");
  while(!Serial.available()) {}
  String command = Serial.readString();
  uint8_t first = command.indexOf(':');
  uint8_t last = command.lastIndexOf(':');
  hour = command.substring(0, first).toInt();
  
  if (hour >= 24) {
    Serial.println("HH should be between 0-23");
    setRealTime();
    return;
  }
  minute = command.substring(first + 1, last).toInt();
  if (minute >= 60) {
    Serial.println("MM should be between 0-59");
    setRealTime();
    return;
  }
  second = command.substring(last + 1).toInt();
  if (second >= 60) {
    Serial.println("SS should be between 0-59");
    setRealTime();
    return;
  }
  Serial.print(hour); Serial.print(":"); Serial.print(minute); Serial.print(":"); Serial.println(second);
  
  //Interrupts activated
  
  
}

void setTimerAndButtons() {
  cli();
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2  = 0;//Contador a 0
  //Comparador
  OCR2A = 249;// = (16*10^6) / (1000*64) - 1 (tiene que ser <256)
  // Poner en modo CTC
  TCCR2A |= (1 << WGM21);
  // Poner el bit CS22 para prescaler 64
  TCCR2B |= (1 << CS22);   

  //Joystick interrupts
  PCICR |= (1<<(PCIE1)); //Pinchange interrupt
  PCIFR |= (1<<(PCIF1));
  PCMSK1 |= (1<<(PCINT9)) | (1<<(PCINT11));
  
  // Habilitar interrupción por comparación
  TIMSK2 |= (1 << OCIE2A);
  sei();
  
}

void newDelay(uint16_t milliseconds) {
  flag = HIGH;
  while (thisDelay < milliseconds) {}
  flag = LOW;
  thisDelay = 0;
}

void setup() {
  Serial.begin(9600); // For debug use

  uint8_t eepromRead = EEPROM.read(0);
  if (eepromRead != 55) {
    setRealTime();
  } else {
    hour = EEPROM.read(1);
    minute = EEPROM.read(2);
    second = EEPROM.read(3);
  }
  setTimerAndButtons();
  
  Wire.begin();
  newDelay(LCD05_I2C_INIT_DELAY);  
  pinMode(UP, INPUT);
  pinMode(DOWN, INPUT);

  digitalWrite(UP, HIGH);
  digitalWrite(DOWN, HIGH);
  
  Serial.print("initializing LCD05 display in address 0x");
  Serial.print(LCD05_I2C_ADDRESS,HEX); Serial.println(" ...");
  
  set_display_type(LCD05_I2C_ADDRESS,LCD_STYLE_16X2);
  clear_screen(LCD05_I2C_ADDRESS);
  cursor_home(LCD05_I2C_ADDRESS);
  //show_blinking_cursor(LCD05_I2C_ADDRESS);
  backlight_on(LCD05_I2C_ADDRESS);
  hide_cursor(LCD05_I2C_ADDRESS);
  Serial.println("CAN Read - Testing receival of CAN Bus message");  
  newDelay(1000);
  
  if(Canbus.init(CANSPEED_500))  //Initialise MCP2515 CAN controller at the specified speed
    Serial.println("CAN Init ok");
  else
    Serial.println("Can't init CAN");
  
    
  newDelay(1000);

  printLCD(text, sizeof(text));
}

//********************************Main Loop*********************************//


void loop(){

 if (!secondsToSleep) goToSleep();
 

  //
  if (mcp2515_check_message()) {
    tCAN message;
    if (mcp2515_get_message(&message)) {
      clear_screen(LCD05_I2C_ADDRESS);
      String msg = receiveMsg(message);
      msg.concat(" grados");
      char buf[msg.length()];
      msg.toCharArray(buf, msg.length() + 1);
      printLCD(buf, sizeof(buf));
      newDelay(5000);
      clear_screen(LCD05_I2C_ADDRESS);
      printLCD(text, sizeof(text));
    }
  }
  printHour();
  newDelay(100);
  
  
}

void printHour() {
  String hourString = "    ";
  if (hour < 10) {
    hourString.concat("0");
  }
  hourString.concat(hour);
  hourString.concat(":");
  if (minute < 10) {
    hourString.concat("0");
  }
  hourString.concat(minute);
  hourString.concat(":");
  if (second < 10) {
    hourString.concat("0");
  }
  hourString.concat(second);
  hourString.concat("     UP:S1;DOWN:S2;");
  char prueba[hourString.length()];
  hourString.toCharArray(prueba, hourString.length() + 1);
  clear_screen(LCD05_I2C_ADDRESS);
  printLCD(prueba, sizeof(prueba));
}

ISR(TIMER2_COMPA_vect){
  rtc++;
  if (flag) thisDelay++;
  if (!(rtc % 1000)) {
    secondsToSleep--;
    second = (second + 1)%60;
    if (!second) {
      minute = (minute + 1)%60;
      if (!minute) {
        hour = (hour + 1)%24;
      }
    }
    //printHour();
  }
} 

ISR(PCINT1_vect) { 

  if (!sleep) {
   if (digitalRead(UP) == 0) {
    send_string("Up", 1);
    secondsToSleep = 20;
   }
  
   if (digitalRead(DOWN) == 0) {
    send_string("Down", 2);
    secondsToSleep = 20;
   }
  } else {
    sleep = LOW;
  }
}

