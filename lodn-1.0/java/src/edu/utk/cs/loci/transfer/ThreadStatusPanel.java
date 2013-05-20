/*
 * Created on Feb 9, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package edu.utk.cs.loci.transfer;

import javax.swing.*;

import com.bayanihancomputing.util.time.*;
import com.bayanihancomputing.util.stat.*;

import edu.utk.cs.loci.ibp.Log;

import java.awt.*;
import java.util.Date;

/**
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class ThreadStatusPanel extends JPanel
{
    public static Log DEBUG = new Log( true );

    protected JTextField threadName;
    protected JTextField hostName;
    protected JTextField port;
    protected JTextField job;
    protected JTextField status;

    protected TransferThread thread;
    protected long lastUpdate;

    private JTextField progress;

    private JTextField totalTime;

    private JTextField speed;

    /**
     * 
     */
    public ThreadStatusPanel( String threadName, TransferThread thread )
    {
        super();
        this.thread = thread;
        this.setLayout( new BoxLayout( this, BoxLayout.X_AXIS ) );

        this.threadName = this.addTextField( 3, "Thread" );
        this.hostName = this.addTextField( 15, "Hostname" );
        this.port = this.addTextField( 5, "Port" );
        this.job = this.addTextField( 15, "Job" );
        this.progress = this.addTextField( 7, "bytes" );
        this.totalTime = this.addTextField( 4, "time (s)" );
        this.speed = this.addTextField( 4, "speed (MB/s)" );
        this.status = this.addTextField( 20, "status" );

        this.validate();

        this.threadName.setText( threadName );
        //
        //        if ( this.thread != null )
        //        {
        //            this.updateStats();
        //        }
    }

    public JTextField addTextField( int width )
    {
        JTextField tf = new JTextField( width );

        // make sure it stays the same size
        // Dimension d = tf.getSize();
        // tf.setMaximumSize( d );
        // tf.setMinimumSize( d );
        this.add( tf );
        return tf;
    }

    public JTextField addTextField( String s )
    {
        JTextField tf = new JTextField( s.length() );

        // make sure it stays the same size
        // Dimension d = tf.getSize();
        // tf.setMaximumSize( d );
        // tf.setMinimumSize( d );
        this.add( tf );
        tf.setText( s );
        return tf;
    }

    public JTextField addTextField( int width, String s )
    {
        JTextField tf = this.addTextField( width );
        tf.setText( s );
        return tf;
    }

    public String formatSpeed( double speed )
    {
        // speed is in bytes / ms
        // divide by ( 1e6 ) to get MB / ms,
        // then multiply by 1e3 to get MB / s
        double scaledToMBs = speed / 1e3;
        return DoubleSpeed.to2DecPlaces( scaledToMBs );
    }

    public synchronized void updateStats()
    {
        if ( thread != null )
        {
            if ( thread.isMonitorOn() )
            {
                DoubleSpeedMeter speedMeter = thread.getSpeedMeter();
                DoubleSpeed speed = speedMeter.getSpeed();

                double d = speed.getSpeed();
                this.speed.setText( formatSpeed( d ) );
                d = speed.getProgress();
                this.progress.setText( "" + (long) d );
                d = speed.getTime();
                this.totalTime.setText( DoubleSpeed.to2DecPlaces( d / 1e3 ) );
            }

            this.hostName.setText( thread.getHost() );
            this.port.setText( "" + thread.getPort() );
            this.job.setText( "" + thread.getCurJob() );
            this.status.setText( thread.getStatus() );
        }
    }

}