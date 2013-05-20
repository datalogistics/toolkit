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
public class ThreadBoundedDepotImpatientLastSpeedComparator implements
    Comparator
{
    public static Log DEBUG = new Log( true );

    public static final ThreadBoundedDepotImpatientLastSpeedComparator singleton = new ThreadBoundedDepotImpatientLastSpeedComparator(
        2 );

    private int maxCount = 2;

    public ThreadBoundedDepotImpatientLastSpeedComparator( int maxCount )
    {
        this.maxCount = maxCount;
    }

    public void setMaxCount( int maxCount )
    {
        this.maxCount = maxCount;
    }

    public int getMaxCount()
    {
        return this.maxCount;
    }

    /* 
     * 
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

        boolean overMaxThreads1 = (tc1 >= this.maxCount);
        boolean overMaxThreads2 = (tc2 >= this.maxCount);

        if ( overMaxThreads1 && !overMaxThreads2 )
        {
            DEBUG.println( d1Name + " will go over max threads, while "
                + d2Name + " will not." );
            return -1;
        }
        else if ( !overMaxThreads1 && overMaxThreads2 )
        {
            DEBUG.println( d2Name + " will go over max threads, while "
                + d1Name + " will not." );
            return 1;
        }

        // if neither or them or BOTH of them are over max threads,
        // then compare speed

        double lSpeed1 = ts1.getLastBlockTransferSpeed().getSpeed();
        double lSpeed2 = ts2.getLastBlockTransferSpeed().getSpeed();

        DEBUG.println( d1Name + " ls=" + lSpeed1 + ", " + d2Name + " ls="
            + lSpeed2 );

        // Note: at this point, we know that neither of their last times failed
        // because of the check above.  Thus, if they don't have a speed,
        // then either they have not been tried, or their first try is still pending.

        boolean noSpeed1 = ((lSpeed1 == 0) || Double.isNaN( lSpeed1 ));
        boolean noSpeed2 = ((lSpeed2 == 0) || Double.isNaN( lSpeed2 ));
        int tries1 = ts1.getTriesCount();
        int tries2 = ts2.getTriesCount();
        boolean untried1 = (tries1 == 0);
        boolean untried2 = (tries2 == 0);
        boolean pendingFirstTry1 = (noSpeed1) && (tries1 == 1);
        boolean pendingFirstTry2 = (noSpeed2) && (tries2 == 1);

        // Give priority to the one with the speed record already, IF
        // that one has at most 1 thread.  (We are assuming that it is generally
        // still ok to have 2 threads on the same depot.)

        // The rationale behind this is that once you have found a good one,
        // stick to it unless you are being forced to have 2 or more threads
        // on the same depot.

        // Note: At this point, we know that neither thread is at its max count yet
        // So it is OK *not* to give the untried depot a chance because
        // presumably the tried depot is fast enough (because it already came
        // back with a result), and is still ok (because it hasn't reached the max threads yet)

        if ( !noSpeed1 && noSpeed2 && (tc1 < 2) )
        {
            DEBUG.println( d1Name + " has speed and < 2 tc, while " + d2Name
                + " has no speed yet." );
            return 1;
        }
        else if ( noSpeed1 && !noSpeed2 && (tc2 < 2) )
        {
            DEBUG.println( d2Name + " has speed and < 2 tc, while " + d1Name
                + " has no speed yet." );
            return -1;
        }
        else if ( !noSpeed1 && !noSpeed2 )
        {
            return (lSpeed1 > lSpeed2) ? 1 : -1;
        }

        // At this point both of them have no speed

        if ( untried1 && (tc2 > 0) )
        {
            // if depot 1 is untried and depot 2 is already busy, then thread 1 has the advantage
            DEBUG.println( d1Name + " has not yet been tried, while " + d2Name
                + " is already busy." );
            return 1;
        }
        else if ( untried2 && (tc1 > 0) )
        {
            // if depot 2 is untried and depot 1 is already busy, then thread 1 has the advantage
            DEBUG.println( d2Name + " has not yet been tried, while " + d1Name
                + " is already busy." );
            return -1;
        }
        else if ( untried1 && untried2 )
        {
            // if both untried, then return equal
            return 0;
        }

        // else both of them have been tried at least once

        if ( !pendingFirstTry1 && pendingFirstTry2 )
        {
            // if depot 1 is not untried and not pending its first try,
            // and depot 2 is still pending its first try, then thread 1 has the advantage
            DEBUG.println( d1Name + " has been succesfully tried, while "
                + d2Name + "'s first try is still pending" );
            return 1;
        }
        else if ( !pendingFirstTry2 && pendingFirstTry1 )
        {
            // if depot 2 is not untried and not pending its first try,
            // and depot 1 is still pending its first try, then thread 2 has the advantage
            DEBUG.println( d2Name + " has been succesfully tried, while "
                + d1Name + "'s first try is still pending" );
            return -1;
        }
        else if ( pendingFirstTry1 && pendingFirstTry2 )
        {
            // if both of them have at least started trying, then compare their
            // threadcounts

            // if speeds are the same or speeds haven't been measured yet,
            // then ts1 is better if ts2 has more threads

            if ( tc2 != tc1 )
            {
                DEBUG.println( "Comparing threadcounts of " + d1Name + " and "
                    + d2Name + " ..." );
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
        // if neither one is pending their first try, then just compare speeds
        {
            return (lSpeed1 > lSpeed2) ? 1 : -1;
        }
    }

}