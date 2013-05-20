/*
 * Created on Jan 31, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package edu.utk.cs.loci.transfer;

import java.util.*;

import edu.utk.cs.loci.ibp.Depot;

/**
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class Scoreboard
{
    protected HashMap depotScores = new HashMap();
    protected HashMap depotStats = new HashMap();
    protected HashMap hostStats = new HashMap();

    /**
     * @return Returns the depotStats.
     */
    public HashMap getDepotStats()
    {
        return this.depotStats;
    }

    /**
     * @return Returns the hostStats.
     */
    public HashMap getHostStats()
    {
        return this.hostStats;
    }

    // protected HashMap subnetStat = new HashMap();

    protected TransferStat createDepotStat( Depot depot )
    {
        TransferStat depotStat = new TransferStat(
            depot.getHost(), depot.getPort(), null );
        return depotStat;
    }

    protected TransferStat createHostStat( Depot depot )
    {
        TransferStat hostStat = new TransferStat( depot.getHost(), 0, null );
        return hostStat;
    }

    protected Object getDepotKey( Depot depot )
    {
        return depot.getHost() + ":" + depot.getPort();
    }

    protected Object getHostKey( Depot depot )
    {
        return depot.getHost();
    }

    protected TransferStat findDepotStat( Depot depot )
    {
        return (TransferStat) depotStats.get( this.getDepotKey( depot ) );
    }

    protected TransferStat findHostStat( Depot depot )
    {
        return (TransferStat) depotStats.get( this.getHostKey( depot ) );
    }

    protected DepotScore addNewDepot( Depot depot )
    {
        TransferStat depotStat = this.createDepotStat( depot );
        Object depotKey = this.getDepotKey( depot );

        synchronized ( depotStats )
        {
            // need to synchronized in case gui is iterating over the stats
            // (i.e., see TransferStatListDispay.updateStats)
            // Note that updateStats is synchronized on the statsMap that it is displaying
            this.depotStats.put( depotKey, depotStat );
        }

        TransferStat hostStat = this.findHostStat( depot );

        if ( hostStat == null )
        {
            synchronized ( hostStats )
            {
                hostStat = this.createHostStat( depot );
                this.hostStats.put( this.getHostKey( depot ), depotStat );
            }
        }

        TransferStat subnetStat = null;

        DepotScore depotScore = new DepotScore(
            depot, depotStat, hostStat, subnetStat );
        this.depotScores.put( depotKey, depotScore );

        return depotScore;
    }

    public DepotScore findDepotScore( Depot depot )
    {
        DepotScore depotScore = (DepotScore) this.depotScores.get( this.getDepotKey( depot ) );
        if ( depotScore == null )
        {
            depotScore = this.addNewDepot( depot );
        }
        return depotScore;
    }

    public synchronized Collection getDepotScores()
    {
        return this.depotScores.values();
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
    public synchronized void startTry( Depot depot, double size )
    {
        DepotScore depotScore = this.findDepotScore( depot );
        depotScore.startTry( size );
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
    public synchronized void endSuccessfulTry( Depot depot, double size,
        long elapsedTime )
    {
        DepotScore depotScore = this.findDepotScore( depot );
        depotScore.endSuccessfulTry( size, elapsedTime );
    }

    /**
     * Call this after a failed try.
     */
    public synchronized void endFailedTry( Depot depot )
    {
        DepotScore depotScore = this.findDepotScore( depot );
        depotScore.endFailedTry();
    }
}