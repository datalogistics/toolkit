/* $Id: WriteThread.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.util.*;

public class WriteThread extends Thread
{

    private JobQueue queue;

    public WriteThread( JobQueue queue )
    {
        this.queue = queue;
    }

    public void run()
    {
        boolean done = false;

        while ( !done )
        {
            try
            {
                WriteJob job = (WriteJob) queue.remove();
                job.execute();
            }
            catch ( NoSuchElementException e )
            {
                done = true;
            }
            catch ( WriteException e )
            {
            }
        }
    }
}