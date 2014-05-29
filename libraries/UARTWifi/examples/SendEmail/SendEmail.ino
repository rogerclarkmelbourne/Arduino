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
#include <UARTWifi.h>
#include <SoftwareSerial.h>
#include <stdio.h>

/* 
 * This program demonstrates how to use the library to send an email via an STMTP server.
 * It also shows how to reset and wait for a network connection
 *
 * The program presumes that the module has already been configued using the Web Admin program on port 80
 *
 */

// This example uses SoftwareSerial, but Hardware serial can be used instead
SoftwareSerial mySerial(10, 11); // RX, TX

/*
 * use serial connection mySerial (Software serial), Reset on pin 8, RTS on pin 9.
 */
UARTWifi myWifi = UARTWifi(&mySerial,8,9);

// Example of Mega2560, or Leonardo etc HardwareSerial use could be UARTWifi myWifi = UARTWifi(&Serial1,8,9);

void setup() 
{
  Serial.begin(115200);
  /*
   * Using software serial, so we can use an Arduino with one one hardware serial e.g. Arduino UNO.
   * Software serial doesn't seem to be able to reliably communicate with the module at above 57600
   * Using an Arduino with more than 1 hardware serial works better and is reliable at 115200 baud.
   *
   * Note. The baud rate of the serial port needs to be set in the setup and is not a function inside the library.
   * This has to be done, because although the serial port, needs to be passed to the Library, the Library can't set the baud rate
   * because the private variable inside the Library that holds the serial channel, is of type Stream *, which doesnt have a begin()
   * method.
   * Both HardwareSerial and SoftwareSerial classes both derive from Stream, and both define begin() which is not in the Stream class
   * 
   */
  mySerial.begin(57600);// Software Serial doesn't seem to work above 57600 baud. Hardware serial works at 115200.

  boolean connected=false;

  // Keep trying until connection to the network is achieved.
  while(!connected)
  {
        Serial.println(F("Resetting the module"));
        // There are 2 methods to reset the module resetModuleUsingDelay() and resetModule()
        // myWifi.resetModuleUsingRTS(); If RTS is connected to the Arduino, it can be used to sense whe the module is ready
        myWifi.resetModuleUsingDelay(5000);// Alternatively just use this method which has a default delay of 5000 which is normally enough.
 
  
        Serial.println(F("Trying to enter command mode")); 
        // entrCommandMode()  sends ""+++" to the module which should respnond straight away with "+OK"
        if (myWifi.enterCommandMode())
        {
          // module must have returned "+OK"

          Serial.println(F("Command mode OK"));
          Serial.println(F("Wait for network to connect"));
          
          // Although the module is responding to commands, its possible that its Wifi link has not finished negotiating and logging in with the router / access point
          // waitForNetworkToConnect sends LKSTT to get the network connection state and waits until its connected. Note, It can wait indefinately retrying.
          myWifi.waitForNetworkToConnect();// Note, this waits forever at the moment. Need to add a timeout to this. To. Do, put timeout and retries into waitForNetworkToConnect()
          Serial.println(F("Network connected"));
          connected=true;
        }
        else
        {
          // If the module doesnt resond to commands, all we can do is reset and try again. This often resolves the problem, even if 2 or 3 reset loops are required.
          Serial.println(F("Error. Unable to enter command mode :-("));
        }
   }

  // we should be ready to go !
  
   // Build a simple message containing the millis() counter
   char message[128];
   sprintf(message,"The millis() %ld",millis());
  
  
   Serial.println("Sending email");
   /*
    * NOTE.
    * You need to get your mail server from your normal email settings
    * You may also need to use the real domain name of your computer (it depends on your ISP's mail server
    * Also note, this is not SSL emial, it is normal emial, usually you will connect to your ISP's mail server.
    * It doesn't work with servers that require authorisation and authentication.
    */
   int responseCode=myWifi.sendEmail("toAddress@email.com","from@yourEmail.com","Your name goes here e.g. John Doe","Subject of test email",message,"localhost","mailserver ip or url");
   
   if (responseCode==0) 
   {
     Serial.println("Email sent");
   }
   else
   {
     /*
      * Note. Sometimes sendEmail can't connect to the SMPT server, so it would be possible to improve this code by looping around and trying again
      * If there was a failure.
      * Also note there are many ways that the emial could fail including bad host name. Some servers don't accept localhost as the host name
	  *
	  * Error -13 is the most common error, which is caused when the module has not been able to open a socket connection to the remote server.
      */
     Serial.print(F("Error sending email. ="));
     Serial.print(responseCode,DEC);
   }
   // The end !
}

void loop() 
{
  return;
}
