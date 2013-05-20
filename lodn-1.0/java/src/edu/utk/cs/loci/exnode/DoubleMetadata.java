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
public class DoubleMetadata extends Metadata
{
    public DoubleMetadata( String name, Double value )
    {
        this.name = name;
        this.value = value;
    }

    public DoubleMetadata( String name, double value )
    {
        this.name = name;
        this.value = new Double( value );
    }

    public Double getDouble()
    {
        return ((Double) value);
    }

    public String toString()
    {
        return (new String( "Metadata(" + name + ",DOUBLE," + (Double) value
            + ")" ));
    }

    public String toXML()
    {
        StringBuffer sb = new StringBuffer();

        sb.append( "<exnode:metadata name=\"" + name + "\" type=\"double\">" );
        sb.append( (Double) value );
        sb.append( "</exnode:metadata>\n" );

        return (sb.toString());
    }

    public static Metadata fromXML( Element e )
    {
        String name = e.getAttribute( "name" );
        Node child = e.getFirstChild();
        Double value = new Double( child.getNodeValue() );

        return (new DoubleMetadata( name, value ));
    }
}