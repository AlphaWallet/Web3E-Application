#include <APIReturn.h>

#include "esp_wifi.h"
#include <Arduino.h>

#include <UdpBridge.h>
#include <Web3.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WifiServer.h>
#include <functional>
#include <string>
#include <trezor/rand.h>
#include <vector>

#define FRONT_LEFT 14
#define REAR_RIGHT 27
#define REAR_LEFT 13
#define FRONT_RIGHT 12

#define REAR_LEFT_NEG 33
#define FRONT_RIGHT_NEG 32
#define FRONT_LEFT_NEG 26
#define REAR_RIGHT_NEG 25
#define BLUE_LED 2

#define CAR_CONTRACT "0x75cb2cc380d1387a79ee64b1b7c9fa051139a319"

const char *ssid = "<your WiFi SSID>";
const char *password = "<your WiFi password>";

const char *INFURA_HOST = "rinkeby.infura.io";
const char *INFURA_PATH = "/v3/c7df4c29472d4d54a39f7aa78f146853";
int wificounter = 0;
const char *seedWords[] = {"Dogecoin", "Bitcoin", "Litecoin", "EOS", "Stellar", "Nervos", "Huobi", "Qtum", "Dash", "Cardano", "Ethereum", 0};
string currentChallenge;
Web3 *web3;
KeyID *keyID;
UdpBridge *udpConnection;
APIReturn *apiReturn;
std::string validAddress = "";
long motorOffTime;
long LEDOffTime;

const char *apiRoute = "api/";
enum APIRoutes
{
    api_unknown,
    api_getChallenge,
    api_checkSignature, //returns session key
    api_allForward,     //require session key
    api_turnLeft,       //require session key
    api_turnRight,      //require session key
    api_backwards,      //require session key
    api_checkToken,
    api_End
};
std::map<std::string, APIRoutes> s_apiRoutes;

void Initialize()
{
    apiReturn = new APIReturn();
    s_apiRoutes["getChallenge"] = api_getChallenge;
    s_apiRoutes["checkSignature"] = api_checkSignature;
    s_apiRoutes["allForward"] = api_allForward;
    s_apiRoutes["turnLeft"] = api_turnLeft;
    s_apiRoutes["turnRight"] = api_turnRight;
    s_apiRoutes["backwards"] = api_backwards;
    s_apiRoutes["checkToken"] = api_checkToken;
    s_apiRoutes["end"] = api_End;
}

void setup_wifi();
void stop();
void generateSeed(BYTE *buffer);
void updateChallenge();
bool QueryBalance(string *userAddress);
void checkTimeEvents();
void forward(int duration);
void leftForward(int duration);
void rightForward(int duration);
void turnLeft(int duration);
void turnRight(int duration);
void backwards(int duration);
void leftRight();

void setup()
{
    pinMode(FRONT_LEFT, OUTPUT);
    pinMode(FRONT_RIGHT, OUTPUT);
    pinMode(REAR_LEFT, OUTPUT);
    pinMode(REAR_RIGHT, OUTPUT);
    pinMode(FRONT_LEFT_NEG, OUTPUT);
    pinMode(FRONT_RIGHT_NEG, OUTPUT);
    pinMode(REAR_LEFT_NEG, OUTPUT);
    pinMode(REAR_RIGHT_NEG, OUTPUT);
    pinMode(BLUE_LED, OUTPUT);

    Initialize();

    LEDOffTime = 0;
    motorOffTime = 0;

    digitalWrite(BLUE_LED, LOW);

    stop();

    Serial.begin(115200);

    web3 = new Web3(INFURA_HOST, INFURA_PATH);
    keyID = new KeyID(web3);

    setup_wifi();
    updateChallenge();

    udpConnection = new UdpBridge();
    udpConnection->setKey(keyID, web3); //Use the default bridge at james.lug.org.cn
    udpConnection->startConnection();

    //To use your own instance of TokenProxy replace the above 3 lines with:
    //udpConnection = new UdpBridge();
    //udpConnection->setKey(keyID, &web3);
    //udpConnection->setupConnection(<your server name>, <your server's port>);
    //udpConnection->startConnection();
}

void toLowerCase(string &data)
{
    std::for_each(data.begin(), data.end(), [](char &c) {
        c = ::tolower(c);
    });
}

void handleAPI(APIReturn *apiReturn, UdpBridge *udpBridge, int methodId)
{
    //handle the returned API call
    Serial.println(apiReturn->apiName.c_str());
    std::string address;

    switch (s_apiRoutes[apiReturn->apiName.c_str()])
    {
    case api_getChallenge:
        digitalWrite(BLUE_LED, HIGH);
        LEDOffTime = millis() + 500;
        udpBridge->sendResponse(currentChallenge, methodId);
        break;
    case api_checkSignature:
    {
        address = Crypto::ECRecoverFromPersonalMessage(&apiReturn->params["sig"], &currentChallenge);
        boolean hasToken = QueryBalance(&address);
        updateChallenge(); //generate a new challenge after each check
        if (hasToken)
        {
            udpBridge->sendResponse("pass", methodId);
            validAddress = address;
            leftRight();
            digitalWrite(BLUE_LED, HIGH);
            LEDOffTime = millis() + 3000;
        }
        else
        {
            udpBridge->sendResponse("fail: doesn't have token", methodId);
        }
    }
    break;
    case api_allForward:
        address = apiReturn->params["addr"];
        toLowerCase(address);
        if (address.compare(validAddress) >= 0)
        {
            forward(1000);
            udpBridge->sendResponse("going forward", methodId);
        }
        else
        {
            udpBridge->sendResponse("no access", methodId);
        }
        break;
    case api_backwards:
        address = apiReturn->params["addr"];
        toLowerCase(address);
        if (address.compare(validAddress) >= 0)
        {
            backwards(1000);
            udpBridge->sendResponse("going backwards", methodId);
        }
        else
        {
            udpBridge->sendResponse("no access", methodId);
        }
        break;
    case api_turnLeft:
        address = apiReturn->params["addr"];
        toLowerCase(address);
        if (address.compare(validAddress) >= 0)
        {
            turnLeft(1000);
            udpBridge->sendResponse("going left", methodId);
        }
        else
        {
            udpBridge->sendResponse("no access", methodId);
        }
        break;
    case api_turnRight:
        address = apiReturn->params["addr"];
        toLowerCase(address);
        if (address.compare(validAddress) >= 0)
        {
            turnRight(1000);
            udpBridge->sendResponse("going right", methodId);
        }
        else
        {
            udpBridge->sendResponse("no access", methodId);
        }
        break;
    case api_checkToken:
        udpBridge->sendResponse(validAddress.c_str(), methodId);
        break;
    default:
        break;
    }
}

void loop()
{
    setup_wifi(); //ensure we maintain a connection. This may cause the server to reboot periodically, if it loses connection

    delay(10);
    udpConnection->checkClientAPI(&handleAPI);
    checkTimeEvents();
}

void stop()
{
    Serial.println("Stop");
    digitalWrite(FRONT_LEFT, LOW);
    digitalWrite(FRONT_RIGHT, LOW);
    digitalWrite(REAR_LEFT, LOW);
    digitalWrite(REAR_RIGHT, LOW);
    digitalWrite(FRONT_LEFT_NEG, LOW);
    digitalWrite(FRONT_RIGHT_NEG, LOW);
    digitalWrite(REAR_LEFT_NEG, LOW);
    digitalWrite(REAR_RIGHT_NEG, LOW);
}

void forward(int duration)
{
    digitalWrite(FRONT_LEFT, HIGH);
    digitalWrite(FRONT_RIGHT, HIGH);
    digitalWrite(REAR_LEFT, HIGH);
    digitalWrite(REAR_RIGHT, HIGH);

    digitalWrite(FRONT_LEFT_NEG, LOW);
    digitalWrite(FRONT_RIGHT_NEG, LOW);
    digitalWrite(REAR_LEFT_NEG, LOW);
    digitalWrite(REAR_RIGHT_NEG, LOW);
    motorOffTime = millis() + duration;
}

void backwards(int duration)
{
    digitalWrite(FRONT_LEFT, LOW);
    digitalWrite(FRONT_RIGHT, LOW);
    digitalWrite(REAR_LEFT, LOW);
    digitalWrite(REAR_RIGHT, LOW);

    digitalWrite(FRONT_LEFT_NEG, HIGH);
    digitalWrite(FRONT_RIGHT_NEG, HIGH);
    digitalWrite(REAR_LEFT_NEG, HIGH);
    digitalWrite(REAR_RIGHT_NEG, HIGH);
    motorOffTime = millis() + duration;
}

void turnRight(int duration)
{
    digitalWrite(FRONT_LEFT, HIGH);
    digitalWrite(FRONT_RIGHT, LOW);
    digitalWrite(REAR_LEFT, HIGH);
    digitalWrite(REAR_RIGHT, LOW);

    digitalWrite(FRONT_LEFT_NEG, LOW);
    digitalWrite(FRONT_RIGHT_NEG, HIGH);
    digitalWrite(REAR_LEFT_NEG, LOW);
    digitalWrite(REAR_RIGHT_NEG, HIGH);
    motorOffTime = millis() + duration;
}

void turnLeft(int duration)
{
    digitalWrite(FRONT_LEFT, LOW);
    digitalWrite(FRONT_RIGHT, HIGH);
    digitalWrite(REAR_LEFT, LOW);
    digitalWrite(REAR_RIGHT, HIGH);

    digitalWrite(FRONT_LEFT_NEG, HIGH);
    digitalWrite(FRONT_RIGHT_NEG, LOW);
    digitalWrite(REAR_LEFT_NEG, HIGH);
    digitalWrite(REAR_RIGHT_NEG, LOW);
    motorOffTime = millis() + duration;
}

void leftRight()
{
    turnLeft(300);
    delay(300);
    turnRight(300);
    delay(300);
    stop();
}

void checkTimeEvents()
{
    if (motorOffTime != 0 && millis() > motorOffTime)
    {
        motorOffTime = 0;
        stop();
    }
    if (LEDOffTime != 0 && millis() > LEDOffTime)
    {
        LEDOffTime = 0;
        digitalWrite(BLUE_LED, LOW);
    }
}

/* This routine is specifically geared for ESP32 perculiarities */
/* You may need to change the code as required */
/* It should work on 8266 as well */
void setup_wifi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.persistent(false);
        WiFi.mode(WIFI_OFF);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
    }

    wificounter = 0;
    while (WiFi.status() != WL_CONNECTED && wificounter < 20)
    {
        delay(500);
        Serial.print(".");
        wificounter++;
    }

    if (wificounter >= 20)
    {
        Serial.println("Restarting ...");
        ESP.restart(); //targetting 8266 & Esp32 - you may need to replace this
    }

    //esp_wifi_set_max_tx_power(34); //save a little power if your unit is near the router. If it's located away then use 78 - max
    esp_wifi_set_ps(WIFI_PS_NONE);

    delay(10);

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void updateChallenge()
{
    //generate a new challenge
    int size = 0;
    while (seedWords[size] != 0)
        size++;
    Serial.println(size);
    char buffer[32];

    int seedIndex = random(0, size);
    currentChallenge = seedWords[seedIndex];
    currentChallenge += "-";
    long challengeVal = random32();
    currentChallenge += itoa(challengeVal, buffer, 16);
}

bool hasValue(BYTE *value, int length)
{
    for (int i = 0; i < length; i++)
    {
        if ((*value++) != 0)
        {
            return true;
        }
    }

    return false;
}

bool QueryBalance(string *userAddress)
{
    bool hasToken = false;
    // transaction
    const char *contractAddr = CAR_CONTRACT;
    Contract contract(web3, contractAddr);
    string func = "balanceOf(address)";
    string param = contract.SetupContractData(func.c_str(), userAddress);
    string result = contract.ViewCall(&param);

    Serial.println(result.c_str());

    BYTE tokenVal[32];

    //break down the result
    vector<string> *vectorResult = Util::InterpretVectorResult(&result);
    for (auto itr = vectorResult->begin(); itr != vectorResult->end(); itr++)
    {
        Util::ConvertHexToBytes(tokenVal, itr->c_str(), 32);
        if (hasValue(tokenVal, 32))
        {
            hasToken = true;
            Serial.println("Has token");
            break;
        }
    }

    delete (vectorResult);
    return hasToken;
}