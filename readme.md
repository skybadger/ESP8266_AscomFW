 ASCOM ALPACA compliant native filterwheel driver, with ASCOM ALPACA header files for ASCOM common and ASCOM filter wheel functions. <br>
 Configure directly in ASCCOM Remote server or access directly via curl/web browser ( not many browsers support effective PUT requests) <br>
 
 
 Hardware: REST Astronomical filter wheel stepper controller using ESP8266 and A4988 controller. Motor I use is a 48:20:1 stepper which is disabled for power when not turning due to the heat.<br>
 
 Operating: <br>
 All Can_ requests don't need the requesting client to be the one that set connected to be 'true' <br>
 All functions that change the state of the filter wheel check that the requestor is the one that has set connected to true.<br>
 The filter wheel will not respond to other clients until connected has been set to false by the one currently in control.<br>
 
 Testing:
 All urls in lower case. 
 All properties need case-matching - typically Upper Camel Case. Until fixed. 
 curl -X get http://espFwl01/api/v1/filterwheel/0/
 curl -X put http://espFwl01/filterwheel/0/ -d "ClientId=1&TransactionId=2&position=3" (quotes are needed to protect '&' from windows command line parser)
 
 Setup webform: http://espFwl01/FilterWheel/0/ 
 
 ASCOM pages: https://ascom-standards.org <br>
 ALPACA ASCOM api pages: https://ascom-standards.org/api <br>
 ALPACA ASCOM coding standards pages: https://github.com/ASCOMInitiative/ASCOMRemote/blob/master/Documentation/ASCOM%20Alpaca%20API%20Reference.docx <br>
 ALPACA Git pages: https://github.com/ASCOMInitiative/ASCOMRemote/ <br>
 Home pages : https://www.skybadger.net <br>
 
 
 

