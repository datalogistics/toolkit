/*
 * Created on Mar 9, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package edu.utk.cs.loci.exnode;

import java.io.IOException;

/**
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class MemorySyncFile implements SyncFile
{
    private byte[] buffer;

    public MemorySyncFile( byte[] byteArray )
    {
        this.buffer = byteArray;
    }

    public MemorySyncFile( int size )
    {
        this.buffer = new byte[size];
    }

    public byte[] getBuffer()
    {
        return this.buffer;
    }

    /* (non-Javadoc)
     * @see edu.utk.cs.loci.exnode.SyncFile#write(byte[], long, int)
     */
    public void write( byte[] inputBuf, long position, int length )
        throws IOException
    {
        if ( (position + length > (long) Integer.MAX_VALUE)
            || (position + length >= (long) this.buffer.length) )
        {
            throw new IOException( "Write to " + position + " of " + length
                + " bytes beyond range of buffer, length=" + this.buffer.length );
        }

        System.arraycopy( inputBuf, 0, this.buffer, (int) position, length );
    }

    /* (non-Javadoc)
     * @see edu.utk.cs.loci.exnode.SyncFile#read(byte[], long)
     */
    public int read( byte[] outputBuf, long position ) throws IOException
    {
        if ( (position > (long) Integer.MAX_VALUE)
            || (position >= (long) this.buffer.length) )
        {
            throw new IOException( "Read from position " + position
                + " is beyond range of buffer, length=" + this.buffer.length );
        }

        int bytesToRead = (int) Math.max(
            this.buffer.length - position, outputBuf.length );

        System.arraycopy(
            this.buffer, (int) position, outputBuf, 0, bytesToRead );

        return bytesToRead;
    }

    /* (non-Javadoc)
     * @see edu.utk.cs.loci.exnode.SyncFile#length()
     */
    public long length() throws IOException
    {
        return this.buffer.length;
    }

    /* (non-Javadoc)
     * @see edu.utk.cs.loci.exnode.SyncFile#close()
     */
    public void close() throws IOException
    {
        // do nothing
    }

}