// 7.11.20 Converted to Photon (dmf_photon_B)
//         Rewired MAX81355 3V3 supply (old Spark 3V3 pin is VBAT on Photon)
//         Now working with Ubidots STEM ok. 
// 
// 7.10.20 Trying to update for Ubidots STEM + Ubidots.h library
// Spark Core is not supported. Therefore have to move to Photon. 
// ** problem ** Photon is not reading the Thermocouple meter (MAX31855)
// So -> seeing null values with corrections (16.7°). 

/*************** DMF 4.22.15
 * This code is used in 'WSHP Water Temp SparkCore'
 * Core Name 'dmf_ExternalAnt_2' [old] 
 * 7.11.20 Running on Photon: dmf_photon_B
 ***************/

 /***************************************************
  This is an example for the Adafruit Thermocouple Sensor w/MAX31855K

  Designed specifically to work with the Adafruit Thermocouple Sensor
  ----> https://www.adafruit.com/products/269

  These displays use SPI to communicate, 3 pins are required to
  interface
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ****************************************************/

// Adapted directly from Adafruit code from my file serialthermocouple_dmf 9.1.12
// dmf 2.7.15


// This #include statement was automatically added by the Spark IDE.
#include "HttpClient.h"
// This #include statement was automatically added by the Spark IDE.
#include "Adafruit_MAX31855.h"
// need to include math.h explicitly for the isnan() test
#include "math.h"

// 7.10.20 
#include "Ubidots.h"

// access tokens are externalized
#include "ubidots_tokens.h"

// Declaring the variables for Http to Ubidots
HttpClient http;

// Headers currently need to be set at init, useful for API keys etc.
http_header_t headers[] = {
      { "Content-Type", "application/json" },
      { "X-Auth-Token" , TOKEN },
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};

http_request_t request;
http_response_t response;

// dmf 9.1.12 change the following from Adafruit defaults (3,4,5)
// SparkCore pins D4, D5, D6
int thermoDO = 4;
int thermoCS = 5;
int thermoCLK = 6;

// dmf 2.21.15 control pin for the ADG608 MUX
// SparkCore pin D0
int thermoMUX = 0;
int thermoToggle = 0;

// note: this will be registered as Spark variable, and reported
double thermoTempI = 0.0;
double thermoTempFA = 0.0;
double thermoTempFB = 0.0;

// dmf 2.19.15 Max31885T, dmf chip 1 device, empirical ~linear correction over range 0-40C
// dmf 3.1.15 this correction has worked for both boxes constructed so far
// Correction applied to Centigrade reading!
double BadZero = -8.498;
double BadSlope = 1.27510;
// dmf 3.9.15 pain in the ass correction for deviation between A and B (approx -3degF observed at outlet)
// Correction applied to derived Farenheit reading!
double AadOffset = 3.0;     // 7.12.20 Note outlet temp ~ hallway pipe temp, so keep this AadOffset
double BadOffset = 2.2;     // 7.12.20 Correction from average deviation over 30 minutes w/ no heat  

Adafruit_MAX31855 thermocouple(thermoCLK, thermoCS, thermoDO);

// 7.10.20 Ubidots migration
Ubidots ubidots(UBIDOTS_TOKEN, UBI_EDUCATIONAL, UBI_TCP); 

void setup() {
  Serial.begin(9600);

  // Setup for Ubidots
  request.hostname = "things.ubidots.com";
  request.port = 80;

  // Register a Spark variable here
  // note: this is following the examples on community.spark.com
  Particle.variable("thermotempFA", &thermoTempFA, DOUBLE);
  Particle.variable("thermotempFB", &thermoTempFB, DOUBLE);
  Particle.variable("thermotempIA", &thermoTempI, DOUBLE);

  // Setup to control the MUX
  pinMode(thermoMUX, OUTPUT);
  digitalWrite(thermoMUX, LOW);

  // wait for MAX chip to stabilize
  delay(500);
}

void loop() {

   double i = thermocouple.readInternal();
   double c = thermocouple.readCelsius();
   double f = thermocouple.readFarenheit();

   // basic readout test, just print the current temp
   Serial.print("Internal Temp = ");
   Serial.println(i);

   if (isnan(c)) {
     Serial.println("Something wrong with thermocouple!");
   } else {
    Serial.print("C = ");
    Serial.println(c);
   }

   Serial.print("Faren = ");
   Serial.println(f);

   // set the Spark.variables thermoTemp
   thermoTempI = i;
   //floatValue = floor(floatValue*10+0.5)/10;
   thermoTempI = floor(thermoTempI*10+0.5)/10;

   // CORRECTION FOR BAD BEHAVIOR!!
   c = (c * BadSlope) + BadZero;
   f = ((9.0 * c)/5.0) + 32.0;

     // report values
   Serial.print("Corrected C =");
   Serial.println(c);
   Serial.print("Corrected F =");
   Serial.println(f);

   delay(100);

   if (thermoToggle == 0) {   // This is from MUX input 1 "A"

        // This is OUTLET temperature

        thermoTempFA = f;
        // CORRECTION FOR TEMPERATURE MISREAD OFFSET!!
        thermoTempFA += AadOffset;
        //floatValue = floor(floatValue*10+0.5)/10;
        thermoTempFA = floor(thermoTempFA*10+0.5)/10;

        Serial.print("thermoTemp_FA - ");
        Serial.println(thermoTempFA);

        delay(100);

        // Send to Ubidots... VARIABLE_IDF_A defined above
        // request.path = "/api/v1.6/variables/" VARIABLE_IDFA "/values";
        // request.body = "{\"value\":" + String(thermoTempFA) + "}";

        // http.post(request, response, headers);

        // toggle the A1 to high -> switch to input 3 "B"
        digitalWrite(thermoMUX, HIGH);
        thermoToggle = 1;
        Serial.println("\n Next line will be input 3");
        // delay 30 seconds to before reading input 3
        // note: 2.22.15 code failed when this interval was set to only 2 seconds!
        // worked when connected to computer by USB, but now when wall-powered.
        delay (30000);


   } else {   // This is from MUX input 3 "B"

        // This is INLET temperature 

        thermoTempFB = f;
        // CORRECTION FOR TEMPERATURE MISREAD OFFSET!!
        thermoTempFB += BadOffset;
        //floatValue = floor(floatValue*10+0.5)/10;
        thermoTempFB = floor(thermoTempFB*10+0.5)/10;

        Serial.print("thermoTemp_FB - ");
        Serial.println(thermoTempFB);

        delay(100);

        // Send to Ubidots... VARIABLE_IDF_B defined above
        // request.path = "/api/v1.6/variables/" VARIABLE_IDFB "/values";
        // request.body = "{\"value\":" + String(thermoTempFB) + "}";
        // http.post(request, response, headers);

        // 7.10.20 Send both temperatures at the same time after both have been recorded
        ubidots.add(VARIABLE_INLET, thermoTempFB);
        ubidots.add(VARIABLE_OUTLET, thermoTempFA);

        bool bufferSent = false;
        bufferSent = ubidots.send(UBIDOTS_DEVICE); 

        if (bufferSent) {
           Serial.println("Values sent by the device");
        }

        // toggle the A1 level to low -> switch to input 1 "A"
        digitalWrite(thermoMUX, LOW);
        thermoToggle = 0;
        Serial.println("\n Next line will be input 1");

        //delay before next pair of readings
        // should be delay(330000) for 6 minutes total between posts
        //this is important! only have 30000 datapoints/month in Ubidots account
        delay (330000);

   }

}

/***********************************************
 Comment from a tindle site:
 I've been running six T type Thermocouples at liquid nitrogen levels and
 it took me a long time to get correct temperature values from the Arduino
 Nano - I finally worked out that the MAX31855 chip uses a linear approximation
 of the thermocouple voltage (search the Arduino forum for 'MAX 31855 and
 Thermocouple Errors') so don't blame this board if the temperatures you may
 get are in error out at extreme temperatures, these boards perform perfectly
 but the MAX31855 output needs 'recalculating' in your code to get correct
 temperature values.
 **********************************************/
