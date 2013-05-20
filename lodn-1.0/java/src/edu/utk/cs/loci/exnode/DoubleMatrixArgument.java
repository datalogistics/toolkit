/*
 * Created on Nov 24, 2003
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.exnode;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class DoubleMatrixArgument extends MatrixArgument
{
    public DoubleMatrixArgument( String name )
    {
        super( name );
    }

    public void insertDouble( int i, int j, DoubleArgument arg )
    {
        insert( i, j, arg );
    }

    public Double getDouble( int i, int j )
    {
        return (getElement( i, j ).getDouble());
    }

    public String toXML()
    {
        return (new String());
    }
}