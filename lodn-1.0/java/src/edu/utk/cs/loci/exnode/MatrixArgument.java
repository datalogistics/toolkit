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
public abstract class MatrixArgument extends Argument
{
    public MatrixArgument( String name )
    {
        this.name = name;
        this.value = new Vector();
    }

    public void insert( int i, int j, Argument arg )
    {
        Vector rows = (Vector) value;

        if ( i > rows.size() )
        {
            rows.setSize( i );
        }

        if ( rows.elementAt( i ) == null )
        {
            rows.add( i, new Vector() );
        }

        Vector v = (Vector) rows.elementAt( i );

        if ( j > v.size() )
        {
            v.setSize( j );
        }

        v.add( j, arg );
    }

    public Argument getElement( int i, int j )
        throws ArrayIndexOutOfBoundsException
    {
        Vector rows = (Vector) value;
        Vector v = (Vector) rows.elementAt( i );
        return ((Argument) v.elementAt( j ));
    }
}