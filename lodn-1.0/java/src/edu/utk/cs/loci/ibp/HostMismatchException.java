package edu.utk.cs.loci.ibp;

import java.lang.Exception;

public class HostMismatchException extends Exception
{
    public HostMismatchException()
    {
        super( "Capability hosts don't match" );
    }
}