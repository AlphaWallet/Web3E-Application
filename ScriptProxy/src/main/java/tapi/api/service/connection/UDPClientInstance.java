package tapi.api.service.connection;

import org.springframework.util.MultiValueMap;
import org.web3j.utils.Numeric;

import java.io.IOException;
import java.math.BigInteger;
import java.net.InetAddress;
import java.security.SecureRandom;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Created by JB on 17/02/2020.
 */
public class UDPClientInstance
{
    private String ethAddress;
    private InetAddress IPAddress;
    int port;
    private long validationTime;
    private byte[] sessionToken;
    int unknownCount = 0;
    private UDPClient connectedClient;

    boolean validated;
    private Map<Integer, String> responses;
    private Map<Integer, byte[]> currentQueries;
    private Map<Integer, Query> currentClientQueries;

    private class Query
    {
        String query;
        String origin;

        Query(String origin, String query)
        {
            this.query = query;
            this.origin = origin;
        }
    };

    UDPClientInstance(InetAddress iAddr, int p, String eAddress)
    {
        ethAddress = eAddress;
        IPAddress = iAddr;
        port = p;
        validated = false;
        responses = new ConcurrentHashMap<>();
        currentQueries = new ConcurrentHashMap<>();
        currentClientQueries = new ConcurrentHashMap<>();

        validationTime = System.currentTimeMillis();
    }

    BigInteger generateNewSessionToken(SecureRandom secRand)
    {
        BigInteger tokenValue = BigInteger.valueOf(secRand.nextLong());
        sessionToken = Numeric.toBytesPadded(tokenValue, 8);
        validated = false;
        return Numeric.toBigInt(sessionToken);
    }

    public boolean hasResponse(int methodId)
    {
        return responses.get(methodId) != null;
    }

    public String getResponse(int methodId)
    {
        String resp = responses.get(methodId);
        //responses.remove(methodId); //don't remove response, so the other method can pick this up
        currentQueries.remove(methodId);
        currentClientQueries.remove(methodId);
        return resp;
    }

    void setResponse(int methodId, String r)
    {
        responses.put(methodId, r);
        currentQueries.remove(methodId);
    }

    void setQuery(byte packetId, byte[] packet, byte payloadSize)
    {
        packet[2] = payloadSize;
        currentQueries.put((int)packetId, packet);
    }

    byte[] getQuery(int methodId)
    {
        return currentQueries.get(methodId);
    }

    public InetAddress getIPAddress()
    {
        return IPAddress;
    }

    public String getEthAddress()
    {
        return ethAddress;
    }

    public byte[] getSessionToken()
    {
        return sessionToken;
    }

    void setEthAddress(String recoveredAddr)
    {
        ethAddress = recoveredAddr;
    }

    public String getSessionTokenStr()
    {
        return Numeric.toHexString(sessionToken);
    }

    public long getValidationTime()
    {
        return validationTime;
    }

    void setValidationTime()
    {
        this.validationTime = System.currentTimeMillis();
    }

    public int sendToClient(String origin, String method, MultiValueMap<String, String> argMap) throws IOException
    {
        int packetId = connectedClient.sendToClient(this, method, argMap);
        if (packetId > -1)
        {
            currentClientQueries.put(packetId, new Query(origin, method));
        }
        return packetId;
    }

    void setConnectedClient(UDPClient udpClient)
    {
        connectedClient = udpClient;
    }

    public void reSendToClient(int methodId) throws IOException
    {
        connectedClient.reSendToClient(this, methodId);
    }

    public int getMatchingQuery(String origin, String query)
    {
        for (int methodId : currentClientQueries.keySet())
        {
            Query q = currentClientQueries.get(methodId);
            if (q.origin.equals(origin) && q.query.equals(query))
            {
                return methodId;
            }
        }

        return -1;
    }
}
