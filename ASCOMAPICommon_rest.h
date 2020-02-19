/*
File to be included into relevant device REST setup 
*/
//Assumes Use of ARDUINO ESP8266WebServer for entry handlers
#if !defined _ASCOMAPI_Common
#define _ASCOMAPI_Common
#include "JSONHelperFunctions.h"

//PUT /{DeviceType}/{DeviceNumber}/Action Invokes the specified device-specific action.
void handleAction(void);
//PUT /{DeviceType}/{DeviceNumber}/CommandBlind Transmits an arbitrary string to the device
void handleCommandBlind(void);
//PUT /{DeviceType}/{DeviceNumber}/CommandBool Transmits an arbitrary string to the device and returns a boolean value from the device.
void handleCommandBool(void);
//PUT /{DeviceType}/{DeviceNumber}/CommandString Transmits an arbitrary string to the device and returns a string value from the device
void handleCommandString(void);
//GET /{DeviceType}/{DeviceNumber}/Connected Retrieves the connected state of the device
//PUT /{DeviceType}/{DeviceNumber}/Connected Sets the connected state of the device
void handleConnected(void);
//GET /{DeviceType}/{DeviceNumber}/Description Device description
void handleDescriptionGet(void);
//GET /{DeviceType}/{DeviceNumber}/Driverinfo Device driver description
void handleDriverInfoGet(void);
//GET /{DeviceType}/{DeviceNumber}/DriverVersion Driver Version
void handleDriverVersionGet(void);
//GET /{DeviceType}/{DeviceNumber}/Name Device name
void handleNameGet(void);
void handleNamePut(void); //Non-ASCOM , required by setup
void handleFilterCountPut(void); 
//GET /{DeviceType}/{DeviceNumber}/SupportedActions Returns the list of action names supported by this driver.  
void handleSupportedActionsGet(void);

void handleAction(void)
{
    String message;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();
    
    if ( connectedClient != clientID) 
    {
      jsonResponseBuilder( root, clientID, transID, "Action", 0x400, "Action not available for 'not connected' client." );
      root["Value"]= "";
      root.printTo(message);
      server.send(400, "application/json", message);      
    }
    else
    {    
      jsonResponseBuilder( root, clientID, transID, "Action", 0x400, "Not implemented" );
      root["Value"]= "";
      root.printTo(message);
      server.send(200, "application/json", message);
    }
    return;
 }

void handleCommandBlind(void)
{
    String message;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);       
    JsonObject& root = jsonBuffer.createObject();
    if ( connectedClient != clientID) 
    {
      jsonResponseBuilder( root, clientID, transID, "Action", 0x400, "Action not available for 'not connected' client." );
      root["Value"]= "";
      root.printTo(message);
      server.send(400, "application/json", message);      
    }
    else
    {
      jsonResponseBuilder( root, clientID, transID, "Action", 0x400, "Not implemented" );
      root["Value"]= "";
      root.printTo(message);
      server.send(200, "application/json", message);
    }
    return;
}

void handleCommandBool(void)
{
    String message;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();
    if ( connectedClient != clientID) 
    {
      jsonResponseBuilder( root, clientID, transID, "Action", 0x400, "Action not available for 'not connected' client." );
      root["Value"]= "";
      root.printTo(message);
      server.send(400, "application/json", message);      
    }
    else
    {
      jsonResponseBuilder( root, clientID, transID, "CommandBool", 0x400, "Not implemented" );
      root["Value"]= false; 
      root.printTo(message);   
      server.send(200, "application/json", message);
    }
    return;
    
}

void handleCommandString(void)
{
    String message;
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
    
    if ( connectedClient != clientID) 
    {
      jsonResponseBuilder( root, clientID, transID, "Action", 0x400, "Action not available for 'not connected' client." );
      root["Value"]= "";
      root.printTo(message);
      server.send(200, "application/json", message);      
    }
    else
    {
      jsonResponseBuilder( root, clientID, transID, "CommandBool", 0x400, "Not implemented" );
      root["Value"]= false; 
      root.printTo(message);   
      server.send(200, "application/json", message);
    }
    return;
}

void handleConnected(void)
{
    String message;
    int outputCode = 200;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();
    
    if ( server.method() == HTTP_PUT )
    { 
       DEBUGSL1( "Entered handleConnected::PUT" );

      //dont like the logic here - if its already connected for this client we should refuse a connect. 
      if( server.arg("Connected") && server.arg("Connected").equalsIgnoreCase("true" ) )
      { //setting to true 
        if ( connected )//already true
        {
          if( clientID == connectedClient )
          {
          DEBUGSL1( "Entered handleConnected::PUT::True::already connected - benign error" );        
            //Check error numbers
            jsonResponseBuilder( root, clientID, transID, "Connected", 0, "Setting connected when already connected" );        
            root["Value"]= connected;    
            root.printTo(message);
            outputCode = 200;
          }
          else
          {
          DEBUGSL1( "Entered handleConnected::PUT::True::already connected but not by this client - error" );        
            //Check error numbers
            jsonResponseBuilder( root, clientID, transID, "Connected", 0x402, "Setting connected when already connected by different client" );        
            root["Value"]= connected;    
            root.printTo(message);
            outputCode = 400;            
          }
        }
        else //OK
        {  
          DEBUGSL1( "Entered handleConnected::PUT::True::setting connected - OK" );
          connected = true;
          connectedClient = clientID;
          jsonResponseBuilder( root, clientID, transID, "Connected", 0, "Setting connected OK" );        
          root["Value"]= false;    
          root.printTo(message);
          outputCode = 200;          
        }
      }
      else //set to false
      {
        if ( connected ) //
        {
          DEBUGSL1( "Entered handleConnected::PUT::False::set unconnected - OK" );
          connected = false; //OK   
          connectedClient = -1;       
          jsonResponseBuilder( root, clientID, transID, "Connected", 0, "Disconnected OK" );        
          root["Value"]= false;    
          root.printTo(message);
          outputCode = 200;
        }
        else
        {
          //Check error numbers
          DEBUGSL1( "Entered handleConnected::PUT::False::not already connected - error" );
          jsonResponseBuilder( root, clientID, transID, "Connected", 0x403, "Setting 'connected' to false when aready false" );        
          root["Value"]= false; 
          root.printTo(message);   
          outputCode = 400;
        }
      }
    }
    else if ( server.method() == HTTP_GET )
    {
      //Check error numbers
      jsonResponseBuilder( root, clientID, transID, "Connected", 0, "" );        
      root["Value"]= connected;
      root.printTo(message);
      outputCode = 200;
    }
    else
    {
      jsonResponseBuilder( root, clientID, transID, "Connected", 0x40B, "Unexpected request method" );        
      root["Value"]= connected;      
      root.printTo(message);
      outputCode = 200;
    }

   server.send( 200, "application/json", message);
   return;          

}

void handleDescriptionGet(void)
{
    String message;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();
    jsonResponseBuilder( root, clientID, transID, "Description", 0, "" );    
    root["Value"]= Description;    
    root.printTo(message);
    server.send(200, "application/json", message);
    return ;
}

void handleDriverInfoGet(void)
{
    String message;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();
    jsonResponseBuilder( root, clientID, transID, "DriverInfo", 0, "" );    
    root["Value"]= Description;    
    root.printTo(message);
    server.send(200, "application/json", message);
    return ;
}

void handleDriverVersionGet(void)
{
    String message;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();
    jsonResponseBuilder( root, clientID, transID, "DriverVersion", 0, "" );    
    root["Value"]= DriverVersion;    
    root.printTo(message);
    server.send(200, "application/json", message);
    return ;
}

void handleNameGet(void)
{
    String message;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();
    jsonResponseBuilder( root, clientID, transID, "Name", 0, "" );    
    root["Value"] = DriverName;    
    root.printTo(message);
    server.send(200, "application/json", message);
    return ;
}

void handleSupportedActionsGet(void)
{
    String message;
    uint32_t clientID = (uint32_t)server.arg("ClientID").toInt();
    uint32_t transID = (uint32_t)server.arg("ClientTransactionID").toInt();
    //JSON data formatter
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();
    jsonResponseBuilder( root, clientID, transID, "SupportedActions", 0, "" );    
    root["Value"]= "";    //Empty array until otherwise 
    root.printTo(message);
    server.send(200, "application/json", message);
    return ;
}
#endif
