/* $Id: ChecksumException.java,v 1.4 2008/05/24 22:25:51 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

public class ChecksumException extends Exception
{

    public ChecksumException()
    {
        super();
    }

    public ChecksumException( String msg )
    {
        super( msg );
    }
}