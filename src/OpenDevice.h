/*
 * ******************************************************************************
 *  Copyright (c) 2013-2014 CriativaSoft (www.criativasoft.com.br)
 *  All rights reserved. This program and the accompanying materials
 *  are made available under the terms of the Eclipse Public License v1.0
 *  which accompanies this distribution, and is available at
 *  http://www.eclipse.org/legal/epl-v10.html
 *
 *  Contributors:
 *  Ricardo JL Rufino - Initial API and Implementation
 * *****************************************************************************
 */


#ifndef OpenDevice_H_
#define OpenDevice_H_

#include <Arduino.h>
#include "config.h"
#include "Command.h"
#include "DeviceConnection.h"
#include "Device.h"
#include "devices/FuncSensor.h"
#include "../dependencies.h"

using namespace od;

// ===========================================================
// Automatic Detection of Connections
// ===========================================================

#if defined(ethernet_h) || defined (UIPETHERNET_H)
	#include "EthernetServerConnection.h"
#endif

// Arduino YUN Wifi/Ethernet bridge
#if defined(_YUN_SERVER_H_) && !defined(PubSubClient_h)
	#include "YunServerConnection.h"
#endif


#if defined(PubSubClient_h)
	#include "MQTTConnection.h"
#endif


// ESP8266 AT Command library
#if defined(__ESP8266AT_H__)
	#include <WifiConnetionEspAT.h>
#endif

// ESP8266 Standalone
#if defined(ESP8266)
	#include "stdlib_noniso.h"
	#include <ESP8266WiFi.h>
	#include <WifiConnection.h>
#endif

#if defined(MFRC522_h)
	#include <devices/RFIDSensor.h>
#endif

#if defined(_RCSwitch_h)
	#include <devices/RFSensor.h>
#endif

extern volatile uint8_t* PIN_INTERRUPT;

#if(ENABLE_DEVICE_INTERRUPTION) // if config.h
#define EI_ARDUINO_INTERRUPTED_PIN
#define LIBCALL_ENABLEINTERRUPT
#include <EnableInterrupt.h>
#endif

/*
 * OpenDeviceClass.h
 *
 *  Created on: 27/06/2014
 *      Author: ricardo
 */
class OpenDeviceClass {

private:

	typedef struct {
				char command[MAX_COMMAND_STRLEN];
				void (*function)();
	} CommandCallback;

	Device* devices[MAX_DEVICE];
	CommandCallback commands[MAX_COMMAND];

	// Debouncing of normal pressing (for Sensor's)
	long time;
	bool autoControl; // Changes in the sensor should affect bonded devices..
	bool keepAliveEnabled;
	long keepAliveTime;
	long keepAliveMiss;
	bool connected;

	// Internal Listeners..
	// NOTE: Static because: deviceConnection->setDefaultListener
	static void onMessageReceived(Command cmd);

	void onSensorChanged(uint8_t id, unsigned long value);

	void notifyReceived(ResponseStatus::ResponseStatus status);
	
	// Utils....
	void clear(Command cmd);
	void showFreeRam();
	void debugChange(uint8_t id, unsigned long value);

	void _loop();
	void _begin();

public:


	Command lastCMD; // Command received / send.

	uint8_t deviceLength;
	uint8_t commandsLength;
	DeviceConnection *deviceConnection;

	OpenDeviceClass();
	// virtual ~OpenDeviceClass();


	#if defined(USING_CUSTOM_CONNECTION)
		void loop(){
			CUSTOM_CONNECTION_CLASS conn = custom_connection_loop(deviceConnection);
			deviceConnection->setStream(&conn);
			_loop();
		}
	#else
		void loop(){
			_loop();
		};
	#endif




	/**
	 * Sets the ID (formally the MAC) of the device / module. <br/>
	 * This ID can be automatically generated by the method: generateID.
	 */
	void id(uint8_t *pid) { memcpy(Config.id, pid, sizeof(Config.id)); }

	void name(char *pname) { Config.moduleName = pname; }
	const char* name() { return Config.moduleName; }
	void ip(uint8_t n1, uint8_t n2, uint8_t n3, uint8_t n4) { Config.ip[0] = n1; Config.ip[1] = n2; Config.ip[2] = n3; Config.ip[3] = n4;}


    /**
	 * Setup using the standard serial port
	 * @param baud - Sets the data rate in bits per second, Values:(300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200)
	 */
	void begin(unsigned long baud);
	void begin(Stream &stream);
	void begin(HardwareSerial &serial, unsigned long baud);

/**
 * Setup connection using default settings <br/>
 * Thus the connection settings are detected according to the active libraries
 */
#if defined(USING_CUSTOM_CONNECTION)
	void begin(){
		_begin();
		custom_connection_begin();
	}
#else
	/**
	 * No parameters, user Serial by default
	 */
	void begin(){

		// Wait serial if using Leonardo / YUN
		#if defined(HAVE_CDCSERIAL) && defined(__AVR_ATmega32U4__)
			while (!Serial){delay(1);}
		#endif

		_begin();
	};
#endif

#if defined(HAVE_CDCSERIAL)

	void begin(Serial_ &serial, unsigned long baud);

#endif

#if defined(SoftwareSerial_h)

	void begin(unsigned long baud, uint8_t rxpin, uint8_t txpin){
		static SoftwareSerial soft(rxpin, txpin);
		soft.begin(baud);
		Serial.begin(baud);
		DeviceConnection *conn =  new DeviceConnection(soft);
		begin(*conn);
	}

#endif

// For: ATtiny/Digispark
#if defined(SoftSerial_h)
	void begin(unsigned long baud, uint8_t rxpin, uint8_t txpin){
		static SoftSerial soft(rxpin, txpin);
		soft.begin(baud);
		//Serial.begin(baud);
		DeviceConnection *conn =  new DeviceConnection(soft);
		begin(*conn);
	}
#endif

// For: ATtiny/Digispark
#if defined(DEFAULT_TO_TINY_DEBUG_SERIAL)
	void begin(TinyDebugSerial &serial, unsigned long baud);
#endif

#if defined(USBserial_h_) // Teensyduino
void begin(usb_serial_class &serial, unsigned long baud){
	serial.begin(baud);

	DeviceConnection *conn =  new DeviceConnection(serial);
	begin(*conn);
}
#endif

#if defined(ESP8266)
void begin(ESP8266WiFiClass &wifi){

	WifiConnection *conn =  new WifiConnection();
	begin(*conn);
}
#endif


// TODO: Make compatible with Due
//	#ifdef _SAM3XA_
//  #include <UARTClass.h>  // Arduino Due
//  Class -> UARTClass


	// FIXME: add SoftSerial void begin(unsigned long baud, uint8_t rxpin, uint8_t txpin);

	void begin(DeviceConnection &deviceConnection);

//	void begin(Stream &serial);

	void checkSensorsStatus();

	static void onInterruptReceived();

	/** When enabled OpenDevice will be sending a PING message to connection to inform you that everything is OK. <br/>
	 * Control of the Keep Alive / Ping can be left to the other side of the connection, in this case the "device" would be disabled */
	void enableKeepAlive(bool val =  false);

	void enableDebug(uint8_t debugTarget = DEBUG_SERIAL);

	void send(Command cmd);

	/** Create a simple command (using lastCMD buffer)*/
	Command cmd(uint8_t type, uint8_t deviceID = 0, unsigned long value = 0);

#ifdef __FlashStringHelper
	void debug(const __FlashStringHelper* data);
#endif

	void debug(const char str[]);
	#ifdef ARDUINO
	void debug(const String &s);
	#endif

	Device* addSensor(uint8_t pin, Device::DeviceType type, uint8_t targetID);
	Device* addSensor(uint8_t pin, Device::DeviceType type);
	Device* addSensor(Device& sensor);
	Device* addSensor(unsigned long (*function)()){
		FuncSensor* func = new FuncSensor(function);
		return addDevice(*func);
	}

	Device* addDevice(uint8_t pin, Device::DeviceType type, bool sensor,uint8_t id);
	Device* addDevice(uint8_t pin, Device::DeviceType type);
	Device* addDevice(Device& device);

	bool addCommand(const char * name, void (*function)());

	Device* getDevice(uint8_t);
	Device* getDeviceAt(uint8_t);

	/**
	 * This function generates a Module.ID/MAC (pseudo-random) to be used in the connection and save to EEPROM for future use.
	 * @param apin  Must be passed to the function an analog pin not used
	 **/
	uint8_t * generateID(uint8_t apin);

	void setDefaultListener(void (*pt2Func)(uint8_t, unsigned long));

	void setValue(uint8_t id, unsigned long value);
	void sendToAll(unsigned long value);

	inline bool isConnected(){return connected;}

	inline String readString() { return deviceConnection->readString(); }
	inline int readInt(){ return deviceConnection->readInt(); }
	inline long readLong(){ return deviceConnection->readLong(); }
	inline float readFloat(){ return deviceConnection->readFloat(); }

	/**
	 * Can read single value list like: [1,2,3,4]
	 * If you need to read two different arrays like: [1,2,3];[5,2,3,4] call the method 'readIntValues' twice
	 */
	inline int readIntValues(int values[], int max = -1){ return deviceConnection->readIntValues(values, max); }
	inline int readLongValues(long values[], int max = -1){ return deviceConnection->readLongValues(values, max); }
	inline int readFloatValues(float values[], int max = -1){ return deviceConnection->readFloatValues(values, max); }
};

extern OpenDeviceClass ODev;

#endif /* OpenDevice_H_ */
