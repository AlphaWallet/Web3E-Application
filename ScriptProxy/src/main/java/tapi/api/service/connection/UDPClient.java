package tapi.api.service.connection;

import org.springframework.util.MultiValueMap;
import org.web3j.crypto.Keys;
import org.web3j.crypto.Sign;
import org.web3j.utils.Numeric;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.math.BigInteger;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.security.SecureRandom;
import java.util.Arrays;
import java.util.List;

import tapi.api.service.AsyncService;

import static tapi.api.service.AsyncService.log;

/**
 * Created by JB on 17/02/2020.
 */
public class UDPClient extends Thread
{
    private static final int CLIENT_REQUEST_AUTHENTICATION = 0;
    private static final int CLIENT_AUTHENTICATION = 1;
    private static final int CLIENT_API_CALL_RETURN = 2;
    private static final int CLIENT_PING = 3;

    private static final byte SERVER_CHALLENGE = 0;
    private static final byte SIGNATURE_VALIDATE = 1;
    private static final byte API_CALL = 2;
    private static final byte PONG = 3;

    private DatagramSocket socket;
    private byte[] receiveData;
    private boolean running;
    private SecureRandom secRand;
    private int port;
    private AsyncService service;

    public UDPClient()
    {
        running = false;
    }

    public void init(AsyncService service, int port) throws SocketException
    {
        receiveData  = new byte[1024];
        secRand = new SecureRandom();
        secRand.setSeed(System.currentTimeMillis());
        socket = new DatagramSocket(port);
        this.port = port;
        this.service = service;
    }

    public void run()
    {
        byte[] rcvSessionToken = new byte[8];
        running = true;

        while (running)
        {
            try
            {
                DatagramPacket packet
                        = new DatagramPacket(receiveData, receiveData.length);
                socket.receive(packet);

                ByteArrayInputStream bas         = new ByteArrayInputStream(packet.getData());
                DataInputStream      inputStream = new DataInputStream(bas);

                InetAddress address = packet.getAddress();
                int         port    = packet.getPort();

                byte type = inputStream.readByte();             //1 byte
                if (inputStream.read(rcvSessionToken) != 8) log(address, "Error reading session Token");  //8 bytes
                BigInteger tokenValue = Numeric.toBigInt(rcvSessionToken);
                int        length     = (inputStream.readByte() & 0xFF);   //1 byte (payload length)
                byte[]     payload    = new byte[length];
                if (inputStream.read(payload) != length) log(address, "Error reading payload");                      //payload bytes
                UDPClientInstance thisClient;
                inputStream.close();
                bas.close();

                thisClient = service.getClientFromToken(tokenValue);

                if (thisClient != null)
                {
                    if (!thisClient.getIPAddress().equals(address) || thisClient.port != port)
                    {
                        System.out.println("IP wrong"); // ignore this connection, if port/ip has changed (eg client dynamic IP reassignment)
                        // then client IP) then client will timeout and create a new link
                        continue;
                    }
                    thisClient.setConnectedClient(this);
                }

                switch (type)
                {
                    case CLIENT_REQUEST_AUTHENTICATION: //request a random
                        if (thisClient == null)
                        {
                            if (tokenValue.equals(BigInteger.ZERO)) //new client
                            {
                                thisClient = new UDPClientInstance(address, port, "");
                                tokenValue = thisClient.generateNewSessionToken(secRand);
                                log(address, "Client login: " + Numeric.toHexString(thisClient.getSessionToken()));
                                sendToClient(thisClient, SERVER_CHALLENGE, thisClient.getSessionToken(), rcvSessionToken);
                                service.updateClientFromToken(tokenValue, thisClient);
                                log(address, "Send Connection Token: " + Numeric.toHexString(thisClient.getSessionToken()));
                            }
                            else
                            {
                                log(address, "Unknown client: " + Numeric.toHexString(rcvSessionToken));
                                break;
                            }
                        }
                        else
                        {
                            log(address, "Re-Send Connection Token: " + Numeric.toHexString(thisClient.getSessionToken()));
                            sendToClient(thisClient, SERVER_CHALLENGE, thisClient.getSessionToken(), rcvSessionToken);
                        }
                        break;

                    case CLIENT_AUTHENTICATION: //address
                        log(address, "Receive Verification From: " + Numeric.toHexString(rcvSessionToken));

                        //recover signature
                        if (thisClient != null && payload.length == 65)
                        {
                            String recoveredAddr = recoverAddressFromSignature(rcvSessionToken, payload);
                            if (recoveredAddr.length() == 0) break;
                            if (thisClient.getEthAddress().length() == 0)
                            {
                                log(address, "Validate client: " + recoveredAddr);
                                thisClient.setEthAddress(recoveredAddr);
                            }
                            else if (recoveredAddr.equalsIgnoreCase(thisClient.getEthAddress()))
                            {
                                log(address, "Renew client.");
                            }
                            else
                            {
                                log(address, "Reject.");
                                break;
                            }

                            if (!thisClient.validated)
                            {
                                log(address, "Validated: " + recoveredAddr);
                                thisClient.setValidationTime();
                                log(address, "New Session T: " + thisClient.getSessionTokenStr());
                                thisClient.unknownCount = 0;
                                thisClient.validated = true;
                                addToAddresses(recoveredAddr.toLowerCase(), thisClient);
                                service.updateClientFromToken(tokenValue, thisClient);
                            }

                            sendToClient(thisClient, SIGNATURE_VALIDATE, thisClient.getSessionToken(), null);
                        }
                        break;

                    case CLIENT_API_CALL_RETURN:
                        int methodId = payload[0];
                        payload = Arrays.copyOfRange(payload, 1, payload.length);
                        String payloadString = new String(payload);
                        log(address, "RCV Message: " + Numeric.toHexString(rcvSessionToken));

                        if (thisClient != null)
                        {
                            log(address, "Receive: MethodId: " + methodId + " : " + payloadString + " Client #" + thisClient.getSessionTokenStr());
                            thisClient.setResponse(methodId, payloadString);
                        }
                        else
                        {
                            log(address, "Inner Receive, client not valid: " + payloadString + " : 0x" + tokenValue.toString(16));
                        }
                        break;

                    case CLIENT_PING:
                        //ping from client, respond with PONG
                        if (thisClient == null) break;
                        sendToClient(thisClient, PONG, thisClient.getSessionToken(), rcvSessionToken);
                        log(address, "PING -> PONG (" + Numeric.toHexString(rcvSessionToken) + ")");
                        break;
                    default:
                        break;
                }
            }
            catch (IOException e)
            {
                e.printStackTrace();
                running = false;
            }
        }
    }

    public boolean isRunning()
    {
        return running;
    }

    public int getPort()
    {
        return port;
    }

    private void addToAddresses(String recoveredAddr, UDPClientInstance thisClient)
    {
        List<UDPClientInstance> addrList = service.getClientListFromAddress(recoveredAddr);
        if (addrList == null)
        {
            addrList = service.initAddrList(recoveredAddr);
        }
        else
        {
            service.pruneClientList(recoveredAddr);
        }
        addrList.add(thisClient);
    }

    //transmit back
    private void sendToClient(UDPClientInstance instance, byte type, byte[] stuffToSend, byte[] extraToSend) throws IOException
    {
        ByteArrayOutputStream bas          = new ByteArrayOutputStream();
        DataOutputStream      outputStream = new DataOutputStream(bas);

        int totalPacketLength = 2 + stuffToSend.length;
        if (extraToSend != null) totalPacketLength += extraToSend.length;
        outputStream.writeByte(type);
        outputStream.writeByte((byte)totalPacketLength - 2);
        outputStream.write(stuffToSend);
        if (extraToSend != null) outputStream.write(extraToSend);
        outputStream.flush();
        outputStream.close();

        DatagramPacket packet = new DatagramPacket(bas.toByteArray(), totalPacketLength, instance.getIPAddress(), instance.port);

        try
        {
            socket.send(packet);
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }
    }

    void reSendToClient(UDPClientInstance instance, int methodId) throws IOException
    {
        if (instance != null && !instance.hasResponse(methodId))
        {
            System.out.println("Re-Send to client: " + methodId + " : " + instance.getSessionTokenStr());
            byte[] packetBytes = instance.getQuery(methodId);
            DatagramPacket packet = new DatagramPacket(packetBytes, packetBytes.length, instance.getIPAddress(), instance.port);
            socket.send(packet);
        }
    }

    int sendToClient(UDPClientInstance instance, String method,
                             MultiValueMap<String, String> argMap) throws IOException
    {
        int packetId = -1;
        if (instance != null)
        {
            //send message
            ByteArrayOutputStream bas          = new ByteArrayOutputStream();
            DataOutputStream      outputStream = new DataOutputStream(bas);

            packetId = service.getLatestQueryID(instance.getEthAddress());

            log(instance.getIPAddress(), "Create API call: " + method + " #" + packetId);

            outputStream.writeByte(API_CALL);
            outputStream.writeByte((byte)packetId);
            outputStream.writeByte((byte)0);

            int payloadSize = 0;

            payloadSize += writeValue(outputStream, method);

            for (String key : argMap.keySet())
            {
                payloadSize += writeValue(outputStream, key);
                String param = "";
                if (argMap.get(key).size() > 0)
                {
                    param = argMap.get(key).get(0);
                }
                payloadSize += writeValue(outputStream, param);
            }

            outputStream.flush();
            outputStream.close();

            instance.setQuery((byte)packetId, bas.toByteArray(), (byte)payloadSize);
            byte[] packetBytes = instance.getQuery(packetId);

            DatagramPacket packet = new DatagramPacket(packetBytes, packetBytes.length, instance.getIPAddress(), instance.port);

            socket.send(packet);
        }

        return packetId;
    }

    private int writeValue(DataOutputStream outputStream, String value) throws IOException
    {
        int writeLength = 1;
        outputStream.writeByte((byte)value.length());
        outputStream.write(value.getBytes());
        writeLength += value.length();
        return writeLength;
    }

    private String recoverAddressFromSignature(byte[] rcvSessionToken, byte[] payload)
    {
        String recoveredAddr = "";
        try
        {
            Sign.SignatureData sigData = sigFromByteArray(payload);
            //recover address from signature
            BigInteger recoveredKey  = Sign.signedMessageToKey(rcvSessionToken, sigData);
            recoveredAddr = "0x" + Keys.getAddress(recoveredKey);
        }
        catch (Exception e)
        {
            e.printStackTrace();
        }

        return recoveredAddr;
    }

    private static Sign.SignatureData sigFromByteArray(byte[] sig)
    {
        if (sig.length < 64 || sig.length > 65) return null;

        byte   subv = sig[64];
        if (subv < 27) subv += 27;

        byte[] subrRev = Arrays.copyOfRange(sig, 0, 32);
        byte[] subsRev = Arrays.copyOfRange(sig, 32, 64);
        return new Sign.SignatureData(subv, subrRev, subsRev);
    }

    public int matchMethod(UDPClientInstance instance, String method)
    {
        return service.getLatestQueryID(instance.getEthAddress());
    }
}
