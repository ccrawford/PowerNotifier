#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "HeaterState.hpp"
#include "MatrixPanel_CC.h"
#include "HardwareConstants.h"
#include "TomThumbCAC.h"
#include "ImpactFull12.h"
#include "esp_wifi.h"

WebServer server(80);
void handleCommand();
void handleCurrentReading();
void updateDisplay(HeaterState curState);
void updateTimer(long seconds);


float currentReading = 0.0;
unsigned long lastCurUpdate = 0;

HeaterMonitor heaterMonitor;

const char compile_info[] = __FILE__ " " __DATE__ " " __TIME__ " ";

HUB75_I2S_CFG::i2s_pins _pins = {R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
HUB75_I2S_CFG mxconfig(
    64,                    // width
    32,                    // height
    1,                     // chain length
    _pins,                 // pin mapping
    HUB75_I2S_CFG::FM6126A // driver chip
);
MatrixPanel_CC *dmaDisplay = MatrixPanel_CC::getInstance(mxconfig); // mxconfig is setup over in the hardware constants file.

void setup()
{

  Serial.begin(115200);
  // Set up the esp32 as a WiFi AP. Password: powerpass. I don't care who knows this. Whatcha gonna do, update my sign?
  WiFi.softAP("HEATPLUG_MONITOR", "powerpass");
  Serial.println(WiFi.softAPIP());

  server.on("/clients", HTTP_GET, []()
            {
    wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t adapter_sta_list;
    WiFi.softAPgetStationNum(); // Get the number of connected devices
    esp_wifi_ap_get_sta_list(&wifi_sta_list); // Get the list of connected stations
    tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list); // Get adapter info
    String respString;
    for (int i = 0; i < adapter_sta_list.num; i++) {
      tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
      respString += "MAC: ";
//      Serial.print("MAC: ");
//      String macAddress;
      for (int i = 0; i < 6; i++) {
        respString += String(station.mac[i], HEX);
        if (i < 5) {
          respString += ":";
        }
      }
//      Serial.println(macAddress);
      respString += " IP: " + IPAddress(station.ip.addr).toString() + String("\n");
      Serial.print("IP: ");
      Serial.println(IPAddress(station.ip.addr));
    }
    server.send(200, "text/plain", "Clients: " + String(WiFi.softAPgetStationNum()) + "\n" + respString); });

  server.on("/cm", HTTP_GET, handleCommand);
  server.on("/current", HTTP_GET, handleCurrentReading);

  server.onNotFound([]()
                    {
    String message = "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    for (uint8_t i = 0; i < server.args(); i++) {
      message += "\n" + server.argName(i) + ": " + server.arg(i);
    }
    Serial.println(message);
    server.send(404, "text/plain", "Not found"); });

  server.begin();

  dmaDisplay->resetPanel(_pins);
  dmaDisplay->setRotation(0);
  dmaDisplay->begin();
  dmaDisplay->setBrightness8(255);
  dmaDisplay->setFont(&TomThumb);

  dmaDisplay->fillScreen(COLOR_BLACK);
  dmaDisplay->printCenter(32, 20, COLOR_WHITE, "Startup");

  dmaDisplay->setFont(&Impact12Caps);
}

void loop()
{
  static time_t lastReadTime = 0;

  server.handleClient();

  heaterMonitor.update(currentReading, lastCurUpdate);

  // Print the current reading and the  flag every 5 second.
  if (millis() % 5000 == 0)
  {
    Serial.println("Current Reading: " + String(currentReading) + " curState: " + String((int)heaterMonitor.getState()) + " @ " + String(lastCurUpdate));
    Serial.println((int)heaterMonitor.getState());
  }

  updateDisplay(heaterMonitor.getState());
  if(millis() % 1000 == 0) updateTimer(heaterMonitor.secondsSinceLastStateChange());

  yield();
}

void updateDisplay(HeaterState curState)
{
  static HeaterState lastState = HeaterState::UNKNOWN;

  dmaDisplay->setFont(&Impact12Caps);

  if (curState == lastState)
  {
    return;
  }

  int bottomY = 20;

  switch (curState)
  {
  case HeaterState::HOT:
    dmaDisplay->fillScreen(COLOR_BLACK);
    dmaDisplay->printCenter(32, bottomY, COLOR_RED, "HOT");
    dmaDisplay->setBrightness8(255);
    break;
  case HeaterState::WARMING:
    dmaDisplay->fillScreen(COLOR_BLACK);
    dmaDisplay->printCenter(32, bottomY, COLOR_DARKORANGE, "WARM");
    dmaDisplay->setBrightness8(255);
    break;
  case HeaterState::COOL:
    dmaDisplay->fillScreen(COLOR_BLACK);
    dmaDisplay->printCenter(32, bottomY, COLOR_BLUE, "COLD");
    dmaDisplay->setBrightness8(255);
    break;
  case HeaterState::OFF:
    dmaDisplay->fillScreen(COLOR_BLACK);
    dmaDisplay->printCenter(32, bottomY, COLOR_WHITE, "OFF");
    dmaDisplay->setBrightness8(10);
    break;
  case HeaterState::COOLING:
    dmaDisplay->fillScreen(COLOR_BLACK);
    dmaDisplay->printCenter(32, bottomY, COLOR_LIGHTBLUE, "WARM");
    dmaDisplay->setBrightness8(255);
    break;
  case HeaterState::UNKNOWN:
    dmaDisplay->fillScreen(COLOR_BLACK);
    dmaDisplay->printCenter(32, bottomY, COLOR_ORANGE, "????");
    dmaDisplay->setBrightness8(255);
    break;
  }

  lastState = curState;

  return;
}

void updateTimer(long dispSeconds)
{
  // format seconds into hh:mm:ss
  long hours = dispSeconds / 3600;
  long minutes = (dispSeconds % 3600) / 60;
  long secs = dispSeconds % 60;
  // Format time. Skip hours if it's 0.
  char timeStr[9];
    
  if (hours > 0)
  {
    sprintf(timeStr, "%ld:%02ld:%02ld", hours, minutes, secs);
  }
  else
  {
    sprintf(timeStr, "%02ld:%02ld", minutes, secs);
  }

  dmaDisplay->fillRect(0,21,64,10,COLOR_BLACK);
  dmaDisplay->setFont(&TomThumb);
  dmaDisplay->printAt(0, 31, COLOR_WHITE50, timeStr);
}

void handleCommand()
{
  if (server.hasArg("cmnd"))
  {
    String cmnd = server.arg("cmnd");
    if (cmnd.startsWith("/current?value="))
    {
      String value = cmnd.substring(15); // Extract the value after "="
      Serial.println("Current reading via cm: " + value);
      currentReading = value.toFloat();
      server.send(200, "text/plain", "Received: " + value);
      lastCurUpdate = millis();
    }
    else
    {
      server.send(400, "text/plain", "Invalid command");
    }
  }
  else
  {
    server.send(400, "text/plain", "No command provided");
  }
}

void handleCurrentReading()
{
  if (server.hasArg("value"))
  {
    String current = server.arg("value");
    Serial.println("Current reading via current: " + current);
    currentReading = current.toFloat();
    lastCurUpdate = millis();
    server.send(200, "text/plain", "Received: " + current);
  }
  else
  {
    server.send(400, "text/plain", "No current value provided");
  }
}

void listConnectedDevices()
{
  wifi_sta_list_t wifi_sta_list;
  tcpip_adapter_sta_list_t adapter_sta_list;

  memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
  memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

  esp_wifi_ap_get_sta_list(&wifi_sta_list);
  tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

  for (int i = 0; i < adapter_sta_list.num; i++)
  {
    tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
    Serial.print("Station ");
    Serial.print(i + 1);
    Serial.print(" - MAC: ");
    for (int j = 0; j < 6; j++)
    {
      Serial.printf("%02X", station.mac[j]);
      if (j < 5)
        Serial.print(":");
    }
    Serial.print(" - IP: ");
    Serial.println(IPAddress(station.ip.addr));
  }
}