/*
 * Created on Jan 31, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package edu.utk.cs.loci.transfer;

import java.util.Comparator;

import edu.utk.cs.loci.ibp.Log;

/**
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class DepotThreadCountComparator implements Comparator
{
    public static Log DEBUG = new Log( true );

    public static final DepotThreadCountComparator singleton = new DepotThreadCountComparator();

    /* 
     * Returns a positive if o1 has a LOWER threadCount than o2.
     * (The rationale is that a depot with fewer outstanding connections
     * is better.)
     */
    public int compare( Object o1, Object o2 )
    {
        TransferStat ts1 = ((DepotScore) o1).getDepotStat();
        TransferStat ts2 = ((DepotScore) o2).getDepotStat();

        String d1Name = ts1.getHostname() + ":" + ts1.getPort();
        String d2Name = ts2.getHostname() + ":" + ts2.getPort();

        DEBUG.print( "Compare: " );

        if ( ts1.isLastTimeFailed() && !ts2.isLastTimeFailed() )
        {
            DEBUG.println( d1Name + " failed last time, while " + d2Name
                + " did not." );
            return -1;
        }
        else if ( !ts1.isLastTimeFailed() && ts2.isLastTimeFailed() )
        {
            DEBUG.println( d2Name + " failed last time, while " + d1Name
                + " did not." );
            return 1;
        }
        else if ( ts1.isLastTimeFailed() && ts2.isLastTimeFailed() )
        {
            double fr1 = ts1.getFailureRate();
            double fr2 = ts2.getFailureRate();
            DEBUG.println( d1Name + " fail rate=" + fr1 + ", " + d2Name
                + " fail rate=" + fr2 );

            // LOWER FAILURE rate should return positive
            int comp = Double.compare( fr2, fr1 );
            if ( comp == 0 )
            {
                // If ts2 failed before ts2 then it's better.
                // (Pick the least recently failed one as better.)
                comp = (ts1.getLastFailTime() < ts2.getLastFailTime()) ? 1 : -1;
            }
            return comp;
        }

        // else neither of their last times are failed

        int tc1 = ts1.getThreadCount();
        int tc2 = ts2.getThreadCount();

        DEBUG.println( d1Name + " tc=" + tc1 + ", " + d2Name + " tc=" + tc2 );

        if ( tc2 != tc1 )
        {
            return (tc2 - tc1);
        }
        else
        {
            // if Threads are equal, then get ts1 is better if it's
            // last try was earlier (meaning, it's more likely to finish its current load sooner).
            long lt1 = ts1.getLastTryTime();
            long lt2 = ts2.getLastTryTime();

            DEBUG.println( d1Name + " lt=" + lt1 + ", " + d2Name + " lt=" + lt2 );

            if ( lt1 == lt2 )
            {
                return 0;
            }
            else if ( lt1 < lt2 )
            {
                return 1;
            }
            else
            {
                return -1;
            }
        }

    }

}