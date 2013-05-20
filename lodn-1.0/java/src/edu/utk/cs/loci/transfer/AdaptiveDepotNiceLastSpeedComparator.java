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
public class AdaptiveDepotNiceLastSpeedComparator implements Comparator
{
    public static Log DEBUG = new Log( true );

    public static final AdaptiveDepotNiceLastSpeedComparator singleton = new AdaptiveDepotNiceLastSpeedComparator(
        2 );

    private int maxCount = 8;

    public AdaptiveDepotNiceLastSpeedComparator( int maxCount )
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

    public boolean ignoreFailure( TransferStat ts )
    {
        if ( ts.getSuccessCount() == 0 )
        {
            // a depot's failure cannot be ignored if it has never
            // ever succeeded yet
            return false;
        }

        // probability that we will "ignore" this node's failure
        // (i.e., that we give it another chance)
        // grows with (1 - failure rate), but can be higher than 50%
        // So if failure rate is high, then the threshold is lowered.
        double threshold = 0.5 * (1 - ts.getFailureRate());

        // return true with the probability computed above
        return (Math.random() <= threshold);
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

        if ( (ts1.isLastTimeFailed() && !ignoreFailure( ts1 ))
            && !ts2.isLastTimeFailed() )
        {
            DEBUG.println( d1Name + " failed last time, while " + d2Name
                + " did not." );
            return -1;
        }
        else if ( !ts1.isLastTimeFailed()
            && (ts2.isLastTimeFailed() && !ignoreFailure( ts2 )) )
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

        // (or ... one of them failed, but was given a chance)

        int tc1 = ts1.getThreadCount();
        int tc2 = ts2.getThreadCount();

        DEBUG.println( d1Name + " tc=" + tc1 + ", " + d2Name + " tc=" + tc2 );

        double lSpeed1 = ts1.getLastBlockTransferSpeed().getSpeed();
        double lSpeed2 = ts2.getLastBlockTransferSpeed().getSpeed();

        DEBUG.println( d1Name + " ls=" + lSpeed1 + ", " + d2Name + " ls="
            + lSpeed2 );

        double wSpeed1 = ts1.getWallClockSpeed().getSpeedDoubleValue();
        double wSpeed2 = ts2.getWallClockSpeed().getSpeedDoubleValue();

        DEBUG.println( d1Name + " ws=" + wSpeed1 + ", " + d2Name + " ws="
            + wSpeed2 );

        // this doesn't work right
        //        int maxCount1 = (int) Math.round( tc1 * ( (lSpeed1 * tc1) / wSpeed1 ) );
        //        int maxCount2 = (int) Math.round( tc2 * ( (lSpeed2 * tc2) / wSpeed2 ) );

        // I don't think this works right either because this increases the limit when lSpeed goes DOWN
        // It should be the other way.  When lSpeed goes up, it should increase the limit
        //        int maxCount1 = (int) Math.ceil( (wSpeed1 / lSpeed1) + 0.5 );
        //        int maxCount2 = (int) Math.ceil( (wSpeed2 / lSpeed2) + 0.5 );

        // How about do a min of the two?
        // So there is a pull in both directions. 
        //        int maxCount1a = (int) Math.round( tc1 * ((lSpeed1 * tc1) / wSpeed1) );
        //        int maxCount2a = (int) Math.round( tc2 * ((lSpeed2 * tc2) / wSpeed2) );
        //
        //        int maxCount1b = (int) Math.ceil( (wSpeed1 / lSpeed1) + 0.5 );
        //        int maxCount2b = (int) Math.ceil( (wSpeed2 / lSpeed2) + 0.5 );
        //
        //        int maxCount1 = Math.min( maxCount1a, maxCount1b );
        //        int maxCount2 = Math.min( maxCount2a, maxCount2b );

        // No, I don't know if the above works either ...

        // recompute PREDICTED speed if we add another thread 

        // Not clear this is appropriate either.
        // The idea here is that adding a thread will decrease the speed,
        // but presumably the higher tc is, the SMALLER the decrease
        // because presumably the current speed already allows for the fact
        // that there are multiple threads.  Thus, we should see a sharper
        // decrease between 1 and 2, than between 2 and 3.
        
        // Not sure this actually happens in practice though.
        // Also, note that at this point, since the speed is update
        // AFTER the last success, then the speed is actually the speed
        // at tc + 1 (not tc).  So arguably, it is already predictive
        // of what the speed with the additional thread will be.

        // NOTE: I tried just using last Speed without a thread limit,
        // and what happens is that the fast ones get a lot of threads
        // -- to the point that they start failing.
        // I am reimplementing the scheme below since it seemed to work
        // the last time I tried it.
        lSpeed1 = lSpeed1 * (double) (tc1 + 1) / (double) (tc1 + 2);
        lSpeed2 = lSpeed2 * (double) (tc1 + 1) / (double) (tc2 + 2);

        DEBUG.println( d1Name + " ps=" + lSpeed1 + ", " + d2Name + " ps="
            + lSpeed2 );

        int maxCount1 = this.maxCount;
        int maxCount2 = this.maxCount;

        //        DEBUG.println( "maxCount: " + d1Name + "=" + maxCount1 + ", " + d2Name
        //            + "=" + maxCount2 );

        // Note: we use >=, not = because the actual number of threads
        // that will be placed on this depot is the current count plus 1
        boolean overMaxThreads1 = ((maxCount1 > 1) && (tc1 >= maxCount1))
            || (tc1 >= this.maxCount);
        boolean overMaxThreads2 = ((maxCount2 > 1) && (tc2 >= maxCount2))
            || (tc1 >= this.maxCount);

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