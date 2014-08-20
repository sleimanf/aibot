//TODO:
//Create threads and message passing
//Create a face recognition thread that talks to STM Thread
//Write the code in main to read the face detected and track it using the robot structure's neck
//
//
//Create a class called SpeechSynthesizer that intiailzes itself in main
//Speech Synthesizer must have a function Speak(text, volume, speed)
//
//

#include <iostream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <thread>
#include <string>
#include <sstream>
#include <vector>
#include <math.h>

//OpenCV includes, this is for face recognition to work
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

//Windows Socket Includes for the server
#define WIN32_LEAN_AND_MEAN

#include <WinSock2.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")

//Microsoft Speech API (for Text-To-Speech (TTS) Engine)
#include <sapi.h>

//AIPU Structures
#include "ShortTermMemory.h"
#include "RobotStructure.h"
#include "TTSTcpClient.h"


using namespace std;
using namespace std::chrono;
using namespace cv;

//Comment out "#define DEBUGGING_ENABLED" to disable debugging
#define DEBUGGING_ENABLED
#ifdef DEBUGGING_ENABLED //if variable "#define DEBUGGING_ENABLED" is defined, then enable debugging
#define DEBUG_MSG(str) do { cout << str ; } while( false )
#else
#define DEBUG_MSG(str) do { } while( false )
#endif

#pragma region VARIABLES DECLARATION

ShortTermMemory *AIPU_STM;
RobotStructure * robot;

//Global Variables for Face Recognition Functional Block
const int CAPTUREWIDTH = 640; //in pixels
const int CAPTUREHEIGHT = 480; //in pixels
const double XCAPTUREVIEWINGANGLE = 78;
const double YCAPTUREVIEWINGANGLE = 45;
//

//Global Variables defining App Communicator socket server parameters
#define DEFAULT_APP_COMMUNICATOR_PORT 27005
#define DEFAULT_APP_COMMUNICATOR_BUFFER_LENGTH    512
//

//Global Variable for Text-To-Speech TTS Engine
//Why are we running TTS as a server so it wont hang the main program, and becuase it might run on a diff. machine in the future
#define DEFAULT_TTS_ENGINE_PORT 27006
#define DEFAULT_TTS_ENGINE_BUFFER_LENGTH    512

#pragma endregion VARIABLES DECLARATION

#pragma region STM FUNCTIONS DECLARATION

void printSTMRecordType(STMRecord * inputRecord) //This is a test function that simply prints the type of Record
{
	if (inputRecord == NULL)
	{
		DEBUG_MSG("**POINTER TO NULL**" << endl);
		return;
	}
	else
	{
		DEBUG_MSG(inputRecord->getRecordTypeString() << endl);
	}
}

vector<string> getSubStrings(string input , char delimiter) //Returns a vector of substrings from the input string based on a delimter, for example to separate Hello,1,1 by the delimiter ","
{
	istringstream ss(input);
	string token;
	vector<string> returnedVectorofStrings; // Empty on creation
	while (std::getline(ss, token, delimiter)) {
		returnedVectorofStrings.push_back(token);
	}

	return returnedVectorofStrings;
}

#pragma endregion STM FUNTIONS DECLARATION

#pragma region FUNCTIONALBLOCK1 FACE RECOGNITION
double getFaceDistance(int faceWidth, int faceHeight)
{
	return ((double)CAPTUREWIDTH / faceWidth) * (double)18;

	//NOTE: if a new camera is used, the formula must be re-calibrated
	//FORMULA: f = d*Z / D
	//d = faceWidth in Pixels
	//Z = Distance to Face
	//D = Actual Face Width
	//f = Capture Width in Pixels
}

void faceRecognizer()
{
	//create the cascade classifier object used for the face detection
	CascadeClassifier face_cascade;
	//use the haarcascade_frontalface_alt.xml library
	face_cascade.load("..\\..\\..\\config\\Opencv\\haarcascade_frontalface_alt.xml");

	//setup video capture device and link it to the first capture device
	VideoCapture captureDevice;

	captureDevice.open(0);
	//Set Dimensions
	captureDevice.set(CV_CAP_PROP_FRAME_HEIGHT, CAPTUREHEIGHT);
	captureDevice.set(CV_CAP_PROP_FRAME_WIDTH, CAPTUREWIDTH);

	//setup image files used in the capture process
	Mat captureFrame;
	Mat grayscaleFrame;

	//create a window to present the results
	namedWindow("outputCapture", 1);

	//create a loop to capture and find faces
	while (true)
	{
		//Why capture multiple frames, testing revealed that if this is not done, we do not get the current capture from the cam
		//capture a new image frame
		captureDevice >> captureFrame;
		//capture a new image frame
		captureDevice >> captureFrame;
		//capture a new image frame
		captureDevice >> captureFrame;
		//capture a new image frame
		captureDevice >> captureFrame;
		//capture a new image frame
		captureDevice >> captureFrame;
		//capture a new image frame
		captureDevice >> captureFrame;

		//convert captured image to gray scale and equalize
		cvtColor(captureFrame, grayscaleFrame, CV_BGR2GRAY);
		equalizeHist(grayscaleFrame, grayscaleFrame);

		//create a vector array to store the face found
		std::vector<Rect> faces;

		//find faces and store them in the vector array
		face_cascade.detectMultiScale(grayscaleFrame, faces, 1.1, 3, CV_HAAR_FIND_BIGGEST_OBJECT | CV_HAAR_SCALE_IMAGE, Size(35, 35));


		//draw a rectangle for all found faces in the vector array on the original image
		for (int i = 0; i < faces.size(); i++)
		{
			Point pt1(faces[i].x + faces[i].width, faces[i].y + faces[i].height);
			Point pt2(faces[i].x, faces[i].y);

			rectangle(captureFrame, pt1, pt2, cvScalar(0, 255, 0, 0), 1, 8, 0);
		
			FaceDetectionRecord *recognizedFace = new FaceDetectionRecord(faces[i].width, faces[i].height, faces[i].x + (faces[i].width / 2), faces[i].y + (faces[i].height / 2));
			AIPU_STM->inputRecord(static_cast<STMRecord*>(recognizedFace));

		}

		//print the output
		imshow("outputCapture", captureFrame);

		//pause for X-ms
		waitKey(10);
	}
}
#pragma endregion FUNCTIONALBLOCK1 FACE RECOGNITION

#pragma region FUNCTIONALBLOCK2 REBECCA AIML Parser

#pragma endregion FUNCTIONALBLOCK2 REBECCA AIML Parser


#pragma region SOCKET SERVER FOR APP COMMUNICATOR
int socketServer()
{
	WSADATA wsa;
	SOCKET serverSocket, clientSocket;
	struct sockaddr_in server, client;
	int c;
	char *message;

	DEBUG_MSG("Initializing App Communicator Socket..." << endl);
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (iResult != 0)
	{
		DEBUG_MSG("Failed. Error Code : " << WSAGetLastError() << endl);
		return 1;
	}

	DEBUG_MSG("App Communicator Socket Initialized" << endl);
	//Create a socket
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		DEBUG_MSG("Could Not Create App Communicator Socket : " << WSAGetLastError() << endl);
	}

	DEBUG_MSG("App Communicator Socket Created.\n");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(DEFAULT_APP_COMMUNICATOR_PORT);

	//Bind
	if (::bind(serverSocket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR)
	{
		DEBUG_MSG("Bind failed for App Communicator Socket with error code : " << WSAGetLastError() << endl);
		exit(EXIT_FAILURE);
	}

	DEBUG_MSG("Bind App Communicator Socket Done" << endl);

	
	while (true)
	{
		//Listen to incoming connections
		listen(serverSocket, 3);

		//Accept and incoming connection
		DEBUG_MSG("Waiting for incoming connections on App Communicator Socket..." << endl);

		c = sizeof(struct sockaddr_in);
		clientSocket = accept(serverSocket, (struct sockaddr *)&client, &c);

		if (clientSocket != INVALID_SOCKET)
		{
			DEBUG_MSG("Connection accepted on App Communicator Socket" << endl;);

			char recvbuf[DEFAULT_APP_COMMUNICATOR_BUFFER_LENGTH];
			int iSendResult;

			// reveice until the client shutdown the connection
			do {
				iResult = recv(clientSocket, recvbuf, DEFAULT_APP_COMMUNICATOR_BUFFER_LENGTH, 0);
				if (iResult > 0)
				{
					char msg[DEFAULT_APP_COMMUNICATOR_BUFFER_LENGTH];
					memset(&msg, 0, sizeof(msg));
					strncpy_s(msg, recvbuf, iResult);

					DEBUG_MSG("Received: " << msg << endl);

					AppCommanderRecord *inputCommand = new AppCommanderRecord(msg);
					AIPU_STM->inputRecord(static_cast<STMRecord*>(inputCommand));

					iSendResult = send(clientSocket, recvbuf, iResult, 0);

					if (iSendResult == SOCKET_ERROR)
					{
						DEBUG_MSG("send failed: " << WSAGetLastError() << endl);
						closesocket(clientSocket);
						WSACleanup();
						return 1;
					}

					DEBUG_MSG("Bytes sent: " << iSendResult << endl);
				}
				else if (iResult == 0)
				{
					DEBUG_MSG("Connection closed\n");
				}
				else
				{
					DEBUG_MSG("recv failed: " << WSAGetLastError() << endl);
					//closesocket(serverSocket);
					//WSACleanup();
				}
			} while (iResult > 0);
		}
	}
	return 1;
}
#pragma endregion SOCKET SERVER FOR APP COMMUNICATOR


void STMServerThread()
{
	//Wait for incoming connections from Functional Blocks
	STMRecordType incomingConnection = STMRecordType::SPEECH_INPUT; //these are temp until actual code is written
	string inputFromFunctionalBlock;  //these are temp until actual code is written

	switch (incomingConnection)
	{
		case STMRecordType::SPEECH_INPUT:
		{
			DEBUG_MSG("SPEECH_INPUT" << endl);
			vector<string> serverInputSubStrings = getSubStrings(inputFromFunctionalBlock, ','); //convert comma separated input into usable substrings
			if (serverInputSubStrings.size() != 3)
			{
				DEBUG_MSG("SYNTAX RECEIVED FROM FUNCTIONAL BLOCK IS INCORRECT!" << endl);
				break;
			}
			string transcriptionText = serverInputSubStrings[0]; 
			int transcriptionAccuracy = atoi(serverInputSubStrings[1].c_str());
			int transcriptionLoudness = atoi(serverInputSubStrings[2].c_str());
			//SpeechRecord *incomingSpeech = new SpeechRecord(transcriptionText, transcriptionAccuracy, transcriptionLoudness); // get input and create 
			//AIPU_STM->inputRecord(static_cast<STMRecord*>(incomingSpeech));
			
			break;
		}
		case STMRecordType::FACE_DETECTION:
			DEBUG_MSG("FACE_DETECTION" << endl);
			break;
		case STMRecordType::SOUND_DETECTION:
			DEBUG_MSG("SOUND_DETECTION" << endl);
			break;
		case STMRecordType::SKELETAL_DETECTION:
			DEBUG_MSG("SKELETAL_DETECTION" << endl);
			break;
		case STMRecordType::THERMAL_DETECTION:
			DEBUG_MSG("THERMAL_DETECTION" << endl);
			break;
		case STMRecordType::HAND_GESTURE:
			DEBUG_MSG("HAND_GESTURE" << endl);
			break;
		case STMRecordType::APP_COMMANDER:
			DEBUG_MSG("APP_COMMANDER" << endl);
			break;
		case STMRecordType::SLAM:
			DEBUG_MSG("SLAM" << endl);
			break;
		case STMRecordType::OBJECTIVE:
			DEBUG_MSG("OBJECTIVE" << endl);
			break;
		case STMRecordType::OBSTACLE_DETECTION:
			DEBUG_MSG("OBSTACLE_DETECTION" << endl);
			break;
		case STMRecordType::OBJECT_RECOGNITION:
			DEBUG_MSG("OBJECT_RECOGNITION" << endl);
			break;
		case STMRecordType::FACE_RECOGNITION:
			DEBUG_MSG("FACE_RECOGNITION" << endl);
			break;
		default:
			DEBUG_MSG("UNRECOGNIZED STM RECORD TYPE" << endl);
			break;
	}
	

	//Process Data and Put into STM

}


#pragma region Text-To-Speech Engine

std::wstring s2ws(const std::string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

int TTSEngineServer()
{
	ISpVoice * pVoice = NULL;
	if (FAILED(::CoInitialize(NULL)))
		return FALSE;

	HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);

	if (SUCCEEDED(hr))
	{
		DEBUG_MSG("Speech Initialized" << endl);
	}
	else
	{
		return 1;
	}

	string str = "Robot Online";
	pVoice->Speak(s2ws(str).c_str(), SPF_DEFAULT, NULL);
	pVoice->WaitUntilDone(15000);

	WSADATA wsa;
	SOCKET serverSocket, clientSocket;
	struct sockaddr_in server, client;
	int c;
	char *message;

	DEBUG_MSG("Initializing TTS Engine Socket..." << endl);
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (iResult != 0)
	{
		DEBUG_MSG("Failed. Error Code : " << WSAGetLastError() << endl);
		return 1;
	}

	DEBUG_MSG("TTS Engine Socket Initialized" << endl);

	//Create a socket
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		DEBUG_MSG("Could Not Create TTS Engine Socket : " << WSAGetLastError() << endl);
	}

	DEBUG_MSG("TTS Engine Socket Created.\n");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(DEFAULT_TTS_ENGINE_PORT);

	//Bind
	if (::bind(serverSocket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR)
	{
		DEBUG_MSG("Bind failed for TTS Engine Socket with error code : " << WSAGetLastError() << endl);
		exit(EXIT_FAILURE);
	}

	DEBUG_MSG("Bind TTS Engine Socket Done" << endl);
	while (true)
	{
		//Listen to incoming connections
		listen(serverSocket, 3);

		//Accept and incoming connection
		DEBUG_MSG("Waiting for incoming connections on TTS Engine Socket..." << endl);

		c = sizeof(struct sockaddr_in);
		clientSocket = accept(serverSocket, (struct sockaddr *)&client, &c);

		if (clientSocket != INVALID_SOCKET)
		{
			DEBUG_MSG("Connection accepted on TTS Engine Socket" << endl;);

			char recvbuf[DEFAULT_TTS_ENGINE_BUFFER_LENGTH];
			int iSendResult;

			// reveice until the client shutdown the connection
			do {
				iResult = recv(clientSocket, recvbuf, DEFAULT_TTS_ENGINE_BUFFER_LENGTH, 0);
				if (iResult > 0)
				{
					char msg[DEFAULT_TTS_ENGINE_BUFFER_LENGTH];
					memset(&msg, 0, sizeof(msg));
					strncpy_s(msg, recvbuf, iResult);

					DEBUG_MSG("TTS MSG RECEIVED: " << msg << endl);

					string str(msg);
					pVoice->Speak(s2ws(str).c_str(), SPF_DEFAULT, NULL);
					pVoice->WaitUntilDone(15000);
				}
				else if (iResult == 0)
					DEBUG_MSG("Connection closed\n");
				else
				{
					DEBUG_MSG("recv failed: " << WSAGetLastError() << endl);
					//closesocket(serverSocket);
					//WSACleanup();
				}
			} while (iResult > 0);
		}
	}
	return 1;
}

#pragma endregion Text-To-Speech Engine




int main() {

	//Initialize Functional Blocks, Represented in Code as Threads

	AIPU_STM = new ShortTermMemory(); // Initialize Short Term Memory Structure

	//Initialize the state of the AIPU to IDLE
	AIPU_STM->setCurrentAIPUState(AIPUSTATE::IDLE);
    
    //Create robot structure of servos for control
	robot = new RobotStructure();
	
	//Start the Face Recognition functional blocks
	std::thread t1(faceRecognizer); //Face Recognition Engine
	std::thread t2(socketServer); //APP Communicator
	std::thread t3(TTSEngineServer); //Text-To-Speech Engine

	//std::this_thread::sleep_for(std::chrono::milliseconds(10000000));
    
    //Start State Machine
    //Read Data from STM (Short-Term-Memory) based on State
	//Process STM Data
	//Take Action
	//
	//Important Rule for STM, in any state besides IDLE, once a positive record is hit, you will most likely need to
	//re-set the AIPU state, otherwise it could timeout (depending on the code)
   
	while (true)
	{
		//std::this_thread::sleep_for(std::chrono::milliseconds(10));
		switch (AIPU_STM->getCurrentAIPUState())
		{
			case AIPUSTATE::IDLE:
			{
				//DEBUG_MSG("IDLE STATE" << endl);
				/*
				if a face is detected move to engaged in convo within 1-2 sec
				if kickoff phrase received in good accuracy then go to engaged in convo
				*/
				FaceDetectionRecord *detectedFaceRecord = NULL;

				STMRecord *tempReadRecord;
				for (int i = 0; i < AIPU_STM->getSTMSize(); i++)
				{
					tempReadRecord = AIPU_STM->getSTMRecord(i);
					if (tempReadRecord == NULL || tempReadRecord->processed)
						continue;
					if (tempReadRecord->recordType == STMRecordType::FACE_DETECTION) //code must block here otherwise a combo serial number for record must be created this will conmbine type with record number
					{
						detectedFaceRecord = static_cast<FaceDetectionRecord*>(tempReadRecord);
						detectedFaceRecord->markAsProcessed(); 
						AIPU_STM->setCurrentAIPUState(AIPUSTATE::ENGAGED_IN_COVERSATION);
						DEBUG_MSG("READ RECORD: " << detectedFaceRecord->faceXCoordinate << "," << detectedFaceRecord->faceYCoordinate << endl);
								
						break; //this record would be the current state, the reason is that our STM is a stack, Last in First Out
					}
				}
				break;
			}
			case AIPUSTATE::SOUND_DETECTED:
			{
				break;
			}
			case AIPUSTATE::LOCATING_SPEAKER:
			{
				/*
				if a face is detected move to engaged in convo
				*/
				break;
			}
			case AIPUSTATE::AWAITING_KICKOFF_PHRASE:
			{
				/*
				if a face is detected move to engaged in convo within 1-2 sec
				if a hand gesture is detected within 1-2 sec then move to in convo
				if kickoff phrase received in good accuracy then go to engaged in convo
				*/
				break;
			}
			case AIPUSTATE::ENGAGED_IN_COVERSATION:
			{
				/*
				no speech input in 5 seconds then go back to idle
				if no eye contact in the last 5 seconds then go back to idle
				if sound is detected from another source then ignore it
				keep track of face and maintain eye contact with it
				*/
				STMRecord *tempReadRecord;
				for (int i = 0; i < AIPU_STM->getSTMSize(); i++)
				{
					tempReadRecord = AIPU_STM->getSTMRecord(i);
					if (tempReadRecord == NULL || tempReadRecord->processed || (tempReadRecord->getTimeRelativeToNowInMilliSeconds() > AIPU_STM->getLastAIPUStateChangeTimeRelativeToNowInMilliSeconds()))
						continue;
					if (tempReadRecord->recordType == STMRecordType::FACE_DETECTION ) //code must block here otherwise a combo serial number for record must be created this will conmbine type with record number
					{
						FaceDetectionRecord * inputRecord = static_cast<FaceDetectionRecord*>(tempReadRecord);
						inputRecord->markAsProcessed();
						AIPU_STM->setCurrentAIPUState(AIPUSTATE::ENGAGED_IN_COVERSATION); //reset current state
						DEBUG_MSG("READING RECORD: " << inputRecord->faceXCoordinate << "," << inputRecord->faceYCoordinate << endl);
						/*
						STEPS FOR CALCULATING ANGLE SHIFT for Servo Motor
						1-getFaceDistance() function must be calibrated to get an accurate measurement of planar distance to the face
						2-Shift from center camera must be converted to cm using this formula
						Shift in cm = (tan(camera viewing angle/2) x (planar distance to face in cm) ) x Center ShiftX / (CAPTUREWIDTH/2)
						3-atan(Shift from center / planar distance to face) will give us the angle shift
						NOTE: this result will be in radian
						4-position to set the servo to must be set using the following formula (assuming 512 is center position)
						position to set = (angle calculated in step3 / 1.57) x (unsigned additional servo int value at a 90degree angle)
						the value represents 90degrees converted to radian
						*/

						//CALCULATIONS STEP 1
						double centerShiftX = -(inputRecord->faceXCoordinate + (inputRecord->faceWidth / 2) - (CAPTUREWIDTH / 2));
						double centerShiftY = -(inputRecord->faceYCoordinate + (inputRecord->faceHeight / 2) - (CAPTUREHEIGHT / 2));
						double planarFaceDistance = getFaceDistance(inputRecord->faceWidth, inputRecord->faceHeight);
						DEBUG_MSG("Approx. Distance in cm: " << planarFaceDistance << endl);


						//CALCULATIONS STEP 2
						//note we are multiplying the angel by pi and divided by 180 because we need to work in radians
						double centerShiftXinCM = tan((XCAPTUREVIEWINGANGLE / 2) *3.14159265 / 180) * planarFaceDistance * centerShiftX / (CAPTUREWIDTH / 2);
						double centerShiftYinCM = tan((YCAPTUREVIEWINGANGLE / 2) *3.14159265 / 180) * planarFaceDistance * centerShiftY / (CAPTUREHEIGHT / 2);

						//CALCULATIONS STEP 3
						double angleShiftX = atan(centerShiftXinCM / planarFaceDistance);
						double angleShiftY = atan(centerShiftYinCM / planarFaceDistance);

						//CALCULATIONS STEP 4
						int XServoPositiontoSet = ((angleShiftX / 1.57) * 310);
						int YServoPositiontoSet = ((angleShiftY / 1.57) * 310);

						if (-0.07 > angleShiftX && angleShiftX > 0.07)
						{
						}
						else
						{
							//Note servos do not have overload management, so if you have a bracket and try to set an unreachable angel without setting limits, the servo will most definitely go to overload
							robot->Neck->panServo->setPosition(XServoPositiontoSet + robot->Neck->panServo->position, 128); //;1023 will never be achieved, it will only go to the limit.
						}
						if (-0.02 < angleShiftY && angleShiftY < 0.02)
						{
						}
						else
						{
							robot->Neck->tiltServo->setPosition(YServoPositiontoSet + robot->Neck->tiltServo->position, 128); //;1023 will never be achieved, it will only go to the limit.
						}
						do
						{
							robot->Neck->panServo->setCurrentParameters();
							DEBUG_MSG("Current (Position,Speed,Load,Temperature): (");
							DEBUG_MSG(robot->Neck->panServo->position);
							DEBUG_MSG("," << robot->Neck->panServo->speed);
							DEBUG_MSG("," << robot->Neck->panServo->load);
							DEBUG_MSG("," << robot->Neck->panServo->temperature << ")" << endl);
							DEBUG_MSG("Comm Status: " << robot->Neck->panServo->checkCommStatus() << endl);
						} while (robot->Neck->panServo->isMoving());

						do
						{
							robot->Neck->tiltServo->setCurrentParameters();
							DEBUG_MSG("Current (Position,Speed,Load,Temperature): (");
							DEBUG_MSG(robot->Neck->tiltServo->position);
							DEBUG_MSG("," << robot->Neck->tiltServo->speed);
							DEBUG_MSG("," << robot->Neck->tiltServo->load);
							DEBUG_MSG("," << robot->Neck->tiltServo->temperature << ")" << endl);
							DEBUG_MSG("Comm Status: " << robot->Neck->tiltServo->checkCommStatus() << endl);
						} while (robot->Neck->tiltServo->isMoving());

						break; //this record would be the current state, the reason is that our STM is a stack, Last in First Out
					}
					else if (tempReadRecord->recordType == STMRecordType::APP_COMMANDER) //code must block here otherwise a combo serial number for record must be created this will conmbine type with record number
					{
						AppCommanderRecord* inputRecord = static_cast<AppCommanderRecord*>(tempReadRecord);
						inputRecord->markAsProcessed();

						//Here send the text to AIML engine

						string textToSpeak = "You Wrote, " + inputRecord->inputCommand;

						//Send MSG to TTS Engine to Speak the command

						TcpClient client("127.0.0.1");

						if (!client.Start(DEFAULT_TTS_ENGINE_PORT))
						{
							DEBUG_MSG("Error Communicating with TTS Engine");
							break;
						}

						client.Send((char*)textToSpeak.c_str());

						client.Stop();

						AIPU_STM->setCurrentAIPUState(AIPUSTATE::ENGAGED_IN_COVERSATION); //reset current state
					}
				}
				break;
			}
			case AIPUSTATE::CHARGING_BATTERY:
			{
				break;
			}
			case AIPUSTATE::PROCESSING_DATA:
			{
				break;
			}
			default:
			{
					break;
			}

		}
		
	}

	//Clean STM
	
	DEBUG_MSG("Press any Key to Exit" << endl);
	int x;
	std::cin >> x;
	
	return 0;
}












/*
robot->RightArm->Shoulder->enabletorque(false);
robot->RightArm->armTwist->enabletorque(false);
robot->RightArm->Elbow->enabletorque(false);
robot->RightArm->Gripper->enabletorque(false);

while (true)
{


robot->RightArm->Shoulder->setPosition(512, 256);
robot->RightArm->armTwist->setPosition(512, 256);
robot->RightArm->Elbow->setPosition(512, 256);
robot->RightArm->Gripper->setPosition(512, 256);

for (int i = 0; i < 10; i++)
{

int flag = 0;
while (robot->RightArm->Elbow->isMoving())
{
robot->RightArm->Elbow->setCurrentParameters();
if (flag == 0)
{
if (robot->RightArm->Elbow->position < 825)
{
robot->RightArm->Elbow->setPosition(725, 125);
flag = 1;
}
}
}


robot->RightArm->Shoulder->setPosition(512, 256);
robot->RightArm->armTwist->setPosition(815, 256);
robot->RightArm->Elbow->setPosition(845, 256);
robot->RightArm->Gripper->setPosition(722, 256);

flag = 0;
while (robot->RightArm->Elbow->isMoving())
{
robot->RightArm->Elbow->setCurrentParameters();
if (flag == 0)
{
if (robot->RightArm->Elbow->position > 750)
{
robot->RightArm->Elbow->setPosition(845, 125);
flag = 1;
}
}
}

robot->RightArm->Shoulder->setPosition(512, 256);
robot->RightArm->armTwist->setPosition(815, 256);
robot->RightArm->Elbow->setPosition(725, 256);
robot->RightArm->Gripper->setPosition(395, 256);
}

while (robot->RightArm->isMoving())
{
}

robot->RightArm->Shoulder->setPosition(512, 256);
robot->RightArm->armTwist->setPosition(512, 256);
robot->RightArm->Elbow->setPosition(512, 256);
robot->RightArm->Gripper->setPosition(512, 256);

//robot->RightArm->setCurrentParameters();
//cout << robot->RightArm->Shoulder->position << ",";
//cout << robot->RightArm->armTwist->position << ",";
//cout << robot->RightArm->Elbow->position << ",";
//cout << robot->RightArm->Gripper->position << endl;


std::this_thread::sleep_for(std::chrono::milliseconds(2000));

}
*/
