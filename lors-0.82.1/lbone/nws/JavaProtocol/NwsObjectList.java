import java.awt.*;
import java.util.*;


/**
 * The NwsObjectList class is a List component for NWS name server objects.
 * Objects are entered in the list in sorted order, and elision of duplicate
 * attributes values can be used to tidy the display.
 */
public class NwsObjectList extends java.awt.List {

  /**
   * Produces a NwsObjectList with <i>rows</i> visible entries that shows the
   * values of each of the attribute named in <i>displayAttributes</i>.  If
   * <i>multipleMode</i> is true, the user is allowed to choose multiple
   * items from the list.  If <i>elide</i> is true, duplicated attribute
   * values in the display are replace by elipses.
   */
  public NwsObjectList(int rows,
                       boolean multipleMode,
                       String[] displayAttributes,
                       boolean elide) {
    super(rows, multipleMode);
    this.displayAttributes = displayAttributes;
    this.elide = elide;
  }

  /** Adds a new object with the specified attributes to the list. */
  public void add(NwsNameServer.Attribute[] nwsObject) {

    String display = getDisplay(nwsObject);
    int i;
    String oldDisplay = "";

    /* Find where to insert the item (in sorted order). */
    for(i = 0; i < getItemCount(); i++) {
      oldDisplay = getDisplay(getAttributes(i));
      if(oldDisplay.compareTo(display) > 0)
        break;
    }

    if(elide) {
      if(i < getItemCount()) {
        remove(i);
        add(getElidedDisplay(oldDisplay, display), i);
      }
      if(i > 0)
        display = getElidedDisplay(display, getDisplay(getAttributes(i - 1)));
    }
    add(display, i);
    entries.insertElementAt(nwsObject, i);

  }

  /** Deselects every element of the list. */
  public void deselectAll() {
    int[] selected = getSelectedIndexes();
    for(int i = 0; i < selected.length; i++)
      deselect(selected[i]);
  }

  /** Returns the attributes associated with the <i>index</i>th list entry. */
  public NwsNameServer.Attribute[] getAttributes(int index) {
    return (NwsNameServer.Attribute[])entries.elementAt(index);
  }

  /**
   * Given two returnValues from <i>getDisplay()</i>, returns <i>display</i>
   * with any leading components that duplicate those in <i>prior</i>
   * replaced with elipses.
   */
  protected static String getElidedDisplay(String display,
                                           String prior) {
    int substringStart = 0;
    String returnValue = "";
    for(int dashPlace = prior.indexOf("-");
            dashPlace != -1;
            dashPlace = prior.indexOf("-", substringStart)) {
      if(!display.regionMatches(0, prior, 0, dashPlace))
        break;
      returnValue = returnValue + "...-";
      substringStart = dashPlace + 1;
    }
    return returnValue + display.substring(substringStart);
  }

  /** Returns the string to add to the list to represent <i>nwsObject</i>. */
  protected String getDisplay(NwsNameServer.Attribute[] nwsObject) {
    String returnValue = "";
    String separator = "";
    String value;
    for(int i = 0; i < displayAttributes.length; i++) {
      value = NwsNameServer.getAttributeValue(nwsObject,displayAttributes[i]);
      if(value != null) {
        returnValue = returnValue + separator + value;
        separator = "-";
      }
    }
    return returnValue;
  }

  String[] displayAttributes;
  boolean elide;
  Vector entries = new Vector();

}
