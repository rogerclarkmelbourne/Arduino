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
#ifndef WifiUart_h
#define UARTWifi_h

#if defined(ARDUINO) && ARDUINO >= 100
    #include "Arduino.h"
#else
    #include "WProgram.h"
#endif

class UARTWifi 
{
  public:
    UARTWifi(Stream *serial,int resetPin,int rtsPin);// constructor
	// Low level commands

	void resetModuleUsingRTS();
	void resetModuleUsingDelay(int delayMS=5000) ;// delay after resetting 5000ms normally seems enough.
	int waitCommandComplete(char *responseBuf,int timeoutMillis);
	int waitDataPattern(char *responseBuf,char *pattern,int timeoutMillis);
	int enterCommandMode(int timeout=100);
	int sendAT(int timeout=500);
	int getResponseStatus(char *responseBuf);
	int socketCreate(char *protocol,char *clientOrServer,char *host,char *portNumber,int *socketNumCreated);
	int socketGetConnectionState(char *buffer,int socketNum);
	int socketClose(int socketNum);
	int socketReceive(char *buffer,int buffSize,int socketNum);
	int socketSend(char *buffer,int buffSize,int socketNum);
	int setDefaultSocket(int socketNum);
	int getNetworkStatus(char *buffer);
	int enterTransparentMode();
	void waitForNetworkToConnect();
	int getAutoWorkSocketInfo(char *responseBuf);
	
	// High level commands
	int sendEmail(char *toAddress,char *fromAddress,char *toFriendlyName,char *subject,char *message,char *loginDomain,char *mailServer);
	
  private:
	void debounce(int pin,boolean desiredValue,int delayMS);
  
	Stream 	*_serial;
	int 	_resetPin;
	int		_rtsPin;
	char 	_gResponseBuf[96];// Probably not the most efficient way to do this, but it works !
	
};
#endif //UARTWifi_h
