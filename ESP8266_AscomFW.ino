/*
   This program uses an esp8266 board to host a web server providing a rest API to control a stepper-motor 
   driven filter wheel as used on a telescope. The Stepper motor controller is accessed using the ESP8266 I2c interface 
   in order to provide a user button-interface through the i2c expander chip. 
   AS of 22/ Feb 2019 this now implements the ASCOM REST api for a filterwheel natively and adds a 
   user setup html page which ASCOM doesn't provide unless you use a native asCOM driver as initial setup. 
   Specified filterWheel default ID as zero since we will only ever handle the one filterwheel for this hostname/address. 
   For testing, bear in mind that while browsers can't provide a manual put action, neither can curl 
   run from the windows command-line without wrapping the arguments in quotes due to windows interpreting the "&" character as an additional command. 
   Enable the debug http core setting in arduino instead to get access to all the inline request args as debug output.

To do 
Test handler functions  - tested
Add functions to handle update for hostname, wheel name, number of filters - in progress. - complete
Add EEPROM saving/restoring functions and offsets for fields. - complete - not sure that it detects properly though.
Write fields to EEPROM on successful setting. - complete - not tested
Store last position - complete - not tested.
Fix checks for case-insensitive parameters - not started.
Add Wifimanager to be able for user to set Wifi parameters
Add MQTT client to write state to subscribers
Replace/tee debug output to registered listeners or syslog location. UDP ?

To test:
 curl -X PUT -d "name=myWheel&filtersPerWheel=5" http://EspFwl01/filterwheel/0/FilterCount
 curl -X get http://Espfwl01/api/filterwheel/0/devicename
 
Dependencies
Arduino JSON library 5.13 ( moving to 6 is a big change) 
Expressif ESP8266 board library for arduino - configured for v2.5
ESP8266WebServer

ESP8266-12 Huzzah
  /INT - GPIO3
  SDA - GPIO4
  SCL - GPIO5
  [SPI SCK = GPIO #14 (default)
  SPI MOSI = GPIO #13 (default)
  SPI MISO = GPIO #12 (default)]

Condition	                        Alpaca Error Number	
Successful transaction	            0x0 
Property or method not implemented	0x400 
Invalid value	                     0x401 
Value not set	                     0x402 
Not connected	                     0x407 
Invalid while parked	               0x408 
Invalid while slaved	               0x409 
Invalid operation	                  0x40B 
Action not implemented	            0x40C 

All five elements of the API path are case sensitive and must always be in lower case. For example, this is the only valid casing for a call to the Telescope.CanSlew property:
/api/v1/telescope/0/canslew

Alpaca parameters are key-value pairs where:
•	The parameter key is case insensitive
•	The parameter value can have any casing required.
This is the same behaviour as defined for HTTP header keys in RFC7230.
For example, these are all valid API parameters:
/api/v1/telescope/0/canslew?clientid=0&clienttransactionid=23
/api/v1/telescope/0/canslew?ClientID=0&ClientTransactionID=23
/api/v1/telescope/0/canslew?CLIENTID=0&CLIENTTRANSACTIONID=23
Clients and drivers must expect incoming API parameter keys to have any casing.
*/

#include "DebugSerial.h"

//Manage different pinout variants of the ESP8266
#define __ESP8266_12
#ifdef __ESP8266_12
#define DIRN_PIN 4
#define STEP_PIN 5
#define ENABLE_PIN 2
#define HALF_STEP_PIN 7
#define BUTN_A_PIN 13
#define BUTN_B_PIN 14
#define BUTN_C_PIN 15
#else // __ESP8266_01
#define DIRN_PIN 2
#define STEP_PIN 3
#endif

//Define direction constants 
#define DIRN_CW 0
#define DIRN_CCW 1

#include <Esp.h> //used for restart
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
//https://links2004.github.io/Arduino/d3/d58/class_e_s_p8266_web_server.html
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <EEPROMAnything.h>
#include "JSONHelperFunctions.h"
#include <GDBStub.h> //Debugging stub for GDB

//Ntp dependencies - available from v2.4
#include <time.h>
#include <sys/time.h>
#include <coredecls.h>

#ifdef ESP8266
extern "C" {
#include "ets_sys.h" //Base timer and interrupt handling 
#include "osapi.h"   //re-cast ets_sys names  - might be missing hw_timer_t - cast to uint32_t  or _ETSTIMER_ ?
#include "user_interface.h"
}
#endif

const int MAX_NAME_LENGTH  = 25;
const int MAX_FILTER_COUNT = 10;

//Setup the network characteristics
#include "SkybadgerStrings.h"
const char defaultHostname[MAX_NAME_LENGTH] = "espFwl01";
char* hostname = NULL;
char* thisID = NULL;

//ASCOM variables 
unsigned int transactionId;
unsigned int connectedClient = -1;
bool connected = false;
const String DriverName = "Skybadger.ESPFilterWheel";
const String DriverVersion = "0.0.1";
const String DriverInfo = "Skybadger.ESPFilterWheel RESTful native device. ";
const String Description = "Skybadger ESP2866-based wireless ASCOM filter wheel";
const String InterfaceVersion = "2";

//States to move through - when this is moved to a state machine
enum filterDriverStates { FILTER_IDLE, FILTER_MOVING, FILTER_BL_PRE, FILTER_BL_POST, FILTER_ERR };
int filterState = FILTER_IDLE;

//Filterwheel filter information
const int FilterNameLengthLimit = MAX_NAME_LENGTH;
const int defaultFiltersPerWheel = 5;
const int stepsPerRevolution = 2048; //Determined by hardware

int targetFilterId = 0; //next requested position - updated into current when we get there
int currentFilterId = 0;
int targetDistance = 0;
int filtersPerWheel = defaultFiltersPerWheel; 
int newfiltersPerWheel = 0; //used when the number of filters is updated. 
int defaultFilterPositions[defaultFiltersPerWheel] = { 0, stepsPerRevolution/5, stepsPerRevolution*2/5, stepsPerRevolution*3/5, stepsPerRevolution*4/5 };
int*   filterPositions = NULL;
int*   focusOffsets    = NULL;
char*  wheelName       = NULL;
char** filterNames     = NULL ;

//Go to target, wind out in a single direction and then wind back in to target once complete so always approach from a single direction
bool backlashEnabled = false;
enum backlashStates { WINDING_OUT, WINDING_IN }; 
 
//Basic stepper info - update based on your stepper and number of filters. 
// Assumes filters are evenly spaced.
int stepPosition  = 0;
int backlash = 25;
int stepDirn = DIRN_CW;
volatile int stepFlag = 0;
volatile boolean newButtonFlag = 0;
bool isMoving = false;
volatile int t2Flag = 0;
int16_t home = 0;

// Create an instance of the server
// specify the port to listen on as an argument
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer updater;

//Hardware device system functions - reset/restart etc
EspClass device;
ETSTimer timer;
ETSTimer timoutTimer;

//local functions
void onTimer(void);
void onTimeoutTimer(void);
void backlashCompensate(void);
void enableStepper( boolean );
void step( void);
void setup(void);
void setDefaults(void);
void updateStepDirection(bool direction); //false = 0 = reverse, true = forward = 1

//Declarations - web handlers
// REST URL handling
#include "FWEeprom.h"
//Device Driver common functions
#include "ASCOMAPICommon_rest.h"
//ASCOM Filterwheel REST API specific functions
#include "ASCOMAPIfilterwheel_rest.h"

//others
void handleRootReset(void);
void handlerNotFound(void);

void setup_wifi()
{
  //Setup Wifi
  int zz = 0;
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostname);
  WiFi.begin( ssid1, password1 );
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);//Thisdelay is essentially for the DHCP response. Shouldn't be required for static config.
    Serial.print(".");
    if ( zz++ > 400 )
       device.restart();
  }
    //WiFi.setDNS( badgerDNS, gatewayDNS );
  Serial.println("WiFi connected");
  Serial.printf("SSID: %s, Signal strength %i dBm \n\r", WiFi.SSID().c_str(), WiFi.RSSI() );
  Serial.printf("Hostname: %s\n\r",       WiFi.hostname().c_str() );
  Serial.printf("IP address: %s\n\r",     WiFi.localIP().toString().c_str() );
  Serial.printf("DNS address 0: %s\n\r",  WiFi.dnsIP(0).toString().c_str() );
  Serial.printf("DNS address 1: %s\n\r",  WiFi.dnsIP(1).toString().c_str() );
  Serial.println();

  //Setup sleep parameters
  //WiFi.mode(WIFI_NONE_SLEEP);
  //wifi_set_sleep_type(LIGHT_SLEEP_T);
  wifi_set_sleep_type(NONE_SLEEP_T);

}

void setup()
{
  int i=0;
  
  // put your setup code here, to run once:
  Serial.begin( 115200, SERIAL_8N1, SERIAL_TX_ONLY);
  Serial.println();
  Serial.println("ESP stepper starting:uses step and direction only.");
  gdbstub_init();

  delay(2000);
  //Start NTP client
  configTime(TZ_SEC, DST_SEC, timeServer1, timeServer2, timeServer3 );

  
  //Read stored settings
  EEPROM.begin(512);  
  setupFromEeprom();
   
  //filterwheel hardware setup
  pinMode(DIRN_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite( DIRN_PIN, DIRN_CW );
  digitalWrite( STEP_PIN, LOW);
  digitalWrite( ENABLE_PIN, HIGH); //Active low.

  setup_wifi();
  
  //Web server handler functions 
  server.onNotFound(handlerNotFound);

  //Common ASCOM handlers
  server.on("/api/v1/filterwheel/0/action",           HTTP_PUT, handleAction );
  server.on("/api/v1/filterwheel/0/commandblind",     HTTP_PUT, handleCommandBlind );
  server.on("/api/v1/filterwheel/0/commandbool",      HTTP_PUT, handleCommandBool );
  server.on("/api/v1/filterwheel/0/commandastring",   HTTP_GET, handleCommandString );
  server.on("/api/v1/filterwheel/0/connected", handleConnected );
  server.on("/api/v1/filterwheel/0/description",      HTTP_GET, handleDescriptionGet );
  server.on("/api/v1/filterwheel/0/driverinfo",       HTTP_GET, handleDriverInfoGet );
  server.on("/api/v1/filterwheel/0/driveraersion",    HTTP_GET, handleDriverVersionGet );
  server.on("/api/v1/filterwheel/0/name",             HTTP_GET, handleNameGet );
  server.on("/api/v1/filterwheel/0/supportedactions", HTTP_GET, handleSupportedActionsGet );

  //Filterwheel specific handlers
  server.on("/", HTTP_GET, handleSetup);
  server.on("/api/v1/filterwheel/0/Names",        HTTP_GET, handleFilterNamesGet );
  server.on("/api/v1/filterwheel/0/FocusOffsets", HTTP_GET, handleFocusOffsetsGet );
  server.on("/api/v1/filterwheel/0/Position",     HTTP_PUT, handlePositionPut ); 
  server.on("/api/v1/filterwheel/0/Position",     HTTP_GET, handlePositionGet ); 
  
  //Setup webpage handlers - HTML forms don't support PUT - use GET instead.
  //Modern web browsers (chrome) will turn a put into a get. 
  //Change this to PUT once initial test complete.
  //Currently actually different URLs since different case. 
  //For a filter wheel, only required PUT is position
  server.on("/filterwheel/0/FilterNames",  HTTP_GET, handleFilterNamesPut );
  server.on("/filterwheel/0/Hostname",     HTTP_GET, handleHostnamePut );
  server.on("/filterwheel/0/Wheelname",    HTTP_GET, handleNamePut );
  server.on("/filterwheel/0/FilterCount",  HTTP_GET, handleFilterCountPut );
  server.on("/filterwheel/0/FocusOffsets", HTTP_GET, handleFocusOffsetsPut );
  
  //setup interrupt-based 'soft' alarm handler for software stepping
  ets_timer_setfn( &timer, onTimer, NULL ); 
  ets_timer_setfn( &timeoutTImer, onTimeoutTimer, NULL ); 
  //ets_timer_arm_new( &timer, 4000, 1/*repeat*/, 0);//the last arg indicates usecs(1)  rather than msecs (0). 
 
  //Start web server
  updater.setup( &server);
  server.begin();

  DEBUGSL1( "setup complete");
}

/*
  Interrupt handler function captures button press changes pulses from user buttons and flags to main loop to process. 
  Do this by capturing time of interrupt, waiting until after timeout msecs and checking again.
  https://github.com/esp8266/esp8266-wiki/wiki/gpio-registers
*/
void pulseCounter(void)
{
 //if edge rising, capture counter
 byte signalDirection = GPIP(3); //READ_PERI_REG( 0x60000318, PulseCounterPin );
 if ( signalDirection == 0 ) 
 {
    startTime =  ESP.getCycleCount();
    //set flag.
    newButtonFlag = 1;
 }
}

//Interrupt handler for async event timer
void onTimeoutTimer( void )
{
  timeoutFlag = true;
}

void onTimer( void* pArg )
{
  if ( stepFlag >= 0 )
	  stepFlag++;
  else 
	  stepFlag = 1;
}

void loop()
{
	String outbuf;
  long int nowTime = system_get_time();
  long int indexTime = startTime - nowTime;
  
  // Main code here, to run repeatedly:
  //frequency timing test goes here - use system clock counter
#if defined DEBUGLOOP
  if( isMoving && stepFlag ) 
  {
    DEBUGS1 ( "Position:");
    DEBUGSL2( stepPosition, DEC );
    DEBUGS1 ( "Dirn: "); 
    DEBUGSL1( (stepDirn == DIRN_CW)? "DIRN_CW": "DIRN_CCW" );
    DEBUGSL2( targetDistance, DEC );

    if ( targetDistance == 0 ) //May need to decelerate
    {
       DEBUGSL1 ( "Distance to selected filter is zero, halting"); 
       currentFilterId = targetFilterId;
       enableStepper(false);
    }
    else 
    {
      step();  
    }
  }
  else if ( !isMoving )
  {
    stepFlag = 0; //reset when start moving or disable the timer when not in use.
    if ( stepPosition != filterPositions[currentFilterId ] )
    {
      //Detected change required due to offset from desired position.
      DEBUGSL1 ( "Detected position offset - setting up move"); 
      targetDistance = filterPositions[currentFilterId] - stepPosition;
    }
    else if ( targetFilterId != currentFilterId )
    {
      DEBUGSL1 ( "Detected new filter specified - setting up move"); 

      //case 0 - increasing by less than 3 - CW
      //2->4 800->1600 targetDistance = 800, stepcount = 1600
      //case 1 - increasing by 3 or more   - CCW
      //1->4 400->1600 targetDistance = 1200->-800, stepcount = 1600 CW 
      //case 2 - decreasing by less than 3 - CCW
      //4->2 1600->800 targetDistance = -800, stepcount = 800 CCW
      //case 3 - decreasing by 3 or more   - CW
      //4->1 1600->400 targetDistance = -1200->800, stepcount = 400 CW
      //4->0 1600->2048/0 targetDistance = -1600->400, stepcount = 0 CW
      targetDistance = filterPositions[targetFilterId] - stepPosition;
    }
    if( targetDistance > 0 )
      stepDirn = DIRN_CW; 
    else
      stepDirn = DIRN_CCW;       
    targetDistance = abs(targetDistance);
    
    if ( targetDistance > stepsPerRevolution/2 )
    {
      DEBUGSL1 ( "Reversing direction to minimise move to selected filter"); 
      targetDistance = abs(stepsPerRevolution - targetDistance);
      stepDirn = ( stepDirn == DIRN_CW )? DIRN_CCW : DIRN_CW;//reverse direction
    }
    updateStepDirection( stepDirn );
    
    //update isMoving flag to indicate a move has been requested. Catch next time around.
    if ( targetDistance != 0 )
      enableStepper(true);     
  }
#endif
  //If there are any client connections - handle them.
  server.handleClient();
 }


  void enableStepper( boolean enable )
  {
      DEBUGSL1("------------------------------------");
      DEBUGSL1( "Position:"); DEBUGSL2( stepPosition, DEC );
      DEBUGSL1( "Dirn: ");  DEBUGSL1( (stepDirn == DIRN_CW)? "DIRN_CW": "DIRN_CCW" );
      DEBUGSL1( "Distance: ");DEBUGSL2( targetDistance, DEC );
      DEBUGSL1("------------------------------------");

    //stop/start the timer
	  if (enable)
	  {
		  DEBUGSL1 ("Enabling stepper");
		  ets_timer_arm_new(&timer, 10, 1, 0);
      digitalWrite( ENABLE_PIN, LOW );
      stepFlag = 0;
      isMoving = true;
	  }
	  else
	  {
      DEBUGSL1 ("Disabling stepper");      
      ets_timer_disarm(&timer);
      timer1_disable();
      digitalWrite( ENABLE_PIN, HIGH );
      isMoving = false;
	  }
  }
  
  void step(void)
  {
    //toggle step line
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(100);
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(100);
    digitalWrite(STEP_PIN, LOW);
    
    //Book-keeping of position and distance. 
    if ( stepDirn == DIRN_CW)
    {
        stepPosition++;
    }
    else
    {
        stepPosition--;
    }
    targetDistance--;

    //keep count in range 0-stepsPerRevolution
    stepPosition += stepsPerRevolution;
    stepPosition %= stepsPerRevolution;
    
    stepFlag--;
  }

  void updateStepDirection( bool direction )
  {
  	//Setup DIRN line
  	digitalWrite(DIRN_PIN, direction );
  	delayMicroseconds(200);
  }

void backlashCompensate()
{
  /*
   * The point of backlash compensation is to always come at the final position from the same direction. 
   * So we overshoot or undershoot by the backlash amount to ensure approaching from the same direction.
   */
  if (backlashEnabled)
  {
    if( stepDirn == DIRN_CW )
    {
      targetDistance -= backlash;
      stepDirn = DIRN_CCW;
    }
    else
    {
      targetDistance -= backlash;
      stepDirn = DIRN_CCW;
    }
    targetDistance += stepsPerRevolution;
    targetDistance = targetDistance % stepsPerRevolution;
  }
}
  
/*
 * Reset the device - Non-ASCOM call
 */
  void handleRootReset()
  {
    String message;
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);

    JsonObject& root = jsonBuffer.createObject();
    root["messageType"] = "Alert";
    root["message"]= "Esp8266 Resetting";
    Serial.println( "Server resetting" );
    root.printTo(message);
    server.send(200, "application/json", message);
    device.restart();
    return;
  }  

void handlerNotFound()
{
  String message;
  int responseCode = 400;
  uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
  uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
  
  //JSON data formatter
  DynamicJsonBuffer jsonBuffer(256);

  JsonObject& root = jsonBuffer.createObject();
  jsonResponseBuilder( root, clientID, transID, "HandlerNotFound", 0x500, "No REST handler found for argument - check ASCOM filterwheel v2 specification" );    
  JsonObject& err = root.createNestedObject("ErrorMessage");
  err["Value"] = "Filter wheel REST handler not found or parameters incomplete";
  DEBUGSL1( message );
  root.printTo(message);
  server.send( 400, "text/json", message);
  return;
};

/* old functions
  void handleFilter()
  {
    int args;
    String arg, message;
    int responseCode = 200;
    int method; 
    String newFilterName;
    int newFilterPosition;
    int newFilterID;
    int i=0;
    
    JsonObject& root = jsonBuffer.createObject();
            
    args = server.args();
    //No args  - return which filter is currently selected/requested. 
    // Not a guarantee of actual position if we are currently moving.
    if ( args == 0 )
    {
      root["messageType"] = "status";
      root["filterID"] = currentFilterId;
      root["filter"] = filterNames[currentFilterId];
      root["filterPosition"] = stepPosition;
    }   
    //Otherwise update the filter name or position relating to filter <id>
	  else if (server.hasArg("id"))
	  {
  		newFilterID = server.arg("id").toInt();
  		
  		///need to check id is in range
  		if (newFilterID < 0 || newFilterID >= filtersPerWheel)
  		{
  			root["messageType"] = "alert";
  			root["message"] = "Filter specified must be between 0 and 5";
  			responseCode = 402;
  		}
  		else 
  		{
  			String newName;
  			//Update the filter name for id 
  			if (server.hasArg("name"))
  			{
  				newName = server.arg("name");
  				if (newName.length() > 0 && newName.length() <= 30)
  				{
  					filterNames[newFilterID] = server.arg("name");
  					root["messageType"] = "status";
  					root["message"] = "new filter name applied";
  					responseCode = 200;
  				}
  				else
  				{
  					root["messageType"] = "alert";
  					root["message"] = "new filter name must be > 0 and less than 30 characters long";
  					responseCode = 402;
  				}
  			}
  			//Update the fiter position for ID
  			else if (server.hasArg("position"))
  			{
  				if (server.arg("position").toInt() >= 0 || server.arg("position").toInt() < stepsPerRevolution)
  				{
  					filterPositions[newFilterID] = server.arg("position").toInt();
  					root["messageType"] = "status";
  					root["message"] = "Position updated ";
  					responseCode = 200;
  				}
  				else
  				{
  					root["messageType"] = "alert";
  					root["message"] = "new position for filter must be >= 0 and less than the maximum for a revolution.";
  					responseCode = 402;
  				}
  			}
  			//Set the next filter ID to move to.
  			else
  			{
  				//Update the current target filter to move to.
  				if ( !isMoving )
          {
  				  targetFilterId = newFilterID;
            root["messageType"] = "status";
            root["message"] = "new filter target requested";
            responseCode = 200;
           //let the loop handler manage moving.
          }
          else
          {
            root["messageType"] = "alert";
            root["message"] = "Filter wheel still moving to last position - check isMoving status before requesting next";
            responseCode = 402;
          }
  			}
  		}
	  }
    
    root.printTo(message);
    server.send( responseCode, "application/json", message);
  }
 */
 /*  
  void handleStep()
  {
    int args;
    String arg, message;
    int responseCode = 200;
    int method; 
    int i=0;
    int position = 0;
    
    JsonObject& root = jsonBuffer.createObject();
            
    args = server.args();
    // No args  - return current step position. 
    // Not a guarantee of actual position if we are currently moving.
    if ( args == 0 )
    {
      root["messageType"] = "status";
      root["filterWheelPosition"] = stepPosition;
      responseCode = 200;
    }
    else if (server.hasArg("position") )
    {
      position = server.arg("position").toInt();
      if( position >=0 && position < stepsPerRevolution )
      {
        stepPosition = position;
        root["messageType"] = "status";
        root["filterWheelPosition"] = stepPosition;
        responseCode = 200;
      }
      else
      {
        root["messageType"] = "alert";
        root["message"] = "Unable to set position - must be between 0 and 'stepsPerRevolution'";
        responseCode = 404;        
      }
    }
    else
    {
        root["messageType"] = "alert";
        root["message"] = "Unable to parse or invalid parameter";
        responseCode = 404;
    }
    root.printTo(message);
    server.send( responseCode, "application/json", message);
  }
*/
/*
  void handleRoot()
  {
    String message;
    JsonObject& root = jsonBuffer.createObject();
    root["messageType"] = "status";
    if (isMoving) 
    	root["isMoving"] = "true";
    else 
    	root["isMoving"] = "false";
    root["currentFilterId"] = currentFilterId;
    root["currentFilter"] = filterNames[currentFilterId];
    root["targetFilterId"] = targetFilterId;
    root["currentPosition"] = stepPosition;
    root["stepsPerRevolution"] = stepsPerRevolution;
    root.printTo(message);
    server.send(200, "application/json", message);
    
    return;
  }
  */
