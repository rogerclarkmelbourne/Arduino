/*
 * Simple web server using a TLN13UA06 UART Wifi module
 *
 * By Roger Clark
 *
 * May work with other similar UART wifi modules 
 *
 * This program acts as a very simple web server when the Arduino is connected to a UART Wifi module via RS232
 * 
 * The wifi module is assumed to be in "transparent" aka "auto mode", in which all comming TCP traffic e.g. on port 80 is sent to the Arduino the Serial port
 * By Serial I mean Software Serial, in this setting. However you can change the code to use Hardware Serial if you are using an Arduino that has a free 
 * Hardware Serial port, e.g. using a Mega 2560 etc
 *
 * Serial data sent to the module is returned to the browser in response to the GET request
 *
 * The main drawback with using "transparent" communications between the browser and the wifi module, is that there is no way to close the connection 
 * after the page has been set. 
 * (It may be possible to enter into command mode e.g. send +++ ,guess what socket the request came from, and close that socket, but its messy)
 * So this code builds HTTP 1.1 responses which have the content lengrh in the header. The browser then closes the connection after it has received the specified
 * content length.
 *
 * The main complication with using HTTP 1.1, is that the page length (in bytes) needs to be determined in advance of the page being constructed.
 * So I used a 2 pass strategy.
 * Pass 1 builds the page, but just returns the length, which is then sent in the header
 * Pass 2 builds the page and at the same time sends it to the specified serial port.
 * 
 * Using this approach means that the page does not need to exist in RAM, which is a scarse respouse on the Arduinno, and long pages e.g. 10k could be used.
 * The only limit is the size of the program memory.
 *
 * As static web pages are not much use to anyone. This sketch allows the creation of dynamic pages by using variable insertion/substitution .
 *
 * The delimiter \a is used to signifiy the start and send of a substitution, and the number between the substitution delimiter is the index into an array of
 *  variables (which are strings). Hence \a1\a  means insert variable[1] here.
 * The buildPage() function handles the insertion and calculation of the now variable page length, as it is passed both the static page (in progmem) and also 
 * the variables array.
 * To save code duplication, the buildpage() function can also send the page data directly to the serial port, if the serial paramater is non-null.
 *
 * The sketch parses the GET request to determine the requested page name, then determines if a matching name is in the pages array.
 * If a match to the requested page name is found, the correct page is returned.
 * If no match is found, the "index" page is returned
 * 
 * The sketch doesn't currently handle URL paramaters, the entire request path is considered to be the page name 
 * I'm currently working on a version which will handle GET request paramaters.
 *
 * This code comes with no warranties. Use at your own risk etc.
 */

//#include <SoftwareSerial.h>

//SoftwareSerial Serial1(10, 11); // RX, TX



// Defines used for the variables within the web pages
#define NUMBER_OF_VARIABLES 7
#define STRLEN_OF_VARIABLES 16
#define NUMBER_OF_PAGE_ARRAY_ELEMENTS 3

typedef enum  RequestState
{
  waiting,
  readingLine1,
  readingJunk
} ;


// Data of the web pages and their page names, are stored in PROGMEM
// Note the use of \a as a delimter for insertion of the number of the variables array index
// e.g. \a1\a  means insert variables[1] here.

// Simple index page, including variable insertion/ substitution 
#define PAGE_NAME_INDEX 0
#define PAGE_DATA_INDEX 1
#define PAGE_MIMETYPE_INDEX 2

// Use this in the head block if you want to test using a CSS file with your html file
// <link rel=\"stylesheet\" type=\"text/css\" href=\"s.css\">
// to test repeadly loading the page use <meta http-equiv=\"refresh\" content=\"5\">
prog_char index_htm[] PROGMEM = "<!DOCTYPE html><html>"
                              "<head>"
                             // "<link rel=\"stylesheet\" type=\"text/css\" href=\"s.css\">"
                             // "<meta http-equiv=\"refresh\" content=\"5\">"
                              "</head>" 
                              "<body><h3>Millis are \a0\a</h3>"
                              "<h3>Analog A0 = \a1\a <meter min=\"0\" max=\"1023\" value=\"\a1\a\"></meter></h3>"
                              "<h3>Analog A1 = \a2\a</h3>"
                              "<h3>Analog A2 = \a3\a</h3>"
                              "<h3>Analog A3 = \a4\a</h3>"   
                              "</body>";// This is the page data
prog_char index_pageName[] PROGMEM =  "index.htm";// This is the page name

// Same as index page except with meta refresh every 5 seconds to soak test the comms etc
prog_char getpin_htm[] PROGMEM = "<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"s.css\"></head>" 
                              "<body><h3>Pin \a0\a"                                   
                              "<body>";// This is the page data
prog_char getpin_pageName[] PROGMEM =  "get_pin.htm";// This is the page name

// A CSS page
prog_char style_css[] PROGMEM = " h3 { font-family:\"Arial\";color:blue}";// This is the data (css)
prog_char style_pageName[] PROGMEM =  "s.css";// This is the css file name

// A test (page) with JSON data (using variable insertion/ substitution
prog_char jsondata_pagename[] PROGMEM =  "jsondata.htm";// This is the page name
prog_char jsondata_json[] PROGMEM = "["
                                    "{\"name\": \"millis()\",\"value\": \a0\a},"
                                    "{\"name\": \"A0\",\"value\": \a1\a},"
                                    "{\"name\": \"A1\",\"value\": \a2\a},"
                                    "{\"name\": \"A2\",\"value\": \a3\a},"
                                    "{\"name\": \"A3\",\"value\": \a4\a},"
                                    "{\"name\": \"A4\",\"value\": \a5\a}"
                                    "]";

// Various useful mimetypes
prog_char mimetype_text_html[] PROGMEM =  "text/html";
prog_char mimetype_text_css[] PROGMEM =  "text/css";
prog_char mimetype_application_json[] PROGMEM =  "application/json";

// Code to make array of page(s) data
PROGMEM const char *pages[][NUMBER_OF_PAGE_ARRAY_ELEMENTS] =
{   
  {index_pageName,index_htm,mimetype_text_html},
  {getpin_pageName,getpin_htm,mimetype_text_html},
  {style_pageName,style_css,mimetype_text_css},
  {jsondata_pagename,jsondata_json,mimetype_application_json}
};

int RXLED = 17;  // The RX LED has a defined Arduino pin
boolean flashState;
long lastMillis;
RequestState requestState=waiting;

/*
 * This function has 2 uses
 * 1. Read the page data from PROGMEM and "Variables" in a ram array, and combine then and output to the module (connection to the module)
 * 2. If no stream is specified, the module only calculates the length of the page (total number of characters.
 *
 * Note. The function always calculates page length, regardless of whether it is outputting the page to the module
 * Also note. The function uses the ASCII escape character '\a' to denote start and end of variable insertion 
 * '\a' was used because its outside of the normal printable character range, but any unused ASCII character could be used
 * I initially was going to used '\x01' i.e ASCII code 1, however using hex code numbers didn't work as the compilor 
 * interpreted the variable number after code e.g. \0x011  as being 0x011 not 0x01 followed by a 1 
 * so using an escape sequence like \a worked better.
 * You could use other delimiters, but if these are more than one byte long the code gets more complex e.g "<?var 1 ?>"
 */

int buildPage(int pageIndex,char variables[NUMBER_OF_VARIABLES][STRLEN_OF_VARIABLES],Stream *oStream=((Stream *)0))
{
  char c;
  int l=0;
  boolean processingVariableName=false;
  char varIndex[4];
  char *varIndexPtr;
  char *page = (char*)pgm_read_word(&(pages[pageIndex][PAGE_DATA_INDEX]));//(char *)pages[pageIndex][1];
  
  do
  {
    c =  pgm_read_byte_near(page++);  
    
    if (!processingVariableName && c!='\a')
    {
      l++;
      if (oStream)
      {
        oStream->print(c);
      }
    }
 
    if (processingVariableName && c!='\a')
    {
      *varIndexPtr++=c;// save the index number
    }  
   
    if (c=='\a')
    {
      if (!processingVariableName)
      {
        // Start of processing a variable insertion
        varIndexPtr = varIndex;
      }
      else
      {
        // end of variable insertion found
        *varIndexPtr=0;// terminate the string
        l+=strlen(variables[atoi(varIndex)]);
        if (oStream)
        {
          oStream->print(variables[atoi(varIndex)]);
        } 
      }
      processingVariableName=!processingVariableName;
    }

  } while (c!=0);
  return --l;// deduct last null char
}

// Returns true if it finds the field (variable) otherwise returns false
// if True the returnBuffer contains the text of the field data
boolean searchQueryStringFor(char *queryString,char * variable, char *returnBuffer)
{
char *endPtr, *startPtr = strstr(queryString,variable);

  if (startPtr)
  {
    startPtr = strstr(startPtr,"=");
    if (!startPtr)
    {
      return false;// Bad data (no = sign)
    }
    
    endPtr=strstr(++startPtr,"&");
    if (endPtr)
    {
      strncpy(returnBuffer,startPtr,endPtr-startPtr-1);
      *(returnBuffer + (endPtr-startPtr-1))=0;// terminate the string
    }
    else
    {
      strcpy(returnBuffer,startPtr);
    }
    return true;
  }
  else
  {
    return false;
  }
}

/*
 * Function to determine what page is required
 * populate appropriate variables array
 */
void sendPageResponse(char *requestLine1,Stream *serialPort)
{
  char * pch;
  char * queryString;

  char variables[NUMBER_OF_VARIABLES][STRLEN_OF_VARIABLES];// Create some variables for insertion into the page
  prog_char *str=(prog_char *)0;// make str a null pointer as we test if its been set later.
  int foundPage=0;// Default to first page in the list if we don't find the actual page name by searching the page names array

// Setup all the variables that will be used on the page (Note make sure your array is big enough in the #define 
  sprintf(variables[0],"%ld",millis());// Put the value of the Millis in variable [0]
  sprintf(variables[1],"%d",analogRead(A0));// Put the value of analog 1 into variable [1]
  sprintf(variables[2],"%d",analogRead(A1));// etc
  sprintf(variables[3],"%d",analogRead(A2));//  etc
  sprintf(variables[4],"%d",analogRead(A3));//   etc
  sprintf(variables[5],"%d",analogRead(A4));//    etc
  sprintf(variables[6],"%d",analogRead(A5));//     etc 
  
  strtok (requestLine1," /");// find the first " /"
  pch = strtok (NULL, " ")+1;// get the text from after the last strtok to the next space, strip off the first character (this may result in a null string)
  
  queryString = strstr(pch,"?");// See if there are any GET contains a "query string"
  if (queryString)
  {
    char varBuf[8];// Allocate a small buffer when searching for query string fields
    *queryString++=0;
    // Process query
    if (searchQueryStringFor(queryString,"pin",varBuf))
    {
      int pin=atoi(varBuf);
      sprintf(variables[0],"%d",analogRead(atoi(varBuf)));
    }
  }
  
  if (*pch!=0)
  {
    //   Serial.println(pch);// Debug which page has been requested
    // Iterate through the pages list to find a name that matches
    for(int page=0;page < sizeof(pages)/(sizeof(PROGMEM const char *)*NUMBER_OF_PAGE_ARRAY_ELEMENTS);page++)
    {
      if (strcmp_P(pch,(char*)pgm_read_word(&(pages[page][PAGE_NAME_INDEX])))==0)
      {
        foundPage=page;
        break;
      }
    }
  }


// Send the response back to the module
  serialPort->println(F("HTTP/1.1 200 OK"));
  serialPort->print(F("Content-type: "));
  printP(serialPort,(char*)pgm_read_word(&(pages[foundPage][PAGE_MIMETYPE_INDEX])));
  serialPort->println();
  serialPort->print(F("Content-length: "));
  serialPort->println(buildPage(foundPage,variables),DEC);// pre build the page so we know its overall length including the variable substitution
  serialPort->println();
  buildPage(foundPage,variables,serialPort);// Send the actual page data including variable substitution
  Serial.print("Senresponse");
}

// Helper function. sends a PROGMEM String to the serial port (Used to send the mimetype)
void printP(Stream *serialPort,char * str)
{
char c;
   while((c = pgm_read_byte(str++)))
   {
     serialPort->write(c);
   }
}
/*
 * Function to fill (passed) receive buffer with data from the module until either the buffer is full or the pattern is matched
 * Note. Currently this function doesn't timeout.
 */
boolean waitPattern(char *receiveBuffer, int receiveBufferLen, char *thePattern,Stream *inStream)
{
char *patternPos = thePattern;
char c;

    receiveBufferLen--;// Allow space for zero termination
    
    while(*patternPos!=0 && receiveBufferLen>0)
    {
      if  (inStream->available() && receiveBufferLen-->0)
      {
        c=inStream->read();
        if (receiveBuffer)
        {
          *receiveBuffer++=c;
        }
        if (c==*patternPos)
        {
          patternPos++;
        }
        else
        {
          patternPos = thePattern;// reset the pattern position pointer back to the start of the pattern
        }
      }
      
      handleFlashLED();
    }
    
    if (*patternPos==0 && receiveBuffer)
    {
      *receiveBuffer=0;// terminate receive buffer
      return true; // found the pattern
    }
    else
    {
      return false;// did not find before we ran out of input buffer
    }
}

void handleFlashLED()
{
     if (millis() - lastMillis > 500)
     {
        lastMillis=millis();
        flashState = !flashState;
        digitalWrite(RXLED, flashState);   // set the LED on
/*
#if defined(USART1_RX_vect)
Serial.println("USART1_RX_vect");
#endif
*/
     }
}




void setup() 
{
char buf[64];// Assume the HTTP request doesnt have any single line greater than 63 including the \r\n

// initialize both serial ports:
  Serial.begin(115200);// Debug to PC
  Serial1.begin(115200);// This is serial port for the module
   Serial1.flush();
  pinMode(RXLED, OUTPUT);  // Set RX LED as an output
  flashState=LOW;
  digitalWrite(RXLED, flashState);   // set the LED on
  
  pinMode(2,OUTPUT);
  digitalWrite(2,LOW);
  delay(10);
  digitalWrite(2,HIGH);
  lastMillis=millis();
  

  

  Serial.println(F("Starting web server"));// Debug message

   while(1)
   {
    
     if (waitPattern(buf,sizeof(buf),"\r\n",&Serial1))// Assume that the request is a GET and its the first bit of data we receive
     {
      // Serial.print(F("Got buffer "));// Debug message
      // Serial.print(buf);// Debug message
      // Serial.print(F("Flushing "));// Debug message
        // Check if this is a GET request
        
        waitPattern((char *)0,999999,"\r\n\r\n",&Serial1);// purge all other incomming data from the browser until \r\n\r\n
        
        if (!strncmp(buf,"GET",3))
        {
          // Yes. Its a GET
        //  Serial.print("GET");
         //   Serial.println(buf);
           

            // flush the rest of the input buffer
            while(Serial1.available() && true)
            {
              Serial1.read();
            }
            Serial1.flush(); // flush both input an output. Mainly flushing the input, in case the module has not finished sending all data from the browser (client) before we finish sending the page back. (Probably not needed)

            sendPageResponse(buf,&Serial1);// Send response specific to the GET reuest


        }
     }  
   }

}

void serialEvent1()
{
  Serial.println("serial1Event");
}
void serialEvent()
{
  Serial.print("serialEvent");
}

void loop() {
  // Everything is done in setup ! Its probably faster !
}


