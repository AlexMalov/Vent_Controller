#include "triacPLC.h"
#include "WebServer.h"

#include "Dns.h"

#define URLPREFIX ""

triacPLC myPLC;       //главный оъект ПЛК-конроллера

WebServer webserver(URLPREFIX, 80);

void index_html(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);
void LAN_html(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);
void MQTT_html(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);
void error_html(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);
void PLC_css(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);

extern int __bss_end;
extern void *__brkval;
int memoryFree() {    // Функция, возвращающая количество свободного ОЗУ
  int freeValue;
  if ((int)__brkval == 0) freeValue = ((int)&freeValue) - ((int)&__bss_end);
    else freeValue = ((int)&freeValue) - ((int)__brkval);
  return freeValue;
}

void websetup(){
  webserver.setDefaultCommand(&index_html);
  webserver.setFailureCommand(&error_html);
  webserver.addCommand("index", &index_html);
  webserver.addCommand("LAN", &LAN_html);
  webserver.addCommand("MQTT", &MQTT_html);
  webserver.addCommand("PLC.css", &PLC_css);
  webserver.begin();                      /* start the server to wait for connections */
}

////////---- web interface ----////////
                   // "<link href='http://ajax.googleapis.com/ajax/libs/jqueryui/1.8.21/themes/base/jquery-ui.css' rel=stylesheet />"
                   // "<script src='http://ajax.googleapis.com/ajax/libs/jquery/1.6.4/jquery.min.js'></script>"
                   // "<script src='http://ajax.googleapis.com/ajax/libs/jqueryui/1.8.21/jquery-ui.min.js'></script>"
P(Page_start) = "<html>"
                  "<head>"
                    "<title>PLC Setup</title>"
                    "<meta http-equiv='Pragma' content='no-cache'>"
                    "<link href='PLC.css' type='text/css' rel='stylesheet'>"
                    "<link href='https://code.jquery.com/ui/1.13.1/themes/base/jquery-ui.css' rel=stylesheet />"
                    "<script src='https://code.jquery.com/jquery-1.12.4.min.js' integrity='sha256-ZosEbRLbNQzLpnKIkEdrPv7lOy9C27hHQ+Xp8a4MxAQ=' crossorigin='anonymous'></script>"
                    "<script src='https://code.jquery.com/ui/1.13.1/jquery-ui.min.js' integrity='sha256-eTyxS0rkjpLEo16uXTS0uVCS4815lc40K2iVpWDvdSY=' crossorigin='anonymous'></script>"
                    "<script src='//cdnjs.cloudflare.com/ajax/libs/jqueryui-touch-punch/0.2.3/jquery.ui.touch-punch.min.js'></script>"
                    "<script>"
                      "function dsbtnClick(obj) { $.post('', { dsbtnClick: obj.id } ); setTimeout(function(){location.reload();}, 1000); }" // кнопка получения адреса датчика температуры
                      "function inputChange(Event) { var id = $(this).attr('id'); id = id.replace('indc', ''); var v1 = $(this).val(); $('#dim'+id).slider( 'option', 'value', v1 ); }"     // обработчик инпутов мощности
                      "function changeSlide(event, ui) { jQuery.ajaxSetup({timeout: 200}); var id = $(this).attr('id'); id = id.replace('dim', ''); $('#indc'+ id).val(ui.value); var prm1 = 'Pow' + id; var params = {}; params[prm1] = ui.value;  $.post('', params); }"  // обработчик слайдеров
                      "function inputFanChange(Event) { var id = $(this).attr('id'); id = id.replace('fanIndc', ''); var v1 = $(this).val(); $('#fan'+id).slider( 'option', 'value', v1 ); }"     // обработчик инпутов мощности вентилятора
                      "function changeFanSlide(event, ui) { jQuery.ajaxSetup({timeout: 200}); var id = $(this).attr('id'); id = id.replace('fan', ''); $('#fanIndc'+ id).val(ui.value); var prm1 = 'fan' + id; var params = {}; params[prm1] = ui.value;  $.post('', params); }"  // обработчик слайдеров
                      "function inputRlyChange(Event) { var id = $(this).attr('id'); id = id.replace('rlyIndc', ''); var v1 = $(this).val(); $('#rly'+id).slider( 'option', 'value', v1 ); }"     // обработчик инпутов реле
                      "function changeRlySlide(event, ui) { jQuery.ajaxSetup({timeout: 200}); var id = $(this).attr('id'); id = id.replace('rly', ''); $('#rlyIndc'+ id).val(ui.value); var prm1 = 'rly' + id; var params = {}; params[prm1] = ui.value;  $.post('', params); }"  // обработчик слайдеров реле
                      "$(document).ready(function(){"
                          "$('.slider').slider({min: 0, max:100, change:changeSlide, create: function(event, ui){ $(this).slider('value', $(this).parent().find('.indc').val());  }});"
                          "$('.fanSlider').slider({min: 0, max:100, change:changeFanSlide, create: function(event, ui){ $(this).slider('value', $(this).parent().find('.fanIndc').val());  }});"
                          "$('.rlySlider').slider({min: 0, max:1, change:changeRlySlide, create: function(event, ui){ $(this).slider('value', $(this).parent().find('.rlyIndc').val());  }});"
                          "$('.indc').change(inputChange);"
                          "$('.fanIndc').change(inputFanChange);"
                          "$('.rlyIndc').change(inputRlyChange);"
                          "});"
                    "</script>"
                  "</head>"
                  "<body class='mybody'>\n";
                  
P(Page_end) = "</body></html>";

P(Http400) = "HTTP 400 - BAD REQUEST";
P(top_menu) = "<div align='center'>"
                "<h1 style='margin: 0px 0px 5px'>PLC Vent @ \115\105\130\101\124\120\117\110\40\104\111\131</h1>"
              "</div>"
              "<table class='mytable' align='center'>"
              "<tbody>"
                "<tr id='topnavon'>"
                  "<td> <a href='index'>IO status</a></td>"
                  "<td> <a href='LAN'>LAN Setup</a></td>"
                  "<td> <a href='MQTT'>MQTT Setup</a></td>"
                "</tr>"
              "</tbody></table>";

P(Setup_begin) = "<table class='mytable' align='center'>"
                "<tbody>"
                "<tr valign='top'>"
                  "<td id='sidenav_container' valign='top' align='right' width='125'>"
                    "<div id='sidenav'>"
                      "<ul>"
                        "<li><a href='index'>IO status</a></li>"
                        "<li><a href='LAN'>Lan Setup</a></li>"
                        "<li><a href='MQTT'>MQTT Setup</a></li>"
                        "<li><a href='logout.htm'>Logout</a></li>"
                      "</ul>"
                    "</div>"
                  "</td>"
                "<td id='maincontent'>"
                  "<div id='box_header'>";

P(index_Setup_begin) = "<h1>PLC Status</h1>"
                    "Use this section to view status of your PLC and to control the outputs."
                  "</div>"
                  "<div class='box'>"
                    "<h2>PLC Triacs Status</h2>"
                    "Please change outputs carefully. It may cause an electric shock. "
                    "Every output has a triac to control the load power.<br><br>"
                   "<FORM action='index' method='get'>"
                    "<table cellspacing='1' cellpadding='1' width='525' border='0'><tbody>";
                                  
P(lan_Setup_begin) = "<h1>Network Settings</h1>"
                    "Use this section to configure the internal network settings of your PLC. "
                    "<b>This section is optional and you do not need to change any of the settings here "
                    "to get your network up and running. </b> <br><br>"
                  "</div>"
                  "<div class='box'>"
                    "<h2>PLC LAN Settings</h2>"
                    "The IP address that is configured here is the IP address that you use to access "
                    "the Web-based management interface. If you change the IP address here, you may "
                    "need to adjust your PC's network settings to access the network again. <br><br>"
                   "<FORM action='LAN' method='get'>"
                    "<table cellspacing='1' cellpadding='1' width='525' border='0'><tbody>";

P(mqtt_Setup_begin) ="<h1>MQTT Settings</h1>"
                    "Use this section to configure the MQTT settings of your PLC. "
                    "<b>If you need to use your PLC with HomeAssistant change any of the settings here . </b> <br><br>"
                  "</div>"
                  "<div class='box'>"
                    "<h2>PLC MQTT Settings</h2>"
                    "The PLC can working without any MQTT brokers. If you want to use it as stand-alone device turn off MQTT support. "
                    "Some MQTT brokers need ClietID definition. For HomeAssistant it is not necessary.  "
                    "If you change any options here, you may need to reboot the PLC. <br><br>"
                   "<FORM action='MQTT' method='get'>"
                    "<table cellspacing='1' cellpadding='1' width='525' border='0'><tbody>";

P(lan_Setup_end) = "</tbody></table>"
                    "<input type='submit' value='Save Settigs'>"
                   "</FORM>"
                  "</div>"                
              "</td>"
              "<td class='h_td' id='help_text'>"
                "<strong>Helpful Hints.</strong><br><br>&nbsp;•&nbsp; If you have a DHCP server on " 
                "your network, you may check <strong>Enable DHCP </strong>to enable this feature."
              "</td>"              
            "</tr></tbody></table>";
                                  
P(Index) = "<h3>index</h3>"
           "This is your main site!<br>"
           "The code is found in the WebPages.h<br>"
           "You can add more web pages if you need. Please see the well documented source code.<br> <br>"
           "Use the following link to setup the network.<br>"
           "<a href='LAN'>NETWORK SETUP</a>";

P(table_td_class_l_tb_start) = "<td class='l_tb'><input maxlength='15' name='";

P(css) =  ".r_tb {TEXT-ALIGN: right}"
          "#topnavon {FONT-WEIGHT: bold; WIDTH: 147px;}"
          "#topnavon A{PADDING: 5px; DISPLAY: block; COLOR: #404343; BACKGROUND-COLOR: #f1f1f1; TEXT-ALIGN: center; TEXT-DECORATION: none}"
          "#topnavon A:hover {TEXT-DECORATION: underline}"
          "#sidenav_container {BACKGROUND-COLOR: #404343}"
          "#sidenav {text-align: left}"
          "#sidenav UL {PADDING: 0px;}"
          "#sidenav LI {BORDER-BOTTOM: #f1f1f1 1px solid;}"
          "#sidenav LI A {PADDING: 5px; DISPLAY: block; COLOR: white; TEXT-DECORATION: none}"
          "#sidenav LI A:hover {COLOR: #404343; BACKGROUND-COLOR: #f1f1f1; TEXT-DECORATION: underline}"
          "#help_text {COLOR: white; BACKGROUND-COLOR: #404343}"
          "#maincontent {PADDING: 5px; BACKGROUND-COLOR: white}"
          "#box_header {BORDER: #adc43b 1px solid; PADDING: 10px; PADDING-TOP: 0px; BACKGROUND-COLOR: #dfdfdf}"
          ".box {BORDER: #333333 1px solid; PADDING: 10px; MARGIN: 5px 0px 0px; PADDING-TOP: 0px;}"
          "H1 {PADDING: 5px; FONT-SIZE: 16px; MARGIN: 0px -10px 5px; BACKGROUND-COLOR: #adc43b}"
          "H2 {PADDING: 5px; FONT-SIZE: 14px; MARGIN: 0px -10px 5px; COLOR: white; BACKGROUND-COLOR: #333333}"
          ".mybody {MARGIN-TOP: 1px; MARGIN-LEFT: 0px; MARGIN-RIGHT: 0px; BACKGROUND-COLOR: #757575}"
          "TABLE.mytable {WIDTH: 838px; BACKGROUND-COLOR: white}"
          "TD.h_td {WIDTH: 138px}"
          ".optTable {}"
          ".slider {float: left;  margin: 3px; margin-left:5%; width: 100px}"
          ".indc {max-width: 40px}"
          ".fanSlider {float: left;  margin: 3px; margin-left:5%; width: 100px}"
          ".fanIndc {max-width: 40px}"
          ".rlySlider {float: left;  margin: 3px; margin-left:5%; width: 30px}"
          ".rlyIndc {max-width: 40px}";

#define NAMELEN 12
#define VALUELEN 24

void index_html(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete){
  URLPARAM_RESULT rc;
  char name[NAMELEN];
  char value[VALUELEN];
  bool params_present = false;

  if (type == WebServer::POST){
    bool repeat;
    do{
      /* readPOSTparam returns false when there are no more parameters to read from the input.  We pass in buffers for it to store the name and value strings along with the length of those buffers. */
      repeat = server.readPOSTparam(name, NAMELEN, value, VALUELEN);
      if (strncmp(name, "Pow", 3) == 0) {       // диммеры
          uint8_t idx = atoi(&name[3]);
          myPLC.setPower(idx, atoi(value));
          if ( myPLC.getPower(idx) > 0 ) myPLC.setOn(idx);
            else myPLC.setOff(idx);
          myPLC.needDelayedSaveRom = true;
      }
      if (strncmp(name, "fan", 3) == 0) {       // вентиляторы
          uint8_t idx = atoi(&name[3]);
          myPLC.setFanPower(idx, atoi(value));
          if ( myPLC.getFanPower(idx) > 0 ) myPLC.setFanOn(idx);
            else myPLC.setFanOff(idx);
          myPLC.needDelayedSaveRom = true;
      }
      if (strncmp(name, "rly", 3) == 0) {       // реле
          uint8_t idx = atoi(&name[3]);
          if (atoi(value)) myPLC.setRelayOn(idx);
            else myPLC.setRelayOff(idx);
          myPLC.needDelayedSaveRom = true;
      }
      if (strcmp(name, "dsbtnClick") == 0){       // запрос на получение адреса датчика температуры
        uint8_t idx = atoi(value);
        myPLC.setPower(0, idx);
        myPLC._DSsensors.readAddress(myPLC.DSaddrs[idx]);
        myPLC.needDelayedSaveRom = true;
      }
    } while (repeat); 
    server.httpSeeOther(URLPREFIX); // after procesing the POST data, tell the web browser to reload the page using a GET method.
    return;
  }


  server.httpSuccess();  
  if (type == WebServer::HEAD) return;

  if (strlen(url_tail)) {                          // check for parameters
    while (strlen(url_tail)) {
      rc = server.nextURLparam(&url_tail, name, NAMELEN, value, VALUELEN);
      if (rc != URLPARAM_EOS) {
        params_present = true;
        if (strcmp(name, "flipDisplay") == 0) {
          myPLC.flipDisplay = atoi(value);                               // read flipDisplay ON/OFF
          myPLC.rtDisplay();                                             // вращаем дисплей если надо 
        }
        if (strncmp(name, "Pow", 3) == 0) {
          byte idx = atoi(&name[3]);
          if (idx >= channelAmount) break;
          myPLC.setPower(idx, atoi(value));
          if ( myPLC.getPower(idx) > 0 ) myPLC.setOn(idx);
            else myPLC.setOff(idx);
        }
        if (strncmp(name, "tds", 3) == 0){                // парсим адрес датчика температуры
          byte idx = atoi(&name[3]);
          if (idx >= DS_SENS_AMOUNT) break;
          char * pch = strtok(value, ":");
          uint8_t i = 0;
          while (pch != NULL){                         // пока есть лексемы
            myPLC.DSaddrs[idx][i++] = strtol(pch, NULL, 16);
            pch = strtok (NULL, ":");
          }
        }
      }
    }
    if (params_present) myPLC.needDelayedSaveRom = true;
  }

  server.printP(Page_start);
  server.printP(top_menu);
  server.printP(Setup_begin);
  server.printP(index_Setup_begin);

  server.print("<tr><td class='r_tb'>Rotate display: </td>");
  server.print("<td class='l_tb'> <input type='radio' name='flipDisplay' value='0'");
  if(!myPLC.flipDisplay) server.print(" checked ");
  server.print(">Off <input type='radio' name='flipDisplay' value='1'");
  if(myPLC.flipDisplay) server.print(" checked ");
  server.print(">On </td></tr>");
  server.print(F("<tr><td>&nbsp;</td></tr>"));
  server.print(F("<tr><td></td><td>Triac outputs and discrete inputs</td></tr>"));  
  server.print(F("<tr><td>Id</td><td>Output power</td><td>Input state</td></tr>"));
  for (uint8_t i = 0; i < channelAmount; i++){
    server.print(F("<tr><td>")); server.print(i+1); server.print(F("</td>"));
    server.print(F("<td><div style='float: left'><input maxlength='4' class='indc' id='indc")); server.print(i); server.print("' name='Pow"); server.print(i);          //    <td class='l_tb'><input maxlength='15' name='mqtt_cfg' value='password
    server.print(F("' value='")); server.print(myPLC.getPower(i)); server.print("'></div> <div class='slider' id='dim"); server.print(i); server.print("'></div> </td>");
    server.print("<td> ");
    if (i < btnsAmount) { 
      server.print(F("<input type='checkbox' onclick='return false;'"));
      if(myPLC.getBtnState(i)) server.print(F(" checked "));
      server.print(">");
    }
    server.print(F("</td></tr>"));
  }
  server.print(F("</tbody></table><input type='submit' value='Save Settigs'></FORM></div>"));

  //-----===== fans ====------
  server.print(F( "<div class='box'>"
                    "<h2>Fans control section</h2>"
                    "You may direct control each fan's speed. Use 2 speeds funs.<br><br>"
                    "<FORM action='index' method='get'>"
                      "<table class='optTable' cellspacing='1' cellpadding='1' width='525' border='0'>"
                        "<tbody>"
                          "<tr><td>Id</td><td>Output power</td></tr>"));
  for (uint8_t i = 0; i < fansAmount; i++){
    server.print(F("<tr><td>")); server.print(i+1); server.print(F("</td>"));
    server.print(F("<td><div style='float: left'><input maxlength='4' class='fanIndc' id='fanIndc")); server.print(i); server.print("' name='fanPow"); server.print(i);          //    <td class='l_tb'><input maxlength='15' name='mqtt_cfg' value='password
    server.print(F("' value='")); server.print(myPLC.getFanPower(i)); server.print("'></div> <div class='fanSlider' id='fan"); server.print(i); server.print("'></div> </td>");
    server.print("</tr> ");
  }
  server.print(F("</tbody></table></FORM></div>"));

  //-----===== relays ====------
  server.print(F( "<div class='box'>"
                    "<h2>Relays control section</h2>"
                    "You may direct control each relay.<br><br>"
                    "<FORM action='index' method='get'>"
                      "<table class='optTable' cellspacing='1' cellpadding='1' width='525' border='0'>"
                        "<tbody>"
                          "<tr><td>Id</td><td>State toggle</td></tr>"));
  for (uint8_t i = 0; i < relaysAmount; i++){
    server.print(F("<tr><td>")); server.print(i+1); server.print(F("</td>"));
    server.print(F("<td><div style='float: left'><input maxlength='4' class='rlyIndc' id='rlyIndc")); server.print(i); server.print("' name='rlyPow"); server.print(i);          //    <td class='l_tb'><input maxlength='15' name='mqtt_cfg' value='password
    server.print(F("' value='")); server.print(myPLC.getRelayOnOff(i)); server.print("'></div> <div class='rlySlider' id='rly"); server.print(i); server.print("'></div> </td>");
    server.print("</tr> ");
  }
  server.print(F("</tbody></table></FORM></div>"));
  
  //-----===== ds sensors ====------
  server.print(F( "<div class='box'>"
                    "<h2>DS18b20 sensors status</h2>"
                    "Use this section to detect and view status of your ds18b20 sensors in 1-wire network.<br><br>"
                    "<FORM action='index' method='get'>"
                      "<table cellspacing='1' cellpadding='1' width='525' border='0'>"
                        "<tbody>"
                          "<tr><td>Id</td><td>Description</td><td>Address</td><td>Temp</td><td>Link</td><td>Addr</td></tr>"));
  for (uint8_t i = 0; i < DS_SENS_AMOUNT; i++){
    server.print("<tr><td>"); server.print(i+1); server.print("</td>");
    switch (i) { 
      case 0: server.print(F("<td>Ext. temp</td>")); break;
      case 1: server.print(F("<td>Fresh air after heater</td>")); break;
      case 2: server.print(F("<td>Fresh air after recuper</td>")); break;
      case 3: server.print(F("<td>Waste air after recuper</td>")); break;
      case 4: server.print(F("<td>Supply t for heater</td>")); break;
    }
    server.print("<td class='l_tb'><input maxlength='23' name='tds"); server.print(i); server.print("' value='");  //    <td class='l_tb'><input maxlength='20' name='tds1' value='28:FF:78:5B:50:17:4:CF'> </td>
    for( uint8_t j = 0; j < sizeof(myPLC.DSaddrs[i]); j++){
      server.print(myPLC.DSaddrs[i][j],HEX);
    if (j != sizeof(myPLC.DSaddrs[i]) - 1 ) server.print(":");
    }
    server.print(F("'> </td>"));
    server.print(F("<td>")); server.print(myPLC._DSsensors.getTemp(i), 1); server.print(F("°C</td>"));
    server.print(F("<td><input type='checkbox' onclick='return false;'")); if(myPLC._DSsensors.state[i]) server.print(F(" checked ")); server.print(F("></td>"));
    server.print(F("<td><button type='button' onclick='dsbtnClick(this)' id='")); server.print(i); server.print("'>Get</button></td>");
    server.print(F("</tr>"));
  }  
  server.printP(lan_Setup_end);
  server.printP(Page_end);
}

void LAN_html(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete){
  URLPARAM_RESULT rc;
  char name[NAMELEN];
  char value[VALUELEN];
  bool params_present = false;

  server.httpSuccess();     /* this line sends the standard "we're all OK" headers back to the browser */
  /* if we're handling a GET or POST, we can output our data here. For a HEAD request, we just stop after outputting headers. */
  if (type == WebServer::HEAD) return;

  if (strlen(url_tail)) {                          // check for parameters
    while (strlen(url_tail)) {
      rc = server.nextURLparam(&url_tail, name, NAMELEN, value, VALUELEN);
      if (rc != URLPARAM_EOS) {
        params_present = true;
        #ifdef DEBUG              // debug output for parameters
          server.print(name);
          server.print(" - ");
          server.print(value);
          server.print("<br>");
        #endif

        if (strcmp(name, "mac") == 0){                // парсим mac-адрес
          char * pch = strtok(value, ":");
          uint8_t i = 0;
          while (pch != NULL){                         // пока есть лексемы
            myPLC.inet_cfg.mac[i++] = strtol(pch, NULL, 16);
            pch = strtok (NULL, ":");
          }
        }
        IPAddress ip1;
        if ((strcmp(name, "ip") == 0)&&(ip1.fromString(value))) memcpy( myPLC.inet_cfg.ip, &ip1[0], sizeof(myPLC.inet_cfg.ip));         // read IP address
        if ((strcmp(name, "mask") == 0)&&(ip1.fromString(value))) memcpy( myPLC.inet_cfg.mask, &ip1[0], sizeof(myPLC.inet_cfg.mask));   // read SUBNET 
        if ((strcmp(name, "gw") == 0)&&(ip1.fromString(value))) memcpy( myPLC.inet_cfg.gw, &ip1[0], sizeof(myPLC.inet_cfg.gw));         // read GATEWAY
        if ((strcmp(name, "dns") == 0)&&(ip1.fromString(value))) memcpy( myPLC.inet_cfg.dns, &ip1[0], sizeof(myPLC.inet_cfg.dns));      // read DNS-SERVER
        if (strcmp(name, "webPort") == 0) myPLC.inet_cfg.webSrvPort = atoi(value);                                                      // read WEBServer port
        if (strcmp(name, "dhcp") == 0) myPLC.inet_cfg.use_dhcp = atoi(value);                                                           // read DHCP ON/OFF
      }
    }
    if (params_present) myPLC.needDelayedSaveRom = true;
  }

  server.printP(Page_start);
  server.printP(top_menu);
  server.printP(Setup_begin);
  server.printP(lan_Setup_begin);  
  server.print("<tr><td class='r_tb' width='200'>PLC MAC Address:</td>");     
  server.print("<td class='l_tb'><input maxlength='17' name='mac' value='");  //    <td class='l_tb'><input maxlength='17' name='mac' value='90:B2:FA:0D:4E:59
  for( uint8_t i = 0; i < sizeof(myPLC.inet_cfg.mac); i++){
    server.print(myPLC.inet_cfg.mac[i],HEX);
    if (i != sizeof(myPLC.inet_cfg.mac) - 1 ) server.print(":");
  }
  server.print("'> </td> </tr>");
  
  server.print("<tr><td class='r_tb'>PLC IP Address:</td>");
  server.printP(table_td_class_l_tb_start); server.print("ip' value='");          //    <td class='l_tb'><input maxlength='15' name='ip' value='
  for( uint8_t i = 0; i < sizeof(myPLC.inet_cfg.ip); i++){
    server.print(myPLC.inet_cfg.ip[i]);                                     //      192.168.0.211
    if (i != sizeof(myPLC.inet_cfg.ip) - 1 ) server.print(".");
  }
  server.print("'> </td> </tr>");

  server.print("<tr><td class='r_tb'>Subnet Mask:</td>");                                                                         
  server.printP(table_td_class_l_tb_start); server.print("mask' value='");      //    <td class='l_tb'><input maxlength='15' name='mask' value='255.255.255.0  
  for( uint8_t i = 0; i < sizeof(myPLC.inet_cfg.mask); i++){
    server.print(myPLC.inet_cfg.mask[i]);
    if (i != sizeof(myPLC.inet_cfg.mask) - 1 ) server.print(".");
  }
  server.print("'> </td> </tr>");

  server.print("<tr><td class='r_tb'>Gateway IP:</td>");                                                                          
  server.printP(table_td_class_l_tb_start); server.print("gw' value='");        //    <td class='l_tb'><input maxlength='15' name='gw' value='192.168.0.1  
  for( uint8_t i = 0; i < sizeof(myPLC.inet_cfg.gw); i++){
    server.print(myPLC.inet_cfg.gw[i]);
    if (i != sizeof(myPLC.inet_cfg.gw) - 1 ) server.print(".");
  }
  server.print("'> </td> </tr>");

  server.print("<tr><td class='r_tb'>DNS:</td>");
  server.printP(table_td_class_l_tb_start); server.print("dns' value='");       //    <td class='l_tb'><input maxlength='15' name='dns' value='192.168.0.1  
  for( uint8_t i = 0; i < sizeof(myPLC.inet_cfg.dns); i++){
    server.print(myPLC.inet_cfg.dns[i]);
    if (i != sizeof(myPLC.inet_cfg.dns) - 1 ) server.print(".");
  }
  server.print("'> </td> </tr>");

  server.print("<tr><td class='r_tb'>Web Server Port:</td>");
  server.print("<td class='l_tb'><input maxlength='6' name='webPort' value='"); 
  server.print(myPLC.inet_cfg.webSrvPort);
  server.print("'> </td> </tr>");

  server.print("<tr><td class='r_tb'>Use DHCP: </td>");
  server.print("<td class='l_tb'> <input type='radio' name='dhcp' value='0'");
  if(!myPLC.inet_cfg.use_dhcp) server.print(" checked ");
  server.print(">Off <input type='radio' name='dhcp' value='1'");
  if(myPLC.inet_cfg.use_dhcp) server.print(" checked ");
  server.print(">On </td> </tr>");

  server.printP(lan_Setup_end);
  server.printP(Page_end);
}

void MQTT_html(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete){
  URLPARAM_RESULT rc;
  char name[NAMELEN];
  char value[VALUELEN];
  bool params_present = false;

  server.httpSuccess();  
  if (type == WebServer::HEAD) return;

  if (strlen(url_tail)) {                          // check for parameters
    while (strlen(url_tail)) {
      rc = server.nextURLparam(&url_tail, name, NAMELEN, value, VALUELEN);
      if (rc != URLPARAM_EOS) {
        params_present = true;
        IPAddress ip1;
        if (strcmp(name, "mqttUser") == 0) strcpy( myPLC.mqtt_cfg.User, value);         // read mqttUser
        if (strcmp(name, "mqttPass") == 0) strcpy( myPLC.mqtt_cfg.Pass, value);         // read mqttPass
        if (strcmp(name, "mqttCltID") == 0) strcpy( myPLC.mqtt_cfg.ClientID, value);         // read mqttClientID
        if (strcmp(name, "HADiscov") == 0) strcpy( myPLC.mqtt_cfg.HADiscover, value);                                                         // read "HomeAssistant" 
        if (strcmp(name, "HABirth") == 0) strcpy( myPLC.mqtt_cfg.HABirthTopic, value);                                                         // read "homeassistant/status" 
        if (strcmp(name, "mqttSrvIP") == 0){
          if (ip1.fromString(value)) memcpy( myPLC.mqtt_cfg.SrvIP, &ip1[0], sizeof(myPLC.mqtt_cfg.SrvIP));                                   // read mqttSrvIP
          else {
            DNSClient DNSClient1;
            DNSClient1.begin(IPAddress(myPLC.inet_cfg.dns));
            DNSClient1.getHostByName(value, ip1);
            memcpy( myPLC.mqtt_cfg.SrvIP, &ip1[0], sizeof(myPLC.mqtt_cfg.SrvIP));
          }
        }
        if (strcmp(name, "mqttPort") == 0) myPLC.mqtt_cfg.SrvPort = atoi(value);                                                        // read mqttSrvPort port
        if (strcmp(name, "useMQTT") == 0) myPLC.set_useMQTT(atoi(value));                                                               // read useMQTT ON/OFF 
      }
    }
    if (params_present) myPLC.needDelayedSaveRom = true;
  }

  server.printP(Page_start);
  server.printP(top_menu);
  server.printP(Setup_begin);
  server.printP(mqtt_Setup_begin);  
  server.print("<tr><td class='r_tb' width='200'>MQTT User:</td>");     
  server.print("<td class='l_tb'><input maxlength='17' name='mqttUser' value='");  //    <td class='l_tb'><input maxlength='17' name='mqttUser' value='mqttUserName
  server.print(myPLC.mqtt_cfg.User);
  server.print("'> </td> </tr>");
  
  server.print("<tr><td class='r_tb'>MQTT Pass:</td>");
  server.printP(table_td_class_l_tb_start); server.print("mqttPass' value='");          //    <td class='l_tb'><input maxlength='15' name='mqtt_cfg' value='password
  server.print(myPLC.mqtt_cfg.Pass);                                     
  server.print("'> </td> </tr>");

  server.print("<tr><td class='r_tb'>Client ID:</td>");                                                                         
  server.printP(table_td_class_l_tb_start); server.print("mqttCltID' value='");      //    <td class='l_tb'><input maxlength='15' name='mqttCltID' value='Client ID  
  server.print(myPLC.mqtt_cfg.ClientID);
  server.print("'> </td> </tr>");

  server.print("<tr><td class='r_tb'>HA AutoDiscovery topic:</td>");                                                                          
  server.printP(table_td_class_l_tb_start); server.print("HADiscov' value='");        //    <td class='l_tb'><input maxlength='15' name='HADiscov' value='HomeAssistant  
  server.print(myPLC.mqtt_cfg.HADiscover);
  server.print("'> </td> </tr>");

  server.print("<tr><td class='r_tb'>HA Birth topic:</td>");                                                                          
  server.printP(table_td_class_l_tb_start); server.print("HABirth' value='");        //    <td class='l_tb'><input maxlength='15' name='HADiscov' value='HomeAssistant  
  server.print(myPLC.mqtt_cfg.HABirthTopic);
  server.print("'> </td> </tr>");

  server.print("<tr><td class='r_tb'>MQTT Broker IP:</td>");
  server.printP(table_td_class_l_tb_start); server.print("mqttSrvIP' value='");       //    <td class='l_tb'><input maxlength='15' name='mqttSrvIP' value='192.168.0.124  
  for( uint8_t i = 0; i < sizeof(myPLC.mqtt_cfg.SrvIP); i++){
    server.print(myPLC.mqtt_cfg.SrvIP[i]);
    if (i != sizeof(myPLC.mqtt_cfg.SrvIP) - 1 ) server.print(".");
  }
  server.print("'> </td> </tr>");
  
  server.print("<tr><td class='r_tb'>MQTT Server Port:</td>");
  server.print("<td class='l_tb'><input maxlength='6' name='mqttPort' value='"); 
  server.print(myPLC.mqtt_cfg.SrvPort);
  server.print("'> </td> </tr>");

  server.print("<tr><td class='r_tb'>Use MQTT: </td>");
  server.print("<td class='l_tb'> <input type='radio' name='useMQTT' value='0'");
  if(!myPLC.mqtt_cfg.useMQTT) server.print(" checked ");
  server.print(">Off <input type='radio' name='useMQTT' value='1'");
  if(myPLC.mqtt_cfg.useMQTT) server.print(" checked ");
  server.print(">On </td> </tr>");

  if (myPLC.mqtt_cfg.isHAonline) server.print("<tr><td class='r_tb'>HomeAssistant is</td><td class='l_tb'><b>online</b></td></tr>");
    else server.print("<tr><td class='r_tb'>HomeAssistant is</td><td class='l_tb'><b>offline</b></td></tr>");
  
  server.printP(lan_Setup_end);
  server.printP(Page_end);
}

/**
* errorHTML() function
* This function is called whenever a non extisting page is called.
* It sends a HTTP 400 Bad Request header and the same as text.
*/
void error_html(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete){
  server.httpFail();  /* this line sends the standard "HTTP 400 Bad Request" headers back to the browser */
  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type == WebServer::HEAD) return;
  server.printP(Http400); 
  server.printP(Page_end);
}

void PLC_css(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete){
  /* this line sends the standard "we're all OK" headers back to the
     browser */
  server.httpSuccess();
  if (type == WebServer::HEAD) return;
  server.printP(css);
}

/*

void buzzCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete){
  if (type == WebServer::POST)
  {
    bool repeat;
    char name[16], value[16];
    do
    {
      // readPOSTparam returns false when there are no more parameters
      // * to read from the input.  We pass in buffers for it to store
      // * the name and value strings along with the length of those
      // * buffers. 
      repeat = server.readPOSTparam(name, 16, value, 16);

      // * this is a standard string comparison function.  It returns 0
      // * when there's an exact match.  We're looking for a parameter
      // * named "buzz" here. 
      if (strcmp(name, "buzz") == 0)
      {
  // * use the STRing TO Unsigned Long function to turn the string
  // * version of the delay number into our integer buzzDelay
  // * variable 
        myPLC.setPower(0, strtoul(value, NULL, 10));
      }
    } while (repeat);
    
    // after procesing the POST data, tell the web browser to reload
    // the page using a GET method. 
    server.httpSeeOther(PREFIX);
    return;
  }
  // * for a GET or HEAD, send the standard "it's all OK headers" 
  server.httpSuccess();

  // * we don't output the body for a HEAD request 
  if (type == WebServer::GET)
  {
    // * store the HTML in program memory using the P macro 
    
    P(message) = 
      "<html>"
        "<head>"
          "<title>PLC Channel1</title>"
        "</head>"  
        "<body>"
          "<h1>Set PLC Channel1 Britness!</h1>"
          "<form action='/buzz' method='POST'>"
            "<p><button name='buzz' value='0'>Turn if Off!</button></p>"
            "<p><button name='buzz' value='10'>10</button></p>"
            "<p><button name='buzz' value='15'>15</button></p>"
            "<p><button name='buzz' value='50'>50</button></p>"
            "<p><button name='buzz' value='100'>100</button></p>"
          "</form>"
        "</body>"
      "</html>";
      
    server.printP(message);
  }
}

*/
