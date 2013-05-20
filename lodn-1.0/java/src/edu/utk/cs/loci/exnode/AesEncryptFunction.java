/* $Id: AesEncryptFunction.java,v 1.4 2008/05/24 22:25:51 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.security.*;
import java.io.*;
import javax.crypto.*;
import javax.crypto.spec.*;

import edu.utk.cs.loci.ibp.Log;

import java.security.spec.*;

public class AesEncryptFunction extends Function
{
    public static Log DEBUG = new Log( true );

    private Cipher cipher;
    private String key;

    private int mode = Cipher.DECRYPT_MODE; // by default

    public AesEncryptFunction()
    {
        super( "aes_encrypt" );
    }

    public String genkey() throws Exception
    {
        mode = Cipher.ENCRYPT_MODE;

        try
        {
            KeyGenerator kgen = KeyGenerator.getInstance( "AES" );
            kgen.init( 128 );
            SecretKey skey = kgen.generateKey();
            byte[] rawkey = skey.getEncoded();
            setkey( rawkey );
            key = encode( rawkey ); // transform byte array in string

            return key;

        }
        catch ( java.security.NoSuchAlgorithmException e )
        {
            DEBUG.error( "AES10:" + e );
            //throw(new AesEncryptException(e.getMessage()));
        }
        catch ( Exception e )
        {
            throw (new AesEncryptException( e.getMessage() ));
        }
        return null;
    }

    public void setkey( byte[] rawkey ) throws Exception
    {

        try
        {

            SecretKeySpec skeySpec = new SecretKeySpec( rawkey, "AES" );
            cipher = Cipher.getInstance( "AES" );
            cipher.init( mode, skeySpec );

        }
        catch ( java.security.InvalidKeyException e )
        {
            DEBUG.error( "AES30:" + e );
            throw (new AesEncryptException( e.getMessage() ));
        }
        catch ( java.security.NoSuchAlgorithmException e )
        {
            DEBUG.error( "AES31:" + e );
            throw (new AesEncryptException( "AES encryption : "
                + e.getMessage() ));
        }
        catch ( javax.crypto.NoSuchPaddingException e )
        {
            DEBUG.error( "AES32:" + e );
            throw (new AesEncryptException( e.getMessage() ));
        }
        catch ( java.lang.SecurityException e )
        {
            DEBUG.error( "AES33:" + e );
            throw (new AesEncryptException( "AES encryption : "
                + e.getMessage() ));
        }
    }

    public void setkey( String strkey ) throws Exception
    {
        byte[] rawkey = decode( strkey );
        setkey( rawkey );
    }

    public byte[] execute( byte[] rawBuf ) throws Exception
    {
        try
        {
            rawBuf = cipher.doFinal( rawBuf );
        }
        catch ( javax.crypto.BadPaddingException e )
        {
            DEBUG.error( "AES40:" + e );
            throw (new AesEncryptException( e.getMessage() ));
        }
        catch ( IllegalBlockSizeException e )
        {
            DEBUG.error( "AES41:" + e );
            throw (new AesEncryptException( e.getMessage() ));
        }

        return (rawBuf);
    }
}

