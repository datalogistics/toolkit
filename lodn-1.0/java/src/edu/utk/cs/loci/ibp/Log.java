/*
 * Created on Feb 8, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package edu.utk.cs.loci.ibp;

/**
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class Log
{
    public static final boolean OVERRIDE_ON = false;
    public static final boolean OVERRIDE_OFF = false;
    private boolean on = true;

    public Log( boolean on )
    {
        if ( OVERRIDE_ON )
        {
            this.on = true;
        }
        else if ( OVERRIDE_OFF )
        {
            this.on = false;
        }
        else
        {
            this.on = on;
        }
    }
    
    

    /**
     * @return Returns the on.
     */
    public boolean isOn()
    {
        return this.on;
    }
    
    /**
     * @param on The on to set.
     */
    public void setOn( boolean on )
    {
        this.on = on;
    }
    
    public void print( String s )
    {
        if ( on )
        {
            System.out.print( s );
        }
    }

    public void println( String s )
    {
        if ( on )
        {
            System.out.println( s );
        }
    }

    public void error( String s )
    {
        if ( on )
        {
            System.out.println( "ERROR:  " + s );
            System.err.println( "stderr: " + s );
        }
    }

}