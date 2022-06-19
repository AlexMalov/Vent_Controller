const char* mqtt_payloadAvailable = "online";
const char* mqtt_payloadNotAvailable = "offline";

#define changeLightSpeed 40       // 40 - соответствует 4 секундам. Для регулирования яркости ламп при удержании
#define LinkCheckPeriod 5000        // период проверки подключения к сети
#define MqttReconnectPeriod 10000   // период переподключения к Mqtt при обрыве

#include "triacPLC.h"
#include <SPI.h>
#include "IWatchdog.h"
#include <ArduinoJson.h>
#include <GyverOLED.h>

GyverOLED<SSD1306_128x64, OLED_BUFFER> plcOLED(0x3C);
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);//(server, 1883, mqttCallback, ethClient);


triacPLC::triacPLC(){
  _DSsensors.setAddress((uint8_t*)DSaddrs);                                                           // массив с адресами датчиков температуры
  for (uint8_t i = 0; i < fansAmount; i++) _fans[i].baseRomAddr = channelAmount*2 + 1;                // базовый адрес для сохранения состояния вентиляторов в eeprom
  for (uint8_t i = 0; i < relaysAmount; i++) _relays[i].baseRomAddr = _fans[0].baseRomAddr + fansAmount*2 + 1;     // базовый адрес для сохранения состояния реле в eeprom
  loadEEPROM();
}

void triacPLC::rtDisplay(){
  __HAL_RCC_I2C1_CLK_ENABLE();
  plcOLED.flipV(flipDisplay);   // вращаем дисплей если надо
  plcOLED.flipH(flipDisplay);
  __HAL_RCC_I2C1_CLK_DISABLE();  
}

bool triacPLC::begin(MQTT_CALLBACK_SIGNATURE){         //инициализация дисплея и сетевой карты
  bool res = true;
  pinF1_DisconnectDebug(PB_3);  // disable Serial wire JTAG configuration
  pinF1_DisconnectDebug(PB_4);  // disable Serial wire JTAG configuration
  pinF1_DisconnectDebug(PA_15); // disable Serial wire JTAG configuration
  SPI.setMOSI(PB5);             // переносим SPI1 на альтернаывные пины
  SPI.setMISO(PB4);
  SPI.setSCLK(PB3);
  delay(45);                    // ждем дисплей 
  plcOLED.init();               //инициализируем дисплей  myOLED.begin();
  plcOLED.clear();              //Очищаем буфер дисплея.
  rtDisplay();                  // усанавливаем ориентацию дисплея
  plcOLED.print(F("PLC I0T Vent"));
  plcOLED.setCursorXY(0, 56);
  plcOLED.print(F("\115\105\130\101\124\120\117\110\40\104\111\131"));
  display();
  
// start the Ethernet connection:
  IPAddress ip; memcpy( &ip[0], inet_cfg.ip, sizeof(inet_cfg.ip));                   
  IPAddress myDNS; memcpy( &myDNS[0], inet_cfg.dns, sizeof(inet_cfg.dns));
  IPAddress gateway; memcpy( &gateway[0], inet_cfg.gw, sizeof(inet_cfg.gw));
  IPAddress subnet; memcpy( &subnet[0], inet_cfg.mask, sizeof(inet_cfg.mask));
  plcOLED.setCursorXY(0, 16);
  Ethernet.init(PA15);
  bool dhcpOK = false;
  delay(5000);                    // ждем сетевуху
  Ethernet.linkStatus();
  if (inet_cfg.use_dhcp and (Ethernet.hardwareStatus() != EthernetNoHardware)) {
    plcOLED.print(F("Init DHCP.."));
    display();
    #ifdef DEBUGPLC 
      Serial.print(F("Init Ethernet with DHCP: ")); 
    #endif
    dhcpOK = Ethernet.begin(inet_cfg.mac, 6000, 4000);
    if (!dhcpOK){                         // 
      #ifdef DEBUGPLC 
        Serial.println(F("failed"));         
      #endif
      plcOLED.println(F("fail"));
      plcOLED.print(F("Init staticIP.."));
      display();
    }
  } else {
    plcOLED.print(F("Init staticIP.."));
    display(); 
  }
  if (!dhcpOK) Ethernet.begin(inet_cfg.mac, ip, myDNS, gateway);
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    plcOLED.print(F("No Eth Card"));
    #ifdef DEBUGPLC 
      Serial.println(F("No Eth Card")); 
    #endif
    res = false;
  } else {
    plcOLED.print(F("OK"));
    #ifdef DEBUGPLC 
      Serial.println(F("OK")); 
    #endif
  }
  plcOLED.setCursorXY(0, 8);
  plcOLED.print(F("IP: "));
  plcOLED.print(Ethernet.localIP());
  display();
  #ifdef DEBUGPLC 
    Serial.print(F("My ip: "));
    Serial.println(Ethernet.localIP());
  #endif
  //if (Ethernet.hardwareStatus() != EthernetNoHardware) Ethernet.maintain();
  if (mqtt_cfg.useMQTT){
    IPAddress mqttBrokerIP;
    memcpy( &mqttBrokerIP[0], mqtt_cfg.SrvIP, sizeof(mqtt_cfg.SrvIP));
    mqttClient.setServer(mqttBrokerIP, mqtt_cfg.SrvPort);
    mqttClient.setCallback(callback);
    char HA_autoDiscovery[40];

    //-----=== Dimmmers ===----------
    strcpy(HA_autoDiscovery, mqtt_cfg.HADiscover);
    strcat(HA_autoDiscovery, "/light/");
    strcat(HA_autoDiscovery, mqtt_cfg.ClientID);    // "HomeAssistant/light/plc1Traic";

  //  String st1 = HomeAssistantDiscovery;
  //  st1 += "/light/" + plcName;
    for (byte i = 1; i <= channelAmount; i++) {
      _dimmers[i-1].useMQTT = true;
      char tmp_str[65];
      strcpy(tmp_str, HA_autoDiscovery); 
      char num[3]; itoa(i, num, DEC);
      strcat(tmp_str, num);              // "HomeAssistant/light/plc1Traic1..9"; 
      strcat(tmp_str, "/state");        // "HomeAssistant/light/plc1Traic1..9/state"; 
      _dimmers[i-1].mqtt_topic_state = new char[strlen(tmp_str) + 1];
      strcpy(_dimmers[i-1].mqtt_topic_state, tmp_str);
      strcpy(tmp_str, HA_autoDiscovery); 
      strcat(tmp_str, num);              // "HomeAssistant/light/plc1Traic1..9"; 
      strcat(tmp_str, "/bri_state");        // "HomeAssistant/light/plc1Traic1..9/bri_state";
      _dimmers[i-1].mqtt_topic_bri_state = new char[strlen(tmp_str)+1];
      strcpy(_dimmers[i-1].mqtt_topic_bri_state, tmp_str);
    }

    //-----=== Buttons ===----------
    strcpy(HA_autoDiscovery, mqtt_cfg.HADiscover);
    strcat(HA_autoDiscovery, "/button/");           //button
    strcat(HA_autoDiscovery, mqtt_cfg.ClientID);    // "HomeAssistant/button/plc1Traic";

    for (byte i = 1; i <= btnsAmount; i++) {
      _btns[i-1].useMQTT = true;
      char tmp_str[65];
      strcpy(tmp_str, HA_autoDiscovery); 
      char num[3]; itoa(i, num, DEC);
      strcat(tmp_str, num);              // "HomeAssistant/switch/plc1Traic1..9"; 
      strcat(tmp_str, "/set");        // "HomeAssistant/switch/plc1Traic1..9/set"; 
      _btns[i-1].mqtt_topic_cmd = new char[strlen(tmp_str) + 1];
      strcpy(_btns[i-1].mqtt_topic_cmd, tmp_str);
    }

    //-----=== Fans ===----------
    strcpy(HA_autoDiscovery, mqtt_cfg.HADiscover);
    strcat(HA_autoDiscovery, "/fan/");
    strcat(HA_autoDiscovery, mqtt_cfg.ClientID);    // "HomeAssistant/fan/plc1Traic";

  //  String st1 = HomeAssistantDiscovery;
  //  st1 += "/fan/" + plcName;
    for (byte i = 1; i <= fansAmount; i++) {
      _fans[i-1].useMQTT = true;
      char tmp_str[65];
      strcpy(tmp_str, HA_autoDiscovery); 
      char num[3]; itoa(i, num, DEC);
      strcat(tmp_str, num);              // "HomeAssistant/fan/plc1Traic1..9"; 
      strcat(tmp_str, "/state");        // "HomeAssistant/fan/plc1Traic1..9/state"; 
      _fans[i-1].mqtt_topic_state = new char[strlen(tmp_str) + 1];
      strcpy(_fans[i-1].mqtt_topic_state, tmp_str);
      strcpy(tmp_str, HA_autoDiscovery); 
      strcat(tmp_str, num);              // "HomeAssistant/fan/plc1Traic1..9"; 
      strcat(tmp_str, "/pct_stat_t");        // "HomeAssistant/fan/plc1Traic1..9/pct_stat_t";     percentage_state_topic
      _fans[i-1].mqtt_topic_bri_state = new char[strlen(tmp_str)+1];
      strcpy(_fans[i-1].mqtt_topic_bri_state, tmp_str);
    }

    //-----=== Relays ===----------
    strcpy(HA_autoDiscovery, mqtt_cfg.HADiscover);
    strcat(HA_autoDiscovery, "/switch/");           //relay
    strcat(HA_autoDiscovery, mqtt_cfg.ClientID);    // "HomeAssistant/switch/plc1Traic";

    for (byte i = 1; i <= relaysAmount; i++) {
      _relays[i-1].useMQTT = true;
      char tmp_str[65];
      strcpy(tmp_str, HA_autoDiscovery); 
      char num[3]; itoa(i, num, DEC);
      strcat(tmp_str, num);              // "HomeAssistant/switch/plc1Traic1..9"; 
      strcat(tmp_str, "/state");        // "HomeAssistant/switch/plc1Traic1..9/state"; 
      _relays[i-1].mqtt_topic_state = new char[strlen(tmp_str) + 1];
      strcpy(_relays[i-1].mqtt_topic_state, tmp_str);
    }

    //-----=== Temp sensors ===----------
    strcpy(HA_autoDiscovery, mqtt_cfg.HADiscover);
    strcat(HA_autoDiscovery, "/sensor/");
    strcat(HA_autoDiscovery, mqtt_cfg.ClientID);    // "HomeAssistant/sensor/plc1Traic";

    for (byte i = 1; i <= DS_SENS_AMOUNT; i++) {
      _DSsensors.useMQTT[i-1] = true;
      char tmp_str[65];
      strcpy(tmp_str, HA_autoDiscovery); 
      char num[3]; itoa(i, num, DEC);
      strcat(tmp_str, num);              // "HomeAssistant/fan/plc1Traic1..9"; 
      strcat(tmp_str, "/state");        // "HomeAssistant/fan/plc1Traic1..9/state"; 
      _DSsensors.mqtt_topic_state[i-1] = new char[strlen(tmp_str) + 1];
      strcpy(_DSsensors.mqtt_topic_state[i-1], tmp_str);
    }
  }
  _DSsensors.setResolutionAll(10);
  _zeroCrossTime = millis();
  pinMode(zeroDetectorPin, INPUT_PULLUP);
  IWatchdog.begin(10000000);                          // wathDog 10 sec
  mqtt_cfg.isHAonline = false;
  return res;
}

bool triacPLC::_haDiscover(){
  if (!mqttClient.connected()) return false;
  char tmp_str1[40];
  strcpy(tmp_str1, mqtt_cfg.HADiscover);
  strcat(tmp_str1, "/");
  strcat(tmp_str1, mqtt_cfg.ClientID);
  strcat(tmp_str1, "/avty");                    // "HomeAssistant/plc1Traic/avty"
 { StaticJsonDocument<256> JsonDoc;
  JsonDoc["cmd_t"] = "~/set";
  JsonDoc["stat_t"] = "~/state";
  JsonDoc["bri_cmd_t"] = "~/bri_cmd";
  JsonDoc["bri_stat_t"] = "~/bri_state";
  JsonDoc["bri_scl"] = 100;
  JsonDoc["pl_off"] = "0";
  JsonDoc["pl_on"] = "1";
  JsonDoc["ret"] = true;   //retain
  JsonDoc["avty_t"] = tmp_str1;                 

  for (uint8_t i = 1; i <= channelAmount; i++){
    if (_dimmers[i-1].mqttRegistered) continue;
    char HA_autoDiscovery[40];
    strcpy(HA_autoDiscovery, mqtt_cfg.HADiscover);
    strcat(HA_autoDiscovery, "/light/");
    strcat(HA_autoDiscovery, mqtt_cfg.ClientID);
    char num[3]; itoa(i, num, DEC);
    strcat(HA_autoDiscovery, num);              // "HomeAssistant/light/plc1Traic1..9"; 

    JsonDoc["~"] = HA_autoDiscovery;            //"ha/light/" + plcName + i;
    char tmp_str[65];
    strcpy(tmp_str, mqtt_cfg.ClientID);
    strcat(tmp_str, num);
    JsonDoc["unique_id"] = tmp_str;             // plcName + i;
    JsonDoc["name"] = tmp_str;                  // plcName + i;
    strcpy(tmp_str, HA_autoDiscovery); 
    strcat(tmp_str, "/config");                 // "HomeAssistant/light/plc1Traic1..9/config"; 
    
    mqttClient.beginPublish(tmp_str, measureJson(JsonDoc), true);
    serializeJson(JsonDoc, mqttClient);
    _dimmers[i-1].mqttRegistered = mqttClient.endPublish();
    strcpy(tmp_str, HA_autoDiscovery);
    strcat(tmp_str, "/set");                  // "HomeAssistant/light/plc1Traic1..9/set"; 
    mqttClient.subscribe(tmp_str);
    //Ethernet.maintain();
    strcpy(tmp_str, HA_autoDiscovery);
    strcat(tmp_str, "/bri_cmd");                  // "HomeAssistant/light/plc1Traic1..9/bri_cmd"; 
    mqttClient.subscribe(tmp_str);
    return true;   
  }
 }
 { StaticJsonDocument<256> JsonDoc;
  JsonDoc["cmd_t"] = "~/set";
  JsonDoc["stat_t"] = "~/state";
  JsonDoc["pct_cmd_t"] = "~/pct_cmd";
  JsonDoc["pct_stat_t"] = "~/pct_state";
  JsonDoc["pl_off"] = "0";
  JsonDoc["pl_on"] = "1";
  JsonDoc["ret"] = true;   //retain
  JsonDoc["avty_t"] = tmp_str1;                 

  for (uint8_t i = 1; i <= fansAmount; i++){
    if (_fans[i-1].mqttRegistered) continue;
    char HA_autoDiscovery[40];
    strcpy(HA_autoDiscovery, mqtt_cfg.HADiscover);
    strcat(HA_autoDiscovery, "/fan/");
    strcat(HA_autoDiscovery, mqtt_cfg.ClientID);
    char num[3]; itoa(i, num, DEC);
    strcat(HA_autoDiscovery, num);              // "HomeAssistant/fan/plc1Traic1..9"; 

    JsonDoc["~"] = HA_autoDiscovery;            //"ha/fan/" + plcName + i;
    char tmp_str[65];
    strcpy(tmp_str, mqtt_cfg.ClientID);
    strcat(tmp_str, num);
    JsonDoc["unique_id"] = tmp_str;             // plcName + i;
    JsonDoc["name"] = tmp_str;                  // plcName + i;
    strcpy(tmp_str, HA_autoDiscovery); 
    strcat(tmp_str, "/config");                 // "HomeAssistant/fan/plc1Traic1..9/config"; 
    
    mqttClient.beginPublish(tmp_str, measureJson(JsonDoc), true);
    serializeJson(JsonDoc, mqttClient);
    _fans[i-1].mqttRegistered = mqttClient.endPublish();
    strcpy(tmp_str, HA_autoDiscovery);
    strcat(tmp_str, "/set");                  // "HomeAssistant/fan/plc1Traic1..9/set"; 
    mqttClient.subscribe(tmp_str);
    //Ethernet.maintain();
    strcpy(tmp_str, HA_autoDiscovery);
    strcat(tmp_str, "/pct_cmd");                  // "HomeAssistant/fan/plc1Traic1..9/pct_cmd"; 
    mqttClient.subscribe(tmp_str);
    return true;   
  }
 }

  { 
  StaticJsonDocument<256> JsonDocBtn;
  JsonDocBtn["cmd_t"] = "~/set";
  //JsonDocBtn["payload_press"] = "PRESS";
  JsonDocBtn["ret"] = true;      //retain
  JsonDocBtn["avty_t"] = tmp_str1;                 

  for (uint8_t i = 1; i <= btnsAmount; i++){
    if (_btns[i-1].mqttRegistered) continue;
    char HA_autoDiscovery[40];
    strcpy(HA_autoDiscovery, mqtt_cfg.HADiscover);
    strcat(HA_autoDiscovery, "/button/");
    strcat(HA_autoDiscovery, mqtt_cfg.ClientID);
    char num[3]; itoa(i, num, DEC);
    strcat(HA_autoDiscovery, num);              // "HomeAssistant/button/plc1Traic1..9"; 

    JsonDocBtn["~"] = HA_autoDiscovery;            //"ha/button/" + plcName + i;
    char tmp_str[65];
    strcpy(tmp_str, mqtt_cfg.ClientID);
    strcat(tmp_str, num);
    JsonDocBtn["unique_id"] = tmp_str;// plcName + i;
    JsonDocBtn["name"] = tmp_str;// plcName + i;
    strcpy(tmp_str, HA_autoDiscovery); 
    strcat(tmp_str, "/config");        // "HomeAssistant/button/plc1Traic1..9/config"; 
    
    mqttClient.beginPublish(tmp_str, measureJson(JsonDocBtn), true);
    serializeJson(JsonDocBtn, mqttClient);
    _btns[i-1].mqttRegistered = mqttClient.endPublish();
    strcpy(tmp_str, HA_autoDiscovery);
    strcat(tmp_str, "/set");                  // "HomeAssistant/button/plc1Traic1..9/set"; 
    mqttClient.subscribe(tmp_str);
    return true;   
  }
 }

 { 
  StaticJsonDocument<256> JsonDocBtn;
  JsonDocBtn["cmd_t"] = "~/set";
  JsonDocBtn["stat_t"] = "~/state";
  JsonDocBtn["pl_off"] = "0";
  JsonDocBtn["pl_on"] = "1";
  JsonDocBtn["stat_off"] = "0";
  JsonDocBtn["stat_on"] = "1";
  JsonDocBtn["ret"] = true;      //retain
  JsonDocBtn["avty_t"] = tmp_str1;                 

  for (uint8_t i = 1; i <= relaysAmount; i++){
    if (_relays[i-1].mqttRegistered) continue;
    char HA_autoDiscovery[40];
    strcpy(HA_autoDiscovery, mqtt_cfg.HADiscover);
    strcat(HA_autoDiscovery, "/switch/");
    strcat(HA_autoDiscovery, mqtt_cfg.ClientID);
    char num[3]; itoa(i, num, DEC);
    strcat(HA_autoDiscovery, num);              // "HomeAssistant/switch/plc1Traic1..9"; 

    JsonDocBtn["~"] = HA_autoDiscovery;            //"ha/switch/" + plcName + i;
    char tmp_str[65];
    strcpy(tmp_str, mqtt_cfg.ClientID);
    strcat(tmp_str, num);
    JsonDocBtn["unique_id"] = tmp_str;// plcName + i;
    JsonDocBtn["name"] = tmp_str;// plcName + i;
    strcpy(tmp_str, HA_autoDiscovery); 
    strcat(tmp_str, "/config");        // "HomeAssistant/switch/plc1Traic1..9/config"; 
    
    mqttClient.beginPublish(tmp_str, measureJson(JsonDocBtn), true);
    serializeJson(JsonDocBtn, mqttClient);
    _relays[i-1].mqttRegistered = mqttClient.endPublish();
    strcpy(tmp_str, HA_autoDiscovery);
    strcat(tmp_str, "/set");                  // "HomeAssistant/switch/plc1Traic1..9/set"; 
    mqttClient.subscribe(tmp_str);
    return true;   
  }
 }

  { 
  StaticJsonDocument<256> JsonDocBtn;
  JsonDocBtn["stat_t"] = "~/state";
  JsonDocBtn["expire_after"] = 10;
  JsonDocBtn["unit_of_meas"] = "°C";
  JsonDocBtn["ret"] = true;      //retain
  JsonDocBtn["avty_t"] = tmp_str1;                 

  for (uint8_t i = 1; i <= DS_SENS_AMOUNT; i++){
    if (_DSsensors.mqttRegistered[i-1]) continue;
    char HA_autoDiscovery[40];
    strcpy(HA_autoDiscovery, mqtt_cfg.HADiscover);
    strcat(HA_autoDiscovery, "/sensor/");
    strcat(HA_autoDiscovery, mqtt_cfg.ClientID);
    char num[3]; itoa(i, num, DEC);
    strcat(HA_autoDiscovery, num);              // "HomeAssistant/sensor/plc1Traic1..9"; 

    JsonDocBtn["~"] = HA_autoDiscovery;            //"ha/sensor/" + plcName + i;
    char tmp_str[65];
    strcpy(tmp_str, mqtt_cfg.ClientID);
    strcat(tmp_str, num);
    JsonDocBtn["unique_id"] = tmp_str;// plcName + i;
    JsonDocBtn["name"] = tmp_str;// plcName + i;
    strcpy(tmp_str, HA_autoDiscovery); 
    strcat(tmp_str, "/config");        // "HomeAssistant/sensor/plc1Traic1..9/config"; 
    
    mqttClient.beginPublish(tmp_str, measureJson(JsonDocBtn), true);
    serializeJson(JsonDocBtn, mqttClient);
    _DSsensors.mqttRegistered[i-1] = mqttClient.endPublish();
    return true;   
  }
 }
 return true;
}

bool triacPLC::_mqttReconnect() {
  #ifdef DEBUGPLC 
    Serial.print(F("connectig..")); 
  #endif
  plcOLED.setCursorXY(0, 40);
  plcOLED.print("MQTT CONNECT.."); 
  char mqtt_willTopic[40];
  strcpy(mqtt_willTopic, mqtt_cfg.HADiscover);
  strcat(mqtt_willTopic, "/");
  strcat(mqtt_willTopic, mqtt_cfg.ClientID);
  strcat(mqtt_willTopic, "/avty");                    // "HomeAssistant/plc1Traic/avty"

  if (mqttClient.connect(mqtt_cfg.ClientID, mqtt_cfg.User, mqtt_cfg.Pass, mqtt_willTopic, 0, true, mqtt_payloadNotAvailable, true)){ 
    plcOLED.print(F("OK  "));
    mqttClient.publish(mqtt_willTopic, mqtt_payloadAvailable, true);      // топик статуса нашего контроллера
    mqttClient.loop();
    //Ethernet.maintain();
    mqttClient.loop();
    mqttClient.subscribe(mqtt_cfg.HABirthTopic);                          // подписываемся на топик статуса HA
    #ifdef DEBUGPLC 
      Serial.println(F("OK")); 
    #endif
  } else {
    plcOLED.print(F("FAIL")); 
    #ifdef DEBUGPLC 
      Serial.println(F(" fail")); 
    #endif
  }
  return mqttClient.connected();
}

bool triacPLC::_doMqtt(){
  if (!mqtt_cfg.useMQTT) return true;  
  static uint32_t nextReconnectAttempt = MqttReconnectPeriod/2;
  if (Ethernet.linkStatus() != LinkON) {
    plcOLED.setCursorXY(0, 40);
    plcOLED.print(F("MQTT CONNECT.. FAIL"));
    mqtt_cfg.isHAonline = mqttClient.connected();
    return false;
  }
  bool res = mqttClient.connected();
  if (!res) {
    if (millis() > nextReconnectAttempt) {
      nextReconnectAttempt = millis() + MqttReconnectPeriod;
      for (uint8_t i = 1; i <= channelAmount; i++) _dimmers[i-1].mqttRegistered = false;
      for (uint8_t i = 1; i <= btnsAmount; i++) _btns[i-1].mqttRegistered = false;
      for (uint8_t i = 1; i <= fansAmount; i++) _fans[i-1].mqttRegistered = false;
      for (uint8_t i = 1; i <= relaysAmount; i++) _relays[i-1].mqttRegistered = false;
      for (uint8_t i = 1; i <= DS_SENS_AMOUNT; i++) _DSsensors.mqttRegistered[i-1] = false;
      mqtt_cfg.isHAonline = false;
      _mqttReconnect();    // Attempt to reconnect
    }
  } else {
    plcOLED.setCursorXY(90, 40);
    plcOLED.print(F("OK  "));
    mqttClient.loop();
  }
  return res;
}

bool triacPLC::_doEthernet(){
  static uint32_t nextLinkCheck = 0;
  if (millis() < nextLinkCheck) return true;
  nextLinkCheck = millis() + LinkCheckPeriod;
  auto res = Ethernet.maintain();
  plcOLED.setCursorXY(0, 24);
  switch (res) {
    case 1:
      plcOLED.print(F("RENEW DHCP FAIL"));
      break;
    case 2:
      plcOLED.print(F("RENEW DHCP OK"));
      //Serial.print(F("My IP: ")); Serial.println(Ethernet.localIP());
      break;
    case 3:
      plcOLED.print(("REBIND FAIL"));
      break;
    case 4:
      plcOLED.print(("REBIND OK"));
      //Serial.print(F("My IP: ")); Serial.println(Ethernet.localIP());
      break;
    default:
      plcOLED.print(("def"));
      //nothing happened
      break;
  }
  auto link = Ethernet.linkStatus();
  plcOLED.setCursorXY(0, 32);
  switch (link) {
    case Unknown:
      #ifdef DEBUGPLC 
        Serial.println(F("No Eth card")); 
      #endif
      plcOLED.print(("No Eth card"));
      break;
    case LinkON:
      #ifdef DEBUGPLC 
        Serial.println(F("LINK ON ")); 
      #endif
      plcOLED.print(F("LINK ON "));
      break;
    case LinkOFF:
      #ifdef DEBUGPLC 
        Serial.println(F("LINK OFF ")); 
      #endif
      plcOLED.print(F("LINK OFF"));
      break;
  }
  if ((res == 0) && (link == LinkON)) return true; 
    else return false; 
}

void triacPLC::mqttCallback(char* topic, byte* payload, uint16_t length) {
 // ha/light/plc1Traic1/set
 // ha/switch/plc1Traic1/set
 // ha/fan/plc1Traic1/set
 
  char *uk1, *uk2, stemp[4];
  char *pl1 = new char[length+1];
  strncpy(pl1, (char*)payload, length);
  pl1[length] = '\0';
  uk1 = strstr(topic, "light");                                               // light/plc1Traic1/set
  if (uk1 == topic + strlen(mqtt_cfg.HADiscover) + 1 ) {
    uk1 += strlen("light") + 1 ;                                              // plc1Traic1/set
    uk1 = strstr(uk1, mqtt_cfg.ClientID);
    if ( uk1 == NULL) return;
    uk1 += strlen(mqtt_cfg.ClientID);
    uk2 = strchr(uk1,'/');
    uint8_t num = uk2-uk1; 
    strncpy(stemp, uk1, num);
    stemp[num] = 0;
    num = atoi(stemp);
    if (num > 9) return;      // ошика
    uk2++;
    if (strcmp(uk2, "bri_cmd") == 0)
      setPower(num-1, atoi((char*)pl1));
    if (strcmp(uk2, "set") == 0) {
      if (atoi((char*)pl1)) _dimmers[num-1].setOn();
        else _dimmers[num-1].setOff();
      itoa(_dimmers[num-1].getOnOff(), stemp, DEC);
      mqttClient.publish(_dimmers[num-1].mqtt_topic_state, stemp, true);
    }
  }
  uk1 = strstr(topic, "fan");                                               // fan/plc1Traic1/set
  if (uk1 == topic + strlen(mqtt_cfg.HADiscover) + 1 ) {
    uk1 += strlen("fan") + 1 ;                                              // plc1Traic1/set
    uk1 = strstr(uk1, mqtt_cfg.ClientID);
    if ( uk1 == NULL) return;
    uk1 += strlen(mqtt_cfg.ClientID);
    uk2 = strchr(uk1,'/');
    uint8_t num = uk2-uk1; 
    strncpy(stemp, uk1, num);
    stemp[num] = 0;
    num = atoi(stemp);
    if (num > 9) return;      // ошика
    uk2++;
    if (strcmp(uk2, "pct_cmd") == 0)
      setFanPower(num-1, atoi((char*)pl1));
    if (strcmp(uk2, "set") == 0) {
      if (atoi((char*)pl1)) _fans[num-1].setOn();
        else _fans[num-1].setOff();
      itoa(_fans[num-1].getOnOff(), stemp, DEC);
      mqttClient.publish(_fans[num-1].mqtt_topic_state, stemp, true);
    }
  }

  uk1 = strstr(topic, "switch");                                               // switch/plc1Traic1/set 
  if (uk1 == topic + strlen(mqtt_cfg.HADiscover) + 1 ) {
    uk1 += strlen("switch") + 1 ;                                              // plc1Traic1/set
    uk1 = strstr(uk1, mqtt_cfg.ClientID);
    if ( uk1 == NULL) return;
    uk1 += strlen(mqtt_cfg.ClientID);
    uk2 = strchr(uk1,'/');
    uint8_t num = uk2-uk1; 
    strncpy(stemp, uk1, num);
    stemp[num] = 0;
    num = atoi(stemp);
    if (num > 9) return;      // ошика
    uk2++;
    if (strcmp(uk2, "set") == 0) {
      if (atoi((char*)pl1)) _relays[num-1].setOn();
        else _relays[num-1].setOff();
      itoa(_relays[num-1].getOnOff(), stemp, DEC);
      mqttClient.publish(_relays[num-1].mqtt_topic_state, stemp, true);
    }
  }

/*
  uk1 = strstr(topic, "switch");                                               // switch/plc1Traic1/set 
  if (uk1 == topic + strlen(mqtt_cfg.HADiscover) + 1 ) {
    uk1 += strlen("switch") + 1 ;                                              // plc1Traic1/set
    uk1 = strstr(uk1, mqtt_cfg.ClientID);
    if ( uk1 == NULL) return;
    uk1 += strlen(mqtt_cfg.ClientID);
    uk2 = strchr(uk1,'/');
    uint8_t num = uk2-uk1; 
    strncpy(stemp, uk1, num);
    stemp[num] = 0;
    num = atoi(stemp);
    if (num > 9) return;      // ошика
    uk2++;
    if (strcmp(uk2, "set") == 0) {
      if (atoi((char*)pl1)) _btns[num-1].setOn();
        else _btns[num-1].setOff();
      itoa(_btns[num-1].getOnOff(), stemp, DEC);
      mqttClient.publish(_btns[num-1].mqtt_topic_state, stemp, true);
    }
  }
*/

  if (strcmp(topic, mqtt_cfg.HABirthTopic) == 0) {                              // "homeassistant/status"
    if (strcmp(pl1, mqtt_payloadAvailable) == 0) mqtt_cfg.isHAonline = true;
      else mqtt_cfg.isHAonline = false;
  }  
  delete [] pl1;
}

void triacPLC::_doButtonsDimmers(){
  for (uint8_t i = 0; i < fansAmount; i++) _fans[i].setPower();      // корректно переключаемся без подгорания реле
  for (uint8_t i = 0; i < relaysAmount; i++) 
    if (_relays[i].getOnOff()) _relays[i].setOn(); else _relays[i].setOff();      // корректно переключаемся с задержкой
  
  static uint32_t endTm2 = changeLightSpeed;
  for (uint8_t i = 0; i < btnsAmount; i++) _btns[i].tick();
  char st[4];
   //двойной клик
  for (byte i = 0; i < channelAmount; i++)
    if (_btns[i].isDouble()) setPower(i, 50);

  // включение/выключение
  for (byte i = 0; i < btnsAmount; i++){
    if (_btns[i].isSingle()) {
      if (mqttClient.connected())
        mqttClient.publish(_btns[i].mqtt_topic_cmd, "PRESS", true);  
      if (!mqttClient.connected()) toggle(i);       // если не подключены к MQTT - переключаем канала по нажанию кнопок, а если подключены - то это делают автоматизации HA
    }
  }

  if (millis() < endTm2) return;
  endTm2 = millis() + changeLightSpeed;

  // кнопка удерживается
  for (byte i = 0; i < btnsAmount; i++){
    if (_btns[i].isHold()){
      _dimmers[i].incPower();
      if (mqttClient.connected()){
        itoa(_dimmers[i].getPower(), st, DEC);
        mqttClient.publish(_dimmers[i].mqtt_topic_bri_state, st, true);
      }
    }
  }

  for (byte i = 0; i < channelAmount; i++){
    if (_btns[i].isRelease()){
      _dimmers[i].saveEEPROM();
    }
  }
}

void triacPLC::_doDSsensors(){
  static uint32_t endTm1 = 500;
  static uint32_t endTm2 = 2000;
  static uint8_t curDs = 0;
  if (millis() > endTm1) {        // обработчик датчиков температуры ds
    endTm1 = millis() + 500;
    _DSsensors.requestTempAll();
    for (byte i = 0; i < DS_SENS_AMOUNT; i++) _DSsensors.getTemp(i);
  }
  if (millis() > endTm2) {
    endTm2 = millis() + 2000;
    for (byte i = 0; i < DS_SENS_AMOUNT; i++){
      if (_DSsensors.online(i)) {
        char st1[6]; 
        sprintf(st1, "%.1f", _DSsensors.getTemp(i));
        if (mqttClient.connected()){
          mqttClient.publish(_DSsensors.mqtt_topic_state[i], st1, true);
        } 
        if (i == curDs){
          plcOLED.setCursorXY(0, 48);
          plcOLED.print("DS");
          plcOLED.print(i+1); plcOLED.print(" ");
          plcOLED.print(_DSsensors.getTemp(i), 1); plcOLED.print(F("C  "));
        }
      } else {
        if (!mqttClient.connected()){
           // добавить отправку в НА ошибку датчика температуры
        }
        if (i == curDs){
          plcOLED.setCursorXY(0, 48);
          plcOLED.print("DS");
          plcOLED.print(i+1);
          plcOLED.print(" er!   ");
        }
      } 
    }
    curDs++; if (curDs >= DS_SENS_AMOUNT) curDs = 0;  // текущий датчик для индикации на дисплее
  }
}

bool triacPLC::processEvents(){
  static uint32_t endTm1 = 200;
  IWatchdog.reload();
  _doButtonsDimmers();
  _doEthernet();
  _doDSsensors();
  bool res = _doMqtt();
  if (needDelayedSaveRom) saveEEPROM();
  if (millis() < endTm1) return res;
  endTm1 = millis() + 200;
  return _haDiscover();
}

void triacPLC::display(){                                       // Эа функция нужна из-за аппараного бага при пререносе SPI1 и одновременной раоте I2C1
  __HAL_RCC_I2C1_CLK_ENABLE();
  plcOLED.update();
  __HAL_RCC_I2C1_CLK_DISABLE();  
}

void triacPLC::showFreq(){
  static char a1 = '/';
  plcOLED.setCursorXY(82, 56);
  plcOLED.print(ACfreq/10.0, 1); 
  plcOLED.print(" Hz");
  plcOLED.setCursorXY(127, 0);
  switch (a1) {
    case '/' : a1 = '-'; break;
    case '-' : a1 = '\\'; break;
    case '\\' : a1 = '|'; break;
    case '|' : a1 = '/'; break;
  }
  plcOLED.print(a1);
  display();
}

void triacPLC::zeroCrossISR(){
  _counter = 200;
  for (byte i = 0; i < channelAmount; i++) _dimmers[i].zeroCross();
  for (byte i = 0; i < fansAmount; i++) _fans[i].zeroCross();
  _zeroCrossCntr--;
  if (_zeroCrossCntr == 0) {
    _zeroCrossCntr = 200;
    ACfreq = 1000000/(millis() - _zeroCrossTime);
    _zeroCrossTime = millis();
  }    
}

void triacPLC::timerISR(){
  uint8_t _counter1 = _counter, n = 3;  
  if (_counter1 > 100) _counter1 -= 100; 
  if (_counter1 > 80) n = 10;
  for (uint8_t i = 0; i < channelAmount; i++)
    if ((_counter1 < _dimmers[i].triacTime) && (_counter1 + n > _dimmers[i].triacTime)) digitalWrite(_dimmers[i].pin, HIGH);          //  включаем на 100мкс
      else digitalWrite(_dimmers[i].pin, LOW);                                                                  //  выключаем, но семистор остается включенным до переполюсовки        
  for (uint8_t i = 0; i < fansAmount; i++)
    if ((_counter1 < _fans[i].triacTime) && (_counter1 + n > _fans[i].triacTime)) digitalWrite(_fans[i].pin, HIGH);          //  включаем на 100мкс
      else digitalWrite(_fans[i].pin, LOW);                                                                  //  выключаем, но семистор остается включенным до переполюсовки
  _counter--;
}

void triacPLC::allOff(){
  for (uint8_t i = 0; i < channelAmount; i++) setOff(i);
  for (uint8_t i = 0; i < fansAmount; i++) setFanOff(i);
}

void triacPLC::setOn(uint8_t channel){
  if (channel >= channelAmount) return;
  _dimmers[channel].setOn();
  if (mqttClient.connected()){
    char st[4];        
    itoa(_dimmers[channel].getOnOff(), st, DEC);
    mqttClient.publish(_dimmers[channel].mqtt_topic_state, st, true);
  }
}

void triacPLC::setFanOn(uint8_t channel){
  if (channel >= fansAmount) return;
  _fans[channel].setOn();
  if (mqttClient.connected()){
    char st[4];        
    itoa(_fans[channel].getOnOff(), st, DEC);
    mqttClient.publish(_fans[channel].mqtt_topic_state, st, true);
  }
}

void triacPLC::setRelayOn(uint8_t channel){
  if (channel >= relaysAmount) return;
  _relays[channel].setOn();
  if (mqttClient.connected()){
    char st[4];        
    itoa(_relays[channel].getOnOff(), st, DEC);
    mqttClient.publish(_relays[channel].mqtt_topic_state, st, true);
  }
}

void triacPLC::setOff(uint8_t channel){
  if (channel>=channelAmount) return;
  _dimmers[channel].setOff();
  if (mqttClient.connected()){
    char st[4];        
    itoa(_dimmers[channel].getOnOff(), st, DEC);
    mqttClient.publish(_dimmers[channel].mqtt_topic_state, st, true);
  }
}

void triacPLC::setFanOff(uint8_t channel){
  if (channel>=fansAmount) return;
  _fans[channel].setOff();
  if (mqttClient.connected()){
    char st[4];        
    itoa(_fans[channel].getOnOff(), st, DEC);
    mqttClient.publish(_fans[channel].mqtt_topic_state, st, true);
  }
}

void triacPLC::setRelayOff(uint8_t channel){
  if (channel>=relaysAmount) return;
  _relays[channel].setOff();
  if (mqttClient.connected()){
    char st[4];        
    itoa(_relays[channel].getOnOff(), st, DEC);
    mqttClient.publish(_relays[channel].mqtt_topic_state, st, true);
  }
}

void triacPLC::toggle(uint8_t channel){
  if (channel>=channelAmount) return;
  _dimmers[channel].toggle();
  if (_dimmers[channel].getOnOff() && !mqttClient.connected() && mqtt_cfg.useMQTT && (_dimmers[channel].getPower() < 20)) _dimmers[channel].setPower (97); 
  if (mqttClient.connected()){
    char st[4];        
    itoa(_dimmers[channel].getOnOff(), st, DEC);
    mqttClient.publish(_dimmers[channel].mqtt_topic_state, st, true);
  }
}

void triacPLC::toggleFan(uint8_t channel){
  if (channel>=fansAmount) return;
  _fans[channel].toggle();
  if (_fans[channel].getOnOff() && !mqttClient.connected() && mqtt_cfg.useMQTT && (_fans[channel].getPower() < 20)) _fans[channel].setPower (97); 
  if (mqttClient.connected()){
    char st[4];        
    itoa(_fans[channel].getOnOff(), st, DEC);
    mqttClient.publish(_fans[channel].mqtt_topic_state, st, true);
  }
}

void triacPLC::toggleRelay(uint8_t channel){
  if (channel>=relaysAmount) return;
  _relays[channel].toggle();
  if (mqttClient.connected()){
    char st[4];        
    itoa(_relays[channel].getOnOff(), st, DEC);
    mqttClient.publish(_relays[channel].mqtt_topic_state, st, true);
  }
}

void triacPLC::setPower(uint8_t channel, uint8_t power){
  if (channel>=channelAmount) return; 
  _dimmers[channel].setPower(power);
  if (mqttClient.connected()){
    char st[4];    
    itoa(_dimmers[channel].getPower(), st, DEC);
    mqttClient.publish(_dimmers[channel].mqtt_topic_bri_state, st, true);
  }
}

void triacPLC::setFanPower(uint8_t channel, uint8_t power){
  if (channel>=fansAmount) return; 
  _fans[channel].setPower(power);
  if (mqttClient.connected()){
    char st[4];    
    itoa(_fans[channel].getPower(), st, DEC);
    mqttClient.publish(_fans[channel].mqtt_topic_bri_state, st, true);
  }
}

void triacPLC::incPower(uint8_t channel){
  if (channel>=channelAmount) return;
  _dimmers[channel].incPower();
}

void triacPLC::incFanPower(uint8_t channel){
  if (channel>=fansAmount) return;
  _fans[channel].incPower();
}

void triacPLC::setRampTime(uint8_t channel, uint16_t rampTime){    // ramtTime в секундах
  if (channel>=channelAmount) return;
  _dimmers[channel].setRampTime(rampTime);
}

void triacPLC::setFanRampTime(uint8_t channel, uint16_t rampTime){    // ramtTime в секундах
  if (channel>=fansAmount) return;
  _fans[channel].setRampTime(rampTime);
}

void triacPLC::setDefInetCfg(){
  inet_cfg.use_dhcp = def_useDHCP;
  inet_cfg.dhcp_refresh_minutes = 5;
  {uint8_t mac1[] = def_macAddr; memcpy(inet_cfg.mac, mac1, sizeof(inet_cfg.mac)); }
  {uint8_t ip1[] = {def_staticIP}; memcpy(inet_cfg.ip, ip1, sizeof(inet_cfg.ip)); }
  {uint8_t ip1[] = {def_staticGW}; memcpy(inet_cfg.gw, ip1, sizeof(inet_cfg.gw)); }
  {uint8_t ip1[] = {def_staticDNS}; memcpy(inet_cfg.dns, ip1, sizeof(inet_cfg.dns)); }
  {uint8_t ip1[] = {def_staticMASK}; memcpy(inet_cfg.mask, ip1, sizeof(inet_cfg.mask)); }
  inet_cfg.webSrvPort = 80;
}

void triacPLC::setDefMqttCfg(){
  strcpy(mqtt_cfg.User, def_mqtt_user);
  strcpy(mqtt_cfg.Pass, def_mqtt_pass);
  strcpy(mqtt_cfg.ClientID, def_mqtt_clientID);
  strcpy(mqtt_cfg.HADiscover, def_HomeAssistantDiscovery);    // "HomeAssistant"
  strcpy(mqtt_cfg.HABirthTopic, def_HABirthTopic);            // "homeassistant/status"
  uint8_t ip1[] = {def_mqtt_brokerIP}; memcpy(mqtt_cfg.SrvIP, ip1, sizeof(mqtt_cfg.SrvIP));
  mqtt_cfg.SrvPort = 1883;  
}


void triacPLC::loadEEPROM(){
  for (uint8_t i = 0; i < channelAmount; i++) _dimmers[i].loadEEPROM();                // загружаем сохраненные состояния диммеров из eeprom
  for (uint8_t i = 0; i < fansAmount; i++) _fans[i].loadEEPROM();                // загружаем сохраненные состояния вентиляторов из eeprom
  for (uint8_t i = 0; i < relaysAmount; i++) _relays[i].loadEEPROM();                // загружаем сохраненные состояния реле из eeprom
  
  if (EEPROM[0] != EEPROMdataVer) {     // в EEPROM ничего еще не сохраняли
    setDefInetCfg();
    setDefMqttCfg();
    saveEEPROM();
  } else {
     EEPROM.get(_relays[0].baseRomAddr + relaysAmount*2 + 2, inet_cfg);
     EEPROM.get(_relays[0].baseRomAddr + relaysAmount*2 + 2 + sizeof(inet_cfg), mqtt_cfg);
     EEPROM.get(_relays[0].baseRomAddr + relaysAmount*2 + 2 + sizeof(inet_cfg) + sizeof(mqtt_cfg), flipDisplay);
     EEPROM.get(_relays[0].baseRomAddr + relaysAmount*2 + 2 + sizeof(inet_cfg) + sizeof(mqtt_cfg) + sizeof(flipDisplay), DSaddrs);
  }
}
    
void triacPLC::saveEEPROM(){
  for (uint8_t i = 0; i < channelAmount; i++) _dimmers[i].saveEEPROM();
  for (uint8_t i = 0; i < fansAmount; i++) _fans[i].saveEEPROM();                // сохраняем состояния вентиляторов из eeprom
  for (uint8_t i = 0; i < relaysAmount; i++) _relays[i].saveEEPROM();            // сохраняем состояния реле из eeprom
  EEPROM.put(_relays[0].baseRomAddr + relaysAmount*2 + 2, inet_cfg);
  EEPROM.put(_relays[0].baseRomAddr + relaysAmount*2 + 2 + sizeof(inet_cfg), mqtt_cfg);
  EEPROM.put(_relays[0].baseRomAddr + relaysAmount*2 + 2 + sizeof(inet_cfg) + sizeof(mqtt_cfg), flipDisplay);
  EEPROM.put(_relays[0].baseRomAddr + relaysAmount*2 + 2 + sizeof(inet_cfg) + sizeof(mqtt_cfg) + sizeof(flipDisplay), DSaddrs);
  needDelayedSaveRom = false;
}

uint8_t triacPLC::getRampTime(uint8_t channel){
  if (channel>=channelAmount) return 0;
  return _dimmers[channel].getRampTime();
}

uint8_t triacPLC::getFanRampTime(uint8_t channel){
  if (channel>=fansAmount) return 0;
  return _fans[channel].getRampTime();
}

uint8_t triacPLC::getPower(uint8_t channel){
  if (channel >= channelAmount) return 0;
  return _dimmers[channel].getPower();
}

uint8_t triacPLC::getFanPower(uint8_t channel){
  if (channel >= fansAmount) return 0;
  return _fans[channel].getPower();
}

bool triacPLC::getBtnState(uint8_t btn_idx){
  if (btn_idx >= btnsAmount) return false;
  return _btns[btn_idx].state();
}

bool triacPLC::getOnOff(uint8_t channel){
  if (channel>=channelAmount) return false;
  return _dimmers[channel].getOnOff();
}

bool triacPLC::getFanOnOff(uint8_t channel){
  if (channel>=fansAmount) return false;
  return _fans[channel].getOnOff();
}

bool triacPLC::getRelayOnOff(uint8_t channel){
  if (channel>=relaysAmount) return false;
  return _relays[channel].getOnOff();
}


void triacPLC::set_useMQTT(bool new_useMQTT){
  mqtt_cfg.useMQTT = new_useMQTT;
  for (uint8_t i = 0; i < channelAmount; i++) _dimmers[i].useMQTT = new_useMQTT;
  for (uint8_t i = 0; i < btnsAmount; i++) _btns[i].useMQTT = new_useMQTT;
  for (uint8_t i = 0; i < fansAmount; i++) _fans[i].useMQTT = new_useMQTT;
  for (uint8_t i = 0; i < relaysAmount; i++) _relays[i].useMQTT = new_useMQTT;
}
