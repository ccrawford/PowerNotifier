#include <vector>
#include <tuple>
#include <map>

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
bool shouldDisplayBeOn();

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
  while (!Serial)
    ;

  Serial.println(compile_info);

  dmaDisplay->resetPanel(_pins);
  dmaDisplay->setRotation(0);
  dmaDisplay->begin();
  dmaDisplay->setBrightness8(255);
  dmaDisplay->setFont(&TomThumb);

  dmaDisplay->fillScreen(COLOR_BLACK);
  dmaDisplay->printCenter(32, 7, COLOR_WHITE, "Startup");

  // First, connect to internet and check time of day.
  dmaDisplay->setCursor(0, 14);
  dmaDisplay->setTextWrap(true);
  dmaDisplay->print("Connecting WiFi");
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.begin("ge_wifi", "");
  while (WiFi.status() != WL_CONNECTED)
  {
    static int retryCounter = 0;
    delay(500);
    Serial.print(".");
    dmaDisplay->print(".");
    retryCounter++;
    if (retryCounter > 50)
    {
      dmaDisplay->fillScreen(COLOR_BLACK);
      dmaDisplay->printAt(0, 14, COLOR_RED, "Failed to connect");
      delay(5000);
      // reset and try again
      ESP.restart();
    }
  }
  Serial.println("Connected to WiFi");
  dmaDisplay->fillScreen(COLOR_BLACK);
  dmaDisplay->printAt(0, 14, COLOR_GREEN, "Connected!");
  dmaDisplay->printAt(0, 21, COLOR_WHITE, "Get Time");

  // Get ntp time and set.
  struct tm timeinfo;
  int retryCounter = 0;
  do
  {
    dmaDisplay->print(".");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    retryCounter++;
    delay(500);
  } while (!getLocalTime(&timeinfo) && retryCounter < 4);

  // Set timezone to Central Standard Time and daylight savings time to false.
  setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
  tzset();
  // Check the time is right
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    dmaDisplay->printAt(0, 28, COLOR_RED, "No time fetch");
    return;
  }
  else
  { // Print the time
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    dmaDisplay->printAt(0, 28, asctime(&timeinfo));
  }

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
      for (int i = 0; i < 6; i++) {
        respString += String(station.mac[i], HEX);
        if (i < 5) {
          respString += ":";
        }
      }
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

  dmaDisplay->setFont(&Impact12Caps);
}

void loop()
{
  static time_t lastReadTime = 0;
  static unsigned long lastTimeUpdate = 0;
  static unsigned long lastUpdate = 0;

  server.handleClient();

  heaterMonitor.update(currentReading, lastCurUpdate);

  updateDisplay(heaterMonitor.getState());

  if (millis() - lastUpdate > 200)
  {
    updateTimer(heaterMonitor.secondsSinceLastTrendChange());
    lastUpdate = millis();
  }

  // Print the current reading and the  flag every 5 second.
  if (millis() % 5000 == 0)
  {
    Serial.println("Current Reading: " + String(currentReading) + " curState: " + String((int)heaterMonitor.getState()) + " @ " + String(lastCurUpdate));
    Serial.println((int)heaterMonitor.getState());
  }

  // Update the time at 2am local time if it's been more than 2 hours since the last update.
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  // if (timeinfo.tm_sec == 0 && (millis() - lastTimeUpdate) > 60 * 60 * 1000)
  //  We can try multiple times in the first minute if the update fails.
  if (timeinfo.tm_hour == 2 && timeinfo.tm_min == 0 && (millis() - lastTimeUpdate) > 7200000)
  {

    getLocalTime(&timeinfo);
    Serial.println("Updating time at 2am");
    Serial.println(&timeinfo, "Old time: %A, %B %d %Y %H:%M:%S");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
    tzset();

    if (!getLocalTime(&timeinfo))
    {
      Serial.println("Failed to obtain time");
      dmaDisplay->setFont(&TomThumb);
      dmaDisplay->fillRect(0, 21, 64, 11, COLOR_BLACK);
      dmaDisplay->printAt(0, 28, COLOR_RED, "Time fetch error");
    }
    else
    { // Print the time
      Serial.println(&timeinfo, "New time: %A, %B %d %Y %H:%M:%S");
      dmaDisplay->setFont(&TomThumb);
      dmaDisplay->fillRect(0, 21, 64, 11, COLOR_BLACK);
      dmaDisplay->printAt(0, 28, COLOR_GREEN, "Time updated");
      lastTimeUpdate = millis();
    }
  }



} // Loop

void updateDisplay(HeaterState curState)
{
  static HeaterState lastState = HeaterState::STARTUP;

  

  if (curState==HeaterState::OFF && !shouldDisplayBeOn())
  {
    // Turn off the display and bail
    dmaDisplay->setBrightness8(0);
    // Set the lastState to something that will force a refresh when the display is turned back on in the a.m.
    lastState = HeaterState::UNKNOWN;
    return;
  }

  // Need to refresh when the display goes from shouldDisplayBeOn() == false to true.


  
  
  // If no change, bail to prevent flicker.
  if (curState == lastState)
  {
    return;
  }

  int bottomY = 20;
  dmaDisplay->setFont(&Impact12Caps);
  switch (curState)
  {
  case HeaterState::HOT:
    dmaDisplay->fillScreen(COLOR_BLACK);
    dmaDisplay->printCenter(32, bottomY, COLOR_RED, "HOT");
    dmaDisplay->setBrightness8(255);
    break;
  case HeaterState::WARM:
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
  case HeaterState::UNKNOWN:
    dmaDisplay->fillScreen(COLOR_BLACK);
    dmaDisplay->printCenter(32, bottomY, COLOR_ORANGE, "????");
    dmaDisplay->setBrightness8(255);
    break;
  }

  lastState = curState;

  return;
}

// Update the timer display based on the current duration in seconds.
void updateTimer(long durSeconds)
{
  // format seconds into hh:mm:ss
  long hours = durSeconds / 3600;
  long minutes = (durSeconds % 3600) / 60;
  long secs = durSeconds % 60;
  // Format time. Skip hours if it's 0.
  char durationStr[9];
  static char lastDurationStr[9] = "";

  if (hours > 0)
  {
    sprintf(durationStr, "%ld:%02ld:%02ld", hours, minutes, secs);
  }
  else
  {
    sprintf(durationStr, "%ld:%02ld", minutes, secs);
  }

  // Select color based on current trend.
  HeaterTrend heatTrend = heaterMonitor.getTrend();
  uint16_t trendColor = COLOR_WHITE;
  String timeText = "";
  static String lastTimeText = "";

  switch (heatTrend)
  {
  case HeaterTrend::HEATING:
    trendColor = COLOR_RED;
    timeText = "Heating for: ";
    break;
  case HeaterTrend::COOLING:
    trendColor = COLOR_LIGHTBLUE;
    timeText = "Cooling for: ";
    break;
  case HeaterTrend::MAINTAINING:
    trendColor = COLOR_GREEN;
    timeText = "Ready for: ";
    break;
  case HeaterTrend::IDLE:
    trendColor = COLOR_WHITE;
    timeText = "Idle for: ";
    break;
  case HeaterTrend::UNKNOWN:
    trendColor = COLOR_ORANGE;
    timeText = "Unknown for: ";
    break;
  }

  if (heaterMonitor.getState() == HeaterState::OFF)
  {
    static int lastMinute = -1;
    // Display time of day
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
      Serial.println("Failed to obtain time");
      return;
    }
    // Exit if the time hasn't updated to avoid flicker
    if (timeinfo.tm_min == lastMinute)
    {
      return;
    }

    lastMinute = timeinfo.tm_min;

    char timeOfDay[9];
    // format time of day h:mm am/pm
    strftime(timeOfDay, 9, "%l:%M %p", &timeinfo);
    dmaDisplay->setFont(&TomThumb); // Set font to small
    dmaDisplay->fillRect(0, 21, 64, 11, COLOR_BLACK);
    dmaDisplay->printRight(63, 31, COLOR_WHITE, timeOfDay);
  }
  else if (heaterMonitor.getState() == HeaterState::STARTUP)
  {
    return;
  }
  else
  {

    if (strcmp(durationStr, lastDurationStr) == 0)
    {
      return;
    }
    strcpy(lastDurationStr, durationStr);

    dmaDisplay->fillRect(0, 21, 64, 11, COLOR_BLACK);
    dmaDisplay->setFont(&TomThumb);

    dmaDisplay->printAt(0, 31, trendColor, "%s%s", timeText.c_str(), durationStr);
  }
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
  static double lastReading = -1.0;
  if (server.hasArg("value"))
  {
    String current = server.arg("value");
    if (current.toFloat() != lastReading)
    {
      Serial.println("Current reading via current: " + current);
      currentReading = current.toFloat();
      lastReading = currentReading;
    }
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

bool shouldDisplayBeOn()
{
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int currentHour = timeinfo.tm_hour;
  int currentDay = timeinfo.tm_wday; // 0 = Sunday, 1 = Monday, ..., 6 = Saturday

  // Define the schedule (example: display on from 6am to 10pm on weekdays, 8am to 11pm on weekends)
  std::map<int, std::vector<std::tuple<int, int>>> displaySchedule = {
      {1, {{8, 22}}}, // Monday
      {2, {{8, 22}}}, // Tuesday  
      {3, {{8, 22}}}, // Wednesday
      {4, {{8, 22}}}, // Thursday
      {5, {{8, 19}}}, // Friday
      {6, {{8, 18}}}, // Saturday
      {0, {{12, 18}}}  // Sunday
  };

  if (displaySchedule.find(currentDay) != displaySchedule.end())
  {
    for (const auto &period : displaySchedule.at(currentDay))
    {
      int startHour = std::get<0>(period);
      int endHour = std::get<1>(period);
      if (currentHour >= startHour && currentHour < endHour)
      {
        return true;
      }
    }
  }
  return false;
}