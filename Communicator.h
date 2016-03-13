#include "Arduino.h"
#include <Servo.h>
#include "Adafruit_GPS.h" 

//Drop Bay Servo Details. UPDATE THESE FOR 2016!!
#define DROP_BAY_CLOSED 1100
#define DROP_BAY_OPEN 1900

//MESSAGE CONSTANTS
#define MESSAGE_START       's' 
#define MESSAGE_READY       'r'
#define MESSAGE_DROP        'd'
#define MESSAGE_RESET_AKN   'k'
#define MESSAGE_RESTART_AKN 'q'
#define MESSAGE_CAM_RESET   'x'
#define MESSAGE_ERROR       'e'
#define MESSAGE_DROP_ACK    'y'

//MPU6050 messages
#define MPU6050_READY         '1' 
#define MPU6050_FAILED        '2'
#define MPU6050_DMP_READY     '3'
#define MPU6050_DMP_FAILED    '4'
#define MPU6050_INITIALIZING  '5'

//define Servo pins 
#define DROP_PIN 2


#define XBEE_BAUD 57600
#define SERIAL_USB_BAUD 9600   //this actually doesn't matter - over USB it defaults to some high baudrate
#define XBEE_SERIAL Serial3  //or whatever it's on

//GPS constants
#define MAXLINELENGTH 120
#define GPS_SERIAL Serial1   //or whatever serial it's on
#define GPS_BAUD 9600



class Communicator {
private:
	int dropServoPin;
	int dropBayServoPos;


	double altitudeAtDrop;

	Servo dropServo;

    int dropBayAttached;
       
    boolean dropBayOpen;
    unsigned long closeDropBayTime;
    unsigned long dropBayDelayTime;
	
	//These functions help accomplish the enterBypass mode function
	void sendBypassCommand();
	boolean checkInBypassMode();
	void flushInput();
	int flushInputUntilCurrentTime();
	boolean delayUntilSerialData(unsigned long ms_timeout);
	
        
        
public:

    double altitude, roll, pitch;

	Communicator();
    ~Communicator();
    void initialize();


	int getDropPin();
    int waiting_for_message = false;
    int calibration_flag = false;

    boolean reset;
    boolean restart;
	
	//gps variables and functions
	Adafruit_GPS GPS;
	char nmeaBuf[MAXLINELENGTH];  
	int nmeaBufInd = 0;
	boolean newParsedData = false;
	void getSerialDataFromGPS();
	void setupGPS();
	
	
	//functions called by main program each loop
	void recieveCommands();  //when drop command is received set altitude at drop
	void sendData();  //send current altitude, altitude at drop, roll, pitch, airspeed
	
	//functions to attach sensors and motors	
    void attachDropBay(int _dropServoPin);
        
    //function to send standard message to ground station
    //examples: START, READY, RESET AKNOLAGED, DROP
    void sendMessage(char message); //takes single standard character that is a code for standard message - see definitions above
    
	//Ensure the XBee enters Bypass mode. If this fails, ALL communication will be non-functional
	boolean enterBypass();    
        
	//Calibration code  (no longer used but left for reference)
	//void calibrate();

};
