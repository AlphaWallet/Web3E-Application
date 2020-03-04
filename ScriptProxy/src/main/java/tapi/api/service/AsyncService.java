package tapi.api.service;


import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.context.annotation.Bean;
import org.springframework.stereotype.Service;
import org.springframework.util.MultiValueMap;
import org.springframework.web.client.RestTemplate;
import org.web3j.utils.Numeric;

import java.io.IOException;
import java.math.BigInteger;
import java.net.InetAddress;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;

import tapi.api.service.connection.UDPClient;
import tapi.api.service.connection.UDPClientInstance;

@Service
public class AsyncService
{
    private List<UDPClient> udpClients;
    private Map<BigInteger, UDPClientInstance> tokenToClient = new ConcurrentHashMap<>();
    private Map<String, List<UDPClientInstance>> addressToClient = new ConcurrentHashMap<>();
    private Map<String, Integer> IoTAddrToQueryID = new ConcurrentHashMap<>();

    private final static int UDP_PORT = 5000;
    private final static int UDP_TOP_PORT = 5004;
    private final static long CONNECTION_CLEANUP_TIME = 5L * 60L * 1000L; //after 5 minutes of silence remove a connection

    private UDPClientInstance getLatestClient(String ethAddress)
    {
        List<UDPClientInstance> clients = addressToClient.get(ethAddress);
        if (clients != null && clients.size() > 0) return clients.get(clients.size() - 1);
        else return null;
    }

    @Autowired
    private RestTemplate restTemplate;

    @Bean
    public RestTemplate restTemplate() {
        return new RestTemplate();
    }

    public AsyncService()
    {
        udpClients = new ArrayList<>();
        for (int port = UDP_PORT; port <= UDP_TOP_PORT; port++)
        {
            try
            {
                UDPClient client = new UDPClient();
                client.init(this, port);
                client.start();
                udpClients.add(client);
            }
            catch (Exception e)
            {
                System.out.println("Couldn't start Port: " + port + " Already in use.");
            }
        }


        System.out.println("UDP server started");
    }

    public CompletableFuture<String> getResponse(String address, String method,
                                                 MultiValueMap<String, String> argMap, String origin) throws InterruptedException, IOException
    {
        UDPClientInstance instance = getLatestClient(address.toLowerCase());
        if (instance == null) return CompletableFuture.completedFuture("No device found");

        //is there an identical call in progress from the same client?
        int methodId;
        int checkId = instance.getMatchingQuery(origin, method);
        if (checkId != -1)
        {
            methodId = checkId;
            System.out.println("Duplicate MethodID: " + checkId);
        }
        else
        {
            methodId  = instance.sendToClient(origin, method, argMap);
        }

        if (methodId == -1) return CompletableFuture.completedFuture("API send error");
        int resendIntervalCounter = 0;
        int resendCount = 30; //resend packet 20 times before timeout - attempt connection for 30 * 500ms = 15 seconds timeout
        boolean responseReceived = false;
        while (!responseReceived && resendCount > 0)
        {
            Thread.sleep(10);
            instance = getLatestClient(address.toLowerCase());
            if (instance != null)
            {
                if (resendIntervalCounter++ > 50) //resend every 500 ms (thread sleep time = 10ms)
                {
                    resendIntervalCounter = 0;
                    instance.reSendToClient(methodId);
                    resendCount--;
                }

                if (instance.hasResponse(methodId))
                    responseReceived = true;
            }
        }

        String response = instance.getResponse(methodId);

        if (resendCount == 0)
        {
            System.out.println("Timed out");
            response = "Timed out";
        }
        else
        {
            System.out.println("Received: (" + methodId + ") " + response + ((checkId > -1) ? " (*)" : ""));
        }

        return CompletableFuture.completedFuture(response);
    }

    public CompletableFuture<String> getDeviceAddress(String ipAddress) throws UnknownHostException
    {
        boolean      useFilter    = isLocal(ipAddress);
        InetAddress  inetAddress  = InetAddress.getByName(ipAddress);
        StringBuilder sb = new StringBuilder();
        sb.append("Devices found on IP address: ");
        sb.append(ipAddress);
        byte[]       filter;
        boolean foundAddr = false;

        if (useFilter)
        {
            filter = inetAddress.getAddress();
            filter[3] = 0;
            inetAddress = InetAddress.getByAddress(filter);
        }

        for (List<UDPClientInstance> instances : addressToClient.values())
        {
            UDPClientInstance instance = instances.get(instances.size() - 1);
            byte[] ipBytes = instance.getIPAddress().getAddress();
            if (useFilter) ipBytes[3] = 0;
            InetAddress instanceAddr = InetAddress.getByAddress(ipBytes);
            if (instanceAddr.equals(inetAddress))
            {
                foundAddr = true;
                sb.append("</br>");
                sb.append(instance.getEthAddress());
            }
        }

        if (!foundAddr)
        {
            sb.append("</br>No devices");
        }

        return CompletableFuture.completedFuture(sb.toString());
    }

    private boolean isLocal(String ipAddress) throws UnknownHostException
    {
        InetAddress inetAddress = InetAddress.getByName(ipAddress);
        byte[] filter = inetAddress.getAddress();
        return filter[0] == (byte) 192 && filter[1] == (byte) 168;
    }

    public static void log(InetAddress addr, String msg)
    {
        DateTimeFormatter dtf = DateTimeFormatter.ofPattern("HH:mm:ss");
        LocalDateTime     now = LocalDateTime.now();
        System.out.println(dtf.format(now) + ":" + addr.getHostAddress() + ": " + msg);
    }

    /**
     * Checks to see if any of the server threads have terminated, restart if so.
     */
    public void checkServices()
    {
        for (UDPClient client : udpClients)
        {
            if (!client.isRunning())
            {
                System.out.println("Warning: restarting listener: " + client.getPort());
                try
                {
                    client.init(this, client.getPort());
                    client.start();
                }
                catch (SocketException e)
                {
                    e.printStackTrace();
                }
            }
        }

        //check for very old client connections
        for (List<UDPClientInstance> instances : addressToClient.values())
        {
            if (instances != null && instances.size() > 0)
            {
                UDPClientInstance instance = instances.get(instances.size() - 1);
                if (System.currentTimeMillis() > (instance.getValidationTime() + CONNECTION_CLEANUP_TIME))
                {
                    log(instance.getIPAddress(), "Removing old client: " + instance.getEthAddress());
                    //remove all instances of this connection
                    IoTAddrToQueryID.remove(instance.getEthAddress());
                    addressToClient.remove(instance.getEthAddress());
                    instances.clear();
                    break;
                }
            }
        }

        //remove orphaned session tokens
        for (BigInteger sessionToken : tokenToClient.keySet())
        {
            UDPClientInstance instance = tokenToClient.get(sessionToken);
            if (System.currentTimeMillis() > (instance.getValidationTime() + CONNECTION_CLEANUP_TIME))
            {
                log(instance.getIPAddress(), "Removing old token: " + sessionToken.toString(16) + " : " + instance.getEthAddress());
                tokenToClient.remove(sessionToken);
                break;
            }
        }
    }

    public UDPClientInstance getClientFromToken(BigInteger tokenValue)
    {
        return tokenToClient.get(tokenValue);
    }

    public void updateClientFromToken(BigInteger tokenValue, UDPClientInstance client)
    {
        tokenToClient.put(tokenValue, client);
    }

    public List<UDPClientInstance> getClientListFromAddress(String recoveredAddr)
    {
        return addressToClient.get(recoveredAddr);
    }

    public List<UDPClientInstance> initAddrList(String recoveredAddr)
    {
        List<UDPClientInstance> addrList = new ArrayList<>();
        addressToClient.put(recoveredAddr, addrList);
        return addrList;
    }

    public void pruneClientList(String recoveredAddr)
    {
        List<UDPClientInstance> addrList = getClientListFromAddress(recoveredAddr);
        //check for out of date client
        if (addrList.size() >= 3)
        {
            UDPClientInstance oldClient = addrList.get(0);
            log(oldClient.getIPAddress(), "Removing client from addr map #" + oldClient.getSessionTokenStr());
            addrList.remove(oldClient);

            //remove this guy from main list too
            if (tokenToClient.containsKey(Numeric.toBigInt(oldClient.getSessionToken())))
            {
                tokenToClient.remove(Numeric.toBigInt(oldClient.getSessionToken()));
                log(oldClient.getIPAddress(), "Removing client from token map #" + oldClient.getSessionTokenStr());
            }
        }
    }

    public int getLatestQueryID(String ethAddress)
    {
        int val = IoTAddrToQueryID.getOrDefault(ethAddress, 0);
        if (++val > 256) val = 0;
        IoTAddrToQueryID.put(ethAddress, val);
        return val;
    }
}

