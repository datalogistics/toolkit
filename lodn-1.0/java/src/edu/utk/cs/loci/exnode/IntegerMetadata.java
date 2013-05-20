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
public class IntegerMetadata extends Metadata
{
    public IntegerMetadata( String name, Long value )
    {
        this.name = name;
        this.value = value;
    }

    public IntegerMetadata( String name, long value )
    {
        this.name = name;
        this.value = new Long( value );
    }

    public Long getInteger()
    {
        return ((Long) value);
    }

    public String toString()
    {
        return (new String( "Metadata(" + name + ",INTEGER," + (Integer) value
            + ")" ));
    }

    public String toXML()
    {
        StringBuffer sb = new StringBuffer();

        sb.append( "<exnode:metadata name=\"" + name + "\" type=\"integer\">" );
        sb.append( (Long) value );
        sb.append( "</exnode:metadata>\n" );

        return (sb.toString());
    }

    public static Metadata fromXML( Element e )
    {
        String name = e.getAttribute( "name" );
        Node child = e.getFirstChild();
        Long value = new Long( child.getNodeValue() );

        return (new IntegerMetadata( name, value ));
    }
}