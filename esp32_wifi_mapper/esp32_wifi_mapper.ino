#include <TinyGPSPlus.h>          // https://github.com/mikalhart/TinyGPSPlus
#include <Arduino_GFX_Library.h>  // https://github.com/moononournation/Arduino_GFX
#include "WiFi.h"                 // default ESP32 wifi control
#include "colors.h"               // helper file to make defining colors easier
#include <ezButton.h>             // https://arduinogetstarted.com/tutorials/arduino-button-library
#include <LinkedList.h>           // https://github.com/ivanseidel/LinkedList

// Uncomment to get more information on Serial about calls and such
//#define DEBUG

// Comment this line out to remove battery measuring and display, can still run on battery though
#define BATTERY

// CONFIGURABLE ITEMS
const int utcOffset = -6;   // hours away from UTC
const int readDelay = 3000;   // ms before scrolling to next page of entries when displaying networks on device

// These pins are for the Adafruit HUZZAH32, some are required, some are just chosen: https://learn.adafruit.com/assets/111179
#define DC  27  // Data/Command, just picked from digital pins
#define CS  15  // Chip Select, possibly required - HSPICS0
#define SCK 14  // Serial Clock, Required - HSPICLK
#define SDO 12  // Serial Data Out*, Required - HSPIQ
#define RST 33  // Reset, just chosen from digital pins
// *Serial Data Out: old name is MOSI: https://www.sparkfun.com/spi_signal_names

static const uint32_t GPSBaud = 9600;   // using a cheap GT-U7 module

// set up the SPI bus and display we'll be using. Here a cheap Color OLED with the SSD1331 library
Arduino_DataBus *bus = new Arduino_ESP32SPI(DC, CS, SCK, SDO, GFX_NOT_DEFINED, HSPI);   // SDI isn't used, select the Hardware SPI bus
Arduino_GFX *display = new Arduino_SSD1331(bus, RST, 0);  // no display rotation for this project

// The TinyGPSPlus object
TinyGPSPlus gps;

// Macros
#define show endWrite           // show is used in many Adafruit libraries
#define clear() fillScreen(0)   // clear() is so much easier
#define DISP_WIDTH  96
#define DISP_HEIGHT 64


// stores time and location
struct Location {
  int id = 0;   // used to link location to network results
  char time[33] = "";
  float lat = 0;
  float lng = 0;
};

// stores ssid and rssi
struct Networks {
  int id = 0;
  String ssid = "";
  int rssi = 0;
};

// linked lists to store all that juicy data
LinkedList<Location> locationList;
LinkedList<Networks> networkList;

//  globals to pass data around
char buf[64] = "";
char buf2[32] = "";
int netSeen = 0;

// Battery voltage info, specific to the Adafruit cell in use
#ifdef BATTERY
  const int fullBat = 4900;
  const int flatBat = 3720;
  int bat85, bat20; // make these global, we'll calculate them in setup
#endif

// timing goodies
unsigned long now = millis();
unsigned long then = now;
const int drawRate = 1000;  // ms between screen redraws

const int debounce = 50;
// pins hooked to each button, referenced by GPIO numbers
ezButton wifiScan(32);
ezButton dataDump(26);
ezButton exButton(25);

// Function prototypes (not strictly needed with Arduino IDE but hey, why not?)
static void smartDelay(unsigned long ms);
static void printFloat(float val, bool valid, int len, int prec);
static void printInt(unsigned long val, bool valid, int len);
static void printDateTime(TinyGPSDate &d, TinyGPSTime &t);
static void printStr(const char *str, int len);
void convertTime(TinyGPSDate d, TinyGPSTime t);
void scanWifi();
void storeLists(float lat, float lng);
void printLists();
void displayLists();
void rssiColorHelper(int rssi);
void getVoltage(int y);
uint16_t color565(int r, int g, int b);


void setup()
{
  // set up the display, show some booting text
  display->begin();
  display->setTextWrap(0);  // don't wrap text, useful when listing long SSIDs
  display->clear();
  display->setCursor(0, 0);
  display->setTextColor(CYAN_MEDIUM);
  display->println("Booting...");
  
  Serial.begin(115200);
  Serial1.begin(GPSBaud); // GPS is on S1
  display->println("Serial up!");
  
  // Set WiFi to station mode, disconnect from anything it might know
  WiFi.disconnect();
  display->println("Wifi started!");

  // button debounces, any more and it'd probably be better to use a loop
  wifiScan.setDebounceTime(debounce);
  dataDump.setDebounceTime(debounce);
  exButton.setDebounceTime(debounce);

  #ifdef DEBUG
    Serial.println(F("GPS / WiFi mapping utility"));
    Serial.println(F("Links WiFi signal strength to GPS locations for mapping station locations"));
    Serial.println(F("Script cobbled together by https://github.com/comfy-couch"));
  #endif

  #ifdef BATTERY
    // battery voltage calculations for 85% and 20%
    float batTemp = (fullBat - flatBat) * .85;
    bat85 = int(batTemp + flatBat);
    batTemp = (fullBat - flatBat) * .2;
    bat20 = int(batTemp + flatBat);
  #endif

  smartDelay(1000);
}

void loop()
{
  // ezButton requires these calls to monitor button presses
  wifiScan.loop();
  dataDump.loop();
  exButton.loop();


  // handle the various buttons
  if(wifiScan.isReleased()){ // see if we want to scan for wifi
    #ifdef DEBUG
      Serial.println(F("Storing data to lists"));
    #endif
    display->setTextColor(CYAN_HIGH);
    display->setCursor(28, 26);
    display->print("[SCAN]");
    convertTime(gps.date, gps.time); // convert to local timezone
    storeLists(gps.location.lat(), gps.location.lng());
  }

  if(dataDump.isReleased()){
    #ifdef DEBUG
      Serial.println("Attempting to dump data to serial");
    #endif
    display->setTextColor(GREEN_HIGH);
    display->setCursor(28,26);
    display->print("[DUMP]");
    printLists();
  }

  if(exButton.isReleased()){
    #ifdef DEBUG
      Serial.println("Showing last scan on display");
    #endif
    display->setTextColor(ORANGE_HIGH);
    display->setCursor(28,26);
    display->print("[DISP]");
    displayLists();
  }
  
  int sats = gps.satellites.value();  // get satellite count into a var since we'll use it a few times
  
  #ifdef DEBUG    // prints lots of GPS data directly to the serial; code from TinyGPSPlus examples
    printInt(sats, gps.satellites.isValid(), 5);
    printFloat(gps.hdop.hdop(), gps.hdop.isValid(), 6, 1);
    printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
    printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);
    printInt(gps.location.age(), gps.location.isValid(), 5);
    printDateTime(gps.date, gps.time);
    printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 2);
    printInt(gps.charsProcessed(), true, 6);
    printInt(gps.sentencesWithFix(), true, 10);
    printInt(gps.failedChecksum(), true, 9);
    Serial.println();
  #endif

  if (millis() - then > drawRate){    // is it time to redraw the screen?
    display->clear();
    
    switch (sats){    // sets text color based on number of satellites locked
      case 0:
      case 1:
        display->setTextColor(RED_MEDIUM);
        break;
      case 2:
      case 3:
        display->setTextColor(ORANGE_MEDIUM);
        break;
      case 4:
      case 5:
        display->setTextColor(GREEN_MEDIUM);
        break;
      default:
        display->setTextColor(GREEN_HIGH);
        break;
    }
    
    // start with GPS information
    display->setCursor(0,0);
    snprintf(buf, 32, "Sat: %1d", gps.satellites.value());
    display->println(buf);
    snprintf(buf, 32, "Lat: %4f", gps.location.lat());
    display->println(buf);
    snprintf(buf, 32, "Lon: %4f", gps.location.lng());
    display->println(buf);
  
    convertTime(gps.date, gps.time);  // convert UTC time to local time
  
    // print the time and date
    display->setTextColor(WHITE_LOW);
    display->println("");
    display->println("");
    display->print(buf); display->print("   ");
    display->println(buf2);

    // show how many scans we've got and how many networks were in the last one
    display->setTextColor(PURPLE_HIGH);
    snprintf(buf, 32, " %02d scans saved", locationList.size());
    display->println(buf);
  
    display->setTextColor(CYAN_MEDIUM);
    snprintf(buf, 32, "  Networks: %02d", netSeen);
    display->println(buf);

    then = millis();  // update for next loop

    #ifdef BATTERY
      getVoltage(37); // do a voltage check too; show bar at y=37
    #endif
  }
  
  smartDelay(50); // smartDelay delays while still taking in GPS data

  // if we've been going for more than 5 seconds and haven't gotten data from the GPS, say something
  if (millis() > 5000 && gps.charsProcessed() < 10){
    Serial.println(F("No GPS data received: check wiring"));
    display->setCursor(32,0);
    display->setTextColor(RED_HIGH);
    display->print("NO GPS?");
  }
}

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    while (Serial1.available())
      gps.encode(Serial1.read());
  } while (millis() - start < ms);
}

static void printFloat(float val, bool valid, int len, int prec)
{
  if (!valid)
  {
    while (len-- > 1)
      Serial.print('*');
    Serial.print(' ');
  }
  else
  {
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      Serial.print(' ');
  }
  smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len)
{
  char sz[32] = "*****************";
  if (valid)
    snprintf(sz, 32, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  smartDelay(0);
}

static void printDateTime(TinyGPSDate &d, TinyGPSTime &t)
{
  if (!d.isValid())
  {
    Serial.print(F("********** "));
  }
  else
  {
    char sz[32];
    snprintf(sz, 32, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    Serial.print(sz);
  }
  
  if (!t.isValid())
  {
    Serial.print(F("******** "));
  }
  else
  {
    char sz[32];
    snprintf(sz, 32, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
    Serial.print(sz);
  }

  printInt(d.age(), d.isValid(), 5);
  smartDelay(0);
}

static void printStr(const char *str, int len)
{
  int slen = strlen(str);
  for (int i=0; i<len; ++i)
    Serial.print(i<slen ? str[i] : ' ');
  smartDelay(0);
}

// converts timezones using utcOffset
void convertTime(TinyGPSDate d, TinyGPSTime t) {
  int day, month, year, second, minute, hour;

  month = d.month();
  day = d.day();
  year = d.year();
  hour = t.hour();
  minute = t.minute();
  second = t.second();

  if (hour + utcOffset < 0) {   // doesn't matter if offset is positive or negative
    if (day - 1 < 1) {
      if (month - 1 < 1) {
        year--;
        month = 13;
      }
      month--;
      switch (month) {
        case 4:
        case 6:
        case 9:
        case 11:
          day = 31;
          break;
        case 2:
          day = 29;
          break;
        default:
          day = 32;
          break;
      }
    }
    day--;
    hour = 24 + (hour + utcOffset);
  } else {
    hour = hour + utcOffset;
  }
  
  snprintf(buf, 32, "%02d:%02d:%02d", hour, minute, second);
  snprintf(buf2, 32, "%02d/%02d", day, month);
  #ifdef DEBUG
    Serial.println(buf);
    Serial.println(buf2);
  #endif
  return;
}

// from ESP32 WiFi example, not used but left in for easy reimplementing if needed for testing
void scanWifi(){
  Serial.println("scan start");

  // WiFi.scanNetworks will return the number of networks found
  netSeen = WiFi.scanNetworks();
    
  Serial.println("scan done");
  if (netSeen == 0) {
      Serial.println("no networks found");
  } else {
      Serial.print(netSeen);
      Serial.println(" networks found");
      for (int i = 0; i < netSeen; ++i) {
          // Print SSID and RSSI for each network found
          Serial.print(i + 1);
          Serial.print(": ");
          Serial.print(WiFi.SSID(i));
          Serial.print(" (");
          Serial.print(WiFi.RSSI(i));
          Serial.print(")");
          Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
          delay(10);
      }
  }
  Serial.println("");
}

// stores data to both of the global lists for location and networks
void storeLists(float lat, float lng){
  Location tempLoc = locationList.get(locationList.size()-1);   // get the last element in the list
  Networks tempNet;
  int listid = tempLoc.id + 1;  // get the id of the last item then increment

  tempLoc.id = listid;        // load the temp object with current data
  snprintf(tempLoc.time, 32, "%s %s", buf, buf2);   // buf and buf2 are set in convertTime to give local data
  tempLoc.lat = lat;
  tempLoc.lng = lng;
  locationList.add(tempLoc);  // store it to the end of the list

  netSeen = WiFi.scanNetworks();  // returns an int, also does the scan for us to pull data from below
  if (netSeen == 0) {
    // don't do anything, just report 0 networks seen and move on for now
  } else {
      for (int i = 0; i < netSeen; ++i) {
          tempNet.id = listid;      // collect SSID and RSSI for each network found
          tempNet.ssid = WiFi.SSID(i);
          tempNet.rssi = WiFi.RSSI(i);
          networkList.add(tempNet); // store network to the end of list
          smartDelay(10);
      }
  }
}

// outputs stored data to Serial monitor for further use
void printLists(){

  int locSize = locationList.size();
  int netSize = networkList.size();
  int spaces;
  for (int i=0; i<locSize; i++){    // for each scan ID, start with the location header
    Location tempLoc = locationList.get(i);
    Serial.println(F("ID   Time            Latitude      Longitude")); // time+date is 14 characters
    Serial.println(F("----------------------------------------------"));
    snprintf(buf, 64, "%02d %16s  %02f     %03f", tempLoc.id, tempLoc.time, tempLoc.lat, tempLoc.lng);
    Serial.println(buf);

    // only do the header once before listing all the networks for that scan ID
    Serial.println(F("SSID                               RSSI"));
    Serial.println(F("----------------------------------------------"));
    for (int j=0; j<netSize; j++){
      Networks tempNet = networkList.get(j);
      if (tempNet.id == tempLoc.id){
        Serial.print(tempNet.ssid);
        spaces = 35-(tempNet.ssid.length());  // simple fixed width solution
        while(spaces--){
          Serial.print(" ");
        }
        Serial.println(tempNet.rssi);
      }
    }
    Serial.println("");
  }
}

// displays last scan in list on the local display
void displayLists(){
  
  Location tempLoc = locationList.get(locationList.size()-1);   // get the last element in the list to grab the ID
  Networks tempNet;
  int netSize = networkList.size();
  int line = 1; // counts how many lines have been drawn for paging, start at 1 to count for header
  
  display->clear();
  display->setCursor(0,0);
  display->setTextColor(WHITE_HIGH);
  display->print("Scan ID: "); display->println(tempLoc.id);
  for (int i=0; i<netSize; i++){
    tempNet = networkList.get(i);
    if (tempNet.id == tempLoc.id){
      rssiColorHelper(tempNet.rssi);  // colorHelper colors SSID by signal strength
      display->println(tempNet.ssid);
    
      line++;
      if (line == 8){   // screen is full
        smartDelay(readDelay);
        line = 0;             // reset line counter, it's 0 now, no header
        display->clear();         // clear the screen
        display->setCursor(0,0);  // reset cursor and continue
      }
    }
  }
  // at the end of the list wait for a period before returning
  smartDelay(readDelay);
}

// sets text color based on signal strength
void rssiColorHelper(int rssi){
  rssi = -rssi;  // RSSI values are negative, flip it
  rssi = rssi/10;   // quick and dirty cut

  switch (rssi){
    case 1:
      display->setTextColor(GREEN_HIGH);
      break;
    case 2:
      display->setTextColor(GREEN_MEDIUM);
      break;
    case 3:
      display->setTextColor(GREEN_LOW);
      break;
    case 4:
      display->setTextColor(ORANGE_HIGH);
      break;
    case 5:
      display->setTextColor(ORANGE_MEDIUM);
      break;
    case 6:
      display->setTextColor(ORANGE_LOW);
      break;
    case 7:
      display->setTextColor(RED_HIGH);
      break;
    case 8:
      display->setTextColor(RED_MEDIUM);
      break;
    default:
      display->setTextColor(RED_LOW);
      break;
  }
}

#ifdef BATTERY
// assuming we're on battery, read the voltage and display it
void getVoltage(int y){
  int voltage = analogRead(A13); // gives 1/2 voltage as XXXX
  int batPer;

  // map the voltage in 3 discharge ranges to reflect discharge profiles (logs are hard, linear4lyfe)
  if (voltage >= int(bat85/2)){ // 85-100%, 4.7-4.9+V range
    batPer = constrain(map(voltage, int(bat85/2), int(fullBat/2), 85, 100), 80, 100); // really only care about constrain >100
  }
  else if (voltage >= int(bat20/2)){ // 20-85%, 3.95-4.7V range
    batPer = map(voltage, int(bat20/2), int(bat85/2), 20, 85);
  }
  else { // 0-20%, 3.72-3.95V range
    batPer = map(voltage, int(flatBat/2), 1750, 0, 20);
  }

  // maps percentage to green/red colors, then bitshifts them to create a single color
  // fades from green at 100% to red at 0% battery
  int batGreen = map(batPer, 0, 100, 0, 255);
  int batRed = map(batPer, 0, 100, 255, 0);
  uint16_t batColor = color565(batRed, batGreen, 0);

  if (voltage*2 > fullBat){   // assume charging
    batColor = color565(0,100,200);
  }

  batPer = map(batPer, 0, 100, 1, 96);
  display->drawRect(0, y, batPer, 2, batColor);
}
#endif

// converts 0-255 color to 5/6/5 format
// 255,0,255 -> 1111 1000 0001 1111
uint16_t color565(int r, int g, int b){
  return ((r/8)<<11) | ((g/4)<<5) | (b/8);
}
