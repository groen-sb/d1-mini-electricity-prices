#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <ESP8266WiFi.h> 
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "Fonts/FreeSans9pt7b.h"
#include "Fonts/FreeSansBold9pt7b.h"
#include "Fonts/TomThumb.h"
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
AsyncWebServer server(80);
// WiFi credentials
const char* ssid     = "Your WiFi SSID";
const char* password = "Your WiFi Password";

#define SCREEN_WIDTH 250
#define SCREEN_HEIGHT 122

// Bar chart data (example values)
int data[] = {20, 35, 50, 15, 40};
int numBars = sizeof(data) / sizeof(data[0]);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000);  // Update every minute

// Variable definition
String weekDays[7] = {"Zondag", "Maandag", "Dinsdag", "Woensdag", "Donderdag", "Vrijdag", "Zaterdag"};
String months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
String lastUpdate;
float lowestSum;
float lowestSumTwelveHours;
int startHour;
int startHourTwelveHours;
float lowestPrice;
float highestPrice;
int currentHour;
int currentMinutes;
float additionalCharge = 0.16;  // Overige kosten
float hourlyPrices[48];  // Global array to store hourly prices
int dayTomorrow;
int monthTomorrow;
int yearTomorrow; 
bool tomorrowIsAvailable;
int hoursAvailable;

// Pin assignments 
#define CS   D8  // Chip Select
#define DC   D2  // Data/Command
#define RST  D4  // Reset
#define BUSY D1  // Busy Indicator

// GxEPD2 display driver for GDEY0213Z98 250x122 ePaper display (SSD1680 driver)
GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(GxEPD2_213_Z98c(CS, DC, RST, BUSY));

// Create WiFiClient object
WiFiClientSecure wifiClient;

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("Initializing ePaper Display...");

  // Initialize the ePaper display
  display.init();
  display.setRotation(3);  // Landscape orientation

  // Connect to Wi-Fi
  Serial.println("Connecting to WiFi...");
  WiFi.hostname("Wemos D1 Mini");  // Set the device name
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.println("Connecting...");
  }
  Serial.println("WiFi connected!");

  // Initialize NTP client
  timeClient.begin();

  // Initialize hourlyPrices array with 0.0
  for (int i = 0; i < 48; i++) {
    hourlyPrices[i] = 0.0;
  }

      WebSerial.begin(&server);
    /* Attach Message Callback */
    server.begin();

        Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP().toString());

}
void parseJSON(String jsonString, bool isTomorrow = false) {
  if (isTomorrow) {
    for (int i = 24; i < 48; i++) {
      hourlyPrices[i] = 0.0;  // Reset tomorrow's prices
    }
  }

  DynamicJsonDocument doc(2048);
  deserializeJson(doc, jsonString);

  JsonArray prices = doc["Prices"].as<JsonArray>();

  if (prices.size() == 0) {
    Serial.println("Tomorrows prices are not yet available.");
    tomorrowIsAvailable = false;
    return;
  }

  int hourIndex = isTomorrow ? 24 : 0;  // Start at 24 if parsing tomorrow's prices
      // Reset prices
    lowestPrice = 999;
    highestPrice = -40;

  // Set available prices
    hoursAvailable = prices.size();
  if (isTomorrow) {
  hoursAvailable = 24 + prices.size();
  }

  // Loop through each price entry in the array
  for (JsonObject priceObj : prices) {
    float price = priceObj["price"].as<float>() + additionalCharge;
    String tijd = priceObj["readingDate"];
    Serial.print(tijd);
    Serial.print(" : ");
    Serial.println(price);
    if (hourIndex < 48) {
      hourlyPrices[hourIndex] = price;  // Save hourly price
      hourIndex++;
    }


    // Find the lowest and highest price
    if (price < lowestPrice) {
      lowestPrice = price;
    }
    if (price > highestPrice) {
      highestPrice = price;
    }

  Serial.print("Lowest Price: ");
  Serial.println(lowestPrice);
  Serial.print("Highest Price: ");
  Serial.println(highestPrice);

  }


}





  

void getEnergyPrices() {
  Serial.println("Getting energy prices..");
  int currentDay, currentMonth, currentYear;
  getTimeCodeFrom(currentDay, currentMonth, currentYear);
  String fromDate = String(currentYear) + "-" + padWithZeros(currentMonth, 2) + "-" + padWithZeros(currentDay, 2) + "T00:00:00+00:00";
  String toDate = String(currentYear) + "-" + padWithZeros(currentMonth, 2) + "-" + padWithZeros(currentDay, 2) + "T23:59:59+00:00";
  String price_api_url = "https://api.energyzero.nl/v1/energyprices?&fromDate=" + fromDate + "&tillDate=" + toDate + "&interval=4&usageType=1&inclBtw=true";
  String price_api_url_tomorrow = "https://api.energyzero.nl/v1/energyprices?&fromDate=" + String(yearTomorrow) + "-" + padWithZeros(monthTomorrow, 2) + "-" + padWithZeros(dayTomorrow, 2) + "T00:00:00+00:00" + "&tillDate=" + String(yearTomorrow) + "-" + padWithZeros(monthTomorrow, 2) + "-" + padWithZeros(dayTomorrow, 2) + "T23:59:59+00:00" + "&interval=4&usageType=1&inclBtw=true";

  Serial.println(price_api_url);
  Serial.println(price_api_url_tomorrow);


  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    wifiClient.setInsecure();  // Skip SSL verification (anders werkt het niet)

    // Fetch today's prices
    http.begin(wifiClient, price_api_url);  // Specify the URL
    int httpCode = http.GET();  // Make the request
    if (httpCode > 0) {
      String payload = http.getString();
      parseJSON(payload);
    } else {
      Serial.println("Error in fetching today's prices");
      String payload = http.getString();
      Serial.println(payload);  // Log the response
      display.fillScreen(GxEPD_WHITE);
      display.setTextSize(1);
      display.setCursor(30, 30);
      display.println("Error fetching today's prices.");
      display.display();
    }

    // Fetch tomorrow's prices
    http.begin(wifiClient, price_api_url_tomorrow);  // Specify the URL
    httpCode = http.GET();  // Make the request
    if (httpCode > 0) {
      String payload_tomorrow = http.getString();
      Serial.println("Tomorrows prices have been fetched.");
      tomorrowIsAvailable = true;
      parseJSON(payload_tomorrow, true);  // Parse tomorrow's prices
    } else {
      Serial.println("Error in fetching tomorrows prices");
      tomorrowIsAvailable = false;
    }

    http.end();  // Close the connection
  }
}


void loop() {
  getCurrentTime();
  getEnergyPrices();
  getUpdateTime();
  findCheapestThreeHourWindow();
  updateDisplay();
  delay(3600000);  // Update every hour  
}

void getUpdateTime() {
  // Get the formatted time
  String formattedTime = timeClient.getFormattedTime();
  lastUpdate = formattedTime.substring(0, 5);
}

void getTimeCodeFrom(int &day, int &month, int &year) {
  timeClient.update();
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime(&epochTime);
  day = ptm->tm_mday;
  month = ptm->tm_mon + 1;
  year = ptm->tm_year + 1900;

  time_t tomorrowEpoch = epochTime + 86400; // 86400 seconds in a day
  struct tm *ptmTomorrow = gmtime(&tomorrowEpoch);
  dayTomorrow = ptmTomorrow->tm_mday;
  monthTomorrow = ptmTomorrow->tm_mon + 1;
  yearTomorrow = ptmTomorrow->tm_year + 1900;
}

void getCurrentTime() {
  timeClient.update();
  currentHour = timeClient.getHours();
  currentMinutes = timeClient.getMinutes();
}

String padWithZeros(int num, int length) {
  String str = String(num);
  while (str.length() < length) {
    str = "0" + str;
  }
  return str;
}

void findCheapestThreeHourWindow() {
  float lowestSum = 9999.0; // Initialize to a high value
  startHour = -1;       // Initialize startHour to an invalid value

  lowestSumTwelveHours = 999.0;
  startHourTwelveHours = -1;
  
  // Determine how many hours to check, depending on the availability of tomorrow's data
  int aantalUrenChecken;
  
  aantalUrenChecken = hoursAvailable - 3;  // Check up to hour 44, accounting for 3-hour window
  

  // Loop through the hourly prices, calculating the sum for each 3-hour window
  for (int i = 0; i <= aantalUrenChecken; i++) {  // Adjust to not go out of bounds
    float threeHourPrice = (hourlyPrices[i] + hourlyPrices[i + 1] + hourlyPrices[i + 2]);
    
    Serial.print(i);
    Serial.print(":");
    Serial.println(hourlyPrices[i]);
    Serial.println(threeHourPrice);

    // Ensure that the current window starts after the current hour
    if (threeHourPrice < lowestSum && i < aantalUrenChecken && i+1 > currentHour) {
      lowestSum = threeHourPrice;
      startHour = i+1;  // Keep track of the start of the 3-hour window
    }

    // Ensure that the current window starts after the current hour
    if (threeHourPrice < lowestSumTwelveHours && i < (currentHour + 9) && i+1 > currentHour) {
      lowestSumTwelveHours = threeHourPrice;
      startHourTwelveHours = i+1;  // Keep track of the start of the 3-hour window
    }
  }

  // If a valid start hour was found, print the cheapest window
  if (startHour != -1) {
    Serial.print("Goedkoopste moment vandaag: ");
    Serial.print(startHour);
    Serial.print(":00 to ");
    Serial.print(startHour + 3);
    Serial.print(":00, with total price: ");
    Serial.println(lowestSum);
  } else {
    // Handle the case where no valid window was found
    Serial.println("Geen geldig moment gevonden.");
  }
}


void updateDisplay() {
  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE);

  display.setCursor(0, 10);

    // Laatste update  
  display.setFont(&TomThumb);
  display.setTextSize(2);
  display.print("Update: ");
  display.println(lastUpdate);
  display.print("B: ");
  display.println(hoursAvailable);



  display.setTextSize(2);
  display.println(" ");

  display.println("Goedkoopste prijzen: ");
  display.println();
  display.setTextSize(4);
  display.setFont(&TomThumb);
  if (startHour < 24) {
      display.print(padWithZeros(startHour + 1, 2));
    } else {
      display.print(padWithZeros(startHour + 1 - 24, 2));
    }
  display.print(":00 - ");
    if (startHour < 24) {
      display.print(padWithZeros(startHour + 4, 2));
    } else {
      display.print(padWithZeros(startHour + 4 - 24, 2));
    }
  display.print(":00 ");

if (startHour - currentHour > -1) {
  display.print("(+");
  display.print(startHour - currentHour);
  display.println(")");
} else {
  display.println();
}


display.setTextSize(2);
  display.print("Binnen 12u: ");
  if (startHourTwelveHours < 24) {
      display.print(padWithZeros(startHourTwelveHours + 1, 2));
    } else {
      display.print(padWithZeros(startHourTwelveHours + 1 - 24, 2));
    }
  display.print(":00 - ");
    if (startHourTwelveHours + 3 < 24) {
      display.print(padWithZeros(startHourTwelveHours + 4, 2));
    } else {
      display.print(padWithZeros(startHourTwelveHours + 4 - 24, 2));
    }
  display.print(":00 ");

if (startHourTwelveHours - currentHour > -1) {
  display.print("(+");
  display.print(startHourTwelveHours - currentHour);
  display.println(")");
} else {
  display.println();
}

  // Laagste prijs
  display.print("Laagst: ");
  display.print(lowestPrice, 2);
  display.println(" euro/kWh.");

  // Hoogste prijs
  display.print("Hoogst: ");
  display.print(highestPrice, 2);
  display.println(" euro/kWh");

 // Call function to draw the bar chart
  drawBarChart();

  // Update het display.
  display.display();
}


void drawBarChart() {
  // Set the dimensions and positions for the chart
  int barWidth = 140 / hoursAvailable;   // Width of each bar
  int spacing = 1;                       // Space between bars
  int chartHeight = 28;                  // Maximum height of the chart (adjust as needed)
  int xOffset = 100;                     // Starting x position for the chart
  int yOffset = 30;                      // Starting y position (bottom of the chart)

  // Find the maximum value in the data (hourlyPrices) for scaling the bars
  float maxValue = 0;
  for (int i = 0; i < hoursAvailable; i++) {
    if (hourlyPrices[i] > maxValue) {
      maxValue = hourlyPrices[i];
    }
  }

  // Avoid dividing by zero if maxValue is 0
  if (maxValue == 0) {
    maxValue = 1;
  }

  // Loop through the hourlyPrices to draw each bar
  for (int i = 0; i < hoursAvailable; i++) {
    // Scale the floating-point values to a suitable integer range
    int barHeight = map(hourlyPrices[i] * 100, 0, maxValue * 100, 0, chartHeight);  // Multiply by 100 for better scaling
    int xPos = xOffset + (i * (barWidth + spacing));  // Calculate the x position of the bar
    int yPos = yOffset - barHeight;                   // Calculate the y position of the bar

    // Draw the bar (filled rectangle)
    display.fillRect(xPos, yPos, barWidth, barHeight, GxEPD_BLACK);  // Adjust color as needed
  }

  // Draw a 1-pixel vertical line at the currentHour position
  int currentHourPos = (xOffset + ((currentHour - 2) * (barWidth + spacing))) + 2;  // Calculate x position for currentHour

  // Draw the line from top of the chart to the bottom (yOffset is the bottom)
  display.drawLine(currentHourPos, yOffset - chartHeight, currentHourPos, yOffset + 3 , GxEPD_BLACK);
}

