/* $Id: DepotList.java,v 1.4 2008/05/24 22:25:51 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.util.*;
import java.util.Random;

import edu.utk.cs.loci.ibp.Log;
import edu.utk.cs.loci.ibp.Depot;

public class DepotList
{
    public static Log DEBUG = new Log( true );

    private ArrayList depots;
    private int next;
    private Random rand = new Random();

    public DepotList()
    {
        depots = new ArrayList();
        next = 0;
    }

    public void add( Depot d )
    {
        depots.add( d );
    }

    public void add( Depot[] depots )
    {
        for ( int i = 0; i < depots.length; i++ )
        {
            add( depots[i] );
        }
    }

    public synchronized Depot next()
    {
        Depot d = (Depot) depots.get( next );

        if ( (next + 1) == length() )
        {
            next = 0;
        }
        else
        {
            next = next + 1;
        }
        return (d);
    }

    public synchronized void remove( Depot d )
    {
        try
        {
            depots.remove( depots.indexOf( d ) );
            if ( next == length() )
            {
                next = 0;
            }
        }
        catch ( ArrayIndexOutOfBoundsException e )
        {
            DEBUG.error( "DepotList.remove(d): " + e );
        }
    }

    public synchronized int length()
    {
        return (depots.size());
    }

    // Avoid to start to upload always on the same first depot returned by the LBone
    public synchronized int randomizeNext()
    {
        next = rand.nextInt( this.length() );
        return next;
    }

    public String toString()
    {
        StringBuffer response = new StringBuffer();
        int i = 1;
        for ( Iterator iter = depots.iterator(); iter.hasNext(); i++ )
        {
            response.append( i ).append( " " );
            response.append( iter.next().toString() ).append( "\n" );
        }
        return response.toString();
    }

}