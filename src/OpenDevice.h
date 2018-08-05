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
#include "../dependencies.h"

#include "Command.h"
#include "DeviceConnection.h"
#include "Device.h"
#include "devices/CustomSensor.h"
#include "utility/Logger.h"
#include "utility/Timeout.h"
#include "utility/build_defs.h"

using namespace od;

extern volatile uint8_t* PIN_INTERRUPT;

#if(ENABLE_DEVICE_INTERRUPTION) // if config.h
#define EI_ARDUINO_INTERRUPTED_PIN
#define LIBCALL_ENABLEINTERRUPT
#include <EnableInterrupt.h>
#endif


/**
 * Main point of device configuration and management in firmware.
 * Several settings can be made through the file: config.h
 * The automatic configuration system is implemented in this file in conjunction with dependencies.h
 *
 *  
 * @author Ricardo JL Rufino
 * @date 27/06/2014
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
	unsigned long time;
	bool autoControl; // Changes in the sensor should affect bonded devices..
	long keepAliveTime;
	long keepAliveMiss;
	bool needSaveDevices;
	Timeout saveDevicesTimer;

	// Internal Listeners..
	// NOTE: Static because: deviceConnection->setDefaultListener
	static bool onDeviceChanged(uint8_t iid, value_t value);

	void onSensorChanged(uint8_t id, value_t value);

	void notifyReceived(ResponseStatus::ResponseStatus status);

	// Utils....
	void clear(Command cmd);

	Command resp(CommandType::CommandType type, uint8_t deviceID = 0, value_t value = 0);
	void debugChange(uint8_t id, value_t value);

	void _loop();

	void beginDefault();

	void loadDevicesFromStorage();

public:

	Command lastCMD; // Command received / send.
	bool messageReceived = false;

	uint8_t deviceLength;
	uint8_t commandsLength;
	DeviceConnection *deviceConnection;

#ifdef _TASKSCHEDULER_H_
	Scheduler scheduler;
#endif


	OpenDeviceClass();
	// virtual ~OpenDeviceClass();

	/** OpenDevice main operating point. You should call this method in the Skech main loop */
	void loop(){

		#ifdef CUSTOM_CONNECTION_CLASS
			CUSTOM_CONNECTION_CLASS conn = custom_connection_loop(deviceConnection);
			deviceConnection->setStream(&conn);
		#endif

		_loop();

		if(messageReceived){
			onMessageReceivedImpl();
			deviceConnection->flush();
		}

		#ifdef _TASKSCHEDULER_H_
			scheduler.execute();
		#endif

		#if defined(__ARDUINO_OTA_H)
			RemoteUpdate.check();
		#endif

		#if(ENABLE_ALEXA_PROTOCOL)
			Alexa.loop();
		#endif
	};


	/**
	 * Sets the ID (formally the MAC) of the device / module. <br/>
	 * This ID can be automatically generated by the method: generateID.
	 */
	void id(uint8_t *pid) { memcpy(Config.id, pid, sizeof(Config.id)); }

	/** Configure this Device/Module Name to identify and group devices*/
	void name(const char *pname);

	/** Set server IP or Host to connect */
	void server(char pname[]);

	/** Set APIKey for this Device*/
	void apiKey(char pname[]);

	/** Set reset PIN, if you are using ESP, it must be set to active low */
	void resetPin(byte pin);

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
	 * Allow custom #preprocessor macros
	 */
	void _afterBegin(){

		//#ifdef _TASKSCHEDULER_H_
		//		Serial.println("scheduler.init()");
		//		scheduler.init();
		//#endif

	}


#if defined(USING_CUSTOM_CONNECTION)
	void begin(){

		beginDefault();

		custom_connection_begin();
	}
#else
	/**
	 * Setup connection using default settings <br/>
	 * The connection settings are detected according to the active libraries
	 */
	void begin(){

		// Wait serial if using Leonardo / YUN
		#if defined(__AVR_ATmega32U4__) && !(defined(_YUN_SERVER_H_) || defined(_YUN_CLIENT_H_))
			while (!Serial){delay(1);}
		#endif

		#if defined(__ARDUINO_OTA_H)
			RemoteUpdate.begin();
		#endif

		#if defined(_YUN_SERVER_H_) || defined(_YUN_CLIENT_H_)
			Bridge.begin();
		#endif

    // ESP as MQTT Client
		#if defined(ESP8266) && defined(PubSubClient_h)

			MQTTWifiConnection *conn =  new MQTTWifiConnection();
			begin(*conn);

    // ESP as TCP Server
		#elif defined(ESP8266) && ! defined(PubSubClient_h)

			WifiConnection *conn =  new WifiConnection();
			begin(*conn);

    // YUN MQTT Client
		#elif defined(_YUN_CLIENT_H_)

			static YunClient ethclient;
			MQTTEthConnection *conn =  new MQTTEthConnection(ethclient);
			begin(*conn);

    // Ethernet MQTT Client
		#elif defined(ethernet_h)
			connectNetwork();
			static EthernetClient ethclient;
			MQTTEthConnection *conn =  new MQTTEthConnection(ethclient);
			begin(*conn);
		#else
			beginDefault();
		#endif

		// Initialize Alexa devices (only digital devices)
		#if(ENABLE_ALEXA_PROTOCOL)
			for (int i = 1; i < deviceLength; ++i) {
				Alexa.addDevice(getDeviceAt(i));
			}
			Alexa.begin();
		#endif

	};
#endif

#if defined(ethernet_h)

	void connectNetwork(){

		// byte* mac = generateID();
		// FIXME: use DYNAMIC
		byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

		// // Using saved IP (Sketch or EEPROM)
		// if(Config.ip[0] != 0){
		// 	Serial.println(F("Using saved IP on EEPROM"));
		// 	Ethernet.begin(mac, Config.ip);
		// } // try DHCP
		// else{
			Serial.println(F("Using DHCP"));
			#if(ENABLE_DHCP)
				#if defined (UIPETHERNET_H) && !UIP_CONF_UDP
				#error "Using UIPEthernet with DHCP, you must enable UIP_CONF_UDP ! (This eats space !)"
				#endif
				if (Ethernet.begin(mac)>0) {
					IPAddress ip = Ethernet.localIP();
					Config.ip[0] = ip[0];
					Config.ip[1] = ip[1];
					Config.ip[2] = ip[2];
					Config.ip[3] = ip[3];
					Serial.println("DHCP [OK]");
					// save();
				}else{
					Serial.println("DHCP Failed");
				}
			#else
				Serial.println(F("Please define a IP or enable DHCP"));
			#endif
		// }

		Serial.print("Server is at: "); Serial.println(Ethernet.localIP());
	}

#endif

#if defined(HAVE_CDCSERIAL)

	void begin(Serial_ &serial, unsigned long baud);

#endif

#if defined (__arm__) && defined (__SAM3X8E__) // Arduino Due compatible

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

// #if defined(ESP8266) && !defined(PubSubClient_h)
// void begin(ESP8266WiFiClass &wifi){
//
// 	WifiConnection *conn =  new WifiConnection();
// 	begin(*conn);
// }
// #endif


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

    void showFreeRam();

    /** Reset microcontroller (only for ESP) others need hardware changes */
	void reset();

	void enableDebug(uint8_t debugTarget = DEBUG_SERIAL);

	/** Create a simple command (using lastCMD buffer)*/
	Command cmd(CommandType::CommandType type, uint8_t deviceID = 0, value_t value = 0);

	void send(Command cmd);


#ifdef __FlashStringHelper
	void debug(const __FlashStringHelper* data);
#endif

	void debug(const char str[], unsigned long value = -1);
	#ifdef ARDUINO
	void debug(const String &s);
	#endif

	Device* addSensor(const char* name, uint16_t pin, Device::DeviceType type, uint8_t targetID);
	Device* addSensor(const char* name, uint16_t pin, Device::DeviceType type);
	Device* addSensor(const char* name, Device& sensor);
	Device* addSensor(const char* name, Device* sensor){
		return addDevice(name, *sensor);
	}
	Device* addSensor(const char* name, value_t (*function)()){
		CustomSensor* func = new CustomSensor(function);
		return addDevice(name, *func);
	}

	Device* addDevice(const char* name, uint16_t pin, Device::DeviceType type, bool sensor,uint8_t id);
	Device* addDevice(const char* name, uint16_t pin, Device::DeviceType type);
	Device* addDevice(Device& device);
	Device* addDevice(const char* name, Device& device);

	bool addCommand(const char * name, void (*function)());

#ifdef _TASKSCHEDULER_H_

	inline void addTask(Task& aTask, void (*aCallback)()){
		aTask.setCallback(aCallback);
		scheduler.addTask(aTask);
	}


	inline void deleteTask(Task& aTask){
		scheduler.deleteTask(aTask);
	}

#endif

	Device* getDevice(uint8_t);
	Device* getDeviceAt(uint8_t);
	Device* getDevice(const char* name);

	/**
	 * This function generates a Module.ID/MAC (pseudo-random) to be used in the connection and save to EEPROM for future use.
	 * @param apin  Must be passed to the function an analog pin not used
	 **/
	uint8_t * generateID(uint8_t apin = 0);

	void setValue(uint8_t id, value_t value);
	void sendValue(Device* device);

	void toggle(uint8_t index);
	void sendToAll(value_t value);

	/** @see od::ConfigClass#load() */
	void load(){
		Config.load();
	}

	/** @see od::ConfigClass#save() */
	void save(){
		Config.save();
	}

	/** @see od::ConfigClass#clear() */
	void clear(){
		Config.clear();
	}

	void printStorageSettings(){

	#if defined(ESP8266)
	 uint32_t realSize = ESP.getFlashChipRealSize();
	 uint32_t ideSize = ESP.getFlashChipSize();
	 FlashMode_t ideMode = ESP.getFlashChipMode();

	 Serial.print("Flash real id:"); Serial.println(ESP.getFlashChipId());
	 Serial.print("Flash real size:"); Serial.println(realSize);
	 Serial.print("Flash ide size:"); Serial.println(ideSize);
	 Serial.print("Flash ide mode:"); Serial.println((ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));

	 if(ideSize != realSize) {
			 Serial.println("Flash Chip configuration wrong!\n");
	 } else {
			 Serial.println("Flash Chip configuration ok.\n");
	 }
	 delay(500);
	 #else
	 		 Serial.println("Only for ESP8266");
	 #endif

	}


	bool isConnected(){
		return (deviceConnection && deviceConnection->connected);
	}

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

	static void onMessageReceived(Command cmd);

	/** Called when a command is received by the connection */
	void onMessageReceivedImpl() {

		Command cmd = lastCMD;
		DeviceConnection *conn = deviceConnection;

		messageReceived = false;
		conn->connected = true;
		keepAliveTime = millis();
		keepAliveMiss = 0;

		bool cont = true; // TODO: Chama handlers(functions), se retornar false abota a continuacao;

		debug("CType:", cmd.type);
		// showFreeRam();

		// Directed to a device (Like On/OFF or more complex)
		if (cmd.deviceID > 0) {
			Device *foundDevice = getDevice(cmd.deviceID);
			if (foundDevice != NULL) {
				debugChange(foundDevice->id, cmd.value);
				foundDevice->setValue(cmd.value, false);
				foundDevice->deserializeExtraData(&cmd, conn);
				notifyReceived(ResponseStatus::SUCCESS);
			} else {
				notifyReceived(ResponseStatus::NOT_FOUND);
			}
		// User-defined command, this is an easy way to extend OpenDevice protocol.
		} else if (cmd.type == CommandType::USER_COMMAND) {
			String name = conn->readString();
			for (int i = 0; i < commandsLength; i++) {
				// if(debugMode){ debug("Call function:"); debug(name); }
				if (name.equals(commands[i].command)) {
					notifyReceived(ResponseStatus::SUCCESS);
					(*commands[i].function)();
				}
			}
		} else if (cmd.type == CommandType::PING_REQUEST) {

			send(resp(CommandType::PING_RESPONSE, 0, ResponseStatus::SUCCESS));

		} else if (cmd.type == CommandType::RESET) {

			reset();

		// Send response: GET_DEVICES_RESPONSE;ID;Index;Length;[ID, PIN, VALUE, TARGET, SENSOR?, TYPE];
		// NOTE: This message is sent to each device
		} else if (cmd.type == CommandType::GET_DEVICES) {

			char buffer[DATA_BUFFER]; // FIXME: daria para usar o mesmo buffer do deviceConnection ??
			memset(buffer, 0, sizeof(buffer));

			LOG_DEBUG("GET_DEVICES", deviceLength);

			for (int i = 0; i < deviceLength; ++i) {

				Device *device = getDeviceAt(i);

				Serial.printf("SEND (%d/%d): %s \n", i+1, deviceLength, device->deviceName);

				conn->doStart();
				conn->print(CommandType::GET_DEVICES_RESPONSE);
				conn->doToken();
				conn->print(cmd.id); // cmd index
				conn->doToken();
				conn->print(i + 1); // track current index
				conn->doToken();
				conn->print(deviceLength); // max devices
				conn->doToken();

				device->toString(buffer);

				conn->print(buffer); // Write array to connection..

				memset(buffer, 0, sizeof(buffer));

				conn->doEnd();

			}

	  // Save devices ID on storage
		} else if (cmd.type == CommandType::SYNC_DEVICES_ID) {

			//		conn->printBuffer();

			int length = conn->readInt();

			LOG_DEBUG("SYNC", length);

			if(length!= deviceLength) {   // Invalid Match
				notifyReceived(ResponseStatus::BAD_REQUEST);
				return;
			}

			Config.devicesLength = length;

			for (size_t i = 0; i < length; i++) {
				int uid = conn->readInt();
				if(uid > MAX_DEVICE_ID){
					LOG_DEBUG_S("MAX_ID ERROR");
					notifyReceived(ResponseStatus::BAD_REQUEST);
					return;
				}

				devices[i]->id = uid;
				Config.devices[i] = uid;
				// Serial.print("SYNC :: ");Serial.print(i);Serial.print(" => ");Serial.println(devices[i]->id, DEC);
			}

			save();
			notifyReceived(ResponseStatus::SUCCESS);


		} else if (cmd.type == CommandType::FIRMWARE_UPDATE) {

			#ifdef ESP8266HTTPUPDATE_H_

				int port = ODEV_OTA_REMOTE_PORT;

				if(Config.server[0] == '1' && Config.server[1] == '9'){ // local IP
					port = 8181;
				}

				String uuid = conn->readString();
				 
				String url = "http://" +String(Config.server) + ":" + port + "/middleware/firmwares/download/"+uuid;
				
				bool updated = RemoteUpdate.updateFromURL(url);

				if(updated){
					notifyReceived(ResponseStatus::SUCCESS);
					reset();
				}else{
					notifyReceived(ResponseStatus::INTERNAL_ERROR); 
				}

			#else

				notifyReceived(ResponseStatus::NOT_IMPLEMENTED); 
				
			#endif

		}else{

			// TODO: Send response: UNKNOW_COMMAND

		}

	}

};

extern OpenDeviceClass ODev;


#endif /* OpenDevice_H_ */
