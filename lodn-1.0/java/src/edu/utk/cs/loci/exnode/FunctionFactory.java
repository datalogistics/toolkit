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
public class FunctionFactory
{

    public FunctionFactory()
    {
    }

    public static Function getFunction( String name )
        throws UnknownFunctionException
    {
        if ( name.compareToIgnoreCase( "checksum" ) == 0 )
        {
            return (new ChecksumFunction());
        }
        else if ( name.compareToIgnoreCase( "xor_encrypt" ) == 0 )
        {
            return (new XorEncryptFunction());
        }
        else if ( name.compareToIgnoreCase( "des_encrypt" ) == 0 )
        {
            return (new DesEncryptFunction());
        }
        else if ( name.compareToIgnoreCase( "aes_encrypt" ) == 0 )
        {
            return (new AesEncryptFunction());
        }
        else if ( name.compareToIgnoreCase( "checksum" ) == 0 )
        {
            return (new ChecksumFunction());
        }
        else if ( name.compareToIgnoreCase( "zlib_compress" ) == 0 )
        {
            return (new CompressFunction());
        }
        else if ( name.compareToIgnoreCase( "identity" ) == 0 )
        {
            return (new IdentityFunction());
        }
        else
        {
            throw (new UnknownFunctionException( "Unknown function " + name ));
        }
    }
}