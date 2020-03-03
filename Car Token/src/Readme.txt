To use this Web3E firmware with TokenScript:

1. Deploy your ERC721 or ERC875 tokenscript using a factory (https://mintable.app/create or https://tf.alphawallet.com)
2. Copy the created contract address to CAR_CONTRACT and update Infura network if you're not using Rinkeby.
3. Fill in your WiFi connection details in the firmware code.
4. Flash the firmware to an ESP32.
5. If you have a serial monitor, make a note of the device address chosen - displayed in the first line of Serial eg:

Address: 0x123456789ABCDEF0123456789ABCDEF012345678 

If you do not have a serial monitor, then ensure your laptop/phone is on the same WiFi connection as the device and point a browser here:

http://james.lug.org.cn:8080/api/getEthAddress

You should see something like this:

Devices found on IP address: xx.xx.xxx.149
0x123456789ABCDEF0123456789ABCDEF012345678

6. Copy the contract address from 2. into the "CarToken" declaration on line 17 of the TokenScript XML.
7. Copy the device address from 5. into the 'iotAddr' var at line 356 of the TokenScript. This is the UID of the device that the proxy server will use to forward your API calls to the device.
8. Send the TokenScript XML to your phone by email or messenger.
9. Install AlphaWallet from App Store, Play Store or HuaWei Store if you haven't already.
10. Click on the TokenScript XML and select the AlphaWallet icon to install the script.
11. Find the token in the wallet and click on it to access the IoT device.

Join us in the TokenScript open community here: https://community.tokenscript.org/