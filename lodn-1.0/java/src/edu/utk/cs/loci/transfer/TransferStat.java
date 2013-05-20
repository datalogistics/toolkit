/*
 * Created on Jan 28, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package edu.utk.cs.loci.transfer;

import java.net.InetAddress;

import com.bayanihancomputing.util.stat.DoubleStat;
import com.bayanihancomputing.util.time.DoubleSpeed;
import com.bayanihancomputing.util.time.DoubleSpeedMeter;

/**
 * Holds statistics for data transfers pertaining to a particular
 * depot, host, or subnet.
 * 
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class TransferStat
{
    protected String hostname;
    protected int port;
    protected InetAddress inetAddress;
    protected int threadCount;
    protected DoubleStat blockTransferStat = new DoubleStat();
    protected DoubleSpeedMeter wallClockSpeed = new DoubleSpeedMeter();

    protected DoubleSpeed lastBlockTransferSpeed = new DoubleSpeed();
    protected int successCount;
    protected int failCount;
    protected int triesCount;

    protected long lastFailTime;
    protected long lastSuccessTime;
    protected long lastTryTime;
    protected double curTrySize;

    /**
     * 
     */
    public TransferStat( String hostname, int port, InetAddress inetAddress )
    {
        this.hostname = hostname;
        this.port = port;
        this.inetAddress = inetAddress;
    }

    /**
     * @return Returns the blockTransferStat.
     */
    public synchronized DoubleStat getBlockTransferStat()
    {
        return this.blockTransferStat;
    }

    /**
     * @return Returns the failCount.
     */
    public synchronized int getFailCount()
    {
        return this.failCount;
    }

    /**
     * @return Returns the hostname.
     */
    public String getHostname()
    {
        return this.hostname;
    }

    /**
     * @return Returns the inetAddress.
     */
    public InetAddress getInetAddress()
    {
        return this.inetAddress;
    }

    /**
     * @return Returns the lastBlockTransferSpeed.
     */
    public synchronized DoubleSpeed getLastBlockTransferSpeed()
    {
        return this.lastBlockTransferSpeed;
    }

    /**
     * @return Returns the lastFailTime.
     */
    public synchronized long getLastFailTime()
    {
        return this.lastFailTime;
    }

    /**
     * @return Returns the lastSuccessTime.
     */
    public synchronized long getLastSuccessTime()
    {
        return this.lastSuccessTime;
    }

    /**
     * Returns true is the last failed time
     * is greater than the last success time.
     * If it is equal, returns false, in order
     * to be conservative, and to cover cases
     * when there are no finished times yet.
     * @return
     */
    public synchronized boolean isLastTimeFailed()
    {
        return (this.getLastFailTime() > this.getLastSuccessTime());
    }

    /**
     * @return Returns the lastTryTime.
     */
    public synchronized long getLastTryTime()
    {
        return this.lastTryTime;
    }

    /**
     * @return Returns the port.
     */
    public int getPort()
    {
        return this.port;
    }

    /**
     * @return Returns the successCount.
     */
    public synchronized int getSuccessCount()
    {
        return this.successCount;
    }

    /**
     * @return Returns the threadCount.
     */
    public synchronized int getThreadCount()
    {
        return this.threadCount;
    }

    /**
     * @return Returns the triesCount.
     */
    public synchronized int getTriesCount()
    {
        return this.triesCount;
    }
    
    public synchronized double getFailureRate()
    {
        return (double) this.getFailCount() / (double) this.getTriesCount();
    }

    /**
     * @return Returns the wallClockSpeed.
     */
    public synchronized DoubleSpeedMeter getWallClockSpeed()
    {
        return this.wallClockSpeed;
    }

    /**
     * Resets all the variables except hostname, port, and inetAddress
     */
    public synchronized void reset()
    {
        this.threadCount = 0;
        this.blockTransferStat = new DoubleStat();
        this.wallClockSpeed = new DoubleSpeedMeter();

        this.lastBlockTransferSpeed = new DoubleSpeed();
        this.successCount = 0;
        this.failCount = 0;
        this.triesCount = 0;

        this.lastFailTime = 0;
        this.lastSuccessTime = 0;
        this.lastTryTime = 0;
    }

    /**
     * Call this just before starting an attempt to do a transfer.
     * Note: we use a double here because the statistics are recorded
     * using doubles.  For small integer values of size, it should be OK.
     * However, do NOT depend on TransferStat to keep an EXACT integer
     * record of the size.
     *   
     * @param size
     */
    public synchronized void startTry( double size )
    {
        this.triesCount++;
        this.threadCount++;
        this.lastTryTime = System.currentTimeMillis();
        this.curTrySize = size;

        // NOTE: Stopwatch.start() doesn't change anything if the stopwatch
        // is already running.  If it is paused, then wallclock
        // starts again.
        this.wallClockSpeed.start();
    }

    /**
     * Call this after a successful transfer.
     * Note: we use a double here because the statistics are recorded
     * using doubles.  For small integer values of size, it should be OK.
     * However, do NOT depend on TransferStat to keep an EXACT integer
     * record of the size.
     *   
     * @param size
     * @param elapsedTime
     */
    public synchronized void endSuccessfulTry( double size, long elapsedTime )
    {
        this.lastSuccessTime = System.currentTimeMillis();
        this.lastBlockTransferSpeed.set( size, elapsedTime );
        this.blockTransferStat.addSample( size / elapsedTime );
        this.wallClockSpeed.markIncProgress( size );
        this.successCount++;
        this.threadCount--;

        // NOTE: Pause wall clock if there are no transfers going on.
        // Wallclock should only count active time.
        if ( this.threadCount == 0 )
        {
            this.wallClockSpeed.pause();
        }
    }

    /**
     * Call this after a failed try.
     */
    public synchronized void endFailedTry()
    {
        this.lastFailTime = System.currentTimeMillis();
        this.failCount++;
        this.threadCount--;

        // NOTE: Pause wall clock if there are no transfers going on.
        // Wallclock should only count active time.
        if ( this.threadCount == 0 )
        {
            this.wallClockSpeed.pause();
        }
    }
}