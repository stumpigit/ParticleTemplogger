// This #include statement was automatically added by the Particle IDE.
#include <HttpClient.h>

/*
    ZERMATT Temperature Logger
    ==========================
    (C) Christoph Suter, 2018

*/
#include <ArduinoJson.h>
#include "DS18.h"
#include "cellular_hal.h"
#include "uCommand.h"
STARTUP(cellular_credentials_set("gprs.swisscom.ch", "", "", NULL));
STARTUP(cellular_sms_received_handler_set(smsRecvFlag, NULL, NULL));

// The Temperatur sensor on PIN D2
DS18 sensor(D0);
DS18 sensor2(A0);
uCommand uCmd;

FuelGauge fuel;
HttpClient http;
// Headers currently need to be set at init, useful for API keys etc.
http_header_t headers[] = {
    { "Content-Type", "application/json" },
    //  { "Accept" , "application/json" },
    { "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};
http_request_t request;
http_response_t response;


struct LogValue
{
	time_t time;
	float t1;
	float t2;
};

retained LogValue logValues[6];
retained int counter = 0;

double tempC = 0.0;
double bat = 100.0;

retained int smsAvailableFlag = 0;
retained bool isFirststart = true;

String messageText = "default";
String phoneReturn = "default";

#define RADIO Cellular

#define Version 16


SYSTEM_MODE(MANUAL);

// Reset if the system is not active for 3 hours
ApplicationWatchdog wd(10800000, System.reset);

void setup()
{
	Serial.begin(9600);
	Serial.printf("Zermatt Temperaturlogger");
	
	uCmd.setDebug(false);
	int atResult;
	atResult = uCmd.setSMSMode(1);
	if(atResult == RESP_OK)
	{
	}
	else
	{
	}
	
	PMIC pmic; //Initalize the PMIC class so you can call the Power Management functions below. 
    pmic.setChargeCurrent(0,0,1,0,0,0); //Set charging current to 1024mA (512 + 512 offset)
    pmic.setInputVoltageLimit(4840);   //Set the lowest input voltage to 4.84 volts. This keeps my 5v solar panel from operating below 4.84 volts.  


	deleteSMSOnStart();
		
	if (isFirststart)
	{

	Cellular.on();
	    	delay(1000);
        	Cellular.connect();
        	waitUntil(Cellular.ready);
        
        	//Particle.connect();
        
        	Particle.connect();
        	waitUntil(Particle.connected);
        	Particle.process();

			Serial.printf("Time not good");
			Particle.syncTime();
			// Wait until Electron receives time from Particle Cloud (or connection to Particle Cloud is lost)
			waitUntil(Particle.syncTimeDone);
			Serial.printf("Time is good");


		if (!Time.isValid())
		{
	    	
		}

		isFirststart = false;
	}

}

void loop()
{
	Cellular.on();		
	delay(1000);
	// Read the next available 1-Wire temperature sensor
	if (sensor.read())
	{
		tempC = sensor.celsius();
		if (tempC == 0.0)
		{
			delay(1000);
			tempC = sensor.celsius();
		}
		
		for (int i = 0; i<5; i++) {
		    if (sensor2.read()) i=6;
		}
		
		double tempC2 = sensor2.celsius();
        if (tempC2 == 0.0)
		{
			delay(1000);
			tempC2 = sensor2.celsius();
		}
		
		
		Serial.printf("Temperature %.2f C ", tempC);

        // Write time and Temp to log array
		logValues[counter] = {Time.now(), tempC, tempC2};
		if ((counter >= 3) || (smsAvailableFlag == 1))
		{
			publishData();
		}
		else
		{
			counter++;
		}
		//delay(1000);

	    if (!Time.isValid())
		{
	    	Cellular.on();
	    	delay(1000);
        	Cellular.connect();
        	waitUntil(Cellular.ready);
        
        	//Particle.connect();
        
        	Particle.connect();
        	waitUntil(Particle.connected);
        	Particle.process();

			Serial.printf("Time not good");
			Particle.syncTime();
			// Wait until Electron receives time from Particle Cloud (or connection to Particle Cloud is lost)
			waitUntil(Particle.syncTimeDone);
			Serial.printf("Time is good");

		}           
		// calculate next call
		int currentMinute = Time.minute();
		int currentSecond = Time.second();
		
		
		// measure every 10 Minute
		
		bool isNight = false;
		if ((Time.hour()>=19) || (Time.hour()<3)) isNight = true;
		
		int nextCall = 600 - (((currentMinute*60)+currentSecond) % 600);
		// during night, just du it every 30 minutes
	    	if (isNight) nextCall = 1800 - (((currentMinute*60)+currentSecond) % 1800);
		int nextCallSeconds = nextCall;
		
		/*
		
		int nextCall=0;
		if ((currentMinute >= 0) && (currentMinute < 10)) nextCall = 10;
		if ((currentMinute >= 10) && (currentMinute < 20)) nextCall = 20;
		if ((currentMinute >= 20) && (currentMinute < 30)) nextCall = 30;
		if ((currentMinute >= 30) && (currentMinute < 40)) nextCall = 40;
		if ((currentMinute >= 40) && (currentMinute < 50)) nextCall = 50;
		if ((currentMinute >= 50) && (currentMinute <= 59)) nextCall = 60;
		int nextCallSeconds = (nextCall - currentMinute) * 60;*/


		if (smsAvailableFlag == 1)
		{
			//smsRecvCheck();
			smsAvailableFlag = 0;
		}

        int now = Time.now();

        //System.sleep(1000);
        if (isNight) System.sleep(SLEEP_MODE_SOFTPOWEROFF, nextCallSeconds);
	    System.sleep(RI_UC, RISING, nextCallSeconds, SLEEP_NETWORK_STANDBY);
	    
	    
	    int now2 = Time.now();
	    int diff = now2-now;
	    if ((nextCallSeconds>diff+5)||(nextCallSeconds<diff-5)) {
	        smsAvailableFlag = 1;
	        smsRecvCheck();
	    }
	    /*
	
	if (!success) uCmd.sendMessage("Konnte counter nicht schreiben","+41787210818",10000);
	else uCmd.sendMessage("Counter geschrieben","+41787210818",10000);
	
	*/
	    // check if sms was sent, so we didn't wait nextCallSeconds
	    
	    
		//delay(10000);
		//System.sleep(SLEEP_MODE_DEEP, nextCallSeconds, SLEEP_NETWORK_STANDBY);
		//System.sleep(SLEEP_MODE_SOFTPOWEROFF, nextCallSeconds);

		// If sensor.read() didn't return true you can try again later
		// This next block helps debug what's wrong.
		// It's not needed for the sensor to work properly
	}

}

void publishData()
{
	Cellular.on();
	delay(1000);
	Cellular.connect();
	waitUntil(Cellular.ready);

	//Particle.connect();
/*
	Particle.connect();
	waitUntil(Particle.connected);
	Particle.process();*/


	if (!Time.isValid())
	{

		Serial.printf("Time not good");
		Particle.syncTime();
		// Wait until Electron receives time from Particle Cloud (or connection to Particle Cloud is lost)
		waitUntil(Particle.syncTimeDone);
		Serial.printf("Time is good");

	}
	
	bat = fuel.getSoC();
	//waitUntil(Particle.connected);
	// make JSON from Array
	StaticJsonBuffer<1500> jsonBuffer;
	JsonObject& jSuper = jsonBuffer.createObject();
	jSuper["bat"] = bat;

	JsonArray& root = jSuper.createNestedArray("data");
	JsonArray& dataj = root.createNestedArray();
	dataj.add(logValues[0].time);
	dataj.add(logValues[0].t1);
	dataj.add(logValues[0].t2);
	JsonArray& datak = root.createNestedArray();
	datak.add(logValues[1].time);
	datak.add(logValues[1].t1);
	datak.add(logValues[1].t2);
	JsonArray& datal = root.createNestedArray();
	datal.add(logValues[2].time);
	datal.add(logValues[2].t1);
	datal.add(logValues[2].t2);
	JsonArray& datam = root.createNestedArray();
	datam.add(logValues[3].time);
	datam.add(logValues[3].t1);
	datam.add(logValues[3].t2);



	char jsonChar[jSuper.measureLength() + 1];
	jSuper.printTo((char*)jsonChar, jSuper.measureLength() + 1);
	Serial.printf(jsonChar);
	
	// try sending it 15 times ()
	/*bool success=false;
	for (int i=0; i<5;i++) {
	    success = Particle.publish("temperature", jsonChar, PRIVATE);
	    if (success) i=5;
	}
	
	if (!success) uCmd.sendMessage("Konnte temperature nicht schreiben","+41787210818",10000);
	else counter = 0;*/
	Serial.println("Starte aufruf");
	request.hostname = "www.geobrowser.ch";
    request.port = 80;
    request.path = "/Plot/UploadDataFromIOT/516/";

    String json = jsonChar;
    json.replace("\"","\\\"");
    String end = String("{\"json\":\"" + json + "\"}");
    request.body = end;
    Serial.println(end);
   // request.body = sprintf("{\"json\":\"%s\"}",jsonChar);
    for (int i=0; i<5; i++) {
        http.get(request, response, headers);
        if (response.body=="\"OK\"") i=5;
    }
    Serial.println("Aufruf erfolgreich");
    Serial.println(response.body);
    counter=0;
    
	//Particle.disconnect();
	//Cellular.off();
}


void smsRecvCheck()
{
    bool putOnline = false;
    bool sendSMS = false;
    bool deleteSMS = false;
    bool reset = false;
    //String sendBackTo = "";
    char sendBackTo[20];
    Cellular.on();
	Cellular.connect();
	waitUntil(Cellular.ready);
        int i;
        // read next available messages
        if(uCmd.checkMessages(10000) == RESP_OK){
             //Serial.printf("checkmess");
            uCmd.smsPtr = uCmd.smsResults;
            for(i=0;i<uCmd.numMessages;i++){
                 //Serial.printf("mess");
                messageText = String(uCmd.smsPtr->sms);
                messageText = messageText.toLowerCase();
                phoneReturn = String(uCmd.smsPtr->phone);
                phoneReturn = phoneReturn.trim();
                 //Serial.printf(messageText);
                //processMessage(messageText, phoneReturn);   ///---This is the line that will let us process the message. Note each message is processed 1 at a time. So if a message spans two SMS's it may not be picked up as a 'single' SMS. So keep the messages short and less than 106characters.
                if (messageText=="putonline") {
    			    putOnline=true;

    			}
    			if (messageText=="delete") {
    			    deleteSMS=true;

    			}
    			if (messageText=="reset") {
    			    reset=true;

    			}
    			if (messageText == "sms") {
    			    sendSMS = true;
    			    for(int i=0; i<20; i++)
                    {
                      sendBackTo[i] = uCmd.smsPtr->phone[i];
                    }
    			    //sendBackTo = uCmd.smsPtr->phone;
    			}
                
                uCmd.smsPtr++;
            }
        }
        //----Delete SMS's----
        //We don't want to keep processing the same messages again and again. So delete them from the buffer after we processed them.
        //The buffer will fill up eventually if we don't.. It is unknown what happens when the buffer fills up.
        Serial.printf("finitto");
        
        uCmd.smsPtr = uCmd.smsResults;
        
        for(i=0;i<uCmd.numMessages;i++){
            if(uCmd.deleteMessage(uCmd.smsPtr->mess,10000) == RESP_OK)
            {
                Serial.println("message deleted successfully");
            }
            else 
            {
                Serial.println("could not delete message");  //You will see this message occur for the 'last' empty message every time. 
            }
            uCmd.smsPtr++;
        }
        
        
        if (sendSMS) {
            double tempC1 = sensor.celsius();
            if (tempC1 == 0.0)
    		{
    			delay(1000);
    			tempC1 = sensor.celsius();
    		}
    		for (int i = 0; i<5; i++) {
		        if (sensor2.read()) i=6;
		    }
    		double tempC3 = sensor2.celsius();
            if (tempC3 == 0.0)
    		{
    			delay(1000);
    			tempC3 = sensor2.celsius();
    		}
            bat = fuel.getSoC();
            char smstext[160];
            char numberChar[12]; //+41787210818
            //sendBackTo.toCharArray(numberChar,12);
            sprintf(smstext,"T1: %.2f T2: %.2f Bat: %0.3f %d",tempC1, tempC3, bat, Version);
            uCmd.sendMessage(smstext,sendBackTo,10000);
        }
        if (deleteSMS) {
            deleteSMSOnStart();
        }
        if (reset) {
            System.reset();
        }
        
        if (putOnline) {
            Serial.printf("putonline");
    			    Cellular.on();
		    	    delay(1000);
                	Cellular.connect();
                	waitUntil(Cellular.ready);
                
                	//Particle.connect();
                
                	Particle.connect();
                	waitUntil(Particle.connected);
                	Particle.process();
                	
                	while(int i = 1) {
                	    delay(1000);
                	    Particle.connect();
                	    Particle.process();
                	}
    
    			    
    			}
            
}


void deleteSMSOnStart()
{
    Cellular.on();
    delay(1000);
	Cellular.connect();
	waitUntil(Cellular.ready);
    int i;
    int messages = uCmd.numMessages;
    if (messages==0) messages=1;
    
    for(i=0;i<messages;i++){
            if(uCmd.deleteMessage(uCmd.smsPtr->mess,10000) == RESP_OK)
            {
                Serial.println("message deleted successfully");
            }
            else 
            {
                Serial.println("could not delete message"); //You will see this message occur for the 'last' empty message every time. 
            }
            uCmd.smsPtr++;
        }
}

void smsRecvFlag(void* data, int index)
{
	smsAvailableFlag = 1;
}

void printDebugInfo()
{
	// If there's an electrical error on the 1-Wire bus you'll get a CRC error
	// Just ignore the temperature measurement and try again
	if (sensor.crcError())
	{
		Serial.print("CRC Error ");
	}

	// Print the sensor type
	const char *type;
	switch(sensor.type())
	{
		case WIRE_DS1820:
			type = "DS1820";
			break;
		case WIRE_DS18B20:
			type = "DS18B20";
			break;
		case WIRE_DS1822:
			type = "DS1822";
			break;
		case WIRE_DS2438:
			type = "DS2438";
			break;
		default:
			type = "UNKNOWN";
			break;
	}
	Serial.print(type);

	// Print the ROM (sensor type and unique ID)
	uint8_t addr[8];
	sensor.addr(addr);
	Serial.printf(
	    " ROM=%02X%02X%02X%02X%02X%02X%02X%02X",
	    addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]
	);

	// Print the raw sensor data
	uint8_t data[9];
	sensor.data(data);
	Serial.printf(
	    " data=%02X%02X%02X%02X%02X%02X%02X%02X%02X",
	    data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]
	);
}
