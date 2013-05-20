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
public abstract class VectorArgument extends Argument
{
    public VectorArgument( String name )
    {
        this.name = name;
        this.value = new Vector();
    }

    public void insert( int i, Argument arg )
    {
        Vector v = (Vector) value;

        if ( i > v.size() )
        {
            v.setSize( i );
        }

        v.add( i, arg );
    }

    public Argument getElement( int i ) throws ArrayIndexOutOfBoundsException
    {
        Vector v = (Vector) value;
        return ((Argument) v.elementAt( i ));
    }
}