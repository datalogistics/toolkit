/*
 * Created on Jan 13, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.exnode;

import java.io.*;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class PhysicalSyncFile implements SyncFile
{
    private RandomAccessFile file;

    public PhysicalSyncFile( String name ) throws FileNotFoundException
    {
        file = new RandomAccessFile( name, "rw" );
    }

    public synchronized void write( byte[] inputBuf, long position, int length )
        throws IOException
    {
        file.seek( position );
        file.write( inputBuf, 0, length );
    }

    public synchronized int read( byte[] outputBuf, long position )
        throws IOException
    {
        file.seek( position );
        return (file.read( outputBuf ));
    }

    public synchronized long length() throws IOException
    {
        return (file.length());
    }

    public synchronized void close() throws IOException
    {
        file.close();
    }
}