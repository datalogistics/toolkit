/* $Id: ProgressDialog.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.awt.*;
import java.awt.event.*;

import javax.swing.*;

import java.text.NumberFormat;
import java.util.HashMap;
import java.util.concurrent.CountDownLatch;

import edu.utk.cs.loci.exnode.*;
import edu.utk.cs.loci.ibp.Log;
import edu.utk.cs.loci.transfer.*;

public class ProgressDialog extends JDialog implements ProgressListener
{
    public static Log DEBUG = new Log( true );

    protected TransferStatListDisplay statDisplay = null;
    protected boolean statDisplayVisible = false;

    protected ThreadStatusListDisplay threadDisplay = null;
    protected boolean threadDisplayVisible = false;

    
    protected Exnode exnode;
    protected NumberFormat format;
    protected ProgressDialogPanel progressDialogPanel;
    private CountDownLatch allDone;

    public ProgressDialog(Exnode exnode) {
    	this(exnode, new CountDownLatch(1));
    }
    
    public ProgressDialog( Exnode exnode, CountDownLatch allDoneBarrier )
    {
    	allDone = allDoneBarrier;
        this.setTitle( "LoDN download" );
        this.exnode = exnode;
        this.format = NumberFormat.getNumberInstance();
        this.format.setMaximumFractionDigits( 3 );
        this.progressDialogPanel = new ProgressDialogPanel( this, exnode ); 
        this.setContentPane( this.progressDialogPanel );
        this.setModal( false );
        this.pack();
        exnode.addProgressListener( this );

        Scoreboard scoreboard = ReadJob.getScoreboard();
        HashMap depotStats = scoreboard.getDepotStats();

        Rectangle bounds = this.getBounds();
        this.statDisplay = new TransferStatListDisplay( depotStats );
        this.statDisplay.setLocation( bounds.x, bounds.y + bounds.height + 5 );
        this.threadDisplay = new ThreadStatusListDisplay( exnode );
        this.threadDisplay.setLocation( bounds.x + bounds.width + 5, bounds.y );

        DEBUG.println( "ProgressDialog (bld): addProgressListener done!" );
    }

    public void setVisible( boolean b )
    {
        super.setVisible( b );
        this.statDisplay.setVisible( b && this.statDisplayVisible );
        this.threadDisplay.setVisible( b && this.threadDisplayVisible );
    }

    public void progressUpdated( ProgressEvent e )
    {
        Progress progress = exnode.getProgress();
        ProgressDialogPanel panel = (ProgressDialogPanel) getContentPane();
        panel.progressBar.setIndeterminate( false );

        String statusMsg = progress.getStatusMsg();
        if ( statusMsg != null )
        {
            panel.statusField.setText( statusMsg );
        }

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

        panel.progressBar.setValue( (int) progress.getPercentComplete() );

        this.statDisplay.updateStats();
        this.threadDisplay.updateStats();
    }

    public void progressDone( ProgressEvent e )
    {
        try
        {
            ProgressDialogPanel panel = (ProgressDialogPanel) getContentPane();

            DEBUG.println( "progressDone (lodncli): panel = " + panel );
            panel.progressBar.setValue( panel.progressBar.getMaximum() );
            panel.setComplete( true );
            Toolkit.getDefaultToolkit().beep(); // beep!

            this.statDisplay.updateStats();
            this.threadDisplay.updateStats();
            allDone.countDown();
        }
        catch ( Exception excpt )
        {
            DEBUG.error( "progressDone: " + excpt );
        }
    }

    public void progressError( ProgressEvent e )
    {
        ProgressDialogPanel panel = (ProgressDialogPanel) getContentPane();
        panel.setComplete( true );

        this.statDisplay.updateStats();
        this.threadDisplay.updateStats();
    }
    
    public void addOKActionListener( ActionListener al )
    {
        this.progressDialogPanel.okButton.addActionListener( al );
    }

    public void addCancelActionListener( ActionListener al )
    {
        this.progressDialogPanel.cancelButton.addActionListener( al );
    }
    
    
    public class ProgressDialogPanel extends JPanel
    {
        JLabel statusLabel;
        JTextField statusField;
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

        JPanel checkBoxPanel;
        JCheckBox statDisplayChk;
        JCheckBox threadDisplayChk;

        boolean complete;

        public ProgressDialogPanel( JDialog dialog, Exnode exnode )
        {
            complete = false;
            statusLabel = new JLabel( "Status:" );
            statusField = new JTextField( "Downloading exnode.  Total size: "
                + exnode.getLength() + " bytes" );
            statusLabel.setLabelFor( statusField );

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
            progressBar.setStringPainted( true );

            okButton = new JButton( new ProgressOKAction( dialog, exnode ) );
            okButton.setMnemonic( KeyEvent.VK_O );
            okButton.setVisible( false );

            cancelButton = new JButton( new ProgressCancelAction(
                dialog, exnode ) );
            cancelButton.setMnemonic( KeyEvent.VK_C );

            //          FormLayout layout=new FormLayout("3dlu,right:max(40dlu;p),3dlu,p,3dlu",
            //                                           "3dlu,p,3dlu,p,3dlu,p,3dlu,p,3dlu,p,3dlu,p,9dlu,p,3dlu");
            //          layout.setRowGroups(new int[][] {{4,6,8,10}});

            this.setLayout( new BoxLayout( this, BoxLayout.Y_AXIS ) );
            this.add( new LabeledField( statusLabel, statusField ) );
            this.add( new LabeledField( totalLabel, totalField ) );
            this.add( new LabeledField( throughputLabel, throughputField ) );
            this.add( new LabeledField( percentLabel, percentField ) );
            this.add( new LabeledField( remainingLabel, remainingField ) );

            checkBoxPanel = new JPanel();
            checkBoxPanel.setLayout( new FlowLayout() );
            statDisplayChk = new JCheckBox( "display scoreboard", false );
            statDisplayChk.addItemListener( new ItemListener()
                {
                    public void itemStateChanged( ItemEvent e )
                    {
                        statDisplayVisible = statDisplayChk.isSelected();
                        statDisplay.setVisible( statDisplayVisible );
                    }
                } );
            checkBoxPanel.add( statDisplayChk );
            threadDisplayChk = new JCheckBox( "display thread list", false );
            threadDisplayChk.addItemListener( new ItemListener()
                {
                    public void itemStateChanged( ItemEvent e )
                    {
                        threadDisplayVisible = threadDisplayChk.isSelected();
                        threadDisplay.setVisible( threadDisplayVisible );
                    }
                } );
            checkBoxPanel.add( threadDisplayChk );
            this.add( checkBoxPanel );

            this.add( new LabeledField( new JLabel( "Progress" ), progressBar ) );
            JPanel buttonPanel = new JPanel();
            buttonPanel.setLayout( new GridLayout( 1, 2 ) );
            buttonPanel.add( okButton );
            buttonPanel.add( cancelButton );
            this.add( buttonPanel );

            //          PanelBuilder builder=new PanelBuilder(this,layout);
            //          CellConstraints cc=new CellConstraints();
            //          builder.addSeparator("Transfer Progress",cc.xywh(2,2,3,1));
            //          builder.add(totalLabel,cc.xy(2,4));
            //          builder.add(totalField,cc.xy(4,4));
            //          builder.add(throughputLabel,cc.xy(2,6));
            //          builder.add(throughputField,cc.xy(4,6));
            //          builder.add(percentLabel,cc.xy(2,8));
            //          builder.add(percentField,cc.xy(4,8));
            //          builder.add(remainingLabel,cc.xy(2,10));
            //          builder.add(remainingField,cc.xy(4,10));
            //          builder.add(progressBar,cc.xywh(2,12,3,1));
            //          builder.add(okButton,cc.xy(4,14));
            //          builder.add(cancelButton,cc.xy(4,14));
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

        public class LabeledField extends JPanel
        {
            public LabeledField( JLabel label, JComponent c )
            {
                this.setLayout( new java.awt.BorderLayout() );
                this.add( label, java.awt.BorderLayout.WEST );
                this.add( c, java.awt.BorderLayout.CENTER );
            }
        }

        public class ProgressCancelAction extends AbstractAction
        {

            private JDialog dialog;
            private Exnode exnode;

            public ProgressCancelAction( JDialog dialog, Exnode exnode )
            {
                super( "Cancel" );
                this.dialog = dialog;
                this.exnode = exnode;
            }

            public void actionPerformed( ActionEvent e )
            {
                exnode.setCancel( true );
                dialog.dispose();
            }

        }

        public class ProgressOKAction extends AbstractAction
        {

            private JDialog dialog;
            private Exnode exnode;

            public ProgressOKAction( JDialog dialog, Exnode exnode )
            {
                super( "OK" );
                this.dialog = dialog;
                this.exnode = exnode;
            }

            public void actionPerformed( ActionEvent e )
            {
                dialog.dispose();
                //client.cancelButton.setText("Exit");
            }

        }
    }

}