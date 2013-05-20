/*
 * Created on Jan 31, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package edu.utk.cs.loci.transfer;

import com.bayanihancomputing.util.stat.DoubleStat;
import com.bayanihancomputing.util.time.DoubleSpeed;
import com.bayanihancomputing.util.time.DoubleSpeedMeter;

import edu.utk.cs.loci.ibp.Depot;

/**
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class DepotScore
{
    protected Depot depot;
    protected TransferStat depotStat;
    protected TransferStat hostStat;
    protected TransferStat subnetStat;

    /**
     * @param depot
     * @param depotStat
     * @param hostStat
     * @param subnetStat
     */
    public DepotScore( Depot depot, TransferStat depotStat,
        TransferStat hostStat, TransferStat subnetStat )
    {
        super();
        this.depot = depot;
        this.depotStat = depotStat;
        this.hostStat = hostStat;
        this.subnetStat = subnetStat;
    }

    /**
     * @return Returns the depot.
     */
    public Depot getDepot()
    {
        return this.depot;
    }

    /**
     * @return Returns the depotStat.
     */
    public TransferStat getDepotStat()
    {
        return this.depotStat;
    }

    /**
     * @return Returns the hostStat.
     */
    public TransferStat getHostStat()
    {
        return this.hostStat;
    }

    /**
     * @return Returns the subnetStat.
     */
    public TransferStat getSubnetStat()
    {
        return this.subnetStat;
    }

    /**
     * Resets all the variables except hostname, port, and inetAddress
     */
    public synchronized void reset()
    {
        if ( depotStat != null )
        {
            this.depotStat.reset();
        }
        if ( hostStat != null )
        {
            this.hostStat.reset();
        }
        if ( subnetStat != null )
        {
            this.subnetStat.reset();
        }
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
        if ( depotStat != null )
        {
            this.depotStat.startTry( size );
        }
        if ( hostStat != null )
        {
            this.hostStat.startTry( size );
        }
        if ( subnetStat != null )
        {
            this.subnetStat.startTry( size );
        }
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
        if ( depotStat != null )
        {
            this.depotStat.endSuccessfulTry( size, elapsedTime );
        }
        if ( hostStat != null )
        {
            this.hostStat.endSuccessfulTry( size, elapsedTime );
        }
        if ( subnetStat != null )
        {
            this.subnetStat.endSuccessfulTry( size, elapsedTime );
        }
    }

    /**
     * Call this after a failed try.
     */
    public synchronized void endFailedTry()
    {
        if ( depotStat != null )
        {
            this.depotStat.endFailedTry();
        }
        if ( hostStat != null )
        {
            this.hostStat.endFailedTry();
        }
        if ( subnetStat != null )
        {
            this.subnetStat.endFailedTry();
        }
    }

}