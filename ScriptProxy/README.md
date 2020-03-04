# ScriptProxy
A universal proxy for safely connecting TokenScript and IoT devices

## Build Instructions

Linux: ```gradle build```

Windows: ```.\gradlew.bat build```

## Deploy Instructions

```java -jar build/libs/scriptproxy.jar```

## Setup Instructions

By default no setup is required. The SpringBoot server runs on port 8080 and the IoT connection service runs on ports 5000 to 5004. The Web3E bridge library automatically connects to one of these. Multiple ports are used in case there is interference on one of them.
If you wish to change the ports, change the SpringBoot port as normal SpringBoot in the config settings and change the IoT port range in AsyncService.java

For your prototyping usage there is a permanent ScriptProxy set up here for public test use: http://james.lug.org.cn:8080 
By default the UDPBridge in Web3E is set to point to this server so no setup is required.
