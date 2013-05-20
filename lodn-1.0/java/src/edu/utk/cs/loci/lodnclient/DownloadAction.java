/* $Id: DownloadAction.java,v 1.4 2008/05/24 22:25:53 linuxguy79 Exp $ */

package edu.utk.cs.loci.lodnclient;

import java.awt.event.ActionEvent;
import java.util.List;
import java.util.Vector;
import java.util.concurrent.CountDownLatch;

import javax.swing.*;
import edu.utk.cs.loci.exnode.*;

public class DownloadAction extends AbstractAction
{

    private LoDNClient client;
    private final List<Thread> allThreads = new Vector<Thread>();

    public DownloadAction( LoDNClient client )
    {
        super( "Download" );
        this.client = client;
    }

    public void actionPerformed( ActionEvent e )
    {
    	Thread t = null;
        String connections = (String) client.connectionsCombo.getSelectedItem();
        final int numConnections = Integer.parseInt( connections );

        String size = client.sizeField.getText();
        final int transferSize;

        if ( client.kiloButton.isSelected() )
        {
            transferSize = Integer.parseInt( size ) * 1024;
        }
        else
        {
            transferSize = Integer.parseInt( size ) * 1024 * 1024;
        }
    	final CountDownLatch allDoneBarrier = new CountDownLatch (client.exnodes.length);

		if (client.isCloseAfterTransfer()) {					
	    	startKillingThread(allDoneBarrier);
		}
    	

        if ( client.exnodes.length == 1 )
        { // ONE FILE ONLY IS DOWNLOADED

            try
            {
            	final Exnode exnode = client.exnodes[0];
                final String outputFile = client.outputFileField.getText();
                final long length = exnode.getLength();

                exnode.progress = new Progress( length );

                ProgressDialog progressDialog = new ProgressDialog( exnode, allDoneBarrier );
                progressDialog.setVisible( true );

                t = new Thread( new Runnable()
                    {
                        public void run()
                        {
                            try
                            {
                                exnode.read(
                                    outputFile, 0, length, transferSize,
                                    numConnections );
                            }
                            catch ( Exception e )
                            {
                                e.printStackTrace();
                                JOptionPane.showMessageDialog(
                                    null, "Unable to complete read operation: "
                                        + e.getMessage(), "Error",
                                    JOptionPane.ERROR_MESSAGE );
                            }
                            /* HERE THE DOWNLOAD SHOULD BE DEFINITELY COMPLETED */
                            // Force the completion, just in case...
                            exnode.progress.setDone();
                            exnode.progress.fireProgressChangedEvent();
                            exnode.progress.fireProgressDoneEvent();
                        }
                    } );
                t.start();
                allThreads.add(t);
                //while(client.exnode.getProgress()==null) {}

            }
            catch ( NullPointerException npe )
            {
                npe.printStackTrace();
                JOptionPane.showMessageDialog(
                    null, "Please specify an input (1)", "Error",
                    JOptionPane.ERROR_MESSAGE );
            }

        }
        else
        { // SEVERAL FILES TO DOWNLOAD
            try
            {
                for ( int i = 0; i < client.exnodes.length; i++ )
                {
                	final Exnode exnode = client.exnodes[i];
                    final String outputFile = client.getDefaultOutputFile( client.exnodes[i], "" );
                    client.outputFileField.setText( outputFile );
                    final long length = client.exnodes[i].getLength();
                    final int fi = i; // i must be declared final

                    exnode.progress = new Progress( length );
                    ProgressDialog progressDialog = new ProgressDialog( exnode, allDoneBarrier );
                    progressDialog.setVisible( true );

                    t = new Thread( new Runnable()
                        {
                            public void run()
                            {
                                try
                                {
                                    client.exnodes[fi].read(
                                        outputFile, 0, length, transferSize,
                                        numConnections );
                                }
                                catch ( Exception e )
                                {
                                    e.printStackTrace();
                                    JOptionPane.showMessageDialog(
                                        null,
                                        "Unable to complete read operation: "
                                            + e.getMessage(), "Error",
                                        JOptionPane.ERROR_MESSAGE );
                                }
                            }
                        } );
                    t.start();
                    allThreads.add(t);
                }
            }
            catch ( NullPointerException npe )
            {
                npe.printStackTrace();
                JOptionPane.showMessageDialog(
                    null, "Please specify an input (2)", "Error",
                    JOptionPane.ERROR_MESSAGE );
            }
        }
    } // actionPerformed()

	private void startKillingThread(final CountDownLatch allDoneBarrier) {
		// Setup the auto-close thread
		Thread autoCloser = new Thread  (new Runnable() {
		
			public void run() {
				try {
					allDoneBarrier.await();
					for (Thread t: allThreads) { //We wait for all the rest of the stuff to finish
						t.join();
					}
				} catch (InterruptedException e) {
					// Do nothing
				}
				System.exit(0);
			}
		}, "Killer Thread");
		
		autoCloser.start();
	}
}