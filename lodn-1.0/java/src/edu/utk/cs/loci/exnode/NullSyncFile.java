/*
 * Created on Mar 9, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package edu.utk.cs.loci.exnode;

import java.io.IOException;

import edu.utk.cs.loci.ibp.Log;

/**
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class NullSyncFile implements SyncFile
{
    public static Log DEBUG = new Log( true );
    
    private String name;
    private long length;
    
    public NullSyncFile( String name, long length )
    {
        this.name = name;
        this.length = length;
        DEBUG.error( "NullSyncFile created: " + this.name + ", " + this.length + " bytes." ); 
    }
    
    /* (non-Javadoc)
     * @see edu.utk.cs.loci.exnode.SyncFile#write(byte[], long, int)
     */
    public void write( byte[] inputBuf, long position, int length )
        throws IOException
    {
        if ( position + length >= (long) this.length )
        {
            throw new IOException( "Write to " + position + " of " + length
                + " bytes beyond range of buffer, length=" + this.length );
        }

        DEBUG.error( "NullSyncFile " + this.name + ": write to\t" + position + ", " + length + " bytes" ); 
    }

    /* (non-Javadoc)
     * @see edu.utk.cs.loci.exnode.SyncFile#read(byte[], long)
     */
    public int read( byte[] outputBuf, long position ) throws IOException
    {
        if ( position >= (long) this.length )
        {
            throw new IOException( "Read from position " + position
                + " is beyond range of buffer, length=" + this.length );
        }

        int bytesToRead = (int) Math.max(
            this.length - position, outputBuf.length );

        DEBUG.error( "NullSyncFile " + this.name + ": read from\t" + position + ", " + bytesToRead + " bytes" );

        return bytesToRead;
    }

    /* (non-Javadoc)
     * @see edu.utk.cs.loci.exnode.SyncFile#length()
     */
    public long length() throws IOException
    {
        return this.length;
    }

    /* (non-Javadoc)
     * @see edu.utk.cs.loci.exnode.SyncFile#close()
     */
    public void close() throws IOException
    {
        // do nothing
    }

}
