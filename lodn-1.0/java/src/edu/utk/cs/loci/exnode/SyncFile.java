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
public interface SyncFile
{
    public void write( byte[] inputBuf, long position, int length )
        throws IOException;

    public int read( byte[] outputBuf, long position )
        throws IOException;

    public long length() throws IOException;

    public void close() throws IOException;
}