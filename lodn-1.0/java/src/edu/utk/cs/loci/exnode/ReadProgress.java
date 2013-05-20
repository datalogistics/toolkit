/*
 * Created on Jan 16, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.exnode;

import javax.swing.*;
import java.util.*;

/**
 * NOTE: This seems to be unused as of 20050309
 * 
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class ReadProgress
{
    private ProgressMonitor pm;
    private int length;
    private int progress;
    private Date start;

    public ReadProgress( ProgressMonitor pm, int length )
    {
        this.pm = pm;
        this.length = length;
        this.progress = 0;
        this.start = new Date();
    }

    public synchronized void addProgress( int progress )
    {
        this.progress += progress;
        float percentComplete = (float) this.progress / (float) this.length
            * 100.0f;
        Date now = new Date();
        double elapsed = (now.getTime() - start.getTime()) / 1000.0;
        double throughput = (this.progress * 8.0) / 1024.0 / 1024.0 / elapsed;
        pm.setProgress( (int) percentComplete );
        pm.setNote( (int) percentComplete + "% complete "
            + "Elapsed time (s): " + elapsed + " " + "Throughput (Mb/s): "
            + throughput + " " + "Total downloaded: (MB): " + this.progress
            / 1024.0 / 1024.0 );
    }
}