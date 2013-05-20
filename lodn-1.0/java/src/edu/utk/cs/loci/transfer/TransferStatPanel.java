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
import java.util.GregorianCalendar;
import java.util.Calendar;

/**
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class TransferStatPanel extends JPanel
{
    public static Log DEBUG = new Log( true );

    protected JTextField name;
    protected JTextField port;
    protected JTextField pending;
    protected JTextField lastSpeed;
    protected JTextField aveSpeed;
    protected JTextField speedSD;
    protected JTextField totalVolume;
    protected JTextField totalTime;
    protected JTextField wallSpeed;
    protected JTextField success;
    protected JTextField fails;
    protected JTextField total;
    protected JTextField failRate;
    protected JTextField lastFailed;
    protected JTextField lastTryTime;

    protected TransferStat stat;
    protected long lastUpdate;

    /**
     * 
     */
    public TransferStatPanel( TransferStat stat )
    {
        super();
        this.stat = stat;
        this.setLayout( new BoxLayout( this, BoxLayout.X_AXIS ) );

        // this.setLayout( new GridLayout( 1, 15 ) );

        this.name = this.addTextField( 15, "Hostname" );
        this.port = this.addTextField( 4, "Port" );
        this.pending = this.addTextField( 2, "Pending" );
        this.totalVolume = this.addTextField( 4, "volume (MB)" );
        this.lastSpeed = this.addTextField( 4, "Last Speed (MB/s)" );
        this.wallSpeed = this.addTextField( 4, "Wall Speed (MB/s)" );
        this.aveSpeed = this.addTextField( 4, "Ave (MB/s)" );
        this.speedSD = this.addTextField( 4, "stdev" );
        this.totalTime = this.addTextField( 4, "wall time (s)" );
        this.success = this.addTextField( 3, "Success" );
        this.fails = this.addTextField( 2, "Failure" );
        this.total = this.addTextField( 3, "Total " );
        this.failRate = this.addTextField( 5, "Fail rate" );
        this.lastFailed = this.addTextField( 4, "Last failed?" );
        this.lastTryTime = this.addTextField( 6, "Last try" );

        this.validate();

        if ( this.stat != null )
        {
            this.name.setText( stat.hostname );
            this.port.setText( "" + stat.port );

            this.updateStats();
        }
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
        if ( stat != null )
        {
            if ( (stat.getLastTryTime() > this.lastUpdate)
                || (stat.getLastSuccessTime() > this.lastUpdate)
                || (stat.getLastFailTime() > this.lastUpdate) )
            {
                this.lastUpdate = System.currentTimeMillis();

                // DEBUG.println( "Inside " + this + " updateStats() ... " );

                this.pending.setText( "" + stat.getThreadCount() );

                double d = stat.getLastBlockTransferSpeed().getSpeed();
                this.lastSpeed.setText( formatSpeed( d ) );

                DoubleStat blkTransferStat = stat.getBlockTransferStat();

                d = blkTransferStat.getMean();
                this.aveSpeed.setText( formatSpeed( d ) );

                d = blkTransferStat.getSampleStdDev();
                this.speedSD.setText( formatSpeed( d ) );

                DoubleSpeedMeter wallSpeedMeter = stat.getWallClockSpeed();
                double volumeMB = wallSpeedMeter.getProgress() / 1e6;
                double timeS = wallSpeedMeter.getTime() / 1e3;
                double wallSpeed = wallSpeedMeter.getSpeedDoubleValue();

                this.totalVolume.setText( DoubleSpeed.to2DecPlaces( volumeMB ) );
                this.totalTime.setText( DoubleSpeed.to2DecPlaces( timeS ) );
                this.wallSpeed.setText( formatSpeed( wallSpeed ) );

                this.success.setText( "" + stat.getSuccessCount() );
                this.fails.setText( "" + stat.getFailCount() );
                this.total.setText( "" + stat.getTriesCount() );
                ;
                this.failRate.setText( ""
                    + DoubleSpeed.to2DecPlaces( stat.getFailureRate() * 100 )
                    + "%" );
                this.lastFailed.setText( "" + stat.isLastTimeFailed() );

                Date lastDate = new Date( stat.getLastTryTime() );
		Calendar cal  = new GregorianCalendar();
		cal.setTime(lastDate);
                String timeString = cal.get(Calendar.HOUR_OF_DAY) + ":" 
                    + cal.get(Calendar.MINUTE) + ":"
                    + cal.get(Calendar.SECOND);
                /*String timeString = lastDate.getHours() + ":" 
                    + lastDate.getMinutes() + ":"
                    + lastDate.getSeconds();*/
                this.lastTryTime.setText( timeString );

            }
        }
    }

    //    public static TransferStatPanel getHeaderPanel()
    //    {
    //        TransferStatPanel header = new TransferStatPanel( null );
    //        header.name.setText( "Hostname" );
    //        header.port.setText( "Port" );
    //        header.pending.setText( "Pending" );
    //        header.lastSpeed.setText( "Last (MB/s)" );
    //        header.aveSpeed.setText( "Ave (MB/s)" );
    //        header.speedSD.setText( "stdev" );
    //        header.totalVolume.setText( "volume (MB)" );
    //        header.totalTime.setText( "wall time (s)" );
    //        header.wallSpeed.setText( "Wall Speed (MB/s)" );
    //        header.success.setText( "OK" );
    //        header.fails.setText( "Fail" );
    //        header.total.setText( "Total" );
    //        header.failRate.setText( "Fail rate" );
    //        header.lastFailed.setText( "Last failed?" );
    //        header.timeSinceLastTry.setText( "Last try (s)" );
    //        return header;
    //    }

}
