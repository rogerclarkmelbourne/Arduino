// Demo of web scraping
// By Roger Clark.
// 2012/11/08
// http://opensource.org/licenses/mit-license.php
//
// This demo retrieves 3 items (strings) of weather data and prints them to the console.
// More items could be added by extending the state machine code in the my_callback function
// State 999 is used when all data has been found to terminate futher packets being received (and the callback being called unnecessarily)
// Notes.
// * This code was written as a demostration and is not heavily optimised. Code size could be reduced.
// * The example assumes that the order of the multiple items of data on the web page will always appear in the same order.
//   If this is not the case, a more complex search system would need to be written.
// * Normal issues applying to web page "scraping" ( http://en.wikipedia.org/wiki/Web_scraping ) apply to this code.
// * The code does not do extensive error checking. For robust applications more error checking should be applied.
 
 
#include "EtherCard.h"
//#define DEBUG 1
 
// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
char website[] PROGMEM = "www.weather.com";
byte Ethernet::buffer[700];
static uint32_t timer;
 
// Patterns and pattern buffer pointer
char *searchPattern[] = { "<meta property=\"og:description\" content=\"",
                          "<span class=\"wx-temp\">",
                          "<span class=\"wx-temp\" itemprop=\"visibility-mile\">"};
char *searchPatternProgressPtr;
 
// Output bugger and pointer to the buffer
char displayBuff[64];
char *outputBufferPtr;
 
int foundPatternState;// Initialsed in loop()
 
 
// Utility functions
void removeSubstring(char *s,const char *toremove)
{
  while( s=strstr(s,toremove) )
  {
    memmove(s,s+strlen(toremove),1+strlen(s+strlen(toremove)));
  }
}
 
 
// Function to find a string in a buffer, and return a pointer to the first char after the end of the match
// Returns pointer to NULL if a pattern is not found
// This function is designed to fing the search string across multiple buffers, and uses the patternInProgress pinter to store partial pattern matches
// The calling code needs to maintain patternInProgress between subsequent calls to the function.
char *multiBufferFindPattern(char *buffer,char *searchString,char *patternInProgress)
{
    while (*buffer && *patternInProgress)
    {
        if (*buffer == *patternInProgress)
        {
            patternInProgress++;
        }
        else
        {
            patternInProgress=searchString;// reset to start of the pattern
        }
        buffer++;
    }
    if (!*patternInProgress)
    {
        return buffer;
    }
    return NULL;
}
 
int getData(char *inputBuffer, char *outputBuffPtr, char endMarker)
{
    while(*inputBuffer && *inputBuffer!=endMarker && *outputBuffPtr)
    {
        *outputBuffPtr=*inputBuffer;
        outputBuffPtr++;
        inputBuffer++;
 
    }
    if (*inputBuffer==endMarker && *outputBuffPtr!=0)
    {
        *outputBuffPtr=0;
        // end character found
        return 1;
    }
    else
    {
      return 0;
    }
}
 
 
// Called for each packet of returned data from the call to browseUrl (as persistent mode is set just before the call to browseUrl)
static void browseUrlCallback (byte status, word off, word len)
{
   char *pos;// used for buffer searching
   pos=(char *)(Ethernet::buffer+off);
   Ethernet::buffer[off+len] = 0;// set the byte after the end of the buffer to zero to act as an end marker (also handy for displaying the buffer as a string)
 
   //Serial.println(pos);
   if (foundPatternState==0)
   {
     // initialise pattern search pointers
     searchPatternProgressPtr=searchPattern[0];
     foundPatternState=1;
   }
  
   if (foundPatternState==1)
   {
       pos = multiBufferFindPattern(pos,searchPattern[0],searchPatternProgressPtr);
       if (pos)
       {
         foundPatternState=2;
         outputBufferPtr=displayBuff;
         memset(displayBuff,'0',sizeof(displayBuff));// clear the output display buffer
         displayBuff[sizeof(displayBuff)-1]=0;//end of buffer marker
       } 
       else
       {
         return;// Need to wait for next buffer, so just return to save processing the other if states
       }
   }    
  
   if (foundPatternState==2)
   {
     if (getData(pos,outputBufferPtr,'"'))
     {
          //Serial.print("Weather is ");
          removeSubstring(displayBuff,"&deg;");// Use utility function to remove unwanted characters
          Serial.println(displayBuff); 
          foundPatternState=3; 
     }
     else
     {
       // end marker is not found, stay in same findPatternState and when the callback is called with the next packet of data, outputBufferPtr will continue where it left off
     }
   }
  if (foundPatternState==3)
   {
        searchPatternProgressPtr=searchPattern[1];
        foundPatternState=4;
   }
   if (foundPatternState==4)
   {
       pos = multiBufferFindPattern(pos,searchPattern[1],searchPatternProgressPtr);
       if (pos)
       {
         foundPatternState=5;
         outputBufferPtr=displayBuff; // Reset outbutBuffertPtr ready to receive new data
         memset(displayBuff,'0',sizeof(displayBuff));// clear the output display buffer
         displayBuff[sizeof(displayBuff)-1]=0;//end of buffer marker
       } 
       else
       {
         return;// Need to wait for next buffer, so just return to save processing the other if states
       }
   }    
  
   if (foundPatternState==5)
   {
     if (getData(pos,outputBufferPtr,'<'))
     {
          Serial.print("Wind speed ");Serial.print(displayBuff);Serial.println(" mph");
          foundPatternState=6;  //Move to next state (not used in this demo)
     }
     else
     {
       // end marker is not found, stay in same findPatternState and when the callback is called with the next packet of data, outputBufferPtr will continue where it left off
     }
   }     
  
   if (foundPatternState==6)
   {
        searchPatternProgressPtr=searchPattern[2];
        foundPatternState=7;
   }
   if (foundPatternState==7)
   {
       pos = multiBufferFindPattern(pos,searchPattern[2],searchPatternProgressPtr);
       if (pos)
       {
         foundPatternState=8;
         outputBufferPtr=displayBuff; // Reset outbutBuffertPtr ready to receive new data
         memset(displayBuff,'0',sizeof(displayBuff));// clear the output display buffer
         displayBuff[sizeof(displayBuff)-1]=0;//end of buffer marker
       } 
       else
       {
         return;
       }
   }    
  
   if (foundPatternState==8)
   {
              //Serial.println("Found HR start");
     if (getData(pos,outputBufferPtr,'<'))
     {
          Serial.print("Visability ");Serial.println(displayBuff);
          foundPatternState=999;  //Move to next state (not used in this demo)
     }
     else
     {
       // end marker is not found, stay in same findPatternState and when the callback is called with the next packet of data, outputBufferPtr will continue where it left off
     }
   } 
 
 
   if (foundPatternState==999)
   {
     // Found everything on this page. dissable persistence to stop any more callbacks.
     ether.persistTcpConnection(false);
   }
 }
 
void setup ()
{
  Serial.begin(115200);
  Serial.println("\n[Web scraper example]");
 
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0)
  {
    Serial.println( "Error:Ethercard.begin");
    while(true);
  }
 
  if (!ether.dhcpSetup())
  {
    Serial.println("DHCP failed");
    while(true);
  }
 
  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip); 
  ether.printIp("DNS: ", ether.dnsip);
 
 
  // Wait for link to become up - this speeds up the dnsLoopup in the current version of the Ethercard library
  while (!ether.isLinkUp())
  {
      ether.packetLoop(ether.packetReceive());
  }
  if (!ether.dnsLookup(website,false))
  {
    Serial.println("DNS failed. Unable to continue.");
    while (true);
  }
  ether.printIp("SRV: ", ether.hisip);
}
 
void loop ()
{
  ether.packetLoop(ether.packetReceive());
 
  if (millis() > timer)
  {
    timer = millis() + 60000;// every 30 secs
 
    Serial.println("\nSending request for page /weather/right-now/ASXX0075:1");
    foundPatternState=0;// Reset state machine
    ether.persistTcpConnection(true);// enable persist, so that the callback is called for each received packet.
    ether.browseUrl(PSTR("/weather/right-now/ASXX0075:1"), "", website, browseUrlCallback);
  }
}

