// #################  WebServer #################

#ifndef __MODWEBSERVER_H
#define __MODWEBSERVER_H
#define MODWEBSERVER

#include <ESP8266WebServer.h>
#include "Debug.h"
#include "Settings.h"

ESP8266WebServer server (WEBSERVER_PORT);


void returnOK () {
  //--------------
  server.send(200, F("text/plain"), "");
}


void returnFail(String msg) {
  //-------------------------
  server.send(500, F("text/plain"), msg + "\r\n");
}

void handleHelp () {
//-----------------  
  String out = "<html>";
  out += "<body><h2>Hilfe</h2>";
  out += "<br><br><table>";
  out += "<tr><td>/</td><td>zeigt alle Messwerte in einer Tabelle; refresh alle 10 Sekunden</td></tr>";
  out += "<tr><td>/data</td><td>zum Abruf der Messwerte in der Form Name=wert</td></tr>";
  out += "<tr><td>:{port+1}/update</td><td>OTA</td></tr>";
  out += "<tr><td>/reboot</td><td>startet neu</td></tr>";
  out += "</table></body></html>";
  server.send (200, "text/html", out);
}


void handleReboot () {
  //-------------------
  returnOK ();
  ESP.reset();
}


void handleRoot() {
  //----------------
  String out = "<html><head><meta http-equiv=\"refresh\" content=\"10\":URL=\"" + server.uri() + "\"></head>";
  out += "<body>";
  out += "<h2>Hoymiles Micro-Inverters</h2>";
  char floatString[20];
  char line[100];
  for (uint8_t wr = 0; wr < anzInv; wr++) {
    out += "<h3>" + String(inverters[wr].name) + "</h3>";
    out += "<h3>S/N " + String (getSerialNoTxt(wr)) + "</h3>";
    out += "<br><br><table border='1'>";
    out += "<tr><th>Kanal</th><th>Wert</th><th>Einheit</th></tr>";
    for (uint8_t i = 0; i < inverters[wr].anzTotalMeasures; i++) {
      dtostrf (getMeasureValue(wr, i),1, getDigits(wr,i), floatString);
      sprintf(line, "<tr><td>%s</td><td>%s</td><td>%s</td></tr>", getMeasureName(wr, i), floatString, getUnit(wr, i));
      //DEBUG_OUT.println(line);
      out += String(line); 
/*        out += "<tr><td>" + getMeasureName(i) + "</td>";
      out += "<td>" +  String(getMeasureValue(i)) + "</td></tr>";
      out += "<td>" +  String(getUnit(i)) + "</td></tr>"; */
    }
    out += "</table>";
  }
  int pos = out.indexOf("°");
  do {
    if (pos>1) {
      out = out.substring (0, pos) + "&deg;" + out.substring(pos+2);
    }
    pos = out.indexOf("°");
  } while (pos>1);
 
  out += "</body></html>";
  server.send (200, "text/html", out);
  //DEBUG_OUT.println (out);
}


void handleData () {
//-----------------
  String out = "";
  for (uint8_t wr = 0; wr < anzInv; wr++) {
    for (int i = 0; i < inverters[wr].anzTotalMeasures; i++) {
      out += (anzInv <= 1 ? "" : String (wr) + ".") + String(getMeasureName(wr,i)) + '=' 
             + String (getMeasureValue(wr,i)) /*+ ' ' + String(getUnit(wr,i))*/ + '\n';
    }
  }
  server.send(200, "text/plain", out);  
}


void handleNotFound() {
//--------------------  
  String message  = "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


void setupWebServer (void) {
  //-------------------------
  server.begin();
  server.on("/",        handleRoot);
  server.on("/reboot",  handleReboot);
  server.on("/data",    handleData);
  server.on("/help",    handleHelp);
  //server.onNotFound(handleNotFound);    wegen Spiffs-Dateimanager

  DEBUG_OUT.println ("[HTTP] installed");
}

void webserverHandle() {
//====================
  server.handleClient();  
}


// #################  OTA #################

#ifdef WITH_OTA
#include <ESP8266HTTPUpdateServer.h>

ESP8266WebServer httpUpdateServer (UPDATESERVER_PORT);
ESP8266HTTPUpdateServer httpUpdater;

void setupUpdateByOTA () {
  //------------------------
  httpUpdater.setup (&httpUpdateServer, UPDATESERVER_DIR, UPDATESERVER_USER, UPDATESERVER_PW);
  httpUpdateServer.begin();
  DEBUG_OUT.println ("[OTA] installed");
}

void checkUpdateByOTA() {
//---------------------
  httpUpdateServer.handleClient();  
}
#endif

#endif
