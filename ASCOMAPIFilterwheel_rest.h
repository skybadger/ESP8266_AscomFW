/*
File to be included into relevant device REST setup 
*/
//Assumes Use of ARDUINO ESP8266WebServer for entry handlers
#if !defined _ASCOM_Filterwheel 
#define _ASCOM_Filterwheel
#include "JSONHelperFunctions.h"
//extern String wheelName;

//GET /FilterWheel/{DeviceNumber}/FocusOffsets Filter focus offsets
void handleFocusOffsetsGet(void);

//GET /FilterWheel/{DeviceNumber}/Names Filter wheel filter names
void handleFilterNamesGet(void);

//GET /FilterWheel/{DeviceNumber}/Position Returns the current filter wheel position
void handlePositionGet(void);

//PUT /FilterWheel/{DeviceNumber}/Position Sets the filter wheel position
void handlePositionPut(void);

//Additional handlers used for setup webpage.
void handleSetup(void);
void handleFocusOffsetsPut( void );
void handleFilterNamesPut( void );
void handleHostnamePut( void );
void handleNamePut( void );
void handleFilterCountPut( void );

//Local functions
String& setupFormBuilder( String& htmlForm, String& errMsg );

void handleFocusOffsetsGet(void)
{
    String message;
    int i = 0;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);

    JsonObject& root = jsonBuffer.createObject();
    jsonResponseBuilder( root, clientID, transID, "FocusOffsets", 0, "" );    
    JsonArray& offsets = root.createNestedArray("Value");
    for ( i=0; i<filtersPerWheel ;i++ )
      offsets.add( focusOffsets[i]);
    root.printTo(message);
    server.send(200, "text/json", message);
    return ;
}

void handleFilterNamesGet(void)
{
    String message;
    int i = 0;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();

    DEBUGSL1( "Entered handleFilterNamesGet" );
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);
    
    JsonObject& root = jsonBuffer.createObject();
    JsonArray& names = root.createNestedArray("Value");
    jsonResponseBuilder( root, clientID, transID, "FilterNames", 0, "" );    
    for ( i=0;  i < filtersPerWheel ;i++ )
      names.add( filterNames[i]);
    root.printTo(message);
    server.send(200, "text/json", message);
    return ;  
}

void handlePositionGet(void)
{
    String message;
    String strPosition;
    int filterID = currentFilterId;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();

    DEBUGSL1( "Entered handlePositionGet" );
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);
    
    JsonObject& root = jsonBuffer.createObject();
    jsonResponseBuilder( root, clientID, transID, "Position get", 0, "" );    
    root["Value"] = currentFilterId;
    root.printTo(message);
    server.send(200, "text/json", message);
    return ;    
}

void handlePositionPut(void)
{
    String message;
    String strPosition;
    int responseCode = 200;
    int filterId = currentFilterId;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();

    DEBUGSL1( "Entered handlePositionPut" );
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);

    JsonObject& root = jsonBuffer.createObject();
    if( isMoving )
    {
      responseCode = 400;
      jsonResponseBuilder( root, clientID, transID, "Position put", 0x402, "Filter wheel already moving" );    
    }
    else if( server.hasArg("Position") )
    {
      strPosition = server.arg("Position");
      filterId = strPosition.toInt();      
      jsonResponseBuilder( root, clientID, transID, "Position", 0, "" );    
      if( filterId >= 0 && filterId < filtersPerWheel && filterId != currentFilterId )
        targetFilterId = filterId;
      responseCode = 200;
    }
    else
    {
      responseCode = 400;
      jsonResponseBuilder( root, clientID, transID, "Position", 0x402, "Position argument invalidc" );    
    }
    root.printTo(message);
    server.send(responseCode, "application/json", message);
    return;    
}

//Non ASCOM functions
/*
 * String& setupFormBuilder( String& htmlForm )
 */
 void handleSetup(void)
 {
  String output = "";
  String err = "";
  output = setupFormBuilder( output, err );
  server.send(200, "text/html", output );
  return;
 }

//server.on("/FilterWheel/*/Hostname", HTTP_PUT, handleHostnamePut );
void handleHostnamePut( void ) 
{
  int i=0;
  String form;
  String errMsg;
  int namesFoundCount = 0;
  String newName;
  
  debugURI( errMsg );
  DEBUGSL1 (errMsg);
  DEBUGSL1( "Entered handleHostnamePut" );
  
  //throw error message
  if( server.hasArg("hostname"))
  {
    newName = server.arg("hostname");
    DEBUGS1( "new hostname:" );DEBUGSL1( newName );
  }
  if( newName != NULL && newName.length() != 0 &&  newName.length() < MAX_NAME_LENGTH )
  {
    //save new hostname and cause reboot - requires eeprom read at setup to be in place.  
    strcpy( hostname, newName.c_str() );
    server.send( 200, "text/html", "rebooting!" ); 

    //Write to EEprom
    saveToEeprom();
    device.restart();
  }
  else
  {
    errMsg = "handleHostnamePut: Error handling new hostname";
    DEBUGSL1( errMsg );
    form = setupFormBuilder( form, errMsg );
    server.send( 200, "text/html", form ); 
  }
}

//server.on("/FilterWheel/*/Hostname", HTTP_PUT, handleHostnamePut );
void handleFilterCountPut( void ) 
{
  int i=0;
  String form;
  String errMsg;
  int newfiltercount = 0;
  String newName;

  debugURI( errMsg );
  DEBUGSL1 (errMsg);
  DEBUGSL1( "Entered handlefiltercountPut" );
  
  //throw error message
  if( server.hasArg("filtersPerWheel"))
  {
    newfiltercount = server.arg("filtersPerWheel").toInt();
    DEBUGS1( "new filtercount:" );DEBUGSL1( newfiltercount );
  }

 /*
 * if new filter count > old
 *  copy all the old into the new and add default names
 * if new filter count < old
 *  copy the reduced set of existing filter names in.
 * 
 */
  if ( newfiltercount != filtersPerWheel && newfiltercount > 0 && newfiltercount < MAX_FILTER_COUNT )
  {
    DEBUGS1( "Re-sizing & re-allocating filter array" );DEBUGSL1( newfiltercount );
    //Manage the filter names & memory allocation
    char** newFilterNames = (char**) calloc( sizeof(char**), (size_t) newfiltercount );
    int* newFilterOffsets = (int*) calloc( sizeof(int), (size_t) newfiltercount );
    int* newFilterPositions = (int*) calloc( sizeof(int), (size_t) newfiltercount );
    
    if( newfiltercount < filtersPerWheel )
    {
      for ( i=0; i< newfiltercount; i++ ) //loop and copy the existing ones into the new array
      {
          newFilterNames[i] = (char*) calloc( sizeof( char ), (size_t) MAX_NAME_LENGTH );
          memcpy ( newFilterNames[i], filterNames[i], MAX_NAME_LENGTH );
          free ( filterNames[i] ); 
          newFilterOffsets[i] = focusOffsets[i];          
      }
      //Clear down whats left
      for ( ; i < filtersPerWheel; i++ )
      {
        filterNames[i] = NULL;
        free ( filterNames );        
      }
    }
    else //newfiltercount > old 
    {
      //replace the old
      for ( i=0; i < filtersPerWheel; i++ ) //loop and copy the existing ones into the new array
      {
          newFilterNames[i] = (char*) calloc( sizeof( char ), MAX_NAME_LENGTH );
          strcpy ( newFilterNames[i], filterNames[i] );
          free ( filterNames[i] ); 
          newFilterOffsets[i] = focusOffsets[i];          
      }
      //provide defaults for the additional
      for( ; i< newfiltercount; i++ )
      {
          String temp = String("filter_");
          temp.concat( String(i) );
          
          newFilterNames[i] = (char*) calloc( sizeof( char ), (size_t) MAX_NAME_LENGTH );
          strcpy ( newFilterNames[i], temp.c_str() );
          newFilterOffsets[i] = 0;          
      }
    }
    
    //Update positions
    for ( i=0; i< newfiltercount; i++)
    {
      newFilterPositions[i] = i* stepsPerRevolution/newfiltercount;
    }
    
    //Clear the old memory down
    free( filterNames );
    free( focusOffsets );
    free( filterPositions );
    
    //update the globals 
    filtersPerWheel = newfiltercount;
    filterNames = newFilterNames;  
    focusOffsets = newFilterOffsets;
    filterPositions = newFilterPositions;//TODO
    
    //Write to EEprom
    saveToEeprom();
  }
  else
  {
    errMsg = "handlefiltercountPut: Error handling new filtercount";
    DEBUGSL1( errMsg );
    form = setupFormBuilder( form, errMsg );
    server.send( 200, "text/html", form ); 
  }
  DEBUGSL1( "Exiting handlefiltercountPut" );
}

//  server.on("/FilterWheel/*/Wheelname", HTTP_PUT, handleNamePut );
void handleNamePut( void) 
{
  int i=0;
  String form;
  String errMsg;
  int namesFoundCount = 0;
  String newName;
  bool success = false;
  
  debugURI( errMsg );
  DEBUGSL1 (errMsg);
  DEBUGSL1( "Entered handlewheelnamePut" );
  
  if( server.hasArg("wheelname"))
  {
    newName = server.arg("wheelname");
    DEBUGS1( "new wheelname:" );DEBUGSL1( newName );
    if( newName != NULL && newName.length() != 0 &&  newName.length() < MAX_NAME_LENGTH )
    {
      //save new hostname and cause reboot - requires eeprom read at setup to be in place.  
      strcpy( wheelName, newName.c_str() );
      DEBUGS1( "new wheelname:" );DEBUGSL1( wheelName );
      //Write to EEprom
      saveToEeprom();
      
      errMsg = "";
      form = setupFormBuilder( form, errMsg );
      server.send( 200, "text/html", form ); 
      success = true;
    }
  }
  if ( !success ) 
  {
    DEBUGSL1( errMsg );
    errMsg = "handlenamePut: Error handling new wheel name";
    form = setupFormBuilder( form, errMsg );
    server.send( 200, "text/html", form ); 
  }
  return;
}

/*
 * Set filternames from setup web page - managed outside of ascom  
 * REST api provides no way of doing this at time of writing. 
 */
void handleFilterNamesPut(void)
{
  int i=0;
  const String nameStub = "filterName";
  String name;
  String form;
  String localName;
  String errMsg;
  int namesFoundCount = 0;
  
  debugURI( errMsg );
  DEBUGSL1 (errMsg);
  DEBUGSL1( "Entered handleFilterNamesPut");
  do
  {
    name = nameStub + i;
    if( server.hasArg(name) )
    {
      localName = server.arg(name);
      //I don't care if you call two filters the same... 
      if ( localName != NULL && localName.length() != 0 && localName.length() < MAX_NAME_LENGTH )
      {
        memcpy( filterNames[i], localName.c_str(), localName.length() * sizeof(char)  );
        namesFoundCount++;
      }
    }  
  }while ( i < filtersPerWheel );
  
  if ( namesFoundCount != filtersPerWheel )
  {
    //throw error message
    errMsg = "UpdateFilterNames:Not enough named filternames found in request body";
    DEBUGSL1( "Not enough named filternames found");
    form = setupFormBuilder( form, errMsg );
    server.send( 200, "text/html", form );
  }
  else
  {
    errMsg = String("");
    form = setupFormBuilder( form, errMsg );
    server.send( 200, "text/html", form );
  }
  DEBUGSL1( "Exited handleFilterNamesPut");
  return;
}

void handleFocusOffsetsPut(void)
{
  int i=0;
  const String nameStub = "filterOffset";
  String name;
  String errMsg;
  String form;
  int namesFoundCount = 0;
  int localOffset = 0;
  
  debugURI(errMsg);
  DEBUGSL1 (errMsg);
  DEBUGSL1( "Entered handleOffsetsPut" );
  for ( i= 0; i< filtersPerWheel; i++ )
  {
    name = nameStub + i;
    if( server.hasArg(name) )
    {
      localOffset = server.arg(name).toInt();
      if ( localOffset < stepsPerRevolution && localOffset >= 0 )
      {
        focusOffsets[i] = localOffset;
        namesFoundCount++;
      }
    }  
  }
  
  if ( namesFoundCount != filtersPerWheel )
  {
    //throw error message
    String errMsg = "Update filter names:Not enough filter offsets found in request body";
    DEBUGSL1( "Update filter names:Not enough named filter offsets found");
    form = setupFormBuilder( form, errMsg );
    server.send( 200, "text/html", form );
  }
  else
  {
    errMsg = String("");
    form = setupFormBuilder( form, errMsg );  
    server.send( 200, "text/html", form );
  }
  DEBUGSL1( "Exited handleFilteroffsetsPut");
  return;
}

/*
 * Handler for setup dialog - issue call to <hostname>/setup and receive a webpage
 * Fill in form and submit and handler for each form button will store the variables and return the same page.
 optimise to something like this:
 https://circuits4you.com/2018/02/04/esp8266-ajax-update-part-of-web-page-without-refreshing/
 Bear in mind HTML standard doesn't support use of PUT in forms and changes it to GET so arguments get sent in plain sight as 
 part of the URL.
 */
String& setupFormBuilder( String& htmlForm, String& errMsg )
{
  int i=0;
  
  htmlForm = "<html><head></head><meta></meta><body>\n";
  if( errMsg != NULL && errMsg.length() > 0 ) 
  {
    htmlForm +="<div class=\"errorHeader\" bgcolor='A02222'><b>";
    htmlForm.concat( errMsg );
    htmlForm += "</b></div>";
  }
  //Hostname
  htmlForm += "<h1> Enter new hostname for filter wheel</h1>\n";
  htmlForm += "<form action=\"http://";
  htmlForm.concat( hostname );
  htmlForm += "/filterwheel/0/Hostname\" method=\"PUT\" id=\"hostname\" >\n";
  htmlForm += "Changing the hostname will cause the filter to reboot and may change the address!\n<br>";
  htmlForm += "<input type=\"text\" name=\"hostname\" value=\"";
  htmlForm.concat( hostname );
  htmlForm += "\">\n";
  htmlForm += "<input type=\"submit\" value=\"submit\">\n</form>\n";
  
  //Wheelname
  htmlForm += "<h1> Enter new descriptive name for filter wheel</h1>\n";
  htmlForm += "<form action=\"http://";
  htmlForm.concat( wheelName );
  htmlForm += "/filterwheel/0/Wheelname\" method=\"PUT\" id=\"wheelname\" >\n";
  htmlForm += "<input type=\"text\" name=\"wheelname\" value=\"";
  htmlForm.concat( wheelName );
  htmlForm += "\">\n";
  htmlForm += "<input type=\"submit\" value=\"submit\">\n</form>\n";
  
  //Number of filters
  htmlForm += "<h1> Enter number of filters in wheel</h1>\n";
  htmlForm += "<form action=\"http://";
  htmlForm.concat(hostname);
  htmlForm += "/filterwheel/0/FilterCount\" method=\"PUT\" id=\"filtercount\" >\n";
  htmlForm += "<input type=\"text\" name=\"filtersPerWheel\" value=\"";
  htmlForm.concat( filtersPerWheel );
  htmlForm += "\">\n";
  htmlForm += "<input type=\"submit\" value=\"submit\">\n</form>\n";
  
  //Filter names by position
  htmlForm += "<h1> Enter filter name for each filter </h1>\n";
  htmlForm += "<form action=\"http://";
  htmlForm.concat(hostname);
  htmlForm += "/filterwheel/0/FilterNames\" method=\"PUT\" id=\"filternames\" >\n";
  htmlForm += "<ol>\n";
  for ( i=0; i< filtersPerWheel; i++ )
  {
    htmlForm += "<li>Filter name <input type=\"text\" name=\"filtername_";
    htmlForm.concat( i);
    htmlForm += "\" value=\"" + String(filterNames[i]) + "\"></li>\n";
  }
  htmlForm += "</ol>\n";
  htmlForm += "<input type=\"submit\" value=\"submit\">\n</form>\n";
  
  //Filter focus offsets by position
  htmlForm += "<h1> Enter focuser offset for each filter </h1>\n";
  htmlForm += "<form action=\"http://";
  htmlForm.concat(hostname);
  htmlForm += "/filterwheel/0/FocusOffsets\" method=\"PUT\" id=\"offsets\">\n";
  htmlForm += "<ol>\n";
  for ( i=0; i< filtersPerWheel; i++ )
  {
    htmlForm += "<li> Filter <input type=\"text\" name=\"focusoffset_";
    htmlForm.concat(i);
    htmlForm += "\" value=\"";
    htmlForm.concat( focusOffsets[i] );
    htmlForm += "\"></li>\n";
  }
  htmlForm += "</ol>\n";
  htmlForm += "<input type=\"submit\" value=\"submit\">\n</form>\n";
  htmlForm += "</body>\n</html>\n";

/*  source html used.
<html>
<head></head>
<meta></meta>
<body>
<h1> Enter filter Name for each filter </h1>
<form action="ESPFwl01/Names" id="Names">
<ol>
<li>Filter Name <input type="text" name="filterName0" value="L" ></li>
<li>Filter Name <input type="text" name="filterName1" value="B" ></li>
<li>Filter Name <input type="text" name="filterName2" value="G" ></li>
<li>Filter Name <input type="text" name="filterName3" value="R" ></li>
<li>Filter Name <input type="text" name="filterName4" value="C" ></li>
</ol>
<input type="submit" value="submit" >
</form>

<h1> Enter focus offset for each filter </h1>
<form action="ESPFwl01/Offsets" id="Offsets">
<ol>
<li>Filter Offset <input type="number" name="filterOffset0" value="0" min="0" max="2048"></li>
<li>Filter Offset <input type="number" name="filterOffset1" value=20 min="0" max="2048" > </li>
<li>Filter Offset <input type="number" name="filterOffset2" value=2048*2/5 min="0" max="2048" ></li>
<li>Filter Offset <input type="number" name="filterOffset3" value=2048*3/5 min="0" max="2048" ></li>
<li>Filter Offset <input type="number" name="filterOffset4" value=2048*4/5 min="0" max="2048"  ></li>
</ol>
<input type="submit" value="submit">
</form>
</body>
</html>
*/
 return htmlForm;
}
#endif
