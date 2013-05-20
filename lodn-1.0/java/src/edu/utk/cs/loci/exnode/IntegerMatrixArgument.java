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
public class IntegerMatrixArgument extends MatrixArgument
{
    public IntegerMatrixArgument( String name )
    {
        super( name );
    }

    public void insertInteger( int i, int j, IntegerArgument arg )
    {
        insert( i, j, arg );
    }

    public Integer getInteger( int i, int j )
    {
        return (getElement( i, j ).getInteger());
    }

    public String toXML()
    {
        return (new String());
    }
}