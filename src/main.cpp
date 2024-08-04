#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "HeaterState.hpp"
#include "MatrixPanel_CC.h"
#include "HardwareConstants.h"
#include "TomThumbCAC.h"
#include "ImpactFull12.h"

WebServer server(80);
void handleCommand();
void handleCurrentReading();
void updateDisplay(HeaterState curState);

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


void setup() {

  Serial.begin(115200);
      // Set up the esp32 as a WiFi AP
    WiFi.softAP("ESP32-AP", "powerpass");
    Serial.println(WiFi.softAPIP());


  server.on("/cm", HTTP_GET, handleCommand);
  server.on("/current", HTTP_GET, handleCurrentReading);

   server.onNotFound([]() {
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
    server.send(404, "text/plain", "Not found");
  });

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

void loop() {
  static time_t lastReadTime = 0;
  
  server.handleClient();
  
  heaterMonitor.update(currentReading, lastCurUpdate);

  // Print the current reading and the  flag every 3 second.
  if (millis() % 3000 == 0)
  {
    Serial.println("Current Reading: " + String(currentReading) + " curState: " + String((int)heaterMonitor.getState()) + " @ " + String(lastCurUpdate));
    Serial.println((int)heaterMonitor.getState());
  }

  updateDisplay(heaterMonitor.getState());

  yield();
  
  
}

void updateDisplay(HeaterState curState)
{
  static HeaterState lastState = HeaterState::UNKNOWN;

  if (curState == lastState)
  {
    return;
  }

  switch(curState)
  {
    case HeaterState::HOT:
      dmaDisplay->fillScreen(COLOR_BLACK);
      dmaDisplay->printCenter(32, 26, COLOR_RED, "HOT");
      break;
    case HeaterState::WARMING:
      dmaDisplay->fillScreen(COLOR_BLACK);
      dmaDisplay->printCenter(32, 26, COLOR_DARKORANGE, "WARMING");
      break;
    case HeaterState::COOL:
      dmaDisplay->fillScreen(COLOR_BLACK);
      dmaDisplay->printCenter(32, 26, COLOR_LIGHTBLUE, "COOL");
      break;
    case HeaterState::COOLING:
      dmaDisplay->fillScreen(COLOR_BLACK);
      dmaDisplay->printCenter(32, 26, COLOR_ORANGE, "COOLING");
      break;
    case HeaterState::UNKNOWN:
      dmaDisplay->fillScreen(COLOR_BLACK);
      dmaDisplay->printCenter(32, 26, COLOR_RED, "????");
      break;
  }

  lastState = curState;

  return;
}

void handleCommand() {
  if (server.hasArg("cmnd")) {
    String cmnd = server.arg("cmnd");
    if (cmnd.startsWith("/current?value=")) {
      String value = cmnd.substring(15); // Extract the value after "="
      Serial.println("Current reading via cm: " + value);
      currentReading = value.toFloat();
      server.send(200, "text/plain", "Received: " + value);
      lastCurUpdate = millis();
    } else {
      server.send(400, "text/plain", "Invalid command");
    }
  } else {
    server.send(400, "text/plain", "No command provided");
  }
}

void handleCurrentReading() {
  if (server.hasArg("value")) {
    String current = server.arg("value");
    Serial.println("Current reading via current: " + current);
    currentReading = current.toFloat();
    lastCurUpdate = millis();
    server.send(200, "text/plain", "Received: " + current);
  } else {
    server.send(400, "text/plain", "No current value provided");
  }
}
