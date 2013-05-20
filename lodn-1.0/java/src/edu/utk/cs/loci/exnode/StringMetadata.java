/*
 * Created on Nov 21, 2003
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.exnode;

import org.w3c.dom.*;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class StringMetadata extends Metadata
{
    public StringMetadata( String name, String value )
    {
        this.name = name;
        this.value = value;
    }

    public String getString()
    {
        return ((String) value);
    }

    public String toString()
    {
        return (new String( "Metadata(" + name + ",STRING," + (String) value
            + ")" ));
    }

    public String toXML()
    {
        StringBuffer sb = new StringBuffer();

        sb.append( "<exnode:metadata name=\"" + name + "\" type=\"string\">" );
        sb.append( (String) value );
        sb.append( "</exnode:metadata>\n" );

        return (sb.toString());
    }

    public static Metadata fromXML( Element e )
    {
        String name = e.getAttribute( "name" );
        Node child = e.getFirstChild();
        String value = child.getNodeValue();

        return (new StringMetadata( name, value ));
    }
}