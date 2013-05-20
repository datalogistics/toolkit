package edu.utk.cs.loci.ibp;

public class Depot
{

    // Instance variables
    private String host;
    private int port;

    // Constructors
    public Depot( String host, int port )
    {
        this.host = host;
        this.port = port;
    }

    public Depot( String host, String port )
    {
        this.host = host;
        this.port = Integer.parseInt( port );
    }

    // Instance methods

    public String getHost()
    {
        return (host);
    }

    public int getPort()
    {
        return (port);
    }

    public boolean equals( Object o )
    {
        if ( !(o instanceof Depot) )
        {
            return (false);
        }

        Depot depot = (Depot) o;

        if ( this.host.compareTo( depot.host ) == 0 && this.port == depot.port )
        {
            return (true);
        }
        else
        {
            return (false);
        }
    }

    public String toString()
    {
        return (host + ":" + port);
    }

    void checkSize( int size ) throws IllegalArgumentException
    {
        if ( size <= 0 )
        {
            throw (new IllegalArgumentException());
        }
    }

    void checkDuration( int duration ) throws IllegalArgumentException
    {
        if ( duration <= 0 )
        {
            throw (new IllegalArgumentException());
        }
    }

    void checkReliability( int reliability ) throws IllegalArgumentException
    {
        if ( reliability < 1 || reliability > 2 )
        {
            throw (new IllegalArgumentException());
        }
    }

    void checkType( int type ) throws IllegalArgumentException
    {
        if ( type < 1 || type > 4 )
        {
            throw (new IllegalArgumentException());
        }
    }

    void checkTimeout( int timeout ) throws IllegalArgumentException
    {
        if ( timeout < -1 )
        {
            throw (new IllegalArgumentException());
        }
    }

    public Allocation allocateSoftByteArray( int timeout, int duration,
        long size ) throws IBPException
    {
        return (ProtocolService.allocateSoftByteArray(
            this, timeout, duration, size ));
    }

    public Allocation allocateHardByteArray( int timeout, int duration,
        long size ) throws IBPException
    {
        return (ProtocolService.allocateHardByteArray(
            this, timeout, duration, size ));
    }

    public Allocation allocateSoftBuffer( int timeout, int duration, long size )
        throws IBPException
    {
        return (ProtocolService.allocateSoftBuffer(
            this, timeout, duration, size ));
    }

    public Allocation allocateHardBuffer( int timeout, int duration, long size )
        throws IBPException
    {
        return (ProtocolService.allocateHardBuffer(
            this, timeout, duration, size ));
    }

    public Allocation allocateSoftFifo( int timeout, int duration, long size )
        throws IBPException
    {
        return (ProtocolService.allocateSoftFifo( this, timeout, duration, size ));
    }

    public Allocation allocateHardFifo( int timeout, int duration, long size )
        throws IBPException
    {
        return (ProtocolService.allocateHardFifo( this, timeout, duration, size ));
    }

    public Allocation allocateSoftCirq( int timeout, int duration, long size )
        throws IBPException
    {
        return (ProtocolService.allocateSoftCirq( this, timeout, duration, size ));
    }

    public Allocation allocateHardCirq( int timeout, int duration, long size )
        throws IBPException
    {
        return (ProtocolService.allocateHardCirq( this, timeout, duration, size ));
    }
}