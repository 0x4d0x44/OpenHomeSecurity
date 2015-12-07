// ATMEL ATMEGA1284P main board PCB v. 1.5
//
//                       +---\/---+
//    D IN 7 (D 0) PB0  1|        |40  PA0 (AI 0 / D24) Voltage
//    SS_NET (D 1) PB1  2|        |39  PA1 (AI 1 / D25) A IN 1
//   INT_RFM (D 2) PB2  3|        |38  PA2 (AI 2 / D26) A IN 2
//    AC_OFF (D 3) PB3  4|        |37  PA3 (AI 3 / D27) A IN 3
// SS_RFM/SS (D 4) PB4  5|        |36  PA4 (AI 4 / D28) A IN 4
//      MOSI (D 5) PB5  6|        |35  PA5 (AI 5 / D29) A IN 5
//      MISO (D 6) PB6  7|        |34  PA6 (AI 6 / D30) A IN 6
//       SCK (D 7) PB7  8|        |33  PA7 (AI 7 / D31) A IN 7
//                 RST  9|        |32  AREF
//                 VCC 10|        |31  GND 
//                 GND 11|        |30  AVCC
//               XTAL2 12|        |29  PC7 (D 23) D IN 6
//               XTAL1 13|        |28  PC6 (D 22) D IN 5
//      RX0 (D 8)  PD0 14|        |27  PC5 (D 21) D IN 4
//      TX0 (D 9)  PD1 15|        |26  PC4 (D 20) D IN 3
//      RX1 (D 10) PD2 16|        |25  PC3 (D 19) D IN 2
//      TX1 (D 11) PD3 17|        |24  PC2 (D 18) D IN 1
//      ED1 (D 12) PD4 18|        |23  PC1 (D 17) SDA
//   BAT OK (D 13) PD5 19|        |22  PC0 (D 16) SCL
//     OUT1 (D 14) PD6 20|        |21  PD7 (D 15) OUT2
//                       +--------+
//
#include <NilRTOS.h>
#include <DigitalIO.h>
#include <SPI.h>

#include <Ethernet.h>
static uint8_t mac[6] = { 0x02, 0xAA, 0xBB, 0xCC, 0x00, 0x22 };  // CHANGE THIS TO YOUR OWN UNIQUE VALUE
IPAddress  our_ip( 10,  10,  10, 184);  // CHANGE THIS TO MATCH YOUR HOST NETWORK
IPAddress gateway( 10,  10,  10, 254);  // CHANGE THIS TO MATCH YOUR HOST NETWORK
IPAddress  subnet(255, 255, 255,   0);  // CHANGE THIS TO MATCH YOUR HOST NETWORK

// NTP
IPAddress  timeIP( 81,   0, 239, 181);  // ntp.globe.cz 
#include <EthernetUdp.h>                      //
#define NTP_LOCAL_PORT         9999           // local port to listen for UDP packets
#define NTP_PACKET_SIZE        48             // NTP time stamp is in the first 48 bytes of the message
#define NTP_SECS_YR_1900_2000 (3155673600UL)
#define NTP_TIME_ZONE          2              // Central European Time
#define NTP_USELESS_BYTES      40             // Set useless to 32 for speed; set to 40 for accuracy.
#define NTP_POLL_INTV          100            // poll response this many ms
#define NTP_POLL_MAX           20             // poll response up to this many times
const long ntpFirstFourBytes = 0xEC0600E3;    // NTP request header
EthernetUDP udp;                              // A UDP instance to let us send and receive packets over UDP

// WebServer
#define WEBDUINO_FAVICON_DATA     "" // disable favicon
#define WEBDUINO_AUTH_REALM       "OHS"
#define WEBDUINO_SERIAL_DEBUGGING 0
#define PREFIX                    ""
#include "WebServer.h"
WebServer webserver(PREFIX, 80);
#include "html.h"

// MQTT 
#include <PubSubClient.h>
// Callback function header
void callback(char* topic, byte* payload, unsigned int length);
byte MQTTIP[]       = { 10,  10,  10, 126};
char str_MQTT_dev[] = "OHS"; // device name
EthernetClient MQTTethClient;
PubSubClient client(MQTTIP, 1883, callback, MQTTethClient);

//SMTP
//#include <Client.h>
#include <Mail.h>
#include <SMTPClient.h>
EthernetClient SMTPethClient;
//SMTPClient SMTPclient(SMTPethClient, "mail.smtp2go.com", 2525);
//Mail mail;

#include <RFM12B.h>
// You will need to initialize the radio by telling it what ID it has and what network it's on
// The RADIO_NODEID takes values from 1-127, 0 is reserved for sending broadcast messages (send to all nodes)
// The Network ID takes values from 0-255
// By default the SPI-SS line used is D10 on Atmega328. You can change it by calling .SetCS(pin) where pin can be {8,9,10}
#define RADIO_NODEID      1             //network ID used for this unit
#define RADIO_NETWORKID   255           //the network ID we are on
#define RADIO_FREQUENCY   RF12_868MHZ
#define RADIO_ACK_TIME    20
#define RADIO_UNIT_OFFSET 15
// Need an instance of the Radio Module
RFM12B radio;

// RTOS friendly Analog
#include <NilAnalog.h>

// TWI
#include <TwiMaster.h>
#include <Eeprom24C512.h>
#define EEPROM_ADDRESS   0x50
#define EEPROM_MESSAGE   16
#define EEPROM_PAGE_SIZE 128
#define EEPROM_SIZE      65536 // 512kib
static Eeprom24C512 eeprom(EEPROM_ADDRESS, EEPROM_PAGE_SIZE);
// TWI RTC
#include <RTClib.h>
RTC_DS1307 RTC;
DateTime time_now, time_started;

// RS 485
#include <NilRS485.h> 
RS485_msg RX_msg, TX_msg;

// Global configuration for in chip EEPROM
#include <avr/eeprom.h>

#include <NilGSM.h>
#define Serial GSM  // redefine GSM as standard Serial 0
char sms_text[100];

// NilRTOS FIFO
#include <NilFIFO.h>
// Type for a data record.
struct log_event_t {
  char text[17];
};
NilFIFO<log_event_t, 9> log_fifo;

#define WEB_SERIAL_DEBUGGING 2
#if WEB_SERIAL_DEBUGGING 
#include <WebSerial.h>
#endif 

// Global configuration and default setting
#include "conf.h"

struct alarm_event_t {
  uint8_t zone;
  char    type;
};
NilFIFO<alarm_event_t, 3> alarm_fifo;

struct zone_t {
  systime_t last_PIR = 0;
  systime_t last_OK  = 0;
  char   last_event  = 'N';
};
zone_t zone[ALR_ZONES];

struct group_t {  
  //       |- Free / MQTT publish ???
  //       ||- Free
  //       |||- Free
  //       ||||- Free
  //       |||||- Free
  //       ||||||-  Waiting for authorization
  //       |||||||-  Alarm
  //       ||||||||-  Armed
  //      B00000000
  uint8_t setting   = 0;
  uint8_t arm_delay = 0;
};
group_t group[ALR_GROUPS];

// Dynamic sensors
struct sensor_t {
  uint8_t address;
  char    type;
  uint8_t number;
//          |- MQTT publish
//          ||- Free    
//          |||- Free
//          |||||||- Group number
//          |||||||- 0 .. 15
//          |||||||-  
//          |||||||- 
//          ||||||||-  Enabled   
//          00000000
  uint8_t setting;
  float   value;
  systime_t last_OK = 0;
};
#define SENSORS 8
sensor_t sensor[SENSORS];
volatile uint8_t sensors = 0;
NilFIFO<sensor_t, 5> sensor_fifo;

// Float conversion 
union u_tag {
  byte  b[4]; 
  float fval;
} u;

// Dynamic Authorization units
struct unit_t {
  uint8_t address;
  char    type;
  uint8_t number;
//          |- Free
//          ||- Free
//          |||- Free
//          |||||||- Group number
//          |||||||- 0 .. 15
//          |||||||-  
//          |||||||- 
//          ||||||||- Enabled   
//          00000000
  uint8_t setting;
};
#define AUTH_UNITS 8
unit_t unit[AUTH_UNITS];
volatile uint8_t units = 0;

// Triggers 
struct trigger_t {
  uint8_t address    = 0;
  char    type       = 'U';
  uint8_t number     = 0;
//          |- MQTT publish
//          ||- Alerting enabled 
//          |||- Is alerted/triggered
//          ||||- Free
//          |||||- Free
//          ||||||- Free
//          |||||||- Pass value or constant
//          ||||||||- Enabled   
//          00000000     
  uint8_t setting    = 0;
  char    symbol     = '/';
  uint8_t value      = 0;
  uint8_t constant   = 0;
  uint8_t to_address = 0;
  char    to_type    = 'U';
  uint8_t to_number  = 0;
};
#define TRIGGER_UNITS 1
trigger_t trigger[TRIGGER_UNITS];
volatile uint8_t triggers = TRIGGER_UNITS;

// Registration
struct register_t {
  char    unit;
  uint8_t address;
  char    type;
  uint8_t number;
  uint8_t setting;
};
NilFIFO<register_t, 5> unit_fifo;



// Free ticks
#define idleSlots                 10
volatile uint32_t idleCount[idleSlots];           
volatile uint8_t  idlePointer    = 0;
// Global variables
volatile uint8_t  ACState        = 0;
volatile uint16_t BatteryLevel   = 460;       // 13.8V
volatile uint8_t  OUTs           = 0;         // Output pins
volatile uint8_t  MQTTState      = 0;
volatile uint16_t radio_bad_crc  = 0;
volatile uint16_t radio_no_ack   = 0;
volatile uint32_t radio_received = 0;

char     last_key[KEY_LEN+1] = "UUUUUUUU";

char tmp[17];       // for logger 
char _tmp[4];       // for logger 

char  b64_text[30]; // encode text

// GSM modem 
uint8_t GSMisAlive = 0, GSMreg = 4, GSMstrength = 0, GSMsetSMS = 0;

// tmp
uint8_t n = 0; 

// Pins setting
DigitalPin<18> pinIN1(INPUT);
DigitalPin<19> pinIN2(INPUT);
DigitalPin<20> pinIN3(INPUT);
DigitalPin<21> pinIN4(INPUT);
DigitalPin<22> pinIN5(INPUT);
DigitalPin<23> pinTAMPER(INPUT);    // Tamper
DigitalPin<0>  pinIN7(INPUT);
DigitalPin<3>  pinAC_OFF(INPUT); // The signal turns to be "High" when the power supply turns OFF
DigitalPin<12> pinDE1(OUTPUT);
DigitalPin<14> pinOUT1(OUTPUT);
DigitalPin<15> pinOUT2(OUTPUT);
DigitalPin<13> pinBAT_OK(INPUT); // The signal is "Low" when the voltage of battery is under 11V 

// Declare and initialize a semaphore for limiting access to a region.
SEMAPHORE_DECL(ADCSem, 1);   // one slot only
SEMAPHORE_DECL(TWISem, 1);   // one slot only
SEMAPHORE_DECL(GSMSem, 1);   // one slot only
SEMAPHORE_DECL(RFMSem, 1);   // one slot only
SEMAPHORE_DECL(ETHSem, 1);   // one slot only


// *********************************************************************************
// F U N C T I O N S                F U N C T I O N S              F U N C T I O N S      
// *********************************************************************************

// Put string into a fifo log
void pushToLog(char *what){ 
  log_event_t* p = log_fifo.waitFree(TIME_IMMEDIATE); // Get a free FIFO slot.
  if (p == 0) return; // Continue if no free space.
  
  nilSemWait(&TWISem);     // wait for slot
  time_now = RTC.now();
  nilSemSignal(&TWISem);   // Exit region.
  
  p->text[0] = 0;
  strcat (p->text, time_now.timestamp());
  strcat (p->text, "?");
  strcat (p->text, what);

  log_fifo.signalData();  // Signal idle thread data is available.
}

// wait a few milliseconds for proper ACK to me, return true if indeed received
static bool waitForAck(byte theRADIO_NODEID) {
  uint8_t _counter = 0;
  do {
    _counter++;
    nilThdSleepMilliseconds(5);
  } while (!(radio.ACKReceived(theRADIO_NODEID)) && _counter < RADIO_ACK_TIME);
  if (_counter < RADIO_ACK_TIME) return true;     
  else return false;
}

// Send a command to wired RS485 unit
uint8_t sendCmd(uint8_t unit, uint8_t cmd){ 
  uint8_t _resp = 0;
  if (unit <= RADIO_UNIT_OFFSET) {
    TX_msg.address = unit;
    TX_msg.ctrl = FLAG_CMD;
    TX_msg.data_length = cmd;
    _resp = RS485.msg_write(&TX_msg);
  }
  if (unit >= RADIO_UNIT_OFFSET) { // allow command to be send to radio 0/broadcast
    char _cmd[2];
    _cmd[0] = 'C'; _cmd[1] = cmd;
    nilSemWait(&RFMSem); 
    if (unit-RADIO_UNIT_OFFSET == 0) { // radio 0/broadcast
      radio.Send(unit-RADIO_UNIT_OFFSET, _cmd, 2, false);
      _resp = 1;
    }
    else {
      radio.Send(unit-RADIO_UNIT_OFFSET, _cmd, 2, true);
      _resp = waitForAck(unit-RADIO_UNIT_OFFSET);
      if (!_resp) ++radio_no_ack;
    }
    nilSemSignal(&RFMSem);  // Exit region.
  }
  return _resp;
}

// Send data to wired RS485 unit
uint8_t sendData(uint8_t unit, char *data, uint8_t length){ 
  uint8_t _resp = 0;
  #if WEB_SERIAL_DEBUGGING 
  WS.print(F("Data to unit: ")); WS.print(unit);
  WS.print(F(", len: ")); WS.println(length);
  for (uint8_t i=0; i < length; i++){
      //WS.print((uint8_t)*data++,HEX);
    WS.print((uint8_t)data[i],HEX);
    WS.print(F(" "));
  }
  WS.println();
  #endif
  if (unit <= RADIO_UNIT_OFFSET) {
    TX_msg.address = unit;
    TX_msg.ctrl = FLAG_DTA;
    TX_msg.data_length = length;
    memcpy(TX_msg.buffer, data, length);
    return RS485.msg_write(&TX_msg);
  } else {
    nilSemWait(&RFMSem); 
    radio.Send(unit-RADIO_UNIT_OFFSET, data, length, true);
    _resp = waitForAck(unit-RADIO_UNIT_OFFSET);
    if (!_resp) ++radio_no_ack;
    nilSemSignal(&RFMSem);  // Exit region.
    return _resp;
  }
}

// Send a command to all members of a group
uint8_t sendCmdToGrp(uint8_t grp, uint8_t cmd) {
  int8_t  _resp, _try;
  uint8_t _cnt = 0;
  char _cmd[2];
  _cmd[0] = 'C'; _cmd[1] = cmd;
  // Go throug all units
  //WS.print("Grp cmd: ");
  //WS.print(cmd);
  //WS.print(", unit: ");
  for (int8_t i=0; i < units; i++){
    if (unit[i].setting & B1) {                       // Auth. unit is enabled ?
      if (((unit[i].setting >> 1) & B1111) == grp) {  // Auth. unit belong to group
        if (unit[i].address <= RADIO_UNIT_OFFSET) {
          TX_msg.address = unit[i].address;
          TX_msg.ctrl = FLAG_CMD;
          TX_msg.data_length = cmd;
          _try = 0;
          do {
            _resp = RS485.msg_write(&TX_msg);
            _try++;
            nilThdSleepMilliseconds(10);
          } while (_resp != 1 || _try < 5);
          if (_resp) _cnt++;
          //WS.print(unit[i].address); WS.print(":"); WS.print(_resp); WS.print(", ");
        } else {
          nilSemWait(&RFMSem);
          radio.Send(unit[i].address-RADIO_UNIT_OFFSET, _cmd, 2, true);
          if (waitForAck(unit[i].address-RADIO_UNIT_OFFSET)) {_cnt++;}
          else {++radio_no_ack;}
          nilSemSignal(&RFMSem);  // Exit region.
        }
      }
    }
  }
  //WS.println();
  return _cnt;
}

// Clear EEPROM log
void clearLog(){
  //nilSysLock(); // Lock RTOS  
  nilSemWait(&TWISem);     // wait for slot
  for (uint16_t i = 0; i < (EEPROM_SIZE/EEPROM_MESSAGE); i++){
    eeprom.writeBytes(i*EEPROM_MESSAGE,EEPROM_MESSAGE, "000101000000|XXX");
    nilThdSleepMilliseconds(5);  // wait for eeprom or we lose very frequent entries
  }
  conf.ee_pos = 0;
  nilSemSignal(&TWISem);   // Exit region.
  //nilSysUnlock(); // unlock RTOS
  _tmp[0] = 'S'; _tmp[1] = 'L'; _tmp[2] = 0; pushToLog(_tmp);
  eeprom_update_block((const void*)&conf, (void*)0, sizeof(conf)); // Save current configuration
}

void saveConf(){
  eeprom_update_block((const void*)&conf, (void*)0, sizeof(conf)); // Save current configuration
  _tmp[0] = 'S'; _tmp[1] = 'C'; _tmp[2] = 'W'; _tmp[3] = 0; pushToLog(_tmp);
}

unsigned long GetNTPTime(UDP &udp){
  //udpInited = udp.begin(NTP_LOCAL_PORT); // open socket on arbitrary port
  if (!udp.begin(NTP_LOCAL_PORT)) return 0; // Fail if WiFiUdp.begin() could not init a socket
  udp.flush();              // Clear received data from possible stray received packets
  
  // Send an NTP request
  if (! (udp.beginPacket(timeIP, 123)      // 123 is the NTP port
    && udp.write((byte *)&ntpFirstFourBytes, 48) == 48
    && udp.endPacket())) {
    udp.stop();
        return 0;       // sending request failed
      }

  // Wait for response
      uint8_t _pktLen, _pool;
      for (_pool=0; _pool<NTP_POLL_MAX; _pool++) {
        if ((_pktLen = udp.parsePacket()) == 48) break;
        nilThdSleepMilliseconds(NTP_POLL_INTV);
      }
      if (_pktLen != 48) {
        udp.stop();
    return 0;       // no correct packet received
  }

  // Read and discard the first useless bytes
  for (byte i = 0; i < NTP_USELESS_BYTES; ++i) {
    udp.read();
  }

  // Read the integer part of sending time
  unsigned long time  = (unsigned long)udp.read() << 24;
  time |= (unsigned long)udp.read() << 16;
  time |= (unsigned long)udp.read() << 8;
  time |= (unsigned long)udp.read();

  // Round to the nearest second if we want accuracy
  // The fractionary part is the next byte divided by 256: if it is
  // greater than 500ms we round to the next second; we also account
  // for an assumed network delay of 50ms, and (0.5-0.05)*256=115;
  // additionally, we account for how much we delayed reading the packet
  // since its arrival, which we assume on average to be pollIntv/2.
  time += (udp.read() > 115 - _pool/2);
  udp.flush();  // Discard the rest of the packet
  udp.stop();
  return time - NTP_SECS_YR_1900_2000 + NTP_TIME_ZONE * 3600; // convert NTP time to seconds after 2000
}

// MQTT Callback function
void callback(char* topic, byte* payload, unsigned int length) {
  char * pch;
  char _text[40];
  // In order to republish this payload, a copy must be made
  // as the orignal payload buffer will be overwritten whilst
  // constructing the PUBLISH packet.
  
  strncpy (_text,(char*)payload, length); _text[length] = 0;

  WS.print(topic); WS.print(":"); WS.print(_text); WS.print(":"); WS.println(length);

  // get topic
  pch = strtok (topic,"/"); //OHS
  pch = strtok (topic,"/"); //in
  while (pch != NULL){
    WS.println(pch);
    pch = strtok (NULL, "/");
  }

  //nilSemWait(&ETHSem);    // wait for slot
  //client.publish("OHS/outTopic", p, length);        
  //nilSemSignal(&ETHSem);  // Exit region.
  

}


const char PROGMEM b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz"
                                    "0123456789+/";

inline void a3_to_a4(unsigned char * a4, unsigned char * a3) {
  a4[0] =  (a3[0] & 0xfc) >> 2;
  a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
  a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
  a4[3] =  (a3[2] & 0x3f);
}

int base64_encode(char *output, char *input, uint8_t inputLen) {
  uint8_t i = 0, j = 0, encLen = 0;
  unsigned char a3[3];
  unsigned char a4[4];

  while(inputLen--) {
    a3[i++] = *(input++);
    if(i == 3) {
      a3_to_a4(a4, a3);
      for(i = 0; i < 4; i++) {
        output[encLen++] = pgm_read_byte(&b64_alphabet[a4[i]]);
      }
      i = 0;
    }
  }

  if(i) {
    for(j = i; j < 3; j++) { a3[j] = 0; }
    a3_to_a4(a4, a3);
    for(j = 0; j < i + 1; j++) {
      output[encLen++] = pgm_read_byte(&b64_alphabet[a4[j]]);
    }
    while((i++ < 3)) {
      output[encLen++] = '=';
    }
  }
  output[encLen] = 0;
  return encLen;
}


// *********************************************************************************
// W E B   P A G E S              W E B   P A G E S                W E B   P A G E S  
// *********************************************************************************

typedef enum {
  menu_Home     = 1,
  menu_Global   = 2,
  menu_Zones    = 3,
  menu_Groups   = 4,
  menu_Keys     = 5,
  menu_Contacts = 6,
  menu_Auth     = 7,
  menu_Sens     = 8,
  menu_Triggers = 9,  
  menu_MQTT     = 10,
  menu_Log      = 11,
  menu_Debug    = 12
} menu_t;

void webMenu(WebServer &server, uint8_t selected) {
  server.printP(htmlHead_s);
  server.printP(html_li_a);
  if (selected == menu_Home ) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_Home); server.printP(html_li_e_li_a);
  if (selected == menu_Global ) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_Global); server.printP(html_li_e_li_a);
  if (selected == menu_Zones ) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_Zones); server.printP(html_li_e_li_a);
  if (selected == menu_Groups ) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_Groups); server.printP(html_li_e_li_a);
  if (selected == menu_Keys ) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_Keys); server.printP(html_li_e_li_a);
  if (selected == menu_Contacts ) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_Contacts); server.printP(html_li_e_li_a);
  if (selected == menu_Auth ) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_Auth); server.printP(html_li_e_li_a);
  if (selected == menu_Sens ) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_Sens); server.printP(html_li_e_li_a);
  if (selected == menu_Triggers ) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_Triggers); server.printP(html_li_e_li_a);
  if (selected == menu_MQTT ) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_MQTT); server.printP(html_li_e_li_a);
  if (selected == menu_Log) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_Log); server.printP(html_li_e_li_a);
  if (selected == menu_Debug) { server.printP(html_c_active); }
  server.printP(html_href); server.printP(html_menu_Debug); server.printP(html_li_e);
  server.printP(htmlHead_e);
}

#if WEB_SERIAL_DEBUGGING 
void webDebug(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    if (type == WebServer::POST) {
      // no post
    } else {  
      uint8_t data;
      server.httpSuccess();
      webMenu(server, menu_Debug);
      
      WS.resetRead(); // show all in buffer
      server.printP(html_h1); server.printP(text_Debug); server.printP(html_e_h1); 
      for (uint8_t i = 0; i < idleSlots; ++i){
        if (i == idlePointer) server << "<B>" << idleCount[i] << "</B>"; 
        else                  server << idleCount[i];
        server.printP(text_spdashsp);
      }
      server << "Conf size: " << sizeof(conf) << "</br>";
       
      server.printP(html_pre);
      while (WS.isAvailable()) {
        data = WS.read();
        server << (char)data;
      }
      server.printP(html_e_pre); 
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}
#endif

void webHome(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    if (type == WebServer::POST) {
      bool repeat;
      char name[2], value[16];
      do {
        repeat = server.readPOSTparam(name, 2, value, 16);
        switch(name[0]){
          case 'T': // NTP Sync
            time_now = GetNTPTime(udp);  
            if (time_now.get() > 0) RTC.adjust(time_now.get());
          break;
          case 'e': saveConf(); break; // save to EEPROM
          case 'l': // load from EEPROM
            _tmp[0] = 'S'; _tmp[1] = 'C'; _tmp[2] = 'L'; _tmp[3] = 0; pushToLog(_tmp);
            //pushPToLogP(log_SCL);
            //strcpy_P(_tmp, (const char*) log_SCL); pushToLog();
            eeprom_read_block((void*)&conf, (void*)0, sizeof(conf)); // Read current configuration
          break; 
          case 'r': // reset ro default
            _tmp[0] = 'S'; _tmp[1] = 'C'; _tmp[2] = 'R'; _tmp[3] = 0; pushToLog(_tmp);
            //strcpy_P(_tmp, (const char*) log_SCR); pushToLog();
            //pushPToLogP(log_SCR);
            setDefault();
          break; 
          }
      } while (repeat);
      server.httpSeeOther(PREFIX "/");
    } else {   
      uint32_t voltage = (BatteryLevel * 3222581) / 1071429; // voltage divider, and avoiding float
      if (voltage < 100) voltage = 0; // no sense to messuer lower voltage
      char _tmp_itoa[5]; itoa(voltage, _tmp_itoa, 10); 
      nilSemWait(&TWISem);     // wait for slot
      time_now = RTC.now();
      nilSemSignal(&TWISem);   // Exit region.

      server.httpSuccess();
      webMenu(server, menu_Home);
      server.printP(html_h1); server.printP(text_System); server.printP(html_e_h1);  
      server.printP(html_table_tr_td); 
      server.printP(text_Time); server.printP(html_e_td_td);
      server.print((char*)time_now.formatedDateTime());
      server.printP(html_e_td_e_tr);

      server.printP(html_tr_td);
      server.printP(text_Started); server.printP(html_e_td_td);
      server.print((char*)time_started.formatedDateTime());        
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Uptime); server.printP(html_e_td_td);
      time_now = (time_now.get()-time_started.get());
      server.print((char*)time_now.formatedUpTime());
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_PwrSp); server.printP(html_e_td_td);
      pinAC_OFF.read() ? server.printP(text_i_disabled) : server.printP(text_i_OK); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Battery); server.printP(html_e_td_td);
      pinBAT_OK.read() ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Voltage); server.printP(html_e_td_td); 
      if (voltage > 1000) { server << _tmp_itoa[0] << _tmp_itoa[1] << "." << _tmp_itoa[2] << _tmp_itoa[3];}
      else                { server << _tmp_itoa[0] << "." << _tmp_itoa[1] << _tmp_itoa[2] << _tmp_itoa[3];}
      server.printP(html_e_td_e_tr);
      server.printP(html_e_table);
      
      server.printP(html_h1); server.printP(text_Radio); server.printP(html_e_h1);  
      server.printP(html_table_tr_td);
      server.printP(text_Received); server.printP(text_space); server.printP(text_packets); server.printP(html_e_td_td);
      server << radio_received; server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Failed); server.printP(text_space); server.printP(text_packets); server.printP(html_e_td_td);
      server << radio_bad_crc; server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Not); server.printP(text_space); server.printP(text_acknowledged); server.printP(text_space); server.printP(text_packets); server.printP(html_e_td_td);
      server << radio_no_ack; server.printP(html_e_td_e_tr);
      server.printP(html_e_table);

      server.printP(html_h1); server.printP(text_GSM); server.printP(text_space); server.printP(text_modem); server.printP(html_e_h1);  
      server.printP(html_table_tr_td);
      server.printP(text_GSM); server.printP(text_space); server.printP(text_modem); server.printP(text_space); server.printP(text_is); server.printP(text_space); server.printP(text_connected); 
      server.printP(html_e_td_td);
      GSMisAlive ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_e_tr);
      
      server.printP(html_tr_td);
      server.printP(text_GSM); server.printP(text_space); server.printP(text_network); server.printP(text_space); server.printP(text_is);
      server.printP(html_e_td_td);
      switch(GSMreg){
        case 0 : server.printP(text_nr); break;
        case 1 : server.printP(text_i_home); break;
        case 2 : server.printP(text_i_starting); break;
        case 3 : server.printP(text_rd); break;
        // case 4 : server.printP(text_unk); break;
        case 5 : server.printP(text_rr); break;
        default : server.printP(text_i_question);; break;
      }
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Signal); server.printP(text_space); server.printP(text_strength);
      server.printP(html_e_td_td);
      server << (GSMstrength*3); server.printP(text_percent); 
      server.printP(html_e_td_e_tr);
      server.printP(html_e_table);
      
      server.printP(html_form_s); server << PREFIX "/"; server.printP(html_form_e);
      server.printP(html_F_GetNTP);
      server.printP(html_F_LA); // submit Load all
      server.printP(html_F_SA); // submit Save all
      server.printP(html_F_RD); // submit Save all
      server.printP(html_e_form);
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}

uint16_t ses_eeprom_add = 0;
void webListLog(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    if (type == WebServer::POST) {
      bool repeat;
      char name[2], value[16];
      do {
        repeat = server.readPOSTparam(name, 2, value, 16);
        if (name[0] == 'p') ses_eeprom_add -= 320;
        if (name[0] == 'f') ses_eeprom_add += 320;
        if (name[0] == 'n') ses_eeprom_add = conf.ee_pos - 336 ;
        if (name[0] == 'C') clearLog();
      } while (repeat);
      server.httpSeeOther(PREFIX "/l");
    } else {  
      server.httpSuccess();
      webMenu(server, menu_Log);
      server.printP(html_h1); server.printP(text_Log); server.printP(html_e_h1); 
      server.printP(html_form_s); server << PREFIX "/l"; server.printP(html_form_e);
      server.printP(html_F_Clear); // Clear
      server.printP(html_F_CL);
       
      server.printP(html_table_tr_th);
      server.printP(text_hash);
      server.printP(html_e_th_th); server.printP(text_Date); 
      server.printP(html_e_th_th); server.printP(text_Message);
      server.printP(html_e_th_th); server.printP(text_SMS);
      server.printP(html_e_th_e_tr);
      for (uint8_t i = 0; i < 21; ++i){
        server.printP(html_tr_td);
        // Get data from extrenal EEPROM
        nilSemWait(&TWISem);     // wait for slot
        eeprom.readBytes((uint16_t)(ses_eeprom_add + (i*EEPROM_MESSAGE)),EEPROM_MESSAGE, tmp);
        //    nilThdSleepMilliseconds(10); //??? wait for eeprom
        nilSemSignal(&TWISem);   // Exit region.
        if ((uint16_t)(ses_eeprom_add/EEPROM_MESSAGE + i) > (EEPROM_SIZE/EEPROM_MESSAGE) - 1) 
         server << (uint16_t)(ses_eeprom_add/EEPROM_MESSAGE + i - (EEPROM_SIZE/EEPROM_MESSAGE) + 1); 
       else server << (uint16_t)(ses_eeprom_add/EEPROM_MESSAGE + i + 1);
       server.printP(text_dot);
       server.printP(html_e_td_td);
        // print date and time
       server << tmp[4] << tmp[5]; server.printP(text_dot);
       server << tmp[2] << tmp[3]; server.printP(text_dot);
       server << "20" << tmp[0] << tmp[1]; server.printP(text_space);
       server << tmp[6] << tmp[7]; server.printP(text_semic);
       server << tmp[8] << tmp[9]; server.printP(text_semic);
       server << tmp[10] << tmp[11]; server.printP(text_space);
       server.printP(html_e_td_td);
       switch(tmp[13]){
          case 'S': // System
            server.printP(text_System); server.printP(text_sesp);
            switch(tmp[14]){
              case 'B': // Battery
                server.printP(text_BR);
                if (tmp[15] == 'L') server.printP(text_low);
                else server.printP(text_high);
                server.printP(text_space); server.printP(text_level);
              break;
              case 'A': // AC
                server.printP(text_PWS);
                if (tmp[15] == 'L') server.printP(text_On);
                else server.printP(text_Off);
              break;
              case 'C': server.printP(text_Configuration); server.printP(text_space);
                switch(tmp[15]){
                  case 'W': server.printP(text_saved); break; // conf. saved
                  case 'P': server.printP(text_saved); server.printP(text_cosp); server.printP(text_monitoring);
                    server.printP(text_space); server.printP(text_disabled);break; // conf. saved
                  case 'L': server.printP(text_loaded); break; // conf. saved
                  case 'R': server.printP(text_reset); break; // conf. saved
                  default:  server.printP(text_undefined); break; // unknown  
                }
              break;
              case 'Z': server.printP(text_group); server.printP(text_space);
              server << tmp[15]-48+1; server.printP(text_spdashsp); server << conf.group_name[tmp[15]-48];
              server.printP(text_space); server.printP(text_monitoring); server.printP(text_space); server.printP(text_started);
                        break; // system armed
              case 'S': server.printP(text_Monitoring); server.printP(text_space); server.printP(text_started); break; // monitoring strted
              case 's': server.printP(text_started); break;   // boot
              case 'X': server.printP(text_ALARM); break;     // alarm
              case 'M': 
                server.printP(text_MQTT); server.printP(text_space);
                switch(tmp[15]){
                  case 'F': server.printP(text_network); server.printP(text_space); server.printP(text_failed); break;
                  case 'O': server.printP(text_network); server.printP(text_space); server.printP(text_OK); break;
                  default:  server.printP(text_undefined); break; // unknown  
                }
              break;
              case 'L': server.printP(text_Log); server.printP(text_space); server.printP(text_erased);break;     // Log cleared
              default:  server.printP(text_undefined); break; // unknown
            }
          break;
          case 'P': // PIR
            server.printP(text_Alarm); server.printP(text_sesp); server.printP(text_triggered); server.printP(text_space); server.printP(text_zone); server.printP(text_space);
            server << conf.zone_name[tmp[14]-48];
          break;
          case 'T': // Tamper
            server.printP(text_Tamper); server.printP(text_sesp); server.printP(text_triggered); server.printP(text_space); server.printP(text_zone); server.printP(text_space);
            server << conf.zone_name[tmp[14]-48];
          break;
          case 'A': // Authentication
            server.printP(text_Authentication); server.printP(text_sesp); 
            if (tmp[14] <  'a') { server.printP(text_key); server.printP(text_space); }
            if (tmp[14] == 'A' || tmp[14] == 'D' || tmp[14] == 'F') { server << conf.key_name[tmp[15]-48]; server.printP(text_space); }
            switch(tmp[14]){
              case 'D': server.printP(text_disarmed); break;
              case 'A': server.printP(text_armed); break;
              case 'U': server.printP(text_undefined); break;
              case 'F': server.printP(text_is); server.printP(text_space); server.printP(text_disabled); break;
              case 'a': server.printP(text_auto); server.printP(text_space); server.printP(text_armed);
              server.printP(text_space); server.printP(text_group); server.printP(text_space); server << conf.zone_name[tmp[15]-48];
              break;
              case 'o': server.printP(text_zone); server.printP(text_space); server << conf.zone_name[tmp[15]-48];
              server.printP(text_space); server.printP(text_is); server.printP(text_space); server.printP(text_open);
              break;
              default: break;
            }
              /*
              if (tmp[15] != 'F') { // print no info when unknown key
                server.printP(text_space);  server.printP(text_group); server.printP(text_space); server.print(conf.group_name[tmp[16]-48]);
              }
              */
          break;    
          case 'M': //GSM modem
            server.printP(text_GSM); server.printP(text_space); server.printP(text_network); server.printP(text_sesp);
            switch(tmp[14]){
              case '0' : server.printP(text_nr); break;
              case '1' : server.printP(text_rh); break;
              case '2' : server.printP(text_nrs); break;
              case '3' : server.printP(text_rd); break;
                // case 4 : server.printP(text_unk); break;
              case '5' : server.printP(text_rr); break;
              default : server.printP(text_unk); break;
            }
            server.printP(text_cosp); server.printP(text_strength); server.printP(text_space);
            server << (tmp[15]-48)*3 << "%";
          break;
          case 'U': // remote unit
            if (tmp[14] == 'E') { 
              server.printP(text_Unit); server.printP(text_space); server.printP(text_registration);
              server.printP(text_space); server.printP(text_error);
              break;
            }
            server.printP(text_Remote); server.printP(text_space); server.printP(text_unit); server.printP(text_sesp);
            server.printP(text_address); server.printP(text_space); server << tmp[15]-48; server.printP(text_space);
            switch(tmp[14]){
              case 'F' : server.printP(text_is); server.printP(text_space); server.printP(text_disabled); break;
              case 'R' : server.printP(text_registered); break;
              case 'r' : server.printP(text_re); server.printP(text_registered); break;
              default : server.printP(text_unk); break;
            }
          break;
          case 'a' ... 'x': // remote sensors
            switch(tmp[13]){
              case 't': server.printP(text_Temperature); break;
              case 'h': server.printP(text_Humidity); break;
              case 'p': server.printP(text_Pressure); break;
              default : server.printP(text_unk); break;
            }
            server.printP(text_space); server.printP(text_sensor); server.printP(text_sesp);
            server.printP(text_address); server.printP(text_space); server << tmp[15]-48; server.printP(text_space);
            switch(tmp[14]){
              case 'F' : server.printP(text_is); server.printP(text_space); server.printP(text_disabled); break;
              case 'R' : server.printP(text_registered); break;
              case 'r' : server.printP(text_re); server.printP(text_registered); break;
              default : server.printP(text_unk); break;
            }
          break;
          case 'G': // Groups
            server.printP(text_Group); server.printP(text_sesp);
            server << tmp[15]-48+1; server.printP(text_spdashsp); server << conf.group_name[tmp[15]-48]; server.printP(text_space);
            switch(tmp[14]){
              case 'F' : server.printP(text_is); server.printP(text_space); server.printP(text_disabled); break;
              default : server.printP(text_unk); break;
            }
          break;
          case 'F': // Fifo
            server.printP(text_Fifo); server.printP(text_space);          
            switch(tmp[14]){
              case 'A' : server.printP(text_alarm); break;
              case 'S' : server.printP(text_sensor); break;
              case 'R' : server.printP(text_registration); break;
              default : server.printP(text_unk); break;
            }
            server.printP(text_space); server.printP(text_is); server.printP(text_space); server.printP(text_full);
          break;
          default:
            server.printP(text_Undefined); server.printP(text_sesp);
            server << tmp;
          break;
        }
        server.printP(text_dot);
        server.printP(html_e_td_td);
        switch(tmp[12]){
          case '.': server.printP(text_requested); server.printP(text_sesp); server.printP(text_failed); break;
          case ',': server.printP(text_not); server.printP(text_space); server.printP(text_requested); server.printP(text_cosp); server.printP(text_failed); break;
          case ';': server.printP(text_not); server.printP(text_space); server.printP(text_requested); server.printP(text_cosp); server.printP(text_acknowledged); break;
          case '|': server.printP(text_not); server.printP(text_space); server.printP(text_connected); break;
          default:  server.printP(text_sent); break;
        }
        server.printP(html_e_td_e_tr);
      }
      server.printP(html_e_table);
      server.printP(html_F_LOG); // buttons << NOW >>
      server.printP(html_e_form); 
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}

uint8_t webZone = 0;
void webSetZone(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    int16_t val = 0;
    if (type == WebServer::POST) {
      bool repeat;
      char name[2], value[16];
      do {
        repeat = server.readPOSTparam(name, 2, value, 16);
        //y = (x >> n) & 1;    // n=0..15.  stores nth bit of x in y.  y becomes 0 or 1.
        //x &= ~(1 << n);      // forces nth bit of x to be 0.  all other bits left alone.
        //x &= (1<<(n+1))-1;   // leaves alone the lowest n bits of x; all higher bits set to 0.
        //x |= (1 << n);       // forces nth bit of x to be 1.  all other bits left alone.
        //x ^= (1 << n);       // toggles nth bit of x.  all other bits left alone.
        //x = ~x;              // toggles ALL the bits in x.
        switch(name[0]){
          case 'Z':
            n = strtol(value, NULL, 10);
            if (n != webZone) {
              webZone = n;
              repeat = 0;
            }
          break;
          case 'n': // zone name
            strncpy (conf.zone_name[webZone], value, sizeof(value));
            conf.zone_name[webZone][15] = 0;
          break;
          case 'o': // enable zone
            if (value[0] == '0') conf.zone[webZone] &= ~(1 << 0);
            else conf.zone[webZone] |= (1 << 0);
          break;
          case 'a': // auto arm
            if (value[0] == '0') conf.zone[webZone] &= ~(1 << 7);
            else conf.zone[webZone] |= (1 << 7);
          break;
          case 's': // still open alarm
            if (value[0] == '0') conf.zone[webZone] &= ~(1 << 8);
            else conf.zone[webZone] |= (1 << 8);
          break;
          case 'd': // auth. delay
            n = value[0] - 48;
            if ((n >> 0) & 1) conf.zone[webZone] |= (1 << 5);
            else conf.zone[webZone] &= ~(1 << 5);
            if ((n >> 1) & 1) conf.zone[webZone] |= (1 << 6);
            else conf.zone[webZone] &= ~(1 << 6);
          break;
          case 'g': // group
            n = strtol(value, NULL, 10);
            if ((n >> 0) & 1) conf.zone[webZone] |= (1 << 1);
            else conf.zone[webZone] &= ~(1 << 1);
            if ((n >> 1) & 1) conf.zone[webZone] |= (1 << 2);
            else conf.zone[webZone] &= ~(1 << 2);
            if ((n >> 2) & 1) conf.zone[webZone] |= (1 << 3);
            else conf.zone[webZone] &= ~(1 << 3);
            if ((n >> 3) & 1) conf.zone[webZone] |= (1 << 4);
            else conf.zone[webZone] &= ~(1 << 4);
          break;
          case 'e': saveConf(); break; // save to EEPROM
          }
        } while (repeat);
        server.httpSeeOther(PREFIX "/z");
      } else {
        server.httpSuccess();
        webMenu(server, menu_Zones);
        server.printP(html_h1); server.printP(text_Zone); server.printP(text_space); server.printP(text_setup); server.printP(html_e_h1); 
        server.printP(html_form_s); server << PREFIX "/z"; server.printP(html_form_e);

        server.printP(html_table_tr_th);
        server.printP(text_hash);
        server.printP(html_e_th_th); server.printP(text_Type);
        server.printP(html_e_th_th); server.printP(text_Name); 
        server.printP(html_e_th_th); server.printP(text_Enabled);
        server.printP(html_e_th_th); server.printP(text_Auto); server.printP(text_space); server.printP(text_arm);
        server.printP(html_e_th_th); server.printP(text_Open); server.printP(text_space); server.printP(text_alarm);
        server.printP(html_e_th_th); server.printP(text_Delay);
        server.printP(html_e_th_th); server.printP(text_Group);
        server.printP(html_e_th_th); server.printP(text_Last); server.printP(text_space); server.printP(text_alarm);
        server.printP(html_e_th_th); server.printP(text_Last); server.printP(text_space); server.printP(text_OK);
        server.printP(html_e_th_th); server.printP(text_Status);
        server.printP(html_e_th_e_tr);
        for (uint8_t i = 0; i < ALR_ZONES; ++i) {
          server.printP(html_tr_td); 
          server << i+1; server.printP(text_dot); server.printP(html_e_td_td);
          (conf.zone[i] >> 15) ? server.printP(text_analog) : server.printP(text_digital); server.printP(html_e_td_td);
          server << conf.zone_name[i]; server.printP(html_e_td_td);
          (conf.zone[i] & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
          ((conf.zone[i] >> 7) & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
          ((conf.zone[i] >> 8) & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
          server << (((conf.zone[i] >> 5) & B11)*conf.alr_time); server.printP(text_space); server.printP(text_sec); server.printP(html_e_td_td);
          if ((conf.zone[i] & B1)) { server << ((conf.zone[i] >> 1) & B1111) + 1; server.printP(text_spdashsp); server << conf.group_name[((conf.zone[i] >> 1) & B1111)]; }
          else                     { server.printP(text_spdashsp); }        
          server.printP(html_e_td_td);
          time_now = (nilTimeNow() - zone[i].last_PIR)/NIL_CFG_FREQUENCY;
          if ((conf.zone[i] & B1)) { server.print((char*)time_now.formatedUpTime()); }
          else                     { server.printP(text_spdashsp); }
          time_now = (nilTimeNow() - zone[i].last_OK)/NIL_CFG_FREQUENCY;
          server.printP(html_e_td_td);
          if ((conf.zone[i] & B1)) { server.print((char*)time_now.formatedUpTime()); }
          else                     { server.printP(text_spdashsp); }
          server.printP(html_e_td_td);
          if ((conf.zone[i] & B1)) {
            switch(zone[i].last_event){
              case 'O': server.printP(text_i_OK); break;
              case 'P': server.printP(text_i_ALARM); break;
              case 'N': server.printP(text_i_starting); break;
              default: server.printP(text_tamper); break;
            }
          } else { server.printP(text_i_disabled); } // disabled
          server.printP(html_e_td_e_tr);      
        }
        server.printP(html_e_table); 

        server.printP(html_form_s); server << PREFIX "/zone"; server.printP(html_form_e);
        server.printP(html_table_tr_td);
        server.printP(text_Zone);server.printP(html_e_td_td);
        server.printP(html_select_submit); server << "Z"; server.printP(html_e_tag);
        for (uint8_t ii = 0; ii < ALR_ZONES; ++ii) {
          server.printP(html_option); server << ii;
          if (webZone == ii) { server.printP(html_selected); }
          else               { server.printP(html_e_tag); }
          server << ii + 1; server.printP(text_spdashsp); server << conf.zone_name[ii]; server.printP(html_e_option); 
        }
        server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Type); server.printP(html_e_td_td);
        ((conf.zone[webZone] >> 15) & 1) ? server.printP(text_analog) : server.printP(text_digital); server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Name); server.printP(html_e_td_td);
        server.printP(html_s_tag); server << "n"; server.printP(html_m_tag); server << conf.zone_name[webZone]; server.printP(html_e_tag);
        server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Zone); server.printP(text_space); server.printP(text_is); server.printP(html_e_td_td);
        server.printP(html_radio_s);
        server.radioButton("o", text_1, text_On, conf.zone[webZone] & B1);
        server.radioButton("o", text_0, text_Off, !(conf.zone[webZone] & B1));
        server.printP(html_div_e); server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Auto); server.printP(text_space); server.printP(text_arm); server.printP(html_e_td_td);
        server.printP(html_radio_s);
        server.radioButton("a", text_1, text_On, conf.zone[webZone] >> 7 & B1);
        server.radioButton("a", text_0, text_Off, !(conf.zone[webZone] >> 7 & B1));
        server.printP(html_div_e); server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Open); server.printP(text_space); server.printP(text_alarm); server.printP(html_e_td_td);
        server.printP(html_radio_s);
        server.radioButton("s", text_1, text_On, conf.zone[webZone] >> 8 & B1);
        server.radioButton("s", text_0, text_Off, !(conf.zone[webZone] >> 8 & B1));
        server.printP(html_div_e); server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Authentication); server.printP(text_space); server.printP(text_delay); 
        server.printP(text_space); server.printP(text_x); server << conf.alr_time; server.printP(text_space); server.printP(text_sec); server.printP(html_e_td_td);
        server.printP(html_radio_sl);
        server.radioButton("d", text_0, text_0, !((conf.zone[webZone] >> 6 & B1) & (conf.zone[webZone] >> 5 & B1)));
        server.radioButton("d", text_1, text_1, (!(conf.zone[webZone] >> 6 & B1) & (conf.zone[webZone] >> 5 & B1)));
        server.radioButton("d", text_2, text_2, ((conf.zone[webZone] >> 6 & B1) & !(conf.zone[webZone] >> 5 & B1)));
        server.radioButton("d", text_3, text_3, ((conf.zone[webZone] >> 6 & B1) & (conf.zone[webZone] >> 5 & B1)));
        server.printP(html_div_e); server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Group); server.printP(html_e_td_td);
        server.printP(html_select); server << "g"; server.printP(html_e_tag);
        for (uint8_t ii = 0; ii < ALR_GROUPS; ++ii) {
          server.printP(html_option); server << ii;
          if ((conf.zone[webZone] >> 1 & B1111) == ii) { server.printP(html_selected); }
          else                                         { server.printP(html_e_tag); }
          server << ii + 1; server.printP(text_spdashsp); server << conf.group_name[ii]; server.printP(text_spdashsp); (conf.group[ii] & B1) ? server.printP(text_On) : server.printP(text_Off); server.printP(html_e_option);
        }
        server.printP(html_e_select);
        server.printP(html_e_td_e_tr);
        server.printP(html_e_table);
          
      server.printP(html_F_A); // submit Apply
      server.printP(html_F_SA); // submit Save all
      server.printP(html_e_form);
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}

uint8_t webGroup = 0;
void webSetGroup(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    if (type == WebServer::POST) {
      bool repeat;
      char name[2], value[16];
      do {
        repeat = server.readPOSTparam(name, 2, value, 16);
        switch(name[0]){
          case 'G':
            n = strtol(value, NULL, 10);
            if (n != webGroup) {
              webGroup = n;
              repeat = 0;
            }
          break;
          case 'n': // zone name
            strncpy (conf.group_name[webGroup], value, sizeof(value));
            conf.group_name[webGroup][15] = 0;
          break;
          case 'o': // enable group
            if (value[0] == '0') conf.group[webGroup] &= ~(1 << 0);
            else conf.group[webGroup] |= (1 << 0);
          break;      
          case 'p': // PIR OUT1
            if (value[0] == '0') conf.group[webGroup] &= ~(1 << 4);
            else conf.group[webGroup] |= (1 << 4);
          break;
          case 'q': // PIR OUT2
            if (value[0] == '0') conf.group[webGroup] &= ~(1 << 3);
            else conf.group[webGroup] |= (1 << 3);
          break;
          case 't': // Tamper OUT1
            if (value[0] == '0') conf.group[webGroup] &= ~(1 << 2);
            else conf.group[webGroup] |= (1 << 2);
          break;
          case 'y': // Tamper OUT2
            if (value[0] == '0') conf.group[webGroup] &= ~(1 << 1);
            else conf.group[webGroup] |= (1 << 1);
          break;
          case 'e': saveConf(); break; // save to EEPROM         
          }
        } while (repeat);
        server.httpSeeOther(PREFIX "/g");
    } else {
      server.httpSuccess();
      webMenu(server, menu_Groups);
      server.printP(html_h1); server.printP(text_Group); server.printP(text_space); server.printP(text_setup); server.printP(html_e_h1);  
      server.printP(html_table_tr_th);
      server.printP(text_hash);
      server.printP(html_e_th_th); server.printP(text_Name); 
      server.printP(html_e_th_th); server.printP(text_Enabled);
      server.printP(html_e_th_th); server.printP(text_Armed);
      server.printP(html_e_th_th); server.printP(text_Authentication);
      server.printP(html_e_th_th); server.printP(text_Delay);
      server.printP(html_e_th_th); server.printP(text_i_zone); server.printP(text_space); server.printP(text_Zone); server.printP(text_s);
      server.printP(html_e_th_th); server.printP(text_i_auth); server.printP(text_space);server.printP(text_Authentication); server.printP(text_space); server.printP(text_unit); server.printP(text_s);
      server.printP(html_e_th_th); server.printP(text_i_sens); server.printP(text_space);server.printP(text_Sensor); server.printP(text_s);
      server.printP(html_e_th_th); server.printP(text_i_phone); server.printP(text_space);server.printP(text_Phone); server.printP(text_s);
      server.printP(html_e_th_th); server.printP(text_Alarm);
      server.printP(html_e_th_th); server.printP(text_Tamper);
      server.printP(html_e_th_th); server.printP(text_Status);
      server.printP(html_e_th_e_tr);
      for (uint8_t i = 0; i < ALR_GROUPS; ++i) {
        server.printP(html_tr_td); 
        server << i+1; server.printP(text_dot); server.printP(html_e_td_td);
        server << conf.group_name[i]; server.printP(html_e_td_td);
        (conf.group[i] & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
        (group[i].setting & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
        ((group[i].setting >> 2) & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
        server << group[i].arm_delay/4; server.printP(text_space); server.printP(text_sec); server.printP(html_e_td_td);
        n = 0;
        for (uint8_t ii = 0; ii < ALR_ZONES; ++ii) {
          if ((((conf.zone[ii] >> 1) & B1111) == i) && (conf.zone[ii] & B1)) {
            if (n) { server.printP(text_cosp);}
            server << ii+1; n = 1;
          }
        }
        server.printP(html_e_td_td);
        n = 0;
        for (uint8_t ii = 0; ii < units; ++ii) {      
          if ((((unit[ii].setting >> 1) & B1111) == i) && (unit[ii].setting & B1)) { 
            if (n) { server.printP(text_cosp); }
            server << unit[ii].type; server.printP(text_semic);
            if (unit[ii].address < RADIO_UNIT_OFFSET) { server << "W:" << unit[ii].address; }
            else                                      { server << "R:" << unit[ii].address-RADIO_UNIT_OFFSET; }
            server.printP(text_semic); server << unit[ii].number; n = 1;
          }
        }
        server.printP(html_e_td_td);
        n = 0;
        for (uint8_t ii = 0; ii < SENSORS; ++ii) {      
          if (((sensor[ii].setting >> 1) & B1111) == i) { 
            if (sensor[ii].address && (sensor[ii].setting & B1)) {
              if (n) { server.printP(text_cosp); }
              server << sensor[ii].type; server.printP(text_semic);
              if (sensor[ii].address < RADIO_UNIT_OFFSET) { server << "W:" << sensor[ii].address; }
              else                                        { server << "R:" << sensor[ii].address-RADIO_UNIT_OFFSET; }
              server.printP(text_semic); server << sensor[ii].number;
              n = 1;
            }
          }
        }
        server.printP(html_e_td_td);
        n = 0;
        for (uint8_t ii = 0; ii < NUM_OF_PHONES; ++ii) {
          if (((((conf.tel[ii] >> 1) & B1111) == i) || (conf.tel[ii] >> 5 & B1)) && (conf.tel[ii] & B1)){
            if (n) { server.printP(text_cosp); }
            server << conf.tel_name[ii];
            n = 1;
          }
        }
        server.printP(html_e_td_td);        
        if ((conf.group[i] >> 4) & B1) { server.printP(text_OUT1); server.printP(text_space); }
        if ((conf.group[i] >> 3) & B1) server.printP(text_OUT2);
        server.printP(html_e_td_td);
        if ((conf.group[i] >> 2) & B1) { server.printP(text_OUT1); server.printP(text_space); }
        if ((conf.group[i] >> 1) & B1) server.printP(text_OUT2);
        server.printP(html_e_td_td);

        if (conf.group[i] & B1) {
          (group[i].setting >> 1 & B1) ? server.printP(text_i_ALARM) : server.printP(text_i_OK);
        } else { server.printP(text_i_disabled); }
        server.printP(html_e_td_e_tr);
      }
      server.printP(html_e_table); 
      server.printP(html_form_s); server << PREFIX "/g"; server.printP(html_form_e);
      
      
      server.printP(html_table_tr_td);
      server.printP(text_Group); server.printP(html_e_td_td);
      server.printP(html_select); server << "G"; server.printP(html_e_tag);
      for (uint8_t ii = 0; ii < ALR_GROUPS; ++ii) {
        if (webGroup == ii) 
          { server.printP(html_option); server << ii; server.printP(html_selected); server << ii + 1; server.printP(text_spdashsp); server << conf.group_name[ii]; server.printP(html_e_option); }
        else 
          { server.printP(html_option); server << ii; server.printP(html_e_tag); server << ii + 1; server.printP(text_spdashsp); server << conf.group_name[ii]; server.printP(html_e_option);}
      }
      server.printP(html_e_select);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Name); server.printP(html_e_td_td);
      server.printP(html_s_tag); server << "n"; server.printP(html_m_tag); server << conf.group_name[webGroup]; server.printP(html_e_tag);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Group); server.printP(text_space); server.printP(text_is); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("o", text_1, text_On, conf.group[webGroup] & B1);
      server.radioButton("o", text_0, text_Off, !(conf.group[webGroup] & B1));
      server.printP(html_div_e);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Alarm); server.printP(text_space); server.printP(text_trigger); server.printP(text_s); server.printP(text_space); server.printP(text_OUT1); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("p", text_1, text_On, conf.group[webGroup] >> 4 & B1);
      server.radioButton("p", text_0, text_Off, !(conf.group[webGroup] >> 4 & B1));
      server.printP(html_div_e);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Alarm); server.printP(text_space); server.printP(text_trigger); server.printP(text_s); server.printP(text_space); server.printP(text_OUT2); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("q", text_1, text_On, conf.group[webGroup] >> 3 & B1);
      server.radioButton("q", text_0, text_Off, !(conf.group[webGroup] >> 3 & B1));
      server.printP(html_div_e);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Tamper); server.printP(text_space); server.printP(text_trigger); server.printP(text_s); server.printP(text_space); server.printP(text_OUT1); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("t", text_1, text_On, conf.group[webGroup] >> 2 & B1);
      server.radioButton("t", text_0, text_Off, !(conf.group[webGroup] >> 2 & B1));
      server.printP(html_div_e);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Tamper); server.printP(text_space); server.printP(text_trigger); server.printP(text_s); server.printP(text_space); server.printP(text_OUT2); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("y", text_1, text_On, conf.group[webGroup] >> 1 & B1);
      server.radioButton("y", text_0, text_Off, !(conf.group[webGroup] >> 1 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_e_table);
          
      server.printP(html_F_A); // submit Apply
      server.printP(html_F_SA); // submit Save all
      server.printP(html_e_form);
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}

void formatKey(char* in, char* out) { // Format the key to nice HEX string
  for (uint8_t ii = 0; ii < KEY_LEN; ++ii) { 
    out[ii*2]   = in[ii] >> 4 & B1111;
    out[ii*2+1] = in[ii] & B1111;
    if (out[ii*2] > 9) out[ii*2] = out[ii*2] + 'A' - 10; 
    else out[ii*2] = out[ii*2] + '0';
    if (out[ii*2+1] > 9) out[ii*2+1] = out[ii*2+1] + 'A' - 10;
    else out[ii*2+1] = out[ii*2+1] + '0';
  }
  out[16] = 0;
}

uint8_t webKey = 0;
void webSetKey(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    if (type == WebServer::POST) {
      bool repeat;
      char name[2], value[17];
      do {
        repeat = server.readPOSTparam(name, 2, value, 17);
        switch(name[0]){
          case 'K':
            n = strtol(value, NULL, 10);
            if (n != webKey) {
              webKey = n;
              repeat = 0;
            }
          break;
          case 'k': // key
            for (uint8_t ii = 0; ii < KEY_LEN; ++ii) {
              if (value[ii*2] > '9') value[ii*2] = value[ii*2] - 'A' + 10;
              else value[ii*2] = value[ii*2] - '0';
              if (value[ii*2+1] > '9') value[ii*2+1] = value[ii*2+1] - 'A' + 10;
              else value[ii*2+1] = value[ii*2+1] - '0';
              conf.key[webKey][ii] = value[ii*2] << 4;
              conf.key[webKey][ii] = conf.key[webKey][ii] + value[ii*2+1];
            } 
            conf.key[webKey][8] = 0;
          break;
          case 'n': // key
            strncpy (conf.key_name[webKey], value, 16);
            conf.key_name[webKey][15] = 0;
          break;
          case 'e': saveConf(); break; // save to EEPROM          
          }
        } while (repeat);
        server.httpSeeOther(PREFIX "/k");
    } else {
      server.httpSuccess();
      webMenu(server, menu_Keys);
      server.printP(html_h1); server.printP(text_Key); server.printP(text_space); server.printP(text_setup); server.printP(html_e_h1); 

      server.printP(html_table_tr_th);
      server.printP(text_hash);
      server.printP(html_e_th_th); server.printP(text_Name);
      server.printP(html_e_th_th); server.printP(text_Value);
      server.printP(html_e_th_e_tr);
      for (uint8_t i = 0; i < NUM_OF_KEYS; ++i) {
        server.printP(html_tr_td); 
        server << i+1; server.printP(text_dot); server.printP(html_e_td_td);
        server << conf.key_name[i];
        server.printP(html_e_td_td);
        formatKey(conf.key[i], tmp);
        //server.printP(html_pre);
        server << tmp;
        // server.printP(html_e_pre);
        server.printP(html_e_td_e_tr);
      }
      server.printP(html_e_table); 

      server.printP(html_form_s); server << PREFIX "/k"; server.printP(html_form_e);      
      server.printP(html_table_tr_td);
      server.printP(text_Key); server.printP(html_e_td_td);
      server.printP(html_select_submit); server << "K"; server.printP(html_e_tag);
      for (uint8_t ii = 0; ii < NUM_OF_KEYS; ++ii) {
        server.printP(html_option); server << ii;
        if (webKey == ii) { server.printP(html_selected); }
        else              { server.printP(html_e_tag); }
        server << ii + 1; server.printP(text_spdashsp); server << conf.key_name[ii]; server.printP(html_e_option);
      }
      server.printP(html_e_select);
      server.printP(html_e_td_e_tr); server.printP(html_tr_td);
      server.printP(text_Name); server.printP(html_e_td_td);
      server.printP(html_s_tag); server << "n"; server.printP(html_m_tag); server << conf.key_name[webKey]; server.printP(html_e_tag);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Value); server.printP(html_e_td_td);
      formatKey(conf.key[webKey], tmp);    
      server.printP(html_s_tag); server << "k"; server.printP(html_m_tag); server << tmp; server.printP(html_e_tag);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Last); server.printP(text_space); server.printP(text_key); server.printP(html_e_td_td);
      formatKey(last_key, tmp); server << tmp; server.printP(html_e_td_e_tr);
      server.printP(html_e_table);     
      server.printP(html_F_A); // submit Apply
      server.printP(html_F_SA); // submit Save all
      server.printP(html_e_form);
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}

uint8_t webTel = 0;
void webSetPhone(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    if (type == WebServer::POST) {
      bool repeat;
      char name[2], value[31];
      do {
        repeat = server.readPOSTparam(name, 2, value, 31);
        switch(name[0]){
          case 'P':
            n = strtol(value, NULL, 10);
            if (n != webTel) {
              webTel = n;
              repeat = 0;
            }
          break;
          case 'p': // number
            strncpy (conf.tel_num[webTel], value, 16);
            conf.tel_num[webTel][15] = 0;
          break;
          case 'n': // name
            strncpy (conf.tel_name[webTel], value, 16);
            conf.tel_name[webTel][15] = 0;
          break;
          case 'm': // email
            strncpy (conf.email[webTel], value, 16);
            conf.email[webTel][15] = 0;
          break;
          case 'o': // enable tel
            if (value[0] == '0') conf.tel[webTel] &= ~(1 << 0);
            else conf.tel[webTel] |= (1 << 0);
          break;
          case 'a': // global 
            if (value[0] == '0') conf.tel[webTel] &= ~(1 << 5);
            else conf.tel[webTel] |= (1 << 5);
          break;
          case 'g': // group
            n = strtol(value, NULL, 10);
            if ((n >> 0) & 1) conf.tel[webTel] |= (1 << 1);
            else conf.tel[webTel] &= ~(1 << 1);
            if ((n >> 1) & 1) conf.tel[webTel] |= (1 << 2);
            else conf.tel[webTel] &= ~(1 << 2);
            if ((n >> 2) & 1) conf.tel[webTel] |= (1 << 3);
            else conf.tel[webTel] &= ~(1 << 3);
            if ((n >> 3) & 1) conf.tel[webTel] |= (1 << 4);
            else conf.tel[webTel] &= ~(1 << 4);
          break;
          case 'e': saveConf(); break; // save to EEPROM         
          }
        } while (repeat);
        server.httpSeeOther(PREFIX "/c");
      } else {
        server.httpSuccess();
        webMenu(server, menu_Contacts);
        server.printP(html_h1); server.printP(text_Contact); server.printP(text_s); server.printP(text_space); server.printP(text_setup); server.printP(html_e_h1); 

        server.printP(html_table_tr_th);
        server.printP(text_hash);
        server.printP(html_e_th_th); server.printP(text_Enabled);
        server.printP(html_e_th_th); server.printP(text_Name);
        server.printP(html_e_th_th); server.printP(text_Number);
        server.printP(html_e_th_th); server.printP(text_Email);
        server.printP(html_e_th_th); server.printP(text_Global);
        server.printP(html_e_th_th); server.printP(text_Group);
        server.printP(html_e_th_e_tr);
        for (uint8_t i = 0; i < NUM_OF_PHONES; ++i) {
          server.printP(html_tr_td); 
          server << i+1; server.printP(text_dot); server.printP(html_e_td_td);
          (conf.tel[i] & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
          server << conf.tel_name[i]; server.printP(html_e_td_td);
          server << conf.tel_num[i]; server.printP(html_e_td_td);
          server << conf.email[i]; server.printP(html_e_td_td);
          (conf.tel[i] >> 5 & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
          if (conf.tel[i] & B1) {
            if (conf.tel[i] >> 5 & B1) { server.printP(text_all); }
            else { server << ((conf.tel[i] >> 1) & B1111) + 1; server.printP(text_spdashsp); server << conf.group_name[((conf.tel[i] >> 1) & B1111)]; }
          } 
          else { server.printP(text_i_disabled); }
          server.printP(html_e_td_e_tr);
        }
        server.printP(html_e_table); 
        server.printP(html_form_s); server << PREFIX "/c"; server.printP(html_form_e);
        
        server.printP(html_table_tr_td);
        server.printP(text_Contact); server.printP(html_e_td_td);  
        server.printP(html_select_submit); server << "P"; server.printP(html_e_tag);
        for (uint8_t ii = 0; ii < NUM_OF_PHONES; ++ii) {
          server.printP(html_option); server << ii;
          if (webTel == ii) { server.printP(html_selected); }
          else              { server.printP(html_e_tag); }
          server << ii + 1; server.printP(text_spdashsp); server << conf.tel_name[ii]; server.printP(html_e_option);
        }
        server.printP(html_e_select);
        server.printP(html_e_td_e_tr); server.printP(html_tr_td);
        server.printP(text_Contact); server.printP(text_space); server.printP(text_is); server.printP(html_e_td_td);
        server.printP(html_radio_s);
        server.radioButton("o", text_1, text_On, conf.tel[webTel] & B1);
        server.radioButton("o", text_0, text_Off, !(conf.tel[webTel] & B1));
        server.printP(html_div_e);
        server.printP(html_e_td_e_tr); server.printP(html_tr_td);
        server.printP(text_Name); server.printP(html_e_td_td);
      //if (webTel != 0) { server.printP(html_s_tag); server << "n"; server.printP(html_m_tag); server << conf.tel_name[webTel]; server.printP(html_e_tag); }
      //else             { server << conf.tel_name[webTel]; }
        server.printP(html_s_tag); server << "n"; server.printP(html_m_tag); server << conf.tel_name[webTel]; server.printP(html_e_tag);
        server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Phone); server.printP(text_space); server.printP(text_number); server.printP(html_e_td_td);
        server.printP(html_s_tag); server << "p"; server.printP(html_m_tag); server << conf.tel_num[webTel]; server.printP(html_e_tag);
        server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Email); server.printP(html_e_td_td);
        server.printP(html_s_tag); server << "m"; server.printP(html_m_tag); server << conf.email[webTel]; server.printP(html_e_tag);
        server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Global); server.printP(text_space); server.printP(text_contact); server.printP(html_e_td_td);
        server.printP(html_radio_s);
        server.radioButton("a", text_1, text_Yes, conf.tel[webTel] >> 5 & B1);
        server.radioButton("a", text_0, text_No, !(conf.tel[webTel] >> 5 & B1));
        server.printP(html_div_e);
        server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Group); server.printP(html_e_td_td);
        server.printP(html_select); server << "g"; server.printP(html_e_tag);
        for (uint8_t ii = 0; ii < ALR_GROUPS; ++ii) {
          server.printP(html_option); server << ii; 
          if ((conf.tel[webTel] >> 1 & B1111) == ii) { server.printP(html_selected); }
          else                                       { server.printP(html_e_tag); }
          server << ii + 1; server.printP(text_spdashsp); server << conf.group_name[ii]; server.printP(text_spdashsp); (conf.group[ii] & B1) ? server.printP(text_On) : server.printP(text_Off); server.printP(html_e_option);
        }
        server.printP(html_e_select);
        server.printP(html_e_td_e_tr);
        server.printP(html_e_table);
            
      server.printP(html_F_A); // submit Apply
      server.printP(html_F_SA); // submit Save all
      server.printP(html_e_form);
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}

uint8_t webAuth = 0;
void webSetAuth(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    if (type == WebServer::POST) {
      bool repeat;
      char name[2], value[17];
      do {
        repeat = server.readPOSTparam(name, 2, value, 17);
        switch(name[0]){
          case 'P':
            n = strtol(value, NULL, 10);
            if (n != webAuth) {
              webAuth = n;
              repeat = 0;
            }
          break;
          case 'R':
            n = sendCmd(15,1);
          break;
          case 'A': // Apply
            value[0] = 'R';
            value[1] = 'K';
            value[2] = unit[webAuth].type;
            value[3] = unit[webAuth].number;
            value[4] = (char)unit[webAuth].setting;
            sendData(unit[webAuth].address, value, 5);
          break;
          case 'o': // enable 
            if (value[0] == '0') unit[webAuth].setting &= ~(1 << 0);
            else unit[webAuth].setting |= (1 << 0);
          break;
          case 'g': // group
            n = strtol(value, NULL, 10);
            if ((n >> 0) & 1) unit[webAuth].setting |= (1 << 1);
            else unit[webAuth].setting &= ~(1 << 1);
            if ((n >> 1) & 1) unit[webAuth].setting |= (1 << 2);
            else unit[webAuth].setting &= ~(1 << 2);
            if ((n >> 2) & 1) unit[webAuth].setting |= (1 << 3);
            else unit[webAuth].setting &= ~(1 << 3);
            if ((n >> 3) & 1) unit[webAuth].setting |= (1 << 4);
            else unit[webAuth].setting &= ~(1 << 4);
          break;
          case 'e': saveConf(); break; // save to EEPROM         
          }
        } while (repeat);
        server.httpSeeOther(PREFIX "/a");
      } else {
        server.httpSuccess();
        webMenu(server, menu_Auth);
        server.printP(html_h1); server.printP(text_Authentication); server.printP(text_space); server.printP(text_setup); server.printP(html_e_h1);  
        server.printP(html_form_s); server << PREFIX "/a"; server.printP(html_form_e);

        server.printP(html_table_tr_th);
        server.printP(text_hash);
        server.printP(html_e_th_th); server.printP(text_Enabled);
        server.printP(html_e_th_th); server.printP(text_Address);
        server.printP(html_e_th_th); server.printP(text_Type);
        server.printP(html_e_th_th); server.printP(text_Group);
        server.printP(html_e_th_e_tr);
        for (uint8_t i = 0; i < units; ++i) {
          server.printP(html_tr_td); 
          server << i+1; server.printP(text_dot); server.printP(html_e_td_td);
          (unit[i].setting & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled);
          server.printP(html_e_td_td);
          if (unit[i].address < RADIO_UNIT_OFFSET) { server << "W:" << unit[i].address; }
          else                                     { server << "R:" << unit[i].address-RADIO_UNIT_OFFSET; }
          server.printP(text_semic); server << unit[i].number; server.printP(html_e_td_td);
          switch(unit[i].type){
            case 'i': server.printP(text_iButton); break;
            default: server.printP(text_i_question); break;
          }
          server.printP(html_e_td_td);
          server << ((unit[i].setting >> 1) & B1111) + 1; server.printP(text_spdashsp); server << conf.group_name[((unit[i].setting >> 1) & B1111)];
          server.printP(html_e_td_e_tr);
        }
        server.printP(html_e_table); 
        server.printP(html_table_tr_td);
        server.printP(text_Unit); server.printP(html_e_td_td);
        server.printP(html_select_submit); server << "P"; server.printP(html_e_tag);
        for (uint8_t ii = 0; ii < units; ++ii) {
          server.printP(html_option); server << ii;
          if (webAuth == ii) {  server.printP(html_selected); }
          else               { server.printP(html_e_tag); }
          server << ii + 1; server.printP(html_e_option); 
        }
        server.printP(html_e_select); server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Address); server.printP(html_e_td_td);
        server << unit[webAuth].address; server.printP(text_semic); server << unit[webAuth].number; server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); 
        server.printP(text_Type); server.printP(html_e_td_td);
        switch(unit[webAuth].type){
          case 'i': server.printP(text_iButton); break;
          default: server.printP(text_i_question); break;
        }
        server.printP(html_e_td_e_tr);
        server.printP(html_tr_td); server.printP(text_Unit); server.printP(text_space); server.printP(text_is); server.printP(html_e_td_td);
        server.printP(html_radio_s);
        server.radioButton("o", text_1, text_On, unit[webAuth].setting & B1);
        server.radioButton("o", text_0, text_Off, !(unit[webAuth].setting & B1));
        server.printP(html_div_e); server.printP(html_e_td_e_tr); 
        server.printP(html_tr_td); server.printP(text_Group); server.printP(html_e_td_td);
        server.printP(html_select); server << "g"; server.printP(html_e_tag);
        for (uint8_t ii = 0; ii < ALR_GROUPS; ++ii) {
          server.printP(html_option); server << ii;
          if ((unit[webAuth].setting >> 1 & B1111) == ii) { server.printP(html_selected);  }
          else                                            { server.printP(html_e_tag); }
          server << ii + 1; server.printP(text_spdashsp); server << conf.group_name[ii]; server.printP(text_spdashsp); (conf.group[ii] & B1) ? server.printP(text_On) : server.printP(text_Off); server.printP(html_e_option);
        }
        server.printP(html_e_select); server.printP(html_e_td_e_tr);
        server.printP(html_e_table);
            
      server.printP(html_F_A); // submit Apply
      server.printP(html_F_SA); // submit Save all
      server.printP(html_F_RR); // submit Reregister
      server.printP(html_e_form);
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}

uint8_t webSens = 0;
void webSetSens(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    char name[2], value[17];
    if (type == WebServer::POST) {
      bool repeat;
      do {
        repeat = server.readPOSTparam(name, 2, value, 17);
        switch(name[0]){
          case 'P':
            n = strtol(value, NULL, 10);
            if (n != webSens) {
              webSens = n;
              repeat = 0;
            }
          break;
          case 'R':
            n = sendCmd(15,1);
          break;
          case 'A': // Apply
            value[0] = 'R';
            value[1] = 'S';
            value[2] = sensor[webSens].type;
            value[3] = sensor[webSens].number;
            value[4] = (char)sensor[webSens].setting;
            sendData(sensor[webSens].address,value, 5);
          break;
          case 'o': // enable 
            if (value[0] == '0') sensor[webSens].setting &= ~(1 << 0);
            else sensor[webSens].setting |= (1 << 0);
          break;
          case 'p': // MQtt publish
            if (value[0] == '0') sensor[webSens].setting &= ~(1 << 7);
            else sensor[webSens].setting |= (1 << 7);
          break;
          case 'g': // group
            n = strtol(value, NULL, 10);
            if ((n >> 0) & 1) sensor[webSens].setting |= (1 << 1);
            else sensor[webSens].setting &= ~(1 << 1);
            if ((n >> 1) & 1) sensor[webSens].setting |= (1 << 2);
            else sensor[webSens].setting &= ~(1 << 2);
            if ((n >> 2) & 1) sensor[webSens].setting |= (1 << 3);
            else sensor[webSens].setting &= ~(1 << 3);
            if ((n >> 3) & 1) sensor[webSens].setting |= (1 << 4);
            else sensor[webSens].setting &= ~(1 << 4);
          break;
          case 'e': saveConf(); break; // save to EEPROM          
          }
        } while (repeat);
      server.httpSeeOther(PREFIX "/s");
    } else {
      server.httpSuccess();
      webMenu(server, menu_Sens);
      server.printP(html_h1); server.printP(text_Sensor); server.printP(text_space); server.printP(text_setup); server.printP(html_e_h1);  
      server.printP(html_form_s); server << PREFIX "/s"; server.printP(html_form_e);    
      server.printP(html_table_tr_th);
      server.printP(text_hash);
      server.printP(html_e_th_th); server.printP(text_Enabled);
      server.printP(html_e_th_th); server.printP(text_Address);
      server.printP(html_e_th_th); server.printP(text_MQTT);
      server.printP(html_e_th_th); server.printP(text_Last); server.printP(text_space); server.printP(text_OK);
      server.printP(html_e_th_th); server.printP(text_Type);
      server.printP(html_e_th_th); server.printP(text_Value);
      server.printP(html_e_th_th); server.printP(text_Group);
      server.printP(html_e_th_e_tr);
      for (uint8_t i = 0; i < sensors; ++i) {
        server.printP(html_tr_td); 
        server << i+1; server.printP(text_dot); server.printP(html_e_td_td);
        (sensor[i].setting & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
        if (sensor[i].address < RADIO_UNIT_OFFSET) { server << "W:" << sensor[i].address; }
        else                                 { server << "R:" << sensor[i].address-RADIO_UNIT_OFFSET; }
        server.printP(text_semic); server << sensor[i].number; server.printP(html_e_td_td);
        ((sensor[i].setting >> 7) & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
        time_now = (nilTimeNow() - sensor[i].last_OK)/NIL_CFG_FREQUENCY; 
        server.print((char*)time_now.formatedUpTime()); server.printP(html_e_td_td);
        switch(sensor[i].type){
          case 'T': server.printP(text_Temperature); break;
          case 'H': server.printP(text_Humidity); break;
          case 'P': server.printP(text_Pressure); break;
          default: server.printP(text_i_question); break;
        }
        server.printP(html_e_td_td);
        dtostrf(sensor[i].value, 6, 2, value);
        server << value;
        switch(sensor[i].type){
          case 'T': server.printP(text_degC); break;
          case 'H': server.printP(text_space); server.printP(text_percent); break;
          case 'P': server.printP(text_mBar); break;
          default: break;
        }
        server.printP(html_e_td_td);
        server << ((sensor[i].setting >> 1) & B1111) + 1; server.printP(text_spdashsp); server << conf.group_name[((sensor[i].setting >> 1) & B1111)];
        server.printP(html_e_td_e_tr);
      }
      server.printP(html_e_table); 
      server.printP(html_table_tr_td);
      server.printP(text_Sensor); server.printP(html_e_td_td);
      server.printP(html_select_submit); server << "P"; server.printP(html_e_tag);
      for (uint8_t ii = 0; ii < sensors; ++ii) {
        server.printP(html_option); server << ii;
        if (webSens == ii) { server.printP(html_selected); }
        else               { server.printP(html_e_tag); }
        server << ii + 1; server.printP(html_e_option);
      }
      server.printP(html_e_select); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Address); server.printP(html_e_td_td);
      if (sensor[webSens].address < RADIO_UNIT_OFFSET) { server << "W:" << sensor[webSens].address; }
      else                                             { server << "R:" << sensor[webSens].address-RADIO_UNIT_OFFSET; }
      server.printP(text_semic); server << sensor[webSens].number; server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); 
      server.printP(text_Type); server.printP(html_e_td_td);
      switch(sensor[webSens].type){
        case 'T': server.printP(text_Temperature); break;
        case 'H': server.printP(text_Humidity); break;
        case 'P': server.printP(text_Pressure); break;
        default: server.printP(text_undefined); break;
      }
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Sensor); server.printP(text_space); server.printP(text_is); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("o", text_1, text_On, sensor[webSens].setting & B1);
      server.radioButton("o", text_0, text_Off, !(sensor[webSens].setting & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr); 
      server.printP(html_tr_td); server.printP(text_MQTT); server.printP(text_space); server.printP(text_publish); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("p", text_1, text_On, (sensor[webSens].setting >> 7) & B1);
      server.radioButton("p", text_0, text_Off, !((sensor[webSens].setting >> 7) & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr); 
      server.printP(html_tr_td); server.printP(text_Group); server.printP(html_e_td_td);
      server.printP(html_select); server << "g"; server.printP(html_e_tag);
      for (uint8_t ii = 0; ii < ALR_GROUPS; ++ii) {
        server.printP(html_option); server << ii;
        if ((sensor[webSens].setting >> 1 & B1111) == ii) { server.printP(html_selected); }
        else                                              { server.printP(html_e_tag); }
        server << ii + 1; server.printP(text_spdashsp); server << conf.group_name[ii]; server.printP(text_spdashsp); (conf.group[ii] & B1) ? server.printP(text_On) : server.printP(text_Off); server.printP(html_e_option);
      }
      server.printP(html_e_select);
      server.printP(html_e_td_e_tr);
      server.printP(html_e_table);
          
      server.printP(html_F_A); // submit Apply
      server.printP(html_F_SA); // submit Save all
      server.printP(html_F_RR); // submit Reregister
      server.printP(html_e_form);
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}

void webSetMQTT(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    char name[2], value[17];
    if (type == WebServer::POST) {
      bool repeat;
      //char name[2], value[17];
      do {
        repeat = server.readPOSTparam(name, 2, value, 17);
        switch(name[0]){
          case 'e': saveConf(); break; // save to EEPROM
        }
      } while (repeat);
      server.httpSeeOther(PREFIX "/m");
    } else {
      server.httpSuccess();
      webMenu(server, menu_MQTT);
      server.printP(html_h1); server.printP(text_MQTT); server.printP(text_space); server.printP(text_setup); server.printP(html_e_h1);  
      server.printP(html_table_tr_td);

      server.printP(text_MQTT); server.printP(text_space); server.printP(text_is); server.printP(text_space); server.printP(text_connected); server.printP(html_e_td_td);
      client.connected() ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_e_tr);
      /*
      server.printP(html_tr_td); server.printP(text_Radio); server.printP(text_space); server.printP(text_key); server.printP(html_e_td_td);
      server.printP(text_sesp);
      server.printP(html_s_tag); server << "n"; server.printP(html_m_tag); server << conf.tel_name[webTel]; server.printP(html_e_tag);
      */
      server.printP(html_e_table);
      
      server.printP(html_F_A); // submit Apply
      server.printP(html_F_SA); // submit Save all
      server.printP(html_e_form);
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}

void webSetTriggers(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    char name[2], value[17];
    if (type == WebServer::POST) {
      bool repeat;
      //char name[2], value[17];
      do {
        repeat = server.readPOSTparam(name, 2, value, 17);
        switch(name[0]){
          case 'e': saveConf(); break; // save to EEPROM
        }
      } while (repeat);
      server.httpSeeOther(PREFIX "/m");
    } else {
      server.httpSuccess();
      webMenu(server, menu_Sens);
      server.printP(html_h1); server.printP(text_Triggers); server.printP(text_space); server.printP(text_setup); server.printP(html_e_h1);  
      server.printP(html_form_s); server << PREFIX "/s"; server.printP(html_form_e);    
      server.printP(html_table_tr_th);
      server.printP(text_hash);
      server.printP(html_e_th_th); server.printP(text_Enabled);
      server.printP(html_e_th_th); server.printP(text_Address);
      server.printP(html_e_th_th); server.printP(text_Type);
      server.printP(html_e_th_th); server.printP(text_Symbol);
      server.printP(html_e_th_th); server.printP(text_Value);
      server.printP(html_e_th_th); server.printP(text_MQTT);
      server.printP(html_e_th_th); server.printP(text_Alerting);
      server.printP(html_e_th_th); server.printP(text_Triggered);
      server.printP(html_e_th_th); server.printP(text_Pass);
      server.printP(html_e_th_th); server.printP(text_Constant);
      server.printP(html_e_th_e_tr);
      for (uint8_t i = 0; i < TRIGGER_UNITS; ++i) {
        server.printP(html_tr_td); 
        server << i+1; server.printP(text_dot); server.printP(html_e_td_td);
        (trigger[i].setting & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
        if (trigger[i].address < RADIO_UNIT_OFFSET) { server << "W:" << trigger[i].address; }
        else                                 { server << "R:" << trigger[i].address-RADIO_UNIT_OFFSET; }
        server.printP(text_semic); server << trigger[i].number; server.printP(html_e_td_td);
        switch(trigger[i].type){
          case 'T': server.printP(text_Temperature); break;
          case 'H': server.printP(text_Humidity); break;
          case 'P': server.printP(text_Pressure); break;
          default: server.printP(text_i_question); break;
        }
        server.printP(html_e_td_td);
        server << trigger[i].symbol; server.printP(html_e_td_td);
        dtostrf(trigger[i].value, 6, 2, value); server << value; server.printP(html_e_td_td);
        ((trigger[i].setting >> 7) & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
        ((trigger[i].setting >> 6) & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
        ((trigger[i].setting >> 5) & B1) ? server.printP(text_i_OK) : server.printP(text_i_disabled); server.printP(html_e_td_td);
        ((trigger[i].setting >> 1) & B1) ? server.printP(text_value) : server.printP(text_constant); server.printP(html_e_td_td);
        dtostrf(trigger[i].constant, 6, 2, value); server << value; server.printP(html_e_td_e_tr);
      }
      server.printP(html_e_table); 
      server.printP(html_table_tr_td);
      server.printP(text_Sensor); server.printP(html_e_td_td);
      server.printP(html_select_submit); server << "P"; server.printP(html_e_tag);
      for (uint8_t ii = 0; ii < sensors; ++ii) {
        server.printP(html_option); server << ii;
        if (webSens == ii) { server.printP(html_selected); }
        else               { server.printP(html_e_tag); }
        server << ii + 1; server.printP(html_e_option);
      }
      server.printP(html_e_select); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Address); server.printP(html_e_td_td);
      if (sensor[webSens].address < RADIO_UNIT_OFFSET) { server << "W:" << sensor[webSens].address; }
      else                                             { server << "R:" << sensor[webSens].address-RADIO_UNIT_OFFSET; }
      server.printP(text_semic); server << sensor[webSens].number; server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); 
      server.printP(text_Type); server.printP(html_e_td_td);
      switch(sensor[webSens].type){
        case 'T': server.printP(text_Temperature); break;
        case 'H': server.printP(text_Humidity); break;
        case 'P': server.printP(text_Pressure); break;
        default: server.printP(text_undefined); break;
      }
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Sensor); server.printP(text_space); server.printP(text_is); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("o", text_1, text_On, sensor[webSens].setting & B1);
      server.radioButton("o", text_0, text_Off, !(sensor[webSens].setting & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr); 
      server.printP(html_tr_td); server.printP(text_MQTT); server.printP(text_space); server.printP(text_publish); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("p", text_1, text_On, (sensor[webSens].setting >> 7) & B1);
      server.radioButton("p", text_0, text_Off, !((sensor[webSens].setting >> 7) & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr); 
      server.printP(html_tr_td); server.printP(text_Group); server.printP(html_e_td_td);
      server.printP(html_select); server << "g"; server.printP(html_e_tag);
      for (uint8_t ii = 0; ii < ALR_GROUPS; ++ii) {
        server.printP(html_option); server << ii;
        if ((sensor[webSens].setting >> 1 & B1111) == ii) { server.printP(html_selected); }
        else                                              { server.printP(html_e_tag); }
        server << ii + 1; server.printP(text_spdashsp); server << conf.group_name[ii]; server.printP(text_spdashsp); (conf.group[ii] & B1) ? server.printP(text_On) : server.printP(text_Off); server.printP(html_e_option);
      }
      server.printP(html_e_select);
      server.printP(html_e_td_e_tr);
      server.printP(html_e_table);
          
      server.printP(html_F_A); // submit Apply
      server.printP(html_F_SA); // submit Save all
      server.printP(html_e_form);
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}


void webSetGlobal(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  if (server.checkCredentials(conf.user_pass)) {
    char name[3], value[31];
    if (type == WebServer::POST) {
      bool repeat;
      do {
        repeat = server.readPOSTparam(name, 3, value, 31);
        switch(name[0]){
          case 'a': conf.alr_time = strtol(value, NULL, 10); break;
          case 'b': conf.auto_arm = strtol(value, NULL, 10); break;
          case 'c': conf.open_alarm = strtol(value, NULL, 10);  break;
          case 'd': conf.arm_delay = strtol(value, NULL, 10) * 4;  break;
          case 'e': saveConf(); break; // save to EEPROM
          case 'k': strncpy ((char*)conf.radioKey, value, 16); // key
                    conf.radioKey[16] = 0; // extra byte for null
                    break;
          case 'u': strncpy (conf.user, value, 10);// user
                    conf.user[11] = 0;
                    break;
          case 'p': // password
            strncpy (conf.password, value, 10);
            conf.password[11] = 0;
            b64_text[0] = 0;
            strcat (b64_text, conf.user);
            strcat (b64_text, ":");
            strcat (b64_text, conf.password);
            base64_encode(conf.user_pass, b64_text, strlen(b64_text));
            //WS.print("p>"); WS.println(_text);
            //WS.println(strlen(conf.user_pass));
            //WS.println(conf.user_pass);
          break;
          case 'i': // user
            strncpy (conf.SMTP_user, value, 30);
            conf.SMTP_user[31] = 0;
          break;
          case 'o': // password
            strncpy (conf.SMTP_password, value, 15);
            conf.SMTP_password[16] = 0;
          break;
          case '0' ... '2': // Handle all radio buttons in groups 0 .. 2, A .. P
            if (value[0] == '0') conf.alerts[name[0]-48] &= ~(1 << (name[1]-65));
            else conf.alerts[name[0]-48] |= (1 << (name[1]-65));
          break;
        }
      } while (repeat);
      server.httpSeeOther(PREFIX "/r");
    } else {
      server.httpSuccess();
      webMenu(server, menu_Global);
      server.printP(html_h1); server.printP(text_Global); server.printP(text_space); server.printP(text_setup); server.printP(html_e_h1);  
      server.printP(html_form_s); server << PREFIX "/r"; server.printP(html_form_e);    
      server.printP(html_table_tr_td);
      server.printP(text_User); server.printP(html_e_td_td);
      server.printP(html_s_tag); server << "u"; server.printP(html_m_tag); server << conf.user; server.printP(html_e_tag);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Password); server.printP(html_e_td_td);
      server.printP(html_s_tag); server << "p"; server.printP(html_m_tag); server << conf.password; server.printP(html_e_tag);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_User); server.printP(html_e_td_td);
      server.printP(html_s_tag); server << "i"; server.printP(html_m_tag); server << conf.SMTP_user; server.printP(html_e_tag);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Password); server.printP(html_e_td_td);
      server.printP(html_s_tag); server << "o"; server.printP(html_m_tag); server << conf.SMTP_password; server.printP(html_e_tag);
      server.printP(html_e_td_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Authentication); server.printP(text_space); server.printP(text_time);server.printP(html_e_td_td);
      server.printP(html_select); server << "a"; server.printP(html_e_tag);
      for (uint8_t ii = 5; ii < 26; ++ii) {
        if ((conf.alr_time) == ii)  
          { server.printP(html_option); server << ii; server.printP(html_selected); server << ii; server.printP(html_e_option); }
        else 
          { server.printP(html_option); server << ii; server.printP(html_e_tag); server << ii; server.printP(html_e_option); }
      }
      server.printP(html_e_select);
      server.printP(text_space); server.printP(text_seconds); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Arm); server.printP(text_space); server.printP(text_delay); server.printP(html_e_td_td);
      server.printP(html_select); server << "d"; server.printP(html_e_tag);
      for (uint8_t ii = 10; ii < 41; ++ii) {
        if ((conf.arm_delay/4) == ii) // 250*4 = 1 sec.
          { server.printP(html_option); server << ii; server.printP(html_selected); server << ii; server.printP(html_e_option); }
        else 
          { server.printP(html_option); server << ii; server.printP(html_e_tag); server << ii; server.printP(html_e_option); }
      }
      server.printP(html_e_select);
      server.printP(text_space); server.printP(text_seconds); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Auto); server.printP(text_space); server.printP(text_arm); server.printP(text_space); server.printP(text_delay); server.printP(html_e_td_td);
      server.printP(html_select); server << "b"; server.printP(html_e_tag);
      for (uint8_t ii = 10; ii < 241; ii+=10) {
        if ((conf.auto_arm) == ii) {
          server.printP(html_option); server << ii; server.printP(html_selected); 
          //if (ii>59) { server << "0" << ii/60 << ":" << ii%60;}
          //else       { server << ii;}
          server << ii;
          server.printP(html_e_option);
        }
        else { 
          server.printP(html_option); server << ii; server.printP(html_e_tag);
          //if (ii>59) { server << "0" << ii/60 << ":" << ii%60;}
          //else       { server << ii;}
          server << ii;
          server.printP(html_e_option);
        }
      }
      server.printP(html_e_select);
      server.printP(text_space); server.printP(text_minutes); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Open); server.printP(text_space); server.printP(text_alarm); server.printP(html_e_td_td);
      server.printP(html_select); server << "c"; server.printP(html_e_tag);
      for (uint8_t ii = 10; ii < 241; ii+=5) {
        if ((conf.open_alarm) == ii) {
          server.printP(html_option); server << ii; server.printP(html_selected); 
      //if (ii>59) { server << "0" << ii/60 << ":" << ii%60;}
      //else       { server << ii;}
          server << ii;
          server.printP(html_e_option); }
          else { 
            server.printP(html_option); server << ii; server.printP(html_e_tag);
      //if (ii>59) { server << "0" << ii/60 << ":" << ii%60;}
      //else       { server << ii;}
            server << ii;
            server.printP(html_e_option);
          }
      }
      server.printP(html_e_select);
      server.printP(text_space); server.printP(text_minutes); server.printP(html_e_td_e_tr);
      server.printP(html_e_table);

      server.printP(html_h1); server.printP(text_Radio); server.printP(text_space); server.printP(text_setup); server.printP(html_e_h1);  
      server.printP(html_table_tr_td);
      server.printP(text_Radio); server.printP(text_space); server.printP(text_key); server.printP(html_e_td_td);
      server.printP(html_s_tag); server << "k"; server.printP(html_m_tag); server << (char*)conf.radioKey; server.printP(html_e_tag);
      server.printP(html_e_table);

      server.printP(html_h1); server.printP(text_Alerting); server.printP(html_e_h1);  
      server.printP(html_table_tr_th);
      server.printP(text_Name); server.printP(html_e_th_th);
      server.printP(text_SMS); server.printP(html_e_th_th);
      server.printP(text_Email); server.printP(html_e_th_th);
      server.printP(text_Last); server.printP(html_e_th_e_tr);
      server.printP(html_tr_td);
      server.printP(text_Undefined); server.printP(text_space); server.printP(text_key); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0A", text_1, text_On, conf.alerts[alert_SMS] & B1);
      server.radioButton("0A", text_0, text_Off, !(conf.alerts[alert_SMS] & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1A", text_1, text_On, conf.alerts[1] & B1);
      server.radioButton("1A", text_0, text_Off, !(conf.alerts[1] & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2A", text_1, text_On, conf.alerts[2] & B1);
      server.radioButton("2A", text_0, text_Off, !(conf.alerts[2] & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_System); server.printP(text_space); server.printP(text_disarmed); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0B", text_1, text_On, conf.alerts[alert_SMS] >> 1 & B1);
      server.radioButton("0B", text_0, text_Off, !(conf.alerts[alert_SMS] >> 1 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1B", text_1, text_On, conf.alerts[1] >> 1 & B1);
      server.radioButton("1B", text_0, text_Off, !(conf.alerts[1] >> 1 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2B", text_1, text_On, conf.alerts[2] >> 1 & B1);
      server.radioButton("2B", text_0, text_Off, !(conf.alerts[2] >> 1 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_System); server.printP(text_space); server.printP(text_armed); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0C", text_1, text_On, conf.alerts[alert_SMS] >> 2 & B1);
      server.radioButton("0C", text_0, text_Off, !(conf.alerts[alert_SMS] >> 2 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1C", text_1, text_On, conf.alerts[1] >> 2 & B1);
      server.radioButton("1C", text_0, text_Off, !(conf.alerts[1] >> 2 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2C", text_1, text_On, conf.alerts[2] >> 2 & B1);
      server.radioButton("2C", text_0, text_Off, !(conf.alerts[2] >> 2 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Open); server.printP(text_space); server.printP(text_alarm); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0M", text_1, text_On, conf.alerts[alert_SMS] >> 12 & B1);
      server.radioButton("0M", text_0, text_Off, !(conf.alerts[alert_SMS] >> 12 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1M", text_1, text_On, conf.alerts[1] >> 12 & B1);
      server.radioButton("1M", text_0, text_Off, !(conf.alerts[1] >> 12 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2M", text_1, text_On, conf.alerts[2] >> 12 & B1);
      server.radioButton("2M", text_0, text_Off, !(conf.alerts[2] >> 12 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Tamper); server.printP(text_space); server.printP(text_alarm); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0E", text_1, text_On, conf.alerts[alert_SMS] >> 4 & B1);
      server.radioButton("0E", text_0, text_Off, !(conf.alerts[alert_SMS] >> 4 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1E", text_1, text_On, conf.alerts[1] >> 4 & B1);
      server.radioButton("1E", text_0, text_Off, !(conf.alerts[1] >> 4 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2E", text_1, text_On, conf.alerts[2] >> 4 & B1);
      server.radioButton("2E", text_0, text_Off, !(conf.alerts[2] >> 4 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Sensor); server.printP(text_space); server.printP(text_alarm); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0F", text_1, text_On, conf.alerts[alert_SMS] >> 5 & B1);
      server.radioButton("0F", text_0, text_Off, !(conf.alerts[alert_SMS] >> 5 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1F", text_1, text_On, conf.alerts[1] >> 5 & B1);
      server.radioButton("1F", text_0, text_Off, !(conf.alerts[1] >> 5 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2F", text_1, text_On, conf.alerts[2] >> 5 & B1);
      server.radioButton("2F", text_0, text_Off, !(conf.alerts[2] >> 5 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_System); server.printP(text_space); server.printP(text_alarm); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0G", text_1, text_On, conf.alerts[alert_SMS] >> 6 & B1);
      server.radioButton("0G", text_0, text_Off, !(conf.alerts[alert_SMS] >> 6 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1G", text_1, text_On, conf.alerts[1] >> 6 & B1);
      server.radioButton("1G", text_0, text_Off, !(conf.alerts[1] >> 6 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2G", text_1, text_On, conf.alerts[2] >> 6 & B1);
      server.radioButton("2G", text_0, text_Off, !(conf.alerts[2] >> 6 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Monitoring); server.printP(text_space); server.printP(text_started); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0H", text_1, text_On, conf.alerts[alert_SMS] >> 7 & B1);
      server.radioButton("0H", text_0, text_Off, !(conf.alerts[alert_SMS] >> 7 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1H", text_1, text_On, conf.alerts[1] >> 7 & B1);
      server.radioButton("1H", text_0, text_Off, !(conf.alerts[1] >> 7 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2H", text_1, text_On, conf.alerts[2] >> 7 & B1);
      server.radioButton("2H", text_0, text_Off, !(conf.alerts[2] >> 7 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_System); server.printP(text_space); server.printP(text_group); server.printP(text_space); server.printP(text_armed); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0I", text_1, text_On, conf.alerts[alert_SMS] >> 8 & B1);
      server.radioButton("0I", text_0, text_Off, !(conf.alerts[alert_SMS] >> 8 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1I", text_1, text_On, conf.alerts[1] >> 8 & B1);
      server.radioButton("1I", text_0, text_Off, !(conf.alerts[1] >> 8 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2I", text_1, text_On, conf.alerts[2] >> 8 & B1);
      server.radioButton("2I", text_0, text_Off, !(conf.alerts[2] >> 8 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Configuration); server.printP(text_space); server.printP(text_saved); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0J", text_1, text_On, conf.alerts[alert_SMS] >> 9 & B1);
      server.radioButton("0J", text_0, text_Off, !(conf.alerts[alert_SMS] >> 9 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1J", text_1, text_On, conf.alerts[1] >> 9 & B1);
      server.radioButton("1J", text_0, text_Off, !(conf.alerts[1] >> 9 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2J", text_1, text_On, conf.alerts[2] >> 9 & B1);
      server.radioButton("2J", text_0, text_Off, !(conf.alerts[2] >> 9 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Power); server.printP(text_space); server.printP(text_state); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0K", text_1, text_On, conf.alerts[alert_SMS] >> 10 & B1);
      server.radioButton("0K", text_0, text_Off, !(conf.alerts[alert_SMS] >> 10 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1K", text_1, text_On, conf.alerts[1] >> 10 & B1);
      server.radioButton("1K", text_0, text_Off, !(conf.alerts[1] >> 10 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2K", text_1, text_On, conf.alerts[2] >> 10 & B1);
      server.radioButton("2K", text_0, text_Off, !(conf.alerts[2] >> 10 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_tr_td); server.printP(text_Battery); server.printP(text_space); server.printP(text_state); server.printP(html_e_td_td);
      server.printP(html_radio_s);
      server.radioButton("0L", text_1, text_On, conf.alerts[alert_SMS] >> 11 & B1);
      server.radioButton("0L", text_0, text_Off, !(conf.alerts[alert_SMS] >> 11 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("1L", text_1, text_On, conf.alerts[1] >> 11 & B1);
      server.radioButton("1L", text_0, text_Off, !(conf.alerts[1] >> 11 & B1));
      server.printP(html_div_e_e_td_td_radio_s);
      server.radioButton("2L", text_1, text_On, conf.alerts[2] >> 11 & B1);
      server.radioButton("2L", text_0, text_Off, !(conf.alerts[2] >> 11 & B1));
      server.printP(html_div_e); server.printP(html_e_td_e_tr);
      server.printP(html_e_table);
      
      server.printP(html_F_A); // submit Apply
      server.printP(html_F_SA); // submit Save all
      server.printP(html_e_form);
      server.printP(htmlFoot);
    }
  } else { server.httpUnauthorized(); }
}


// *********************************************************************************
// T H R E A D S    T H R E A D S    T H R E A D S    T H R E A D S    T H R E A D S
// *********************************************************************************

// Zone thread
//
NIL_WORKING_AREA(waZoneThread, 64);
NIL_THREAD(ZoneThread, arg) {
  int16_t val;
  uint8_t _group = 255;
  uint8_t _resp;
  
  nilThdSleepSeconds(60); // Delay to allow PIR sensors to settle up
  _tmp[0] = 'S'; _tmp[1] = 'S'; _tmp[2] = 0; pushToLog(_tmp);
  #if WEB_SERIAL_DEBUGGING 
  WS.println(F("Zone thread started"));
  #endif
  
  // Execute while loop every 0.25 seconds.
  while (TRUE) {
    nilThdSleepMilliseconds(250); // time is used also for arm delay and others ...

    for (int8_t i=0; i < ALR_GROUPS ; i++){ 
      if (group[i].arm_delay) { // wait for arm delay
        group[i].arm_delay--;
        if (!group[i].arm_delay) { 
          _resp = sendCmdToGrp(_group, 15);  // send arm message to all units
          _tmp[0] = 'S'; _tmp[1] = 'Z'; _tmp[2] = 48+i; _tmp[3] = 0; pushToLog(_tmp);
        }
      }
    }

    nilSemWait(&ETHSem);    // wait for slot
    nilSemWait(&ADCSem);    // wait for slot
    BatteryLevel = nilAnalogRead(0); // Sensor voltage level
    nilSemSignal(&ADCSem);  // Exit region.
    nilSemSignal(&ETHSem);  // Exit region.
    
    for (int8_t i=0; i < ALR_ZONES ; i++){
      if (conf.zone[i] & B1){         // Zone enabled ?
        if (conf.zone[i] >> 15){       // Digital 0/ Analog 1 
          nilSemWait(&ADCSem);          // Wait for slot
          val  = nilAnalogRead(i+1);
          nilSemSignal(&ADCSem);        // Exit region.
        } else {
          switch(i) {
            case 7:  pinIN1.read() ? val = ALR_PIR+BatteryLevel : val = ALR_OK+BatteryLevel; break;
            case 8:  pinIN2.read() ? val = ALR_PIR+BatteryLevel : val = ALR_OK+BatteryLevel; break;
            case 9:  pinIN3.read() ? val = ALR_PIR+BatteryLevel : val = ALR_OK+BatteryLevel; break;
            case 10: pinIN4.read() ? val = ALR_PIR+BatteryLevel : val = ALR_OK+BatteryLevel; break;
            case 11: pinIN5.read() ? val = ALR_PIR+BatteryLevel : val = ALR_OK+BatteryLevel; break;
            case 12: pinTAMPER.read() ? val = ALR_PIR+BatteryLevel : val = ALR_OK+BatteryLevel; break;
            default: break;
          } 
        }
        _group = (conf.zone[i] >> 1) & B1111; // set group
        // Decide 
        //WS.print(F("dec: ")); WS.println((int16_t)(val-BatteryLevel));
        switch((int16_t)(val-BatteryLevel)){
          case ALR_OK_LOW ... ALR_OK_HI:
            // All is OK no action
            zone[i].last_event = 'O';
            zone[i].last_OK = nilTimeNow();    // update current timestamp
            break;
          case ALR_PIR_LOW ... ALR_PIR_HI:
            //   group  armed                      group not have alarm
            if ((group[_group].setting & B1) && !((group[_group].setting >> 1) & B1) && !group[_group].arm_delay){             
              // if group not enabled log error to log. 
              if (!(conf.group[_group] & B1)) {
                _tmp[0] = 'G'; _tmp[1] = 'F'; _tmp[2] = 48+_group; _tmp[3] = 0; pushToLog(_tmp);
              } else {
                if (zone[i].last_event == 'P') {
                  _tmp[0] = 'P'; _tmp[1] = 48+i; _tmp[2] = 0; pushToLog(_tmp); 
                  alarm_event_t* p = alarm_fifo.waitFree(TIME_IMMEDIATE); // Get a free FIFO slot.
                  if (p == 0) {
                    _tmp[0] = 'F'; _tmp[1] = 'A'; _tmp[2] = 0; pushToLog(_tmp); // Alarm queue is full
                    return; // Continue if no free space.
                  }
                  p->zone = i; p->type = 'P';
                  group[_group].setting |= (1 << 1); // Set alarm bit On
                  alarm_fifo.signalData(); // Signal idle thread data is available.              
                }
                zone[i].last_event = 'P';
              }
            }
            zone[i].last_PIR = nilTimeNow();    // update current timestamp
            break;
          default: // Line is cut or short or tamper, no difference to alarm
            //  group not have alarm
            if (!((group[_group].setting >> 1) & B1)){
              // if group not enabled log error to log. 
              if (!(conf.group[_group] & B1)) {
                _tmp[0] = 'G'; _tmp[1] = 'F'; _tmp[2] = 48+_group; _tmp[3] = 0; pushToLog(_tmp);
              } else {
                if (zone[i].last_event == 'T') {
                  _tmp[0] = 'T'; _tmp[1] = 48+i; _tmp[2] = 0; pushToLog(_tmp);
                    alarm_event_t* p = alarm_fifo.waitFree(TIME_IMMEDIATE); // Get a free FIFO slot.
                    if (p == 0) {
                      _tmp[0] = 'F'; _tmp[1] = 'A'; _tmp[2] = 0; pushToLog(_tmp); // Alarm queue is full
                      return; // Continue if no free space.
                    }
                    p->zone = i; p->type = 'T';
                    group[_group].setting |= (1 << 1); // Set alarm bit On
                    alarm_fifo.signalData(); // Signal idle thread data is available.
                  }
                  zone[i].last_event = 'T';
              }
            }
            break;
          }     
      } // Zone enabled ?
    } // for (zones)
  } // while thread TRUE
}

//------------------------------------------------------------------------------
// Alarm Events
//
NIL_WORKING_AREA(waAEThread1, 64);
NIL_WORKING_AREA(waAEThread2, 64);
NIL_WORKING_AREA(waAEThread3, 64);
NIL_THREAD(thdFcn, name) {
  uint8_t _group, _wait, _resp, _cnt;
  msg_t _nil_r;
  #if WEB_SERIAL_DEBUGGING 
  WS.print(F("Event thread started ")); WS.println((char*)name);
  #endif
  
  while (TRUE) {
    // Check for data.  Use TIME_IMMEDIATE to prevent sleep in idle thread.
    alarm_event_t* p = alarm_fifo.waitData(TIME_INFINITE);
    if (!p) return;                           // return if no data

    _group = (conf.zone[p->zone] >> 1) & B1111;
    if (p->type == 'P') _wait = (conf.zone[p->zone] >> 5) & B11;
    else                _wait = 0;            // Tamper and others have no wait time

    if (!((group[_group].setting >> 1) & B1)) return; // group has no alarm anymore

    group[_group].setting |= (1 << 2); // Set auth bit On

    /*
    #if WEB_SERIAL_DEBUGGING 
    WS.print((char*)name); WS.print(" zone: "); WS.print(p->zone); WS.print(", type: "); WS.print(p->type);
    WS.print(", group: "); WS.print(_group); WS.print(", Auth time: "); WS.println(_wait);
    #endif
    */

    _resp = sendCmdToGrp(_group, 11 + _wait);

    if (!_wait) { // Alarm wait 0x
      // Combine alarms, so that next alarm will not disable ongoing one
      if (p->type == 'P') {
        OUTs = ((((conf.group[_group] >> 4) & B1) | (OUTs >> 0) & B1) |
         (((conf.group[_group] >> 3) & B1) | (OUTs >> 1) & B1) << 1);
      } else {
        OUTs = ((((conf.group[_group] >> 2) & B1) | (OUTs >> 0) & B1) |
         (((conf.group[_group] >> 1) & B1) | (OUTs >> 1) & B1) << 1);
      }
      // Trigger OUT 1 & 2              
      pinOUT1.write(((OUTs >> 0) & B1));
      pinOUT2.write(((OUTs >> 1) & B1));
      _tmp[0] = 'S'; _tmp[1] = 'X'; _tmp[2] = 48+_group; _tmp[3] = 0; pushToLog(_tmp); // ALARM no auth.
    } else { // Alarm wait > 0x
      do {
        _cnt = 0;
        while ((group[_group].setting >> 2 & B1) && _cnt < (10*conf.alr_time)) {
          nilThdSleepMilliseconds(100);
          _cnt++;
        }
        if (!(group[_group].setting >> 2 & B1)) { // Auth. OK
          break; // escape do while 
        } else {
          _wait--;
          _resp = sendCmdToGrp(_group, 11 + _wait);
        }
      } while (_wait);
      if (!_wait) { // Alarm wait 0x = no auth in time
        // Combine alarms, so that next alarm will not disable ongoing one
        if (p->type == 'P') {
          OUTs = ((((conf.group[_group] >> 4) & B1) | (OUTs >> 0) & B1) |
           (((conf.group[_group] >> 3) & B1) | (OUTs >> 1) & B1) << 1);
        } else {
          OUTs = ((((conf.group[_group] >> 2) & B1) | (OUTs >> 0) & B1) |
           (((conf.group[_group] >> 1) & B1) | (OUTs >> 1) & B1) << 1);
        }
        // Trigger OUT 1 & 2              
        pinOUT1.write(((OUTs >> 0) & B1));
        pinOUT2.write(((OUTs >> 1) & B1));
        _tmp[0] = 'S'; _tmp[1] = 'X';  _tmp[2] = 48 + _group; _tmp[3] = 0;pushToLog(_tmp); // ALARM no auth.
      }
    }
    // Signal FIFO slot is free.
    alarm_fifo.signalFree();
  }
}

//------------------------------------------------------------------------------
// RS485 receiver thread
//
NIL_WORKING_AREA(waRS485RXThread, 128);
NIL_THREAD(RS485RXThread, arg) {
  uint8_t _resp = 0; 
  uint8_t _group = 255;
  uint8_t _pos = 0;
  uint8_t _unit;

  #if WEB_SERIAL_DEBUGGING 
  WS.println(F("RS485 receiver thread started"));
  #endif
  
  while (TRUE) {   
    nilWaitRS485NewMsg(); // wait for event
    
    _resp = RS485.msg_read(&RX_msg);

    /*
    #if WEB_SERIAL_DEBUGGING 
    WS.print(F("> A:")); WS.print(RX_msg.address); WS.print(F(", C:")); WS.print(RX_msg.ctrl);
    WS.print(F(", L:")); WS.print(RX_msg.data_length); WS.print(F(" | "));   
    for (uint8_t i=0; i < RX_msg.data_length; i++){
      WS.print((uint8_t)RX_msg.buffer[i],HEX); WS.print(F(" "));
    }; WS.println();
    #endif
    */
    
    // iButtons keys
    if (RX_msg.ctrl == FLAG_DTA && RX_msg.data_length == KEY_LEN && RX_msg.buffer[0]!='R') {  // incoming message looks like a key 
      _resp = 0;
      // search unit with given address
      for (_unit=0; _unit < units; _unit++) {
        if (unit[_unit].address == RX_msg.address) {
          _resp = 1;
          break;
        }
      }
      //  enabled for authorzation       address found
      if ((unit[_unit].setting & B1) && (_resp))  { 
        for (uint8_t i=0; i < NUM_OF_KEYS; i++){
          _resp = memcmp(RX_msg.buffer, conf.key[i], KEY_LEN); // Compare key
          if (!_resp) { // key matched
            if (true) { // key enabled
              _group = (unit[_unit].setting >> 1) & B1111;
              //     we have alarm                      group is armed
              if  (((group[_group].setting) >> 1 & B1) || ((group[_group].setting) & B1)) { 
                if ((group[_group].setting) >> 1 & B1) { // we have alarm
                  group[_group].setting &= ~(1 << 1);    // set alarm off
                  OUTs = 0; // Reset outs  
                  pinOUT1.write(LOW); pinOUT2.write(LOW); // Turn off OUT 1 & 2
                }
                group[_group].setting &= ~(1 << 0);    // disarm group
                group[_group].setting &= ~(1 << 2);    // set auth bit off
                group[_group].arm_delay = 0;       // Reset arm delay
                _resp = sendCmdToGrp(_group, 20);  // send quiet message to all units
                _tmp[0] = 'A'; _tmp[1] = 'D'; _tmp[2] = 48+i; _tmp[3] = 0; pushToLog(_tmp);
              } else { // Just do arm
                _tmp[0] = 'A'; _tmp[1] = 'A'; _tmp[2] = 48+i; _tmp[3] = 0; pushToLog(_tmp);
                // if group enabled arm group or log error to log. 
                if (conf.group[_group] & B1) { 
                  group[_group].setting |= 1;                   // arm group
                  group[_group].arm_delay = conf.arm_delay; // set arm delay
                  _resp = sendCmdToGrp(_group, 10);  // send arming message to all units
                } 
                else { _tmp[0] = 'G'; _tmp[1] = 'F'; _tmp[2] = 48+_group; _tmp[3] = 0; pushToLog(_tmp); }
              }
              break; // no need to try other
            } else { // key is not enabled
              _tmp[0] = 'A'; _tmp[1] = 'F'; _tmp[2] = 48+i; _tmp[3] = 0; pushToLog(_tmp);
            }
          } // key matched
          if (_resp!=0 && i==NUM_OF_KEYS-1) { // may be we should log unknow keys 
            _tmp[0] = 'A'; _tmp[1] = 'U'; _tmp[2] = ' '; _tmp[3] = 0; pushToLog(_tmp);
            memcpy(last_key, RX_msg.buffer, KEY_LEN); // store last unknown key
            last_key[16] = 0;
          }
        } // for
      } // unit is enabled for authorization
      else { // log disabled remote units
        _tmp[0] = 'U'; _tmp[1] = 'F'; _tmp[2] = 48 + RX_msg.address; _tmp[3] = 0; pushToLog(_tmp);
      } 
    } // incoming message looks like a key

    // Registration
    if (RX_msg.ctrl == FLAG_DTA && RX_msg.buffer[0]=='R') {
      _pos = 1;
      do {
        register_t *p = unit_fifo.waitFree(TIME_IMMEDIATE); // Get a free FIFO slot.
        if (p == 0) {
          _tmp[0] = 'F'; _tmp[1] = 'R'; _tmp[2] = 0; pushToLog(_tmp); // Registration queue is full
          return; // Continue if no free space.
        }
        p->unit    = RX_msg.buffer[_pos];    
        p->address = RX_msg.address;
        p->type    = RX_msg.buffer[_pos+1];
        p->number  = (uint8_t)RX_msg.buffer[_pos+2];
        p->setting = (uint8_t)RX_msg.buffer[_pos+3];
        unit_fifo.signalData();   // Signal idle thread data is available.
        _pos+=4;
      } while (_pos < RX_msg.data_length);
    }
    // Sensors
    if (RX_msg.ctrl == FLAG_DTA && RX_msg.buffer[0]=='S') {
      _pos = 1;
      do {
        sensor_t *p = sensor_fifo.waitFree(TIME_IMMEDIATE); // Get a free FIFO slot.
        if (p == 0) {
          _tmp[0] = 'F'; _tmp[1] = 'S'; _tmp[2] = 0; pushToLog(_tmp); // Sensor queue is full
          return; // Continue if no free space.
        }
        p->address = RX_msg.address;
        p->type    = RX_msg.buffer[1];
        p->number  = (uint8_t)RX_msg.buffer[2];
        u.b[0] = RX_msg.buffer[3]; u.b[1] = RX_msg.buffer[4]; u.b[2] = RX_msg.buffer[5]; u.b[3] = RX_msg.buffer[6];
        p->value   = u.fval;
        p->last_OK = nilTimeNow();    // update current timestamp
        sensor_fifo.signalData();     // Signal idle thread data is available.
        _pos+=6;
      } while (_pos < RX_msg.data_length);
    } // if 'S'

  }
}


int readLine(char *line, int maxLen) {
  systime_t start = nilTimeNow();
  systime_t now = nilTimeNow(); 
  int count = 0;
  int cr    = 0;
  int c     = -1;

  WS.print("RL");
  while (c == -1) {
    WS.print(".");
    nilThdSleepMilliseconds(25);
    now = nilTimeNow();
    if (now < start || now - start > 5000) { return 0; }
    c = SMTPethClient.read();
  }

  while (true) {
    WS.print((char)c);
    if (count < maxLen)  { line[count++] = c; }
    if (cr && c == '\n') { break; }
    if (c == '\r') { cr = 1; }
    else           { cr = 0; }
    c = SMTPethClient.read();
  }

  if (count == maxLen - 1) { line[count - 1] = '\0'; }
  else                     { line[count] = '\0';     }
  return count;
}


int readStatus() {
  char line[4];
  int result;
  while(true) {
    result = readLine(line, 4);
    if (result >= 4 && (line[3] != '-')) { break; }
  }

  if (result < 3) { return 0; }

  char st[4];
  strncpy(st, line, 3);
  st[3] = '\0';
  WS.print("rS:");WS.println(st); 
  return atoi(st);
}

//------------------------------------------------------------------------------
// Logger thread
//
NIL_WORKING_AREA(waLoggerThread, 256);
NIL_THREAD(LoggerThread, arg) {
  uint8_t sms_send, sms_ok, _resp, _group;
  int status;
  char    _text[10]; // MQTT out string

  // Initialize GSM modem
  nilSemWait(&GSMSem);    // wait for slot
  GSMisAlive = Serial.ATsendCmd(AT_is_alive);
  if (GSMisAlive) {
    GSMsetSMS = Serial.ATsendCmd(AT_CLIP_ON); 
    GSMsetSMS = Serial.ATsendCmd(AT_set_sms_to_text);
  }
  nilSemSignal(&GSMSem);  // Exit region.

  nilThdSleepSeconds(1); // Sleep before start service thread
  
  #if WEB_SERIAL_DEBUGGING 
  WS.println(F("Logger thread started"));
  #endif
  
  while (TRUE) {
    // Check for data.  Use TIME_IMMEDIATE to prevent sleep in idle thread.
    log_event_t* p = log_fifo.waitData(TIME_INFINITE);
    if (!p) return; // return if no data

    char* log_message = p->text; // Fetch and print data.

    // SMS handler
    //if (GSMisAlive) {
      sms_send = 0; sms_text[0] = 0;
      _group = 255;
      // SMS body
      switch(log_message[13]){
        case 'S':
          strcat_P(sms_text, (char*)text_System); strcat_P(sms_text, (char*)text_sesp);
          switch(log_message[14]){
          case 'B': // Battery
            if (conf.alerts[alert_SMS] >> 11 & B1) sms_send = 1;
            strcat_P(sms_text, (char*)text_BR);
            if (log_message[15] == 'L') strcat_P(sms_text, (char*)text_low);
            else strcat_P(sms_text, (char*)text_high);
            strcat_P(sms_text, (char*)text_space); strcat_P(sms_text, (char*)text_level);
            break;
          case 'A': // AC
            if (conf.alerts[alert_SMS] >> 10 & B1) sms_send = 1;
            strcat_P(sms_text, (char*)text_PWS);
            if (log_message[15] == 'L') strcat_P(sms_text, (char*)text_On);
            else strcat_P(sms_text, (char*)text_Off);
            break;
          case 'C': strcat_P(sms_text, (char*)text_Configuration); strcat_P(sms_text, (char*)text_space);
            switch(tmp[15]){
              case 'W': strcat_P(sms_text, (char*)text_saved); break; // conf. saved
              case 'P': strcat_P(sms_text, (char*)text_saved); strcat_P(sms_text, (char*)text_cosp);
                        strcat_P(sms_text, (char*)text_monitoring); strcat_P(sms_text, (char*)text_space); strcat_P(sms_text, (char*)text_disabled);
                        break; // conf. saved
              case 'L': strcat_P(sms_text, (char*)text_loaded); break; // conf. saved
              case 'R': strcat_P(sms_text, (char*)text_reset); break; // conf. saved
              default:  strcat_P(sms_text, (char*)text_undefined); break; // unknown  
            }
            if (conf.alerts[alert_SMS] >> 9 & B1) sms_send = 1; // conf. saved
            break;
          case 'Z': strcat_P(sms_text, (char*)text_Group); strcat_P(sms_text, (char*)text_space);
                    strcat(sms_text, conf.group_name[log_message[15]-48]); strcat_P(sms_text, (char*)text_space);
                    strcat_P(sms_text, (char*)text_armed);
                    if (conf.alerts[alert_SMS] >> 8 & B1) sms_send = 1;
                    break; // system armed
          case 'S': strcat_P(sms_text, (char*)text_Monitoring); strcat_P(sms_text, (char*)text_space); strcat_P(sms_text, (char*)text_started); if (conf.alerts[alert_SMS] >> 7 & B1) sms_send = 1;
                    break; // monitoring strted
          case 'X': strcat_P(sms_text, (char*)text_ALARM);  if (conf.alerts[alert_SMS] >> 6 & B1) sms_send = 1;
                    strcat_P(sms_text, (char*)text_space); strcat_P(sms_text, (char*)text_Group); strcat_P(sms_text, (char*)text_sesp);
                    strcat(sms_text, conf.group_name[log_message[15]-48]); // extra text for SMS
                    _group = log_message[15]-48;
                    break; // alarm
            /* ****************************** ADD
            case 's': server.printP(text_started); break;   // boot
            case 'M': 
              server.printP(text_MQTT); server.printP(text_space);
              switch(tmp[15]){
                case 'F': server.printP(text_network); server.printP(text_space); server.printP(text_failed); break;
                case 'O': server.printP(text_network); server.printP(text_space); server.printP(text_OK); break;
                default:  server.printP(text_undefined); break; // unknown  
                }
              break;
            */
          default:  strcat_P(sms_text, (char*)text_undefined); 
            // if (conf.alerts[alert_SMS] >> 9 & B1) sms_send = 1;
            break; // unknown
          }
          break;
        case 'P':
          if (conf.alerts[alert_SMS] >> 5 & B1) sms_send = 1;
          strcat_P(sms_text, (char*)text_Alarm); strcat_P(sms_text, (char*)text_sesp); strcat_P(sms_text, (char*)text_triggered); strcat_P(sms_text, (char*)text_space); strcat_P(sms_text, (char*)text_zone); strcat_P(sms_text, (char*)text_space);
          strcat(sms_text, conf.zone_name[log_message[14]-48]);
          _group = (conf.zone[log_message[14]-48] >> 1) & B1111;
          break;
        case 'T':
          if (conf.alerts[alert_SMS] >> 4 & B1) sms_send = 1;
          strcat_P(sms_text, (char*)text_Tamper); strcat_P(sms_text, (char*)text_sesp); strcat_P(sms_text, (char*)text_triggered); strcat_P(sms_text, (char*)text_space); strcat_P(sms_text, (char*)text_zone); strcat_P(sms_text, (char*)text_space);
          strcat(sms_text, conf.zone_name[log_message[14]-48]);
          _group = (conf.zone[log_message[14]-48] >> 1) & B1111;
          break;
        case 'A': // Authentication
          strcat_P(sms_text, (char*)text_Authentication); strcat_P(sms_text, (char*)text_sesp); 
          if (log_message[14] < 'a') { strcat_P(sms_text, (char*)text_key); strcat_P(sms_text, (char*)text_space); }
          if (log_message[14] == 'A' || log_message[14] == 'D' || log_message[14] == 'F') { strcat(sms_text, conf.key_name[log_message[15]-48]); strcat_P(sms_text, (char*)text_space); }  
          switch(log_message[14]){
            case 'D': strcat_P(sms_text, (char*)text_disarmed); if (conf.alerts[alert_SMS] >> 1 & B1) sms_send = 1; break;
            case 'A': strcat_P(sms_text, (char*)text_armed); if (conf.alerts[alert_SMS] >> 2 & B1) sms_send = 1; break;
            case 'U': strcat_P(sms_text, (char*)text_undefined); if (conf.alerts[alert_SMS] & B1) sms_send = 1; break;
            case 'F': strcat_P(sms_text, (char*)text_is); strcat_P(sms_text, (char*)text_space); strcat_P(sms_text, (char*)text_disabled); break;
            case 'a': strcat_P(sms_text, (char*)text_auto); strcat_P(sms_text, (char*)text_space); strcat_P(sms_text, (char*)text_armed);
                      strcat_P(sms_text, (char*)text_space); strcat_P(sms_text, (char*)text_zone); strcat_P(sms_text, (char*)text_space); strcat(sms_text, conf.zone_name[log_message[15]-48]);
                      if (conf.alerts[alert_SMS] >> 2 & B1) sms_send = 1;
                      _group = (conf.zone[log_message[15]-48] >> 1) & B1111;
                      break; // Auto armed
            case 'o': strcat_P(sms_text, (char*)text_zone); strcat_P(sms_text, (char*)text_space); strcat(sms_text, conf.zone_name[log_message[15]-48]);
                      strcat_P(sms_text, (char*)text_space); strcat_P(sms_text, (char*)text_is); strcat_P(sms_text, (char*)text_space); strcat_P(sms_text, (char*)text_open);
                      if (conf.alerts[alert_SMS] >> 12 & B1) sms_send = 1;
                      _group = (conf.zone[log_message[15]-48] >> 1) & B1111;
                      break; // Open alarm              
            default: break;
          }
          break;
        default: // do nothing
        break;
      }
      strcat_P(sms_text, (char*)text_dot);

      /*
      if (sms_send) {
        for (uint8_t i = 0; i < NUM_OF_PHONES; ++i) {
          //  phone enabled           specific group     global tel.
          if ((conf.tel[i] & B1) && ((_group != 255) || (conf.tel[i] >> 5 & B1))) { 
            nilSemWait(&GSMSem);    // wait for slot
            sms_ok = Serial.ATsendSMSBegin(conf.tel_num[i]);
            if (sms_ok) sms_ok = Serial.ATsendSMSEnd(sms_text, sms_send);
            //WS.println(sms_ok);
            while (Serial.ATWaitMsg()) { // wait for GSM modem
              nilThdSleepMilliseconds(200);
            }
            nilSemSignal(&GSMSem);  // Exit region.
          }
        }
      }
      */

      if (sms_send) {
        SMTPethClient.connect("mail.smtp2go.com", 2525);
        status = readStatus();
        SMTPethClient.println("EHLO");
        status = readStatus();
        SMTPethClient.println("auth login");
        status = readStatus();
        WS.println(conf.SMTP_user);
        b64_text[0] = 0;
        WS.print(base64_encode(b64_text, conf.SMTP_user, strlen(conf.SMTP_user)));
        WS.println(b64_text);
        SMTPethClient.println(b64_text);
        status = readStatus();
        WS.println(conf.SMTP_password);
        b64_text[0] = 0;
        WS.print(base64_encode(b64_text, conf.SMTP_password, strlen(conf.SMTP_password)));
        WS.println(b64_text);
        SMTPethClient.println(b64_text);
        status = readStatus();
        SMTPethClient.stop();
        /*
        SMTPethClient.print("MAIL FROM:");
        SMTPethClient.println("<vysocan76@gmail.com>");
        status = readStatus();
        SMTPethClient.print("RCPT TO:");
        SMTPethClient.println("<vysocan76@gmail.com>");
        status = readStatus();
        SMTPethClient.println("DATA ");
        status = readStatus();
        SMTPethClient.println();
        SMTPethClient.println(".");
        status = readStatus();
        //mail.from("OHS <vysocan76@gmail.com>");
        for (uint8_t i = 0; i < NUM_OF_PHONES; ++i) {
          //  phone enabled           specific group     global tel.
          if ((conf.tel[i] & B1) && ((_group != 255) || (conf.tel[i] >> 5 & B1))) { 
            //mail.to(conf.email[i]);  
          }
        }
        //mail.subject(sms_text);
        //mail.body("I can send email from an Arduino!");
        //WS.print(">");WS.println(SMTPclient.send(&mail)); 
        SMTPethClient.stop();
        */
      }

      if (sms_send) {
        if (!sms_ok) log_message[12] = '.';
      } else {
        if (!sms_ok) log_message[12] = ',';
        else         log_message[12] = ';';
      }

    //} // GSMisAlive
    //else log_message[12] = '|'; // modem not connected

    // Put data into external EEPROM
    nilSemWait(&TWISem);          // wait for slot
    eeprom.writeBytes(conf.ee_pos,EEPROM_MESSAGE, log_message);
    nilThdSleepMilliseconds(10);  // wait for EEPROM or we lose very frequent entries
    nilSemSignal(&TWISem);        // Exit region.
    conf.ee_pos += EEPROM_MESSAGE;// Will overflow by itself as size is uint16_t = EEPROM_SIZE

    // MQTT
    nilSemWait(&ETHSem);    // wait for slot
    if (client.connected()) {
      _text[0] = 0;
      strcat_P(_text, (char*)text_OHS); strcat_P(_text, (char*)text_slash); strcat_P(_text, (char*)text_Log);
      client.publish(_text, log_message);
    } else {
      if (MQTTState) {
        _tmp[0] = 'S'; _tmp[1] = 'M'; _tmp[2] = 'F'; _tmp[3] = 0; pushToLog(_tmp);
        MQTTState = 0;
      }
    }
    nilSemSignal(&ETHSem);  // Exit region.

    // Signal FIFO slot is free.
    log_fifo.signalFree();
  }
}

//------------------------------------------------------------------------------
// Service thread
//
NIL_WORKING_AREA(waServiceThread, 160);  
NIL_THREAD(ServiceThread, arg) {
  int8_t  _resp;
  uint8_t _GSMlastStatus = 10; // get status on start
  uint8_t _text[10];
  msg_t   _smpR;
  uint8_t _counter = 0; // used for pause in alarm

  nilThdSleepSeconds(5); // Sleep before start service thread
  // NTP Sync
  time_now = GetNTPTime(udp);  
  if (time_now.get() > 0) RTC.adjust(time_now.get());
  
  nilThdSleepSeconds(1); // Sleep before start service thread
  // Call for registration of units
  _resp = sendCmd(15,1); nilThdSleepMilliseconds(200);

  #if WEB_SERIAL_DEBUGGING 
  WS.println(F("Service thread started"));
  #endif  

  while (TRUE) {
    nilThdSleepSeconds(10);
    _counter++; 

    // Auto arm
    for (int8_t i=0; i < ALR_ZONES ; i++){
      if ((conf.zone[i] & B1) && ((conf.zone[i] >> 7) & B1)){ // Zone enabled and auto arming
        if (nilTimeNowIsWithin(zone[i].last_PIR + S2ST((conf.auto_arm * 60)-8), zone[i].last_PIR + S2ST((conf.auto_arm * 60)+8))) {
          uint8_t _group = (conf.zone[i] >> 1) & B1111;
          _tmp[0] = 'A'; _tmp[1] = 'a'; _tmp[2] = 48 + _group; _tmp[3] = 0; pushToLog(_tmp); // Authorization auto arm
          // if group is enabled arm zone else log error to log
          if (conf.group[_group] & B1) { group[(conf.zone[i] >> 1) & B1111].setting |= 1; } // arm group
          else  {_tmp[0] = 'G'; _tmp[1] = 'F'; _tmp[2] = 48+_group; _tmp[3] = 0; pushToLog(_tmp);}
        }
      }
    }
    // Open alarm
    for (int8_t i=0; i < ALR_ZONES ; i++){
      if ((conf.zone[i] & B1) && ((conf.zone[i] >> 8) & B1)){ // Zone enabled and still open alarm enabled
        if (nilTimeNowIsWithin(zone[i].last_OK + S2ST((conf.open_alarm * 60)-8), zone[i].last_OK + S2ST((conf.open_alarm * 60)+8))) {
          _tmp[0] = 'A'; _tmp[1] = 'o'; _tmp[2] = 48 + i; _tmp[3] = 0; pushToLog(_tmp); // Authorization still open alarm
          zone[i].last_OK = nilTimeNow();    // update current timestamp
        }
      }
    }

    // Battery check 
    if (pinBAT_OK.read() == LOW) { // The signal is "Low" when the voltage of battery is under 11V 
      _tmp[0] = 'S'; _tmp[1] = 'B'; _tmp[2] = 'L'; _tmp[3] = 0; pushToLog(_tmp); // battery low
      _tmp[0] = 'S'; _tmp[1] = 'C'; _tmp[2] = 'P'; _tmp[3] = 0; pushToLog(_tmp); // conf saved 
      log_event_t *p;
      do { 
        nilThdSleepMilliseconds(100);
      } while (p); // wait for logger thread
      conf.power_loss = 1; // flag that we are out of power
      eeprom_update_block((const void*)&conf, (void*)0, sizeof(conf)); // Save current configuration
      eeprom_update_block((const void*)&group, (void*)sizeof(conf), sizeof(group)); // Save current configuration

      nilSysLock(); // Lock RTOS  

      // Battery is at low level but it might oscillate, so we wait for AC to recharge battery again.
      while (pinAC_OFF.read() == HIGH) { // The signal turns to be "High" when the power supply turns OFF
        delay(1000); // do nothing wait for power supply shutdown
      } 

      conf.power_loss = 0; // un-flag that we are out of power
      nilSysUnlock(); // in case the power is restored we go on
      _tmp[0] = 'S'; _tmp[1] = 'A'; _tmp[2] = 'L'; _tmp[3] = 0; pushToLog(_tmp); // AC ON
      _tmp[0] = 'S'; _tmp[1] = 'B'; _tmp[2] = 'H'; _tmp[3] = 0; pushToLog(_tmp); // Battery High
    }

    // AC power check
    if (pinAC_OFF.read() == LOW && ACState != LOW) { // The signal turns to be "High" when the power supply turns OFF
      ACState = LOW;
      _tmp[0] = 'S'; _tmp[1] = 'A'; _tmp[2] = 'L'; _tmp[3] = 0; pushToLog(_tmp); // AC ON
    }
    if (pinAC_OFF.read() == HIGH && ACState != HIGH) { // The signal turns to be "High" when the power supply turns OFF
      ACState = HIGH;
      _tmp[0] = 'S'; _tmp[1] = 'A'; _tmp[2] = 'H'; _tmp[3] = 0; pushToLog(_tmp); // AC OFF
    }

    if ((_counter%12) == 0) { // GSM modem check every 2 minutes
      _smpR = nilSemWaitTimeout(&GSMSem, TIME_IMMEDIATE); // look if slot is free
      if (_smpR != NIL_MSG_TMO) { // slot is free 
        //WS.println("gsm slot");
        GSMisAlive = Serial.ATsendCmd(AT_is_alive); 
        if (GSMisAlive) {
          //WS.println("gsm alive");
          if (!GSMsetSMS) {
            //_resp = Serial.ATsendCmd(AT_CLIP_ON);             // CLI On
            GSMsetSMS = Serial.ATsendCmd(AT_set_sms_to_text); // set modem to text SMS format
          }
          _resp = Serial.ATsendCmdWR(AT_registered, _text, 3);
          GSMreg = strtol((char*)_text, NULL, 10);
          _resp = Serial.ATsendCmdWR(AT_signal_strength, _text, 2);
          GSMstrength= strtol((char*)_text, NULL, 10);
        } else { GSMreg = 4; GSMstrength = 0; GSMsetSMS = 0;}
        nilSemSignal(&GSMSem);  // Exit region.
        //WS.println("gsm slot exit");
        if (_GSMlastStatus != GSMreg) { // if modem registration changes log it
          _GSMlastStatus = GSMreg;
          _tmp[0] = 'M'; _tmp[1] = 48+GSMreg; _tmp[2] = 48+GSMstrength; _tmp[3] = 0; pushToLog(_tmp); 
        }
      }
    }

    // OUT 1 & 2 handling
    if ((_counter%6) == 0) { // cycle every minute
      pinOUT1.write(LOW);
      pinOUT2.write(LOW);
    } else {
      pinOUT1.write(((OUTs >> 0) & B1));
      pinOUT2.write(((OUTs >> 1) & B1));
    }

    //   MQTT connected
    nilSemWait(&ETHSem);    // wait for slot
    if (!client.connected()) {
      if (!client.connect(str_MQTT_dev)) {       
        // Ethernet
        Ethernet.begin(mac, our_ip, gateway, gateway, subnet);
      } else {
        if (!MQTTState){
          _tmp[0] = 'S'; _tmp[1] = 'M'; _tmp[2] = 'O'; _tmp[3] = 0; pushToLog(_tmp);
          MQTTState = 1;
        }
      }
    }
    nilSemSignal(&ETHSem);  // Exit region.
    
    if (Serial.isMsg()) {
      _resp = Serial.read((uint8_t*)sms_text);                     // read serial
      #if WEB_SERIAL_DEBUGGING
      WS.print(_resp);
      for(uint8_t i = 0; i < _resp; i++) {
        WS.print((char)sms_text[i]);
      }
      WS.println();
      #endif
    }
    
  }
}

//------------------------------------------------------------------------------
// RFM12B thread 
//
NIL_WORKING_AREA(waRadioThread, 128);  
NIL_THREAD(RadioThread, arg) {
  uint8_t  _pos;

  #if WEB_SERIAL_DEBUGGING 
  WS.println(F("Radio thread started"));    
  #endif

  while (TRUE) {
    nilThdSleepMilliseconds(50);
    if (radio.ReceiveComplete()) {
      nilSemWait(&RFMSem); 
      ++radio_received;
      if (radio.CRCPass()) {
        // Registration
        // WS.print(*radio.DataLen); WS.println(">");
        // for (uint8_t _t=0; _t < *radio.DataLen; _t++) {
        //   WS.print(radio.Data[_t], HEX); WS.print(":");
        // }
        // WS.println("<");
        if ((char)radio.Data[0] == 'R') {
          _pos = 1;
          do {
            register_t *p = unit_fifo.waitFree(TIME_IMMEDIATE); // Get a free FIFO slot.
            if (p == 0) {
              _tmp[0] = 'F'; _tmp[1] = 'R'; _tmp[2] = 0; pushToLog(_tmp); // Registration queue is full
              return; // Continue if no free space.
            }
            p->unit    = (char)radio.Data[_pos];    
            p->address = radio.GetSender()+RADIO_UNIT_OFFSET;
            p->type    = radio.Data[_pos+1];
            p->number  = (uint8_t)radio.Data[_pos+2];
            p->setting = (uint8_t)radio.Data[_pos+3];
            unit_fifo.signalData();   // Signal idle thread data is available.
            _pos+=4;
          } while (_pos < *radio.DataLen);
        }

        // Sensors
        if ((char)radio.Data[0] == 'S') {
          _pos = 1;
          do {
            sensor_t *p = sensor_fifo.waitFree(TIME_IMMEDIATE); // Get a free FIFO slot.
            if (p == 0) {
              _tmp[0] = 'F'; _tmp[1] = 'S'; _tmp[2] = 0; pushToLog(_tmp); // Sensor queue is full
              return; // Continue if no free space.
            }    
            p->address = radio.GetSender()+RADIO_UNIT_OFFSET;
            p->type    = radio.Data[_pos];
            p->number  = (uint8_t)radio.Data[_pos+1];
            u.b[0] = radio.Data[_pos+2]; u.b[1] = radio.Data[_pos+3]; u.b[2] = radio.Data[_pos+4]; u.b[3] = radio.Data[_pos+5];
            p->value   = u.fval;
            p->last_OK = nilTimeNow();    // update current timestamp
            sensor_fifo.signalData();   // Signal idle thread data is available.
            _pos+=6;
          } while (_pos < *radio.DataLen);
        } // if 'S'

        if (radio.ACKRequested()) radio.SendACK();
      }
      else {
        ++radio_bad_crc;
      }
      nilSemSignal(&RFMSem);  // Exit region.
    }
  }
}

//------------------------------------------------------------------------------
// Sensor thread 
//
NIL_WORKING_AREA(waSensorThread, 128);  
NIL_THREAD(SensorThread, arg) {
  uint8_t _unit, _found, _trigger;
  char _value[7];
  char _text[40];

  #if WEB_SERIAL_DEBUGGING 
  WS.println(F("Sensor thread started"));    
  #endif

  while (TRUE) {
    // Check for data.  Use TIME_IMMEDIATE to prevent sleep in idle thread.
    sensor_t* p = sensor_fifo.waitData(TIME_INFINITE);
    if (!p) return; // return if no data

    _found = 0;
    for (_unit = 0; _unit < sensors; _unit++) {
      //  sensor enabled          
      if (sensor[_unit].setting & B1) { 
      if ((sensor[_unit].address == p->address) && (sensor[_unit].type == p->type) && (sensor[_unit].number == p->number)) { 
        sensor[_unit].value   = p->value;
        sensor[_unit].last_OK = p->last_OK;
        
        //   MQTT connected           MQTT sensor enabled
        if ((client.connected()) && ((sensor[_unit].setting >> 7) & B1)) {
          _text[0] = 0;
          // Create MQTT string
          strcat_P(_text, (char*)text_OHS); strcat_P(_text, (char*)text_slash);
          strcat(_text, conf.group_name[(sensor[_unit].setting >> 1) & B1111]); strcat_P(_text, (char*)text_slash);
          strcat_P(_text, (char*)text_Sensor); strcat_P(_text, (char*)text_slash);
          switch(sensor[_unit].type){
            case 'T': strcat_P(_text, (char*)text_Temperature); break;
            case 'H': strcat_P(_text, (char*)text_Humidity); break;
            case 'P': strcat_P(_text, (char*)text_Pressure); break;
            default : strcat_P(_text, (char*)text_Undefined); break;
          }
          strcat_P(_text, (char*)text_slash);
          itoa(sensor[_unit].number, _value, 10); strcat(_text, _value);
          dtostrf(sensor[_unit].value, 6, 2, _value); // value to string
          
          nilSemWait(&ETHSem);    // wait for slot
          client.publish(_text,_value);
          nilSemSignal(&ETHSem);  // Exit region.
        }
        if (!client.connected()) {
          if (MQTTState) {
            _tmp[0] = 'S'; _tmp[1] = 'M'; _tmp[2] = 'F'; _tmp[3] = 0; pushToLog(_tmp);
            MQTTState = 0;
          }
        }    

        // Triggers
        for (_trigger = 0; _trigger < triggers; _trigger++) {
          //  trigger enabled
          WS.print(">");WS.print(trigger[_trigger].address);
          WS.print(">");WS.print(trigger[_trigger].type);
          WS.print(">");WS.print(trigger[_trigger].number);
          WS.print(">");WS.print(trigger[_trigger].setting, BIN);
          WS.println();
          if (trigger[_trigger].setting & B1) { 
            if (trigger[_trigger].address == sensor[_unit].address && 
                trigger[_trigger].type    == sensor[_unit].type &&
                trigger[_trigger].number  == sensor[_unit].number) {
              // check value
              _found = 0;
              switch(trigger[_trigger].type){
                case '=': if (trigger[_trigger].value == sensor[_unit].value) _found = 1; break;
                case '<': if (trigger[_trigger].value < sensor[_unit].value) _found = 1; break;
                case '>': if (trigger[_trigger].value > sensor[_unit].value) _found = 1; break;
                case '/': if (trigger[_trigger].value != sensor[_unit].value) _found = 1; break;
                case 'A': _found = 1; break; break; // Always
                default : break;
              }
              if (_found) {

              }
            
            } // trigger match
          } // trigger enabled
        } // for (trigger)
        _found = 1;
        break; // no need to look for other unit
      } // if address 
      } // enbled
    } // for
    if (!_found) {
      // call this address to register
      _unit = sendCmd(p->address,1);
    }
    
    // Signal FIFO slot is free.
    sensor_fifo.signalFree();
  }
}

//------------------------------------------------------------------------------
// Registration thread 
//
NIL_WORKING_AREA(waRegThread, 128);  
NIL_THREAD(RegThread, arg) {
  uint8_t _unit, _resp;

  #if WEB_SERIAL_DEBUGGING 
  WS.println(F("Registration thread started"));    
  #endif

  while (TRUE) {
    // Check for data.  Use TIME_IMMEDIATE to prevent sleep in idle thread.
    register_t* p = unit_fifo.waitData(TIME_INFINITE);
    if (!p) return; // return if no data

    switch(p->unit){
      case 'K': // Key
        _resp = 1;
        for (_unit=0; _unit < units; _unit++) {
          if (unit[_unit].address == p->address && unit[_unit].type == p->type && unit[_unit].number == p->number) {
            _resp = 0;
            break;
          }
        }
        if (_resp) { // if unit not present already
          unit[units].address = p->address;
          unit[units].type    = p->type;
          unit[units].number  = p->number;
          unit[units].setting = p->setting;
          _tmp[0] = 'U'; _tmp[1] = 'R'; _tmp[2] = 48 + unit[units].address; _tmp[3] = 0; pushToLog(_tmp);
          // WS.print(F("Key reg: ")); WS.println(units); WS.print(unit[units].address); WS.print(unit[units].type);
          // WS.println(unit[units].number); WS.println(unit[units].setting,BIN); 
          units++;
        } else {
          unit[_unit].address = p->address;
          unit[_unit].type    = p->type;
          unit[_unit].number  = p->number;
          unit[_unit].setting = p->setting;
          _tmp[0] = 'U'; _tmp[1] = 'r'; _tmp[2] = 48 + unit[_unit].address; _tmp[3] = 0; pushToLog(_tmp);
          // WS.print(F("Key rereg: ")); WS.println(_unit+1); WS.print(unit[_unit].address); WS.print(unit[_unit].type);
          // WS.println(unit[_unit].number); WS.println(unit[_unit].setting,BIN); 
        }
        break;
      case 'S': // Sensor
        _resp = 1;
        for (_unit=0; _unit < sensors; _unit++) {
          if (sensor[_unit].address == p->address && sensor[_unit].type == p->type && sensor[_unit].number == p->number) {
            _resp = 0;
            break;
          }
        }
        if (_resp) { // if unit not present already
          sensor[sensors].address = p->address;
          sensor[sensors].type    = p->type;
          sensor[sensors].number  = p->number;
          sensor[sensors].setting = p->setting;
          sensor[sensors].value   = 0;
          sensor[sensors].last_OK = nilTimeNow();    // update current timestamp
          _tmp[0] = sensor[sensors].type + 32; _tmp[1] = 'R'; _tmp[2] = 48 + sensor[sensors].address; _tmp[3] = 0; pushToLog(_tmp);
          // WS.print(F("Sens reg: ")); WS.println(sensors); WS.print(sensor[sensors].address);
          // WS.print(sensor[sensors].type); WS.println(sensor[sensors].number); WS.println(sensor[sensors].setting,BIN);
          // WS.println(sensor[sensors].value); 
          sensors++;
        } else {
          sensor[_unit].address = p->address;
          sensor[_unit].type    = p->type;
          sensor[_unit].number  = p->number;
          sensor[_unit].setting = p->setting;
          sensor[_unit].value   = 0;
          sensor[_unit].last_OK = nilTimeNow();    // update current timestamp
          _tmp[0] = sensor[_unit].type + 32; _tmp[1] = 'r'; _tmp[2] = 48 + sensor[_unit].address; _tmp[3] = 0; pushToLog(_tmp);
          // WS.print(F("Sens rereg: ")); WS.println(_unit+1); WS.print(sensor[_unit].address);
          // WS.print(sensor[_unit].type); WS.println(sensor[_unit].number);
          // WS.println(sensor[_unit].setting,BIN); WS.println(sensor[_unit].value);

        }
        break;
        default: _tmp[0] = 'U'; _tmp[1] = 'E'; _tmp[2] = 0; pushToLog(_tmp); 
        break;
    }
    // Signal FIFO slot is free.
    unit_fifo.signalFree();
  }
}

//------------------------------------------------------------------------------
// Web server thread Last one lowest priority
//
NIL_WORKING_AREA(waWebThread, 320);  
NIL_THREAD(WebThread, arg) {
  
  #if WEB_SERIAL_DEBUGGING 
  WS.println(F("Web thread started"));    
  #endif

  while (TRUE) {
    nilThdSleepMilliseconds(50);
    nilSemWait(&ETHSem);    // wait for slot
    webserver.processConnection();
    nilSemSignal(&ETHSem);  // Exit region.
    
    nilThdSleepMilliseconds(10);
    nilSemWait(&ETHSem);    // wait for slot
    client.loop(); // MQTT
    nilSemSignal(&ETHSem);  // Exit region.
    //if (!client.loop()) WS.println(F("MQTT loop NC")); // MQTT
  }
}

//------------------------------------------------------------------------------
// 
NIL_WORKING_AREA(waThread9, 128);  
NIL_THREAD(Thread9, arg) {
  uint8_t tt = 0;

  #if WEB_SERIAL_DEBUGGING 
  WS.println(F("Debug started"));  
  #endif
  
  while (TRUE) {
    nilThdSleepSeconds(1);    
    //WS.print(nilTimeNow());
    //nilPrintStackSizes(&WS);
    //nilPrintUnusedStack(&WS);
    idlePointer++; if (idlePointer==idleSlots) { idlePointer=0;
      //nilPrintUnusedStack(&WS);
      //WS.print(strlen(conf.b64_SMTP_user)); WS.print(" "); WS.println(conf.b64_SMTP_user);
      //WS.print(strlen(conf.b64_SMTP_password)); WS.print(" "); WS.println(conf.b64_SMTP_password);
    }
    idleCount[idlePointer] = 0; // reset idle
  }
}

//------------------------------------------------------------------------------
/*
 * Threads static table, one entry per thread.  A thread's priority is
 * determined by its position in the table with highest priority first.
 *
 * These threads start with a null argument.  A thread's name is also
 * null to save RAM since the name is currently not used.
 */
 NIL_THREADS_TABLE_BEGIN()
 NIL_THREADS_TABLE_ENTRY(NULL, ZoneThread, NULL, waZoneThread, sizeof(waZoneThread))
 NIL_THREADS_TABLE_ENTRY(NULL, thdFcn, (void*)"AET 1", waAEThread1, sizeof(waAEThread1))
 NIL_THREADS_TABLE_ENTRY(NULL, thdFcn, (void*)"AET 2", waAEThread2, sizeof(waAEThread2))
 NIL_THREADS_TABLE_ENTRY(NULL, thdFcn, (void*)"AET 3", waAEThread3, sizeof(waAEThread3))
 NIL_THREADS_TABLE_ENTRY(NULL, RS485RXThread, NULL, waRS485RXThread, sizeof(waRS485RXThread))
 NIL_THREADS_TABLE_ENTRY(NULL, LoggerThread, NULL, waLoggerThread, sizeof(waLoggerThread))
 NIL_THREADS_TABLE_ENTRY(NULL, ServiceThread, NULL, waServiceThread, sizeof(waServiceThread))
 NIL_THREADS_TABLE_ENTRY(NULL, RadioThread, NULL, waRadioThread, sizeof(waRadioThread))
 NIL_THREADS_TABLE_ENTRY(NULL, SensorThread, NULL, waSensorThread, sizeof(waSensorThread))
 NIL_THREADS_TABLE_ENTRY(NULL, RegThread, NULL, waRegThread, sizeof(waRegThread))
 NIL_THREADS_TABLE_ENTRY(NULL, WebThread, NULL, waWebThread, sizeof(waWebThread))
 NIL_THREADS_TABLE_ENTRY(NULL, Thread9, NULL, waThread9, sizeof(waThread9))

 NIL_THREADS_TABLE_END()

//------------------------------------------------------------------------------
 void setup() {
  
  /*
  // On start set groups
  for (int8_t i=0; i < ALR_GROUPS ; i++){
    group[i].setting   = 0;
    group[i].arm_delay = 0;
  }
  // On start set zones
  for (int8_t i=0; i < ALR_ZONES ; i++){
    zone[i].last_PIR   = 0;
    zone[i].last_OK    = 0;
    zone[i].last_event = 'N';
  }
  */

  // Read current configuration
  eeprom_read_block((void*)&conf, (void*)0, sizeof(conf)); 
  if (conf.version != VERSION) { 
    setDefault();
    // encode default user pass 
    char _text[20];
    _text[0] = 0;
    strcat (_text, conf.user);
    strcat (_text, ":");
    strcat (_text, conf.password);
    base64_encode(conf.user_pass, _text, strlen(_text));
  }

  // Read group setting if power was lost
  if (conf.power_loss) {
    eeprom_read_block((void*)&group, (void*)sizeof(conf), sizeof(group));
    conf.power_loss = 0;
  }

  //Serial.begin(9600); // GSM modem
  Serial.begin(115200); 
  #if WEB_SERIAL_DEBUGGING 
  WS.println(F("Start"));
  #endif

  // RFM12B radio
  radio.Initialize(RADIO_NODEID, RADIO_FREQUENCY, RADIO_NETWORKID);
  radio.Encrypt(conf.radioKey); //comment this out to disable encryption

  RS485.begin(19200, 0); // RS485 network, speed, our address  

  // ADC_PS_64 - 250kHz, ADC speed for 16MHz, faster readings less time for idle
  //  - ADC_PS_128: Arduino default (125 kHz on a 16 MHz CPU)
  //  - ADC_PS_64: (250 kHz on a 16 MHz CPU)
  //  - ADC_PS_32: (500 kHz on a 16 MHz CPU)
  nilAnalogPrescalar(ADC_PS_64); 

  // TWI 
  Wire.begin(); Wire.speed(I2C_400KHZ);

  // Ethernet
  Ethernet.begin(mac, our_ip, gateway, gateway, subnet);
  //WS.println(Ethernet.begin(mac));

  // MQTT connect
  if (client.connect(str_MQTT_dev)) {
    client.subscribe("OHS/In/#");;
  }

  // Web server
  webserver.begin();
  webserver.setDefaultCommand(&webHome);
  webserver.addCommand("h", &webHome);
  webserver.addCommand("l", &webListLog);
  webserver.addCommand("z", &webSetZone);
  webserver.addCommand("g", &webSetGroup);
  webserver.addCommand("r", &webSetGlobal);
  webserver.addCommand("c", &webSetPhone);
  #if WEB_SERIAL_DEBUGGING 
  webserver.addCommand("d", &webDebug);
  #endif
  webserver.addCommand("a", &webSetAuth);
  webserver.addCommand("s", &webSetSens);
  webserver.addCommand("k", &webSetKey);
  webserver.addCommand("m", &webSetMQTT);
  webserver.addCommand("t", &webSetTriggers);

  // start kernel
  nilSysBegin();
  time_started = RTC.now();
  _tmp[0] = 'S'; _tmp[1] = 's'; _tmp[2] = 0; pushToLog(_tmp);
}
//------------------------------------------------------------------------------
// Loop is the idle thread.  The idle thread must not invoke any
// kernel primitive able to change its state to not runnable.
void loop() {
  while (1) {
    // Disable interrupts to insure increment is atomic.
    nilSysLock();
    idleCount[idlePointer]++;
    nilSysUnlock();
  }
}


/*
Zone thread started
-sms b-AT+CMGS="+420731435556"
-sms s1-> System: Configuration undefined.
-sms s2-+CMGS: 111
-sms el-
-sms s3-OK
-sms b-AT+CMGS="+420732742788"
-sms s1-> System: Configuration undefined.
-sms s2-+CMGS: 112
-sms el-
-sms s3-OK
-sms b-AT+CMGS="+420731435556"
-sms s1-uthentication: zone Vstupni dvere is open.
-sms b-AT+CMGS="+420732742788"
-sms s1-> Authentication: zone Vstupni dvere is open.
-sms s2-+CMGS: 114
-sms el-
-sms s3-OK
-sms b-AT+CMGS="+420731435556"
-sms s1-uthentication: zone Vstupni dvere is open.
-sms b-AT+CMGS="+420732742788"
-sms s1-> Authentication: zone Vstupni dvere is open.
-sms s2-+CMGS: 116
-sms el-
-sms s3-OK
4AT+C
5ERROR
*/