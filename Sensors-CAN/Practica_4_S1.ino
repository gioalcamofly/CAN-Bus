#include <Canbus.h>
#include <defaults.h>
#include <global.h>
#include <mcp2515.h>
#include <mcp2515_defs.h>

int indice = 0;

void send_string(String msg){
  uint8_t tam = msg.length() +1;
  char x [tam];
  msg.toCharArray(x, sizeof(x));
  
  tCAN message;

  message.id = 0x631; //formatted in HEX
  message.header.rtr = 0;
  message.header.length = tam-1; //formatted in DEC
  for(int i = 0; i < tam; i++){
    message.data[i] = msg[i];  
  }
  mcp2515_bit_modify(CANCTRL, (1<<REQOP2)|(1<<REQOP1)|(1<<REQOP0), 0);
  mcp2515_send_message(&message);
}

double GetTemp()
{
  unsigned int wADC;
  double t;
  ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
  ADCSRA |= _BV(ADEN);  // enable the ADC
  delay(20); 
  ADCSRA |= _BV(ADSC);  // Start the ADC
  while (bit_is_set(ADCSRA,ADSC));
  wADC = ADCW;
  t = (wADC - 324.31 ) / 1.22;
  return (t);
}

//********************************Setup Loop*********************************//

void setup() {
  Serial.begin(9600);
  Serial.println("CAN Write - Testing transmission of CAN Bus messages");
  delay(1000);
  
  if(Canbus.init(CANSPEED_500))  //Initialise MCP2515 CAN controller at the specified speed
    Serial.println("CAN Init ok");
  else
    Serial.println("Can't init CAN");
    
  delay(1000);
}

//********************************Main Loop*********************************//

void loop() 
{
  tCAN message;
  if (mcp2515_check_message()) {
      if (mcp2515_get_message(&message)) {
        if (message.id == 1){
          String temperature = "";
          temperature = String(GetTemp(), 2);
          Serial.println(temperature);
          send_string(temperature);
        }
      }
  }
}
