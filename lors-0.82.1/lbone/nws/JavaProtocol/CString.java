/** The CString class implements a C-style nul-terminated character array. */
public class CString {


  /** Produces a <i>howLong</i>-long (in bytes) empty CString.  */
  public CString(int howLong) {
    this(howLong, "");
  }


  /**
   * Produces a (<i>value</i>.length() + 1)-long CString containing
   * <i>value</i>.
   */
  public CString(String value) {
    this(value.length() + 1, value);
  }


  /** Produces a <i>howLong</i>-long CString containing <i>value</i>.  */
  public CString(int howLong,
                 String value) {
    byte[] valueBytes = value.getBytes();
    bytes = new byte[howLong];
    if(valueBytes.length >= howLong) {
      System.arraycopy(valueBytes, 0, bytes, 0, howLong - 1);
      bytes[howLong - 1] = 0;
    }
    else {
      System.arraycopy(valueBytes, 0, bytes, 0, valueBytes.length);
      bytes[valueBytes.length] = 0;
    }
  }


  /**
   * Produces a <i>howLong</i>-long CString initialized by reading characters
   * from <i>stream</i>.
   */
  public CString(int howLong,
                 java.io.DataInputStream stream) throws Exception {
    this(howLong);
    stream.readFully(bytes);
  }


  /**
   * Produces a CString initialized by reading characters from <i>stream</i>
   * until a nul is encountered.  The resulting CString will be exactly large
   * enough to hold this nul-terminated string.
   */
  public CString(java.io.DataInputStream stream) throws Exception {

    int bytesRead = 0;
    byte current;
    byte[] soFar = new byte[100];

    do {
      current = stream.readByte();
      if(bytesRead == soFar.length) {
        byte[] extended = new byte[bytesRead + 100];
        System.arraycopy(soFar, 0, extended, 0, bytesRead);
        soFar = extended;
      }
      soFar[bytesRead++] = current;
    } while(current != 0); 

    bytes = new byte[bytesRead];
    System.arraycopy(soFar, 0, bytes, 0, bytesRead);

  }


  /** Returns the contents of the CString */
  public byte[] toBytes() {
    return bytes;
  }


  /** Converts the contents of the CString to a Java string. */
  public String toString() {
    return new String(bytes);
  }


  protected byte[] bytes;


}
