#include <Wire.h>
#include <WiFiClient.h>
#include <WiFi.h>
#include <WebServer.h>
#include <RtcDS3231.h>
#include <time.h>
#include <SPIFFS.h>
#define ARDUINOJSON_DEFAULT_NESTING_LIMIT 5
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <StreamUtils.h>
#include <Timezone.h>
#include <cred.h>  // remove & change WLAN_ID and WLAN_PASSWORD to your liking. only contains my ssid and password.

#define FORMAT_SPIFFS_IF_FAILED true
// NTP
WiFiUDP ntpUDP;

// DEV
// #define NUM_LEDS_BACK 30
// #define A_HORIZONTAL 10
// #define A_VERTICAL 5
// #define NUM_LEDS_SIDE 32
// #define B_HORIZONTAL 8
// #define B_VERTICAL 8

// LIVE
#define NUM_LEDS_BACK 176
#define A_HORIZONTAL 59
#define A_VERTICAL 29
#define NUM_LEDS_SIDE 116
#define B_HORIZONTAL 29
#define B_VERTICAL 29

#define DATA_PIN_BACK 14
// #define DATA_PIN_SIDE 33

#define HOUR 3600
#define MINUTE 60
#define CONFIGTIME 25
#define TVTIME 50

const char* WLAN_ID = ssid;           
const char* WLAN_PASSWORD = password;

RtcDS3231<TwoWire> Rtc(Wire);
//variables for Clock over Internet
// const long  gmtOffset_sec = 3600;
// const int   daylightOffset_sec = 3600;
NTPClient timeClient(ntpUDP, "de.pool.ntp.org");

int wifiCheck = 0;

AsyncWebServer server(80);

int state;

int curhour = 0;
int curminute = 0;
int cursecond = 0;
int lastSecond = 0;
int lastMillis;


NeoGamma<NeoGammaTableMethod> colorGamma; // for any fade animations, best to correct gamma
NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod> strip(NUM_LEDS_BACK+NUM_LEDS_SIDE, DATA_PIN_BACK);
// NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt1800KbpsMethod > stripBACK(NUM_LEDS_BACK, DATA_PIN_BACK);
// NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt2800KbpsMethod > stripSIDE(NUM_LEDS_SIDE, DATA_PIN_SIDE);
// NeoPixel animation time management object
// NeoPixelAnimator animations(PixelCount, NEO_CENTISECONDS);
NeoPixelAnimator animations(NUM_LEDS_BACK+NUM_LEDS_SIDE, NEO_DECISECONDS);
// NeoPixelAnimator animationsBACK(NUM_LEDS_BACK, NEO_DECISECONDS);
// NeoPixelAnimator animationsSIDE(NUM_LEDS_SIDE, NEO_DECISECONDS);
// Possible values from 1 to 32768, and there some helpful constants defined as...
// NEO_MILLISECONDS        1    // ~65 seconds max duration, ms updates
// NEO_CENTISECONDS       10    // ~10.9 minutes max duration, centisecond updates
// NEO_DECISECONDS       100    // ~1.8 hours max duration, decisecond updates
// NEO_SECONDS          1000    // ~18.2 hours max duration, second updates
// NEO_DECASECONDS     10000    // ~7.5 days, 10 second updates
//

#define colorSaturation 128

RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

// Define the colors for each corner
RgbColor topLeftColor(255, 0, 0); // Red
RgbColor topRightColor(0, 255, 0); // Green
RgbColor bottomLeftColor(0, 0, 255); // Blue
RgbColor bottomRightColor(255, 255, 0); // Yellow

struct MyColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

MyColor myColor;

// 
// CONFIG
// 
struct Config {
  bool power;
  bool autoMode;
  uint8_t upTimeHour;
  uint8_t upTimeMinute;
  uint8_t downTimeHour;
  uint8_t downTimeMinute;
  uint32_t upTime;
  uint32_t downTime;
  uint32_t fadeTime;
  uint8_t setBrightness;
  uint8_t maxBrightness;
  int state;
  RgbColor colorTopLeft;
  RgbColor colorTopRight;
  RgbColor colorBottomLeft;
  RgbColor colorBottomRight;
  bool tvMode;
};

const char *filename = "/config.txt";  // <- SD library uses 8.3 filenames
Config config;                         // <- global configuration object


String rgbToJSON(RgbColor color) {
  JsonDocument jsonColor;
  // jsonColor["r"] = String(color.R);
  // jsonColor["g"] = String(color.G);
  // jsonColor["b"] = String(color.B);
  JsonArray data = jsonColor["data"].to<JsonArray>();
  data.add(color.R);
  data.add(color.G);
  data.add(color.B);
  String json;
  serializeJson(jsonColor, json);
  return json;
}

String rgbToString(RgbColor color) {
    char buffer[20];
    sprintf(buffer, "{r:%03d,g:%03d,b:%03d}", color.R, color.G, color.B);
    return String(buffer);
}

JsonDocument jsonConfig() {
  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  JsonDocument doc;

  // Set the values in the document
  doc["power"] = config.power;
  doc["autoMode"] = config.autoMode;
  doc["upTime"] = config.upTime;
  doc["upTimeHour"] = config.upTimeHour;
  doc["upTimeMinute"] = config.upTimeMinute;
  doc["downTime"] = config.downTime;
  doc["downTimeHour"] = config.downTimeHour;
  doc["downTimeMinute"] = config.downTimeMinute;
  doc["fadeTime"] = config.fadeTime;
  doc["maxBrightness"] = config.maxBrightness;
  doc["setBrightness"] = config.setBrightness;
  JsonArray jColorBottomLeft = doc["colorBottomLeft"].to<JsonArray>();
  jColorBottomLeft.add(config.colorBottomLeft.R);
  jColorBottomLeft.add(config.colorBottomLeft.G);
  jColorBottomLeft.add(config.colorBottomLeft.B);
  JsonArray jColorBottomRight = doc["colorBottomRight"].to<JsonArray>();
  jColorBottomRight.add(config.colorBottomRight.R);
  jColorBottomRight.add(config.colorBottomRight.G);
  jColorBottomRight.add(config.colorBottomRight.B);
  JsonArray jColorTopLeft = doc["colorTopLeft"].to<JsonArray>();
  jColorTopLeft.add(config.colorTopLeft.R);
  jColorTopLeft.add(config.colorTopLeft.G);
  jColorTopLeft.add(config.colorTopLeft.B);
  JsonArray jColorTopRight = doc["colorTopRight"].to<JsonArray>();
  jColorTopRight.add(config.colorTopRight.R);
  jColorTopRight.add(config.colorTopRight.G);
  jColorTopRight.add(config.colorTopRight.B);
  doc["tvMode"] = config.tvMode;
  
  return doc;
}

// Loads the configuration from a file
void loadConfiguration(const char *filename, Config &config) {
  // Open file for reading
  File file = SPIFFS.open(filename);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  JsonDocument doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  // prep colors
  RgbColor tempA = RgbColor(doc["colorBottomLeft"][0],doc["colorBottomLeft"][1],doc["colorBottomLeft"][2]);
  RgbColor tempB = RgbColor(doc["colorBottomRight"][0],doc["colorBottomRight"][1],doc["colorBottomRight"][2]);
  RgbColor tempC = RgbColor(doc["colorTopLeft"][0],doc["colorTopLeft"][1],doc["colorTopLeft"][2]);
  RgbColor tempD = RgbColor(doc["colorTopRight"][0],doc["colorTopRight"][1],doc["colorTopRight"][2]);

  // Copy values from the JsonDocument to the Config
  config.power = doc["power"] | false;
  config.autoMode = doc["autoMode"] | true;
  config.upTime = doc["upTime"] | 0;
  config.upTimeHour = doc["upTimeHour"] | 0;
  config.upTimeMinute = doc["upTimeMinute"] | 0;
  config.downTime = doc["downTime"] | 0;
  config.downTimeHour = doc["downTimeHour"] | 0;
  config.downTimeMinute = doc["downTimeMinute"] | 0;
  config.fadeTime = doc["fadeTime"] | 100;
  config.maxBrightness = doc["maxBrightness"] | 20;
  config.setBrightness = doc["setBrightness"] | 0;
  config.colorBottomLeft = tempA;
  config.colorBottomRight = tempB;
  config.colorTopLeft = tempC;
  config.colorTopRight = tempD;
  config.tvMode = doc["tvMode"] | false;

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
  
}

// Saves the configuration to a file
void saveConfiguration(const char *filename, const Config &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  SPIFFS.remove(filename);

  // Open file for writing
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  String cofigJson;
  JsonDocument doc = jsonConfig();

  WriteLoggingStream loggedFile(file, Serial);
  serializeJson(doc, loggedFile);
  file.close();
  // serializeJson(doc, file);


  // Close the file
  file.close();
}

void printConfig() {
  Serial.println("CONFIG DUMP:");
  Serial.print("config.power: "); Serial.println(config.power);
  Serial.print("config.autoMode: "); Serial.println(config.autoMode);
  Serial.print("config.tvMode: "); Serial.println(config.tvMode);
  Serial.print("config.upTime: "); Serial.println(config.upTime);
  Serial.print("config.upTimeHour: "); Serial.println(config.upTimeHour);
  Serial.print("config.upTimeMinute: "); Serial.println(config.upTimeMinute);
  Serial.print("config.downTime: "); Serial.println(config.downTime);
  Serial.print("config.downTimeHour: "); Serial.println(config.downTimeHour);
  Serial.print("config.downTimeMinute: "); Serial.println(config.downTimeMinute);
  Serial.print("config.fadeTime: "); Serial.println(config.fadeTime);
  Serial.print("config.maxBrightness: "); Serial.println(config.maxBrightness);
  Serial.print("config.setBrightness: "); Serial.println(config.setBrightness);
  Serial.print("config.colorBottomLeft: "); Serial.println(rgbToString(config.colorBottomLeft));
  Serial.print("config.colorBottomRight: "); Serial.println(rgbToString(config.colorBottomRight));
  Serial.print("config.colorTopLeft: "); Serial.println(rgbToString(config.colorTopLeft));
  Serial.print("config.colorTopRight: "); Serial.println(rgbToString(config.colorTopRight));
  Serial.println("END OF CONFIG");

}


// ************************************
// ** RTC Setup
// ************************************

// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
Timezone CE(CEST, CET);

void RTC_Update(){
  // Do udp NTP lookup, epoch time is unix time - subtract the 30 extra yrs (946684800UL) library expects 2000
  timeClient.update();
  unsigned long tcTime = timeClient.getEpochTime()-946684800UL;
  unsigned long localTime = CE.toLocal(tcTime);
  RtcDateTime localEpochTime = RtcDateTime(localTime);
  Rtc.SetDateTime(localEpochTime); 
}


// Utility print function
#define countof(a) (sizeof(a) / sizeof(a[0]))
void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];
    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u.%02u.%04u %02u:%02u:%02u"),
            dt.Day(),
            dt.Month(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}

// 
// WIFI
// 
String get_wifi_status(int status){
  switch(status){
    case WL_IDLE_STATUS:
      return "WL_IDLE_STATUS";
    case WL_SCAN_COMPLETED:
      return "WL_SCAN_COMPLETED";
    case WL_NO_SSID_AVAIL:
      return "WL_NO_SSID_AVAIL";
    case WL_CONNECT_FAILED:
      return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "WL_CONNECTION_LOST";
    case WL_CONNECTED:
      return "WL_CONNECTED";
    case WL_DISCONNECTED:
      return "WL_DISCONNECTED";
  }
  return "NULL";
}

String exportAsJson(){
  String json;
  serializeJson(jsonConfig(), json);
  return json;
}

// Animation color to color
void animFadeToColor(uint16_t time, 
      RgbColor endTopLeftColor, 
      RgbColor endTopRightColor, 
      RgbColor endBottomLeftColor, 
      RgbColor endBottomRightColor 
  ) {
    state = 1;
    uint8_t brightness = config.setBrightness;

    AnimUpdateCallback animUpdate = [=](const AnimationParam& param)
    {
        AnimEaseFunction easing = NeoEase::CubicInOut;

        float fadeAmount = easing(param.progress);

        // Calculate current colors for each corner based on transition progress
        RgbColor currentTopLeftColor = RgbColor::LinearBlend(topLeftColor, endTopLeftColor.Dim(brightness), fadeAmount);
        RgbColor currentTopRightColor = RgbColor::LinearBlend(topRightColor, endTopRightColor.Dim(brightness), fadeAmount);
        RgbColor currentBottomRightColor = RgbColor::LinearBlend(bottomRightColor, endBottomRightColor.Dim(brightness), fadeAmount);
        RgbColor currentBottomLeftColor = RgbColor::LinearBlend(bottomLeftColor, endBottomLeftColor.Dim(brightness), fadeAmount);
        
        // BACK
          // Bottom side gradient
          for (int i = 0; i < A_HORIZONTAL; i++) {
              strip.SetPixelColor(i, colorGamma.Correct(RgbColor::LinearBlend(currentBottomLeftColor, currentBottomRightColor, float(i) / (A_HORIZONTAL - 1))));
          }
          // Right side gradient
          for (int i = 0; i < A_VERTICAL; i++) {
              strip.SetPixelColor(A_HORIZONTAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentBottomRightColor, currentTopRightColor, float(i) / (A_VERTICAL - 1))));
          }
          // Top side gradient
          for (int i = 0; i < A_HORIZONTAL; i++) {
              strip.SetPixelColor(A_HORIZONTAL + A_VERTICAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentTopRightColor, currentTopLeftColor, float(i) / (A_HORIZONTAL - 1))));
          }
          // Left side gradient
          for (int i = 0; i < A_VERTICAL; i++) {
              strip.SetPixelColor(A_HORIZONTAL + A_VERTICAL + A_HORIZONTAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentTopLeftColor, currentBottomLeftColor, float(i) / (A_VERTICAL - 1))));
          }    


        // THIS IS MIRRORED FROM CONFIG: LEFT = RIGHT & RIGHT = LEFT!
        // Calculate current colors for each corner based on transition progress
        currentTopLeftColor = RgbColor::LinearBlend(topRightColor, endTopRightColor.Dim(brightness), fadeAmount);
        currentTopRightColor = RgbColor::LinearBlend(topLeftColor, endTopLeftColor.Dim(brightness), fadeAmount);
        currentBottomRightColor = RgbColor::LinearBlend(bottomLeftColor, endBottomLeftColor.Dim(brightness), fadeAmount);
        currentBottomLeftColor = RgbColor::LinearBlend(bottomRightColor, endBottomRightColor.Dim(brightness), fadeAmount);
        
        // SIDE
          // Bottom side gradient
          for (int i = 0; i < B_HORIZONTAL; i++) {
              strip.SetPixelColor(NUM_LEDS_BACK + i, colorGamma.Correct(RgbColor::LinearBlend(currentBottomLeftColor, currentBottomRightColor, float(i) / (B_HORIZONTAL - 1))));
          }
          // Right side gradient
          for (int i = 0; i < B_VERTICAL; i++) {
              strip.SetPixelColor(NUM_LEDS_BACK + B_HORIZONTAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentBottomRightColor, currentTopRightColor, float(i) / (B_VERTICAL - 1))));
          }
          // Top side gradient
          for (int i = 0; i < B_HORIZONTAL; i++) {
              strip.SetPixelColor(NUM_LEDS_BACK + B_HORIZONTAL + B_VERTICAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentTopRightColor, currentTopLeftColor, float(i) / (B_HORIZONTAL - 1))));
          }
          // Left side gradient
          for (int i = 0; i < B_VERTICAL; i++) {
              strip.SetPixelColor(NUM_LEDS_BACK + B_HORIZONTAL + B_VERTICAL + B_HORIZONTAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentTopLeftColor, currentBottomLeftColor, float(i) / (B_VERTICAL - 1))));
          }            
        if (param.state == AnimationState_Completed) {
            state = 2;
            topLeftColor = currentTopLeftColor;
            topRightColor = currentTopRightColor;
            bottomLeftColor = currentBottomLeftColor;
            bottomRightColor = currentBottomRightColor;
        }
    };

    animations.StartAnimation(0, time, animUpdate);
}

// Animation callback function
void animFadeOn(uint16_t time) {
    state = 1;
    Serial.println("Fade on...");
    uint8_t brightness = config.setBrightness;

    AnimUpdateCallback animUpdate = [=](const AnimationParam& param)
    {
        AnimEaseFunction easing = NeoEase::CubicInOut;
        float fadeAmount = easing(param.progress);

        // Calculate current colors for each corner based on transition progress
        RgbColor currentTopLeftColor = RgbColor::LinearBlend(black, config.colorTopLeft.Dim(brightness), fadeAmount);
        RgbColor currentTopRightColor = RgbColor::LinearBlend(black, config.colorTopRight.Dim(brightness), fadeAmount);
        RgbColor currentBottomRightColor = RgbColor::LinearBlend(black, config.colorBottomRight.Dim(brightness), fadeAmount);
        RgbColor currentBottomLeftColor = RgbColor::LinearBlend(black, config.colorBottomLeft.Dim(brightness), fadeAmount);
        
        // BACK
          // Bottom side gradient
          for (int i = 0; i < A_HORIZONTAL; i++) {
              strip.SetPixelColor(i, colorGamma.Correct(RgbColor::LinearBlend(currentBottomLeftColor, currentBottomRightColor, float(i) / (A_HORIZONTAL - 1))));
          }
          // Right side gradient
          for (int i = 0; i < A_VERTICAL; i++) {
              strip.SetPixelColor(A_HORIZONTAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentBottomRightColor, currentTopRightColor, float(i) / (A_VERTICAL - 1))));
          }
          // Top side gradient
          for (int i = 0; i < A_HORIZONTAL; i++) {
              strip.SetPixelColor(A_HORIZONTAL + A_VERTICAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentTopRightColor, currentTopLeftColor, float(i) / (A_HORIZONTAL - 1))));
          }
          // Left side gradient
          for (int i = 0; i < A_VERTICAL; i++) {
              strip.SetPixelColor(A_HORIZONTAL + A_VERTICAL + A_HORIZONTAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentTopLeftColor, currentBottomLeftColor, float(i) / (A_VERTICAL - 1))));
          }

        // SIDE IS MIRRORED FROM CONFIG: LEFT = RIGHT & RIGHT = LEFT!
        // Calculate current colors for each corner based on transition progress
        currentTopLeftColor = RgbColor::LinearBlend(black, config.colorTopRight.Dim(brightness), fadeAmount);
        currentTopRightColor = RgbColor::LinearBlend(black, config.colorTopLeft.Dim(brightness), fadeAmount);
        currentBottomRightColor = RgbColor::LinearBlend(black, config.colorBottomLeft.Dim(brightness), fadeAmount);
        currentBottomLeftColor = RgbColor::LinearBlend(black, config.colorBottomRight.Dim(brightness), fadeAmount);
        
        // SIDE
          // Bottom side gradient
          for (int i = 0; i < B_HORIZONTAL; i++) {
              strip.SetPixelColor(NUM_LEDS_BACK + i, colorGamma.Correct(RgbColor::LinearBlend(currentBottomLeftColor, currentBottomRightColor, float(i) / (B_HORIZONTAL - 1))));
          }
          // Right side gradient
          for (int i = 0; i < B_VERTICAL; i++) {
              strip.SetPixelColor(NUM_LEDS_BACK + B_HORIZONTAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentBottomRightColor, currentTopRightColor, float(i) / (B_VERTICAL - 1))));
          }
          // Top side gradient
          for (int i = 0; i < B_HORIZONTAL; i++) {
              strip.SetPixelColor(NUM_LEDS_BACK + B_HORIZONTAL + B_VERTICAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentTopRightColor, currentTopLeftColor, float(i) / (B_HORIZONTAL - 1))));
          }
          // Left side gradient
          for (int i = 0; i < B_VERTICAL; i++) {
              strip.SetPixelColor(NUM_LEDS_BACK + B_HORIZONTAL + B_VERTICAL + B_HORIZONTAL + i, colorGamma.Correct(RgbColor::LinearBlend(currentTopLeftColor, currentBottomLeftColor, float(i) / (B_VERTICAL - 1))));
          }   

        if (param.state == AnimationState_Completed) {
            state = 2;
            topLeftColor = config.colorTopLeft;
            topRightColor = config.colorTopRight;
            bottomLeftColor = config.colorBottomLeft;
            bottomRightColor= config.colorBottomRight;

        }
    };

    animations.StartAnimation(0, time, animUpdate);
}

// Animation Fade off
void animFadeOff(uint16_t time) {
    state = 1;
    Serial.println("Fade off...");

    AnimEaseFunction easing;
    easing = NeoEase::CubicIn;

    // BACK
    for (uint16_t pixel = 0; pixel < NUM_LEDS_BACK + NUM_LEDS_SIDE; pixel++)
    {
        float p = (static_cast<float>(pixel)/static_cast<float>(NUM_LEDS_BACK+NUM_LEDS_SIDE));
        RgbColor currentColor = strip.GetPixelColor<RgbColor>(pixel);

        AnimUpdateCallback animUpdate = [=](const AnimationParam& param)
        {
            float progress = easing(param.progress);

            RgbColor updatedColor = RgbColor::LinearBlend(currentColor, RgbColor(0), progress);
            strip.SetPixelColor(pixel, updatedColor);

            if (param.state == AnimationState_Completed) {
                state = 3;
                topLeftColor = black;
                topRightColor = black;
                bottomLeftColor = black;
                bottomRightColor = black;
            }
        };

        animations.StartAnimation(pixel, time, animUpdate);
    }

}



void timeCheck(int time){
  if (config.power) {
    if (config.autoMode) {
      if (state != 1) { 
        if(time == config.upTime && state != 2) {
          Serial.println("upTime triggered. Fade up...");
          animFadeOn(config.fadeTime);
        }
        if(time == config.downTime && state != 3){
          Serial.println("downTime triggered. Fade down...");
          animFadeOff(config.fadeTime);
        }
        if((time > config.upTime && time < config.downTime) && state != 2){
          Serial.println("Should be on and isn't. Fade up...");
          animFadeOn(CONFIGTIME);
        }
        if((time < config.upTime || time > config.downTime) && state != 3){
          Serial.println("Should be off and isn't. Fade down...");
          animFadeOff(CONFIGTIME);
        }
      }
    }
  }
}

void setup() {
  
  Serial.begin(115200);
  Serial.println("starting up...");
  state = 0;

  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);
  delay(100);

  strip.Begin();
  for (uint16_t pixel = 0; pixel < NUM_LEDS_BACK; pixel++) {
    strip.SetPixelColor(pixel, black);
  }
  strip.Show();
  Serial.println("NPB initialized.");
  strip.SetPixelColor(A_HORIZONTAL+A_VERTICAL+A_HORIZONTAL, green);
  strip.Show();
  delay(100);

  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    strip.SetPixelColor(A_HORIZONTAL+A_VERTICAL+A_HORIZONTAL-1, red);
    strip.Show();
    return;
  } else {
    Serial.println("SPIFFS initialized.");
    strip.SetPixelColor(A_HORIZONTAL+A_VERTICAL+A_HORIZONTAL-1, green);
    strip.Show();
  }
  delay(100);

  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename, config);
  delay(100);

  // Dump config file
  printConfig();

  int status = WL_IDLE_STATUS;
  Serial.println("\nConnecting");
  Serial.println(get_wifi_status(status));
  WiFi.begin(WLAN_ID, WLAN_PASSWORD);
  uint16_t i = A_HORIZONTAL+A_VERTICAL+A_HORIZONTAL-2;
  while(status != WL_CONNECTED){
      strip.SetPixelColor(i, blue);
      strip.Show();
      i -= 1;
      delay(500);
      status = WiFi.status();
      Serial.println(get_wifi_status(status));
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
  delay(100);

// *************************************************************************************************
  // API
  // *************************************************************************************************

  Serial.print("Preparing API...");
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Link wurde nicht gefunden!");
  });

  server.on("/reboot", [](AsyncWebServerRequest *request) {
    ESP.restart();
    request->send(200, "text/html", "OK!");
  });

  server.on("/getInformation", [](AsyncWebServerRequest *request) {
    String json = "\"ledStrips\":[" + exportAsJson() + "]";
    Serial.println(json);
    request->send(200, "text/html", json);
  });

  server.on("/configPage", [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/configPage.html", "text/html");
  });

  server.on("/tvModeOff", [](AsyncWebServerRequest *request) {
    // config.power = false;
    config.tvMode = false;
    config.autoMode = true;
    Serial.println("TV Mode off...");
    // animFadeOff(TVTIME);
    request->send(200, "text/html", "TV Mode off...");
  });

  server.on("/tvModeOn", [](AsyncWebServerRequest *request) {
    // config.power = false;
    config.tvMode = true;
    config.autoMode = false;
    Serial.println("TV Mode on...");
    animFadeOff(TVTIME);
    request->send(200, "text/html", "TV Mode on...");
  });

  server.on("/setClock", [](AsyncWebServerRequest *request) {
    RTC_Update();
    RtcDateTime now = Rtc.GetDateTime();
    printDateTime(now);
    request->send(200, "text/html", "String(printDateTime(now))");
  });

  server.on("/on", [](AsyncWebServerRequest *request) {
    config.power = true;
    Serial.println("Power on");
    animFadeOn(CONFIGTIME);
    saveConfiguration(filename, config);
    request->send(200, "text/html", "ON");
  });

  server.on("/off", [](AsyncWebServerRequest *request) {
    config.power = false;
    Serial.println("Power off");
    animFadeOff(CONFIGTIME);
    saveConfiguration(filename, config);
    request->send(200, "text/html", "OFF");
  });

  server.on("/activateAutoMode", [](AsyncWebServerRequest *request) {
    config.autoMode = !config.autoMode;
    saveConfiguration(filename, config);

    if (config.autoMode) {
      Serial.println("AutoMode on");
      request->send(200, "text/html", "AutoMode on");
    } else {
      Serial.println("AutoMode off");
      request->send(200, "text/html", "AutoMode off");
    }
  });

  server.on("/saveConfig", [](AsyncWebServerRequest *request) {
    AsyncWebParameter* p1 = request->getParam(0);
    AsyncWebParameter* p2 = request->getParam(1);
    AsyncWebParameter* p3 = request->getParam(2);
    AsyncWebParameter* p4 = request->getParam(3);
    config.upTimeHour = p1->value().toInt();
    config.upTimeMinute = p2->value().toInt();
    config.downTimeHour = p3->value().toInt();
    config.downTimeMinute = p4->value().toInt();
    config.upTime = config.upTimeHour * HOUR + config.upTimeMinute * MINUTE;
    config.downTime = config.downTimeHour * HOUR + config.downTimeMinute * MINUTE;

    AsyncWebParameter* p5 = request->getParam(4);
    config.fadeTime = p5->value().toInt();

    AsyncWebParameter* p6 = request->getParam(5);
    AsyncWebParameter* p7 = request->getParam(6);
    AsyncWebParameter* p8 = request->getParam(7);
    AsyncWebParameter* p9 = request->getParam(8);
    RgbColor T_A(static_cast<uint8_t>(p6->value().substring(1,4).toInt()), static_cast<uint8_t>(p6->value().substring(5,8).toInt()), static_cast<uint8_t>(p6->value().substring(9,12).toInt()));
    RgbColor T_B(static_cast<uint8_t>(p7->value().substring(1,4).toInt()), static_cast<uint8_t>(p7->value().substring(5,8).toInt()), static_cast<uint8_t>(p7->value().substring(9,12).toInt()));
    RgbColor B_A(static_cast<uint8_t>(p8->value().substring(1,4).toInt()), static_cast<uint8_t>(p8->value().substring(5,8).toInt()), static_cast<uint8_t>(p8->value().substring(9,12).toInt()));
    RgbColor B_B(static_cast<uint8_t>(p9->value().substring(1,4).toInt()), static_cast<uint8_t>(p9->value().substring(5,8).toInt()), static_cast<uint8_t>(p9->value().substring(9,12).toInt()));
    config.colorTopLeft = T_A;
    config.colorTopRight = T_B;
    config.colorBottomLeft = B_A;
    config.colorBottomRight = B_B;

    AsyncWebParameter* p10 = request->getParam(9);
    AsyncWebParameter* p11 = request->getParam(10);
    config.maxBrightness = p10->value().toInt();
    config.setBrightness = p11->value().toInt();
    if (config.maxBrightness > 255) config.maxBrightness = 255;
    if (config.setBrightness > 255) config.setBrightness = 255;
    if (config.setBrightness > config.maxBrightness) config.setBrightness = config.maxBrightness;

    saveConfiguration(filename, config);

    animFadeToColor(CONFIGTIME, T_A, T_B, B_A, B_B);

    request->send(200, "text/html", "Config wurde gesetzt!");

  });

  // *************************************************************************************************
  // *************************************************************************************************

  // Start NTP Time Client
  timeClient.begin();
  delay(2000);
  // timeClient.update();
  Rtc.Begin();
  RTC_Update();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Serial.print("compile time: "); printDateTime(compiled);
  Serial.println();
  Serial.print("ntp time: "); 
  Serial.print(timeClient.getFormattedTime());
  Serial.println();

  server.begin();

  animFadeOff(CONFIGTIME);

  Serial.println(" done.");
  delay(100);
}


void loop() {
  wifiCheck++;
  if (WiFi.status() != WL_CONNECTED && wifiCheck > 80000) {
      ESP.restart();
      wifiCheck = 0;
  };

  RtcDateTime now = Rtc.GetDateTime();
  curhour = now.Hour();
  curminute = now.Minute();
  cursecond = now.Second();
  if (curhour == 3) RTC_Update();
  int jetzt = curhour * HOUR + curminute * MINUTE;


  timeCheck(jetzt);
  if (cursecond != lastSecond) {
    printDateTime(now);
    Serial.print(" / ");
    RtcTemperature temp = Rtc.GetTemperature();
    temp.Print(Serial);
    Serial.print("C");
    Serial.print(" / ");
    Serial.print("state: "); Serial.print(state);
    Serial.println();

  }

  
  if (animations.IsAnimating()) {
    // the normal loop just needs these two to run the active animations
    animations.UpdateAnimations();
    strip.Show();

  }


  lastSecond = cursecond;
}

