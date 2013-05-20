/*
 * Created on Nov 24, 2003
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.exnode;

import java.util.*;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class DoubleVectorArgument extends VectorArgument
{
    public DoubleVectorArgument( String name )
    {
        super( name );
    }

    public void insertDouble( int i, DoubleArgument arg )
    {
        insert( i, arg );
    }

    public Double getDouble( int i )
    {
        return (getElement( i ).getDouble());
    }

    public String toXML()
    {
        StringBuffer sb = new StringBuffer();

        sb.append( "<exnode:argument name=\"" + name + "\" type=\"meta\">\n" );

        Vector v = (Vector) value;
        Argument arg;
        for ( int i = 0; i < v.size(); i++ )
        {
            try
            {
                arg = getElement( i );
                sb.append( arg.toXML() );
            }
            catch ( ArrayIndexOutOfBoundsException e )
            {
                sb.append( new DoubleArgument( "", 0.0 ).toXML() );
            }
        }
        sb.append( "</exnode:argument>\n" );

        return (sb.toString());
    }
}