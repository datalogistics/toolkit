/*
 * Created on Jan 27, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package com.bayanihancomputing.util.time;

/**
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class DoubleSpeedMeter extends Stopwatch
{
    protected double progress = 0;

    /* (non-Javadoc)
     * @see com.bayanihancomputing.util.time.Stopwatch#reset()
     */
    public void reset()
    {
        this.progress = 0;
        super.reset();
    }
    
    public void setProgress( double progress )
    {
        this.progress = progress;
    }

    public long markProgress( double progress )
    {
        this.progress = progress;
        return this.mark();
    }

    public double getProgress()
    {
        return this.progress;
    }

    /**
     * Increment the progress.
     * Warning: This is not thread-safe.  Use only with one thread.
     * If using with multiple threads, make sure you synchronize
     * properly.
     */
    public void incProgress( double dP )
    {
        this.progress += dP;
    }

    public long markIncProgress( double dP )
    {
        this.progress += dP;
        return this.mark();
    }

    /**
     * Get the speed as a double.
     * Warning: This is not thread-safe.  Use only with one thread.
     * If using with multiple threads, make sure you synchronize
     * properly.
     */
    public double getSpeedDoubleValue()
    {
        return this.progress / this.getTime();
    }

    /**
     * Returns a new DoubleSpeed object with
     * the current progress and time.
     * Warning: This is not thread-safe.  Use only with one thread.
     * If using with multiple threads, make sure you synchronize
     * properly.
     * synchronized.
     * 
     * @return
     */
    public DoubleSpeed getSpeed()
    {
        return new DoubleSpeed( this.progress, this.getTime() );
    }

    /**
     * Sets the new DoubleSpeed object with
     * the current progress and time.
     * Use this with an existing DoubleSpeed object
     * to avoid the overhead of creating a new object.
     * Warning: This is not thread-safe.  Use only with one thread.
     * If using with multiple threads, make sure you synchronize
     * properly.
     * synchronized.
     * 
     * @return
     */
    public void getSpeed( DoubleSpeed dSpeed )
    {
        dSpeed.set( this.progress, this.getTime() );
    }

    /**
     * Set the progress and get the new speed.
     * Warning: This is not thread-safe.  Use only with one thread.
     * If using with multiple threads, make sure you synchronize
     * properly.
     */
    public double setProgressGetSpeed( double curProgress )
    {
        this.progress = curProgress;
        return this.getSpeedDoubleValue();
    }

    /**
     * Increment the progress and get the new speed.
     * Warning: This is not thread-safe.  Use only with one thread.
     * If using with multiple threads, make sure you synchronize
     * properly.
     */
    public double incProgressGetSpeed( double dP )
    {
        this.progress += dP;
        return this.getSpeedDoubleValue();
    }

    public static void main( String[] args )
    {
    }
}