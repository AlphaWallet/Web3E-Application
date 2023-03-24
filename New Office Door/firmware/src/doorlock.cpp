#include <Arduino.h>
#include <stdint.h>
#include <august.h>

#include "EEPROM.h"
#include <APIReturn.h>

#include "esp_wifi.h"

#include <Web3.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WifiServer.h>
#include <TcpBridge.h>
#include <string>
#include <vector>
#include <ESPUltralightTimers.h>

typedef struct {
  const char *ssid;
  const char *password;
} WiFiCredentials;

std::vector<WiFiCredentials> wiFiCredentials {
  {"<Your SSID>", "<WiFi Password>"},
  // Note you can add multiple credentials here, the device will try each until it gets a connection
};

#define DOOR_CONTRACT "0xB424e50674a38e83c7Eca39945fe5B45B4cd3705" 
#define DEVICE_PRIVATE_KEY "19204a1ad06894b1bd11483d30f1c9e009daf85a212a36147cf5b2a94b9cd4ca" //NB This private key is intentionally leaked to provide an 'out of the box' build.
                                                                                              //If you use this script, you should generate a new key (eg using MetaMask), copy that key here
                                                                                              //and then update the TokenScript var iotAddr with the corresponding address
                                                                                              //This key is only used for authenticating the IoT device and for comms
                                                                                              //There is no value on any ethereum chain or sidechain associated with this key

#define DISCONNECT_TIMEOUT 45 //45 Seconds disconnect timeout

enum APIRoutes
{
  api_unknown,
  api_getChallenge,
  api_checkSignature,
  api_checkSignatureLock,
  api_checkMarqueeSig,
  api_end
};

enum LockComms
{
  lock_command,
  unlock_command,
  wait_for_command_complete,
  idle
};

std::map<std::string, APIRoutes> s_apiRoutes;

void Initialize()
{
  s_apiRoutes["getChallenge"] = api_getChallenge;
  s_apiRoutes["checkSignature"] = api_checkSignature;
  s_apiRoutes["checkSignatureLock"] = api_checkSignatureLock;
  s_apiRoutes["end"] = api_end;
}

void generateSeed(BYTE* buffer);
void updateChallenge();
void setupWifi();
Web3* web3;
KeyID* keyID;
LockComms lockStatus = idle;

const char* seedWords[] = { "Apples", "Oranges", "Grapes", "DragonFruit", "BreadFruit", "Pomegranate", "Aubergine", "Fungi", "Falafel", "Cryptokitty", "Kookaburra", "Elvis", "Koala", 0 };

string currentChallenge;

TcpBridge* tcpConnection;

const char* apiRoute = "api/";

AugustLock augustLock("78:00:00:00:00:00", "123456789abcdef0123456789abcdef0", 3); // Lock Bluetooth address, lock handshakeKey, lock handshakeKeyIndex

bool isLocked;
void doLockCommand();
void doUnlockCommand();

// These two callback decls required here for the case of mutiple instances of AugustLock.
// It would be required to switch them here based on client address. TODO: It may be possible to fold them into the AugustLock class
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  // If you have multiple locks you'll need to switch here using the service address: pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress()
  augustLock._notifyCB(pRemoteCharacteristic, pData, length);
}

void secureLockCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  augustLock._secureLockCallback(pRemoteCharacteristic, pData, length);
}

void connectCallback(const char* msg, bool status)
{
  Serial.print("Connect Callback: ");
  Serial.println(msg);
  isLocked = status;
}

void disConnectCallback()
{
  Serial.println("Disconnect complete");
  augustLock.blankClient();
  lockStatus = idle;
}

bool QueryBalance(const char* contractAddr, std::string* userAddress)
{
  // transaction
  bool hasToken = false;
  Contract contract(web3, contractAddr);
  string func = "balanceOf(address)";
  string param = contract.SetupContractData(func.c_str(), userAddress);
  string result = contract.ViewCall(&param);

  Serial.println(result.c_str());

  // break down the result
  long long baseBalance = web3->getLongLong(&result);

  Serial.println(baseBalance);

  if (baseBalance > 0)
  {
    hasToken = true;
    Serial.println("Has token");
  }

  return hasToken;
}

std::string handleTCPAPI(APIReturn* apiReturn)
{
  Serial.println(apiReturn->apiName.c_str());
  string address;

  switch (s_apiRoutes[apiReturn->apiName])
  {
  case api_getChallenge:
    Serial.println(currentChallenge.c_str());
    return currentChallenge;
  case api_checkSignature:
  {
    Serial.print("Sig: ");
    Serial.println(apiReturn->params["sig"].c_str());
    address = Crypto::ECRecoverFromPersonalMessage(&apiReturn->params["sig"], &currentChallenge);
    int unlockSeconds = strtol(apiReturn->params["openTime"].c_str(), NULL, 10);
    Serial.print("EC-Addr: ");
    Serial.println(address.c_str());
    boolean hasToken = QueryBalance(DOOR_CONTRACT, &address);
    updateChallenge(); // generate a new challenge after each check
    if (hasToken)
    {
      lockStatus = unlock_command;
      return string("pass");
    }
    else
    {
      return string("fail");
    }
  }
  break;
  case api_checkSignatureLock:
  {
    Serial.print("Sig: ");
    Serial.println(apiReturn->params["sig"].c_str());
    address = Crypto::ECRecoverFromPersonalMessage(&apiReturn->params["sig"], &currentChallenge);
    int unlockSeconds = strtol(apiReturn->params["openTime"].c_str(), NULL, 10);
    Serial.print("EC-Addr: ");
    Serial.println(address.c_str());
    boolean hasToken = QueryBalance(DOOR_CONTRACT, &address);
    updateChallenge(); // generate a new challenge after each check
    if (hasToken)
    {
      lockStatus = lock_command;
      return string("pass");
    }
    else
    {
      return string("fail");
    }
  }
  break;
  case api_unknown:
  case api_end:
    break;
  }

  return string("");
}

void setup()
{
  Serial.begin(115200);
  Initialize();
  augustLock.init();
  delay(100);
  setupWifi();
  web3 = new Web3(MUMBAI_TEST_ID);
  keyID = new KeyID(web3, DEVICE_PRIVATE_KEY);
  updateChallenge();

  tcpConnection = new TcpBridge();
  tcpConnection->setKey(keyID, web3);
  tcpConnection->startConnection();

  Serial.println();
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());
}

void loop()
{
  switch (lockStatus)
  {
    case lock_command:
      lockStatus = wait_for_command_complete;
      doLockCommand();
      break;
    case unlock_command:
      lockStatus = wait_for_command_complete;
      doUnlockCommand();
      break;
    case wait_for_command_complete:
      augustLock.checkStatus(); // required in loop() to handle lock comms, in case of connection failure
      break;
    case idle:
    default:
      setupWifi(); // ensure we maintain a connection. This may cause the server to reboot periodically, if it loses connection
      tcpConnection->checkClientAPI(&handleTCPAPI);
      break;
  }
  
  delay(100);
}

void lockTimeout()
{
  if (lockStatus == wait_for_command_complete)
  {
    Serial.println("lockTimeout - Reset device");
    //still using bluetooth - should have finished - reset the device
    ESP.restart(); // targetting 8266 & Esp32 - you may need to replace this
  }
}

void doLockCommand()
{
  WiFi.disconnect(); 
  WiFi.mode(WIFI_OFF);

  augustLock.connect(&connectCallback, &notifyCB, &secureLockCallback, &disConnectCallback);
  augustLock.lockAction(LOCK);
  setTimer(DISCONNECT_TIMEOUT, &lockTimeout);
}

void doUnlockCommand()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);

  augustLock.connect(&connectCallback, &notifyCB, &secureLockCallback, &disConnectCallback);
  augustLock.lockAction(UNLOCK);
  setTimer(DISCONNECT_TIMEOUT, &lockTimeout);
}

void doToggleCommand()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);

  augustLock.connect(&connectCallback, &notifyCB, &secureLockCallback, &disConnectCallback);
  augustLock.lockAction(LockAction::TOGGLE_LOCK);
  setTimer(DISCONNECT_TIMEOUT, &lockTimeout);
}

void updateChallenge()
{
  // generate a new challenge
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

  Serial.print("Challenge: ");
  Serial.println(currentChallenge.c_str());
}

bool wifiConnect(const char* ssid, const char* password)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  if (WiFi.status() != WL_CONNECTED)
  {
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    WiFi.begin(ssid, password);
  }

  int wificounter = 0;
  while (WiFi.status() != WL_CONNECTED && wificounter < 20)
  {
    for (int i = 0; i < 500; i++)
    {
      delay(1);
    }
    Serial.print(".");
    wificounter++;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("-");
    return false;
  }
  else
  {
    return true;
  }
}

void setupWifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  delay(100);

  WiFi.enableSTA(true);
  WiFi.mode(WIFI_STA);

  bool connected = false;
  int index = 0;

  while (!connected)
  {
    connected = wifiConnect(wiFiCredentials[index].ssid, wiFiCredentials[index].password);
    if (++index > wiFiCredentials.size())
    {
      break;
    } 
  }

  if (!connected)
  {
    Serial.println("Restarting ...");
    ESP.restart();
  }

  esp_wifi_set_max_tx_power(78); // save a little power if your unit is near the router. If it's located away then use 78 - max
  delay(10);

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}