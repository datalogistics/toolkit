/*
 * Created on Mar 1, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.lodnpublisher;

import java.awt.event.*;
import javax.swing.*;
import com.jgoodies.forms.layout.*;
import com.jgoodies.forms.builder.*;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class ProgressDialogPanel extends JPanel
{
    JLabel totalLabel;
    JLabel throughputLabel;
    JLabel percentLabel;
    JLabel remainingLabel;
    JTextField totalField;
    JTextField throughputField;
    JTextField percentField;
    JTextField remainingField;
    JProgressBar progressBar;
    JButton okButton;
    JButton cancelButton;
    boolean complete;

    public ProgressDialogPanel( JDialog dialog, LoDNPublisher pub, long length )
    {
        complete = false;

        totalLabel = new JLabel( "Total Transferred (MB):" );
        totalLabel.setLabelFor( totalField );

        throughputLabel = new JLabel( "Avg. Throughput (Mb/s):" );
        throughputLabel.setLabelFor( throughputField );

        percentLabel = new JLabel( "Percent complete:" );
        percentLabel.setLabelFor( percentField );

        remainingLabel = new JLabel( "Time Remaining (h:mm:ss):" );
        remainingLabel.setLabelFor( remainingField );

        totalField = new JTextField();
        totalField.setEditable( false );

        throughputField = new JTextField();
        throughputField.setEditable( false );

        percentField = new JTextField();
        percentField.setEditable( false );

        remainingField = new JTextField();
        remainingField.setEditable( false );

        progressBar = new JProgressBar();
        progressBar.setIndeterminate( true );
        progressBar.setMaximum( 100 );

        okButton = new JButton( new ProgressOKAction( dialog, pub ) );
        okButton.setMnemonic( KeyEvent.VK_O );
        okButton.setVisible( false );

        cancelButton = new JButton( new ProgressCancelAction( dialog, pub ) );
        cancelButton.setMnemonic( KeyEvent.VK_C );

        FormLayout layout = new FormLayout(
            "3dlu,right:max(40dlu;p),3dlu,p,3dlu",
            "3dlu,p,3dlu,p,3dlu,p,3dlu,p,3dlu,p,3dlu,p,9dlu,p,3dlu" );
        layout.setRowGroups( new int[][]
            {
                { 4, 6, 8, 10 } } );

        PanelBuilder builder = new PanelBuilder( this, layout );
        CellConstraints cc = new CellConstraints();
        builder.addSeparator( "Transfer Progress", cc.xywh( 2, 2, 3, 1 ) );
        builder.add( totalLabel, cc.xy( 2, 4 ) );
        builder.add( totalField, cc.xy( 4, 4 ) );
        builder.add( throughputLabel, cc.xy( 2, 6 ) );
        builder.add( throughputField, cc.xy( 4, 6 ) );
        builder.add( percentLabel, cc.xy( 2, 8 ) );
        builder.add( percentField, cc.xy( 4, 8 ) );
        builder.add( remainingLabel, cc.xy( 2, 10 ) );
        builder.add( remainingField, cc.xy( 4, 10 ) );
        builder.add( progressBar, cc.xywh( 2, 12, 3, 1 ) );
        builder.add( okButton, cc.xy( 4, 14 ) );
        builder.add( cancelButton, cc.xy( 4, 14 ) );
    }

    public void setComplete( boolean flag )
    {
        if ( flag == true )
        {
            okButton.setVisible( true );
            cancelButton.setVisible( false );
        }
        else
        {
            okButton.setVisible( false );
            cancelButton.setVisible( true );
        }
        revalidate();
    }
}

