package edu.utk.cs.loci.exnode;

import java.security.*;
import java.io.*;
import javax.crypto.*;
import javax.crypto.spec.*;

import edu.utk.cs.loci.ibp.Log;

import java.security.spec.*;

public class DesEncryptFunction extends Function
{
    public static Log DEBUG = new Log( true );

    private Cipher cipher;
    private String key;

    private int mode = Cipher.DECRYPT_MODE; // by default

    public DesEncryptFunction()
    {
        super( "des_encrypt" );
    }

    public String genkey() throws Exception
    {
        //String key;

        key = super.genkey(); // TODO: Currently use the default key (provided by class Function)
        mode = Cipher.ENCRYPT_MODE;
        setkey( key );

        return key;
    }

    public void setkey( String key )
    {

        PBEKeySpec pbeKeySpec;
        PBEParameterSpec pbeParamSpec;
        SecretKeyFactory keyFac;

        // Salt
        byte[] salt =
            { (byte) 0xc7, (byte) 0x73, (byte) 0x21, (byte) 0x8c, (byte) 0x7e,
                (byte) 0xc8, (byte) 0xee, (byte) 0x99 };

        // Iteration count
        int count = 20;

        this.key = key; // useless, the most important is to init cipher

        try
        {
            //DEBUG.println("SETKEY : " + key);

            char[] charkey = key.toCharArray();

            // Create PBE parameter set
            pbeParamSpec = new PBEParameterSpec( salt, count );
            pbeKeySpec = new PBEKeySpec( charkey );
            keyFac = SecretKeyFactory.getInstance( "PBEWithMD5AndDES" );
            SecretKey pbeKey = keyFac.generateSecret( pbeKeySpec );

            // Create PBE Cipher
            cipher = Cipher.getInstance( "PBEWithMD5AndDES" );

            // Initialize PBE Cipher with key and parameters
            cipher.init( mode, pbeKey, pbeParamSpec );

        }
        catch ( java.security.InvalidKeyException e )
        {
            DEBUG.error( "DES30:" + e );
        }
        catch ( java.security.NoSuchAlgorithmException e )
        {
            DEBUG.error( "DES31:" + e );
        }
        catch ( java.security.spec.InvalidKeySpecException e )
        {
            DEBUG.error( "DES32:" + e );
        }
        catch ( java.security.InvalidAlgorithmParameterException e )
        {
            DEBUG.error( "DES33:" + e );
        }
        catch ( javax.crypto.NoSuchPaddingException e )
        {
            DEBUG.error( "DES34:" + e );
        }

    }

    public byte[] execute( byte[] rawBuf ) throws Exception
    {

        try
        {
            rawBuf = cipher.doFinal( rawBuf );
        }
        catch ( javax.crypto.BadPaddingException e )
        {
            DEBUG.error( "DES40:" + e );
        }
        catch ( IllegalBlockSizeException e )
        {
            DEBUG.error( "DES41:" + e );
        }

        return (rawBuf);
    }
}

