/* $Id: Progress.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.util.*;

import edu.utk.cs.loci.ibp.Log;

public class Progress
{
    public static Log DEBUG = new Log( true );

    private long length;
    private long total;
    private double percentComplete;
    private double throughput;
    private double eta;
    private Date start;
    private List listeners;

    private String statusMsg;

    private static ProgressEvent e = new ProgressEvent();

    public Progress( long length )
    {
        this.length = length;
        this.total = 0;
        this.percentComplete = 0.0;
        this.throughput = 0.0;
        this.eta = 0.0;
        this.start = new Date();
        this.listeners = new ArrayList();
        this.statusMsg = "";
    }

    public synchronized void addProgressListener( Object listener )
    {

        DEBUG.println( "addProgressListener!" );
        listeners.add( listener );
    }

    public synchronized void update( long progress )
    {
        this.update( progress, "Continuing ..." );
    }

    public synchronized void update( long progress, String statusMsg )
    {

        this.statusMsg = statusMsg;

        total += progress;

        percentComplete = ( (double) total / (double) length ) * 100.0f;

        Date now = new Date();
        double elapsed = (now.getTime() - start.getTime()) / 1000.0;

        throughput = (total * 8.0) / 1024.0 / 1024.0 / elapsed;

        long remaining = length - total;
        eta = (double) remaining * elapsed / (double) total;

        DEBUG.println( "Exnode Progress: " + elapsed + "s, " + total
            + " bytes = " + (throughput / 8.0) + "MB/s, eta=" + eta + "s" );

        if ( remaining != 0 )
        {
            fireProgressChangedEvent();
            DEBUG.println( "Progress (update):01 remaining = " + remaining );
        }
        else
        {
            fireProgressChangedEvent();
            fireProgressDoneEvent();
            DEBUG.println( "Progress (update):FINAL remaining = " + remaining );
        }

    }

    public void fireProgressChangedEvent()
    {
        Iterator i = listeners.iterator();

        while ( i.hasNext() )
        {
            ProgressListener l = (ProgressListener) i.next();
            l.progressUpdated( e );
        }
    }

    public void fireProgressDoneEvent()
    {
        try
        {
            Iterator i = listeners.iterator();

            //if(!i.hasNext()) DEBUG.error("12: fireProgressDoneEvent: Y A RIEN DU TOUT! i = " + i);
            while ( i.hasNext() )
            {
                ProgressListener l = (ProgressListener) i.next();
                l.progressDone( e );
            }

        }
        catch ( Exception excpt )
        {
            DEBUG.error( "fireProgressDoneEvent: " + excpt );
        }
    }

    public void fireProgressErrorEvent()
    {
        Iterator i = listeners.iterator();
        while ( i.hasNext() )
        {
            ProgressListener l = (ProgressListener) i.next();
            l.progressError( e );
        }
    }

    public synchronized long getProgress()
    {
        return (total);
    }

    public synchronized double getPercentComplete()
    {
        return (percentComplete);
    }

    public synchronized double getThroughput()
    {
        return (throughput);
    }

    public synchronized double getETA()
    {
        return (eta);
    }

    public synchronized String getStatusMsg()
    {
        return this.statusMsg;
    }

    public synchronized void setDone()
    {
        this.statusMsg = "Done.";
        percentComplete = 100.0;
        eta = 0.0;
        // lfgs: don't set to 0 so you can still see the final throughput
        // throughput = 0.0;
    }

}