/*
 * Author: Roger Clark www.rogerclark.net
 *
 * Copyright (c) 2014 Roger Clark
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

 /*
 * This library is designed to communicate with various RS232 Wifi modules
 * These are marketed as UART Wifi and mostly have a part number starting with TLD
 * e.g. TLD13AU09 It may work with other modules as well
 * These modules have only 6 data pins
 * RS232 RX and TX data
 * Reset
 * RTS
 * CTS
 * GPIO / Link
 * 
 * These module operate on 3.3V and consume more current than can be delivered by the Arduino 3.3V output
 * so a separate voltage supply is required. This can also be achieved by using a 3.3V regulator IC from the Arduino 5V pin.
 * Signals from the Arduino to the module need to have their levels converted from 5V to 3.3V using an appropriate method.
 * e.g using a resistor divider of 1k / 2k or a level converter IC or module.
 *
 */
#include "UARTWifi.h"

#define DEBUG_LEVEL 0
#define INTER_COMMAND_DELAY 50
#define SOCKET_RECEIVE_RETRY_TIME 250

// Constructor
UARTWifi::UARTWifi(Stream *serial,int resetPin,int rtsPin)
{
	this->_serial=serial;
	this->_resetPin=resetPin;
	this->_rtsPin=rtsPin;
}

void UARTWifi::debounce(int pin,boolean desiredValue,int delayMS)
{
boolean sample1;
boolean	sample2 = digitalRead(pin);

	do
	{
		sample1=sample2;
		delay(delayMS);
		sample2=digitalRead(pin);
	} while (sample1!=desiredValue && sample2!=desiredValue);
}
/*
 * resetModule() 
 *
 * Method to reset the module and wait until module is ready
 *
 * Note. This method uses the rst pin on the module to determine when its ready to receive RS232 traffic.
 * However there can be some noise problems on the rst pin which cause false inputs into the Arduino.
 * One method to overcome this is pulling the rts line to GND via a 10k resistor, or putting a small capacitor 
 * e.g. 100nF between rts and GND.
 * Sometimes, increasing the debounce delay can overcome the noice problems.
 *
 *
 */
void UARTWifi::resetModuleUsingRTS() 
{
	delay(100);// Wait 100ms for Arduino to settle etc.
	pinMode(_rtsPin, INPUT); // read the value of the RTS pin
	pinMode(_resetPin, OUTPUT); // control the Reset pin. Note this needs to be connected via a resistor divider network of 1k / 2k as the module is 3.3V

	//  Wait for the RTS pin to be low. If its not low, the module is not in an operational state.
	//	i.e it may have a glitch caused by the upload or power-up etc
	debounce(_rtsPin,0,1);
	
	digitalWrite(_resetPin,LOW);// Put module into reset
	
	// RTS will go low when module is in reset. So wait for it to go low.
	debounce(_rtsPin,0,1);
	
	digitalWrite(_resetPin,HIGH);  // Take module out of reset

	// RTS stays low some time after reset. Low normally means that device is ready, 
	// but this is a false report just after a reset, so wait for it to go high
	debounce(_rtsPin,1,1);

	// Wait for RTS to go low again, which will indicate the the module is finally ready. 
	// Note this may take some time as it depends on not only booting up, but also connection to access points if in STA mode.
	debounce(_rtsPin,0,1);
}

void UARTWifi::resetModuleUsingDelay(int delayMS) 
{
	delay(100);// Wait 100ms for Arduino to settle etc.
	pinMode(_rtsPin, INPUT); // read the value of the RTS pin
	pinMode(_resetPin, OUTPUT); // control the Reset pin. Note this needs to be connected via a resistor divider network of 1k / 2k as the module is 3.3V
	
	digitalWrite(_resetPin,LOW);// Put module into reset
	delay(50);// 50ms is plenty of time to reset the module (less than 1ms often works)
	digitalWrite(_resetPin,HIGH);  // Take module out of reset
	delay(delayMS);//delay by user specified delay period
}

int UARTWifi::waitCommandComplete(char *responseBuf,int timeoutMillis)
{
long timeoutTime = millis()+timeoutMillis;
int status=false;
int bytesReceived=0;

  while(millis()<timeoutTime && status==false)
  {
    if (_serial->available())
    {
      *responseBuf++ = _serial->read();
      if (++bytesReceived>4) 
      {
        if (strncmp_P(responseBuf-4,PSTR("\r\n\r\n"),4)==0)
        {
          //Serial.println("Command complete");
		  char *p = strstr_P(responseBuf,PSTR("\r\n\r\n"));
		  *p=0;// terminate string at first matching char
          status=true;
        }
      }  
    }  
  }
  //*responseBuf=0;// terminate the buffer so it can be used as a string
  return status;
}

int UARTWifi::waitDataPattern(char *responseBuf,char *pattern,int timeoutMillis)
{
long timeoutTime = millis()+timeoutMillis;
int status=false;
int bytesReceived=0;
int patternLength = strlen(pattern);

  while(millis()<timeoutTime && status==false)
  {
    if (_serial->available())
    {
      *responseBuf++ = _serial->read();
      if (++bytesReceived>patternLength)
      {
       // Serial.print(*responseBuf);
        if (strcmp(responseBuf-patternLength,pattern)==0)
        {
          status=true;
        }
      }  
    }  
  }
  *responseBuf=0;// terminate the buffer so it can be used as a string
  return status;
}

int UARTWifi::enterCommandMode(int timeout)
{
	_serial->print("+++");
	return waitCommandComplete(_gResponseBuf,timeout);
}

int UARTWifi::sendAT(int timeout)
{
	delay(INTER_COMMAND_DELAY);
	_serial->print(F("AT+\r"));
	return waitCommandComplete(_gResponseBuf,timeout);
}

int UARTWifi::getResponseStatus(char *responseBuf)
{
  //Serial.print(F("getResponseStatus received "));
  //Serial.print(responseBuf);
  if (strncmp(responseBuf,"+OK",3)==0)
  {
    //Serial.println(F("getResponseStatus buffer Started with +OK"));
    return 0;
  }
  else
  {
    if (strncmp(responseBuf,"+ERR=",5)==0)
    {
      //Serial.print(F("getResponseStatus  ERROR"));
      //Serial.println(responseBuf);
      return atoi(responseBuf+5);
    }
  }
}

int UARTWifi::socketCreate(char *protocol,char *clientOrServer,char *host,char *portNumber,int *socketNumCreated)
{
#if DEBUG_LEVEL > 0
	Serial.println(F("socketCreate "));
#endif
	char responseBuf[16];
	delay(INTER_COMMAND_DELAY);
	_serial->print(F("AT+SKCT="));  
	_serial->print(protocol);
	_serial->print(F(","));
	_serial->print(clientOrServer);
	_serial->print(F(","));  
	_serial->print(host);
	_serial->print(F(","));  
	_serial->print(portNumber);
	_serial->print(F("\r"));  
  
	if (waitCommandComplete(responseBuf,5000))
	{
		int responseStatus = getResponseStatus(responseBuf);
		if (responseStatus==0)
		{
		  // Note. Spec says that max ports is "supports 8 TCP client connections at most", so response should be single digit number
		  // Normally the port num returned for new ports is 2, so largest number would be 9

		  *(responseBuf+5)=0;// terminate the string  1 characters after the "+OK="
		  
//		  Serial.println(responseBuf);Serial.println(atoi(responseBuf+4));
		  
		  *socketNumCreated = atoi(responseBuf+4);
		  return  responseStatus;
		}
		else
		{
#if DEBUG_LEVEL > 0
		Serial.println(F("ERROR in getResponseStatus in create socket"));
#endif		
		  return responseStatus;// return the error code
		}
	}
	else
	{
		return -200;
	}
}

int UARTWifi::socketGetConnectionState(char *buffer,int socketNum)
{
// ---------------WARNING Not fully tested code ---------------------------

char strSocket[3];
	itoa ( socketNum, strSocket,10 );
#if DEBUG_LEVEL > 0  
	Serial.println(F("Sending AT+LKSTT"));
#endif	
	_serial->print(F("AT+SKSTT"));
	_serial->print(strSocket);
	_serial->print(F("\r"));  
	if (waitCommandComplete(buffer,5000))
	{
		int responseStatus = getResponseStatus(buffer);
		return 0;
	}
	else
	{
		return -200;
	}
}

int UARTWifi::socketClose(int socketNum)
{
#if DEBUG_LEVEL > 0  
	Serial.print(F("socketClose "));
	Serial.println(socketNum,DEC);
#endif
	char responseBuf[16];
	char strSocket[2];
	itoa ( socketNum, strSocket,10 );
	delay(INTER_COMMAND_DELAY);
	_serial->print(F("AT+SKCLS="));  
	_serial->print(strSocket);  
	_serial->print(F("\r"));  
	waitCommandComplete(responseBuf,5000);
	return getResponseStatus(responseBuf); 
}

int UARTWifi::socketReceive(char *buffer,int buffSize,int socketNum)
{
  char tmpStr[8];

#if LEVEL > 0 
	Serial.println("socketReceive");
#endif	

  delay(INTER_COMMAND_DELAY);
  itoa ( socketNum, tmpStr,10 );
  _serial->print(F("AT+SKRCV="));  
  _serial->print(tmpStr);  
  _serial->print(F(","));  
  itoa ( buffSize, tmpStr,10 );
  _serial->print(tmpStr);   
  _serial->print(F("\r"));  
  
  if (waitCommandComplete(_gResponseBuf,10000)!=0)
  {
    int responseStatus = getResponseStatus(_gResponseBuf);

    if (responseStatus==0)
    {
      char *p = strstr_P(_gResponseBuf,PSTR("\r\n\r\n"));
      *p=0;// terminate string at first matching char

      int sizeToRead=atoi((_gResponseBuf+4));
      int responseBufferSize=sizeToRead;
      //Serial.print("Size to read is "); Serial.println(sizeToRead,DEC);
      while(sizeToRead>0)
      {
        if (_serial->available())
        {
          *buffer++ = _serial->read();
#if DEBUG_LEVEL > 1		  
          Serial.print(F("."));
#endif 
          sizeToRead--;
        }
      }
     *buffer=0;// Terminate buffer for debugging   
     return  responseBufferSize;
    }
    else
    {
#if DEBUG_LEVEL > 0	
      Serial.println(F("Error. Incorrect buffer size."));
#endif	  
      return responseStatus;
    }
  }
  else
  {
    return -200;
  }
}

int UARTWifi::socketSend(char *buffer,int buffSize,int socketNum)
{
  char tmpStr[8];

#if DEBUG_LEVEL > 0  
  Serial.println(F("socketSend..."));
#endif  
  delay(INTER_COMMAND_DELAY);
  itoa ( socketNum, tmpStr,10 );
  _serial->print(F("AT+SKSND="));  
  _serial->print(tmpStr);  
  _serial->print(F(","));  
  itoa ( buffSize, tmpStr,10 );
  _serial->print(tmpStr);   _serial->print(F("\r"));  
  
  if (waitCommandComplete(_gResponseBuf,5000)!=0)
  {
    int responseStatus = getResponseStatus(_gResponseBuf);
    if (responseStatus!=buffSize)
    {
      char *p = strstr_P(_gResponseBuf,PSTR("\r\n\r\n"));
      *p=0;// terminate string at first matching char
      int sizeToSend=atoi((_gResponseBuf+4));
      //Serial.print("Size to send is "); Serial.println(sizeToSend,DEC);
      delay(INTER_COMMAND_DELAY);
      _serial->print(buffer);
    }
    else
    {
#if DEBUG_LEVEL > 0	
      Serial.println(F("Error. Send Incorrect buffer size."));
#endif	  
      return responseStatus;
    }
  }
  else
  {
    return -200;
  }
}

int UARTWifi::setDefaultSocket(int socketNum)
{
	char tmpBuf[3];
	delay(INTER_COMMAND_DELAY);
	_serial->print(F("AT+SKSDF="));
	itoa(socketNum,tmpBuf,10);
	_serial->print(tmpBuf);
	_serial->print(F("\r"));
	if (waitCommandComplete(_gResponseBuf,5000))
	{
		return getResponseStatus(_gResponseBuf);
	}
	else
	{
#if DEBUG_LEVEL > 0		
		Serial.println(F("Timeout trying to set default socket"));
#endif		
		return -200;
	}
}
int UARTWifi::getNetworkStatus(char *buffer)
{
#if DEBUG_LEVEL > 0  
	Serial.println(F("Sending AT+LKSTT"));
#endif	
	_serial->print(F("AT+LKSTT\r"));  
	if (waitCommandComplete(buffer,5000))
	{
		return 0;
	}
	else
	{
		return -200;
	}
}

int UARTWifi::enterTransparentMode()
{
	delay(INTER_COMMAND_DELAY);
	_serial->print(F("AT+ENTM\r"));

	if (waitCommandComplete(_gResponseBuf,5000))
	{
		return getResponseStatus(_gResponseBuf);
	}
	else
	{
		return -200;
	}
}

void UARTWifi::waitForNetworkToConnect()
{
  while(true)
  {
      getNetworkStatus(_gResponseBuf);
      char *p = strstr_P(_gResponseBuf,PSTR("\r\n\r\n"));
      *p=0;// terminate string at first matching char
#if DEBUG_LEVEL > 0		  
      Serial.println(_gResponseBuf);
#endif	  
      if (strncmp((_gResponseBuf+4),"1",1)==0)
      {
#if DEBUG_LEVEL > 0  
		Serial.println(F("Network was connected"));
#endif		
        return;
      }
      else
      {
#if DEBUG_LEVEL > 0  
		Serial.println(F("Waiting for network to be connected"));
#endif		
        delay(1000);// wait 1000ms before we re-try
      }
  }
}

int UARTWifi::getAutoWorkSocketInfo(char *responseBuf)
{
#if DEBUG_LEVEL > 0
	Serial.println(F("Sending AT+ATRM"));
#endif	
	_serial->print(F("AT+ATRM\r"));  
	if (waitCommandComplete(responseBuf,5000))
	{
		char *p = strstr_P(_gResponseBuf,PSTR("\r\n\r\n"));
		*p=0;// terminate string at first matching char
		return 0;
	}
	else
	{
		return -200;
	}  
}

int UARTWifi::sendEmail(char *toAddress,char *fromAddress,char *toFriendlyName,char *subject,char *message,char *loginDomain,char *mailServer)
{
char dataBuf[256];
int theSocket;
int responseCode = socketCreate("0","0",mailServer,"25",&theSocket);// 0=client,0=tcp,mailserver,port 25 for smtp, var into which the socket num is returned

	if (responseCode==0)
	{
#if DEBUG_LEVEL > 2		
	Serial.print(F("Socket created was "));//Serial.println(theSocket);
#endif
    delay(250);
    socketReceive(dataBuf,sizeof(dataBuf),theSocket);
    //Serial.print(F("socketReceive got "));//Serial.println(dataBuf);
    if (strncmp(dataBuf,"220",3)==0)
    {
      //Serial.println(F("Connection to SMTP server OK"));
      strcpy(dataBuf,"HELO ");
	  strcat(dataBuf,loginDomain);
	  strcat(dataBuf,"\r\n");
      socketSend(dataBuf,strlen(dataBuf),theSocket);
      //Serial.println("waiting for 250 message");

      while (socketReceive(dataBuf,sizeof(dataBuf),theSocket)==0)
      { 
        //Serial.println(F("No data yet;-)"));
        delay(SOCKET_RECEIVE_RETRY_TIME);
      } 
      //Serial.print("Response was ");//Serial.println(dataBuf);
  
      if (strncmp_P(dataBuf,PSTR("250"),3)==0)
      {
        //Serial.println("HELO message was OK");
        strcpy_P(dataBuf,PSTR("MAIL FROM: "));
		strcat(dataBuf,fromAddress);
		strcat_P(dataBuf,PSTR("\r\n"));
        socketSend(dataBuf,strlen(dataBuf),theSocket);
        while (socketReceive(dataBuf,sizeof(dataBuf),theSocket)==0)
        { 
          //Serial.println(F("Wait socketReceive retry"));
          delay(SOCKET_RECEIVE_RETRY_TIME);
        } 
        //Serial.print(F("socketReceive got "));//Serial.println(dataBuf);
        }
        if (strncmp_P(dataBuf,PSTR("250"),3)==0)
        {
          //Serial.println(F("MAIL FROM message was OK"));
          
          strcpy_P(dataBuf,PSTR("RCPT TO: "));
  		  strcat(dataBuf,toAddress);
		  strcat_P(dataBuf,PSTR("\r\n"));
          socketSend(dataBuf,strlen(dataBuf),theSocket);
          
          while (socketReceive(dataBuf,sizeof(dataBuf),theSocket)==0)
          { 
            //Serial.println(F("Wait socketReceive retry"));
            delay(SOCKET_RECEIVE_RETRY_TIME);// if there wasn't any data, wait and try again
          } 
          //Serial.print(F("socketReceive 1 got "));
          //Serial.println(dataBuf);       
          if (strncmp(dataBuf,"250",3)==0)
          {
            //Serial.println(F("RCPT TO message was OK"));
            
            strcpy_P(dataBuf,PSTR("DATA\r\n"));
            socketSend(dataBuf,strlen(dataBuf),theSocket);
            
            while (socketReceive(dataBuf,sizeof(dataBuf),theSocket)==0)
            { 
              //Serial.println(F("Wait socketReceive retry"));
              delay(SOCKET_RECEIVE_RETRY_TIME);// if there wasn't any data, wait and try again
            } 
            //Serial.print(F("socketReceive got "));//Serial.println(dataBuf);     
           if (strncmp(dataBuf,"354",3)==0)
           {
            //Serial.println(F("DATA message was OK"));
            
            strcpy_P(dataBuf,PSTR("SUBJECT: "));
			strcat(dataBuf,subject);
			strcat_P(dataBuf,PSTR("\r\n"));
            socketSend(dataBuf,strlen(dataBuf),theSocket);

            strcpy_P(dataBuf,PSTR("FROM: "));
			strcat(dataBuf,toFriendlyName);
			strcat_P(dataBuf,PSTR(" <"));
            strcat(dataBuf,fromAddress);
            strcat_P(dataBuf,PSTR(">\r\n"));
            socketSend(dataBuf,strlen(dataBuf),theSocket);
            
            strcpy_P(dataBuf,PSTR("To: "));
            strcat(dataBuf,toAddress);
            strcat_P(dataBuf,PSTR("\r\n")) ; 
            socketSend(dataBuf,strlen(dataBuf),theSocket);

            socketSend(message,strlen(message),theSocket);

            strcpy_P(dataBuf,PSTR("\r\n.\r\n"));// termination string
            socketSend(dataBuf,strlen(dataBuf),theSocket); 
            
            while (socketReceive(dataBuf,sizeof(dataBuf),theSocket)==0)
            { 
              //Serial.println("Wait socketReceive retry");
              delay(SOCKET_RECEIVE_RETRY_TIME);// if there wasn't any data, wait and try again
            } 
            //Serial.print(F("socketReceive got"));Serial.println(dataBuf);   
            if (strncmp_P(dataBuf,PSTR("250"),3)==0)
            {
              strcpy_P(dataBuf,PSTR("quit\r\n"));
              socketSend(dataBuf,strlen(dataBuf),theSocket);   
              while (socketReceive(dataBuf,sizeof(dataBuf),theSocket)==0)
              { 
                //Serial.println(F("Wait socketReceive retry"));
                delay(SOCKET_RECEIVE_RETRY_TIME);// if there wasn't any data, wait 500ms and try again
              }            
            }
			else
			{
			    socketClose(theSocket);
				return -250;
			}
          }
		  else
		  {
				socketClose(theSocket);
				return -354;
		  }
        }
		else
		{
		    socketClose(theSocket);
			return -250;
		}
      }
	  else
	  {
		socketClose(theSocket);
		return -250;
	  }
    }

    socketClose(theSocket);
    return 0;
  }
  else
  {
    //Serial.println(responseCode);
    return responseCode;
  }  
}