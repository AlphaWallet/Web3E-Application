# STL Office door

## TokenScript, EIP5169 + IoT integrated demo

These three code pieces form a prototype Token-centric door entry system which illustrates how Smart Tokens could be used.

The Token Contract is an ERC-721 NFT which has the EIP5169 scriptURI() function. This function points to the tokenscript which is hosted on IPFS (see the ethereum contract).

When a user owns the token, a TokenScript compatible wallet will fetch the tokenscript using EIP5169, which gives the 'unlock' and 'lock' verbs on the token in the wallet (in addition to the usual 'transfer'). In the case of a door entry system the transfer is limited to admins (to prevent a recipient from transferring the token), except for the special transfer case 'burn'.

The advantage of this system is convenience without sacrificing security.