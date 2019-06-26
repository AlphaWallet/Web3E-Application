#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WifiServer.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "SSD1306.h"
#include <Web3.h>
#include <Contract.h>
#include <Crypto.h>
#include <vector>
#include <string>
#include <Util.h>
#include "ActionHandler.h"
#include "TokenCache.h"

const char *ssid = "PNP";
const char *password = "1234567890";
const char *INFURA_HOST = "rinkeby.infura.io";
const char *INFURA_PATH = "/v3/c7df4c29472d4d54a39f7aa78f146853";
#define STORMBIRD_CONTRACT "0xd2a0ddf0f4d7876303a784cfadd8f95ec1fb791c"
#define INTERCOM_PIN 13     // ID of the pin intercom relay is connected to
#define GATE_PIN 12         // ID of the pin the door relay is connected to
#define BLUE_LED 2          // Little blue LED on the ESP8266/ESP32

//use these if you want to expose your DApp server to outside world via portforwarding
// IPAddress ipStat(192, 168, 1, 85);
// IPAddress gateway(192, 168, 1, 1);
// IPAddress subnet(255, 255, 255, 0);
// IPAddress dns(192, 168, 1, 1);

Web3 web3(INFURA_HOST, INFURA_PATH);
WiFiServer server(80);
//SSD1306  display(0x3c, 4, 15); //LILYGO Basic
SSD1306  display(0x3c, 21, 22);  //LILYGO LORA
int wificounter = 0;

const char * seedWords[] = { "Apples", "Oranges", "Grapes", "Dragon fruit", "Bread fruit", "Pomegranate", "Mangifera indica", "Persea americana", "Falafel", 0 };

string header;
string currentChallenge;

void checkClientHeader(WiFiClient client);
void setup_wifi();
void generateSeed(BYTE *buffer);
void updateChallenge();
void sendPage(WiFiClient client);
bool checkSignature(string signature, string address);
void insertCSS(WiFiClient client);
bool checkReceivedData(string userData, bool *signaturePass);
bool QueryBalance(string userAddress);
void OpenDoor();
void addLCDMessage(string message);
void activeState();

ActionHandler* actionHandler;
TokenCache* tokenCache;

void setup() 
{
    Serial.begin(115200); //ensure you set your Arduino IDE port config or platformio.ini with monitor_speed = 115200
    actionHandler = new ActionHandler(12);
    tokenCache = new TokenCache(10);

    pinMode(BLUE_LED, OUTPUT);
    pinMode(GATE_PIN, OUTPUT);
    pinMode(INTERCOM_PIN, OUTPUT);
    digitalWrite(BLUE_LED, LOW);
    digitalWrite(GATE_PIN, HIGH);       //these need to be pulled high to keep the relay closed
    digitalWrite(INTERCOM_PIN, HIGH);

    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    addLCDMessage("Connecting ...");

    setup_wifi();
    updateChallenge();
    server.begin();

    addLCDMessage("Active");
}

void loop() 
{
    setup_wifi(); //ensure we maintain a connection. This may cause the server to reboot periodically, if it loses connection

    WiFiClient client = server.available(); // Listen for incoming clients

    if (client)
    {
        Serial.println("New Client.");
        checkClientHeader(client);
    }

    actionHandler->CheckEvents(millis());
}

void checkClientHeader(WiFiClient client)
{
    boolean currentLineIsBlank = true;
    String currentLine = ""; // make a String to hold incoming data from the client
    int timeout = 0;

    while (client.connected())
    { // loop while the client's connected
        if (client.available())
        {
            timeout = 0;
            // if there's bytes to read from the client,
            char c = client.read(); // read a byte, then
            Serial.write(c);        // print it out the serial monitor
            header += c;
            if (c == '\n' && currentLineIsBlank)
            {
                // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                // and a content-type so the client knows what's coming, then a blank line:
                sendPage(client);

                // Break out of the while loop
                break;
            }
            if (c == '\n')
            { // if you got a newline, then clear currentLine
                currentLine = "";
                currentLineIsBlank = true;
            }
            else if (c != '\r')
            {
                currentLineIsBlank = false;
                currentLine += c;
            }
        }
        else
        {
            delay(1);
            if (timeout++ > 300)
                break;
            Serial.print(".");
        }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    delay(1);
    client.flush();
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
}

void sendPage(WiFiClient client)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();

    string checkLine = "RCV/";
    string getCheck = "GET /" + checkLine;

    size_t userDataIndex = header.find(getCheck);
    bool signaturePass = false;
    bool hasToken = false;
    bool hasData = false;

    // Deal with received user data (we use URL to receive data from the app)
    if (userDataIndex != string::npos)
    {
        hasData = true;
        int sigIndex = userDataIndex + getCheck.length();
        string receivedUserData = string(header.substr(sigIndex));
        hasToken = checkReceivedData(receivedUserData, &signaturePass);
    }

    // Display the HTML web page
    client.println("<!DOCTYPE html><html>");
    client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
    client.println("<link rel=\"icon\" href=\"data:,\">");
    insertCSS(client);
    client.println("</head>");

    // Web Page Heading
    client.println("<body><h1>Stormbird Door Server</h1>");

    if (hasData) // got a return from the client
    {
        if (hasToken)
        {
            addLCDMessage("Signature Valid!");
            OpenDoor();
            //signature passed and has token
            client.println("<h2><div id=\"sig\">ACCESS GRANTED!</div></h2>");
            //return to home page after 2 seconds
            client.println("<script>function loadDelay2() { setTimeout(function () { window.location.replace('/'); }, 2000);} window.onload = loadDelay2;</script>");
        }
        else if (signaturePass)
        {
            addLCDMessage("No Token");
            actionHandler->AddCallback(10000, &activeState);
            //signature passed but no token
            client.println("<h2><div id=\"sig\">ACCESS DENIED - NO TOKEN</div></h2>");
            //return to home page after 2 seconds
            client.println("<script>function loadDelay2() { setTimeout(function () { window.location.replace('/'); }, 2000);} window.onload = loadDelay2;</script>");
        }
        else
        {
            addLCDMessage("Signature failed");
            actionHandler->AddCallback(10000, &activeState);
            //signature failed
            client.println("<h2><div id=\"sig\">Signature Check Failed</div></h2>");
            //return to home page after 2 seconds
            client.println("<script>function loadDelay2() { setTimeout(function () { window.location.replace('/'); }, 2000);} window.onload = loadDelay2;</script>");
        }

        client.println("<a href=\"/\" id=\"resetbutton\" visibility: visible; target=\"/\" class=\"stormbut\">Reset Page</a>");
        client.println("<script>function loadDelay2() { var resetButton = document.getElementById('resetbutton'); resetButton.style.visibility = \"visible\"; setTimeout(function () { window.location.replace('/'); }, 2000);} window.onload = loadDelay2;</script>");
 
    }
    else
    {
        client.println("<p>Challenge: " + String(currentChallenge.c_str()) + "</p>");
        client.println("<h4><div id=\"useraddr\">User Account:</div></h4>");
        client.println("<p id=\"activatebutton\" class=\"stormbut2\"></p>");
        client.println("<p id=\"signLayer\"><button onclick=\"signButton()\" id=\"signButton\" class=\"stormbut\">Sign Challenge</button></p>");
        client.println("<div id=\"sig\"></div>");
        //Sign button handling - web3.eth.sign asks AlphaWallet/Metamask to sign the challenge then inserts the received signature and address into URL
        client.println("<script>function signButton() { var accountElement = document.getElementById('useraddr'); var account = web3.eth.coinbase; var container = document.getElementById('message'); var sig = document.getElementById('sig'); var message = '" + String(currentChallenge.c_str()) + "'; web3.eth.sign(account, message, (err, data) => { sig.innerHTML = data; accountElement.innerHTML = 'Validating Signature and Attestation'; var signButton = document.getElementById(\"signButton\"); signButton.style.visibility = \"hidden\"; var currentLoc = window.location.href; window.location.replace(currentLoc + '" + String(checkLine.c_str()) + "' + data + '/' + account + '/'); });}</script>");
        client.println("<script>");
        client.println(Web3::getDAppCode());

        //reads the user's ethereum address from web3.eth.coinbase and displays on the page
        client.println("function displayAddress(){ var signButton = document.getElementById('signButton'); var account = web3.eth.coinbase; var accountElement = document.getElementById('useraddr'); accountElement.innerHTML = 'User Account: ' + account; }");
        //half second deley before trying to read coinbase - wait for race condition between starting the page and the injector code populating the address
        client.println("function loadDelay() { setTimeout(function () { displayAddress(); }, 500); } window.onload = loadDelay;");
        client.println("</script>");
    }

    client.println("</body></html>");

    // The HTTP response ends with another blank line
    client.println();
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

        // if (!WiFi.config(ipStat, gateway, subnet, dns, dns))
        // {
        //     Serial.println("STA Failed to configure");
        // }
        WiFi.begin(ssid, password);
    }

    wificounter = 0;
    while (WiFi.status() != WL_CONNECTED && wificounter < 10)
    {
        for (int i = 0; i < 500; i++)
        {
            delay(1);
        }
        Serial.print(".");
        wificounter++;
    }

    if (wificounter >= 10)
    {
        Serial.println("Restarting ...");
        ESP.restart(); //targetting 8266 & Esp32 - you may need to replace this
    }

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
    while (seedWords[size] != 0) size++;
    Serial.println(size);
    char buffer[32];
    
    int seedIndex = random(0, size);
    currentChallenge = seedWords[seedIndex];
    currentChallenge += " 0x";
    long challengeVal = (millis() + random(0,16384)) & 0xFFFF; //NB Do not use this in production systems, use a secure random
    currentChallenge += itoa(challengeVal, buffer, 16);
}

bool checkSignature(string signature, string address)
{
    uint8_t challengeHash[ETHERS_KECCAK256_LENGTH];

    //we have to hash the challenge first
    //first convert to a hex string
    string hexVal = Util::ConvertBytesToHex((uint8_t*)currentChallenge.c_str(), currentChallenge.length());
    Serial.println(hexVal.c_str());

    //String signMessage = "\u0019Ethereum Signed Message:\n";
    //signMessage += hexVal.length();
    //signMessage += hexVal.substring(2); // cut off the leading '0x'

    Serial.println("BYTES");
    Serial.println(Util::ConvertBytesToHex((uint8_t*)currentChallenge.c_str(), currentChallenge.length()).c_str());

    //hash the full challenge    
    Crypto::Keccak256((uint8_t*)currentChallenge.c_str(), currentChallenge.length(), challengeHash);

    Serial.println("Challenge : " + String(currentChallenge.c_str()));
    Serial.print("Challenge#: ");
    Serial.println(Util::ConvertBytesToHex(challengeHash, ETHERS_KECCAK256_LENGTH).c_str());

    //convert address to BYTE
    BYTE addrBytes[ETHERS_ADDRESS_LENGTH];
    BYTE signatureBytes[ETHERS_SIGNATURE_LENGTH];
    BYTE publicKeyBytes[ETHERS_PUBLICKEY_LENGTH];

    Util::ConvertHexToBytes(addrBytes, address.c_str(), ETHERS_ADDRESS_LENGTH);
    Util::ConvertHexToBytes(signatureBytes, signature.c_str(), ETHERS_SIGNATURE_LENGTH);

    Serial.println("SIG");
    Serial.println(Util::ConvertBytesToHex(signatureBytes, ETHERS_SIGNATURE_LENGTH).c_str());

    Serial.println();

    // Perform ECRecover to get the public key - this extracts the public key of the private key
    // that was used to sign the message - that is, the private key held by the wallet/metamask
    Crypto::ECRecover(signatureBytes, publicKeyBytes, challengeHash); 

    Serial.println("Recover Pub:");
    Serial.println(Util::ConvertBytesToHex(publicKeyBytes, ETHERS_PUBLICKEY_LENGTH).c_str());

    BYTE recoverAddr[ETHERS_ADDRESS_LENGTH];
    // Now reduce the recovered public key to Ethereum address
    Crypto::PublicKeyToAddress(publicKeyBytes, recoverAddr);

    Serial.println("Received Address:");
    Serial.println(address.c_str());
    Serial.println("Recovered Address:");
    Serial.println(Util::ConvertBytesToHex(recoverAddr, ETHERS_ADDRESS_LENGTH).c_str());

    if (memcmp(recoverAddr, addrBytes, ETHERS_ADDRESS_LENGTH) == 0) //did the same address sign the message as the user is claiming to own?
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool checkReceivedData(string userData, bool *signaturePass)
{
    size_t sigEnd = userData.find_first_of('/');
    if (sigEnd == string::npos || sigEnd > (ETHERS_SIGNATURE_LENGTH*2 + 2)) //validate input
    {
        Serial.print("Invalid Signature: ");
        Serial.println(userData.c_str());
        return false;
    } 
    string address = userData.substr(sigEnd + 1);
    size_t addressEnd = address.find_first_of('/');
    if (addressEnd == string::npos || addressEnd > (ETHERS_ADDRESS_LENGTH*2 + 2)) //validate input
    {
        Serial.print("Invalid Address: ");
        Serial.println(address.c_str());
        return false; 
    } 
    string signature = userData.substr(0, sigEnd);
    address = address.substr(0, addressEnd);

    Serial.print("Got Sig: ");
    Serial.println(signature.c_str());
    Serial.print("Got Addr: ");
    Serial.println(address.c_str());

    *signaturePass = checkSignature(signature, address);

    bool hasToken = false;
    
    if (*signaturePass)
    {
        Serial.println("Correct Signature obtained!");
        hasToken = QueryBalance(address);
        
        updateChallenge(); //generate a new challenge
    }
    else
    {
        Serial.println("Signature Fails");
    }

    return hasToken;
}

void insertCSS(WiFiClient client)
{
    client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
    client.println(".stormbut { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
    client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer; }");
    client.println(".stormbut2 { background-color: coral; border: none; color: white; padding: 16px 40px;");
    client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer; visibility: hidden;}");
    client.println(".button2 {background-color: #77878A;}</style>");
}

void activeState()
{
    addLCDMessage("Active");
}

void entryComplete()
{
    addLCDMessage("Entry Complete");
    actionHandler->AddCallback(20000, &activeState);
}

void doorOpening()
{
    addLCDMessage("Door Opening");
}

void OpenDoor()
{
    //we should only start this sequence if there's no sequence already running
    if (actionHandler->detectCallback(&entryComplete))
    {
        Serial.println("Door already opening, no need to open");
        return;
    }

    digitalWrite(BLUE_LED, HIGH);
    digitalWrite(INTERCOM_PIN, LOW);                        //intercom press start
    //code action sequence to open the gate
    actionHandler->AddEventAfter(1000, BLUE_LED, LOW);      //switch off the BLUE LED
    actionHandler->AddEventAfter(800, INTERCOM_PIN, HIGH);  //intercom press finish
    actionHandler->AddEventAfter(1000, GATE_PIN, LOW);      //door switch press
    actionHandler->AddEventAfter(1800, GATE_PIN, HIGH);     //door switch press finish
    actionHandler->AddCallback(2000, &doorOpening);
    actionHandler->AddEventAfter(7000, BLUE_LED, HIGH);     //switch on the BLUE LED
    actionHandler->AddEventAfter(7000, INTERCOM_PIN, LOW);  //intercom press start
    actionHandler->AddEventAfter(7800, INTERCOM_PIN, HIGH); //intercom press finish
    actionHandler->AddEventAfter(8000, BLUE_LED, LOW);      //switch off the BLUE LED
    actionHandler->AddCallback(8500, &entryComplete);
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

bool QueryBalance(string userAddress)
{
    bool hasToken = tokenCache->CheckToken(userAddress);

    if (hasToken)
    {
        Serial.println("Cache hit");
        return true;
    }

    // transaction
    const char *contractAddr = STORMBIRD_CONTRACT;
    Contract contract(&web3, contractAddr);
    string func = "balanceOf(address)";
    string param = contract.SetupContractData(func.c_str(), &userAddress);
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
            tokenCache->AddToken(userAddress);
            break;
        }
    }

    delete(vectorResult);
    return hasToken;
}

void addLCDMessage(string message) 
{
    string challengeDisplay = "Chlg: "; 
    challengeDisplay += currentChallenge.substr(0, 16);

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 10, challengeDisplay.c_str());
    display.drawString(0, 22, "Stormbird Door System");
    display.drawString(0, 33, message.c_str());
    display.display();
}
