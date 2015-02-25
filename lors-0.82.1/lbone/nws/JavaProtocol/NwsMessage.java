import java.io.*;
import java.net.*;


/**
 * The NwsMessage class provides support to the NWS host classes for sending
 * messages to and from NWS hosts.  It encapsulates a number of useful class
 * functions and values and is not intended to be instantiated.
 */
public class NwsMessage {


  /** Message number ranges as defined in messages.h.  */
  public static final int ALLHOSTS_FIRST_MESSAGE = 200;
  public static final int ALLHOSTS_LAST_MESSAGE = 299;

  public static final int CLIQUE_FIRST_MESSAGE = 400;
  public static final int CLIQUE_LAST_MESSAGE = 499;

  public static final int FORECASTER_FIRST_MESSAGE = 500;
  public static final int FORECASTER_LAST_MESSAGE = 599;

  public static final int MEMORY_FIRST_MESSAGE = 100;
  public static final int MEMORY_LAST_MESSAGE = 199;

  public static final int NAMESERVER_FIRST_MESSAGE = 700;
  public static final int NAMESERVER_LAST_MESSAGE = 799;

  public static final int PERIODIC_FIRST_MESSAGE = 300;
  public static final int PERIODIC_LAST_MESSAGE = 399;

  public static final int SENSOR_FIRST_MESSAGE = 600;
  public static final int SENSOR_LAST_MESSAGE = 699;

  public static final int SKILLS_FIRST_MESSAGE = 800;
  public static final int SKILLS_LAST_MESSAGE = 899;


  /**
   * A convenience function that concatenates any number of byte arrays into a
   * single large byte array.
   */
  public static byte[] concatenateBytes(byte[][] arrays) {

    int i;
    byte[] returnValue;
    int totalSize;

    totalSize = 0;
    for(i = 0; i < arrays.length; i++)
      totalSize += arrays[i].length;

    returnValue = new byte[totalSize];

    totalSize = 0;
    for(i = 0; i < arrays.length; i++) {
      System.arraycopy(arrays[i], 0, returnValue, totalSize, arrays[i].length);
      totalSize += arrays[i].length;
    }

    return returnValue;

  }


  /** An interface for a class that can receive NWS messages. */
  public static interface MessageReceiver {
   /**
    * Notification that <i>message</i> has arrived, accompanied by
    * <i>dataLength</i> bytes of data which may be read from <i>stream</i>.
    */
    public Object receive(int message,
                          DataInputStream stream,
                          int dataLength) throws Exception;
  }


  /** Passes the next message that arrives on <i>sd</i> to <i>receiver</i>. */
  public static Object receive(Socket sd,
                               MessageReceiver receiver) throws Exception {
    DataInputStream dataStream = new DataInputStream(sd.getInputStream());
    MessageHeader header = new MessageHeader(dataStream);
    return receiver.receive(header.type, dataStream, header.dataSize);
  }


  /** Sends <i>message</i> on <i>sd</i> accompanied by no data. */
  public static void send(Socket sd,
                          int message) throws Exception {
    sd.getOutputStream().write(new MessageHeader(message, 0).toBytes());
  }


  /** Sends <i>message</i> on <i>sd</i> accompanied by <i>data</i>. */
  public static void send(Socket sd,
                          int message,
                          byte[] data) throws Exception {
    sd.getOutputStream().write
      (new MessageHeader(message, data.length).toBytes());
    sd.getOutputStream().write(data);
  }


  /** A convenience function for converting a double to bytes. */
  public static byte[] toBytes(double value) {
    ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
    try {
      new DataOutputStream(byteStream).writeDouble(value);
    } catch(Exception x) { }
    return byteStream.toByteArray();
  }


  /** A convenience function for converting an integer to bytes. */
  public static byte[] toBytes(int value) {
    ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
    try {
      new DataOutputStream(byteStream).writeInt(value);
    } catch(Exception x) { }
    return byteStream.toByteArray();
  }


  /** The standard NWS message header */
  protected static class MessageHeader {

    public int version = 0x02000000;
    public int type;
    public int dataSize = 0;

    /**
     * Produces an NWS header with the type and size fields set to the
     * parameters.
     */
    public MessageHeader(int type,
                         int size) {
      this.type = type;
      dataSize = size;
    }

    /** Produces an NWS header initialized by reading <i>stream</i>. */
    public MessageHeader(DataInputStream stream) throws Exception {
      version = stream.readInt();
      type = stream.readInt();
      dataSize = stream.readInt();
    }

    /** Returns the contents of the header converted to bytes. */
    public byte[] toBytes() {
      byte[][] allBytes =
       {NwsMessage.toBytes(version), NwsMessage.toBytes(type),
        NwsMessage.toBytes(dataSize)};
      return concatenateBytes(allBytes);
    }

  }


}
