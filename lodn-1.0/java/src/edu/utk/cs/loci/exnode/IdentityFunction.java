/*
 * Created on Jan 28, 2004
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
public class IdentityFunction extends Function
{

    public IdentityFunction()
    {
        super( "identity" );
    }

    public void setkey( String key )
    {
    }

    public byte[] execute( byte[] rawBuf )
    {
        return (rawBuf);
    }
}