# Web3E-Application
Applications Using Web3E

## Samples showing real world use cases of Web3E

### AlphaWallet Office Door. https://www.youtube.com/watch?v=D_pMOMxXrYY

Runs a DApp Server on an embedded device running Arduino framework.

. When a user connects to the server, the microcontroller produces a random challenge string embedded in the webpage/DApp that is served.
. The user clicks on the 'Open Gate' button and a Web3 function call in the DApp JavaScript is intercepted by the wallet/Metamask, which prompts the user the sign the challenge string with their private key.
. The DApp JavaScript embeds the resulting signature and the wallet address into a header which is sent back to the microcontroller as a redirect-URL (like adverts in emails do to analyse who clicked on their email).
. The DApp uses the signature and the challenge it sent to perform an EC-recover.
. The address calculated from the EC-recover is first compared with the address sent to check they match. If they match, we know the user owns the private key for that address.
. The address is checked to see if it holds an ERC875 AlphaWallet office entry attestation token. If the address does, then the microcontroller begins the sequence to open the door.
. Opening the door is a series of three button presses to simulate opening the intercom (usually the check the identity of the person asking for access), with intercom open the door open command is sent, then the intercom closed. Some electromechanical relays controlled from the device and wired up to the switches perform these actions.
