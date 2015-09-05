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

#ifndef DeviceConnection_h
#define DeviceConnection_h

#include <Arduino.h>
#include "config.h"
#include "Command.h"

extern "C"
{
  // Definition of the listener function
  typedef void (*CommandListener) (Command);
}

/******************************************************************************
* Definitions
******************************************************************************/

/**
 * Class que implements the application level protocol of the OpenDevice.
 */
class DeviceConnection{


protected:
	virtual void init();
private:
	
	// Alias
	static const uint8_t START_BIT = Command::START_BIT;
	static const uint8_t ACK_BIT = Command::ACK_BIT;
	static const uint8_t SEPARATOR = Command::SEPARATOR;
	const char* SEPARATOR_LIST = ";";

	bool processing;
	char _buffer[DATA_BUFFER];

    const uint16_t _len = DATA_BUFFER;
    bool _buffer_overflow;
    uint16_t _endOffset;
	volatile uint8_t _readOffset;

	uint16_t readTimeout; // time in ms to the wait for end command

	CommandListener defaultListener;  			// default listener
//	CommandListener listeners[MAX_LISTENERS];   // user listeners
//	uint8_t listeners_key[MAX_LISTENERS];       // listeners keys(CommandType).
//	int listenersLength;

	// private methods
	void parseCommand(uint8_t type);
	void notifyListeners(Command);
	void notifyError(ResponseStatus::ResponseStatus status);

	int getArrayLength();

	long parseInt(); // returns the first valid (long) integer value from the current position.
	float parseFloat();               // float version of parseInt
	int peekNextDigit(); // returns the next numeric digit in the stream or -1 if timeout
	const uint16_t current_length() const { return _endOffset; }
	bool overflow() { return _buffer_overflow; }
    size_t store(uint8_t byte);
    int peek();
    int read();
    int available();
    int nextEndOffSet();
    bool isListEnd(char c);

public: 
	Stream  *conn;
	Command cmd;

	// public methods
	DeviceConnection();
	DeviceConnection(Stream &serial);
	
	void flush(void);
	virtual bool checkDataAvalible(void);

	void setStream(Stream *stream) { conn = stream; };
	void setDefaultListener(CommandListener);
//	void addListener(uint8_t,CommandListener);
//	void removeListener(uint8_t);
	void getBuffer(uint8_t[]);
	
	String readString();
	inline int readInt(){ return parseInt(); }
	inline long readLong(){ return parseInt(); }
	inline float readFloat(){ return parseFloat(); }

	/**
	 * Can read single value list like: [1,2,3,4]
	 * If you need to read two different arrays like: [1,2,3];[5,2,3,4] call the method 'readIntValues' twice
	 */
	int readIntValues(int values[], int max);
	int readLongValues(long values[], int max);
	int readFloatValues(float values[], int max);
	

	#if defined(ARDUINO) && ARDUINO >= 100
	size_t write(uint8_t);
	#else
	void write(uint8_t);
	#endif
	

	void doStart();
	void doToken();
	void doEnd();

	void send(char);
    void send(const char[]);
    void send(int[], int);
    void send(long values[], int size);
    void send(uint8_t);
    void send(int);
    void send(unsigned int);
    void send(long);
    void send(unsigned long);
    void send(long, int);
    void send(double);
    void send(Command, bool complete = true);

    template < class T > void sendCmdArg (T arg){
    	conn->write(START_BIT);
    	conn->print(arg);
    	conn->write(ACK_BIT);
    }

    /** Umanaged send data, must be used with doStart/doToken/doEnd */
    template < class T > void print (T arg){
    	conn->print(arg);
    }
	
	static int api_version() { return API_VERSION;}
};


#endif
