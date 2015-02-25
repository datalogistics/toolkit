import java.io.*;
import java.net.*;


/**
 * The NwsHost class implements the protocol common to all NWS hosts -- status
 * testing, diagnostics toggling, halting, and registration messages.
 */
public class NwsHost implements NwsMessage.MessageReceiver {


  /**
   * Produces a host representing the process listing to <i>hostPort</i> on
   * machine <i>hostName</i>, which may be a DNS name or IP address.
   * <i>hostName</i> may also end with a colon and port number, in which case
   * this port number is used instead of <i>hostPort</i>.  <i>keepOpen</i>
   * indicates whether or not the connection to the host should be left open
   * between service requests.
   */
  public NwsHost(String hostName,
                 int hostPort,
                 boolean keepOpen) {
    int colonPlace = hostName.indexOf(":");
    if(colonPlace == -1) {
      name = hostName;
      port = hostPort;
    }
    else {
      name = hostName.substring(0, colonPlace);
      port = Integer.parseInt(hostName.substring(colonPlace + 1));
    }
    persistent = keepOpen;
    connection = null;
  }


  /** Equivalent to NwsHost(hostName, hostPort, false) */
  public NwsHost(String hostName,
                 int hostPort) {
    this(hostName, hostPort, false);
  }


  /** Produces a host connected persistently via <i>s</i>. */
  public NwsHost(Socket s) {
    connection = s;
    persistent = true;
    name = s.getInetAddress().getHostName();
    port = s.getPort();
  }


  /** Information about the host returned by the getHostInfo() method. */
  public static class HostInfo {

    /* Values for the <i>hostType</i> field. */
    public static final int FORECASTER_HOST = 0;
    public static final int MEMORY_HOST = 1;
    public static final int NAME_SERVER_HOST = 2;
    public static final int SENSOR_HOST = 3;

    /** The name with which the host is registered with its name server. */
    public String registrationName;
    /** The type of host; 0..3 for forecaster, memory, name server, sensor. */
    public int hostType;
    /** The machine:port specification of the host's name server. */
    public String nameServer;
    /** An indication of whether the host's registration is current. */
    public boolean healthy;

    protected static final int REGISTRATION_NAME_LEN = MAX_HOST_NAME;
    protected static final int NAME_SERVER_LEN = MAX_HOST_NAME;

    /** Produces a host info with all empty fields. */
    public HostInfo() {
      registrationName = "";
      hostType = 0;
      nameServer = "";
      healthy = false;
    }

    /** Produces a host info intialized by reading <i>stream</i>. */
    public HostInfo(DataInputStream stream) throws Exception {
      registrationName =
        new CString(REGISTRATION_NAME_LEN, stream).toString();
      hostType = stream.readInt();
      nameServer =
        new CString(NAME_SERVER_LEN, stream).toString();
      healthy = stream.readInt() != 0;
    }

    /** Returns the fields of the HostInfo converted to a byte array. */
    public byte[] toBytes() {
      byte[][] allBytes =
       {new CString(REGISTRATION_NAME_LEN, registrationName).toBytes(),
        NwsMessage.toBytes(hostType),
        new CString(NAME_SERVER_LEN, nameServer).toBytes(),
        NwsMessage.toBytes(healthy ? 1 : 0)};
      return NwsMessage.concatenateBytes(allBytes);
    }

    /**
     * A convenience function; formats and returns the field values in the form:
     *  <i>hostType</i> <i>registrationName</i> registered with
     *  <i>nameServer</i> <i>health</i>
     */
    public String toString() {
      StringBuffer returnValue = new StringBuffer();
      returnValue.append(
        (hostType == HostInfo.FORECASTER_HOST) ? "forecaster" :
        (hostType == HostInfo.MEMORY_HOST) ? "memory" :
        (hostType == HostInfo.NAME_SERVER_HOST) ? "name_server" :
        (hostType == HostInfo.SENSOR_HOST) ? "sensor" :
        String.valueOf(hostType));
      returnValue.append(" " + new String(registrationName));
      returnValue.append(" registered with " + new String(nameServer));
      returnValue.append(" " + (healthy ? "healthy" : "sick"));
      return returnValue.toString();
    }

  }


  /** Returns a HostInfo (see) record for this host. */
  public HostInfo getHostInfo() throws Exception {
    HostInfo returnValue;
    messagePrelude();
    NwsMessage.send(connection, HOST_TEST);
    returnValue = (HostInfo)NwsMessage.receive(connection, this);
    messagePostlude();
    return returnValue;
  }


  /**
   * Requests that the host terminate.  <i>password</i> is used to verify that
   * the requester has the right to terminate the host.
   */
  public void halt(String password) throws Exception {
    messagePrelude();
    NwsMessage.send(connection, HOST_DIE, new CString(password).toBytes());
    NwsMessage.receive(connection, this);
    messagePostlude();
  }


  /**
   * Requests that the host change its name server to <i>withWho</i>, a host
   * specification of the form <i>machine</i>[:<i>port</i>].
   */
  public void register(NwsHost withWho) throws Exception {
    messagePrelude();
    NwsMessage.send
      (connection, HOST_REGISTER, new CString(withWho.toString()).toBytes());
    NwsMessage.receive(connection, this);
    messagePostlude();
  }


  /** Error diagnostics: warning, error, and abort levels. */
  public static final int ALL_ERRORS = -1;
  /** Log diagnostics: debug, info, and log levels. */
  public static final int ALL_LOGS = -2;
  /** Both error and log diagnostics. */
  public static final int ALL_DIAGNOSTICS = -3;


  /**
   * Requests that the host toggle production of the diagnostics specified by
   * <i>whichOnes</i>.  If the host is presently writing these diagnostics,
   * they will be suppressed after the call; if the host is presently
   * suppressing these diagnostics, they will be written after the call.
   */
  public void toggleDiagnostics(int whichOnes) throws Exception {
    messagePrelude();
    NwsMessage.send
      (connection, HOST_DIAGNOSTICS, NwsMessage.toBytes(whichOnes));
    NwsMessage.receive(connection, this);
    messagePostlude();
  }


  /** Returns the fields of the host in the form <i>machine</i>:<i>port</i>. */
  public String toString() {
    return name + ":" + port;
  }


  protected static final int MAX_MACHINE_NAME = 64;
  protected static final int MAX_HOST_NAME = MAX_MACHINE_NAME + 1 + 4;


  protected static final int HOST_TEST = NwsMessage.ALLHOSTS_FIRST_MESSAGE;
  protected static final int HOST_TEST_RESULT = HOST_TEST + 1;

  protected static final int HOST_DIAGNOSTICS = HOST_TEST_RESULT + 1;
  protected static final int HOST_DIAGNOSTICS_ACK = HOST_DIAGNOSTICS + 1;

  protected static final int HOST_DIE = HOST_DIAGNOSTICS_ACK + 1;
  protected static final int HOST_DYING = HOST_DIE + 1;

  protected static final int HOST_REGISTER = HOST_DYING + 1;
  protected static final int HOST_REGISTERED = HOST_REGISTER + 1;

  protected static final int HOST_FAILED = NwsMessage.ALLHOSTS_LAST_MESSAGE;
  protected static final int HOST_REFUSED = HOST_FAILED - 1;


  protected Socket connection;
  protected String name;
  protected boolean persistent;
  protected int port;


  /** Closes the host connection if it is not intended to be persistent. */
  protected void messagePostlude() {
    if(!persistent) {
      try {
        connection.close();
      } catch(Exception x) { }
      connection = null;
    }
  }


  /** Tries to make a connection to the host if one does not already exist. */
  protected void messagePrelude() throws Exception {
    if(connection == null)
      connection = new Socket(name, port);
  }


  /** See NwsMessage. */
  public Object receive(int message,
                        DataInputStream stream,
                        int dataLength) throws Exception {
    return (message == HOST_DIAGNOSTICS_ACK) ? null :
           (message == HOST_DYING) ? null :
           (message == HOST_REGISTERED) ? null :
           (message == HOST_TEST_RESULT) ? new HostInfo(stream) : null;
  }
   

  /**
   * The main() method for this class is a small test program that takes a
   * list of host specifications (<i>machine</i>[:<i>port</i>]) and prints the
   * results of a getHostInfo operation for each.
   */
  public static void main(String[] args) {

    for(int i = 0; i < args.length; i++) {
      try {
        System.out.println
          (new NwsHost(args[i], 8060).getHostInfo().toString());
      } catch(Exception x) {
        System.err.println("Unable to contact " + args[i]);
      }
    }

  }


}
