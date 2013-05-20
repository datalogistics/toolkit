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
public class StringMatrixArgument extends MatrixArgument
{
    public StringMatrixArgument( String name )
    {
        super( name );
    }

    public void insertString( int i, int j, StringArgument arg )
    {
        insert( i, j, arg );
    }

    public String getString( int i, int j )
    {
        return (getElement( i, j ).getString());
    }

    public String toXML()
    {
        return (new String());
    }
}