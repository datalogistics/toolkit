/*
 * Created on Nov 24, 2003
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
public class DoubleArgument extends Argument
{
    public DoubleArgument( String name, Double value )
    {
        this.name = name;
        this.value = value;
    }

    public DoubleArgument( String name, double value )
    {
        this( name, new Double( value ) );
    }

    public Double getDouble()
    {
        return ((Double) value);
    }

    public String toXML()
    {
        StringBuffer sb = new StringBuffer();

        sb.append( "<exnode:argument name=\"" + name + "\" type=\"double\">" );
        sb.append( (Double) value );
        sb.append( "</exnode:argument>\n" );

        return (sb.toString());
    }

    public static Argument fromXML( Element e )
    {
        String name = e.getAttribute( "name" );
        Node child = e.getFirstChild();
        Double value = new Double( child.getNodeValue() );

        return (new DoubleArgument( name, value ));
    }
}

