package edu.utk.cs.loci.ibp;

import java.lang.Exception;

public class PortMismatchException extends Exception
{
    public PortMismatchException()
    {
        super( "Capability ports don't match" );
    }
}