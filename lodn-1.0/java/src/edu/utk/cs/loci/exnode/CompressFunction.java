/* $Id: CompressFunction.java,v 1.4 2008/05/24 22:25:51 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.io.*;
import java.util.zip.*;

import edu.utk.cs.loci.ibp.Log;

public class CompressFunction extends Function
{
    public static Log DEBUG = new Log( true );

    private static final int COMPRESS = 0;
    private static final int UNCOMPRESS = 1;

    private String key = null;

    private int mode = UNCOMPRESS; // by default

    public CompressFunction()
    {
        super( "zlib_compress" );
    }

    public void setkey( String key )
    {
        this.key = key;
        DEBUG.println( "GZIPFunction : key = " + key );
    }

    public String genkey()
    {

        mode = COMPRESS;
        key = "(useless)";
        return key;
    }

    // GZIPdata : return a byte array of compressed data in the GZIP
    // format.
    byte[] GZIPdata( byte[] bufin ) throws IOException
    {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        GZIPOutputStream gzos = new GZIPOutputStream( baos );

        gzos.write( bufin, 0, bufin.length );
        gzos.close();
        return (baos.toByteArray());
    }

    // GUNZIPdata :
    byte[] GUNZIPdata( byte[] bufin ) throws IOException
    {
        ByteArrayInputStream bais = new ByteArrayInputStream( bufin );
        GZIPInputStream gunzipis = new GZIPInputStream( bais );

        // Use blocksize field to store the original size
        // lfgs 20050304.1408: changed this to int and changed call to .longValue to .intValue
        // Anyway, the old code was calling .getInteger() anyway so longValues
        // would've always failed anyway even in the old code (I think that's
        // probably why there was a !!! in the comments here.)
        int original_size = getArgument( "blocksize" ).getInteger().intValue(); /* !!! */

        byte[] bufout = new byte[original_size];

        DEBUG.println( "CompressFunction10: bufout.length = " + bufout.length );
        int v = gunzipis.read( bufout, 0, bufout.length );
        gunzipis.close();
        DEBUG.println( "CompressFunction12: v = " + v );
        return bufout;
    }

    public byte[] execute( byte[] rawBuf )
    {
        try
        {
            if ( mode == COMPRESS )
            {
                DEBUG.println( "CompressFunction: COMPRESS!" );
                return GZIPdata( rawBuf );
            }
            else
            {
                DEBUG.println( "CompressFunction: UNCOMPRESS!" );
                return GUNZIPdata( rawBuf );
            }
        }
        catch ( IOException e )
        {
            DEBUG.error( "CompressFunction: " + e );
        }
        return null;
    }
}

