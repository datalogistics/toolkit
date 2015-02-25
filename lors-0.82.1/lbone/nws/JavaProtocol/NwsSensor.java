import java.io.*;
import java.net.*;


/**
 * The NwsSensor class implements the protocol specific to NWS sensors --
 * activity starting and stopping and experiment requests.  It does not
 * presently support clique protocol requests.
 */
public class NwsSensor extends NwsHost {


  /** See the constructor for NwsHost. */
  public NwsSensor(String hostName,
                   int hostPort,
                   boolean keepOpen) {
    super(hostName, hostPort, keepOpen);
  }


  /** See the constructor for NwsHost. */
  public NwsSensor(String hostName,
                   int hostPort) {
    super(hostName, hostPort);
  }


  /** See the constructor for NwsHost. */
  public NwsSensor(Socket s) {
    super(s);
  }


  /** Information returned by the tcpMessageTest() method. */
  public class TcpMessageTestResult {
    /** Bandwidth to the sensor in megabits/second. */
    public double bandwidth;
    /**
     * Latency in milliseconds.  Because Java provides no means to track
     * elapsed time in units finer than milliseconds, this value is truncated
     * (i.e. it will always be a whole number).
     */
    public double latency;
    /** Produces a TcpMessageTestResult with the specified field values. */
    public TcpMessageTestResult(double bandwidth,
                                double latency) {
      this.bandwidth = bandwidth;
      this.latency = latency;
    }
  }


  /** Requests that the sensor halt the activity registered as <i>name</i>. */
  public void haltActivity(String name) throws Exception {
    messagePrelude();
    NwsMessage.send(connection, ACTIVITY_STOP, new CString(name).toBytes());
    NwsMessage.receive(connection, this);
    messagePostlude();
  }


  /**
   * Requests that the sensor begin a new activity which applies <i>skill</i>,
   * under <i>control</i> and parameterized by <i>options</i>, and register it
   * as <i>name</i>.
   */
  public void startActivity(String name,
                            String control,
                            String skill,
                            NwsNameServer.Attribute[] options) throws Exception{
    byte[][] allBytes = {new CString(name).toBytes(),
                         new CString(control).toBytes(),
                         new CString(skill).toBytes(),
                         null};
    String allOptions = new String();
    for(int i = 0; i < options.length; i++) {
      allOptions = allOptions + options[i].toString() + "\t";
    }
    allBytes[3] = new CString(allOptions).toBytes();
    messagePrelude();
    NwsMessage.send
      (connection, ACTIVITY_START, NwsMessage.concatenateBytes(allBytes));
    NwsMessage.receive(connection, this);
    messagePostlude();
  }


  /**
   * Performs a TCP connection test with the sensor and returns the number of
   * milliseconds it takes to complete.
   */
  public long tcpConnectionTest() throws Exception {
    long startTime = System.currentTimeMillis();
    messagePrelude();
    long endTime = System.currentTimeMillis();
    messagePostlude();
    return endTime - startTime;
  }


  /**
   * Returns the result of a TCP message test, parameterized by
   * <i>experimentSize</i>, <i>bufferSize</i>, and <i>messageSize</i>,
   * performed with the sensor.
   */
  public TcpMessageTestResult tcpMessageTest(int experimentSize,
                                             int bufferSize,
                                             int messageSize) throws Exception {
    byte[][] allBytes = {NwsMessage.toBytes(experimentSize),
                         NwsMessage.toBytes(bufferSize),
                         NwsMessage.toBytes(messageSize)};

    long bandwidthFinish;
    long bandwidthStart;
    Socket expConnection;
    Handshake handshake;
    long latencyFinish;
    long latencyStart;
    int leftToSend = experimentSize;
    byte[] payload = new byte[messageSize];
    TcpMessageTestResult returnValue = new TcpMessageTestResult(0.0, 0.0);
    long startTime;

    messagePrelude();

    NwsMessage.send
      (connection, TCP_BW_REQ, NwsMessage.concatenateBytes(allBytes));
    handshake = (Handshake)NwsMessage.receive(connection, this);
    /*
     * Note: Java provides no way to set the socket send buffer size.  Even the
     * setSendBufferSize() method added in 1.2 supports only UDP sockets.
     */
    expConnection = new Socket(connection.getInetAddress(), handshake.port);

    latencyStart = System.currentTimeMillis();
    expConnection.getOutputStream().write(0);
    expConnection.getInputStream().read();
    latencyFinish = System.currentTimeMillis();
    returnValue.latency = (double)(latencyFinish - latencyStart);

    bandwidthStart = System.currentTimeMillis();
    while(leftToSend > messageSize) {
      expConnection.getOutputStream().write(payload);
      leftToSend -= messageSize;
    }
    expConnection.getOutputStream().write(payload, 0, leftToSend);
    expConnection.getInputStream().read();
    bandwidthFinish = System.currentTimeMillis();
    expConnection.close();
    returnValue.bandwidth = ((double)experimentSize * 8.0) /
                            (double)(bandwidthFinish-bandwidthStart) / 1000.0;

    messagePostlude();
    return returnValue;

  }


  /** The record transmitted in response to a TCP_BW_REQ. */
  protected static class Handshake {

    /** Reserved for future use. */
    public int address;
    /** The port to connect to. */
    public short port;

    /** Produces a Handshake from the parameters. */
    public Handshake(int address,
                     short port) {
      this.address = address;
      this.port = port;
    }

    /** Produces a Handshake initialized by reading <i>stream</i>. */
    public Handshake(DataInputStream stream) throws Exception {
      address = stream.readInt();
      port = stream.readShort();
    }

  }


  protected static final int ACTIVITY_START = NwsMessage.SENSOR_FIRST_MESSAGE;
  protected static final int ACTIVITY_STARTED = ACTIVITY_START + 1;

  protected static final int ACTIVITY_STOP =  ACTIVITY_STARTED + 1;
  protected static final int ACTIVITY_STOPPED = ACTIVITY_STOP + 1;

  protected static final int SENSOR_FAILED = NwsMessage.SENSOR_LAST_MESSAGE;

  protected static final int CLIQUE_ACTIVATE = NwsMessage.CLIQUE_FIRST_MESSAGE;
  protected static final int CLIQUE_DIE = CLIQUE_ACTIVATE + 1;
  protected static final int CLIQUE_TOKEN_ACK = CLIQUE_DIE + 1;
  protected static final int CLIQUE_TOKEN_FWD = CLIQUE_TOKEN_ACK + 1;
  protected static final int TCP_BW_REQ = NwsMessage.SKILLS_FIRST_MESSAGE;
  protected static final int TCP_HANDSHAKE = 411;


  /** See NwsMessage. */
  public Object receive(int message,
                        DataInputStream stream,
                        int dataLength) throws Exception {
    return (message == ACTIVITY_STARTED) ? null :
           (message == ACTIVITY_STOPPED) ? null :
           (message == TCP_HANDSHAKE) ? new Handshake(stream) :
           super.receive(message, stream, dataLength);
  }


  /**
   * The main() method for this class is a small test program that takes a list
   * of sensor specifications and prints the results of a standard TCP message
   * test for each of them.
   */
  public static void main(String[] args) {

    final int ONE_K = 1024;
    int bufferSize = 32 * ONE_K;
    int experimentSize = 64 * ONE_K;
    int messageSize = 16 * ONE_K;
    NwsSensor sensor;
    TcpMessageTestResult result;

    for(int i = 0; i < args.length; i++) {
      sensor = new NwsSensor(args[i], 8060);
      System.out.print("(" + experimentSize / ONE_K + "k" +
                       "," + bufferSize / ONE_K + "k" +
                       "," + messageSize / ONE_K + "k" +
                       ") to " + sensor.toString() + ":");
      try {
        result = sensor.tcpMessageTest(experimentSize, bufferSize, messageSize);
        System.out.println(" bandwidth: " + result.bandwidth +
                           " latency: " + result.latency);
      } catch(Exception x) {
        System.out.println(" failed");
      }
    }

  }


}
