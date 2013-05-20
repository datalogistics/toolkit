/* $Id: ReadJob.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.util.*;
import java.io.*;

import com.bayanihancomputing.util.time.*;

import edu.utk.cs.loci.ibp.*;
import edu.utk.cs.loci.transfer.*;

public class ReadJob
{
    public static Log DEBUG = new Log( true );

    static Scoreboard scoreboard = new Scoreboard();

    public static int READ_TIMEOUT = 20;
    public static int MAX_DEPOT_THREADS = 3;
    public static Comparator comparator
    // = new ThreadBoundedDepotImpatientLastSpeedComparator( MAX_DEPOT_THREADS );
    = new AdaptiveDepotNiceLastSpeedComparator( MAX_DEPOT_THREADS );

    private int timesRetried = 0;
    private boolean done = false;
    //private boolean exhausted = false;

    private Exnode exnode;
    private long offset;
    private int length;
    private long baseOffset;
    private byte[] buf = null;
    private byte[] outputBuffer = null;
    private ArrayList untriedMappings;
    private ArrayList triedMappings;
    private SyncFile file = null;

    private int concurrentTries = 0;
    private long lastTryTime = System.currentTimeMillis();

    public ReadJob( Exnode exnode, long offset, int length, byte[] buf,
        long baseOffset )
    {
        this.exnode = exnode;
        this.offset = offset;
        this.length = length;
        this.baseOffset = baseOffset;
        this.untriedMappings = this.getContainingMappings( true );

        this.triedMappings = new ArrayList();
        this.buf = buf;
    }

    public ReadJob( Exnode exnode, long offset, int length, SyncFile file,
        long baseOffset )
    {
        this.exnode = exnode;
        this.offset = offset;
        this.length = length;
        this.untriedMappings = this.getContainingMappings( true );

        this.triedMappings = new ArrayList();
        this.file = file;
        this.baseOffset = baseOffset;
    }

    /**
     * @return Returns the lastTryTime.
     */
    public long getLastTryTime()
    {
        return this.lastTryTime;
    }

    public Exnode getExnode()
    {
        return this.exnode;
    }

    public synchronized void resetAfterExhausted()
    {
        DEBUG.println( "Resetting exhausted ReadJob " + this + " for retry ..." );

        if ( (this.untriedMappings.size() == 0)
            && (this.triedMappings.size() > 0) )
        {
            DEBUG.println( "Using triedMappings list for mappings ..." );
            this.untriedMappings = triedMappings;
            this.triedMappings = new ArrayList();
        }
        else
        {
            DEBUG.println( "untriedmappings.size() is not 0, getting all mappings again ..." );
            this.untriedMappings = this.getContainingMappings( true );
        }
        this.timesRetried++;
        //this.exhausted = false;
        this.done = false;
    }

    public synchronized int getTimesRetried()
    {
        return this.timesRetried;
    }

    public ArrayList getContainingMappings( boolean randomized )
    {
        ArrayList mappings = exnode.getContainingMappings( offset, length );

        if ( randomized )
        {
            int size = mappings.size();

            for ( int i = 0; i < size; i++ )
            {
                int srcIndex = (int) (Math.random() * size);
                int destIndex = (int) (Math.random() * size);
                Object srcObj = mappings.get( srcIndex );
                Object destObj = mappings.get( destIndex );
                mappings.set( srcIndex, destObj );
                mappings.set( destIndex, srcObj );
            }
        }

        return mappings;
    }

    public synchronized boolean isDone()
    {
        return this.done;
    }

    public synchronized boolean isExhausted()
    {
        // return this.exhausted;
        return (this.untriedMappings.size() == 0);
    }

    public static Scoreboard getScoreboard()
    {
        return scoreboard;
    }

    public int compareDepotScores( DepotScore ds1, DepotScore ds2 )
    {
        // return DepotThreadCountComparator.singleton.compare( ds1, ds2 );
        // return DepotWallclockSpeedComparator.singleton.compare( ds1, ds2 );
        // return DepotLastSpeedComparator.singleton.compare( ds1, ds2 );
        // return DepotLenientWallSpeedComparator.singleton.compare( ds1, ds2 );
        // return DepotLenientLastSpeedComparator.singleton.compare( ds1, ds2 );
        return this.comparator.compare( ds1, ds2 );
    }

    /**
     * Finds the best mapping among untriedMappings, and then,
     * when found removes it from untriedMappings and adds it to
     * triedMappings.
     * @return
     */
    private synchronized Mapping findAndTryBestMapping()
    {
        // NOTE: synchronize on scoreboard to avoid overlapping
        // findAndTryBestMappings of separate jobs, as well
        // as overlapping scoreboard updates with comparisons here

        // TODO: Maybe later, the more appropriate thing to do
        // would be to include the findBestMapping feature
        // with the scoreboard.  (Maybe call it findBestDepot)

        synchronized ( this.scoreboard )
        {
            Mapping bestMapping = (Mapping) this.untriedMappings.get( 0 );
            Depot bestDepot = bestMapping.getAllocation().getDepot();
            DepotScore bestDepotScore = this.scoreboard.findDepotScore( bestDepot );

            for ( int i = 1; i < this.untriedMappings.size(); i++ )
            {
                Mapping m = (Mapping) this.untriedMappings.get( i );
                Depot depot = m.getAllocation().getDepot();
                DepotScore depotScore = this.scoreboard.findDepotScore( depot );
                if ( this.compareDepotScores( depotScore, bestDepotScore ) > 0 )
                {
                    bestMapping = m;
                    bestDepot = depot;
                    bestDepotScore = depotScore;
                }
            }

            TransferStat bestDepotStat = bestDepotScore.getDepotStat();
            int bestThreadCount = bestDepotStat.getThreadCount();

            // COMMENT OUT the following if you don't want
            // a limit on the number of threads
            if ( bestThreadCount >= this.MAX_DEPOT_THREADS )
            {
                DEBUG.println( "Thread " + Thread.currentThread()
                    + ": Best depot choice " + bestDepot
                    + " currently has max threads already." );
                return null;
            }
            else if ( (bestThreadCount > 0)
                && (bestDepotStat.getSuccessCount() == 0) )
            {
                DEBUG.println( "Thread "
                    + Thread.currentThread()
                    + ": Best depot choice "
                    + bestDepot
                    + " already has a pending thread, and has not proven itself yet." );
                return null;
            }
            else
            {
                // NOTE: only remove the bestMapping if we're actually going to use it.
                // If we're returning null (which will cause the ReadThread to wait),
                // then don't remove the mapping from the queue so it can be retried again later.

                this.untriedMappings.remove( bestMapping );
                this.triedMappings.add( bestMapping );

                return bestMapping;
            }
        }
    }

    protected synchronized DoubleSpeedMeter startTry( Mapping m )
    {
        DoubleSpeedMeter speedMeter = new DoubleSpeedMeter();
        Depot depot = m.getAllocation().getDepot();
        // NOTE: we are assuming here that transfer length
        // if the same as the ReadJob length
        this.scoreboard.startTry( depot, (double) this.length );
        speedMeter.start();
        return speedMeter;
    }

    protected synchronized void endSuccessfulTry( Mapping m,
        DoubleSpeedMeter speedMeter )
    {
        this.done = true;
        Depot depot = m.getAllocation().getDepot();
        // NOTE: we are assuming here that transfer length
        // if the same as the ReadJob length
        speedMeter.stop();
        speedMeter.setProgress( (double) this.length );
        this.scoreboard.endSuccessfulTry(
            depot, (double) this.length, speedMeter.getTime() );

        DEBUG.println( "SUCCESS! " + depot.getHost() + ":" + depot.getPort()
        // + ", " + speedMeter.getProgress() + " bytes / " + speedMeter.getTime() + " ms = "
            + " "
            + DoubleSpeed.to2DecPlaces( speedMeter.getSpeedDoubleValue() / 1e3 )
            + " MB/s" );

        // NOTE: There is a chance that the depotScore will be updated
        // between the println above and the displayDepotStat below.
        // (I've actually seen this happen.)
        // In fact, there is even a chance that it'll be updated
        // within displayDepotStat since the these methods are synchronized
        // on the ReadJob, not the depotScores.

        // TODO: Maybe printing/logging should be a function of the scoreboard and can thus
        // be synchronized with the scoreboard
        this.displayDepotStat( this.scoreboard.findDepotScore( depot ).getDepotStat() );
    }

    protected synchronized void endFailedTry( Mapping m,
        DoubleSpeedMeter speedMeter )
    {
        // NOTE: we don't set done to false, since we assume it's false
        // to begin with.  If it's already true, then another thread must have
        // it to true.

        Depot depot = m.getAllocation().getDepot();
        // NOTE: we are assuming here that transfer length
        // if the same as the ReadJob length
        speedMeter.stop();
        this.scoreboard.endFailedTry( depot );

        DEBUG.println( "FAILURE! " + depot.getHost() + ":" + depot.getPort() );
        this.displayDepotStat( this.scoreboard.findDepotScore( depot ).getDepotStat() );
    }

    public static synchronized void displayDepotStat( TransferStat ds )
    {
        DEBUG.println( ">>> Stats for \t" + ds.getHostname() + ":"
            + ds.getPort() + "\t<<<" );
        DEBUG.println( " Tries: (" + ds.getSuccessCount() + "/"
            + ds.getFailCount() + "/" + ds.getThreadCount() + ")= "
            + ds.getTriesCount() + " total" );
        DEBUG.println( " Last block: "
            + ds.getLastBlockTransferSpeed().getProgress()
            + " Bytes / "
            + ds.getLastBlockTransferSpeed().getTime()
            + " ms= "
            + DoubleSpeed.to2DecPlaces( ds.getLastBlockTransferSpeed().getSpeed() / 1e3 )
            + " MB/s" );
        DEBUG.println( " Block speed: "
            + DoubleSpeed.to2DecPlaces( ds.getBlockTransferStat().getMean() / 1e3 )
            + " MB/s"
            + ", sd= "
            + DoubleSpeed.to2DecPlaces( ds.getBlockTransferStat().getSampleStdDev() / 1e3 )
            + " MB/s" );
        // + ", of " + ds.getBlockTransferStat().getN() );
        DEBUG.println( " Wallclock: "
            + ds.getWallClockSpeed().getProgress()
            + " Bytes / "
            + ds.getWallClockSpeed().getTime()
            + " ms= "
            + DoubleSpeed.to2DecPlaces( ds.getWallClockSpeed().getSpeedDoubleValue() / 1e3 )
            + " MB/s" );
    }

    public synchronized int getConcurrentTries()
    {
        return this.concurrentTries;
    }

    public synchronized Mapping getLastTriedMapping()
    {
        return (Mapping) this.triedMappings.get( this.triedMappings.size() );
    }

    public synchronized DepotScore getLastTriedDepotScore()
    {
        if ( this.triedMappings.size() < 1 )
        {
            DEBUG.println( "Job " + this
                + ": last tried mapping does not exist." );
            return null;
        }
        else
        {
            Mapping m = (Mapping) this.triedMappings.get( this.triedMappings.size() - 1 );
            DEBUG.println( "Job " + this + " last tried mapping = " + m );
            Depot depot = m.getAllocation().getDepot();
            DEBUG.println( "Job " + this + " last tried depot = " + depot );
            DepotScore depotScore = this.scoreboard.findDepotScore( depot );
            return depotScore;
        }
    }

    public synchronized boolean shouldProbablyBeRetriedInParallel()
    {
        if ( this.getConcurrentTries() < 1 )
        {
            if ( this.isDone() )
            {
                DEBUG.println( "ReadJob " + this
                    + " already done. No need to retry." );
                return false;
            }
            else
            {
                DEBUG.println( "ReadJob "
                    + this
                    + " has no conc. tries but not yet done.  It should be retried." );
                return true;
            }
        }
        else
        {
            DepotScore lastDepotScore = this.getLastTriedDepotScore();
            if ( lastDepotScore != null )
            {
                TransferStat lastDepotStat = lastDepotScore.getDepotStat();
                if ( lastDepotStat != null && lastDepotStat.isLastTimeFailed() )
                {
                    DEBUG.println( "ReadJob " + this
                        + ": last depot try was failed. Job retriable." );
                    return true;
                }
            }

            // else if either the last depot try was not a failure
            // or for some reason, the last depot score or stat
            // was not set, look at elapsed time.

            long timeElapsed = System.currentTimeMillis() - this.lastTryTime;
            DEBUG.println( "ReadJob " + this + ": last depot try was "
                + timeElapsed + "ms ago" );
            if ( timeElapsed > READ_TIMEOUT * 1000 )
            {
                return true;
            }
            else
            {
                return false;
            }

        }
    }

    public void execute()
    {
        synchronized ( this )
        {
            this.concurrentTries++;
            this.lastTryTime = System.currentTimeMillis();
            DEBUG.println( "Entering Job " + this + " conc. tries= "
                + this.concurrentTries + ", time=" + this.lastTryTime );
        }

        try
        {
            String errorMsg = "";
            boolean hasNextMapping = false;
            Mapping m = null;
            String capaHostInfo = null;
            String capaPortInfo = null;

            while ( (untriedMappings.size() > 0) && !done && !exnode.cancel
                && exnode.error == null )
            {
                DEBUG.println( "Thread " + Thread.currentThread()
                    + " finding best mapping ... " );

                boolean doRead = false;

                DoubleSpeedMeter speedMeter = null;

                synchronized ( this.scoreboard )
                {
                    m = this.findAndTryBestMapping();
                    doRead = (m != null);

                    if ( doRead )
                    {
                        // need to call startTry in the same synchronized block as findAndTryBestMapping
                        // so that there are no race conditions

                        capaHostInfo = m.getAllocation().getReadCapability().getHost();
                        capaPortInfo = m.getAllocation().getReadCapability().getPort();

                        DEBUG.println( "Thread " + Thread.currentThread()
                            + " trying mapping " + m.getExnodeOffset()
                            + " from " + capaHostInfo + ":" + capaPortInfo );

                        speedMeter = this.startTry( m );
                    }

                }

                if ( !doRead ) // ( m == null )
                {
                    // TODO: If m is null not because max threads is reached,
                    // but because the best depot for this job has not proven
                    // itself yet, then don't pause.  Let the ReadThread put
                    // it in the retryQueue instead.  That way, the ReadThread
                    // can be freed up to try other Jobs while it is waiting.

                    //int delay = 1000;
                    //                    DEBUG.println( "Thread " + Thread.currentThread()
                    //                        + ": job " + this
                    //                        + " findBestMapping returned null.  Sleeping " + delay );
                    //                    try
                    //                    {
                    //                        Thread.sleep( delay );
                    //                    }
                    //                    catch ( InterruptedException ie )
                    //                    {
                    //                        DEBUG.println( "Thread "
                    //                            + Thread.currentThread()
                    //                            + " interrupted while waiting for depots to have lower thread count." );
                    //                    }
                    //
                    //                    DEBUG.println( "Thread " + Thread.currentThread()
                    //                        + ": job " + this
                    //                        + " exiting to let ReadThread put it in queue." );
                    //                    return;

                    int delay = READ_TIMEOUT * 1000 / 4;
                    DEBUG.println( "Thread " + Thread.currentThread()
                        + ": findBestMapping returned null.  Sleeping " + delay );
                    try
                    {
                        Thread.sleep( delay );
                    }
                    catch ( InterruptedException ie )
                    {
                        DEBUG.println( "Thread "
                            + Thread.currentThread()
                            + " interrupted while waiting for depots to have lower thread count." );
                    }

                    DEBUG.println( "Thread "
                        + Thread.currentThread()
                        + " done waiting.  Checking for depot thread counts again ..." );
                }
                else
                // doRead
                {
                    boolean alreadyRegisteredFailure = false;

                    try
                    {
                        //DEBUG.println("Reading exnode["+offset+","+length+"]");
                        //DEBUG.println("Reading mapping["+(offset-m.getExnodeOffset())+","+length+"]");

                        //int timeout=calcTimeout();
                        //DEBUG.println("Timeout: "+timeout);

                        DEBUG.println( "ReadJob20: length: " + length );

                        DEBUG.println( "ReadJob20: offset: " + offset );

                        DEBUG.println( "ReadJob20: m.getExnodeOffset: "
                            + m.getExnodeOffset() );

                        DEBUG.println( "Thread " + Thread.currentThread()
                            + ": Loading\t" + offset + " " + length
                            + "\tfrom\t" + capaHostInfo + ":" + capaPortInfo );

                        outputBuffer = m.read( READ_TIMEOUT, length, offset
                            - m.getExnodeOffset(), 0 ); // 1st arg. is timeout

                        int outBufferSize = outputBuffer.length;

                        //                        DEBUG.println( "Output buffer size = " + outBufferSize
                        //                            + " bytes" );

                        if ( outBufferSize > 0 )
                        { //== length) {
                            String statusMsg = "";

                            if ( buf != null )
                            {
                                // NOTE: This assumes that offset - baseOffset is
                                // not greater than Integer.MAX_INT
                                
                                System.arraycopy(
                                    outputBuffer, 0, buf,
                                    (int) (offset - baseOffset), length );

                                this.endSuccessfulTry( m, speedMeter );
                            }
                            else
                            {
                                try
                                {
                                    // NOTE: even though outBuffer may be big,
                                    // we are only expecting to read *length* logical
                                    // bytes, so use length instead of outBufferSize

                                    //file.write(outputBuffer,(int)(offset-baseOffset),length);
                                    DEBUG.println( "Exnode.read(): Writing "
                                        + length + " bytes to "
                                        + (offset - baseOffset) );

                                    // FIXME: In some cases, it seems that bytesRead
                                    // can exceed the end of the real file

                                    file.write(
                                        outputBuffer,
                                        (offset - baseOffset), length );

                                    this.endSuccessfulTry( m, speedMeter );

                                    String threadName = "Thread "
                                        + ((TransferThread) Thread.currentThread()).getId();

                                    statusMsg = threadName + ": Read " + offset
                                        + " " + length + " from "
                                        + capaHostInfo + ":" + capaPortInfo
                                        + ". DONE!";

                                    DEBUG.println( statusMsg );
                                    
                                    ((TransferThread) Thread.currentThread()).setStatus( "DONE!" );

                                    DEBUG.println( "ReadJob22: ["
                                        + (offset - baseOffset) + ", " + length
                                        + "] DONE!" );

                                }
                                catch ( IOException e )
                                {
                                    statusMsg = "Thread "
                                        + Thread.currentThread() + ": Read "
                                        + offset
                                        // + " " + length 
                                        + " from " + capaHostInfo + ":"
                                        + capaPortInfo + ".  FAILED!";

                                    DEBUG.error( "ReadJob40 (execute): " + e );

                                    this.endFailedTry( m, speedMeter );
                                    alreadyRegisteredFailure = true;
                                }
                            }

                            // FIXME: This isn't synchronized
                            exnode.updateProgress( length, statusMsg );
                        }
                    }
                    catch ( ReadException e )
                    {
                        DEBUG.println( "ReadJob41 (execute): Caught ReadException while reading bytes["
                            + offset
                            + ","
                            + length
                            + "] from "
                            + capaHostInfo
                            + ":" + capaPortInfo + " (non-critical)" );
                        if ( !alreadyRegisteredFailure )
                        {
                            this.endFailedTry( m, speedMeter );
                            alreadyRegisteredFailure = true;
                        }
                    }
                    catch ( Exception e )
                    {
                        DEBUG.error( "ReadJob42 (execute)" + e );
                        // e.printStackTrace();
                        if ( !alreadyRegisteredFailure )
                        {
                            this.endFailedTry( m, speedMeter );
                            alreadyRegisteredFailure = true;
                        }
                    }
                }
            }

            if ( !done )
            {
                if ( exnode.error == null && !exnode.cancel
                //&& !hasNextMapping )
                    && this.isExhausted() )
                {
                    // this.exhausted = true;

                    DEBUG.println( "No more mappings left for this job. Quitting it." );
                    //                    DEBUG.println( "ReadJob: all mappings for bytes ["
                    //                        + offset + "," + length + "] have failed.  "
                    //                        + "Cancelling exnode download, and setting error " );
                }
                //            DEBUG.println( "ReadJob !done error. "
                //                + Thread.currentThread()
                //                + ", hasNextMapping="
                //                + hasNextMapping + ", exnode.error=" + exnode.error
                //                + ", exnode.cancel=" + exnode.cancel );

                // NOTE: don't cancel everything yet because we might want to requeue and retry
                //            exnode.setCancel( true );
                //            exnode.setError( new ReadException( "Unable to read bytes ["
                //                + offset + "," + length + "]\n" + errorMsg ) );
            }
        }
        finally
        {
            synchronized ( this )
            {
                this.concurrentTries--;
                DEBUG.println( "Exiting Job " + this + " conc. tries= "
                    + this.concurrentTries );
            }
        }
    }

    public String toString()
    {
        // TODO: Add more info like exnode, etc.
        return "ReadJob [" + this.offset + "," + this.length + "]";
    }
}