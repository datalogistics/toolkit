/* $Id: ProgressListener.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

public interface ProgressListener
{
    public void progressUpdated( ProgressEvent e );

    public void progressDone( ProgressEvent e );

    public void progressError( ProgressEvent e );
}