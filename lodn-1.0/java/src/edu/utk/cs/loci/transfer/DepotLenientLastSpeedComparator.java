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
public class DepotLenientLastSpeedComparator implements Comparator
{
    public static Log DEBUG = new Log( true );

    public static final DepotLenientLastSpeedComparator singleton = new DepotLenientLastSpeedComparator();

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

        double lSpeed1 = ts1.getLastBlockTransferSpeed().getSpeed();
        double lSpeed2 = ts2.getLastBlockTransferSpeed().getSpeed();

        DEBUG.println( d1Name + " ls=" + lSpeed1 + ", " + d2Name + " ls="
            + lSpeed2 );

        // This is the "wall2" variant.
        // It did not work well, though.  For some reason, 
        // the bitrate is fast at the beginning, but then it slows down during the
        // middle.  I suspect that new servers that are slow are hogging
        // down the finite number of threads, and thus preventing the faster
        // servers from getting multiple threads.  Wall1 is still the best.

        if ( (lSpeed1 == lSpeed2)
            || ((lSpeed1 == 0) || Double.isNaN( lSpeed1 )
                && !ts1.isLastTimeFailed())
            || ((lSpeed2 == 0) || Double.isNaN( lSpeed2 )
                && !ts2.isLastTimeFailed()) )
        {

            // if one of them is new, then give that one a chance

            int tries1 = ts1.getTriesCount();
            int tries2 = ts2.getTriesCount();

            if ( tries1 == 0 && tries2 > 0 )
            {
                return 1;
            }
            else if ( tries2 == 0 && tries1 > 0 )
            {
                return -1;
            }

            // if both of them have at least started trying, then compare their
            // threadcounts

            // if speeds are the same or speeds haven't been measured yet,
            // then ts1 is better if ts2 has more threads

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

                DEBUG.println( d1Name + " lt=" + lt1 + ", " + d2Name + " lt="
                    + lt2 );

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
        else
        {
            return (lSpeed1 > lSpeed2) ? 1 : -1;
        }
    }

}