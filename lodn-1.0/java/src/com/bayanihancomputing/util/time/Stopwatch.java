/* 
 *      //\\
 *    ////\\\\	  Project Bayanihan
 *   o |.[].|o 
 *  -->|....|->-  Worldwide Volunteer Computing Using Java
 *  o o.o.o.o\<\  
 * -->->->->->-   Copyright 1999-2000, Luis F. G. Sarmenta.
 *   <\<\<\<\<\   All rights reserved.
 * 
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for NON-COMMERCIAL purposes and without
 * fee is hereby granted provided that this copyright notice
 * and disclaimer appear in all copies.
 *
 * LUIS F. G. SARMENTA MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE
 * SUITABILITY OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT. LUIS F. G. SARMENTA
 * SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT
 * OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
 * ----------------------------------------------------------------------------|
 *
 * Timer.java
 *
 * by Luis F. G. Sarmenta
 * started: 961107.1159
 * v0.3: 9707012.0225
 *
 * Modified from 970107.1455 version: removed ReadWriteable
 * 20000424.1007: made it implement bayanihan.Serializable
 * 20000610.1509: added pause functionality
 * 
 * 20050127.1804: moved to com.bayanihancomputing and renamed from Timer 
 * to Stopwatch to avoid confusion with Timer classes in standard packages 
 * removed bayanihan.Serializable
 */
package com.bayanihancomputing.util.time;

public class Stopwatch implements java.io.Serializable // implements bayanihan.Serializable
{
    protected boolean running = false;
    protected long starttime = 0;
    protected long stoptime = 0;
    protected long basetime = 0; // used for pausing

    /**
     * Returns true if the Timer has been started,
     * and is not currently stopped or paused.
     */
    public boolean isRunning()
    {
        return running;
    }

    /**
     * Starts a stopped Timer, or restarts a paused Timer
     * (sets starttime).
     */
    public void start()
    {
        if ( !running )
        {
            starttime = System.currentTimeMillis();
            running = true;
        }
    }

    /**
     * Stops a running timer sets stoptime.
     */
    public void stop()
    {
        if ( running )
        {
            stoptime = System.currentTimeMillis();
            running = false;
        }
    }

    /**
     * Stops timer, and remembers running time so far;
     * to restart, call start; getTime() will
     * return time since last reset(), not including
     * paused periods.
     */
    public void pause()
    {
        if ( running )
        {
            this.stop();
            basetime = this.getTime();
            // NOTE: Need to move starttime again so 
            // if you call getTime() on a paused Stopwatch
            // before starting it again, it
            // doesn't double count basetime
            this.starttime = this.stoptime;
        }
    }

    /**
     * Returns the current running time (i.e., getTime())
     * without stopping the timer, if it's running; 
     * also sets stoptime if the timer is running (but not
     * if it is not).
     */
    public long mark()
    {
        if ( running )
        {
            stoptime = System.currentTimeMillis();
        }

        return this.getTime();
    }

    /**
     * Stops and resets the basetime stored from previous pauses, 
     * so that the next call to start() would start the timer from 0 time.
     */
    public void reset()
    {
        running = false;
        starttime = 0;
        stoptime = 0;
        basetime = 0;
    }

    /**
     * Returns time from start time to stop time, not including pauses.
     */
    public long getTime()
    {
        // If it's running, do NOT use stoptime to read the time

        long curTime = running ? System.currentTimeMillis() : stoptime;
        return (curTime - starttime) + basetime;
    }

    /**
     * Returns the System time when this Timer
     * was last started.
     */
    public long getAbsStart()
    {
        return starttime;
    }

    /**
     * Returns the System time when this Timer was
     * last stopped, marked, or paused.
     */
    public long getAbsStop()
    {
        return stoptime;
    }

    /**
     * Returns total time on Timer before the last pause.
     * This is added to the time different of the
     * next start-stop/mark pair to get the total running time.
     */
    public long getBaseTime()
    {
        return basetime;
    }

    /**
     * Returns the current System time.
     */
    public static long getAbsCurTime()
    {
        return (System.currentTimeMillis());
    }

    // For testing //

    public static void main( String args[] )
    {
        Stopwatch t = new Stopwatch();

        System.out.println( "Testing Timer class ... " );

        t.start();
        System.out.println( "Timer started: " + t.getAbsStart() + " ms" );

        for ( int i = 0; i < 10; i++ )
        {
            Stopwatch t2 = new Stopwatch();

            t.start(); // This should NOT have an effect on the time.
            t2.start();
            System.out.println( "Timer 2 started." );
            for ( int j = 0; j < 100000; j++ )
                t.mark();
            System.out.println( "Timer 1 at: " + t.mark() + " ms" );
            t2.stop();
            System.out.println( "Timer 2 at: " + t2.getTime() + " ms" );
            t2.reset();
            System.out.println( "Timer 2 reset." );
        }
        t.stop();
        System.out.println( "Timer stopped: " + t.getAbsStop() + " ms" );
        System.out.println( "Total Running Time: " + t.getTime() + " ms" );
    }
}

