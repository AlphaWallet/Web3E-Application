# Web3E Car Control with TokenScript

## To use this Web3E firmware with TokenScript:

1. Deploy your ERC721 or ERC875 tokenscript using a factory (https://mintable.app/create or https://tf.alphawallet.com)
2. Copy the created contract address to both the firmware and the TokenScript.
  - In main.cpp replace the 0x123456... with the created contract address from 1.
  - In the ```karma.canonicalized.xml``` in this repo replace the text '0xYOUR ERC721 OR ERC875 CONTRACT ADDRESS HERE' at line 17 with the address from step 1.
  - If you aren't using Rinkeby then update the Infura network definition. In ```main.cpp``` replace 'rinkeby' with the network name eg mainnet, ropsten etc. In ```karma.canonicalised.xml``` replace the X in network="X" on the same line as the contract with the network ID, mainnet is 1, ropsten is 3.
  
  Let's say the contract address mined in step 1. was 0xd046625935e276526E0f785cb4e969AC279E5E02, on Rinkeby network.
  You should have something like this at line 17 in ```karma.canonicalized.xml```:
  ```
  <ts:address network="4">0xd046625935e276526E0f785cb4e969AC279E5E02</ts:address>
  ```
  and in ```main.cpp```:
  ```
  #define CAR_CONTRACT "0xd046625935e276526E0f785cb4e969AC279E5E02"
  ```
  
3. Fill in your WiFi connection details in the firmware code in ```main.cpp```.
4. Flash the firmware to an ESP32.
5. Obtain the device address. If you have a serial monitor, make a note of the device address chosen - displayed in the first line of Serial eg:

```Address: 0x123456789ABCDEF0123456789ABCDEF012345678```

If you do not have a serial monitor, then ensure your laptop/phone is on the same WiFi connection as the device and point a browser here:

```http://james.lug.org.cn:8080/api/getEthAddress```

You should see something like this:

```
Devices found on IP address: xx.xx.xxx.149
0x123456789ABCDEF0123456789ABCDEF012345678
```

6. Copy the device address from 5. into the 'iotAddr' var at line 340 of the TokenScript. This is the UID of the device that the proxy server will use to forward your API calls to the device.
```
    var iotAddr = "0x123456789ABCDEF0123456789ABCDEF012345678"; // <-- your device address here
    var serverAddr = "http://james.lug.org.cn";
```
7. Send the edited TokenScript XML ```karma.canonicalized.xml``` to your phone by email or messenger.
8. Install AlphaWallet from App Store, Play Store or HuaWei Store if you haven't already.
9. Click on the TokenScript XML and select the AlphaWallet icon to install the script.
10. Find the token in the wallet and click on it to access the IoT device.

## Join us in the TokenScript open community here: https://community.tokenscript.org/
