/* $Id: ProgressDialog.java,v 1.4 2008/05/24 22:25:53 linuxguy79 Exp $ */

package edu.utk.cs.loci.lodnpublisher;

import javax.swing.*;
import java.text.NumberFormat;
import edu.utk.cs.loci.exnode.*;

public class ProgressDialog extends JDialog implements ProgressListener
{

    static final boolean DBG = false;

    LoDNPublisher pub;
    NumberFormat format;

    public ProgressDialog( LoDNPublisher pub, long length )
    {
        this.format = NumberFormat.getNumberInstance();
        this.format.setMaximumFractionDigits( 3 );
        this.pub = pub;
        this.setContentPane( new ProgressDialogPanel( this, pub, length ) );
        this.setModal( true );
        this.pack();
        pub.exnode.addProgressListener( this );
    }

    public void progressUpdated( ProgressEvent e )
    {
        Progress progress = pub.exnode.getProgress();
        ProgressDialogPanel panel = (ProgressDialogPanel) getContentPane();
        panel.progressBar.setIndeterminate( false );

        Double totalProgress = new Double(
            progress.getProgress() / 1024.0 / 1024.0 );
        panel.totalField.setText( format.format( totalProgress ) );

        Double throughput = new Double( progress.getThroughput() );
        panel.throughputField.setText( format.format( throughput ) );

        Double percentComplete = new Double( progress.getPercentComplete() );
        panel.percentField.setText( format.format( percentComplete ) );

        int remaining = new Double( progress.getETA() ).intValue();
        int hours = remaining / 3600;
        int minutes = (remaining % 3600) / 60;
        int seconds = (remaining % 3600) % 60;
        String eta = new String( hours + " : " + minutes + " : " + seconds );
        panel.remainingField.setText( eta );

        panel.progressBar.setValue( (int)( 100 * progress.getPercentComplete() ) );
    }

    public void progressDone( ProgressEvent e )
    {
        ProgressDialogPanel panel = (ProgressDialogPanel) getContentPane();
        if ( DBG )
            System.out.println( "progressDone (lodnpub): panel = " + panel );
        panel.progressBar.setValue( panel.progressBar.getMaximum() );
        panel.setComplete( true );
    }

    public void progressError( ProgressEvent e )
    {
        ProgressDialogPanel panel = (ProgressDialogPanel) getContentPane();
        panel.setComplete( true );
    }
}
