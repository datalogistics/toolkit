package edu.utk.cs.loci.ibp;

public class Allocation
{

    // Class variables
    public static final int SOFT = 1;
    public static final int HARD = 2;

    public static final int BYTEARRAY = 1;
    public static final int BUFFER = 2;
    public static final int FIFO = 3;
    public static final int CIRQ = 4;

    // Instance variables
    Capability readCapability = null;
    Capability writeCapability = null;
    Capability manageCapability = null;

    // Constructors
    public Allocation( Capability read, Capability write, Capability manage )
        throws HostMismatchException, PortMismatchException
    {
        setReadCapability( read );
        setWriteCapability( write );
        setManageCapability( manage );
    }

    private void setReadCapability( Capability read )
        throws HostMismatchException, PortMismatchException
    {
        if ( read != null )
        {
            if ( writeCapability != null )
            {
                if ( read.getHost().compareTo( writeCapability.getHost() ) != 0 )
                {
                    throw (new HostMismatchException());
                }
                if ( read.getPort().compareTo( writeCapability.getPort() ) != 0 )
                {
                    throw (new PortMismatchException());
                }
            }
            if ( manageCapability != null )
            {
                if ( read.getHost().compareTo( manageCapability.getHost() ) != 0 )
                {
                    throw (new HostMismatchException());
                }
                if ( read.getPort().compareTo( manageCapability.getPort() ) != 0 )
                {
                    throw (new PortMismatchException());
                }
            }
        }

        readCapability = read;
    }

    private void setWriteCapability( Capability write )
        throws HostMismatchException, PortMismatchException
    {
        if ( write != null )
        {
            if ( readCapability != null )
            {
                if ( write.getHost().compareTo( readCapability.getHost() ) != 0 )
                {
                    throw (new HostMismatchException());
                }
                if ( write.getPort().compareTo( readCapability.getPort() ) != 0 )
                {
                    throw (new PortMismatchException());
                }
            }
            if ( manageCapability != null )
            {
                if ( write.getHost().compareTo( manageCapability.getHost() ) != 0 )
                {
                    throw (new HostMismatchException());
                }
                if ( write.getPort().compareTo( manageCapability.getPort() ) != 0 )
                {
                    throw (new PortMismatchException());
                }
            }
        }

        writeCapability = write;
    }

    private void setManageCapability( Capability manage )
        throws HostMismatchException, PortMismatchException
    {
        if ( manage != null )
        {
            if ( readCapability != null )
            {
                if ( manage.getHost().compareTo( readCapability.getHost() ) != 0 )
                {
                    throw (new HostMismatchException());
                }
                if ( manage.getPort().compareTo( readCapability.getPort() ) != 0 )
                {
                    throw (new PortMismatchException());
                }
            }
            if ( writeCapability != null )
            {
                if ( manage.getHost().compareTo( writeCapability.getHost() ) != 0 )
                {
                    throw (new HostMismatchException());
                }
                if ( manage.getPort().compareTo( writeCapability.getPort() ) != 0 )
                {
                    throw (new PortMismatchException());
                }
            }
        }

        manageCapability = manage;
    }

    public Capability getReadCapability()
    {
        return (readCapability);
    }

    public Capability getWriteCapability()
    {
        return (writeCapability);
    }

    public Capability getManageCapability()
    {
        return (manageCapability);
    }

    public Depot getDepot()
    {
        if ( readCapability != null )
        {
            return (new Depot(
                readCapability.getHost(), readCapability.getPort() ));
        }
        else if ( writeCapability != null )
        {
            return (new Depot(
                writeCapability.getHost(), writeCapability.getPort() ));
        }
        else if ( manageCapability != null )
        {
            return (new Depot(
                manageCapability.getHost(), manageCapability.getPort() ));
        }

        return (null);
    }

    public int store( byte[] buf, int timeout, long size ) throws IBPException
    {
        return (ProtocolService.store( this, buf, timeout, size ));
    }

    public int load( byte[] buf, int timeout, long size, long readOffset,
        int writeOffset ) throws IBPException
    {
        return (ProtocolService.load(
            this, buf, timeout, size, readOffset, writeOffset ));
    }

    public int copy( Allocation destination, int timeout, long size, long offset )
        throws IBPException
    {
        return (ProtocolService.copy( this, destination, timeout, size, offset ));
    }

    public String toString()
    {
        return (readCapability + "\n" + writeCapability + "\n" + manageCapability);
    }
}