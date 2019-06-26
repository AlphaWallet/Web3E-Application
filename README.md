# Web3E-Application
Applications Using Web3E

## Samples showing real world use cases of Web3E

NB: All samples use Platformio IDE

### AlphaWallet Office Door. https://www.youtube.com/watch?v=D_pMOMxXrYY

Runs a DApp Server on an embedded device running Arduino framework.

- When a user connects to the server, the microcontroller produces a random challenge string embedded in the webpage/DApp that is served.
- The user clicks on the 'Open Gate' button and a Web3 function call in the DApp JavaScript is intercepted by the wallet/Metamask, which prompts the user the sign the challenge string with their private key.
- The DApp JavaScript embeds the resulting signature and the wallet address into a header which is sent back to the microcontroller as a redirect-URL (like adverts in emails do to analyse who clicked on their email).
- The DApp uses the signature and the challenge it sent to perform an EC-recover.
- The address calculated from the EC-recover is first compared with the address sent to check they match. If they match, we know the user owns the private key for that address.
- The address is checked to see if it holds an ERC875 AlphaWallet office entry attestation token. If the address does, then the microcontroller begins the sequence to open the door.
- Opening the door is a series of three button presses to simulate opening the intercom (usually the check the identity of the person asking for access), with intercom open the door open command is sent, then the intercom closed. Some electromechanical relays controlled from the device and wired up to the switches perform these actions.

### Setup Instructions

- Install [Platformio](https://platformio.org/)
- Clone this repo and open in Platformio.
- Set up your WiFi credentials in main.cpp
- [Get AlphaWallet Android](https://1x.alphawallet.com/dl/latest.apk) or [iOS](https://testflight.apple.com/join/HRsmubhS)
- [Get some testnet Eth](https://faucet.kovan.network). Visit this site on the AlphaWallet DApp browser.
- [Mint some ERC875 tokens](https://tf.alphawallet.com). Visit here on your DApp browser.
- Take a note of the contract address. Copy/paste contract address into the source code inside the 'STORMBIRD_CONTRACT' define.
- Build and deploy the sample to your Arduino framework device. [We used this one.](https://www.aliexpress.com/item/Lolin-ESP32-OLED-V2-0-Pro-ESP32-OLED-wemos-pour-Arduino-ESP32-OLED-WiFi-Modules-Bluetooth/32824839148.html?spm=a2g0s.9042311.0.0.17af4c4du3MLai)
- Use the transfer or MagicLink on AlphaWallet to give out the tokens.


# Donations
If you support the cause, we could certainly use donations to help fund development:

0xbc8dAfeacA658Ae0857C80D8Aa6dE4D487577c63
