/* $Id: JobQueue.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.util.*;

public class JobQueue
{

    private LinkedList queue;

    public JobQueue()
    {
        queue = new LinkedList();
    }

    public synchronized void clear()
    {
        queue.clear();
    }

    public synchronized void add( Object o )
    {
        queue.add( o );
    }

    public synchronized void remove( Object o )
    {
        queue.remove( o );
    }

    public synchronized Object remove()
    {
        return (queue.removeFirst());
    }

    public synchronized int size()
    {
        return (queue.size());
    }

}