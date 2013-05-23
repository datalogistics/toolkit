import java.io.*;
import java.net.*;
import java.util.*;


/**
 * The NwsNameServer class implements the protocol specific to name server
 * hosts -- attribute-based object registration and retrieval.
 */
public class NwsNameServer extends NwsHost {


  /** See the constructor for NwsHost. */
  public NwsNameServer(String hostName,
                       int hostPort,
                       boolean keepOpen) {
    super(hostName, hostPort, keepOpen);
  }


  /** See the constructor for NwsHost. */
  public NwsNameServer(String hostName,
                       int hostPort) {
    super(hostName, hostPort);
  }


  /** See the constructor for NwsHost. */
  public NwsNameServer(Socket s) {
    super(s);
  }


  /**
   * The Attribute class pairs a name with a value.  NWS name servers store
   * objects as sets of these attributes.
   */
  public static class Attribute {

    public String name;
    public String value;

    /** Produces an attribute that pairs <i>name</i> with <i>value</i>. */
    public Attribute(String name,
                     String value) {
      this.name = name;
      this.value = value;
    }

    /** Produces an attribute from the "name:value" string <i>asString</i> */
    public Attribute(String asString) {
      int colonPlace = asString.indexOf(":");
      if(colonPlace == -1) {
        name = asString;
        value = "";
      }
      else {
        name = asString.substring(0, colonPlace);
        value = asString.substring(colonPlace + 1);
      }
    }

    /** Returns the fields of the attribute in the form "name:value". */
    public String toString() {
      return name + ":" + value;
    }

  }


  /** Indicates that a registration should be held permanently. */
  public static final int FOREVER = 0;


  /**
   * Searches <i>attributes</i> for an attribute named <i>name</i>.  Returns
   * the attribute if found; otherwise, returns null.
   */
  public static Attribute findAttribute(Attribute[] attributes,
                                        String name) {
    for(int i = 0; i < attributes.length; i++) {
      if(name.equals(attributes[i].name))
        return attributes[i];
    }
    return null;
  }


  /**
   * Searches <i>attributes</i> for an attribute named <i>name</i>.  Returns
   * the attribute value if found; otherwise, returns null.
   */
  public static String getAttributeValue(Attribute[] attributes,
                                         String name) {
    Attribute attribute = findAttribute(attributes, name);
    return (attribute == null) ? null : attribute.value;
  }


  /**
   * Registers an object consisting of the set of attributes in <i>object</i>.
   * The registration will be retained for <i>forHowLong</i> seconds.
   */
  public void register(Attribute[] object,
                       int forHowLong) throws Exception {

    StringBuffer allAttributes = new StringBuffer();
    String completeObject;
    byte[][] messageBytes = {null, null};

    for(int i = 0; i < object.length; i++)
      allAttributes.append(object[i].name + ":" + object[i].value + "\t");
    allAttributes.append("\t");
    completeObject = allAttributes.toString();
    messageBytes[0] = (new CString(completeObject)).toBytes();
    messageBytes[1] = NwsMessage.toBytes(forHowLong);

    messagePrelude();
    NwsMessage.send
      (connection, NS_REGISTER, NwsMessage.concatenateBytes(messageBytes));
    NwsMessage.receive(connection, this);
    messagePostlude();

  }


  /**
   * Returns the set of registered objects that match <i>filter</i>, an
   * LDAP-style search filter.  Each term in a search filter specifies, in
   * parentheses, an attribute name and a value or value range which the
   * retrieved objects must have.  NWS name servers support the relational
   * operators =, &lt;=, and &gt;=, and wild cards (*) can be included in the
   * value of the = operator.  Logical AND (&), OR (|) and NOT (!) operations
   * may be used as prefix operators, surrounded by another set of parentheses,
   * to combine filter terms into more complex filters.  A typical filter:
   *   "(&(hostType=sensor)(|((port=8030))(!(port=80*))))"
   * This would retrieve all sensor registrations that have a port attribute
   * which is either equal to 8030 or does not begin with 80.
   */
  public Attribute[][] search(String filter) throws Exception {

    Vector allObjects;
    int attributeEnd;
    Vector attributes;
    int objectEnd;
    String object;
    String results;
    Attribute[][] returnValue;

    messagePrelude();
    NwsMessage.send(connection, NS_SEARCH, (new CString(filter)).toBytes());
    results = (String)NwsMessage.receive(connection, this);
    messagePostlude();

    allObjects = new Vector();
    while(!results.equals("")) {
      objectEnd = results.indexOf("\t\t");
      if(objectEnd == -1) break;  /* Shouldn't happen. */
      object = results.substring(0, objectEnd + 1);
      results = results.substring(objectEnd + 2);
      attributes = new Vector();
      while(!object.equals("")) {
        attributeEnd = object.indexOf("\t");
        attributes.addElement(new Attribute(object.substring(0, attributeEnd)));
        object = object.substring(attributeEnd + 1);
      }
      allObjects.addElement(attributes);
    }

    returnValue = new Attribute[allObjects.size()][];
    for(int i = 0; i < returnValue.length; i++) {
      returnValue[i] = new Attribute[((Vector)allObjects.elementAt(i)).size()];
      for(int j = 0; j < returnValue[i].length; j++)
        returnValue[i][j] =
          (Attribute)((Vector)allObjects.elementAt(i)).elementAt(j);
    }
    return returnValue;

  }


  public void unregister(String filter) throws Exception {
    messagePrelude();
    NwsMessage.send(connection, NS_UNREGISTER, (new CString(filter)).toBytes());
    NwsMessage.receive(connection, this);
    messagePostlude();
  }


  protected static final int NS_REGISTER = NwsMessage.NAMESERVER_FIRST_MESSAGE;
  protected static final int NS_REGISTERED = NS_REGISTER + 1;
  protected static final int NS_SEARCH = NS_REGISTERED + 1;
  protected static final int NS_SEARCHED = NS_SEARCH + 1;
  protected static final int NS_UNREGISTER = NS_SEARCHED + 1;
  protected static final int NS_UNREGISTERED = NS_UNREGISTER + 1;

  protected static final int NS_FAILED = NwsMessage.NAMESERVER_LAST_MESSAGE;


  /** See NwsMessage. */
  public Object receive(int message,
                        DataInputStream stream,
                        int dataLength) throws Exception {
    return (message == NS_REGISTERED) ? null :
           (message == NS_SEARCHED) ?
              (new CString(dataLength, stream)).toString() :
           (message == NS_UNREGISTERED) ? null :
           super.receive(message, stream, dataLength);
  }


  /**
   * The main() method for this class is a small test program that takes a
   * name server specification and a search filter and prints the results of
   * the filtered search.
   */
  public static void main(String[] args) {
    try {
      Attribute[][] results =
        new NwsNameServer(args[0], 8090).search(args[1]);
      for(int i = 0; i < results.length; i++) {
        for(int j = 0; j < results[i].length; j++) {
          System.out.print
            (results[i][j].name + ":" + results[i][j].value + " ");
        }
        System.out.println("");
      }
    } catch(Exception x) { System.err.println(x.toString()); }
  }


}
