import java.io.*;
import java.net.*;


/**
 * The NwsMemory class implements the protocol specific to memory hosts --
 * fixed-length record storage and retrieval and expired storage purging.
 */
public class NwsMemory extends NwsHost {


  /** See the constructor for NwsHost. */
  public NwsMemory(String hostName,
                   int hostPort,
                   boolean keepOpen) {
    super(hostName, hostPort, keepOpen);
  }


  /** See the constructor for NwsHost. */
  public NwsMemory(String hostName,
                   int hostPort) {
    super(hostName, hostPort);
  }


  /** See the constructor for NwsHost. */
  public NwsMemory(Socket s) {
    super(s);
  }


  /**
   * Requests that the memory begin automatically forwarding records each time
   * one is stored under any of the names listed in <i>names</i>.  Calling this
   * method terminates any previous autofetch request, so calling the method
   * with an empty string effectively ends autofetching activity.  Forwarded
   * records can be retrieved from the autoFetchCheck() method.  NOTE: it makes
   * no sense to call this method on a host for which the keepOpen constructor
   * parameter was false, since the connection will be closed before returning.
   */
  public void autoFetchBegin(String[] names) throws Exception {
    String allNames = new String();
    messagePrelude();
    for(int i = 0; i < names.length; i++) {
      allNames = allNames + names[i] + "\t";
    }
    allNames = allNames.substring(0, allNames.length() - 2);
    NwsMessage.send
      (connection, AUTOFETCH_BEGIN, new CString(allNames).toBytes());
    NwsMessage.receive(connection, this);
    messagePostlude();
  }


  /**
   * Checks to see if any records have arrived from the host since the
   * previous call.  If so, returns a string consisting of the record appended
   * to the name under which it was stored; if not, returns null.
   */
  public String autoFetchCheck() throws Exception {
    State fetchedState;
    if(connection.getInputStream().available() == 0)
      return null;
    fetchedState = (State)NwsMessage.receive(connection, this);
    return fetchedState.id + " " + fetchedState.contents[0];
  }


  /**
   * Requests that the memory delete any stored files which have not been
   * accessed or modified in the last <i>idle</i> seconds.
   */
  public void clean(int idle) throws Exception {
    messagePrelude();
    NwsMessage.send(connection, MEMORY_CLEAN, NwsMessage.toBytes(idle));
    NwsMessage.receive(connection, this);
    messagePostlude();
  }


  /** Returns <i>howMany</i> records most recently stored under <i>name</i>. */
  public String[] retrieve(String name,
                           int howMany) throws Exception {
    State fetchedState;
    State requestState = new State(name, howMany);
    messagePrelude();
    NwsMessage.send(connection, FETCH_STATE, requestState.toBytes());
    fetchedState = (State)NwsMessage.receive(connection, this);
    messagePostlude();
    return fetchedState.contents;
  }


  /**
   * Stores <i>contents</i> under <i>name</i> with each element padded with
   * spaces to <i>recordSize</i> characters.
   */
  public void store(String name,
                    int recordSize,
                    String[] contents) throws Exception {
    State storeState = new State(name, recordSize, contents);
    messagePrelude();
    NwsMessage.send(connection, STORE_STATE, storeState.toBytes());
    NwsMessage.receive(connection, this);
    messagePostlude();
  }


  /** The state record transmitted with memory store/fetch requests. */
  protected static class State {

    /** The state name. */
    public String id;
    /** The size of each (fixed-length) record. */
    public int recSize;
    /** The count of records enclosed (store) or to retrieve (fetch) */
    public int recCount;
    /**
     * The generation time of the enclosed records (store) or the oldest to
     * retrieve (fetch).  Presently unused here.
     */
    public double seqNo;
    /** How long to keep the enclosed records (store).  Unused here. */
    public double timeOut;
    /** The records.  Empty for retrieval requests. */
    public String[] contents;

    protected static final int STATE_NAME_SIZE = 128;

    /** Produces a state to get <i>recCount</i> records from state <i>id</i>. */
    public State(String id,
                 int recCount) {
      this.id = id;
      recSize = 0;
      this.recCount = recCount;
      seqNo = 0.0;
      timeOut = 0.0;
      contents = null;
    }

    /**
     * Produces a state to store <i>contents</i>, padded to <i>recSize</i>,
     * under <i>id</i>.
     */
    public State(String id,
                 int recSize,
                 String[] contents) {
      int i;
      String padding = new String();
      this.id = id;
      this.recSize = recSize;
      recCount = contents.length;
      seqNo = System.currentTimeMillis() / 1000;
      timeOut = 315360000.0; /* 10 years */
      this.contents = new String[recCount];
      for(i = 0; i < recSize; i++)
        padding = padding + " ";
      for(i = 0; i < recCount; i++) {
        this.contents[i] =
          (contents[i].length() >= recSize) ?
          contents[i].substring(0, recSize) :
          contents[i] + padding.substring(0, recSize - contents[i].length());
      }
    }

    /** Produces a state initialized by reading <i>stream</i>. */
    public State(DataInputStream stream) throws Exception {
      id = new CString(STATE_NAME_SIZE, stream).toString();
      recSize = stream.readInt();
      recCount = stream.readInt();
      seqNo = stream.readDouble();
      timeOut = stream.readDouble();
      contents = new String[recCount];
      for(int i = 0; i < recCount; i++)
        contents[i] = new CString(recSize, stream).toString();
    }

    /** Returns the fields of the State converted to a byte array. */
    public byte[] toBytes() {
      ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
      DataOutputStream dataStream = new DataOutputStream(byteStream);
      try {
        dataStream.write(new CString(STATE_NAME_SIZE, id).toBytes());
        dataStream.writeInt(recSize);
        dataStream.writeInt(recCount);
        dataStream.writeDouble(seqNo);
        dataStream.writeDouble(timeOut);
        for(int i = 0; i < recCount; i++)
          dataStream.writeBytes(contents[i]);
      } catch(Exception x) { }
      return byteStream.toByteArray();
    }

  }


  /** See NwsMessage. */
  public Object receive(int message,
                        DataInputStream stream,
                        int dataLength) throws Exception {
    return (message == AUTOFETCH_ACK) ? null :
           (message == MEMORY_CLEANED) ? null :
           (message == STATE_FETCHED) ? new State(stream) :
           (message == STATE_STORED) ? null :
           super.receive(message, stream, dataLength);
  }


  protected static final int STORE_STATE = NwsMessage.MEMORY_FIRST_MESSAGE;
  protected static final int STATE_STORED = STORE_STATE + 1;

  protected static final int FETCH_STATE = STATE_STORED + 1;
  protected static final int STATE_FETCHED = FETCH_STATE + 1;

  protected static final int AUTOFETCH_BEGIN = STATE_FETCHED + 1;
  protected static final int AUTOFETCH_ACK = AUTOFETCH_BEGIN + 1;

  protected static final int MEMORY_CLEAN = AUTOFETCH_ACK + 1;
  protected static final int MEMORY_CLEANED = MEMORY_CLEAN + 1;

  protected static final int MEMORY_FAILED = NwsMessage.MEMORY_LAST_MESSAGE;


  /**
   * The main() method for this class is a small test program that takes a
   * memory specification and a state name and prints the newest 1000 records.
   */
  public static void main(String[] args) {
    String[] contents;
    if(args.length != 2) {
      System.err.println("Usage: NwsMemory <host> <state>");
      System.exit(1);
    }
    try {
      contents = new NwsMemory(args[0], 8050).retrieve(args[1], 1000);
      for(int i = 0; i < contents.length; i++)
        System.out.println(contents[i].trim());
    } catch(Exception x) { System.err.println(x.toString()); }
  }


}
