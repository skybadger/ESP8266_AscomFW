#if !defined _FWEEPROM_H_
#define _FWEEPROM_H_
void setDefaults(void );
void saveToEeprom( void );
void setupFromEeprom( void );

void setDefaults()
{
  int i=0;
  DEBUGSL1( "setDefaults: entered");
  
  //hostname, wheelname is assumed to be the same as hostname
  hostname = (char*) calloc( sizeof(char), MAX_NAME_LENGTH );
  memcpy( hostname, defaultHostname, MAX_NAME_LENGTH * sizeof(char) );
  DEBUGS1( "hostname:  ");DEBUGSL1( hostname );
  
  wheelName = (char*) calloc( sizeof(char), MAX_NAME_LENGTH );
  memcpy( wheelName, defaultHostname, MAX_NAME_LENGTH * sizeof(char) );
  DEBUGS1( "wheelName:  ");DEBUGSL1(  wheelName );

  thisID = (char*) calloc( sizeof(char), MAX_NAME_LENGTH );
  memcpy( thisID, defaultHostname, MAX_NAME_LENGTH * sizeof(char) );
  DEBUGS1( "MQTT ID:  ");DEBUGSL1( thisID );
    
  //filternames and filter offsets
  filterNames = (char**) calloc( sizeof(char*), (unsigned) defaultFiltersPerWheel );
  focusOffsets = (int*)  calloc( sizeof(int),   (unsigned) defaultFiltersPerWheel );
  filterPositions = (int*) calloc( sizeof(int), (unsigned) defaultFiltersPerWheel );
  for ( i=0; i< defaultFiltersPerWheel ; i++ )
  {
    filterNames[i] = (char*) malloc( sizeof(char) * MAX_NAME_LENGTH);
    focusOffsets[i] = 0;
    filterPositions[i] = i* ((2048)/defaultFiltersPerWheel);
    String thing = "filter_";
    thing.concat(i);
    memcpy( filterNames[i], thing.c_str(), thing.length() * sizeof(char) );
    DEBUGS1( "setDefaults: filterNames ");
    DEBUGSL1(  filterNames[i] );
    DEBUGS1( "setDefaults: filterOffsets ");
    DEBUGSL1( focusOffsets[i] );
    DEBUGS1( "setDefaults: filterPositions: ");
    DEBUGSL1( filterPositions[i] );

  }
  
 DEBUGSL1( "setDefaults: exiting" );
}

void saveToEeprom( void )
{
  int i = 0;
  int eepromAddr = 1;

  DEBUGSL1( "savetoEeprom: Entered ");
  
  //hostname
  EEPROMWriteString( eepromAddr = 1, hostname, MAX_NAME_LENGTH );
  eepromAddr += MAX_NAME_LENGTH;  
  DEBUGS1( "Written hostname: ");DEBUGSL1( hostname );

  //wheel name
  EEPROMWriteString( eepromAddr, wheelName, MAX_NAME_LENGTH );
  eepromAddr += MAX_NAME_LENGTH;
  DEBUGS1( "Written wheelName: ");DEBUGSL1( wheelName );
  
  //current filter selected
  EEPROMWriteAnything( eepromAddr, currentFilterId ); 
  eepromAddr += sizeof( currentFilterId );
  DEBUGS1( "Written currentFilterId: ");DEBUGSL1( currentFilterId );
  
  //number of filters
  EEPROMWriteAnything( eepromAddr, filtersPerWheel ); 
  eepromAddr += sizeof(filtersPerWheel);
  DEBUGS1( "Written filtersPerWheel: "); DEBUGSL1( filtersPerWheel );
  
  //positions
  for ( i=0; i < filtersPerWheel; i++ )
  {
     EEPROMWriteAnything( eepromAddr, filterPositions[i] ); 
     eepromAddr += sizeof(filterPositions[i]);
     DEBUGS1( "Written filterPositions[]: "); DEBUGSL1( filterPositions[i] );
  }
  
  //focus offsets
  for ( i=0; i < filtersPerWheel; i++ )
  {
    EEPROMWriteAnything( eepromAddr, focusOffsets[i] ); 
    eepromAddr += sizeof(focusOffsets[i]);
    DEBUGS1( "Written focusOffsets[]: ");DEBUGSL1( focusOffsets[i] );
  }
  
  //filtername length/filter name - handle local vs global string references. Could go out of scope.
  for ( i=0; i < filtersPerWheel; i++ )
  {
     EEPROMWriteString( eepromAddr, filterNames[i], MAX_NAME_LENGTH ); 
     eepromAddr += (MAX_NAME_LENGTH * sizeof(char) );
     DEBUGS1( "Written filterNames[]: ");DEBUGSL1( filterNames[i] );
  }
  
  EEPROMWriteAnything( eepromAddr=0, byte('#') );
  DEBUGSL1( "saveToEeprom: exiting ");

  EEPROM.commit();

  //Test readback of contents
  char ch;
  String input;
  for (i=0; i< 512 ; i++ )
  {
    ch = (char) EEPROM.read( i );
    if ( ch == '\0' )
      ch = '_';
    input.concat( ch );
  }

  Serial.printf( "EEPROM contents: %s \n", input.c_str() );
}

void setupFromEeprom()
{
  int eepromAddr = 0;
  byte bTemp = 0;
  int i=0;
    
  DEBUGSL1( "setUpFromEeprom: Entering ");
  
  //Setup internal variables - read from EEPROM.
  bTemp = EEPROM.read( eepromAddr );
  DEBUGS1( "Read init byte: ");DEBUGSL1( (char) bTemp );
  if ( (byte) bTemp != '#' ) //initialise
  {
    setDefaults();
    saveToEeprom();
    DEBUGSL1( "Failed to find init byte - wrote defaults ");
    return;
  }    
    
  //hostname - directly into variable array 
  if( hostname != NULL ) free (hostname);
  hostname = (char*) calloc( sizeof(char), MAX_NAME_LENGTH );
  i = EEPROMReadString( eepromAddr=1, hostname, MAX_NAME_LENGTH );
  eepromAddr += MAX_NAME_LENGTH;
  DEBUGS1( "Read hostname: ");DEBUGSL1( hostname );

  //MQTT ID  - copy hostname
  if( thisID != NULL ) free (thisID);
  thisID = (char*) calloc( sizeof(char), MAX_NAME_LENGTH );
  strcpy( thisID, hostname );
  DEBUGS1( "Read MQTT ID: ");DEBUGSL1( thisID );

  //wheel name 
  if( wheelName != NULL ) free (wheelName);
  wheelName = (char*) calloc( sizeof(char), MAX_NAME_LENGTH );
  i = EEPROMReadString( eepromAddr, wheelName, MAX_NAME_LENGTH );
  eepromAddr += MAX_NAME_LENGTH;
  DEBUGS1( "Read wheelName: ");DEBUGSL1( wheelName );
  
  //current filter id
  EEPROMReadAnything( eepromAddr, currentFilterId ); 
  eepromAddr += sizeof(currentFilterId);
  DEBUGS1( "Read currentFilterId: ");DEBUGSL1( currentFilterId );
  
  //number of filters
  EEPROMReadAnything( eepromAddr, filtersPerWheel ); 
  eepromAddr += sizeof( filtersPerWheel );
  if( filtersPerWheel >12 )
    filtersPerWheel = 5; //default
  DEBUGS1( "Read filtersPerWheel: ");DEBUGSL1( filtersPerWheel );
  
  //positions
  if( filterPositions != NULL) free( filterPositions);
  filterPositions = (int*) calloc( filtersPerWheel, sizeof(int) );  
  for ( i=0; i < filtersPerWheel; i++ )
  {
     EEPROMReadAnything( eepromAddr, filterPositions[i] ); 
     eepromAddr += sizeof( filterPositions[i] );
     DEBUGS1( "Read filterPositions[] ");DEBUGSL1( filterPositions[i] );
  }
  
  //stepPosition - should always be at the step Position for the current filter ID. 
  stepPosition = filterPositions[currentFilterId];
  targetFilterId = currentFilterId;
      
  //focus offsets
  if( focusOffsets != NULL ) free(focusOffsets);
  focusOffsets = (int*) calloc( filtersPerWheel, sizeof(int) );  
  for ( i=0; i < filtersPerWheel; i++ )
  {
     EEPROMReadAnything( eepromAddr, focusOffsets[i] ); 
     eepromAddr += sizeof(focusOffsets[i]);
     DEBUGS1( "Read focusOffsets[] ");DEBUGSL1( focusOffsets[i] );
  }
  
  //filtername length/filter name 
  if ( filterNames != NULL ) 
  {
    for ( i=0; filtersPerWheel; i++ )
    {
      if( filterNames[i] != NULL ) 
        free( filterNames[i]);
    }
    free( filterNames);
  }
  filterNames = (char**) calloc( sizeof(char*), filtersPerWheel );
  for ( i=0; i < filtersPerWheel; i++ )
  {
     filterNames[i] = (char*) calloc( sizeof(char), MAX_NAME_LENGTH );
     EEPROMReadString( eepromAddr, filterNames[i], MAX_NAME_LENGTH  );
     eepromAddr += (MAX_NAME_LENGTH * sizeof(char));
     DEBUGS1( "Read filterNames[] ");DEBUGSL1( filterNames[i] );
  }
 DEBUGS1( "setupFromEeprom: exiting having read " );DEBUGS1( eepromAddr );DEBUGSL1( " bytes." );
}
#endif
