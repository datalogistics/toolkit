/* 
 *      //\\
 *    ////\\\\    Project Bayanihan
 *   o |.[].|o 
 *  -->|....|->-  Worldwide Volunteer Computing Using Java
 *  o o.o.o.o\<\  
 * -->->->->->-   Copyright 1999, Luis F. G. Sarmenta.
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
 * 20050127.1827: Moved it to com.bayanihancomputing.util.stat package
 * and renamed to DoubleStat
 */
// package bayanihan.api.mc;
package com.bayanihancomputing.util.stat;

/**
 * A class for collecting statistics on a list of numbers.
 */
public class DoubleStat implements java.io.Serializable
{
    // note: these should be private, but keep them protected
    // for now to make them compatible with HORB, just in case

    protected long n;
    protected double total;
    protected double mean;
    protected double totalSq;
    protected double totalVar;
    protected double min;
    protected double max;

    //////////////////
    // Constructors //
    //////////////////

    /**
     */
    public DoubleStat()
    {
        this.reset();
    }

    //////////////////////
    // Accessor methods //
    //////////////////////
    public long getN()
    {
        return this.n;
    }

    public double getTotal()
    {
        return this.total;
    }

    protected double getTotalSq()
    {
        return this.totalSq;
    }

    public double getMean()
    {
        return this.mean;
    }

    protected double getTotalVar()
    {
        return this.totalVar;
    }

    public double getSampleVar()
    {
        return this.totalVar / (this.n - 1);
    }

    /*
     * Variance of the mean <em>estimator</em>.
     * This is equal to getVar() / getN().
     */
    public double getMeanVar()
    {
        return (this.getSampleVar() / this.getN());
    }

    public double getSampleStdDev()
    {
        return Math.sqrt( this.getSampleVar() );
    }

    /*
     * Stddev of the mean <em>estimator</em> (not the
     * same as the mean of the stddev).
     * Specifically, this is Math.sqrt( getMeanVar() ),
     * not getStdDev() / n.
     */
    public double getMeanStdDev()
    {
        return Math.sqrt( this.getMeanVar() );
    }

    public double getMin()
    {
        return this.min;
    }

    public double getMax()
    {
        return this.max;
    }

    //////////////////////////////////////
    // Interface implementation methods //
    //////////////////////////////////////

    public void reset()
    {
        this.n = 0;
        this.total = 0;
        this.mean = 0;
        this.totalSq = 0;
        this.totalVar = 0;
        this.min = Double.POSITIVE_INFINITY;
        this.max = Double.NEGATIVE_INFINITY;
    }

    // for adding data numbers directly

    /**
     */
    public void addSample( double d )
    {
        n++;
        this.total += d;
        this.totalSq += d * d;

        this.mean = this.computeMean();
        this.totalVar = this.computeTotalVar();

        if ( d < this.min )
        {
            this.min = d;
        }

        if ( d > this.max )
        {
            this.max = d;
        }
    }

    /**
     */
    public double computeTotalVar()
    {
        return this.totalSq - (this.total * this.total) / this.n;
    }

    // for merging DStats

    /**
     * Meant to be used separately from addSample.
     * That is, a single DStat instance should be used either
     * solely for samples, or for DStat's, not both.
     * Although theoretically, we might be able to mix them
     * if we do all the addSamples and computeVarSamples
     * before adding the DStats.
     */
    public void addDStat( DoubleStat dStat )
    {
        this.total += dStat.getTotal();
        n += dStat.getN();

        this.mean = this.computeMean();

        this.totalSq += dStat.getTotalSq();
        this.totalVar = this.computeTotalVar();

        if ( dStat.getMin() < this.min )
        {
            this.min = dStat.getMin();
        }

        if ( dStat.getMax() > this.max )
        {
            this.max = dStat.getMax();
        }
    }

    //////////////////////
    // Internal methods //
    //////////////////////

    protected double computeMean()
    {
        return this.total / this.n;
    }

    ///////////////////////
    // toString() method //
    ///////////////////////

    /**
     * Returns CSV string with Mean, SampleStdDev, N, MeanStdDev,
     * Min, and Max
     */
    public String toString()
    {
        return "" + this.getMean() + "," + this.getSampleStdDev() + ","
            + this.getN() + "," + this.getMeanStdDev() + "," + this.getTotal()
            + "," + this.getMin() + "," + this.getMax();
    }

    public static String getCSVHeaders()
    {
        return "Mean,StdDev,N,StDofMean,Total,Min,Max";
    }

    ////////////////////////
    // test main() method //
    ////////////////////////

    /**
     * This test demonstrates how it is possible to combine DStats.
     * Note the following:
     * <ul>
     * <li> The results for the single run are the same for the
     *      DStat of DStats. 
     *      </li>
     * 
     * <li> The meanStDev of each sub-DStat is roughly the same
     *      as the SampleStDev of the DStat of Means.  This shows
     *      that the meanStDev successfully estimates the stdev of the means.
     *      </li>
     * 
     * <li> The meanStDev of the DStat of Means is roughly the 
     *      same as the meanStDev of the DStat of DStats, which
     *      makes sense since they are both depicting the estimated
     *      stdev of the overall mean.
     *      </li>
     * </ul>
     */
    /*
     public static void main( String[] args )
     {
     int N = 100000;
     int P = 100;

     RandomSeedGenerator rsg = new CERNSeedGenerator();

     long seedInit = 0;

     if ( args.length > 0 )
     {
     N = Util.parseInt( args[0], N );
     }

     if ( args.length > 1 )
     {
     P = Util.parseInt( args[1], P );
     }

     if ( args.length > 2 )
     {
     seedInit = Util.parseLong( args[2], seedInit );
     }

     System.out.println( "seedInit = 0x" + Long.toHexString( seedInit ) );
     rsg.init( seedInit );

     long seed = rsg.nextSeed();
     System.out.println( "seed = 0x" + Long.toHexString( seed ) );

     RanecuRNG rng = new RanecuRNG( seed );

     int[] a = new int[N];

     for ( int i = 0; i < N; i++ )
     {
     a[i] = rng.nextInt( 10000 );
     }

     DStat aStat = new DStat();

     for ( int i = 0; i < N; i++ )
     {
     aStat.addSample( a[i] );
     // System.out.println( "adding " + a[i] + " -> " + aStat );
     }
     System.out.println( "DStat: " + aStat.fullStatString() );

     DStat[] a10Stat = new DStat[P];
     for ( int i = 0; i < P; i++ )
     {
     a10Stat[i] = new DStat();
     }

     for ( int i = 0; i < N; i++ )
     {
     a10Stat[i % P].addSample( a[i] );
     }

     DStat aSuperStat = new DStat();
     DStat aMeanStat = new DStat();

     for ( int i = 0; i < P; i++ )
     {
     System.out.println( "adding " + a10Stat[i].fullStatString() );
     aSuperStat.addDStat( a10Stat[i] );
     aMeanStat.addSample( a10Stat[i].getMean() );
     }

     System.out.println( "DStat of DStats: " + aSuperStat.fullStatString() );

     System.out.println( "DStat of Means: " + aMeanStat.fullStatString() );
     }
     */
}

